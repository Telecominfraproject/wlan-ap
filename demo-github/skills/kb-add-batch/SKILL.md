---
name: kb-add-batch
description: Batch-import reference files using parallel workers for conversion and sequential git operations
skill_version: "1.0.0"
source_kb: gw3-reference-docs
---

# Batch Add Reference Files Skill

Batch-import a directory of reference files (PDFs) into a knowledge base
using parallel workers for conversion and sequential git operations.
This is the parallelized version of `kb-add-file`.

## Usage

```
/kb-add-batch [-f] <kb-name> /path/to/directory [category/path]
/kb-add-batch [-f] /path/to/directory [category/path]
```

- **-f** (optional) -- Force re-import of files that are already in the catalog.
  Existing `.md`, `.idx`, and catalog entries are overwritten with fresh conversions.
- **kb-name** (optional) -- Name of the KB under `refs/`. If omitted, auto-selects
  when only one KB exists; prompts the user when multiple KBs are mounted.
- **directory** -- Path containing PDF files to import
- **category/path** (optional) -- Target category for all files (e.g.,
  `vendor-docs/qualcomm/sdk`). If omitted, each file is auto-categorized
  using the same logic as `kb-add-file`.

Examples:
```
/kb-add-batch gw3-reference-docs /tmp/Export vendor-docs/qualcomm/sdk
/kb-add-batch /tmp/Export vendor-docs/qualcomm/sdk
/kb-add-batch /tmp/Export
/kb-add-batch -f /tmp/Export          # force re-import all files
```

## Context Resolution

1. **Determine KB**: From the `<kb-name>` argument or auto-detection:
   - If first argument matches a directory under `refs/` that contains `kb.yaml`, use it as KB name
   - If first argument is a file path (starts with `/` or `./`), scan `refs/*/kb.yaml`:
     - Exactly 1 KB found: auto-select it
     - Multiple KBs found: prompt user to choose
2. **KB root**: `refs/<kb-name>/` relative to host project root
3. **Verify**: Confirm `refs/<kb-name>/kb.yaml` exists
4. **Paths**: All operations use `refs/<kb-name>/md/`, `refs/<kb-name>/origin/`, etc.

## Prerequisites

- The KB must be cloned under `refs/` (run `./scripts/refs-setup.sh`)
- `refs/<kb-name>/kb.yaml` must exist
- `pymupdf4llm` Python package installed (`pip install pymupdf4llm`)
- PDF files accessible on the local filesystem
- Git credentials configured for push to the KB repository

## Architecture

```
Coordinator (main agent)              Workers (parallel, via team)
-----------------------------         --------------------------------
1. Discover PDFs
2. Filter already-imported
3. Expand sparse checkout
4. Copy ALL PDFs to origin/
5. Create team + tasks           -->  6. Process assigned batch:
                                         a. Convert PDF -> markdown
                                         b. Clean headers/footers
                                         c. Analyze images
                                         d. Convert diagrams to ASCII
                                         e. Generate .idx search index
                                         f. Report metadata
                                 <--  7. Report completion + metadata
8. For each batch of completed files:
   a. Build catalog.yaml entries
   b. Stage files
   c. Commit with descriptive message
   d. Push
9. Restore sparse checkout
10. Shutdown team + summary
```

**Key principle**: Workers NEVER touch git. Only the coordinator commits and
pushes. This eliminates all git conflicts.

## Coordinator Steps (Detailed)

### Step 1: Discover PDFs

List all PDF files in the source directory:
```bash
find /path/to/directory -maxdepth 1 -name '*.pdf' -type f | sort
```

Count total files. Report: "Found N PDF files in /path/to/directory".

### Step 2: Filter Already-Imported Files

If the **`-f`** (force) flag is set, skip this step entirely — all PDFs
proceed to import. Existing catalog entries for re-imported files are
**replaced** (not duplicated) in Step 8 when updating `catalog.yaml`.
Existing `.md` and `.idx` files are overwritten by workers.

