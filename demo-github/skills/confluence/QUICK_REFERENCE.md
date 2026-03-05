# Confluence Skill Quick Reference

## Common Tasks

### Search Confluence
```
"Find all pages about authentication in the DEV space"
"Search for API documentation created this month"
```

### Create Page
```
"Create a Confluence page from this Markdown in the DEV space"
"Create a page titled 'API Guide' under 'Documentation' parent"
```

### Update Page
```
"Update the 'Getting Started' page with this content"
"Find and update the authentication guide"
```

### Convert Formats
```
"Convert this Wiki Markup to Markdown"
"Convert this Markdown to Confluence format"
```

### Handle Diagrams
```
"Convert this Markdown with Mermaid diagrams to a Confluence page"
"Render these Mermaid diagrams and upload to Confluence"
```

## Format Conversion Cheat Sheet

| Element | Markdown | Wiki Markup |
|---------|----------|-------------|
| H1 | `# Heading` | `h1. Heading` |
| Bold | `**text**` | `*text*` |
| Italic | `*text*` | `_text_` |
| Code | `` `code` `` | `{{code}}` |
| Link | `[text](url)` | `[text\|url]` |
| Image | `![alt](url)` | `!url!` |
| Bullet | `- item` | `* item` |
| Number | `1. item` | `# item` |

## CQL Search Quick Examples

```cql
# Find in space
space = "DEV"

# Find by title
title ~ "authentication"

# Find by content
text ~ "REST API"

# Created this month
created >= startOfMonth()

# My pages
creator = currentUser()

# With labels
label IN ("api", "docs")

# Complex
space = "DEV" AND
type = page AND
created >= startOfYear() AND
label = "api"
```

## mark CLI Quick Commands

```bash
# Basic sync
mark -f file.md

# With credentials
mark -u email@example.com -p token -f file.md

# Dry run
mark --dry-run -f file.md

# Add metadata first
python scripts/generate_mark_metadata.py file.md \
  --space DEV --title "Page Title" --labels api,docs
```

## Python Scripts

```bash
# Convert Markdown → Wiki
python scripts/convert_markdown_to_wiki.py input.md output.wiki

# Render Mermaid diagram
python scripts/render_mermaid.py diagram.mmd output.png

# Add mark metadata
python scripts/generate_mark_metadata.py file.md \
  --space DEV --title "Title"
```

## Available MCP Tools

- `confluence_search` - Search pages with CQL
- `confluence_get_page` - Get page by ID or title
- `confluence_create_page` - Create new page
- `confluence_update_page` - Update existing page
- `confluence_delete_page` - Delete page
- `confluence_get_page_children` - Get child pages
- `confluence_add_label` - Add label to page
- `confluence_get_labels` - Get page labels
- `confluence_add_comment` - Add comment
- `confluence_get_comments` - Get page comments

## File Locations

- **Main documentation**: `~/.claude/skills/confluence/SKILL.md`
- **Wiki Markup guide**: `~/.claude/skills/confluence/references/wiki_markup_guide.md`
- **Conversion guide**: `~/.claude/skills/confluence/references/conversion_guide.md`
- **mark tool guide**: `~/.claude/skills/confluence/references/mark_tool_guide.md`
- **Scripts**: `~/.claude/skills/confluence/scripts/`
- **Examples**: `~/.claude/skills/confluence/examples/`

## Common Workflows

### 1. Create from Markdown
1. Write Markdown with Mermaid diagrams
2. Claude extracts and renders diagrams
3. Converts Markdown to Wiki Markup
4. Uploads diagrams as attachments
5. Creates Confluence page

### 2. Sync Git → Confluence
1. Add mark metadata to Markdown files
2. Use mark CLI to sync: `mark -f file.md`
3. Attachments uploaded automatically
4. Page hierarchy maintained

### 3. Search and Update
1. Search for page with CQL
2. Get current content
3. Make changes
4. Update page with version comment

## Tips

- Always test with `--dry-run` first
- Use labels consistently for organization
- Keep diagram source files (.mmd) in Git
- Review conversions for edge cases
- Set up CI/CD for automatic syncing
- Use parent pages for proper hierarchy
