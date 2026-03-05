#!/usr/bin/env python3
"""
Step 3: Generate enhanced XML by cross-referencing TR-181 with ODL implementation.

Reads the original TR-181 full XML, runs the ODL parser + service map,
and annotates every element with ``gw3:`` attributes indicating coverage status.

GW3 annotation attributes:
    gw3:Microservice          — service name (e.g. "ip-manager")
    gw3:Microservice_version  — package version (from tmp/.packageinfo)
    gw3:Mark                  — IMPLEMENTED / NOT IMPLEMENTED / NOT DEFINED / MISMATCH / VENDOR EXTENSION / PRPL EXTENSION
    gw3:ODL_Type              — ODL data type
    gw3:ODL_Access            — readOnly / readWrite
    gw3:ODL_Default           — default value from ODL
    gw3:ODL_Source_File       — ODL source filename
    gw3:DIFF_Details          — mismatch description

Mark semantics:
    NOT DEFINED      = in TR-181 spec but no ODL definition at all
    NOT IMPLEMENTED  = defined in ODL but not actively served (no callbacks, no confirmed C write)
    DEFAULT ONLY     = readOnly param in ODL, no action callbacks, no confirmed C write
    REPORT ONLY      = readWrite param in ODL, no action callbacks, but confirmed C write
    IMPLEMENTED      = has action callbacks, or readOnly param with confirmed C write
    MISMATCH         = both exist but type/access differs
    VENDOR EXTENSION = in ODL but not in TR-181 (X_ prefix paths)
    PRPL EXTENSION   = in ODL but not in TR-181 (non-X_ paths, prplOS additions)

Output:
    local/datamodel/gw3_current_datamodel.xml
"""

import argparse
import os
import re
import sys
import xml.etree.ElementTree as ET
from copy import deepcopy

from config import (
    LOCAL_DATAMODEL_DIR, GW3_NAMESPACE, GW3_NS_PREFIX, VARIABLES,
    PROJECT_ROOT, staging_amx_path, build_dir_path, ensure_output_dir,
)
from xml_utils import strip_ns, normalize_path, get_syntax_type
from odl_parser import parse_all_services, ODL_TYPE_MAP
from service_map import build_service_map


def scan_emitted_events(bdir):
    """Scan C source files in the build directory for amxd_object_emit_signal() calls.

    Extracts event names (string literals ending with ``!``) from production code,
    excluding test directories and library test files.

    Returns:
        set: Event names found in emit_signal calls, e.g. ``{"Boot!", "Neighbor!"}``
    """
    if not os.path.isdir(bdir):
        return set()

    # Pattern: amxd_object_emit_signal(obj, "EventName!", ...)
    pattern = re.compile(r'amxd_object_emit_signal\s*\([^,]+,\s*"([^"]+!)"')

    emitted = set()

    for dirpath, dirnames, filenames in os.walk(bdir):
        # Skip test directories
        rel = os.path.relpath(dirpath, bdir)
        parts = rel.split(os.sep)
        if any(p in ('test', 'tests', 'mock', 'mocks') for p in parts):
            continue

        for fname in filenames:
            if not fname.endswith('.c'):
                continue
            filepath = os.path.join(dirpath, fname)
            try:
                with open(filepath, 'r', errors='replace') as f:
                    content = f.read()
            except OSError:
                continue

            for m in pattern.finditer(content):
                event_name = m.group(1)
                emitted.add(event_name)

    return emitted


def promote_emitted_events(odl_index, emitted_events):
    """Promote ODL event entries to IMPLEMENTED if the event name appears in C source.

    For each emitted event name, find matching entries in the ODL index where
    ``entry_type == "event"`` and ``impl_level != "IMPLEMENTED"``, and promote them.

    Returns:
        int: Number of entries promoted.
    """
    promoted = 0
    for path, entry in odl_index.items():
        if entry.get("entry_type") != "event":
            continue
        if entry.get("impl_level") == "IMPLEMENTED":
            continue
        # Match event name: last component of device_path
        dp = entry.get("device_path", "")
        # Event paths look like "Device.DeviceInfo.Boot!" or just end with the event name
        event_name = dp.rsplit('.', 1)[-1] if '.' in dp else dp
        if not event_name:
            # Try the "name" field directly
            event_name = entry.get("name", "")
        if event_name in emitted_events:
            entry["impl_level"] = "IMPLEMENTED"
            entry["has_actions"] = True
            promoted += 1
    return promoted


