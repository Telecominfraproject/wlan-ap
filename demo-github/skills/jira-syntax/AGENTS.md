<!-- Managed by agent: keep sections & order; edit content, not structure. Last updated: 2025-12-12 -->

# AGENTS.md — jira-syntax

Development guide for maintaining templates, references, and validation scripts.

## Overview

Static content skill: templates, reference docs, and a validation script. No runtime dependencies.

## Setup & environment

No special setup. Files are plain text/markdown/shell.

## Build & tests

```bash
# Test validation script
scripts/validate-jira-syntax.sh templates/bug-report-template.md
```

## Code style & conventions

**Templates (`templates/`):**
- Use actual Jira wiki markup, never Markdown
- Include section comments explaining purpose
- Test in real Jira before committing

**References (`references/`):**
- Keep `jira-syntax-quick-reference.md` as single source of truth
- Examples must be copy-pasteable into Jira

**Validation script:**
- Bash with grep patterns
- Should catch common Markdown mistakes

## Security & safety

- No executable code in templates
- No sensitive data in examples

## PR/commit checklist

- [ ] All examples use valid Jira wiki markup
- [ ] Tested in actual Jira instance
- [ ] Updated quick-reference if adding new syntax
- [ ] Validation script catches the patterns

## Good vs. bad examples

**Template changes:**
```
# ✓ Test paste into Jira before commit
# ✓ Update quick-reference if new syntax

# ✗ Assume Markdown works in Jira
# ✗ Add syntax without testing
```

## When stuck

- Paste content into real Jira to verify rendering
- Check official docs: https://jira.atlassian.com/secure/WikiRendererHelpAction.jspa

## House rules

- SKILL.md has the user-facing syntax reference
- This file is for maintaining the skill itself

---

**Maintaining this file:** See root `AGENTS.md` for convention reference.
