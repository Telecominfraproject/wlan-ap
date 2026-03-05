# Contract: local-build

**Skill**: `local-build`  
**Version**: 1.0.0  
**Phase**: 1 — Design  
**Date**: 2026-03-05

## Invocation

```
/local-build <profile>
/local-build <profile> [--openwrt-dir <path>]
```

| Argument | Required | Default | Description |
|----------|----------|---------|-------------|
| `profile` | Yes | — | Profile name matching `profiles/<profile>.yml` |
| `--openwrt-dir` | No | `openwrt/` | Path to openwrt build directory |

## Workflow Steps

1. **Validate openwrt dir**: Check `<openwrt-dir>` exists. If not, print error with setup instructions and exit 1.
2. **Validate profile**: Check `profiles/<profile>.yml` exists. If not, print available profiles and exit 1.
3. **Configure**: `cd <openwrt-dir> && ./scripts/gen_config.py <profile>`
4. **Build**: `make -j$(nproc) 2>&1 | tee /tmp/build-<profile>.log`
5. **Report**:
   - Success (exit 0): scan `<openwrt-dir>/bin/` for image files, print paths.
   - Failure (exit ≠ 0): print last 50 lines of log and exit 1.

## Output Contract

### Success

```
Build succeeded: <profile>
Image: openwrt/bin/targets/<target>/<subtarget>/<image-filename>
```

### Failure

```
Build failed: <profile> (exit code <N>)
Last 50 lines of build log:
<log output>
```

### openwrt/ not found

```
ERROR: openwrt/ directory not found at <path>.
Run './setup.py --setup' first to initialize the build tree.
See README.md §Building for details.
```

### Profile not found

```
ERROR: Profile '<profile>' not found in profiles/
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
