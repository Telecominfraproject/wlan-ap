# Jira Wiki Markup Syntax - Quick Reference

Complete reference for Jira's wiki markup syntax to ensure proper formatting in tickets, comments, and descriptions.

## Table of Contents

- [Text Formatting](#text-formatting)
- [Headings](#headings)
- [Lists](#lists)
- [Links](#links)
- [Code Blocks](#code-blocks)
- [Tables](#tables)
- [Panels and Quotes](#panels-and-quotes)
- [Colors](#colors)
- [Special Blocks](#special-blocks)
- [Line Breaks and Horizontal Rules](#line-breaks-and-horizontal-rules)
- [Special Characters](#special-characters)
- [Emoticons](#emoticons)
- [Common Patterns](#common-patterns)
- [Validation Checklist](#validation-checklist)
- [Common Mistakes to Avoid](#common-mistakes-to-avoid)

## Text Formatting

| Syntax | Output | Use Case |
|--------|--------|----------|
| `*text*` | **text** | Bold/strong emphasis |
| `_text_` | *text* | Italic/emphasis |
| `{{text}}` | `text` | Monospace for code/paths |
| `-text-` | ~~text~~ | Strikethrough |
| `+text+` | <u>text</u> | Underline/inserted text |
| `^text^` | text^superscript^ | Superscript |
| `~text~` | text~subscript~ | Subscript |
| `??text??` | text (citation) | Citation format |

## Headings

```
h1. Heading Level 1 (largest)
h2. Heading Level 2
h3. Heading Level 3
h4. Heading Level 4
h5. Heading Level 5
h6. Heading Level 6 (smallest)
```

**Rules:**
- Space required after `h1.` through `h6.`
- One heading per line
- Use h2 for main sections, h3 for subsections

## Lists

### Bulleted Lists
```
* Level 1 item
** Level 2 nested item
*** Level 3 nested item
* Another level 1 item
```

### Numbered Lists
```
# First item
## Nested item
## Another nested item
# Second item
```

### Mixed Lists
```
# Numbered item
#* Nested bullet
#* Another bullet
# Another numbered item
```

**Rules:**
- Space after `*` or `#`
- Nesting uses additional symbols (`**`, `##`)
- Can mix list types with combined syntax (`#*`)

## Links

| Type | Syntax | Example |
|------|--------|---------|
| Issue Link | `[KEY-123]` | `[PROJ-456]` |
| User Mention | `[~username]` | `[~john.doe]` |
| External URL | `[http://url]` | `[http://example.com]` |
| Labeled Link | `[Label\|url]` | `[Google\|http://google.com]` |
| Attachment | `[^filename]` | `[^screenshot.png]` |
| Email | `[mailto:email]` | `[mailto:team@example.com]` |
| Anchor | `{anchor:name}` + `[#name]` | `{anchor:intro}` → `[#intro]` |

## Code Blocks

### Inline Code
```
Use {{code}} for inline monospace text
```

### Code Blocks with Syntax Highlighting
```
{code:java}
public class Example {
    public static void main(String[] args) {
        System.out.println("Hello, World!");
    }
}
{code}
```

**Supported Languages:**
- `java`, `javascript`, `python`, `sql`, `xml`, `json`
- `bash`, `shell`, `php`, `ruby`, `go`, `rust`
- `html`, `css`, `typescript`, `c`, `cpp`, `csharp`
- Many more - check Jira documentation

### Preformatted Text (No Highlighting)
```
{noformat}
Plain text without syntax highlighting
Preserves whitespace and formatting
{noformat}
```

## Tables

### Basic Table
```
||Header 1||Header 2||Header 3||
|Cell A1|Cell A2|Cell A3|
|Cell B1|Cell B2|Cell B3|
```

**Rules:**
- `||` for header cells (double pipe)
- `|` for regular cells (single pipe)
- Rows must have same number of cells
- No trailing pipe at end of row

### Example with Content
```
||Feature||Status||Owner||Priority||
|User Login|{color:green}Complete{color}|[~john.doe]|High|
|Password Reset|{color:yellow}In Progress{color}|[~jane.smith]|Medium|
|2FA|{color:red}Not Started{color}|Unassigned|Low|
```

## Panels and Quotes

### Panel with Title and Background
```
{panel:title=Important Information|bgColor=#FFFFCE}
Content inside the panel
{panel}
```

**Panel Parameters:**
- `title=text` - Panel heading
- `bgColor=#HEXCODE` - Background color
- `borderStyle=solid|dashed` - Border style
- `borderColor=#HEXCODE` - Border color
- `titleBGColor=#HEXCODE` - Title background

### Quote Block
```
{quote}
Multi-line quoted text
Can span multiple paragraphs
{quote}
```

### Single Line Quote
```
bq. This is a block quote on one line
```

## Colors

```
{color:red}Red text{color}
{color:blue}Blue text{color}
{color:green}Green text{color}
{color:#FF0000}Hex color text{color}
```

**Named Colors:**
- `red`, `blue`, `green`, `yellow`, `orange`, `purple`
- `black`, `white`, `gray`, `grey`
- Or use hex codes: `#FF0000`, `#00FF00`, `#0000FF`

## Special Blocks

### Notice/Info Panels
```
{panel:title=⚠️ Warning|bgColor=#FFEBE9|borderColor=#FF0000}
This is a warning message
{panel}

{panel:title=ℹ️ Information|bgColor=#DEEBFF|borderColor=#0052CC}
This is an info message
{panel}

{panel:title=✅ Success|bgColor=#E3FCEF|borderColor=#00875A}
This is a success message
{panel}
```

### Expand/Collapse Section
```
{expand:title=Click to expand}
Hidden content that can be toggled
{expand}
```

## Line Breaks and Horizontal Rules

```
Line 1\\
Line 2 (line break with \\)

First paragraph

Second paragraph (blank line creates new paragraph)

----
Horizontal rule (4 dashes)
```

## Special Characters

```
--- (em-dash: —)
-- (en-dash: –)
\\ (line break)
\{escaped brace\}
```

To escape special characters, use backslash: `\*`, `\{`, `\[`

## Emoticons

| Code | Emoji | Meaning |
|------|-------|---------|
| `:)` | 🙂 | Happy |
| `:(` | 🙁 | Sad |
| `:P` | 😛 | Tongue |
| `:D` | 😀 | Big smile |
| `;)` | 😉 | Wink |
| `(y)` | 👍 | Thumbs up |
| `(n)` | 👎 | Thumbs down |
| `(!)` | ⚠️ | Warning |
| `(?)` | ❓ | Question |
| `(on)` | 💡 | Light bulb on |
| `(off)` | 🔌 | Light bulb off |
| `(*)` | ⭐ | Star |

## Common Patterns

### Status Update Comment
```
h3. Status Update - 2025-11-06

h4. Completed
* Implemented user authentication
* Added unit tests (95% coverage)
* Updated documentation

h4. In Progress
* Integration testing
* Performance optimization

h4. Blocked
* Waiting for API key from [~admin]
* See [PROJ-123] for details

h4. Next Steps
# Deploy to staging environment
# Conduct security review
# Schedule production deployment
```

### Code Review Comment
```
h3. Code Review Findings

h4. ✅ Approved Changes
* Clean separation of concerns
* Comprehensive error handling
* Well-documented functions

h4. 🔧 Suggestions
{code:java}
// Current implementation
public void processData(String input) {
    // Process directly
}

// Suggested improvement
public void processData(String input) {
    validateInput(input);  // Add validation
    // Process after validation
}
{code}

h4. ❌ Issues Found
* Missing null check on line 45
* Potential memory leak in {{DataProcessor}}
* Security vulnerability: [OWASP-A03|https://owasp.org/Top10/A03_2021-Injection/]

[~developer] Please address these before merging.
```

### Meeting Notes
```
h2. Sprint Planning Meeting - 2025-11-06

h3. Attendees
* [~pm] - Product Manager
* [~tech-lead] - Technical Lead
* [~dev1], [~dev2], [~dev3] - Development Team

h3. Agenda
# Review last sprint outcomes
# Plan current sprint scope
# Assign tasks and estimates

h3. Decisions
||Decision||Owner||Action Items||
|Implement caching layer|[~tech-lead]|[PROJ-500] - Research Redis options|
|Upgrade to Node 20|[~dev1]|[PROJ-501] - Test compatibility|
|Refactor authentication|[~dev2]|[PROJ-502] - Design proposal needed|

h3. Action Items
# [~pm] - Update roadmap with Q1 priorities
# [~tech-lead] - Schedule architecture review
# [~dev1] - Provide effort estimates by Friday

h3. Next Meeting
*Date:* 2025-11-13 10:00 AM
*Focus:* Sprint retrospective
```

## Validation Checklist

Before submitting, verify:

### Headings
- [ ] Using `h1.` through `h6.` (not Markdown `#`)
- [ ] Space after period (`h2. Title` not `h2.Title`)
- [ ] One heading per line

### Text Formatting
- [ ] `*bold*` not `**bold**`
- [ ] `_italic_` not `*italic*`
- [ ] `{{code}}` not `` `code` ``

### Lists
- [ ] `*` for bullets, not `-`
- [ ] `#` for numbers
- [ ] Proper nesting (`**`, `##` not spaces/tabs)

### Code
- [ ] `{code:language}` not ``` language ```
- [ ] Proper language identifier
- [ ] Closing `{code}` tag

### Links
- [ ] `[Label|url]` not `[Label](url)`
- [ ] `[PROJ-123]` for issues
- [ ] `[~username]` for mentions

### Tables
- [ ] `||` for headers
- [ ] `|` for cells
- [ ] Consistent column count

### Colors
- [ ] `{color:name}text{color}` format
- [ ] Proper closing `{color}` tag

### Panels
- [ ] Opening `{panel:params}`
- [ ] Closing `{panel}`
- [ ] Valid parameters

## Common Mistakes to Avoid

| ❌ Wrong | ✅ Correct | Note |
|---------|-----------|------|
| `## Heading` | `h2. Heading` | Markdown vs Jira |
| `**bold**` | `*bold*` | Double asterisk is not bold |
| `` `code` `` | `{{code}}` | Markdown backticks don't work |
| `[text](url)` | `[text\|url]` | Markdown link format |
| `- item` | `* item` | Use asterisk for bullets |
| `h2.Title` | `h2. Title` | Missing space after period |
| `{code}` | `{code:java}` | Missing language identifier |
| `|Header|` | `||Header||` | Header needs double pipes |

## Resources

- [Official Jira Wiki Markup](https://jira.atlassian.com/secure/WikiRendererHelpAction.jspa?section=all)
- [Jira Text Formatting](https://support.atlassian.com/jira-cloud-administration/docs/advanced-formatting/)
- [JQL Reference](https://support.atlassian.com/jira-service-management-cloud/docs/use-advanced-search-with-jira-query-language-jql/)
