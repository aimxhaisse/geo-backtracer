#!/usr/bin/env python

from ansible.module_utils.basic import AnsibleModule
from collections import defaultdict, namedtuple, OrderedDict
from jinja2 import Template

import calendar
import time
import yaml


TEMPLATE = """
# Do not update this file: it is updated by Ansible to populate the
# sharding mapping, which can evolve each time a new shard is added to
# the cluster. This make it possible to start with a small cluster and
# little by little, add machines as the backtracer takes more and
# space.

instance_type: "mixer"

# Whether or not to fail fast connections to workers. In production
# environments, it must be set to false as we want to do long
# exponential backoffs to handles nicely upgrades etc.
backoff_fail_fast: false

network:
  host: "{{ mixer_ip }}"
  port: {{ mixer_port }}

correlator:
  # Two points close enough in time will be considers at the same
  # moment, this shouldn't be relied on a lot, the client should
  # synchronize clocks (using GPS time) so that they fetch points at
  # around the same moment, doing so on the client increases the
  # accuracy as moving users can be correlated.
  nearby_seconds: 30

  # About 4.4 meters (this is the precision of GPS coordinates).
  nearby_gps_distance: 0.000004

shards:
{%- for shard in shards %}
  - name: "{{ shard['name'] }}"
    workers: [{% for w in shard['hosts'] %}"{{ w }}:{{ shard['worker_port'] }}"{{", " if not loop.last}}{% endfor %}]
{%- endfor %}

partitions:
{%- for at, partitions in dated_partitions %}
  - at: {{ at }}
    shards:
    {%- for partition in partitions %}
    - shard: "{{ partition['shard'] }}"
      {% if partition['area']['area'] == "default" -%}
      area: "default"
      {%- else -%}
      area: "{{ partition['area']['area'] }}"
      bottom_left: {{ partition['bottom_left'] }}
      top_right: {{ partition['top_right'] }}
      {%- endif %}
    {%- endfor %}
{% endfor %}
"""


def make_latitude_top(start, end, nb_shard, idx):
    dist = end - start
    inc = dist / float(nb_shard)

    return round(start + idx * inc, 2)


def make_latitude_bot(start, end, nb_shard, idx):
    dist = end - start
    inc = dist / float(nb_shard)

    return round(start + (idx + 1) * inc, 2)


def make_partition(area, shard, idx, shard_count):
    """Creates a partition given a shard and its id."""
    if (area['area'] == "default"):
        return {
            'area': area,
            'shard': shard['name'],
            'bottom_left': None,
            'top_right': None
        }

    # Here we shard the area in horizontal stripes, this is a
    # arbitrary way of sharding. It doesn't need to be aligned on a
    # block, the mixer will round properly.
    bottom_left = [
        make_latitude_top(
            area['bottom_left'][0], area['top_right'][0], shard_count, idx),
        area['bottom_left'][1]
    ]
    top_right = [
        make_latitude_bot(
            area['bottom_left'][0], area['top_right'][0], shard_count, idx),
        area['top_right'][1]
    ]

    return {
        'area': area,
        'shard': shard['name'],
        'bottom_left': bottom_left,
        'top_right': top_right
    }


def make_partitions(areas, shard_history, all_shards):
    """Creates a list of partitions given shards and areas.
    """
    partitions = []

    shards = []
    for history in shard_history:
        for all_shard in all_shards:
            if all_shard['name'] == history:
                shards.append(all_shard)
                continue

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


def get_existing_config(dest):
    """Returns the existing configuration or an empty dict.
    """
    try:
        with open(dest, 'r') as stream:
            return yaml.safe_load(stream)
    except (yaml.YAMLError, FileNotFoundError):
        pass

    return dict()


def run_module():
    fields = {
        "shards": {"required": True, "type": "list"},
        "history": {"required": True, "type": "dict"},
        "geo": {"required": True, "type": "list"},
        "dest": {"required": True, "type": "str"},
        "mixer_ip": {"required": True, "type": "str"},
        "mixer_port": {"required": True, "type": "int"},
    }

    module = AnsibleModule(argument_spec=fields)
    shards = module.params['shards']
    areas = module.params['geo']

    dated_partitions = dict()
    for ts, shard_history in module.params['history'].items():
        dated_partitions[ts] = make_partitions(areas, shard_history, shards)

    content = Template(TEMPLATE).render(
        shards=shards,
        dated_partitions=[(k, dated_partitions[k]) for k in sorted(dated_partitions)],
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
