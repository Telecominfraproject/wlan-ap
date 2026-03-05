---
name: confluence
description: Manage Confluence documentation with downloads, uploads, conversions, and diagrams. Use when asked to "download Confluence pages", "upload to Confluence", "convert Wiki Markup", "sync markdown to Confluence", "create Confluence page", or "handle Confluence images".
---

# Confluence Management Skill

Manage Confluence documentation through Claude Code: download pages to Markdown, upload large documents with images, convert between formats, and integrate Mermaid/PlantUML diagrams.

## Table of Contents

- [Quick Decision Matrix](#quick-decision-matrix)
- [MCP Size Limits](#mcp-size-limits)
- [Prerequisites](#prerequisites)
- [Core Workflows](#core-workflows)
- [Reference Documentation](#reference-documentation)

## Quick Decision Matrix

| Task | Tool | Notes |
|------|------|-------|
| Read pages | MCP tools | `confluence_get_page`, `confluence_search` |
| Small text-only uploads (<10KB) | MCP tools | `confluence_create_page`, `confluence_update_page` |
| Large documents (>10KB) | `upload_confluence_v2.py` | REST API, no size limits |
| Documents with images | `upload_confluence_v2.py` | Handles attachments automatically |
| Git-to-Confluence sync | mark CLI | Best for CI/CD workflows |
| Download pages to Markdown | `download_confluence.py` | Converts macros, downloads attachments |

## MCP Size Limits

MCP tools have size limits (10-20KB) for uploads. For large documents or pages with images, use the REST API via `upload_confluence_v2.py`:

```bash
# Upload large document
python3 ~/.claude/skills/confluence/scripts/upload_confluence_v2.py \
    document.md --id 780369923

# Dry-run preview
python3 ~/.claude/skills/confluence/scripts/upload_confluence_v2.py \
    document.md --id 780369923 --dry-run
```

MCP works for reading pages but not for uploading large content.

## Prerequisites

### Required

- **Atlassian MCP Server** (`mcp__atlassian-evinova`) with Confluence credentials

### Optional

- **mark CLI**: Git-to-Confluence sync (`brew install kovetskiy/mark/mark`)
- **Mermaid CLI**: Diagram rendering (`npm install -g @mermaid-js/mermaid-cli`)

## Core Workflows

### Download Pages to Markdown

```bash
# Single page
python3 ~/.claude/skills/confluence/scripts/download_confluence.py 123456789

# With child pages
python3 ~/.claude/skills/confluence/scripts/download_confluence.py --download-children 123456789

# Custom output directory
python3 ~/.claude/skills/confluence/scripts/download_confluence.py --output-dir ./docs 123456789
```

See [Downloading Guide](references/conversion_guide.md) for details.

### Upload Pages with Images

1. Convert diagrams to images first using `design-doc-mermaid` or `plantuml` skills
2. Reference images with standard markdown: `![Description](./images/diagram.png)`
3. Upload via REST API:

```bash
python3 ~/.claude/skills/confluence/scripts/upload_confluence_v2.py \
    document.md --id PAGE_ID
```

See [Image Handling Best Practices](references/image_handling_best_practices.md) for details.

### Search Confluence

```javascript
mcp__atlassian-evinova__confluence_search({
  query: 'space = "DEV" AND text ~ "API"',
  limit: 10
})
```

### Create/Update Pages (Small Documents)

```javascript
// Create page
mcp__atlassian-evinova__confluence_create_page({
  space_key: "DEV",
  title: "API Documentation",
  content: "h1. Overview\n\nContent here...",
  content_format: "wiki"
})

// Update page
mcp__atlassian-evinova__confluence_update_page({
  page_id: "123456789",
  title: "Updated Title",
  content: "h1. New Content",
  version_comment: "Updated via Claude Code"
})
```

### Sync from Git (mark CLI)

Add metadata to Markdown files:

```markdown
<!-- Space: DEV -->
<!-- Parent: Documentation -->
<!-- Title: API Guide -->

# API Guide
Content...
```

Sync to Confluence:

```bash
mark -f documentation.md
mark --dry-run -f documentation.md  # Preview first
```

See [mark Tool Guide](references/mark_tool_guide.md) for details.

### Convert Between Formats

See [Conversion Guide](references/conversion_guide.md) for the complete conversion matrix.

Quick reference:

| Markdown | Wiki Markup |
|----------|-------------|
| `# Heading` | `h1. Heading` |
| `**bold**` | `*bold*` |
| `*italic*` | `_italic_` |
| `` `code` `` | `{{code}}` |
| `[text](url)` | `[text\|url]` |

## Reference Documentation

Detailed guides in the `references/` directory:

| Guide | Purpose |
|-------|---------|
| [Wiki Markup Reference](references/wiki_markup_guide.md) | Complete syntax for Confluence Wiki Markup |
| [Conversion Guide](references/conversion_guide.md) | Markdown to Wiki Markup conversion rules |
| [Storage Format](references/confluence_storage_format.md) | Confluence XML storage format details |
| [Image Handling](references/image_handling_best_practices.md) | Workflows for images, Mermaid, PlantUML |
| [mark Tool Guide](references/mark_tool_guide.md) | Git-to-Confluence sync with mark CLI |
| [Troubleshooting](references/troubleshooting_guide.md) | Common errors and solutions |

## Available MCP Tools

| Tool | Description |
|------|-------------|
| `confluence_search` | Search using CQL or text |
| `confluence_get_page` | Retrieve page by ID or title |
| `confluence_create_page` | Create new page |
| `confluence_update_page` | Update existing page |
| `confluence_delete_page` | Delete page |
| `confluence_get_page_children` | Get child pages |
| `confluence_add_label` | Add label to page |
| `confluence_get_labels` | Get page labels |
| `confluence_add_comment` | Add comment to page |
| `confluence_get_comments` | Get page comments |

## Utility Scripts

| Script | Purpose |
|--------|---------|
| `scripts/upload_confluence_v2.py` | Upload large documents with images |
| `scripts/download_confluence.py` | Download pages to Markdown |
| `scripts/convert_markdown_to_wiki.py` | Convert Markdown to Wiki Markup |
| `scripts/convert_wiki_to_markdown.py` | Convert Wiki Markup to Markdown |
| `scripts/render_mermaid.py` | Render Mermaid diagrams |

---

**Version**: 2.1.0 | **Last Updated**: 2025-01-21
