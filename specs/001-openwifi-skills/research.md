# Research: OpenWiFi Agents Skills Suite

**Feature**: 001-openwifi-skills  
**Phase**: 0 — Research  
**Date**: 2026-03-05

---

## Finding 1: Skill file conventions in demo-github/skills/

**Decision**: All skills follow a `SKILL.md` + optional shell-script pattern.  
`SKILL.md` contains the YAML front-matter (`name`, `description`, `allowed-tools`) 
and the full agent-readable invocation instructions. Supporting scripts live 
alongside. No build system or packaging needed.

**Rationale**: Simple file convention requires zero tooling to deploy; copying 
the directory to `.github/skills/` is the entire deployment step.

**Alternatives considered**: Packaging as npm/pip module — rejected, unnecessary 
complexity for static instruction files.

---

## Finding 2: Adaptation scope — gw3-specific references

**Decision**: The following files contain `gw3-reference-docs` hard-coded and 
MUST be updated during adaptation:

| File | Change needed |
|------|---------------|
| `kb-add-file/SKILL.md` | Change `source_kb: gw3-reference-docs` → `source_kb: openwifi-reference-docs`; update usage examples |
| `kb-add-batch/SKILL.md` | Same `source_kb` frontmatter and inline examples |
| `kb-remove/SKILL.md` | Same `source_kb` frontmatter |
| `refs-download/SKILL.md` | Replace all `gw3-reference-docs` occurrences with `openwifi-reference-docs`; update size estimates |

All Python scripts in jira-communication and confluence are **URL-agnostic** 
(credentials fetched from `~/.env.jira` and `~/.env.confluence` at runtime) — 
**no code changes needed**.

The `detect-wsl/detect-wsl.sh`, `jira-syntax/scripts/validate-jira-syntax.sh`, 
and `bkt/` documentation are fully platform-agnostic — **copy verbatim**.

**Rationale**: Minimal-touch adaptation preserves ability to pull upstream 
improvements from the gw3 skill source later.

**Alternatives considered**: Forking all SKILL.md files with openwifi-specific 
rewrites — rejected, creates unnecessary maintenance divergence.

---

## Finding 3: refs-setup.sh and refs.yaml are absent in openwifi

**Decision**: The `refs-download` skill depends on `scripts/refs-setup.sh` and 
`refs/refs.yaml`. Neither exists in the openwifi repository. Both must be created:

1. `scripts/refs-setup.sh` — a simplified clone of the gw3 version with the 
   `gw3-reference-docs` default source replaced by `openwifi-reference-docs`. 
   The sparse-checkout logic is preserved.
2. `refs/refs.yaml` — initial manifest with one entry for `openwifi-reference-docs` 
   (URL TBD by team; placeholder used until KB repository is created).

**Rationale**: The refs ecosystem is self-contained; creating the two files 
makes `/refs-download` immediately functional.

**Alternatives considered**: Skipping refs-download until a KB repo exists — 
rejected, the SKILL.md + scaffold should be in place so developers can populate 
it when ready.

---

## Finding 4: server-build — SSH + Docker invocation pattern

**Decision**: The skill defines a six-step workflow executed sequentially via 
the agent using native SSH commands:

```
Step 1: ssh ruanyaoyu@192.168.20.30 "run_build_docker wf188_tip"
Step 2: ssh ruanyaoyu@192.168.20.30 "cd openwrt && ./scripts/gen_config.py <PROFILE>"
Step 3: ssh ruanyaoyu@192.168.20.30 "cd openwrt && make -j\$(nproc) 2>&1 | tee /tmp/build-<PROFILE>.log"
Step 4: Parse log tail for errors or image path
```

The wrapper script `server-build.sh` encapsulates steps 1–4 and passes the 
profile name as `$1`. It handles:
- Profile validation against a local copy of `profiles/` file list
- Non-zero exit code detection → tail last 50 lines from remote log
- Zero exit → grep for `openwrt/bin/` image path reported to user

**Credential handling**: The password `openwifi` is not stored in the script. 
The script checks for `sshpass` first; if absent it instructs the user to set 
up SSH key authentication or enter the password interactively.

**Rationale**: A thin wrapper script is preferable to an MCP-dependent approach 
because it works in any terminal session without VS Code.

**Alternatives considered**: Using the agent to issue raw SSH commands without 
a wrapper — rejected, less testable and harder to invoke directly outside agent.

---

## Finding 5: local-build — validation before make

**Decision**: The `local-build.sh` script must first verify that `openwrt/` 
exists. If absent, it prints a clear error message:

```
ERROR: openwrt/ directory not found. Run ./setup.py --setup first.
See README.md §Building for instructions.
```

Profile validation follows the same approach as server-build: the script 
accepts `$1` and validates it against `ls profiles/*.yml | sed 's|.*/||;s|\.yml||'`.

**Rationale**: Immediate helpful error message prevents the silent failure of 
running `./scripts/gen_config.py` in the wrong directory.

---

## Finding 6: upgrade-dut — sysupgrade connection-drop behaviour

**Decision**: `upgrade-dut.sh` must treat SSH exit code 255 (connection reset 
after reboot) as a successful outcome when it occurs after the `sysupgrade` 
command is issued. Standard SSH exit 255 would normally be treated as an error.

Implementation pattern:
```bash
ssh root@${DUT_IP} "sysupgrade -n /tmp/${FILENAME}" || true
echo "Upgrade command issued. DUT is rebooting — connection drop is expected."
```

**Rationale**: `sysupgrade` kills the SSH session mid-execution as part of its 
normal reboot process. Treating this as an error would confuse developers into 
thinking the upgrade failed.

**Alternatives considered**: Monitoring reboot via polling — adds complexity; 
out of scope for MVP; developers can check DUT manually post-reboot.

---

## Finding 7: Auto-discovery of firmware images

**Decision**: When no image path is provided, `upgrade-dut.sh` searches 
`openwrt/bin/` recursively for files matching known OpenWiFi image name 
patterns: `*.bin`, `*.img`, `*-factory.bin`, `*-sysupgrade.bin`. If multiple 
matches, present a numbered list and prompt via stdin.

**Rationale**: The `openwrt/bin/` tree structure varies by target; a recursive 
glob is more robust than hardcoding subdirectory patterns.

---

## Finding 8: Skill placement path convention

**Decision**: All skills go under `.github/skills/<name>/`. This is consistent 
with VS Code Copilot's auto-discovery of `.github/` agent files and mirrors the 
AGENTS.md instruction files already present in `demo-github/skills/`.

The copilot-instructions.md in `demo-github/` references `.github/skills/` as 
the documented location for skill files used in the OpenWiFi AI agent context.

**Rationale**: Centralised under `.github/` makes skills discoverable by 
GitHub Copilot and any other agent that respects the `.github/` conventions.

---

## Summary: All NEEDS CLARIFICATION items resolved

| Unknown | Resolution |
|---------|------------|
| KB name for openwifi | `openwifi-reference-docs` |
| refs-setup.sh location | `scripts/refs-setup.sh` (new file) |
| refs.yaml location | `refs/refs.yaml` (new file) |
| server-build credential approach | `sshpass` check + fallback to interactive |
| sysupgrade SSH drop handling | `|| true` guard, explicit success message |
| Image auto-discovery pattern | Recursive glob `openwrt/bin/**/*.bin,*.img` |
| Skill installation path | `.github/skills/<name>/` |
