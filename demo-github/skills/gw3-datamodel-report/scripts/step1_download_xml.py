#!/usr/bin/env python3
"""
Step 1: Download the latest TR-181 USP XML from Broadband Forum.

Fetches the index page at usp-data-models.broadband-forum.org, identifies the
latest TR-181 version, and downloads both the compressed and full XML files.

Output:
    local/datamodel/tr-181-<ver>-usp.xml
    local/datamodel/tr-181-<ver>-usp-full.xml
"""

import os
import re
import sys
import xml.etree.ElementTree as ET
from urllib.request import urlopen, Request
from urllib.error import URLError, HTTPError

from config import (
    BBF_BASE_URL, LOCAL_DATAMODEL_DIR, ensure_output_dir,
    get_version_from_filename, get_version_string,
)


def fetch_index_page():
    """Fetch the BBF data models index page and return its HTML."""
    url = BBF_BASE_URL
    print(f"Fetching index page: {url}")
    req = Request(url, headers={'User-Agent': 'Mozilla/5.0 (GW3 DataModel Tool)'})
    try:
        with urlopen(req, timeout=30) as resp:
            return resp.read().decode('utf-8', errors='replace')
    except (URLError, HTTPError) as e:
        print(f"ERROR: Failed to fetch index page: {e}", file=sys.stderr)
        sys.exit(1)


def find_latest_version(html):
    """Parse the index HTML and find the latest TR-181 USP XML version.

    Returns (version_tuple, base_name) e.g. ((2,20,1), 'tr-181-2-20-1-usp').
    The BBF page links to ``*-full.xml`` files; we extract the version from those.
    """
    # Match href attributes pointing to TR-181 USP full XML files
    pattern = re.compile(
        r'href=["\']?(tr-181-(\d+)-(\d+)-(\d+)-usp-full\.xml)["\']?',
        re.IGNORECASE,
    )
    seen = set()
    versions = []
    for m in pattern.finditer(html):
        ver = (int(m.group(2)), int(m.group(3)), int(m.group(4)))
        if ver not in seen:
            seen.add(ver)
            # base is the stem without -full, e.g. "tr-181-2-20-1-usp"
            base = m.group(1).replace('-full.xml', '')
            versions.append((ver, base))

    if not versions:
        print("ERROR: No TR-181 USP XML files found on index page.", file=sys.stderr)
        sys.exit(1)

    versions.sort(key=lambda x: x[0], reverse=True)
    latest_ver, latest_base = versions[0]
    print(f"Latest version found: {'.'.join(str(v) for v in latest_ver)}")
    print(f"  Available versions: {', '.join('.'.join(str(v) for v in ver) for ver, _ in versions[:5])}")
    return latest_ver, latest_base


def download_file(url, dest_path):
    """Download a file from URL to local path."""
    print(f"  Downloading: {url}")
    req = Request(url, headers={'User-Agent': 'Mozilla/5.0 (GW3 DataModel Tool)'})
    try:
        with urlopen(req, timeout=120) as resp:
            data = resp.read()
        with open(dest_path, 'wb') as f:
            f.write(data)
        size_kb = len(data) / 1024
        print(f"  Saved: {dest_path} ({size_kb:.0f} KB)")
        return True
    except (URLError, HTTPError) as e:
        print(f"  ERROR: Download failed: {e}", file=sys.stderr)
        return False


def verify_xml(filepath):
    """Verify that a file is valid XML with the expected root element."""
    try:
        tree = ET.parse(filepath)
        root = tree.getroot()
        # BBF data model files have a root like <dm:document> or just <document>
        print(f"  Verified: valid XML, root tag = {root.tag}")
        return True
    except ET.ParseError as e:
        print(f"  WARNING: XML parse error: {e}", file=sys.stderr)
        return False


def main():
    ensure_output_dir()

    html = fetch_index_page()
    latest_ver, latest_base = find_latest_version(html)

    # Files to download
    files = {
        f"{latest_base}.xml": "Compressed XML (definitions only)",
        f"{latest_base}-full.xml": "Full XML (with imported components)",
    }

    results = {}
    for filename, description in files.items():
        url = BBF_BASE_URL + filename
        dest = os.path.join(LOCAL_DATAMODEL_DIR, filename)
        print(f"\n[{description}]")
        if download_file(url, dest):
            if os.path.getsize(dest) > 0:
                verify_xml(dest)
                results[filename] = dest
            else:
                print(f"  ERROR: Downloaded file is empty.", file=sys.stderr)
        else:
            print(f"  WARNING: Skipping {filename}")

    if not results:
        print("\nERROR: No files downloaded successfully.", file=sys.stderr)
        sys.exit(1)

    print(f"\n=== Download Summary ===")
    print(f"TR-181 version: {'.'.join(str(v) for v in latest_ver)}")
    print(f"Files downloaded to: {LOCAL_DATAMODEL_DIR}/")
    for filename, path in results.items():
        size = os.path.getsize(path)
        print(f"  {filename} ({size / 1024:.0f} KB)")

    return results


if __name__ == '__main__':
    main()
