#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.10"
# dependencies = [
#     "atlassian-python-api>=3.41.0",
#     "click>=8.1.0",
#     "requests>=2.28.0",
# ]
# ///
"""Jira environment validation - verify runtime, configuration, and connectivity."""

import shutil
import subprocess
import sys
from pathlib import Path

# ═══════════════════════════════════════════════════════════════════════════════
# Shared library import (TR1.1.1 - PYTHONPATH approach)
# ═══════════════════════════════════════════════════════════════════════════════
_script_dir = Path(__file__).parent
_lib_path = _script_dir.parent / "lib"
if _lib_path.exists():
    sys.path.insert(0, str(_lib_path.parent))

import json as json_module

import click
import requests
from lib.config import load_env, validate_config, get_auth_mode, DEFAULT_ENV_FILE
from lib.client import get_jira_client
from lib.output import success, error, warning

# ═══════════════════════════════════════════════════════════════════════════════
# Exit Codes (TR2.3)
# ═══════════════════════════════════════════════════════════════════════════════
EXIT_SUCCESS = 0
EXIT_RUNTIME_ERROR = 1
EXIT_CONFIG_ERROR = 2
EXIT_CONNECTION_ERROR = 3


def check_runtime(verbose: bool = False) -> tuple[bool, dict]:
    """Check runtime dependencies (D7)."""
    checks_passed = True
    info = {}

    # Check uv/uvx
    uv_path = shutil.which('uv')
    if uv_path:
        result = subprocess.run(['uv', '--version'], capture_output=True, text=True)
        uv_version = result.stdout.strip() if result.returncode == 0 else 'unknown'
        info['uv_path'] = uv_path
        info['uv_version'] = uv_version
        if verbose:
            success(f"uv found: {uv_path} ({uv_version})")
    else:
        error(
            "Runtime check failed: 'uv' command not found",
            "To install uv, run:\n"
            "    curl -LsSf https://astral.sh/uv/install.sh | sh\n\n"
            "  Or visit: https://docs.astral.sh/uv/getting-started/installation/"
        )
        checks_passed = False

    # Check Python version
    py_version = sys.version_info
    info['python_version'] = f"{py_version.major}.{py_version.minor}.{py_version.micro}"
    if py_version >= (3, 10):
        if verbose:
            success(f"Python version: {py_version.major}.{py_version.minor}.{py_version.micro}")
    else:
        error(
            f"Python version {py_version.major}.{py_version.minor} < 3.10 required",
            "Please upgrade Python to 3.10 or later"
        )
        checks_passed = False

    return checks_passed, info


def check_environment(env_file: str | None, verbose: bool = False) -> dict | None:
    """Check environment configuration."""
    try:
        config = load_env(env_file)
        errors = validate_config(config)

        if errors:
            for err in errors:
                error(f"Configuration error: {err}")
            return None

        if verbose:
            path = Path(env_file) if env_file else DEFAULT_ENV_FILE
            success(f"Environment file: {path}")
            success(f"JIRA_URL: {config['JIRA_URL']}")

            # Show auth mode-specific credentials
            auth_mode = get_auth_mode(config)
            if auth_mode == 'pat':
                success("Auth mode: Personal Access Token (Server/DC)")
                success("JIRA_PERSONAL_TOKEN: ******* (hidden)")
            else:
                success("Auth mode: Username + API Token (Cloud)")
                success(f"JIRA_USERNAME: {config.get('JIRA_USERNAME', 'N/A')}")
                success("JIRA_API_TOKEN: ******* (hidden)")

            if 'JIRA_CLOUD' in config:
                success(f"JIRA_CLOUD: {config['JIRA_CLOUD']}")

        return config

    except FileNotFoundError as e:
        error(str(e))
        return None


