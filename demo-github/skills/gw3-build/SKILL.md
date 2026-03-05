---
name: gw3-build
description: Build Gateway 3 with intelligent configuration and build management
---

# Gateway 3 Build Skill

This skill provides comprehensive instructions for building Gateway 3 (prplOS) firmware with intelligent configuration management.

## Overview

Gateway 3 is based on prplOS, a custom OpenWrt distribution for embedded networking devices. The build system uses:
- **Profile-based configuration**: YAML files in `profiles/` directory
- **Multi-layer profiles**: Target hardware + feature sets (prpl, security, f-secure, etc.)
- **Custom package feeds**: Private repositories for AMX, prplMesh, and prplOS-specific packages
- **OpenWrt build system**: Standard make-based workflow with custom extensions

## Build Configuration Process

**IMPORTANT RULE:** Always run build commands from the root directory of the project.

### Configuration Process

The `scripts/gen_config.py` script is the primary tool for configuring builds.
1. Loads multiple YAML profile files and merges them
2. Configures external package feeds
3. Generates `.config` file with target, packages, and options
4. Runs `make defconfig` to validate configuration

**Usage:**
```bash
./scripts/gen_config.py <target_profile> [feature_profile1] [feature_profile2] ...
```

**Common commands:**
```bash
# List available profiles
./scripts/gen_config.py list

# Clean feeds and configuration
./scripts/gen_config.py clean

# Configure for specific target with features
./scripts/gen_config.py qca_ipq54xx prpl security f-secure
```

#### Profile Types

1. **Target Profiles** (Hardware platforms):
   - `qca_ipq54xx.yml` - Qualcomm IPQ54xx 

2. **Feature Profiles** (Software features):
   - `prpl.yml` - Core prpl features (includes prpl_core, lcm, mgmt)
   - `prpl_core.yml` - Base prpl functionality
   - `security.yml` - Compiler security hardening
   - `f-secure.yml` - F-Secure integration
   - `lcm.yml` - Lifecycle Management (containers)
   - `mgmt.yml` - Device management protocols
   - `cellular.yml` - Cellular modem support
   - `thread.yml` - Thread networking
   - `debug.yml` - Debug tools and symbols

3. **Customer/Product Profiles**:
   - `aei_generic.yml` - Generic Actiontec configuration
   - `aei-dvt-mfg.yml` - DVT/Manufacturing builds
   - Custom customer profiles

】
## Build Workflow

### Step 1: Setup Mirror (Optional)

**REQUIRED**: The download mirror may be configured before building to ensure consistent and reliable builds.

Verify `./dl` folder exists and contains files:
```bash
ls -lh ./dl | head -10
du -sh ./dl
```

**Setup mirror (REQUIRED):**
```bash
# Download the mirror from the build server
ncftpget -R dlmirror-sc.optimdev.io . bitbucket/Actiontec/dl-mirror

# Move to the proper location
mv ./dl-mirror ./dl
```

**Verify mirror setup:**
```bash
ls ./dl | wc -l  # Should show many files
du -sh ./dl      # Should show substantial size
```

### Step 2: Install Prerequisites

**Required system packages:**
```bash
sudo apt-get update
sudo apt-get install build-essential gcc g++ binutils patch bzip2 flex \
    make gettext unzip libc6-dev libncurses5-dev libz-dev libssl-dev \
    subversion git wget rsync python3 python3-distutils python3-minimal \
    coccinelle gawk file
```

**Key dependencies:**
- `libssl-dev` - Required for U-boot tools (mkimage, dumpimage)
- `coccinelle` - Required for kernel mac80211 compilation (spatch tool)

### Step 3: Configure Build

Checks if `.config` exists - skips configuration generation if found to avoid unnecessary reconfiguration.

**For new configuration**, select a platform:
- **WF-728A** or **WF-728N** (both are Qualcomm IPQ54xx, running the same image):
  ```bash
  ./scripts/gen_config.py qca_ipq54xx prpl aei_generic security f-secure
  ```
- **WF-710G2** (not available yet)

**Manual configuration commands** (see Configuration section above for details).

### Step 4: Update and Install Feeds

The `gen_config.py` script automatically handles feeds, but for manual management:

```bash
# Update feed definitions
./scripts/feeds update -a

# Install all packages from feeds
./scripts/feeds install -a

# Install specific package
./scripts/feeds install <package_name>

# Install from specific feed
./scripts/feeds install -p <feed_name> <package_name>
```

### Step 5: Build Firmware

**Full build:**
```bash
make -j$(nproc) V=s
```

**Build options:**
- `-j$(nproc)` - Parallel build using all CPU cores
- `V=s` - Verbose output (REQUIRED for troubleshooting)
- `V=sc` - Verbose with command line display

