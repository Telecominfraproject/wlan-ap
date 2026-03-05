# Contract: upgrade-dut

**Skill**: `upgrade-dut`  
**Version**: 1.0.0  
**Phase**: 1 — Design  
**Date**: 2026-03-05

## Invocation

```
/upgrade-dut [<image-path>] [<dut-ip>]
/upgrade-dut [<image-path>] [--dut-ip <ip>] [--dut-user <user>]
```

| Argument | Required | Default | Description |
|----------|----------|---------|-------------|
| `image-path` | No | auto-discovered | Path to firmware image file |
| `dut-ip` | No | `192.168.1.1` | DUT management IP |
| `--dut-user` | No | `root` | SSH username on DUT |

## Workflow Steps

1. **Resolve image**: If `image-path` provided, verify file exists. If absent, run auto-discovery:
   - Search `openwrt/bin/` recursively for `*.bin` and `*.img` files.
   - If 0 found: print error and exit 1.
   - If 1 found: use it automatically.
   - If >1 found: print numbered list, prompt user to choose.
2. **Verify DUT reachability**: SSH connect test to `<dut-user>@<dut-ip>`. If unreachable, print error and exit 1.
3. **Transfer image**: `scp <image-path> <dut-user>@<dut-ip>:/tmp/`
4. **Execute upgrade**: `ssh <dut-user>@<dut-ip> "sysupgrade -n /tmp/<filename>" || true`
5. **Report**: Print "Upgrade command issued. DUT is rebooting — connection drop is expected."

## Output Contract

### Success

```
Transferring image to DUT (192.168.1.1)...
  <image-filename> → /tmp/<image-filename>  [OK]
Issuing sysupgrade...
Upgrade command issued. DUT is rebooting — connection drop is expected.
```

### Image not found (auto-discovery, 0 results)

```
ERROR: No firmware images found in openwrt/bin/.
Run /server-build <profile> or /local-build <profile> first.
```

### Image not found (auto-discovery, multiple results)

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
ERROR: File transfer failed (scp exit <N>).
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

## Security Notes

- Password `openwifi` is NOT stored in the script.
- SSH key or interactive password prompt used (same approach as server-build).
- `/tmp/` on the DUT is cleared on every reboot; no cleanup step needed.
