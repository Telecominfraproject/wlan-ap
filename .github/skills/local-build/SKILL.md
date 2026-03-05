---
name: local-build
description: Build OpenWiFi firmware locally using the openwrt/ build tree
skill_version: "1.0.0"
---

# Local Build Skill

Build OpenWiFi AP NOS firmware locally using the `openwrt/` build tree
in the current repository. No SSH or network access required.

## Usage

```
/local-build <profile>
/local-build <profile> [--openwrt-dir <path>]
```

| Argument | Required | Default | Description |
|----------|----------|---------|-------------|
| `profile` | Yes | — | Profile name matching `profiles/<profile>.yml` |
| `--openwrt-dir` | No | `openwrt/` | Path to the openwrt build directory |

### Examples

```
/local-build cig_wf188n
/local-build edgecore_eap104
/local-build cig_wf186h --openwrt-dir /opt/openwrt
```

## How It Works

The skill delegates to `.github/skills/local-build/local-build.sh`:

1. **Validate openwrt dir**: Check `<openwrt-dir>` exists. If not, print setup
   instructions and exit 1.
2. **Validate profile**: Check `profiles/<profile>.yml` exists. Print available
   profiles and exit 2 if not found.
3. **Configure**: `cd <openwrt-dir> && ./scripts/gen_config.py <profile>`
   Exit 3 on failure.
4. **Build**: `make -j$(nproc) 2>&1 | tee /tmp/build-<profile>.log`
   Exit 4 on failure.
5. **Report**:
   - Success (exit 0): list image paths from `<openwrt-dir>/bin/`.
   - Failure (exit ≠ 0): print last 50 lines of build log.

## Output

### Success

```
Build succeeded: cig_wf188n
Image: openwrt/bin/targets/ipq40xx/generic/openwifi-cig_wf188n-sysupgrade.bin
```

### Failure

```
Build failed: cig_wf188n (exit code 2)
Last 50 lines of build log:
...
```

### openwrt/ not found

```
ERROR: openwrt/ directory not found at /path/to/openwrt.
Run './setup.py --setup' first to initialize the build tree.
See README.md §Building for details.
```

### Profile not found

```
ERROR: Profile 'unknown_profile' not found in profiles/
Available profiles:
  asterfusion_ap7330
  cig_wf186h
  ...
```

## Exit Codes

| Code | Meaning |
|------|---------|
| 0 | Build succeeded |
| 1 | openwrt/ directory missing |
| 2 | Profile validation error |
| 3 | gen_config.py failure |
| 4 | Build (make) failure |

## Prerequisites

- `openwrt/` build tree initialized (run `./setup.py --setup` first)
- Profile file `profiles/<profile>.yml` present in this repository
- Build dependencies installed (OpenWrt build system requirements)

## Related Skills

- `/server-build` — Build firmware remotely on the shared build server
- `/upgrade-dut` — Flash built firmware to a DUT
