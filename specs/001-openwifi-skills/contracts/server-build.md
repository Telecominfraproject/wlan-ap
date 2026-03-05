# Contract: server-build

**Skill**: `server-build`  
**Version**: 1.0.0  
**Phase**: 1 — Design  
**Date**: 2026-03-05

## Invocation

```
/server-build <profile>
/server-build <profile> [--host <ip>] [--user <username>] [--docker-env <env>]
```

| Argument | Required | Default | Description |
|----------|----------|---------|-------------|
| `profile` | Yes | — | Profile name matching `profiles/<profile>.yml` |
| `--host` | No | `192.168.20.30` | Build server IP or hostname |
| `--user` | No | `ruanyaoyu` | SSH username |
| `--docker-env` | No | `wf188_tip` | Argument passed to `run_build_docker` |

## Workflow Steps

1. **Validate profile**: Check `profiles/<profile>.yml` exists in local repo. If not, print available profiles and exit 1.
2. **SSH connect**: `ssh <user>@<host>` — confirm reachability. If unreachable, suggest `/local-build` and exit 1.
3. **Start Docker**: `run_build_docker <docker-env>` on remote host.
4. **Configure**: `cd openwrt && ./scripts/gen_config.py <profile>`
5. **Build**: `make -j$(nproc) 2>&1 | tee /tmp/build-<profile>.log`
6. **Report**:
   - Success (exit 0): scan `openwrt/bin/` for image files, print paths.
   - Failure (exit ≠ 0): print last 50 lines of `/tmp/build-<profile>.log` and exit 1.

## Output Contract

### Success

```
Build succeeded: <profile>
Image: openwrt/bin/targets/<target>/<subtarget>/<image-filename>
```

### Failure

```
Build failed: <profile> (exit code <N>)
Last 50 lines of build log:
<log output>
```

### Profile not found

```
ERROR: Profile '<profile>' not found in profiles/
Available profiles:
  asterfusion_ap7330
  cig_wf186h
  ...
```

### Server unreachable

```
ERROR: Cannot reach build server <host>. Is it online?
Suggestion: Try /local-build <profile> for a local build instead.
```

## Exit Codes

| Code | Meaning |
|------|---------|
| 0 | Build succeeded |
| 1 | Profile validation error |
| 2 | SSH connection error |
| 3 | Build (make) failure |

## Security Notes

- Password `openwifi` is NOT stored in the script.
- If `sshpass` is available: `sshpass -p <password> ssh <user>@<host>` (password passed via env var `SSH_PASS`, not command argument).
- If `sshpass` is absent: SSH proceeds interactively; user is prompted once per session.
- SSH key authentication is the recommended long-term setup.
