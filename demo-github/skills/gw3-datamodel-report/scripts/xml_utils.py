#!/usr/bin/env python3
"""
Shared XML parsing utilities for TR-181 data-model XML files.

Provides namespace stripping, description extraction, type-string building,
and path normalization — used by step2 (standard Excel), step3 (enhanced XML),
step5 (enhanced Excel), and step4 (JSON conversion).
"""

import re


def strip_ns(tag):
    """Remove the namespace URI from an ElementTree tag.

    >>> strip_ns('{urn:broadband-forum-org:cwmp:datamodel-1-15}model')
    'model'
    """
    if '}' in tag:
        return tag.split('}', 1)[1]
    return tag


def get_description_text(elem):
    """Extract all text from a <description> element, handling mixed content."""
    if elem is None:
        return ''
    parts = list(elem.itertext())
    result = ' '.join(parts)
    result = re.sub(r'\s+', ' ', result).strip()
    result = clean_description(result)
    return result


def clean_description(text):
    """Clean BBF template markers (``{{bibref}}``, ``{{enum}}``, etc.)."""
    # {{bibref|XXX}} -> [XXX]
    text = re.sub(r"\{\{bibref\|([^}]+)\}\}", r"[\1]", text)
    # {{object|XXX}} -> XXX
    text = re.sub(r"\{\{object\|([^}]+)\}\}", r"\1", text)
    # {{param|XXX}} -> XXX
    text = re.sub(r"\{\{param\|([^}]+)\}\}", r"\1", text)
    # {{event|XXX}} -> XXX
    text = re.sub(r"\{\{event\|([^}]+)\}\}", r"\1", text)
    # {{command|XXX}} -> XXX
    text = re.sub(r"\{\{command\|([^}]+)\}\}", r"\1", text)
    # {{datatype|XXX}} -> XXX
    text = re.sub(r"\{\{datatype\|([^}]+)\}\}", r"\1", text)
    # {{enum|XXX|YYY}} -> XXX
    text = re.sub(r"\{\{enum\|([^|}]+)(?:\|[^}]*)?\}\}", r"\1", text)
    # {{pattern|XXX}} -> XXX
    text = re.sub(r"\{\{pattern\|([^}]+)\}\}", r"\1", text)
    # Simple markers
    text = re.sub(r"\{\{noreference\}\}", "", text)
    text = re.sub(r"\{\{empty\}\}", "<Empty>", text)
    text = re.sub(r"\{\{true\}\}", "true", text)
    text = re.sub(r"\{\{false\}\}", "false", text)
    text = re.sub(r"\{\{units\}\}", "units", text)
    text = re.sub(r"\{\{numentries\}\}", "The number of entries in the corresponding table.", text)
    text = re.sub(r"\{\{enum\}\}", "Enumeration of:", text)
    text = re.sub(r"\{\{list\}\}", "Comma-separated list.", text)
    text = re.sub(r"\{\{hidden\}\}", "[hidden]", text)
    text = re.sub(r"\{\{secured\}\}", "[secured]", text)
    # {{deprecated|...}}, {{obsoleted|...}}
    text = re.sub(r"\{\{deprecated\|([^}]*)\}\}", r"DEPRECATED: \1", text)
    text = re.sub(r"\{\{obsoleted\|([^}]*)\}\}", r"OBSOLETED: \1", text)
    # Any remaining {{...}}
    text = re.sub(r"\{\{([^}]*)\}\}", r"\1", text)
    # Cleanup
    text = re.sub(r'  +', ' ', text)
    text = re.sub(r"''([^']+)''", r"\1", text)
    return text.strip()


def build_type_string(syntax_elem):
    """Build the display type string from a <syntax> element.

    Handles base types, dataType references, size/range constraints, and list prefixes.
    """
    if syntax_elem is None:
        return ''

    children = list(syntax_elem)
    if not children:
        return ''

    type_str = ''
    list_prefix = False

    for child in children:
        tag = strip_ns(child.tag)

        if tag == 'list':
            list_prefix = True
            continue

        if tag in ('string', 'unsignedInt', 'unsignedLong', 'int', 'long',
                   'boolean', 'dateTime', 'hexBinary', 'base64', 'object'):
            type_str = tag
            for sc in child:
                sctag = strip_ns(sc.tag)
                if sctag == 'size':
                    min_len = sc.attrib.get('minLength', '')
                    max_len = sc.attrib.get('maxLength', '')
                    if max_len:
                        if min_len and min_len != '0':
                            type_str += f'({min_len}:{max_len})'
                        else:
                            type_str += f'(:{max_len})'
                elif sctag == 'range':
                    min_val = sc.attrib.get('minInclusive', '')
                    max_val = sc.attrib.get('maxInclusive', '')
                    step = sc.attrib.get('step', '')
                    range_parts = []
                    if min_val:
                        range_parts.append(min_val)
                    if max_val:
                        if not range_parts:
                            range_parts.append('')
                        range_parts.append(max_val)
                    if range_parts:
                        range_str = ':'.join(range_parts)
                        if step:
                            range_str += f' step {step}'
                        type_str += f'({range_str})'

        elif tag == 'dataType':
            ref = child.attrib.get('ref', '')
            type_str = ref if ref else 'dataType'

        # Skip tags that don't affect the type string
        elif tag in ('default', 'pathRef', 'instanceRef', 'enumerationRef',
                     'units', 'pattern', 'enumeration', 'size', 'range'):
            continue

    if list_prefix and type_str:
        type_str += '[]'

    return type_str


def get_syntax_type(param_elem):
    """Shorthand: get the display type string for a <parameter> element."""
    syntax = param_elem.find('syntax')
    if syntax is None:
        return ''
    return build_type_string(syntax)


def get_default_value(param_elem):
    """Extract the default value from a <parameter> element's <syntax>."""
    syntax = param_elem.find('syntax')
    if syntax is not None:
        for elem in syntax.iter():
            if strip_ns(elem.tag) == 'default':
                dtype = elem.attrib.get('type', '')
                value = elem.attrib.get('value', '')
                if dtype == 'object':
                    return value if value else '-'
                if value == '':
                    return '<Empty>'
                return value
    return '-'


def normalize_path(path):
    """Normalize a TR-181 path for matching by removing ``{i}`` placeholders.

    >>> normalize_path('Device.IP.Interface.{i}.IPv4Address.{i}.')
    'Device.IP.Interface.IPv4Address.'
    """
    if not path:
        return ""
    return re.sub(r'\.\{i\}', '', path)
