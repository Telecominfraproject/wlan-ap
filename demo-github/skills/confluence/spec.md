# confluence Specification

| Field | Value |
|-------|-------|
| Status | Active |
| Version | 1.0.0 |
| Created | 2026-02-20 |
| Depends | Atlassian MCP server, Python 3.11+, `atlassian-python-api`, `md2cf`, `mistune`, `markdownify`; optional: `mark` CLI, `mmdc` (mermaid-cli) |

## 1. Overview

The confluence skill provides AI coding assistants and developers with a complete toolkit for managing Confluence documentation: downloading pages to Markdown, uploading large documents with images, converting between Markdown and Wiki Markup, rendering Mermaid diagrams, and synchronizing Git repositories to Confluence.

The skill combines **prompt-based guidance** (format conversion rules, CQL patterns, workflow instructions) with **6 Python utility scripts** for operations that exceed MCP tool capabilities (large uploads, batch downloads, diagram rendering).

**Users**: Developers, technical writers, AI coding assistants.
**Trigger keywords**: "download Confluence pages", "upload to Confluence", "convert Wiki Markup", "sync markdown to Confluence", "create Confluence page", "handle Confluence images".

## 2. Architecture

### Directory Layout

```
.github/skills/confluence/
  SKILL.md                          # Primary AI reference (decision matrix, workflows)
  README.md                         # Installation, multi-instance support, CI/CD
  QUICK_REFERENCE.md                # Cheat sheet for common tasks
  PARENT_RELATIONSHIP_GUIDE.md      # Critical guide for parent handling during migrations
  INSTALLATION.md                   # Post-install verification guide
  scripts/
    confluence_auth.py              # Shared credential discovery (7-level fallback chain)
    upload_confluence.py            # V1 uploader (Mermaid support, --ignore-frontmatter)
    upload_confluence_v2.py         # V2 uploader (fixes image handling, skip existing attachments)
    download_confluence.py          # Page downloader with XHTML-to-Markdown conversion
    convert_markdown_to_wiki.py     # Markdown to Confluence Wiki Markup converter
    render_mermaid.py               # Mermaid diagram renderer (wraps mmdc)
    mermaid_renderer.py             # Mistune renderer extension for Mermaid code blocks
    generate_mark_metadata.py       # Add/update mark CLI metadata headers
    requirements.txt                # Python dependencies
  references/
    wiki_markup_guide.md            # Complete Confluence Wiki Markup syntax
    conversion_guide.md             # Markdown to Wiki Markup conversion rules
    confluence_storage_format.md    # XHTML storage format details
    image_handling_best_practices.md # Image and diagram workflows
    mark_tool_guide.md              # mark CLI Git-to-Confluence sync
    troubleshooting_guide.md        # Common errors and solutions
  examples/
    .env.example                    # Credential file template
    .env.confluence.example         # Confluence-specific credential template
    page_ids.example.txt            # Page ID file format for batch downloads
    sample-confluence-page.md       # Example Markdown with all features
    upload_example.md               # Upload workflow example
```

### Data Flow

```
Markdown File
  |
  +--> [upload_confluence.py / upload_confluence_v2.py]
  |      |
  |      +--> Parse YAML frontmatter (page ID, space, parent, version)
  |      +--> Convert Markdown to Confluence storage format (md2cf + mistune)
  |      +--> Render Mermaid diagrams to SVG (mmdc via mermaid_renderer.py)
  |      +--> Upload page content via REST API (atlassian-python-api)
  |      +--> Upload image attachments
  |
  +--> [convert_markdown_to_wiki.py]  --> Wiki Markup output
  |
  +--> [generate_mark_metadata.py]    --> Markdown with mark headers
         |
         +--> [mark CLI]              --> Confluence page (Git sync)

Confluence Page
  |
  +--> [download_confluence.py]
         |
         +--> Fetch page via REST API (body.storage, ancestors, labels)
         +--> Download all attachments to {PageTitle}_attachments/
         +--> Convert Confluence XHTML macros (code, children, images)
         +--> Convert HTML to Markdown (markdownify)
         +--> Post-process code language tags
         +--> Write Markdown with YAML frontmatter
         +--> Optionally recurse into child pages
```

## 3. Interface / Subcommands

### Tool Selection Decision Matrix

| Task | Tool | When to Use |
|------|------|-------------|
| Read pages | MCP `confluence_get_page` | Always (no size limits for reading) |
| Search | MCP `confluence_search` | CQL queries of any complexity |
| Small text-only uploads (<10KB) | MCP `confluence_create_page` / `confluence_update_page` | No images, short content |
| Large documents (>10KB) | `upload_confluence_v2.py` | REST API, no size limits |
| Documents with images | `upload_confluence_v2.py` | Handles attachments automatically |
| Git-to-Confluence sync | `mark` CLI | CI/CD pipelines, batch sync |
| Download to Markdown | `download_confluence.py` | Backup, offline editing |
| Format conversion | `convert_markdown_to_wiki.py` | When Wiki Markup output is needed |
| Diagram rendering | `render_mermaid.py` | Standalone Mermaid-to-PNG/SVG |

