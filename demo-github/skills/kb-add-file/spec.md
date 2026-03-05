# kb-add-file Specification

| Field | Value |
|-------|-------|
| Status | Active |
| Version | 1.0.0 |
| Created | 2026-02-20 |
| Depends | pymupdf4llm, git, refs-setup.sh |

## Overview

The **kb-add-file** skill adds a single reference file (PDF, document) to a knowledge base (KB) repository under `refs/`. It handles the full lifecycle: copying the original file, converting to markdown, generating a grep-optimized search index, updating the master catalog, and committing/pushing all changes. The skill is prompt-driven (no scripts) -- Claude Code executes the 12-step workflow directly using Bash, Read, and Write tools.

This is the single-file counterpart to **kb-add-batch**, which uses parallel workers for bulk imports.

## Architecture

### KB Repository Layout

```
refs/<kb-name>/
  kb.yaml                          # KB descriptor (must exist)
  origin/<category>/<path>/        # Original files (PDFs)
    <original-filename>.pdf
  md/                              # Markdown + search artifacts
    catalog.yaml                   # Master document catalog
    <category>/<path>/
      <original-filename>.md       # Full markdown conversion
      <original-filename>.idx      # Grep-optimized search index
```

The `origin/` and `md/` trees mirror the same directory hierarchy. Sparse-checkout configurations may exclude `origin/` from the working tree to save disk space; the skill handles this transparently.

### Data Flow

```
Input PDF --> origin/ (copy) --> pymupdf4llm --> raw markdown
  --> clean headers/footers --> analyze images --> ASCII art diagrams
  --> final .md (md/) --> generate .idx (md/) --> update catalog.yaml
  --> git add + commit + push
```

## Interface

```
/kb-add-file <kb-name> /path/to/document.pdf [category/path]
/kb-add-file /path/to/document.pdf [category/path]
/kb-add-file /path/to/document.pdf
```

| Argument | Required | Description |
|----------|----------|-------------|
| `kb-name` | No | KB directory name under `refs/`. Auto-detected if exactly one KB exists |
| `/path/to/file` | Yes | Absolute or relative path to the source file |
| `category/path` | No | Target category directory (max 5 tiers). Auto-determined from file content if omitted |

## Logic / Workflow

### Step 1: Detect Checkout Mode

Check whether `origin/` exists in the working tree. If absent, set `SPARSE_MODE=true`.

### Step 2: Expand Sparse Checkout (conditional)

If sparse mode, run `git sparse-checkout add "origin/<category>/<path>"` to make the target directory writable.

### Step 3: Analyze File Content

Read the file to determine title, type, subject area, vendor, and chipset. This informs automatic category selection.

### Step 4: Determine Category Directory

Select from the established taxonomy (see Category Taxonomy below) or create a new category. Maximum 5 tiers deep.

### Step 5: Create Directories

Create matching directories under both `origin/` and `md/` if they do not exist.

### Step 6: Copy Original File

Copy to `origin/<category>/<path>/` preserving the original filename. If a file with the same name exists, replace it.

### Step 7: Convert to Markdown

Run `pymupdf4llm.to_markdown()` via the Bash tool (never use Read for PDF extraction). Clean repeated headers/footers, extract diagrams as ASCII art, remove temporary image files. Write to `md/<category>/<path>/<filename>.md` with a metadata header.

### Step 8: Generate Search Index

Create `md/<category>/<path>/<filename>.idx` in the format `TYPE:TERM:LINE:CONTEXT` with a document metadata header. Extract entries for: SEC (sections), FEAT (features), UCI (UCI options), CMD (CLI commands), WMI (WMI commands), NL (NL80211 attributes), FUNC (functions), PATH (file paths), CONF (config files), KEY (keywords), TBL (table rows).

### Step 9: Update Catalog

Add or update the document entry in `md/catalog.yaml` with metadata (title, doc_id, vendor, chips, features, pages, summary, path).

### Step 10: Verify Cleanup

Confirm no temporary files remain (.png, .tmp, images/).

### Step 11: Commit and Push

Stage only the specific files changed (never `git add -A`). Use a descriptive commit message including document title, doc-id, file counts, line counts, index entry counts, and key topics. Push immediately after commit.

### Step 12: Restore Sparse Checkout (conditional)

If sparse mode was active, run `git sparse-checkout set "md/" "skills/"` to remove `origin/` from the working tree while keeping the commit intact. Cone mode includes root-level files automatically.

## Category Taxonomy

| Category | Example Path | Content |
|----------|-------------|---------|
| `bbf-specs/` | `bbf-specs/tr-181/` | Broadband Forum specification PDFs |
| `vendor-docs/<vendor>/sdk/` | `vendor-docs/qualcomm/sdk/` | Vendor SDK guides |
| `vendor-docs/<vendor>/hardware/` | `vendor-docs/qualcomm/hardware/` | Hardware datasheets |
| `standards/ieee/` | `standards/ieee/` | IEEE standards |
| `standards/ietf/` | `standards/ietf/` | IETF RFCs |
| `external-tools/<tool>/` | `external-tools/ambiorix/` | External tool/library docs |

New categories may be created automatically when a file does not fit existing ones. Both `origin/` and `md/` must always have matching directory structures.

## Search Index Format (.idx)

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

The search system operates in three tiers: `catalog.yaml` for document discovery, `.idx` files for term-level search, and `.md` files for reading full sections.

## Safety Rules

- **Never use Read to extract PDF content** -- it returns a model summary, not actual text. Always use `pymupdf4llm` via Bash
- **Never use `git add -A`** -- stage only specific files to avoid committing unrelated changes
- **Preserve original filenames** -- do not rename PDFs; `.md` and `.idx` files inherit the same base name
- **Mirror directory structures** -- `origin/` and `md/` must always have matching hierarchies
- **Clean up all temporary files** -- no `.png`, `.tmp`, or `images/` directories may remain in the repo after the operation
- **One commit per document** -- each add is atomic, covering the original, markdown, index, and catalog update

## Dependencies

| Dependency | Type | Purpose |
|------------|------|---------|
| `pymupdf4llm` | Python package | PDF to markdown conversion |
| `git` | CLI tool | Version control, sparse checkout, push |
| `refs-setup.sh` | Script (prerequisite) | Initial KB clone under `refs/` |
| `kb.yaml` | Config file | KB descriptor that must exist in the KB repo root |

## Future Work

- Support non-PDF formats (DOCX, HTML, EPUB) with appropriate converters
- Deduplication check against existing catalog entries by content hash
- Automatic cross-referencing between related documents in the index
