from ast import literal_eval
import os
import sys
dir_path = os.path.dirname(os.path.realpath(__file__))
sys.path.append(dir_path)
import utils.utils as utils

class OpenflowEntry(utils.ConfEntry):
    def __init__(self, openflow_name, config):
        utils.ConfEntry.__init__(self, openflow_name, config)
        self.components = [
            'token',
            'bridge',
            'table',
            'priority',
            'rule',
            'action'
        ]
        for c in self.components:
            try:
                self.__dict__[c] = self.config[openflow_name][c]
            except KeyError:
                raise utils.ConfOptionError(
                    "{} is missing {} field".format(openflow_name, c))

        self.ovs_table = 'Openflow_Config'
        self.insert_cmd = []
        self.gen_insert_cmd()
        self.delete_cmd = []
        self.gen_delete_cmd()

    def gen_delete_cmd(self):
        cmd = "/usr/plume/tools/ovsh d {} -w ".format(self.ovs_table)
        cmd += "token=={}".format(self.token)
        self.delete_cmd.append(cmd)

    def gen_insert_cmd(self):
        cmd = "/usr/plume/tools/ovsh i {}".format(self.ovs_table)
        for c in self.components:
            cmd += ' {}:="{}"'.format(c,self.__dict__[c])
        self.insert_cmd.append(cmd)


class OpenflowConfEntry(utils.ConfEntry):
    def __init__(self, config):
        utils.ConfEntry.__init__(self, 'openflow_config', config)
        self.config = config
        self.get_oflows()

    def get_oflows(self):
        for p in self.objects:
            oflow = OpenflowEntry(p, self.config)
            self.nodes.append(oflow)
