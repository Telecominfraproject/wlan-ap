#!/usr/bin/env python3
"""
Step 7: Upload data model report to Confluence.

Generates a summary markdown page and uploads it (with attachments) to the
configured Confluence page using ``upload_confluence_v2.py``.

Also updates ``confluence-pages.yaml`` with the page mapping.

Prerequisites:
    - Confluence credentials in ``~/.claude/.mcp.json``
    - ``upload_confluence_v2.py`` at ``~/.claude/skills/confluence/scripts/``
    - All prior steps completed (files in local/datamodel/)
"""

import argparse
import html
import json
import os
import re
import subprocess
import sys
import tempfile
from datetime import datetime, timezone

from config import (
    PROJECT_ROOT, LOCAL_DATAMODEL_DIR, SPECS_DATAMODEL_DIR,
    CONFLUENCE_PAGE_ID, ensure_output_dir, get_version_string,
)


def read_confluence_credentials():
    """Read Confluence credentials from MCP server config."""
    mcp_path = os.path.expanduser('~/.claude/.mcp.json')
    if not os.path.exists(mcp_path):
        print(f"ERROR: MCP config not found: {mcp_path}", file=sys.stderr)
        sys.exit(1)

    with open(mcp_path) as f:
        mcp = json.load(f)

    atlassian_env = {}
    for key in ('mcpServers', 'servers'):
        servers = mcp.get(key, {})
        if 'atlassian' in servers:
            atlassian_env = servers['atlassian'].get('env', {})
            break
        # Try other common names
        for sname in servers:
            if 'atlassian' in sname.lower() or 'confluence' in sname.lower():
                atlassian_env = servers[sname].get('env', {})
                break
        if atlassian_env:
            break

    url = atlassian_env.get('CONFLUENCE_URL', '')
    username = atlassian_env.get('CONFLUENCE_EMAIL', '') or atlassian_env.get('CONFLUENCE_USERNAME', '')
    token = atlassian_env.get('CONFLUENCE_API_TOKEN', '')

    if not all([url, username, token]):
        print("ERROR: Missing Confluence credentials in MCP config.", file=sys.stderr)
        print(f"  CONFLUENCE_URL: {'set' if url else 'MISSING'}", file=sys.stderr)
        print(f"  CONFLUENCE_EMAIL: {'set' if username else 'MISSING'}", file=sys.stderr)
        print(f"  CONFLUENCE_API_TOKEN: {'set' if token else 'MISSING'}", file=sys.stderr)
        sys.exit(1)

    return url, username, token


def detect_tr181_version():
    """Detect TR-181 version from files."""
    source = SPECS_DATAMODEL_DIR if os.path.isdir(SPECS_DATAMODEL_DIR) else LOCAL_DATAMODEL_DIR
    if not os.path.isdir(source):
        return 'unknown'
    for fname in sorted(os.listdir(source), reverse=True):
        if fname.startswith('tr-181-') and fname.endswith('-usp-full.xml'):
            return get_version_string(fname)
    return 'unknown'


