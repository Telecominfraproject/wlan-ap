#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.10"
# dependencies = [
#     "atlassian-python-api>=3.41.0",
#     "click>=8.1.0",
# ]
# ///
"""Jira field operations - search and list fields."""

import sys
from pathlib import Path

# ═══════════════════════════════════════════════════════════════════════════════
# Shared library import (TR1.1.1 - PYTHONPATH approach)
# ═══════════════════════════════════════════════════════════════════════════════
_script_dir = Path(__file__).parent
_lib_path = _script_dir.parent / "lib"
if _lib_path.exists():
    sys.path.insert(0, str(_lib_path.parent))

import click
from lib.client import get_jira_client
from lib.output import format_output, format_table, error

# ═══════════════════════════════════════════════════════════════════════════════
# CLI Definition
# ═══════════════════════════════════════════════════════════════════════════════

@click.group()
@click.option('--json', 'output_json', is_flag=True, help='Output as JSON')
@click.option('--quiet', '-q', is_flag=True, help='Minimal output (field IDs only)')
@click.option('--env-file', type=click.Path(), help='Environment file path')
@click.option('--debug', is_flag=True, help='Show debug information on errors')
@click.pass_context
def cli(ctx, output_json: bool, quiet: bool, env_file: str | None, debug: bool):
    """Jira field operations.

    Search and list Jira fields (including custom fields).
    """
    ctx.ensure_object(dict)
    ctx.obj['json'] = output_json
    ctx.obj['quiet'] = quiet
    ctx.obj['debug'] = debug
    try:
        ctx.obj['client'] = get_jira_client(env_file)
    except Exception as e:
        if debug:
            raise
        error(str(e))
        sys.exit(1)


@cli.command()
@click.argument('keyword')
@click.option('--limit', '-n', default=20, help='Max results to show')
@click.pass_context
def search(ctx, keyword: str, limit: int):
    """Search fields by keyword.

    KEYWORD: Search term (matches name or ID)

    Useful for finding custom field IDs for --fields-json options.

    Examples:

      jira-fields search sprint

      jira-fields search "story points"

      jira-fields search customfield
    """
    client = ctx.obj['client']

    try:
        # Get all fields
        fields = client.get_all_fields()

        # Filter by keyword (case-insensitive)
        keyword_lower = keyword.lower()
        matching = [
            f for f in fields
            if keyword_lower in f.get('name', '').lower()
            or keyword_lower in f.get('id', '').lower()
        ][:limit]

        if ctx.obj['json']:
            format_output(matching, as_json=True)
        elif ctx.obj['quiet']:
            for f in matching:
                print(f.get('id', ''))
        else:
            if not matching:
                print(f"No fields matching '{keyword}'")
            else:
                print(f"Fields matching '{keyword}' ({len(matching)} shown):\n")
                rows = []
                for f in matching:
                    rows.append({
                        'ID': f.get('id', ''),
                        'Name': f.get('name', ''),
                        'Type': f.get('schema', {}).get('type', '-'),
                        'Custom': 'Yes' if f.get('custom', False) else 'No'
                    })
                print(format_table(rows, ['ID', 'Name', 'Type', 'Custom']))

    except Exception as e:
        if ctx.obj['debug']:
            raise
        error(f"Failed to search fields: {e}")
        sys.exit(1)


@cli.command('list')
@click.option('--type', '-t', 'field_type', type=click.Choice(['custom', 'system', 'all']),
              default='all', help='Filter by field type')
@click.option('--limit', '-n', default=50, help='Max results to show')
@click.pass_context
def list_fields(ctx, field_type: str, limit: int):
    """List available fields.

    Examples:

      jira-fields list

      jira-fields list --type custom

      jira-fields list --type system --limit 100
    """
    client = ctx.obj['client']

    try:
        fields = client.get_all_fields()

        # Filter by type
        if field_type == 'custom':
            fields = [f for f in fields if f.get('custom', False)]
        elif field_type == 'system':
            fields = [f for f in fields if not f.get('custom', False)]

        fields = fields[:limit]

        if ctx.obj['json']:
            format_output(fields, as_json=True)
        elif ctx.obj['quiet']:
            for f in fields:
                print(f.get('id', ''))
        else:
            type_label = field_type if field_type != 'all' else 'all'
            print(f"Jira fields ({type_label}, {len(fields)} shown):\n")
            rows = []
            for f in fields:
                rows.append({
                    'ID': f.get('id', ''),
                    'Name': f.get('name', ''),
                    'Custom': 'Yes' if f.get('custom', False) else 'No'
                })
            print(format_table(rows, ['ID', 'Name', 'Custom']))

    except Exception as e:
        if ctx.obj['debug']:
            raise
        error(f"Failed to list fields: {e}")
        sys.exit(1)


if __name__ == '__main__':
    cli()
