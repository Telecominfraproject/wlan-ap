<!--
Sync Impact Report — constitution v1.0.0 → v1.1.0
===================================================
Change type : MINOR (new rule added to existing principle)
Modified principles:
  - Principle II (Spec-First Development): added ODL interface contract requirement
Added sections : none
Removed sections : none
Templates checked:
  - plan-template.md   : ✅ Already has contracts/ in project structure (line 46); no update needed
  - spec-template.md   : ✅ FR-xxx format compatible; no template change needed (contract creation is feature-specific)
  - tasks-template.md  : ✅ Already references contracts/ as prerequisite input (line 9, 37); no update needed
  - CLAUDE.md          : ⚠ Consider adding ODL contract rule to SDD-Driven Development section (low priority — constitution is authoritative)
  - copilot-instructions.md : ⚠ Same as CLAUDE.md
Deferred TODOs : none
-->

# GW3 Project Constitution

This constitution codifies the non-negotiable development rules and conventions for the Gateway 3 (GW3) platform. It is the single authoritative source of project-level invariants. All developers, AI assistants, reviewers, and CI pipelines must comply.

---

## Core Principles

### I. Feature-Branch-First Workflow

Every change — feature, hotfix, or experiment — begins with a coordinated `feature/<name>` (or `hotfix/<name>`) branch across prplos and all affected feed and source repos. The `gw3-branch` skill (`gw3_branch.sh`) is the standard entry point for creating and managing these branches.

**Rules:**
- Never commit directly to `r3_mainline` or `main` in any GW3-owned repository
- Use the **same branch name** across prplos and every affected feed/source repo for cross-repo traceability
- Branches are **published to remote on creation** — local-only branches are a data-loss risk since `aei/` is gitignored
- Only **one active feature per workspace** — `src-link` replaces `feeds.conf` entries globally; run `src_local.sh --clean` before switching features
- Third-party upstream repos (packages, luci, routing, telephony, feed_qca) are **snapshot-only** — never create GW3 branches in them
- Profile YAMLs on mainline use **commit pins** (`^sha1`); branch tracking (`;branch`) is only permitted on feature/hotfix branches

### II. Spec-First Development (NON-NEGOTIABLE)

No implementation work begins without a specification. The SDD spec is the source of truth for every feature, and code must never diverge from it.

**Rules:**
- For every new feature, create `specs/<feature>-features/spec.md` on the prplos feature branch **before** writing code in feed repos
- Read the relevant baseline SDDs in `specs/baseline/` before starting any implementation
- Specs must include: exact TR-181 data model paths, ODL definitions, UBUS interface contracts, UCI configuration schema, inter-module interaction sequences, edge cases, and error handling
- When a feature requires **new or modified Ambiorix signals, data model parameters, or ODL definitions** from other microservices, each change MUST be formally specified as an **interface contract** in `specs/<feature>/contracts/`. Each contract file documents: object path, signal/parameter name, typed parameter list, event semantics, and consuming events. Contracts are the authoritative reference for cross-team extension requests — implementation teams use the contract as their acceptance criteria
- When implementation reveals the spec is incomplete or incorrect, **update the spec first** in the prplos branch, then continue coding
- At merge time, all specs must reflect the final implementation — no undocumented behavior
- The prplos feature branch is **always required**, even if no code in prplos itself changes, because it carries the spec and profile updates

### III. Source Access Hierarchy (NON-NEGOTIABLE)

Source code access follows a strict four-level priority. Violating this hierarchy causes silent data loss (edits overwritten by the build system) or stale reads.

**Priority order:**

| Priority | Location | Read | Modify | Commit |
|----------|----------|------|--------|--------|
| 1 | `aei/src/<pkg>/` | Yes | **Yes** | Yes — push to package source repo |
| 2 | `aei/<feed>/<pkg>/` | Yes | **Yes** | Yes — push to feed repo |
| 3 | `build_dir/target-*/<pkg>-<version>/` | Yes | **NEVER** | N/A — ephemeral, overwritten on `make clean` |
| 4 | `feeds/<feed>/<pkg>/` | Avoid | **NEVER** | N/A — symlinks, prefer `aei/<feed>/` directly |

**Rules:**
- If `aei/src/<pkg>/` exists, **all** source reads and edits MUST use that directory
- Never modify files under `build_dir/` — they are ephemeral snapshots recreated from `PKG_SOURCE_URL` or `aei/src/` via `PKG_UNPACK`
- To make a package's source editable, use `src_local.sh --source <pkg>` to clone it into `aei/src/`
- Feed tier (feed src-link) changes autorebuild; module tier (module checkout) changes require `make package/<pkg>/clean` first

