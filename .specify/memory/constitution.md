<!--
SYNC IMPACT REPORT
==================
Version change: [TEMPLATE] → 1.0.0
Modified principles: None (initial ratification — all principles newly defined)
Added sections:
  - Core Principles (I–V)
  - Development Workflow
  - Quality Gates
  - Governance
Templates reviewed:
  - .specify/templates/plan-template.md ✅ Constitution Check gates align with principles I–III
  - .specify/templates/spec-template.md ✅ No mandatory section changes required
  - .specify/templates/tasks-template.md ✅ Phase structure aligns with Principle V (git commit per phase)
  - No commands/ directory present in this repo ✅
Follow-up TODOs: None — all placeholders resolved.
-->

# OpenWiFi AP NOS Constitution

## Core Principles

### I. OpenWrt-Based Development

All features and bug fixes MUST be implemented as OpenWrt-compatible patches or
feed packages. Changes that cannot be expressed as clean patches against an
upstream OpenWrt tree are prohibited without explicit justification documented
in the spec. Feed packages MUST declare correct versioning and dependencies;
no implicit runtime assumptions are allowed.

**Rationale**: Upstream alignment ensures long-term maintainability and reduces
custom fork divergence as OpenWrt evolves.

### II. Cross-Platform Target Compatibility

Code, patches, and packages MUST support all platforms listed in the `profiles/`
directory unless a spec explicitly scopes a change to a named target. Platform-
specific workarounds MUST be gated by Kconfig symbols or board detection, never
by hardcoded device names.

**Rationale**: OpenWiFi hardware diversity demands portable implementations;
ad-hoc device targeting creates fragile builds and hidden failures.

### III. Test-First Development (NON-NEGOTIABLE)

For every feature or fix: tests (build validation, integration checks, or manual
acceptance scenarios from the spec) MUST be defined and reviewed before
implementation begins. The Red-Green-Refactor cycle MUST be followed. A feature
is not complete until all acceptance scenarios pass.

**Rationale**: Firmware defects are expensive to field-patch; early validation
prevents regressions across the broad hardware matrix.

### IV. Language Convention

AI agent responses to the user MUST be written in Chinese (中文). All other
artifacts — source code, comments, commit messages, documentation, spec files,
plan files, task files, and template content — MUST be written in English.

**Rationale**: Clear separation between conversational language and technical
artifact language maintains consistency across the repository and simplifies
code review by non-Chinese-speaking contributors.

### V. Git Discipline — Commit After Every Phase

After completing modifications in each phase (setup, foundational work, per-user-
story implementation, QA, etc.), changes MUST be committed to git immediately.
Each commit message MUST include:
- A concise subject line (≤72 chars) in the imperative mood.
- A body listing what was changed, which files were affected, and why the change
  was made.
- A reference to the spec/plan phase (e.g., `Phase 2: foundational data models`).

No work-in-progress state should persist across phase boundaries without a
checkpoint commit.

**Rationale**: Granular commits make bisecting firmware regressions tractable
and provide an auditable trail of incremental progress aligned with the spec.

## Development Workflow

All feature work MUST follow the speckit workflow in order:

1. `speckit.specify` — capture user requirements as a spec.
2. `speckit.clarify` — resolve ambiguities before planning.
3. `speckit.plan` — produce research, data model, and contracts.
4. `speckit.tasks` — generate a dependency-ordered task list.
5. `speckit.implement` — execute tasks phase by phase, committing after each
   phase per Principle V.

Skipping phases is permitted only when a spec explicitly justifies doing so and
the justification is recorded in plan.md.

## Quality Gates

Before merging any feature branch the following MUST be true:

- **Build gate**: `./build.sh <target>` completes without error for at least the
  primary target referenced in the spec.
- **Patch hygiene**: All new patches apply cleanly with `git am` and carry a
  valid `Signed-off-by` line.
- **Feed validity**: Any new or modified feed package passes `make package/<name>/check`.
- **Acceptance scenarios**: All user-story acceptance scenarios from spec.md are
  verified and documented in the PR description.
- **Constitution compliance**: The PR author MUST attest that all five principles
  have been respected; reviewers MUST verify this before approval.

## Governance

This constitution supersedes all informal coding conventions and undocumented
practices in the repository. Amendments require:

1. A written proposal describing the change and its rationale.
2. Consensus from at least two active maintainers.
3. A constitution version bump following semantic versioning (see below).
4. A synchronization pass over all `.specify/templates/` files to reflect the
   updated principles.

**Versioning policy**:
- MAJOR: Removal or redefinition of an existing principle.
- MINOR: New principle or section added; material expansion of existing guidance.
- PATCH: Clarifications, wording improvements, typo fixes.

All PRs and code reviews MUST verify compliance with the five core principles.
Complexity beyond what a principle permits MUST be justified in the spec or plan.

**Version**: 1.0.0 | **Ratified**: 2026-03-05 | **Last Amended**: 2026-03-05
