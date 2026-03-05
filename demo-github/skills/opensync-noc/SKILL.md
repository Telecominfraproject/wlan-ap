# OpenSync NOC Automation Skill

**Automate OpenSync NOC node inspection with screenshots and firmware version extraction.**

This skill automates web browser interactions with the OpenSync Network Operations Center (NOC) to inspect nodes, view topology, navigate to Pods & Nodes page, capture screenshots, and extract firmware versions.

---

## ⭐ Key Features

- ✅ **Okta Authentication**: Automatic login to OpenSync NOC
- 🔍 **Node Search**: Search and navigate to specific nodes by ID
- 🗺️ **Topology Visualization**: View network topology
- 📸 **Screenshot Capture**: Capture screenshots at key stages (especially Nodes and Device page)
- 📊 **Firmware Extraction**: Extract and save firmware version information
- 🎯 **VNC Preview**: View browser execution in real-time via VNC

---

## 📦 Prerequisites

### 1. Selenium Grid (Required)

Start Selenium Grid with VNC viewer:

```bash
docker run -d -p 4444:4444 -p 7900:7900 \
  --name selenium \
  selenium/standalone-chrome:latest
```

Check status:
```bash
curl http://localhost:4444/wd/hub/status
```

VNC Preview: http://localhost:7900 (Password: `secret`)

### 2. Python Dependencies (Required)

```bash
pip3 install selenium
```

### 3. Network Configuration

- HTTP Proxy: `10.20.11.161:3128` (configured in `noc_config.conf`)
- OpenSync NOC URL: `https://opensync.noc.plume.com/`

---

## 🚀 Usage

### Natural Language (from Chat)

Simply ask:
- "Check node 1HG231000110 on OpenSync NOC"
- "Inspect OpenSync node and take screenshot"
- "Get firmware version for node 1HG231000110"

### Direct Execution

**Default node ID (from config):**
```bash
cd /home/hughcheng/Project/WF710G_260109/.github/skills/opensync-noc
./universal_opensync_noc.sh
```

**Specify node ID:**
```bash
./universal_opensync_noc.sh 1HG231000110
```

**Help:**
```bash
./universal_opensync_noc.sh --help
```

---

## 📂 Output Structure

```
WF710G_260109/
├── screenshots/opensync-noc/
│   ├── 01_initial.png              # Initial page
│   ├── 02_logged_in.png            # After login
│   ├── 03_node_selected.png        # Node option selected
│   ├── 04_search_input.png         # Search input
│   ├── 05_node_page.png            # Node detail page
│   ├── 06_topology.png             # Topology view
│   ├── 07_nodes_and_device.png     # ⭐⭐⭐ KEY: Nodes and Device page
│   └── 08_final.png                # Final page
│
└── results/opensync-noc/
    └── firmware_version_<NODE_ID>.txt   # Firmware version info
```

---

## ⚙️ Configuration

Configuration file: `noc_config.conf`

### Key Settings:

```bash
# OpenSync NOC
NOC_URL="https://opensync.noc.plume.com/"
NOC_USERNAME="chenghongliang@cigtech.com"
NOC_PASSWORD="Hugh5678@AEI"

# Selenium Grid
SELENIUM_URL="http://localhost:4444/wd/hub"
SELENIUM_VNC_URL="http://localhost:7900"
SELENIUM_VNC_PASSWORD="secret"

# Proxy
PROXY_SERVER="http://10.20.11.161:3128"

# Default Node
DEFAULT_NODE_ID="1HG231000110"

# Output Directories (relative to WORKSPACE)
SCREENSHOT_DIR="screenshots/opensync-noc"
RESULTS_DIR="results/opensync-noc"

# Feature Toggles
ENABLE_TOPOLOGY_VIEW=true
ENABLE_FIRMWARE_CHECK=true
ENABLE_SCREENSHOTS=true
ENABLE_VNC_PREVIEW=true

# Timeouts
PAGE_LOAD_TIMEOUT=30
ELEMENT_WAIT_TIMEOUT=30
AFTER_LOGIN_WAIT=8
AFTER_SEARCH_WAIT=5
BROWSER_KEEP_ALIVE=20
```

---

## 🔧 Workflow Steps

The automation follows a 9-step workflow:

1. **Connect to Selenium Grid** - Connect to remote browser
2. **Open NOC URL** - Navigate to OpenSync NOC
3. **Accept Terms** - Click "I agree" if prompted
4. **Okta Login** - Click "LOGIN with OKTA"
5. **Authenticate** - Enter credentials and login
6. **Search Node** - Select NODE, enter ID, and search
7. **View Topology** - Navigate to topology view (optional)
8. **⭐ Nodes and Device** - Navigate to Pods & Nodes page and capture screenshot
9. **Extract Firmware** - Extract and save firmware version (optional)

---

## 📸 Screenshot Highlights

