#!/usr/bin/env python3

import configparser
import importlib
from importlib import util as importlib_util
import os
import tempfile
import unittest

class TestConf:
    def __init__(self, dict_sections):
        self.config = configparser.ConfigParser()
        for k,v in dict_sections.items():
            self.config[k] = v
        with open('/tmp/test.conf', 'w') as f:
            self.config.write(f)
        with open('/tmp/test.conf', 'r') as f:
            self.config.read_file(f)

class Modules:
    def __init__(self):
        self.device = self.load_lib('device.py')
        self.fsm_conf = self.load_lib('fsm_config.py')
        self.tags = self.load_lib('tags.py')
        self.flows = self.load_lib('flows.py')
        self.taps = self.load_lib('iface.py')

    def load_lib(self, modname):
        dir_path = os.path.dirname(os.path.realpath(__file__))
        lib_path = os.path.realpath(dir_path + '/../lib/')
        modfile = os.path.join(lib_path, modname)
        modfile = os.path.realpath(modfile)
        spec = importlib_util.spec_from_file_location("lib.device", modfile)
        module = importlib_util.module_from_spec(spec)
        spec.loader.exec_module(module)
        return module

testDict = {
    'fsm_plugins': {
        'plugin_test_foo': '', 'plugin_test_bar': ''
    },
    'plugin_test_foo': {
        'handler': 'dev_foo',
        'if_name': 'br-home.foo',
        'pkt_capt_filter': 'udp port 12345',
        'plugin': '/tmp/libfsm_foo.so',
        'other_config': {
            'mqtt_v': 'foo_mqtt_v',
            'dso_init': 'fsm_foo_init'
        }
    },
    'plugin_test_bar': {
        'handler': 'dev_bar',
        'if_name': 'br-home.bar',
        'pkt_capt_filter': 'tcp port 54321',
        'plugin': '/tmp/libfsm_bar.so',
        'other_config': {
            'mqtt_v': 'bar_mqtt_v',
            'dso_init': 'fsm_bar_init'
        }
    },
    'openflow_tags': {
        'tag_foo': '', 'tag_bar': ''
    },
    'tag_foo': {
        'name': 'dev_tag_foo',
        'cloud_value': ['aa:bb:cc:dd:ee:ff', '11:22:33:44:55:66'],
        'device_value': []
    },
    'tag_bar': {
        'name': 'dev_tag_bar',
        'cloud_value': [],
        'device_value': ['de:ad:be:ef:00:11', '66:55:44:33:22:11']
    },
    'openflow_config': {
        'flow_foo': '', 'flow_bar': ''
    },
    'flow_foo': {
        'token': 'dev_flow_foo',
        'bridge': 'br-home',
        'table': '0',
        'priority': '200',
        'rule': 'dl_src=${dev_tag_foo},udp,tp_dst=12345',
        'action': 'normal,output:601'
    },
    'flow_bar': {
        'token': 'dev_flow_bar',
        'bridge': 'br-home',
        'table': '0',
        'priority': '250',
        'rule': 'dl_src=\${dev_tag_bar},tcp,tp_dst=54321',
        'action': 'normal,output:602'
    },
    'tap_interfaces': {
        'tap_foo': '', 'tap_bar': ''
    },
    'tap_foo': {
        'if_name': 'br-home.foo',
        'bridge': 'br-home',
        'of_port': '1001'
    },
    'tap_bar': {
        'if_name': 'br-home.bar',
        'bridge': 'br-home',
        'of_port': '1002'
    }
}

class TestDevice(unittest.TestCase):
    def setUp(self):
        self.modules = Modules()

    # Test a valid mac address with upper case characters
    def test_valid_uppercase(self):
        mac = "AA:BB:CC:DD:EE:FF"
        device = self.modules.device.Device(mac)
        self.assertTrue(device.valid)

    # Test an invalid mac address (one character is an 'X')
    def test_invalid_uppercase(self):
        mac = "AA:BB:CC:DD:EE:FX"
        device = self.modules.device.Device(mac)
        self.assertFalse(device.valid)

    # Test a valid mac address with a mix of upper and lowe case
    def test_valid_mix(self):
        mac = "aA:b1:C2:3d:4e:f5"
        device = self.modules.device.Device(mac)
        self.assertTrue(device.valid)

class TestFsmConf(unittest.TestCase):
    def setUp(self):
        self.modules = Modules()
        self.conf = TestConf(testDict)

    def test_fsm_plugins_config(self):
        plugins = self.modules.fsm_conf.FsmConfEntry(self.conf.config)

        self.assertTrue(1 == 1)

class TestTags(unittest.TestCase):
    def setUp(self):
        self.modules = Modules()
        self.conf = TestConf(testDict)

    def test_fsm_tags_config(self):
        tags = self.modules.tags.TagsConfEntry(self.conf.config)

        self.assertTrue(1 == 1)

class TestOpenflows(unittest.TestCase):
    def setUp(self):
        self.modules = Modules()
        self.conf = TestConf(testDict)

    def test_fsm_flows_config(self):
        flows = self.modules.flows.OpenflowConfEntry(self.conf.config)

        self.assertTrue(1 == 1)

class TestTapInterfaces(unittest.TestCase):
    def setUp(self):
        self.modules = Modules()
        self.conf = TestConf(testDict)

    def test_fsm_tap_config(self):
        taps = self.modules.taps.TapConfEntry(self.conf.config)

        self.assertTrue(1 == 1)
if __name__ == '__main__':
    unittest.main()
