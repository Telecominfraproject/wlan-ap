---
name: firmware_upgrade
description: Automate firmware download and DUT upgrade for OpenSync devices (WF710G, WF728). Handles version file reading, firmware filename generation, download from build server, transfer through Jumphost, and sysupgrade on DUT. Use when upgrading firmware, flashing DUT, downloading firmware image, or running sysupgrade.
---

## How to invoke with parameters

When user invokes `/firmware_upgrade` or natural language, extract:
- **PROJECT** — device model (e.g. WF728, WF710G)
- **VERSION** — optional firmware version string

Then run:
```bash
cd /home/hughcheng/Project/WF728_260226/prplos/aei_ai_skills/firmware-upgrade
bash universal_firmware_upgrade.sh <PROJECT> [VERSION]
```

Examples:
- `/firmware_upgrade WF728` → `bash universal_firmware_upgrade.sh WF728`
- `/firmware_upgrade WF728 0.9.0-p.18` → `bash universal_firmware_upgrade.sh WF728 0.9.0-p.18`
- `/firmware_upgrade WF710G 12.2.6.25-WF710X-T26011002` → `bash universal_firmware_upgrade.sh WF710G 12.2.6.25-WF710X-T26011002`


# firmware-upgrade Skill

Automate firmware download and DUT (Device Under Test) upgrade with version file integration. This skill manages the complete firmware upgrade workflow: reading version files, generating firmware filenames, downloading from source server, transferring through Jumphost, and executing sysupgrade on DUT.

## When to use this skill

Use this skill when:
- User wants to upgrade firmware on DUT device
- User mentions firmware upgrade, sysupgrade, flash, or deployment
- User asks to upgrade to a specific version
- User wants to deploy built firmware to device
- Keywords: "升级固件", "upgrade firmware", "flash", "deploy", "sysupgrade"

## Key Features

- **Multi-project support**: WF710G, WF810, WF710 with independent configurations
- **Automatic version detection**: Reads version from source server if not specified
- **Smart version conversion**: Supports two version formats
  - Without timestamp: `12.2.6.25-WF710X` → `nand-ipq5332-single-12.2.6.25-WF710X-dev.img`
  - With timestamp: `12.2.6.25-WF710X-T2601406` → `nand-ipq5332-single-12.2.6.25-WF710X-dev-T2601406.img`
- **Three-stage upgrade**: Download → Transfer → Upgrade
- **Integration with ssh-connection skill**: Reuses SSH configuration for Jumphost and DUT access

## Usage

### Basic usage (recommended)
```bash
cd /home/hughcheng/Project/WF710G_260109/.github/skills/firmware-upgrade
./universal_firmware_upgrade.sh WF710G
```
This will automatically read the version from the version file on the source server.

### Specify version manually
```bash
# Without timestamp
./universal_firmware_upgrade.sh WF710G 12.2.6.25-WF710X

# With timestamp
./universal_firmware_upgrade.sh WF710G 12.2.6.25-WF710X-T2601406
```

### Upgrade workflow
1. **Read version**: From source server's version file or command parameter
2. **Generate firmware filename**: Apply version-to-filename conversion rules
3. **Download firmware**: From source server (192.168.20.33) to Jumphost (172.16.10.81)
4. **Transfer firmware**: From Jumphost to DUT (192.168.40.1)
5. **Execute upgrade**: Run sysupgrade on DUT and reboot

## Configuration

### Project Configuration (`firmware_upgrade.conf`)
Each project has these settings:
- `{PROJECT}_VERSION_FILE`: Path to version file (relative to SRC_BASE_PATH)
- `{PROJECT}_FIRMWARE_PREFIX`: Firmware filename prefix (e.g., `nand-ipq5332-single-`)
- `{PROJECT}_FIRMWARE_SUFFIX`: Firmware filename suffix (e.g., `.img`)
- `{PROJECT}_SRC_HOST`: Source server IP
- `{PROJECT}_SRC_USER`: Source server username
- `{PROJECT}_SRC_PASSWORD`: Source server password
- `{PROJECT}_SRC_BASE_PATH`: Base path on source server

