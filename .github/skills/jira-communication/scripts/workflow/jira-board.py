#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.10"
# dependencies = [
#     "atlassian-python-api>=3.41.0",
#     "click>=8.1.0",
# ]
# ///
"""Jira board operations - list boards and get board issues."""

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
@click.option('--quiet', '-q', is_flag=True, help='Minimal output')
@click.option('--env-file', type=click.Path(), help='Environment file path')
@click.option('--debug', is_flag=True, help='Show debug information on errors')
@click.pass_context
def cli(ctx, output_json: bool, quiet: bool, env_file: str | None, debug: bool):
    """Jira board operations.

    List agile boards and get board issues.
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


@cli.command('list')
@click.option('--project', '-p', help='Filter by project key')
@click.option('--type', '-t', 'board_type', type=click.Choice(['scrum', 'kanban']),
              help='Filter by board type')
@click.pass_context
def list_boards(ctx, project: str | None, board_type: str | None):
    """List agile boards.

    Examples:

      jira-board list

      jira-board list --project PROJ

      jira-board list --type scrum
    """
    client = ctx.obj['client']

    try:
        params = {}
        if project:
            params['projectKeyOrId'] = project
        if board_type:
            params['type'] = board_type

        response = client.get("rest/agile/1.0/board", params=params)
        boards = response.get('values', [])

        if ctx.obj['json']:
            format_output(boards, as_json=True)
        elif ctx.obj['quiet']:
            for b in boards:
                print(b.get('id', ''))
        else:
            if not boards:
                print("No boards found")
                if project:
                    print(f"  (filtered by project: {project})")
            else:
                print(f"Agile boards ({len(boards)} found):\n")
                rows = []
                for b in boards:
                    loc = b.get('location', {})
                    rows.append({
                        'ID': b.get('id', ''),
                        'Name': b.get('name', ''),
                        'Type': b.get('type', ''),
                        'Project': loc.get('projectKey', '-')
                    })
                print(format_table(rows, ['ID', 'Name', 'Type', 'Project']))

    except Exception as e:
        if ctx.obj['debug']:
            raise
        error(f"Failed to list boards: {e}")
        sys.exit(1)


@cli.command()
@click.argument('board_id', type=int)
@click.option('--jql', help='Additional JQL filter')
@click.option('--max-results', '-n', default=50, help='Maximum results')
@click.pass_context
def issues(ctx, board_id: int, jql: str | None, max_results: int):
    """Get issues on a board.

    BOARD_ID: The Jira agile board ID

    Examples:

      jira-board issues 42

      jira-board issues 42 --jql "status = 'In Progress'"

      jira-board issues 42 --max-results 20
    """
    client = ctx.obj['client']

    try:
        params = {'maxResults': max_results}
        if jql:
            params['jql'] = jql

        response = client.get(
            f"rest/agile/1.0/board/{board_id}/issue",
            params=params
        )
        issues_list = response.get('issues', [])

        if ctx.obj['json']:
            format_output(issues_list, as_json=True)
        elif ctx.obj['quiet']:
            for issue in issues_list:
                print(issue['key'])
        else:
            if not issues_list:
                print(f"No issues on board {board_id}")
                if jql:
                    print(f"  (filtered by JQL: {jql})")
            else:
                print(f"Issues on board {board_id} ({len(issues_list)} shown):\n")
                rows = []
                for issue in issues_list:
                    fields = issue.get('fields', {})
                    status = fields.get('status', {}).get('name', '-')
                    assignee = fields.get('assignee', {})
                    assignee_name = assignee.get('displayName', '-') if assignee else '-'
                    summary = fields.get('summary', '')
                    if len(summary) > 40:
                        summary = summary[:37] + '...'
                    rows.append({
                        'Key': issue['key'],
                        'Summary': summary,
                        'Status': status,
                        'Assignee': assignee_name
                    })
                print(format_table(rows, ['Key', 'Summary', 'Status', 'Assignee']))

    except Exception as e:
        if ctx.obj['debug']:
            raise
        error(f"Failed to get issues for board {board_id}: {e}")
        sys.exit(1)


if __name__ == '__main__':
    cli()
