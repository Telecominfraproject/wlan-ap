#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"
AEI_DIR="${GW3_AEI_DIR:-$PROJECT_ROOT/aei}"
SRC_DIR="$AEI_DIR/src"
FEEDS_CONF="$PROJECT_ROOT/feeds.conf"
STATE_FILE="$AEI_DIR/.srclocal.state"
SRC_STATE_FILE="$AEI_DIR/.srclocal-source.state"

# GW3-owned source repo URL patterns (for --deep filtering).
# Only repos matching one of these patterns are cloned during --deep setup.
# Third-party upstream repos (freedesktop.org, github.com/BroadbandForum, etc.)
# are excluded — they are read-only dependencies, not feature-development targets.
# Override: GW3_OWNED_URL_PATTERNS="pattern1|pattern2" or use --source to clone explicitly.
GW3_OWNED_URL_PATTERNS="${GW3_OWNED_URL_PATTERNS:-bitbucket\.org[:/]Actiontec/}"

die()  { echo "Error: $*" >&2; exit 1; }
info() { echo "==> $*"; }
warn() { echo "Warning: $*" >&2; }

# --- Check if a URL belongs to a GW3-owned repo ---
is_owned_url() {
    local url="$1"
    [[ "$url" =~ $GW3_OWNED_URL_PATTERNS ]]
}

# --- Parse a feeds.conf line: sets FC_TYPE, FC_NAME, FC_URI, FC_BRANCH, FC_SHA1 ---
parse_feed_line() {
    FC_TYPE="" FC_NAME="" FC_URI="" FC_BRANCH="" FC_SHA1=""
    local line="$1"
    read -r FC_TYPE FC_NAME FC_SRC <<< "$line"
    [[ -n "$FC_TYPE" && -n "$FC_NAME" ]] || return 1
    [[ "$FC_TYPE" == "src-link" ]] && { FC_URI="$FC_SRC"; return 0; }
    if [[ "$FC_SRC" == *"^"* ]]; then
        FC_URI="${FC_SRC%%^*}"; FC_SHA1="${FC_SRC#*^}"
    elif [[ "$FC_SRC" == *";"* ]]; then
        FC_URI="${FC_SRC%%;*}"; FC_BRANCH="${FC_SRC#*;}"
    else
        FC_URI="$FC_SRC"
    fi
}

get_feed_line() { grep -E "^src-\w+\s+$1\s" "$FEEDS_CONF" 2>/dev/null || true; }

replace_feed_line() {
    local name="$1" new="$2"
    sed -i "s|^src-[a-z_]*[[:space:]]\+${name}[[:space:]]\+.*|${new}|" "$FEEDS_CONF"
}

save_state() {
    mkdir -p "$AEI_DIR"
    [[ -f "$STATE_FILE" ]] && sed -i "/^$1=/d" "$STATE_FILE"
    echo "$1=$2" >> "$STATE_FILE"
}

remove_state() {
    [[ -f "$STATE_FILE" ]] || return 0
    sed -i "/^$1=/d" "$STATE_FILE"
    [[ -s "$STATE_FILE" ]] || rm -f "$STATE_FILE"
}

save_src_state() {
    mkdir -p "$SRC_DIR"
    [[ -f "$SRC_STATE_FILE" ]] && sed -i "/^$1=/d" "$SRC_STATE_FILE"
    echo "$1=$2" >> "$SRC_STATE_FILE"
}

remove_src_state() {
    [[ -f "$SRC_STATE_FILE" ]] || return 0
    sed -i "/^$1=/d" "$SRC_STATE_FILE"
    [[ -s "$SRC_STATE_FILE" ]] || rm -f "$SRC_STATE_FILE"
}

# --- Push a new local branch to remote and set upstream tracking ---
push_new_branch() {
    local dir="$1" branch="$2"
    # Skip third-party repos — we don't have push access
    local url; url=$(git -C "$dir" remote get-url origin 2>/dev/null || true)
    if [[ -n "$url" ]] && ! is_owned_url "$url"; then
        return 0
    fi
    # Check if remote branch already exists
    if git -C "$dir" ls-remote --heads origin "$branch" 2>/dev/null | grep -q .; then
        # Remote exists, just set tracking
        git -C "$dir" branch --set-upstream-to="origin/$branch" "$branch" 2>/dev/null || true
        return 0
    fi
    info "Pushing new branch '$branch' to remote..."
    if git -C "$dir" push -u origin "$branch" 2>/dev/null; then
        info "Branch '$branch' pushed and tracking set"
    else
        warn "Could not push branch '$branch' to remote for $(basename "$dir"). Push manually or check credentials."
    fi
}

