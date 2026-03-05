# Gateway 3 - GitHub Copilot Instructions

> **Dual AI Assistant Setup**: Developers use both Claude Code (`CLAUDE.md`) and GitHub Copilot (`.github/copilot-instructions.md`). When adding, deleting, or changing important project-specific instructions, **always update both files** to keep them in sync.

## Project Overview

**Gateway 3 (GW3)** is a custom embedded networking gateway platform based on **prplOS**, extending **OpenWrt** for CPE devices (routers, gateways, WiFi extenders). Microservice architecture: 100+ TR-181 services via UBUS IPC, managed by procd.

**Key Technologies:** prplMesh (EasyMesh R5/R6 WiFi mesh) · LXC via LCM (container runtime) · Qualcomm QCA SDK (IPQ54xx/IPQ95xx) · Ambiorix/AMX (TR-181 data model framework) · TR-181 (Broadband Forum device data models)

## Architecture

### Key Directories
- `package/` — OpenWrt package definitions (base-files, boot, devel, firmware, kernel, libs, network, system, utils)
- `feeds/` — Cloned package feed repos (created by `scripts/feeds update`)
- `build_dir/` — Extracted/compiled package sources (**read-only — NEVER modify**)
- `staging_dir/` — Cross-compilation sysroot (`host/` for host tools, `target-*/` for target)
- `target/linux/` — 46 architecture targets with kernel configs and patches
- `include/` — Build system macros (package.mk, image.mk, kernel.mk, etc.)
- `specs/` — Feature specifications and design docs
- `refs/` — External reference materials (**content gitignored**), populated by `./scripts/refs-setup.sh`
- `local/` — Dev files, logs, temp files (**gitignored — never commit**)
- `aei/` — Local dev: feed checkouts (`aei/<feed>/`) + module checkouts (`aei/src/<pkg>/`) (**gitignored — never commit**)
- `.github/skills/` — Reusable automation scripts

### Build System Layers
```
profiles/*.yml → gen_config.py → .config + feeds.conf
  → tools/ (75+ host tools) → toolchain/ (cross-compiler)
    → target/linux/ (kernel + patches) → package/ (543 Makefiles, IPK)
      → bin/targets/ (firmware images)
```

### Profile System
YAML files in `profiles/` that layer together:
1. **Target** — hardware: `qca_ipq54xx.yml`, `qca_ipq95xx.yml`, `x86_64.yml`, `mvebu.yml`
2. **Feature** — capabilities: `prpl.yml`, `security.yml`, `f-secure.yml`, `lcm.yml`, `cellular.yml`, `thread.yml`, `debug.yml`
3. **Customer** — customizations: `aei_generic.yml`, `aei-dvt-mfg.yml`

Each profile can specify: `target`, `subtarget`, `feeds`, `packages`, `diffconfig`, `include`, `packages_remove`.

### Package Feeds (private repositories)

| Feed | Purpose |
|------|---------|
| `feed_amx` | Ambiorix framework (17 core packages) |
| `feed_prplos` | TR-181 plugins + network model (93 packages) |
| `feed_prplmesh` | WiFi mesh (7 packages) |
| `feed_lcm` | Container lifecycle (15 packages) |
| `feed_aei` | Vendor customizations (14 packages) |
| `feed_qca` | Qualcomm SDK |

Base feeds (from `feeds.conf.default`): packages, luci, routing, telephony (Actiontec forks).

### Package Conventions
- `PKG_NAME`, `PKG_VERSION`, `PKG_RELEASE` in Makefiles
- ODL (Object Definition Language) for TR-181 data models
- UBUS for IPC, procd for init/service management
- Network managers use libnetmodel and AMX framework

### Source Code Access Rules
**Critical** — follow this priority when reading/modifying package source:

1. **`aei/src/<pkg>/`** (local checkout) — **Always prefer.** Live git repos on feature branches, set up by `src_local.sh --source` or `--deep`. Changes committed/pushed to the package's own repo.
2. **`aei/<feed>/<pkg>/`** (feed checkout) — Feed-level files: Makefiles, ODL, patches, configs. Also live git repos.
3. **`build_dir/target-*/<pkg>-<version>/`** (build snapshot) — **Read-only reference.** Never modify — ephemeral, overwritten on `make package/<pkg>/clean`.
4. **`feeds/<feed>/<pkg>/`** (feed symlinks) — When src-linked, point into `aei/<feed>/`. Prefer reading from `aei/<feed>/` directly.

**Summary**: `aei/src/` = editable source, `build_dir/` = read-only reference, never edit `build_dir/` or `feeds/`.

## Build

All commands run from project root. Always use verbose mode (`V=s`).

