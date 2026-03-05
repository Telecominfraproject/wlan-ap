#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.10"
# dependencies = [
#     "atlassian-python-api>=3.41.0",
#     "click>=8.1.0",
# ]
# ///
"""Jira comment operations - add and list issue comments."""

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
from lib.output import format_output, success, error

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
    """Jira comment operations.

    Add and list comments on Jira issues.
    Note: Comments should use Jira wiki markup syntax, not Markdown.
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
@click.argument('comment_text')
@click.pass_context
def add(ctx, issue_key: str, comment_text: str):
    """Add a comment to an issue.

    ISSUE_KEY: The Jira issue key (e.g., PROJ-123)

    COMMENT_TEXT: Comment text (use Jira wiki markup, not Markdown)

    Note: Use Jira wiki syntax:
      - *bold* not **bold**
      - _italic_ not *italic*
      - {code}...{code} for code blocks
      - [link text|url] for links

    Examples:

      jira-comment add PROJ-123 "Fixed in commit abc123"

      jira-comment add PROJ-123 "See {code}config.py{code} for details"
    """
    client = ctx.obj['client']

    try:
        result = client.issue_add_comment(issue_key, comment_text)

        if ctx.obj['quiet']:
            print(result.get('id', 'ok'))
        elif ctx.obj['json']:
            format_output(result, as_json=True)
        else:
            success(f"Added comment to {issue_key}")
            print(f"  Comment ID: {result.get('id', 'N/A')}")

    except Exception as e:
        if ctx.obj['debug']:
            raise
        error(f"Failed to add comment to {issue_key}: {e}")
        sys.exit(1)


@cli.command('list')
@click.argument('issue_key')
@click.option('--limit', '-n', default=10, help='Max comments to show')
@click.option('--truncate', type=int, metavar='N', help='Truncate comment body to N characters')
@click.pass_context
def list_comments(ctx, issue_key: str, limit: int, truncate: int | None):
    """List comments on an issue.

    ISSUE_KEY: The Jira issue key (e.g., PROJ-123)

    Examples:

      jira-comment list PROJ-123

      jira-comment list PROJ-123 --limit 5 --json
    """
    client = ctx.obj['client']

    try:
        # Get issue with comments
        issue = client.issue(issue_key, fields='comment')
        comments = issue.get('fields', {}).get('comment', {}).get('comments', [])

        # Limit and reverse (newest first)
        comments = list(reversed(comments))[:limit]

        if ctx.obj['json']:
            format_output(comments, as_json=True)
        elif ctx.obj['quiet']:
            for c in comments:
                print(c.get('id', ''))
        else:
            if not comments:
                print(f"No comments on {issue_key}")
            else:
                print(f"Comments on {issue_key} ({len(comments)} shown):\n")
                for c in comments:
                    author = c.get('author', {}).get('displayName', 'Unknown')
                    created = c.get('created', '')[:16].replace('T', ' ') if c.get('created') else 'N/A'
                    body = c.get('body', '')

                    # Handle ADF format
                    if isinstance(body, dict):
                        body = _extract_text(body)

                    # Truncate if requested
                    if truncate and len(body) > truncate:
                        body = body[:truncate-3] + "..."

                    print("-" * 80)
                    print(f"[{created}] {author}:")
                    print()
                    for line in body.split('\n'):
                        print(line)
                    print()

    except Exception as e:
        if ctx.obj['debug']:
            raise
        error(f"Failed to get comments for {issue_key}: {e}")
        sys.exit(1)


def _extract_text(adf: dict) -> str:
    """Extract plain text from Atlassian Document Format."""
    if not isinstance(adf, dict):
        return str(adf)

    text_parts = []
    for block in adf.get('content', []):
        if block.get('type') == 'paragraph':
            for item in block.get('content', []):
                if item.get('type') == 'text':
                    text_parts.append(item.get('text', ''))
        elif block.get('type') == 'text':
            text_parts.append(block.get('text', ''))

    return ' '.join(text_parts)


if __name__ == '__main__':
    cli()
