#!/usr/bin/env python3
"""
Parse ODL definition files to extract TR-181 data model entries.

Uses a character-level scanner with brace-depth tracking to extract:
- Objects (singleton and multi-instance)
- Parameters (with type, access, defaults, action callbacks)
- Commands/functions
- Events

When run standalone, writes ``local/datamodel/odl-parsed.json``.
"""

import os
import re
import json
import sys

from config import (
    PROJECT_ROOT, VARIABLES, staging_amx_path,
    ensure_output_dir, LOCAL_DATAMODEL_DIR,
)
from service_map import build_service_map

# Add additional known variables
VARIABLES.setdefault("prefix_", "")

# ODL type -> TR-181 type mapping
ODL_TYPE_MAP = {
    "string": "string",
    "cstring_t": "string",
    "bool": "boolean",
    "uint8": "unsignedInt",
    "uint16": "unsignedInt",
    "uint32": "unsignedInt",
    "uint64": "unsignedLong",
    "int8": "int",
    "int16": "int",
    "int32": "int",
    "int64": "long",
    "float": "string",
    "double": "string",
    "csv_string": "string",
    "ssv_string": "string",
    "datetime": "dateTime",
    "list": "string",
    "htable": "string",
    "variant": "string",
}

ODL_TYPES = set(ODL_TYPE_MAP.keys())

MODIFIERS = {
    "%persistent", "%read-only", "%read_only", "%protected", "%private",
    "%volatile", "%variable", "%key", "%unique", "%template", "%instance",
}

# Modifiers that represent access/visibility attributes (mapped to short names)
ACCESS_MODIFIERS = {
    "%read-only": "readOnly",
    "%read_only": "readOnly",
    "%protected": "protected",
    "%private": "private",
    "%persistent": "persistent",
    "%volatile": "volatile",
    "%locked": "locked",
}


def parse_access_attrs(modifiers_str):
    """Extract access/visibility attributes from an ODL modifier string.

    Returns a dict with:
        access      — 'readOnly' or 'readWrite'
        persistent  — True/False
        private     — True if %private present
        protected   — True if %protected present
        volatile    — True if %volatile present
        locked      — True if %locked present
        access_display — human-readable string for the ODL_Access column
                         (access type only; persistent and volatile have their
                          own columns, private/protected entries are excluded)
    """
    tokens = re.findall(r'%[\w-]+', modifiers_str)
    access = "readWrite"
    persistent = False
    private = False
    protected = False
    volatile = False
    locked = False
    for tok in tokens:
        if tok in ("%read-only", "%read_only"):
            access = "readOnly"
        elif tok == "%persistent":
            persistent = True
        elif tok == "%private":
            private = True
        elif tok == "%protected":
            protected = True
        elif tok == "%volatile":
            volatile = True
        elif tok == "%locked":
            locked = True
    return {
        'access': access,
        'persistent': persistent,
        'private': private,
        'protected': protected,
        'volatile': volatile,
        'locked': locked,
        'access_display': access,
    }


def substitute_vars(text):
    """Replace ``${var}`` with known variable values."""
    def replacer(m):
        return VARIABLES.get(m.group(1), m.group(0))
    return re.sub(r'\$\{([^}]+)\}', replacer, text)


def strip_comments(text):
    """Remove C-style block and line comments, preserving string literals."""
    result = []
    i = 0
    in_string = False
    while i < len(text):
        if text[i] == '"' and not in_string:
            in_string = True
            result.append(text[i])
            i += 1
            continue
        if in_string:
            if text[i] == '\\' and i + 1 < len(text):
                result.append(text[i:i + 2])
                i += 2
                continue
            if text[i] == '"':
                in_string = False
            result.append(text[i])
            i += 1
            continue
        if text[i:i + 2] == '/*':
            end = text.find('*/', i + 2)
            if end == -1:
                break
            i = end + 2
            result.append(' ')
            continue
        if text[i:i + 2] == '//':
            end = text.find('\n', i)
            if end == -1:
                break
            i = end
            continue
        result.append(text[i])
        i += 1
    return ''.join(result)


