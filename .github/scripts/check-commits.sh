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
#   - summary starting with an acronym/identifier instead of a verb (A3)
#   - no explanatory body                     (§2; mechanical commits may skip)
#
# Messages are single-line and quote code in backticks: the workflow folds every
# ::error::/::warning:: line into a markdown bullet in the sticky PR comment, so
# placeholders like <number> must be fenced or GitHub eats them as HTML.

set -euo pipefail

BASE_SHA="${BASE_SHA:?BASE_SHA not set}"
HEAD_SHA="${HEAD_SHA:?HEAD_SHA not set}"
HEAD_REF="${HEAD_REF:-}"

fail=0

err()  { echo "::error::$*";   fail=1; }
warn() { echo "::warning::$*"; }

# github.event.pull_request.base.sha is the base-branch tip at event time, not
# the branch point. Once main moves on, BASE..HEAD also contains commits the PR
# never touched — which shows up as an inflated commit count and, worse, as
# "merge commit in PR branch" errors for main's own release merges. Diff from
# the merge base so the range is exactly the PR's own commits.
merge_base="$(git merge-base "$BASE_SHA" "$HEAD_SHA" 2>/dev/null || true)"
if [ -z "$merge_base" ]; then
  warn "Could not compute the merge base of $BASE_SHA and $HEAD_SHA; falling back to the raw base, so the commit range may include unrelated commits."
  merge_base="$BASE_SHA"
fi
range="${merge_base}..${HEAD_SHA}"

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
if [ -n "$HEAD_REF" ] && ! echo "$HEAD_REF" | grep -qE '^staging-WIFI-[0-9]+-[a-zA-Z0-9.-]+$'; then
  # Suggest a concrete name from the ticket the branch/commits already mention.
  branch_ticket="$(printf '%s' "$HEAD_REF" | grep -oE 'WIFI-[0-9]+' | head -n1 || true)"
  if [ -n "$branch_ticket" ]; then
    warn "Branch \`$HEAD_REF\` is missing the \`-<short-hyphenated-description>\` suffix; expected \`staging-$branch_ticket-<short-hyphenated-description>\`, e.g. \`staging-$branch_ticket-hostapd-fix-ttlm-netlink-leak\` (guidelines A1)."
  else
    warn "Branch \`$HEAD_REF\` does not match \`staging-WIFI-<number>-<short-hyphenated-description>\`, e.g. \`staging-WIFI-14728-ipq50xx-add-sonicfi-rap630e\` (guidelines A1)."
  fi
fi

# ---------------------------------------------------------------
# 4. Per-commit checks: subject format, trailers
# ---------------------------------------------------------------
# Canonical subject:  subsystem: imperative lower-case summary
#   - one or more "prefix: " components (may stack: "qca-wifi-7: wifi-scripts: fix ...")
#   - prefix chars: letters, digits, . _ / -  (covers config.yml:, patches-25.12:, qca-wifi-7/wifi-scripts:)
#   - first char of the summary is lower-case or a digit
#   - no trailing period
# Reverts are exempt (they use git's standard format).
#
# Rather than matching one all-or-nothing regex, the subject is split into its
# "<prefix>: " chain and its summary so every violation can name itself: a
# rejected subject should say *what* is wrong ("hostapd / nl80211:" has spaces),
# not just "not canonical".
PREFIX_RE='^[A-Za-z0-9._/-]+$'
TICKET_RE='^WIFI-[0-9]+$'

# Split "$1" into prefix_chain + summary, consuming as many valid
# "<subsystem>: " components as possible from the front. bad_prefix holds the
# first component that is not a valid subsystem token, so a subject with no
# usable prefix at all can be diagnosed precisely.
split_subject() {
  local s="$1" part
  prefix_chain=""
  bad_prefix=""
  while [[ "$s" == *": "* ]]; do
    part="${s%%: *}"
    if [[ ! "$part" =~ $PREFIX_RE ]]; then
      bad_prefix="$part"
      break
    fi
    prefix_chain+="$part: "
    s="${s#*: }"
  done
  summary="$s"
}

# Best-effort "did you mean" for a malformed prefix: drop the stray whitespace
# around separators that causes almost every rejection ("hostapd / nl80211",
# "ipq50xx "). Prints nothing when the result is still not a valid token.
suggest_prefix() {
  local p
  p="$(printf '%s' "$1" | sed -E 's/[[:space:]]*([/._-])[[:space:]]*/\1/g; s/^[[:space:]]+//; s/[[:space:]]+$//')"
  if [[ "$p" =~ $PREFIX_RE ]]; then
    printf '%s' "$p"
  fi
  return 0
}