def collect_stats_from_excel():
    """Try to read coverage stats from the enhanced Excel (via XML mark attributes)."""
    # Quick stats from the enhanced XML
    xml_path = os.path.join(LOCAL_DATAMODEL_DIR, 'gw3_current_datamodel.xml')
    if not os.path.exists(xml_path):
        xml_path = os.path.join(SPECS_DATAMODEL_DIR, 'gw3_current_datamodel.xml')
    if not os.path.exists(xml_path):
        return None

    import xml.etree.ElementTree as ET
    from xml_utils import strip_ns

    gw3_ns = '{urn:actiontec:gw3:datamodel-coverage-1-0}'
    tree = ET.parse(xml_path)
    root = tree.getroot()

    marks = {}
    services = {}
    mismatch_details = {}  # {service: [(path, diff_details), ...]}

    model = None
    for child in root:
        if strip_ns(child.tag) == 'model':
            model = child
    if model is None:
        return None

    for obj in model:
        if strip_ns(obj.tag) != 'object':
            continue
        obj_name = obj.attrib.get('name', '')

        for elem in [obj] + list(obj):
            etag = strip_ns(elem.tag)
            if etag not in ('object', 'parameter', 'command', 'event'):
                continue

            mark = elem.attrib.get(f'{gw3_ns}Mark', '')
            svc = elem.attrib.get(f'{gw3_ns}Microservice', '')
            if not mark:
                continue

            marks[mark] = marks.get(mark, 0) + 1
            if svc:
                if svc not in services:
                    services[svc] = {}
                services[svc][mark] = services[svc].get(mark, 0) + 1

                # Collect MISMATCH details for tooltips
                if mark == 'MISMATCH':
                    elem_name = elem.attrib.get('name', '')
                    item_path = obj_name + elem_name if etag != 'object' else obj_name
                    diff = elem.attrib.get(f'{gw3_ns}DIFF_Details', '')
                    mismatch_details.setdefault(svc, []).append((item_path, diff))

    return {'marks': marks, 'services': services, 'mismatch_details': mismatch_details}


MARK_DEFS = {
    'IMPLEMENTED': 'ODL entry with action callbacks, or readOnly param with confirmed C source write, or event emitted via C source',
    'NOT IMPLEMENTED': 'ODL entry exists; readWrite param without callbacks and no confirmed C writes',
    'DEFAULT ONLY': 'readOnly param in ODL, no action callbacks, no confirmed C source write — AMX serves default/init value only',
    'REPORT ONLY': 'readWrite param in ODL, no action callbacks, but confirmed C source write — value readable, external writes have no side effects',
    'NOT DEFINED': 'In TR-181 spec but no ODL definition found in any microservice',
    'MISMATCH': 'ODL entry exists but access type differs from TR-181 spec (e.g., spec says readWrite but ODL says readOnly)',
    'VENDOR EXTENSION': 'ODL entry with X_ prefix, not in TR-181 standard — vendor-specific addition',
    'PRPL EXTENSION': 'ODL entry without X_ prefix, not in TR-181 standard — prplOS platform addition',
}

# Mark row colors for Coverage Summary table (Confluence bg colors)
MARK_COLORS = {
    'IMPLEMENTED':     '#dff0d8',
    'NOT IMPLEMENTED': '#fcf8e3',
    'DEFAULT ONLY':    '#e8e8e8',
    'REPORT ONLY':     '#dce4ed',
    'NOT DEFINED':     '#f2dede',
    'MISMATCH':        '#fce4d6',
    'VENDOR EXTENSION': '#d9edf7',
    'PRPL EXTENSION':  '#e8dff0',
}


def _esc(text):
    """HTML-escape text for storage format."""
    return html.escape(str(text), quote=True)


