# gw3-src-local Specification

| Field | Value |
|-------|-------|
| Status | Active |
| Version | 1.0.0 |
| Created | 2026-02-20 |
| Depends | git, python3, bash, OpenWrt feeds system, `include/aei-local-development.mk` |

## 1. Overview

The `gw3-src-local` skill provides a three-tier local development system for GW3 (prplOS).
It enables developers and AI assistants to work with feed-level packaging files (Makefiles,
ODL definitions, patches) and package source code (C/C++) in local git checkouts, bypassing
the normal download-and-extract build flow.

**Users:** Developers and AI assistants performing cross-module feature work.

**When to use:**
- Starting a new feature that spans multiple packages or feeds
- Editing package source code that normally lives in external repositories
- Preparing a feature branch for merge (pinning commits)
- Hotfixing a release branch

## 2. Architecture

### 2.1 Three-Tier Model

| Tier | Location | Purpose | Editable |
|------|----------|---------|----------|
| Project | `<root>/` (prplos repo) | Profiles, build system, specs | Yes (always present) |
| Feed | `aei/<feed>/` | Makefiles, ODL, patches, configs | Yes (via src-link) |
| Module | `aei/src/<pkg>/` | C/C++ source code | Yes (via local override) |

### 2.2 Directory Layout

```
aei/                                # Local development directory (gitignored)
├── .srclocal.state                 # Feed tier state: original feeds.conf lines
├── .srclocal-source.state          # Module tier state: pkg=url mappings
├── feed_prplos/                    # Feed checkout (live git repo)
│   ├── mod-amx-thread/             #   Package recipe: Makefile, files/, patches/
│   └── mod-amx-wifi/
├── feed_aei/                       # Another feed checkout
│   └── aei-network/
└── src/                            # Module checkouts (live git repos)
    ├── mod-amx-thread/             #   Actual C/C++ source
    │   ├── src/
    │   ├── include/
    │   ├── CMakeLists.txt
    │   └── .git/
    └── mod-amx-wifi/
```

### 2.3 Build System Integration

The module tier relies on `include/aei-local-development.mk`, which is globally included
from `package.mk`. When `aei/src/<PKG_NAME>/` exists and is non-empty, it overrides:

| Variable | Override Value |
|----------|---------------|
| `PKG_VERSION` | `local-<git-describe>` |
| `PKG_SOURCE_URL` | *(cleared)* |
| `PKG_SOURCE` | *(cleared)* |
| `PKG_HASH` | `skip` |
| `PKG_MIRROR_HASH` | `skip` |
| `PKG_UNPACK` | `cp` from `aei/src/<pkg>/` into `PKG_BUILD_DIR` |

Feed tier changes are detected automatically because `feeds/<feed>` symlinks resolve
through `src-link` entries to `aei/<feed>/`, so the build system sees modified timestamps
without any override mechanism.

### 2.4 Data Flow

```
feeds.conf  ──src-link──►  aei/<feed>/  ──symlink──►  feeds/<feed>/<pkg>/
                                                           │
                                    Makefile references ───┘
                                    PKG_SOURCE_URL ───► aei/src/<pkg>/ (if present)
                                                   └──► remote git (fallback)
```

## 3. Interface / Subcommands

### 3.1 Script Path

```
.github/skills/gw3-src-local/src_local.sh
```

### 3.2 Feed Tier Commands

| Command | Description |
|---------|-------------|
| `src_local.sh [feed[:branch] ...]` | Set up feed src-links for named feeds |
| `src_local.sh` *(no args)* | Interactive mode: show status, present feed menu |
| `src_local.sh --deep [feed[:branch] ...]` | Src-link feed + auto-clone all GW3-owned module sources |
| `src_local.sh --clean` | Restore original feeds.conf lines; remove published repos |
| `src_local.sh --status` | Display state of all feeds and modules |

### 3.3 Module Tier Commands

| Command | Description |
|---------|-------------|
| `src_local.sh --source <pkg[:branch]> [...]` | Clone specific module source repos |
| `src_local.sh --source <pkg> --url <url>` | Clone with explicit git URL |
| `src_local.sh --source-clean [<pkg> ...]` | Remove module state (specific or all) |

### 3.4 Lifecycle / Pinning Commands

| Command | Description |
|---------|-------------|
| `src_local.sh --pin-sources` | Pin module HEAD commits into feed Makefiles (`PKG_SOURCE_VERSION`) |
| `src_local.sh --pin-feeds` | Pin feed HEAD commits into profile YAMLs (`revision:`) |
| `src_local.sh --pin` | Run both `--pin-sources` then `--pin-feeds` in sequence |

