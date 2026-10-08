#!/usr/bin/env bash
# check-c-style.sh — enforce OPENWIFI_CODING_GUIDELINES.md §3 on changed C files.
#
# Checks (on .c/.h files touched by the PR only):
#   - NEW files must start with an SPDX-License-Identifier line   (hard fail)
#   - ADDED lines must not use leading spaces for indentation      (hard fail)
#     (space-then-tab is also flagged; a single leading space is
#      allowed for continuation of block comments ' * ...')
#
# This intentionally does NOT lint pre-existing code — only what the
# PR adds — so legacy files don't block contributors.
#
# Findings are emitted as file/line annotations so they render inline on the PR
# diff, and each message repeats "path:line" in its text because the workflow
# strips annotation properties when folding these lines into the PR comment.

set -euo pipefail

BASE_SHA="${BASE_SHA:?BASE_SHA not set}"
HEAD_SHA="${HEAD_SHA:?HEAD_SHA not set}"

# How many offending lines to report per file before collapsing the rest.
MAX_REPORT=10

fail=0
err() { echo "::error::$*"; fail=1; }

# err_at <file> <line> <message…> — annotation anchored to a line of the diff.
err_at() {
  local f="$1" l="$2"
  shift 2
  echo "::error file=$f,line=$l::$*"
  fail=1
}

# Keep a quoted source line short so one runaway line can't flood the comment.
trunc() {
  local s="$1" max=80
  if [ "${#s}" -gt "$max" ]; then
    printf '%s…' "${s:0:max}"
  else
    printf '%s' "$s"
  fi
}

# Three-dot range = diff against the merge base. With the two-dot form, a PR
# whose base branch has moved on would also be judged on commits it never made
# (base.sha is the base-branch tip at event time, not the branch point).
RANGE="${BASE_SHA}...${HEAD_SHA}"

# Changed C files in the PR
mapfile -t changed < <(git diff --name-only --diff-filter=ACMR "$RANGE" -- '*.c' '*.h')

if [ "${#changed[@]}" -eq 0 ]; then
  echo "No C files changed; skipping."
  exit 0
fi

# --- SPDX header required on NEW files ---
mapfile -t added < <(git diff --name-only --diff-filter=A "$RANGE" -- '*.c' '*.h')
for f in "${added[@]:-}"; do
  [ -z "$f" ] && continue
  if ! head -n1 "$f" | grep -q 'SPDX-License-Identifier:'; then
    err_at "$f" 1 "$f:1: new C file must start with \`/* SPDX-License-Identifier: BSD-3-Clause */\` (coding guidelines §3)."
  fi
done

# --- Space-indentation in ADDED lines ---
# Walk the unified diff and flag '+' lines that start with a space used as
# indentation, excluding block-comment continuations (' *'). The @@ header
# carries the first new-file line of each hunk, so each offender is reported at
# its real location in the file rather than at an offset into the diff text.
for f in "${changed[@]}"; do
  offenders="$(git diff --unified=0 "$RANGE" -- "$f" | awk '
    /^\+\+\+/    { next }
    /^@@/        { match($0, /\+[0-9]+/)
                   line = substr($0, RSTART + 1, RLENGTH - 1)
                   next }
    /^\+/        { text = substr($0, 2)
                   if (text ~ /^ +[^ *]/) printf "%d\t%s\n", line, text
                   line++ }
  ')"

  [ -z "$offenders" ] && continue

  n=0
  while IFS=$'\t' read -r line text; do
    n=$((n + 1))
    [ "$n" -gt "$MAX_REPORT" ] && continue
    err_at "$f" "$line" "$f:$line: indented with spaces; use tabs (coding guidelines §3): \`$(trunc "$text")\`"
  done <<< "$offenders"

  if [ "$n" -gt "$MAX_REPORT" ]; then
    err_at "$f" 1 "$f: $((n - MAX_REPORT)) further space-indented line(s) not listed — re-indent the file with tabs (coding guidelines §3)."
  fi
done

if [ "$fail" -ne 0 ]; then
  exit 1
fi
echo "C style checks passed on ${#changed[@]} changed file(s)."
