# Confluence Wiki Markup Reference

Complete reference for Confluence Wiki Markup syntax with examples.

## Table of Contents

- [Basic Text Formatting](#basic-text-formatting)
- [Headings](#headings)
- [Lists](#lists)
- [Links](#links)
- [Tables](#tables)
- [Images](#images)
- [Code Blocks](#code-blocks)
- [Macros](#macros)
- [Panels and Messages](#panels-and-messages)
- [Special Characters](#special-characters)

## Basic Text Formatting

### Bold and Italic

```wiki
*bold text*
_italic text_
*_bold and italic_*
```

### Strikethrough and Other Formatting

```wiki
-strikethrough text-
+underlined text+
^superscript^
~subscript~
{{monospaced text}}
```

### Text Colors

```wiki
{color:red}red text{color}
{color:#0000FF}blue text{color}
{color:green}green text{color}
```

## Headings

```wiki
h1. Biggest heading
h2. Second level heading
h3. Third level heading
h4. Fourth level heading
h5. Fifth level heading
h6. Smallest heading
```

## Lists

### Simple Lists

```wiki
* bullet point
* another bullet point
** sub bullet point
** another sub bullet
*** deep sub bullet

# numbered item
# another numbered item
## sub numbered item
## another sub numbered
### deep sub numbered
```

### Mixed Lists

```wiki
# Main item
#* Sub bullet under numbered
#* Another sub bullet
# Next main item
#- Sub with dash
```

## Links

### Page Links

```wiki
[Page Title]
[Link Text|Page Title]
[Link to Specific Page|space:page title]
```

### External Links

```wiki
[http://www.example.com]
[Example Website|http://www.example.com]
[Example|http://www.example.com|Link Title]
```

### Anchors

```wiki
{anchor:section1}

[Jump to Section 1|#section1]
[Jump to Section in Another Page|Page Title#section1]
```

### Attachment Links

```wiki
[^attachment.pdf]
[View PDF|^attachment.pdf]
```

### Email Links

```wiki
[mailto:user@example.com]
[Email Us|mailto:support@example.com]
```

## Tables

### Basic Table

```wiki
||Heading 1||Heading 2||Heading 3||
|Cell A1|Cell A2|Cell A3|
|Cell B1|Cell B2|Cell B3|
|Cell C1|Cell C2|Cell C3|
```

### Table with Formatting

```wiki
||Name||Age||Role||
|*John Doe*|30|_Developer_|
|*Jane Smith*|28|{{Designer}}|
|*Bob Wilson*|35|[Manager|http://example.com]|
```

## Code

### Inline Code

```wiki
This is {{inline code}} in a sentence.
Use {{git commit}} to save changes.
```

### Code Blocks

```wiki
{code}
def hello():
    print("Hello World")
{code}
```

### Code Blocks with Options

```wiki
{code:title=Example.java|language=java|theme=Emacs|linenumbers=true|collapse=false}
public class Example {
    public static void main(String[] args) {
        System.out.println("Hello, Confluence!");
    }
}
{code}
```

### Supported Languages

- java
- javascript
- python
- bash/shell
- sql
- xml
- html
- css
- json
- yaml
- ruby
- php
- c/cpp
- csharp
- go
- rust
- typescript

## Macros

### Panel Macro

```wiki
{panel:title=Panel Title}
Content inside the panel.
{panel}
```

### Panel with Styling

```wiki
{panel:title=Important Info|borderStyle=solid|borderColor=#ccc|titleBGColor=#F7D6C1|bgColor=#FFFFCE}
This is a styled panel.
{panel}
```

### Info, Tip, Note, Warning Macros

```wiki
{info}
This is informational content.
{info}

{tip}
This is a helpful tip.
{tip}

{note}
This is a note.
{note}

{warning}
This is a warning!
{warning}
```

### Quote Macro

```wiki
{quote}
This is a quote.
It can span multiple lines.
- Author Name
{quote}
```

### Expand Macro

```wiki
{expand:title=Click to see more}
This content is hidden until clicked.
{expand}
```

### NoFormat Macro

```wiki
{noformat}
This text will be displayed exactly as typed.
  Including   spacing and
    line breaks.
{noformat}
```

### Table of Contents

```wiki
{toc}

{toc:maxLevel=3}

{toc:minLevel=2|maxLevel=4}

{toc:printable=true|style=disc|maxLevel=5|indent=10px}
```

### Excerpt Macro

```wiki
{excerpt}
This text can be included in other pages using the excerpt-include macro.
{excerpt}

{excerpt-include:Page Title}
```

### Include Macro

```wiki
{include:Space:Page Title}

{include:Page Title}
```

### Attachments Macro

```wiki
{attachments}

{attachments:old=true|upload=false}
```

### Children Display

```wiki
{children}

{children:all=true|depth=2}

{children:sort=title|reverse=true}
```

### Recently Updated

```wiki
{recently-updated}

{recently-updated:max=10|spaces=DEV,PROJ}
```

### Page Properties

```wiki
{page-properties}
||Property||Value||
|Status|In Progress|
|Owner|John Doe|
|Version|2.1|
{page-properties}
```

### Jira Issues

```wiki
{jira:PROJ-123}

{jira:jqlQuery=project = PROJ AND status = Open|maximumIssues=5}
```

## Images

### Basic Image

```wiki
!image.png!
!http://example.com/image.png!
```

### Image with Attributes

```wiki
!image.png|thumbnail!
!image.png|width=300!
!image.png|height=200!
!image.png|width=300,height=200!
!image.png|align=center!
!image.png|align=left!
!image.png|align=right!
!image.png|border=1!
```

### Image with Link

```wiki
!image.png|thumbnail!
[Click for full size|^image.png]
```

### Image with Alt Text

```wiki
!image.png|alt=Description of image!
```

## Special Formatting

### Horizontal Rule

```wiki
----
```

### Blockquote

```wiki
bq. This is a blockquote.
It only affects one paragraph.
```

### Line Break

```wiki
Line 1\\
Line 2\\
Line 3
```

## Emoticons and Symbols

### Emoticons

```wiki
:)   - smile
:(   - sad
:D   - big grin
;)   - wink
:P   - tongue
(y)  - thumbs up
(n)  - thumbs down
```

### Symbols

```wiki
(/)  - checkmark
(x)  - cross
(!)  - warning/error
(?)  - question
(i)  - info
(+)  - plus
(-)  - minus
(on) - lightbulb on
(off) - lightbulb off
(*)  - star
```

## Advanced Tables

### Merged Cells

```wiki
||Heading 1||Heading 2||
|Cell spans two columns||
|Cell 1|Cell 2|
```

### Nested Tables

```wiki
||Outer Header 1||Outer Header 2||
|Normal cell|{td}||Inner Header 1||Inner Header 2||
|Inner Cell 1|Inner Cell 2|{td}|
```

## Task Lists

```wiki
[] Unchecked task
[x] Checked task
[] Another unchecked task
```

## Advanced Macros

### Status Macro

```wiki
{status:colour=Green|title=Complete}
{status:colour=Yellow|title=In Progress}
{status:colour=Red|title=Blocked}
```

### Metadata List

```wiki
{metadata-list}
||Key||Value||
|Author|John Doe|
|Date|2025-01-21|
|Version|1.0|
{metadata-list}
```

### Div and Span

```wiki
{div:style=border: 1px solid #ccc; padding: 10px;}
Content in a div
{div}

{span:style=color: red; font-weight: bold;}Important text{span}
```

### Column Macro

```wiki
{section}
{column:width=50%}
Left column content
{column}
{column:width=50%}
Right column content
{column}
{section}
```

## Escape Characters

### Escaping Wiki Markup

```wiki
\*not bold\*
\_not italic\_
\[not a link\]
\!not an image\!
\{not a macro\}
```

### Using NoFormat for Complex Escaping

```wiki
{noformat}
*This will not be bold*
[This will not be a link]
{This will not be a macro}
{noformat}
```

## Best Practices

### Structure

1. Use headings hierarchically (h1, then h2, then h3, etc.)
2. Include a table of contents for long pages
3. Use panels to highlight important information
4. Break up long content with horizontal rules

### Formatting

1. Use bold for emphasis, not for headings
2. Use code blocks for all code, not just inline code
3. Always specify language for code blocks
4. Use tables for structured data, not for layout

### Links

1. Use descriptive link text, not "click here"
2. Link to anchors for long pages
3. Use page links instead of URLs when possible
4. Check links after moving or renaming pages

### Images

1. Always specify dimensions for faster loading
2. Use thumbnails for large images
3. Add alt text for accessibility
4. Use alignment to control layout

### Macros

1. Use info/tip/note/warning macros appropriately
2. Keep expand macros for optional content
3. Use excerpts for content reuse
4. Test macros after creating them

## Common Patterns

### Documentation Page Template

```wiki
h1. Page Title

{toc}

h2. Overview

{excerpt}
Brief description of this page's content.
{excerpt}

h2. Prerequisites

* Prerequisite 1
* Prerequisite 2

h2. Main Content

Content here...

h2. Examples

{code:language=java}
// Example code
{code}

h2. See Also

* [Related Page 1]
* [Related Page 2]

h2. References

* [External Link|http://example.com]

----
_Last updated: {date}_
```

### API Documentation Template

```wiki
h1. API Endpoint: {endpoint-name}

{panel:title=Quick Reference}
*Method:* {{GET}}
*URL:* {{/api/v1/resource}}
*Auth:* Required
{panel}

h2. Description

Overview of the endpoint...

h2. Parameters

||Name||Type||Required||Description||
|param1|string|Yes|Description of param1|
|param2|integer|No|Description of param2|

h2. Request Example

{code:language=bash}
curl -X GET "https://api.example.com/v1/resource?param1=value" \
  -H "Authorization: Bearer token"
{code}

h2. Response

{code:language=json}
{
  "status": "success",
  "data": { }
}
{code}

h2. Error Codes

||Code||Description||
|400|Bad Request|
|401|Unauthorized|
|404|Not Found|
```

### Meeting Notes Template

```wiki
h1. Meeting: {meeting-title}

||Date||{date}||
||Time||{time}||
||Location||{location}||
||Attendees||{attendees}||

h2. Agenda

# Topic 1
# Topic 2
# Topic 3

h2. Discussion

h3. Topic 1

Notes on topic 1...

h3. Topic 2

Notes on topic 2...

h2. Action Items

||Task||Owner||Due Date||Status||
|Task 1|John Doe|2025-01-28|[] Not Started|
|Task 2|Jane Smith|2025-01-25|[] Not Started|

h2. Next Meeting

*Date:* {next-date}
*Time:* {next-time}
*Agenda:* {next-agenda}
```

---

**Version**: 1.0.0
**Last Updated**: 2025-01-21
