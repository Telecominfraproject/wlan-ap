---
name: build_and_upgrade
description: Full firmware development workflow for OpenSync devices — automate version update, Docker build, firmware download, DUT upgrade, and validation in one flow. Use when doing complete build-and-flash cycle, build then upgrade, or end-to-end firmware deployment for WF710G or WF728.
---

## How to invoke with parameters

When user invokes this skill via `/build_and_upgrade` or natural language, extract:
- **PROJECT** — device model (e.g. WF728, WF710G, WF810)
- **VERSION** — firmware version string (e.g. 0.9.0-p.18, 12.2.6.25-WF710X-T26011002)

Then run:
```bash
cd /home/hughcheng/Project/WF728_260226/prplos/aei_ai_skills/build-and-upgrade
bash universal_build_and_upgrade.sh <PROJECT> <VERSION>
```

Examples:
- `/build_and_upgrade WF728 0.9.0-p.18` → `bash universal_build_and_upgrade.sh WF728 0.9.0-p.18`
- `/build_and_upgrade WF710G 12.2.6.25-WF710X-T26011002` → `bash universal_build_and_upgrade.sh WF710G 12.2.6.25-WF710X-T26011002`

# Build and Upgrade Skill

完整的固件开发流程：自动化版本更新、构建、升级和验证。

## 功能描述 / Function Description

这个 skill 整合了完整的固件开发流程：
- 更新版本号
- 构建固件（调用 universal-build skill）
- 查找生成的镜像文件
- 升级固件到 DUT（调用 firmware-upgrade skill）
- 等待设备重启
- 验证版本

This skill integrates the complete firmware development workflow:
- Update version number
- Build firmware (calls universal-build skill)
- Locate generated image file
- Upgrade firmware to DUT (calls firmware-upgrade skill)
- Wait for device reboot
- Verify version

## 使用场景 / Use Cases

用户可以通过以下自然语言触发此 skill：

**中文示例**:
- "构建并升级 WF710G 固件"
- "自动构建和烧录 WF710G 12.2.6.25-WF710X-T26011002"
- "完整流程构建升级 WF710G 固件"
- "Build and flash WF710G"

**英文示例**:
- "Build and upgrade WF710G firmware"
- "Complete build and flash workflow for WF710G"
- "Auto build and deploy WF710G 12.2.6.25-WF710X-T26011002"

## 直接运行 / Direct Execution

```bash
# 使用默认项目和版本
./universal_build_and_upgrade.sh

# 指定项目
./universal_build_and_upgrade.sh WF710G

# 指定项目和版本
./universal_build_and_upgrade.sh WF710G 12.2.6.25-WF710X-T26011002

# 显示帮助
./universal_build_and_upgrade.sh help
```

## 工作流程 / Workflow

```
[1/7] 更新版本号 / Update Version Number
      ↓
[2/7] 构建固件 / Build Firmware (30-60 分钟)
      ↓
[3/7] 查找镜像文件 / Locate Image File
      ↓
[4/7] 升级固件 / Upgrade Firmware
      ↓
[5/7] 等待设备断开 / Wait for Device Disconnect
      ↓
[6/7] 等待 SSH 就绪 / Wait for SSH Ready
      ↓
[7/7] 验证版本 / Verify Version
      ↓
    ✅ 完成 / Completed
```

## 配置文件 / Configuration

### build_upgrade.conf

主配置文件，包含：

```bash
# WORKSPACE 配置（留空自动检测）
WORKSPACE_DEFAULT=""

# 项目特定配置
WF710G_WORKSPACE=""
WF810_WORKSPACE=""
WF710_WORKSPACE=""

# 默认项目和版本
DEFAULT_PROJECT="WF710G"
DEFAULT_VERSION="12.2.6.25-WF710X-T26011002"

# 版本文件路径（相对于 WORKSPACE）
WF710G_VERSION_FILE="WiFi7/opensync_ws/OPENSYNC/WF710G/modify/qsdk/version"

# 镜像文件路径（相对于 WORKSPACE）
IMAGE_BASE_PATH="WiFi7/sdk.qca.comp/qca/qca-networking"

# 固件命名规则
WF710G_FIRMWARE_PREFIX="nand-ipq5332-single-"
WF710G_FIRMWARE_SUFFIX="-dev"

# 等待时间配置
MAX_WAIT_DISCONNECT=60       # 等待设备断开（秒）
MAX_WAIT_SSH_READY=180       # 等待 SSH 就绪（秒）
CHECK_INTERVAL=5             # 检测间隔（秒）

# DUT 版本文件
DUT_VERSION_FILE="/sbin/version"
```

