---
name: gw3-datamodel-report
description: Generate TR-181 data model coverage reports for Gateway 3 by downloading the BBF specification, cross-referencing with ODL implementation, and producing Excel/XML/JSON deliverables for specs/ and Confluence.
---

# GW3 Data Model Report Skill

Generate comprehensive TR-181 data model coverage reports for the Gateway 3 platform. The pipeline downloads the latest BBF specification XML, cross-references it against the ODL implementation in the build's staging directory, and produces annotated deliverables in multiple formats.

## Overview

The report pipeline has 7 steps that run sequentially:

| Step | Script | What it does |
|------|--------|-------------|
| 1 | `step1_download_xml.py` | Download latest TR-181 USP XML from Broadband Forum |
| 2 | `step2_generate_excel.py` | Generate standard Excel from TR-181 XML |
| 3 | `step3_generate_enhanced_xml.py` | Cross-reference ODL implementation → annotated XML |
| 4 | `step4_generate_json.py` | Convert XML files to JSON format |
| 5 | `step5_generate_enhanced_excel.py` | Generate enhanced Excel with coverage columns |
| 6 | `step6_publish_to_specs.py` | Copy deliverables to `specs/datamodel/` |
| 7 | `step7_upload_confluence.py` | Upload report to Confluence page |

Working directory: `local/datamodel/` (gitignored)
Published output: `specs/datamodel/` (committed to git)

## Prerequisites

### Build Environment

A completed build with a populated staging directory is **required** for steps 3-5 (ODL cross-referencing). The ODL files are read from:

```
staging_dir/target-aarch64_cortex-a55+neon-vfpv4_musl/root-ipq54xx/etc/amx/
```

Microservice version strings are resolved from `tmp/.packageinfo` (generated during build). If this file is missing, versions default to `"unknown"`.

If you have not built yet, run steps 1-2 first (they only need the downloaded XML).

### Python Packages

```bash
pip install openpyxl
```

The `openpyxl` package is required for Excel generation (steps 2 and 5). All other dependencies are Python standard library.

### Confluence Upload (Step 7 only)

- Confluence credentials in `~/.claude/.mcp.json` (Atlassian MCP server)
- `upload_confluence_v2.py` at `~/.claude/skills/confluence/scripts/`

## Quick Start — Full Pipeline

Run all steps from the project root:

```bash
# Step 1: Download TR-181 XML
python3 .github/skills/gw3-datamodel-report/scripts/step1_download_xml.py

# Step 2: Generate standard Excel
python3 .github/skills/gw3-datamodel-report/scripts/step2_generate_excel.py

# Step 3: Cross-reference with ODL → enhanced XML
python3 .github/skills/gw3-datamodel-report/scripts/step3_generate_enhanced_xml.py

# Step 4: Convert to JSON
python3 .github/skills/gw3-datamodel-report/scripts/step4_generate_json.py

# Step 5: Generate enhanced Excel
python3 .github/skills/gw3-datamodel-report/scripts/step5_generate_enhanced_excel.py

# Step 6: Publish to specs/datamodel/
python3 .github/skills/gw3-datamodel-report/scripts/step6_publish_to_specs.py

# Step 7: Upload to Confluence (optional)
python3 .github/skills/gw3-datamodel-report/scripts/step7_upload_confluence.py
```

## Step-by-Step Details

### Step 1: Download TR-181 XML

**What it does:** Fetches the index page at `usp-data-models.broadband-forum.org`, finds the latest TR-181 version, and downloads both the compressed and full XML files.

**Command:**
```bash
python3 .github/skills/gw3-datamodel-report/scripts/step1_download_xml.py
```

**Output files:**
- `local/datamodel/tr-181-<ver>-usp.xml` — compressed XML (definitions only)
- `local/datamodel/tr-181-<ver>-usp-full.xml` — full XML (with imported components)

**How to verify:**
- Both files exist and are > 0 KB
- Parse with: `python3 -c "import xml.etree.ElementTree as ET; ET.parse('local/datamodel/tr-181-*-full.xml')"`
- The full XML should be 10-30 MB

**Error handling:**
- Network errors: retry or manually download from the BBF website
- If the page format changes, update the URL pattern in `step1_download_xml.py`

---

### Step 2: Generate Standard Excel

**What it does:** Parses the TR-181 full XML and generates a multi-sheet Excel workbook with all objects, parameters, commands, events, data types, and references.

**Command:**
```bash
python3 .github/skills/gw3-datamodel-report/scripts/step2_generate_excel.py
# Or with explicit input:
python3 .github/skills/gw3-datamodel-report/scripts/step2_generate_excel.py --input local/datamodel/tr-181-2-20-1-usp-full.xml
```