def scan_written_params(bdir):
    """Scan C source files in the build directory for parameter write calls.

    Extracts parameter names (string literals) from ``amxd_trans_set_value()``
    and ``amxd_object_set_value()`` calls in production code, grouped by
    service package directory.

    Returns:
        dict: ``{service_name: set(param_names)}`` e.g.
              ``{"ip-manager": {"Status", "IPAddress", ...}}``
    """
    if not os.path.isdir(bdir):
        return {}

    # amxd_trans_set_value(type, &trans, "ParamName", value)
    # amxd_object_set_value(type, obj, "ParamName", value)
    patterns = [
        re.compile(r'amxd_trans_set_value\s*\([^,]+,\s*[^,]+,\s*"([^"]+)"'),
        re.compile(r'amxd_object_set_value\s*\([^,]+,\s*[^,]+,\s*"([^"]+)"'),
    ]

    result = {}

    for entry in os.listdir(bdir):
        pkg_dir = os.path.join(bdir, entry)
        if not os.path.isdir(pkg_dir):
            continue

        # Extract service name from directory: e.g. "tr181-ip-v1.51.0" -> look inside
        params = set()

        for dirpath, dirnames, filenames in os.walk(pkg_dir):
            # Skip test directories
            rel = os.path.relpath(dirpath, pkg_dir)
            parts = rel.split(os.sep)
            if any(p in ('test', 'tests', 'mock', 'mocks') for p in parts):
                continue

            for fname in filenames:
                if not fname.endswith('.c'):
                    continue
                filepath = os.path.join(dirpath, fname)
                try:
                    with open(filepath, 'r', errors='replace') as f:
                        content = f.read()
                except OSError:
                    continue

                for pat in patterns:
                    for m in pat.finditer(content):
                        params.add(m.group(1))

        if params:
            result[entry] = params

    return result


def promote_written_params(odl_index, written_params):
    """Promote ODL params based on confirmed C source writes.

    For each ODL parameter entry that is NOT IMPLEMENTED, check whether the
    parameter name appears in the C source write calls for the same service.

    - readOnly params with confirmed C write → promoted to IMPLEMENTED
    - readWrite params with confirmed C write → flagged with ``c_write_confirmed``
      (used by ``determine_mark()`` to assign REPORT ONLY)

    Returns:
        tuple: (promoted_readonly, flagged_readwrite) counts.
    """
    svc_params = {}
    for pkg_dir, params in written_params.items():
        svc_params[pkg_dir] = params

    promoted_ro = 0
    flagged_rw = 0
    for path, entry in odl_index.items():
        if entry.get("entry_type") != "parameter":
            continue
        if entry.get("impl_level") == "IMPLEMENTED":
            continue

        param_name = entry.get("name", "")
        service = entry.get("service", "")
        if not param_name or not service:
            continue

        access = entry.get("access", "")

        # Check if any build dir package has this param in its C writes
        svc_hyphen = service.replace('_', '-')
        for pkg_dir, params in svc_params.items():
            pkg_lower = pkg_dir.lower()
            if svc_hyphen in pkg_lower or service in pkg_lower:
                if param_name in params:
                    if access == "readOnly":
                        entry["impl_level"] = "IMPLEMENTED"
                        promoted_ro += 1
                    else:
                        entry["c_write_confirmed"] = True
                        flagged_rw += 1
                    break

    return promoted_ro, flagged_rw


def build_odl_index(odl_entries):
    """Build a lookup index from ODL entries, keyed by normalized Device.* path."""
    by_path = {}
    for entry in odl_entries:
        dp = entry["device_path"]
        norm = normalize_path(dp)
        if norm not in by_path:
            by_path[norm] = entry
        else:
            existing = by_path[norm]
            if entry["impl_level"] == "IMPLEMENTED" and existing["impl_level"] != "IMPLEMENTED":
                by_path[norm] = entry
    return by_path


