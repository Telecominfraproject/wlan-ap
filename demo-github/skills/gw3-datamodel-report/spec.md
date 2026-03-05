# Feature Specification: GW3 TR-181 Data Model Coverage Report

| Metadata | Value |
|----------|-------|
| **Feature Branch** | `gw3_copilot` |
| **Created** | 2026-02-13 |
| **Status** | Draft |
| **Skill Path** | `.github/skills/gw3-datamodel-report/` |
| **TR-181 Version** | 2.20.1 (auto-detected) |

---

## Table of Contents

1. [Executive Summary](#executive-summary)
2. [Pipeline Architecture](#pipeline-architecture)
3. [Quick Reference](#quick-reference)
4. [User Scenarios & Testing](#user-scenarios--testing)
5. [Functional Requirements](#functional-requirements)
   - [5.1 Pipeline Infrastructure](#51-pipeline-infrastructure-fr-001-to-fr-010)
   - [5.2 Step 1: XML Download](#52-step-1-xml-download-fr-011-to-fr-020)
   - [5.3 Step 2: Standard Excel](#53-step-2-standard-excel-fr-021-to-fr-035)
   - [5.4 Step 3: ODL Cross-Reference](#54-step-3-odl-cross-reference-fr-036-to-fr-076)
   - [5.5 Step 4: JSON Conversion](#55-step-4-json-conversion-fr-077-to-fr-084)
   - [5.6 Step 5: Enhanced Excel](#56-step-5-enhanced-excel-fr-085-to-fr-101)
   - [5.7 Step 6: Publish](#57-step-6-publish-fr-102-to-fr-111)
   - [5.8 Step 7: Confluence Upload](#58-step-7-confluence-upload-fr-112-to-fr-126)
   - [5.9 Verification](#59-verification-fr-127-to-fr-146)
6. [Data Model & Entity Definitions](#data-model--entity-definitions)
7. [Algorithm Descriptions](#algorithm-descriptions)
8. [Success Criteria](#success-criteria)
9. [Dependencies & Assumptions](#dependencies--assumptions)
10. [Appendix: Type Mapping Tables](#appendix-type-mapping-tables)

---

## Executive Summary

The GW3 Data Model Coverage Report is a 7-step automated pipeline that generates comprehensive TR-181 data model coverage reports for the Gateway 3 platform. It downloads the latest Broadband Forum (BBF) TR-181 USP XML specification, cross-references it against the ODL (Object Definition Language) implementation in the build staging directory, and produces annotated deliverables in XML, JSON, and Excel formats.

The pipeline answers the question: *"For every TR-181 data model element, does Gateway 3 have an ODL implementation, and what is its status?"*

### Key Capabilities

- **Automatic version detection**: Fetches the latest TR-181 version from the BBF website
- **Service-level attribution**: Maps each data model element to the microservice that implements it, with package version from `tmp/.packageinfo`
- **Eight-state coverage model**: IMPLEMENTED, NOT IMPLEMENTED, DEFAULT ONLY, REPORT ONLY, NOT DEFINED, MISMATCH, VENDOR EXTENSION, PRPL EXTENSION
- **Proxy-object resolution**: Handles Ambiorix proxy-mapped paths that alias Device.* subtrees
- **Event argument extraction**: Parses typed parameter fields from block-form events (`event "Name" { string Param; }`)
- **Privacy filtering**: Excludes `%private` and `%protected` ODL entries not accessible to external clients
- **Extension classification**: Classifies unmatched ODL entries as VENDOR EXTENSION (X_ prefix) or PRPL EXTENSION (non-X_)
- **Multi-format output**: XML (with GW3 namespace annotations), Excel (with color-coded coverage), JSON (hierarchical)
- **Automated verification**: 25+ checks across 6 categories validate output correctness
- **Confluence publishing**: Uploads summary page with statistics tables and file attachments

---

## Pipeline Architecture

The pipeline consists of 7 sequential steps plus a verification step. Steps 1-2 require only network access. Steps 3-5 require a completed build with a populated staging directory. Steps 6-7 publish the results.

```
Step 1: Download XML ─────────────────────────────────────────────┐
  (BBF website → local/datamodel/tr-181-*-usp-full.xml)          │
                                                                   │
Step 2: Standard Excel ──────────────────────────────────────┐    │
  (tr-181-*-usp-full.xml → tr181_definition.xlsx)            │    │
                                                              │    │
Step 3: ODL Cross-Reference ──────────────────────────────┐  │    │
  (tr-181-*-usp-full.xml + staging_dir/etc/amx/*          │  │    │
   → gw3_current_datamodel.xml)                            │  │    │
                                                            │  │    │
Step 4: JSON Conversion ──────────────────────────────┐   │  │    │
  (both XMLs → .json files)                            │   │  │    │
                                                        │   │  │    │
Step 5: Enhanced Excel ────────────────────────────┐  │   │  │    │
  (gw3_current_datamodel.xml → .xlsx)              │  │   │  │    │
                                                    │  │   │  │    │
Step 6: Publish ──────────────────────────────┐   │  │   │  │    │
  (local/datamodel/ → specs/datamodel/)        │   │  │   │  │    │
                                                │   │  │   │  │    │
Verify: Automated checks ─────────────────┐  │   │  │   │  │    │
  (specs/datamodel/ → PASS/FAIL)           │  │   │  │   │  │    │
                                            │  │   │  │   │  │    │
Step 7: Confluence Upload                  │  │   │  │   │  │    │
  (summary + attachments → Confluence)     │  │   │  │   │  │    │
                                            ▼  ▼   ▼  ▼   ▼  ▼    ▼
                                      Output files in specs/datamodel/
```

### Data Flow

```
BBF Website ──HTTP GET──→ tr-181-2-20-1-usp-full.xml (5 MB, ~900 objects, ~15000 params)
                                     │
                                     ├──→ Step 2 ──→ tr181_definition.xlsx (standard, 8 columns)
                                     │
staging_dir/etc/amx/ ──┐             │
  ├─ service1/         │             │
  │   ├─ *.odl         │──ODL parse──┤
  │   └─ extensions/   │   + service │
  │       └─ *_usp.odl │     map     │
  ├─ service2/         │             │
  │   ...              │             │
  └─ tr181-device/     │             │
      └─ extensions/   │             │
          └─ *_mapping │──proxy map──┤
                                     │
                                     ├──→ Step 3 ──→ gw3_current_datamodel.xml (annotated)
                                     │                    │
                                     │                    ├──→ Step 4 ──→ .json files
                                     │                    │
                                     │                    └──→ Step 5 ──→ gw3_current_datamodel.xlsx
                                     │
                                     └──→ Step 6 ──→ specs/datamodel/ (+ MANIFEST.md)
                                                          │
                                                          ├──→ Verify ──→ exit code 0/1
                                                          │
                                                          └──→ Step 7 ──→ Confluence page
```

---

## Quick Reference

### Output Files

| File | Format | Size | Description |
|------|--------|------|-------------|
| `tr-181-2-20-1-usp.xml` | XML | ~260 KB | BBF TR-181 compressed definition |
| `tr-181-2-20-1-usp-full.xml` | XML | ~5 MB | BBF TR-181 full definition (with imports) |
| `tr181_definition.xlsx` | Excel | ~850 KB | Standard TR-181 spreadsheet (8 columns) |
| `gw3_current_datamodel.xml` | XML | ~7 MB | Enhanced XML with gw3: namespace annotations |
| `gw3_current_datamodel.xlsx` | Excel | ~620 KB | Enhanced spreadsheet (18 columns + coverage summary) |
| `tr181_original.json` | JSON | ~3.7 MB | Original TR-181 hierarchical JSON |
| `gw3_current_datamodel.json` | JSON | ~4.3 MB | Enhanced JSON with gw3_ annotation fields |
| `MANIFEST.md` | Markdown | ~1 KB | Generation metadata and file index |

### Mark Semantics

| Mark | Color | Meaning | Decision Rule |
|------|-------|---------|---------------|
| NOT DEFINED | Red | In TR-181 spec but no ODL definition exists | No ODL entry found for this path |
| NOT IMPLEMENTED | Yellow | ODL structure exists but not actively served | ODL entry exists; readWrite param without callbacks and no confirmed C writes |
| DEFAULT ONLY | Gray | ReadOnly param in ODL, no action callbacks, no confirmed C source write | ODL readOnly param; no `on action` callbacks; param name not found in `amxd_trans_set_value`/`amxd_object_set_value` calls |
| REPORT ONLY | Steel | ReadWrite param in ODL, no action callbacks, but confirmed C source write | ODL readWrite param; no callbacks; param name found in C source `amxd_trans_set_value`/`amxd_object_set_value` calls; value readable but external writes have no side effects |
| IMPLEMENTED | Green | Actively served: has action callbacks, or readOnly param with confirmed C write | ODL entry with `on action read/write call`, or readOnly param confirmed by C source scan, or event emitted via C source |
| MISMATCH | Orange | Both exist but type or access differs | TR-181 says readWrite, ODL says readOnly (or vice versa) |
| VENDOR EXTENSION | Blue | In ODL with X_ prefix, not in TR-181 | ODL Device.* path with X_ component, not matched to TR-181 |
| PRPL EXTENSION | Purple | In ODL without X_ prefix, not in TR-181 | ODL Device.* path without X_ component, not matched to TR-181 |

### GW3 Namespace Attributes

All 10 attributes added to each annotated XML element under `xmlns:gw3="urn:actiontec:gw3:datamodel-coverage-1-0"`:

| Attribute | Example Value | Description |
|-----------|---------------|-------------|
| `gw3:Microservice` | `ip-manager` | AMX service name |
| `gw3:Microservice_version` | `v1.51.0-r1` | Package version from `tmp/.packageinfo` (falls back to `unknown`) |
| `gw3:Mark` | `IMPLEMENTED` | Coverage status (one of 6 marks) |
| `gw3:ODL_Type` | `object` | ODL data type |
| `gw3:ODL_Access` | `readOnly` | ODL access type (readOnly / readWrite) |
| `gw3:ODL_Persistent` | `yes` | Whether value persists across reboots |
| `gw3:ODL_Volatile` | `no` | Whether value is recomputed on each read |
| `gw3:ODL_Default` | `0` | Default value from ODL |
| `gw3:ODL_Source_File` | `ip_manager.odl` | Source ODL filename |
| `gw3:DIFF_Details` | `access:spec_RW_odl_RO` | Mismatch description |

---

## User Scenarios & Testing

### US-1: Generate Full Report *(Priority: P1)*

**As a** firmware engineer, **I want** to generate a complete TR-181 coverage report **so that** I can assess which data model elements are implemented on Gateway 3.

| Step | Action | Expected |
|------|--------|----------|
| 1 | Run steps 1-7 sequentially | All output files created |
| 2 | Open enhanced Excel | 18 columns, color-coded marks |
| 3 | Filter by Mark=IMPLEMENTED | Shows services with action callbacks |
| 4 | Check Coverage Summary sheet | Implementation rate > 5% |

### US-2: Spot-Check Specific Service *(Priority: P1)*

**As a** developer working on the IP manager, **I want** to verify that `Device.IP.Interface.*` shows as IMPLEMENTED with `ip-manager` attribution **so that** I know my ODL definitions are correctly detected.

| Check | Expected |
|-------|----------|
| `Device.IP.Interface.` mark | IMPLEMENTED |
| `Device.IP.Interface.` microservice | ip-manager |
| `Device.IP.Interface.` children | Multiple with mark=IMPLEMENTED |

### US-3: Verify Proxy Mappings *(Priority: P2)*

**As a** platform engineer, **I want** to verify that proxy-mapped objects (e.g., PowerStatus) resolve correctly **so that** the report doesn't undercount implementation.

| Check | Expected |
|-------|----------|
| `Device.DeviceInfo.PowerStatus.*` | Entries with non-NOT-DEFINED marks |
| Verification script PROXY checks | All PASS |

### US-4: Run Verification *(Priority: P1)*

**As a** CI engineer, **I want** to run automated verification after the pipeline **so that** I can gate report publishing on correctness.

| Step | Action | Expected |
|------|--------|----------|
| 1 | Run `step_verify.py` | Exit code 0 |
| 2 | Run with `--strict` | Exit code 0 (no WARNs) |
| 3 | Run with `--json-output` | Valid JSON with check results |

---

## Functional Requirements

### 5.1 Pipeline Infrastructure (FR-001 to FR-010)

| ID | Requirement | Priority |
|----|-------------|----------|
| FR-001 | All scripts SHALL be executable from the project root directory | P1 |
| FR-002 | All scripts SHALL import shared configuration from `config.py` | P1 |
| FR-003 | The `config.py` module SHALL define: `PROJECT_ROOT`, `SCRIPTS_DIR`, `SKILL_DIR`, `LOCAL_DATAMODEL_DIR`, `SPECS_DATAMODEL_DIR` | P1 |
| FR-004 | The `config.py` module SHALL define: `BBF_BASE_URL`, `GW3_NAMESPACE`, `GW3_NS_PREFIX`, `BBF_NS`, `BBF_DMR_NS` | P1 |
| FR-005 | The `config.py` module SHALL define: `STAGING_AMX` path, `CONFLUENCE_PAGE_ID`, `VARIABLES` dict | P1 |
| FR-006 | The `config.py` module SHALL provide: `get_version_from_filename()`, `get_version_string()`, `ensure_output_dir()`, `staging_amx_path()` | P1 |
| FR-007 | All XML parsing utilities SHALL be centralized in `xml_utils.py` | P1 |
| FR-008 | The `xml_utils.py` module SHALL provide: `strip_ns()`, `normalize_path()`, `get_description_text()`, `build_type_string()`, `get_syntax_type()`, `get_default_value()`, `clean_description()` | P1 |
| FR-009 | Working output SHALL go to `local/datamodel/` (gitignored) | P1 |
| FR-010 | Published output SHALL go to `specs/datamodel/` (committed) | P1 |

### 5.2 Step 1: XML Download (FR-011 to FR-020)

| ID | Requirement | Priority |
|----|-------------|----------|
| FR-011 | The script SHALL fetch the BBF index page at `usp-data-models.broadband-forum.org` | P1 |
| FR-012 | The script SHALL parse the index HTML to find all available TR-181 USP XML version links | P1 |
| FR-013 | The script SHALL select the latest version by sorting version tuples in descending order | P1 |
| FR-014 | The script SHALL download both the compressed (`*-usp.xml`) and full (`*-usp-full.xml`) XML files | P1 |
| FR-015 | Downloaded files SHALL be saved to `local/datamodel/` | P1 |
| FR-016 | Each downloaded file SHALL be verified as valid XML using `ET.parse()` | P1 |
| FR-017 | The script SHALL report download sizes and verification results | P2 |
| FR-018 | The script SHALL fail with exit code 1 if no files download successfully | P1 |
| FR-019 | The script SHALL use a User-Agent header to avoid server blocks | P2 |
| FR-020 | The script SHALL handle URL and HTTP errors gracefully with error messages | P1 |

### 5.3 Step 2: Standard Excel (FR-021 to FR-035)

| ID | Requirement | Priority |
|----|-------------|----------|
| FR-021 | The script SHALL auto-detect the latest full XML in `local/datamodel/` if no `--input` is given | P1 |
| FR-022 | The output workbook SHALL have 4 sheets: Device-2, Data Types, References, Legend | P1 |
| FR-023 | The Device-2 sheet SHALL have 8 columns: Object Path, Item Path, Name, Type, Write, Description, Object Default, Version | P1 |
| FR-024 | The Device-2 sheet SHALL include rows for: objects, parameters, commands, events, argument containers, argument parameters, argument objects | P1 |
| FR-025 | Object rows SHALL use bold font and light blue background | P2 |
| FR-026 | Command rows SHALL use green bold font and light green background | P2 |
| FR-027 | Event rows SHALL use orange bold font and light orange background | P2 |
| FR-028 | Deprecated items SHALL use light yellow background; obsoleted items SHALL use gray background | P2 |
| FR-029 | The sheet SHALL have freeze panes at row 2 and auto-filter on all columns | P1 |
| FR-030 | Description text SHALL be cleaned of BBF template markers (`{{bibref}}`, `{{enum}}`, etc.) via `clean_description()` | P1 |
| FR-031 | Descriptions exceeding 32,000 characters SHALL be truncated with `[truncated]` suffix | P1 |
| FR-032 | The Data Types sheet SHALL list all `<dataType>` definitions with name, base type, and description | P2 |
| FR-033 | The References sheet SHALL list all `<bibliography><reference>` entries with ID, name, title, URL | P2 |
| FR-034 | The Legend sheet SHALL document row colors and column meanings | P2 |
| FR-035 | The output file SHALL be named `tr181_definition.xlsx` | P1 |

### 5.4 Step 3: ODL Cross-Reference (FR-036 to FR-076)

This is the most complex step. It parses all ODL files from the build staging directory, builds a service map and proxy mappings, and annotates every TR-181 XML element.

**Service Map (FR-036 to FR-042)**

| ID | Requirement | Priority |
|----|-------------|----------|
| FR-036 | The service map SHALL be built by scanning `staging_dir/etc/amx/*/extensions/*_usp.odl` | P1 |
| FR-037 | Each `*_usp.odl` file SHALL be parsed for `usp.translate` blocks containing internal→device path mappings | P1 |
| FR-038 | Variable substitutions (`${var}`) SHALL be applied using `config.VARIABLES` before parsing | P1 |
| FR-039 | The service map SHALL include hardcoded fallbacks for services without USP translate files: `tr181-device`, `netmodel`, `deviceinfo-system`, `aei_network`, `aei_led_manager` | P2 |
| FR-040 | The service map output SHALL be a dict: `{service_name: [(internal_prefix, device_prefix), ...]}` | P1 |
| FR-041 | The service map SHALL be usable standalone via `service_map.py` writing to `service-map.json` | P2 |
| FR-042 | Services without a directory in the staging dir SHALL be excluded from fallbacks | P1 |

**ODL Parser (FR-043 to FR-058)**

| ID | Requirement | Priority |
|----|-------------|----------|
| FR-043 | The ODL parser SHALL use a character-level scanner with brace-depth tracking | P1 |
| FR-044 | The parser SHALL handle `%define { }` blocks (definition context) | P1 |
| FR-045 | The parser SHALL handle `select <path> { }` blocks (extension context with initial path stack) | P1 |
| FR-046 | The parser SHALL strip C-style block (`/* */`) and line (`//`) comments while preserving string literals | P1 |
| FR-047 | The parser SHALL extract objects (singleton and multi-instance `[]`) with name, access, and modifiers | P1 |
| FR-048 | The parser SHALL extract parameters with: type, name, access, default value, action callbacks. Parameters without action callbacks SHALL be marked NOT IMPLEMENTED; C source scanning (step 3) may later promote readOnly params to IMPLEMENTED. | P1 |
| FR-049 | The parser SHALL extract commands/functions with return type, name, and mark as IMPLEMENTED | P1 |
| FR-050 | The parser SHALL extract events in both semicolon form (`event 'Name!';`) and block form (`event "Name" { ... }`) | P1 |
| FR-050a | For block-form events, the parser SHALL extract typed parameter declarations (e.g., `string ParamPath;`) from the event body as `eventArg` entries | P1 |
| FR-050b | Event argument entries SHALL have `entry_type = "eventArg"`, inherit `private`/`protected` from the parent event, and use the event's device path as prefix (e.g., `Device.X.Event!.ParamName`) | P1 |
| FR-050c | Statements inside event blocks SHALL NOT be processed by `_process_statement()` (prevented via `in_data_block` skip list) to avoid double-processing | P1 |
| FR-051 | For parameters with `{ }` blocks, the parser SHALL scan for `on action read/write call` to determine IMPLEMENTED status | P1 |
| FR-052 | The parser SHALL apply variable substitutions before scanning | P1 |
| FR-053 | The parser SHALL skip directories: `extensions/`, `rules4/`, `rules6/`, `defaults*/`, `iptables*/` | P1 |
| FR-054 | The parser SHALL translate internal ODL paths to Device.* paths using the service map | P1 |
| FR-055 | If no service map entry exists, the parser SHALL prepend `Device.` to internal paths as fallback | P1 |
| FR-056 | Post-processing SHALL mark objects as IMPLEMENTED if any child has action callbacks (bottom-up propagation) | P1 |
| FR-057 | Deduplication SHALL prefer entries with `impl_level=IMPLEMENTED` over `NOT IMPLEMENTED` | P1 |
| FR-058 | The parser SHALL recognize 16 ODL types and map them to TR-181 types via `ODL_TYPE_MAP` | P1 |

**Proxy Mapping (FR-059 to FR-064)**

| ID | Requirement | Priority |
|----|-------------|----------|
| FR-059 | The proxy parser SHALL scan all `*_mapping.odl` files under the staging directory recursively | P1 |
| FR-060 | The parser SHALL extract `%global "proxy-object.'<device_prefix>'" = "<local_prefix>";` directives | P1 |
| FR-061 | Mappings with empty local prefix SHALL be skipped (root mapping) | P1 |
| FR-062 | For each proxy mapping `Device.X. → Y.`, the augmentation SHALL create aliases by checking: (a) entries at `Device.Y.` (fallback prefix), (b) entries at the USP-translated path | P1 |
| FR-063 | Alias entries SHALL copy the source entry data but with the aliased device path | P1 |
| FR-064 | The augmentation SHALL NOT overwrite existing ODL index entries | P1 |

**Emitted Event Promotion (FR-065 to FR-068)**

| ID | Requirement | Priority |
|----|-------------|----------|
| FR-065 | The script SHALL scan all `.c` files under `build_dir/` for `amxd_object_emit_signal()` calls | P1 |
| FR-066 | The scanner SHALL extract event names (strings ending with `!`) from the second argument of `amxd_object_emit_signal()` | P1 |
| FR-067 | The scanner SHALL skip test directories (`test/`, `tests/`, `mock/`, `mocks/`) to avoid counting test-only events | P1 |
| FR-068 | For each ODL event entry whose name matches an emitted event, the script SHALL promote `impl_level` to IMPLEMENTED | P1 |

**C Source Parameter Write Scan (FR-068a to FR-068f)**

| ID | Requirement | Priority |
|----|-------------|----------|
| FR-068a | The script SHALL scan all `.c` files under `build_dir/` for `amxd_trans_set_value()` and `amxd_object_set_value()` calls | P1 |
| FR-068b | The scanner SHALL extract parameter names from string literal arguments (second-to-last quoted string in the call) | P1 |
| FR-068c | The scanner SHALL skip test directories (`test/`, `tests/`, `mock/`, `mocks/`) | P1 |
| FR-068d | Results SHALL be grouped by build package directory, yielding `{pkg_dir: set(param_names)}` | P1 |
| FR-068e | For each readOnly ODL parameter with `impl_level=NOT IMPLEMENTED`, if the parameter name is found in the C write set for the matching service package, the script SHALL promote `impl_level` to IMPLEMENTED | P1 |
| FR-068f | Service-to-package matching SHALL try the service name (with underscores replaced by hyphens) as a substring of the build directory name | P1 |

**Private/Protected Filtering (FR-069 to FR-070a)**

| ID | Requirement | Priority |
|----|-------------|----------|
| FR-069 | After ODL index augmentation and before annotation, entries with `%private` modifier SHALL be removed from the ODL entries list and ODL index | P1 |
| FR-069a | After ODL index augmentation and before annotation, entries with `%protected` modifier SHALL be removed from the ODL entries list and ODL index | P1 |
| FR-070a | The script SHALL log the count of filtered private/protected entries | P2 |

**Microservice Version Lookup (FR-070b to FR-070e)**

| ID | Requirement | Priority |
|----|-------------|----------|
| FR-070b | The script SHALL parse `tmp/.packageinfo` to build a `{package_name: version}` cache for version lookup | P1 |
| FR-070c | The cache SHALL be loaded once (lazy singleton) and reused across all `get_pkg_version()` calls | P2 |
| FR-070d | `get_pkg_version()` SHALL try exact service name match first, then underscore→hyphen substitution (e.g., `ssh_server` → `ssh-server`) | P1 |
| FR-070e | If no match is found in `tmp/.packageinfo`, `get_pkg_version()` SHALL return `"unknown"` | P1 |

**XML Annotation (FR-071 to FR-076)**

| ID | Requirement | Priority |
|----|-------------|----------|
| FR-071 | The GW3 namespace `urn:actiontec:gw3:datamodel-coverage-1-0` SHALL be registered with prefix `gw3` | P1 |
| FR-072 | Every `<object>`, `<parameter>`, `<command>`, and `<event>` element in the model SHALL receive 10 gw3: attributes | P1 |
| FR-072a | Event child `<parameter>` elements (event arguments) SHALL also receive 10 gw3: attributes by looking up `EventPath.ParamName` in the ODL index | P1 |
| FR-073 | Mark determination SHALL follow: no ODL entry → NOT DEFINED; ODL with actions → IMPLEMENTED; readOnly param without actions/C-write → DEFAULT ONLY; readWrite param without actions but with C-write → REPORT ONLY; readWrite param without actions and without C-write → NOT IMPLEMENTED; access mismatch → MISMATCH | P1 |
| FR-074 | After annotation, extension elements SHALL be added for all ODL Device.* entries not matched to any TR-181 element, classified as VENDOR EXTENSION (X_ prefix) or PRPL EXTENSION (non-X_) | P1 |
| FR-075 | Extensions SHALL be grouped by parent object and added as `<object>` elements with child `<parameter>`, `<command>`, `<event>` subelements | P1 |
| FR-075a | Extension `<event>` elements SHALL include child `<parameter>` elements for any `eventArg` entries whose path starts with the event's device path | P1 |
| FR-076 | The output SHALL be written as `gw3_current_datamodel.xml` with XML declaration and UTF-8 encoding | P1 |

### 5.5 Step 4: JSON Conversion (FR-077 to FR-084)

| ID | Requirement | Priority |
|----|-------------|----------|
| FR-077 | The script SHALL convert the original TR-181 XML to `tr181_original.json` (without gw3 attributes) | P1 |
| FR-078 | The script SHALL convert the enhanced XML to `gw3_current_datamodel.json` (with gw3 attributes as `gw3_` prefixed fields) | P1 |
| FR-079 | Each JSON object entry SHALL contain: `_tag`, `name`, `access`, `version`, `description`, `parameters[]`, `commands[]`, `events[]` | P1 |
| FR-080 | Each JSON parameter SHALL contain: `_tag`, `name`, `access`, `type`, `default`, `description` | P1 |
| FR-081 | Each JSON command SHALL contain: `_tag`, `name`, `input_args[]`, `output_args[]` | P1 |
| FR-081a | Each JSON event SHALL contain: `_tag`, `name`, `args[]` where each arg is a parameter dict with gw3 attributes when available | P1 |
| FR-082 | Enhanced JSON entries SHALL additionally contain: `gw3_Microservice`, `gw3_Mark`, `gw3_ODL_Type`, `gw3_ODL_Access`, `gw3_ODL_Persistent`, `gw3_ODL_Volatile`, `gw3_ODL_Default`, `gw3_ODL_Source_File`, `gw3_Microservice_version`, `gw3_DIFF_Details` | P1 |
| FR-083 | The JSON output SHALL be indented with 2 spaces | P2 |
| FR-084 | The top-level JSON structure SHALL be `{model_name, objects[]}` | P1 |

### 5.6 Step 5: Enhanced Excel (FR-085 to FR-101)

| ID | Requirement | Priority |
|----|-------------|----------|
| FR-085 | The input SHALL be `gw3_current_datamodel.xml` | P1 |
| FR-086 | The output workbook SHALL have 3 sheets: Coverage Summary, Device-2, Legend | P1 |
| FR-087 | The Device-2 sheet SHALL have 18 columns (A-R): the 8 standard columns plus 10 GW3 annotation columns | P1 |
| FR-088 | The 10 GW3 columns SHALL be: Microservice (I), Mark (J), ODL Type (K), ODL Access (L), ODL Persistent (M), ODL Volatile (N), ODL Default (O), ODL Source File (P), Microservice Version (Q), Diff Details (R) | P1 |
| FR-089 | The Mark column (J) SHALL be color-coded: green=IMPLEMENTED, yellow=NOT IMPLEMENTED, red=NOT DEFINED, orange=MISMATCH, blue=VENDOR EXTENSION, purple=PRPL EXTENSION | P1 |
| FR-090 | The sheet SHALL have freeze panes at A2 and auto-filter on all 18 columns | P1 |
| FR-091 | Row background colors SHALL follow Step 2 conventions (blue=object, white=param, green=command, orange=event) | P2 |
| FR-092 | The Coverage Summary sheet SHALL show overall statistics: total items, count per mark, implementation rate, coverage rate | P1 |
| FR-093 | The Coverage Summary sheet SHALL include a per-subtree table sorted by total count descending | P1 |
| FR-094 | The Coverage Summary sheet SHALL include a per-microservice table sorted by total count descending | P1 |
| FR-095 | Subtree coverage percentages SHALL use color coding: green >= 80%, yellow >= 40%, red < 40% | P2 |
| FR-096 | Implementation rate SHALL be calculated as: IMPLEMENTED / total * 100 | P1 |
| FR-097 | Coverage rate SHALL be calculated as: (IMPLEMENTED + NOT IMPLEMENTED) / total * 100 | P1 |
| FR-098 | The Legend sheet SHALL document all 6 mark colors and their meanings | P1 |
| FR-098a | Event argument parameter rows SHALL read gw3: attributes from annotated event child `<parameter>` elements (not empty strings) | P1 |
| FR-099 | Statistics SHALL only count rows with marks (not argument-container or argument-parameter rows) | P1 |
| FR-100 | MISMATCH entries SHALL be counted as IMPLEMENTED for coverage calculation purposes | P2 |
| FR-101 | The output file SHALL be named `gw3_current_datamodel.xlsx` | P1 |

### 5.7 Step 6: Publish (FR-102 to FR-111)

| ID | Requirement | Priority |
|----|-------------|----------|
| FR-102 | The script SHALL verify that required files exist in `local/datamodel/` before copying | P1 |
| FR-103 | Required files: `tr181_definition.xlsx`, `gw3_current_datamodel.xml`, `gw3_current_datamodel.xlsx` | P1 |
| FR-104 | Optional files: `tr181_original.json`, `gw3_current_datamodel.json` | P1 |
| FR-105 | All `tr-181-*.xml` files SHALL also be copied | P1 |
| FR-106 | Existing non-hidden files in `specs/datamodel/` SHALL be removed before copying | P1 |
| FR-107 | A `MANIFEST.md` SHALL be generated with: generation timestamp (UTC), TR-181 version, tool name | P1 |
| FR-108 | The MANIFEST SHALL include a files table with filename, description, and file size | P1 |
| FR-109 | The script SHALL fail with exit code 1 if required files are missing | P1 |
| FR-110 | The script SHALL print a summary of copied files with sizes | P2 |
| FR-111 | The TR-181 version SHALL be detected from XML filenames in `local/datamodel/` | P1 |

### 5.8 Step 7: Confluence Upload (FR-112 to FR-126)

| ID | Requirement | Priority |
|----|-------------|----------|
| FR-112 | The script SHALL read Confluence credentials from `~/.claude/.mcp.json` (Atlassian MCP server config) | P1 |
| FR-113 | The script SHALL map `CONFLUENCE_EMAIL` to `CONFLUENCE_USERNAME` for the upload script | P1 |
| FR-114 | The script SHALL generate a summary markdown document with: AI-generated notice, TOC marker, coverage statistics tables | P1 |
| FR-115 | The AI-generated notice SHALL include: tool name, generation timestamp, source path, "do not edit" warning | P1 |
| FR-116 | Statistics SHALL be collected by parsing the enhanced XML (not Excel) for reliability | P1 |
| FR-117 | The summary SHALL include tables for: overall stats, per-subtree coverage, per-microservice coverage (top 30) | P1 |
| FR-118 | Subtrees with fewer than 5 items SHALL be excluded from the per-subtree table | P2 |
| FR-119 | The markdown SHALL be uploaded via `upload_confluence_v2.py` | P1 |
| FR-120 | All Excel, XML, and JSON files from `specs/datamodel/` SHALL be uploaded as Confluence attachments | P1 |
| FR-121 | Existing attachments with the same filename SHALL be updated (not duplicated) | P1 |
| FR-122 | After upload, the script SHALL inject clickable attachment download links into the page body using Confluence storage format | P1 |
| FR-123 | The attachment table SHALL use `<ac:link><ri:attachment>` Confluence macros | P1 |
| FR-124 | The script SHALL update `confluence-pages.yaml` with the page mapping (if not already present) | P2 |
| FR-125 | The `--dry-run` flag SHALL generate markdown without uploading | P1 |
| FR-126 | The target Confluence page SHALL be configurable via `--page-id` (default from `config.CONFLUENCE_PAGE_ID`) | P1 |

### 5.9 Verification (FR-127 to FR-146)

| ID | Requirement | Priority |
|----|-------------|----------|
| FR-127 | The verification script SHALL accept `--input-dir`, `--staging-dir`, `--strict`, `--json-output`, `--verbose` arguments | P1 |
| FR-128 | Default input directory SHALL be `specs/datamodel/`, falling back to `local/datamodel/` | P1 |
| FR-129 | The script SHALL run 6 check categories: File Integrity, Statistics, Spot Checks, Proxy Mapping, Cross-Format, Parent-Child | P1 |
| FR-130 | Each check SHALL produce a result with: check_id, category, status (PASS/WARN/FAIL/SKIP), message, optional details | P1 |
| FR-131 | File Integrity checks SHALL verify: required files exist, no zero-byte files, XML parses, JSON parses, GW3 namespace present, Excel has 18 correct headers | P1 |
| FR-132 | Statistics checks SHALL verify: total >= 5,000 items, mark sum equals total, IMPLEMENTED rate 3-60%, NOT DEFINED is largest category, VENDOR EXTENSION > 0 | P1 |
| FR-133 | Spot checks SHALL verify known paths: `Device.IP.Interface.` (IMPLEMENTED/ip-manager), `Device.DeviceInfo.` (IMPLEMENTED), DSL/MoCA (NOT DEFINED), `Device.Hosts.` (IMPLEMENTED), vendor extensions (exist), WiFi/wld entries, SFPs, PowerStatus | P1 |
| FR-134 | Proxy checks SHALL run only if staging directory is available; SKIP otherwise | P1 |
| FR-135 | Proxy checks SHALL verify: mappings found > 0, each resolves entries, known proxy paths have non-NOT-DEFINED marks | P1 |
| FR-136 | Cross-format checks SHALL verify: JSON object count within 5% of XML, Excel rows within 10% of XML elements, file sizes match between local/ and specs/ | P1 |
| FR-137 | Parent-child checks SHALL verify: IMPLEMENTED params under NOT DEFINED parent (FAIL if > 100, WARN if > 10), IMPLEMENTED objects with no IMPLEMENTED children | P1 |
| FR-138 | `--strict` mode SHALL treat WARNs as FAILs for exit code calculation | P1 |
| FR-139 | `--json-output` SHALL produce a JSON object with `checks[]` array and `summary` | P1 |
| FR-140 | Normal output SHALL group results by category with `[PASS]`/`[WARN]`/`[FAIL]`/`[SKIP]` prefixes | P1 |
| FR-141 | A summary line SHALL show counts: PASS, WARN, FAIL, SKIP, and overall Result | P1 |
| FR-142 | Exit code SHALL be 0 if no FAILs (or no FAILs+WARNs in strict mode), 1 otherwise | P1 |
| FR-143 | `--verbose` SHALL show details for each check result | P2 |
| FR-144 | The script SHALL import shared modules from `config.py` and `xml_utils.py` | P1 |
| FR-145 | Proxy checks SHALL conditionally import from `step3_generate_enhanced_xml` and `service_map` | P1 |
| FR-146 | The script SHALL handle missing optional dependencies (openpyxl) gracefully with SKIP status | P1 |

---

## Data Model & Entity Definitions

### GW3 Namespace

- **URI**: `urn:actiontec:gw3:datamodel-coverage-1-0`
- **Prefix**: `gw3`
- **Applied to**: All `<object>`, `<parameter>`, `<command>`, `<event>` elements in the `<model>` section, plus `<parameter>` children of `<event>` elements (event arguments)

### Mark Decision Table

| TR-181 Element | ODL Entry Found | Has Actions | Access | C Write Confirmed | Access Match | Result |
|----------------|-----------------|-------------|--------|-------------------|--------------|--------|
| Present | No | - | - | - | - | NOT DEFINED |
| Present | Yes (param) | No | readOnly | Yes | Yes | IMPLEMENTED |
| Present | Yes (param) | No | readOnly | No | Yes | DEFAULT ONLY |
| Present | Yes (param) | No | readWrite | Yes | Yes | REPORT ONLY |
| Present | Yes (param) | No | readWrite | No | Yes | NOT IMPLEMENTED |
| Present | Yes | Yes | Yes | IMPLEMENTED |
| Present | Yes | Any | No | MISMATCH |
| Absent | Yes (X_ prefix path) | Any | - | VENDOR EXTENSION |
| Absent | Yes (non-X_ path) | Any | - | PRPL EXTENSION |

### ODL Entry Structure

Each parsed ODL entry contains:

```
{
  "device_path": "Device.IP.Interface.",    # Translated Device.* path
  "name": "Interface",                       # Short name
  "service": "ip-manager",                   # AMX service name
  "entry_type": "object|parameter|command|event|eventArg",
  "odl_type": "string|bool|uint32|...",     # ODL type
  "tr181_type": "string|boolean|unsignedInt|...",  # Mapped TR-181 type
  "access": "readOnly|readWrite",            # Raw access type
  "access_display": "readOnly|readWrite",    # Access type for display (same as access)
  "persistent": true|false,                  # %persistent modifier
  "private": true|false,                     # %private modifier (entry excluded from output)
  "protected": true|false,                   # %protected modifier (entry excluded from output)
  "volatile": true|false,                    # %volatile modifier
  "is_multi_instance": true|false,
  "default": "value"|null,
  "has_actions": true|false,                 # Has on action read/write call
  "impl_level": "IMPLEMENTED|NOT IMPLEMENTED",
  "source_file": "ip_manager.odl"
}
```

### Service Map Entry

```
{
  "service_name": [(internal_prefix, device_prefix), ...]
}
```

Example: `{"ip-manager": [("IP.", "Device.IP.")]}`

### Proxy Mapping Entry

```
{device_prefix: local_prefix}
```

Example: `{"Device.DeviceInfo.PowerStatus.": "PowerStatus."}`

---

## Algorithm Descriptions

### 7.1 ODL Character-Level Scanner

The scanner (`scan_with_param_blocks()` in `odl_parser.py`) processes ODL text character by character with the following state:

1. **Path stack**: List of object names forming the current hierarchy (e.g., `["IP", "Interface"]`)
2. **Depth info stack**: Parallel stack tracking block types (`object`, `param`, `event`, `mib`, `other`)
3. **String tracking**: Skips characters inside `"..."` and `'...'` literals

**State transitions:**

- **`{` (opening brace)**: Examine the preceding statement text:
  - If `object <name>` pattern → push name onto path stack, emit object entry, push `('object', ...)` to depth info
  - If `<type> <name>` where type is ODL type → emit parameter entry, push `('param', entry, block_start)` to depth info
  - If `event <name>` → emit event entry, push `('event', entry, block_start)` to depth info
  - If `mib <name>` → push `('mib',)` to depth info (skip block)
  - Otherwise → push `('other',)` to depth info

- **`}` (closing brace)**: Pop from depth info:
  - If `object` → pop path stack
  - If `event` → extract block text, scan for typed parameter declarations via `_scan_event_block()`, add as `eventArg` entries
  - If `param` → extract block text, scan for `on action read/write call` and `default`, update entry

- **`;` (semicolon)**: Process the accumulated statement:
  - Only process if path stack is non-empty and we're inside an object block (not param/mib/other/event)
  - Check for: event declarations, function/command declarations, parameter declarations

### 7.2 Proxy Object Resolution

Two-phase process in `augment_odl_index_with_proxy_mappings()`:

**Phase 1 — Candidate prefix generation:**
For each proxy mapping `Device.X. → Y.`:
1. Compute fallback prefix: `Device.Y.` (parser prepends `Device.` to unresolved internal paths)
2. Look up USP translation: if `Y.` maps to `Device.Z.` via service map, add `Device.Z.` as candidate

**Phase 2 — Alias creation:**
For each candidate prefix:
1. Scan all ODL index entries matching the candidate prefix
2. For each match, compute aliased path: replace candidate prefix with `Device.X.`
3. If aliased path not already in ODL index → add aliased entry (copy of source with new device_path)

### 7.3 Emitted Event Promotion

Events in the AMX framework are emitted from C code via `amxd_object_emit_signal(object, "EventName!", ...)`. Unlike parameters and commands that use ODL action callbacks (`on action read/write call`), events are considered implemented when the C source contains an explicit emit call.

Process in `scan_emitted_events()` + `promote_emitted_events()`:

1. Walk all `.c` files under `build_dir/` (the cross-compiled package source tree)
2. Skip test directories (`test/`, `tests/`, `mock/`, `mocks/`) to exclude test-only events
3. Match regex pattern: `amxd_object_emit_signal\s*\([^,]+,\s*"([^"]+!)"` — extracts event names ending with `!`
4. Collect unique event names into a set
5. For each ODL index entry with `entry_type == "event"` and `impl_level != "IMPLEMENTED"`:
   - Extract the event name from the entry's device path (last path component)
   - If the name matches an emitted event → promote to IMPLEMENTED
6. This step runs after proxy mapping augmentation and before XML annotation

### 7.4 Extension Injection

Process in `add_vendor_extensions()`:

1. Identify unmatched ODL entries (not in `matched_paths` set) under `Device.*`
2. Deduplicate entries by device_path, preferring IMPLEMENTED over NOT IMPLEMENTED
3. Classify each entry's mark via `_extension_mark()`:
   - If any path component starts with `X_` → **VENDOR EXTENSION**
   - Otherwise → **PRPL EXTENSION**
4. Exclude `eventArg` entries from object-level grouping (they are nested under events instead)
5. Group remaining entries by parent object path
6. For each group:
   - Create `<object>` element with `name=parent_path`, `mark=<classified mark>`
   - Add child `<parameter>`, `<command>`, `<event>` elements with `mark=<classified mark>`
   - For each `<event>` element, scan `eventArg` entries matching the event's device path prefix and add them as child `<parameter>` elements
7. All elements receive full set of 10 gw3: attributes (including ODL_Persistent and ODL_Volatile)

### 7.5 Event Argument Extraction

ODL events come in two forms:

1. **Semicolon form**: `event 'Name!';` — declares the event with no argument fields
2. **Block form**: `event "Name" { string Param1; uint32 Param2; }` — declares event with typed argument parameters

Process in `_scan_event_block()`:

1. When the scanner encounters a block-form event `{`, it stores the event entry and block start position in `depth_info`
2. The `in_data_block` check includes `'event'` in the skip list, preventing `_process_statement()` from processing inner statements
3. When the closing `}` is reached, `_scan_event_block()` is called with the full block text
4. The method scans for typed parameter declarations using regex: `(modifiers)(type) (name);`
5. Each matched parameter (where type is a known ODL type) creates an `eventArg` entry with:
   - `device_path` = event's device_path + `.` + param_name
   - `entry_type` = `"eventArg"`
   - `private`/`protected` inherited from the parent event entry
6. Event arguments are indexed in the ODL index and looked up during XML annotation

**Example**: ODL `event "ButtonEvent" { string Button; string Event; uint32 Interval; }` under object `Buttons.Button.` produces three `eventArg` entries at `Device.X_PRPLWARE-COM_Buttons.Button.{i}.ButtonEvent.Button`, `.Event`, and `.Interval`.

### 7.6 Private/Protected Filtering

ODL entries with `%private` or `%protected` modifiers represent data model nodes that are not accessible to external clients (USP controllers, web UI, CWMP ACS). These entries are filtered out before XML annotation to avoid reporting coverage for inaccessible paths.

Process in `main()` after ODL index augmentation:

1. Iterate all entries in the `odl_entries` list and `odl_index` dict
2. Remove entries where `private == True` or `protected == True`
3. This typically removes 400-500 entries from the ODL index
4. Filtering occurs after proxy mapping augmentation and event promotion, but before XML annotation
5. Filtered entries do not appear in any output format (XML, Excel, JSON)

### 7.7 Access Attribute Parsing

The `parse_access_attrs()` function in `odl_parser.py` extracts structured access/visibility attributes from ODL modifier strings.

Input: modifier string (e.g., `%read-only %persistent %volatile`)
Output: dict with fields: `access`, `persistent`, `private`, `protected`, `volatile`, `locked`, `access_display`

- `access`: "readOnly" if `%read-only` or `%read_only` present, else "readWrite"
- `persistent`, `private`, `protected`, `volatile`, `locked`: boolean flags
- `access_display`: same as `access` (clean display value — persistent/volatile have dedicated columns)

---

## Success Criteria

| ID | Criterion | Validation Method |
|----|-----------|-------------------|
| SC-001 | Full pipeline produces all 8 output files | Check file existence |
| SC-002 | Enhanced XML has >= 5,000 annotated elements | STAT-001 check |
| SC-003 | Implementation rate >= 5% | STAT-003 check |
| SC-004 | Verification script exits 0 | `step_verify.py` exit code |
| SC-005 | Known spot-check items correct (IP, DeviceInfo, DSL, MoCA, Hosts) | SPOT-001 through SPOT-005 |
| SC-006 | Proxy-mapped entries resolve (PowerStatus) | PROXY-003 and SPOT-009 |
| SC-007 | Confluence page shows coverage tables and attachment links | Manual inspection after Step 7 |
| SC-008 | Pipeline is idempotent (re-running produces same output) | Run twice, compare file sizes |

---

## Dependencies & Assumptions

### Dependencies

| Dependency | Version | Required For |
|------------|---------|--------------|
| Python 3 | >= 3.8 | All scripts |
| openpyxl | any | Steps 2, 5, verify (Excel checks) |
| xml.etree.ElementTree | stdlib | All XML processing |
| Completed build | any | Steps 3-5 (ODL cross-referencing, version lookup from `tmp/.packageinfo`) |
| Confluence credentials | - | Step 7 only |
| `upload_confluence_v2.py` | - | Step 7 only |

### Assumptions

1. The build staging directory follows the AMX convention: `staging_dir/target-*/root-*/etc/amx/<service>/`
2. ODL files use the Ambiorix v6.x syntax with `%define`, `select`, and `usp.translate` blocks
3. The BBF website serves TR-181 USP XML files at stable URLs matching the `tr-181-<M>-<m>-<p>-usp[-full].xml` pattern
4. The TR-181 XML follows the BBF CWMP data model schema version 1-15
5. Proxy object mappings use the `%global "proxy-object.'...'" = "...";` syntax
6. Variable substitutions use only the three variables defined in `config.VARIABLES`

---

## Appendix: Type Mapping Tables

### ODL Type → TR-181 Type Mapping

| ODL Type | TR-181 Type |
|----------|-------------|
| `string` | `string` |
| `cstring_t` | `string` |
| `bool` | `boolean` |
| `uint8` | `unsignedInt` |
| `uint16` | `unsignedInt` |
| `uint32` | `unsignedInt` |
| `uint64` | `unsignedLong` |
| `int8` | `int` |
| `int16` | `int` |
| `int32` | `int` |
| `int64` | `long` |
| `float` | `string` |
| `double` | `string` |
| `csv_string` | `string` |
| `ssv_string` | `string` |
| `datetime` | `dateTime` |
| `list` | `string` |
| `htable` | `string` |
| `variant` | `string` |

### ODL Modifiers

| Modifier | Effect | Report Column |
|----------|--------|---------------|
| `%persistent` | Value persists across reboots | ODL Persistent = yes |
| `%read-only` / `%read_only` | Parameter access is readOnly | ODL Access = readOnly |
| `%protected` | Not accessible to external clients (USP/web) | **Entry excluded from report** |
| `%private` | Not accessible to any remote client | **Entry excluded from report** |
| `%volatile` | Value recomputed on each read | ODL Volatile = yes |
| `%locked` | Cannot add parameters dynamically | (tracked internally) |
| `%variable` | Variable instance | (structural) |
| `%key` | Table key parameter | (structural) |
| `%unique` | Unique constraint | (structural) |
| `%template` | Template object | (structural) |
| `%instance` | Instance object | (structural) |

### BBF Template Markers Cleaned by `clean_description()`

| Input Pattern | Output |
|---------------|--------|
| `{{bibref\|XXX}}` | `[XXX]` |
| `{{object\|XXX}}` | `XXX` |
| `{{param\|XXX}}` | `XXX` |
| `{{enum\|XXX\|YYY}}` | `XXX` |
| `{{numentries}}` | `The number of entries in the corresponding table.` |
| `{{empty}}` | `<Empty>` |
| `{{true}}` / `{{false}}` | `true` / `false` |
| `{{deprecated\|...}}` | `DEPRECATED: ...` |
| `{{obsoleted\|...}}` | `OBSOLETED: ...` |