### SSH Configuration
Reuses `../ssh-connection/ssh_dut.conf` for:
- Jumphost credentials and settings
- DUT credentials and network configuration

## Version Conversion Rules

The skill automatically converts version strings to firmware filenames:

| Version Format | Firmware Filename |
|----------------|------------------|
| `12.2.6.25-WF710X` | `nand-ipq5332-single-12.2.6.25-WF710X-dev.img` |
| `12.2.6.25-WF710X-T2601406` | `nand-ipq5332-single-12.2.6.25-WF710X-dev-T2601406.img` |

Conversion logic:
- If version contains `-T{digits}`: Insert `-dev` before `-T`
- If version doesn't contain `-T`: Append `-dev`
- Add prefix and suffix from configuration

## Dependencies

- `ssh-connection` skill for Jumphost and DUT access
- `sshpass` for password-based SSH authentication
- Source server accessible from Jumphost
- DUT accessible from Jumphost on network 192.168.40.x

## Files

- `universal_firmware_upgrade.sh`: Main upgrade script
- `firmware_upgrade.conf`: Multi-project configuration file
- `SKILL.md`: This documentation

## Examples

### Example 1: Upgrade WF710G with auto-detected version
```bash
./universal_firmware_upgrade.sh WF710G
```
Output:
```
========================================
通用固件升级脚本 / Universal Firmware Upgrade Script
项目 / Project: WF710G
========================================
>>> 加载配置文件 / Loading configuration...
✓ 配置文件加载完成 / Configuration loaded
>>> 加载项目配置 / Loading project configuration...
✓ 项目 WF710G 配置加载成功 / Project WF710G configuration loaded
>>> 未指定版本，从源服务器读取 / Version not specified, reading from source server
    路径 / Path: hughcheng@192.168.20.33:WiFi7/sdk.qca.comp/qca/qca-networking/WiFi7/opensync_ws/OPENSYNC/WF710G/modify/qsdk/version
✓ 版本号: 12.2.6.25-WF710X-T2601406
>>> 生成固件文件名 / Generating firmware filename...
✓ 固件文件名: nand-ipq5332-single-12.2.6.25-WF710X-dev-T2601406.img
>>> 开始升级流程 / Starting upgrade process...
[Steps 1-3 execute...]
✓ 升级流程完成 / Upgrade process completed
```

### Example 2: Upgrade WF810 with specified version
```bash
./universal_firmware_upgrade.sh WF810 12.2.6.25-WF810X
```

### Example 3: Integration with build workflow
```bash
# Step 1: Build firmware
cd /home/hughcheng/Project/WF710G_260109/.github/skills/universal-build
./universal_build_skill.sh WF710G 12.2.6.25-WF710X-T2601406

# Step 2: Upgrade DUT with built firmware
cd ../firmware-upgrade
./universal_firmware_upgrade.sh WF710G
```

## Troubleshooting

### Firmware download failed
- Check source server credentials in `firmware_upgrade.conf`
- Verify firmware file exists on source server
- Ensure Jumphost can access source server

### Firmware transfer failed
- Verify DUT is accessible from Jumphost
- Check network configuration (192.168.40.x)
- Ensure SSH credentials are correct in `../ssh-connection/ssh_dut.conf`

### Upgrade command execution failed
- Check DUT has sufficient space in /var
- Verify sysupgrade command is available on DUT
- Ensure firmware file is compatible with device

## Related Skills

- `universal-build`: Build OpenSync firmware before upgrading
- `ssh-connection`: Manage SSH connections to Jumphost and DUT

## Notes

- DUT will automatically reboot after successful upgrade (typically 2-3 minutes)
- Firmware files are transferred to `/var/` directory on DUT
- The skill uses sysupgrade with `-s -n` flags (save settings, no backup)
- Version file is expected to contain a single line with version string
