# kb-remove Specification

| Field | Value |
|-------|-------|
| Status | Active |
| Version | 1.0.0 |
| Created | 2026-02-20 |
| Depends | git, refs/\<kb-name\>/kb.yaml, refs/\<kb-name\>/md/catalog.yaml |

## Overview

The `kb-remove` skill removes one or more documents from a knowledge base (KB)
stored under `refs/`. It accepts targets by catalog ID, filename stem, category
directory, or glob pattern, then deletes the corresponding `.md`, `.idx`, and
origin PDF files, updates `catalog.yaml`, and commits the result. It is a
prompt-only skill -- Claude performs all operations directly using shell commands
and file editing, with no helper scripts.

**Who uses it:** Developers maintaining a KB who need to retire outdated or
incorrect documents.

**When:** When documents are superseded, incorrectly converted, or no longer
relevant to the project.

## Architecture

### Directory Layout (within a KB)

```
refs/<kb-name>/
  kb.yaml                  # KB descriptor (identifies this as a KB)
  md/
    catalog.yaml           # Document registry: id, title, path, pages
    <category>/<path>/
      <stem>.md            # Converted markdown content
      <stem>.idx           # Search index (keywords, summaries)
  origin/
    <category>/<path>/
      <stem>.pdf           # Original source PDF (may be sparse-excluded)
```

### Data Flow

```
User target args
  -> resolve against catalog.yaml
    -> confirmation prompt (dry-run display)
      -> file deletion (.md, .idx, .pdf)
        -> catalog.yaml update
          -> git commit + push
            -> sparse checkout restore (if expanded)
              -> consistency check
```

## Interface / Subcommands

### Invocation

```
/kb-remove <kb-name> <target> [target...]
/kb-remove <target> [target...]
```

### Parameters

| Parameter | Required | Description |
|-----------|----------|-------------|
| `kb-name` | No | KB directory name under `refs/`. Auto-detected when only one KB exists; user prompted when multiple are mounted. |
| `target` | Yes (1+) | One or more removal targets. See Target Resolution below. |

### Target Resolution

Targets are matched against `catalog.yaml` entries in this priority order:

1. **Catalog ID** -- exact match on the `id:` field (e.g., `wfa-wpa3-v3.5`)
2. **Filename stem** -- match on the last path component of the `path:` field, without extension (e.g., `WPA3_Specification_v3.5`)
3. **Category directory** -- match on a `path:` prefix shared by multiple entries; selects all documents under that prefix (e.g., `vendor-docs/qualcomm/sdk`)
4. **Glob pattern** -- `*` or `?` characters trigger glob matching against `path:` fields (e.g., `standards/wifi-alliance/WPA*`)

Targets that match nothing produce a warning and are skipped; processing
continues with remaining targets. All matches are deduplicated.

## Logic / Workflow

### Step 1: Determine KB

1. If the first argument matches a directory under `refs/` containing `kb.yaml`, use it as the KB name and treat remaining arguments as targets.
2. Otherwise, scan `refs/*/kb.yaml`:
   - Exactly one KB: auto-select it; all arguments are targets.
   - Multiple KBs: prompt the user to choose.
3. Verify `refs/<kb-name>/md/catalog.yaml` exists.

### Step 2: Resolve targets to document list

Read `catalog.yaml`. For each target argument, apply the four-priority
resolution described above. Collect all matched entries into a deduplicated
removal list.

### Step 3: Confirm with user (dry-run display)

Display the removal list showing: document title, catalog ID, page count,
and path for each entry, plus aggregate file counts (`.md`, `.idx`, `.pdf`,
catalog entries). For lists exceeding 10 documents, emphasize the count and
request explicit confirmation. Do not modify any files until the user confirms.

### Step 4: Expand sparse checkout (if needed)

If origin PDFs must be deleted and `origin/` is not in the working tree:

```bash
cd refs/<kb-name> && git sparse-checkout add "origin/"
```

Track this expansion for restoration in Step 8.

### Step 5: Delete files

For each document in the removal list:

```bash
rm -f "md/<category>/<path>/<stem>.md"
rm -f "md/<category>/<path>/<stem>.idx"
rm -f "origin/<category>/<path>/<stem>.pdf"
```

Remove empty parent directories afterward:

```bash
find md/<category>/<path>/ -type d -empty -delete 2>/dev/null
find origin/<category>/<path>/ -type d -empty -delete 2>/dev/null
```

### Step 6: Update catalog.yaml

Remove matching entries from `catalog.yaml`. Preserve:

- File header comments (lines starting with `#`)
- Section comment separators (e.g., `# -- Wi-Fi Alliance Standards --`)
- The `documents:` key and YAML structure
- All non-removed entries exactly as they appear

Remove section separators only when ALL documents in that section are removed.

### Step 7: Commit and push

```bash
cd refs/<kb-name>
git add -u "md/" "origin/"
git add "md/catalog.yaml"
git commit -m "kb-remove: Remove <N> documents from <category>

Documents removed:
- <title-1> (<id-1>)
- <title-2> (<id-2>)

Category: <category>/<path>
Files deleted: <N> markdown, <N> indexes, <N> PDFs
catalog.yaml: <N> entries removed"
git push
```

If push fails due to remote conflict, pull with rebase and retry (up to 3
attempts with 10-second delays between retries).

### Step 8: Restore sparse checkout

If sparse checkout was expanded in Step 4:

```bash
cd refs/<kb-name> && git sparse-checkout set "md/" "skills/"
```

### Step 9: Report summary

Output: total documents removed, files deleted by type, catalog entries removed,
any warnings (unmatched targets, missing files), and the commit hash.

Run a **catalog consistency check**: verify every `.md` file under `md/` has a
matching `path:` entry in `catalog.yaml`, and every catalog entry has a
corresponding `.md` and `.idx` file. Report any orphans or missing files.

## Safety Rules

- **Always confirm before deleting.** Never delete files without explicit user approval.
- **Dry-run by default.** Step 3 shows the full removal plan before any file system changes.
- **No permanent data loss.** Deleted files remain in git history; an accidental removal can be reverted with `git revert`.
- **Never modify files outside `refs/<kb-name>/`.** The skill operates exclusively within the target KB directory.
- **Catalog consistency.** After removal, the catalog must remain valid YAML with no dangling references.
- **Abort cleanly on user cancel.** If the user declines confirmation, exit without modifying any files or git state.
- **Sparse checkout must be restored.** If expanded during the operation, always restore it even if an error occurs mid-workflow.

## Dependencies

| Dependency | Type | Purpose |
|------------|------|---------|
| `git` | CLI tool | File staging, commit, push, sparse checkout |
| `refs/<kb-name>/kb.yaml` | File | Identifies a directory as a knowledge base |
| `refs/<kb-name>/md/catalog.yaml` | File | Document registry for target resolution |
| `python3` (optional) | Runtime | YAML parsing if needed for complex catalog operations |

### Environment

- Working directory: project root (`/home/hb/work/prpl/prplos` or equivalent)
- SSH access to KB remote repository for `git push`

## Future Work

- **Undo subcommand** -- `kb-remove --undo <commit-hash>` to revert a removal.
- **Batch removal from file** -- Accept a list of targets from a text file.
- **Cross-KB removal** -- Remove a document from multiple KBs simultaneously.
