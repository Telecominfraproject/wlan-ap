---
name: kb-add-file
description: Add a new reference file to a knowledge base
skill_version: "1.0.0"
source_kb: openwifi-reference-docs
---

# Add Reference File Skill

Add a new reference file (PDF, document, external source) to a knowledge base.

## Usage

```
/kb-add-file <kb-name> /path/to/document.pdf [category/path]
/kb-add-file /path/to/document.pdf [category/path]
```

- **kb-name** (optional) -- Name of the KB under `refs/`. If omitted, auto-selects
  when only one KB exists; prompts the user when multiple KBs are mounted.
- **path** -- Path to the file to add
- **category/path** (optional) -- Target category directory

Examples:
```
/kb-add-file openwifi-reference-docs /tmp/spec.pdf vendor-docs/qualcomm/sdk
/kb-add-file /tmp/spec.pdf vendor-docs/qualcomm/sdk
/kb-add-file /tmp/spec.pdf
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

## Directory Structure

Each KB repository has two parallel directory trees:

```
<kb-name>/
+-- origin/<category>/<path>/   # Original files (PDF, etc.)
|   +-- <original-filename>.pdf
+-- md/<category>/<path>/       # Markdown + search indexes
    +-- catalog.yaml            # Master document catalog
    +-- <original-filename>.md  # Full markdown conversion
    +-- <original-filename>.idx # Grep-optimized search index
```

Both `origin/` and `md/` MUST mirror the same directory hierarchy at all times.

## Workflow

1. **Detect checkout mode** -- check if `origin/` exists in working tree:
   ```bash
   cd refs/<kb-name>
   SPARSE_MODE=false
   if [[ ! -d origin ]]; then
       SPARSE_MODE=true
   fi
   ```

2. **If sparse mode, temporarily expand sparse checkout** to include the target
   `origin/` path so the PDF can be committed:
   ```bash
   if $SPARSE_MODE; then
       git sparse-checkout add "origin/<category>/<path>"
   fi
   ```

3. **Analyze** the file content (title, type, subject area, vendor, chipset)

4. **Determine** the correct category directory (max 5 tiers deep):
   - `bbf-specs/` -- Broadband Forum specification PDFs
   - `vendor-docs/<vendor>/<subcategory>/` -- Vendor SDK guides, hardware datasheets
   - `standards/` -- IEEE, IETF, and other standards docs
   - `external-tools/` -- Documentation for external tools/libraries

5. **Create directories** if needed -- always create in both `origin/` and `md/`

6. **Copy** the original file to `origin/<category>/<path>/` preserving the original filename
   - If a file with the same name exists, **replace** it (and regenerate md + idx)

7. **Convert to markdown** at `md/<category>/<path>/<original-filename>.md`:
   - Use `pymupdf4llm` for PDF -> markdown conversion
   - Clean up repeated headers/footers
   - Extract diagrams and convert to ASCII art inline
   - Remove all temporary files (images, etc.) -- only the .md file remains
   - Add document header with metadata

8. **Generate search index** at `md/<category>/<path>/<original-filename>.idx`:
   - Plain text, grep-optimized, format: `TYPE:TERM:LINE:CONTEXT`
   - Extract: sections (SEC), features (FEAT), UCI options (UCI), CLI commands (CMD),
     WMI commands (WMI), NL80211 attributes (NL), functions (FUNC), paths (PATH),
     config files (CONF), keywords (KEY), table rows (TBL)
   - Include document metadata header (#DOC, #TITLE, #CHIPS, etc.)

9. **Update catalog** -- add/update the document entry in `md/catalog.yaml`

10. **Verify** no temporary files remain (no images/, no .png, no .tmp)

11. **Commit and push** all changes in `refs/<kb-name>/`:
    - One commit per document covering all files (original, markdown, index, catalog update)
    - Stage only the specific files added/changed -- do not use `git add -A`
    - Commit message must be descriptive. Format:
      ```
      kb-add-file: <document-title> (<doc-id>)

      Add <category>/<path>/<original-filename>:
      - origin: <original-filename> (<pages> pages, <size>)
      - md: markdown conversion with ASCII diagrams (<lines> lines)
      - md: grep-optimized search index (<entries> entries)
      - catalog.yaml updated

      Key topics: <comma-separated list of main features/topics>
      ```
    - Push automatically after successful commit:
      ```bash
      cd refs/<kb-name>
      git add origin/<path>/<filename> md/<path>/<filename>.md md/<path>/<filename>.idx md/catalog.yaml
      git commit -m "<descriptive message>"
      git push
      ```

12. **If was sparse mode, restore sparse checkout** to exclude `origin/` again:
    ```bash
    if $SPARSE_MODE; then
        git sparse-checkout set md/ skills/
    fi
    ```
    This removes the `origin/` directory from the working tree while keeping
    the PDF committed and pushed to the remote. Other team members cloning
    with `--full` will get it; sparse users keep only `md/` and `skills/`.
    Note: cone mode always includes root-level files (README.md, .gitignore,
    kb.yaml) automatically -- only directories need to be specified.

## File Naming

**Original filenames are preserved as-is.** Do not rename the original PDF.
The `.md` and `.idx` files use the same base name with their respective extensions.

Example:
- Original: `80_18950_1_AG_WLAN_Upstream_Driver_for_Access_Poin.pdf`
- Markdown: `80_18950_1_AG_WLAN_Upstream_Driver_for_Access_Poin.md`
- Index: `80_18950_1_AG_WLAN_Upstream_Driver_for_Access_Poin.idx`

## Prerequisites

- The KB must be cloned under `refs/` (run `./scripts/refs-setup.sh` first)
- `refs/<kb-name>/kb.yaml` must exist
- The file to add must be accessible on the local filesystem
- `pymupdf4llm` Python package must be installed

## Category Examples

| Category | Example Path |
|----------|-------------|
| BBF specs | `bbf-specs/tr-181/` |
| Qualcomm SDK | `vendor-docs/qualcomm/sdk/` |
| Qualcomm HW | `vendor-docs/qualcomm/hardware/` |
| MaxLinear | `vendor-docs/maxlinear/` |
| IEEE standards | `standards/ieee/` |
| IETF RFCs | `standards/ietf/` |
| Ambiorix | `external-tools/ambiorix/` |
| LXC/LCM | `external-tools/lxc/` |

If a file doesn't fit existing categories, create a new one automatically.
Always create the same directory under both `origin/` and `md/`.

## Search System

The search system allows Claude to find content across hundreds of documents:

1. **catalog.yaml** -- Master index listing all documents with metadata
2. **\*.idx** -- Per-document grep-optimized indexes
3. **\*.md** -- Full markdown for reading specific sections

See `README.md` in the KB repository root for detailed search instructions.
