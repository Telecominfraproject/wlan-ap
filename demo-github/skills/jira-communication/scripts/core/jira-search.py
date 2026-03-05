#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.10"
# dependencies = [
#     "atlassian-python-api>=3.41.0",
#     "click>=8.1.0",
# ]
# ///
"""Jira search operations - query issues using JQL."""

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
@click.option('--quiet', '-q', is_flag=True, help='Minimal output (keys only)')
@click.option('--env-file', type=click.Path(), help='Environment file path')
@click.option('--debug', is_flag=True, help='Show debug information on errors')
@click.pass_context
def cli(ctx, output_json: bool, quiet: bool, env_file: str | None, debug: bool):
    """Jira search operations.

    Query Jira issues using JQL (Jira Query Language).
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
@click.argument('jql')
@click.option('--max-results', '-n', default=50, help='Maximum results to return')
@click.option('--fields', '-f', default='key,summary,status,assignee,priority',
              help='Comma-separated fields to return')
@click.option('--truncate', type=int, metavar='N', help='Truncate field values to N characters')
@click.pass_context
def query(ctx, jql: str, max_results: int, fields: str, truncate: int | None):
    """Search issues using JQL.

    JQL: Jira Query Language query string

    Examples:

      jira-search query "project = PROJ AND status = 'In Progress'"

      jira-search query "assignee = currentUser()" --max-results 20

      jira-search --json query "updated >= -7d"

      jira-search --quiet query "labels = urgent"

    Common JQL patterns:

      project = PROJ                    # Issues in project
      assignee = currentUser()          # My issues
      status = "In Progress"            # By status
      updated >= -7d                    # Updated last 7 days
      sprint in openSprints()           # Current sprint
      labels = backend                  # By label
      priority = High                   # By priority
    """
    client = ctx.obj['client']

    try:
        # Parse fields
        field_list = [f.strip() for f in fields.split(',')]

        # Execute search
        results = client.jql(jql, limit=max_results, fields=field_list)
        issues = results.get('issues', [])

        if ctx.obj['json']:
            format_output(issues, as_json=True)
        elif ctx.obj['quiet']:
            for issue in issues:
                print(issue['key'])
        else:
            # Table output
            if not issues:
                print("No issues found")
            else:
                _print_results_table(issues, field_list, truncate=truncate)
                print(f"\n({len(issues)} issue{'s' if len(issues) != 1 else ''} found)")

    except Exception as e:
        if ctx.obj['debug']:
            raise
        error(f"Search failed: {e}")
        sys.exit(1)


def _print_results_table(issues: list, fields: list, truncate: int | None = None) -> None:
    """Print search results as a table.

    Args:
        issues: List of issue dicts from Jira API
        fields: List of field names to display
        truncate: If set, truncate field values to this many characters
    """
    # Build table data
    rows = []
    for issue in issues:
        row = {'key': issue['key']}
        issue_fields = issue.get('fields', {})

        for field in fields:
            if field == 'key':
                continue

            value = issue_fields.get(field)

            # Handle nested objects
            if isinstance(value, dict):
                if 'name' in value:
                    value = value['name']
                elif 'displayName' in value:
                    value = value['displayName']
                elif 'value' in value:
                    value = value['value']
                else:
                    value = str(value)
            elif isinstance(value, list):
                value = ', '.join(str(v) for v in value[:3])
                if len(issue_fields.get(field, [])) > 3:
                    value += '...'
            elif value is None:
                value = '-'
            else:
                value = str(value)

            # Truncate if requested
            if truncate and len(str(value)) > truncate:
                value = str(value)[:truncate-3] + '...'

            row[field] = value

        rows.append(row)

    # Print table
    columns = ['key'] + [f for f in fields if f != 'key']
    print(format_table(rows, columns))


if __name__ == '__main__':
    cli()
