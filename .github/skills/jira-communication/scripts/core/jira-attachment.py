#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.10"
# dependencies = [
#     "atlassian-python-api>=3.41.0",
#     "click>=8.1.0",
#     "requests>=2.31.0",
# ]
# ///
"""Jira attachment operations - download attachments."""

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
import requests
from lib.client import get_jira_client
from lib.config import load_env
from lib.output import success, error

# Chunk size for streaming large file downloads (1 MB)
CHUNK_SIZE = 1048576

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
    """Jira attachment operations.

    Download attachments from Jira issues.
    """
    ctx.ensure_object(dict)
    ctx.obj['json'] = output_json
    ctx.obj['quiet'] = quiet
    ctx.obj['env_file'] = env_file
    ctx.obj['debug'] = debug


@cli.command()
@click.argument('attachment_url')
@click.argument('output_file')
@click.pass_context
def download(ctx, attachment_url: str, output_file: str):
    """Download a Jira attachment.

    ATTACHMENT_URL: Full URL or attachment ID/content path

    OUTPUT_FILE: Output file path

    Examples:

      jira-attachment download https://example.atlassian.net/rest/api/2/attachment/content/12345 file.zip

      jira-attachment download /rest/api/2/attachment/content/12345 file.zip
    """
    try:
        # Load config for authentication
        config = load_env(ctx.obj['env_file'])
        jira_url = config['JIRA_URL']

        # Determine authentication method
        if 'JIRA_PERSONAL_TOKEN' in config:
            auth_token = config['JIRA_PERSONAL_TOKEN']
            auth = None
            headers = {'Authorization': f'Bearer {auth_token}'}
        else:
            username = config['JIRA_USERNAME']
            api_token = config['JIRA_API_TOKEN']
            auth = (username, api_token)
            headers = {}

        # Build full URL if needed
        if attachment_url.startswith('http'):
            url = attachment_url
        else:
            url = jira_url + attachment_url

        # Validate output path
        output_path = Path(output_file)
        parent_dir = output_path.parent

        if not parent_dir.exists():
            error(f"Directory does not exist: {parent_dir}")
            sys.exit(1)

        if output_path.exists() and not output_path.is_file():
            error(f"Output path exists and is not a file: {output_file}")
            sys.exit(1)

        # Download with authentication
        response = requests.get(
            url,
            auth=auth,
            headers=headers,
            allow_redirects=True,
            stream=True
        )
        response.raise_for_status()

        # Write to file
        with open(output_file, 'wb') as f:
            for chunk in response.iter_content(chunk_size=CHUNK_SIZE):
                f.write(chunk)

        if ctx.obj['quiet']:
            print(output_file)
        elif ctx.obj['json']:
            import json
            print(json.dumps({'status': 'success', 'file': output_file}))
        else:
            success(f"Downloaded to: {output_file}")

    except KeyError as e:
        if ctx.obj['debug']:
            raise
        error(f"Missing required configuration: {e}")
        sys.exit(1)
    except requests.exceptions.RequestException as e:
        if ctx.obj['debug']:
            raise
        error(f"Download failed: {e}")
        sys.exit(1)
    except Exception as e:
        if ctx.obj['debug']:
            raise
        error(f"Failed to download attachment: {e}")
        sys.exit(1)


if __name__ == '__main__':
    cli()