### upload_confluence_v2.py (Recommended Uploader)

```
upload_confluence_v2.py <file.md> [options]
```

| Option | Purpose |
|--------|---------|
| `--id PAGE_ID` | Page ID for updates |
| `--space SPACE` | Space key for new pages |
| `--title TITLE` | Override title (from frontmatter/H1) |
| `--parent-id ID` | Parent page ID |
| `--dry-run` | Preview without uploading |
| `--env-file PATH` | Custom credential file |
| `--force-reupload` | Re-upload existing attachments |

### upload_confluence.py (V1 Uploader with Parent Control)

Adds critical parent relationship handling options:

| Option | Purpose |
|--------|---------|
| `--ignore-frontmatter` | Ignore `parent.id` from YAML frontmatter (content-only update) |
| `--parent-id ID` | Explicit parent (overrides frontmatter) |
| `--update-frontmatter` | Update frontmatter after upload (planned) |
| `--output-dir DIR` | Directory for generated Mermaid diagrams |

**Parent handling behavior matrix**:

| `--ignore-frontmatter` | `--parent-id` | Frontmatter `parent.id` | Result |
|------------------------|---------------|------------------------|--------|
| No | None | None | No parent set |
| No | None | 123 | Parent = 123 (frontmatter) |
| No | 456 | 123 | Parent = 456 (explicit wins) |
| Yes | None | 123 | No parent change (ignored) |
| Yes | 456 | 123 | Parent = 456 (explicit only) |

### download_confluence.py

```
download_confluence.py [options] <page_ids...>
```

| Option | Purpose |
|--------|---------|
| `--page-ids-file FILE` | Read page IDs from file (one per line) |
| `--output-dir DIR` | Output directory |
| `--download-children` | Recursively download child pages to subdirectories |
| `--save-html` | Save intermediate HTML for debugging |
| `--env-file PATH` | Custom credential file |

### convert_markdown_to_wiki.py

```
convert_markdown_to_wiki.py <input.md> [output.wiki]
```

Converts headings, lists, code blocks, tables, links, images, bold, italic, strikethrough, blockquotes, task lists. Outputs to stdout if no output file specified.

### render_mermaid.py

```
render_mermaid.py <input.mmd> <output.png>
render_mermaid.py -c "graph TD; A-->B" output.png
render_mermaid.py --extract-from-markdown input.md --output-dir diagrams/
```

| Option | Purpose |
|--------|---------|
| `-c, --code` | Inline Mermaid code |
| `-f, --format` | Output format: png, svg, pdf |
| `-t, --theme` | Mermaid theme: default, forest, dark, neutral |
| `-b, --background` | Background color |
| `-w, --width` / `-H, --height` | Output dimensions |
| `--extract-from-markdown` | Extract all diagrams from a Markdown file |
| `--output-dir` | Directory for extracted diagrams |

### generate_mark_metadata.py

```
generate_mark_metadata.py <file.md> --space DEV [options]
```

| Option | Purpose |
|--------|---------|
| `--space, -s` | Confluence space key (required) |
| `--title, -t` | Page title (inferred from H1 if omitted) |
| `--parent, -p` | Parent page title |
| `--parents` | Multiple parents (comma-separated) |
| `--labels, -l` | Page labels (comma-separated) |
| `--attachments, -a` | Attachment paths (comma-separated) |
| `--no-infer-title` | Do not infer title from H1 |
| `--preserve-existing` | Merge with existing metadata |

## 4. Logic / Workflow

### Credential Discovery (confluence_auth.py)

The shared auth module searches for credentials in this priority order:

1. Explicit `--env-file` parameter
2. Environment variables (`CONFLUENCE_URL`, `CONFLUENCE_USERNAME`, `CONFLUENCE_API_TOKEN`)
3. `.env` in current directory
4. `.env.confluence` in current directory
5. `.env.jira` in current directory
6. `.env.atlassian` in current directory
7. Walk up parent directories for any of the above files
8. Home directory `.env` variants
9. MCP configuration (`~/.config/mcp/.mcp.json` or `~/.mcp.json`)

Auto-detects Cloud vs Server/DC based on `.atlassian.net` in URL.

### Upload Workflow (Numbered Steps)

1. Parse Markdown file: extract YAML frontmatter, content, and title
2. Determine parameters: CLI flags override frontmatter values
3. Convert Markdown to Confluence storage format using `md2cf` + `mistune`
4. Detect image references in Markdown (local file paths become attachments)
5. If `--dry-run`: print preview and exit
6. Authenticate via credential discovery chain
7. If page ID exists: fetch current version, increment, call `update_page()`
8. If no page ID: call `create_page()` with space key
9. Upload image attachments (skip existing unless `--force-reupload`)
10. Print result: page ID, version, URL

