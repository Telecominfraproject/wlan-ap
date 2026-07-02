# OpenWiFi / OpenLAN — Pull Request Guidelines

How to submit and integrate pull requests for the OpenWiFi/OpenLAN repositories:

- **`wlan-ap`** — AP firmware, targets, packages, patches
- **`wlan-ucentral-schema`** — the uCentral config/state schema and renderer
- **`wlan-ucentral-client`** — the on-device uCentral client (C)

Code formatting and commit-message rules live in
[openwifi-coding-guidelines.md](openwifi-coding-guidelines.md). This document covers the
**PR lifecycle** and has two tracks:

- **[Part A — Contributor Guide](#part-a--contributor-guide):** submitting a PR.
- **[Part B — Reviewer / Merger Guide](#part-b--reviewer--merger-guide):** integrating a PR.

---

## The core principle: PRs are rebased, not merged

These repositories keep a **linear history**. A PR lands by **replaying its commits onto
`main`** (cherry-pick / rebase / `git am`), not by creating a merge commit. As a result:

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

## A1. Branch off the latest `main`, named after the ticket

`main` is the integration branch — never branch off a release or `next`.

```bash
git checkout main
git pull origin main
git checkout -b WIFI-14728-ipq50xx-add-sonicfi-rap630e
```

Branch name = **`WIFI-<number>-<short-kebab-description>`**, e.g.
`WIFI-14817-6G-CBP-fail-on-20M-BW`, `WIFI-14896-support-HaLow-Mesh`.
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

- Subject `subsystem: imperative lower-case summary` (no trailing period) —
  e.g. `ipq50xx: add SonicFi RAP630E`, not `WIFI-14728 ipq50xx : add sonicfi rap630e`.
- Patch-package commits prefixed with the OpenWrt version: `patches-25.12: …`.
- Body explaining **why**, with `-` bullets for multi-part changes.
- **`Fixes: WIFI-<number>`** and **`Signed-off-by: Your Name <email>`** on *every* commit (DCO).
- **`main` builds at every commit** — they may be applied individually.

## A4. Open the PR with a self-justifying description

- **What & why** (mirror the commit body for single-commit PRs).
- **Ticket:** link `WIFI-<number>`.
- **Affected targets / boards / drivers:** be explicit (`ipq50xx`, `WF189`, `qca-wifi-7`).
- **Test evidence:** what you ran, on which hardware, and the result. Board regressions are
  reverted, so show you tested on the affected hardware.
- **Schema changes (`wlan-ucentral-schema`):** confirm you edited the `.yml` source and
  regenerated JSON via `generate.sh` (never hand-edit generated JSON); note any device⇄cloud
  compatibility impact.

## A5. Respond to review by re-pushing a rebased branch

```bash
git rebase -i origin/main       # fold review fixes into the right commits
git push --force-with-lease     # update the same PR; --force-with-lease, not --force
```

Fold review fixes into the relevant commit rather than stacking "address review" commits.
Expect your commit to be **reworded or split** by the integrator for consistency — that is
normal, not a rejection.

## A6. Submodule & cross-repo changes

`wlan-ucentral-schema` is a **submodule** of `wlan-ap`. A schema change is **two PRs**: land
it in `wlan-ucentral-schema` first, then bump the submodule in `wlan-ap` as its own commit
(see B5 for the bump format). The same applies to any change spanning
`wlan-ucentral-client` and the schema it relies on — land the dependency first.

## A7. Contributor checklist

- [ ] Branch `WIFI-<ticket>-<desc>`, cut from latest `main`.
- [ ] Rebased on `main` — **no `Merge branch 'main'` commits**.
- [ ] 1–3 focused commits; no duplicate/fixup/"address review" commits.
- [ ] Each commit: `subsystem: imperative summary`, body says *why*, `Fixes: WIFI-#####`,
      `Signed-off-by:`.
- [ ] `main` builds at every commit.
- [ ] PR description: what/why, ticket, affected targets/boards, **test evidence**.
- [ ] Schema edited in `.yml` + regenerated; submodule bump is its own PR/commit.
- [ ] PR targets `main`, not a release branch.
- [ ] `git log origin/main..HEAD` shows only your real commits, no merge lines.

---

# Part B — Reviewer / Merger Guide

Integrate by **replaying commits onto `main`**. Do **not** use GitHub's "Merge pull request"
button for feature PRs — it creates a merge commit.

## B1. Review before integrating

- **Scope:** one ticket, focused, 1–3 commits. Push back on bundled/unrelated changes.
- **Commit hygiene:** each commit standalone, canonical `subsystem: summary` subject,
  `Fixes:` + `Signed-off-by:` present. Reword non-canonical subjects on apply.
- **Per-commit build:** since commits are applied individually, every commit must leave
  `main` buildable.
- **Hardware risk:** for board/driver changes, require test evidence on the named target.
  When a regression slips through, revert it with a reason (see B6).
- **Schema (`wlan-ucentral-schema`):** confirm `.yml` edited and JSON regenerated via
  `generate.sh`, not hand-edited; check device⇄cloud compatibility.

## B2. Integrate by replaying onto `main`

```bash
git fetch origin WIFI-14728-ipq50xx-add-sonicfi-rap630e
git checkout main
git pull --ff-only origin main

# Option A — cherry-pick specific commits (take some, drop others):
git cherry-pick <sha1> <sha2>

# Option B — rebase the branch onto main, then fast-forward:
git rebase main WIFI-14728-ipq50xx-add-sonicfi-rap630e
git checkout main
git merge --ff-only WIFI-14728-ipq50xx-add-sonicfi-rap630e

# Option C — apply as mailed patches (preserves authorship + S-o-b):
git am < patches.mbox
```

All three keep the contributor as **author** and you as **committer**, preserve their
`Signed-off-by`, and add no merge commit.

## B3. Clean up on apply

- **Reword** non-canonical subjects (`WIFI-14728 ipq50xx : add sonicfi rap630e` →
  `ipq50xx: add SonicFi RAP630E`).
- **Drop** any `Merge branch 'main'` commits that leaked into the branch — rebase past them.
- **Add your own `Signed-off-by`** below the contributor's when you take responsibility for
  the patch; keep theirs intact.
- **Split or squash** if a commit mixes concerns or a fixup should fold into its parent.

## B4. Push and confirm linear history

```bash
git log --pretty='%h %an (committer %cn) %s' -5    # authorship preserved?
git log --merges -1                                 # not your feature apply?
git push origin main
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

- `main` is the integration branch — feature PRs land there first, **never on a release
  branch**.
- Releases are **semver tags with RC candidates** (`v4.2.x`, `v5.0.0`, `…-rcN`). Release/
  version branches (`v2.8.0`, `release/v2.6.0`, `next`, `staging-*`) are the **only** place
  merge commits are allowed — they integrate the release train, e.g.
  `Merge pull request #509 from .../v2.8.0-rc4`.
- Fixes are integrated on `main` and **cherry-picked into the release train**.

## B8. Reviewer / merger checklist

- [ ] PR is scoped to one ticket, 1–3 standalone commits.
- [ ] Each commit canonical subject + `Fixes:` + `Signed-off-by:`; subjects reworded if needed.
- [ ] `main` builds at every commit.
- [ ] Hardware/board change has test evidence on the named target.
- [ ] Applied via cherry-pick / rebase+ff / `git am` — **no merge commit**.
- [ ] Authorship preserved, contributor `Signed-off-by` kept, your `Signed-off-by` added.
- [ ] History still linear (`git log --merges` unchanged for feature work).
- [ ] Submodule bump lists included upstream commits; reverts state a reason.
- [ ] Release work merged only on release/version branches.
