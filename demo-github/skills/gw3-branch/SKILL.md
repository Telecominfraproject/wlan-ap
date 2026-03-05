---
name: gw3-branch
description: Multi-repo branch management for GW3 — status, pull, commit, push, and branch create/switch across prplos and all feed/module repos
---

# GW3 Branch Management

Coordinated branch operations across the GW3 multi-repo workspace (prplos + feed repos + module repos). Complements `gw3-src-local` by adding branch lifecycle management on top of the feed/module tier local dev setup.

## Quick Start

```bash
# Status report (brief if clean, detailed if issues)
.github/skills/gw3-branch/gw3_branch.sh

# Pull all repos
.github/skills/gw3-branch/gw3_branch.sh pull

# Commit all dirty repos with generated messages
.github/skills/gw3-branch/gw3_branch.sh commit

# Push all repos with unpushed commits
.github/skills/gw3-branch/gw3_branch.sh push

# Sync all feed/module repos to match prplos branch
.github/skills/gw3-branch/gw3_branch.sh sync
```

## Pull Requests

Automate the bottom-up multi-repo PR workflow: modules → feeds → prplos. Each tier is gated on the tier below being fully merged and pinned.

```bash
# Run the PR workflow (rebase, push, create PRs, pin merged repos)
gw3_branch.sh pr

# Preview what would happen without making changes
gw3_branch.sh pr --dry-run

# Override default reviewers for all PRs created in this session
gw3_branch.sh pr --reviewer alice --reviewer bob
```

**Requirements**:
- prplos must be on a `feature/*` branch
- `bkt` CLI must be installed and authenticated (`bkt auth status`)
- `BKT_KEYRING_PASSPHRASE` must be set (WSL)

**Workflow**:
1. **Modules**: For each module under `aei/src/`, rebase onto `origin/main`, push, then detect PR state:
   - **Merged** (0 commits ahead): Pin `PKG_SOURCE_VERSION` in the feed Makefile, auto-commit, clean up `aei/src/<module>/`
   - **PR open**: Report and continue
   - **No PR**: Create PR via `bkt pr create`
2. **Feeds**: Once all modules in a feed are pinned, rebase the feed, push, and detect PR state:
   - **Merged**: Pin feed revision in profile YAML, auto-commit in prplos
   - **PR open/created**: Report and continue
   - **Blocked**: If any module PRs are still pending
3. **prplos**: Once all feeds are pinned, rebase onto `r3_mainline`, push, and create PR with aggregate description

**Idempotent**: Safe to run repeatedly as PRs are reviewed and merged. Each run re-queries Bitbucket for current PR state.

**Feed order**: By default, feeds are processed alphabetically with `feed_prplmesh` second-to-last and `feed_aei` last. Override with `aei/.pr-config.yaml`:

```yaml
feed_order:
  - feed_prplmesh
  - feed_aei
```

Feeds listed in `feed_order` are processed after unlisted feeds (which go alphabetically). Only localized (branch-tracking) feeds are processed.

## Sync

Switch all feed and module repos in `aei/` to match the current prplos branch. Only works on feature/hotfix branches.

```bash
gw3_branch.sh sync
```

**Behavior**:
- Skips non-GW3-owned repos (third-party feeds)
- Skips repos already on the correct branch
- Skips dirty repos (warns instead — never discards uncommitted work)
- For each remaining repo: fetches from origin, then checks out the target branch
- Prints summary of switched and skipped repos

**When to use**: After switching prplos to a different feature branch, or when `gw3_branch.sh` status shows branch mismatch warnings.

## Branch Operations

### Create/Switch Feature Branch

```bash
# Create feature branch (interactive feed selection menu)
gw3_branch.sh feature/my-feature

# Create with specific feeds (deep by default — clones all module repos)
gw3_branch.sh feature/my-feature feed_aei feed_prplos

# Shallow: feed src-link only, no module clones (feed tier only)
gw3_branch.sh feature/my-feature --shallow feed_aei feed_prplos

# Add specific module repos to existing feature
gw3_branch.sh feature/my-feature --source mod-amx-thread

# Extend existing feature (additive — adds feeds without disturbing existing checkouts)
gw3_branch.sh feature/my-feature feed_prplmesh
```

