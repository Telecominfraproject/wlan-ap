#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.10"
# dependencies = [
#     "atlassian-python-api>=3.41.0",
#     "click>=8.1.0",
# ]
# ///
"""Jira worklog operations - add and list time tracking entries."""

import sys
from pathlib import Path
from datetime import datetime, timezone

# ═══════════════════════════════════════════════════════════════════════════════
# Shared library import (TR1.1.1 - PYTHONPATH approach)
# ═══════════════════════════════════════════════════════════════════════════════
_script_dir = Path(__file__).parent
_lib_path = _script_dir.parent / "lib"
if _lib_path.exists():
    sys.path.insert(0, str(_lib_path.parent))

import re
import click
from lib.client import get_jira_client
from lib.output import format_output, success, error


def normalize_iso_timestamp(timestamp: str) -> str:
    """Normalize ISO timestamp to Jira's required format.

    Jira requires: YYYY-MM-DDTHH:MM:SS.sss+ZZZZ (e.g., 2025-01-15T09:00:00.000+0100)

    Accepts various formats:
      - 2025-01-15T09:00:00 (adds local timezone)
      - 2025-01-15T09:00 (adds seconds and local timezone)
      - 2025-01-15 (adds time 00:00:00 and local timezone)
      - 2025-01-15T09:00:00+01:00 (converts timezone format)
      - 2025-01-15T09:00:00.000+0100 (pass through)
    """
    # Already in Jira format (has milliseconds and compact timezone)
    if re.match(r'\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}\.\d{3}[+-]\d{4}$', timestamp):
        return timestamp

    # Get local timezone offset
    local_tz = datetime.now().astimezone().strftime('%z')  # e.g., +0100

    # Date only: 2025-01-15
    if re.match(r'^\d{4}-\d{2}-\d{2}$', timestamp):
        return f"{timestamp}T00:00:00.000{local_tz}"

    # Has timezone with colon: 2025-01-15T09:00:00+01:00
    tz_match = re.search(r'([+-])(\d{2}):(\d{2})$', timestamp)
    if tz_match:
        tz_compact = f"{tz_match.group(1)}{tz_match.group(2)}{tz_match.group(3)}"
        timestamp = timestamp[:tz_match.start()]
    else:
        tz_compact = local_tz

    # No seconds: 2025-01-15T09:00
    if re.match(r'^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}$', timestamp):
        timestamp = f"{timestamp}:00"

    # Has seconds but no milliseconds: 2025-01-15T09:00:00
    if re.match(r'^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}$', timestamp):
        return f"{timestamp}.000{tz_compact}"

    # Fallback: return as-is (let Jira API handle/reject it)
    return timestamp

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
    """Jira worklog operations.

    Add and list time tracking entries for Jira issues.

    TIME_SPENT format examples: '2h', '2h 30m', '1d', '30m'
    (passed directly to Jira API - see D10)
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
@click.argument('issue_key')
@click.argument('time_spent')
@click.option('--comment', '-c', help='Worklog comment')
@click.option('--started', help='Start time (ISO format: YYYY-MM-DD, YYYY-MM-DDTHH:MM, or YYYY-MM-DDTHH:MM:SS; default: now)')
@click.pass_context
def add(ctx, issue_key: str, time_spent: str, comment: str | None, started: str | None):
    """Add worklog entry to an issue.

    ISSUE_KEY: The Jira issue key (e.g., PROJ-123)

    TIME_SPENT: Time spent in Jira format (e.g., '2h 30m', '1d', '30m')

    Examples:

      jira-worklog add PROJ-123 "2h 30m" -c "Code review"

      jira-worklog add PROJ-123 "1d" --started "2025-01-15T09:00:00"
    """
    client = ctx.obj['client']

    try:
        # Build worklog data for JSON API
        worklog_data = {
            'timeSpent': time_spent,
        }

        if comment:
            worklog_data['comment'] = comment

        if started:
            worklog_data['started'] = normalize_iso_timestamp(started)
        else:
            # Default to current time in local timezone (Jira format)
            worklog_data['started'] = datetime.now().astimezone().strftime('%Y-%m-%dT%H:%M:%S.000%z')

        # Add worklog via REST API (using issue_add_json_worklog which accepts timeSpent string)
        result = client.issue_add_json_worklog(issue_key, worklog_data)

        if ctx.obj['quiet']:
            print(result.get('id', 'ok'))
        elif ctx.obj['json']:
            format_output(result, as_json=True)
        else:
            success(f"Added worklog to {issue_key}: {time_spent}")
            if comment:
                print(f"  Comment: {comment}")
            print(f"  Worklog ID: {result.get('id', 'N/A')}")

    except Exception as e:
        if ctx.obj['debug']:
            raise
        error(f"Failed to add worklog to {issue_key}: {e}")
        sys.exit(1)


@cli.command('list')
@click.argument('issue_key')
@click.option('--limit', '-n', default=10, help='Max entries to show')
@click.option('--truncate', type=int, metavar='N', help='Truncate comments to N characters')
@click.pass_context
def list_worklogs(ctx, issue_key: str, limit: int, truncate: int | None):
    """List worklog entries for an issue.

    ISSUE_KEY: The Jira issue key (e.g., PROJ-123)

    Examples:

      jira-worklog list PROJ-123

      jira-worklog list PROJ-123 --limit 5 --json
    """
    client = ctx.obj['client']

    try:
        result = client.issue_get_worklog(issue_key)
        worklogs = result.get('worklogs', [])

        # Limit results
        worklogs = worklogs[:limit]

        if ctx.obj['json']:
            format_output(worklogs, as_json=True)
        elif ctx.obj['quiet']:
            for wl in worklogs:
                print(wl.get('id', ''))
        else:
            if not worklogs:
                print(f"No worklogs found for {issue_key}")
            else:
                print(f"Worklogs for {issue_key} ({len(worklogs)} shown):\n")
                for wl in worklogs:
                    author = wl.get('author', {}).get('displayName', 'Unknown')
                    time_spent = wl.get('timeSpent', 'N/A')
                    started = wl.get('started', 'N/A')[:10] if wl.get('started') else 'N/A'
                    comment = wl.get('comment', '')

                    print(f"  [{started}] {author}: {time_spent}")
                    if comment:
                        # Truncate if requested
                        if truncate and len(comment) > truncate:
                            comment = comment[:truncate-3] + "..."
                        print(f"           {comment}")

    except Exception as e:
        if ctx.obj['debug']:
            raise
        error(f"Failed to get worklogs for {issue_key}: {e}")
        sys.exit(1)


if __name__ == '__main__':
    cli()
