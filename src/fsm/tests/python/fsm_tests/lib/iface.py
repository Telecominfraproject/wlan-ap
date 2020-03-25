from ast import literal_eval
import os
import sys
dir_path = os.path.dirname(os.path.realpath(__file__))
sys.path.append(dir_path)
import utils.utils as utils

class TapEntry(utils.ConfEntry):
    def __init__(self, iface_name, config):
        utils.ConfEntry.__init__(self, iface_name, config)
        self.components = [
            'if_name',
            'bridge',
            'of_port'
        ]

        for c in self.components:
            try:
                self.__dict__[c] = self.config[iface_name][c]
            except KeyError:
                raise utils.ConfOptionError(
                    "{} is missing {} field".format(iface_name, c))
            if not self.__dict__[c]:
                raise utils.ConfOptionError(
                    "{}: {} has no value set".format(plugin_name, c))

        self.insert_cmd = []
        self.gen_insert_cmd()
        self.delete_cmd = []
        self.gen_delete_cmd()

    def gen_delete_cmd(self):
        cmd = '/usr/bin/ovs-vsctl del-port {} {}'.format(self.bridge,
                                                         self.if_name)
        self.delete_cmd.append(cmd)

    def gen_insert_cmd(self):
        cmd = '/usr/bin/ovs-vsctl add-port {} {} '.format(self.bridge,
                                                          self.if_name)
        cmd += ' -- set interface {} '.format(self.if_name)
        cmd += ' type=internal '
        cmd += '-- set interface {} '.format(self.if_name)
        cmd += ' ofport_request={}'.format(self.of_port)
        self.insert_cmd.append(cmd)
        cmd = '/usr/sbin/ip link set {} up'.format(self.if_name)
        self.insert_cmd.append(cmd)

class TapConfEntry(utils.ConfEntry):
    def __init__(self, config):
        utils.ConfEntry.__init__(self, 'tap_interfaces', config)
        self.config = config
        self.get_taps()

    def get_taps(self):
        for p in self.objects:
            tap = TapEntry(p, self.config)
            self.nodes.append(tap)