Feature branch creation:
1. Creates `feature/<name>` in prplos
2. Updates profile YAMLs to branch-track selected feeds
3. Calls `src_local.sh --deep` to clone feed repos + all module repos (feed + module tiers)
4. Use `--shallow` to skip module cloning (feed tier only)

### Create/Switch Hotfix Branch

```bash
# Must specify --from release branch when creating
gw3_branch.sh hotfix/fix-crash --from release/1.0 feed_aei

# Switch to existing hotfix
gw3_branch.sh hotfix/fix-crash
```

Hotfix branches are created from a release branch. Feed branches are created from the commit pinned in the release's profile YAMLs.

### Create/Switch Release Branch

```bash
gw3_branch.sh release/1.0
```

Requires all feed revisions and PKG_SOURCE_VERSION values to be commit-pinned. Only operates on prplos (no feed operations).

### Switch to Mainline

```bash
gw3_branch.sh main
```

Cleans all src-links, switches prplos to `r3_mainline`, and updates feeds.

## Stale Repo Handling

When switching to a different feature or hotfix branch, feeds and module repos left over from a previous feature may still be in `aei/` on their old branch. The script detects these stale repos and prompts you to add or remove each one.

### Prompt Options

| Key | Action |
|-----|--------|
| `y` | Add this repo to the current feature branch |
| `N` | Remove this repo from `aei/` (default) |
| `a` | Add all remaining stale repos |
| `r` | Remove all remaining stale repos |

### Deep vs Shallow

- **Deep mode (default)**: When a stale feed is added, all its module repos are also cloned/switched to the target branch via `src_local.sh --deep`.
- **Shallow mode (`--shallow`)**: Only the feed itself is switched; module repos are prompted individually.

### Removal Behavior

When a stale repo is removed:
- **Feed**: Its module repos are also removed, the `feeds.conf` entry is restored from `.srclocal.state`, and the feed is reindexed.
- **Module repo**: Removed from `aei/src/` and cleaned from `.srclocal-source.state`.
- **Safety**: Repos with uncommitted changes or unpushed commits trigger a warning before removal.

### Example

```bash
# Set up feature A with two feeds
gw3_branch.sh feature/feat-a feed_aei feed_prplos

# Switch to feature B with only one feed — prompts about stale feed_prplos
gw3_branch.sh feature/feat-b feed_aei
# => feed_prplos is on 'feature/feat-a', not 'feature/feat-b'. Add to feature? [y/N/a/r]
```

## Status Report

The status report adapts its output based on workspace state:

**Brief mode** (everything clean and consistent):
```
prplos @ feature/xyz | 3 feed(s), 12 source(s) | all clean
```

**Detailed mode** (when issues exist):
```
prplos @ feature/xyz (feature branch)

Feeds: feed_aei(ok) feed_prplos(dirty) feed_prplmesh(ok)
Sources: 12 repo(s), 1 dirty

Warnings:
  feed_prplos: 3 uncommitted change(s)
  aei/src/mod-amx-thread: 1 uncommitted change(s)
```

Use `--verbose` to force detailed output regardless of state.

## Commit Workflow

### On feature/hotfix branches
Iterates all dirty repos (modules, feeds, prplos) and commits each with an auto-generated message based on `git diff --stat`.

### On main/release branches
Enforces **both tiers** of pinning before allowing commit:
1. Feed revisions in profile YAMLs must be commit-pinned (40-char SHA)
2. `PKG_SOURCE_VERSION` in package Makefiles must be commit hashes

If unpinned, prints error and suggests running `src_local.sh --pin`.

## Relationship to Other Skills

| Skill | Responsibility |
|-------|---------------|
| `gw3-src-local` | Feed tier (feed src-link) + module tier (module checkout) + pinning |
| `gw3-branch` | Branch lifecycle: status, pull, commit, push, create/switch |
| `gw3-build` | Build system: gen_config.py, make, packages |

Typical workflow: `gw3-branch` creates branches and sets up workspace → `gw3-src-local` for additional fine-grained source management → `gw3-branch` for daily commit/push/pull → `gw3-src-local --pin` before merge.

## Environment Variables

| Variable | Default | Description |
|----------|---------|-------------|
| `GW3_AEI_DIR` | `<project_root>/aei` | Override aei/ directory location |
