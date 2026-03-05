---
name: upgrade-dut
description: Transfer firmware image to a DUT and trigger sysupgrade over SSH
skill_version: "1.0.0"
---

# DUT Firmware Upgrade Skill

Upload a firmware image to an OpenWiFi Device Under Test (DUT) via SCP
and trigger `sysupgrade` over SSH. Connection drop after sysupgrade is
expected and treated as success.

## Usage

```
/upgrade-dut [<image-path>] [--dut-ip <ip>] [--dut-user <user>]
```

| Argument | Required | Default | Description |
|----------|----------|---------|-------------|
| `image-path` | No | auto-discovered | Path to firmware image file |
| `--dut-ip` | No | `192.168.1.1` | DUT management IP |
| `--dut-user` | No | `root` | SSH username on DUT |

### Examples

```
/upgrade-dut
/upgrade-dut openwrt/bin/targets/ipq40xx/generic/firmware.bin
/upgrade-dut --dut-ip 192.168.1.1 --dut-user root
/upgrade-dut firmware.bin --dut-ip 10.0.0.1
```

## How It Works

The skill delegates to `.github/skills/upgrade-dut/upgrade-dut.sh`:

1. **Resolve image**: If `image-path` is provided, verify the file exists.
   Otherwise, auto-discover from `openwrt/bin/`:
   - 0 found → print error and exit 1
   - 1 found → use automatically
   - >1 found → print numbered list, prompt for selection
2. **Verify DUT reachability**: SSH connect test. Exit 2 if unreachable.
3. **Transfer image**: `scp <image> root@<dut-ip>:/tmp/`
   Exit 3 on failure — sysupgrade is **NOT** executed.
4. **Execute upgrade**: `ssh root@<dut-ip> "sysupgrade -n /tmp/<filename>" || true`
5. **Report**: Print "Upgrade command issued. DUT is rebooting — connection drop
   is expected."

## Output

### Success

```
Transferring image to DUT (192.168.1.1)...
  firmware.bin → /tmp/firmware.bin  [OK]
Issuing sysupgrade...
Upgrade command issued. DUT is rebooting — connection drop is expected.
```

### Image not found (0 results)

```
ERROR: No firmware images found in openwrt/bin/.
Run /server-build <profile> or /local-build <profile> first.
```

### Multiple images found

```
Multiple firmware images found. Select one:
  [1] openwrt/bin/targets/ath79/generic/firmware-a.bin
  [2] openwrt/bin/targets/ipq40xx/generic/firmware-b.bin
Enter number: _
```

### DUT unreachable

```
ERROR: Cannot reach DUT at 192.168.1.1.
Check that the DUT is powered on and the management interface is up.
```

### SCP failure

```
ERROR: File transfer failed (scp exit 1).
Possible causes: DUT disk full (/tmp); network interrupted; wrong credentials.
Sysupgrade NOT executed.
```

## Exit Codes

| Code | Meaning |
|------|---------|
| 0 | Upgrade command issued (DUT rebooting) |
| 1 | Image file not found |
| 2 | DUT unreachable |
| 3 | SCP transfer failure |

## Connection Drop Behavior

`sysupgrade` causes the DUT to reboot immediately, dropping the SSH connection.
This is **expected** and handled with `|| true` — the exit code of sysupgrade
itself is ignored. The upgrade is considered issued as long as the SCP transfer
succeeded.

Wait approximately 2–3 minutes for the DUT to reboot before attempting to
reconnect.

## Security

- **Password is never stored** in this script or any committed file.
- Use SSH key authentication (recommended) for passwordless upgrades.
- If using `sshpass`, set the `SSH_PASS` environment variable:
  `SSH_PASS=openwifi /upgrade-dut firmware.bin`
- `/tmp/` on the DUT is cleared on every reboot — no cleanup step needed.

## Prerequisites

- Network access to the DUT management interface (`192.168.1.1`)
- Firmware image built by `/server-build` or `/local-build`
- DUT running OpenWiFi (supports `sysupgrade -n`)

## Related Skills

- `/server-build` — Build firmware remotely on the shared build server
- `/local-build` — Build firmware locally
