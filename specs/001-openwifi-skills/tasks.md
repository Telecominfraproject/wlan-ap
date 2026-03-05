---
description: "Task list for OpenWiFi Agents Skills Suite"
---

# Tasks: OpenWiFi Agents Skills Suite

**Input**: Design documents from `specs/001-openwifi-skills/`
**Prerequisites**: plan.md ✅, spec.md ✅, research.md ✅, data-model.md ✅, contracts/ ✅

**Organization**: Tasks are grouped by user story to enable independent implementation
and testing of each story. Tests are NOT required (not requested in spec).

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies)
- **[Story]**: Which user story this task belongs to (US1–US6)

---

## Phase 1: Setup

**Purpose**: Create the `.github/skills/` directory scaffold

- [ ] T001 Create `.github/skills/` directory structure (one subdirectory per skill)

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Infrastructure that MUST exist before any skill content can be placed

**⚠️ CRITICAL**: No story work can begin until this phase is complete

- [ ] T002 Create `refs/` directory and `refs/refs.yaml` manifest with placeholder entry for `openwifi-reference-docs` in `refs/refs.yaml`
- [ ] T003 Create `scripts/refs-setup.sh` (adapted from gw3 version: replace `gw3-reference-docs` with `openwifi-reference-docs`, preserve sparse-checkout logic) in `scripts/refs-setup.sh`

**Checkpoint**: Foundation ready — user story implementation can now begin

---

## Phase 3: User Story 1 — KB Management Skills (Priority: P1) 🎯 MVP

**Goal**: Adapt four knowledge-base skills from `demo-github/skills/` to
`.github/skills/`, updating gw3-specific KB name references to `openwifi-reference-docs`.

**Independent Test**: Invoke `/kb-add-file /tmp/sample.pdf` in the OpenWiFi repo
and confirm `refs/openwifi-reference-docs/md/catalog.yaml` is updated. Invoke
`/refs-download` and confirm cloning proceeds using `scripts/refs-setup.sh`.

### Implementation for User Story 1

- [ ] T004 [P] [US1] Copy `demo-github/skills/kb-add-file/` to `.github/skills/kb-add-file/`; update `source_kb: gw3-reference-docs` → `source_kb: openwifi-reference-docs` and examples in `.github/skills/kb-add-file/SKILL.md`
- [ ] T005 [P] [US1] Copy `demo-github/skills/kb-add-file/spec.md` to `.github/skills/kb-add-file/spec.md`; replace `gw3-reference-docs` with `openwifi-reference-docs` throughout `.github/skills/kb-add-file/spec.md`
- [ ] T006 [P] [US1] Copy `demo-github/skills/kb-add-batch/` to `.github/skills/kb-add-batch/`; update `source_kb` and KB name examples in `.github/skills/kb-add-batch/SKILL.md`
- [ ] T007 [P] [US1] Copy `demo-github/skills/kb-add-batch/spec.md` to `.github/skills/kb-add-batch/spec.md`; replace `gw3-reference-docs` with `openwifi-reference-docs` in `.github/skills/kb-add-batch/spec.md`
- [ ] T008 [P] [US1] Copy `demo-github/skills/kb-remove/` to `.github/skills/kb-remove/`; update `source_kb` frontmatter in `.github/skills/kb-remove/SKILL.md`
- [ ] T009 [P] [US1] Copy `demo-github/skills/kb-remove/spec.md` to `.github/skills/kb-remove/spec.md`; replace `gw3-reference-docs` with `openwifi-reference-docs` in `.github/skills/kb-remove/spec.md`
- [ ] T010 [P] [US1] Copy `demo-github/skills/refs-download/` to `.github/skills/refs-download/`; replace all `gw3-reference-docs` occurrences with `openwifi-reference-docs` and update script path to `scripts/refs-setup.sh` in `.github/skills/refs-download/SKILL.md`
- [ ] T011 [P] [US1] Copy `demo-github/skills/refs-download/spec.md` to `.github/skills/refs-download/spec.md`; replace `gw3-reference-docs` with `openwifi-reference-docs` in `.github/skills/refs-download/spec.md`

**Checkpoint**: US1 complete — KB skills fully functional and independently testable

---

## Phase 4: User Story 2 — Project Collaboration Skills (Priority: P2)

**Goal**: Adapt Jira, Confluence, and Bitbucket skills from `demo-github/skills/`
to `.github/skills/`. Python scripts are URL-agnostic and copied verbatim; only
SKILL.md header references are reviewed.

**Independent Test**: Run `uv run .github/skills/jira-communication/scripts/core/jira-validate.py --verbose`
and confirm it can connect to the Jira instance using credentials from `~/.env.jira`.

