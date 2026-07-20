#!/usr/bin/env bash
# check-commits.sh — enforce OPENWIFI_PR_GUIDELINES.md / OPENWIFI_CODING_GUIDELINES.md
# commit rules on the range $BASE_SHA..$HEAD_SHA.
#
# HARD FAILURES (exit 1):
#   - merge commits in the PR branch          (A2 / A7)
#   - missing Signed-off-by on any commit     (A3 / DCO)
#   - subject not "subsystem: lower-case summary" or trailing period (A3, §2)
#
# WARNINGS (annotate, don't fail — "typically"/"where applicable" rules):
#   - more than 3 commits                     (A1: "typically 1-3")
#   - missing Fixes: WIFI-#### trailer        (§2: "where applicable")
#   - branch name not staging-WIFI-<num>-<desc> (A1; fork branches vary)
#   - subject line longer than 72 chars
#   - no explanatory body                     (§2; mechanical commits may skip)

set -euo pipefail

BASE_SHA="${BASE_SHA:?BASE_SHA not set}"
HEAD_SHA="${HEAD_SHA:?HEAD_SHA not set}"
HEAD_REF="${HEAD_REF:-}"

fail=0
range="${BASE_SHA}..${HEAD_SHA}"

err()  { echo "::error::$*";   fail=1; }
warn() { echo "::warning::$*"; }

# Shorten a subject for display in messages so a runaway subject line doesn't
# flood the annotations / PR comment. Reports the full length separately.
trunc() {
  local s="$1" max=80
  if [ "${#s}" -gt "$max" ]; then
    printf '%s…' "${s:0:max}"
  else
    printf '%s' "$s"
  fi
}

# ---------------------------------------------------------------
# 1. No merge commits — history must replay cleanly onto main
# ---------------------------------------------------------------
merges=$(git rev-list --merges "$range")
if [ -n "$merges" ]; then
  for m in $merges; do
    err "Merge commit in PR branch: $(git log -1 --format='%h %s' "$m"). Rebase onto origin/main instead of merging (guidelines A2)."
  done
fi

# ---------------------------------------------------------------
# 2. Commit count — typically 1-3 focused commits
# ---------------------------------------------------------------
count=$(git rev-list --count --no-merges "$range")
if [ "$count" -eq 0 ]; then
  err "PR contains no commits relative to base."
elif [ "$count" -gt 3 ]; then
  warn "PR has $count commits; guidelines target 1-3 focused, standalone commits (A1/A3). Consider squashing fixups."
fi

# ---------------------------------------------------------------
# 3. Branch naming — staging-WIFI-<number>-<short-hyphenated-description>
# ---------------------------------------------------------------
if [ -n "$HEAD_REF" ] && ! echo "$HEAD_REF" | grep -qE '^staging-(WIFI|WLAN)-[0-9]+-[a-zA-Z0-9.-]+$'; then
  warn "Branch '$HEAD_REF' does not match staging-WIFI-<number>-<short-hyphenated-description> (guidelines A1)."
fi

# ---------------------------------------------------------------
# 4. Per-commit checks: subject format, trailers
# ---------------------------------------------------------------
# Canonical subject:  subsystem: imperative lower-case summary
#   - one or more "prefix: " components (may stack: "qca-wifi-7: wifi-scripts: fix ...")
#   - prefix chars: letters, digits, . _ / -  (covers config.yml:, patches-25.12:, qca-wifi-7/wifi-scripts:)
#   - first char of the summary is lower-case or a digit
#   - no trailing period
# Reverts are exempt from the subject regex (they use git's standard format).
SUBJECT_RE='^([A-Za-z0-9._/-]+: )+[a-z0-9].*[^.]$'

for sha in $(git rev-list --reverse --no-merges "$range"); do
  short=$(git log -1 --format='%h' "$sha")
  subject=$(git log -1 --format='%s' "$sha")
  body=$(git log -1 --format='%B' "$sha")

  # --- subject ---
  if echo "$subject" | grep -qE '^Revert "'; then
    : # standard revert format is fine; reason-in-body is reviewed by humans
  elif ! echo "$subject" | grep -qE "$SUBJECT_RE"; then
    err "$short: subject '$(trunc "$subject")' is not canonical. Use 'subsystem: imperative lower-case summary', no trailing period (e.g. 'ipq50xx: add SonicFi RAP630E')."
  fi

  if [ "${#subject}" -gt 72 ]; then
    warn "$short: subject is ${#subject} chars; keep it concise (~72 max)."
  fi

  # --- Signed-off-by (DCO) — required on every commit ---
  if ! echo "$body" | grep -qE '^Signed-off-by: .+ <.+@.+>'; then
    err "$short: missing 'Signed-off-by: Name <email>' trailer (DCO, guidelines A3). Use 'git commit -s'."
  fi

  # --- Fixes: trailer — expected 'where applicable'; must be capital-F 'Fixes:' ---
  if echo "$body" | grep -qE '^Fixes: (WIFI|WLAN)-[0-9]+'; then
    :  # correct: capital-F 'Fixes:' with a valid ticket
  elif echo "$body" | grep -qiE '^fixes:' && ! echo "$body" | grep -qE '^Fixes:'; then
    warn "$short: the trailer must be a capital-F 'Fixes: WIFI-#####' (found a different case) (guidelines A3)."
  else
    warn "$short: no 'Fixes: WIFI-#####' trailer. Add one if this commit relates to a ticket (guidelines A3)."
  fi

  # --- Explanatory body — required by §2, but mechanical commits may skip ---
  # "Body prose" = the message minus the subject, blank lines, and known
  # trailers. If nothing is left, the commit has only a subject (+ trailers).
  body_prose=$(git log -1 --format='%b' "$sha" \
    | grep -vE '^[[:space:]]*$' \
    | grep -viE '^(Signed-off-by|Fixes|Tested-by|Reviewed-by|Acked-by|Reported-by|Suggested-by|Co-developed-by|Co-authored-by|Cc|Link|Closes|Depends-on|Change-Id):' \
    || true)
  if [ -z "$body_prose" ]; then
    warn "$short: no explanatory body — §2 expects why, then what. Mechanical commits (bumps, renames, reverts) may ignore this."
  fi
done

# ---------------------------------------------------------------
# Result
# ---------------------------------------------------------------
if [ "$fail" -ne 0 ]; then
  echo ""
  echo "Commit checks FAILED. See OPENWIFI_PR_GUIDELINES.md (A2/A3) and"
  echo "OPENWIFI_CODING_GUIDELINES.md (§2). Fix with 'git rebase -i origin/main'"
  echo "and re-push with 'git push --force-with-lease'."
  exit 1
fi

echo "All commit checks passed ($count commit(s))."
