class ConfException(Exception):
    pass

class ConfSectionError(ConfException):
    pass

class ConfOptionError(ConfException):
    pass

class ConfFormatError(ConfException):
    pass

class ConfEntry:
    def __init__(self, config_section, config):
        self.config = config
        self.config_section = config_section
        self.nodes = []
        if self.config_section not in config.sections():
            raise ConfSectionError(
                "{} section is missing".format(config_section))
        self.objects = [option for option in config[self.config_section]]

    # outputs a "[\"set\",[\"str1\",\"str2\",...]]" string
    # or a "[\"set\",[int1,int2,...]]" string.
    # used to push a set through ovsh
    def prepare_json_set(self, elements):
        set_str = '"[\\"set\\",['
        # Check the elements type
        ints = isinstance(elements[0], int)
        if ints:
            set_str += "{}".format(elements[0])
        else:
            set_str += '\\"{}\\"'.format(elements[0])

        for e in elements[1:]:
            if ints:
                set_str += ",{}".format(e)
            else:
                set_str += ',\\"{}\\"'.format(e)

        set_str += ']]"'
        return set_str

    def prepare_json_map(self, dict_map):
        first = True
        map_str = '"[\\"map\\",['
        for k,v in dict_map.items():
            comma = ','
            if first:
                comma = ''
                first = False
            kv_str = '{}[\\"{}\\",\\"{}\\"]'.format(comma, k, v)
            map_str += kv_str
        map_str += ']]"'
        return map_str

    def get_delete_cmds(self):
        del_cmd = []
        for node in self.nodes:
            del_cmd += node.delete_cmd
        return del_cmd

    def get_insert_cmds(self):
        insert_cmd = []
        for node in self.nodes:
            insert_cmd += node.insert_cmd
        return insert_cmd