### 3.5 Environment Variables

| Variable | Default | Purpose |
|----------|---------|---------|
| `GW3_AEI_DIR` | `<project_root>/aei` | Override development directory path |
| `GW3_OWNED_URL_PATTERNS` | `bitbucket\.org[:/]Actiontec/` | Regex for GW3-owned source URLs (controls `--deep` filtering) |
| `GW3_VERBOSE` | *(unset)* | Set to `1` to show skipped third-party repos during `--deep` |

## 4. Logic / Workflow

### 4.1 Feed Src-Link Setup (`setup_feed`)

1. Parse `feeds.conf` for the requested feed name
2. Reject if already src-linked (idempotent)
3. Reject commit-pinned feeds (`^sha1`) unless a branch is explicitly provided
4. Clone feed repo into `aei/<feed>/` (or switch branch if already cloned)
5. If the requested branch does not exist remotely, create it locally and push to origin
6. Save original `feeds.conf` line to `.srclocal.state`
7. Replace `src-git` line with `src-link <name> /absolute/path/to/aei/<name>`
8. Run `./scripts/feeds update <name>` and `./scripts/feeds install -a -f -p <name>`

### 4.2 Module Source Checkout (`setup_source`)

1. Locate the package Makefile in `feeds/`, `package/`, or `aei/<feed>/`
2. Verify `PKG_SOURCE_PROTO=git` (reject non-git sources)
3. Extract `PKG_SOURCE_URL` and resolve `$(PKG_NAME)` references
4. Extract `PKG_SOURCE_VERSION` as the base commit for new branch creation
5. Clone to `aei/src/<pkg>/` with requested branch, or create branch from `PKG_SOURCE_VERSION`
6. If no branch specified, create a `dev` branch at `PKG_SOURCE_VERSION`
7. Push newly created local branches to remote (GW3-owned repos only)
8. Record `pkg=url` in `.srclocal-source.state`

### 4.3 Deep Setup (`--deep`)

1. For each specified feed, run `setup_feed` (feed tier)
2. Walk the feed directory, scanning every `Makefile` for `PKG_SOURCE_PROTO=git`
3. For each package with a git source URL:
   - Check URL against `GW3_OWNED_URL_PATTERNS`
   - Skip third-party upstream repos (freedesktop.org, BroadbandForum, etc.)
   - Clone GW3-owned repos via `setup_source`, passing through the feed branch
4. Report count of owned repos cloned and third-party repos skipped

If `--deep` is called with no feed arguments, auto-detect branch-tracked feeds
(lines containing `;branch` in `feeds.conf`).

### 4.4 Interactive Mode (no arguments)

1. Call `do_status` to display current feed and module state
2. Parse all feeds from `feeds.conf` with their status (src-linked / branch / pinned)
3. Present numbered menu for feed selection
4. Prompt for branch name (optional)
5. Prompt for deep mode (`--deep`) yes/no
6. Execute selected feeds with chosen options

### 4.5 Pinning Sources (`--pin-sources`)

For each module checkout in `aei/src/`:

1. Warn if working tree has uncommitted changes
2. Get `HEAD` commit hash
3. Find the package Makefile (prefer `aei/<feed>/<pkg>/`)
4. Replace `PKG_SOURCE_VERSION` with the HEAD hash
5. Set `PKG_MIRROR_HASH` and `PKG_HASH` to `skip` (must be recomputed)

### 4.6 Pinning Feeds (`--pin-feeds`)

For each src-linked feed in `.srclocal.state`:

1. Warn if feed repo has uncommitted changes
2. Get `HEAD` commit hash
3. Find profile YAML(s) in `profiles/` that reference this feed by name
4. Update the `revision:` field using Python YAML line-editing (preserves formatting)

### 4.7 Clean (`--clean`)

1. For each entry in `.srclocal.state`:
   - Restore original `feeds.conf` line (replace `src-link` with saved `src-git`)
   - Reindex the feed (`feeds update` + `feeds install`)
   - If feed repo is clean and fully pushed, remove `aei/<feed>/`; otherwise preserve with warning
2. For each entry in `.srclocal-source.state`:
   - If module repo is clean and fully pushed, remove `aei/src/<pkg>/`; otherwise preserve
3. Remove state files and clean up empty directories

### 4.8 Source Clean (`--source-clean`)

- With package names: remove specific module checkouts (if published) and their state entries
- Without arguments: remove all module checkouts (if published) and delete `.srclocal-source.state`

