# OpenWiFi / OpenLAN — Release Procedure (main → LTS release branches)

How a change flows from `main` into an **official release branch** (`release/vX.Y.0`) and is
cut as a `vX.Y.Z` tag, for `wlan-ap` and its submodules `wlan-ucentral-schema` /
`wlan-ucentral-client`. This is the companion to
[OPENWIFI_PR_GUIDELINES.md](OPENWIFI_PR_GUIDELINES.md) (trunk PR lifecycle) and
[OPENWIFI_CODING_GUIDELINES.md](OPENWIFI_CODING_GUIDELINES.md).

The two current LTS lines are **`release/v4.2.0`** (tags `v4.2.0…v4.2.5`) and
**`release/v2.11.0`** (tags `v2.11.0…v2.11.5`). Both follow the identical model below.

---

## Branch model

| Branch | Role | Merge policy |
|---|---|---|
| `main` | Integration trunk. Feature work and general fixes land here first. | **Linear** — rebase / cherry-pick / `git am`, **no merge commits**. |
| `release/vX.Y.0` | **Official release/LTS branch.** All `vX.Y.Z` tags live here — never on `main`. | **Merge-based** — fixes arrive via GitHub PRs (merge commits are intended). |
| `staging-*` | Short-lived branches carrying the fix(es) for one release PR. Deleted after merge. | Disposable. |
| `next` | Optional pre-integration staging for larger sets. | Merged into `main`. |

Two facts, both verified against history:

1. **`main` is linear; the release branches are merge-based.** These are deliberately
   opposite. Don't merge `main` into a feature branch; don't rebase a release branch.
2. **All `vX.Y.Z` tags sit on the release branch, not `main`** (only the very first `-rc1`
   is tagged on `main`, before the branch is cut).

---

## Where PRs are created

| Change type | Branch from | PR base branch |
|---|---|---|
| New feature / general fix | `main` | **`main`** |
| Fix for a shipped release (4.2.x / 2.11.x) | **`release/vX.Y.0`** | **`release/vX.Y.0`** |

Every recent patch-release PR into both `release/v4.2.0` and `release/v2.11.0` was **forked
from the release branch itself** and merged back into it — not forked from `main`. Set the
GitHub **base branch to the release branch** for a backport PR.

---

## Stage 1 — Land the change on `main` (if it applies to the trunk)

Follow [OPENWIFI_PR_GUIDELINES.md](OPENWIFI_PR_GUIDELINES.md): branch `WIFI-<ticket>-<desc>`
off `main`, clean rebased commits, PR against `main`, integrated by replaying commits.
**Record the resulting SHA(s)** — you cherry-pick them in Stage 2.

Skip this stage only for **release-only** fixes (version-specific BDF, a fix that no longer
applies to a diverged `main`, vendor-specific work). These are authored directly on the
release-branch staging branch. Both paths occur in practice — e.g.
`staging-WIFI-15388-pmf-value-fix-for-4.2.3` and the `inventum-v2.11-pki2.0` vendor branch
carry commits that are **not** on `main`.

---

## Stage 2 — Promote the fix into the release branch

Two mechanisms are used, and a line shifts from the first to the second as it matures.

### 2a. Selective backport via a `staging-*` branch (maintenance default)

Used for `v4.2.3–v4.2.5` and `v2.11.2–v2.11.5`. Only chosen fixes are taken.

```bash
git fetch origin
# Branch the staging area off the RELEASE branch (not main):
git checkout -b staging-WIFI-15388-pmf-value-fix-for-4.2.3 origin/release/v4.2.0

# Bring in the fix — cherry-pick from main if it landed there, else commit directly:
git cherry-pick <sha-from-main>        # keep authorship + Signed-off-by intact
#   (or author the release-only fix here)

git push origin staging-WIFI-15388-pmf-value-fix-for-4.2.3
# Open PR  ->  base: release/v4.2.0
```

**Staging branch naming** (established):
- Per-ticket backport: `staging-WIFI-<number>-<short-desc>[-for-4.2.x]`
  (`staging-WIFI-15420`, `staging-WIFI-15379-wifi5-CN-mismatch-hostname-validate`,
  `staging-WIFI-15388-pmf-value-fix-for-4.2.3`)
- Release roll-up: `staging-for-vX.Y.Z` / `staging-for-vX.Y.0-LTS`
- Vendor line: e.g. `inventum-v2.11-pki2.0`

