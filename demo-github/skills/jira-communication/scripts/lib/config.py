"""Environment configuration handling for Jira CLI scripts."""

import os
from pathlib import Path
from typing import Optional

# === INLINE_START: config ===

DEFAULT_ENV_FILE = Path.home() / ".env.jira"

# Cloud authentication: JIRA_USERNAME + JIRA_API_TOKEN
# Server/DC authentication: JIRA_PERSONAL_TOKEN (PAT)
REQUIRED_URL = 'JIRA_URL'
CLOUD_VARS = ['JIRA_USERNAME', 'JIRA_API_TOKEN']
SERVER_VARS = ['JIRA_PERSONAL_TOKEN']
OPTIONAL_VARS = ['JIRA_CLOUD']
ALL_VARS = [REQUIRED_URL] + CLOUD_VARS + SERVER_VARS + OPTIONAL_VARS


def load_env(env_file: Optional[str] = None) -> dict:
    """Load configuration from file with environment variable fallback.

    Priority order:
    1. Explicit env_file parameter (must exist if specified)
    2. ~/.env.jira (if exists)
    3. Environment variables (fallback for missing values)

    Supports two authentication modes:
    - Cloud: JIRA_URL + JIRA_USERNAME + JIRA_API_TOKEN
    - Server/DC: JIRA_URL + JIRA_PERSONAL_TOKEN

    Args:
        env_file: Path to environment file. If specified, file must exist.

    Returns:
        Dictionary of configuration values

    Raises:
        FileNotFoundError: If explicit env_file doesn't exist
    """
    config = {}
    path = Path(env_file) if env_file else DEFAULT_ENV_FILE

    # Load from file if it exists (or raise if explicitly specified but missing)
    if path.exists():
        with open(path) as f:
            for line in f:
                line = line.strip()
                if line and not line.startswith('#') and '=' in line:
                    key, _, value = line.partition('=')
                    config[key.strip()] = value.strip().strip('"').strip("'")
    elif env_file:
        # Explicit file was specified but doesn't exist
        raise FileNotFoundError(f"Environment file not found: {path}")

    # Fill in missing values from environment variables
    for var in ALL_VARS:
        if var not in config and var in os.environ:
            config[var] = os.environ[var]

    return config


def validate_config(config: dict) -> list:
    """Validate configuration has all required variables.

    Supports two authentication modes:
    - Cloud: JIRA_URL + JIRA_USERNAME + JIRA_API_TOKEN
    - Server/DC: JIRA_URL + JIRA_PERSONAL_TOKEN

    Args:
        config: Configuration dictionary

    Returns:
        List of validation errors (empty if valid)
    """
    errors = []

    # JIRA_URL is always required
    if REQUIRED_URL not in config or not config[REQUIRED_URL]:
        errors.append(f"Missing required variable: {REQUIRED_URL}")

    # Validate URL format
    if REQUIRED_URL in config and config[REQUIRED_URL]:
        url = config[REQUIRED_URL]
        if not url.startswith(('http://', 'https://')):
            errors.append(f"JIRA_URL must start with http:// or https://: {url}")

    # Check for valid authentication configuration
    has_cloud_auth = all(config.get(var) for var in CLOUD_VARS)
    has_server_auth = config.get('JIRA_PERSONAL_TOKEN')

    if not has_cloud_auth and not has_server_auth:
        errors.append(
            "Missing authentication credentials. Provide either:\n"
            "    - JIRA_USERNAME + JIRA_API_TOKEN (for Cloud)\n"
            "    - JIRA_PERSONAL_TOKEN (for Server/DC)"
        )

    return errors


def get_auth_mode(config: dict) -> str:
    """Determine authentication mode from config.

    Args:
        config: Configuration dictionary

    Returns:
        'cloud' for Cloud auth, 'pat' for Personal Access Token
    """
    if config.get('JIRA_PERSONAL_TOKEN'):
        return 'pat'
    return 'cloud'

# === INLINE_END: config ===
