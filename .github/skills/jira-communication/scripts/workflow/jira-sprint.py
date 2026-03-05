#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.10"
# dependencies = [
#     "atlassian-python-api>=3.41.0",
#     "click>=8.1.0",
# ]
# ///
"""Jira sprint operations - list sprints and get sprint issues."""

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
    """Jira sprint operations.

    List sprints and get sprint issues from agile boards.
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
@click.argument('board_id', type=int)
@click.option('--state', '-s', type=click.Choice(['active', 'future', 'closed']),
              help='Filter by sprint state')
@click.pass_context
def list_sprints(ctx, board_id: int, state: str | None):
    """List sprints for a board.

    BOARD_ID: The Jira agile board ID

    Examples:

      jira-sprint list 42

      jira-sprint list 42 --state active

      jira-sprint list 42 --state future --json
    """
    client = ctx.obj['client']

    try:
        # Get sprints using agile API
        params = {}
        if state:
            params['state'] = state

        # Use the Jira agile API
        response = client.get(
            f"rest/agile/1.0/board/{board_id}/sprint",
            params=params
        )
        sprints = response.get('values', [])

        if ctx.obj['json']:
            format_output(sprints, as_json=True)
        elif ctx.obj['quiet']:
            for s in sprints:
                print(s.get('id', ''))
        else:
            if not sprints:
                print(f"No sprints found for board {board_id}")
                if state:
                    print(f"  (filtered by state: {state})")
            else:
                print(f"Sprints for board {board_id}:\n")
                rows = []
                for s in sprints:
                    start = s.get('startDate', '')[:10] if s.get('startDate') else '-'
                    end = s.get('endDate', '')[:10] if s.get('endDate') else '-'
                    rows.append({
                        'ID': s.get('id', ''),
                        'Name': s.get('name', ''),
                        'State': s.get('state', ''),
                        'Start': start,
                        'End': end
                    })
                print(format_table(rows, ['ID', 'Name', 'State', 'Start', 'End']))

    except Exception as e:
        if ctx.obj['debug']:
            raise
        error(f"Failed to get sprints for board {board_id}: {e}")
        sys.exit(1)


@cli.command()
@click.argument('sprint_id', type=int)
@click.option('--fields', '-f', default='key,summary,status,assignee',
              help='Comma-separated fields to return')
@click.pass_context
def issues(ctx, sprint_id: int, fields: str):
    """Get issues in a sprint.

    SPRINT_ID: The sprint ID

    Examples:

      jira-sprint issues 123

      jira-sprint issues 123 --fields key,summary,status,priority
    """
    client = ctx.obj['client']

    try:
        field_list = [f.strip() for f in fields.split(',')]

        # Get sprint issues using agile API
        response = client.get(
            f"rest/agile/1.0/sprint/{sprint_id}/issue",
            params={'fields': ','.join(field_list)}
        )
        issues_list = response.get('issues', [])

        if ctx.obj['json']:
            format_output(issues_list, as_json=True)
        elif ctx.obj['quiet']:
            for issue in issues_list:
                print(issue['key'])
        else:
            if not issues_list:
                print(f"No issues in sprint {sprint_id}")
            else:
                print(f"Issues in sprint {sprint_id} ({len(issues_list)} total):\n")
                rows = []
                for issue in issues_list:
                    row = {'key': issue['key']}
                    issue_fields = issue.get('fields', {})
                    for f in field_list:
                        if f == 'key':
                            continue
                        value = issue_fields.get(f)
                        if isinstance(value, dict):
                            value = value.get('name') or value.get('displayName') or str(value)
                        row[f] = value or '-'
                    rows.append(row)
                columns = ['key'] + [f for f in field_list if f != 'key']
                print(format_table(rows, columns))

    except Exception as e:
        if ctx.obj['debug']:
            raise
        error(f"Failed to get issues for sprint {sprint_id}: {e}")
        sys.exit(1)


@cli.command()
@click.argument('board_id', type=int)
@click.pass_context
def current(ctx, board_id: int):
    """Get the current active sprint for a board.

    BOARD_ID: The Jira agile board ID

    Examples:

      jira-sprint current 42
    """
    client = ctx.obj['client']

    try:
        # Get active sprints
        response = client.get(
            f"rest/agile/1.0/board/{board_id}/sprint",
            params={'state': 'active'}
        )
        sprints = response.get('values', [])

        if not sprints:
            print(f"No active sprint for board {board_id}")
            return

        sprint = sprints[0]  # Get first active sprint

        if ctx.obj['json']:
            format_output(sprint, as_json=True)
        elif ctx.obj['quiet']:
            print(sprint.get('id', ''))
        else:
            print(f"Current sprint for board {board_id}:\n")
            print(f"  ID: {sprint.get('id', '')}")
            print(f"  Name: {sprint.get('name', '')}")
            print(f"  Goal: {sprint.get('goal', '-')}")
            start = sprint.get('startDate', '')[:10] if sprint.get('startDate') else '-'
            end = sprint.get('endDate', '')[:10] if sprint.get('endDate') else '-'
            print(f"  Start: {start}")
            print(f"  End: {end}")

    except Exception as e:
        if ctx.obj['debug']:
            raise
        error(f"Failed to get current sprint for board {board_id}: {e}")
        sys.exit(1)


if __name__ == '__main__':
    cli()