def extract_top_level_blocks(text):
    """Find all ``%define { ... }`` and ``select ... { ... }`` blocks."""
    blocks = []

    for m in re.finditer(r'%define\s*\{', text):
        start = m.end()
        depth, pos, in_str = 1, start, False
        while pos < len(text) and depth > 0:
            ch = text[pos]
            if ch == '"' and not in_str:
                in_str = True
            elif ch == '"' and in_str and text[pos - 1:pos] != '\\':
                in_str = False
            elif not in_str:
                if ch == '{':
                    depth += 1
                elif ch == '}':
                    depth -= 1
            pos += 1
        blocks.append(('define', '', text[start:pos - 1]))

    for m in re.finditer(r'\bselect\s+([\w.]+)\s*\{', text):
        base_path = m.group(1)
        start = m.end()
        depth, pos, in_str = 1, start, False
        while pos < len(text) and depth > 0:
            ch = text[pos]
            if ch == '"' and not in_str:
                in_str = True
            elif ch == '"' and in_str and text[pos - 1:pos] != '\\':
                in_str = False
            elif not in_str:
                if ch == '{':
                    depth += 1
                elif ch == '}':
                    depth -= 1
            pos += 1
        blocks.append(('select', base_path, text[start:pos - 1]))

    return blocks


class ODLScanner:
    """Scan ODL block text to extract objects, parameters, commands, events."""

    def __init__(self, service_name, translate_path_fn, source_file):
        self.service_name = service_name
        self.translate_path = translate_path_fn
        self.source_file = source_file
        self.entries = []

    def _process_statement(self, stmt, path_stack):
        """Process a single ODL statement (text between semicolons)."""
        if not path_stack:
            return

        current_path = '.'.join(path_stack) + '.'

        skip_prefixes = (
            'counted with', 'on action', 'on event', 'userflags',
            'entry-point', 'import ', '#include', 'include ',
            'requires ', 'default ', 'mib ',
        )
        stmt_lower = stmt.lstrip()
        for prefix in skip_prefixes:
            if stmt_lower.startswith(prefix):
                return

        # Event declaration
        event_m = re.match(r'event\s+[\'"]([^"\']+)[\'"]', stmt)
        if event_m:
            event_name = event_m.group(1)
            if ':' not in event_name:
                device_path = self.translate_path(current_path)
                self.entries.append({
                    "device_path": device_path + event_name,
                    "name": event_name,
                    "service": self.service_name,
                    "entry_type": "event",
                    "odl_type": "event",
                    "access": "readOnly",
                    "access_display": "readOnly",
                    "persistent": False,
                    "private": False,
                    "protected": False,
                    "volatile": False,
                    "is_multi_instance": False,
                    "default": None,
                    "has_actions": False,
                    "impl_level": "NOT IMPLEMENTED",
                    "source_file": self.source_file,
                })
            return

        # Function/command declaration
        func_m = re.match(
            r'(?:(?:%[\w-]+)\s+)*'
            r'(\w+)\s+'
            r'(\w+)\s*\(([^)]*)\)',
            stmt
        )
        if func_m:
            ret_type = func_m.group(1)
            func_name = func_m.group(2)
            if ret_type in ODL_TYPES or ret_type in ('void',):
                device_path = self.translate_path(current_path)
                self.entries.append({
                    "device_path": device_path + func_name + "()",
                    "name": func_name + "()",
                    "service": self.service_name,
                    "entry_type": "command",
                    "odl_type": ret_type,
                    "access": "readOnly",
                    "access_display": "readOnly",
                    "persistent": False,
                    "private": False,
                    "protected": False,
                    "volatile": False,
                    "is_multi_instance": False,
                    "default": None,
                    "has_actions": True,
                    "impl_level": "IMPLEMENTED",
                    "source_file": self.source_file,
                })
                return

        # Parameter declaration
        param_m = re.match(
            r'((?:(?:%[\w-]+)\s+)*)'
            r'(\w+)\s+'
            r'(\w[\w-]*)'
            r'(?:\s*=\s*(.+))?',
            stmt
        )
        if param_m:
            modifiers = param_m.group(1).strip()
            param_type = param_m.group(2)
            param_name = param_m.group(3)
            default_val = param_m.group(4)

            if param_type in ODL_TYPES:
                if default_val:
                    default_val = default_val.strip().strip('"\'')

                attrs = parse_access_attrs(modifiers)

                device_path = self.translate_path(current_path)
                self.entries.append({
                    "device_path": device_path + param_name,
                    "name": param_name,
                    "service": self.service_name,
                    "entry_type": "parameter",
                    "odl_type": param_type,
                    "tr181_type": ODL_TYPE_MAP.get(param_type, param_type),
                    "access": attrs['access'],
                    "access_display": attrs['access_display'],
                    "persistent": attrs['persistent'],
                    "private": attrs['private'],
                    "protected": attrs['protected'],
                    "volatile": attrs['volatile'],
                    "is_multi_instance": False,
                    "default": default_val,
                    "has_actions": False,
                    "impl_level": "NOT IMPLEMENTED",
                    "source_file": self.source_file,
                })
                return

    def _scan_param_block(self, block_text, entry):
        """Scan a parameter's ``{ }`` block for defaults and action callbacks."""
        default_m = re.search(r'\bdefault\s+([^;]+)', block_text)
        if default_m:
            entry["default"] = default_m.group(1).strip().strip('"\'')

        has_read = bool(re.search(r'on\s+action\s+read\s+call', block_text))
        has_write = bool(re.search(r'on\s+action\s+write\s+call', block_text))
        has_validate = bool(re.search(r'on\s+action\s+validate\s+call', block_text))

        if has_read or has_write:
            entry["has_actions"] = True
            entry["impl_level"] = "IMPLEMENTED"
        elif has_validate:
            entry["has_actions"] = True

    def _scan_event_block(self, block_text, event_entry):
        """Scan an event's ``{ }`` block for argument parameter declarations.

        Extracts typed parameters like ``string ParamPath;`` and adds them
        as ``eventArg`` entries under the event's device path.
        """
        event_dp = event_entry['device_path']

        for m in re.finditer(
            r'((?:(?:%[\w-]+)\s+)*)(\w+)\s+([\w][\w-]*)\s*;',
            block_text
        ):
            param_type = m.group(2)
            param_name = m.group(3)

            if param_type not in ODL_TYPES:
                continue

            self.entries.append({
                "device_path": event_dp + '.' + param_name,
                "name": param_name,
                "service": self.service_name,
                "entry_type": "eventArg",
                "odl_type": param_type,
                "tr181_type": ODL_TYPE_MAP.get(param_type, param_type),
                "access": "readOnly",
                "access_display": "readOnly",
                "persistent": False,
                "private": event_entry.get('private', False),
                "protected": event_entry.get('protected', False),
                "volatile": False,
                "is_multi_instance": False,
                "default": None,
                "has_actions": False,
                "impl_level": "NOT IMPLEMENTED",
                "source_file": self.source_file,
            })


