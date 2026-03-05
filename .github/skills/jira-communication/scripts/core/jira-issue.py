#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.10"
# dependencies = [
#     "atlassian-python-api>=3.41.0",
#     "click>=8.1.0",
# ]
# ///
"""Jira issue operations - get and update issue details."""

import json
import re
import sys
from pathlib import Path

ACCOUNT_ID_PATTERN = re.compile(r'^[a-zA-Z0-9:\-]+$')
LEGACY_ACCOUNT_ID_PATTERN = re.compile(r'^[a-f0-9]{24}$')

def is_account_id(s: str) -> bool:
    if ':' in s:
        return bool(ACCOUNT_ID_PATTERN.match(s))
    return bool(LEGACY_ACCOUNT_ID_PATTERN.match(s))

# ═══════════════════════════════════════════════════════════════════════════════
# Shared library import (TR1.1.1 - PYTHONPATH approach)
# ═══════════════════════════════════════════════════════════════════════════════
_script_dir = Path(__file__).parent
_lib_path = _script_dir.parent / "lib"
if _lib_path.exists():
    sys.path.insert(0, str(_lib_path.parent))

import click
from lib.client import get_jira_client
from lib.output import format_output, success, error, warning

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
    """Jira issue operations.

    Get and update Jira issue details.
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
@click.option('--fields', '-f', help='Comma-separated fields to return')
@click.option('--expand', '-e', help='Fields to expand (changelog,transitions,renderedFields)')
@click.option('--truncate', type=int, metavar='N', help='Truncate description to N characters')
@click.option('--full', is_flag=True, help='[DEPRECATED] Show full content (now default behavior)')
@click.option('--json', 'cmd_json', is_flag=True, help='Output as JSON')
@click.option('--quiet', '-q', 'cmd_quiet', is_flag=True, help='Minimal output')
@click.pass_context
def get(ctx, issue_key: str, fields: str | None, expand: str | None,
        truncate: int | None, full: bool, cmd_json: bool, cmd_quiet: bool):
    """Get issue details.

    ISSUE_KEY: The Jira issue key (e.g., PROJ-123)

    Examples:

      jira-issue get PROJ-123

      jira-issue get PROJ-123 --fields summary,status,assignee

      jira-issue get PROJ-123 --expand changelog,transitions
    """
    client = ctx.obj['client']

    # Warn about deprecated --full flag
    if full:
        warning("--full is deprecated (full content is now shown by default). Use --truncate N to limit output.")

    try:
        # Build parameters
        params = {}
        if fields:
            # Jira API expects fields as comma-separated string, not list
            params['fields'] = fields
        if expand:
            params['expand'] = expand

        issue = client.issue(issue_key, **params)

        # Command-level flags override group-level flags
        use_json = cmd_json or ctx.obj['json']
        use_quiet = cmd_quiet or ctx.obj['quiet']

        if use_json:
            format_output(issue, as_json=True)
        elif use_quiet:
            print(issue['key'])
        else:
            _print_issue(issue, truncate=truncate, requested_fields=fields)

    except Exception as e:
        if ctx.obj['debug']:
            raise
        error(f"Failed to get issue {issue_key}: {e}")
        sys.exit(1)


def _print_issue(issue: dict, truncate: int | None = None, requested_fields: str | None = None) -> None:
    """Pretty print issue details.

    Args:
        issue: The issue dict from Jira API
        truncate: If set, truncate description to this many characters
        requested_fields: Comma-separated fields that were requested (affects output)
    """
    fields = issue.get('fields', {})
    requested = set(requested_fields.split(',')) if requested_fields else None

    def should_show(field_name: str) -> bool:
        """Check if a field should be shown based on requested fields."""
        if requested is None:
            return True
        return field_name in requested

    def field_available(field_name: str) -> bool:
        """Check if a field was returned by the API."""
        return field_name in fields

    # Header with summary
    if should_show('summary') or requested is None:
        summary = fields.get('summary', 'No summary') if field_available('summary') else '[not requested]'
        print(f"\n{issue['key']}: {summary}")
        print("=" * 60)
    else:
        print(f"\n{issue['key']}")
        print("=" * 60)

    # Status, type, priority row - only show if any were requested or no filter
    show_status_row = (requested is None or
                       any(f in requested for f in ['status', 'issuetype', 'priority']))
    if show_status_row:
        parts = []
        if should_show('issuetype') and field_available('issuetype'):
            issue_type = fields.get('issuetype', {}).get('name', 'Unknown')
            parts.append(f"Type: {issue_type}")
        if should_show('status') and field_available('status'):
            status = fields.get('status', {}).get('name', 'Unknown')
            parts.append(f"Status: {status}")
        if should_show('priority') and field_available('priority'):
            priority = fields.get('priority', {}).get('name', 'None') if fields.get('priority') else 'None'
            parts.append(f"Priority: {priority}")
        if parts:
            print(" | ".join(parts))

    # Assignee and reporter row
    show_people_row = (requested is None or
                       any(f in requested for f in ['assignee', 'reporter']))
    if show_people_row:
        parts = []
        if should_show('assignee') and field_available('assignee'):
            assignee = fields.get('assignee', {})
            assignee_name = assignee.get('displayName', 'Unassigned') if assignee else 'Unassigned'
            parts.append(f"Assignee: {assignee_name}")
        if should_show('reporter') and field_available('reporter'):
            reporter = fields.get('reporter', {})
            reporter_name = reporter.get('displayName', 'Unknown') if reporter else 'Unknown'
            parts.append(f"Reporter: {reporter_name}")
        if parts:
            print(" | ".join(parts))

    # Labels
    if should_show('labels') and field_available('labels'):
        labels = fields.get('labels', [])
        if labels:
            print(f"Labels: {', '.join(labels)}")

    # Description
    if should_show('description') and field_available('description'):
        description = fields.get('description')
        if description:
            print(f"\nDescription:")
            # Handle both string and ADF format
            if isinstance(description, str):
                desc_text = description
            elif isinstance(description, dict):
                # ADF format - extract text content
                desc_text = _extract_adf_text(description)
            else:
                desc_text = str(description)

            # Truncate if requested
            if truncate and len(desc_text) > truncate:
                # Find word boundary for clean truncation
                truncated = desc_text[:truncate].rsplit(' ', 1)[0]
                print(f"  {truncated}...")
                print(f"  [truncated at {truncate} chars]")
            else:
                # Print full description, preserving line breaks
                for line in desc_text.split('\n'):
                    print(f"  {line}")

    # Dates
    show_dates_row = (requested is None or
                      any(f in requested for f in ['created', 'updated']))
    if show_dates_row:
        parts = []
        if should_show('created') and field_available('created'):
            created = fields.get('created', '')[:10] if fields.get('created') else 'N/A'
            parts.append(f"Created: {created}")
        if should_show('updated') and field_available('updated'):
            updated = fields.get('updated', '')[:10] if fields.get('updated') else 'N/A'
            parts.append(f"Updated: {updated}")
        if parts:
            print(f"\n{' | '.join(parts)}")

    # Attachments
    if should_show('attachment') and field_available('attachment'):
        attachments = fields.get('attachment', [])
        if attachments:
            print("\n" + "=" * 60)
            print("ATTACHMENTS")
            print("=" * 60)
            for att in attachments:
                filename = att.get('filename', 'Unknown')
                url = att.get('content', '')
                print(f"  • {filename} - {url}")

    print()


def _extract_adf_text(adf: dict) -> str:
    """Extract plain text from Atlassian Document Format."""
    if not isinstance(adf, dict):
        return str(adf)

    text_parts = []
    content = adf.get('content', [])

    for block in content:
        if block.get('type') == 'paragraph':
            for item in block.get('content', []):
                if item.get('type') == 'text':
                    text_parts.append(item.get('text', ''))
        elif block.get('type') == 'text':
            text_parts.append(block.get('text', ''))

    return ' '.join(text_parts)


@cli.command()
@click.argument('issue_key')
@click.option('--summary', '-s', help='New summary')
@click.option('--priority', '-p', help='Priority name')
@click.option('--labels', '-l', help='Comma-separated labels (replaces existing)')
@click.option('--assignee', '-a', help='Assignee username or email')
@click.option('--fields-json', help='JSON string of additional fields to update')
@click.option('--dry-run', is_flag=True, help='Show what would be updated without making changes')
@click.pass_context
def update(ctx, issue_key: str, summary: str | None, priority: str | None,
           labels: str | None, assignee: str | None, fields_json: str | None,
           dry_run: bool):
    """Update issue fields.

    ISSUE_KEY: The Jira issue key (e.g., PROJ-123)

    Examples:

      jira-issue update PROJ-123 --summary "New title"

      jira-issue update PROJ-123 --priority High --labels backend,urgent

      jira-issue update PROJ-123 --fields-json '{"customfield_10001": "value"}'

      jira-issue update PROJ-123 --summary "Test" --dry-run
    """
    client = ctx.obj['client']

    # Build update payload
    update_fields = {}

    if summary:
        update_fields['summary'] = summary

    if priority:
        update_fields['priority'] = {'name': priority}

    if labels:
        update_fields['labels'] = [l.strip() for l in labels.split(',')]

    if assignee:
        if is_account_id(assignee):
            update_fields['assignee'] = {'accountId': assignee}
        else:
            users = client.user_find_by_user_string(query=assignee)
            if users:
                update_fields['assignee'] = {'accountId': users[0]['accountId']}
            else:
                error(f"User not found: {assignee}")
                sys.exit(1)

    if fields_json:
        try:
            extra_fields = json.loads(fields_json)
            update_fields.update(extra_fields)
        except json.JSONDecodeError as e:
            error(f"Invalid JSON in --fields-json: {e}")
            sys.exit(1)

    if not update_fields:
        error("No fields specified for update")
        click.echo("\nUse one or more of: --summary, --priority, --labels, --assignee, --fields-json")
        sys.exit(1)

    if dry_run:
        warning("DRY RUN - No changes will be made")
        print(f"\nWould update {issue_key} with:")
        for key, value in update_fields.items():
            print(f"  {key}: {value}")
        return

    try:
        client.update_issue_field(issue_key, update_fields)

        if ctx.obj['quiet']:
            print(issue_key)
        elif ctx.obj['json']:
            format_output({'key': issue_key, 'updated': list(update_fields.keys())}, as_json=True)
        else:
            success(f"Updated {issue_key}")
            for key in update_fields:
                print(f"  ✓ {key}")

    except Exception as e:
        if ctx.obj['debug']:
            raise
        error(f"Failed to update {issue_key}: {e}")
        sys.exit(1)


if __name__ == '__main__':
    cli()