def generate_confluence_storage_html(version, stats):
    """Generate Confluence storage-format HTML with proper column widths and tooltips.

    Produces the full page body as Confluence storage-format XHTML,
    bypassing markdown conversion for precise table control.
    """
    now = datetime.now(timezone.utc).strftime('%Y-%m-%d %H:%M:%S UTC')
    p = []  # parts list

    # --- Info panel ---
    p.append(
        '<ac:structured-macro ac:name="info">'
        '<ac:rich-text-body>'
        '<p><strong>AI-Generated Page</strong><br />'
        'Tool: Claude Code (claude-opus-4-6) — gw3-datamodel-report skill<br />'
        f'Generated: {_esc(now)}<br />'
        'Source: <code>.github/skills/gw3-datamodel-report/</code><br />'
        'Warning: This page is auto-generated. Do not edit manually — changes will be overwritten.</p>'
        '</ac:rich-text-body>'
        '</ac:structured-macro>'
    )

    # --- Title & metadata ---
    p.append(f'<h1>GW3 TR-181 Data Model Coverage Report</h1>')
    p.append(
        f'<p><strong>TR-181 Version:</strong> {_esc(version)}<br />'
        f'<strong>Generated:</strong> {_esc(now)}</p>'
    )

    # --- TOC macro ---
    p.append(
        '<ac:structured-macro ac:name="toc">'
        '<ac:parameter ac:name="maxLevel">3</ac:parameter>'
        '</ac:structured-macro>'
    )

    if stats:
        marks = stats['marks']
        total = sum(marks.values())
        mismatch_details = stats.get('mismatch_details', {})

        impl = marks.get('IMPLEMENTED', 0)
        not_impl = marks.get('NOT IMPLEMENTED', 0)
        default_only = marks.get('DEFAULT ONLY', 0)
        report_only = marks.get('REPORT ONLY', 0)
        not_def = marks.get('NOT DEFINED', 0)
        mismatch = marks.get('MISMATCH', 0)
        vendor = marks.get('VENDOR EXTENSION', 0)
        prpl_ext = marks.get('PRPL EXTENSION', 0)
        impl_pct = impl / total * 100 if total else 0
        cov_pct = (impl + not_impl + default_only + report_only) / total * 100 if total else 0

        # --- Coverage Summary table ---
        p.append('<h2>Coverage Summary</h2>')
        p.append('<table>')
        p.append(
            '<thead><tr>'
            '<th style="width: 130px;">Mark</th>'
            '<th style="width: 20px; text-align: right;">Count</th>'
            '<th style="width: 20px; text-align: right;">%</th>'
            '<th>Definition</th>'
            '</tr></thead>'
        )
        p.append('<tbody>')

        mark_rows = [
            ('IMPLEMENTED',     impl,         f'{impl_pct:.1f}%'),
            ('NOT IMPLEMENTED', not_impl,     f'{not_impl / total * 100:.1f}%' if total else '0%'),
            ('DEFAULT ONLY',    default_only, f'{default_only / total * 100:.1f}%' if total else '0%'),
            ('REPORT ONLY',     report_only,  f'{report_only / total * 100:.1f}%' if total else '0%'),
            ('NOT DEFINED',     not_def,      f'{not_def / total * 100:.1f}%' if total else '0%'),
            ('MISMATCH',        mismatch,     f'{mismatch / total * 100:.1f}%' if total else '0%'),
            ('VENDOR EXTENSION', vendor,      '\u2014'),
            ('PRPL EXTENSION',  prpl_ext,     '\u2014'),
        ]
        for mark_name, count, pct in mark_rows:
            bg = MARK_COLORS.get(mark_name, '')
            style = f' style="background-color: {bg};"' if bg else ''
            p.append(
                f'<tr>'
                f'<td{style}><strong>{_esc(mark_name)}</strong></td>'
                f'<td style="text-align: right;">{count}</td>'
                f'<td style="text-align: right;">{_esc(pct)}</td>'
                f'<td>{_esc(MARK_DEFS.get(mark_name, ""))}</td>'
                f'</tr>'
            )
        # Total row
        p.append(
            '<tr>'
            '<td><strong>Total</strong></td>'
            f'<td style="text-align: right;"><strong>{total}</strong></td>'
            '<td></td><td></td>'
            '</tr>'
        )
        p.append('</tbody></table>')

        p.append(
            f'<p><strong>Implementation rate:</strong> {impl_pct:.1f}% &nbsp;&nbsp; '
            f'<strong>Coverage (defined in ODL):</strong> {cov_pct:.1f}%</p>'
        )

        # --- Coverage by Microservice table ---
        if stats.get('services'):
            p.append('<h2>Coverage by Microservice</h2>')
            p.append('<table>')
            p.append(
                '<thead><tr>'
                '<th>Service</th>'
                '<th style="width: 20px; text-align: right;">Total</th>'
                '<th style="width: 20px; text-align: right;">Implemented</th>'
                '<th style="width: 20px; text-align: right;">Not Impl</th>'
                '<th style="width: 20px; text-align: right;">Default Only</th>'
                '<th style="width: 20px; text-align: right;">Report Only</th>'
                '<th style="width: 20px; text-align: right;">Not Defined</th>'
                '<th style="width: 20px; text-align: right;">Warnings</th>'
                '</tr></thead>'
            )
            p.append('<tbody>')

            for svc, sc in sorted(stats['services'].items(), key=lambda x: -sum(x[1].values())):
                t = sum(sc.values())
                s_imp = sc.get('IMPLEMENTED', 0)
                s_ni = sc.get('NOT IMPLEMENTED', 0)
                s_do = sc.get('DEFAULT ONLY', 0)
                s_ro = sc.get('REPORT ONLY', 0)
                s_nd = sc.get('NOT DEFINED', 0)
                s_warn = sc.get('MISMATCH', 0)

                # Build warning cell with tooltip
                if s_warn > 0:
                    details = mismatch_details.get(svc, [])
                    tip_lines = []
                    for path, diff in details:
                        detail_str = f': {diff}' if diff else ''
                        tip_lines.append(f'{path}{detail_str}')
                    tooltip = _esc('\n'.join(tip_lines)).replace('\n', '&#10;')
                    warn_cell = (
                        f'<td style="text-align: right;">'
                        f'<span style="color: red; font-weight: bold; cursor: help;" '
                        f'title="{tooltip}">{s_warn}</span></td>'
                    )
                else:
                    warn_cell = '<td></td>'

                p.append(
                    f'<tr>'
                    f'<td>{_esc(svc)}</td>'
                    f'<td style="text-align: right;">{t}</td>'
                    f'<td style="text-align: right;">{s_imp}</td>'
                    f'<td style="text-align: right;">{s_ni}</td>'
                    f'<td style="text-align: right;">{s_do}</td>'
                    f'<td style="text-align: right;">{s_ro}</td>'
                    f'<td style="text-align: right;">{s_nd}</td>'
                    f'{warn_cell}'
                    f'</tr>'
                )
            p.append('</tbody></table>')

    # --- Warnings Detail section ---
    if stats:
        mismatch_details = stats.get('mismatch_details', {})
        if mismatch_details:
            total_warnings = sum(len(v) for v in mismatch_details.values())
            p.append('<h2>Warnings Detail</h2>')
            p.append(
                f'<p>{total_warnings} MISMATCH warning(s) across '
                f'{len(mismatch_details)} microservice(s). '
                f'Each entry indicates the TR-181 access type differs from the ODL definition.</p>'
            )
            p.append('<table>')
            p.append(
                '<thead><tr>'
                '<th style="width: 150px;">Service</th>'
                '<th>Path</th>'
                '<th style="width: 200px;">Detail</th>'
                '</tr></thead>'
            )
            p.append('<tbody>')
            for svc in sorted(mismatch_details.keys()):
                entries = mismatch_details[svc]
                for i, (path, diff) in enumerate(sorted(entries)):
                    svc_cell = _esc(svc) if i == 0 else ''
                    if i == 0 and len(entries) > 1:
                        svc_cell = (
                            f'<td rowspan="{len(entries)}" '
                            f'style="vertical-align: top;">{_esc(svc)}</td>'
                        )
                    diff_display = _esc(diff).replace('spec_RW_odl_RO', 'spec=readWrite, ODL=readOnly')
                    diff_display = diff_display.replace('spec_RO_odl_RW', 'spec=readOnly, ODL=readWrite')
                    diff_display = diff_display.replace('access:', '')
                    if i == 0 and len(entries) > 1:
                        p.append(
                            f'<tr>{svc_cell}'
                            f'<td>{_esc(path)}</td>'
                            f'<td>{diff_display}</td></tr>'
                        )
                    elif i == 0:
                        p.append(
                            f'<tr><td>{_esc(svc)}</td>'
                            f'<td>{_esc(path)}</td>'
                            f'<td>{diff_display}</td></tr>'
                        )
                    else:
                        p.append(
                            f'<tr>'
                            f'<td>{_esc(path)}</td>'
                            f'<td>{diff_display}</td></tr>'
                        )
            p.append('</tbody></table>')

    # --- Attached Files placeholder ---
    p.append('<h2>Attached Files</h2>')
    p.append('<p>See attachment links below.</p>')

    return '\n'.join(p)