**Output files:**
- `local/datamodel/tr181_definition.xlsx`

**Sheets:** Device-2, Data Types, References, Legend

**How to verify:**
- Row count on Device-2 sheet should be 15,000-20,000+
- Spot-check: `Device.DeviceInfo.` row should exist as an object
- All 8 columns (A-H) populated: Object Path, Item Path, Name, Type, Write, Description, Object Default, Version

---

### Step 3: Generate Enhanced XML (Most Complex)

**What it does:** Parses all ODL files from the staging directory, builds a service map, scans C source in `build_dir/` for emitted events (`amxd_object_emit_signal()`), filters out private/protected entries (not externally accessible), and annotates every TR-181 XML element with `gw3:` coverage attributes. Block-form events (`event "Name" { string Param; }`) have their argument parameter fields extracted and annotated. Events emitted from C code are promoted to IMPLEMENTED even if the ODL declaration lacks action callbacks.

**Command:**
```bash
python3 .github/skills/gw3-datamodel-report/scripts/step3_generate_enhanced_xml.py
```

**Requires:** Completed build with populated `staging_dir/`

**Output files:**
- `local/datamodel/gw3_current_datamodel.xml`

**GW3 annotation attributes added to each element (10 total):**
- `gw3:Microservice` — service name (e.g., "ip-manager")
- `gw3:Mark` — IMPLEMENTED / NOT IMPLEMENTED / NOT DEFINED / MISMATCH / VENDOR EXTENSION / PRPL EXTENSION
- `gw3:ODL_Type` — ODL data type
- `gw3:ODL_Access` — readOnly / readWrite (access type only)
- `gw3:ODL_Persistent` — yes / no (value persists across reboots)
- `gw3:ODL_Volatile` — yes / no (value recomputed on each read)
- `gw3:ODL_Default` — default value
- `gw3:ODL_Source_File` — ODL source filename
- `gw3:Microservice_version` — package version (from `tmp/.packageinfo`, underscore→hyphen fallback)
- `gw3:DIFF_Details` — mismatch details

**Mark semantics:**
| Mark | Meaning |
|------|---------|
| NOT DEFINED | In TR-181 spec but no ODL definition at all |
| NOT IMPLEMENTED | Defined in ODL but not actively served (readWrite param without callbacks, no confirmed C writes) |
| DEFAULT ONLY | ReadOnly param in ODL, no action callbacks, no confirmed C source write |
| REPORT ONLY | ReadWrite param in ODL, no action callbacks, but confirmed C source write; value readable but external writes have no side effects |
| IMPLEMENTED | Has action callbacks in ODL, or readOnly param with confirmed C write (`amxd_trans_set_value`/`amxd_object_set_value`), or event emitted via `amxd_object_emit_signal()` in C source |
| MISMATCH | Both exist but type or access differs |
| VENDOR EXTENSION | In ODL with X_ prefix but not in TR-181 standard (vendor-specific) |
| PRPL EXTENSION | In ODL without X_ prefix but not in TR-181 standard (prplOS additions) |

**Private/protected filtering:** ODL entries with `%private` or `%protected` modifiers are excluded from the output since they are not accessible to external clients (USP controllers, web UI). This typically removes 400-500 entries.

**Event argument extraction:** Block-form events like `event "ButtonEvent" { string Button; string Event; uint32 Interval; }` have their typed parameter fields parsed as `eventArg` entries. These are annotated in the TR-181 XML as `<parameter>` children of the `<event>` element, and included in vendor/prpl extension events.

**C source parameter write scan:** Scans all `.c` files under `build_dir/` for `amxd_trans_set_value()` and `amxd_object_set_value()` calls, extracting parameter names from string literals. ReadOnly ODL parameters whose names appear in the C write set for their service are promoted from NOT IMPLEMENTED to IMPLEMENTED. ReadOnly parameters without confirmed C writes are marked DEFAULT ONLY. ReadWrite parameters without callbacks but with confirmed C writes are marked REPORT ONLY (value is populated by the microservice and readable, but external writes have no action handlers).

**Microservice version lookup:** Package versions are resolved from `tmp/.packageinfo` (generated by the build system). The lookup tries exact service name match first, then underscore→hyphen substitution (e.g., `ssh_server` → `ssh-server`). Falls back to `"unknown"` for packages not present in `.packageinfo`.

**How to verify:**
- File has `xmlns:gw3="urn:actiontec:gw3:datamodel-coverage-1-0"` namespace
- >6000 entries annotated
- Spot-check: `Device.IP.Interface.` → should have mark=IMPLEMENTED, microservice=ip-manager
- Spot-check: `Device.DSL.` → should have mark=NOT DEFINED (no DSL service)
- Vendor extensions (X_PRPLWARE-COM_*) present with mark=VENDOR EXTENSION
- No private/protected entries in output (entries with `%private` or `%protected` ODL modifiers are excluded)
- Event arguments annotated: e.g., `Triggered!` event's `ParamPath` and `ParamValue` child parameters should have gw3: attributes
- `gw3:Microservice_version` populated for most services (e.g., ip-manager → v1.51.0-r1)

