---
name: universal_build
description: Build OpenSync firmware for WF710G and other models using Docker containers. Handles version management, Docker container lifecycle, and multi-project configurations. Use when building firmware, checking build status, or managing build environments.
---

## How to invoke with parameters

When user invokes `/universal_build` or natural language, extract:
- **PROJECT** — device model (e.g. WF728, WF710G, WF810, WF710)
- **VERSION** — optional firmware version string

Then run:
```bash
cd /home/hughcheng/Project/WF728_260226/prplos/aei_ai_skills/universal-build
bash universal_build_skill.sh build <PROJECT> [VERSION]
```

Examples:
- `/universal_build WF728` → `bash universal_build_skill.sh build WF728`
- `/universal_build WF728 0.9.0-p.19` → `bash universal_build_skill.sh build WF728 0.9.0-p.19`
- `/universal_build WF710G 12.2.6.25-WF710X-T26011002` → `bash universal_build_skill.sh build WF710G 12.2.6.25-WF710X-T26011002`
- `/universal_build status WF728` → `bash universal_build_skill.sh status WF728`


# OpenSync Firmware Build Skill

This skill helps you build OpenSync firmware for various router models (WF710G, WF810, WF710) using Docker-based build environments.

## Capabilities

- **Build firmware** for multiple router models with version control
- **Check build status** of running Docker containers
- **Clean build directories** to start fresh builds
- **Show configuration** for current build environment
- **Multi-project support** with customizable configurations

## When to Use This Skill

Use this skill when you need to:
- Build OpenSync firmware for WF710G or other supported models
- Update firmware version numbers before building
- Check if a firmware build is currently running
- Clean up build artifacts or Docker containers
- View or verify build configuration settings

## Build Script Reference

The skill uses the [`universal_build_skill.sh`](./universal_build_skill.sh) script located in this directory.

### Available Commands

#### 1. Build Firmware
```bash
./universal_build_skill.sh build [PROJECT] [VERSION]
```

**Parameters:**
- `PROJECT`: Optional. Project name (e.g., WF710G, WF810, WF710). Defaults to WF710G.
- `VERSION`: Optional. Version string (e.g., 12.2.6.25-WF710X-T2601203). If provided, updates the version file before building.

**Examples:**
```bash
# Build WF710G with default settings
./universal_build_skill.sh build WF710G

# Build WF710G with specific version
./universal_build_skill.sh build WF710G 12.2.6.25-WF710X-T2601203

# Build WF810 model
./universal_build_skill.sh build WF810 1.0.0.1-WF810-T2601203
```

**What It Does:**
1. Loads project configuration from `build_config.env`
2. Updates version file if version is provided
3. Stops and removes old Docker container
4. Starts new Docker container with build image
5. Executes build command inside container
6. Generates timestamped log files
7. Returns JSON status with build results

**Output Files:**
- `{MODEL}_{TIMESTAMP}.log` - Full build output
- `{MODEL}_{TIMESTAMP}.errlog` - Error messages only

#### 2. Check Build Status
```bash
./universal_build_skill.sh status [PROJECT]
```

**Example:**
```bash
./universal_build_skill.sh status WF710G
```

**Returns:** JSON with container name, running status, and Docker status details.

#### 3. Clean Build Directory
```bash
./universal_build_skill.sh clean [PROJECT]
```

**Example:**
```bash
./universal_build_skill.sh clean WF710G
```

**What It Does:**
Removes the OpenSync build directory at:
`WiFi7/sdk.qca.comp/qca/qca-networking/qsdk/build_dir/target-arm/opensync-6.6`

#### 4. Show Configuration
```bash
./universal_build_skill.sh config
```

Displays current configuration including workspace path, default project, and owner.

## Configuration File

The skill uses [`build_config.env`](./build_config.env) for project-specific settings.

### Configuration Format

```bash
# Project Configuration
{PROJECT}_MODEL_NAME="ModelName"
{PROJECT}_CONTAINER_DIR="/src/build/path"
{PROJECT}_MAKE_CMD="make command"
{PROJECT}_DOCKER_IMAGE="image:tag"

# Default project
DEFAULT_PROJECT="WF710G"

# Options
KEEP_CONTAINER=false
```

### Current Supported Projects

- **WF710G** (default)
- **WF810**
- **WF710**

## Workflow Examples

### Building Firmware with Version Update

When a user asks to build firmware:

1. Ask for project name (or use WF710G default)
2. Ask for version string if updating version
3. Run the build command
4. Monitor the log files for results
5. Report build status and log file locations

Example conversation:
```
User: "Build WF710G firmware with version 12.2.6.25-WF710X-T2601203"
Agent: I'll build the firmware now.
       [Execute: ./universal_build_skill.sh build WF710G 12.2.6.25-WF710X-T2601203]
       Build started. Log file: WF710G_260114-153045.log
```

### Checking Build Progress

When a user wants to know if a build is running:

```
User: "Is the WF710G build still running?"
Agent: Let me check the status.
       [Execute: ./universal_build_skill.sh status WF710G]
       The WF710G build container is running. Status: Up 15 minutes
```

### Clean Build

When a user needs to clean the build environment:

```
User: "Clean the build directory for WF710G"
Agent: [Execute: ./universal_build_skill.sh clean WF710G]
       Build directory cleaned successfully.
```

## Important Notes

- **Docker Required**: All builds run in Docker containers
- **Build Time**: Builds can take 30+ minutes depending on the model
- **Log Files**: Always generated with timestamp in workspace root
- **Container Names**: Format is `docker_{owner}_{model}` (e.g., docker_hughcheng_WF710G)
- **Version File**: Located at `WiFi7/opensync_ws/OPENSYNC/{MODEL}/modify/qsdk/version`

## Error Handling

If a build fails:
1. Check the `.errlog` file for error messages
2. Verify Docker image exists and is accessible
3. Ensure workspace paths are correct
4. Check if previous container was properly cleaned up

## Return Format

Build commands return JSON:
```json
{
  "project": "WF710G",
  "model": "WF710G",
  "status": "success",
  "exit_code": 0,
  "log_file": "/home/hughcheng/Project/WF710G_260109/WF710G_260114-153045.log",
  "error_log": "/home/hughcheng/Project/WF710G_260109/WF710G_260114-153045.errlog",
  "timestamp": "260114-153045"
}
```

## Integration with Copilot

This skill automatically activates when you:
- Mention building OpenSync or firmware
- Ask about WF710G, WF810, or WF710 models
- Request version updates or build status
- Need to clean build directories

No manual activation needed - just describe what you want to build!