def generate_summary_markdown(version, stats):
    """Generate a plain-text markdown preview (used by --dry-run only)."""
    now = datetime.now(timezone.utc).strftime('%Y-%m-%d %H:%M:%S UTC')

    lines = [
        f'# GW3 TR-181 Data Model Coverage Report',
        f'TR-181 Version: {version}  |  Generated: {now}',
        '',
    ]

    if stats:
        marks = stats['marks']
        total = sum(marks.values())

        lines.append('## Coverage Summary')
        lines.append(f'{"Mark":<20s} {"Count":>6s} {"%":>6s}  Definition')
        lines.append('-' * 90)
        mark_order = [
            'IMPLEMENTED', 'NOT IMPLEMENTED', 'DEFAULT ONLY', 'REPORT ONLY',
            'NOT DEFINED', 'MISMATCH', 'VENDOR EXTENSION', 'PRPL EXTENSION',
        ]
        for m in mark_order:
            c = marks.get(m, 0)
            pct = f'{c / total * 100:.1f}%' if total else '0%'
            if m in ('VENDOR EXTENSION', 'PRPL EXTENSION'):
                pct = '\u2014'
            lines.append(f'{m:<20s} {c:>6d} {pct:>6s}  {MARK_DEFS.get(m, "")}')
        lines.append(f'{"Total":<20s} {total:>6d}')
        lines.append('')

        if stats.get('services'):
            lines.append('## Coverage by Microservice')
            lines.append(
                f'{"Service":<25s} {"Tot":>4s} {"Impl":>4s} {"NI":>4s} '
                f'{"DO":>4s} {"RO":>4s} {"ND":>4s} {"Warn":>4s}'
            )
            lines.append('-' * 72)
            for svc, sc in sorted(stats['services'].items(), key=lambda x: -sum(x[1].values())):
                t = sum(sc.values())
                lines.append(
                    f'{svc:<25s} {t:>4d} {sc.get("IMPLEMENTED", 0):>4d} '
                    f'{sc.get("NOT IMPLEMENTED", 0):>4d} '
                    f'{sc.get("DEFAULT ONLY", 0):>4d} '
                    f'{sc.get("REPORT ONLY", 0):>4d} '
                    f'{sc.get("NOT DEFINED", 0):>4d} '
                    f'{sc.get("MISMATCH", 0):>4d}'
                )
            lines.append('')

    lines.append('## Attached Files')
    lines.append('[attachment links injected after upload]')

    return '\n'.join(lines)


