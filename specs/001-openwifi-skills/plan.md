# Implementation Plan: OpenWiFi Agents Skills Suite

**Branch**: `001-openwifi-skills` | **Date**: 2026-03-05 | **Spec**: [spec.md](spec.md)
**Input**: Feature specification from `specs/001-openwifi-skills/spec.md`

## Summary

Port 9 agent skills from demo-github/skills/ to .github/skills/ in the OpenWiFi
repository (knowledge-base management, Jira/Confluence/Bitbucket collaboration,
WSL detection), then develop three new skills — server-build, local-build, and
upgrade-dut — to automate the firmware build-flash-test cycle.

Adapted skills require no functional code changes; only destination paths and
any gw3-specific references in SKILL.md/spec.md are updated. New skills are
pure shell-script + SKILL.md pairs following the same skill directory convention.

## Technical Context

**Language/Version**: Bash 5.x (all new skills); Python 3.11 (inherited jira/confluence scripts)  
**Primary Dependencies**: sshpass or ssh-agent (server-build, upgrade-dut); uv (jira Python scripts); pymupdf4llm (KB conversion); git (KB commit/push)  
**Storage**: File system — skills live under `.github/skills/<name>/`; KB data under `refs/<kb-name>/`  
**Testing**: Manual acceptance scenario execution per spec.md; shell script `--dry-run` flags where provided  
**Target Platform**: Linux (native and WSL2); macOS secondary  
**Project Type**: Developer tooling / agent skill collection  
**Performance Goals**: server-build SSH session started within 5 s of invocation; upgrade-dut SCP completes proportional to image size (~40 MB typical = <60 s on LAN)  
**Constraints**: No credentials committed to repository; SSH passwords handled via interactive prompt or sshpass; KB skills require pymupdf4llm installed by user  
**Scale/Scope**: 12 skill directories; ~5–15 files per skill; single developer at a time

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

- [x] **I. OpenWrt-Based Development**: Skills are developer-tooling files under `.github/skills/` — they do not modify the OpenWrt tree itself. Justified: tooling support is orthogonal to firmware patches and does not require an OpenWrt patch or feed package.
- [x] **II. Cross-Platform Compatibility**: Build skills accept any profile name from `profiles/` and do not hardcode device names; gen_config.py resolves platform-specific config. Justified scope: build/upgrade tooling wrapper does not need to be a Kconfig symbol.
- [x] **III. Test-First**: All acceptance scenarios are fully defined in spec.md §User Scenarios before any implementation task begins. ✅
- [x] **IV. Language Convention**: All skill files, scripts, and commit messages written in English. ✅
- [x] **V. Git Discipline**: This plan mandates a checkpoint commit after each implementation phase. ✅

**Constitution Check Status**: All gates PASS. Proceed to Phase 0.

*Note on Principle I*: This feature produces `.github/skills/` metadata files
and Bash helper scripts, not OpenWrt patches. This is explicitly justified: agent
skill support is infrastructure for *developers*, not for the firmware itself. No
OpenWrt tree modification occurs.

## Project Structure

### Documentation (this feature)

```text
specs/001-openwifi-skills/
├── plan.md              # This file
├── research.md          # Phase 0 output
├── data-model.md        # Phase 1 output
├── quickstart.md        # Phase 1 output
├── contracts/           # Phase 1 output
│   ├── server-build.md
│   ├── local-build.md
│   └── upgrade-dut.md
└── tasks.md             # Phase 2 output (/speckit.tasks — NOT created here)
```

### Source Code (repository root)

```text
.github/
└── skills/
    │
    │   # Group A: Knowledge Base Management (adapted from demo-github/skills/)
    ├── kb-add-file/
    │   ├── SKILL.md         # Adapted: update source_kb reference
    │   └── spec.md          # Adapted: strip gw3-specific KB name
    ├── kb-add-batch/
    │   ├── SKILL.md
    │   └── spec.md
    ├── kb-remove/
    │   ├── SKILL.md
    │   └── spec.md
    ├── refs-download/
    │   ├── SKILL.md         # Adapted: update script path reference
    │   └── spec.md
    │
    │   # Group B: Project Collaboration (adapted from demo-github/skills/)
    ├── jira-communication/
    │   ├── SKILL.md
    │   ├── AGENTS.md
    │   ├── references/
    │   └── scripts/         # All Python scripts copied verbatim
    ├── jira-syntax/
    │   ├── SKILL.md
    │   ├── AGENTS.md
    │   ├── references/
    │   ├── scripts/
    │   └── templates/
    ├── confluence/
    │   ├── SKILL.md
    │   ├── examples/
    │   ├── references/
    │   └── scripts/         # All Python scripts copied verbatim
    ├── bkt/
    │   ├── SKILL.md
    │   └── references/
    │
    │   # Group C: Environment Detection (adapted from demo-github/skills/)
    ├── detect-wsl/
    │   ├── SKILL.md
    │   ├── detect-wsl.sh    # Copied verbatim — no changes needed
    │   └── spec.md
    │
    │   # Group D/E/F: New skills
    ├── server-build/
    │   ├── SKILL.md         # Invocation guide + step-by-step workflow
    │   └── server-build.sh  # Wrapper: SSH + docker + gen_config + make
    ├── local-build/
    │   ├── SKILL.md
    │   └── local-build.sh   # Wrapper: gen_config + make
    └── upgrade-dut/
        ├── SKILL.md
        └── upgrade-dut.sh   # Wrapper: SCP + sysupgrade

refs/
└── refs.yaml                # Manifest for /refs-download skill (new file)
```

**Structure Decision**: Single flat skill directory under `.github/skills/`.
Skills are self-contained directories; no src/ or tests/ hierarchy needed.
All skill logic lives in SKILL.md (agent instructions) + optional shell scripts.

## Complexity Tracking

No Constitution Check violations recorded. No complexity justification required.