def check_connectivity(config: dict, project: str | None, verbose: bool = False) -> tuple[bool, dict]:
    """Check connectivity and authentication."""
    url = config['JIRA_URL']
    info = {'url': url}

    # Test server reachability
    try:
        response = requests.head(url, timeout=10, allow_redirects=True)
        info['server_reachable'] = True
        if verbose:
            success(f"Server reachable: {url} (status: {response.status_code})")
    except requests.exceptions.Timeout:
        error(
            f"Connection timeout: {url}",
            "The server did not respond within 10 seconds.\n"
            "  Check your network connection and JIRA_URL."
        )
        return False, info
    except requests.exceptions.ConnectionError as e:
        error(
            f"Connection failed: {url}",
            f"Could not connect to the server.\n  Error: {e}"
        )
        return False, info

    # Test authentication
    try:
        client = get_jira_client()
        user = client.myself()
        display_name = user.get('displayName', user.get('name', 'Unknown'))
        email = user.get('emailAddress', 'N/A')
        info['user'] = display_name
        info['email'] = email

        if verbose:
            success(f"Authenticated as: {display_name} ({email})")

    except Exception as e:
        error(
            "Authentication failed",
            f"Could not authenticate with the provided credentials.\n  Error: {e}"
        )
        return False, info

    # Test project access (optional)
    if project:
        try:
            proj = client.project(project)
            info['project_access'] = project
            if verbose:
                success(f"Project access: {project} ({proj.get('name', 'Unknown')})")
            else:
                success(f"Project access verified: {project}")
        except Exception as e:
            warning(f"Could not access project {project}: {e}")

    return True, info


@click.command()
@click.option('--json', 'output_json', is_flag=True, help='Output as JSON')
@click.option('--quiet', '-q', is_flag=True, help='Minimal output')
@click.option('--verbose', '-v', is_flag=True, help='Show detailed output')
@click.option('--project', '-p', help='Verify access to specific project')
@click.option('--env-file', type=click.Path(exists=False), help='Path to environment file')
@click.option('--debug', is_flag=True, help='Show debug information on errors')
def main(output_json: bool, quiet: bool, verbose: bool, project: str | None,
         env_file: str | None, debug: bool):
    """Validate Jira environment configuration.

    Checks runtime dependencies, environment configuration, and connectivity
    to ensure the Jira CLI scripts will work correctly.

    Exit codes:
      0 - All checks passed
      1 - Runtime dependency missing
      2 - Environment configuration error
      3 - Connectivity/authentication failure
    """
    result = {'status': 'ok'}

    # Suppress verbose output if JSON or quiet mode
    show_verbose = verbose and not output_json and not quiet

    if show_verbose:
        click.echo("=" * 60)
        click.echo("Jira Environment Validation")
        click.echo("=" * 60)
        click.echo()

    # Check 1: Runtime
    if show_verbose:
        click.echo("Runtime Checks:")
    runtime_ok, runtime_info = check_runtime(show_verbose)
    result['runtime'] = runtime_info
    if not runtime_ok:
        result['status'] = 'error'
        result['error'] = 'runtime_check_failed'
        if output_json:
            print(json_module.dumps(result, indent=2))
        elif quiet:
            print('error')
        sys.exit(EXIT_RUNTIME_ERROR)
    if show_verbose:
        click.echo()

    # Check 2: Environment
    if show_verbose:
        click.echo("Environment Checks:")
    config = check_environment(env_file, show_verbose)
    if config is None:
        result['status'] = 'error'
        result['error'] = 'config_error'
        if output_json:
            print(json_module.dumps(result, indent=2))
        elif quiet:
            print('error')
        sys.exit(EXIT_CONFIG_ERROR)

    result['url'] = config['JIRA_URL']
    # Strict check: must end with .atlassian.net (not just contain it)
    from urllib.parse import urlparse
    netloc = urlparse(config['JIRA_URL']).netloc.lower()
    is_cloud = netloc == 'atlassian.net' or netloc.endswith('.atlassian.net')
    result['server_type'] = 'cloud' if is_cloud else 'server'
    auth_mode = get_auth_mode(config)
    result['auth_mode'] = auth_mode
    if auth_mode == 'basic':
        result['username'] = config.get('JIRA_USERNAME', 'N/A')
    if show_verbose:
        click.echo()

    # Check 3: Connectivity
    if show_verbose:
        click.echo("Connectivity Checks:")
    conn_ok, conn_info = check_connectivity(config, project, show_verbose)
    result['user'] = conn_info.get('user', 'Unknown')
    if 'project_access' in conn_info:
        result['project_access'] = conn_info['project_access']
    if not conn_ok:
        result['status'] = 'error'
        result['error'] = 'connectivity_error'
        if output_json:
            print(json_module.dumps(result, indent=2))
        elif quiet:
            print('error')
        sys.exit(EXIT_CONNECTION_ERROR)
    if show_verbose:
        click.echo()

    # All passed
    if output_json:
        print(json_module.dumps(result, indent=2))
    elif quiet:
        print('ok')
    else:
        if show_verbose:
            click.echo("=" * 60)
        success("All validation checks passed!")
    sys.exit(EXIT_SUCCESS)


if __name__ == '__main__':
    main()
