#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.10"
# dependencies = [
#     "atlassian-python-api>=3.41.0",
#     "click>=8.1.0",
# ]
# ///
"""Jira user operations - get user information."""

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
from lib.output import format_output, error

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
    """Jira user operations.

    Get information about Jira users.
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
@click.pass_context
def me(ctx):
    """Get current user information.

    Shows details about the authenticated user.

    Example:

      jira-user me
    """
    client = ctx.obj['client']

    try:
        user = client.myself()

        if ctx.obj['json']:
            format_output(user, as_json=True)
        elif ctx.obj['quiet']:
            print(user.get('accountId', user.get('name', '')))
        else:
            print("Current User:")
            print(f"  Name: {user.get('displayName', 'Unknown')}")
            print(f"  Email: {user.get('emailAddress', 'N/A')}")
            print(f"  Account ID: {user.get('accountId', user.get('key', 'N/A'))}")
            print(f"  Active: {'Yes' if user.get('active', True) else 'No'}")
            timezone = user.get('timeZone', 'N/A')
            print(f"  Timezone: {timezone}")

    except Exception as e:
        if ctx.obj['debug']:
            raise
        error(f"Failed to get current user: {e}")
        sys.exit(1)


@cli.command()
@click.argument('identifier')
@click.pass_context
def get(ctx, identifier: str):
    """Get user by identifier.

    IDENTIFIER: Username, email, or account ID

    Examples:

      jira-user get john.doe

      jira-user get john.doe@example.com

      jira-user get 5b10ac8d82e05b22cc7d4ef5
    """
    client = ctx.obj['client']

    try:
        # Try different methods to find user
        user = None

        # Try by account ID first (Cloud)
        if identifier.startswith('5') and len(identifier) > 20:
            try:
                user = client.user(account_id=identifier)
            except Exception:
                pass

        # Try by username directly (Server/DC)
        if not user:
            try:
                user = client.user(username=identifier)
            except Exception:
                pass

        # Try user search API (works for email on Server/DC)
        if not user:
            try:
                users = client.get('rest/api/2/user/search', params={'username': identifier})
                if users and isinstance(users, list) and len(users) > 0:
                    user = users[0]
            except Exception:
                pass

        # Try user search as fallback (Cloud)
        if not user:
            try:
                users = client.user_find_by_user_string(query=identifier)
                if users and isinstance(users, list) and len(users) > 0:
                    found = users[0]
                    if isinstance(found, dict):
                        user = found
                    elif isinstance(found, str) and not found.startswith('Username'):
                        # It's a username string, fetch full object
                        user = client.user(username=found)
            except Exception:
                pass

        if not user:
            error(f"User not found: {identifier}")
            sys.exit(1)

        if ctx.obj['json']:
            format_output(user, as_json=True)
        elif ctx.obj['quiet']:
            print(user.get('accountId', user.get('name', '')))
        else:
            print(f"User: {user.get('displayName', 'Unknown')}")
            print(f"  Email: {user.get('emailAddress', 'N/A')}")
            print(f"  Account ID: {user.get('accountId', user.get('key', 'N/A'))}")
            print(f"  Active: {'Yes' if user.get('active', True) else 'No'}")

    except Exception as e:
        if ctx.obj['debug']:
            raise
        error(f"Failed to get user {identifier}: {e}")
        sys.exit(1)


if __name__ == '__main__':
    cli()
