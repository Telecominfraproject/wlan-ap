"""Jira client initialization for CLI scripts."""

from typing import Optional
from atlassian import Jira
from requests import Response

from .config import load_env, validate_config, get_auth_mode

# === INLINE_START: client ===


class CaptchaError(Exception):
    """Error raised when Jira requires CAPTCHA resolution.

    This happens when Jira Server/DC detects suspicious login activity
    and requires the user to complete a CAPTCHA challenge in the web UI.
    """

    def __init__(self, message: str, login_url: str):
        super().__init__(message)
        self.login_url = login_url


def _check_captcha_challenge(response: Response, jira_url: str) -> None:
    """Check response for CAPTCHA challenge and raise exception if found.

    Jira Server/DC may require CAPTCHA resolution after failed login attempts.
    This is indicated by the X-Authentication-Denied-Reason header containing
    'CAPTCHA_CHALLENGE'.

    Args:
        response: HTTP response to check
        jira_url: Base Jira URL for constructing login URL

    Raises:
        CaptchaError: If CAPTCHA challenge is detected
    """
    header_name = "X-Authentication-Denied-Reason"
    if header_name not in response.headers:
        return

    header_value = response.headers[header_name]
    if "CAPTCHA_CHALLENGE" not in header_value:
        return

    # Extract login URL if present in header
    login_url = f"{jira_url}/login.jsp"
    if "; login-url=" in header_value:
        login_url = header_value.split("; login-url=")[1].strip()

    raise CaptchaError(
        f"CAPTCHA challenge detected!\n\n"
        f"  Jira requires you to solve a CAPTCHA before API access is allowed.\n\n"
        f"  To resolve:\n"
        f"    1. Open {login_url} in your web browser\n"
        f"    2. Log in and complete the CAPTCHA challenge\n"
        f"    3. Retry this command\n\n"
        f"  This typically happens after several failed login attempts.",
        login_url=login_url
    )


def _patch_session_for_captcha(client: Jira, jira_url: str) -> None:
    """Patch the Jira client session to detect CAPTCHA challenges.

    The atlassian-python-api library doesn't check for CAPTCHA responses,
    so we patch the session's request method to add this check.

    Args:
        client: Jira client instance to patch
        jira_url: Base Jira URL for error messages
    """
    original_request = client._session.request

    def patched_request(method: str, url: str, **kwargs) -> Response:
        response = original_request(method, url, **kwargs)
        _check_captcha_challenge(response, jira_url)
        return response

    client._session.request = patched_request


def get_jira_client(env_file: Optional[str] = None) -> Jira:
    """Initialize and return a Jira client.

    Supports two authentication modes:
    - Cloud: JIRA_USERNAME + JIRA_API_TOKEN
    - Server/DC: JIRA_PERSONAL_TOKEN (Personal Access Token)

    Args:
        env_file: Optional path to environment file

    Returns:
        Configured Jira client instance

    Raises:
        FileNotFoundError: If env file doesn't exist
        ValueError: If configuration is invalid
        ConnectionError: If cannot connect to Jira
    """
    config = load_env(env_file)

    errors = validate_config(config)
    if errors:
        raise ValueError("Configuration errors:\n  " + "\n  ".join(errors))

    url = config['JIRA_URL']
    auth_mode = get_auth_mode(config)

    # Determine if Cloud or Server/DC
    is_cloud = config.get('JIRA_CLOUD', '').lower() == 'true'

    # Auto-detect if not specified
    if 'JIRA_CLOUD' not in config:
        # Strict check: must end with .atlassian.net (not just contain it)
        # This prevents bypass via malicious domains like attacker-atlassian.net.evil.com
        from urllib.parse import urlparse
        netloc = urlparse(url).netloc.lower()
        is_cloud = netloc == 'atlassian.net' or netloc.endswith('.atlassian.net')

    try:
        if auth_mode == 'pat':
            # Server/DC with Personal Access Token
            client = Jira(
                url=url,
                token=config['JIRA_PERSONAL_TOKEN'],
                cloud=is_cloud
            )
        else:
            # Cloud with username + API token
            client = Jira(
                url=url,
                username=config['JIRA_USERNAME'],
                password=config['JIRA_API_TOKEN'],
                cloud=is_cloud
            )

        # Patch session to detect CAPTCHA challenges (primarily for Server/DC)
        _patch_session_for_captcha(client, url)

        return client
    except CaptchaError:
        # Re-raise CAPTCHA errors with full context
        raise
    except Exception as e:
        if auth_mode == 'pat':
            raise ConnectionError(
                f"Failed to connect to Jira at {url}\n\n"
                f"  Error: {e}\n\n"
                f"  Please verify:\n"
                f"    - JIRA_URL is correct\n"
                f"    - JIRA_PERSONAL_TOKEN is a valid Personal Access Token\n"
            )
        else:
            raise ConnectionError(
                f"Failed to connect to Jira at {url}\n\n"
                f"  Error: {e}\n\n"
                f"  Please verify:\n"
                f"    - JIRA_URL is correct\n"
                f"    - JIRA_USERNAME is your email (Cloud) or username (Server/DC)\n"
                f"    - JIRA_API_TOKEN is valid\n"
            )

# === INLINE_END: client ===
