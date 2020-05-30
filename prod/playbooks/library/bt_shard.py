#!/usr/bin/env python

from ansible.module_utils.basic import AnsibleModule
from collections import defaultdict, namedtuple
from jinja2 import Template


TEMPLATE = """
# Do not update this file: it is updated by Ansible to populate the
# sharding mapping, which can evolve each time a new shard is added to
# the cluster. This make it possible to start with a small cluster and
# little by little, add machines as the backtracer takes more and
# space.

instance_type: "mixer"
worker_timeout_ms: 60000

network:
  host: "{{ mixer_ip }}"
  port: {{ mixer_port }}

shards:
{%- for shard in shards %}
  - name: "{{ shard['name'] }}"
    workers: {{ shard['workers'] }}
{%- endfor %}

partitions:
{%- for at, partitions in partitions.items() %}
  - at: {{ at }}
    shards:
    {%- for partition in partitions %}
    - shard: "{{ partition['shard'] }}"
      {% if partition['area']['area'] == "default" -%}
      area: "default"
      {%- else -%}
      area: "{{ partition['area']['area'] }}"
      top_left: {{ partition['top_left'] }}
      bottom_right: {{ partition['bottom_right'] }}
      {%- endif %}
    {%- endfor %}
{% endfor %}
"""


Partition = namedtuple(
    "Partition", ["area", "shard", "top_left", "bottom_right"])


def make_partition(area, shard, idx, shard_count):
    """Creates a partition given a shard and its id."""
    if (area['area'] == "default"):
        return Partition(
            area=area, shard=shard['name'], top_left=None, bottom_right=None)

    increments = (
        area['bottom_right'][1] - area['top_left'][1]) / float(shard_count)

    # Here we shard the area in horizontal stripes, this is a
    # arbitrary way of sharding. It doesn't need to be aligned on a
    # block, the mixer will round properly.
    top_left = [
        area['top_left'][0],
        area['top_left'][1] + idx * increments
    ]
    bottom_right = [
        area['bottom_right'][0],
        area['top_left'][1] + (idx + 1) * increments
    ]

    return Partition(
        area=area,
        shard=shard['name'],
        top_left=top_left,
        bottom_right=bottom_right
    )


def make_partitions(areas, shards):
    """Creates a list of partitions given shards and areas.
    """
    partitions = []

    shards_per_area = defaultdict(list)
    for shard in shards:
        shards_per_area[shard['area']].append(shard)

    areas_per_code = dict()
    for area in areas:
        areas_per_code[area['area']] = area

    for area, shard_entries in shards_per_area.items():
        i = 0
        while i < len(shard_entries):
            partitions.append(
                make_partition(
                    areas_per_code[area], shard_entries[i], i,
                    len(shard_entries)))
            i = i + 1
    
    return partitions


def run_module():
    fields = {
        "shards": {"required": True, "type": "list"},
        "geo": {"required": True, "type": "list"},
        "dest": {"required": True, "type": "str"},
        "mixer_ip": {"required": True, "type": "str"},
        "mixer_port": {"required": True, "type": "int"},
    }

    module = AnsibleModule(argument_spec=fields)
    shards = module.params['shards']
    areas = module.params['geo']
    partitions = make_partitions(areas, shards)

    # This is to be properly handled when we want to support cluster
    # growth properly; to be done:
    #
    # Parse existing config, compare last partitions with the ones
    # just computed, if they differ, it means a new shard was added to
    # the cluster and so, we need to append a new partition scheme at
    # the end, using the current timestamp + 1h or so, to let time for
    # mixers to catch the new config.

    ts = 0
    dated_partitions = dict()
    dated_partitions[ts] = partitions

    content = Template(TEMPLATE).render(
        shards=shards,
        partitions=dated_partitions,
        mixer_port=module.params['mixer_port'],
        mixer_ip=module.params['mixer_ip'])

    with open(module.params['dest'], "w+") as f:
        f.write(content)

    response = {"status": "ok"}
    module.exit_json(changed=True, meta=response)
   

def main():
    run_module()


if __name__ == '__main__':
    main()
