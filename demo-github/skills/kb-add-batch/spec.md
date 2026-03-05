# kb-add-batch Specification

| Field | Value |
|-------|-------|
| Status | Active |
| Version | 1.0.0 |
| Created | 2026-02-20 |
| Depends | pymupdf4llm, git, refs-setup.sh, kb-add-file (shared conventions) |

## Overview

The **kb-add-batch** skill batch-imports a directory of reference files (PDFs) into a knowledge base using parallel workers for conversion and sequential git operations. It is the parallelized counterpart to **kb-add-file**: the coordinator handles all git operations while 3-4 worker agents process PDF conversions concurrently, reducing import time from hours to under an hour for large batches (e.g., 95 files in ~45 minutes).

The skill is prompt-driven (no scripts). Claude Code orchestrates the workflow by spawning a team of subagents, distributing work, applying quality gates, and committing results.

## Architecture

### Coordinator/Worker Model

```
Coordinator (main agent)              Workers (parallel subagents)
-----------------------------         --------------------------------
1. Discover PDFs in directory
2. Filter already-imported files
3. Expand sparse checkout
4. Copy ALL PDFs to origin/
5. Create team + task batches    -->  6. Process assigned batch:
                                         a. pymupdf4llm conversion
                                         b. Header/footer cleanup
                                         c. Image analysis + ASCII art
                                         d. Generate .idx search index
                                         e. Finalize .md with metadata
                                         f. Clean up temp files
                                 <--  7. Report completion + metadata
8. Quality gate + batch commit
9. Restore sparse checkout
10. Shutdown team + summary
```

**Key principle**: Workers NEVER touch git. Only the coordinator stages, commits, and pushes. This eliminates all git conflicts between parallel agents.

### Directory Layout (same as kb-add-file)

```
refs/<kb-name>/
  kb.yaml
  origin/<category>/<path>/        # Original PDFs (committed by coordinator)
  md/<category>/<path>/            # Markdown + .idx (written by workers, committed by coordinator)
    catalog.yaml                   # Updated by coordinator only
```

## Interface

```
/kb-add-batch [-f] <kb-name> /path/to/directory [category/path]
/kb-add-batch [-f] /path/to/directory [category/path]
/kb-add-batch [-f] /path/to/directory
```

| Argument | Required | Description |
|----------|----------|-------------|
| `-f` | No | Force re-import files already in the catalog (overwrite existing .md, .idx, and catalog entries) |
| `kb-name` | No | KB directory name under `refs/`. Auto-detected if exactly one KB exists |
| `/path/to/directory` | Yes | Directory containing PDF files to import |
| `category/path` | No | Target category for all files. If omitted, each file is auto-categorized individually |

## Logic / Workflow

### Coordinator Steps

#### Step 1: Discover PDFs

List all PDF files in the source directory (non-recursive, `maxdepth 1`). Report total count to the user.

#### Step 2: Filter Already-Imported Files

If `-f` flag is set, skip this step (all PDFs proceed to import). Otherwise, read `md/catalog.yaml` and match by filename stem. Skip files whose basename (without `.pdf`) already appears in any catalog `path:` value. Report skip count.

#### Step 3: Expand Sparse Checkout

If `origin/` is not present in the working tree (`SPARSE_MODE=true`), run `git sparse-checkout add "origin/"` to make the entire origin tree writable.

#### Step 4: Copy All PDFs to origin/

Create target directories and copy all PDFs upfront, before spawning workers. This ensures workers only read from the repo and never perform file copying.

#### Step 5: Create Team and Tasks

1. Create a team named `kb-batch-import` via `TeamCreate`
2. Divide the PDF list into batches of ~25 files each
3. Create one `TaskCreate` entry per batch, listing exact filenames and category paths
4. Spawn 3-4 workers as `general-purpose` subagents via the Task tool
5. Include the full **Worker Prompt Template** (from SKILL.md) in each Task tool `prompt` parameter -- workers never see the SKILL.md directly

#### Steps 6-7: Worker Processing

Workers process their batch one file at a time (see Worker Steps below). The coordinator monitors task statuses and collects metadata as workers complete.

#### Step 8: Batch Commit and Push

For each group of 5-10 completed files:

1. **Verify output** -- confirm `.md` and `.idx` files exist for each PDF
2. **Quality gate** -- compute lines-per-page ratio for each `.md`:
   - Ratio = `wc -l <file>.md` / page count from worker metadata
   - If ratio < 15: **REJECT** the file (indicates worker summarized instead of converting). Reconvert directly using pymupdf4llm, bypassing the worker pipeline
   - Typical acceptable range: 30-80 lines per page
