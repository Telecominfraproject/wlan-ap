#!/usr/bin/env python3
"""
Step 4: Convert XML files to JSON.

Parses both the original TR-181 XML and the enhanced (annotated) XML and
produces hierarchical JSON representations preserving all fields.

Output:
    local/datamodel/tr181_original.json
    local/datamodel/gw3_current_datamodel.json
"""

import argparse
import json
import os
import sys
import xml.etree.ElementTree as ET

from config import LOCAL_DATAMODEL_DIR, GW3_NAMESPACE, ensure_output_dir
from xml_utils import strip_ns, get_description_text, get_syntax_type, get_default_value


GW3_NS = f'{{{GW3_NAMESPACE}}}'


def element_to_dict(elem, include_gw3=False):
    """Convert an XML element to a dictionary recursively.

    For data-model elements (object, parameter, command, event), extracts
    structured fields.  For other elements, does generic conversion.
    """
    tag = strip_ns(elem.tag)
    result = {'_tag': tag}

    # Copy all attributes
    for key, val in elem.attrib.items():
        if key.startswith(GW3_NS) and include_gw3:
            attr_name = 'gw3_' + key[len(GW3_NS):]
            result[attr_name] = val
        elif not key.startswith('{'):
            result[key] = val

    # Extract description text if present
    desc_elem = elem.find('description')
    if desc_elem is not None:
        result['description'] = get_description_text(desc_elem)

    return result


def process_model(xml_path, include_gw3=False):
    """Parse a TR-181 XML and return a structured dict of the model."""
    tree = ET.parse(xml_path)
    root = tree.getroot()

    # Find the model element
    model = None
    for child in root:
        if strip_ns(child.tag) == 'model':
            model = child
    if model is None:
        print(f"ERROR: No <model> element in {xml_path}", file=sys.stderr)
        sys.exit(1)

    model_info = {
        'model_name': model.attrib.get('name', 'Device:2'),
        'objects': [],
    }

    for obj in model:
        if strip_ns(obj.tag) != 'object':
            continue

        obj_dict = element_to_dict(obj, include_gw3)
        obj_dict['parameters'] = []
        obj_dict['commands'] = []
        obj_dict['events'] = []

        for child in obj:
            ctag = strip_ns(child.tag)
            if ctag == 'parameter':
                p = element_to_dict(child, include_gw3)
                p['type'] = get_syntax_type(child)
                p['default'] = get_default_value(child)
                obj_dict['parameters'].append(p)
            elif ctag == 'command':
                c = element_to_dict(child, include_gw3)
                c['input_args'] = _extract_args(child.find('input'), include_gw3)
                c['output_args'] = _extract_args(child.find('output'), include_gw3)
                obj_dict['commands'].append(c)
            elif ctag == 'event':
                e = element_to_dict(child, include_gw3)
                e['args'] = []
                for ec in child:
                    if strip_ns(ec.tag) == 'parameter':
                        e['args'].append(element_to_dict(ec, include_gw3))
                obj_dict['events'].append(e)

        model_info['objects'].append(obj_dict)

    return model_info


def _extract_args(section_elem, include_gw3):
    """Extract argument parameters from an input/output section."""
    if section_elem is None:
        return []
    args = []
    for child in section_elem:
        ctag = strip_ns(child.tag)
        if ctag == 'parameter':
            args.append(element_to_dict(child, include_gw3))
        elif ctag == 'object':
            obj_arg = element_to_dict(child, include_gw3)
            obj_arg['parameters'] = []
            for p in child:
                if strip_ns(p.tag) == 'parameter':
                    obj_arg['parameters'].append(element_to_dict(p, include_gw3))
            args.append(obj_arg)
    return args


def main():
    parser = argparse.ArgumentParser(description='Convert TR-181 XML to JSON')
    parser.add_argument('--output-dir', '-o', default=LOCAL_DATAMODEL_DIR)
    args = parser.parse_args()

    ensure_output_dir(args.output_dir)

    # Process original XML
    original_xml = None
    enhanced_xml = None
    for fname in sorted(os.listdir(LOCAL_DATAMODEL_DIR), reverse=True):
        if fname.startswith('tr-181-') and fname.endswith('-usp-full.xml') and 'gw3' not in fname:
            original_xml = os.path.join(LOCAL_DATAMODEL_DIR, fname)
            break
    enhanced_xml_path = os.path.join(LOCAL_DATAMODEL_DIR, 'gw3_current_datamodel.xml')
    if os.path.exists(enhanced_xml_path):
        enhanced_xml = enhanced_xml_path

    if original_xml:
        print(f"Processing original XML: {original_xml}")
        original_data = process_model(original_xml, include_gw3=False)
        out_path = os.path.join(args.output_dir, 'tr181_original.json')
        with open(out_path, 'w') as f:
            json.dump(original_data, f, indent=2)
        print(f"  Saved: {out_path} ({len(original_data['objects'])} objects)")
    else:
        print("WARNING: No original TR-181 XML found. Run step1 first.", file=sys.stderr)

    if enhanced_xml:
        print(f"Processing enhanced XML: {enhanced_xml}")
        enhanced_data = process_model(enhanced_xml, include_gw3=True)
        out_path = os.path.join(args.output_dir, 'gw3_current_datamodel.json')
        with open(out_path, 'w') as f:
            json.dump(enhanced_data, f, indent=2)
        print(f"  Saved: {out_path} ({len(enhanced_data['objects'])} objects)")
    else:
        print("WARNING: No enhanced XML found. Run step3 first.", file=sys.stderr)


if __name__ == '__main__':
    main()