def scan_with_param_blocks(text, scanner, initial_path_stack=None):
    """Enhanced scanner that handles parameter blocks with defaults/actions.

    Tracks brace depth and block start positions to correctly capture
    the full text of parameter blocks for default/action extraction.
    """
    path_stack = list(initial_path_stack) if initial_path_stack else []
    depth_info = []

    pos = 0
    stmt_start = 0

    while pos < len(text):
        ch = text[pos]

        # Skip string literals
        if ch == '"':
            pos += 1
            while pos < len(text):
                if text[pos] == '\\':
                    pos += 2
                    continue
                if text[pos] == '"':
                    pos += 1
                    break
                pos += 1
            continue

        if ch == "'":
            pos += 1
            while pos < len(text) and text[pos] != "'":
                if text[pos] == '\\':
                    pos += 1
                pos += 1
            if pos < len(text):
                pos += 1
            continue

        # Opening brace
        if ch == '{':
            stmt = text[stmt_start:pos].strip()
            block_content_start = pos + 1
            pos += 1
            stmt_start = pos

            obj_m = re.search(
                r'(?:^|\s)object\s+[\'"]?([\w-]+)[\'"]?\s*(\[\])?\s*$',
                stmt
            )
            if obj_m:
                obj_name = obj_m.group(1)
                is_multi = obj_m.group(2) is not None
                modifiers = stmt[:obj_m.start()].strip()

                path_stack.append(obj_name)
                depth_info.append(('object', obj_name, is_multi))

                obj_path = '.'.join(path_stack) + '.'
                device_path = scanner.translate_path(obj_path)

                attrs = parse_access_attrs(modifiers)
                access = attrs['access']
                if not is_multi and access != "readOnly":
                    access = "readOnly"

                scanner.entries.append({
                    "device_path": device_path,
                    "name": obj_name,
                    "service": scanner.service_name,
                    "entry_type": "object",
                    "odl_type": "object",
                    "access": access,
                    "access_display": attrs['access_display'],
                    "persistent": attrs['persistent'],
                    "private": attrs['private'],
                    "protected": attrs['protected'],
                    "volatile": attrs['volatile'],
                    "is_multi_instance": is_multi,
                    "default": None,
                    "has_actions": False,
                    "impl_level": "NOT IMPLEMENTED",
                    "source_file": scanner.source_file,
                })
                continue

            param_m = re.match(
                r'((?:(?:%[\w-]+)\s+)*)'
                r'(\w+)\s+'
                r'([\w][\w-]*)\s*'
                r'(?:=\s*(.+?)\s*)?$',
                stmt
            )
            if param_m and param_m.group(2) in ODL_TYPES and path_stack:
                modifiers = param_m.group(1).strip()
                param_type = param_m.group(2)
                param_name = param_m.group(3)
                default_val = param_m.group(4)
                if default_val:
                    default_val = default_val.strip().strip('"\'')

                attrs = parse_access_attrs(modifiers)

                current_path = '.'.join(path_stack) + '.'
                device_path = scanner.translate_path(current_path)

                entry = {
                    "device_path": device_path + param_name,
                    "name": param_name,
                    "service": scanner.service_name,
                    "entry_type": "parameter",
                    "odl_type": param_type,
                    "tr181_type": ODL_TYPE_MAP.get(param_type, param_type),
                    "access": attrs['access'],
                    "access_display": attrs['access_display'],
                    "persistent": attrs['persistent'],
                    "private": attrs['private'],
                    "protected": attrs['protected'],
                    "volatile": attrs['volatile'],
                    "is_multi_instance": False,
                    "default": default_val,
                    "has_actions": False,
                    "impl_level": "NOT IMPLEMENTED",
                    "source_file": scanner.source_file,
                }
                scanner.entries.append(entry)
                depth_info.append(('param', entry, block_content_start))
                continue

            event_m = re.search(r'event\s+[\'"]([^"\']+)[\'"]\s*$', stmt)
            if event_m and path_stack:
                event_name = event_m.group(1)
                event_entry = None
                if ':' not in event_name:
                    current_path = '.'.join(path_stack) + '.'
                    device_path = scanner.translate_path(current_path)
                    event_entry = {
                        "device_path": device_path + event_name,
                        "name": event_name,
                        "service": scanner.service_name,
                        "entry_type": "event",
                        "odl_type": "event",
                        "access": "readOnly",
                        "access_display": "readOnly",
                        "persistent": False,
                        "private": False,
                        "protected": False,
                        "volatile": False,
                        "is_multi_instance": False,
                        "default": None,
                        "has_actions": False,
                        "impl_level": "NOT IMPLEMENTED",
                        "source_file": scanner.source_file,
                    }
                    scanner.entries.append(event_entry)
                depth_info.append(('event', event_entry, block_content_start))
                continue

            mib_m = re.search(r'\bmib\s+\w+\s*$', stmt)
            if mib_m:
                depth_info.append(('mib',))
                continue

            depth_info.append(('other',))
            continue

        # Closing brace
        if ch == '}':
            if depth_info:
                info = depth_info.pop()
                if info[0] == 'object':
                    path_stack.pop()
                elif info[0] == 'event':
                    event_entry = info[1]
                    block_start = info[2]
                    if event_entry is not None:
                        full_block_text = text[block_start:pos]
                        scanner._scan_event_block(full_block_text, event_entry)
                elif info[0] == 'param':
                    entry = info[1]
                    block_start = info[2]
                    full_block_text = text[block_start:pos]
                    scanner._scan_param_block(full_block_text, entry)
            pos += 1
            stmt_start = pos
            continue

        # Semicolon
        if ch == ';':
            stmt = text[stmt_start:pos].strip()
            pos += 1
            stmt_start = pos

            if stmt and path_stack:
                in_data_block = True
                for info in depth_info:
                    if info[0] in ('param', 'mib', 'other', 'event'):
                        in_data_block = False
                        break
                if in_data_block:
                    scanner._process_statement(stmt, path_stack)
            continue

        pos += 1


