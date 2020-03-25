from ast import literal_eval
import os
import sys
dir_path = os.path.dirname(os.path.realpath(__file__))
sys.path.append(dir_path)
import utils.utils as utils

class TagEntry(utils.ConfEntry):
    def __init__(self, tag_name, config):
        utils.ConfEntry.__init__(self, tag_name, config)
        self.components = [
            'name',
            'cloud_value',
            'device_value'
        ]
        for c in self.components:
            try:
                self.__dict__[c] = self.config[tag_name][c]
            except KeyError:
                raise utils.ConfOptionError(
                    "{} is missing {} field".format(tag_name, c))
            if c == 'name' and not self.__dict__[c]:
                raise utils.ConfOptionError(
                    "{}: {} has no value set".format(tag_name, c))
            if c != 'name':
                self.__dict__[c] = literal_eval("{}".format(
                    self.config[tag_name][c]))

        self.table = 'Openflow_Tag'
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
            if c == 'name':
                cmd += ' {}:="{}"'.format(c,self.__dict__[c])
            elif self.__dict__[c]:
                cmd += ' {}:={}'.format(c,
                                        self.prepare_json_set(self.__dict__[c]))
        self.insert_cmd.append(cmd)


class TagsConfEntry(utils.ConfEntry):
    def __init__(self, config):
        utils.ConfEntry.__init__(self, 'openflow_tags', config)
        self.config = config
        self.get_tags()

    def get_tags(self):
        for p in self.objects:
            tag = TagEntry(p, self.config)
            self.nodes.append(tag)
