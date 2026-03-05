# Quickstart: OpenWiFi Agents Skills Suite

**Feature**: 001-openwifi-skills  
**Phase**: 1 — Design  
**Date**: 2026-03-05

This guide describes how to set up and use the OpenWiFi agent skills once they 
are deployed under `.github/skills/`.

---

## Prerequisites

- VS Code with GitHub Copilot (or compatible agent that reads `.github/skills/`)
- Git credentials configured for any KB repositories you wish to use
- For KB skills: `pymupdf4llm` installed (`pip install pymupdf4llm`)
- For Jira/Confluence skills: `uv` installed (`pip install uv` or `brew install uv`)
- SSH access to build server `192.168.20.30` and DUT `192.168.1.1`
- `sshpass` recommended for non-interactive SSH (`apt install sshpass`)

---

## Knowledge Base Skills

### Add a reference document

```
/kb-add-file /path/to/vendor-spec.pdf
```

The skill auto-detects the active KB under `refs/`. The document is converted 
to markdown, indexed, catalogued, and committed to the KB repository.

### Batch import a directory of PDFs

```
/kb-add-batch /path/to/docs/ vendor-docs/qualcomm
```

### Remove a document

```
/kb-remove qualcomm-qsdk-v2.1
```

### Set up reference repositories

First time setup — clones all sources from `refs/refs.yaml`:

```
/refs-download
```

Use `--full` to also download original PDFs (larger download):

```
/refs-download --full
```

---

## Project Collaboration Skills

### Jira — first-time setup

```bash
uv run .github/skills/jira-communication/scripts/core/jira-setup.py
```

After setup, mentioning a Jira issue key (e.g. `OWIFI-123`) in chat 
triggers the jira-communication skill automatically.

### Validate Jira markup before posting

```
/jira-syntax validate path/to/comment.txt
```

### Confluence — upload a page

```bash
python3 .github/skills/confluence/scripts/upload_confluence_v2.py \
    document.md --id <page-id>
```

### Bitbucket — authenticate

```bash
bkt auth login https://your-bitbucket-host --username yourname --token <PAT>
```

---

## Build Skills

### Build firmware on the remote server

```
/server-build cig_wf188n
```

Lists available profiles if no profile specified.

### Build firmware locally

```
/local-build edgecore_eap104
```

Requires `openwrt/` to be initialized first (`./setup.py --setup`).

---

## Firmware Upgrade

### Flash a DUT

```
/upgrade-dut openwrt/bin/targets/ath79/generic/firmware.bin
```

Or with a custom DUT IP:

```
/upgrade-dut openwrt/bin/targets/ath79/generic/firmware.bin 192.168.2.10
```

If no image path is given, the skill auto-discovers images from `openwrt/bin/`.

---

## WSL Detection

Skills that behave differently under WSL can use:

```bash
.github/skills/detect-wsl/detect-wsl.sh
if [ $? -eq 0 ]; then
    echo "Running in WSL"
fi
```

---

## Skill Discovery

All skills are installed at `.github/skills/<name>/SKILL.md`. To see available 
skills, list the directory:

```bash
ls .github/skills/
```
