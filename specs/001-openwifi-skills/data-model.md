# Data Model: OpenWiFi Agents Skills Suite

**Feature**: 001-openwifi-skills  
**Phase**: 1 — Design  
**Date**: 2026-03-05

---

## Entities

### Skill

The primary deployable unit. A self-contained directory under `.github/skills/`.

| Field | Type | Description |
|-------|------|-------------|
| `name` | string | Unique skill identifier (directory name and YAML `name:` field) |
| `description` | string | One-line description used by agent auto-discovery |
| `version` | semver string | Skill version in YAML frontmatter |
| `allowed-tools` | string (optional) | Restricts which agent tools may be used |
| `SKILL.md` | file | Agent-readable invocation instructions (required) |
| `scripts/` | directory (optional) | Supporting shell or Python scripts |
| `references/` | directory (optional) | Reference documents bundled with skill |
| `templates/` | directory (optional) | Template files used by the skill |

**State transitions**: None — skills are static configuration files.

**Validation rules**:
- `SKILL.md` must exist and contain a YAML front-matter block with at least `name` and `description`
- `name` must match the directory name exactly
- No credentials may appear in any skill file

---

### Knowledge Base (KB)

An external git repository mounted locally under `refs/`.

| Field | Type | Description |
|-------|------|-------------|
| `name` | string | Directory name under `refs/`, e.g. `openwifi-reference-docs` |
| `kb.yaml` | file | KB descriptor (must exist for skill auto-detection) |
| `origin/<category>/<path>/` | directory tree | Original source files (PDFs) |
| `md/<category>/<path>/` | directory tree | Markdown conversions + `.idx` search indexes |
| `md/catalog.yaml` | file | Master document catalog |

**Validation rules**:
- `refs/<kb-name>/kb.yaml` must exist before any kb-* skill can operate
- `origin/` and `md/` directory hierarchies must mirror the same category/path structure

---

### Document (in KB)

A single reference file tracked by the knowledge base.

| Field | Type | Description |
|-------|------|-------------|
| `id` | string | Catalog ID, e.g. `qualcomm-qsdk-v2.1` |
| `path` | string | Relative path within `md/<category>/<path>/` |
| `title` | string | Human-readable document title |
| `vendor` | string (optional) | Hardware vendor name |
| `chipset` | string (optional) | Chipset family |
| `category` | string | Category directory path (max 5 tiers) |
| `.md file` | file | Full markdown conversion |
| `.idx file` | file | Grep-optimized search index |
| origin PDF | file | Original source file under `origin/` |

---

### RefSource (in refs.yaml)

One entry in the `refs/refs.yaml` manifest.

| Field | Type | Description |
|-------|------|-------------|
| `name` | string | Source identifier, matches the cloned directory name under `refs/` |
| `uri` | string | Git clone URL |
| `sparse` | boolean | If true, clone with sparse checkout (`md/` only) |
| `description` | string (optional) | Human-readable description |

---

### BuildTarget

Describes a firmware build request.

| Field | Type | Description |
|-------|------|-------------|
| `profile` | string | Profile name matching a file in `profiles/<profile>.yml` |
| `mode` | enum: `server` \| `local` | Whether to build on remote server or local machine |
| `server_host` | string | SSH host for server-build (default: `192.168.20.30`) |
| `server_user` | string | SSH username (default: `ruanyaoyu`) |
| `docker_env` | string | Docker environment name passed to `run_build_docker` (default: `wf188_tip`) |
| `openwrt_dir` | string | Path to openwrt directory on the build host (default: `openwrt/`) |

**State transitions**:
```
REQUESTED → CONFIGURING (gen_config.py) → BUILDING (make) → SUCCESS | FAILED
```

**Validation rules**:
- `profile` must match an entry in `profiles/` directory
- For `local` mode, `openwrt/` must exist at repository root

---

### FirmwareImage

The output artifact of a successful build.

| Field | Type | Description |
|-------|------|-------------|
| `path` | string | Absolute or relative path to image file under `openwrt/bin/` |
| `filename` | string | Basename of the image file |
| `profile` | string | Profile used to build this image |
| `size_bytes` | integer | File size in bytes |

---

### DUTUpgrade

A firmware flashing operation targeting a Device Under Test.

| Field | Type | Description |
|-------|------|-------------|
| `dut_ip` | string | IP address of the DUT (default: `192.168.1.1`) |
| `dut_user` | string | SSH username (default: `root`) |
| `image_path` | string | Local path to the firmware image |
| `remote_path` | string | Destination on DUT (always `/tmp/<filename>`) |
| `sysupgrade_flags` | string | Flags passed to sysupgrade (default: `-n`) |

**State transitions**:
```
REQUESTED → TRANSFERRING (scp) → UPGRADING (sysupgrade) → REBOOTING (connection drop = success)
```

**Validation rules**:
- `image_path` must point to an existing readable file
- `dut_ip` must be reachable via SSH before SCP begins
- SSH connection drop after `sysupgrade` is treated as `REBOOTING` (success), not error

---

## Entity Relationships

```
refs/refs.yaml
  └── 1..N  RefSource
              └── clones to → refs/<name>/
                                └── contains → KnowledgeBase
                                                └── 0..N  Document

.github/skills/
  └── 1..N  Skill

profiles/<name>.yml
  └── referenced by → BuildTarget.profile

BuildTarget
  └── produces → FirmwareImage
                    └── consumed by → DUTUpgrade
```