## 5. Safety Rules

### 5.1 Data Loss Prevention

- **Never delete unpublished repos.** `--clean` and `--source-clean` check `repo_is_published()`
  before removing: working tree must be clean AND all commits must be pushed to upstream.
  Dirty or unpushed repos are preserved with a warning.
- **Push new branches immediately.** When a local-only branch is created (not found on remote),
  the script pushes it to origin to prevent data loss. Third-party repos are skipped.
- **State files track originals.** `.srclocal.state` stores the original `feeds.conf` line
  so `--clean` can always restore it, even if the user has made other changes.

### 5.2 Commit-Pinned Feed Guard

Feeds using `^sha1` (commit-pinned) in `feeds.conf` cannot be src-linked without an explicit
branch specification. This prevents accidentally working on a detached HEAD.

### 5.3 Third-Party Filtering

`--deep` only clones repos matching `GW3_OWNED_URL_PATTERNS` (default: Actiontec Bitbucket).
Third-party upstream repos are never cloned automatically. Use `--source` with explicit URL
to override.

### 5.4 Build System Constraints

- Module source changes require `make package/<pkg>/clean` before recompile (PKG_UNPACK
  copies at prepare time, not incrementally)
- Feed tier changes auto-rebuild via symlink chain (no clean needed)
- Removing `aei/src/<pkg>/` causes automatic fallback to `PKG_SOURCE_URL`

### 5.5 gen_config.py Interaction

Running `gen_config.py` regenerates `feeds.conf`, destroying src-link entries. Always
re-run `src_local.sh` (with no arguments or with saved feed names) after `gen_config.py`
to restore src-links from `.srclocal.state`.

## 6. Dependencies

| Dependency | Type | Purpose |
|------------|------|---------|
| `git` | External tool | Clone, branch, push repos |
| `python3` | External tool | YAML editing for `--pin-feeds` |
| `bash` | External tool | Script runtime |
| `screen` or `find` | External tool | Standard utilities |
| `include/aei-local-development.mk` | Build system | Module tier override mechanism |
| `package.mk` | Build system | Global include of `aei-local-development.mk` |
| `scripts/feeds` | Build system | Feed update and install |
| `scripts/gen_config.py` | Build system | Configuration generation (feeds.conf) |
| `feeds.conf` | Config file | Must exist before running (created by `gen_config.py`) |
| `profiles/*.yml` | Config files | Target of `--pin-feeds` updates |

## 7. State Files

### 7.1 `.srclocal.state`

Feed tier state. Format: one line per feed, `name=original_feeds_conf_line`.

```
feed_prplos=src-git feed_prplos https://bitbucket.org/Actiontec/feed_prplos.git;feature/xyz
feed_aei=src-git feed_aei https://bitbucket.org/Actiontec/feed_aei.git^abc123def
```

### 7.2 `.srclocal-source.state`

Module tier state. Format: one line per package, `pkg=git_url`.

```
mod-amx-thread=https://bitbucket.org/Actiontec/mod-amx-thread.git
mod-amx-wifi=https://bitbucket.org/Actiontec/mod-amx-wifi.git
```

## 8. Feature Lifecycle

### Phase 1: Setup

```bash
git checkout -b feature/xyz
src_local.sh --deep feed_prplos:feature/xyz
```

### Phase 2: Develop

```bash
# Edit feed-level files: aei/feed_prplos/<pkg>/Makefile, ODL, patches
# Edit source code: aei/src/<pkg>/src/main.c
make package/<pkg>/clean && make package/<pkg>/compile V=s
# Deploy to DUT, test, iterate
```

### Phase 3: Finish (pin and merge)

```bash
src_local.sh --pin-sources           # Update PKG_SOURCE_VERSION in feed Makefiles
cd aei/feed_prplos && git commit -a && git push
src_local.sh --pin-feeds             # Update revision: in profile YAMLs
gen_config.py qca_ipq54xx prpl ...   # Regenerate feeds.conf with pinned commits
git commit -a && git push            # Push prplos changes
```

### Phase 4: Cleanup

```bash
src_local.sh --source-clean          # Remove module checkouts
src_local.sh --clean                 # Restore feeds.conf, remove feed checkouts
```

## 9. Future Work

- Automatic detection of `gen_config.py` runs to auto-restore src-links
- Multi-feature workspace support (currently limited to one active feature per workspace)
- Hash recomputation for `PKG_HASH`/`PKG_MIRROR_HASH` after `--pin-sources`
- Integration with CI to validate pinned state before merge
