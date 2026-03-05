#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.10"
# dependencies = [
#     "atlassian-python-api>=3.41.0",
#     "click>=8.1.0",
# ]
# ///
"""Jira issue link operations - create links and list link types."""

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
from lib.output import format_output, format_table, success, error, warning

# ═══════════════════════════════════════════════════════════════════════════════
# CLI Definition
# ═══════════════════════════════════════════════════════════════════════════════

@click.group()
@click.option('--json', 'output_json', is_flag=True, help='Output as JSON')
@click.option('--quiet', '-q', is_flag=True, help='Minimal output')
@click.option('--env-file', type=click.Path(), help='Environment file path')
@click.option('--debug', is_flag=True, help='Show debug information on errors')
@click.pass_context
def cli(ctx, output_json: bool, quiet: bool, env_file: str | None, debug: bool):
    """Jira issue link operations.

    Create links between issues and list available link types.
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
@click.argument('from_key')
@click.argument('to_key')
@click.option('--type', '-t', 'link_type', required=True,
              help='Link type name (e.g., "Blocks", "Relates")')
@click.option('--dry-run', is_flag=True, help='Show what would be created')
@click.pass_context
def create(ctx, from_key: str, to_key: str, link_type: str, dry_run: bool):
    """Create a link between two issues.

    FROM_KEY: Source issue key

    TO_KEY: Target issue key

    Examples:

      jira-link create PROJ-123 PROJ-456 --type "Blocks"

      jira-link create PROJ-123 PROJ-456 --type "Relates" --dry-run
    """
    client = ctx.obj['client']

    if dry_run:
        warning("DRY RUN - No link will be created")
        print(f"\nWould create link:")
        print(f"  {from_key} --[{link_type}]--> {to_key}")
        return

    try:
        client.create_issue_link({
            "type": {"name": link_type},
            "inwardIssue": {"key": to_key},
            "outwardIssue": {"key": from_key}
        })

        if ctx.obj['json']:
            format_output({
                'from': from_key,
                'to': to_key,
                'type': link_type,
                'created': True
            }, as_json=True)
        elif ctx.obj['quiet']:
            print('ok')
        else:
            success(f"Created link: {from_key} --[{link_type}]--> {to_key}")

    except Exception as e:
        if ctx.obj['debug']:
            raise
        error(f"Failed to create link: {e}")
        sys.exit(1)


@cli.command('list-types')
@click.pass_context
def list_types(ctx):
    """List available link types.

    Shows all issue link types configured in your Jira instance.

    Example:

      jira-link list-types
    """
    client = ctx.obj['client']

    try:
        link_types = client.get_issue_link_types()

        if ctx.obj['json']:
            format_output(link_types, as_json=True)
        elif ctx.obj['quiet']:
            for lt in link_types:
                print(lt.get('name', ''))
        else:
            print("Available link types:\n")
            rows = []
            for lt in link_types:
                rows.append({
                    'Name': lt.get('name', ''),
                    'Inward': lt.get('inward', ''),
                    'Outward': lt.get('outward', '')
                })
            print(format_table(rows, ['Name', 'Inward', 'Outward']))

    except Exception as e:
        if ctx.obj['debug']:
            raise
        error(f"Failed to get link types: {e}")
        sys.exit(1)


if __name__ == '__main__':
    cli()
