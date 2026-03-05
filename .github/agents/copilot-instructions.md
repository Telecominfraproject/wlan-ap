# OpenWiFi AP NOS — Development Guidelines

Auto-generated from all feature plans. Last updated: 2026-03-05

## Technologies

- **Bash 5.x**: New skills (`server-build`, `local-build`, `upgrade-dut`, `refs-setup.sh`)
- **Python 3.11**: Inherited Jira/Confluence scripts (run via `uv run`)
- **SSH/sshpass**: Remote build server (192.168.20.30) and DUT (192.168.1.1)
- **pymupdf4llm**: KB document conversion (PDF → Markdown)
- **git**: KB commit/push operations

## Project Structure

```text
.github/skills/          # Agent skills (12 skills)
  kb-add-file/           # Add single doc to KB
  kb-add-batch/          # Batch import docs to KB
  kb-remove/             # Remove doc from KB
  refs-download/         # Clone/update reference repos
  jira-communication/    # Jira CLI scripts (Python)
  jira-syntax/           # Jira formatting helpers
  confluence/            # Confluence upload scripts
  bkt/                   # Bitbucket skill
  detect-wsl/            # WSL environment detection
  server-build/          # Remote build on 192.168.20.30
  local-build/           # Local firmware build
  upgrade-dut/           # DUT sysupgrade over SSH
refs/
  refs.yaml              # Reference repo manifest
scripts/
  refs-setup.sh          # Clone/update reference repos
profiles/                # Build profile YAML files
```

## Agent Skills Quick Reference

| Skill | Command | Description |
|-------|---------|-------------|
| `server-build` | `/server-build <profile>` | Build on remote server (192.168.20.30) |
| `local-build` | `/local-build <profile>` | Build locally in openwrt/ |
| `upgrade-dut` | `/upgrade-dut [<image>]` | Flash firmware to DUT (192.168.1.1) |
| `refs-download` | `/refs-download` | Clone openwifi-reference-docs (KB) |
| `kb-add-file` | `/kb-add-file <kb> /path/doc.pdf` | Add single doc to KB |
| `kb-add-batch` | `/kb-add-batch <kb> /path/dir` | Batch import docs to KB |

## Security

- Passwords are **never** stored in scripts.
- Use SSH key auth (recommended) or `SSH_PASS` env var with `sshpass`.
- Example: `SSH_PASS=openwifi /server-build cig_wf188n`

## Recent Changes

- 001-openwifi-skills: 12 agent skills added to `.github/skills/`; `refs/refs.yaml` manifest; `scripts/refs-setup.sh` for KB management

<!-- MANUAL ADDITIONS START -->
<!-- MANUAL ADDITIONS END -->
