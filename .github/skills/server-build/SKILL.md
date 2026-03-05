---
name: server-build
description: Build OpenWiFi firmware remotely on the shared build server via SSH
skill_version: "1.0.0"
---

# Remote Server Build Skill

Trigger a firmware build for OpenWiFi AP NOS on the shared build server
(`192.168.20.30`) over SSH. The build runs inside the Docker environment on
the remote host, producing firmware images in `openwrt/bin/`.

## Usage

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

### Examples

```
/server-build cig_wf188n
/server-build asterfusion_ap7330 --host 192.168.20.30 --user ruanyaoyu
/server-build cig_wf186h --docker-env wf188_tip
```

## How It Works

The skill delegates to `.github/skills/server-build/server-build.sh`:

1. **Validate profile**: Check `profiles/<profile>.yml` exists locally. Print
   available profiles and exit 1 if not found.
2. **SSH reachability check**: Confirm `<user>@<host>` is reachable.
   - If `sshpass` is available and `$SSH_PASS` env var is set, use it.
   - Otherwise, SSH proceeds interactively (key or password prompt).
   - If unreachable: suggest `/local-build <profile>` and exit 2.
3. **Start Docker**: Run `run_build_docker <docker-env>` on the remote host.
4. **Configure**: `cd openwrt && ./scripts/gen_config.py <profile>`
5. **Build**: `make -j$(nproc) 2>&1 | tee /tmp/build-<profile>.log`
6. **Report**:
   - Success (exit 0): list image paths from `openwrt/bin/`.
   - Failure (exit ≠ 0): print last 50 lines of build log and exit 3.

## Output

### Success

```
Build succeeded: cig_wf188n
Image: openwrt/bin/targets/ipq40xx/generic/openwifi-cig_wf188n-sysupgrade.bin
```

### Failure

```
Build failed: cig_wf188n (exit code 2)
Last 50 lines of build log:
...
```

### Profile not found

```
ERROR: Profile 'unknown_profile' not found in profiles/
Available profiles:
  asterfusion_ap7330
  cig_wf186h
  ...
```

### Server unreachable

```
ERROR: Cannot reach build server 192.168.20.30. Is it online?
Suggestion: Try /local-build cig_wf188n for a local build instead.
```

## Exit Codes

| Code | Meaning |
|------|---------|
| 0 | Build succeeded |
| 1 | Profile validation error |
| 2 | SSH connection error |
| 3 | Build (make) failure |

## Credentials

| Field | Value |
|-------|-------|
| Host | `192.168.20.30` |
| User | `ruanyaoyu` |
| Password | `openwifi` |

Credentials are hardcoded in `server-build.sh` for this project.
`sshpass` must be installed: `sudo apt install sshpass`

## Prerequisites

- SSH access to the build server (`192.168.20.30`)
- `sshpass` installed on the local machine
- `run_build_docker` command available on the remote host
- `openwrt/` directory initialized on the remote host
- Profile file `profiles/<profile>.yml` present in this repository

## Related Skills

- `/local-build` — Build firmware locally (no SSH required)
- `/upgrade-dut` — Flash built firmware to a DUT
