---
name: config-update
description: Update variables in skills_config.conf for any project (WF728, WF710G, WF810, WF710, WF630). Use when changing workspace path, DUT IP, SSH credentials, jumphost settings, build server, firmware config, or any project variable. Triggers on "update config", "change workspace", "set DUT IP", "modify skills_config", "update jumphost", "change password", "set project variable".
---

# Config Update Skill

Update variables in `.github/skills/skills_config.conf` without manual editing.

## Script

[`config_update.sh`](./config_update.sh) — located in this directory.

## Commands

### Update a variable
```bash
.github/skills/config-update/config_update.sh <VAR_NAME> <NEW_VALUE>
```

### Show all variables for a project
```bash
.github/skills/config-update/config_update.sh --show <PROJECT>
# PROJECT: WF728, WF710G, WF810, WF710, WF630
```

### List all variable names
```bash
.github/skills/config-update/config_update.sh --list
```

## Common Variables

### Workspace / Paths
| Variable | Description |
|---|---|
| `WF728_WORKSPACE` | WF728 project root path |
| `WF710G_WORKSPACE` | WF710G project root path |
| `DEFAULT_PROJECT` | Default project when none specified |
| `CURRENT_PROJECT` | Current active project |

### DUT SSH
| Variable | Description |
|---|---|
| `WF728_DUT_SSH_HOST` | DUT IP address |
| `WF728_DUT_SSH_USER` | DUT SSH username |
| `WF728_DUT_SSH_PASSWORD` | DUT SSH password |

### Jumphost SSH
| Variable | Description |
|---|---|
| `WF728_JUMPHOST_SSH_HOST` | Jumphost IP address |
| `WF728_JUMPHOST_SSH_USER` | Jumphost username |
| `WF728_JUMPHOST_SSH_PASSWORD` | Jumphost password |
| `WF728_JUMPHOST_DUT_IP` | DUT IP as seen from jumphost |
| `WF728_JUMPHOST_DUT_INTERFACE` | Network interface on jumphost |

### Build Server
| Variable | Description |
|---|---|
| `WF728_BUILDSERVER_HOST` | Build server IP |
| `WF728_BUILDSERVER_USER` | Build server username |
| `WF728_BUILDSERVER_PASSWORD` | Build server password |

> Replace `WF728_` prefix with `WF710G_`, `WF810_`, `WF710_`, or `WF630_` for other projects.

## Examples

```bash
# Change WF728 workspace path
.github/skills/config-update/config_update.sh WF728_WORKSPACE /home/user/Project/WF728_260226

# Change DUT IP
.github/skills/config-update/config_update.sh WF728_DUT_SSH_HOST 192.168.1.1

# Switch default project
.github/skills/config-update/config_update.sh DEFAULT_PROJECT WF728

# Update jumphost IP
.github/skills/config-update/config_update.sh WF728_JUMPHOST_SSH_HOST 172.16.10.80

# Show all WF728 settings
.github/skills/config-update/config_update.sh --show WF728
```

## Notes

- The script automatically preserves quoting style (quoted vs unquoted values)
- `WORKSPACE` changes automatically cascade to `BUILDSERVER_BASE_PATH` via variable references (`${VAR}` syntax in the config)
- Always run from the prplos project root