def upload_to_confluence(storage_html, page_id, url, username, token, attachments=None):
    """Upload storage-format HTML directly to Confluence and attach files.

    Uses the REST API directly (no markdown converter) for full control
    over table column widths, tooltips, and formatting.
    """
    print(f"Uploading storage HTML to Confluence page {page_id}...")

    # Fetch current page to get version and title
    try:
        page = _get_page(url, username, token, page_id)
    except Exception as e:
        print(f"ERROR: Cannot fetch page {page_id}: {e}", file=sys.stderr)
        return False

    title = page['title']
    cur_version = page['version']['number']

    # Upload page content
    try:
        new_ver = _update_page_body(
            url, username, token, page_id, title,
            cur_version + 1, storage_html,
        )
        print(f"  Page updated (version {cur_version} → {new_ver})")
    except Exception as e:
        print(f"ERROR: Upload failed: {e}", file=sys.stderr)
        return False

    # Set page to full-width layout
    try:
        _set_full_width(url, username, token, page_id)
        print("  Page set to full-width")
    except Exception as e:
        print(f"  WARNING: Could not set full-width: {e}", file=sys.stderr)

    # Upload attachments
    attached_names = []
    if attachments:
        for att_path in attachments:
            if os.path.exists(att_path):
                att_name = os.path.basename(att_path)
                print(f"  Attaching: {att_name}")
                _attach_file(url, username, token, page_id, att_path)
                attached_names.append(att_name)

    # Inject clickable attachment links into the page body
    if attached_names:
        try:
            _inject_attachment_links(url, username, token, page_id, attached_names)
        except Exception as e:
            print(f"  WARNING: Failed to inject attachment links: {e}", file=sys.stderr)

    return True


