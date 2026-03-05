#!/bin/bash
# ============================================================
# gw3_branch_pr.sh — PR workflow for GW3 multi-repo branches
# ============================================================
#
# Sourced by gw3_branch.sh. All utility functions and variables
# (PROJECT_ROOT, AEI_DIR, SRC_DIR, FEED_MAIN_BRANCH, etc.) are
# available from the parent script.
#
# Implements the bottom-up PR workflow:
#   modules → feeds → prplos
# Each tier is gated on the completion of the tier below.

# ============================================================
# PR Summary Tracking
# ============================================================

declare -a PR_SUMMARY_NAMES=()
declare -a PR_SUMMARY_TYPES=()     # "module" | "feed" | "project"
declare -a PR_SUMMARY_STATUSES=()  # "merged" | "PR #N open" | "PR #N created" | "blocked" | "no changes"
declare -a PR_SUMMARY_ACTIONS=()   # "pinned → abc1234" | "waiting for review" | etc.

summary_add() {
    PR_SUMMARY_NAMES+=("$1")
    PR_SUMMARY_TYPES+=("$2")
    PR_SUMMARY_STATUSES+=("$3")
    PR_SUMMARY_ACTIONS+=("$4")
}

print_summary() {
    local branch="$1"

    echo ""
    echo "═══════════════════════════════════════════════════════════════"
    echo "  gw3-branch pr — $branch"
    echo "═══════════════════════════════════════════════════════════════"
    echo ""

    # Column widths
    local name_w=28 status_w=20

    printf "  %-${name_w}s %-${status_w}s %s\n" "Module/Feed" "Status" "Action"
    echo "  ─────────────────────────────────────────────────────────────"

    local prev_type=""
    for i in "${!PR_SUMMARY_NAMES[@]}"; do
        local cur_type="${PR_SUMMARY_TYPES[$i]}"
        # Print separator between tiers
        if [[ -n "$prev_type" && "$prev_type" != "$cur_type" ]]; then
            echo "  ─────────────────────────────────────────────────────────────"
        fi
        printf "  %-${name_w}s %-${status_w}s %s\n" \
            "${PR_SUMMARY_NAMES[$i]}" \
            "${PR_SUMMARY_STATUSES[$i]}" \
            "${PR_SUMMARY_ACTIONS[$i]}"
        prev_type="$cur_type"
    done

    echo ""
    echo "═══════════════════════════════════════════════════════════════"

    # Next steps
    local -a next_steps=()
    for i in "${!PR_SUMMARY_STATUSES[@]}"; do
        local status="${PR_SUMMARY_STATUSES[$i]}"
        local name="${PR_SUMMARY_NAMES[$i]}"
        if [[ "$status" == PR\ *\ open || "$status" == PR\ *\ created ]]; then
            local pr_num="${status#PR #}"
            pr_num="${pr_num%% *}"
            next_steps+=("Review and merge PR #$pr_num ($name) on Bitbucket")
        fi
    done

    if [[ ${#next_steps[@]} -gt 0 ]]; then
        echo "  Next steps:"
        for step in "${next_steps[@]}"; do
            echo "  - $step"
        done
        echo "  - Re-run: gw3-branch pr"
        echo "═══════════════════════════════════════════════════════════════"
    fi
}

# ============================================================
# PR Helper Functions
# ============================================================

# Extract Jira ticket from branch name or commit messages.
# Looks for patterns like S12W01-1234, PROJ-123, etc.
# Args: $1=branch_name  $2=commit_log (optional)
extract_jira_ticket() {
    local branch="$1"
    local commit_log="${2:-}"

    # Try branch name first (e.g., feature/S12W01-1234-description)
    local ticket
    ticket=$(echo "$branch" | grep -oP '[A-Z][A-Z0-9]+-[0-9]+' | head -1)
    if [[ -n "$ticket" ]]; then
        echo "$ticket"
        return 0
    fi

    # Fall back to first commit message that contains a ticket
    if [[ -n "$commit_log" ]]; then
        ticket=$(echo "$commit_log" | grep -oP '[A-Z][A-Z0-9]+-[0-9]+' | head -1)
        if [[ -n "$ticket" ]]; then
            echo "$ticket"
            return 0
        fi
    fi

    return 1
}

# Build a PR title with Jira ticket prefix.
# Args: $1=branch_name  $2=suffix (e.g., "module_name" or "merge to r3_mainline")
make_pr_title() {
    local branch="$1"
    local suffix="$2"
    local ticket; ticket=$(extract_jira_ticket "$branch") || true

    if [[ -n "$ticket" ]]; then
        echo "$ticket $suffix"
    else
        echo "$branch: $suffix"
    fi
}

# Build a PR description body with structured formatting.
# Args: $1=branch_name  $2=commit_log
make_pr_body() {
    local branch="$1"
    local commit_log="$2"
    local ticket; ticket=$(extract_jira_ticket "$branch" "$commit_log") || true
    local commit_count; commit_count=$(echo "$commit_log" | wc -l)

    local body=""
    [[ -n "$ticket" ]] && body+="$ticket\n\n"
    body+="## Summary\n\n"
    body+="Branch: \`$branch\`\n\n"
    body+="## Commits ($commit_count)\n\n"

    while IFS= read -r line; do
        body+="- $line\n"
    done <<< "$commit_log"

    echo -e "$body"
}

# Update an existing PR's description (body only) via bkt pr edit.
# Does NOT touch the title — preserve whatever was set at creation or by the user.
# Args: $1=slug  $2=pr_id  $3=description  $4=label (for logging)
update_pr_description() {
    local slug="$1" pr_id="$2" description="$3" label="${4:-repo}"
    if bkt pr edit "$pr_id" --repo "$slug" --body "$description" &>/dev/null; then
        info "$label: Updated PR #$pr_id description"
    else
        warn "$label: Failed to update PR #$pr_id description (non-fatal)"
    fi
}

# ============================================================
# Jira Integration
# ============================================================

JIRA_COMMENT_SCRIPT="$PROJECT_ROOT/.github/skills/jira-communication/scripts/workflow/jira-comment.py"
JIRA_ISSUE_SCRIPT="$PROJECT_ROOT/.github/skills/jira-communication/scripts/core/jira-issue.py"

# Update Jira ticket with PR status summary.
# Adds a comment with the current PR status table, ensures
# the feature branch is set as a label, and reassigns to the
# first PR reviewer (if any).
# Args: $1=branch  $2=dry_run
update_jira_ticket() {
    local branch="$1" dry_run="$2"

    # Extract ticket from branch or commit log
    local commit_log; commit_log=$(git -C "$PROJECT_ROOT" log --oneline "origin/$MAIN_BRANCH..HEAD" 2>/dev/null || true)
    local ticket; ticket=$(extract_jira_ticket "$branch" "$commit_log") || true
    [[ -n "$ticket" ]] || return 0  # No ticket found — nothing to do

    # Check prerequisites
    command -v uv &>/dev/null || { warn "uv not found — skipping Jira update"; return 0; }
    [[ -f "$JIRA_COMMENT_SCRIPT" ]] || { warn "Jira skill not installed — skipping Jira update"; return 0; }

    if $dry_run; then
        info "Jira: would update $ticket with PR status"
        return 0
    fi

    # Build Jira wiki markup comment (NOT Markdown)
    local comment="h3. PR Workflow Status\n\n"
    comment+="*Branch:* {{$branch}}\n"

    # Build status table
    comment+="||Repo||Type||Status||Detail||\n"
    for i in "${!PR_SUMMARY_NAMES[@]}"; do
        comment+="|${PR_SUMMARY_NAMES[$i]}|${PR_SUMMARY_TYPES[$i]}|${PR_SUMMARY_STATUSES[$i]}|${PR_SUMMARY_ACTIONS[$i]}|\n"
    done

    # Add PR links where available
    for i in "${!PR_SUMMARY_STATUSES[@]}"; do
        local status="${PR_SUMMARY_STATUSES[$i]}"
        local name="${PR_SUMMARY_NAMES[$i]}"
        if [[ "$status" =~ PR\ \#([0-9]+) ]]; then
            local pr_num="${BASH_REMATCH[1]}"
            local slug; slug=$(basename "${name}")
            # Use prplos slug for project-level PR
            [[ "${PR_SUMMARY_TYPES[$i]}" == "project" ]] && slug="prplos"
            comment+="\n[PR #$pr_num ($name)|https://bitbucket.org/Actiontec/$slug/pull-requests/$pr_num]"
        fi
    done

    # Post comment
    if uv run "$JIRA_COMMENT_SCRIPT" add "$ticket" "$(echo -e "$comment")" &>/dev/null; then
        info "Jira: Updated $ticket with PR status"
    else
        warn "Jira: Failed to update $ticket (non-fatal)"
    fi

    # Ensure branch label is set (fetch existing, append if missing)
    local existing_labels
    existing_labels=$(uv run "$JIRA_ISSUE_SCRIPT" --json get "$ticket" 2>/dev/null | \
        python3 -c "import sys,json; d=json.load(sys.stdin); print(','.join(d.get('fields',{}).get('labels',[])))" 2>/dev/null) || true
    if [[ -z "$existing_labels" ]]; then
        uv run "$JIRA_ISSUE_SCRIPT" update "$ticket" --labels "$branch" &>/dev/null || true
    elif [[ ",$existing_labels," != *",$branch,"* ]]; then
        uv run "$JIRA_ISSUE_SCRIPT" update "$ticket" --labels "$existing_labels,$branch" &>/dev/null || true
    fi

    # Reassign to the first PR reviewer (if any were specified via --reviewer)
    if [[ ${#PR_REVIEWERS[@]} -gt 0 && -n "${PR_REVIEWERS[0]:-}" ]]; then
        local reviewer="${PR_REVIEWERS[0]}"
        if uv run "$JIRA_ISSUE_SCRIPT" update "$ticket" --assignee "$reviewer" &>/dev/null; then
            info "Jira: Reassigned $ticket to $reviewer"
        else
            warn "Jira: Failed to reassign $ticket to $reviewer (non-fatal)"
        fi
    fi
}

# Get the remote default branch for a repo (the PR target)
# Uses origin/HEAD, falls back to probing common branch names
get_target_branch() {
    local dir="$1"
    # Check origin/HEAD symbolic ref
    local ref; ref=$(git -C "$dir" symbolic-ref refs/remotes/origin/HEAD 2>/dev/null)
    if [[ -n "$ref" ]]; then
        echo "${ref#refs/remotes/origin/}"
        return 0
    fi
    # Probe common default branch names
    for candidate in "$FEED_MAIN_BRANCH" "r3_main" "master"; do
        if git -C "$dir" rev-parse --verify "origin/$candidate" &>/dev/null; then
            echo "$candidate"
            return 0
        fi
    done
    echo "$FEED_MAIN_BRANCH"  # ultimate fallback
}

# Extract Bitbucket repo slug from git remote URL
# git@bitbucket.org:Actiontec/feed-aei.git → feed-aei
# https://bitbucket.org/Actiontec/feed-aei.git → feed-aei
get_repo_slug() {
    local dir="$1"
    local url; url=$(git -C "$dir" remote get-url origin 2>/dev/null)
    basename "${url%.git}"
}

# Find open PR from source branch on Bitbucket
# Returns PR ID or empty string
find_open_pr() {
    local slug="$1" source_branch="$2"
    bkt pr list --repo "$slug" --state OPEN --json 2>/dev/null | \
        python3 -c "
import sys, json
try:
    data = json.load(sys.stdin)
    prs = data if isinstance(data, list) else data.get('pull_requests', data.get('values', []))
    for pr in prs:
        src = pr.get('source', {})
        branch = src.get('branch', src.get('name', ''))
        if isinstance(branch, dict):
            branch = branch.get('name', '')
        if branch == '$source_branch':
            print(pr.get('id', ''))
            break
except Exception:
    pass
"
}

# Check for uncommitted changes, exit 1 if dirty
ensure_clean() {
    local dir="$1" label="$2"
    if repo_is_dirty "$dir"; then
        die "$label has uncommitted changes. Commit or discard changes, then re-run 'gw3-branch pr'."
    fi
}

# Fetch + rebase onto target branch, exit 1 on conflict
rebase_onto() {
    local dir="$1" label="$2" target="${3:-$FEED_MAIN_BRANCH}"
    git -C "$dir" fetch origin 2>/dev/null || die "Failed to fetch origin for $label"
    if ! git -C "$dir" rebase "origin/$target" 2>/dev/null; then
        git -C "$dir" rebase --abort 2>/dev/null || true
        die "Rebase conflict in $label. Resolve manually:\n  cd $dir && git rebase origin/$target\nFix conflicts, then re-run 'gw3-branch pr'."
    fi
}

# Push with --force-with-lease after rebase
push_force_lease() {
    local dir="$1" label="$2"
    local branch; branch=$(repo_branch "$dir")
    git -C "$dir" push --force-with-lease origin "$branch" 2>/dev/null || \
        die "Failed to push $label. Check remote state."
}

# Pin a module's commit in its feed Makefile
pin_module_in_feed() {
    local module_name="$1" pin_sha="$2" feed_dir="$3"
    local makefile="$feed_dir/$module_name/Makefile"
    [[ -f "$makefile" ]] || return 1
    sed -i "s|^\(PKG_SOURCE_VERSION\s*[:?]\?=\s*\).*|\1${pin_sha}|" "$makefile"
    if grep -qE '^PKG_MIRROR_HASH\s*[:?]?=' "$makefile"; then
        sed -i "s|^\(PKG_MIRROR_HASH\s*[:?]\?=\s*\).*|\1skip|" "$makefile"
    fi
    if grep -qE '^PKG_HASH\s*[:?]?=' "$makefile"; then
        sed -i "s|^\(PKG_HASH\s*[:?]\?=\s*\).*|\1skip|" "$makefile"
    fi
}

# Remove module from aei/src/ and .srclocal-source.state
cleanup_merged_module() {
    local module_name="$1"
    rm -rf "$SRC_DIR/$module_name"
    local state_file="$AEI_DIR/.srclocal-source.state"
    if [[ -f "$state_file" ]]; then
        sed -i "/^${module_name}=/d" "$state_file"
        [[ -s "$state_file" ]] || rm -f "$state_file"
    fi
}

# Map module name to its parent feed directory
find_module_feed() {
    local module_name="$1"
    while IFS= read -r feed_dir; do
        [[ -n "$feed_dir" ]] || continue
        if [[ -f "$feed_dir/$module_name/Makefile" ]]; then
            echo "$feed_dir"
            return 0
        fi
    done < <(discover_feed_repos)
    return 1
}

# Read feed processing order from config or use defaults
get_feed_order() {
    local config="$AEI_DIR/.pr-config.yaml"
    local -a tracked_feeds=()

    # Get all localized (branch-tracking) feed names
    while IFS='|' read -r name uri revision profile; do
        tracked_feeds+=("$name")
    done < <(get_tracked_feeds)

    [[ ${#tracked_feeds[@]} -gt 0 ]] || return 0

    local -a ordered=()

    if [[ -f "$config" ]]; then
        # Read explicit order from config
        local -a config_order=()
        while IFS= read -r line; do
            [[ -n "$line" ]] && config_order+=("$line")
        done < <(python3 -c "
import yaml
with open('$config') as f:
    data = yaml.safe_load(f) or {}
for name in data.get('feed_order', []):
    print(name)
" 2>/dev/null)

        # Tracked feeds NOT in config → alphabetical first
        local -a unlisted=()
        for f in "${tracked_feeds[@]}"; do
            local found=false
            for c in "${config_order[@]}"; do
                [[ "$f" == "$c" ]] && { found=true; break; }
            done
            $found || unlisted+=("$f")
        done
        IFS=$'\n' unlisted=($(sort <<<"${unlisted[*]}")); unset IFS

        # Unlisted alphabetically, then config-listed in order (if tracked)
        ordered=("${unlisted[@]}")
        for c in "${config_order[@]}"; do
            for f in "${tracked_feeds[@]}"; do
                [[ "$f" == "$c" ]] && { ordered+=("$f"); break; }
            done
        done
    else
        # Default: alphabetical, feed_prplmesh second-to-last, feed_aei last
        local -a normal=()
        local has_prplmesh=false has_aei=false
        for f in "${tracked_feeds[@]}"; do
            case "$f" in
                feed_prplmesh) has_prplmesh=true ;;
                feed_aei)      has_aei=true ;;
                *)             normal+=("$f") ;;
            esac
        done
        IFS=$'\n' normal=($(sort <<<"${normal[*]}")); unset IFS
        ordered=("${normal[@]}")
        $has_prplmesh && ordered+=("feed_prplmesh")
        $has_aei && ordered+=("feed_aei")
    fi

    printf '%s\n' "${ordered[@]}"
}

# ============================================================
# Core PR Processing Functions
# ============================================================

# Process a single module repo
# Returns 0 if pinned (merged), 1 if pending/created
process_module() {
    local module_dir="$1" feed_dir="$2" branch="$3" dry_run="$4"
    local module_name; module_name=$(basename "$module_dir")
    local feed_name; feed_name=$(basename "$feed_dir")
    local target; target=$(get_target_branch "$module_dir")

    # Step 3.1: Safety check
    ensure_clean "$module_dir" "Module $module_name"

    if ! $dry_run; then
        # Step 3.2: Fetch and rebase
        rebase_onto "$module_dir" "module $module_name" "$target"

        # Step 3.3: Push
        push_force_lease "$module_dir" "module $module_name"
    else
        # Dry run: just fetch to get accurate divergence count
        git -C "$module_dir" fetch origin 2>/dev/null || true
    fi

    # Step 3.4: PR state detection
    # Verify target ref exists (rev-list silently fails → false "merged" detection)
    if ! git -C "$module_dir" rev-parse --verify "origin/$target" &>/dev/null; then
        die "Module $module_name: remote branch 'origin/$target' not found. Run: git -C $module_dir fetch origin"
    fi

    local diff_count
    diff_count=$(git -C "$module_dir" rev-list --count "origin/$target..HEAD" 2>/dev/null || echo "0")

    if [[ "$diff_count" -eq 0 ]]; then
        # Merged state — pin and clean up
        local pin_sha; pin_sha=$(git -C "$module_dir" rev-parse --verify "origin/$target" 2>/dev/null)
        local short_sha="${pin_sha:0:12}"

        if $dry_run; then
            info "Module $module_name: merged (would pin → $short_sha)"
            summary_add "$module_name" "module" "merged" "would pin → $short_sha (dry run)"
        else
            info "Module $module_name: merged — pinning to $short_sha"
            pin_module_in_feed "$module_name" "$pin_sha" "$feed_dir"

            # Auto-commit pin in feed repo
            git -C "$feed_dir" add "$module_name/Makefile" 2>/dev/null || true
            git -C "$feed_dir" commit -m "Pin $module_name to ${short_sha} (merged to $target)" 2>/dev/null || true

            # Clean up module directory and state
            cleanup_merged_module "$module_name"

            summary_add "$module_name" "module" "merged" "pinned → $short_sha"
        fi
        return 0
    fi

    # DIFF_COUNT > 0 — check for existing PR
    local slug; slug=$(get_repo_slug "$module_dir")
    local pr_id; pr_id=$(find_open_pr "$slug" "$branch")

    if [[ -n "$pr_id" ]]; then
        # Update description with latest commits
        if ! $dry_run; then
            local commit_log; commit_log=$(git -C "$module_dir" log --oneline "origin/$target..HEAD" 2>/dev/null)
            local pr_body; pr_body=$(make_pr_body "$branch" "$commit_log")
            update_pr_description "$slug" "$pr_id" "$pr_body" "Module $module_name"
        fi
        info "Module $module_name: PR #$pr_id is open — waiting for review/merge"
        summary_add "$module_name" "module" "PR #$pr_id open" "waiting for review"
        return 1
    fi

    # No open PR — create one
    if $dry_run; then
        info "Module $module_name: would create PR ($diff_count commit(s) ahead of $target)"
        summary_add "$module_name" "module" "needs PR" "would create PR (dry run)"
        return 1
    fi

    local commit_log; commit_log=$(git -C "$module_dir" log --oneline "origin/$target..HEAD" 2>/dev/null)
    local pr_title; pr_title=$(make_pr_title "$branch" "$module_name")
    local pr_body; pr_body=$(make_pr_body "$branch" "$commit_log")
    local -a pr_args=(
        pr create
        --repo "$slug"
        --title "$pr_title"
        --source "$branch"
        --target "$target"
        --description "$pr_body"
        --close-source
    )

    # Add reviewers if specified
    for r in "${PR_REVIEWERS[@]:-}"; do
        [[ -n "$r" ]] && pr_args+=(--reviewer "$r")
    done

    local pr_output
    if pr_output=$(bkt "${pr_args[@]}" 2>&1); then
        # Extract PR ID from bkt output
        local new_pr_id
        new_pr_id=$(echo "$pr_output" | grep -oP '#\K[0-9]+' | head -1)
        [[ -n "$new_pr_id" ]] || new_pr_id="?"
        info "Module $module_name: Created PR #$new_pr_id"
        echo "$pr_output"
        summary_add "$module_name" "module" "PR #$new_pr_id created" "waiting for review"
    else
        warn "Failed to create PR for module $module_name: $pr_output"
        summary_add "$module_name" "module" "PR failed" "bkt error — create manually"
    fi
    return 1
}

# Process a single feed repo
# Returns 0 if pinned (merged), 1 if pending/created
process_feed() {
    local feed_dir="$1" branch="$2" dry_run="$3"
    local feed_name; feed_name=$(basename "$feed_dir")
    local target; target=$(get_target_branch "$feed_dir")

    # Step 4.1: Safety check — allow pin-update commits (already staged+committed)
    # Only fail on truly uncommitted changes
    ensure_clean "$feed_dir" "Feed $feed_name"

    if ! $dry_run; then
        # Step 4.2: Fetch and rebase
        rebase_onto "$feed_dir" "feed $feed_name" "$target"

        # Step 4.3: Push
        push_force_lease "$feed_dir" "feed $feed_name"
    else
        git -C "$feed_dir" fetch origin 2>/dev/null || true
    fi

    # Step 4.4: PR state detection
    # Verify target ref exists (rev-list silently fails → false "merged" detection)
    if ! git -C "$feed_dir" rev-parse --verify "origin/$target" &>/dev/null; then
        die "Feed $feed_name: remote branch 'origin/$target' not found. Run: git -C $feed_dir fetch origin"
    fi

    local diff_count
    diff_count=$(git -C "$feed_dir" rev-list --count "origin/$target..HEAD" 2>/dev/null || echo "0")

    if [[ "$diff_count" -eq 0 ]]; then
        # Merged state — pin in prplos profile YAML
        local pin_sha; pin_sha=$(git -C "$feed_dir" rev-parse --verify "origin/$target" 2>/dev/null)
        local short_sha="${pin_sha:0:12}"

        if $dry_run; then
            info "Feed $feed_name: merged (would pin → $short_sha)"
            summary_add "$feed_name" "feed" "merged" "would pin → $short_sha (dry run)"
        else
            info "Feed $feed_name: merged — pinning to $short_sha"
            update_profile_revision "$feed_name" "$pin_sha"

            # Auto-commit pin in prplos
            git -C "$PROJECT_ROOT" add profiles/ 2>/dev/null || true
            git -C "$PROJECT_ROOT" commit -m "Pin $feed_name to ${short_sha} (merged to $target)" 2>/dev/null || true

            summary_add "$feed_name" "feed" "merged" "pinned → $short_sha"
        fi
        return 0
    fi

    # DIFF_COUNT > 0 — check for existing PR
    local slug; slug=$(get_repo_slug "$feed_dir")
    local pr_id; pr_id=$(find_open_pr "$slug" "$branch")

    if [[ -n "$pr_id" ]]; then
        # Update description with latest commits
        if ! $dry_run; then
            local commit_log; commit_log=$(git -C "$feed_dir" log --oneline "origin/$target..HEAD" 2>/dev/null)
            local pr_body; pr_body=$(make_pr_body "$branch" "$commit_log")
            update_pr_description "$slug" "$pr_id" "$pr_body" "Feed $feed_name"
        fi
        info "Feed $feed_name: PR #$pr_id is open — waiting for review/merge"
        summary_add "$feed_name" "feed" "PR #$pr_id open" "waiting for review"
        return 1
    fi

    # No open PR — create one
    if $dry_run; then
        info "Feed $feed_name: would create PR ($diff_count commit(s) ahead of $target)"
        summary_add "$feed_name" "feed" "needs PR" "would create PR (dry run)"
        return 1
    fi

    local commit_log; commit_log=$(git -C "$feed_dir" log --oneline "origin/$target..HEAD" 2>/dev/null)
    local pr_title; pr_title=$(make_pr_title "$branch" "$feed_name")
    local pr_body; pr_body=$(make_pr_body "$branch" "$commit_log")
    local -a pr_args=(
        pr create
        --repo "$slug"
        --title "$pr_title"
        --source "$branch"
        --target "$target"
        --description "$pr_body"
        --close-source
    )

    for r in "${PR_REVIEWERS[@]:-}"; do
        [[ -n "$r" ]] && pr_args+=(--reviewer "$r")
    done

    local pr_output
    if pr_output=$(bkt "${pr_args[@]}" 2>&1); then
        local new_pr_id
        new_pr_id=$(echo "$pr_output" | grep -oP '#\K[0-9]+' | head -1)
        [[ -n "$new_pr_id" ]] || new_pr_id="?"
        info "Feed $feed_name: Created PR #$new_pr_id"
        echo "$pr_output"
        summary_add "$feed_name" "feed" "PR #$new_pr_id created" "waiting for review"
    else
        warn "Failed to create PR for feed $feed_name: $pr_output"
        summary_add "$feed_name" "feed" "PR failed" "bkt error — create manually"
    fi
    return 1
}

# Process prplos — final PR
# Returns 0 if merged, 1 if pending/created
process_project() {
    local branch="$1" dry_run="$2"
    local target="$MAIN_BRANCH"

    # Step 5.1: Safety check
    ensure_clean "$PROJECT_ROOT" "prplos"

    if ! $dry_run; then
        # Step 5.2: Fetch and rebase
        rebase_onto "$PROJECT_ROOT" "prplos" "$target"

        # Step 5.3: Push
        push_force_lease "$PROJECT_ROOT" "prplos"
    else
        git -C "$PROJECT_ROOT" fetch origin 2>/dev/null || true
    fi

    # Step 5.4: PR state detection
    if ! git -C "$PROJECT_ROOT" rev-parse --verify "origin/$target" &>/dev/null; then
        die "prplos: remote branch 'origin/$target' not found. Run: git fetch origin"
    fi

    local diff_count
    diff_count=$(git -C "$PROJECT_ROOT" rev-list --count "origin/$target..HEAD" 2>/dev/null || echo "0")

    if [[ "$diff_count" -eq 0 ]]; then
        info "Feature $branch is fully merged to $target."
        summary_add "prplos" "project" "merged" "fully merged"
        return 0
    fi

    # Check for existing PR
    local slug; slug=$(get_repo_slug "$PROJECT_ROOT")
    local pr_id; pr_id=$(find_open_pr "$slug" "$branch")

    if [[ -n "$pr_id" ]]; then
        # Update description with latest commits and dependency status
        if ! $dry_run; then
            local commit_log; commit_log=$(git -C "$PROJECT_ROOT" log --oneline "origin/$target..HEAD" 2>/dev/null)
            local jira_ticket; jira_ticket=$(extract_jira_ticket "$branch" "$commit_log")
            local commit_count; commit_count=$(echo "$commit_log" | wc -l)

            local description=""
            [[ -n "$jira_ticket" ]] && description+="$jira_ticket\n\n"
            description+="## Summary\n\n"
            description+="Feature branch: \`$branch\` → \`$target\`\n\n"

            local has_feed_section=false
            for i in "${!PR_SUMMARY_NAMES[@]}"; do
                if [[ "${PR_SUMMARY_TYPES[$i]}" == "feed" ]]; then
                    if ! $has_feed_section; then
                        description+="## Feed Dependencies\n\n"
                        description+="| Feed | Status | Detail |\n"
                        description+="|------|--------|--------|\n"
                        has_feed_section=true
                    fi
                    description+="| ${PR_SUMMARY_NAMES[$i]} | ${PR_SUMMARY_STATUSES[$i]} | ${PR_SUMMARY_ACTIONS[$i]} |\n"
                fi
            done
            $has_feed_section && description+="\n"

            local has_module_section=false
            for i in "${!PR_SUMMARY_NAMES[@]}"; do
                if [[ "${PR_SUMMARY_TYPES[$i]}" == "module" ]]; then
                    if ! $has_module_section; then
                        description+="## Module Dependencies\n\n"
                        description+="| Module | Status | Detail |\n"
                        description+="|--------|--------|--------|\n"
                        has_module_section=true
                    fi
                    description+="| ${PR_SUMMARY_NAMES[$i]} | ${PR_SUMMARY_STATUSES[$i]} | ${PR_SUMMARY_ACTIONS[$i]} |\n"
                fi
            done
            $has_module_section && description+="\n"

            description+="## Commits ($commit_count)\n\n"
            while IFS= read -r line; do
                description+="- $line\n"
            done <<< "$commit_log"

            update_pr_description "$slug" "$pr_id" "$(echo -e "$description")" "prplos"
        fi
        info "prplos: PR #$pr_id is open — waiting for review/merge"
        summary_add "prplos" "project" "PR #$pr_id open" "waiting for review"
        return 1
    fi

    # No open PR — create one
    if $dry_run; then
        info "prplos: would create PR ($diff_count commit(s) ahead of $target)"
        summary_add "prplos" "project" "needs PR" "would create PR (dry run)"
        return 1
    fi

    # Build aggregate description
    local commit_log; commit_log=$(git -C "$PROJECT_ROOT" log --oneline "origin/$target..HEAD" 2>/dev/null)
    local jira_ticket; jira_ticket=$(extract_jira_ticket "$branch" "$commit_log")
    local commit_count; commit_count=$(echo "$commit_log" | wc -l)

    local description=""
    [[ -n "$jira_ticket" ]] && description+="$jira_ticket\n\n"
    description+="## Summary\n\n"
    description+="Feature branch: \`$branch\` → \`$target\`\n\n"

    # Feed dependency table
    local has_feed_section=false
    for i in "${!PR_SUMMARY_NAMES[@]}"; do
        if [[ "${PR_SUMMARY_TYPES[$i]}" == "feed" ]]; then
            if ! $has_feed_section; then
                description+="## Feed Dependencies\n\n"
                description+="| Feed | Status | Detail |\n"
                description+="|------|--------|--------|\n"
                has_feed_section=true
            fi
            description+="| ${PR_SUMMARY_NAMES[$i]} | ${PR_SUMMARY_STATUSES[$i]} | ${PR_SUMMARY_ACTIONS[$i]} |\n"
        fi
    done
    $has_feed_section && description+="\n"

    # Module table (if any)
    local has_module_section=false
    for i in "${!PR_SUMMARY_NAMES[@]}"; do
        if [[ "${PR_SUMMARY_TYPES[$i]}" == "module" ]]; then
            if ! $has_module_section; then
                description+="## Module Dependencies\n\n"
                description+="| Module | Status | Detail |\n"
                description+="|--------|--------|--------|\n"
                has_module_section=true
            fi
            description+="| ${PR_SUMMARY_NAMES[$i]} | ${PR_SUMMARY_STATUSES[$i]} | ${PR_SUMMARY_ACTIONS[$i]} |\n"
        fi
    done
    $has_module_section && description+="\n"

    # Commit list as bullet points
    description+="## Commits ($commit_count)\n\n"
    while IFS= read -r line; do
        description+="- $line\n"
    done <<< "$commit_log"

    local pr_title; pr_title=$(make_pr_title "$branch" "merge to $target")

    local -a pr_args=(
        pr create
        --repo "$slug"
        --title "$pr_title"
        --source "$branch"
        --target "$target"
        --description "$(echo -e "$description")"
        --close-source
    )

    for r in "${PR_REVIEWERS[@]:-}"; do
        [[ -n "$r" ]] && pr_args+=(--reviewer "$r")
    done

    local pr_output
    if pr_output=$(bkt "${pr_args[@]}" 2>&1); then
        local new_pr_id
        new_pr_id=$(echo "$pr_output" | grep -oP '#\K[0-9]+' | head -1)
        [[ -n "$new_pr_id" ]] || new_pr_id="?"
        info "prplos: Created PR #$new_pr_id"
        echo "$pr_output"
        summary_add "prplos" "project" "PR #$new_pr_id created" "waiting for review"
    else
        warn "Failed to create PR for prplos: $pr_output"
        summary_add "prplos" "project" "PR failed" "bkt error — create manually"
    fi
    return 1
}

# ============================================================
# Main PR Entry Point
# ============================================================

do_pr() {
    shift  # remove "pr" from args
    local dry_run=false
    declare -a PR_REVIEWERS=()

    # Parse options
    while [[ $# -gt 0 ]]; do
        case "$1" in
            --dry-run)    dry_run=true; shift ;;
            --reviewer)   [[ -n "${2:-}" ]] || die "--reviewer requires a username"
                          PR_REVIEWERS+=("$2"); shift 2 ;;
            *)            die "Unknown pr option: $1" ;;
        esac
    done

    # Phase 1: Validation
    local branch; branch=$(get_prplos_branch)
    local btype; btype=$(get_branch_type "$branch")
    [[ "$btype" == "feature" ]] || die "PR workflow is only supported on feature branches. Hotfix support is planned for a future release."
    command -v bkt &>/dev/null || die "bkt CLI not found. Install from .github/skills/bkt/"
    bkt auth status &>/dev/null || die "bkt not authenticated. Run: bkt auth login"

    if $dry_run; then
        info "Dry-run mode — no changes will be made"
        echo ""
    fi

    # Phase 2: Feed order
    local -a feed_order=()
    while IFS= read -r f; do
        [[ -n "$f" ]] && feed_order+=("$f")
    done < <(get_feed_order)

    if [[ ${#feed_order[@]} -eq 0 ]]; then
        # No tracked feeds — might still have prplos-only changes
        local diff_count
        diff_count=$(git -C "$PROJECT_ROOT" rev-list --count "origin/$MAIN_BRANCH..HEAD" 2>/dev/null || echo "0")
        if [[ "$diff_count" -eq 0 ]]; then
            info "Nothing to do — no tracked feeds and no prplos changes"
            return 0
        fi
        # Process prplos directly
        process_project "$branch" "$dry_run" || true
        update_jira_ticket "$branch" "$dry_run"
        print_summary "$branch"
        return 0
    fi

    # Phase 3+4: Process feeds (modules first, then feed itself)
    local all_feeds_pinned=true
    for feed_name in "${feed_order[@]}"; do
        local feed_dir="$AEI_DIR/$feed_name"
        [[ -d "$feed_dir/.git" ]] || continue

        header "Processing $feed_name"

        # Phase 3: Process modules for this feed
        local all_modules_pinned=true
        while IFS= read -r module_dir; do
            [[ -n "$module_dir" ]] || continue
            local mname; mname=$(basename "$module_dir")

            # Check if this module belongs to this feed
            local mfeed; mfeed=$(find_module_feed "$mname") || continue
            [[ "$(basename "$mfeed")" == "$feed_name" ]] || continue

            process_module "$module_dir" "$feed_dir" "$branch" "$dry_run" || all_modules_pinned=false
        done < <(discover_source_repos)

        # Phase 4: Process feed if all modules pinned
        if $all_modules_pinned; then
            process_feed "$feed_dir" "$branch" "$dry_run" || all_feeds_pinned=false
        else
            summary_add "$feed_name" "feed" "blocked" "module PR(s) pending"
            all_feeds_pinned=false
        fi
    done

    # Phase 5: Process prplos if all feeds pinned
    if $all_feeds_pinned; then
        header "Processing prplos"
        process_project "$branch" "$dry_run" || true
    else
        summary_add "prplos" "project" "blocked" "feed PR(s) pending"
    fi

    # Phase 6: Jira update
    update_jira_ticket "$branch" "$dry_run"

    # Phase 7: Summary
    print_summary "$branch"
}