def parse_service(service_dir, service_name, path_mappings):
    """Parse all ODL definition files in a service directory."""
    all_entries = []

    def make_translate_fn(mappings):
        def translate(internal_path):
            for internal_prefix, device_prefix in mappings:
                if internal_path.startswith(internal_prefix):
                    remainder = internal_path[len(internal_prefix):]
                    return device_prefix + remainder
            if not internal_path.startswith("Device."):
                return "Device." + internal_path
            return internal_path
        return translate

    translate_fn = make_translate_fn(path_mappings)

    skip_dirs = {'extensions', 'rules4', 'rules6'}
    odl_files = []
    for dirpath, dirnames, filenames in os.walk(service_dir):
        rel = os.path.relpath(dirpath, service_dir)
        parts = rel.split(os.sep)
        skip = False
        for p in parts:
            if p in skip_dirs or p.startswith('defaults') or p.startswith('iptables'):
                skip = True
                break
        if skip:
            continue
        for fname in sorted(filenames):
            if fname.endswith('.odl'):
                odl_files.append(os.path.join(dirpath, fname))

    for odl_path in odl_files:
        try:
            with open(odl_path, 'r') as f:
                raw_content = f.read()
        except (OSError, IOError):
            continue

        if '%define' not in raw_content and 'select ' not in raw_content:
            continue

        content = substitute_vars(raw_content)
        content = strip_comments(content)

        blocks = extract_top_level_blocks(content)
        if not blocks:
            continue

        source_file = os.path.basename(odl_path)
        scanner_obj = ODLScanner(service_name, translate_fn, source_file)

        for block_type, base_path, block_text in blocks:
            if block_type == 'define':
                scan_with_param_blocks(block_text, scanner_obj, [])
            elif block_type == 'select':
                initial_stack = [p for p in base_path.split('.') if p]
                scan_with_param_blocks(block_text, scanner_obj, initial_stack)

        all_entries.extend(scanner_obj.entries)

    return all_entries


