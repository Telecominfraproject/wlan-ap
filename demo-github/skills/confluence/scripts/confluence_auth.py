"""
Confluence Authentication and Credential Discovery

Provides shared credential discovery for both upload and download scripts.
Searches for credentials in the following priority order:
1. Environment variables
2. .env in current directory
3. .env.confluence in current directory
4. .env.jira in current directory
5. .env.atlassian in current directory
6. Walk up parent directories for above files
7. MCP config (~/.config/mcp/.mcp.json)

Usage:
    from confluence_auth import get_confluence_client

    confluence = get_confluence_client()
    # Or with custom env file:
    confluence = get_confluence_client(env_file="/path/to/.env")
"""

import os
import json
from pathlib import Path
from typing import Optional, Dict
from dotenv import load_dotenv


ENV_FILE_VARIANTS = ['.env', '.env.confluence', '.env.jira', '.env.atlassian']


def _check_env_vars() -> Optional[Dict[str, str]]:
    """Check if required environment variables are set"""
    url = os.getenv('CONFLUENCE_URL')
    username = os.getenv('CONFLUENCE_USERNAME')
    token = os.getenv('CONFLUENCE_API_TOKEN') or os.getenv('CONFLUENCE_PASSWORD')

    if all([url, username, token]):
        return {
            'url': url,
            'username': username,
            'token': token
        }
    return None


def _find_env_file_in_directory(directory: Path) -> Optional[Path]:
    """Find first matching .env variant in a directory"""
    for env_variant in ENV_FILE_VARIANTS:
        env_path = directory / env_variant
        if env_path.exists() and env_path.is_file():
            return env_path
    return None


def _walk_up_for_env_file(start_dir: Optional[Path] = None) -> Optional[Path]:
    """Walk up directory tree to find .env file"""
    if start_dir is None:
        start_dir = Path.cwd()

    current = start_dir.resolve()
    root = Path(current.anchor)  # Stop at filesystem root

    while current != root:
        env_file = _find_env_file_in_directory(current)
        if env_file:
            return env_file

        parent = current.parent
        if parent == current:  # Reached root
            break
        current = parent

    return None


def _load_mcp_config() -> Optional[Dict[str, str]]:
    """
    Load credentials from MCP server configuration.

    Checks:
    - ~/.config/mcp/.mcp.json
    - ~/.mcp.json

    Returns dict with url, username, token or None
    """
    config_paths = [
        Path.home() / '.config' / 'mcp' / '.mcp.json',
        Path.home() / '.mcp.json'
    ]

    for config_path in config_paths:
        if not config_path.exists():
            continue

        try:
            with open(config_path, 'r') as f:
                config = json.load(f)

            # Look for atlassian or confluence MCP server
            mcp_servers = config.get('mcpServers', {})

            for server_name in ['atlassian-evinova', 'atlassian', 'confluence']:
                if server_name not in mcp_servers:
                    continue

                server_config = mcp_servers[server_name]
                env_vars = server_config.get('env', {})

                url = env_vars.get('CONFLUENCE_URL')
                username = env_vars.get('CONFLUENCE_USERNAME')
                token = (env_vars.get('CONFLUENCE_API_TOKEN') or
                        env_vars.get('CONFLUENCE_PASSWORD'))

                # Handle ${VAR} references
                if token and token.startswith('${') and token.endswith('}'):
                    var_name = token[2:-1]
                    token = os.getenv(var_name)

                if all([url, username, token]):
                    return {
                        'url': url,
                        'username': username,
                        'token': token
                    }

        except (json.JSONDecodeError, KeyError, IOError):
            continue

    return None


def get_confluence_credentials(env_file: Optional[str] = None) -> Dict[str, str]:
    """
    Discover Confluence credentials using fallback chain.

    Args:
        env_file: Optional path to specific .env file (overrides discovery)

    Returns:
        Dict with 'url', 'username', 'token' keys

    Raises:
        ValueError: If no valid credentials found

    Priority order:
        1. Explicit env_file parameter
        2. Environment variables
        3. .env files in current directory
        4. .env files in parent directories
        5. MCP configuration
    """

    # Priority 1: Explicit env_file parameter
    if env_file:
        env_path = Path(env_file)
        if not env_path.exists():
            raise ValueError(f"Specified env file not found: {env_file}")

        load_dotenv(env_path)
        creds = _check_env_vars()
        if creds:
            return creds
        raise ValueError(f"Env file {env_file} does not contain required credentials")

    # Priority 2: Environment variables (already set)
    creds = _check_env_vars()
    if creds:
        return creds

    # Priority 3-4: .env files in current directory
    env_file_path = _find_env_file_in_directory(Path.cwd())
    if env_file_path:
        load_dotenv(env_file_path)
        creds = _check_env_vars()
        if creds:
            return creds

    # Priority 5: Walk up parent directories
    env_file_path = _walk_up_for_env_file()
    if env_file_path:
        load_dotenv(env_file_path)
        creds = _check_env_vars()
        if creds:
            return creds

    # Priority 6: Home directory .env files
    for env_variant in ENV_FILE_VARIANTS:
        home_env = Path.home() / env_variant
        if home_env.exists():
            load_dotenv(home_env)
            creds = _check_env_vars()
            if creds:
                return creds

    # Priority 7: MCP configuration
    creds = _load_mcp_config()
    if creds:
        return creds

    # No credentials found
    raise ValueError(
        "No Confluence credentials found. Please set environment variables or create "
        "one of: .env, .env.confluence, .env.jira, .env.atlassian\n"
        "Required variables: CONFLUENCE_URL, CONFLUENCE_USERNAME, CONFLUENCE_API_TOKEN"
    )


def get_confluence_client(env_file: Optional[str] = None, **overrides):
    """
    Get authenticated Confluence client.

    Args:
        env_file: Optional path to .env file
        **overrides: Optional credential overrides (url, username, token)

    Returns:
        atlassian.Confluence client instance

    Raises:
        ValueError: If credentials not found
        ImportError: If atlassian-python-api not installed
    """
    try:
        from atlassian import Confluence
    except ImportError:
        raise ImportError(
            "atlassian-python-api not installed. "
            "Install with: pip install atlassian-python-api"
        )

    # Get credentials
    creds = get_confluence_credentials(env_file)

    # Apply overrides
    url = overrides.get('url', creds['url'])
    username = overrides.get('username', creds['username'])
    token = overrides.get('token', creds['token'])

    # Determine if Cloud or Server/Data Center
    is_cloud = '.atlassian.net' in url

    return Confluence(
        url=url,
        username=username,
        password=token,  # atlassian-python-api uses 'password' for both password and token
        cloud=is_cloud
    )


if __name__ == '__main__':
    """Test credential discovery"""
    import sys

    try:
        creds = get_confluence_credentials()
        print("✅ Credentials found:")
        print(f"  URL: {creds['url']}")
        print(f"  Username: {creds['username']}")
        print(f"  Token: {'*' * len(creds['token'])}")

        # Test client creation
        client = get_confluence_client()
        print("\n✅ Confluence client created successfully")

    except ValueError as e:
        print(f"❌ Error: {e}", file=sys.stderr)
        sys.exit(1)
    except Exception as e:
        print(f"❌ Unexpected error: {e}", file=sys.stderr)
        sys.exit(1)