def _attach_file(url, username, token, page_id, file_path):
    """Attach a file to a Confluence page via REST API.

    Creates a new attachment or updates an existing one with the same filename.
    """
    import base64
    from urllib.request import Request, urlopen
    from urllib.error import URLError, HTTPError

    api_base = _confluence_api_base(url)
    filename = os.path.basename(file_path)
    auth = base64.b64encode(f"{username}:{token}".encode()).decode()

    # Check if attachment already exists
    existing_id = None
    try:
        check_url = (
            f"{api_base}/rest/api/content/{page_id}/child/attachment"
            f"?filename={filename}"
        )
        req = Request(check_url)
        req.add_header('Authorization', f'Basic {auth}')
        req.add_header('Accept', 'application/json')
        with urlopen(req, timeout=30) as resp:
            data = json.loads(resp.read().decode('utf-8'))
            results = data.get('results', [])
            if results:
                existing_id = results[0]['id']
    except (URLError, HTTPError):
        pass

    # Build multipart body
    boundary = '----WebKitFormBoundary7MA4YWxkTrZu0gW'
    with open(file_path, 'rb') as f:
        file_data = f.read()

    body = (
        f'--{boundary}\r\n'
        f'Content-Disposition: form-data; name="file"; filename="{filename}"\r\n'
        f'Content-Type: application/octet-stream\r\n\r\n'
    ).encode() + file_data + f'\r\n--{boundary}--\r\n'.encode()

    # Use update endpoint if attachment exists, create endpoint otherwise
    if existing_id:
        attach_url = (
            f"{api_base}/rest/api/content/{page_id}"
            f"/child/attachment/{existing_id}/data"
        )
    else:
        attach_url = f"{api_base}/rest/api/content/{page_id}/child/attachment"

    req = Request(attach_url, data=body, method='POST')
    req.add_header('Authorization', f'Basic {auth}')
    req.add_header('Content-Type', f'multipart/form-data; boundary={boundary}')
    req.add_header('X-Atlassian-Token', 'nocheck')

    try:
        with urlopen(req, timeout=120) as resp:
            action = "Updated" if existing_id else "Attached"
            print(f"    {action}: {filename} ({len(file_data) / 1024:.0f} KB)")
    except (URLError, HTTPError) as e:
        print(f"    WARNING: Failed to attach {filename}: {e}", file=sys.stderr)


def _confluence_api_base(url):
    """Return the Confluence REST API base URL."""
    base = url.rstrip('/')
    if '/wiki' not in base:
        base += '/wiki'
    return base


def _set_full_width(url, username, token, page_id):
    """Set a Confluence page to full-width layout via content properties API.

    Uses the ``content-appearance-published`` property set to ``full-width``.
    Creates the property if it doesn't exist, updates it otherwise.
    """
    import base64
    from urllib.request import Request, urlopen
    from urllib.error import HTTPError

    base = _confluence_api_base(url)
    auth = base64.b64encode(f"{username}:{token}".encode()).decode()
    prop_key = 'content-appearance-published'
    prop_url = f"{base}/rest/api/content/{page_id}/property/{prop_key}"

    # Check if property exists
    existing_version = None
    try:
        req = Request(prop_url)
        req.add_header('Authorization', f'Basic {auth}')
        req.add_header('Accept', 'application/json')
        with urlopen(req, timeout=15) as resp:
            data = json.loads(resp.read().decode('utf-8'))
            existing_version = data.get('version', {}).get('number', 1)
            if data.get('value') == 'full-width':
                return  # already full-width
    except HTTPError as e:
        if e.code != 404:
            raise

    if existing_version is not None:
        # Update existing property
        payload = json.dumps({
            "key": prop_key,
            "value": "full-width",
            "version": {"number": existing_version + 1, "minorEdit": True},
        })
        req = Request(prop_url, data=payload.encode('utf-8'), method='PUT')
    else:
        # Create new property
        create_url = f"{base}/rest/api/content/{page_id}/property"
        payload = json.dumps({
            "key": prop_key,
            "value": "full-width",
        })
        req = Request(create_url, data=payload.encode('utf-8'), method='POST')

    req.add_header('Authorization', f'Basic {auth}')
    req.add_header('Content-Type', 'application/json')
    with urlopen(req, timeout=15) as resp:
        resp.read()


