#!/usr/bin/env bash
# scripts/refs-setup.sh — Reference Materials Setup
# --------------------------------------------------
# Clone, update, and manage external reference repositories defined in
# refs/refs.yaml.  Called directly, or via the /refs-download skill.
#
# Usage:
#   ./scripts/refs-setup.sh                    # Clone all (sparse for KB repos)
#   ./scripts/refs-setup.sh --full             # Clone all with full content
#   ./scripts/refs-setup.sh update [<name>]    # Pull latest
#   ./scripts/refs-setup.sh status             # Show clone status
#   ./scripts/refs-setup.sh clean              # Remove all cloned content
#   ./scripts/refs-setup.sh <name>             # Clone/update a single source
#   ./scripts/refs-setup.sh <name> --full      # Single source, full mode

set -euo pipefail

# ---------------------------------------------------------------------------
# Paths
# ---------------------------------------------------------------------------
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
REFS_DIR="${REPO_ROOT}/refs"
MANIFEST="${REFS_DIR}/refs.yaml"

# ---------------------------------------------------------------------------
# Colour helpers
# ---------------------------------------------------------------------------
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
RESET='\033[0m'

info()    { echo -e "${CYAN}[refs-setup]${RESET} $*"; }
ok()      { echo -e "${GREEN}[OK]${RESET} $*"; }
warn()    { echo -e "${YELLOW}[WARN]${RESET} $*"; }
err()     { echo -e "${RED}[ERROR]${RESET} $*" >&2; }