def parse_proxy_mappings(base_dir):
    """Parse proxy-object mapping files from the AMX staging directory.

    Scans tr181-device/extensions/ and all service directories for
    ``*_mapping.odl`` files containing ``proxy-object`` directives.

    Returns:
        dict: ``{device_prefix: local_prefix}`` e.g.
              ``{"Device.DeviceInfo.PowerStatus.": "PowerStatus."}``
    """
    mappings = {}

    # Pattern: %global "proxy-object.'Device.X.'" = "Y.";
    pattern = re.compile(
        r'''%global\s+"proxy-object\.'([^']+)'"\s*=\s*"([^"]*)"''',
    )

    # Collect all *_mapping.odl files
    mapping_files = []
    for dirpath, _dirnames, filenames in os.walk(base_dir):
        for fname in filenames:
            if fname.endswith('_mapping.odl'):
                mapping_files.append(os.path.join(dirpath, fname))

    for fpath in sorted(mapping_files):
        try:
            with open(fpath, 'r') as f:
                content = f.read()
        except OSError:
            continue

        if 'proxy-object' not in content:
            continue

        # Substitute known variables
        for var, val in VARIABLES.items():
            content = content.replace(f'${{{var}}}', val)

        for m in pattern.finditer(content):
            device_prefix = m.group(1)   # e.g. "Device.DeviceInfo.PowerStatus."
            local_prefix = m.group(2)    # e.g. "PowerStatus."
            # Skip root mapping (empty local prefix)
            if not local_prefix:
                continue
            mappings[device_prefix] = local_prefix

    return mappings


def augment_odl_index_with_proxy_mappings(odl_index, proxy_mappings, service_map):
    """Add proxy-mapped aliases to the ODL index.

    For each proxy mapping ``Device.X. → Y.``, find ODL entries by checking:

    1. Entries at ``Device.Y.`` (the fallback path the parser creates for
       services without USP translate files).
    2. Entries at the USP-translated path (e.g. ``Device.X_PRPLWARE-COM_SFPs.``
       for SFPs, where the service has its own USP translate mapping).

    Returns the number of new aliases added.
    """
    # Build reverse lookup: internal_prefix → device_prefix (from USP translate)
    usp_translations = {}
    for _svc, mappings in service_map.items():
        for internal, device in mappings:
            usp_translations[internal] = device

    new_entries = {}

    for device_prefix, local_prefix in proxy_mappings.items():
        # Candidate prefixes where ODL entries might be indexed
        prefixes_to_check = []

        # Case 1: Fallback — parser prepends "Device." to internal paths
        fallback_prefix = "Device." + local_prefix
        prefixes_to_check.append(fallback_prefix)

        # Case 2: USP translate maps internal path to a different Device.* path
        usp_device = usp_translations.get(local_prefix)
        if usp_device and usp_device != device_prefix:
            prefixes_to_check.append(usp_device)

        for check_prefix in prefixes_to_check:
            for path, entry in list(odl_index.items()):
                if not path.startswith(check_prefix):
                    continue
                # Build the aliased path under the real Device.* tree
                aliased_path = device_prefix + path[len(check_prefix):]
                aliased_norm = normalize_path(aliased_path)
                if aliased_norm not in odl_index and aliased_norm not in new_entries:
                    aliased_entry = dict(entry)
                    aliased_entry["device_path"] = aliased_path
                    new_entries[aliased_norm] = aliased_entry

    odl_index.update(new_entries)
    return len(new_entries)


def lookup_odl(odl_index, tr181_path):
    """Look up a TR-181 path in the ODL index, trying several normalizations."""
    norm = normalize_path(tr181_path)

    if norm in odl_index:
        return odl_index[norm]

    # Try without trailing dot
    if norm.endswith('.'):
        no_dot = norm.rstrip('.')
        if no_dot in odl_index:
            return odl_index[no_dot]

    # Try with trailing dot
    if not norm.endswith('.'):
        with_dot = norm + '.'
        if with_dot in odl_index:
            return odl_index[with_dot]

    # Try ancestor paths for leaf items (events/commands/params).
    # ODL may declare an item on a parent object while TR-181 XML nests it
    # under a child object (e.g., TemperatureStatus.HighTemperatureAlarm!
    # vs TemperatureSensor.{i}.HighTemperatureAlarm!).
    if not norm.endswith('.'):
        leaf = norm.rsplit('.', 1)[-1]
        parts = norm.split('.')
        # Walk up ancestor chain, trying each parent + leaf
        for i in range(len(parts) - 2, 0, -1):
            ancestor = '.'.join(parts[:i]) + '.' + leaf
            if ancestor in odl_index:
                return odl_index[ancestor]

    return None


