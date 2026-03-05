# jira-communication Specification

| Field | Value |
|-------|-------|
| Status | Active |
| Version | 1.0.0 |
| Created | 2026-02-20 |
| Depends | `uv` (Python package runner), Python 3.10+, `atlassian-python-api`, `click`, `~/.env.jira` |

## 1. Overview

The jira-communication skill provides a suite of **14 standalone Python CLI scripts** for interacting with Jira Cloud and Server/Data Center instances. Scripts handle issue CRUD, JQL search, status transitions, comments, worklogs, attachments, sprints, boards, fields, users, and issue links.

All scripts use `uv run` for zero-setup execution (PEP 723 inline dependencies), share a common library (`lib/`) for authentication, configuration, and output formatting, and follow a consistent CLI pattern built on Click.

**Users**: Developers, AI coding assistants, CI/CD pipelines.
**Trigger keywords**: Jira URLs (`https://*.atlassian.net/browse/*`), issue keys (`PROJ-123`), any Jira operation mention.

## 2. Architecture

### Directory Layout

```
.github/skills/jira-communication/
  SKILL.md                          # Primary AI reference (script table, examples)
  AGENTS.md                         # Developer guide for extending scripts
  .skillfish.json                   # Skill provenance (upstream: netresearch/jira-skill)
  scripts/
    lib/
      __init__.py                   # Re-exports: get_jira_client, format_output, etc.
      config.py                     # Environment config loading and validation
      client.py                     # Jira client initialization with CAPTCHA detection
      output.py                     # JSON, table, quiet output formatting
    core/                           # Essential operations
      jira-setup.py                 # Interactive credential configuration
      jira-validate.py              # Environment/connectivity validation
      jira-issue.py                 # Get/update issue details
      jira-search.py                # JQL search
      jira-worklog.py               # Time tracking (add/list)
      jira-attachment.py            # Download attachments
    workflow/                       # Status and collaboration
      jira-create.py                # Create issues
      jira-transition.py            # Status transitions (list/do)
      jira-comment.py               # Add/list comments
      jira-sprint.py                # Sprint operations (list/issues/current)
      jira-board.py                 # Board operations (list/issues)
    utility/                        # Discovery and metadata
      jira-fields.py                # Search/list fields (find custom field IDs)
      jira-user.py                  # User info (me/get)
      jira-link.py                  # Issue links (create/list-types)
  references/
    jql-quick-reference.md          # JQL operators, functions, common queries
    troubleshooting.md              # Setup errors, auth modes, debug tips
```

### Script Architecture Pattern

Every script follows the same structure:

1. PEP 723 inline dependency header (`# /// script`)
2. PYTHONPATH manipulation to find `lib/` directory
3. Click group with global flags (`--json`, `--quiet`, `--env-file`, `--debug`)
4. Subcommands with Click decorators
5. Jira client initialization in the group command (shared via `ctx.obj`)
6. Three output modes: human-readable table, JSON (`--json`), minimal (`--quiet`)
7. `--dry-run` flag on all write operations
8. Error handling with optional `--debug` for full stack traces

### Shared Library

| Module | Purpose |
|--------|---------|
| `lib/config.py` | Loads `~/.env.jira` or env vars; validates Cloud vs Server/DC auth; priority: explicit file > default file > env vars |
| `lib/client.py` | Creates `atlassian.Jira` client; auto-detects Cloud/DC from URL; patches session for CAPTCHA challenge detection on Server/DC |
| `lib/output.py` | `format_output()` (JSON/table/quiet), `format_table()` (ASCII tables), `error()`/`success()`/`warning()` helpers |

## 3. Interface / Subcommands

### Critical: Flag Ordering Rule

Global flags (`--json`, `--quiet`, `--debug`, `--env-file`) **MUST** come **BEFORE** the subcommand name. This is a Click framework requirement.

```bash
# CORRECT
uv run scripts/core/jira-issue.py --json get PROJ-123

# WRONG (will fail with "No such option: --json")
uv run scripts/core/jira-issue.py get PROJ-123 --json
```

### Core Scripts

#### jira-setup.py -- Interactive Credential Setup

```bash
uv run scripts/core/jira-setup.py [--url URL] [--type cloud|server|auto] [--output PATH] [--force] [--test-only]
```

Walks through: URL validation, auth type detection (Cloud vs DC), credential input, credential validation (calls `myself()`), file write with `chmod 600`.

#### jira-validate.py -- Environment Validation

```bash
uv run scripts/core/jira-validate.py [--verbose] [--project KEY] [--json] [--quiet]
```