# --- Clone/checkout repo ---
# Args: name uri branch target_dir [base_commit]
# base_commit: when creating a new local branch, start from this commit
#              instead of remote HEAD (typically PKG_SOURCE_VERSION)
ensure_repo() {
    local name="$1" uri="$2" branch="$3" target_dir="$4" base_commit="${5:-}"
    [[ -n "$target_dir" ]] || target_dir="$AEI_DIR/$name"
    local created_local_branch=false
    if [[ -d "$target_dir/.git" ]]; then
        info "Repository exists: $target_dir"
        if [[ -n "$branch" ]]; then
            local cur; cur=$(git -C "$target_dir" branch --show-current 2>/dev/null || true)
            if [[ "$cur" != "$branch" ]]; then
                info "Switching to branch: $branch"
                git -C "$target_dir" fetch origin
                if git -C "$target_dir" checkout "$branch" 2>/dev/null; then
                    true  # existing local branch
                elif git -C "$target_dir" checkout -b "$branch" "origin/$branch" 2>/dev/null; then
                    true  # tracking remote branch
                elif [[ -n "$base_commit" ]] && git -C "$target_dir" cat-file -e "$base_commit" 2>/dev/null; then
                    info "Creating branch '$branch' from PKG_SOURCE_VERSION $base_commit"
                    git -C "$target_dir" checkout -b "$branch" "$base_commit"
                    created_local_branch=true
                else
                    git -C "$target_dir" checkout -b "$branch"
                    created_local_branch=true
                fi
            fi
        fi
    else
        info "Cloning $uri → $target_dir"
        mkdir -p "$(dirname "$target_dir")"
        if [[ -n "$branch" ]]; then
            git clone --branch "$branch" "$uri" "$target_dir" 2>/dev/null || {
                # Branch may not exist remotely yet — clone default and create
                git clone "$uri" "$target_dir"
                if [[ -n "$base_commit" ]] && git -C "$target_dir" cat-file -e "$base_commit" 2>/dev/null; then
                    # Create feature branch from PKG_SOURCE_VERSION commit
                    info "Creating branch '$branch' from PKG_SOURCE_VERSION $base_commit"
                    git -C "$target_dir" checkout -b "$branch" "$base_commit"
                else
                    # base_commit not available — fall back to remote HEAD
                    # (commit may be on a non-default branch not fetched yet)
                    if [[ -n "$base_commit" ]]; then
                        warn "PKG_SOURCE_VERSION $base_commit not found on default branch for $name; fetching all remotes..."
                        git -C "$target_dir" fetch --all 2>/dev/null
                        if git -C "$target_dir" cat-file -e "$base_commit" 2>/dev/null; then
                            info "Creating branch '$branch' from PKG_SOURCE_VERSION $base_commit"
                            git -C "$target_dir" checkout -b "$branch" "$base_commit"
                        else
                            warn "PKG_SOURCE_VERSION $base_commit still not found for $name; branching from remote HEAD"
                            git -C "$target_dir" checkout -b "$branch"
                        fi
                    else
                        git -C "$target_dir" checkout -b "$branch"
                    fi
                fi
                created_local_branch=true
            }
        else
            git clone "$uri" "$target_dir"
        fi
    fi

    # Push new local-only branches to remote to prevent data loss
    if $created_local_branch && [[ -n "$branch" ]]; then
        push_new_branch "$target_dir" "$branch"
    fi
}

# --- Reindex one feed: remove dir, feeds update, feeds install ---
reindex_feed() {
    local name="$1"
    rm -rf "$PROJECT_ROOT/feeds/$name"
    ( cd "$PROJECT_ROOT" && ./scripts/feeds update "$name" )
    ( cd "$PROJECT_ROOT" && ./scripts/feeds install -a -f -p "$name" )
}

# --- Setup src-link for one feed ---
setup_feed() {
    local name="$1" branch_override="$2"
    local line; line=$(get_feed_line "$name")
    [[ -n "$line" ]] || die "Feed '$name' not found in feeds.conf"

    # Already src-linked?
    if [[ "$line" == src-link* ]]; then
        info "Feed '$name' already src-linked, skipping"
        return 0
    fi

    parse_feed_line "$line" || die "Cannot parse feeds.conf line for '$name'"
    local uri="$FC_URI" branch="${branch_override:-$FC_BRANCH}"

    [[ -n "$FC_SHA1" && -z "$branch" ]] && \
        die "Feed '$name' is commit-pinned. Specify branch: $name:<branch>"

    ensure_repo "$name" "$uri" "$branch"
    save_state "$name" "$line"

    local new_line="src-link $name $AEI_DIR/$name"
    replace_feed_line "$name" "$new_line"
    info "feeds.conf: $new_line"

    reindex_feed "$name"
    info "Done: $name → $AEI_DIR/$name"
}

