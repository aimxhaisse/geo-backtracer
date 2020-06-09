#!/usr/bin/env python3

import os
import tempfile
import textwrap
import unittest
import uuid

from unittest.mock import patch

import bt_shard


def MakeAnsibleMock(params):

    class AnsibleModuleMock(object):
        def __init__(self, *args, **kwargs):
            self.params = params

        def exit_json(self, *args, **kwargs):
            pass

    return AnsibleModuleMock


class BtShardTest(unittest.TestCase):

    def setUp(self):
        self._tmp_file = '/tmp/{0}.tmpfile'.format(str(uuid.uuid1()))
        self.maxDiff = None

    def tearDown(self):
        if os.path.exists(self._tmp_file):
            os.remove(self._tmp_file)

    def mixerConfig(self):
        with open(self._tmp_file, mode='r') as fh:
            return fh.read()

    def testSingleShard(self):
        params = {
            'shards': [
                {
                    'name': 'shard-a',
                    'area': 'default',
                    'hosts': ['gamgee', 'bombadil'],
                    'worker_port': 7000,
                    'mixer_port': 8000
                }
            ],
            'geo': [
                {
                    'area': 'default'
                }
            ],
            'dest': self._tmp_file,
            'mixer_ip': '10.0.0.1',
            'mixer_port': 8000
        }
        with patch.object(bt_shard, 'AnsibleModule', MakeAnsibleMock(params)):
            bt_shard.run_module()

            expected = textwrap.dedent("""
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
              host: "10.0.0.1"
              port: 8000

            shards:
              - name: "shard-a"
                workers: ["gamgee:7000", "bombadil:7000"]

            partitions:
              - at: 0
                shards:
                - shard: "shard-a"
                  area: "default"
            """)

            self.assertEqual(self.mixerConfig(), expected)

    def testTwoShards(self):
        params = {
            'shards': [
                {
                    'name': 'shard-a',
                    'area': 'default',
                    'hosts': ['gamgee', 'bombadil'],
                    'worker_port': 7000,
                    'mixer_port': 8000
                },
                {
                    'name': 'shard-b',
                    'area': 'fr',
                    'hosts': ['gamgee', 'bombadil'],
                    'worker_port': 7001,
                    'mixer_port': 8001
                },
            ],
            'geo': [
                {
                    'area': 'default'
                },
                {
                    'area': "fr",
                    'top_left': [-5.00, 51.00],
                    'bottom_right': [7.50, 44.00]
                }
            ],
            'dest': self._tmp_file,
            'mixer_ip': '10.0.0.1',
            'mixer_port': 8000
        }
        with patch.object(bt_shard, 'AnsibleModule', MakeAnsibleMock(params)):
            bt_shard.run_module()

            expected = textwrap.dedent("""
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
              host: "10.0.0.1"
              port: 8000

            shards:
              - name: "shard-a"
                workers: ["gamgee:7000", "bombadil:7000"]
              - name: "shard-b"
                workers: ["gamgee:7001", "bombadil:7001"]

            partitions:
              - at: 0
                shards:
                - shard: "shard-a"
                  area: "default"
                - shard: "shard-b"
                  area: "fr"
                  top_left: [-5.0, 51.0]
                  bottom_right: [7.5, 44.0]
            """)

            self.assertEqual(self.mixerConfig(), expected)

    def testThreeShards(self):
        params = {
            'shards': [
                {
                    'name': 'shard-a',
                    'area': 'default',
                    'hosts': ['gamgee', 'bombadil'],
                    'worker_port': 7000,
                    'mixer_port': 8000
                },
                {
                    'name': 'shard-b',
                    'area': 'fr',
                    'hosts': ['gamgee', 'bombadil'],
                    'worker_port': 7001,
                    'mixer_port': 8001
                },
                {
                    'name': 'shard-c',
                    'area': 'fr',
                    'hosts': ['gamgee', 'bombadil'],
                    'worker_port': 7002,
                    'mixer_port': 8002
                },
            ],
            'geo': [
                {
                    'area': 'default'
                },
                {
                    'area': "fr",
                    'top_left': [-5.00, 51.00],
                    'bottom_right': [7.50, 44.00]
                }
            ],
            'dest': self._tmp_file,
            'mixer_ip': '10.0.0.1',
            'mixer_port': 8000
        }
        with patch.object(bt_shard, 'AnsibleModule', MakeAnsibleMock(params)):
            bt_shard.run_module()

            expected = textwrap.dedent("""
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
              host: "10.0.0.1"
              port: 8000

            shards:
              - name: "shard-a"
                workers: ["gamgee:7000", "bombadil:7000"]
              - name: "shard-b"
                workers: ["gamgee:7001", "bombadil:7001"]
              - name: "shard-c"
                workers: ["gamgee:7002", "bombadil:7002"]

            partitions:
              - at: 0
                shards:
                - shard: "shard-a"
                  area: "default"
                - shard: "shard-b"
                  area: "fr"
                  top_left: [-5.0, 51.0]
                  bottom_right: [1.25, 44.0]
                - shard: "shard-c"
                  area: "fr"
                  top_left: [1.25, 51.0]
                  bottom_right: [7.5, 44.0]
            """)

            self.assertEqual(self.mixerConfig(), expected)

    def testFourShards(self):
        params = {
            'shards': [
                {
                    'name': 'shard-a',
                    'area': 'default',
                    'hosts': ['gamgee', 'bombadil'],
                    'worker_port': 7000,
                    'mixer_port': 8000
                },
                {
                    'name': 'shard-b',
                    'area': 'fr',
                    'hosts': ['gamgee', 'bombadil'],
                    'worker_port': 7001,
                    'mixer_port': 8001
                },
                {
                    'name': 'shard-c',
                    'area': 'fr',
                    'hosts': ['gamgee', 'bombadil'],
                    'worker_port': 7002,
                    'mixer_port': 8002
                },
                {
                    'name': 'shard-d',
                    'area': 'fr',
                    'hosts': ['gamgee', 'bombadil'],
                    'worker_port': 7003,
                    'mixer_port': 8003
                },
            ],
            'geo': [
                {
                    'area': 'default'
                },
                {
                    'area': "fr",
                    'top_left': [-5.00, 51.00],
                    'bottom_right': [7.50, 44.00]
                }
            ],
            'dest': self._tmp_file,
            'mixer_ip': '10.0.0.1',
            'mixer_port': 8000
        }
        with patch.object(bt_shard, 'AnsibleModule', MakeAnsibleMock(params)):
            bt_shard.run_module()

            expected = textwrap.dedent("""
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
              host: "10.0.0.1"
              port: 8000

            shards:
              - name: "shard-a"
                workers: ["gamgee:7000", "bombadil:7000"]
              - name: "shard-b"
                workers: ["gamgee:7001", "bombadil:7001"]
              - name: "shard-c"
                workers: ["gamgee:7002", "bombadil:7002"]
              - name: "shard-d"
                workers: ["gamgee:7003", "bombadil:7003"]

            partitions:
              - at: 0
                shards:
                - shard: "shard-a"
                  area: "default"
                - shard: "shard-b"
                  area: "fr"
                  top_left: [-5.0, 51.0]
                  bottom_right: [-0.83, 44.0]
                - shard: "shard-c"
                  area: "fr"
                  top_left: [-0.83, 51.0]
                  bottom_right: [3.33, 44.0]
                - shard: "shard-d"
                  area: "fr"
                  top_left: [3.33, 51.0]
                  bottom_right: [7.5, 44.0]
            """)

            self.assertEqual(self.mixerConfig(), expected)


if __name__ == '__main__':
    unittest.main()
