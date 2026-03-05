---
name: gw3-src-local
description: Three-tier local development — feed (recipe tier) + module (source tier) with lifecycle pinning
---

# gw3-src-local

Three-tier local development for AI-assisted cross-module editing:

- **Project tier** — The prplos repo itself: profiles, build system, specs. Always present.
- **Feed tier** — Feed src-link: clone feed repos into `aei/<feed>/` and configure `src-link` entries in `feeds.conf` so the build system reads Makefiles, ODL, patches directly from local git repos. Enables autorebuild (no QUILT).
- **Module tier** — Module checkout: clone package source repos into `aei/src/<pkg>/` so the AI can edit actual C/C++ source code. The build system (`aei-local-development.mk`, included globally from `package.mk`) auto-detects these and uses them instead of downloading from `PKG_SOURCE_URL`.

## Quick Start

```bash
# Interactive: show status + feed selection menu
.github/skills/gw3-src-local/src_local.sh

# Feed tier: Specific feeds with explicit branches
.github/skills/gw3-src-local/src_local.sh feed_prplos:feature/xyz feed_aei:feature/xyz

# Deep setup — src-link feed, then auto-clone all module source repos within it
.github/skills/gw3-src-local/src_local.sh --deep feed_prplos:feature/xyz

# Module tier: Clone specific module source repos
.github/skills/gw3-src-local/src_local.sh --source mod-amx-thread:feature/xyz

# Check current state (all tiers)
.github/skills/gw3-src-local/src_local.sh --status

# Pin commits for merge (end of feature)
.github/skills/gw3-src-local/src_local.sh --pin
```

## Usage

```
Feed tier - Feed src-link:
  src_local.sh [feed[:branch] ...]           Setup feed src-links
  src_local.sh --deep [feed[:branch] ...]    Src-link feed, then auto-clone all module source repos
  src_local.sh                               Interactive: show status + feed selection menu
  src_local.sh --clean                       Restore all src-links to original
  src_local.sh --status                      Show all state (feeds + modules)

Module tier - Module checkout:
  src_local.sh --source <pkg[:branch]> [...]  Clone specific module source repos
  src_local.sh --source <pkg> --url <url>     Clone with explicit git URL
  src_local.sh --source-clean [<pkg> ...]     Remove module state

Lifecycle - Pinning:
  src_local.sh --pin-sources                  Pin source commits into feed Makefiles
  src_local.sh --pin-feeds                    Pin feed commits into profile YAMLs
  src_local.sh --pin                          Both: pin sources then feeds
```

**Environment variables:**
- `GW3_AEI_DIR` — override development directory (default: `<project_root>/aei`)

## How It Works

### Feed Tier: Feed Src-Link

1. Parses `feeds.conf` for the requested feed entry
2. Clones the feed repo into `aei/<feed_name>/` (or checks out the requested branch)
3. Saves the original `src-git` line to `aei/.srclocal.state`
4. Replaces the `src-git` line with `src-link <name> /absolute/path/to/aei/<name>`
5. Runs `./scripts/feeds update <name>` + `install` to create symlinks

### Module Tier: Module Checkout

1. Finds the package Makefile in `feeds/` or `aei/<feed>/`
2. Extracts `PKG_SOURCE_URL` and `PKG_SOURCE_VERSION`
3. Clones to `aei/src/<pkg>/`, checks out requested branch (or creates `dev` branch at `PKG_SOURCE_VERSION`)
4. `include/aei-local-development.mk` (globally included from `package.mk`) auto-detects `aei/src/<pkg>/` and overrides `PKG_UNPACK` to copy from local source

### --deep: Feed Src-Link with Automatic Module Checkout

When `--deep` is specified, the script first sets up the feed src-link (feed tier), then walks into the feed and scans every package Makefile for `PKG_SOURCE_URL`. Each package with a source URL gets its repo cloned into `aei/src/<pkg>/`. The branch from the feed spec is passed through to all module checkouts.

### Pinning

**`--pin-sources`**: For each module checkout in `aei/src/<pkg>/`, updates `PKG_SOURCE_VERSION` in the feed Makefile to the current HEAD commit.

**`--pin-feeds`**: For each src-linked feed in `aei/<feed>/`, updates the `revision:` field in the profile YAML to the current HEAD commit hash.

**`--pin`**: Runs both in sequence.

## After gen_config.py

`gen_config.py` regenerates `feeds.conf`, removing src-link entries. Re-run to restore:

```bash
./scripts/gen_config.py qca_ipq54xx prpl aei_generic security f-secure
.github/skills/gw3-src-local/src_local.sh   # restores src-links
```

## Directory Layout

```
aei/                             # local development directory (gitignored)
├── .srclocal.state              # feed src-link state
├── .srclocal-source.state       # module checkout state
├── feed_prplos/                 # Feed tier: feed checkout (feature/xyz branch)
│   ├── mod-amx-thread/          # Makefile, files/, patches/
│   └── mod-amx-wifi/
├── feed_aei/                    # Feed tier: feed checkout
│   └── aei-network/
└── src/                         # Module tier: module checkouts
    ├── mod-amx-thread/          # actual C/C++ source (feature/xyz branch)
    │   ├── src/
    │   ├── include/
    │   ├── CMakeLists.txt
    │   └── .git/
    └── mod-amx-wifi/
        ├── src/
        └── .git/
```

## Feature Lifecycle

```bash
# Phase 1: SETUP
git checkout -b feature/xyz
src_local.sh --deep feed_prplos:feature/xyz   # Feed src-link + all package sources

# Phase 2: DEVELOP
# Edit aei/feed_prplos/<pkg>/Makefile          ← feed-level (ODL, config)
# Edit aei/src/<pkg>/src/main.c                ← actual source code
make package/<pkg>/clean && make package/<pkg>/compile V=s
# Deploy → test → iterate

# Phase 3: FINISH (pin & merge)
src_local.sh --pin-sources                    # PKG_SOURCE_VERSION update
cd aei/feed_prplos && git commit && git push  # push feed changes
src_local.sh --pin-feeds                      # profile YAML update
gen_config.py ...                             # regenerate feeds.conf
git commit && git push                        # push prplos changes

# Phase 4: CLEANUP
src_local.sh --source-clean                   # remove source state
src_local.sh --clean                          # restore feeds.conf
```

## Hotfix Lifecycle

Hotfixes target a release branch instead of mainline. The `src_local.sh` workflow is identical — only the branch origin and merge targets differ.

```bash
# Phase 1: SETUP (branch from release, not mainline)
git checkout release/v3.2
git checkout -b hotfix/dhcp-crash
src_local.sh --deep feed_aei:hotfix/dhcp-crash

# Phase 2: DEVELOP (same as features)
# Edit → build → deploy → test → iterate

# Phase 3: FINISH — dual merge (see SDD-09 §3.6)
src_local.sh --pin                            # pin sources + feeds
# Merge to release/v3.2 AND r3_mainline
```

## Build Behavior

- **Module changes require `make clean`**: `PKG_UNPACK` copies at prepare time. Module source changes need `make package/<pkg>/clean && make package/<pkg>/compile V=s`
- **Feed tier changes auto-rebuild**: Changes in `aei/<feed>/<pkg>/` are detected because `${CURDIR}` resolves through the src-link symlink chain
- **Removing `aei/src/<pkg>/`**: Build falls back to `PKG_SOURCE_URL` automatically