### ⭐⭐⭐ Key Screenshot: `07_nodes_and_device.png`

This is the **primary screenshot** requested by the user. It captures the Nodes and Device page after clicking the Pods & Nodes menu. The screenshot is marked with stars (⭐⭐⭐) in the output summary.

All screenshots are timestamped and saved to `screenshots/opensync-noc/` relative to the project workspace.

---

## 🎯 Firmware Extraction Methods

The skill uses multiple methods to extract firmware version:

1. **Firmware Version div**: Search for divs containing "Firmware Version" text
2. **Full text search**: Regex patterns for version strings like:
   - `12.2.6.25-WF710X-T26011408`
   - `12.2.6.25-WF710X-dev-T26011408`

Output saved to: `results/opensync-noc/firmware_version_<NODE_ID>.txt`

Example:
```
节点ID / Node ID: 1HG231000110
固件版本 / Firmware Version: 12.2.6.25-WF710X-dev-T26011408
提取时间 / Extract Time: 2025-01-14 22:30:45
提取方法 / Method: Full text search
```

---

## 🌐 VNC Live Viewing

While the automation runs, you can watch the browser in real-time:

1. Open browser: http://localhost:7900
2. Enter password: `secret`
3. Watch Chrome automation live

Browser stays alive for 20 seconds (configurable) after completion for viewing.

---

## 🛠️ Troubleshooting

### Selenium Grid not running:
```bash
docker ps | grep selenium
# If not running:
docker run -d -p 4444:4444 -p 7900:7900 \
  --name selenium selenium/standalone-chrome:latest
```

### Python dependencies missing:
```bash
pip3 install selenium
```

### Connection errors:
- Check proxy settings in `noc_config.conf`
- Verify OpenSync NOC URL is accessible
- Check Okta credentials

### Screenshot not captured:
- Enable screenshots: `ENABLE_SCREENSHOTS=true`
- Check directory permissions
- Verify WORKSPACE path detection

---

## 📋 Example Output

```
========================================
OpenSync NOC 自动化脚本
OpenSync NOC Automation Script
========================================
工作区 / Workspace: /home/hughcheng/Project/WF710G_260109
节点 ID / Node ID: 1HG231000110
截图目录 / Screenshots: /home/hughcheng/Project/WF710G_260109/screenshots/opensync-noc
========================================

[步骤 1/9] 连接 Selenium Grid
✓ 连接成功 / Connected successfully

[步骤 2/9] 打开 https://opensync.noc.plume.com/
✓ 初始页面 已截图: 01_initial.png

...

[步骤 8/9] 点击 Nodes and Device 并截图
>>> 点击 Pods & Nodes 菜单 / Pods & Nodes menu
✓ ⭐ Nodes and Device 页面 / Nodes and Device page 已截图: 07_nodes_and_device.png
✅ 重点截图已保存 / Key screenshot saved: .../07_nodes_and_device.png

[步骤 9/9] 提取节点 1HG231000110 的固件版本
✓ 成功提取固件版本 / Successfully extracted firmware version:
  节点ID / Node ID: 1HG231000110
  固件版本 / Firmware: 12.2.6.25-WF710X-dev-T26011408
  提取方法 / Method: Full text search

========================================
✅ 完成！ / Completed!
========================================

生成的文件 / Generated Files:

  截图 / Screenshots:
    01_initial.png
    02_logged_in.png
    03_node_selected.png
    04_search_input.png
    05_node_page.png
    06_topology.png
    07_nodes_and_device.png ⭐⭐⭐
    08_final.png

  数据 / Data:
    firmware_version_1HG231000110.txt

浏览器保持 20 秒... / Browser will stay for 20 seconds...
VNC 预览 / VNC Preview: http://localhost:7900 (密码 / Password: secret)
```

---

## 🔗 Related Skills

- **ssh-connection**: Manage SSH connections to DUT devices
- **universal-build**: Build OpenSync firmware
- **firmware-upgrade**: Upgrade DUT firmware
- **build-and-upgrade**: Complete build and upgrade workflow

---

## 📄 Files

- `universal_opensync_noc.sh` - Bash wrapper script
- `universal_opensync_noc.py` - Python automation script
- `noc_config.conf` - Configuration file
- `SKILL.md` - This documentation

---

## 🌟 Highlights

- ⭐ **Portable**: Uses WORKSPACE auto-detection for any machine
- 🎯 **Configurable**: All settings in `noc_config.conf`
- 🔒 **Secure**: Credentials stored in config (can be externalized)
- 📸 **Visual**: Screenshots at every key step
- 🎬 **Observable**: VNC preview for live viewing
- 🐛 **Debuggable**: Comprehensive logging and error handling

---

**Created**: 2025-01-14  
**Version**: 1.0  
**Author**: Hugh Cheng (chenghongliang@cigtech.com)