Checks: runtime (`uv` installed, Python >= 3.10), environment (`~/.env.jira` exists with valid vars), connectivity (server reachable, auth works, optional project access). Exit codes: 0=ok, 1=runtime, 2=config, 3=connectivity.

#### jira-issue.py -- Get/Update Issues

| Subcommand | Arguments | Key Options |
|------------|-----------|-------------|
| `get` | `ISSUE_KEY` | `--fields`, `--expand`, `--truncate N` |
| `update` | `ISSUE_KEY` | `--summary`, `--priority`, `--labels`, `--assignee`, `--fields-json`, `--dry-run` |

Handles both string and ADF (Atlassian Document Format) descriptions. Assignee accepts email, username, or account ID.

#### jira-search.py -- JQL Search

| Subcommand | Arguments | Key Options |
|------------|-----------|-------------|
| `query` | `JQL` | `--max-results N`, `--fields`, `--truncate N` |

Default fields: `key,summary,status,assignee,priority`. Outputs ASCII table with auto-width columns.

#### jira-worklog.py -- Time Tracking

| Subcommand | Arguments | Key Options |
|------------|-----------|-------------|
| `add` | `ISSUE_KEY TIME_SPENT` | `--comment`, `--started` (ISO format) |
| `list` | `ISSUE_KEY` | `--limit N`, `--truncate N` |

Time format: Jira notation (`2h 30m`, `1d`, `30m`). ISO timestamp normalization handles various input formats.

#### jira-attachment.py -- Download Attachments

| Subcommand | Arguments | Key Options |
|------------|-----------|-------------|
| `download` | `ATTACHMENT_URL OUTPUT_FILE` | (inherits global flags) |

Supports full URLs or relative paths. Streams large files in 1MB chunks. Handles both Cloud (basic auth) and Server/DC (bearer token) authentication.

### Workflow Scripts

#### jira-create.py -- Create Issues

| Subcommand | Arguments | Key Options |
|------------|-----------|-------------|
| `issue` | `PROJECT_KEY SUMMARY` | `--type` (required), `--description`, `--priority`, `--labels`, `--assignee`, `--parent`, `--components`, `--fields-json`, `--dry-run` |

For GW3 project (S12W01): must include `customfield_10069` (Issue Category) with value `{"id": "10041"}` (SW) via `--fields-json`.

#### jira-transition.py -- Status Changes

| Subcommand | Arguments | Key Options |
|------------|-----------|-------------|
| `list` | `ISSUE_KEY` | Shows available transitions from current state |
| `do` | `ISSUE_KEY STATUS_NAME` | `--comment`, `--resolution`, `--dry-run` |

Case-insensitive status matching. Matches against both transition name and target status name.

#### jira-comment.py -- Comments

| Subcommand | Arguments | Key Options |
|------------|-----------|-------------|
| `add` | `ISSUE_KEY COMMENT_TEXT` | Comments use Jira wiki markup, NOT Markdown |
| `list` | `ISSUE_KEY` | `--limit N`, `--truncate N` |

Comments are displayed newest-first. Handles ADF format in responses.

#### jira-sprint.py -- Sprint Operations

| Subcommand | Arguments | Key Options |
|------------|-----------|-------------|
| `list` | `BOARD_ID` | `--state active\|future\|closed` |
| `issues` | `SPRINT_ID` | `--fields` |
| `current` | `BOARD_ID` | Shows active sprint details |

Uses Jira Agile REST API (`rest/agile/1.0/`).

#### jira-board.py -- Board Operations

| Subcommand | Arguments | Key Options |
|------------|-----------|-------------|
| `list` | (none) | `--project KEY`, `--type scrum\|kanban` |
| `issues` | `BOARD_ID` | `--jql`, `--max-results N` |

### Utility Scripts

#### jira-fields.py -- Field Discovery

| Subcommand | Arguments | Key Options |
|------------|-----------|-------------|
| `search` | `KEYWORD` | `--limit N` |
| `list` | (none) | `--type custom\|system\|all`, `--limit N` |

Essential for finding custom field IDs needed for `--fields-json` in create/update operations.

#### jira-user.py -- User Information

| Subcommand | Arguments | Key Options |
|------------|-----------|-------------|
| `me` | (none) | Shows authenticated user details |
| `get` | `IDENTIFIER` | Accepts username, email, or account ID |

#### jira-link.py -- Issue Links

| Subcommand | Arguments | Key Options |
|------------|-----------|-------------|
| `create` | `FROM_KEY TO_KEY` | `--type` (required: "Blocks", "Relates", etc.), `--dry-run` |
| `list-types` | (none) | Shows available link types with inward/outward names |

## 4. Logic / Workflow

### Authentication Flow