### Implementation for User Story 2

- [ ] T012 [P] [US2] Copy `demo-github/skills/jira-communication/` tree verbatim to `.github/skills/jira-communication/` (SKILL.md, AGENTS.md, references/, all scripts/ subdirs)
- [ ] T013 [P] [US2] Copy `demo-github/skills/jira-syntax/` tree verbatim to `.github/skills/jira-syntax/` (SKILL.md, AGENTS.md, references/, scripts/, templates/)
- [ ] T014 [P] [US2] Copy `demo-github/skills/confluence/` tree verbatim to `.github/skills/confluence/` (SKILL.md, examples/, references/, all scripts/)
- [ ] T015 [P] [US2] Copy `demo-github/skills/bkt/` tree verbatim to `.github/skills/bkt/` (SKILL.md, references/)

**Checkpoint**: US2 complete — collaboration skills functional and independently testable

---

## Phase 5: User Story 3 — WSL Environment Detection (Priority: P3)

**Goal**: Copy the `detect-wsl` skill verbatim — no OpenWiFi-specific changes needed.

**Independent Test**: Execute `.github/skills/detect-wsl/detect-wsl.sh --json` and
confirm valid JSON output with `is_wsl`, `wsl_version`, and `detection_methods` fields.

### Implementation for User Story 3

- [ ] T016 [P] [US3] Copy `demo-github/skills/detect-wsl/` tree verbatim to `.github/skills/detect-wsl/` (SKILL.md, detect-wsl.sh, spec.md); verify `detect-wsl.sh` is executable (`chmod +x`)

**Checkpoint**: US3 complete — WSL detection independently testable

---

## Phase 6: User Story 4 — Remote Server Build (Priority: P1) 🎯 MVP

**Goal**: Develop `server-build` skill from scratch per contract in
`specs/001-openwifi-skills/contracts/server-build.md`.

**Independent Test**: Invoke `/server-build cig_wf188n`; confirm SSH to
192.168.20.30 succeeds, `run_build_docker wf188_tip` is called, `gen_config.py`
and `make` run, and image path is reported on success.

### Implementation for User Story 4

- [ ] T017 [US4] Create `.github/skills/server-build/server-build.sh` implementing:
  - Profile validation against `profiles/*.yml` list (exit 1 if invalid)
  - SSH reachability check for `ruanyaoyu@192.168.20.30` (exit 2 if unreachable; suggest `/local-build`)
  - `sshpass` detection + fallback to interactive SSH
  - Remote: `run_build_docker wf188_tip` → `cd openwrt` → `./scripts/gen_config.py <profile>` → `make -j$(nproc) 2>&1 | tee /tmp/build-<profile>.log`
  - On exit 0: grep `openwrt/bin/` for image files and print paths
  - On exit ≠ 0: tail last 50 lines of remote log and exit 3
  in `.github/skills/server-build/server-build.sh`
- [ ] T018 [US4] Create `.github/skills/server-build/SKILL.md` with YAML frontmatter (`name: server-build`, description) and full invocation guide referencing `server-build.sh`, defaulting to `--host 192.168.20.30 --user ruanyaoyu --docker-env wf188_tip`, listing all output formats and exit codes from the contract in `.github/skills/server-build/SKILL.md`

**Checkpoint**: US4 complete — remote server build independently testable

---

## Phase 7: User Story 5 — Local Build (Priority: P2)

**Goal**: Develop `local-build` skill from scratch per contract in
`specs/001-openwifi-skills/contracts/local-build.md`.

**Independent Test**: Invoke `/local-build edgecore_eap104` from repo root;
confirm `gen_config.py` and `make` run in local `openwrt/` directory.

### Implementation for User Story 5

- [ ] T019 [US5] Create `.github/skills/local-build/local-build.sh` implementing:
  - `openwrt/` existence check (exit 1 with `./setup.py --setup` guidance if missing)
  - Profile validation against `profiles/*.yml` list (exit 2 if invalid)
  - `cd openwrt && ./scripts/gen_config.py <profile>` (exit 3 on failure)
  - `make -j$(nproc) 2>&1 | tee /tmp/build-<profile>.log` (exit 4 on failure)
  - On success: scan `openwrt/bin/` for image files and print paths
  - On failure: print last 50 lines of log
  in `.github/skills/local-build/local-build.sh`
- [ ] T020 [US5] Create `.github/skills/local-build/SKILL.md` with YAML frontmatter and invocation guide referencing `local-build.sh`, defaulting `--openwrt-dir openwrt/`, documenting all exit codes and output formats from the contract in `.github/skills/local-build/SKILL.md`

**Checkpoint**: US5 complete — local build independently testable

---

