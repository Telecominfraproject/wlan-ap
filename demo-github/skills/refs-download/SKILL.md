---
name: refs-download
description: Populate reference materials under refs/ from refs.yaml manifest
---

# Download Reference Materials Skill

Clone and populate external reference materials defined in `refs/refs.yaml`.

## Usage

```
/refs-download              # Default: clone all, gw3-reference-docs md/ only
/refs-download --full       # Clone all with full content (including origin/ PDFs)
/refs-download <source>     # Clone/update a single source
/refs-download update       # Pull latest for all sources
/refs-download status       # Show status of all sources
```

## How It Works

This skill runs `scripts/refs-setup.sh` with the appropriate options to
populate the `refs/` directory from the manifest.

### Default Mode (no `--full`)

Most sources are cloned normally. The exception is **gw3-reference-docs**,
which uses sparse checkout to skip the large `origin/` directory (PDFs):

```bash
# gw3-reference-docs: sparse checkout — md/ only (indexes + markdown)
git clone --filter=blob:none --sparse <uri> refs/gw3-reference-docs
cd refs/gw3-reference-docs
git sparse-checkout set md/
```

This gives full AI search capability (catalog.yaml, .idx, .md files) without
downloading hundreds of MB of original PDFs.

### Full Mode (`--full`)

All sources are cloned completely, including `origin/` with full PDF files:

```bash
# gw3-reference-docs: full clone — everything
git clone <uri> refs/gw3-reference-docs
```

Use `--full` when you need to:
- Read original PDF files
- Add new documents via `/kb-add-file`
- Verify markdown conversion quality against originals

### Single Source

Clone or update just one source:
```
/refs-download gw3-reference-docs          # md/ only
/refs-download gw3-reference-docs --full   # full clone
/refs-download optimeventproto             # normal clone
```

### Update

Pull latest changes for already-cloned sources:
```
/refs-download update                      # all sources
/refs-download update gw3-reference-docs   # single source
```

### Status

Show clone status, branches, and disk usage:
```
/refs-download status
```

## Workflow

1. **Read** `refs/refs.yaml` to get the list of sources
2. **For each source**, determine clone strategy:
   - `gw3-reference-docs` (default mode): sparse checkout `md/` only
   - `gw3-reference-docs` (full mode): full clone
   - All other sources: normal clone (respecting existing `sparse:` config in refs.yaml)
3. **Clone or update** using git
4. **Report** status and disk usage

## Implementation

Run `scripts/refs-setup.sh` which handles all git operations. The script
reads `refs/refs.yaml` and supports these commands:

```bash
./scripts/refs-setup.sh                    # Clone all (default sparse for gw3-reference-docs)
./scripts/refs-setup.sh --full             # Clone all with full content
./scripts/refs-setup.sh update [<name>]    # Pull latest
./scripts/refs-setup.sh status             # Show status
./scripts/refs-setup.sh clean              # Remove all cloned content
./scripts/refs-setup.sh <name>             # Single source
./scripts/refs-setup.sh <name> --full      # Single source, full mode
```

## Prerequisites

- SSH access to Bitbucket/GitLab repositories
- `refs/refs.yaml` must exist in the project root

## Disk Usage Estimates

| Source | Default | Full |
|--------|---------|------|
| optimeventproto | ~5 MB | ~5 MB |
| optimprovision | ~120 KB | ~120 KB |
| optim5_doc | ~14 MB | ~14 MB |
| gw3-reference-docs | **~5-20 MB** (md/ only) | **~200+ MB** (with all PDFs) |

The default mode saves significant disk space by skipping original PDFs
while retaining full AI search capability through the markdown and index files.

## Interaction with kb-add-file

The `/kb-add-file` skill works in both sparse and full modes:

- **Full mode**: Copies the PDF directly to `origin/`, generates md + idx, commits, pushes.
- **Sparse mode**: Temporarily expands sparse checkout to include the target `origin/` path,
  copies the PDF, generates md + idx, commits, pushes, then restores sparse checkout
  (removes `origin/` from working tree). The PDF is committed to git and available
  to `--full` users, but doesn't consume local disk for sparse users.
