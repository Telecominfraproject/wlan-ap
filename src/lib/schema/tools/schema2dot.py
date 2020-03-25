#!/usr/bin/env python3

import json
import sys
import os

DEFAULT_STRING_LEN = 128            # Default string length, if not a set or map
DEFAULT_MAP_STRING_LEN = 64         # Default string length, if map or set
DEFAULT_MAP_SIZE = 64               # Default map size

class OvsType:
    def __init__(self, obj, default_strlen = DEFAULT_STRING_LEN):
        self.type = "Unknown"
        self.max = 0
        self.min = 0
        self.references = None
        self.refType = 'strong'

        # Check for atomic types
        if isinstance(obj, str):
            self.type = obj
        elif isinstance(obj, dict):
            # Non-atomic type, parse it
            self.type = obj.get('type', None)
            if not self.type:
                raise TypeError('Non-atomic type requires a "key" object.')

            self.references = obj.get('refTable', None);
            self.refType = obj.get('refType', 'strong')

            if self.type == "string":
                self.min = obj.get("minLength", 0)
                self.max = obj.get("maxLength", 0)

            if self.type == "integer":
                self.min = obj.get("minInteger", 0)
                self.max = obj.get("maxInteger", 0)

        elif not isinstance(obj, dict):
            raise TypeError("Unknown OVS type: " + str(obj))


        return

    def pstr(self):
        if self.type == 'string':
            if self.min > 0:
                return "{}[{}..{}]".format(
                        self.type,
                        self.min,
                        self.max if self.max > 0 else "")
            elif self.max > 0:
                return "{}[{}]".format(self.type, self.max)

        elif self.type == 'integer':
            if self.min > 0:
                return "{}({}:{})".format(
                        self.type,
                        self.min,
                        self.max if self.max > 0 else "N")
            elif self.max > 0:
                return "{}({})".format(self.type, self.max)

        return self.type

class OvsColumn:
    def __init__(self, name, obj):
        # Initialize defaults
        self.name = name
        self.min = 1
        self.max = 1
        self.key = None
        self.value = None
        self.mutable = True
        self.references = None
        self.ref_weak = False

        if not 'type' in obj:
            raise TypeError('OVS Column %s requires a "type" object: %s' % (self.name, str(obj)))

        self.__parse_type_field(obj['type'])

        if 'mutable' in obj:
            self.mutable = obj['mutable']

    def __parse_type_field(self, obj):
        default_string_len = DEFAULT_STRING_LEN

        if 'min' in obj:
            self.min = int(obj['min'])

        if 'max' in obj:
            self.max = DEFAULT_MAP_SIZE
            if obj['max'] != "unlimited":
                self.max = int(obj['max'])

            if self.max > 1:
                default_string_len = DEFAULT_MAP_STRING_LEN

        if not isinstance(obj, dict):
            self.key = OvsType(obj, default_strlen = default_string_len)
            return

        if 'key' in obj:
            self.key = OvsType(obj['key'], default_strlen = default_string_len)
        else:
            raise TypeError('OVS Column %s requires a "type/key" object' % (self.name))

        if 'value' in obj:
            self.value = OvsType(obj['value'], default_strlen = default_string_len)

        if self.key and self.key.references:
            self.references = self.key.references
            self.is_weakref = self.key.refType != 'strong'

        if self.value and self.value.references:
            self.references = self.key.references
            self.is_weakref = self.key.refType != 'strong'

    def is_optional(self):
        """
        Return True if the current element is optional
        """

        return self.min == 0

    def is_map(self):
        """
        Return True if current object is a OVS MAP
        """

        if self.value and self.max > 1:
            return True

        return False

    def is_set(self):
        """
        Return True if current object is a OVS SET
        """

        if self.max > 1:
            return True

        return False

    def __str__(self):
        stringify = lambda: "meh"

        # Atomic types
        if self.is_map():
            if self.key.type == "integer":
                type_mod = 'D'
            elif self.key.type == "string":
                type_mod = 'S'
            else:
                raise TypeError("Unsupported MAP type, must be integer or string")

            map_map = \
            {
                "string":   lambda: "PJS_OVS_%sMAP_STRING(%s, %s, %d + 1)"  % (type_mod, self.name, self.value.maxLength, self.max),
                "uuid":     lambda: "PJS_OVS_%sMAP_UUID(%s, %d)"            % (type_mod, self.name, self.max),
                "integer":  lambda: "PJS_OVS_%sMAP_INT(%s, %d)"             % (type_mod, self.name, self.max),
                "boolean":  lambda: "PJS_OVS_%sMAP_BOOL(%s, %d)"            % (type_mod, self.name, self.max),
                "real":     lambda: "PJS_OVS_%sMAP_REAL(%s, %d)"            % (type_mod, self.name, self.max),
            }

            stringify = map_map.get(self.value.type, None)

        elif self.is_set():
            set_map = \
            {
                "string":   lambda: "PJS_OVS_SET_STRING(%s, %s, %d + 1)"    % (self.name, self.key.maxLength, self.max),
                "uuid":     lambda: "PJS_OVS_SET_UUID(%s, %d)"              % (self.name, self.max),
                "integer":  lambda: "PJS_OVS_SET_INT(%s, %d)"               % (self.name, self.max),
                "boolean":  lambda: "PJS_OVS_SET_BOOL(%s, %d)"              % (self.name, self.max),
                "real":     lambda: "PJS_OVS_SET_REAL(%s, %d)"              % (self.name, self.max),
            }

            stringify = set_map.get(self.key.type, None)

        else:
            opt_mod = "_Q" if self.is_optional() else ""

            atomic_map = \
            {
                "string":   lambda: "PJS_OVS_STRING%s(%s, %d + 1)"          % (opt_mod, self.name, self.key.maxLength),
                "uuid":     lambda: "PJS_OVS_UUID%s(%s)"                    % (opt_mod, self.name),
                "integer":  lambda: "PJS_OVS_INT%s(%s)"                     % (opt_mod, self.name),
                "boolean":  lambda: "PJS_OVS_BOOL%s(%s)"                    % (opt_mod, self.name),
                "real":     lambda: "PJS_OVS_REAL%s(%s)"                    % (opt_mod, self.name),
            }

            stringify = atomic_map.get(self.key.type, None)

        if not stringify:
            raise TypeError("Unable to stringify OVsColumn")

        return stringify()



