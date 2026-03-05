---
name: kb-remove
description: Remove files or directories from a knowledge base
skill_version: "1.0.0"
source_kb: openwifi-reference-docs
---

# Remove from Knowledge Base Skill

Remove one or more documents from a knowledge base by filename, path stem,
catalog ID, or entire category directory. Cleans up `.md`, `.idx`, origin PDF,
and `catalog.yaml` entries, then commits and pushes.

## Usage

```
/kb-remove <kb-name> <target> [target...]
/kb-remove <target> [target...]
```

- **kb-name** (optional) -- Name of the KB under `refs/`. If omitted, auto-selects
  when only one KB exists; prompts the user when multiple KBs are mounted.
- **target** -- What to remove. Accepts one or more of:
  - **catalog ID** -- e.g., `wfa-wpa3-v3.5` (matches the `id:` field in catalog.yaml)
  - **filename stem** -- e.g., `WPA3_Specification_v3.5` (without `.md`/`.idx`/`.pdf`)
  - **category directory** -- e.g., `vendor-docs/qualcomm/sdk` (removes ALL documents
    in that category)
  - **glob pattern** -- e.g., `standards/wifi-alliance/WPA*` (removes matching documents)

Examples:
```
/kb-remove wfa-wpa3-v3.5                              # by catalog ID
/kb-remove WPA3_Specification_v3.5                     # by filename stem
/kb-remove vendor-docs/qualcomm/sdk                    # entire category
/kb-remove standards/wifi-alliance/WPA*                # glob pattern
/kb-remove wfa-wpa3-v3.5 wfa-wmm-v1.2.0               # multiple targets
```

## Context Resolution

1. **Determine KB**: From the `<kb-name>` argument or auto-detection:
   - If first argument matches a directory under `refs/` that contains `kb.yaml`, use it as KB name
   - If first argument is a file path, glob, or catalog ID, scan `refs/*/kb.yaml`:
     - Exactly 1 KB found: auto-select it
     - Multiple KBs found: prompt user to choose
2. **KB root**: `refs/<kb-name>/` relative to host project root
3. **Verify**: Confirm `refs/<kb-name>/kb.yaml` exists
4. **Paths**: All operations use `refs/<kb-name>/md/`, `refs/<kb-name>/origin/`, etc.

## Steps

### Step 1: Resolve Targets to Document List

Read `refs/<kb-name>/md/catalog.yaml` and build the full document list.

For each target argument, resolve it to matching catalog entries:

1. **Exact catalog ID match** -- If target matches an `id:` field, select that entry
2. **Filename stem match** -- If target matches the last component of a `path:` field,
   select that entry
3. **Category directory match** -- If target matches a `path:` prefix (i.e., multiple
   documents share that path prefix), select all matching entries
4. **Glob match** -- If target contains `*` or `?`, match against `path:` fields
   using glob rules

If a target matches nothing, report a warning: "No match for target: <target>".
Collect all matched entries into a deduplicated removal list.

### Step 2: Confirm with User

Display the removal list:

```
Will remove N documents from KB <kb-name>:

  1. <title> (<id>, <pages> pages)
     path: <category>/<path>/<stem>
  2. <title> (<id>, <pages> pages)
     path: <category>/<path>/<stem>
  ...

Files to delete:
  - N .md files
  - N .idx files
  - N .pdf files (if origin/ is checked out)
  - N catalog.yaml entries
```

Ask the user to confirm before proceeding. If the removal list is large
(>10 documents), emphasize the count and ask for explicit confirmation.

### Step 3: Expand Sparse Checkout (if needed)

If any origin PDFs need to be removed and `origin/` is not checked out:

```bash
cd refs/<kb-name>
git sparse-checkout add "origin/"
```

Track whether sparse checkout was expanded so it can be restored in Step 7.

### Step 4: Delete Files

For each document in the removal list:

```bash
cd refs/<kb-name>
rm -f "md/<category>/<path>/<stem>.md"
rm -f "md/<category>/<path>/<stem>.idx"
rm -f "origin/<category>/<path>/<stem>.pdf"
```

After deleting all files, check if any category directories are now empty
and remove them:

```bash
find md/<category>/<path>/ -type d -empty -delete 2>/dev/null
find origin/<category>/<path>/ -type d -empty -delete 2>/dev/null
```

### Step 5: Update catalog.yaml

Read `refs/<kb-name>/md/catalog.yaml`. Remove all entries whose `id:` matches
the removal list. Write the updated file back.

Preserve:
- The file header comments (lines starting with `#`)
- Section comment separators (e.g., `# ── Wi-Fi Alliance Standards ──`)
- The `documents:` key
- All non-removed entries exactly as they appear

Remove section comment separators only if ALL documents in that section
are removed.

### Step 6: Commit and Push

Stage all deletions and the updated catalog:

```bash
cd refs/<kb-name>
git add -u "md/<category>/<path>/"
git add -u "origin/<category>/<path>/"
git add "md/catalog.yaml"
```

Commit with message format:

```
kb-remove: Remove <N> documents from <category>

Documents removed:
- <title-1> (<id-1>)
- <title-2> (<id-2>)
...

Category: <category>/<path>
Files deleted: <N> markdown, <N> indexes, <N> PDFs
catalog.yaml: <N> entries removed
```

Push: `git push`

If push fails due to conflict, pull with rebase and retry (up to 3 times).

### Step 7: Restore Sparse Checkout

If sparse checkout was expanded in Step 3:

```bash
cd refs/<kb-name>
git sparse-checkout set "md/" "skills/"
```

### Step 8: Summary

Report:
- Total documents removed
- Total files deleted (.md, .idx, .pdf)
- Catalog entries removed
- Any warnings (unmatched targets, missing files)
- Commit hash
- **Catalog consistency check** -- Verify every `.md` file under `md/` has a
  matching `path:` entry in `catalog.yaml`, and every catalog entry has a
  corresponding `.md` and `.idx` file. Report any orphans or missing files.

## Error Handling

| Scenario | Action |
|----------|--------|
| Target matches nothing | Warn and skip, continue with other targets |
| .md exists but .idx missing | Delete .md, warn about missing .idx |
| Origin PDF not found | Skip PDF deletion (may not be checked out), warn |
| catalog.yaml entry missing | Skip catalog removal for that entry, warn |
| Push fails (network) | Retry up to 3 times with 10s delay |
| Push fails (conflict) | Pull with rebase, then retry push |
| User cancels confirmation | Abort without changes |

## Safety

- **Always confirm** before deleting. Never delete without user approval.
- **Dry-run by default** -- Step 2 shows exactly what will be removed before
  any files are touched.
- **No data loss** -- Deleted files remain in git history. If removal was
  accidental, the commit can be reverted.
