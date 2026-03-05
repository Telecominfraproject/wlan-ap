#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.10"
# dependencies = [
#     "atlassian-python-api>=3.41.0",
#     "click>=8.1.0",
# ]
# ///
"""Jira issue transitions - list available transitions and change issue status."""

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
# Helper Functions
# ═══════════════════════════════════════════════════════════════════════════════


def _get_to_status(transition: dict) -> str:
    """Get target status name from transition, handling both Cloud and Server formats.

    Cloud returns: {'to': {'name': 'In Progress', ...}}
    Server/DC returns: {'to': 'In Progress'}
    """
    to_value = transition.get('to', '')
    if isinstance(to_value, dict):
        return to_value.get('name', '')
    return str(to_value)


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
    """Jira issue transitions.

    List available transitions and change issue status.
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
@click.argument('issue_key')
@click.pass_context
def list_transitions(ctx, issue_key: str):
    """List available transitions for an issue.

    ISSUE_KEY: The Jira issue key (e.g., PROJ-123)

    Shows all valid status transitions from the issue's current state.

    Example:

      jira-transition list PROJ-123
    """
    client = ctx.obj['client']

    try:
        transitions = client.get_issue_transitions(issue_key)

        if ctx.obj['json']:
            format_output(transitions, as_json=True)
        elif ctx.obj['quiet']:
            for t in transitions:
                print(t.get('name', ''))
        else:
            # Get current status
            issue = client.issue(issue_key, fields='status')
            current_status = issue['fields']['status']['name']

            print(f"Available transitions for {issue_key}")
            print(f"Current status: {current_status}\n")

            if not transitions:
                print("No transitions available from this status")
            else:
                rows = []
                for t in transitions:
                    rows.append({
                        'ID': t.get('id', ''),
                        'Name': t.get('name', ''),
                        'To Status': _get_to_status(t)
                    })
                print(format_table(rows, ['ID', 'Name', 'To Status']))

    except Exception as e:
        if ctx.obj['debug']:
            raise
        error(f"Failed to get transitions for {issue_key}: {e}")
        sys.exit(1)


@cli.command('do')
@click.argument('issue_key')
@click.argument('status_name')
@click.option('--comment', '-c', help='Comment to add during transition')
@click.option('--resolution', '-r', help='Resolution name (for closing transitions)')
@click.option('--dry-run', is_flag=True, help='Show what would happen without making changes')
@click.pass_context
def do_transition(ctx, issue_key: str, status_name: str,
                  comment: str | None, resolution: str | None, dry_run: bool):
    """Transition an issue to a new status.

    ISSUE_KEY: The Jira issue key (e.g., PROJ-123)

    STATUS_NAME: Target status name (e.g., "In Progress", "Done")

    Examples:

      jira-transition do PROJ-123 "In Progress"

      jira-transition do PROJ-123 "Done" --resolution Fixed

      jira-transition do PROJ-123 "Done" -c "Deployed to production" -r Fixed

      jira-transition do PROJ-123 "In Review" --dry-run
    """
    client = ctx.obj['client']

    try:
        # Get available transitions
        transitions = client.get_issue_transitions(issue_key)

        # Find matching transition (case-insensitive)
        matching = None
        for t in transitions:
            if t.get('name', '').lower() == status_name.lower():
                matching = t
                break
            # Also check target status name
            to_status = _get_to_status(t)
            if to_status.lower() == status_name.lower():
                matching = t
                break

        if not matching:
            available = [t.get('name', '') for t in transitions]
            error(f"Transition '{status_name}' not available for {issue_key}")
            print(f"\nAvailable transitions: {', '.join(available)}")
            sys.exit(1)

        # Dry run
        if dry_run:
            warning("DRY RUN - No transition will be performed")
            print(f"\nWould transition {issue_key}:")
            print(f"  Transition: {matching['name']}")
            print(f"  To status: {_get_to_status(matching)}")
            if comment:
                print(f"  Comment: {comment}")
            if resolution:
                print(f"  Resolution: {resolution}")
            return

        # Build transition payload
        fields = {}
        if resolution:
            fields['resolution'] = {'name': resolution}

        # Perform transition - API uses target status name (not transition name/ID)
        # set_issue_status handles the transition ID lookup internally
        target_status = _get_to_status(matching)

        # Build update dict for comment if provided
        update = None
        if comment:
            update = {"comment": [{"add": {"body": comment}}]}

        client.set_issue_status(
            issue_key,
            target_status,
            fields=fields if fields else None,
            update=update
        )

        if ctx.obj['quiet']:
            print(issue_key)
        elif ctx.obj['json']:
            format_output({
                'key': issue_key,
                'transition': matching['name'],
                'to_status': _get_to_status(matching)
            }, as_json=True)
        else:
            success(f"Transitioned {issue_key}")
            print(f"  Status: {_get_to_status(matching)}")
            if comment:
                print(f"  Comment added: {comment[:50]}...")

    except Exception as e:
        if ctx.obj['debug']:
            raise
        error(f"Failed to transition {issue_key}: {e}")
        sys.exit(1)


if __name__ == '__main__':
    cli()
