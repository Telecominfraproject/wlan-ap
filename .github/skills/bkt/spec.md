# bkt Specification

| Field | Value |
|-------|-------|
| Status | Active |
| Version | 1.0.0 |
| Created | 2026-02-20 |
| Depends | `bkt` CLI (Go binary), OS keychain or `BKT_KEYRING_PASSPHRASE` |

## 1. Overview

The `bkt` skill provides AI coding assistants with reference documentation for the **Bitbucket CLI** (`bkt`), a unified command-line interface for both Bitbucket Data Center and Bitbucket Cloud. It mirrors `gh` (GitHub CLI) ergonomics and produces structured JSON/YAML output suitable for automation.

This is a **prompt-only skill** (no scripts). It teaches the AI assistant how to invoke `bkt` commands correctly, handle authentication differences between Data Center and Cloud, configure contexts, and perform GW3-specific PR workflows against the Actiontec Bitbucket Cloud workspace.

**Users**: Developers, AI coding assistants, CI/CD pipelines.
**Trigger keywords**: "bitbucket", "bkt", "pull request", "PR", "repo list", "branch create", "Bitbucket Data Center", "Bitbucket Cloud", "keyring timeout".

## 2. Architecture

### Directory Layout

```
.github/skills/bkt/
  SKILL.md              # Primary reference (loaded by AI assistants)
  .skillfish.json       # Skill provenance metadata (upstream repo/sha)
  references/
    commands.md         # Exhaustive command reference for all bkt subcommands
```

### Upstream Source

The skill originates from `avivsinai/bitbucket-cli` (branch `master`), pinned at commit `33d176d`. It was installed manually and is maintained as a local copy.

## 3. Interface / Subcommands

### Authentication (`bkt auth`)

| Subcommand | Purpose | Key Flags |
|------------|---------|-----------|
| `auth login <url>` | Authenticate to a Bitbucket instance | `--kind cloud\|dc`, `--username`, `--token`, `--web`, `--allow-insecure-store` |
| `auth status` | Show configured hosts and contexts | |
| `auth logout <host>` | Remove stored credentials | |

**Cloud vs Data Center auth differences**:
- **Cloud**: `--kind cloud --username <email> --token <bitbucket-scoped-api-token>`. Token must be created at Atlassian Account Settings with Bitbucket application scope. General Atlassian API tokens will NOT work.
- **Data Center**: Default kind. Uses a Personal Access Token (PAT). `--username <pat-owner> --token <PAT>`.

### Context Management (`bkt context`)

| Subcommand | Purpose | Key Flags |
|------------|---------|-----------|
| `context create <name>` | Create named context | `--host`, `--project` (DC), `--workspace` (Cloud), `--repo`, `--set-active` |
| `context list` | List contexts (asterisk marks active) | |
| `context use <name>` | Switch active context | |
| `context delete <name>` | Remove a context | |

### Repository Operations (`bkt repo`)

| Subcommand | Purpose |
|------------|---------|
| `repo list` | List repos in project/workspace |
| `repo view <slug>` | View repo details |
| `repo create <name>` | Create repository |
| `repo clone <slug>` | Clone via HTTPS or `--ssh` |
| `repo browse` | Print/open repo web URL |

### Pull Request Workflows (`bkt pr`)

| Subcommand | Purpose |
|------------|---------|
| `pr list` | List PRs (filter: `--state`, `--mine`) |
| `pr view <id>` | View PR details (`--web` opens browser) |
| `pr create` | Create PR (`--title`, `--source`, `--target`, `--reviewer`) |
| `pr edit <id>` | Edit title/body |
| `pr approve <id>` | Approve a PR |
| `pr comment <id>` | Add comment |
| `pr merge <id>` | Merge (`--strategy`, `--message`, `--close-source`) |
| `pr checks <id>` | Build/CI status (`--wait`, `--timeout`, `--fail-fast`) |
| `pr checkout <id>` | Fetch PR branch locally |
| `pr diff <id>` | Show PR diff (`--stat` for stats) |
| `pr task` | PR tasks: list, create, complete, reopen (DC only) |
| `pr suggestion` | Apply code suggestions (DC only) |
| `pr auto-merge` | Enable/disable/status of auto-merge |
| `pr reaction` | Reactions on PR comments (DC only) |
| `pr reviewer-group` | Default reviewer group management (DC only) |

### Branch Management (`bkt branch`)

| Subcommand | Purpose |
|------------|---------|
| `branch list` | List branches (`--filter` for pattern matching) |
| `branch create <name>` | Create branch (`--from <ref>`) |
| `branch delete <name>` | Delete branch (`--dry-run`) |
| `branch set-default` | Set default branch (DC only) |
| `branch protect` | Branch restrictions: list, add, remove (DC only) |
| `branch rebase` | Rebase branch (DC only) |

### Issue Tracking (`bkt issue`) -- Cloud Only