for sha in $(git rev-list --reverse --no-merges "$range"); do
  short=$(git log -1 --format='%h' "$sha")
  subject=$(git log -1 --format='%s' "$sha")
  body=$(git log -1 --format='%B' "$sha")

  echo "Checking $short: $subject"

  # --- subject ---
  if echo "$subject" | grep -qE '^Revert "'; then
    : # standard revert format is fine; reason-in-body is reviewed by humans
  else
    split_subject "$subject"
    first_prefix="${prefix_chain%%: *}"

    if [ -z "$prefix_chain" ]; then
      suggestion="$(suggest_prefix "$bad_prefix")"
      if [[ "$subject" =~ ^[A-Za-z0-9._/-]+:[[:space:]]*$ ]]; then
        err "$short: subject \`$subject\` is only a subsystem prefix with no summary. Add an imperative summary, e.g. \`${subject%%:*}: add SonicFi RAP630E\` (guidelines A3)."
      elif [[ "$subject" =~ ^[A-Za-z0-9._/-]+:[^[:space:]] ]]; then
        err "$short: subject \`$(trunc "$subject")\` needs a space after the subsystem colon: \`${subject%%:*}: ${subject#*:}\` (guidelines A3)."
      elif [ -z "$bad_prefix" ]; then
        err "$short: subject \`$(trunc "$subject")\` has no subsystem prefix. Use \`subsystem: imperative summary\`, e.g. \`ipq50xx: add SonicFi RAP630E\` (guidelines A3)."
      elif [ -n "$suggestion" ]; then
        err "$short: subsystem prefix \`$bad_prefix:\` contains spaces — use \`$suggestion:\` instead, so the subject reads \`$suggestion: $(trunc "${subject#*: }")\` (guidelines A3)."
      elif [[ "${bad_prefix%% *}" =~ $TICKET_RE ]]; then
        err "$short: subject starts with the ticket \`${bad_prefix%% *}\`. The ticket belongs in a \`Fixes:\` trailer, not the subject: \`ipq50xx: add SonicFi RAP630E\`, not \`WIFI-14728 ipq50xx : add sonicfi rap630e\` (guidelines A3)."
      else
        err "$short: invalid subsystem prefix \`$bad_prefix:\` — only letters, digits and \`. _ / -\` are allowed, e.g. \`hostapd/nl80211:\`, \`patches-25.12:\`, \`qca-wifi-7: wifi-scripts:\` (guidelines A3)."
      fi
    elif [[ "$first_prefix" =~ $TICKET_RE ]]; then
      err "$short: subsystem prefix is the ticket \`$first_prefix\`. Name the subsystem instead and keep the ticket in a \`Fixes:\` trailer, e.g. \`ipq50xx: add SonicFi RAP630E\` (guidelines A3)."
    elif [ -z "$summary" ]; then
      err "$short: subject is only the prefix \`$prefix_chain\` with no summary. Add an imperative summary, e.g. \`${first_prefix}: add SonicFi RAP630E\` (guidelines A3)."
    elif [[ "$summary" =~ ^[A-Z][a-z] ]]; then
      first_word="${summary%% *}"
      err "$short: summary must start lower-case — \`${first_word}\` should be \`${first_word,}\`, so the subject reads \`${prefix_chain}${summary,}\` (guidelines A3)."
    elif [[ ! "$summary" =~ ^[A-Za-z0-9] ]]; then
      err "$short: summary \`$(trunc "$summary")\` must start with a letter or digit, not punctuation (guidelines A3)."
    elif [[ "$summary" =~ ^[A-Z] ]]; then
      # All-caps / mixed-case opener: acronyms and board names legitimately keep
      # their case, but the first word is meant to be an imperative verb.
      warn "$short: summary starts with \`${summary%% *}\`; the first word after the colon should be an imperative verb (proper nouns and board names keep their case later in the line) (guidelines A3)."
    fi

    case "$subject" in
      *.) err "$short: subject \`$(trunc "$subject")\` must not end with a period (guidelines A3)." ;;
    esac
  fi

  if [ "${#subject}" -gt 72 ]; then
    warn "$short: subject is ${#subject} chars; keep it concise (~72 max)."
  fi

  # --- Signed-off-by (DCO) — required on every commit ---
  if ! echo "$body" | grep -qE '^Signed-off-by: .+ <.+@.+>'; then
    err "$short: missing 'Signed-off-by: Name <email>' trailer (DCO, guidelines A3). Use 'git commit -s'."
  fi

  # --- Fixes: trailer — expected 'where applicable'; must be 'Fixes: WIFI-<n>' ---
  # A trailer that exists but is malformed must not be reported as "missing":
  # quote the offending line and name the part that is wrong.
  #
  # Only the last paragraph is scanned, because that is where git looks for
  # trailers — searching the whole message would misread wrapped body prose that
  # happens to start a line with "Fixes:".
  trailer_block="$(printf '%s\n' "$body" | awk 'BEGIN { RS = ""; FS = "\n" } { block = $0 } END { print block }')"
  fixes_line="$(echo "$trailer_block" | grep -iE '^[[:space:]]*fixes:' | head -n1 || true)"
  if echo "$trailer_block" | grep -qE '^Fixes: WIFI-[0-9]+'; then
    :  # correct: capital-F 'Fixes:' with a valid ticket
  elif [ -z "$fixes_line" ]; then
    warn "$short: no \`Fixes: WIFI-#####\` trailer. Add one if this commit relates to a ticket (guidelines A3)."
  elif ! echo "$fixes_line" | grep -qE '^Fixes:'; then
    warn "$short: trailer \`$(trunc "$fixes_line")\` must be spelled \`Fixes:\` — capital F, no leading whitespace (guidelines A3)."
  else
    warn "$short: trailer \`$(trunc "$fixes_line")\` must reference the ticket as \`Fixes: WIFI-<number>\`, e.g. \`Fixes: WIFI-15706\` — \`WIFI-\` is the only ticket prefix in use (§2)."
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