### Download Workflow (Numbered Steps)

1. Load credentials via discovery chain
2. For each page ID (with retry and exponential backoff):
   a. Fetch page metadata via REST API (body.storage, ancestors, labels, children)
   b. Download all attachments to `{PageTitle}_attachments/` directory
   c. Convert Confluence `<ac:structured-macro>` code blocks to HTML with language markers
   d. Convert `<ac:image>` tags to standard `<img>` with local paths
   e. Replace children macros with actual child page lists
   f. Convert HTML to Markdown using `markdownify`
   g. Post-process: add language tags to code fences, clean up whitespace
   h. Generate YAML frontmatter (page ID, space, version, parent, children, labels, breadcrumb)
   i. Write `{PageTitle}.md` file
   j. If `--download-children`: recurse into child pages, creating subdirectories
3. Write `download_results.json` summary

### GW3 Confluence Integration

For the GW3 project, credentials are read from the Atlassian MCP server config at `~/.claude/.mcp.json`:
- Extract `CONFLUENCE_URL`, `CONFLUENCE_EMAIL` (mapped to `CONFLUENCE_USERNAME`), `CONFLUENCE_API_TOKEN`
- Parent page for specifications: R3 Gateway Specifications (space R3G)
- Page mapping file: `confluence-pages.yaml` tracks local files to Confluence page IDs
- TOC: Use `<!-- confluence-toc -->` marker (auto-replaced with `{toc}` macro)
- Diagrams: Render with `mmdc --scale 3`, upload with `--image-scale 3`
- Tables: Ensure blank line before first `|` row for correct parsing

## 5. Safety Rules

- **MCP size limits**: Never use MCP tools for uploads exceeding 10-20KB. Always use `upload_confluence_v2.py` for large documents or pages with images.
- **Parent relationship safety**: When restoring content after page moves, ALWAYS use `--ignore-frontmatter` to prevent inadvertent moves back to original parents. This was discovered during the PDR migration incident where 8 of 10 pages were moved back.
- **Dry-run first**: Always use `--dry-run` before executing uploads during migrations or batch operations.
- **Validate after moves**: After any parent-changing operation, verify the parent-child relationship via MCP or API.
- **Do not modify original Markdown for Confluence**: When converting diagrams for Confluence upload, work on a temporary copy. Never alter the source Markdown file.
- **Never upload credentials**: Credential files (`.env`, `.env.confluence`, `.env.jira`) must never be committed to version control.
- **V1 vs V2 uploader**: Prefer `upload_confluence_v2.py` for new work. It fixes image handling issues in V1. Use V1 (`upload_confluence.py`) only when `--ignore-frontmatter` parent control is needed.

## 6. Dependencies

### Python Packages (requirements.txt)

| Package | Version | Purpose |
|---------|---------|---------|
| `requests` | >=2.31.0 | HTTP client for downloads |
| `python-dotenv` | >=1.0.0 | Credential file parsing |
| `PyYAML` | >=6.0.1 | Frontmatter parsing |
| `atlassian-python-api` | >=3.41.0 | Confluence REST API client |
| `md2cf` | >=1.0.0 | Markdown to storage format |
| `mistune` | ==0.8.4 | Markdown parser (md2cf requires 0.8.x) |
| `markdownify` | >=0.11.6 | HTML to Markdown conversion |
| `beautifulsoup4` | >=4.12.0 | HTML parsing for macro transformation |

### External Tools

| Tool | Required | Purpose |
|------|----------|---------|
| Atlassian MCP server | For MCP operations | Page read/search/create/update via AI |
| `mark` CLI | Optional | Git-to-Confluence sync |
| `mmdc` (mermaid-cli) | Optional | Mermaid diagram rendering |

### Environment Variables

| Variable | Purpose |
|----------|---------|
| `CONFLUENCE_URL` | Confluence instance URL |
| `CONFLUENCE_USERNAME` | Atlassian email address |
| `CONFLUENCE_API_TOKEN` | Atlassian API token |
| `CONFLUENCE_OUTPUT_DIR` | Default output directory for downloads |

### Related Skills

- **jira-communication**: Shares the same Atlassian API token
- **bkt**: Different token (Bitbucket-scoped)

## 7. Future Work

- Implement `--update-frontmatter` in V1 uploader to update frontmatter with current parent after moves
- Add parent validation (verify parent exists before move, warn if frontmatter parent differs from current)
- Move detection: detect when page has been moved since backup, prompt before using frontmatter parent
- Merge V1 and V2 uploader functionality into a single script
- Add batch upload support (process entire directories)
- Support for Confluence Data Center (currently Cloud-focused)