def determine_mark(elem_tag, tr181_access, odl_entry):
    """Determine the mark (coverage status) for a TR-181 element."""
    if odl_entry is None:
        return "NOT DEFINED", ""

    impl_level = odl_entry.get("impl_level", "NOT IMPLEMENTED")

    # Check for type/access mismatches
    mismatches = []
    odl_access = odl_entry.get("access", "")

    if tr181_access == 'readWrite' and odl_access == 'readOnly':
        mismatches.append("access:spec_RW_odl_RO")
    elif tr181_access == 'readOnly' and odl_access == 'readWrite':
        mismatches.append("access:spec_RO_odl_RW")

    if mismatches:
        return "MISMATCH", "; ".join(mismatches)

    if impl_level == "IMPLEMENTED":
        return "IMPLEMENTED", ""

    entry_type = odl_entry.get("entry_type", "")
    odl_access = odl_entry.get("access", "")

    # readOnly params without callbacks and no confirmed C write:
    # the ODL defines the entry but we found no evidence the microservice
    # actively populates it.
    if entry_type == "parameter" and odl_access == "readOnly":
        return "DEFAULT ONLY", ""

    # readWrite params with confirmed C write but no action callbacks:
    # the microservice populates the value for reporting, but write
    # handlers are absent so external writes won't have side effects.
    if entry_type == "parameter" and odl_entry.get("c_write_confirmed"):
        return "REPORT ONLY", ""

    return "NOT IMPLEMENTED", ""


_pkg_version_cache = None


def _load_package_versions():
    """Parse tmp/.packageinfo and return a dict of {package_name: version}."""
    global _pkg_version_cache
    if _pkg_version_cache is not None:
        return _pkg_version_cache

    _pkg_version_cache = {}
    pkginfo_path = os.path.join(PROJECT_ROOT, 'tmp', '.packageinfo')
    if not os.path.isfile(pkginfo_path):
        return _pkg_version_cache

    current_pkg = None
    with open(pkginfo_path, 'r', errors='replace') as f:
        for line in f:
            line = line.rstrip('\n')
            if line.startswith('Package: '):
                current_pkg = line[9:]
            elif line.startswith('Version: ') and current_pkg:
                _pkg_version_cache[current_pkg] = line[9:]
                current_pkg = None
    return _pkg_version_cache


def get_pkg_version(service_name, base_dir):
    """Look up the package version for a service from tmp/.packageinfo.

    Tries exact match first, then underscore→hyphen substitution (ODL directory
    names sometimes use underscores while package names use hyphens).
    Falls back to "unknown" if the package is not found.
    """
    if not service_name:
        return "unknown"
    versions = _load_package_versions()
    v = versions.get(service_name)
    if v:
        return v
    # ODL dirs use underscores, packages use hyphens
    hyphen_name = service_name.replace('_', '-')
    if hyphen_name != service_name:
        v = versions.get(hyphen_name)
        if v:
            return v
    return "unknown"


def annotate_element(elem, odl_entry, mark, diff_details, base_dir):
    """Add gw3: attributes to an XML element."""
    ns = f'{{{GW3_NAMESPACE}}}'

    if odl_entry:
        elem.set(f'{ns}Microservice', odl_entry.get('service', ''))
        elem.set(f'{ns}Microservice_version', get_pkg_version(odl_entry.get('service', ''), base_dir))
        elem.set(f'{ns}ODL_Type', odl_entry.get('odl_type', ''))
        elem.set(f'{ns}ODL_Access', odl_entry.get('access_display', odl_entry.get('access', '')))
        elem.set(f'{ns}ODL_Persistent', 'yes' if odl_entry.get('persistent') else 'no')
        elem.set(f'{ns}ODL_Volatile', 'yes' if odl_entry.get('volatile') else 'no')
        elem.set(f'{ns}ODL_Default', str(odl_entry.get('default', '') or ''))
        elem.set(f'{ns}ODL_Source_File', odl_entry.get('source_file', ''))
    else:
        elem.set(f'{ns}Microservice', '')
        elem.set(f'{ns}Microservice_version', '')
        elem.set(f'{ns}ODL_Type', '')
        elem.set(f'{ns}ODL_Access', '')
        elem.set(f'{ns}ODL_Persistent', '')
        elem.set(f'{ns}ODL_Volatile', '')
        elem.set(f'{ns}ODL_Default', '')
        elem.set(f'{ns}ODL_Source_File', '')

    elem.set(f'{ns}Mark', mark)
    elem.set(f'{ns}DIFF_Details', diff_details)