## 依赖 Skills / Skill Dependencies

此 skill 依赖于以下 skills，通过相对路径调用：

1. **universal-build** - 固件构建
   - 路径: `../universal-build/universal_build_skill.sh`
   - 用途: 在 Docker 容器中构建固件

2. **firmware-upgrade** - 固件升级
   - 路径: `../firmware-upgrade/universal_firmware_upgrade.sh`
   - 用途: 下载、传输、升级固件到 DUT

3. **ssh-connection** - SSH 连接
   - 路径: `../ssh-connection/ssh2jumphost2dut.sh`
   - 用途: 通过 Jumphost 连接 DUT，执行命令

## WORKSPACE 机制 / WORKSPACE Mechanism

### 自动检测 / Auto-detection

脚本自动检测项目根目录：
```bash
# 脚本位置: .github/skills/build-and-upgrade/universal_build_and_upgrade.sh
# WORKSPACE = 3 级父目录 = 项目根目录
WORKSPACE=$(dirname $(dirname $(dirname $SCRIPT_DIR)))
```

### 手动配置 / Manual Configuration

在 `build_upgrade.conf` 中设置：
```bash
# 全局默认
WORKSPACE_DEFAULT="/path/to/project"

# 项目特定覆盖
WF710G_WORKSPACE="/specific/path/for/WF710G"
```

## 版本转换 / Version Conversion

支持两种版本格式：

1. **带时间戳** / With timestamp:
   ```
   12.2.6.25-WF710X-T2601406
   → nand-ipq5332-single-12.2.6.25-WF710X-dev-T2601406.img
   ```

2. **不带时间戳** / Without timestamp:
   ```
   12.2.6.25-WF710X
   → nand-ipq5332-single-12.2.6.25-WF710X-dev.img
   ```

## 输出示例 / Output Example

```
========================================
完整构建和升级流程 / Complete Build and Upgrade Process
========================================
项目 / Project: WF710G
版本 / Version: 12.2.6.25-WF710X-T26011002
工作区 / Workspace: /home/hughcheng/Project/WF710G_260109
目标设备 / Target Device: 192.168.40.1
========================================

========================================
[步骤 1/7] 更新版本号
[Step 1/7] Update Version Number
========================================
>>> 版本文件路径 / Version file path: /home/.../version
✓ 版本号已更新 / Version number updated: 12.2.6.25-WF710X-T26011002

========================================
[步骤 2/7] 构建固件（预计 30-60 分钟）
[Step 2/7] Build Firmware (Estimated 30-60 minutes)
========================================
>>> 开始构建 / Starting build...
✓ 固件构建完成 / Firmware build completed

========================================
[步骤 3/7] 查找生成的镜像文件
[Step 3/7] Locate Generated Image File
========================================
✓ 找到镜像文件 / Image file found
>>> 文件大小 / File size: 45M

========================================
[步骤 4/7] 升级固件到设备
[Step 4/7] Upgrade Firmware to Device
========================================
✓ 固件升级完成 / Firmware upgrade completed

========================================
[步骤 5/7] 等待设备断开连接（确认重启开始）
[Step 5/7] Wait for Device Disconnect (Confirm Reboot Start)
========================================
✓ 设备已断开，正在重启 / Device disconnected, rebooting (10秒)

========================================
[步骤 6/7] 等待 SSH 服务就绪
[Step 6/7] Wait for SSH Service Ready
========================================
✓ SSH 服务已就绪 / SSH service ready (等待时间: 85秒)

========================================
[步骤 7/7] 验证固件版本
[Step 7/7] Verify Firmware Version
========================================
>>> 预期版本 / Expected version: 12.2.6.25-WF710X-T26011002
>>> 实际版本 / Actual version: nand-ipq5332-single-12.2.6.25-WF710X-dev-T26011002

========================================
✅ 升级成功！ / Upgrade Successful!
========================================

========================================
🎉 流程完成 / Process Completed
========================================
```

