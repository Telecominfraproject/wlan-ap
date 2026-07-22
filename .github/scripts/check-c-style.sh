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

set -euo pipefail

BASE_SHA="${BASE_SHA:?BASE_SHA not set}"
HEAD_SHA="${HEAD_SHA:?HEAD_SHA not set}"

fail=0
err() { echo "::error::$*"; fail=1; }

# Changed C files in the PR
mapfile -t changed < <(git diff --name-only --diff-filter=ACMR "$BASE_SHA" "$HEAD_SHA" -- '*.c' '*.h')

if [ "${#changed[@]}" -eq 0 ]; then
  echo "No C files changed; skipping."
  exit 0
fi

# --- SPDX header required on NEW files ---
mapfile -t added < <(git diff --name-only --diff-filter=A "$BASE_SHA" "$HEAD_SHA" -- '*.c' '*.h')
for f in "${added[@]:-}"; do
  [ -z "$f" ] && continue
  if ! head -n1 "$f" | grep -q 'SPDX-License-Identifier:'; then
    err "$f: new C file must start with '/* SPDX-License-Identifier: BSD-3-Clause */' (coding guidelines §3)."
  fi
done

# --- Space-indentation in ADDED lines ---
# Walk the unified diff; flag '+' lines that start with a space used as
# indentation, excluding block-comment continuations (' *').
for f in "${changed[@]}"; do
  bad=$(git diff --unified=0 "$BASE_SHA" "$HEAD_SHA" -- "$f" \
        | grep -nE '^\+ +[^ *]' || true)
  if [ -n "$bad" ]; then
    err "$f: added lines are indented with spaces; use tabs (coding guidelines §3). Offending diff lines:"
    echo "$bad" | head -n 10
  fi
done

if [ "$fail" -ne 0 ]; then
  exit 1
fi
echo "C style checks passed on ${#changed[@]} changed file(s)."
