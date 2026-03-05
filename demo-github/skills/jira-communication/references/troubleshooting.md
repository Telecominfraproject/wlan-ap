# Troubleshooting Guide

## Setup Validation

Always start with:
```bash
uv run scripts/core/jira-validate.py --verbose
```

### Exit Codes
| Code | Meaning | Action |
|------|---------|--------|
| 0 | All checks passed | Ready to use |
| 1 | Runtime dependency missing | Install `uv` |
| 2 | Environment config error | Check `~/.env.jira` |
| 3 | Connectivity/auth failure | Verify credentials |

## Configuration

Scripts load configuration in priority order:
1. `~/.env.jira` file (if exists)
2. Environment variables (fallback for missing values)

You can use either approach or combine them.

### Option A: Environment File

Create `~/.env.jira`:

### Jira Cloud
```bash
JIRA_URL=https://yourcompany.atlassian.net
JIRA_USERNAME=your-email@example.com
JIRA_API_TOKEN=your-api-token-here
```

### Jira Server/Data Center
```bash
JIRA_URL=https://jira.yourcompany.com
JIRA_PERSONAL_TOKEN=your-personal-access-token
```

### Option B: Environment Variables

Export variables directly (useful in CI/CD or when credentials are managed externally):

```bash
# Jira Cloud
export JIRA_URL=https://yourcompany.atlassian.net
export JIRA_USERNAME=your-email@example.com
export JIRA_API_TOKEN=your-api-token-here

# Or Jira Server/DC
export JIRA_URL=https://jira.yourcompany.com
export JIRA_PERSONAL_TOKEN=your-personal-access-token
```

## Common Errors

### "Configuration errors: Missing required"

**Cause**: Required variables not found in file or environment.

**Fix**:
1. Check `~/.env.jira` exists with correct values, OR
2. Verify environment variables are exported
3. Variable names are case-sensitive
4. No quotes around values needed in `.env.jira`

### "Failed to connect to Jira"

**Cause**: Network, URL, or SSL issues.

**Fix**:
1. Verify URL is correct (include `https://`)
2. Test URL in browser
3. Check VPN if on corporate network
4. For self-signed certs, may need `JIRA_VERIFY_SSL=false`

### "401 Unauthorized"

**Cause**: Invalid credentials.

**Cloud Fix**:
1. Generate new API token at https://id.atlassian.com/manage-profile/security/api-tokens
2. Use email as `JIRA_USERNAME`, not display name

**Server/DC Fix**:
1. Create PAT in Jira: Profile → Personal Access Tokens
2. Use only `JIRA_PERSONAL_TOKEN`, not username/password

### "403 Forbidden"

**Cause**: Valid auth but no permission.

**Fix**:
1. Verify account has project access
2. Check if IP allowlisting blocks API access
3. Confirm API access not disabled by admin

### "No such option: --json"

**Cause**: Flag placed after subcommand.

**Fix**: Move flags before subcommand:
```bash
# Wrong
uv run scripts/core/jira-issue.py get PROJ-123 --json

# Correct
uv run scripts/core/jira-issue.py --json get PROJ-123
```

### "Issue does not exist"

**Cause**: Wrong key or no permission.

**Fix**:
1. Verify issue key spelling and case
2. Confirm you have "Browse" permission on project
3. Check if issue was moved/deleted

### "Field 'xyz' cannot be set"

**Cause**: Field not editable or wrong format.

**Fix**:
1. Use `jira-fields.py search xyz` to find correct field ID
2. Check field is on the edit screen for that issue type
3. Verify field format (some need `{"name": "value"}`)

## Debug Mode

Add `--debug` for full stack traces:
```bash
uv run scripts/core/jira-issue.py --debug get PROJ-123
```

## Auth Mode Detection

Scripts auto-detect auth mode:
- If `JIRA_PERSONAL_TOKEN` set → Server/DC PAT auth
- If `JIRA_USERNAME` + `JIRA_API_TOKEN` set → Cloud basic auth
- URL containing `.atlassian.net` → Cloud mode

Override with `JIRA_CLOUD=true` or `JIRA_CLOUD=false`.