| Subcommand | Purpose |
|------------|---------|
| `issue list` | List issues (filter: `--state`, `--kind`, `--priority`) |
| `issue view <id>` | View issue (`--comments`, `--web`) |
| `issue create` | Create issue (`-t`, `-k`, `-p`, `-a`) |
| `issue edit <id>` | Edit issue fields |
| `issue close / reopen / delete` | Lifecycle operations |
| `issue comment <id>` | Add/list comments |
| `issue status` | Your assigned/created issues |
| `issue attachment` | List, upload, download, delete attachments |

### Other Command Groups

| Group | Purpose | Scope |
|-------|---------|-------|
| `webhook` | Create, list, delete, test webhooks | DC + Cloud |
| `pipeline` | Run, list, view, logs for pipelines | Cloud only |
| `perms` | Project and repo permissions | DC only |
| `status` | Commit/PR/pipeline/rate-limit status | Mixed |
| `project list` | List projects | DC only |
| `admin` | Secrets rotation, logging config | DC only |
| `api` | Raw REST API escape hatch | DC + Cloud |
| `extension` | Install/remove/exec CLI extensions | DC + Cloud |

### Global Options (all commands)

| Flag | Purpose |
|------|---------|
| `-c, --context <name>` | Use specific context |
| `--json` | JSON output |
| `--yaml` | YAML output |
| `--jq <expr>` | Apply jq expression (requires `--json`) |
| `--template <tmpl>` | Go template rendering |
| `--project <key>` | Override project (DC, per-command) |
| `--workspace <name>` | Override workspace (Cloud, per-command) |
| `--repo <slug>` | Override repository (per-command) |

## 4. Logic / Workflow

### AI Assistant Decision Logic

1. **Dependency check**: Before any `bkt` command, verify installation with `bkt --version`. If missing, guide installation.
2. **Context awareness**: Check `bkt auth status` to determine if authentication is configured. If not, guide through login flow.
3. **DC vs Cloud routing**: Inspect the context host. Cloud uses `api.bitbucket.org` and workspace-based routing. DC uses project-based routing. Some commands are platform-specific (issues = Cloud only, permissions = DC only).
4. **Output parsing**: Default to `--json` for structured output when automation or data extraction is needed. Use `| jq` for filtering.

### GW3 Project Setup Flow

1. Install `bkt` via `go install github.com/avivsinai/bitbucket-cli/cmd/bkt@latest`
2. Add `~/go/bin` to PATH
3. Set `BKT_KEYRING_PASSPHRASE="bkt"` in `~/.bashrc` (WSL requirement)
4. Create Bitbucket-scoped API token at Atlassian Account Settings (NOT a general Atlassian token)
5. `bkt auth login https://bitbucket.org --kind cloud --allow-insecure-store --username <email> --token <token>`
6. `bkt context create actiontec --host api.bitbucket.org --workspace Actiontec --set-active`
7. Verify: `bkt auth status && bkt repo list --limit 5`

## 5. Safety Rules

- **Never hardcode tokens** in commands or scripts. Tokens are stored in the OS keychain or encrypted file backend.
- **WSL/headless environments**: Must set `BKT_KEYRING_PASSPHRASE` to any non-empty value to avoid blocking interactive prompts. Use `--allow-insecure-store` during login.
- **Token separation**: Bitbucket requires its own scoped API token. The general Atlassian API token used for Jira/Confluence will NOT work for Bitbucket.
- **Cloud token scopes**: Must select "Bitbucket" as the application when creating the token. Required scope: Account: Read. Recommended: Repositories (Read/Write), Pull requests (Read/Write).
- **Platform-specific commands**: Do not attempt issue commands on DC instances or permission commands on Cloud instances. The AI should check the active context type first.

## 6. Dependencies

### External Tools

| Tool | Required | Installation |
|------|----------|-------------|
| `bkt` | Yes | `go install github.com/avivsinai/bitbucket-cli/cmd/bkt@latest` or binary from GitHub Releases |
| Go 1.24+ | For source install | System package manager |

### Environment Variables

| Variable | Purpose | When Needed |
|----------|---------|-------------|
| `BKT_KEYRING_PASSPHRASE` | Encrypted file backend passphrase | WSL/headless (required) |
| `BKT_ALLOW_INSECURE_STORE` | Allow encrypted file-based credential storage | WSL/headless (alternative to login flag) |
| `BKT_KEYRING_TIMEOUT` | Keyring operation timeout | Interactive keyring unlock scenarios |
| `BKT_CONFIG_DIR` | Config directory override | Non-standard config locations |
| `BKT_HTTP_DEBUG` | Log HTTP requests/responses | Debugging API issues |

### Related Skills

- **jira-communication**: Shares the Atlassian ecosystem but uses a different token
- **confluence**: Same Atlassian API token as Jira; separate from Bitbucket

## 7. Future Work

- Automate `bkt` installation check and guided setup within the skill
- Add script-based wrappers for common multi-step GW3 workflows (e.g., create PR across multiple feed repos)
- Support for bulk PR operations across the GW3 feed ecosystem
