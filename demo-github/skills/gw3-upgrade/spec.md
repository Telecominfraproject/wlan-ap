# gw3-upgrade Specification

| Field | Value |
|-------|-------|
| Status | Active |
| Version | 1.0.0 |
| Created | 2026-02-20 |
| Depends | ssh, scp, ping, find |

## Overview

The `gw3-upgrade` skill automates firmware deployment to a Gateway 3 DUT. It locates a built firmware image, transfers it to the device over SCP, and triggers `sysupgrade` to flash and reboot. The script performs pre-flight checks (connectivity, SSH access, available disk space) and provides post-upgrade guidance.

**Primary users**: Developers iterating on firmware builds who need to quickly flash a DUT from the build host.

**When to use**: After a successful `make` build produces a firmware image in `bin/targets/`, and a DUT is powered on and reachable via SSH.

## Architecture

### Directory Layout

```
.github/skills/gw3-upgrade/
  SKILL.md       # Usage documentation
  spec.md        # This specification
  upgrade.sh     # Main upgrade script
```

### Data Flow

```
upgrade.sh [IMAGE_PATH] [DUT_IP]
  |-- locate image (arg or find *-single-aei.img)
  |-- pre-flight: ping DUT
  |-- pre-flight: SSH echo test
  |-- pre-flight: check /tmp free space (>50 MB)
  |-- scp image --> root@DUT:/tmp/upgrade.img
  |-- ssh sysupgrade /tmp/upgrade.img (backgrounded)
  '--> print post-upgrade instructions
```

## Interface / Subcommands

### upgrade.sh

```
Usage: .github/skills/gw3-upgrade/upgrade.sh [IMAGE_PATH] [DUT_IP]

  IMAGE_PATH   Path to firmware .img file.
               Default: auto-detect first *-single-aei.img under
               bin/targets/ipq54xx/generic/
  DUT_IP       Target device IP address.
               Default: 192.168.1.1

Exit codes:
  0  Upgrade initiated successfully
  1  Pre-flight check failed or image not found
```

### sysupgrade Flags (manual use)

| Flag | Effect |
|------|--------|
| (none) | Default: flash image, wipe configuration, reboot |
| `-c` | Preserve configuration across upgrade |
| `-F` | Force upgrade, bypass image compatibility checks |

The script itself invokes `sysupgrade` without flags (clean flash). Developers wanting `-c` or `-F` must use the manual SSH workflow documented in SKILL.md.

## Logic / Workflow

### Step 1: Image Resolution

1. If `IMAGE_PATH` argument is provided, use it directly.
2. Otherwise, run `find` under `$PROJECT_ROOT/bin/targets/ipq54xx/generic/` for files matching `*-single-aei.img`.
3. Take the first match (`head -1`).
4. If no image is found, print error and exit 1.
5. Verify the resolved path is a regular file (`-f` test).

### Step 2: Pre-flight Checks

All checks use short timeouts to avoid blocking:

1. **Ping** -- `ping -c 1 -W 2 $DUT_IP`. Fail-fast if DUT is unreachable.
2. **SSH** -- `ssh -o ConnectTimeout=3 -o StrictHostKeyChecking=no root@DUT 'echo ok'`. Confirms SSH daemon is running and accepts connections.
3. **Disk space** -- `df /tmp` on the DUT via SSH; parses available kilobytes. Requires at least 51200 KB (50 MB) free. Prevents out-of-space failures mid-transfer.

### Step 3: Image Transfer

1. `scp -o StrictHostKeyChecking=no $IMAGE_PATH root@$DUT_IP:/tmp/upgrade.img`
2. Destination is always `/tmp/upgrade.img` (fixed name, simplifies sysupgrade call).
3. On SCP failure, print error and exit 1.

### Step 4: Sysupgrade Execution

1. `ssh root@$DUT_IP "sysupgrade /tmp/upgrade.img"` is launched in background (`&`).
2. Script sleeps 2 seconds to let the command start.
3. Checks if the SSH process is still alive; if so, waits for it (it will die when DUT reboots).
4. SSH disconnection during reboot is expected and suppressed.

### Step 5: Post-upgrade Guidance

Print instructions:
1. Wait 60-90 seconds for DUT reboot.
2. `ping $DUT_IP` to confirm it is back.
3. `ssh root@$DUT_IP 'cat /etc/prplos_version'` to verify firmware version.

## Safety Rules

- **`set -e` is enabled** -- any command failure aborts the script immediately, preventing partial upgrades from proceeding without all pre-flight checks passing.
- **No `-F` (force) flag by default** -- the script does not bypass image validation. If the image is incompatible, sysupgrade will refuse it.
- **No `-c` (keep config) by default** -- clean flash is the default to avoid stale configuration artifacts during development. Preserving config is a deliberate opt-in.
- **SSH uses `StrictHostKeyChecking=no`** -- acceptable on a private test LAN where DUT host keys change after every reflash.
- **No `--force-reinstall` or raw dd** -- only the standard sysupgrade path is used, which includes image validation.
- **Free space check** -- prevents SCP from filling `/tmp` and destabilizing the DUT before upgrade.
- **SSH is backgrounded** -- the script does not hang waiting for a connection that will be killed by the DUT reboot. The 2-second sleep plus `kill -0` / `wait` pattern handles the expected disconnect gracefully.
- **No automatic retry or reboot loop** -- if the upgrade fails, the developer must investigate manually (via console). The script never power-cycles or re-flashes automatically.

## Dependencies

| Dependency | Type | Purpose |
|------------|------|---------|
| `ssh` | System | Remote command execution on DUT |
| `scp` | System | Image transfer to DUT |
| `ping` | System | Connectivity pre-check |
| `find` | System | Auto-detect firmware image |
| `df` | DUT | Check `/tmp` free space |
| `sysupgrade` | DUT | OpenWrt/prplOS firmware flash utility |
| `gw3-console` skill | Optional | Monitoring upgrade progress via serial |
| `gw3-unit-test-setup` skill | Optional | Provides `DUT_LAN_IP` if sourced beforehand |

## Future Work

- Accept `-c` / `-F` flags as script arguments instead of requiring manual SSH.
- Post-upgrade polling loop to wait for DUT to come back online and verify the new version automatically.
- Integration with `local/ut-setup.sh` to read `DUT_LAN_IP` as the default target.
