#!/bin/bash
set -e

# ============================================================
# gw3_branch.sh — Multi-repo branch management for GW3
# ============================================================

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"
AEI_DIR="${GW3_AEI_DIR:-$PROJECT_ROOT/aei}"
SRC_DIR="$AEI_DIR/src"
FEEDS_CONF="$PROJECT_ROOT/feeds.conf"
MAIN_BRANCH="r3_mainline"
FEED_MAIN_BRANCH="main"
SRC_LOCAL="$SCRIPT_DIR/../gw3-src-local/src_local.sh"
VERBOSE=false

# Source PR subcommand (adds do_pr and helpers)
source "$SCRIPT_DIR/gw3_branch_pr.sh"

# GW3-owned source repo URL patterns (shared with src_local.sh)
GW3_OWNED_URL_PATTERNS="${GW3_OWNED_URL_PATTERNS:-bitbucket\.org[:/]Actiontec/}"

is_owned_url() {
    local url="$1"
    [[ "$url" =~ $GW3_OWNED_URL_PATTERNS ]]
}

is_owned_repo() {
    local dir="$1"
    local url; url=$(git -C "$dir" remote get-url origin 2>/dev/null || true)
    [[ -z "$url" ]] && return 0  # no remote = local repo, treat as owned
    is_owned_url "$url"
}

# ============================================================
# Utilities
# ============================================================

die()    { echo "Error: $*" >&2; exit 1; }
info()   { echo "==> $*"; }
warn()   { echo "Warning: $*" >&2; }
header() { echo ""; echo "--- $* ---"; }

get_prplos_branch() {
    git -C "$PROJECT_ROOT" branch --show-current 2>/dev/null || echo "detached"
}

get_branch_type() {
    local branch="$1"
    case "$branch" in
        "$MAIN_BRANCH"|main) echo "main" ;;
        feature/*)           echo "feature" ;;
        release/*)           echo "release" ;;
        hotfix/*)            echo "hotfix" ;;
        *)                   echo "unknown" ;;
    esac
}

repo_is_dirty() {
    local dir="$1"
    [[ -d "$dir/.git" ]] || return 1
    [[ -n "$(git -C "$dir" status --porcelain 2>/dev/null)" ]]
}

repo_branch() {
    local dir="$1"
    git -C "$dir" branch --show-current 2>/dev/null || echo "detached"
}

repo_ahead_count() {
    local dir="$1"
    git -C "$dir" rev-list '@{u}..HEAD' --count 2>/dev/null || echo "0"
}

# Check if the current branch has a remote tracking branch
repo_has_upstream() {
    local dir="$1"
    git -C "$dir" rev-parse --abbrev-ref '@{u}' &>/dev/null
}

# Check if the current branch exists on remote
repo_branch_on_remote() {
    local dir="$1"
    local branch; branch=$(repo_branch "$dir")
    [[ "$branch" != "detached" ]] || return 1
    git -C "$dir" ls-remote --heads origin "$branch" 2>/dev/null | grep -q .
}

# Push a new local branch to remote with upstream tracking
# Skips third-party repos. Safe to call even if branch already exists on remote.
publish_branch() {
    local dir="$1" label="$2"
    local br; br=$(repo_branch "$dir")
    [[ "$br" != "detached" ]] || return 0
    # Skip third-party repos
    is_owned_repo "$dir" || return 0
    # Skip if already on remote
    repo_branch_on_remote "$dir" && return 0
    info "Publishing branch '$br' for $label..."
    git -C "$dir" push -u origin "$br" 2>/dev/null || \
        warn "Could not push branch '$br' for $label — push manually later"
}

# Parse a feeds.conf line: sets FC_TYPE, FC_NAME, FC_URI, FC_BRANCH, FC_SHA1
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

extract_pkg_var() {
    local makefile="$1" varname="$2"
    grep -E "^${varname}\s*[:?]?=" "$makefile" 2>/dev/null | head -1 | sed 's/^[^=]*=[[:space:]]*//; s/\r$//'
}

# Check if a revision is a commit pin (40-char hex, or tag@40-char-hex)
is_commit_pin() {
    [[ "$1" =~ ^[0-9a-f]{40}$ ]] || [[ "$1" =~ @[0-9a-f]{40}$ ]]
}

branch_exists_anywhere() {
    local dir="$1" branch="$2"
    git -C "$dir" rev-parse --verify "$branch" &>/dev/null && return 0
    git -C "$dir" rev-parse --verify "origin/$branch" &>/dev/null && return 0
    return 1
}

# ============================================================
# Discovery
# ============================================================

# Find all git repos under aei/<feed>/ (not aei/src/)
discover_feed_repos() {
    [[ -d "$AEI_DIR" ]] || return 0
    for d in "$AEI_DIR"/*/; do
        [[ -d "$d/.git" && "$(basename "$d")" != "src" ]] && echo "$d"
    done
}

# Find all git repos under aei/src/<pkg>/
discover_source_repos() {
    [[ -d "$SRC_DIR" ]] || return 0
    for d in "$SRC_DIR"/*/; do
        [[ -d "$d/.git" ]] && echo "$d"
    done
}