def process_xml(xml_path, odl_index, base_dir):
    """Walk the TR-181 XML tree and annotate every data-model element."""
    tree = ET.parse(xml_path)
    root = tree.getroot()

    # Register the GW3 namespace
    ET.register_namespace(GW3_NS_PREFIX, GW3_NAMESPACE)

    # Also preserve original namespaces
    # Re-register known BBF namespaces to keep clean output
    for prefix, uri in [
        ('', 'urn:broadband-forum-org:cwmp:datamodel-1-15'),
        ('dmr', 'urn:broadband-forum-org:cwmp:datamodel-report-1-0'),
        ('xsi', 'http://www.w3.org/2001/XMLSchema-instance'),
    ]:
        try:
            ET.register_namespace(prefix, uri)
        except ValueError:
            pass

    model = None
    for child in root:
        if strip_ns(child.tag) == 'model':
            model = child
    if model is None:
        print("ERROR: No <model> element found.", file=sys.stderr)
        sys.exit(1)

    stats = {
        'IMPLEMENTED': 0, 'NOT IMPLEMENTED': 0, 'DEFAULT ONLY': 0,
        'REPORT ONLY': 0, 'NOT DEFINED': 0, 'MISMATCH': 0,
        'VENDOR EXTENSION': 0, 'PRPL EXTENSION': 0, 'total': 0,
    }
    matched_paths = set()

    for obj in model:
        if strip_ns(obj.tag) != 'object':
            continue

        obj_name = obj.attrib.get('name', '')
        obj_access = obj.attrib.get('access', 'readOnly')
        obj_norm = normalize_path(obj_name)

        # Annotate the object
        odl_entry = lookup_odl(odl_index, obj_name)
        mark, diff = determine_mark('object', obj_access, odl_entry)
        annotate_element(obj, odl_entry, mark, diff, base_dir)
        stats[mark] += 1
        stats['total'] += 1
        if odl_entry:
            matched_paths.add(normalize_path(odl_entry['device_path']))

        # Annotate children
        for child in obj:
            ctag = strip_ns(child.tag)

            if ctag == 'parameter':
                param_name = child.attrib.get('name', '')
                param_access = child.attrib.get('access', 'readOnly')
                full_path = obj_name + param_name
                odl_e = lookup_odl(odl_index, full_path)
                m, d = determine_mark('parameter', param_access, odl_e)
                annotate_element(child, odl_e, m, d, base_dir)
                stats[m] += 1
                stats['total'] += 1
                if odl_e:
                    matched_paths.add(normalize_path(odl_e['device_path']))

            elif ctag == 'command':
                cmd_name = child.attrib.get('name', '')
                full_path = obj_name + cmd_name + "()"
                odl_e = lookup_odl(odl_index, full_path)
                # Also try without ()
                if not odl_e:
                    odl_e = lookup_odl(odl_index, obj_name + cmd_name)
                m, d = determine_mark('command', 'readOnly', odl_e)
                annotate_element(child, odl_e, m, d, base_dir)
                stats[m] += 1
                stats['total'] += 1
                if odl_e:
                    matched_paths.add(normalize_path(odl_e['device_path']))

            elif ctag == 'event':
                event_name = child.attrib.get('name', '')
                full_path = obj_name + event_name
                odl_e = lookup_odl(odl_index, full_path)
                m, d = determine_mark('event', 'readOnly', odl_e)
                annotate_element(child, odl_e, m, d, base_dir)
                stats[m] += 1
                stats['total'] += 1
                if odl_e:
                    matched_paths.add(normalize_path(odl_e['device_path']))

                # Annotate event argument parameters
                for event_child in child:
                    ectag = strip_ns(event_child.tag)
                    if ectag == 'parameter':
                        eparam_name = event_child.attrib.get('name', '')
                        efull_path = full_path + '.' + eparam_name
                        odl_ep = lookup_odl(odl_index, efull_path)
                        em, ed = determine_mark('parameter', 'readOnly', odl_ep)
                        annotate_element(event_child, odl_ep, em, ed, base_dir)
                        stats[em] += 1
                        stats['total'] += 1
                        if odl_ep:
                            matched_paths.add(normalize_path(odl_ep['device_path']))

    return tree, stats, matched_paths


