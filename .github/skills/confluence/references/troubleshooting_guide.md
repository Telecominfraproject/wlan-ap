# Confluence Upload Troubleshooting Guide

Solutions to common errors and issues when uploading to Confluence.

## Table of Contents

- [Critical Issues](#critical-issues)
- [Markdown Conversion Issues](#markdown-conversion-issues)
- [API and Authentication Issues](#api-and-authentication-issues)
- [File and Path Issues](#file-and-path-issues)
- [Version and Update Issues](#version-and-update-issues)
- [Performance Issues](#performance-issues)
- [Debugging Techniques](#debugging-techniques)
- [Prevention Checklist](#prevention-checklist)

## Critical Issues

### 🚨 ERROR: MCP Size Limit Exceeded

**Symptom:**
```
Error: Request payload too large
Error: MCP request failed: content exceeds maximum size
```

**Cause:** MCP tools have size limits and cannot handle large documents (typically > 10-20KB).

**Solution:**
**DO NOT USE MCP FOR CONFLUENCE PAGE UPLOADS!**

Use REST API instead:
```bash
# ❌ Don't use MCP
# mcp__atlassian-evinova__confluence_update_page

# ✅ Use REST API via upload script
python3 ~/.claude/skills/confluence/scripts/upload_confluence_v2.py \
    document.md --id 780369923
```

**Prevention:**
- Add to project memory: "DO NOT USE MCP FOR CONFLUENCE UPLOADS"
- Always use `upload_confluence_v2.py` script for page uploads
- MCP is fine for reading pages, but not for uploading content

---

### 🚨 ERROR: Images Appear as Literal Text

**Symptom:**
Images show as escaped HTML on the Confluence page:
```
&lt;ac:image&gt;&lt;ri:attachment ri:filename="diagram.png"/&gt;&lt;/ac:image&gt;
```

**Cause:** Raw Confluence XML was placed in markdown, and md2cf HTML-escaped it.

**Root Cause Explanation:**
1. You put: `<ac:image><ri:attachment ri:filename="diagram.png"/></ac:image>` in markdown
2. md2cf treats this as plain text (not markdown)
3. md2cf HTML-escapes special characters: `<` → `&lt;`, `>` → `&gt;`
4. Confluence receives escaped text and displays it literally

**Solution:**
Use markdown image syntax instead:

```markdown
# ❌ Wrong - Don't put raw XML in markdown
<ac:image><ri:attachment ri:filename="diagram.png"/></ac:image>

# ✅ Correct - Use markdown syntax
![Diagram Description](./path/to/diagram.png)
```

**How md2cf Handles This:**
```markdown
![Diagram](./diagram.png)
↓ md2cf converts to ↓
<ac:image ac:alt="Diagram">
  <ri:attachment ri:filename="diagram.png"/>
</ac:image>
```

**Prevention:**
- Never put raw Confluence XML in markdown files
- Always use standard markdown image syntax: `![alt](path)`
- Let md2cf handle conversion to storage format

---

### 🚨 ERROR: "string indices must be integers, not 'str'"

**Symptom:**
Upload script crashes with Python error:
```python
TypeError: string indices must be integers, not 'str'
  File "upload_confluence.py", line 176, in upload_to_confluence
    'version': result['version']['number']
```

**Cause:**
1. Old `upload_confluence.py` script has incorrect error handling
2. API response structure is different than expected
3. Often happens when `MermaidConfluenceRenderer` is used incorrectly

**Solution:**
Use the improved `upload_confluence_v2.py` script:

```bash
# ✅ Use v2 script with better error handling
python3 ~/.claude/skills/confluence/scripts/upload_confluence_v2.py \
    document.md --id 780369923
```

**What v2 Fixes:**
- Uses base `ConfluenceRenderer` (not `MermaidConfluenceRenderer`)
- Better error handling for API responses
- Proper handling of version numbers
- Clearer error messages

---

### 🚨 ERROR: Images Show as Broken Icons

**Symptom:**
- Page uploads successfully
- Content appears correctly
- Images show as gray placeholder icons or "missing attachment" icons

**Cause:** Image attachments were not uploaded, only page content was updated.

**Solution:**

**Check 1: Verify attachments were uploaded**
```python
# Make sure upload script includes attachment upload
for image_path in attachments:
    confluence.attach_file(
        filename=image_path,
        page_id=page_id,
        ...
    )
```

**Check 2: Verify image files exist**
```bash
# List all images referenced in markdown
grep -o '!\[.*\](.*\.png)' document.md

# Verify each file exists
ls -lh ./path/to/images/
```

**Check 3: Verify image paths are correct**
```python
# In your markdown, paths must be relative or absolute
![Diagram](./diagrams/diagram.png)  # ✅ Relative path
![Diagram](/tmp/diagrams/diagram.png)  # ✅ Absolute path
![Diagram](diagram.png)  # ⚠️  Must be in same directory
```

**Prevention:**
- Always use `upload_confluence_v2.py` which handles attachments correctly
- Verify all image files exist before uploading
- Check script output for "Uploading N attachments..." message

---

## Markdown Conversion Issues

### Images Not Detected by md2cf

**Symptom:**
```python
attachments = []  # Empty list, but markdown has images
```

**Causes:**
1. **Incorrect Image Syntax:**
   ```markdown
   # ❌ Wrong syntax
   <img src="diagram.png">  # HTML, not markdown
   [Diagram](diagram.png)   # Link, not image (missing !)

   # ✅ Correct syntax
   ![Diagram](diagram.png)
   ```

2. **Using MermaidConfluenceRenderer:**
   ```python
   # ❌ Breaks regular images
   renderer = MermaidConfluenceRenderer()

   # ✅ Use base renderer
   renderer = ConfluenceRenderer()
   ```

**Solution:**
1. Check markdown image syntax: `![alt](path)`
2. Use base `ConfluenceRenderer`
3. Verify paths are correct

---

### Mermaid Diagrams Not Rendering

**Symptom:**
- Mermaid code blocks appear as regular code, not diagrams

**Cause:**
You're using Mermaid code blocks in markdown, but Confluence doesn't support Mermaid natively.

**Solution:**
Convert Mermaid to PNG/SVG FIRST, then reference as regular image:

```bash
# Option 1: Use design-doc-mermaid skill
Skill: "design-doc-mermaid"
# Follow prompts to convert diagrams

# Option 2: Manual conversion
mmdc -i architecture.mmd -o architecture.png -b transparent

# Then in markdown:
![Architecture](./diagrams/architecture.png)
```

**Why Not Use MermaidConfluenceRenderer?**
- It overwrites parent's `attachments` attribute
- Breaks regular markdown image handling
- Creates incompatible attachment list format

**Better Approach:**
1. Convert diagrams to images separately
2. Use base `ConfluenceRenderer`
3. Reference images with markdown syntax

---

### PlantUML Diagrams Not Rendering

**Symptom:**
- PlantUML code appears as text, not diagrams

**Solution:**
Same as Mermaid - convert to images first:

```bash
# Use plantuml skill
Skill: "plantuml"
# Follow prompts

# Or manually:
plantuml diagram.puml -tpng

# Reference in markdown:
![Component Diagram](./diagrams/component.png)
```

---

## API and Authentication Issues

### ERROR: "Unauthorized" or "Authentication Failed"

**Symptom:**
```
ERROR: 401 Unauthorized
ERROR: Authentication failed
```

**Causes:**
1. **Missing or Invalid Credentials:**
   - `.env` file not found
   - API token expired
   - Wrong username/password

2. **Wrong .env File Path:**
   ```bash
   # ❌ Default .env not found
   python3 upload_confluence_v2.py document.md --id 123

   # ✅ Specify correct path
   python3 upload_confluence_v2.py document.md --id 123 \
       --env-file /Users/you/clients/evinova/.env.jira
   ```

**Solution:**

**Check 1: Verify credentials file exists**
```bash
ls -la /Users/you/clients/evinova/.env.jira
```

**Check 2: Verify credentials format**
```bash
# .env.jira should contain:
CONFLUENCE_USERNAME=your.email@company.com
CONFLUENCE_API_TOKEN=your_api_token_here
CONFLUENCE_BASE_URL=https://yourcompany.atlassian.net/wiki
```

**Check 3: Test credentials**
```bash
# Use confluence_auth.py to test
python3 -c "
from confluence_auth import get_confluence_client
client = get_confluence_client(env_file='/path/to/.env.jira')
print('✅ Authentication successful!')
"
```

**Generate New API Token:**
1. Go to: https://id.atlassian.com/manage-profile/security/api-tokens
2. Click "Create API token"
3. Copy token and update `.env.jira` file

---

### ERROR: "Page not found" or "Invalid page ID"

**Symptom:**
```
ERROR: Failed to fetch current version for page 123456: Page not found
```

**Causes:**
1. Page ID is incorrect
2. Page was deleted
3. No permission to access page

**Solution:**

**Find Correct Page ID:**
```
# From Confluence URL:
https://company.atlassian.net/wiki/spaces/TEAM/pages/780369923/Page+Title
                                                        ^^^^^^^^^ This is the page ID
```

**Verify Page Exists:**
```bash
# Try reading the page first
python3 -c "
from confluence_auth import get_confluence_client
client = get_confluence_client()
page = client.get_page_by_id('780369923')
print(f'Page found: {page[\"title\"]}')
"
```

---

## File and Path Issues

### ERROR: "File not found" for Images

**Symptom:**
```
❌ File not found: ./diagrams/architecture.png
```

**Causes:**
1. Image file doesn't exist at specified path
2. Relative path is incorrect
3. Working directory is wrong

**Solution:**

**Check 1: Verify file exists**
```bash
# From directory where markdown file is located
ls -lh ./diagrams/architecture.png
```

**Check 2: Use absolute paths**
```markdown
# Instead of relative:
![Diagram](./diagrams/architecture.png)

# Use absolute:
![Diagram](/Users/you/project/diagrams/architecture.png)
```

**Check 3: Verify working directory**
```bash
# Run upload script from same directory as markdown file
cd /path/to/markdown/directory
python3 ~/.claude/skills/confluence/scripts/upload_confluence_v2.py document.md --id 123
```

---

## Version and Update Issues

### ERROR: "Version mismatch" or "Concurrent update"

**Symptom:**
```
ERROR: Version conflict - page has been updated since last read
```

**Cause:** Someone else updated the page while you were preparing your update.

**Solution:**

**Option 1: Retry (script will fetch latest version)**
```bash
# Simply run again - script auto-fetches current version
python3 upload_confluence_v2.py document.md --id 780369923
```

**Option 2: Manual version check**
```python
# Get current version before updating
page_info = confluence.get_page_by_id(page_id, expand='version')
current_version = page_info['version']['number']
print(f"Current version: {current_version}")
```

---

## Performance Issues

### Upload Takes Too Long

**Symptom:**
- Upload takes > 30 seconds
- Script appears to hang

**Causes:**
1. **Large images:** PNG files > 1MB each
2. **Many images:** > 20 images to upload
3. **Slow network:** Connection to Confluence is slow

**Solution:**

**Optimize Images:**
```bash
# Compress PNG files
pngquant --quality=70-85 diagram.png -o diagram-optimized.png

# Convert to SVG for diagrams (usually smaller)
mmdc -i diagram.mmd -o diagram.svg -b transparent

# Resize large screenshots
convert screenshot.png -resize 1920x1080 screenshot-resized.png
```

**Skip Re-uploading Existing Attachments:**
```bash
# v2 script skips by default
python3 upload_confluence_v2.py document.md --id 780369923

# Force re-upload if needed
python3 upload_confluence_v2.py document.md --id 780369923 --force-reupload
```

---

## Debugging Techniques

### Enable Dry-Run Mode

Preview what will be uploaded without making changes:

```bash
python3 upload_confluence_v2.py document.md --id 780369923 --dry-run
```

**Output Shows:**
- Mode (CREATE or UPDATE)
- Title
- Page ID
- Attachments list with file existence check
- Content preview (first 500 chars)

### Check Attachments List

```python
from md2cf.confluence_renderer import ConfluenceRenderer
import mistune

with open('document.md') as f:
    markdown = f.read()

renderer = ConfluenceRenderer()
parser = mistune.Markdown(renderer=renderer)
html = parser(markdown)

print(f"Found {len(renderer.attachments)} images:")
for att in renderer.attachments:
    print(f"  - {att}")
```

### Inspect Storage Format Output

```python
# Save storage format to file for inspection
with open('storage_output.html', 'w') as f:
    f.write(storage_html)

print("Storage format saved to: storage_output.html")
```

### Test API Connection

```python
from confluence_auth import get_confluence_client

# Test connection
client = get_confluence_client(env_file='/path/to/.env.jira')
print(f"✅ Connected to: {client.url}")

# Test page access
page = client.get_page_by_id('780369923')
print(f"✅ Can access page: {page['title']}")
print(f"   Current version: {page['version']['number']}")
```

---

## Prevention Checklist

Before uploading to Confluence, verify:

- [ ] Using `upload_confluence_v2.py` (not MCP, not old v1 script)
- [ ] All images converted to PNG/SVG (not Mermaid/PlantUML code blocks)
- [ ] Using markdown image syntax: `![alt](path)`
- [ ] No raw Confluence XML in markdown
- [ ] All image files exist at specified paths
- [ ] Credentials file path is correct
- [ ] Page ID is correct (from Confluence URL)
- [ ] Dry-run mode tested first (`--dry-run`)

---

## Getting Help

**Documentation References:**
- [confluence_storage_format.md](./confluence_storage_format.md)
- [image_handling_best_practices.md](./image_handling_best_practices.md)

**Related Skills:**
- `design-doc-mermaid` - Mermaid diagram conversion
- `plantuml` - PlantUML diagram conversion

**Confluence Resources:**
- [Confluence REST API Docs](https://docs.atlassian.com/atlassian-confluence/REST/latest/)
- [md2cf GitHub](https://github.com/iamjackg/md2cf)