### IV. Data Model as Authoritative Record

The TR-181 data model and its ODL implementation are the authoritative record of what the gateway exposes. The `gw3-datamodel-report` skill generates coverage reports that are the definitive source for data model status.

**Mark semantics:**

| Mark | Meaning |
|------|---------|
| IMPLEMENTED | ODL entry with action callbacks or confirmed C source writes |
| NOT IMPLEMENTED | ODL entry exists but has no action callbacks and no confirmed C writes |
| DEFAULT ONLY | ReadOnly param in ODL without callbacks; AMX serves default/init value only |
| REPORT ONLY | ReadWrite param with confirmed C writes but no action callbacks |
| NOT DEFINED | In TR-181 spec but no ODL definition exists |
| MISMATCH | ODL entry exists but type or access differs from TR-181 spec |
| VENDOR EXTENSION | ODL entry with `X_` prefix, not in TR-181 standard |
| PRPL EXTENSION | ODL entry without `X_` prefix, not in TR-181 standard |

**Rules:**
- When a feature touches the data model, its spec (Principle II) must document the affected TR-181 paths and expected mark transitions
- Data model reports regenerated by the skill are the single source of truth for implementation status — not inline comments, not meeting notes
- MISMATCH marks are defects and must be resolved before merging to mainline
- NOT DEFINED items in a feature's scope must be explicitly addressed in the spec (implement, defer with justification, or mark out-of-scope)

### V. Reference Materials Immutability

Reference materials under `refs/` are external resources managed by `refs/refs.yaml`. They are read-only within the GW3 workspace.

**Rules:**
- Never modify files under `refs/` from the prplos repo — changes to reference materials must be committed **in their own repository** (inside `refs/<name>/`)
- `refs/refs.yaml` is the manifest: it declares sources, types, revision modes (branch tracking vs SHA pin), and sparse checkout paths
- Populate references with `./scripts/refs-setup.sh`; knowledge base repos with `kb.yaml` have their skills auto-installed by `scripts/refs-activate.sh`
- Reference materials are gitignored in prplos — only `refs/refs.yaml` is committed
- When working on a feature, check `refs/refs.yaml` for available reference materials and read docs from `refs/<name>/` for context

**Current reference catalog:**

| Source | Type | Description |
|--------|------|-------------|
| `refs/optimeventproto/` | git (branch) | OptimEvent protobuf definitions and codegen |
| `refs/optim-provision-model/` | git (branch) | Optim Provision Schema |
| `refs/optim5_doc/` | git (branch, sparse) | Optim5 documentation (specification/ + README) |
| `refs/gw3-reference-docs/` | git (branch, sparse) | BBF specs, vendor docs, external guides (KB) |

### VI. Build System Discipline

The OpenWrt/prplOS build system has strict conventions. Violating them causes non-reproducible builds, wasted CI time, or silent miscompilation.

**Rules:**
- Always use verbose mode (`V=s`) for builds
- Run `./scripts/gen_config.py` to configure; never hand-edit `.config` or `feeds.conf` for persistent changes
- After `gen_config.py`, re-run `src_local.sh` to restore any active src-links (gen_config.py regenerates `feeds.conf`, destroying src-link entries)
- Build on the WSL/Linux filesystem, never on Windows mounts (`/mnt/c/`) — the build system filters Windows paths from PATH automatically
- Use `make package/<pkg>/compile V=s` for single packages; use `make package/<pkg>/clean && make package/<pkg>/compile V=s` when module tier source has changed
- Profile YAMLs in `profiles/` are the canonical build configuration — they layer: target (hardware) + feature (capabilities) + customer (customization)
- Build output goes to `bin/targets/<target>/<subtarget>/`; logs go to `local/records/` (gitignored)

### VII. Dual AI Assistant Coherence

Developers use both Claude Code (`CLAUDE.md`) and GitHub Copilot (`.github/copilot-instructions.md`). These files must stay synchronized.

**Rules:**
- When adding, deleting, or changing project-specific instructions, **always update both files**
- The constitution (this document) is the canonical source of project-level invariants; CLAUDE.md and copilot-instructions.md encode operational guidance that must not contradict the constitution
- AI-generated code must follow OpenWrt package structure conventions, TR-181 data model naming, and Ambiorix (AMX) framework patterns
- All AI output (responses, logs, generated files) must be in English regardless of user prompt language