# ============================================================
# Module Tier: Module checkout
# ============================================================

# --- Find package Makefile and extract PKG_SOURCE_URL ---
find_pkg_makefile() {
    local pkg="$1" makefile=""
    # Search feeds first (most common) — feeds may have nested subdirs (apps/, libs/, etc.)
    makefile=$(find "$PROJECT_ROOT"/feeds -maxdepth 5 -path "*/$pkg/Makefile" -not -path '*/.git/*' 2>/dev/null | head -1)
    # Fallback to base packages
    if [[ -z "$makefile" ]]; then
        makefile=$(find "$PROJECT_ROOT"/package -maxdepth 5 -path "*/$pkg/Makefile" -not -path '*/.git/*' 2>/dev/null | head -1)
    fi
    # Also check src-linked aei/ directories
    if [[ -z "$makefile" ]]; then
        makefile=$(find "$AEI_DIR" -maxdepth 5 -path "*/$pkg/Makefile" -not -path '*/src/*' -not -path '*/.git/*' 2>/dev/null | head -1)
    fi
    echo "$makefile"
}

extract_pkg_var() {
    local makefile="$1" varname="$2"
    grep -E "^${varname}\s*[:?]?=" "$makefile" 2>/dev/null | head -1 | sed 's/^[^=]*=[[:space:]]*//; s/\r$//'
}

