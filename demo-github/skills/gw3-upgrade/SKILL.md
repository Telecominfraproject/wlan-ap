---
name: gw3-upgrade
description: Upgrade Gateway 3 DUT with firmware image
---

# Gateway 3 Upgrade Skill

Automates firmware upgrade of Gateway 3 Device Under Test (DUT) using sysupgrade.

## Overview

This skill handles the complete firmware upgrade process:
1. Locates the built firmware image
2. Copies image to DUT via SCP
3. Executes sysupgrade command
4. Monitors upgrade progress via console

## Prerequisites

- DUT must be reachable via network (SSH working)
- Firmware image built in `bin/targets/ipq54xx/generic/`
- Console session active (recommended for monitoring)

## Usage

### Basic Upgrade

```bash
# Upgrade with auto-detected image
.github/skills/gw3-upgrade/upgrade.sh

# Upgrade with specific image
.github/skills/gw3-upgrade/upgrade.sh bin/targets/ipq54xx/generic/emmc-ipq5424_64-0.9.0-p.1-single-aei.img

# Specify DUT IP if not default
.github/skills/gw3-upgrade/upgrade.sh <image> 192.168.1.1
```

### Manual Upgrade Steps

1. **Copy image to DUT**:
```bash
scp ./bin/targets/ipq54xx/generic/emmc-ipq5424_64-0.9.0-p.1-single-aei.img root@192.168.1.1:/tmp
```

2. **Execute upgrade**:
```bash
ssh root@192.168.1.1
sysupgrade /tmp/emmc-ipq5424_64-0.9.0-p.1-single-aei.img
```

3. **Monitor via console** (recommended):
```bash
# Attach to console
screen -r gw3-console-ttyUSB0

# Or capture output
.github/skills/gw3-console/console_capture.sh ttyUSB0 10
```

## Image File Naming

Gateway 3 firmware images follow the pattern:
- `emmc-ipq5424_64-<version>-single-aei.img` - Single partition image
- Located in `bin/targets/ipq54xx/generic/` after build

## Upgrade Process

1. **Pre-upgrade checks**:
   - Verify DUT connectivity
   - Verify image file exists
   - Check available space on /tmp

2. **Image transfer**:
   - SCP copy to `/tmp` directory on DUT
   - Typical size: 20-50 MB, takes 5-10 seconds

3. **Sysupgrade execution**:
   - Validates image
   - Writes to flash/eMMC
   - Automatically reboots DUT
   - Typical duration: 1-2 minutes

4. **Post-upgrade**:
   - DUT reboots automatically
   - Boot process: 30-60 seconds
   - Network configuration restored
   - Verify new firmware version

## Safety Features

- **No configuration backup**: `sysupgrade` by default does not preserve settings
- **Keep settings**: Add `-c` flag to preserve configuration
- **Force upgrade**: Add `-F` flag to bypass compatibility checks

```bash
# Preserve configuration
sysupgrade -c /tmp/image.img

# Force upgrade (use with caution)
sysupgrade -F /tmp/image.img
```

## Monitoring Upgrade

**Via Console** (recommended):
```bash
# Monitor boot process after upgrade
tail -f local/records/console_ttyUSB0_*.log
```

**Via SSH** (will disconnect during upgrade):
```bash
# SSH will timeout when DUT reboots
# Wait 60 seconds, then reconnect
sleep 60
ssh root@192.168.1.1 'cat /etc/prplos_version'
```

## Troubleshooting

### Image not found
```bash
# List available images
ls -lh bin/targets/ipq54xx/generic/*.img

# Build firmware first
.github/skills/gw3-build/build.sh
```

### SCP fails
```bash
# Check network connectivity
ping -c 2 192.168.1.1

# Verify SSH access
ssh root@192.168.1.1 'df -h /tmp'
```

### Upgrade hangs
```bash
# Check console for error messages
.github/skills/gw3-console/console_capture.sh ttyUSB0 20

# If needed, power cycle DUT
```

### DUT doesn't boot after upgrade
```bash
# Access U-Boot via console
# Press any key during boot countdown
# Restore previous firmware or enter recovery mode
```

## Examples

### Complete upgrade workflow

```bash
# 1. Build firmware
.github/skills/gw3-build/build.sh prpl

# 2. Verify image exists
ls -lh bin/targets/ipq54xx/generic/*-single-aei.img

# 3. Upgrade DUT
.github/skills/gw3-upgrade/upgrade.sh

# 4. Monitor via console
.github/skills/gw3-console/console_capture.sh ttyUSB0 30

# 5. Wait for boot (60 seconds)
sleep 60

# 6. Verify new version
ssh root@192.168.1.1 'cat /etc/prplos_version'
```

### Upgrade with settings preservation

```bash
# Copy and upgrade with config backup
IMAGE=$(ls bin/targets/ipq54xx/generic/*-single-aei.img | head -1)
scp "$IMAGE" root@192.168.1.1:/tmp/upgrade.img
ssh root@192.168.1.1 'sysupgrade -c /tmp/upgrade.img'
```

## Integration with Other Skills

- **gw3-build**: Build firmware before upgrade
- **gw3-console**: Monitor upgrade process and boot
- **gw3-unit-test-setup**: Verify network connectivity before upgrade

## Notes

- **Backup important data**: Upgrade may erase configuration
- **Console access recommended**: Essential for troubleshooting boot issues
- **Network loss expected**: SSH disconnects during upgrade, DUT reboots
- **Wait for boot**: Allow 60-90 seconds after upgrade for full system boot
- **Verify version**: Always check firmware version after upgrade
