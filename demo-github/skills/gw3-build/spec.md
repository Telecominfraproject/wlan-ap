# gw3-build Specification

| Field | Value |
|-------|-------|
| Status | Active |
| Version | 1.0.0 |
| Created | 2026-02-20 |
| Depends | python3, make, gcc toolchain, git, system build packages |

## 1. Overview

The `gw3-build` skill provides comprehensive guidance for configuring and building
Gateway 3 (prplOS) firmware. It is a prompt-only skill (no scripts) that instructs AI
assistants on the profile-based configuration system, multi-stage build workflow,
clean operations, troubleshooting patterns, and deployment procedures.

**Users:** Developers and AI assistants building firmware images or individual packages.

**When to use:**
- Configuring a build for a specific hardware target and feature set
- Running full or incremental firmware builds
- Building, cleaning, or rebuilding individual packages
- Diagnosing and resolving build failures
- Deploying firmware or packages to a DUT

## 2. Architecture

### 2.1 Build System Layers

```
Profile YAML files (profiles/*.yml)
  -> scripts/gen_config.py merges profiles -> .config + feeds.conf
    -> tools/ (75+ host build tools)
      -> toolchain/ (cross-compiler: GCC, binutils, musl/glibc)
        -> target/linux/ (kernel + patches per architecture)
          -> package/ (543 Makefiles, IPK packaging)
            -> bin/targets/ (firmware images)
```

### 2.2 Profile System

Profiles are YAML files in `profiles/` that layer together through `gen_config.py`.
Each profile can specify: `target`, `subtarget`, `feeds`, `packages`, `diffconfig`,
`include`, `packages_remove`.

| Profile Type | Examples | Purpose |
|-------------|----------|---------|
| Target | `qca_ipq54xx.yml`, `qca_ipq95xx.yml`, `x86_64.yml` | Define hardware platform |
| Feature | `prpl.yml`, `security.yml`, `f-secure.yml`, `lcm.yml`, `debug.yml` | Add software capabilities |
| Customer | `aei_generic.yml`, `aei-dvt-mfg.yml` | Vendor customizations |

### 2.3 Key Directories

| Directory | Purpose |
|-----------|---------|
| `profiles/` | YAML profile definitions |
| `feeds/` | Cloned package feed repos (symlinks) |
| `package/` | OpenWrt base package definitions |
| `build_dir/` | Extracted/compiled sources (read-only snapshots) |
| `staging_dir/` | Cross-compilation sysroot |
| `bin/targets/` | Output firmware images and packages |
| `dl/` | Downloaded source tarballs (mirror) |
| `tmp/` | Temporary build artifacts |

### 2.4 Build Output

Firmware images are placed in `bin/targets/<target>/<subtarget>/`:

```
bin/targets/ipq54xx/generic/
  openwrt-ipq54xx-generic-prpl_freedom-squashfs-nand-factory.bin
  openwrt-ipq54xx-generic-prpl_freedom-squashfs-nand-sysupgrade.bin
  packages/
  sha256sums
```

## 3. Interface / Subcommands

This is a prompt-only skill -- it has no executable scripts. The AI assistant uses the
knowledge below to execute build commands directly.

### 3.1 Configuration Commands

| Command | Purpose |
|---------|---------|
| `./scripts/gen_config.py <target> [features...]` | Generate `.config` and `feeds.conf` from profiles |
| `./scripts/gen_config.py list` | List available profiles |
| `./scripts/gen_config.py clean` | Clean feeds and configuration |
| `make menuconfig` | Interactive kernel/package configuration |
| `make defconfig` | Validate and expand `.config` with defaults |

### 3.2 Feed Management

| Command | Purpose |
|---------|---------|
| `./scripts/feeds update -a` | Update all feed definitions |
| `./scripts/feeds install -a` | Install all packages from feeds |
| `./scripts/feeds install <pkg>` | Install specific package |
| `./scripts/feeds install -p <feed> <pkg>` | Install from specific feed |

### 3.3 Build Commands

| Command | Purpose |
|---------|---------|
| `make -j$(nproc) V=s` | Full parallel build (verbose) |
| `make package/<pkg>/compile V=s` | Build single package |
| `make package/<pkg>/clean` | Clean single package |
| `make target/linux/compile V=s` | Build kernel |

### 3.4 Clean Commands (increasing aggressiveness)

| Command | Removes |
|---------|---------|
| `make package/<pkg>/clean` | Single package build artifacts |
| `make clean` | All build artifacts |
| `make targetclean` | Build artifacts + toolchain directory |
| `make dirclean` | All above + host tools, tmp |
| `./scripts/gen_config.py clean` | Feeds + configuration files |

## 4. Logic / Workflow

### 4.1 Initial Build (Full Workflow)

1. **Install prerequisites:**
   ```
   sudo apt-get install build-essential gcc g++ binutils patch bzip2 flex
       make gettext unzip libc6-dev libncurses5-dev libz-dev libssl-dev
       subversion git wget rsync python3 python3-distutils python3-minimal
       coccinelle gawk file
   ```