---

### Step 4: Generate JSON

**What it does:** Converts both the original and enhanced XML files to hierarchical JSON format. Event argument parameters are included in each event's `args[]` array with gw3 annotation fields.

**Command:**
```bash
python3 .github/skills/gw3-datamodel-report/scripts/step4_generate_json.py
```

**Output files:**
- `local/datamodel/tr181_original.json`
- `local/datamodel/gw3_current_datamodel.json`

**How to verify:**
- Both files are valid JSON: `python3 -c "import json; json.load(open('local/datamodel/tr181_original.json'))"`
- Object count matches XML element count

---

### Step 5: Generate Enhanced Excel

**What it does:** Reads the annotated XML from step 3 and generates a comprehensive Excel workbook with all 18 columns and a coverage summary sheet. Event argument parameter rows include GW3 annotation attributes (microservice, mark, ODL type, etc.) when annotated in the XML.

**Command:**
```bash
python3 .github/skills/gw3-datamodel-report/scripts/step5_generate_enhanced_excel.py
```

**Output files:**
- `local/datamodel/gw3_current_datamodel.xlsx`

**Columns (18 total):**
A: Object Path, B: Item Path, C: Name, D: Type, E: Write, F: Description, G: Object Default, H: Version, I: Microservice, J: Mark, K: ODL Type, L: ODL Access, M: ODL Persistent, N: ODL Volatile, O: ODL Default, P: ODL Source File, Q: Microservice Version, R: Diff Details

**Sheets:** Coverage Summary, Device-2, Legend

**Color coding in Mark column:**
- Green = IMPLEMENTED
- Yellow = NOT IMPLEMENTED
- Gray = DEFAULT ONLY
- Steel = REPORT ONLY
- Red = NOT DEFINED
- Orange = MISMATCH
- Blue = VENDOR EXTENSION (X_ prefix)
- Purple = PRPL EXTENSION (non-X_)

**How to verify:**
- 18 columns present (A through R)
- Coverage Summary sheet shows >20% implemented
- Freeze panes and auto-filter active

---

### Step 6: Publish to specs/datamodel/

**What it does:** Copies all generated files from `local/datamodel/` to `specs/datamodel/` and generates a `MANIFEST.md` with metadata.

**Command:**
```bash
python3 .github/skills/gw3-datamodel-report/scripts/step6_publish_to_specs.py
```

**Output files:**
- `specs/datamodel/` — all files from local/datamodel/
- `specs/datamodel/MANIFEST.md` — generation metadata

**How to verify:**
- All expected files present in `specs/datamodel/`
- `MANIFEST.md` contains correct TR-181 version and timestamps
- Files are ready to commit to git

---

### Step 7: Upload to Confluence

**What it does:** Generates a summary markdown page with coverage statistics and uploads it (with file attachments) to the configured Confluence page.

**Command:**
```bash
# Dry run (preview markdown, no upload)
python3 .github/skills/gw3-datamodel-report/scripts/step7_upload_confluence.py --dry-run

# Upload
python3 .github/skills/gw3-datamodel-report/scripts/step7_upload_confluence.py
```

**Target page:** Confluence page ID `4187652158` (R3 Gateway Specifications)

**What gets uploaded:**
- Page body: coverage summary with statistics tables
- Attachments: all Excel, XML, and JSON files from specs/datamodel/

**How to verify:**
- Confluence page updated with current date
- AI-generated notice block present
- Coverage statistics table populated
- Attachments listed on page

---

## Verification

After running steps 1-6, validate pipeline output with the automated verification script:

```bash
# Default: verify specs/datamodel/ (falls back to local/datamodel/)
python3 .github/skills/gw3-datamodel-report/scripts/step_verify.py

# Verbose output with details
python3 .github/skills/gw3-datamodel-report/scripts/step_verify.py --verbose

# Strict mode (WARNs treated as FAILs)
python3 .github/skills/gw3-datamodel-report/scripts/step_verify.py --strict

# JSON output for CI pipelines
python3 .github/skills/gw3-datamodel-report/scripts/step_verify.py --json-output

# Custom input directory
python3 .github/skills/gw3-datamodel-report/scripts/step_verify.py --input-dir local/datamodel/

# With staging directory for proxy mapping checks
python3 .github/skills/gw3-datamodel-report/scripts/step_verify.py --staging-dir staging_dir/target-aarch64_cortex-a55+neon-vfpv4_musl/root-ipq54xx/etc/amx
```

