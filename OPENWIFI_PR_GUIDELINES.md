# OpenWiFi / OpenLAN — Pull Request Guidelines

How to submit and integrate pull requests for the OpenWiFi/OpenLAN repositories:

- **`wlan-ap`** — AP firmware, targets, packages, patches
- **`wlan-ucentral-schema`** — the uCentral config/state schema and renderer
- **`wlan-ucentral-client`** — the on-device uCentral client (C)

Code formatting and commit-message rules live in
[OPENWIFI_CODING_GUIDELINES.md](OPENWIFI_CODING_GUIDELINES.md). This document covers the
**PR lifecycle** and has two tracks:

- **[Part A — Contributor Guide](#part-a--contributor-guide):** submitting a PR.
- **[Part B — Reviewer / Merger Guide](#part-b--reviewer--merger-guide):** integrating a PR.

---

## The core principle: PRs are rebased, not merged

These repositories keep a **linear history**. A PR lands by **replaying its commits onto
`main`** (GitHub's "Rebase and merge" button), not by creating a merge commit. As a result:

- The **author field stays the contributor**; the integrator becomes the *committer*.
- The contributor's **`Signed-off-by`** is preserved.
- There is **no "Merge pull request #N" bubble**. The GitHub PR still auto-closes as
  "merged" because its commits now appear on `main`.

Merge commits are reserved exclusively for the release train (version/release branches —
see B7). Everything below is what each side does to make this work.

---

# Part A — Contributor Guide

Deliver a **clean, replayable sequence of standalone commits** so the integrator can replay
your branch without rewriting it.

> **Which base branch?** This guide covers **feature / general-fix PRs, which target
> `main`.** A fix for an already-shipped release (4.2.x / 2.11.x) is a **backport PR that
> targets that release's `staging-for-vX.Y.0-LTS` branch** — nothing lands on
> `release/vX.Y.0` directly (see B7) — see
> [OPENWIFI_RELEASE_PROCEDURE.md](OPENWIFI_RELEASE_PROCEDURE.md).

## A1. Branch off the latest `main`, named after the ticket

`main` is the integration branch for feature work — branch feature/general-fix PRs off
`main` (release backports branch off `staging-for-vX.Y.0-LTS` instead; see the release
procedure).

```bash
git checkout main
git pull origin main
git checkout -b staging-WIFI-14728-ipq50xx-add-sonicfi-rap630e
```

Branch name = **`staging-WIFI-<number>-<short-hyphenated-description>`** — all PR branches
carry the `staging-` prefix. Words are separated by hyphens, with acronyms, band names, and
board identifiers kept in their conventional case (mirroring the subject-line rule) — e.g.
`staging-WIFI-14817-6G-CBP-fail-on-20M-BW`, `staging-WIFI-14896-support-HaLow-Mesh`.
**One ticket → one branch → one focused PR.** Keep PRs small — typically **1–3 commits**.

## A2. Stay current with `rebase`, never `merge`

Do not merge `main` into your branch. Rebase onto it.

```bash
git fetch origin
git rebase origin/main          # ✅ replays your commits on top of main
# NOT: git merge main           # ❌ creates a merge bubble that must be undone on apply
```

## A3. Squash to clean, standalone commits before submitting

```bash
git rebase -i origin/main
```

`squash`/`fixup` the "fix typo", "address review", and duplicate commits into their parent.
Target state: **1–3 commits, each builds on its own**, each with:

- Subject `subsystem: imperative summary` — first word after the colon lower-case, proper
  nouns / board names / identifiers keep their case, no trailing period —
  e.g. `ipq50xx: add SonicFi RAP630E`, not `WIFI-14728 ipq50xx : add sonicfi rap630e`.
- Patch-package commits prefixed with the OpenWrt version: `patches-25.12: …`.
- Required body (see the coding guide, §2 → *Body*): explain why, then what, as plain text.
  Include test evidence ("Tested on EAP105 ...") for functional/board/driver changes.
- **`Signed-off-by: Your Name <email>`** on *every* commit (DCO).
- **`Fixes: WIFI-<number>`** where the commit addresses a tracked issue (bug fixes reference
  a ticket; refactors, additions, and bumps often have none).
- **`main` builds at every commit** — they may be applied individually.

## A4. Open the PR with a self-justifying description

- **What & why** — mirror the commit body. For a single-commit PR, restate why the change
  is needed and what it does; for a multi-commit PR, give a short per-commit summary plus
  the overall test evidence.
- **Ticket:** link `WIFI-<number>`.
- **Affected targets / boards / drivers:** be explicit (`ipq50xx`, `WF189`, `qca-wifi-7`).
- **Test evidence:** what you ran, on which hardware, and the result — the PR-level view of
  the test evidence in each commit body. Board regressions are reverted, so show you tested
  on the affected hardware.
- **Schema changes (`wlan-ucentral-schema`):** confirm you edited the `.yml` source and
  regenerated JSON via `generate.sh` (never hand-edit generated JSON); note any device⇄cloud
  compatibility impact.

## A5. Respond to review by re-pushing a rebased branch

```bash
git rebase -i origin/main       # fold review fixes into the right commits
git push --force-with-lease     # update the same PR; --force-with-lease, not --force
```

Fold review fixes into the relevant commit rather than stacking "address review" commits.
Commits land on `main` exactly as pushed — nothing is rewritten on apply — so expect
reviewers to ask you to **reword or split** commits before the PR is merged; that is
normal, not a rejection.

## A6. Submodule & cross-repo changes

`wlan-ucentral-schema` is a **submodule** of `wlan-ap`. A schema change is **two PRs**: land
it in `wlan-ucentral-schema` first, then bump the submodule in `wlan-ap` as its own commit
(see B5 for the bump format). The same applies to any change spanning
`wlan-ucentral-client` and the schema it relies on — land the dependency first.

## A7. Contributor checklist

- [ ] Branch `staging-WIFI-<ticket>-<desc>`, cut from latest `main`.
- [ ] Rebased on `main` — **no `Merge branch 'main'` commits**.
- [ ] 1–3 focused commits; no duplicate/fixup/"address review" commits.
- [ ] Each commit: `subsystem: imperative summary`; required body (why, then what) with
      test evidence for functional/board changes; `Signed-off-by:` present; `Fixes: WIFI-#####`
      where applicable.
- [ ] `main` builds at every commit.
- [ ] PR description: what/why, ticket, affected targets/boards, **test evidence**.
- [ ] Schema edited in `.yml` + regenerated; submodule bump is its own PR/commit.
- [ ] No trunk-applicable fix targets an LTS branch directly — landed on `main` first and
      cherry-picked (release-only fixes excepted, with a note saying why).
- [ ] Feature/general-fix PR targets `main`; a release backport targets
      `staging-for-vX.Y.0-LTS` (see [OPENWIFI_RELEASE_PROCEDURE.md](OPENWIFI_RELEASE_PROCEDURE.md)).
- [ ] `git log origin/main..HEAD` shows only your real commits, no merge lines.

---

# Part B — Reviewer / Merger Guide

Integrate with GitHub's **"Rebase and merge"** button — it replays the PR's commits onto
`main` with no merge commit. Do **not** use "Create a merge commit" or "Squash and merge"
for feature PRs.

## B1. Review before integrating

- **Scope:** one ticket, focused, 1–3 commits. Push back on bundled/unrelated changes.
- **Commit hygiene:** each commit standalone, canonical `subsystem: summary` subject,
  a required body (why, then what), `Signed-off-by:` present (and `Fixes:` where the commit
  fixes a tracked issue). Reword non-canonical subjects on apply.
- **Per-commit build:** since commits are applied individually, every commit must leave
  `main` buildable.
- **Hardware risk:** for board/driver changes, require test evidence on the named target,
  recorded in the commit body ("Tested on ..."). When a regression slips through, revert it
  with a reason (see B6).
- **Schema (`wlan-ucentral-schema`):** confirm `.yml` edited and JSON regenerated via
  `generate.sh`, not hand-edited; check device⇄cloud compatibility.

## B2. Integrate with Rebase and merge

Confirm the PR branch is rebased on the current `main` and every commit is in its final
shape — canonical subject, required body, `Signed-off-by` present — then press GitHub's
**"Rebase and merge"** button. Nothing is rewritten on apply.

This keeps the contributor as **author** and you as **committer**, preserves their
`Signed-off-by`, and adds no merge commit. If only some commits should land, ask the
contributor to drop or split them and re-push, rather than cherry-picking around the PR.

## B3. Clean up before merge

Rebase and merge applies commits verbatim, so fix-ups happen on the PR branch before the
button is pressed — request them from the contributor, or push to the PR branch:

- **Reword** non-canonical subjects (`WIFI-14728 ipq50xx : add sonicfi rap630e` →
  `ipq50xx: add SonicFi RAP630E`).
- **Drop** any `Merge branch 'main'` commits that leaked into the branch — rebase past them.
- **Split or squash** if a commit mixes concerns or a fixup should fold into its parent.

## B4. Push and confirm linear history

```bash
git log --pretty='%h %an (committer %cn) %s' -5    # authorship preserved?
git log --merges -1                                 # not your feature apply?
```

The contributor's PR auto-closes as "merged" once the commits are on `main`.

## B5. Submodule bumps are explicit, documented commits

When `wlan-ucentral-schema` (submodule of `wlan-ap`) moves, the bump commit body **lists the
included upstream commits** so the diff is self-documenting:

```
ucentral-schema: update to latest HEAD

96e4e5e renderer: skip 160MHz fallback on 5G when DFS is disabled
382515a renderer: wait for netifd wireless to settle before promoting config

Signed-off-by: John Crispin <john@phrozen.org>
```

Never bump to an arbitrary unreleased SHA without listing what moved.

## B6. Reverts

When something breaks after landing, revert with the standard format **plus a reason**:

```
Revert "qca-wifi-7: ipq53xx: rtk_phy: Fix 10G link mode for RTL8261N"

This reverts commit 5b73a776...
This commit is causing a regression on boards like WF198

Signed-off-by: ...
```

## B7. Releases

- `main` is the trunk — **feature/general-fix PRs land there first**. Changes for an LTS
  release go onto its **`staging-for-v<X.Y.0>-LTS`** branch (e.g. `staging-for-v4.2.0-LTS`),
  where they are validated, and the staging branch is then **merged into `release/v<X.Y.0>`**
  (e.g. `release/v4.2.0`) only once everything has been checked. `release/v<X.Y.0>` is not
  touched until then, so it is never affected by unverified changes.
- Releases are **semver tags with RC candidates** (`v4.2.x`, `…-rcN`), cut **on the release
  branch only** (`release/v<X.Y.0>`) — never on `main` or the staging branch. The LTS branch
  is **merge-based**: backport PRs from `staging-WIFI-…-for-vX.Y.Z` branches are merged in,
  e.g. `Merge pull request #1046 from .../staging-WIFI-15388-pmf-value-fix-for-4.2.3`.
- **Trunk-first (required):** a fix that also applies to the trunk **lands on `main` first**,
  then is **cherry-picked** into the LTS branch (`staging-for-v<X.Y.0>-LTS`). Do **not** open
  a fix PR directly against the LTS branch when it also applies to `main`. **Release-only**
  fixes (version bumps, RC-specific or release-branch-only changes) are authored directly on
  the LTS branch, with a note in the PR saying why they don't apply to `main`. Reviewers
  reject direct-to-LTS PRs that aren't genuinely release-only. The full workflow — branching,
  backport PRs, version bump, tagging — is in
  [OPENWIFI_RELEASE_PROCEDURE.md](OPENWIFI_RELEASE_PROCEDURE.md).

## B8. Reviewer / merger checklist

- [ ] PR is scoped to one ticket, 1–3 standalone commits.
- [ ] Each commit: canonical subject; required body (why, then what) with test evidence for
      functional/board changes; `Signed-off-by:` present; `Fixes:` where applicable; subjects
      reworded if needed.
- [ ] `main` builds at every commit.
- [ ] Hardware/board change has test evidence on the named target (commit body or
      optional `Tested-by:` trailer).
- [ ] QA has signed off on the ticket's staging branch, where QA coverage exists
      (non-blocking; note in the PR when a change ships without QA validation).
- [ ] Integrated via **Rebase and merge** — **no merge commit**.
- [ ] Authorship preserved, contributor `Signed-off-by` kept.
- [ ] History still linear (`git log --merges` unchanged for feature work).
- [ ] Submodule bump lists included upstream commits; reverts state a reason.
- [ ] Release work merged only on release/version branches.