# ---------------------------------------------------------------------------
# Dependency checks
# ---------------------------------------------------------------------------
check_deps() {
    local missing=()
    command -v git  >/dev/null 2>&1 || missing+=("git")
    command -v python3 >/dev/null 2>&1 || missing+=("python3")
    if [[ ${#missing[@]} -gt 0 ]]; then
        err "Missing required tools: ${missing[*]}"
        exit 1
    fi
}

# ---------------------------------------------------------------------------
# YAML parser (minimal — reads refs.yaml source list)
# ---------------------------------------------------------------------------
# parse_manifest <field> prints all values of <field> across sources[]
parse_manifest_names() {
    python3 - "$MANIFEST" <<'EOF'
import sys, re
lines = open(sys.argv[1]).readlines()
for line in lines:
    m = re.match(r'\s+-\s+name:\s+(\S+)', line)
    if m:
        print(m.group(1))
EOF
}

# get_field <name> <field>
get_field() {
    local name="$1" field="$2"
    python3 - "$MANIFEST" "$name" "$field" <<'EOF'
import sys, re
manifest, name, field = sys.argv[1], sys.argv[2], sys.argv[3]
lines = open(manifest).readlines()
in_source = False
for line in lines:
    if re.match(r'\s+-\s+name:\s+' + re.escape(name) + r'\s*$', line):
        in_source = True
    elif in_source:
        m = re.match(r'\s+' + re.escape(field) + r':\s+(.*)', line)
        if m:
            print(m.group(1).strip().strip('"').strip("'"))
            break
        elif re.match(r'\s+-\s+name:', line):
            break
EOF
}

# ---------------------------------------------------------------------------
# Clone helpers
# ---------------------------------------------------------------------------
clone_sparse() {
    local name="$1" uri="$2" dest="${REFS_DIR}/${name}"
    info "Cloning ${name} (sparse: md/ only)..."
    git clone --filter=blob:none --no-checkout --sparse "${uri}" "${dest}"
    cd "${dest}"
    git sparse-checkout set md/
    git checkout
    ok "Cloned ${name} (sparse)"
}

clone_full() {
    local name="$1" uri="$2" dest="${REFS_DIR}/${name}"
    info "Cloning ${name} (full)..."
    git clone "${uri}" "${dest}"
    ok "Cloned ${name} (full)"
}

update_source() {
    local name="$1" dest="${REFS_DIR}/${name}"
    if [[ ! -d "${dest}/.git" ]]; then
        warn "${name}: not cloned yet — skipping update (run without 'update' to clone)"
        return
    fi
    info "Updating ${name}..."
    git -C "${dest}" pull --ff-only
    ok "Updated ${name}"
}

# ---------------------------------------------------------------------------
# Commands
# ---------------------------------------------------------------------------
cmd_clone() {
    local full_mode="$1"      # true/false
    local target_name="$2"    # "" = all sources

    [[ -f "$MANIFEST" ]] || { err "Manifest not found: $MANIFEST"; exit 1; }

    mapfile -t names < <(parse_manifest_names)
    for name in "${names[@]}"; do
        [[ -z "$target_name" || "$target_name" == "$name" ]] || continue
        local uri dest sparse
        uri="$(get_field "$name" "uri")"
        sparse="$(get_field "$name" "sparse")"
        dest="${REFS_DIR}/${name}"

        if [[ -d "${dest}/.git" ]]; then
            info "${name}: already cloned at ${dest} — skipping (use 'update' to refresh)"
            continue
        fi

        if [[ "$full_mode" == "true" ]]; then
            clone_full "$name" "$uri"
        elif [[ "$sparse" == "true" ]]; then
            clone_sparse "$name" "$uri"
        else
            clone_full "$name" "$uri"
        fi
    done
}

cmd_update() {
    local target_name="$1"   # "" = all sources
    [[ -f "$MANIFEST" ]] || { err "Manifest not found: $MANIFEST"; exit 1; }
    mapfile -t names < <(parse_manifest_names)
    for name in "${names[@]}"; do
        [[ -z "$target_name" || "$target_name" == "$name" ]] || continue
        update_source "$name"
    done
}

cmd_status() {
    [[ -f "$MANIFEST" ]] || { err "Manifest not found: $MANIFEST"; exit 1; }
    mapfile -t names < <(parse_manifest_names)
    printf "%-36s %-10s %-12s %s\n" "SOURCE" "STATUS" "BRANCH" "DISK"
    printf "%-36s %-10s %-12s %s\n" "------" "------" "------" "----"
    for name in "${names[@]}"; do
        local dest="${REFS_DIR}/${name}"
        if [[ -d "${dest}/.git" ]]; then
            local branch disk
            branch="$(git -C "$dest" rev-parse --abbrev-ref HEAD 2>/dev/null || echo '?')"
            disk="$(du -sh "$dest" 2>/dev/null | cut -f1 || echo '?')"
            printf "%-36s %-10s %-12s %s\n" "$name" "cloned" "$branch" "$disk"
        else
            printf "%-36s %-10s %-12s %s\n" "$name" "missing" "-" "-"
        fi
    done
}

cmd_clean() {
    [[ -f "$MANIFEST" ]] || { err "Manifest not found: $MANIFEST"; exit 1; }
    mapfile -t names < <(parse_manifest_names)
    for name in "${names[@]}"; do
        local dest="${REFS_DIR}/${name}"
        if [[ -d "$dest" ]]; then
            info "Removing ${dest}..."
            rm -rf "$dest"
            ok "Removed ${name}"
        fi
    done
}

# ---------------------------------------------------------------------------
# Argument parsing
# ---------------------------------------------------------------------------
check_deps

FULL_MODE=false
COMMAND=""
TARGET=""

case "${1:-}" in
    "")
        COMMAND="clone"
        ;;
    --full)
        COMMAND="clone"
        FULL_MODE=true
        ;;
    update)
        COMMAND="update"
        TARGET="${2:-}"
        ;;
    status)
        COMMAND="status"
        ;;
    clean)
        COMMAND="clean"
        ;;
    *)
        # Treat first arg as a source name
        TARGET="$1"
        COMMAND="clone"
        if [[ "${2:-}" == "--full" ]]; then
            FULL_MODE=true
        fi
        ;;
esac

case "$COMMAND" in
    clone)   cmd_clone  "$FULL_MODE" "$TARGET" ;;
    update)  cmd_update "$TARGET" ;;
    status)  cmd_status ;;
    clean)   cmd_clean ;;
esac