1. Load config from `~/.env.jira` (or explicit `--env-file`)
2. Fill missing values from environment variables
3. Validate: `JIRA_URL` always required
4. Determine auth mode:
   - If `JIRA_PERSONAL_TOKEN` present: Server/DC PAT authentication
   - If `JIRA_USERNAME` + `JIRA_API_TOKEN` present: Cloud basic authentication
5. Auto-detect Cloud vs DC from URL (strict `.atlassian.net` check; override with `JIRA_CLOUD=true|false`)
6. Create `atlassian.Jira` client instance
7. Patch session with CAPTCHA challenge detection (Server/DC)

### Auto-Trigger Logic (AI Assistant)

1. User mentions Jira URL -> extract issue key from URL path -> run `jira-issue.py get <KEY>`
2. User mentions issue key pattern (`PROJ-123`) -> run `jira-issue.py get <KEY>`
3. Auth failure on any operation -> offer `jira-setup.py` for interactive configuration

### GW3 Project Setup

1. Install `uv`: `curl -LsSf https://astral.sh/uv/install.sh | sh`
2. Create Atlassian API token (shared with Confluence, NOT with Bitbucket)
3. Write `~/.env.jira` with `JIRA_URL=https://actiontec.atlassian.net`, `JIRA_USERNAME`, `JIRA_API_TOKEN`
4. Set file permissions: `chmod 600 ~/.env.jira`
5. Verify: `uv run scripts/core/jira-validate.py --verbose`
6. GW3 project key: `S12W01`, board ID: `301`

### GW3 Issue Creation Requirements

When creating issues in project S12W01, the `customfield_10069` (Issue Category) field is **mandatory**. Use value `{"id": "10041"}` for SW category:

```bash
uv run scripts/workflow/jira-create.py issue S12W01 "Issue summary" \
  --type Task \
  --fields-json '{"customfield_10069": {"id": "10041"}}'
```

**Known issue**: The `--assignee` flag may not work correctly when combined with `--parent` during creation. Assign separately via `jira-issue.py update`.

## 5. Safety Rules

- **Never hardcode credentials** in scripts, commands, or automation. Use `~/.env.jira` (chmod 600) or environment variables.
- **Flag ordering is critical**: Global flags MUST precede subcommands. Failure to follow this causes confusing "No such option" errors.
- **Always use `--dry-run`** on write operations (`create`, `update`, `transition`, `link create`) before executing for the first time or in bulk.
- **Jira wiki markup, not Markdown**: Comments and descriptions use Jira wiki syntax (`*bold*`, `_italic_`, `{code}...{code}`). The related `jira-syntax` skill provides conversion guidance.
- **Search API version**: Use `/rest/api/3/search/jql` (GET). Older `/search` endpoints return 410 on Cloud.
- **Sprint assignment**: Use POST to `/rest/agile/1.0/sprint/{id}/issue` with `{"issues": ["KEY-123"]}`. This is not exposed as a script subcommand and requires `bkt api` or direct REST calls.
- **Board pagination**: Default 50 results. Use `--project S12W01` to find board 301 for GW3.
- **CAPTCHA handling**: On Server/DC, failed login attempts may trigger CAPTCHA. The client detects this and provides instructions to resolve via browser login.

## 6. Dependencies

### Runtime

| Tool | Version | Purpose |
|------|---------|---------|
| `uv` | Latest | Python package runner (handles inline deps) |
| Python | >= 3.10 | Script runtime |

### Python Packages (PEP 723 inline)

| Package | Version | Purpose |
|---------|---------|---------|
| `atlassian-python-api` | >= 3.41.0 | Jira REST API client |
| `click` | >= 8.1.0 | CLI framework |
| `requests` | >= 2.28.0 | HTTP client (attachment downloads, setup validation) |

### Configuration

| File/Variable | Purpose |
|---------------|---------|
| `~/.env.jira` | Primary credential store (Cloud: `JIRA_URL` + `JIRA_USERNAME` + `JIRA_API_TOKEN`; DC: `JIRA_URL` + `JIRA_PERSONAL_TOKEN`) |
| `JIRA_CLOUD` | Optional override for Cloud/DC detection (`true`/`false`) |

### Related Skills

- **jira-syntax**: Jira wiki markup formatting for descriptions and comments (companion skill)
- **confluence**: Shares the same Atlassian API token
- **bkt**: Requires a separate Bitbucket-scoped token

## 7. Future Work

- Add `--assignee` fix for create-with-parent race condition (use separate update call)
- Sprint assignment subcommand in `jira-sprint.py`
- Bulk operations: transition multiple issues, batch comment
- Attachment upload support (currently download-only)
- Worklog reporting/aggregation across issues
- Integration with `gw3-branch` for automated issue-to-branch linking