3. **Build catalog entries** -- construct YAML entries from worker-reported metadata
4. **Update catalog.yaml** -- read current file, append new entries, write back. For `-f` re-imports, replace (not duplicate) existing entries
5. **Stage specific files** -- `git add` each PDF, .md, .idx, and catalog.yaml individually
6. **Commit** -- use the batch commit message format (see below)
7. **Push** -- `git push` after each batch commit

#### Step 9: Restore Sparse Checkout

If `SPARSE_MODE` was true, run `git sparse-checkout set "md/" "skills/"` to remove `origin/` from the working tree.

#### Step 10: Shutdown Team and Summary

1. Send `shutdown_request` to all workers
2. Delete the team with `TeamDelete`
3. Report: total processed, total skipped, total commits, any failures with reasons
4. **Catalog consistency check**: verify every `.md` file under `md/` has a matching catalog entry and vice versa. Report orphans or missing files.

### Worker Steps (per PDF)

Workers execute these steps sequentially for each file in their batch:

| Step | Action | Tool |
|------|--------|------|
| W1 | Convert PDF to markdown via `pymupdf4llm.to_markdown()` | Bash (Python) |
| W2 | Clean repeated headers/footers (>50% page frequency) | Write |
| W3 | Analyze extracted images (skip <100x100px, classify remainder) | Read (images only) |
| W4 | Convert real diagrams to ASCII art, remove non-diagram images | Write |
| W5 | Generate `.idx` search index with TYPE:TERM:LINE:CONTEXT format | Write |
| W6 | Finalize `.md` with metadata header, remove image refs, clean artifacts | Write |
| W7 | Delete temporary files (`/tmp/kb-batch-images/<filename>/`) | Bash |
| W8 | Report YAML metadata to coordinator via SendMessage | SendMessage |

If conversion fails for a file, the worker reports `status: failed` with reason and continues to the next file.

### Batch Commit Message Format

```
kb-add-batch: Add <N> <category> reference documents

Documents added:
- <title-1> (<doc-id-1>, <pages-1> pages)
- <title-2> (<doc-id-2>, <pages-2> pages)
...

Category: <category>/<path>
Files: <N> PDFs, <N> markdown, <N> search indexes
Total pages: <sum>
catalog.yaml updated with <N> new entries
```

## Safety Rules

- **Workers NEVER touch git** -- no `git add`, `git commit`, `git push`, or any git command from worker agents
- **Never use Read to extract PDF content** -- it returns a model summary, not actual text. Always use `pymupdf4llm` via Bash
- **Never use `git add -A`** -- stage only specific files
- **Quality gate is mandatory** -- every `.md` file must pass the 15 lines-per-page minimum before commit. Rejected files are reconverted by the coordinator
- **Preserve original filenames** -- never rename PDFs
- **Check disk space** -- abort if less than 2GB free before starting
- **Clean up all temporary files** -- no `.png`, `.tmp`, or `images/` directories may remain after processing
- **Atomic catalog updates** -- only the coordinator modifies `catalog.yaml` to prevent concurrent write conflicts

## Error Handling

| Scenario | Action |
|----------|--------|
| Single PDF fails conversion | Worker logs error, skips file, continues batch |
| Worker crashes | Coordinator reassigns remaining files to other workers |
| Push fails (network) | Retry up to 3 times with 10-second delay |
| Push fails (conflict) | Pull with rebase, then retry push |
| catalog.yaml conflict | Re-read file, re-append entries, retry |
| Disk space < 2GB | Abort before starting with error message |

## Dependencies

| Dependency | Type | Purpose |
|------------|------|---------|
| `pymupdf4llm` | Python package | PDF to markdown conversion |
| `git` | CLI tool | Version control, sparse checkout, push |
| `refs-setup.sh` | Script (prerequisite) | Initial KB clone under `refs/` |
| `kb.yaml` | Config file | KB descriptor in KB repo root |
| `TeamCreate` / `Task` | Claude Code tools | Parallel worker orchestration |

## Performance Expectations

| Phase | Time per File | Notes |
|-------|--------------|-------|
| PDF conversion | 30-60 seconds | Bottleneck; parallelized across workers |
| Image analysis | 10-30 seconds | Depends on extracted image count |
| Index generation | 5-10 seconds | Text scanning |
| Git commit + push | 5-10 seconds | Per batch of 5-10 files |
| **Total (3 workers, 95 files)** | **~45 minutes** | vs ~3+ hours sequential |

## Future Work

- Dynamic worker scaling based on file count (fewer workers for small batches)
- Resume interrupted batch imports by reading partial catalog state
- Support mixed file types (DOCX, HTML) alongside PDFs in a single batch