extract_pkg_url() {
    local makefile="$1"
    local url; url=$(extract_pkg_var "$makefile" "PKG_SOURCE_URL")
    local pkg_name; pkg_name=$(extract_pkg_var "$makefile" "PKG_NAME")
    # Resolve $(PKG_NAME) / ${PKG_NAME}
    url="${url/\$(PKG_NAME)/$pkg_name}"
    url="${url/\$\{PKG_NAME\}/$pkg_name}"
    # Strip trailing whitespace
    url="${url%"${url##*[![:space:]]}"}"
    echo "$url"
}

# --- Clone source for a package ---
setup_source() {
    local pkg="$1" branch="$2" explicit_url="$3"
    local url="$explicit_url"

    if [[ -z "$url" ]]; then
        local makefile; makefile=$(find_pkg_makefile "$pkg")
        [[ -n "$makefile" ]] || die "Cannot find Makefile for package '$pkg'. Use --url to specify."
        local proto; proto=$(extract_pkg_var "$makefile" "PKG_SOURCE_PROTO")
        [[ "$proto" == "git" ]] || die "Package '$pkg' uses PKG_SOURCE_PROTO=$proto (not git). Use --url for non-git sources."
        url=$(extract_pkg_url "$makefile")
        [[ -n "$url" ]] || die "No PKG_SOURCE_URL in $makefile. Use --url to specify."
    fi

    local target="$SRC_DIR/$pkg"

    if [[ -d "$target/.git" ]]; then
        info "Source repo exists: $target"
        if [[ -n "$branch" ]]; then
            local cur; cur=$(git -C "$target" branch --show-current 2>/dev/null || true)
            if [[ "$cur" != "$branch" ]]; then
                # Resolve PKG_SOURCE_VERSION for branch base
                local base_commit_existing=""
                local mf; mf=$(find_pkg_makefile "$pkg")
                if [[ -n "$mf" ]]; then
                    local ver; ver=$(extract_pkg_var "$mf" "PKG_SOURCE_VERSION")
                    if [[ -n "$ver" && "$ver" =~ ^[0-9a-f]{7,40}$ ]]; then
                        base_commit_existing="$ver"
                    fi
                fi
                info "Switching to branch: $branch"
                git -C "$target" fetch origin
                if git -C "$target" checkout "$branch" 2>/dev/null; then
                    true  # existing local branch
                elif git -C "$target" checkout -b "$branch" "origin/$branch" 2>/dev/null; then
                    true  # tracking remote branch
                elif [[ -n "$base_commit_existing" ]] && git -C "$target" cat-file -e "$base_commit_existing" 2>/dev/null; then
                    info "Creating branch '$branch' from PKG_SOURCE_VERSION $base_commit_existing"
                    git -C "$target" checkout -b "$branch" "$base_commit_existing"
                else
                    git -C "$target" checkout -b "$branch"
                fi
            fi
        fi
    else
        # Resolve PKG_SOURCE_VERSION as base commit for branch creation
        local base_commit=""
        local makefile; makefile=$(find_pkg_makefile "$pkg")
        if [[ -n "$makefile" ]]; then
            local version; version=$(extract_pkg_var "$makefile" "PKG_SOURCE_VERSION")
            if [[ -n "$version" && "$version" =~ ^[0-9a-f]{7,40}$ ]]; then
                base_commit="$version"
            fi
        fi

        # Determine initial checkout point
        local checkout_ref="$branch"
        if [[ -z "$branch" && -n "$base_commit" ]]; then
            checkout_ref=""  # clone default, then checkout hash below
        fi

        ensure_repo "$pkg" "$url" "$checkout_ref" "$target" "$base_commit"

        # If we have a commit hash and no branch, create a dev branch from it
        if [[ -z "$branch" && -n "$base_commit" ]]; then
            git -C "$target" checkout -b dev "$base_commit" 2>/dev/null || \
                warn "Could not create dev branch at $base_commit for $pkg"
        fi
    fi

    save_src_state "$pkg" "$url"
    info "Source ready: $pkg → $target"
}

# --- Deep setup: feed src-link + clone all package sources ---
do_deep() {
    local feeds=("$@")
    for feed_spec in "${feeds[@]}"; do
        local fname="${feed_spec%%:*}"
        local fbranch=""
        [[ "$feed_spec" == *:* ]] && fbranch="${feed_spec#*:}"

        # Feed tier: src-link the feed
        setup_feed "$fname" "$fbranch"

        # Module tier: scan all packages in the feed for PKG_SOURCE_URL
        local feed_dir="$AEI_DIR/$fname"
        [[ -d "$feed_dir" ]] || { warn "Feed dir not found: $feed_dir"; continue; }

        info "Deep scan: finding GW3-owned packages with PKG_SOURCE_URL in $fname..."
        local count=0 skipped=0
        while IFS= read -r makefile; do
            local pkg_dir; pkg_dir=$(dirname "$makefile")
            local pkg; pkg=$(basename "$pkg_dir")
            # Only clone git-based sources (skip tarball downloads)
            local proto; proto=$(extract_pkg_var "$makefile" "PKG_SOURCE_PROTO")
            [[ "$proto" == "git" ]] || continue
            local url; url=$(extract_pkg_url "$makefile")
            [[ -n "$url" ]] || continue
            # Skip third-party upstream repos — only clone GW3-owned sources
            if ! is_owned_url "$url"; then
                $VERBOSE_DEEP && info "  Skip (third-party): $pkg → $url"
                skipped=$((skipped + 1))
                continue
            fi
            info "  Found: $pkg → $url"
            setup_source "$pkg" "$fbranch" "$url"
            count=$((count + 1))
        done < <(find "$feed_dir" -maxdepth 5 -name Makefile -not -path '*/.git/*')
        info "Deep setup complete for $fname: $count owned repos cloned, $skipped third-party skipped"
    done
}

# ============================================================
# Lifecycle: Pinning
# ============================================================

do_pin_sources() {
    info "Pinning source commits into feed Makefiles..."
    [[ -d "$SRC_DIR" ]] || { info "No module checkouts in $SRC_DIR"; return 0; }

    local count=0
    for src_dir in "$SRC_DIR"/*/; do
        [[ -d "$src_dir/.git" ]] || continue
        local pkg; pkg=$(basename "$src_dir")

        # Check for uncommitted changes
        local dirty; dirty=$(git -C "$src_dir" status --porcelain 2>/dev/null | wc -l)
        [[ "$dirty" -eq 0 ]] || warn "$pkg has $dirty uncommitted changes"

        # Get HEAD commit
        local head; head=$(git -C "$src_dir" rev-parse HEAD 2>/dev/null)
        [[ -n "$head" ]] || { warn "Cannot get HEAD for $pkg"; continue; }

        # Find the package Makefile (prefer src-linked feed dirs in aei/)
        local makefile=""
        for d in "$AEI_DIR"/*/; do
            [[ -d "$d" && ! "$d" == */src/ ]] || continue
            [[ -f "$d$pkg/Makefile" ]] && { makefile="$d$pkg/Makefile"; break; }
        done
        [[ -n "$makefile" ]] || makefile=$(find_pkg_makefile "$pkg")
        [[ -n "$makefile" ]] || { warn "Cannot find Makefile for $pkg"; continue; }

        # Update PKG_SOURCE_VERSION
        if grep -qE '^PKG_SOURCE_VERSION\s*[:?]?=' "$makefile"; then
            sed -i "s|^\(PKG_SOURCE_VERSION\s*[:?]\?=\s*\).*|\1${head}|" "$makefile"
            info "Pinned $pkg → ${head:0:12} in $makefile"
            count=$((count + 1))
        else
            warn "$pkg: no PKG_SOURCE_VERSION in $makefile"
        fi

        # Set PKG_MIRROR_HASH to skip (needs recompute)
        if grep -qE '^PKG_MIRROR_HASH\s*[:?]?=' "$makefile"; then
            sed -i "s|^\(PKG_MIRROR_HASH\s*[:?]\?=\s*\).*|\1skip|" "$makefile"
        fi
        # Also reset PKG_HASH if present (will need recalculation)
        if grep -qE '^PKG_HASH\s*[:?]?=' "$makefile"; then
            sed -i "s|^\(PKG_HASH\s*[:?]\?=\s*\).*|\1skip|" "$makefile"
        fi
    done
    info "Pinned $count source commit(s)"
}

do_pin_feeds() {
    info "Pinning feed commits into profile YAMLs..."
    [[ -f "$STATE_FILE" ]] || { info "No src-linked feeds (no state file)"; return 0; }

    local count=0
    while IFS='=' read -r name _; do
        [[ -n "$name" ]] || continue
        local feed_dir="$AEI_DIR/$name"
        [[ -d "$feed_dir/.git" ]] || { warn "No git repo at $feed_dir"; continue; }

        # Check for uncommitted changes
        local dirty; dirty=$(git -C "$feed_dir" status --porcelain 2>/dev/null | wc -l)
        [[ "$dirty" -eq 0 ]] || warn "$name has $dirty uncommitted changes"

        # Get HEAD commit
        local head; head=$(git -C "$feed_dir" rev-parse HEAD 2>/dev/null)
        [[ -n "$head" ]] || { warn "Cannot get HEAD for $name"; continue; }

        # Find profile YAML(s) that define this feed
        local updated=0
        for yml in "$PROJECT_ROOT"/profiles/*.yml; do
            [[ -f "$yml" ]] || continue
            # Check if this YAML has a feed entry for this name
            if grep -qE "^\s*-?\s*name:\s*$name\s*$" "$yml"; then
                # Use Python for reliable YAML editing
                python3 -c "
import sys, re

name = '$name'
new_rev = '$head'
lines = open('$yml').readlines()
out = []
in_feed = False
for i, line in enumerate(lines):
    if re.match(r'\s*-?\s*name:\s*' + re.escape(name) + r'\s*$', line):
        in_feed = True
        out.append(line)
        continue
    if in_feed and re.match(r'\s*revision:\s*', line):
        indent = re.match(r'(\s*)', line).group(1)
        out.append(f'{indent}revision: {new_rev}\n')
        in_feed = False
        continue
    if in_feed and re.match(r'\s*-\s*name:', line):
        in_feed = False  # moved to next feed entry without finding revision
    out.append(line)

open('$yml', 'w').writelines(out)
" 2>/dev/null && {
                    info "Pinned $name → ${head:0:12} in $(basename "$yml")"
                    updated=1
                    count=$((count + 1))
                }
            fi
        done
        [[ "$updated" -eq 1 ]] || warn "No profile YAML found for feed $name"
    done < "$STATE_FILE"
    info "Pinned $count feed(s) in profile YAMLs"
}

do_pin() {
    do_pin_sources
    echo ""
    info "Source pins applied. Review and commit feed changes before running --pin-feeds."
    info "Then run: src_local.sh --pin-feeds"
    echo ""
    do_pin_feeds
}

# ============================================================
# Commands
# ============================================================

# --- Check if a repo is fully published (clean + pushed) ---
repo_is_published() {
    local dir="$1"
    [[ -d "$dir/.git" ]] || return 1
    # Must have clean working tree
    local dirty; dirty=$(git -C "$dir" status --porcelain 2>/dev/null | wc -l)
    [[ "$dirty" -eq 0 ]] || return 1
    # Must have upstream tracking
    git -C "$dir" rev-parse --abbrev-ref '@{u}' &>/dev/null || return 1
    # Must have zero unpushed commits
    local ahead; ahead=$(git -C "$dir" rev-list '@{u}..HEAD' --count 2>/dev/null || echo "1")
    [[ "$ahead" -eq 0 ]]
}

do_clean() {
    [[ -f "$STATE_FILE" ]] || { info "No src-links active (no state file)."; exit 0; }

    local restored=0 removed=0 preserved=0
    while IFS='=' read -r name original_line; do
        [[ -n "$name" && -n "$original_line" ]] || continue
        local current; current=$(get_feed_line "$name")

        # Only restore if currently src-linked
        if [[ "$current" == src-link* ]]; then
            info "Restoring: $name"
            replace_feed_line "$name" "$original_line"
            reindex_feed "$name"
            restored=$((restored + 1))
        else
            info "Feed '$name' already restored (not src-linked), skipping"
        fi

        # Remove directory if fully published; preserve if dirty/unpushed
        local feed_dir="$AEI_DIR/$name"
        if [[ -d "$feed_dir/.git" ]]; then
            if repo_is_published "$feed_dir"; then
                info "Removing $name (fully pushed, clean)"
                rm -rf "$feed_dir"
                removed=$((removed + 1))
            else
                warn "Preserving $name — has uncommitted changes or unpushed commits"
                preserved=$((preserved + 1))
            fi
        fi
    done < "$STATE_FILE"

    # Clean source repos tracked by this session
    if [[ -f "$SRC_STATE_FILE" ]]; then
        local src_removed=0 src_preserved=0
        while IFS='=' read -r pkg _; do
            [[ -n "$pkg" ]] || continue
            local src_dir="$SRC_DIR/$pkg"
            [[ -d "$src_dir/.git" ]] || continue
            if repo_is_published "$src_dir"; then
                info "Removing aei/src/$pkg (fully pushed, clean)"
                rm -rf "$src_dir"
                src_removed=$((src_removed + 1))
            else
                warn "Preserving aei/src/$pkg — has uncommitted changes or unpushed commits"
                src_preserved=$((src_preserved + 1))
            fi
        done < "$SRC_STATE_FILE"
        rm -f "$SRC_STATE_FILE"
        removed=$((removed + src_removed))
        preserved=$((preserved + src_preserved))
    fi

    rm -f "$STATE_FILE"
    # Clean up empty aei/src/ directory
    [[ -d "$SRC_DIR" ]] && rmdir "$SRC_DIR" 2>/dev/null || true
    [[ -d "$AEI_DIR" ]] && rmdir "$AEI_DIR" 2>/dev/null || true

    info "Cleaned $restored feed(s). Removed $removed repo(s), preserved $preserved repo(s) with local changes."
}

do_source_clean() {
    local pkgs=("$@")
    if [[ ${#pkgs[@]} -eq 0 ]]; then
        # Clean all module checkouts
        [[ -f "$SRC_STATE_FILE" ]] || { info "No module checkouts tracked."; return 0; }
        local removed=0 preserved=0
        while IFS='=' read -r pkg _; do
            [[ -n "$pkg" ]] || continue
            local src_dir="$SRC_DIR/$pkg"
            [[ -d "$src_dir/.git" ]] || continue
            if repo_is_published "$src_dir"; then
                info "Removing aei/src/$pkg (fully pushed, clean)"
                rm -rf "$src_dir"
                removed=$((removed + 1))
            else
                warn "Preserving aei/src/$pkg — has uncommitted changes or unpushed commits"
                preserved=$((preserved + 1))
            fi
        done < "$SRC_STATE_FILE"
        rm -f "$SRC_STATE_FILE"
        [[ -d "$SRC_DIR" ]] && rmdir "$SRC_DIR" 2>/dev/null || true
        info "Source clean: removed $removed repo(s), preserved $preserved repo(s) with local changes."
    else
        for pkg in "${pkgs[@]}"; do
            local src_dir="$SRC_DIR/$pkg"
            if [[ -d "$src_dir/.git" ]]; then
                if repo_is_published "$src_dir"; then
                    info "Removing aei/src/$pkg (fully pushed, clean)"
                    rm -rf "$src_dir"
                else
                    warn "Preserving aei/src/$pkg — has uncommitted changes or unpushed commits"
                fi
            fi
            remove_src_state "$pkg"
        done
    fi
}

do_status() {
    echo "Development dir: $AEI_DIR"
    echo ""

    # Feed tier
    echo "=== Feed Tier ==="
    local feed_state="$([[ -f "$STATE_FILE" ]] && echo "$(wc -l < "$STATE_FILE") feed(s)" || echo "none")"
    echo "State: $feed_state"
    local found=0
    while IFS= read -r line; do
        [[ "$line" == src-link* ]] || continue
        read -r _ fname fpath <<< "$line"
        echo "  $fname → $fpath"
        [[ -d "$fpath/.git" ]] && {
            local br; br=$(git -C "$fpath" branch --show-current 2>/dev/null || echo "detached")
            local ch; ch=$(git -C "$fpath" status --porcelain 2>/dev/null | wc -l)
            echo "    branch: $br  uncommitted: $ch"
        }
        found=1
    done < "$FEEDS_CONF"
    [[ "$found" -eq 1 ]] || echo "  No src-linked feeds."
    echo ""

    # Module tier
    echo "=== Module Tier ==="
    if [[ -d "$SRC_DIR" ]] && ls -d "$SRC_DIR"/*/.git &>/dev/null; then
        for src_dir in "$SRC_DIR"/*/; do
            [[ -d "$src_dir/.git" ]] || continue
            local pkg; pkg=$(basename "$src_dir")
            local br; br=$(git -C "$src_dir" branch --show-current 2>/dev/null || echo "detached")
            local ch; ch=$(git -C "$src_dir" status --porcelain 2>/dev/null | wc -l)
            local head; head=$(git -C "$src_dir" rev-parse --short HEAD 2>/dev/null || echo "???")
            echo "  $pkg ($br @ $head)  uncommitted: $ch"
        done
    else
        echo "  No module checkouts in $SRC_DIR/"
    fi
}

# ============================================================
# Main
# ============================================================

show_help() {
    cat <<'USAGE'
Usage: src_local.sh [command] [args...]

Feed tier - Feed src-link:
  [feed[:branch] ...]            Setup feed src-links
  --deep [feed[:branch] ...]     Src-link feed + clone GW3-owned module repos (skip third-party)
  (no args)                      Interactive: show status + feed selection menu
  --clean                        Restore src-links + remove published repos (keep dirty/unpushed)
  --status                       Show all state (feeds + modules)

Module tier - Module checkout:
  --source <pkg[:branch]> [...]  Clone specific module source repos
  --source <pkg> --url <url>     Clone with explicit git URL
  --source-clean [<pkg> ...]     Remove published module repos (keep dirty/unpushed)

Lifecycle - Pinning:
  --pin-sources                  Pin source commits into feed Makefiles
  --pin-feeds                    Pin feed commits into profile YAMLs
  --pin                          Both: pin sources then feeds

Environment:
  GW3_AEI_DIR              Override aei/ directory (default: <project_root>/aei)
  GW3_OWNED_URL_PATTERNS   Regex for GW3-owned source URLs (default: bitbucket.org Actiontec)
USAGE
}

[[ -f "$FEEDS_CONF" ]] || die "feeds.conf not found. Run gen_config.py first."

# Verbose mode for deep scan (show skipped third-party repos)
VERBOSE_DEEP=false
[[ "${GW3_VERBOSE:-}" == "1" ]] && VERBOSE_DEEP=true

case "${1:-}" in
    --clean)        do_clean; exit 0 ;;
    --status)       do_status; exit 0 ;;
    --source-clean) shift; do_source_clean "$@"; exit 0 ;;
    --pin-sources)  do_pin_sources; exit 0 ;;
    --pin-feeds)    do_pin_feeds; exit 0 ;;
    --pin)          do_pin; exit 0 ;;
    --help|-h)      show_help; exit 0 ;;
    --deep)
        shift
        # Collect feed arguments for deep mode
        DEEP_FEEDS=()
        if [[ $# -eq 0 ]]; then
            while IFS= read -r line; do
                [[ "$line" == src-git* && "$line" == *";"* ]] || continue
                read -r _ fname _ <<< "$line"
                DEEP_FEEDS+=("$fname")
            done < "$FEEDS_CONF"
            [[ ${#DEEP_FEEDS[@]} -gt 0 ]] || { info "No branch-tracked feeds found."; exit 0; }
            info "Auto-detected for deep: ${DEEP_FEEDS[*]}"
        else
            DEEP_FEEDS=("$@")
        fi
        do_deep "${DEEP_FEEDS[@]}"
        exit 0
        ;;
    --source)
        shift
        [[ $# -gt 0 ]] || die "Usage: src_local.sh --source <pkg[:branch]> [--url <url>]"
        # Parse source arguments
        SRC_PKGS=()
        SRC_BRANCHES=()
        EXPLICIT_URL=""
        while [[ $# -gt 0 ]]; do
            case "$1" in
                --url) shift; EXPLICIT_URL="${1:-}"; shift || die "--url requires a value" ;;
                -*)   die "Unknown option: $1" ;;
                *)
                    local_pkg="${1%%:*}"
                    local_branch=""
                    [[ "$1" == *:* ]] && local_branch="${1#*:}"
                    SRC_PKGS+=("$local_pkg")
                    SRC_BRANCHES+=("$local_branch")
                    shift
                    ;;
            esac
        done
        for i in "${!SRC_PKGS[@]}"; do
            setup_source "${SRC_PKGS[$i]}" "${SRC_BRANCHES[$i]}" "$EXPLICIT_URL"
        done
        exit 0
        ;;
esac

# Default: Feed tier src-link setup
declare -A BRANCHES
FEEDS=()
if [[ $# -eq 0 ]]; then
    # Interactive mode: show status, then present feed menu
    do_status
    echo ""

    # Collect all feeds from feeds.conf
    ALL_FEEDS=()
    ALL_FEED_STATUS=()
    while IFS= read -r line; do
        [[ "$line" == src-git* || "$line" == src-link* ]] || continue
        read -r ftype fname _ <<< "$line"
        ALL_FEEDS+=("$fname")
        if [[ "$ftype" == "src-link" ]]; then
            ALL_FEED_STATUS+=("src-linked")
        elif [[ "$line" == *";"* ]]; then
            local_branch="${line#*;}"
            ALL_FEED_STATUS+=("branch: $local_branch")
        else
            local_sha="${line#*^}"
            ALL_FEED_STATUS+=("pinned: ${local_sha:0:12}")
        fi
    done < "$FEEDS_CONF"

    [[ ${#ALL_FEEDS[@]} -gt 0 ]] || { info "No feeds found in feeds.conf."; exit 0; }

    echo "Available feeds:"
    for i in "${!ALL_FEEDS[@]}"; do
        printf "  %2d) %-25s [%s]\n" "$((i+1))" "${ALL_FEEDS[$i]}" "${ALL_FEED_STATUS[$i]}"
    done
    echo ""

    read -rp "Select feeds (numbers, comma/space separated, or 'q' to quit): " selection
    [[ -n "$selection" && "$selection" != "q" ]] || { echo "Cancelled."; exit 0; }

    # Parse selection into feed indices
    SELECTED=()
    for tok in ${selection//,/ }; do
        if [[ "$tok" =~ ^[0-9]+$ && "$tok" -ge 1 && "$tok" -le ${#ALL_FEEDS[@]} ]]; then
            SELECTED+=("$((tok-1))")
        else
            warn "Invalid selection: $tok (skipping)"
        fi
    done
    [[ ${#SELECTED[@]} -gt 0 ]] || { echo "No valid feeds selected."; exit 0; }

    # Show selected feeds
    echo ""
    echo "Selected:"
    for idx in "${SELECTED[@]}"; do
        echo "  - ${ALL_FEEDS[$idx]}"
    done

    # Ask for branch
    read -rp "Branch name (leave empty to use feed's current branch/pin): " user_branch

    # Ask for deep mode
    read -rp "Clone package source repos too? (--deep) [y/N]: " deep_answer
    USE_DEEP=false
    [[ "$deep_answer" =~ ^[Yy] ]] && USE_DEEP=true

    echo ""

    # Build feed list and execute
    for idx in "${SELECTED[@]}"; do
        fname="${ALL_FEEDS[$idx]}"
        if [[ -n "$user_branch" ]]; then
            FEEDS+=("$fname")
            BRANCHES["$fname"]="$user_branch"
        else
            FEEDS+=("$fname")
        fi
    done

    if $USE_DEEP; then
        DEEP_ARGS=()
        for f in "${FEEDS[@]}"; do
            if [[ -n "${BRANCHES[$f]:-}" ]]; then
                DEEP_ARGS+=("$f:${BRANCHES[$f]}")
            else
                DEEP_ARGS+=("$f")
            fi
        done
        do_deep "${DEEP_ARGS[@]}"
        exit 0
    fi
else
    for arg in "$@"; do
        if [[ "$arg" == *:* ]]; then
            FEEDS+=("${arg%%:*}")
            BRANCHES["${arg%%:*}"]="${arg#*:}"
        else
            FEEDS+=("$arg")
        fi
    done
fi

for f in "${FEEDS[@]}"; do
    setup_feed "$f" "${BRANCHES[$f]:-}"
done

echo ""
echo "Re-run this after gen_config.py to restore src-links."