### VIII. HOST-DUT Context Awareness

Commands execute in two distinct contexts. Confusing them causes at best wasted time, at worst bricked devices.

**Rules:**
- **HOST** (development PC / WSL): direct terminal commands, builds, file editing, git operations
- **DUT** (Gateway device at `192.168.1.1`): accessed via `ssh root@192.168.1.1 'command'` or serial console (`gw3-console` skill)
- Always prefix DUT commands with explicit `ssh` or use the console skill — never assume the terminal is connected to the device
- Deploy packages with `scp` + `opkg install --force-reinstall`; deploy firmware with `scp` + `sysupgrade`
- Serial console access: `.github/skills/gw3-console/auto_console.sh` (UART 115200 8N1)
- When debugging multi-module UBUS interactions on DUT, use `ubus monitor`, `logread -f -e <service>`, and AMX data model queries

### IX. Commit Hygiene and Multi-Repo Atomicity

In a multi-repo project, commits must be coordinated across repositories. Sloppy commit practices break the connection between specification, code, and build configuration.

**Rules:**
- Commit in **inner-to-outer order**: source repos (`aei/src/<pkg>/`) first, then feed repos (`aei/<feed>/`), then prplos (specs + profiles)
- Before merging to mainline, **pin all revisions**: run `src_local.sh --pin-sources` (updates `PKG_SOURCE_VERSION` in feed Makefiles), then `src_local.sh --pin-feeds` (updates profile YAML revisions to SHA-1)
- Never merge branch-tracking revisions (`;branch`) to `r3_mainline` — mainline must always have all feed revisions commit-pinned
- Feed repos use **squash-merge** for `feature/*` to `main` (clean per-feed history, single commit to pin)
- Feed PRs merge **before** the prplos PR — commit hashes must exist before they can be pinned in profile YAMLs
- Unchanged feeds keep their existing pin — minimize diff, only update what changed
- The `gw3-branch` skill's `commit` and `push` commands enforce correct ordering and safety checks
- The `local/` directory is gitignored — never stage, commit, or push any files under `local/`

---

## Source Code Access and Multi-Repo Rules

### Directory Hierarchy

```
prplos/                              ← project root (prplos repo)
├── aei/                             ← gitignored — local development directory
│   ├── .srclocal.state              ← Feed tier state (original feeds.conf entries)
│   ├── .srclocal-source.state       ← Module tier state (module checkout URLs)
│   ├── feed_prplos/                 ← Feed tier: feed git clone on feature branch
│   │   ├── .git/
│   │   ├── mod-amx-thread/
│   │   │   ├── Makefile             ← editable (autorebuild)
│   │   │   └── files/               ← ODL, init scripts, UCI defaults
│   │   └── ...
│   ├── feed_aei/                    ← Feed tier: another feed clone
│   │   └── ...
│   └── src/                         ← Module tier: module checkouts
│       ├── mod-amx-thread/          ← actual C/C++ source (git repo)
│       │   ├── .git/
│       │   ├── src/
│       │   ├── include/
│       │   └── CMakeLists.txt
│       └── ...
├── feeds/                           ← managed by scripts/feeds
│   ├── feed_prplos → aei/feed_prplos  (symlink when src-linked)
│   ├── feed_amx/                      (normal git clone when not src-linked)
│   └── ...
├── build_dir/target-*/              ← READ-ONLY build snapshots
├── profiles/                        ← profile YAMLs (committed to prplos)
├── specs/                           ← specifications (committed to prplos)
├── refs/                            ← reference materials (gitignored except refs.yaml)
└── local/                           ← dev logs, temp files (gitignored)
```

### Modification Rules

| Location | Read | Modify | Commit To | Notes |
|----------|------|--------|-----------|-------|
| `aei/src/<pkg>/` | Yes | Yes | Package source repo | Live git repo; module tier checkout |
| `aei/<feed>/<pkg>/` | Yes | Yes | Feed repo | Live git repo; feed tier checkout; autorebuild |
| `build_dir/target-*/<pkg>/` | Yes | **NEVER** | N/A | Ephemeral; overwritten on `make clean` |
| `feeds/<feed>/<pkg>/` | Avoid | **NEVER** | N/A | Symlinks; use `aei/<feed>/` directly |
| `specs/` | Yes | Yes | prplos repo | Specifications; keep aligned with code |
| `profiles/` | Yes | Yes | prplos repo | Build profiles; feed revisions live here |
| `refs/<name>/` | Yes | **In own repo only** | Reference's own repo | Never commit ref changes to prplos |
| `local/` | Yes | Yes | **NEVER** | Gitignored; logs, temp files |

