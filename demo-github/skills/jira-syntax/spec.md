# jira-syntax Specification

| Field | Value |
|-------|-------|
| Status | Active |
| Version | 1.0.0 |
| Created | 2026-02-20 |
| Depends | bash, jira-communication (for submission) |

## Overview

The **jira-syntax** skill provides Jira wiki markup syntax validation, formatting guidance, and issue templates for Claude Code. It ensures that all content destined for Jira (descriptions, comments) uses correct wiki markup instead of Markdown. The skill is used whenever an AI assistant or developer composes Jira text, before submission via the **jira-communication** skill.

## Architecture

```
jira-syntax/
  SKILL.md                                  # User-facing quick reference and usage
  AGENTS.md                                 # Maintainer guide
  .skillfish.json                           # Skill metadata (origin, version)
  scripts/
    validate-jira-syntax.sh                 # Automated syntax checker (bash + grep)
  references/
    jira-syntax-quick-reference.md          # Complete syntax documentation
  templates/
    bug-report-template.md                  # Bug report template with filled example
    feature-request-template.md             # Feature request template with filled example
```

No runtime services or state. All artifacts are static text files and one shell script.

## Interface

### Validation Script

```bash
scripts/validate-jira-syntax.sh <file1> [file2] ...
```

| Option | Description |
|--------|-------------|
| `<file>` | One or more text files containing Jira wiki markup to validate |

**Output**: Color-coded report (errors in red, warnings in yellow, passes in green) with a summary line. Exit code 0 if no errors (warnings allowed), exit code 1 if errors found.

### Templates

| Template | Path | Sections |
|----------|------|----------|
| Bug Report | `templates/bug-report-template.md` | Environment, Steps to Reproduce, Expected/Actual Behavior, Error Messages, Technical Notes |
| Feature Request | `templates/feature-request-template.md` | Overview, User Stories, Acceptance Criteria, Functional/Non-Functional Requirements, Technical Considerations, Success Metrics |

Each template includes a blank skeleton and a fully filled example demonstrating correct syntax.

## Logic / Workflow

### Composing Jira Content

1. Select the appropriate template from `templates/`
2. Fill in each section using Jira wiki markup (refer to the syntax table in SKILL.md or `references/jira-syntax-quick-reference.md`)
3. Save content to a temporary file
4. Run `scripts/validate-jira-syntax.sh` against the file
5. Fix any reported errors; review warnings
6. Submit via jira-communication skill (e.g., `jira-create.py`)

### Validation Checks

The script performs these checks in order:

| # | Check | Severity | Pattern Detected |
|---|-------|----------|-----------------|
| 1 | Markdown headings | Error | `^##+ ` |
| 2 | Markdown bold | Warning | `**text**` |
| 3 | Markdown code blocks | Error | Lines starting with triple backticks |
| 4 | Markdown inline code | Warning | Backtick-wrapped text without `{{}}` present |
| 5 | Markdown links | Error | `[text](url)` |
| 6 | Missing heading space | Error | `h2.Title` (no space after period) |
| 7 | Code blocks without language | Warning | `{code}` without `:language` |
| 8 | Table headers without double pipes | Warning | `|Header|` instead of `||Header||` |
| 9 | Unclosed `{code}` blocks | Error | Mismatched open/close count |
| 10 | Unclosed `{panel}` blocks | Error | Mismatched open/close count |
| 11 | Unclosed `{color}` tags | Warning | Odd number of `{color` occurrences |
| 12 | Markdown bullet lists | Warning | `- item` instead of `* item` |

Positive confirmations are printed for correctly formatted headings, code blocks with language, user mentions, and issue links.

## Safety Rules

- **Never submit Markdown to Jira** -- always convert to wiki markup first
- **Always validate before submission** -- run the validation script on every piece of content
- Templates must contain only valid Jira wiki markup, never Markdown
- No executable code or sensitive data in templates or examples
- The validation script must never modify input files (read-only analysis)

## Dependencies

| Dependency | Type | Purpose |
|------------|------|---------|
| `bash` | Runtime | Validation script execution |
| `grep` | Runtime | Pattern matching in validation script |
| `jira-communication` | Skill (optional) | Submitting validated content to Jira |

No Python packages, no network access, no environment variables required.

## Future Work

- Add `--fix` mode to auto-correct common Markdown-to-Jira mistakes
- Support piped input (`echo "content" | validate-jira-syntax.sh`)
- Add templates for sprint retrospectives and technical design documents