def _get_page(url, username, token, page_id):
    """Fetch the current page content and version from Confluence."""
    import base64
    from urllib.request import Request, urlopen

    base = _confluence_api_base(url)
    api_url = f"{base}/rest/api/content/{page_id}?expand=body.storage,version"
    auth = base64.b64encode(f"{username}:{token}".encode()).decode()

    req = Request(api_url)
    req.add_header('Authorization', f'Basic {auth}')
    req.add_header('Accept', 'application/json')

    with urlopen(req, timeout=30) as resp:
        return json.loads(resp.read().decode('utf-8'))


def _update_page_body(url, username, token, page_id, title, version, new_body):
    """Update a Confluence page body with new storage-format HTML."""
    import base64
    from urllib.request import Request, urlopen

    base = _confluence_api_base(url)
    api_url = f"{base}/rest/api/content/{page_id}"
    auth = base64.b64encode(f"{username}:{token}".encode()).decode()

    payload = json.dumps({
        "version": {"number": version},
        "title": title,
        "type": "page",
        "body": {
            "storage": {
                "value": new_body,
                "representation": "storage",
            }
        }
    })

    req = Request(api_url, data=payload.encode('utf-8'), method='PUT')
    req.add_header('Authorization', f'Basic {auth}')
    req.add_header('Content-Type', 'application/json')

    with urlopen(req, timeout=30) as resp:
        result = json.loads(resp.read().decode('utf-8'))
        return result.get('version', {}).get('number', version)


ATTACHMENT_DESCRIPTIONS = {
    'gw3_current_datamodel.xlsx': 'Enhanced Excel with full coverage report (8-mark model, color-coded)',
    'gw3_current_datamodel.xml': 'Enhanced XML with gw3: namespace coverage annotations',
    'gw3_current_datamodel.json': 'Enhanced data model in JSON format (hierarchical)',
    'tr181_definition.xlsx': 'Standard TR-181 definition Excel (8 columns, no GW3 annotations)',
    'tr181_original.json': 'Original TR-181 data model in JSON format (no GW3 annotations)',
}

# Pattern-based descriptions for version-specific downloaded XMLs
ATTACHMENT_DESC_PATTERNS = [
    ('tr-181-*-usp-full.xml', 'BBF TR-181 USP full XML — complete data model with all imported components (~5 MB)'),
    ('tr-181-*-usp.xml', 'BBF TR-181 USP XML — compact definitions only, references external components (~260 KB)'),
]


def _get_attachment_description(fname):
    """Get description for an attachment file, checking exact matches then patterns."""
    desc = ATTACHMENT_DESCRIPTIONS.get(fname)
    if desc:
        return desc
    import fnmatch
    for pattern, pdesc in ATTACHMENT_DESC_PATTERNS:
        if fnmatch.fnmatch(fname, pattern):
            return pdesc
    return ''


def _build_attachment_storage_html(filenames):
    """Build Confluence storage-format HTML with attachment download links."""
    rows = []
    for fname in filenames:
        desc = _get_attachment_description(fname)
        link = (
            f'<ac:link><ri:attachment ri:filename="{fname}" />'
            f'<ac:plain-text-link-body><![CDATA[{fname}]]>'
            f'</ac:plain-text-link-body></ac:link>'
        )
        rows.append(f'<tr><td>{link}</td><td>{desc}</td></tr>')

    return (
        '<table>'
        '<thead><tr><th>File</th><th>Description</th></tr></thead>'
        '<tbody>' + ''.join(rows) + '</tbody>'
        '</table>'
    )