def _extension_mark(device_path):
    """Return VENDOR EXTENSION for X_ prefix paths, PRPL EXTENSION otherwise."""
    # Check if any component after 'Device.' starts with 'X_'
    parts = device_path.split('.')
    for part in parts:
        if part.startswith('X_'):
            return 'VENDOR EXTENSION'
    return 'PRPL EXTENSION'


def add_vendor_extensions(tree, odl_entries, matched_paths, base_dir):
    """Add vendor/prpl extension elements (ODL entries not in TR-181) to the XML tree."""
    root = tree.getroot()
    model = None
    for child in root:
        if strip_ns(child.tag) == 'model':
            model = child
    if model is None:
        return {'VENDOR EXTENSION': 0, 'PRPL EXTENSION': 0}

    ns = f'{{{GW3_NAMESPACE}}}'
    bbf_ns = ''
    # Detect the BBF namespace from existing objects
    for obj in model:
        if 'object' in obj.tag:
            bbf_ns = obj.tag.replace('object', '')
            break

    ext_counts = {'VENDOR EXTENSION': 0, 'PRPL EXTENSION': 0}
    vendor_entries = []

    for entry in odl_entries:
        dp = entry["device_path"]
        norm = normalize_path(dp)

        if norm in matched_paths:
            continue

        # Skip internal-only paths that never map to Device.*
        if not dp.startswith('Device.'):
            continue

        vendor_entries.append(entry)

    # Group vendor entries by their object path
    vendor_objects = {}
    for entry in sorted(vendor_entries, key=lambda e: e['device_path']):
        if entry['entry_type'] == 'object':
            vendor_objects[entry['device_path']] = entry
        elif entry['entry_type'] == 'eventArg':
            # Skip event arguments here — added as children of event elements below
            continue
        elif entry['entry_type'] in ('parameter', 'command', 'event'):
            # Find or create parent object
            parts = entry['device_path'].rsplit('.', 1)
            if len(parts) == 2:
                parent_path = parts[0] + '.'
            else:
                parent_path = entry['device_path']
            if parent_path not in vendor_objects:
                vendor_objects[parent_path] = None

    # Build index of existing TR-181 object elements so we can append
    # vendor children to existing objects instead of creating duplicates
    existing_objects = {}
    for obj in model:
        if strip_ns(obj.tag) == 'object':
            existing_objects[normalize_path(obj.attrib.get('name', ''))] = obj

    # Add vendor/prpl extension objects to the model
    for obj_path in sorted(vendor_objects.keys()):
        obj_entry = vendor_objects[obj_path]
        norm_obj = normalize_path(obj_path)

        # Reuse existing TR-181 object element if present
        if norm_obj in existing_objects:
            obj_elem = existing_objects[norm_obj]
        else:
            mark = _extension_mark(obj_path)
            obj_elem = ET.SubElement(model, f'{bbf_ns}object')
            obj_elem.set('name', obj_path)
            if obj_entry:
                obj_elem.set('access', obj_entry.get('access', 'readOnly'))
                annotate_element(obj_elem, obj_entry, mark, '', base_dir)
            else:
                # Synthetic parent — derive service/source from first child
                representative = None
                for e in vendor_entries:
                    if e['entry_type'] != 'object':
                        ep = e['device_path'].rsplit('.', 1)[0] + '.' if '.' in e['device_path'] else ''
                        if normalize_path(ep) == norm_obj:
                            representative = e
                            break
                if representative:
                    # Build a minimal object-like entry from the child
                    synth = {
                        'service': representative.get('service', ''),
                        'odl_type': 'object',
                        'access': 'readOnly',
                        'access_display': 'readOnly',
                        'persistent': False,
                        'volatile': False,
                        'default': '',
                        'source_file': representative.get('source_file', ''),
                    }
                    obj_elem.set('access', 'readOnly')
                    annotate_element(obj_elem, synth, mark, '', base_dir)
                else:
                    obj_elem.set('access', 'readOnly')
                    annotate_element(obj_elem, None, mark, '', base_dir)
            ext_counts[mark] += 1

        # Add child parameters/commands/events
        for entry in vendor_entries:
            if entry['entry_type'] == 'object':
                continue
            entry_parent = entry['device_path'].rsplit('.', 1)[0] + '.' if '.' in entry['device_path'] else ''
            if normalize_path(entry_parent) != normalize_path(obj_path):
                continue

            mark = _extension_mark(entry['device_path'])
            if entry['entry_type'] == 'parameter':
                p_elem = ET.SubElement(obj_elem, f'{bbf_ns}parameter')
                p_elem.set('name', entry['name'])
                p_elem.set('access', entry.get('access', 'readOnly'))
                annotate_element(p_elem, entry, mark, '', base_dir)
                ext_counts[mark] += 1
            elif entry['entry_type'] == 'command':
                c_elem = ET.SubElement(obj_elem, f'{bbf_ns}command')
                c_elem.set('name', entry['name'].rstrip('()'))
                annotate_element(c_elem, entry, mark, '', base_dir)
                ext_counts[mark] += 1
            elif entry['entry_type'] == 'event':
                e_elem = ET.SubElement(obj_elem, f'{bbf_ns}event')
                e_elem.set('name', entry['name'])
                annotate_element(e_elem, entry, mark, '', base_dir)
                ext_counts[mark] += 1

                # Add event argument parameters
                event_path = entry['device_path']
                for arg_entry in vendor_entries:
                    if arg_entry['entry_type'] != 'eventArg':
                        continue
                    if not arg_entry['device_path'].startswith(event_path + '.'):
                        continue
                    arg_mark = _extension_mark(arg_entry['device_path'])
                    ap_elem = ET.SubElement(e_elem, f'{bbf_ns}parameter')
                    ap_elem.set('name', arg_entry['name'])
                    ap_elem.set('access', 'readOnly')
                    annotate_element(ap_elem, arg_entry, arg_mark, '', base_dir)
                    ext_counts[arg_mark] += 1

    return ext_counts


