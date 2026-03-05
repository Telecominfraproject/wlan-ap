---
name: jira-communication
description: "Use when interacting with Jira issues - searching, creating, updating, transitioning, commenting, logging work, downloading attachments, managing sprints, boards, issue links, fields, or users. Auto-triggers on Jira URLs and issue keys (PROJ-123)."
allowed-tools: Bash(uv run scripts/*:*) Read
---

# Jira Communication

CLI scripts for Jira operations using `uv run`. All scripts support `--help`, `--json`, `--quiet`, `--debug`.

## Auto-Trigger

Trigger when user mentions:
- **Jira URLs**: `https://jira.*/browse/*`, `https://*.atlassian.net/browse/*`
- **Issue keys**: `PROJ-123`, `NRS-4167`

When triggered by URL → extract issue key → run `jira-issue.py get PROJ-123`

## Auth Failure Handling

When auth fails, offer: `uv run scripts/core/jira-setup.py` (interactive credential setup)

## Scripts

| Script | Purpose |
|--------|---------|
| `scripts/core/jira-setup.py` | Interactive credential config |
| `scripts/core/jira-validate.py` | Verify connection |
| `scripts/core/jira-issue.py` | Get/update issue details |
| `scripts/core/jira-search.py` | Search with JQL |
| `scripts/core/jira-worklog.py` | Time tracking |
| `scripts/core/jira-attachment.py` | Download attachments |
| `scripts/workflow/jira-create.py` | Create issues |
| `scripts/workflow/jira-transition.py` | Change status |
| `scripts/workflow/jira-comment.py` | Add comments |
| `scripts/workflow/jira-sprint.py` | List sprints |
| `scripts/workflow/jira-board.py` | List boards |
| `scripts/utility/jira-user.py` | User info |
| `scripts/utility/jira-fields.py` | Search fields |
| `scripts/utility/jira-link.py` | Issue links |

## Critical: Flag Ordering

Global flags **MUST** come **before** subcommand:
```bash
# Correct:  uv run scripts/core/jira-issue.py --json get PROJ-123
# Wrong:    uv run scripts/core/jira-issue.py get PROJ-123 --json
```

## Quick Examples

```bash
uv run scripts/core/jira-validate.py --verbose
uv run scripts/core/jira-search.py query "assignee = currentUser()"
uv run scripts/core/jira-issue.py get PROJ-123
uv run scripts/core/jira-worklog.py add PROJ-123 2h --comment "Work done"
uv run scripts/workflow/jira-transition.py do PROJ-123 "In Progress" --dry-run
```

## Related Skills

**jira-syntax**: For descriptions/comments. Jira uses wiki markup, NOT Markdown.

## References

- `references/jql-quick-reference.md` - JQL syntax
- `references/troubleshooting.md` - Setup and auth issues

## Authentication

**Cloud**: `JIRA_URL` + `JIRA_USERNAME` + `JIRA_API_TOKEN`
**Server/DC**: `JIRA_URL` + `JIRA_PERSONAL_TOKEN`

Config via `~/.env.jira` or env vars. Run `jira-validate.py --verbose` to verify.

## GW3 Project Setup

Follow these steps to set up the Jira skill for the GW3 project (Actiontec Jira Cloud).

### Prerequisites

- **uv** (Python package runner): `curl -LsSf https://astral.sh/uv/install.sh | sh`

### Step 1: Create an Atlassian API token

Jira, Confluence, and other Atlassian Cloud products (except Bitbucket) share the same API token. If you already have a token for Confluence, reuse it here.

1. Go to [Atlassian Account Settings > API tokens](https://id.atlassian.com/manage-profile/security/api-tokens)
2. Click **Create API token**
3. Copy the token

> **Note**: Bitbucket requires a separate scoped token — see the `bkt` skill for details.

### Step 2: Create `~/.env.jira`

```bash
cat > ~/.env.jira << 'EOF'
# Jira CLI Configuration
JIRA_URL=https://actiontec.atlassian.net
JIRA_USERNAME=<your-email>@actiontec.com
JIRA_API_TOKEN=<your-token>
EOF
chmod 600 ~/.env.jira
```

Or run the interactive setup:
```bash
uv run .github/skills/jira-communication/scripts/core/jira-setup.py --url https://actiontec.atlassian.net
```

### Step 3: Verify

```bash
uv run .github/skills/jira-communication/scripts/core/jira-validate.py --verbose
```

### GW3 Jira Project

- **Project key**: `S12W01`
- Common queries:
  ```bash
  # My open issues
  uv run .github/skills/jira-communication/scripts/core/jira-search.py query "project = S12W01 AND assignee = currentUser() AND status != Done"

  # View issue
  uv run .github/skills/jira-communication/scripts/core/jira-issue.py get S12W01-1234
  ```