def parse_column_type(obj):

    if isinstance(obj, dict):
        if not 'key' in obj:
            print("Object does not contain required 'key' value:", str(obj))

        if isinstance(obj["key"], dict):
            return obj["key"]["type"]
        else:
            return obj["key"]
    else:
        return obj

def parse_column_value(obj):
    if isinstance(obj, dict):
        if not 'type' in obj:
            print("Object does not contain required 'type' vlaue:", str(obj))

        return obj['type']
    else:
        return obj

def parse_schema_table(table, obj):
    print("#define PJS_SCHEMA_%s \\" % (table))
    print("    PJS(schema_%s, \\" % (table))
    print("        PJS_OVS_UUID_Q(_uuid)\\")
    print("        PJS_OVS_UUID_Q(_version)\\")

    # Check for required fields
    if not "columns" in obj.keys():
        print("Table %s: the filed 'columns' is required." % (table))
        raise

    for column in obj["columns"]:
        parse_schema_table_column(table, column, obj["columns"][column])

    # boom
    print("    )\n")

#
# This function is messy, but so is the standard
#
def parse_schema_table_column(table, column, obj):

    col = OvsColumn(column, obj)

    print("        %s \\" % (col))

def dump_dot(schema):
    print("digraph SCHEMA {")
    refs = []

    for (tname, table) in schema["tables"].items():
        print('{} [\n'
              '    shape=plaintext\n'
              '    label=\n'
              '        <<table border="1" cellborder="0" bgcolor="#a1c1f4">\n'
              '        <tr><td valign="center" align="center" colspan="4" bgcolor="#003384">'
              '             <b><font color="white" point-size="24">{}</font></b>'
              '        </td></tr>'.format(tname, tname))

        for (cname, column) in table["columns"].items():
            cobj = OvsColumn(name = cname, obj = column)
            ctype = cobj.key.pstr()

            lname = cname
            if cobj.is_map():
                lname += "{{{}..{}}}".format(cobj.min, cobj.max)
            elif cobj.is_set():
                lname += "[{}..{}]".format(cobj.min, cobj.max)

            attr = []
            if not cobj.mutable: attr.append("immutable")

            print('        <tr><td port="{}">{}</td><td>{}</td><td>{}</td></tr>'.format(
                    cname,
                    lname,
                    ctype,
                    ",".join(attr)))

            # Remember references
            if cobj.references:
                refs.append( ("{}:{}".format(tname, cname), cobj.references, cobj.is_weakref) )

        print('        </table>>];')

    # Dump references
    print('');
    for ref in refs:
        if ref[2]:
            print('    {} -> {} [style=dashed];'.format(*ref))
        else:
            print('    {} -> {};'.format(*ref))


    print("}")



#
# Main function block
#
if len(sys.argv) < 2:
    print("Not enough parameters.")
    sys.exit(1)

if not os.path.isfile(sys.argv[1]):
    print('Schema file "%s" does not exist.' % (sys.argv[1]))
    sys.exit(1)

with open(sys.argv[1]) as jsfile:
    schema = json.load(jsfile);

# Some rudimentary checks
if not "name" in schema.keys() or schema["name"] != "Open_vSwitch":
    print("Invalid schema file.")
    sys.exit(1)

dump_dot(schema)