def main():
    parser = argparse.ArgumentParser(description='Generate enhanced TR-181 XML with GW3 annotations')
    parser.add_argument('--input', '-i', help='Input full XML file')
    parser.add_argument('--output-dir', '-o', default=LOCAL_DATAMODEL_DIR)
    args = parser.parse_args()

    ensure_output_dir(args.output_dir)

    # Find input XML
    xml_path = args.input
    if not xml_path:
        for fname in sorted(os.listdir(LOCAL_DATAMODEL_DIR), reverse=True):
            if fname.startswith('tr-181-') and fname.endswith('-usp-full.xml'):
                xml_path = os.path.join(LOCAL_DATAMODEL_DIR, fname)
                break
    if not xml_path or not os.path.exists(xml_path):
        print("ERROR: No input XML found. Run step1 first or specify --input.", file=sys.stderr)
        sys.exit(1)

    base_dir = staging_amx_path()
    print(f"Input XML: {xml_path}")
    print(f"ODL base: {base_dir}")

    # Parse ODL
    print("\n--- Parsing ODL files ---")
    odl_entries = parse_all_services(base_dir)
    odl_index = build_odl_index(odl_entries)
    print(f"ODL index: {len(odl_index)} unique paths")

    # Parse proxy-object mappings and augment ODL index
    print("\n--- Resolving proxy-object mappings ---")
    proxy_mappings = parse_proxy_mappings(base_dir)
    service_map = build_service_map(base_dir)
    print(f"Proxy mappings found: {len(proxy_mappings)}")
    for dp, lp in sorted(proxy_mappings.items()):
        print(f"  {dp} -> {lp}")
    alias_count = augment_odl_index_with_proxy_mappings(odl_index, proxy_mappings, service_map)
    print(f"Proxy-derived aliases added to index: {alias_count}")
    print(f"ODL index after augmentation: {len(odl_index)} unique paths")

    # Filter out private/protected entries — they are not accessible externally
    print("\n--- Filtering private/protected entries ---")
    priv_count_entries = sum(1 for e in odl_entries if e.get('private') or e.get('protected'))
    odl_entries = [e for e in odl_entries if not e.get('private') and not e.get('protected')]
    priv_count_index = 0
    for path in list(odl_index.keys()):
        entry = odl_index[path]
        if entry.get('private') or entry.get('protected'):
            del odl_index[path]
            priv_count_index += 1
    print(f"Excluded {priv_count_entries} private/protected entries from ODL list")
    print(f"Excluded {priv_count_index} private/protected entries from ODL index")
    print(f"ODL index after filtering: {len(odl_index)} unique paths")

    # Scan C source for emitted events and promote matching ODL entries
    bdir = build_dir_path()
    if os.path.isdir(bdir):
        print("\n--- Scanning C source for emitted events ---")
        emitted_events = scan_emitted_events(bdir)
        if emitted_events:
            print(f"Events found in C source: {', '.join(sorted(emitted_events))}")
            promoted = promote_emitted_events(odl_index, emitted_events)
            print(f"ODL events promoted to IMPLEMENTED: {promoted}")
        else:
            print("No emitted events found in C source.")
    else:
        print(f"\n--- Skipping C source scan (build dir not found: {bdir}) ---")

    # Scan C source for parameter writes and promote readOnly params
    if os.path.isdir(bdir):
        print("\n--- Scanning C source for parameter writes ---")
        written_params = scan_written_params(bdir)
        total_params = sum(len(v) for v in written_params.values())
        print(f"Scanned {len(written_params)} packages, found {total_params} unique param names")
        promoted_ro, flagged_rw = promote_written_params(odl_index, written_params)
        print(f"ReadOnly params promoted to IMPLEMENTED: {promoted_ro}")
        print(f"ReadWrite params flagged as REPORT ONLY: {flagged_rw}")

    # Annotate XML
    print("\n--- Annotating XML ---")
    tree, stats, matched_paths = process_xml(xml_path, odl_index, base_dir)

    # Add vendor/prpl extensions
    print("\n--- Adding vendor/prpl extensions ---")
    ext_counts = add_vendor_extensions(tree, odl_entries, matched_paths, base_dir)
    stats['VENDOR EXTENSION'] = ext_counts['VENDOR EXTENSION']
    stats['PRPL EXTENSION'] = ext_counts['PRPL EXTENSION']

    # Write output
    output_path = os.path.join(args.output_dir, 'gw3_current_datamodel.xml')
    tree.write(output_path, xml_declaration=True, encoding='utf-8')

    # Print summary
    total = stats['total']
    print(f"\n=== Enhanced XML Summary ===")
    print(f"Output: {output_path}")
    print(f"Total TR-181 elements annotated: {total}")
    for mark in ('IMPLEMENTED', 'NOT IMPLEMENTED', 'DEFAULT ONLY', 'REPORT ONLY', 'NOT DEFINED', 'MISMATCH'):
        count = stats.get(mark, 0)
        pct = count / total * 100 if total else 0
        print(f"  {mark}: {count} ({pct:.1f}%)")
    print(f"  VENDOR EXTENSION: {ext_counts['VENDOR EXTENSION']}")
    print(f"  PRPL EXTENSION: {ext_counts['PRPL EXTENSION']}")

    impl_pct = (stats['IMPLEMENTED']) / total * 100 if total else 0
    coverage_pct = (stats['IMPLEMENTED'] + stats['NOT IMPLEMENTED'] + stats.get('DEFAULT ONLY', 0) + stats.get('REPORT ONLY', 0)) / total * 100 if total else 0
    print(f"\nImplementation rate: {impl_pct:.1f}%")
    print(f"Coverage (defined in ODL): {coverage_pct:.1f}%")


if __name__ == '__main__':
    main()