**Merge with the GitHub "Merge pull request" button** — a real merge commit on the release
branch is intended here (committer shows as `GitHub`):
```
Merge pull request #1046 from Telecominfraproject/staging-WIFI-15388-pmf-value-fix-for-4.2.3
```
Delete the `staging-*` branch after merge.

### 2b. Bulk forward-merge of `main` (early in a line's life)

Used for `v4.2.0/1/2` and `v2.11.0`: the whole of `main` is merged into the release branch
via a PR from `main`:
```
Merge pull request #985  from Telecominfraproject/main   → v4.2.0
Merge pull request #583  from Telecominfraproject/main   → v2.11.0
```
Use this only when the branch is meant to track `main` wholesale — typically the `.0` and the
first patches. Once in maintenance, prefer **2a** so unrelated `main` churn stays out.

---

## Stage 3 — Bump versions and tag

On the release branch, after the release's fix PR(s) are merged:

1. **Bump the version** in the affected components (own commits, canonical style):
   ```
   ucentral: set version to 4.2.0
   ucentral-client: set version to v4.2.x
   ucentral-schema: set version to v4.2.x
   ```
   A moved submodule (`wlan-ucentral-schema`) is bumped with an explicit commit listing the
   included upstream commits (see coding guidelines §2).

2. **Tag on the release branch** — RC first if validating, then GA:
   ```bash
   git checkout release/v4.2.0
   git pull --ff-only origin release/v4.2.0
   git tag -a v4.2.5-rc1 -m "wlan-ap v4.2.5-rc1"
   git tag -a v4.2.5     -m "wlan-ap v4.2.5"
   git push origin v4.2.5-rc1 v4.2.5
   ```
   The `vX.Y.Z` tag sits on the merge commit that brought in the release's fixes
   (e.g. `v4.2.5` → `Merge pull request #1078 … staging-for-v4.2.0-LTS`;
   `v2.11.5` → `Merge pull request #1086 … staging-WIFI-15487`).

---

## Worked examples — how the tags were actually cut

**`release/v4.2.0`:**

| Tag | Cut from | Mechanism |
|---|---|---|
| v4.2.0 | `Merge PR #985 … /main` | 2b bulk forward-merge |
| v4.2.1 | `Merge PR #1002 … /main` | 2b bulk forward-merge |
| v4.2.2 | `Merge PR #1016 … /main` | 2b bulk forward-merge |
| v4.2.3 | `Merge PR #1036 … /staging-for-v4.2.3` | 2a selective backport |
| v4.2.4 | `Merge PR #1052 … /staging-for-v4.2.0` | 2a selective backport |
| v4.2.5 | `Merge PR #1078 … /staging-for-v4.2.0-LTS` | 2a selective backport |

**`release/v2.11.0`:**

| Tag | Cut from | Mechanism |
|---|---|---|
| v2.11.0 | `Merge PR #583 … /main` | 2b bulk forward-merge |
| v2.11.1 | `Merge PR #990 … /inventum-v2.11-pki2.0` | 2a (vendor branch) |
| v2.11.2 | `Merge PR #1025 … /staging-WIFI-15379-…` | 2a selective backport |
| v2.11.3 | `Merge PR #1064 … /staging-WIFI-15420` | 2a selective backport |
| v2.11.4 | `Merge PR #1034 … /staging-fix-stats-null-bridge-interface` | 2a selective backport |
| v2.11.5 | `Merge PR #1086 … /staging-WIFI-15487` | 2a selective backport |

Both lines shift from "track main" (2b) to "selective backport" (2a) once in LTS maintenance.

---

## Quick checklist — shipping a fix in a release

- [ ] If it applies to the trunk, it is merged on `main` first (linear). SHA(s) recorded.
- [ ] `staging-WIFI-<ticket>-<desc>[-for-X.Y.Z]` branch cut **from `release/vX.Y.0`**.
- [ ] Fix cherry-picked from `main` (or authored directly for a release-only fix); authorship
      + `Signed-off-by` preserved.
- [ ] PR opened with **base = `release/vX.Y.0`**; merged with the **GitHub merge button**;
      staging branch deleted.
- [ ] Version bump commits added; submodule bumps list included upstream commits.
- [ ] `vX.Y.Z-rcN` tag pushed and validated, then `vX.Y.Z` GA tag pushed on the release branch.
- [ ] Feature work continues to target `main`, not the release branch.
