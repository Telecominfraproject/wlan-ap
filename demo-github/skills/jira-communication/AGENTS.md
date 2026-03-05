<!-- Managed by agent: keep sections & order; edit content, not structure. Last updated: 2025-12-12 -->

# AGENTS.md — jira-communication

Development guide for maintaining and extending the Jira communication scripts.

## Overview

Python CLI scripts using `uv run`. Each script is standalone with PEP 723 inline dependencies.

## Setup & environment

Development requires Python 3.11+ and `uv`. No virtual environment needed - `uv run` handles dependencies.

## Build & tests

```bash
# Test a script works
uv run scripts/core/jira-validate.py --help

# Test against real Jira (need ~/.env.jira configured)
uv run scripts/core/jira-validate.py --verbose
```

## Code style & conventions

**Script structure:**
- Use argparse with subcommands
- Import shared lib: `from lib.client import get_jira_client`
- PEP 723 header for inline dependencies
- PYTHONPATH manipulation at top (copy from existing scripts)

**Output formats:** Every script must support `--json`, `--quiet`, and default table output via `lib/output.py`.

**Write operations:** Must include `--dry-run` flag.

## Security & safety

- Never hardcode credentials
- Use `lib/config.py` for env loading
- Test `--dry-run` before actual writes

## PR/commit checklist

- [ ] Script follows existing structure (copy from similar script)
- [ ] All three output formats work (`--json`, `--quiet`, table)
- [ ] `--dry-run` for any write operation
- [ ] `--help` is descriptive
- [ ] Update SKILL.md with new script docs

## Good vs. bad examples

**Adding a new script:**
```python
# ✓ Copy structure from existing script
# ✓ Use lib/ imports
# ✓ Support all output formats

# ✗ Write from scratch without looking at patterns
# ✗ Hardcode auth or skip --dry-run
```

## When stuck

- Copy structure from `scripts/core/jira-issue.py` (good reference)
- Check `lib/` for shared utilities
- Run `--help` on similar scripts

## House rules

- Don't read SKILL.md for development - it's user docs
- Test against real Jira before PR

---

**Maintaining this file:** See root `AGENTS.md` for convention reference.
