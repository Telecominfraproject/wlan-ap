# Confluence Skill Installation Guide

## ✅ Installation Complete!

The Confluence skill has been installed to:
```
~/.claude/skills/confluence/
```

## 📁 What Was Installed

### Main Documentation
- **SKILL.md** - Complete skill documentation with all features and workflows
- **README.md** - Overview and quick start guide
- **QUICK_REFERENCE.md** - Cheat sheet for common tasks
- **INSTALLATION.md** - This file

### Reference Guides (references/)
- **wiki_markup_guide.md** - Complete Confluence Wiki Markup syntax reference
- **conversion_guide.md** - Detailed Markdown ↔ Wiki Markup conversion rules
- **mark_tool_guide.md** - Comprehensive guide to the mark CLI tool

### Utility Scripts (scripts/)
- **convert_markdown_to_wiki.py** - Convert Markdown to Confluence Wiki Markup
- **render_mermaid.py** - Render Mermaid diagrams to PNG/SVG images
- **generate_mark_metadata.py** - Add mark-compatible metadata to Markdown files

### Examples (examples/)
- **sample-confluence-page.md** - Example Markdown file demonstrating all features

## 🚀 Quick Start

### 1. Verify Installation

```bash
ls ~/.claude/skills/confluence/
```

You should see:
```
SKILL.md
README.md
QUICK_REFERENCE.md
INSTALLATION.md
references/
scripts/
examples/
assets/
```

### 2. Test the Skill

Ask Claude Code:
```
"Help me search for Confluence pages in the DEV space"
```

Claude will automatically use the Confluence skill!

### 3. Install Optional Tools

For full functionality, install these optional tools:

#### mark CLI (for Git → Confluence sync)
```bash
brew install kovetskiy/mark/mark
```

#### Mermaid CLI (for diagram rendering)
```bash
npm install -g @mermaid-js/mermaid-cli
```

## 📚 How to Use

### Using with Claude Code

Simply ask Claude Code to help with Confluence tasks:

**Examples:**
```
"Search for API documentation in Confluence"
"Create a Confluence page from this Markdown"
"Convert this Wiki Markup to Markdown"
"Find pages about authentication created this month"
```

Claude will automatically:
1. Detect it's a Confluence task
2. Load the Confluence skill
3. Use the appropriate MCP tools
4. Apply conversion scripts if needed
5. Handle diagram rendering
6. Provide formatted output

### Using Scripts Directly

#### Convert Markdown to Wiki Markup
```bash
python ~/.claude/skills/confluence/scripts/convert_markdown_to_wiki.py input.md output.wiki
```

#### Render Mermaid Diagrams
```bash
python ~/.claude/skills/confluence/scripts/render_mermaid.py diagram.mmd output.png
```

#### Add mark Metadata
```bash
python ~/.claude/skills/confluence/scripts/generate_mark_metadata.py file.md \
  --space DEV \
  --title "Page Title" \
  --labels api,documentation
```

## ⚙️ Configuration

### Atlassian MCP Server

Ensure your Atlassian MCP server is configured with:

1. **Confluence instance URL**
2. **Authentication credentials** (API token)
3. **Appropriate permissions** for the spaces you want to manage

### mark CLI Configuration (Optional)

If using the mark tool, create `~/.config/mark`:

```toml
username = "your-email@example.com"
password = "your-api-token"
base_url = "https://your-instance.atlassian.net/wiki"
space = "DEV"
```

## 🎯 Common Tasks

### Task 1: Create Confluence Page from Markdown

```
You: "Create a Confluence page from this Markdown document in the DEV space"

[Paste your Markdown content]

Claude:
1. Converts Markdown to Wiki Markup
2. Renders any Mermaid diagrams
3. Uploads diagrams as attachments
4. Creates the page via MCP
5. Returns page URL
```

### Task 2: Search Confluence

```
You: "Find all pages about 'authentication' in the DEV space created this year"

Claude:
1. Builds CQL query: 'space = "DEV" AND text ~ "authentication" AND created >= startOfYear()'
2. Executes search via MCP
3. Returns formatted results
```

### Task 3: Convert Formats

```
You: "Convert this Wiki Markup to Markdown"

[Paste Wiki Markup content]

Claude:
1. Analyzes the Wiki Markup
2. Applies conversion rules
3. Returns Markdown format
4. Notes any elements that couldn't be converted
```

### Task 4: Sync Git Repository to Confluence

```
You: "Help me sync this docs/ folder to Confluence using mark"

Claude:
1. Checks for mark installation
2. Adds metadata headers to Markdown files
3. Provides mark command to run
4. Or executes sync directly
```

## 📖 Learning Resources

### Start Here
1. Read **QUICK_REFERENCE.md** for common commands
2. Review **examples/sample-confluence-page.md** for examples
3. Check **SKILL.md** for complete documentation

### Deep Dives
1. **references/wiki_markup_guide.md** - Learn Wiki Markup syntax
2. **references/conversion_guide.md** - Understand conversion rules
3. **references/mark_tool_guide.md** - Master the mark CLI

## 🔧 Troubleshooting

### Skill Not Loading

If Claude doesn't seem to recognize Confluence tasks:

1. Verify skill is in `~/.claude/skills/confluence/`
2. Check that `SKILL.md` exists and is readable
3. Try restarting Claude Code
4. Explicitly mention "using the Confluence skill"

### MCP Tools Not Available

If Confluence MCP tools aren't working:

1. Check Atlassian MCP server is running
2. Verify credentials are configured
3. Test connection manually
4. Review MCP server logs

### Scripts Not Executing

If Python scripts fail:

1. Ensure Python 3 is installed: `python3 --version`
2. Check script permissions: `ls -l ~/.claude/skills/confluence/scripts/`
3. Run directly: `python3 ~/.claude/skills/confluence/scripts/convert_markdown_to_wiki.py`
4. Check error messages for missing dependencies

## 🆘 Getting Help

### Within Claude Code
```
"Help me with the Confluence skill"
"Show me Confluence skill documentation"
"What can the Confluence skill do?"
```

### Documentation Files
- **SKILL.md** - Complete feature documentation
- **QUICK_REFERENCE.md** - Quick command reference
- **references/** - Detailed guides

### External Resources
- Atlassian MCP: Check your MCP server documentation
- mark tool: https://github.com/kovetskiy/mark
- Mermaid: https://mermaid.js.org/

## 🎉 You're Ready!

The Confluence skill is now installed and ready to use. Try it out with a simple task:

```
"Search Confluence for pages about API in the DEV space"
```

Happy documenting! 📝