Otherwise (no `-f`):

Read `refs/<kb-name>/md/catalog.yaml` and extract all `path:` values.
For each PDF, check if a document with a matching filename already exists in
the catalog. Skip those files and report: "Skipping N already-imported files".

The match is by filename stem -- if the catalog has a `path:` ending in the
PDF's basename (without `.pdf`), it's already imported.

### Step 3: Expand Sparse Checkout

```bash
cd refs/<kb-name>
SPARSE_MODE=false
if [[ ! -d origin ]]; then
    SPARSE_MODE=true
    git sparse-checkout add "origin/"
fi
```

This expands the working tree to include `origin/` so PDFs can be committed.

### Step 4: Copy All PDFs to origin/

For each PDF to import, create the target directory and copy:
```bash
mkdir -p "refs/<kb-name>/origin/<category>/<path>/"
cp "/path/to/source.pdf" "refs/<kb-name>/origin/<category>/<path>/"
```

If all files share the same category/path (provided as argument), this is a
single `mkdir -p` + bulk `cp`. Do this upfront so workers only need to read
from the repo -- no file copying during parallel processing.

### Step 5: Create Team and Tasks

Create a team with `TeamCreate`:
```
team_name: "kb-batch-import"
description: "Parallel batch import of reference documents"
```

Divide the PDF list into batches of ~25 files each (for 3-4 workers).
Create one task per worker batch using `TaskCreate`, with the task description
listing:
- The exact PDF filenames in the batch
- The category/path for each file
- The full path to the KB repo (`refs/<kb-name>/`)

Spawn 3-4 workers via the `Task` tool with `team_name: "kb-batch-import"`.
Each worker is a `general-purpose` subagent type running in the background.

#### Worker Prompt Template

Workers spawned via the Task tool are independent subagents that **never see
this SKILL.md**. The coordinator MUST include the following template verbatim
in every Task tool `prompt` parameter, filling in the `{{...}}` placeholders:

````
You are a KB batch-import worker. Process each PDF in your batch by following
these steps IN ORDER. Do NOT skip or shortcut any step.

**CRITICAL**: NEVER use the Read tool to extract PDF text content. The Read
tool returns a model-generated summary, NOT the actual document text. You
MUST run the pymupdf4llm Python script via the Bash tool for all PDF
conversion. The Read tool may ONLY be used for viewing extracted images
(PNG files), never for PDFs.

## Your Batch

{{LIST_OF_PDF_FILENAMES_WITH_CATEGORY_PATH}}

KB repo path: {{KB_REPO_PATH}} (e.g., refs/gw3-reference-docs)

## For Each PDF, Do:

### 1. Convert PDF to Markdown (pymupdf4llm via Bash — MANDATORY)

Run this exact command via the Bash tool:

```bash
python3 -c "
import pymupdf4llm
md = pymupdf4llm.to_markdown(
    '{{KB_REPO_PATH}}/origin/{{CATEGORY}}/{{PATH}}/{{FILENAME}}.pdf',
    write_images=True,
    image_path='/tmp/kb-batch-images/{{FILENAME}}/',
    force_text=False
)
with open('/tmp/kb-batch-images/{{FILENAME}}/raw.md', 'w') as f:
    f.write(md)
print(f'Lines: {len(md.splitlines())}  Chars: {len(md)}')
"
```

Write the COMPLETE raw output to the temp file. Do NOT summarize, truncate,
or restructure the pymupdf4llm output. The raw conversion is the
ground-truth content. A 100-page PDF typically produces 3,000–8,000+ lines.
If your output is under 15 lines per page, something is wrong — re-run the
conversion.

### 2. Clean Headers and Footers

Identify repeated header/footer patterns across pages (doc IDs, page
numbers, confidentiality notices, company names). Remove lines appearing
on >50% of pages.

### 3. Analyze Extracted Images