### Localization Workflow (Adding a Module to `aei/`)

To make a package editable for feature development:

1. **Feed tier:** `src_local.sh feed_name:feature/branch` — clones the feed into `aei/<feed>/`, sets up src-link in `feeds.conf`, enables autorebuild for Makefiles/ODL/patches
2. **Module tier:** `src_local.sh --source pkg_name:feature/branch` — clones the package source into `aei/src/<pkg>/`, detected automatically by `aei-local-development.mk`
3. **Deep setup (both tiers):** `src_local.sh --deep feed_name:feature/branch` — feed tier + auto-clones all GW3-owned module sources in the feed
4. **Verify:** `src_local.sh --status` confirms active src-links and module checkouts

### Pinning Lifecycle

The transition from development mode to merge-ready state follows a strict sequence:

```
Development (branch tracking)     Merge-Ready (commit pinning)
─────────────────────────────     ────────────────────────────
Profile YAMLs: ;feature/xyz  →   Profile YAMLs: ^a1b2c3d4...
PKG_SOURCE_VERSION: branch    →   PKG_SOURCE_VERSION: 9f8e7d6c...
feeds update pulls latest     →   feeds update gets exact commit
Builds may vary over time     →   Builds are reproducible
```

**Pinning sequence:**
1. `src_local.sh --pin-sources` — pins `PKG_SOURCE_VERSION` in feed Makefiles to current HEAD of each `aei/src/<pkg>/`
2. Commit and push feed changes (pinned Makefiles)
3. `src_local.sh --pin-feeds` — pins profile YAML `revision:` fields to current HEAD of each `aei/<feed>/`
4. Commit and push prplos changes (pinned profiles)
5. `src_local.sh --clean` — restores `feeds.conf` to `src-git` entries
6. Rebuild with pinned config to verify reproducibility

---

## Reference Materials and Data Model Access

### Reference Material Catalog

| Source | Mode | Revision | Description |
|--------|------|----------|-------------|
| `refs/optimeventproto/` | Branch tracking | `copilot-integration` | OptimEvent protobuf definitions and codegen |
| `refs/optim-provision-model/` | Branch tracking | `main` | Optim Provision Schema |
| `refs/optim5_doc/` | Branch tracking (sparse) | `master` | Optim5 documentation |
| `refs/gw3-reference-docs/` | Branch tracking (sparse) | `main` | BBF specs, vendor docs, external guides (KB) |