## Phase 8: User Story 6 — DUT Firmware Upgrade (Priority: P1) 🎯 MVP

**Goal**: Develop `upgrade-dut` skill from scratch per contract in
`specs/001-openwifi-skills/contracts/upgrade-dut.md`.

**Independent Test**: Invoke `/upgrade-dut openwrt/bin/targets/ath79/generic/firmware.bin`;
confirm SCP to DUT `/tmp/` succeeds and `sysupgrade -n` is issued; confirm
SSH connection drop after sysupgrade is not treated as an error.

### Implementation for User Story 6

- [ ] T021 [US6] Create `.github/skills/upgrade-dut/upgrade-dut.sh` implementing:
  - Image resolution: if `$1` given verify file exists (exit 1 if not); else recursive glob `openwrt/bin/**/*.bin openwrt/bin/**/*.img` → exit 1 if 0 found, auto-select if 1, numbered prompt if >1
  - DUT SSH reachability check for `root@<dut-ip>` (exit 2 if unreachable)
  - `scp <image> root@<dut-ip>:/tmp/` (exit 3 on failure; do NOT proceed to sysupgrade)
  - `ssh root@<dut-ip> "sysupgrade -n /tmp/<filename>" || true`
  - Print "Upgrade command issued. DUT is rebooting — connection drop is expected."
  in `.github/skills/upgrade-dut/upgrade-dut.sh`
- [ ] T022 [US6] Create `.github/skills/upgrade-dut/SKILL.md` with YAML frontmatter and invocation guide referencing `upgrade-dut.sh`, defaulting `dut-ip=192.168.1.1 dut-user=root`, documenting image auto-discovery logic, all exit codes, SCP failure behavior, and sysupgrade connection-drop behavior from the contract in `.github/skills/upgrade-dut/SKILL.md`

**Checkpoint**: US6 complete — DUT upgrade independently testable

---

## Phase 9: Polish & Cross-Cutting Concerns

- [ ] T023 [P] Verify all shell scripts have executable permission (`chmod +x .github/skills/server-build/server-build.sh .github/skills/local-build/local-build.sh .github/skills/upgrade-dut/upgrade-dut.sh`)
- [ ] T024 [P] Verify all `SKILL.md` files have valid YAML frontmatter (name, description fields present) in all 12 `.github/skills/*/SKILL.md` files
- [ ] T025 [P] Add `.github/skills/` entry to `.gitignore` exclusion patterns for any auto-generated `__pycache__` and `.pyc` files; update `.gitignore`
- [ ] T026 Update `demo-github/copilot-instructions.md` (if it references gw3 skill paths) to also reference `.github/skills/` in `demo-github/copilot-instructions.md`

---

## Dependencies (Story Completion Order)

```
Phase 1 (Setup)
  └─► Phase 2 (Foundation: refs/refs.yaml, scripts/refs-setup.sh)
        ├─► Phase 3 (US1: KB skills) — INDEPENDENT, MVP
        ├─► Phase 4 (US2: Collaboration skills) — INDEPENDENT
        ├─► Phase 5 (US3: detect-wsl) — INDEPENDENT
        ├─► Phase 6 (US4: server-build) — INDEPENDENT, MVP
        ├─► Phase 7 (US5: local-build) — INDEPENDENT
        └─► Phase 8 (US6: upgrade-dut) — INDEPENDENT, MVP
              └─► Phase 9 (Polish)
```

**US4 (server-build) and US6 (upgrade-dut) are independent of US1–US3**.
US6 (upgrade-dut) can consume images from US4 (server-build) or US5 (local-build)
but the skills themselves are independent artifacts.

## Parallel Execution Examples

### After Phase 2 completes, all of these can run simultaneously:

- **Agent A**: T004–T011 (US1: KB skills)
- **Agent B**: T012–T015 (US2: Collaboration skills)
- **Agent C**: T016 (US3: detect-wsl)
- **Agent D**: T017–T018 (US4: server-build)
- **Agent E**: T019–T020 (US5: local-build)
- **Agent F**: T021–T022 (US6: upgrade-dut)

### Within US1, T004–T011 are all [P] (different skill directories, no interdependencies)

### Within US2, T012–T015 are all [P] (different skill directories, no interdependencies)

## Implementation Strategy

**MVP Scope** (deliver value immediately): Phases 1–2 + Phase 6 (server-build) + Phase 8 (upgrade-dut)
- A developer can build firmware on the shared server and flash a DUT without any other skills

**Increment 2**: Add Phase 3 (KB skills) — enables AI-assisted development with reference docs

**Increment 3**: Add Phases 4–5 + Phase 7 — full collaboration + local build capability

**Full delivery**: Phase 9 polish