List PNGs extracted to `/tmp/kb-batch-images/{{FILENAME}}/`. Skip images
smaller than 100x100px. View each candidate with the Read tool (images
only, NOT PDFs). Classify as: real diagram → convert to ASCII art;
table/code/text as image → describe and remove; banner/logo → remove.

### 4. Convert Diagrams to ASCII Art

For real diagrams: create ASCII art preserving labels, flow arrows, and
hierarchy. Wrap in fenced code block with caption. Replace image reference.

### 5. Generate .idx Search Index

Create `{{KB_REPO_PATH}}/md/{{CATEGORY}}/{{PATH}}/{{FILENAME}}.idx` with
header (#DOC, #TITLE, #VENDOR, #CHIPS, #PAGES, #DATE) and entries
(SEC, FEAT, UCI, CMD, WMI, NL, FUNC, PATH, CONF, KEY, TBL).

### 6. Finalize Markdown

Add document header (kb-add-file comment, title, doc-id, vendor, chips).
Remove all remaining image references. Clean conversion artifacts. Write
final `.md` to `{{KB_REPO_PATH}}/md/{{CATEGORY}}/{{PATH}}/`.

### 7. Clean Up Temporary Files

```bash
rm -rf /tmp/kb-batch-images/{{FILENAME}}/
```

### 8. Report Metadata

Send a message to the coordinator with YAML metadata for each file:
file, status, title, doc_id, vendor, chips, features, protocols, pages,
summary, md_lines, idx_entries. If conversion fails, report status: failed
with reason and continue to next file.
````

### Step 6-7: Worker Processing (see Worker Steps below)

Wait for workers to complete. Monitor progress by reading task statuses.
As workers complete tasks, proceed to commit their output.

### Step 8: Batch Commit and Push

After a worker completes a batch (or periodically as files are done), the
coordinator:

1. **Verify output** -- Check that `.md` and `.idx` files exist for each
   completed PDF
2. **Quality gate (lines-per-page check)** -- For each completed `.md` file:
   1. Count lines: `wc -l <file>.md`
   2. Get page count from worker metadata (`pages` field)
   3. Compute ratio = lines / pages
   4. If ratio < 15: **REJECT** — flag the file for reconversion.
      Typical range is 30-80 lines per page. A ratio below 15 indicates
      the worker summarized the content instead of performing full
      pymupdf4llm conversion. Rejected files are reconverted by the
      coordinator directly using the pymupdf4llm Python script (Step W1
      command), bypassing the worker pipeline.
   5. Log accepted/rejected files with their ratios for the final summary
3. **Build catalog entries** -- For each completed PDF, construct a YAML entry
   for `catalog.yaml` using metadata reported by the worker (title, doc_id,
   vendor, chips, features, protocols, pages, summary)
4. **Update catalog.yaml** -- Append new entries to the `documents:` list.
   Read the current file, append entries, write it back.
5. **Stage files** -- Stage specific files only (no `git add -A`):
   ```bash
   cd refs/<kb-name>
   git add "origin/<category>/<path>/<filename>.pdf"
   git add "md/<category>/<path>/<filename>.md"
   git add "md/<category>/<path>/<filename>.idx"
   git add "md/catalog.yaml"
   ```
6. **Commit** -- Use the batch commit format (see below)
7. **Push** -- `git push` after each batch commit

Group files into commits of 5-10 documents each. This balances descriptive
commit messages against push overhead.

### Step 9: Restore Sparse Checkout

After all files are committed and pushed:
```bash
cd refs/<kb-name>
if $SPARSE_MODE; then
    git sparse-checkout set "md/" "skills/"
fi
```

This removes `origin/` from the working tree while keeping everything
committed. Cone mode always includes root-level files automatically.

### Step 10: Shutdown Team and Summary

1. Send shutdown requests to all workers
2. Delete the team with `TeamDelete`
3. Report final summary:
   - Total files processed
   - Total files skipped (already imported)
   - Total commits made
   - Any failures with reasons
4. **Catalog consistency check** -- Verify every `.md` file under `md/` has a
   matching `path:` entry in `catalog.yaml`, and every catalog entry has a
   corresponding `.md` and `.idx` file. Report any orphans or missing files.
   This catches cases where files were committed but catalog entries were
   missed (or vice versa).

## Worker Steps (Detailed)

> **CRITICAL**: Workers MUST execute pymupdf4llm as a Python script via the
> Bash tool. NEVER use the Read tool to extract PDF text — it returns a
> model-generated summary, not the actual content. The Read tool may only be
> used for viewing extracted images (PNG files), not PDFs.

Each worker receives a batch of PDFs and processes them sequentially. Workers
run in parallel with each other but process their own batch one file at a time.

Workers write output files directly into the repo working tree. Workers do NOT
perform any git operations.

### For Each PDF in the Batch:

#### Step W1: Convert PDF to Markdown

Run the following command via the **Bash tool** (never use Read for PDF extraction):

```bash
python3 -c "
import pymupdf4llm
md = pymupdf4llm.to_markdown(
    'refs/<kb-name>/origin/<category>/<path>/<filename>.pdf',
    write_images=True,
    image_path='/tmp/kb-batch-images/<filename>/',
    force_text=False
)
with open('/tmp/kb-batch-images/<filename>/raw.md', 'w') as f:
    f.write(md)
print(f'Lines: {len(md.splitlines())}  Chars: {len(md)}')
"
```

Write the COMPLETE raw output to a temp file. Do NOT summarize, truncate,
or restructure the pymupdf4llm output. The raw conversion is the
ground-truth content. A 100-page PDF typically produces 3,000-8,000+ lines.
If the output is under 15 lines per page, something is wrong — re-run the
conversion.

#### Step W2: Clean Headers and Footers

Identify repeated header/footer patterns across pages. These typically include:
- Document ID and revision strings (e.g., "80-12345-1 Rev. AG")
- Page numbers
- Confidentiality notices
- Company names/logos

Remove these repeated patterns from the markdown. Use frequency analysis:
if a line appears on >50% of pages, it's likely a header/footer.

#### Step W3: Analyze Extracted Images

If `pymupdf4llm` extracted images to the temp directory:

1. **List all extracted PNGs** and their dimensions
2. **Filter by size** -- Images smaller than 100x100 pixels are likely icons or
   artifacts; skip them
3. **View each candidate image** using the Read tool (which can view images)
4. **Classify each image**:
   - **Real diagram** (architecture, flow, block diagram, sequence diagram) ->
     Convert to ASCII art and embed inline in the markdown
   - **Table rendered as image** -> Describe content, remove image reference
   - **Code/text rendered as image** -> Describe content, remove image reference
   - **Banner/logo/decoration** -> Remove image reference entirely
   - **Screenshot** -> Describe briefly, remove image reference

#### Step W4: Convert Diagrams to ASCII Art

For images classified as real diagrams:
1. Study the diagram content carefully
2. Create an ASCII art representation that preserves:
   - Component names and labels
   - Directional flow (arrows: `-->`, `<--`, `<-->`)
   - Hierarchical relationships
   - Key connections and data paths
3. Wrap in a fenced code block with a descriptive caption
4. Replace the image reference in the markdown with the ASCII block

#### Step W5: Generate .idx Search Index

Create `md/<category>/<path>/<filename>.idx` with the format:

```
#DOC:<category>/<path>/<filename>
#TITLE:<document title>
#VENDOR:<vendor name>
#CHIPS:<comma-separated chip list>
#PAGES:<page count>
#DATE:<document date if available>
---
TYPE:TERM:LINE:CONTEXT
```

Extract the following entry types by scanning the markdown:

| Type | Description | Example |
|------|-------------|---------|
| SEC | Section headings | `SEC:WiFi Configuration:42:## WiFi Configuration` |
| FEAT | Feature names/capabilities | `FEAT:MLO:85:Multi-Link Operation support` |
| UCI | UCI config options | `UCI:wifi-iface.encryption:120:option encryption` |
| CMD | CLI commands | `CMD:iwpriv:145:iwpriv ath0 get_mode` |
| WMI | WMI commands | `WMI:WMI_VDEV_PARAM:200:WMI_VDEV_PARAM_BEACON_INTERVAL` |
| NL | NL80211 attributes | `NL:NL80211_ATTR_WIPHY:210:NL80211_ATTR_WIPHY` |
| FUNC | Function names | `FUNC:ath12k_mac_op_start:300:ath12k_mac_op_start()` |
| PATH | File paths | `PATH:/etc/config/wireless:50:/etc/config/wireless` |
| CONF | Config files/sections | `CONF:wireless:52:config wifi-device` |
| KEY | Important keywords | `KEY:puncturing:88:RU puncturing for 320MHz` |
| TBL | Table entries | `TBL:beacon_int:130:Beacon interval in TU` |

#### Step W6: Finalize Markdown

1. Add document header at the top of the `.md` file:
   ```markdown
   <!-- kb-add-file: <filename>.pdf | <pages> pages | <date> -->
   # <Document Title>

   **Document ID**: <doc-id>
   **Vendor**: <vendor>
   **Chips**: <chip-list>
   ```
2. Ensure no image references remain in the markdown (all should be replaced
   with ASCII art or removed)
3. Clean up any conversion artifacts (excessive blank lines, broken tables)
4. Write the final `.md` to `refs/<kb-name>/md/<category>/<path>/`

#### Step W7: Clean Up Temporary Files

```bash
rm -rf /tmp/kb-batch-images/<filename>/
```

Verify no `.png`, `.tmp`, or other temporary files remain in the repo directory.

#### Step W8: Report Metadata

After processing each PDF, the worker updates the task with metadata needed
for the catalog entry. The worker sends a message to the coordinator with:

```yaml
file: <filename>.pdf
status: success  # or "failed" with reason
title: "<Document Title>"
doc_id: "<80-XXXXX-X Rev. XX>"
vendor: "<Vendor Name>"
chips: [chip1, chip2, ...]
features: [feat1, feat2, ...]
protocols: [proto1, proto2, ...]
pages: <N>
summary: "<1-3 sentence summary>"
md_lines: <N>
idx_entries: <N>
```

If conversion fails for a file, report `status: failed` with the error reason
and continue to the next file in the batch.

## Batch Commit Message Format

Each commit covers 5-10 documents:

```
kb-add-batch: Add <N> <category> reference documents

Documents added:
- <title-1> (<doc-id-1>, <pages-1> pages)
- <title-2> (<doc-id-2>, <pages-2> pages)
- <title-3> (<doc-id-3>, <pages-3> pages)
...

Category: <category>/<path>
Files: <N> PDFs, <N> markdown, <N> search indexes
Total pages: <sum>
catalog.yaml updated with <N> new entries
```

## Error Handling

| Scenario | Action |
|----------|--------|
| Single PDF fails conversion | Worker logs error, skips file, continues batch |
| Worker crashes | Coordinator reassigns remaining files to other workers |
| Push fails (network) | Coordinator retries up to 3 times with 10s delay |
| Push fails (conflict) | Coordinator pulls with rebase, then retries push |
| catalog.yaml conflict | Coordinator re-reads file, re-appends entries, retries |
| Disk space low | Coordinator checks `df` before starting; abort if <2GB free |

## File Summary

For a batch of N files, the skill produces:

| Location | Files |
|----------|-------|
| `origin/<category>/<path>/` | N original PDFs |
| `md/<category>/<path>/` | N `.md` files + N `.idx` files |
| `md/catalog.yaml` | N new entries appended |

## Performance Expectations

- **PDF conversion**: ~30-60 seconds per file (the bottleneck)
- **Image analysis**: ~10-30 seconds per file (depends on image count)
- **Index generation**: ~5-10 seconds per file
- **Git commit+push**: ~5-10 seconds per batch
- **3 workers, 95 files**: ~45 minutes total (vs ~3+ hours sequential)