### Check Categories (6 categories, ~25 checks)

| Category | IDs | What it checks |
|----------|-----|----------------|
| **File Integrity** | INT-001 to INT-006 | Required files exist, non-zero, XML/JSON parse, GW3 namespace, Excel headers |
| **Statistics** | STAT-001 to STAT-005 | Total >= 5,000, mark sum consistency, IMPLEMENTED rate 3-60%, extensions > 0 |
| **Spot Checks** | SPOT-001 to SPOT-009 | Known paths (IP.Interface, DeviceInfo, DSL, MoCA, Hosts, WiFi, SFPs, PowerStatus) |
| **Proxy Mapping** | PROXY-001 to PROXY-003 | Proxy mappings found, each resolves entries, known proxies resolve (requires staging dir) |
| **Cross-Format** | XFMT-001 to XFMT-003 | JSON vs XML object count, Excel vs XML element count, local/ vs specs/ file sizes |
| **Parent-Child** | TREE-001 to TREE-002 | IMPLEMENTED params under NOT DEFINED parent, IMPLEMENTED objects with children |

### Exit Codes

- **0**: All checks PASS (or PASS+WARN in non-strict mode)
- **1**: One or more FAIL checks (or FAIL+WARN in strict mode)

### Example Output

```
=== GW3 Data Model Report Verification ===

--- File Integrity ---
  [PASS] INT-001: All 6 required files present
  [PASS] INT-002: No zero-byte files
  [PASS] INT-003: Enhanced XML parses successfully
  ...

--- Spot Checks ---
  [PASS] SPOT-001: Device.IP.Interface. mark=IMPLEMENTED svc=ip-manager
  [PASS] SPOT-002: Device.DeviceInfo. mark=IMPLEMENTED
  ...

=== Summary ===
  PASS: 22   WARN: 2   FAIL: 0   SKIP: 3
  Result: PASS
```

---

## Feature Specification

For detailed functional requirements, algorithm descriptions, and entity definitions, see:
**[`spec.md`](spec.md)** (in this skill directory)

The spec covers ~159 functional requirements across all 7 pipeline steps plus verification.

---

## Output Files Reference

| File | Format | Description |
|------|--------|-------------|
| `tr-181-<ver>-usp.xml` | XML | BBF TR-181 compressed definition |
| `tr-181-<ver>-usp-full.xml` | XML | BBF TR-181 full definition |
| `tr181_definition.xlsx` | Excel | Standard TR-181 definition spreadsheet |
| `gw3_current_datamodel.xml` | XML | Enhanced XML with gw3: coverage annotations |
| `gw3_current_datamodel.xlsx` | Excel | Enhanced spreadsheet with 18 columns + coverage summary |
| `tr181_original.json` | JSON | Original TR-181 data model |
| `gw3_current_datamodel.json` | JSON | Enhanced data model with annotations |
| `MANIFEST.md` | Markdown | Generation metadata and file index |

## Shared Modules

| Module | Purpose |
|--------|---------|
| `config.py` | Shared constants, paths, version helpers |
| `xml_utils.py` | XML parsing utilities (namespace stripping, description extraction, type building) |
| `service_map.py` | USP translate path mapping (ODL internal → Device.* paths) |
| `odl_parser.py` | Character-level ODL scanner with brace tracking |
| `step_verify.py` | Automated verification (25+ checks across 6 categories) |

## Troubleshooting

### "No input XML found"
Run step 1 first to download the TR-181 XML.

### "Directory not found: staging_dir/..."
A completed build is required for ODL cross-referencing (steps 3-5). Run the build first or use steps 1-2 independently.

### "openpyxl not found"
Install with: `pip install openpyxl`

### "Missing Confluence credentials"
Configure the Atlassian MCP server in `~/.claude/.mcp.json` with `CONFLUENCE_URL`, `CONFLUENCE_EMAIL`, and `CONFLUENCE_API_TOKEN`.

### Wrong ODL paths or low coverage
Check that the correct staging directory exists and contains AMX service directories. The default path is for `qca_ipq54xx` builds. For other targets, update `STAGING_AMX` in `config.py`.

## Adapting to Future TR-181 Versions

The pipeline automatically detects the latest TR-181 version from the BBF website. When a new version is published:

1. Re-run step 1 — it will download the newest version automatically
2. If the XML schema changes (new namespace version), update `BBF_NS` in `config.py`
3. If new element types are added, update `step3_generate_enhanced_xml.py` to annotate them
4. The version string propagates through all steps automatically via filenames

For a different data model (e.g., TR-104, TR-135), fork the scripts and update the URL patterns and element processing logic.