## 可移植性 / Portability

### ✅ 优势特性 / Advantages

1. **无绝对路径** - 所有路径都是相对的
2. **自动 WORKSPACE 检测** - 无需手动配置项目路径
3. **复用其他 Skills** - 模块化设计，便于维护
4. **跨机器兼容** - 在任何机器上都能运行

### 🚀 跨机器部署 / Cross-machine Deployment

```bash
# 在新机器上
1. git clone 项目到任意位置
2. cd .github/skills/build-and-upgrade
3. ./universal_build_and_upgrade.sh WF710G
   # WORKSPACE 自动检测，无需修改任何配置
```

## 错误处理 / Error Handling

脚本包含完善的错误处理机制：

1. **构建失败** - 自动停止，显示错误信息
2. **镜像文件未找到** - 显示预期路径和最近的镜像文件
3. **升级失败** - 停止流程，保留日志
4. **SSH 连接超时** - 提示手动检查设备
5. **版本不匹配** - 警告但不失败，便于人工确认

## 时间估算 / Time Estimation

- 更新版本号: < 1 秒
- 构建固件: 30-60 分钟（取决于硬件）
- 查找镜像: < 1 秒
- 升级固件: 2-3 分钟
- 等待断开: 5-15 秒
- 等待 SSH: 60-120 秒
- 验证版本: < 5 秒

**总计**: 约 35-65 分钟

## 故障排除 / Troubleshooting

### 构建失败

```bash
# 检查 Docker 是否运行
docker ps

# 检查 WORKSPACE
echo $WORKSPACE

# 手动测试构建 skill
cd ../universal-build
./universal_build_skill.sh WF710G 12.2.6.25-WF710X-T26011002
```

### 升级失败

```bash
# 检查 SSH 连接
cd ../ssh-connection
./ssh2jumphost2dut.sh "uptime"

# 手动测试升级 skill
cd ../firmware-upgrade
./universal_firmware_upgrade.sh WF710G 12.2.6.25-WF710X-T26011002
```

### 镜像文件未找到

```bash
# 检查镜像目录
ls -lht $WORKSPACE/WiFi7/sdk.qca.comp/qca/qca-networking/*.img

# 检查版本转换
# 确保版本号格式正确
```

## 扩展支持 / Extension Support

添加新项目（例如 WF900）：

1. 在 `build_upgrade.conf` 中添加配置：
```bash
WF900_WORKSPACE=""
WF900_VERSION_FILE="WiFi7/opensync_ws/OPENSYNC/WF900/modify/qsdk/version"
WF900_FIRMWARE_PREFIX="nand-ipq5332-single-"
WF900_FIRMWARE_SUFFIX="-dev"
WF900_VERSION_PATTERN="WF900X"
```

2. 运行：
```bash
./universal_build_and_upgrade.sh WF900 12.2.6.25-WF900X-T26011002
```

## 最佳实践 / Best Practices

1. **版本号规范** - 始终使用带时间戳的版本号以便追溯
2. **构建前检查** - 确认版本文件路径正确
3. **备份重要文件** - 脚本自动备份版本文件
4. **监控构建过程** - 构建需要较长时间，可在后台运行
5. **验证结果** - 完成后手动登录 DUT 再次确认

## 参考文档 / References

- [universal-build SKILL.md](../universal-build/SKILL.md)
- [firmware-upgrade SKILL.md](../firmware-upgrade/SKILL.md)
- [ssh-connection SKILL.md](../ssh-connection/SKILL.md)
- [README.md](../README.md)

---

**维护者**: GitHub Copilot Agent Skills Team  
**更新日期**: 2026-01-14  
**版本**: 1.0.0
