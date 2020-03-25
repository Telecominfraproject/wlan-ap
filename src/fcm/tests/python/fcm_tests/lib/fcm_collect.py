from ast import literal_eval
import os
import sys
dir_path = os.path.dirname(os.path.realpath(__file__))
sys.path.append(dir_path)
import utils.utils as utils

class FcmPluginEntry(utils.ConfEntry):
    def __init__(self, plugin_name, config):
        utils.ConfEntry.__init__(self, plugin_name, config)
        self.components = [
            'name',
            'interval',
            'report_name'
        ]
        for c in self.components:
            try:
                self.__dict__[c] = self.config[plugin_name][c]
            except KeyError:
                raise utils.ConfOptionError(
                    "{} is missing {} field".format(plugin_name, c))
            if not self.__dict__[c]:
                raise utils.ConfOptionError(
                    "{}: {} has no value set".format(plugin_name, c))
            if c == 'other_config':
                try:
                    self.__dict__[c] = literal_eval("{}".format(
                        self.config[plugin_name][c]))
                except:
                    raise utils.ConfFormatError(
                        "{}: {} value has wrong format".format(plugin_name,
                                                               c,
                                                               self.__dict__[c]))
        self.table = 'FCM_Collector_Config'
        self.insert_cmd = []
        self.gen_insert_cmd()
        self.delete_cmd = []
        self.gen_delete_cmd()

    def gen_delete_cmd(self):
        cmd = "/usr/plume/tools/ovsh d {} -w ".format(self.table)
        cmd += "name=={}".format(self.name)
        self.delete_cmd.append(cmd)

    def gen_insert_cmd(self):
        cmd = "/usr/plume/tools/ovsh i {}".format(self.table)
        for c in self.components:
            if c == 'other_config':
                cmd += ' {}:={}'.format(c,
                                        self.prepare_json_map(self.other_config))
            else:
                cmd += ' {}:="{}"'.format(c,self.__dict__[c])
        self.insert_cmd.append(cmd)


class FcmConfEntry(utils.ConfEntry):
    def __init__(self, config):
        utils.ConfEntry.__init__(self, 'fcm_collect', config)
        self.config = config
        self.get_plugins()

    def get_plugins(self):
        for p in self.objects:
            plugin = FcmPluginEntry(p, self.config)
            self.nodes.append(plugin)

