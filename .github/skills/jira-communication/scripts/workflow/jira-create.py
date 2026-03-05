#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.10"
# dependencies = [
#     "atlassian-python-api>=3.41.0",
#     "click>=8.1.0",
# ]
# ///
"""Jira issue creation - create new issues with various types and fields."""

import json
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
from lib.output import format_output, success, error, warning

# ═══════════════════════════════════════════════════════════════════════════════
# CLI Definition
# ═══════════════════════════════════════════════════════════════════════════════


@click.group()
@click.option('--json', 'output_json', is_flag=True, help='Output as JSON')
@click.option('--quiet', '-q', is_flag=True, help='Minimal output (just issue key)')
@click.option('--env-file', type=click.Path(), help='Environment file path')
@click.option('--debug', is_flag=True, help='Show debug information on errors')
@click.pass_context
def cli(ctx, output_json: bool, quiet: bool, env_file: str | None, debug: bool):
    """Jira issue creation.

    Create new Jira issues with various types and configurations.
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
@click.argument('project_key')
@click.argument('summary')
@click.option('--type', '-t', 'issue_type', required=True,
              help='Issue type (Task, Bug, Story, Epic, etc.)')
@click.option('--description', '-d', help='Issue description (Jira wiki markup)')
@click.option('--priority', '-p', help='Priority name (High, Medium, Low, etc.)')
@click.option('--labels', '-l', help='Comma-separated labels')
@click.option('--assignee', '-a', help='Assignee username or email')
@click.option('--parent', help='Parent issue key (for subtasks or epic link)')
@click.option('--components', help='Comma-separated component names')
@click.option('--fields-json', help='JSON string of additional fields')
@click.option('--dry-run', is_flag=True, help='Show what would be created without making changes')
@click.pass_context
def issue(ctx, project_key: str, summary: str, issue_type: str,
          description: str | None, priority: str | None, labels: str | None,
          assignee: str | None, parent: str | None, components: str | None,
          fields_json: str | None, dry_run: bool):
    """Create a new Jira issue.

    PROJECT_KEY: The Jira project key (e.g., PROJ)

    SUMMARY: Issue summary/title

    Examples:

      jira-create issue PROJ "Fix login timeout" --type Bug --priority High

      jira-create issue PROJ "New feature" --type Story --parent PROJ-100

      jira-create issue PROJ "API documentation" --type Task -d "Update API docs" -l docs,api

      jira-create issue PROJ "Sprint goal" --type Epic

      jira-create issue PROJ "Test" --type Task --dry-run
    """
    client = ctx.obj['client']

    # Build issue fields
    fields = {
        'project': {'key': project_key},
        'summary': summary,
        'issuetype': {'name': issue_type},
    }

    if description:
        fields['description'] = description

    if priority:
        fields['priority'] = {'name': priority}

    if labels:
        fields['labels'] = [lbl.strip() for lbl in labels.split(',')]

    if assignee:
        if '@' in assignee:
            fields['assignee'] = {'emailAddress': assignee}
        else:
            fields['assignee'] = {'name': assignee}

    if parent:
        # Determine if subtask or epic link
        if issue_type.lower() in ('subtask', 'sub-task'):
            fields['parent'] = {'key': parent}
        else:
            # Epic link - field name varies by Jira version
            fields['parent'] = {'key': parent}

    if components:
        fields['components'] = [{'name': c.strip()} for c in components.split(',')]

    if fields_json:
        try:
            extra_fields = json.loads(fields_json)
            fields.update(extra_fields)
        except json.JSONDecodeError as e:
            error(f"Invalid JSON in --fields-json: {e}")
            sys.exit(1)

    # Dry run
    if dry_run:
        warning("DRY RUN - No issue will be created")
        print(f"\nWould create issue in {project_key}:")
        print(f"  Type: {issue_type}")
        print(f"  Summary: {summary}")
        if description:
            print(f"  Description: {description[:50]}...")
        if priority:
            print(f"  Priority: {priority}")
        if labels:
            print(f"  Labels: {labels}")
        if assignee:
            print(f"  Assignee: {assignee}")
        if parent:
            print(f"  Parent: {parent}")
        if components:
            print(f"  Components: {components}")
        return

    try:
        result = client.create_issue(fields=fields)

        if ctx.obj['quiet']:
            print(result['key'])
        elif ctx.obj['json']:
            format_output(result, as_json=True)
        else:
            success(f"Created issue: {result['key']}")
            print(f"  Summary: {summary}")
            print(f"  Type: {issue_type}")
            print(f"  URL: {client.url}/browse/{result['key']}")

    except Exception as e:
        if ctx.obj['debug']:
            raise
        error(f"Failed to create issue: {e}")
        sys.exit(1)


if __name__ == '__main__':
    cli()