```bash
# Configure (merges YAML profiles into .config)
./scripts/gen_config.py <target_profile> [feature_profiles...]
./scripts/gen_config.py qca_ipq54xx prpl aei_generic security f-secure  # Production
./scripts/gen_config.py list                                        # List profiles
./scripts/gen_config.py clean                                       # Clean feeds + config

# Feed management (gen_config.py handles automatically)
./scripts/feeds update -a && ./scripts/feeds install -a

# Build
make -j$(nproc) V=s 2>&1 | tee local/records/build.log             # Full build
make package/<name>/compile V=s                                      # Single package
make package/<name>/clean && make package/<name>/compile V=s         # Rebuild package
make target/linux/compile V=s                                        # Kernel

# Clean (increasing aggressiveness)
make clean          # Build artifacts
make targetclean    # + toolchain
make dirclean       # + host tools, tmp

# Configuration
make menuconfig     # Interactive
make defconfig      # Defaults
```

**Output:** `bin/targets/<target>/<subtarget>/` (e.g., `bin/targets/ipq54xx/generic/openwrt-ipq54xx-generic-prpl_freedom-squashfs-nand-sysupgrade.bin`)

**Prerequisites:** `build-essential gcc g++ binutils patch bzip2 flex make gettext unzip libc6-dev libncurses5-dev libz-dev libssl-dev subversion git wget rsync python3 python3-distutils python3-minimal coccinelle gawk file`

## Development Workflow

### Multi-Repo Code Management
Full details: [`specs/baseline/09-development-workflow-code-management.md`](specs/baseline/09-development-workflow-code-management.md) (SDD-09).

- **Branch naming**: `feature/<name>` (or `hotfix/<name>`) consistently across prplos and all affected feed repos
- **Branch types**: Main (`r3_mainline`/`main`); `feature/<name>` for new work; `release/<version>` in prplos only (feeds commit-pinned); `hotfix/<name>` for critical fixes (dual-merge to release + mainline)
- **Single active feature**: `src-link` replaces `feeds.conf` entries globally — only one feature's branches src-linked at a time
- **Feed revisions**: Profile YAMLs use `^sha1` (commit pin) for mainline/release, `;branch` (branch tracking) for active dev
- **Merge rule**: Never merge branch-tracking revisions to `r3_mainline` — always pin to commit hashes first
- **Merge flow**: Feed repo PRs merge first → update prplos profile with pinned commits → prplos PR to `r3_mainline`

### Local Development
Three tiers via `gw3-src-local` skill (`.github/skills/gw3-src-local/src_local.sh`):
- **Feed tier**: Clone feed repos into `aei/<feed>/`, set up `src-link` in `feeds.conf`. Enables autorebuild (no QUILT)
- **Module tier**: Clone package source repos into `aei/src/<pkg>/`. `include/aei-local-development.mk` auto-detects local source
- **Deep setup**: `src_local.sh --deep feed:branch` — src-links feed + auto-clones all package source repos
- **Pinning**: `src_local.sh --pin` — pins source and feed commits for merge readiness
- Re-run `src_local.sh` after `gen_config.py` to restore src-links

### HOST vs DUT
- **HOST** (development PC/WSL): direct terminal commands, builds, file editing
- **DUT** (Gateway device): `ssh root@192.168.1.1 'command'`

### Device Testing
```bash
# Unit test setup
.github/skills/gw3-unit-test-setup/generate-ut-setup.sh && source local/ut-setup.sh

# Deploy firmware
scp bin/targets/.../openwrt-*-sysupgrade.bin root@192.168.1.1:/tmp/
ssh root@192.168.1.1 'sysupgrade /tmp/openwrt-*-sysupgrade.bin'

# Deploy single package
scp bin/packages/.../package.ipk root@192.168.1.1:/tmp/
ssh root@192.168.1.1 'opkg install --force-reinstall /tmp/package.ipk'

# Serial console (UART 115200 8N1)
.github/skills/gw3-console/auto_console.sh
.github/skills/gw3-console/console_exec.sh 'uname -a'
```

### Logging
Build: `local/records/build_*.log` | Console: `local/records/console_*.log` | Prompts: `local/records/prompt_log.md`. The `local/` directory is gitignored — never commit.

## Reference Materials

External reference materials and knowledge bases in `refs/`. Populated by `./scripts/refs-setup.sh` from `refs/refs.yaml`. KB repos with `kb.yaml` have skills auto-installed by `scripts/refs-activate.sh`.

| Source | Description |
|--------|-------------|
| `refs/optim5_doc/` | Optim5 Cloud Specification |
| `refs/optimeventproto/` | OptimEvent protobuf definitions |
| `refs/gw3-reference-docs/` | Wi-Fi Alliance, 802.11, Qualcomm SDK, etc. (KB) |

KB skills listed in `.refs-skills.yaml`, copied into `.github/skills/`. Management: `/kb-add-file <kb-name> /path/to/file.pdf`, `/kb-add-batch <kb-name> /path/to/directory` (omit name if one KB mounted).

Check `refs/refs.yaml` for available materials. Changes to reference materials must be committed in their own repo (inside `refs/<name>/`), never in GW3.

### Research Workflow
Search order for technical questions:
1. **Feature specs** — `specs/<xxx>-features/research-*.md` for existing analysis
2. **Baseline SDDs** — `specs/baseline/` for architectural context
3. **KB catalog** — `refs/gw3-reference-docs/md/catalog.yaml` for candidate docs
4. **KB index** — `grep -i "term" refs/gw3-reference-docs/md/**/*.idx` (format: `TYPE:TERM:LINE:CONTEXT`)
5. **KB document** — Read `.md` at line indicated by index hit
6. **Protobuf/API** — `refs/optimeventproto/` for event schemas, 802.11 codes, telemetry
7. **Source code** — `aei/src/<pkg>/` and `aei/<feed>/` (follow Source Code Access Rules)