def detect_object_actions(entries):
    """Post-process: mark objects as IMPLEMENTED if any child has action callbacks."""
    obj_entries = {e["device_path"]: e for e in entries if e["entry_type"] == "object"}

    for entry in entries:
        if entry["entry_type"] != "object":
            parent_path = entry["device_path"].rsplit('.', 1)[0] + '.' if '.' in entry["device_path"] else ''
            path = parent_path
            while path:
                if path in obj_entries:
                    parent_obj = obj_entries[path]
                    if entry.get("has_actions") or entry.get("impl_level") == "IMPLEMENTED":
                        parent_obj["has_actions"] = True
                        parent_obj["impl_level"] = "IMPLEMENTED"
                parts = path.rstrip('.').rsplit('.', 1)
                if len(parts) > 1:
                    path = parts[0] + '.'
                else:
                    break


def parse_all_services(base_dir=None):
    """Parse all ODL services and return deduplicated, sorted entries.

    This is the main entry point used by other pipeline steps.
    """
    if base_dir is None:
        base_dir = staging_amx_path()

    service_map = build_service_map(base_dir)
    print(f"Service map: {len(service_map)} services with path mappings")

    all_entries = []
    services_parsed = 0

    for service_name in sorted(os.listdir(base_dir)):
        service_dir = os.path.join(base_dir, service_name)
        if not os.path.isdir(service_dir):
            continue

        path_mappings = service_map.get(service_name, [])
        entries = parse_service(service_dir, service_name, path_mappings)
        if entries:
            services_parsed += 1
            all_entries.extend(entries)
            print(f"  {service_name}: {len(entries)} entries")

    detect_object_actions(all_entries)

    # Deduplicate
    seen = {}
    unique_entries = []
    for entry in all_entries:
        key = entry["device_path"]
        if key not in seen:
            seen[key] = entry
            unique_entries.append(entry)
        else:
            existing = seen[key]
            if entry["impl_level"] == "IMPLEMENTED" and existing["impl_level"] != "IMPLEMENTED":
                idx = unique_entries.index(existing)
                seen[key] = entry
                unique_entries[idx] = entry
            elif entry["has_actions"] and not existing["has_actions"]:
                existing["has_actions"] = True
                existing["impl_level"] = "IMPLEMENTED"

    unique_entries.sort(key=lambda e: e["device_path"])

    print(f"\nServices parsed: {services_parsed}")
    print(f"Total unique entries: {len(unique_entries)}")

    return unique_entries


def main():
    entries = parse_all_services()

    ensure_output_dir()
    output_path = os.path.join(LOCAL_DATAMODEL_DIR, "odl-parsed.json")
    with open(output_path, 'w') as f:
        json.dump(entries, f, indent=2)

    # Summary
    type_counts = {}
    impl_counts = {}
    svc_counts = {}
    for e in entries:
        type_counts[e["entry_type"]] = type_counts.get(e["entry_type"], 0) + 1
        impl_counts[e["impl_level"]] = impl_counts.get(e["impl_level"], 0) + 1
        svc_counts[e["service"]] = svc_counts.get(e["service"], 0) + 1

    print(f"\nBy type:")
    for t, c in sorted(type_counts.items()):
        print(f"  {t}: {c}")
    print(f"\nBy implementation level:")
    for level, c in sorted(impl_counts.items()):
        print(f"  {level}: {c}")
    print(f"\nTop services by entry count:")
    for svc, c in sorted(svc_counts.items(), key=lambda x: -x[1])[:15]:
        print(f"  {svc}: {c}")
    print(f"\nSaved to: {output_path}")

    return entries


if __name__ == '__main__':
    main()
