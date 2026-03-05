#!/usr/bin/env python3
"""
Parse USP translate ODL files to build service -> Device.* path mappings.

Scans the AMX staging directory for ``*_usp.odl`` files and extracts
``usp.translate`` entries that map internal ODL paths to Device.* paths.

When run standalone, writes ``local/datamodel/service-map.json``.
"""

import os
import re
import json
import sys

from config import PROJECT_ROOT, VARIABLES, staging_amx_path, ensure_output_dir, LOCAL_DATAMODEL_DIR


def substitute_vars(text):
    """Replace ``${var}`` with known variable values from config.VARIABLES."""
    def replacer(m):
        return VARIABLES.get(m.group(1), m.group(0))
    return re.sub(r'\$\{([^}]+)\}', replacer, text)


def parse_usp_translate(filepath):
    """Parse a ``*_usp.odl`` file and extract usp.translate mappings.

    Returns a list of ``(internal_path, device_path)`` tuples.
    """
    mappings = []
    try:
        with open(filepath, 'r') as f:
            content = f.read()
    except (OSError, IOError):
        return mappings

    content = substitute_vars(content)

    translate_match = re.search(r'usp\.translate\s*=\s*\{([^}]+)\}', content, re.DOTALL)
    if translate_match:
        block = translate_match.group(1)
        for m in re.finditer(r"""['"]['"]?(\w[\w.-]*)['"]['"]?\s*=\s*["']([^"']+)["']""", block):
            mappings.append((m.group(1), m.group(2)))

    return mappings


def build_service_map(base_dir=None):
    """Build complete service -> Device.* path mapping from USP translate files.

    Parameters
    ----------
    base_dir : str, optional
        Absolute path to the AMX staging root.  Defaults to the project's staging_dir.

    Returns
    -------
    dict
        ``{service_name: [(internal_prefix, device_prefix), ...]}``
    """
    if base_dir is None:
        base_dir = staging_amx_path()

    service_map = {}

    if not os.path.isdir(base_dir):
        print(f"ERROR: Directory not found: {base_dir}", file=sys.stderr)
        return service_map

    for service_name in sorted(os.listdir(base_dir)):
        service_dir = os.path.join(base_dir, service_name)
        if not os.path.isdir(service_dir):
            continue

        extensions_dir = os.path.join(service_dir, "extensions")
        if os.path.isdir(extensions_dir):
            for fname in sorted(os.listdir(extensions_dir)):
                if fname.endswith("_usp.odl"):
                    filepath = os.path.join(extensions_dir, fname)
                    mappings = parse_usp_translate(filepath)
                    if mappings:
                        if service_name not in service_map:
                            service_map[service_name] = []
                        service_map[service_name].extend(mappings)

    # Hardcoded fallbacks for services without explicit USP translate files
    fallbacks = {
        "tr181-device": [("Device.", "Device.")],
        "netmodel": [("NetDev.", "NetDev."), ("NetModel.", "NetModel.")],
        "deviceinfo-system": [("DeviceInfo.", "Device.DeviceInfo.")],
        "aei_network": [("X_AEI_COM_Network.", "Device.X_AEI_COM_Network.")],
        "aei_led_manager": [("X_AEI_COM_LED.", "Device.X_AEI_COM_LED.")],
    }
    for svc, maps in fallbacks.items():
        if svc not in service_map and os.path.isdir(os.path.join(base_dir, svc)):
            service_map[svc] = maps

    return service_map


def main():
    base_dir = staging_amx_path()
    service_map = build_service_map(base_dir)

    output = {}
    for svc, maps in sorted(service_map.items()):
        output[svc] = [{"internal": i, "device": d} for i, d in maps]

    ensure_output_dir()
    output_path = os.path.join(LOCAL_DATAMODEL_DIR, "service-map.json")
    with open(output_path, 'w') as f:
        json.dump(output, f, indent=2)

    print(f"Service map: {len(service_map)} services with path mappings")
    for svc, maps in sorted(service_map.items()):
        for internal, device in maps:
            print(f"  {svc}: {internal} -> {device}")
    print(f"\nSaved to: {output_path}")

    return service_map


if __name__ == '__main__':
    main()
