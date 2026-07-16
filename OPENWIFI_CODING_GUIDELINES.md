# OpenWiFi / OpenLAN — Coding Guidelines

Coding standards for the OpenWiFi/OpenLAN repositories:

- **`wlan-ap`** — AP firmware, targets, packages, patches
- **`wlan-ucentral-schema`** — the uCentral config/state schema and renderer (`.uc` / YAML)
- **`wlan-ucentral-client`** — the on-device uCentral client (C)

For the PR lifecycle (branching, submitting, integrating), see
[OPENWIFI_PR_GUIDELINES.md](OPENWIFI_PR_GUIDELINES.md). This document covers **code
formatting, commit messages, and per-language conventions**.

---

## 1. Git history

The history is **linear**. Keep it that way:

- **No merge bubbles for feature work.** Submit clean, rebasable commits — they land via
  rebase / cherry-pick / `git am`, not merge. Merge commits are reserved for the release
  train (version/release branches only).
- **One logical change per commit.** Don't bundle unrelated fixes; commits are applied
  individually and must each leave the tree buildable.
- **Authorship and `Signed-off-by` are preserved** when a patch is applied — the author
  stays the contributor, the integrator becomes the committer.
- **Submodule/feed bumps are explicit commits**, never silent. When `wlan-ucentral-schema`
  (a submodule of `wlan-ap`) moves, it lands as its own commit that lists the included
  upstream commits (see §2).

---

## 2. Commit message conventions

Every commit follows the OpenWrt/kernel trailer style:

```
<subsystem>: <imperative summary — first word lower-case; proper nouns, board names, and identifiers keep their case; no trailing period>

<Required body, wrapped at ~72 cols: explain why the change is needed, then what
it does. Use "-" bullets for multi-part fixes. Record test evidence as plain text
("Tested on EAP105 ...") for functional/board/driver changes.>

Fixes: WIFI-15468
Signed-off-by: Your Name <email>
```

### Subject line

- **`subsystem: description`** — the prefix names the package, target, or component and is
  **mandatory**. Examples in use: `ucentral-schema:`, `ipq807x:`, `ucentral:`, `hostapd:`,
  `patches-25.12:`, `ucentral-client:`, `uspot:`, `netifd:`, `mac80211:`, `profiles:`,
  `qca-wifi-7:`, `config.yml:`.
- **First word after the colon is lower-case and imperative** (`fix`, `add`, `remove`,
  `update`, `disable`); **preserve the case of proper nouns, board names, and identifiers**
  (e.g. `SonicFi RAP630E`, `OpenWrt`, `RTGS`); **no trailing period**.
- **Patch commits carry the OpenWrt version** in the prefix: `patches-25.12: …`. Bump the
  number when you renumber/rebase patches.
- Target/driver prefixes can stack: `qca-wifi-7: wifi-scripts: fix …`,
  `qca-wifi-7/wifi-scripts: …`.

Examples:
```
netifd: fix bridge reconfiguration when interfaces move between bridges
wifi-scripts: remove stale bridge/VLAN membership on wireless reconfig
hostapd: add CUI to ACL RADIUS Access-Request
config.yml: update OpenWrt baseline to v25.12.3
```

### Body

A body is **required** on every commit. Write it as plain text, not labelled
sections:

- **Explain why, then what.** Lead with the reason the change is needed (the bug, gap, or
  motivation the diff can't show), then what the commit does. Imperative mood, wrapped at
  ~72 columns; use `-` bullets for multi-part fixes.
- **Record test evidence as plain text** for functional, board, or driver changes — e.g.
  "Tested on EAP105 and EAP014, verified FT roam with no DHCP drop." This is required for
  the changes B1 (Hardware risk) calls out; mechanical commits (bumps, renames, reverts)
  need none. Optionally use a `Tested-by:` trailer when someone else verified the change.

Trailers follow the body:

- **`Fixes: <TICKET>`** (`WIFI-#####`, `WLAN-####`) **where the commit addresses a tracked
  issue.** Bug fixes reference a ticket; refactors, additions, and bumps often have none.
- **`Signed-off-by:` is required on every commit** (DCO). Preserve upstream sign-offs when
  forwarding a patch; add your own.

### Special commit types

- **Reverts** use the standard `git revert` format plus a reason:
  ```
  Revert "qca-wifi-7: ipq53xx: rtk_phy: Fix 10G link mode for RTL8261N"

  This reverts commit 5b73a776...
  This commit is causing a regression on boards like WF198
  ```
- **Submodule / feed bumps** name the moved component and **list the included upstream
  commits** so the diff is self-documenting:
  ```
  ucentral-schema: update to latest HEAD

  96e4e5e renderer: skip 160MHz fallback on 5G when DFS is disabled
  382515a renderer: wait for netifd wireless to settle before promoting config
  561628e network: aggregate network ports from multiple sources

  Signed-off-by: ...
  ```

---

## 3. File headers (SPDX)

Every **new** source file begins with an SPDX license identifier, written in that file's own
comment syntax; for files with a shebang, the identifier goes **immediately after the
shebang line**. Use the **same license as the rest of the package** — C sources in these
repos use `BSD-3-Clause`. When editing an existing file, match its current header; don't
change a file's license identifier as a drive-by.

Comment syntax per language:

- C / headers: `/* SPDX-License-Identifier: BSD-3-Clause */`
- ucode (`.uc`): `// SPDX-License-Identifier: <package license>` — above `"use strict";`
- Shell / procd init scripts: shebang first, then `# SPDX-License-Identifier: <package license>`
- YAML / UCI config: `# SPDX-License-Identifier: <package license>`

---

## 4. C code style (`wlan-ucentral-client`, packages)

Follow the **OpenWrt / libubox / Linux-kernel** idiom — be consistent with it, not with
generic C conventions.

- **`/* SPDX-License-Identifier: BSD-3-Clause */`** as the first line of every C source and
  header file (see §3 for the cross-language rule).
- **Tabs for indentation.** Never spaces.
- **K&R braces.** For function *definitions*, the return type and name go on the same line,
  but the opening brace is on its own line:
  ```c
  static void
  send_blob(struct blob_buf *blob)
  {
          char *msg;
          int len;
          ...
  }
  ```
- **`static` for everything file-local** — functions and globals.
- **snake_case**, lower-case, no Hungarian notation.
- **libubox/libubus/blobmsg idioms throughout:**
  - Attribute enums end with a `__FOO_MAX` sentinel and pair with a
    `static const struct blobmsg_policy foo_policy[__FOO_MAX]` using designated
    initializers (`[JSONRPC_VER] = { .name = "jsonrpc", .type = BLOBMSG_TYPE_STRING }`).
  - Logging via **`ULOG_ERR` / `ULOG_DBG`**, not `printf`/`fprintf` in production paths.
- **Dead code is `#if 0`'d out**, not deleted, when kept for reference. Keep these blocks rare.
- Declarations at the top of the block; early-return guard style for error paths.

---

## 5. ucode (`.uc`) style (`wlan-ucentral-schema` renderer & state)

- **`"use strict";`** at the top of every module.
- **Tabs for indentation.**
- **`let` for locals**, never bare globals.
- **Modular helpers via `require`/`import`.** Shared logic lives in `libs/` and is imported
  explicitly:
  ```js
  import { ipcalc } from 'libs.ipcalc';
  import { create_wiphy } from 'libs/wiphy.uc';
  import {
          b, s, uci_cmd, uci_set_string, uci_set_boolean, ...
  } from 'libs/uci_helpers.uc';
  ```
- **JSDoc-style doc comments** on non-trivial functions (`@param`, `@memberof`).
- **Defensive coding:** null-guard every external read
  (`let conn = ubus ? ubus.connect() : null;`), and use **`assert()` for invariants**
  (`assert(cursor, "Unable to instantiate uci");`).
- **`??=` for nullish defaults** (`default_config.country ??= 'US';`).
- Wrap risky includes in `try/catch` and `warn()` rather than aborting.

---

## 6. Schema authoring (`wlan-ucentral-schema`)

- **Author in YAML, never hand-edit generated JSON.** The source of truth is the
  `schema/*.yml` files (2-space indent); `ucentral.schema.json`, `*.pretty.json`, and the
  full schema are **generated** via `generate.sh` / `merge-schema.py`. Regenerate and commit
  the output; don't patch it by hand.
- Each property block carries **`description`, `type`, constraints (`maximum`/`minimum`),
  and `examples`** — keep all four. Descriptions are full prose sentences.
- The schema is consumed by both the device (`schemareader.uc`, generated by
  `generate-reader.uc`) and the cloud, so **breaking changes ripple**. Prefer additive
  changes; bump and document when you must break.

---

## 7. Quick checklist

- [ ] One logical change per commit; tree builds at every commit.
- [ ] Subject `subsystem: imperative summary`: first word lower-case, proper nouns/board
      names keep their case, no trailing period.
- [ ] Body present (why, then what); test evidence in plain text for functional/board/driver
      changes; `Fixes: <TICKET>` where applicable.
- [ ] `Signed-off-by:` present on every commit (yours preserved + upstream preserved).
- [ ] New source files carry an SPDX identifier (first line, or immediately after a shebang).
- [ ] C: tabs, `static`, blobmsg policy + `__MAX` sentinel, `ULOG_*` logging.
- [ ] ucode: `"use strict"`, tabs, `let`, helpers imported from `libs/`, null-guards + `assert()`.
- [ ] Schema: edited the `.yml`, regenerated JSON via `generate.sh`, didn't hand-edit JSON.
- [ ] Patch packages prefixed with the OpenWrt version (`patches-25.12: …`).
- [ ] Submodule/feed bumps are explicit commits listing the included upstream commits.