def _inject_attachment_links(url, username, token, page_id, filenames):
    """Fetch the page, replace the attachment placeholder with real links, and update."""
    page = _get_page(url, username, token, page_id)
    body = page['body']['storage']['value']
    title = page['title']
    cur_version = page['version']['number']

    # Find the placeholder paragraph and replace it
    placeholder = 'See attachment links below.'
    if placeholder not in body:
        # Try HTML-encoded version
        placeholder = '<p>See attachment links below.</p>'
    attachment_html = _build_attachment_storage_html(filenames)

    if 'See attachment links below.' in body:
        new_body = body.replace('See attachment links below.', attachment_html)
    elif placeholder in body:
        new_body = body.replace(placeholder, attachment_html)
    else:
        # Append at the end
        new_body = body + attachment_html

    new_ver = _update_page_body(
        url, username, token, page_id, title, cur_version + 1, new_body
    )
    print(f"  Injected attachment links (page version → {new_ver})")


def update_confluence_pages_yaml(page_id, version):
    """Add/update the entry in confluence-pages.yaml."""
    yaml_path = os.path.join(PROJECT_ROOT, 'confluence-pages.yaml')
    if not os.path.exists(yaml_path):
        print(f"WARNING: {yaml_path} not found, skipping YAML update.", file=sys.stderr)
        return

    with open(yaml_path, 'r') as f:
        content = f.read()

    # Check if page already has an entry
    if str(page_id) in content:
        print(f"  confluence-pages.yaml already has entry for page {page_id}")
        return

    # Append new entry
    entry = f"""
# Data Model Report
  - file: specs/datamodel/MANIFEST.md
    page_id: {page_id}
    title: "GW3 TR-181 Data Model Coverage Report (v{version})"
    parent_id: 4178870422
    upload_method: step7_upload_confluence.py
    diagrams: 0
"""
    with open(yaml_path, 'a') as f:
        f.write(entry)
    print(f"  Updated confluence-pages.yaml with page {page_id}")


def main():
    parser = argparse.ArgumentParser(description='Upload data model report to Confluence')
    parser.add_argument('--page-id', default=CONFLUENCE_PAGE_ID, help='Confluence page ID')
    parser.add_argument('--dry-run', action='store_true', help='Preview content, do not upload')
    args = parser.parse_args()

    url, username, token = read_confluence_credentials()
    version = detect_tr181_version()

    print(f"TR-181 version: {version}")
    print(f"Target page: {args.page_id}")

    # Collect statistics
    stats = collect_stats_from_excel()

    if args.dry_run:
        preview = generate_summary_markdown(version, stats)
        print("\n--- DRY RUN: Preview ---")
        print(preview)
        return

    # Generate Confluence storage-format HTML (direct, no markdown conversion)
    storage_html = generate_confluence_storage_html(version, stats)

    # Save a copy for reference
    ensure_output_dir()
    html_path = os.path.join(LOCAL_DATAMODEL_DIR, 'confluence_summary.html')
    with open(html_path, 'w') as f:
        f.write(storage_html)
    print(f"Storage HTML: {html_path} ({len(storage_html)} chars)")

    # Collect attachments from specs/datamodel/ (preferred) or local/datamodel/
    source_dir = SPECS_DATAMODEL_DIR if os.path.isdir(SPECS_DATAMODEL_DIR) else LOCAL_DATAMODEL_DIR
    attachments = []
    for fname in sorted(os.listdir(source_dir)):
        if fname.endswith(('.xlsx', '.xml', '.json')) and not fname.startswith('.'):
            attachments.append(os.path.join(source_dir, fname))

    # Upload
    success = upload_to_confluence(storage_html, args.page_id, url, username, token, attachments)

    if success:
        update_confluence_pages_yaml(args.page_id, version)
        print(f"\n=== Upload complete ===")
        print(f"Page: {url}/wiki/spaces/R3G/pages/{args.page_id}")
    else:
        print("\nUpload failed.", file=sys.stderr)
        sys.exit(1)


if __name__ == '__main__':
    main()