## Coding Guidelines
- Follow OpenWrt package structure conventions
- Use TR-181 data model naming for device management features
- All responses, logs, and generated files must be in English (regardless of user prompt language)
- Shell scripts: use efficient bash patterns (`||`/`&&` flow control, `[[ ]]` for regex, brace expansion, parameter expansion)
- WSL: build on WSL filesystem, never on Windows mounts (`/mnt/c/`). Build system filters Windows paths from PATH automatically

## SDD-Driven Development

### Spec Structure
- **`specs/baseline/`** — Baseline SDDs: system architecture, all functional areas (build, AMX, networking, WiFi/mesh, containers, device management, platform/security)
- **`specs/<xxx>-features/`** — Feature specs with deeper detail. Complex features use [spec-kit](https://github.com/github/spec-kit); simpler ones use a general SDD file. Primary file: `specs/<xxx>-features/spec.md`

### Workflow
1. **Create feature branches** in prplos AND all affected feed repos (`feature/<name>` or `hotfix/<name>`). Never develop directly on `r3_mainline`
2. **Reference baseline** — Read relevant SDD(s) in `specs/baseline/`
3. **Check feature specs** — Look in `specs/<xxx>-features/` for related specs
4. **Create spec if needed** — Spec-kit for complex features, general SDD file for simple ones
5. **Implement from spec** — Code from specification, not ad-hoc
6. **Keep specs aligned** — Update specs in prplos when modifying code in feed repos

### Sync Rules
- **New features**: Create spec before writing code
- **Modifications**: Update spec first, then implement
- **Implementation-discovered changes**: Update spec to match before merging
- **Never diverge**: Undocumented code changes cause confusion for future developers and AI
- **Multi-repo alignment**: At merge time, all specs must reflect final implementation

## Skills Development

Skills are reusable automation scripts in `.github/skills/`, each a self-contained directory with `SKILL.md` (documentation) and shell scripts (implementation).

### Guidelines
- **Single responsibility**: Each script does one thing well
- **Minimal output**: Only essential information
- **Keep it simple**: Minimize complexity, maximize readability
- **Error handling**: Use `|| { error; exit; }` patterns
- **No unnecessary features**: Remove auto-documentation, complex state tracking unless truly needed
- **Test all changes**: Verify functionality after modifications
- **Bash idioms**: Prefer `||`/`&&` flow control, `[[ ]]` for regex/glob, brace expansion, parameter expansion over verbose if/else

## Integrations

### Confluence
- **Space:** R3G | **Parent:** [R3 Gateway Specifications](https://actiontec.atlassian.net/wiki/spaces/R3G/pages/4178870422/R3+Gateway+Specifications)
- All generated spec pages go **under this parent**, organized by topic
- **AI-generated notice**: Every AI page includes info panel with: tool used, prompt/task, source file path, "auto-generated — do not edit manually" warning
- **Page mapping**: `confluence-pages.yaml` maps local files → Confluence pages. Keep in sync when creating/updating pages
- **TOC**: Use `<!-- confluence-toc -->` marker (not `#anchor` links). Upload script replaces with `{toc}` macro (maxLevel=3)
- **Diagrams**: Convert ASCII diagrams to Mermaid → PNG on a temp copy (never modify original). Render with `mmdc --scale 3`, upload with `--image-scale 3`
- **Images**: Never enlarged beyond natural size. With `--image-scale N`, display width = actual_pixels / N, capped at `--image-width` (default 900)
- **Tables**: Must have blank line before first `|` row (upload script auto-inserts via `preprocess_markdown_tables()`)
- **Credentials**: Read from `~/.claude/.mcp.json` (`mcpServers.atlassian.env`). Map `CONFLUENCE_EMAIL` → `CONFLUENCE_USERNAME` for upload script:
  ```bash
  CONFLUENCE_URL="..." CONFLUENCE_USERNAME="..." CONFLUENCE_API_TOKEN="..." \
    python3 ~/.claude/skills/confluence/scripts/upload_confluence_v2.py document.md --id PAGE_ID
  ```

### Jira
- **Instance:** https://actiontec.atlassian.net | **Project:** `S12W01` | **Board:** 301
- **Credentials:** `~/.env.jira` (`JIRA_URL`, `JIRA_USERNAME`, `JIRA_API_TOKEN`). Jira/Confluence share one Atlassian API token; Bitbucket requires a separate scoped token
- **Skill:** `.github/skills/jira-communication/` — CLI scripts via `uv run` for search, create, update, transitions, sprints, comments, worklogs

## Copilot-Specific Rules
- **Build execution**: Always run make in foreground (never use `isBackground=true`) to monitor build progress
- **DUT automation**: Use `console_exec.sh` or `console_capture.sh` for automated DUT commands (never interactive screen)