**Single package build:**
```bash
# Build specific package
make package/<package_name>/compile V=s

# Clean and rebuild package
make package/<package_name>/clean
make package/<package_name>/compile V=s
```

**Kernel build:**
```bash
make target/linux/compile V=s
```

### Step 6: Collect Build Artifacts

**Firmware images location:**
```
bin/targets/<target>/<subtarget>/
```

**Example for qca_ipq54xx:**
```
bin/targets/ipq54xx/generic/
├── openwrt-ipq54xx-generic-prpl_freedom-squashfs-nand-factory.bin
├── openwrt-ipq54xx-generic-prpl_freedom-squashfs-nand-sysupgrade.bin
├── packages/
└── sha256sums
```

## Advanced Build Operations

### Clean Operations

```bash
# Clean everything (complete rebuild)
make clean

# Clean configuration and feeds
./scripts/gen_config.py clean
rm -rf tmp/ .config

# Clean specific package
make package/<package_name>/clean

# Clean toolchain (rarely needed)
make toolchain/clean

# Clean target
make target/linux/clean
```

### Build Logging

**Always log builds for troubleshooting:**
```bash
# Create log directory if it doesn't exist
mkdir -p local/records

# Log full build
make -j$(nproc) V=s 2>&1 | tee local/records/build_$(date +%Y%m%d_%H%M%S).log

# Log specific package
make package/<package>/compile V=s 2>&1 | tee local/records/build_<package>_$(date +%Y%m%d_%H%M%S).log
```

### Incremental Builds

After initial build, subsequent builds are faster:
```bash
# Rebuild after source changes
make -j$(nproc) V=s

# Force rebuild of changed packages
make package/*/compile -j$(nproc) V=s
```

## Common Build Issues and Solutions

### 1. Package Build Failures

**Empty package names (__.ipk):**
- **Cause**: File timestamp issues
- **Solution**: Clean and rebuild the specific package
```bash
make package/<package_name>/clean
make package/<package_name>/compile V=s
```

### 2. Feed Configuration Issues

**Feed not found or authentication failed:**
- **Cause**: Missing SSH keys or incorrect feed URLs
- **Solution**: Check SSH keys for git access to private repositories
```bash
ssh -T git@bitbucket.org
ssh -T git@gitlab.com
```

### 3. Missing Dependencies

**libssl-dev errors in U-boot:**
```bash
sudo apt-get install libssl-dev
make target/linux/clean
make target/linux/compile V=s
```

**coccinelle (spatch) not found:**
```bash
sudo apt-get install coccinelle
make target/linux/clean
make target/linux/compile V=s
```

### 4. Configuration Errors

**Target not found:**
- Ensure target profile is loaded correctly
- Verify external_target feeds are installed

**Package conflicts:**
- Check for duplicate packages in different feeds
- Use `additional_packages` to specify feed priority

### 5. Build System Errors

**Case-sensitive filesystem required:**
- WSL users: Ensure building on WSL filesystem, not Windows mounts (`/mnt/c/`)
- The build system automatically filters Windows paths from PATH

**Umask issues:**
```bash
umask 022
make -j$(nproc) V=s
```

**Build hangs or freezes:**
- Reduce parallel jobs: `make -j4` instead of `make -j$(nproc)`
- Check disk space: `df -h`
- Monitor memory: `free -h`

## Workflow Integration

### Continuous Integration Builds

The project uses GitLab CI/CD (`.gitlab-ci.yml`) for automated builds:
- Multiple target configurations
- Automated testing with CRAM tests
- Artifact collection and storage

## Build Performance Tips

1. **Use parallel builds**: `-j$(nproc)` for all CPU cores
2. **Enable ccache**: Speeds up recompilation
   ```bash
   CONFIG_CCACHE=y
   ```
3. **Build on SSD**: Much faster than HDD
4. **Sufficient RAM**: 8GB minimum, 16GB+ recommended
5. **Clean tmp/ periodically**: Prevents stale artifacts


### Development Workflow

1. **Initial setup**:
   ```bash
   ./scripts/gen_config.py qca_ipq54xx prpl security debug
   make -j$(nproc) V=s 2>&1 | tee local/records/initial_build.log
   ```

2. **Modify package source** in appropriate directory:
   - Custom packages: `package/`
   - Feed packages: `feeds/<feed_name>/`

3. **Rebuild modified package**:
   ```bash
   make package/<package_name>/clean
   make package/<package_name>/compile V=s
   ```

4. **Test on device**:
   ```bash
   scp bin/targets/.../openwrt-*single-aei.img root@device:/tmp/
   ssh root@device 'sysupgrade /tmp/openwrt-*single-aei.img'
   ```