# Parse all profile YAMLs for feed definitions
# Output: name|uri|revision|profile_file (one line per feed)
get_profile_feeds() {
    python3 -c "
import yaml, glob, os

profile_dir = os.path.join('$PROJECT_ROOT', 'profiles')
feeds = {}
for yml_path in sorted(glob.glob(os.path.join(profile_dir, '*.yml'))):
    try:
        with open(yml_path) as f:
            data = yaml.safe_load(f) or {}
    except Exception:
        continue
    for feed in data.get('feeds', []) or []:
        name = feed.get('name', '')
        if name:
            feeds[name] = {
                'uri': feed.get('uri', ''),
                'revision': str(feed.get('revision', '')),
                'profile': os.path.basename(yml_path)
            }
for name, info in sorted(feeds.items()):
    print(f\"{name}|{info['uri']}|{info['revision']}|{info['profile']}\")
" 2>/dev/null
}

# Get feeds with branch-tracking revisions (not commit-pinned)
get_tracked_feeds() {
    while IFS='|' read -r name uri revision profile; do
        is_commit_pin "$revision" || echo "$name|$uri|$revision|$profile"
    done < <(get_profile_feeds)
}

# Get GW3-owned feed names (from profile YAMLs, not feeds.conf.default)
get_gw3_owned_feeds() {
    get_profile_feeds | cut -d'|' -f1
}

# Update a feed's revision in profile YAMLs
update_profile_revision() {
    local feed_name="$1" new_revision="$2"
    python3 -c "
import re, glob, os

name = '$feed_name'
new_rev = '$new_revision'
profile_dir = os.path.join('$PROJECT_ROOT', 'profiles')

for yml_path in glob.glob(os.path.join(profile_dir, '*.yml')):
    with open(yml_path) as f:
        lines = f.readlines()
    modified = False
    in_feed = False
    out = []
    for line in lines:
        if re.match(r'\s*-?\s*name:\s*' + re.escape(name) + r'\s*$', line):
            in_feed = True
            out.append(line)
            continue
        if in_feed and re.match(r'\s*revision:\s*', line):
            indent = re.match(r'(\s*)', line).group(1)
            out.append(f'{indent}revision: {new_rev}\n')
            in_feed = False
            modified = True
            continue
        if in_feed and re.match(r'\s*-\s*name:', line):
            in_feed = False
        out.append(line)
    if modified:
        with open(yml_path, 'w') as f:
            f.writelines(out)
        print(f'Updated {os.path.basename(yml_path)}: {name} -> {new_rev}')
" 2>/dev/null
}

# ============================================================
# Stale Repo Handling
# ============================================================

# Check if a repo is fully published (clean + pushed)
repo_is_published() {
    local dir="$1"
    [[ -d "$dir/.git" ]] || return 1
    repo_is_dirty "$dir" && return 1
    repo_has_upstream "$dir" || return 1
    local ahead; ahead=$(repo_ahead_count "$dir")
    [[ "$ahead" -eq 0 ]]
}

# Get a feed's current line from feeds.conf
get_feed_line() {
    grep -E "^src-\w+\s+$1\s" "$FEEDS_CONF" 2>/dev/null || true
}

# Replace a feed's line in feeds.conf
replace_feed_line() {
    local name="$1" new="$2"
    sed -i "s|^src-[a-z_]*[[:space:]]\+${name}[[:space:]]\+.*|${new}|" "$FEEDS_CONF"
}

# Reindex a single feed (remove + update + install)
reindex_feed() {
    local name="$1"
    rm -rf "$PROJECT_ROOT/feeds/$name"
    ( cd "$PROJECT_ROOT" && ./scripts/feeds update "$name" )
    ( cd "$PROJECT_ROOT" && ./scripts/feeds install -a -f -p "$name" )
}

# Remove a feed entry from .srclocal.state
remove_state() {
    local name="$1"
    local state_file="$AEI_DIR/.srclocal.state"
    [[ -f "$state_file" ]] && sed -i "/^${name}=/d" "$state_file"
}

# Remove a source entry from .srclocal-source.state
remove_src_state() {
    local pkg="$1"
    local state_file="$AEI_DIR/.srclocal-source.state"
    [[ -f "$state_file" ]] && sed -i "/^${pkg}=/d" "$state_file"
}

# Given a package name, find which feed it belongs to
find_source_feed() {
    local pkg="$1"
    for d in "$AEI_DIR"/*/; do
        [[ -d "$d/.git" && "$(basename "$d")" != "src" ]] || continue
        if find "$d" -maxdepth 5 -path "*/$pkg/Makefile" -not -path '*/.git/*' 2>/dev/null | grep -q .; then
            basename "$d"
            return 0
        fi
    done
    return 1
}

# Remove all source repos belonging to a feed
remove_feed_sources() {
    local feed_name="$1"
    [[ -d "$SRC_DIR" ]] || return 0
    for d in "$SRC_DIR"/*/; do
        [[ -d "$d/.git" ]] || continue
        local pkg; pkg=$(basename "$d")
        local parent; parent=$(find_source_feed "$pkg") || continue
        if [[ "$parent" == "$feed_name" ]]; then
            repo_is_published "$d" || warn "aei/src/$pkg has unpushed changes"
            info "Removing aei/src/$pkg"
            rm -rf "$d"
            remove_src_state "$pkg"
        fi
    done
}

# Remove a single feed: restore feeds.conf, reindex, remove sources + dir
remove_single_feed() {
    local name="$1"
    local feed_dir="$AEI_DIR/$name"

    repo_is_published "$feed_dir" || warn "$name has uncommitted changes or unpushed commits"

    # Remove source repos belonging to this feed
    remove_feed_sources "$name"

    # Restore feeds.conf entry from state
    local state_file="$AEI_DIR/.srclocal.state"
    if [[ -f "$state_file" ]]; then
        local original; original=$(grep "^${name}=" "$state_file" 2>/dev/null | cut -d= -f2-)
        if [[ -n "$original" ]]; then
            replace_feed_line "$name" "$original"
            reindex_feed "$name"
        fi
        remove_state "$name"
    fi

    rm -rf "$feed_dir"
    info "Removed $name"
}

# Remove a single source repo
remove_single_source() {
    local pkg="$1"
    local src_dir="$SRC_DIR/$pkg"

    repo_is_published "$src_dir" || warn "aei/src/$pkg has uncommitted changes or unpushed commits"

    rm -rf "$src_dir"
    remove_src_state "$pkg"
    info "Removed aei/src/$pkg"
}

# Add a stale feed to the target branch (deep or shallow)
add_stale_feed() {
    local name="$1" target_branch="$2" is_shallow="$3"
    update_profile_revision "$name" "$target_branch"
    # Switch feed branch if already checked out on wrong branch
    if [[ -d "$AEI_DIR/$name/.git" ]]; then
        local cur; cur=$(repo_branch "$AEI_DIR/$name")
        if [[ "$cur" != "$target_branch" ]]; then
            info "Switching $name to $target_branch..."
            git -C "$AEI_DIR/$name" fetch origin 2>/dev/null || true
            git -C "$AEI_DIR/$name" checkout "$target_branch" 2>/dev/null || \
                git -C "$AEI_DIR/$name" checkout -b "$target_branch" "origin/$target_branch" 2>/dev/null || \
                git -C "$AEI_DIR/$name" checkout -b "$target_branch"
            publish_branch "$AEI_DIR/$name" "$name"
        fi
    fi
    if [[ "$is_shallow" == "true" ]]; then
        "$SRC_LOCAL" "$name:$target_branch"
    else
        "$SRC_LOCAL" --deep "$name:$target_branch"
    fi
}

# Switch a stale source repo to the target branch
add_stale_source() {
    local pkg="$1" target_branch="$2"
    local src_dir="$SRC_DIR/$pkg"

    info "Switching aei/src/$pkg to $target_branch..."
    git -C "$src_dir" fetch origin 2>/dev/null || true
    if git -C "$src_dir" checkout "$target_branch" 2>/dev/null; then
        true
    elif git -C "$src_dir" checkout -b "$target_branch" "origin/$target_branch" 2>/dev/null; then
        true
    elif git -C "$src_dir" checkout -b "$target_branch" 2>/dev/null; then
        publish_branch "$src_dir" "aei/src/$pkg"
    else
        warn "Could not switch $pkg to $target_branch"
    fi
}

# Scan for and handle repos left over from a previous feature/hotfix.
# Prompts user to add each stale repo to the current feature or remove it.
# Options per prompt: y=add, N=remove (default), a=add all, r=remove all.
handle_stale_repos() {
    local target_branch="$1" is_shallow="$2"
    local -n _processed_feeds="$3"

    # Build lookup of processed feeds
    local -A processed_set=()
    for f in "${_processed_feeds[@]}"; do
        processed_set["$f"]=1
    done

    # Collect stale feeds (on wrong branch, not already processed)
    local stale_feeds=()
    while IFS= read -r dir; do
        [[ -n "$dir" ]] || continue
        local name; name=$(basename "$dir")
        [[ -z "${processed_set[$name]+x}" ]] || continue
        local cur; cur=$(repo_branch "$dir")
        [[ "$cur" != "$target_branch" ]] && stale_feeds+=("$name")
    done < <(discover_feed_repos)

    # Quick check: any stale sources? (only if no stale feeds to avoid early scan)
    local has_stale_sources=false
    if [[ ${#stale_feeds[@]} -eq 0 ]]; then
        while IFS= read -r dir; do
            [[ -n "$dir" ]] || continue
            local cur; cur=$(repo_branch "$dir")
            if [[ "$cur" != "$target_branch" ]]; then
                has_stale_sources=true; break
            fi
        done < <(discover_source_repos)
    fi

    [[ ${#stale_feeds[@]} -gt 0 || "$has_stale_sources" == "true" ]] || return 0

    echo ""
    header "Stale repos from previous feature"

    local bulk=""  # once set to "a" or "r", applies to all remaining

    # --- Handle stale feeds ---
    for name in "${stale_feeds[@]}"; do
        local cur; cur=$(repo_branch "$AEI_DIR/$name")
        local choice="$bulk"
        if [[ -z "$choice" ]]; then
            read -rp "$name is on '$cur', not '$target_branch'. Add to feature? [y/N/a/r] " choice || choice=""
            choice="${choice:-n}"
        fi
        case "$choice" in
            [aA]) bulk="a"; add_stale_feed "$name" "$target_branch" "$is_shallow" ;;
            [rR]) bulk="r"; remove_single_feed "$name" ;;
            [yY])           add_stale_feed "$name" "$target_branch" "$is_shallow" ;;
            *)              remove_single_feed "$name" ;;
        esac
    done

    # --- Handle stale source repos ---
    # Re-scan after feed handling: feeds added deep already switched their
    # sources, and feeds removed already deleted theirs.
    local stale_sources=()
    while IFS= read -r dir; do
        [[ -n "$dir" ]] || continue
        local pkg; pkg=$(basename "$dir")
        local cur; cur=$(repo_branch "$dir")
        [[ "$cur" != "$target_branch" ]] || continue
        stale_sources+=("$pkg")
    done < <(discover_source_repos)

    for pkg in "${stale_sources[@]}"; do
        local cur; cur=$(repo_branch "$SRC_DIR/$pkg")
        local choice="$bulk"
        if [[ -z "$choice" ]]; then
            read -rp "aei/src/$pkg is on '$cur', not '$target_branch'. Add to feature? [y/N/a/r] " choice || choice=""
            choice="${choice:-n}"
        fi
        case "$choice" in
            [aA]) bulk="a"; add_stale_source "$pkg" "$target_branch" ;;
            [rR]) bulk="r"; remove_single_source "$pkg" ;;
            [yY])           add_stale_source "$pkg" "$target_branch" ;;
            *)              remove_single_source "$pkg" ;;
        esac
    done
}

# ============================================================
# Status Report
# ============================================================

do_status() {
    local branch; branch=$(get_prplos_branch)
    local btype; btype=$(get_branch_type "$branch")

    # --- Collect feed repo data ---
    local feed_repos=() feed_names=() feed_branches=() feed_dirty=()
    local total_feeds=0 dirty_feeds=0
    while IFS= read -r dir; do
        [[ -n "$dir" ]] || continue
        local name; name=$(basename "$dir")
        feed_repos+=("$dir")
        feed_names+=("$name")
        feed_branches+=("$(repo_branch "$dir")")
        local dirty=0; repo_is_dirty "$dir" && dirty=1
        feed_dirty+=("$dirty")
        total_feeds=$((total_feeds + 1))
        [[ "$dirty" -eq 0 ]] || dirty_feeds=$((dirty_feeds + 1))
    done < <(discover_feed_repos)

    # --- Collect source repo data ---
    local src_repos=() src_names=() src_branches=() src_dirty_list=()
    local total_sources=0 dirty_sources=0
    while IFS= read -r dir; do
        [[ -n "$dir" ]] || continue
        local name; name=$(basename "$dir")
        src_repos+=("$dir")
        src_names+=("$name")
        src_branches+=("$(repo_branch "$dir")")
        local dirty=0; repo_is_dirty "$dir" && dirty=1
        src_dirty_list+=("$dirty")
        total_sources=$((total_sources + 1))
        [[ "$dirty" -eq 0 ]] || dirty_sources=$((dirty_sources + 1))
    done < <(discover_source_repos)

    # --- prplos dirty check ---
    local prplos_dirty=0
    repo_is_dirty "$PROJECT_ROOT" && prplos_dirty=1

    # --- Collect warnings ---
    local warnings=()

    # Branch consistency for feeds
    for i in "${!feed_names[@]}"; do
        if [[ "$btype" == "feature" || "$btype" == "hotfix" ]]; then
            if [[ "${feed_branches[$i]}" != "$branch" ]]; then
                warnings+=("${feed_names[$i]}: on '${feed_branches[$i]}', expected '$branch'")
            fi
        fi
        if [[ "${feed_dirty[$i]}" -eq 1 ]]; then
            local ch; ch=$(git -C "${feed_repos[$i]}" status --porcelain 2>/dev/null | wc -l)
            warnings+=("${feed_names[$i]}: $ch uncommitted change(s)")
        fi
    done

    # Branch consistency for source repos
    for i in "${!src_names[@]}"; do
        if [[ "$btype" == "feature" || "$btype" == "hotfix" ]]; then
            if [[ "${src_branches[$i]}" != "$branch" ]]; then
                warnings+=("aei/src/${src_names[$i]}: on '${src_branches[$i]}', expected '$branch'")
            fi
        fi
        if [[ "${src_dirty_list[$i]}" -eq 1 ]]; then
            local ch; ch=$(git -C "${src_repos[$i]}" status --porcelain 2>/dev/null | wc -l)
            warnings+=("aei/src/${src_names[$i]}: $ch uncommitted change(s)")
        fi
    done

    # prplos dirty
    if [[ "$prplos_dirty" -eq 1 ]]; then
        local ch; ch=$(git -C "$PROJECT_ROOT" status --porcelain 2>/dev/null | wc -l)
        warnings+=("prplos: $ch uncommitted change(s)")
    fi

    # Local-only branches (no remote counterpart) — data loss risk (owned repos only)
    # Run git ls-remote checks in parallel to avoid N×7s sequential network calls
    local _remote_check_dir; _remote_check_dir=$(mktemp -d)
    local _remote_pids=()

    for i in "${!feed_names[@]}"; do
        if is_owned_repo "${feed_repos[$i]}"; then
            ( repo_branch_on_remote "${feed_repos[$i]}" && echo "ok" || echo "local" ) \
                > "$_remote_check_dir/feed_$i" 2>/dev/null &
            _remote_pids+=($!)
        fi
    done
    for i in "${!src_names[@]}"; do
        if is_owned_repo "${src_repos[$i]}"; then
            ( repo_branch_on_remote "${src_repos[$i]}" && echo "ok" || echo "local" ) \
                > "$_remote_check_dir/src_$i" 2>/dev/null &
            _remote_pids+=($!)
        fi
    done

    # Wait for all parallel checks
    for pid in "${_remote_pids[@]}"; do
        wait "$pid" 2>/dev/null || true
    done

    # Collect results
    for i in "${!feed_names[@]}"; do
        local _result_file="$_remote_check_dir/feed_$i"
        if [[ -f "$_result_file" ]] && [[ "$(cat "$_result_file")" == "local" ]]; then
            warnings+=("${feed_names[$i]}: branch '${feed_branches[$i]}' is LOCAL ONLY (not pushed to remote). Run 'gw3_branch.sh push' to protect against data loss.")
        fi
    done
    for i in "${!src_names[@]}"; do
        local _result_file="$_remote_check_dir/src_$i"
        if [[ -f "$_result_file" ]] && [[ "$(cat "$_result_file")" == "local" ]]; then
            warnings+=("aei/src/${src_names[$i]}: branch '${src_branches[$i]}' is LOCAL ONLY (not pushed to remote). Run 'gw3_branch.sh push' to protect against data loss.")
        fi
    done
    rm -rf "$_remote_check_dir"

    # On main/release: check pinning
    if [[ "$btype" == "main" || "$btype" == "release" ]]; then
        while IFS='|' read -r name uri revision profile; do
            is_commit_pin "$revision" || warnings+=("Unpinned feed: $name (in $profile) tracking '$revision'")
        done < <(get_profile_feeds)

        # Check PKG_SOURCE_VERSION in src-linked feed repos
        while IFS= read -r dir; do
            [[ -n "$dir" ]] || continue
            local fname; fname=$(basename "$dir")
            while IFS= read -r makefile; do
                local proto; proto=$(extract_pkg_var "$makefile" "PKG_SOURCE_PROTO")
                [[ "$proto" == "git" ]] || continue
                local ver; ver=$(extract_pkg_var "$makefile" "PKG_SOURCE_VERSION")
                [[ -n "$ver" ]] || continue
                if ! [[ "$ver" =~ ^[0-9a-f]{40}$ ]]; then
                    local pkg; pkg=$(basename "$(dirname "$makefile")")
                    warnings+=("Unpinned source: $fname/$pkg PKG_SOURCE_VERSION='$ver'")
                fi
            done < <(find "$dir" -maxdepth 5 -name Makefile -not -path '*/.git/*' 2>/dev/null)
        done < <(discover_feed_repos)
    fi

    # Warn if on main/release with aei/ checkouts
    if [[ "$btype" == "main" || "$btype" == "release" ]] && [[ $total_feeds -gt 0 || $total_sources -gt 0 ]]; then
        warnings+=("aei/ checkouts exist on $btype branch — consider cleaning up")
    fi

    # --- Output ---
    local has_issues=false
    [[ ${#warnings[@]} -gt 0 ]] && has_issues=true

    if [[ "$has_issues" == "false" && "$VERBOSE" == "false" ]]; then
        echo "prplos @ $branch | $total_feeds feed(s), $total_sources source(s) | all clean"
    else
        echo "prplos @ $branch ($btype branch)"
        echo ""
        if [[ $total_feeds -gt 0 ]]; then
            printf "Feeds:"
            for i in "${!feed_names[@]}"; do
                local status="ok"
                [[ "${feed_dirty[$i]}" -eq 1 ]] && status="dirty"
                printf " %s(%s)" "${feed_names[$i]}" "$status"
            done
            echo ""
        else
            echo "Feeds: none src-linked"
        fi
        if [[ $total_sources -gt 0 ]]; then
            echo "Sources: $total_sources repo(s), $dirty_sources dirty"
            if $VERBOSE; then
                for i in "${!src_names[@]}"; do
                    local status="ok"
                    [[ "${src_dirty_list[$i]}" -eq 1 ]] && status="dirty"
                    echo "  ${src_names[$i]} (${src_branches[$i]}) $status"
                done
            fi
        else
            echo "Sources: none"
        fi
        if [[ ${#warnings[@]} -gt 0 ]]; then
            echo ""
            echo "Warnings:"
            for w in "${warnings[@]}"; do
                echo "  $w"
            done
        fi
    fi
}

# ============================================================
# Pull
# ============================================================

do_pull() {
    header "Pulling prplos"
    git -C "$PROJECT_ROOT" pull || warn "Failed to pull prplos"

    while IFS= read -r dir; do
        [[ -n "$dir" ]] || continue
        local name; name=$(basename "$dir")
        header "Pulling $name"
        git -C "$dir" pull || warn "Failed to pull $name"
    done < <(discover_feed_repos)

    while IFS= read -r dir; do
        [[ -n "$dir" ]] || continue
        local name; name=$(basename "$dir")
        header "Pulling aei/src/$name"
        git -C "$dir" pull || warn "Failed to pull $name"
    done < <(discover_source_repos)

    echo ""
    do_status
}

# ============================================================
# Commit
# ============================================================

do_commit() {
    local branch; branch=$(get_prplos_branch)
    local btype; btype=$(get_branch_type "$branch")

    # On main/release: enforce pinning before commit
    if [[ "$btype" == "main" || "$btype" == "release" ]]; then
        local errors=()

        # Check feed revisions are pinned
        while IFS='|' read -r name uri revision profile; do
            is_commit_pin "$revision" || errors+=("Feed $name (in $profile): tracking '$revision'")
        done < <(get_profile_feeds)

        # Check PKG_SOURCE_VERSION in feed repos
        while IFS= read -r dir; do
            [[ -n "$dir" ]] || continue
            local fname; fname=$(basename "$dir")
            while IFS= read -r makefile; do
                local proto; proto=$(extract_pkg_var "$makefile" "PKG_SOURCE_PROTO")
                [[ "$proto" == "git" ]] || continue
                local ver; ver=$(extract_pkg_var "$makefile" "PKG_SOURCE_VERSION")
                [[ -n "$ver" ]] || continue
                if ! [[ "$ver" =~ ^[0-9a-f]{40}$ ]]; then
                    local pkg; pkg=$(basename "$(dirname "$makefile")")
                    errors+=("$fname/$pkg: PKG_SOURCE_VERSION='$ver'")
                fi
            done < <(find "$dir" -maxdepth 5 -name Makefile -not -path '*/.git/*' 2>/dev/null)
        done < <(discover_feed_repos)

        if [[ ${#errors[@]} -gt 0 ]]; then
            echo "Cannot commit on $btype branch — unpinned revisions:"
            for e in "${errors[@]}"; do
                echo "  $e"
            done
            echo ""
            echo "Run 'src_local.sh --pin' to pin all revisions first."
            exit 1
        fi

        # Only commit prplos
        if repo_is_dirty "$PROJECT_ROOT"; then
            info "Committing prplos..."
            local stat; stat=$(git -C "$PROJECT_ROOT" diff --stat 2>/dev/null)
            git -C "$PROJECT_ROOT" add -A
            git -C "$PROJECT_ROOT" commit -m "Update specs/profiles for $branch" -m "$stat"
            info "Committed prplos"
        else
            info "prplos: nothing to commit"
        fi
        return
    fi

    # Feature/hotfix: commit all dirty repos
    local committed=0

    # Source repos first
    while IFS= read -r dir; do
        [[ -n "$dir" ]] || continue
        repo_is_dirty "$dir" || continue
        local name; name=$(basename "$dir")
        info "Committing aei/src/$name..."
        local stat; stat=$(git -C "$dir" diff --stat 2>/dev/null)
        git -C "$dir" add -A
        git -C "$dir" commit -m "Update $name for $branch" -m "$stat"
        committed=$((committed + 1))
    done < <(discover_source_repos)

    # Feed repos
    while IFS= read -r dir; do
        [[ -n "$dir" ]] || continue
        repo_is_dirty "$dir" || continue
        local name; name=$(basename "$dir")
        info "Committing $name..."
        local stat; stat=$(git -C "$dir" diff --stat 2>/dev/null)
        git -C "$dir" add -A
        git -C "$dir" commit -m "Update $name packages for $branch" -m "$stat"
        committed=$((committed + 1))
    done < <(discover_feed_repos)

    # prplos
    if repo_is_dirty "$PROJECT_ROOT"; then
        info "Committing prplos..."
        local stat; stat=$(git -C "$PROJECT_ROOT" diff --stat 2>/dev/null)
        git -C "$PROJECT_ROOT" add -A
        git -C "$PROJECT_ROOT" commit -m "Update specs/profiles for $branch" -m "$stat"
        committed=$((committed + 1))
    fi

    if [[ "$committed" -eq 0 ]]; then
        info "Nothing to commit — all repos are clean"
    else
        info "Committed $committed repo(s)"
        echo ""
        echo "Run 'gw3_branch.sh push' to push all repos."
    fi
}

# ============================================================
# Push
# ============================================================

push_repo() {
    local dir="$1" label="$2"
    local br; br=$(repo_branch "$dir")
    [[ "$br" != "detached" ]] || return 0

    # Skip third-party repos — we don't have push access
    if ! is_owned_repo "$dir"; then
        return 1
    fi

    # Check if branch exists on remote
    if ! repo_branch_on_remote "$dir"; then
        info "Pushing new branch '$br' for $label..."
        git -C "$dir" push -u origin "$br" || { warn "Failed to push $label"; return 1; }
        return 0
    fi

    # Branch exists on remote — check for unpushed commits
    if ! repo_has_upstream "$dir"; then
        # Local branch not tracking remote yet, set it up
        git -C "$dir" branch --set-upstream-to="origin/$br" "$br" 2>/dev/null || true
    fi
    local ahead; ahead=$(repo_ahead_count "$dir")
    if [[ "$ahead" -gt 0 ]]; then
        info "Pushing $label ($ahead commit(s))..."
        git -C "$dir" push origin "$br" || { warn "Failed to push $label"; return 1; }
        return 0
    fi
    return 1  # nothing to push
}

do_push() {
    local pushed=0

    # prplos
    push_repo "$PROJECT_ROOT" "prplos" && pushed=$((pushed + 1))

    # Feeds
    while IFS= read -r dir; do
        [[ -n "$dir" ]] || continue
        local name; name=$(basename "$dir")
        push_repo "$dir" "$name" && pushed=$((pushed + 1))
    done < <(discover_feed_repos)

    # Sources
    while IFS= read -r dir; do
        [[ -n "$dir" ]] || continue
        local name; name=$(basename "$dir")
        push_repo "$dir" "aei/src/$name" && pushed=$((pushed + 1))
    done < <(discover_source_repos)

    if [[ "$pushed" -eq 0 ]]; then
        info "All repos are up to date with remote"
    else
        info "Pushed $pushed repo(s)"
    fi
}

# ============================================================
# Feature
# ============================================================

do_feature() {
    local branch_name="$1"; shift
    local feeds=() source_pkgs=() deep_feeds=() shallow=false

    # Parse remaining args
    # Default: all feeds get --deep treatment (feed + module tiers)
    # Use --shallow to opt out (feed tier only)
    while [[ $# -gt 0 ]]; do
        case "$1" in
            --shallow) shallow=true; shift ;;
            --source)
                shift
                while [[ $# -gt 0 && "$1" != --* ]]; do
                    source_pkgs+=("$1"); shift
                done ;;
            --deep)
                shift
                while [[ $# -gt 0 && "$1" != --* ]]; do
                    deep_feeds+=("$1"); shift
                done ;;
            feed_*) feeds+=("$1"); shift ;;
            *)      die "Unknown argument: $1" ;;
        esac
    done

    # Default: promote plain feeds to deep (unless --shallow)
    if ! $shallow && [[ ${#feeds[@]} -gt 0 ]]; then
        deep_feeds+=("${feeds[@]}")
        feeds=()
    fi

    local current; current=$(get_prplos_branch)

    if branch_exists_anywhere "$PROJECT_ROOT" "$branch_name"; then
        # --- Switch + extend existing branch ---
        if [[ "$current" != "$branch_name" ]]; then
            repo_is_dirty "$PROJECT_ROOT" && warn "prplos has uncommitted changes"
            info "Switching prplos to $branch_name..."
            git -C "$PROJECT_ROOT" checkout "$branch_name" 2>/dev/null || \
                git -C "$PROJECT_ROOT" checkout -b "$branch_name" "origin/$branch_name"
        else
            info "prplos already on $branch_name"
        fi

        # Set up feeds if specified (additive)
        for f in "${feeds[@]}"; do
            if [[ -d "$AEI_DIR/$f/.git" ]]; then
                info "Feed $f already checked out, ensuring branch..."
                local cur; cur=$(repo_branch "$AEI_DIR/$f")
                if [[ "$cur" != "$branch_name" ]]; then
                    git -C "$AEI_DIR/$f" fetch origin 2>/dev/null || true
                    git -C "$AEI_DIR/$f" checkout "$branch_name" 2>/dev/null || \
                        git -C "$AEI_DIR/$f" checkout -b "$branch_name" "origin/$branch_name" 2>/dev/null || \
                        git -C "$AEI_DIR/$f" checkout -b "$branch_name"
                    publish_branch "$AEI_DIR/$f" "$f"
                fi
            else
                "$SRC_LOCAL" "$f:$branch_name"
            fi
        done

        # Deep feeds — switch branch first if already checked out on wrong branch
        for f in "${deep_feeds[@]}"; do
            if [[ -d "$AEI_DIR/$f/.git" ]]; then
                local cur; cur=$(repo_branch "$AEI_DIR/$f")
                if [[ "$cur" != "$branch_name" ]]; then
                    info "Switching $f to $branch_name..."
                    git -C "$AEI_DIR/$f" fetch origin 2>/dev/null || true
                    git -C "$AEI_DIR/$f" checkout "$branch_name" 2>/dev/null || \
                        git -C "$AEI_DIR/$f" checkout -b "$branch_name" "origin/$branch_name" 2>/dev/null || \
                        git -C "$AEI_DIR/$f" checkout -b "$branch_name"
                    publish_branch "$AEI_DIR/$f" "$f"
                fi
            fi
            "$SRC_LOCAL" --deep "$f:$branch_name"
        done

        # Source packages
        for pkg in "${source_pkgs[@]}"; do
            "$SRC_LOCAL" --source "$pkg:$branch_name"
        done

        # If no extra args and no feeds src-linked yet: detect tracked feeds
        if [[ ${#feeds[@]} -eq 0 && ${#deep_feeds[@]} -eq 0 && ${#source_pkgs[@]} -eq 0 ]]; then
            local has_srclinks=false
            [[ -f "$AEI_DIR/.srclocal.state" ]] && has_srclinks=true
            if ! $has_srclinks; then
                local tracked_count=0
                while IFS='|' read -r name uri revision profile; do
                    info "Auto-setting up tracked feed: $name ($revision)"
                    "$SRC_LOCAL" "$name:$revision"
                    tracked_count=$((tracked_count + 1))
                done < <(get_tracked_feeds)
                [[ $tracked_count -gt 0 ]] || info "No branch-tracked feeds to auto-setup"
            fi
        fi

    else
        # --- Create new feature branch ---
        repo_is_dirty "$PROJECT_ROOT" && warn "prplos has uncommitted changes"

        # If no feeds specified, show interactive menu
        if [[ ${#feeds[@]} -eq 0 && ${#deep_feeds[@]} -eq 0 ]]; then
            local owned_feeds=()
            while IFS= read -r f; do
                [[ -n "$f" ]] && owned_feeds+=("$f")
            done < <(get_gw3_owned_feeds)

            if [[ ${#owned_feeds[@]} -gt 0 ]]; then
                echo "GW3-owned feeds:"
                for i in "${!owned_feeds[@]}"; do
                    printf "  %2d) %s\n" "$((i+1))" "${owned_feeds[$i]}"
                done
                echo "   0) None (prplos-only branch)"
                echo ""
                read -rp "Select feeds for this feature (numbers, comma/space separated): " selection
                [[ -n "$selection" && "$selection" != "0" ]] || true
                for tok in ${selection//,/ }; do
                    if [[ "$tok" =~ ^[0-9]+$ && "$tok" -ge 1 && "$tok" -le ${#owned_feeds[@]} ]]; then
                        feeds+=("${owned_feeds[$((tok-1))]}")
                    fi
                done
            fi
        fi

        # Create branch in prplos and publish to remote
        info "Creating branch $branch_name in prplos..."
        git -C "$PROJECT_ROOT" checkout -b "$branch_name"
        publish_branch "$PROJECT_ROOT" "prplos"

        # Update profile YAMLs for selected feeds
        for f in "${feeds[@]}" "${deep_feeds[@]}"; do
            info "Updating profile revision: $f → $branch_name"
            update_profile_revision "$f" "$branch_name"
        done

        # Set up feeds via src_local.sh
        for f in "${feeds[@]}"; do
            "$SRC_LOCAL" "$f:$branch_name"
        done
        for f in "${deep_feeds[@]}"; do
            "$SRC_LOCAL" --deep "$f:$branch_name"
        done
        for pkg in "${source_pkgs[@]}"; do
            "$SRC_LOCAL" --source "$pkg:$branch_name"
        done
    fi

    # Handle stale repos from previous feature
    local processed_feeds=("${feeds[@]}" "${deep_feeds[@]}")
    handle_stale_repos "$branch_name" "$shallow" processed_feeds

    echo ""
    do_status
}

# ============================================================
# Hotfix
# ============================================================

do_hotfix() {
    local branch_name="$1"; shift
    local from_release="" feeds=() source_pkgs=() deep_feeds=() shallow=false

    # Parse args
    while [[ $# -gt 0 ]]; do
        case "$1" in
            --from)    shift; from_release="${1:-}"; shift || die "--from requires a release branch" ;;
            --shallow) shallow=true; shift ;;
            --source)
                shift
                while [[ $# -gt 0 && "$1" != --* ]]; do
                    source_pkgs+=("$1"); shift
                done ;;
            --deep)
                shift
                while [[ $# -gt 0 && "$1" != --* ]]; do
                    deep_feeds+=("$1"); shift
                done ;;
            feed_*) feeds+=("$1"); shift ;;
            *)      die "Unknown argument: $1" ;;
        esac
    done

    # Default: promote plain feeds to deep (unless --shallow)
    if ! $shallow && [[ ${#feeds[@]} -gt 0 ]]; then
        deep_feeds+=("${feeds[@]}")
        feeds=()
    fi

    if branch_exists_anywhere "$PROJECT_ROOT" "$branch_name"; then
        # Switch to existing hotfix — reuse feature switch logic
        info "Switching to existing hotfix $branch_name..."
        local args=("$branch_name")
        [[ ${#feeds[@]} -gt 0 ]] && args+=("${feeds[@]}")
        [[ ${#source_pkgs[@]} -gt 0 ]] && args+=("--source" "${source_pkgs[@]}")
        [[ ${#deep_feeds[@]} -gt 0 ]] && args+=("--deep" "${deep_feeds[@]}")
        do_feature "${args[@]}"
        return
    fi

    # Create new hotfix
    [[ -n "$from_release" ]] || die "Hotfix creation requires --from release/<version>"
    [[ "$from_release" == release/* ]] || die "--from must be a release branch (release/<version>)"

    repo_is_dirty "$PROJECT_ROOT" && warn "prplos has uncommitted changes"

    info "Creating hotfix $branch_name from $from_release..."
    # Checkout release branch first
    if ! git -C "$PROJECT_ROOT" rev-parse --verify "$from_release" &>/dev/null; then
        git -C "$PROJECT_ROOT" fetch origin "$from_release" 2>/dev/null || true
    fi
    git -C "$PROJECT_ROOT" checkout "$from_release" 2>/dev/null || \
        git -C "$PROJECT_ROOT" checkout -b "$from_release" "origin/$from_release" 2>/dev/null || \
        die "Release branch '$from_release' not found"
    git -C "$PROJECT_ROOT" checkout -b "$branch_name"
    publish_branch "$PROJECT_ROOT" "prplos"

    # Update profile YAMLs for selected feeds
    for f in "${feeds[@]}" "${deep_feeds[@]}"; do
        info "Updating profile revision: $f → $branch_name"
        update_profile_revision "$f" "$branch_name"
    done

    # Set up feeds
    for f in "${feeds[@]}"; do
        "$SRC_LOCAL" "$f:$branch_name"
    done
    for f in "${deep_feeds[@]}"; do
        "$SRC_LOCAL" --deep "$f:$branch_name"
    done
    for pkg in "${source_pkgs[@]}"; do
        "$SRC_LOCAL" --source "$pkg:$branch_name"
    done

    # Handle stale repos from previous feature
    local processed_feeds=("${feeds[@]}" "${deep_feeds[@]}")
    handle_stale_repos "$branch_name" "$shallow" processed_feeds

    echo ""
    do_status
}

# ============================================================
# Release
# ============================================================

do_release() {
    local branch_name="$1"

    repo_is_dirty "$PROJECT_ROOT" && \
        die "prplos has uncommitted changes. Commit or stash before creating a release branch."

    # Check pinning (both levels)
    local errors=()
    while IFS='|' read -r name uri revision profile; do
        is_commit_pin "$revision" || errors+=("Feed $name (in $profile): tracking '$revision'")
    done < <(get_profile_feeds)

    # Check PKG_SOURCE_VERSION in any checked-out feeds
    while IFS= read -r dir; do
        [[ -n "$dir" ]] || continue
        local fname; fname=$(basename "$dir")
        while IFS= read -r makefile; do
            local proto; proto=$(extract_pkg_var "$makefile" "PKG_SOURCE_PROTO")
            [[ "$proto" == "git" ]] || continue
            local ver; ver=$(extract_pkg_var "$makefile" "PKG_SOURCE_VERSION")
            [[ -n "$ver" ]] || continue
            if ! [[ "$ver" =~ ^[0-9a-f]{40}$ ]]; then
                local pkg; pkg=$(basename "$(dirname "$makefile")")
                errors+=("$fname/$pkg: PKG_SOURCE_VERSION='$ver'")
            fi
        done < <(find "$dir" -maxdepth 5 -name Makefile -not -path '*/.git/*' 2>/dev/null)
    done < <(discover_feed_repos)

    if [[ ${#errors[@]} -gt 0 ]]; then
        echo "Cannot create release — unpinned revisions:"
        for e in "${errors[@]}"; do
            echo "  $e"
        done
        echo ""
        echo "Run 'src_local.sh --pin' to pin all revisions first."
        exit 1
    fi

    if branch_exists_anywhere "$PROJECT_ROOT" "$branch_name"; then
        info "Switching to existing release branch $branch_name..."
        git -C "$PROJECT_ROOT" checkout "$branch_name" 2>/dev/null || \
            git -C "$PROJECT_ROOT" checkout -b "$branch_name" "origin/$branch_name"
    else
        info "Creating release branch $branch_name from current HEAD..."
        git -C "$PROJECT_ROOT" checkout -b "$branch_name"
        publish_branch "$PROJECT_ROOT" "prplos"
    fi

    echo ""
    do_status
}

# ============================================================
# Main (switch to mainline)
# ============================================================

do_main() {
    repo_is_dirty "$PROJECT_ROOT" && warn "prplos has uncommitted changes"

    # Clean src-links
    if [[ -f "$AEI_DIR/.srclocal.state" ]]; then
        info "Cleaning src-links..."
        "$SRC_LOCAL" --clean
    fi

    # Switch to mainline
    info "Switching to $MAIN_BRANCH..."
    git -C "$PROJECT_ROOT" checkout "$MAIN_BRANCH"

    # Regenerate feeds
    if [[ -f "$FEEDS_CONF" ]]; then
        info "Updating feeds..."
        ( cd "$PROJECT_ROOT" && ./scripts/feeds update -a && ./scripts/feeds install -a ) || \
            warn "Feed update failed. Re-run gen_config.py with your target profiles."
    else
        warn "No feeds.conf found. Run gen_config.py with your target profiles."
    fi

    echo ""
    do_status
}

# ============================================================
# Sync
# ============================================================

do_sync() {
    local branch; branch=$(get_prplos_branch)
    local btype; btype=$(get_branch_type "$branch")

    [[ "$btype" == "feature" || "$btype" == "hotfix" ]] || \
        die "sync only works on feature/hotfix branches (current: $branch)"

    local switched=0 skipped=0

    # Sync feed repos
    while IFS= read -r dir; do
        [[ -n "$dir" ]] || continue
        local name; name=$(basename "$dir")
        is_owned_repo "$dir" || continue
        local cur; cur=$(repo_branch "$dir")
        [[ "$cur" != "$branch" ]] || continue
        if repo_is_dirty "$dir"; then
            warn "$name is dirty — skipping (commit or stash first)"
            skipped=$((skipped + 1))
            continue
        fi
        info "Switching $name from '$cur' to '$branch'..."
        git -C "$dir" fetch origin 2>/dev/null || true
        git -C "$dir" checkout "$branch" 2>/dev/null || \
            git -C "$dir" checkout -b "$branch" "origin/$branch" 2>/dev/null || \
            git -C "$dir" checkout -b "$branch"
        switched=$((switched + 1))
    done < <(discover_feed_repos)

    # Sync module repos
    while IFS= read -r dir; do
        [[ -n "$dir" ]] || continue
        local pkg; pkg=$(basename "$dir")
        is_owned_repo "$dir" || continue
        local cur; cur=$(repo_branch "$dir")
        [[ "$cur" != "$branch" ]] || continue
        if repo_is_dirty "$dir"; then
            warn "aei/src/$pkg is dirty — skipping (commit or stash first)"
            skipped=$((skipped + 1))
            continue
        fi
        info "Switching aei/src/$pkg from '$cur' to '$branch'..."
        git -C "$dir" fetch origin 2>/dev/null || true
        git -C "$dir" checkout "$branch" 2>/dev/null || \
            git -C "$dir" checkout -b "$branch" "origin/$branch" 2>/dev/null || \
            git -C "$dir" checkout -b "$branch"
        switched=$((switched + 1))
    done < <(discover_source_repos)

    # Summary
    info "Synced: $switched repo(s) switched, $skipped skipped"
    [[ "$skipped" -eq 0 ]] || warn "Commit or stash dirty repos, then re-run sync"
}

# ============================================================
# Main Dispatch
# ============================================================

show_help() {
    cat <<'USAGE'
Usage: gw3_branch.sh [options] [command] [args...]

Status:
  (no args)              Status report (brief if clean, detailed if issues)
  --verbose              Force detailed status output

Operations:
  pull                   Pull all repos + status
  commit                 Coordinated commit across all dirty repos
  push                   Push all repos with unpushed commits
  sync                   Switch all feed/module repos to match prplos branch
  pr [--dry-run] [--reviewer <user>...]
                         Bottom-up PR workflow (modules → feeds → prplos)

Branch management:
  feature/<name> [feeds..] [--source pkg...] [--shallow]
                         Create/switch/extend feature branch
                         Feeds get deep treatment by default (feed + module tiers)
                         Use --shallow for feed tier only (no module clones)
  hotfix/<name> [--from release/<ver>] [feeds..] [--shallow]
                         Create/switch hotfix branch
  release/<name>         Create/switch release branch
  main                   Switch to mainline (cleans src-links)

Stale repo handling:
  When switching features, repos in aei/ on a different branch are
  detected and you are prompted to add or remove each one.
  Prompts: y=add, N=remove (default), a=add all, r=remove all
  Deep mode (default): adding a feed also switches all its module repos.
  Repos with unpushed commits trigger a warning before removal.

Examples:
  gw3_branch.sh                                    # Status report
  gw3_branch.sh pull                               # Pull all repos
  gw3_branch.sh commit                             # Commit all dirty repos
  gw3_branch.sh push                               # Push all repos
  gw3_branch.sh feature/my-feat feed_aei           # Create feature (deep by default)
  gw3_branch.sh feature/my-feat --shallow feed_aei # Feed only, no module clones
  gw3_branch.sh feature/my-feat --source mod-x     # Add specific module repo
  gw3_branch.sh hotfix/fix-1 --from release/1.0 feed_aei
  gw3_branch.sh release/1.0                        # Create release branch
  gw3_branch.sh main                               # Switch to mainline
  gw3_branch.sh pr                                 # Bottom-up PR workflow
  gw3_branch.sh pr --dry-run                       # Preview without changes
  gw3_branch.sh pr --reviewer alice --reviewer bob  # Override reviewers
USAGE
}

# Parse global options
while [[ $# -gt 0 ]]; do
    case "$1" in
        --verbose) VERBOSE=true; shift ;;
        --help|-h) show_help; exit 0 ;;
        *)         break ;;
    esac
done

case "${1:-}" in
    "")         do_status ;;
    pull)       do_pull ;;
    commit)     do_commit ;;
    push)       do_push ;;
    feature/*)  do_feature "$@" ;;
    hotfix/*)   do_hotfix "$@" ;;
    release/*)  do_release "$1" ;;
    sync)       do_sync ;;
    pr)         do_pr "$@" ;;
    main)       do_main ;;
    *)          die "Unknown command: $1. Run with --help for usage." ;;
esac