**Access workflow:**
1. Check `refs/refs.yaml` for available references
2. Run `./scripts/refs-setup.sh` to populate `refs/` (gitignored content)
3. Read documentation from `refs/<name>/` — treat as read-only in the prplos workspace
4. If changes are needed, commit them inside `refs/<name>/` (the reference's own git repo)

### Data Model Access Workflow

When working on a feature that involves the TR-181 data model, follow this 4-step lookup:

1. **Quick lookup** — Check the most recent data model report (Excel/JSON in `specs/`) for current implementation status and marks
2. **Standard reference** — Read the BBF TR-181 specification in `refs/gw3-reference-docs/md/` for the canonical parameter definitions, types, and semantics
3. **ODL source** — Read the package's ODL files in `aei/<feed>/<pkg>/files/` (or `build_dir/` if not src-linked) for the actual implementation definitions
4. **Regenerate** — Run the `gw3-datamodel-report` skill to produce a fresh coverage report if the existing one is stale or missing the objects you need

### Data Model Mark Semantics

| Mark | Color | Decision Rule |
|------|-------|---------------|
| IMPLEMENTED | Green | ODL entry with action callbacks, or readOnly with confirmed C writes, or event emitted |
| NOT IMPLEMENTED | Yellow | ODL entry exists; no action callbacks; no confirmed C writes |
| DEFAULT ONLY | Gray | ReadOnly param in ODL; no callbacks; AMX serves default/init value only |
| REPORT ONLY | Steel | ReadWrite param with confirmed C writes but no action callbacks |
| NOT DEFINED | Red | In TR-181 spec but no ODL definition exists |
| MISMATCH | Orange | ODL exists but type or access differs from TR-181 spec |
| VENDOR EXTENSION | Blue | ODL entry with `X_` prefix; not in TR-181 standard |
| PRPL EXTENSION | Purple | ODL entry without `X_` prefix; not in TR-181 standard |

---

## Quality Gates and Verification

### Pre-Implementation Checklist

Before writing any code for a feature:

- [ ] Feature branch created in prplos and all affected feed/source repos (Principle I)
- [ ] Feature spec exists at `specs/<feature>-features/spec.md` (Principle II)
- [ ] Relevant baseline SDDs in `specs/baseline/` have been read (Principle II)
- [ ] Reference materials checked in `refs/refs.yaml` and populated if needed (Principle V)
- [ ] If feature touches data model: TR-181 paths documented in spec, current marks reviewed (Principle IV)
- [ ] If feature requires new/modified ODL signals or parameters from other services: interface contracts created in `specs/<feature>/contracts/` (Principle II)
- [ ] Local development environment set up via `src_local.sh` — correct layers active (Principle III)
- [ ] Build configuration generated via `gen_config.py` and src-links restored (Principle VI)

### Pre-Commit Checklist

Before committing changes in any repo:

- [ ] Source access hierarchy respected — edits are in `aei/src/` or `aei/<feed>/`, never in `build_dir/` (Principle III)
- [ ] Spec updated to reflect any implementation-discovered changes (Principle II)
- [ ] Commit order: source repos → feed repos → prplos (Principle IX)
- [ ] No files from `local/` or `refs/` content staged (Principles V, IX)
- [ ] HOST vs DUT context verified — no DUT commands in HOST scripts or vice versa (Principle VIII)
- [ ] Both `CLAUDE.md` and `copilot-instructions.md` updated if project rules changed (Principle VII)

### Pre-Merge Checklist

Before creating PRs to `main` / `r3_mainline`:

- [ ] All source commits pinned: `src_local.sh --pin-sources` (Principle IX)
- [ ] All feed commits pinned: `src_local.sh --pin-feeds` (Principle IX)
- [ ] No branch-tracking revisions (`;branch`) remain in profile YAMLs targeting mainline (Principle IX)
- [ ] Feed PRs merged before prplos PR (Principle IX)
- [ ] Build succeeds with pinned revisions (Principle VI)
- [ ] MISMATCH marks resolved for all data model objects in scope (Principle IV)
- [ ] Spec reflects final implementation — no undocumented behavior (Principle II)
- [ ] Feature branch name consistent across all repos (Principle I)

### Periodic Verification

Performed at integration checkpoints or before releases:

- [ ] Data model report regenerated; no unexpected MISMATCH or NOT DEFINED items in feature scope (Principle IV)
- [ ] Reference materials up-to-date: `./scripts/refs-setup.sh update` (Principle V)
- [ ] All active feature branches published to remote — no local-only branches in `aei/` (Principle I)
- [ ] Constitution still aligned with CLAUDE.md and copilot-instructions.md (Principle VII)
- [ ] Build reproducibility verified: clean build from pinned config matches previous output (Principle VI)

---

## Governance

1. **Supremacy.** This constitution supersedes conflicting guidance in CLAUDE.md, copilot-instructions.md, individual SDDs, or ad-hoc team agreements. Where those documents provide additional operational detail that does not contradict a principle, both apply.

2. **Amendments.** Changing or adding a principle requires:
   - A written proposal describing the change and its rationale
   - Review and approval by the project lead
   - Simultaneous update to this constitution, `CLAUDE.md`, and `.github/copilot-instructions.md` (Principle VII)
   - Version bump following semver: MAJOR for new principles or removals, MINOR for rule changes within existing principles, PATCH for clarifications

3. **Compliance verification.** All PRs and code reviews must verify compliance with the relevant principles. Reviewers should reference principle numbers (I-IX) when flagging violations.

4. **AI assistant binding.** AI assistants (Claude Code, GitHub Copilot) operating in the GW3 workspace are bound by this constitution. The principles are encoded in CLAUDE.md and copilot-instructions.md to ensure they are loaded into assistant context automatically.

5. **Conflict resolution.** If a principle conflicts with a practical constraint (e.g., emergency hotfix requiring mainline commit), the deviation must be documented in the PR description with the principle number, the reason for deviation, and the remediation plan.

**Version**: 1.1.0 | **Ratified**: 2026-02-18 | **Last Amended**: 2026-02-19