2. **Set up download mirror (optional but recommended):**
   ```
   ncftpget -R dlmirror-sc.optimdev.io . bitbucket/Actiontec/dl-mirror
   mv ./dl-mirror ./dl
   ```

3. **Configure build:**
   ```
   ./scripts/gen_config.py qca_ipq54xx prpl aei_generic security f-secure
   ```

4. **Build firmware:**
   ```
   mkdir -p local/records
   make -j$(nproc) V=s 2>&1 | tee local/records/build_$(date +%Y%m%d_%H%M%S).log
   ```

5. **Collect artifacts** from `bin/targets/<target>/<subtarget>/`

### 4.2 Single Package Rebuild

1. Clean the package: `make package/<pkg>/clean`
2. Compile: `make package/<pkg>/compile V=s`
3. Find IPK in `bin/packages/` or `bin/targets/.../packages/`

### 4.3 Incremental Build After Source Changes

1. If source is in `aei/src/<pkg>/` (module checkout):
   - `make package/<pkg>/clean && make package/<pkg>/compile V=s`
2. If source is in `aei/<feed>/<pkg>/` (feed checkout):
   - `make package/<pkg>/compile V=s` (auto-detected via symlink)
3. For kernel changes:
   - `make target/linux/compile V=s`

### 4.4 Configuration Change Workflow

1. Run `gen_config.py` with desired profiles
2. Run `./scripts/feeds update -a && ./scripts/feeds install -a`
3. If using `gw3-src-local`, re-run `src_local.sh` to restore src-links
4. Build: `make -j$(nproc) V=s`

### 4.5 Deployment to DUT

```bash
# Full firmware upgrade
scp bin/targets/.../openwrt-*-sysupgrade.bin root@192.168.1.1:/tmp/
ssh root@192.168.1.1 'sysupgrade /tmp/openwrt-*-sysupgrade.bin'

# Single package install
scp bin/packages/.../package.ipk root@192.168.1.1:/tmp/
ssh root@192.168.1.1 'opkg install --force-reinstall /tmp/package.ipk'
```

## 5. Safety Rules

### 5.1 Build Environment

- **Always build from project root.** All `make` and `gen_config.py` commands must run
  from the prplos repository root directory.
- **Always use `V=s` (verbose).** Required for troubleshooting build failures.
- **Always log builds.** Direct output to `local/records/build_*.log` via `tee`.
- **WSL users must build on WSL filesystem**, never on Windows mounts (`/mnt/c/`).
  Case-sensitive filesystem is required.

### 5.2 Directory Rules

- **Never modify `build_dir/`.** It contains ephemeral build snapshots overwritten by
  `make package/<pkg>/clean`. Edit source in `aei/src/<pkg>/` or `aei/<feed>/<pkg>/`.
- **`local/` is gitignored.** Never stage, commit, or push files from `local/`.
- **`dl/` is the download mirror.** Do not delete unless rebuilding from scratch.

### 5.3 Build Flags

| Flag | Required | Purpose |
|------|----------|---------|
| `V=s` | Yes | Verbose output for all builds |
| `-j$(nproc)` | Recommended | Parallel compilation |
| `V=sc` | Optional | Verbose + command line display |

### 5.4 Configuration Guards

- Check for existing `.config` before running `gen_config.py` to avoid overwriting
  manual customizations
- After `gen_config.py`, re-run `src_local.sh` if any feeds were src-linked

## 6. Dependencies

### 6.1 System Packages

| Package | Purpose |
|---------|---------|
| `build-essential`, `gcc`, `g++`, `binutils` | C/C++ compilation |
| `libssl-dev` | U-boot tools (mkimage, dumpimage) |
| `coccinelle` | Kernel mac80211 compilation (spatch) |
| `libncurses5-dev` | menuconfig UI |
| `python3` | gen_config.py and build scripts |
| `git`, `subversion` | Source control for feeds |

### 6.2 Project Dependencies

| Component | Purpose |
|-----------|---------|
| `scripts/gen_config.py` | Profile-to-config generator |
| `scripts/feeds` | Feed management |
| `profiles/*.yml` | Build profile definitions |
| `include/package.mk` | Package build macros |
| SSH keys for Bitbucket/GitLab | Access to private feed repos |

## 7. Troubleshooting Patterns

| Symptom | Cause | Solution |
|---------|-------|----------|
| Empty package name (`__.ipk`) | File timestamp issues | Clean and rebuild the package |
| Feed auth failure | Missing SSH keys | Verify `ssh -T git@bitbucket.org` |
| U-boot mkimage error | Missing libssl-dev | `apt-get install libssl-dev` |
| spatch not found | Missing coccinelle | `apt-get install coccinelle` |
| Build hangs/freezes | Resource exhaustion | Reduce `-j` count, check `df -h` and `free -h` |
| Umask issues | Wrong file permissions | Run `umask 022` before building |
| Target not found | Missing external_target feed | Verify feeds are installed |
| Package conflicts | Duplicate packages in feeds | Use `additional_packages` for feed priority |

## 8. Future Work

- Automated build artifact verification (checksum validation)
- Integration with CI pipeline status reporting
- ccache configuration guidance and performance metrics
- Build time profiling and optimization recommendations
