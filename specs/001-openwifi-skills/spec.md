# Feature Specification: OpenWiFi Agents Skills Suite

**Feature Branch**: `001-openwifi-skills`  
**Created**: 2026-03-05  
**Status**: Draft  
**Input**: Adapt gw3 agents/skills to OpenWiFi project and develop three new build/upgrade skills

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Adapt Knowledge Base Management Skills (Priority: P1)

A developer working on the OpenWiFi project needs to manage a local knowledge
base of reference documents (vendor datasheets, standards PDFs, SDK guides).
They can invoke `/kb-add-file`, `/kb-add-batch`, `/kb-remove`, and
`/refs-download` within the OpenWiFi repository using the same workflow as the
gw3 project, with the knowledge base configured to reference OpenWiFi-specific
document categories.

**Why this priority**: Reference documentation management is foundational for
every other developer task. Without accessible KB skills, agents cannot
assist with hardware-specific development or standards compliance work.

**Independent Test**: A developer runs `/kb-add-file refs/openwifi-reference-docs
/tmp/sample.pdf vendor-docs` against the OpenWiFi repo and the document
appears correctly in `refs/openwifi-reference-docs/md/catalog.yaml` with a
matching `.md` and `.idx` file committed.

**Acceptance Scenarios**:

1. **Given** a PDF exists at `/tmp/doc.pdf`, **When** a developer invokes
   `/kb-add-file /tmp/doc.pdf`, **Then** the file is copied to `origin/`,
   converted to markdown, indexed as `.idx`, catalogued in `catalog.yaml`,
   and committed to the knowledge base repository.
2. **Given** a directory contains multiple PDFs, **When** a developer invokes
   `/kb-add-batch /tmp/docs/`, **Then** all PDFs are imported in parallel
   and committed in a single sequential git operation.
3. **Given** a document exists in the KB, **When** a developer invokes
   `/kb-remove <catalog-id>`, **Then** the `.md`, `.idx`, origin file, and
   catalog entry are all removed and the deletion is committed.
4. **Given** a `refs/refs.yaml` manifest exists, **When** a developer invokes
   `/refs-download`, **Then** all referenced external repositories are cloned
   or updated under `refs/` using sparse checkout for large repositories.

---

### User Story 2 - Adapt Project Collaboration Skills (Priority: P2)

An OpenWiFi developer needs to interact with Jira and Confluence (the project's
issue tracker and documentation platform) and with Bitbucket (source control)
directly from within the AI agent session. They can create issues, update
statuses, log work, upload Confluence pages, and manage pull requests without
leaving the development context.

**Why this priority**: Project collaboration tools are used daily for issue
tracking, documentation, and code review. Having first-class skill support
reduces context-switching overhead.

**Independent Test**: A developer runs `uv run scripts/core/jira-validate.py
--verbose` inside the OpenWiFi project and receives a successful connection
confirmation using credentials stored for the project's Jira instance.

**Acceptance Scenarios**:

1. **Given** valid Jira credentials are configured, **When** a developer
   mentions a Jira issue key (e.g., `OWIFI-123`) in the chat, **Then** the
   jira-communication skill auto-triggers and fetches the issue details.
2. **Given** formatted wiki markup content, **When** a developer invokes
   `/jira-syntax validate path/to/comment.txt`, **Then** the script reports
   any Jira markup errors with line references.
3. **Given** a markdown document exists, **When** a developer invokes the
   confluence skill to upload to a Confluence page ID, **Then** the content
   is uploaded via REST API and the page URL is returned.
4. **Given** `bkt` CLI is installed, **When** a developer invokes bkt commands
   for PR creation or branch management, **Then** the skill authenticates
   against the OpenWiFi Bitbucket instance and executes the requested
   operation.

---

### User Story 3 - Adapt WSL Environment Detection (Priority: P3)

A developer working on OpenWiFi on a Windows machine via WSL needs the AI
agent to automatically detect the environment before running build or SSH
commands that behave differently under WSL versus native Linux.

**Why this priority**: WSL detection is a low-complexity adaptation that
prevents incorrect command execution in mixed Windows/Linux environments.

**Independent Test**: A developer invokes `.github/skills/detect-wsl/detect-wsl.sh
--verbose` and the output correctly identifies whether the host is WSL1,
WSL2, or native Linux.

**Acceptance Scenarios**:

1. **Given** the shell is running in WSL2, **When** `detect-wsl.sh` is
   executed, **Then** the exit code is 0 and `--verbose` output includes
   `WSL_VERSION=2`.
2. **Given** the shell is running on native Linux, **When** `detect-wsl.sh`
   is executed, **Then** the exit code is 1.
3. **Given** `--json` flag is passed, **When** executed in any environment,
   **Then** output is valid JSON with `is_wsl`, `wsl_version`, and
   `detection_methods` fields.

---

### User Story 4 - Remote Server Build via SSH (Priority: P1)

A developer needs to compile OpenWiFi firmware on a dedicated build server
without setting up the full toolchain locally. They specify the target profile
and the agent connects to the build server, starts the Docker build environment,
and runs the build, streaming output and reporting success or failure.

**Why this priority**: Firmware compilation requires a large toolchain and
takes significant time. Centralizing builds on a dedicated server is the
standard workflow for the team.

**Independent Test**: A developer invokes `/server-build cig_wf188n` and the
agent SSHes to the build server, runs the full build sequence, and returns
the path of the generated firmware image.

**Acceptance Scenarios**:

1. **Given** the build server is reachable at 192.168.20.30, **When** a
   developer invokes `/server-build <profile>`, **Then** the agent SSHes
   to the server, runs `run_build_docker wf188_tip`, navigates to `openwrt/`,
   runs `./scripts/gen_config.py <profile>`, and then `make -j$(nproc)`.
2. **Given** the build succeeds, **When** the make command exits with code 0,
   **Then** the agent reports the firmware image path under `openwrt/bin/`.
3. **Given** the build fails, **When** make exits with a non-zero code,
   **Then** the agent captures and displays the last 50 lines of make output
   and reports the target and profile that failed.
4. **Given** no profile is specified, **When** the agent is invoked with
   `/server-build`, **Then** it lists all available profiles from the
   `profiles/` directory and prompts the developer to choose one.

---

### User Story 5 - Local Machine Build (Priority: P2)

A developer with the OpenWiFi build environment already set up locally wants to
compile firmware directly on their workstation without SSH. They specify the
profile and the agent runs the build sequence in the local `openwrt/`
directory.

**Why this priority**: Some developers run native Linux builds; local-build
provides the same guided experience as server-build without requiring a
remote connection.

**Independent Test**: A developer invokes `/local-build edgecore_eap104` from
the repository root and the agent runs `gen_config.py` and `make` in the local
`openwrt/` directory.

**Acceptance Scenarios**:

1. **Given** the `openwrt/` directory exists at the repository root, **When**
   a developer invokes `/local-build <profile>`, **Then** the agent runs
   `./scripts/gen_config.py <profile>` followed by `make -j$(nproc)` from
   within `openwrt/`.
2. **Given** the `openwrt/` directory does not exist, **When** a developer
   invokes `/local-build`, **Then** the agent displays an error message
   explaining that `./setup.py --setup` must be run first.
3. **Given** the build succeeds, **When** make exits with code 0, **Then**
   the agent reports the firmware image path under `openwrt/bin/`.
4. **Given** the build fails, **When** make exits with a non-zero code,
   **Then** the agent reports the error and suggests checking the build log.

---

### User Story 6 - DUT Firmware Upgrade via SSH (Priority: P1)

A developer needs to flash a newly built firmware image to a Device Under Test
(DUT) for validation. They specify the image path and the agent handles
transferring the file to the DUT and executing the upgrade command, monitoring
the process until the DUT reboots.

**Why this priority**: Flashing firmware to a DUT is the final step of every
build-test cycle. Automating this reduces manual error and speeds up iteration.

**Independent Test**: A developer invokes `/upgrade-dut
openwrt/bin/targets/ath79/generic/firmware.bin` and the agent completes the
SCP transfer to DUT `/tmp/` and issues `sysupgrade -n /tmp/firmware.bin`.

**Acceptance Scenarios**:

1. **Given** a firmware image path is provided and the DUT is reachable at
   192.168.1.1, **When** a developer invokes `/upgrade-dut <image-path>`,
   **Then** the agent SCPs the image to `/tmp/` on the DUT and issues
   `sysupgrade -n /tmp/<filename>`.
2. **Given** no image path is provided, **When** the agent is invoked with
   `/upgrade-dut`, **Then** it searches `openwrt/bin/` for firmware image
   files and presents a selection list to the developer.
3. **Given** the SCP transfer fails (network unreachable, disk full),
   **When** the transfer exits with an error, **Then** the agent reports
   the failure reason and does not attempt `sysupgrade`.
4. **Given** the DUT IP differs from the default, **When** a developer
   passes a custom IP as argument (e.g., `/upgrade-dut image.bin 192.168.2.1`),
   **Then** the agent uses the custom IP for SSH and SCP.
5. **Given** `sysupgrade` command is issued, **When** the DUT drops the SSH
   connection (expected during reboot), **Then** the agent reports "Upgrade
   command sent, DUT is rebooting" without treating the connection drop as an
   error.

---

### Edge Cases

- What happens when the build server is unreachable when `/server-build` is invoked?
  Agent must report "Build server unavailable" and offer the `/local-build` alternative.
- What happens when the profile name passed to `/server-build` or `/local-build`
  does not match any file in `profiles/`? Agent must list valid profile names
  and prompt for correction.
- What happens when the DUT already has a newer firmware version?
  `sysupgrade -n` overwrites unconditionally; agent should warn the developer
  before proceeding if the current DUT version can be detected.
- What happens when `refs/refs.yaml` does not exist in the repository?
  `/refs-download` must report the missing manifest and explain how to create one.
- What happens when KB tools are invoked but no `refs/<kb-name>/kb.yaml` exists?
  Skill must report the missing KB descriptor and offer to create it.

## Requirements *(mandatory)*

### Functional Requirements

#### Group A: Knowledge Base Management (adapted from gw3)

- **FR-001**: The `kb-add-file` skill MUST be deployable in the OpenWiFi
  repository under `.github/skills/kb-add-file/` and function identically
  to the gw3 version for any KB mounted under `refs/`.
- **FR-002**: The `kb-add-batch` skill MUST be deployable under
  `.github/skills/kb-add-batch/` and support parallel import of PDFs into
  any KB under `refs/`.
- **FR-003**: The `kb-remove` skill MUST be deployable under
  `.github/skills/kb-remove/` and remove all artifacts (origin, md, idx,
  catalog entry) for targeted documents.
- **FR-004**: The `refs-download` skill MUST be deployable and reference a
  `refs/refs.yaml` manifest populated for OpenWiFi reference repositories.
- **FR-005**: All KB skills MUST auto-detect the active KB when exactly one
  `refs/*/kb.yaml` exists, without requiring the developer to specify the
  KB name.

#### Group B: Project Collaboration (adapted from gw3)

- **FR-006**: The `jira-communication` skill MUST function against the
  OpenWiFi team's Jira instance using credentials stored via `jira-setup.py`.
- **FR-007**: The `jira-syntax` skill MUST be available unchanged; it has no
  project-specific dependencies.
- **FR-008**: The `confluence` skill MUST be adapted to point to the OpenWiFi
  team's Confluence space.
- **FR-009**: The `bkt` (Bitbucket CLI) skill MUST be available without
  modification; server URL is configured per-user via `bkt auth login`.
- **FR-010**: All collaboration skills MUST provide clear auth-failure messages
  with instructions to run the corresponding setup script.

#### Group C: Environment Detection (adapted from gw3)

- **FR-011**: The `detect-wsl` skill with its shell script MUST be copied
  to `.github/skills/detect-wsl/` without modification; it is fully
  platform-agnostic.

#### Group D: server-build (new)

- **FR-012**: The `server-build` skill MUST SSH to host 192.168.20.30 as user
  `ruanyaoyu` and execute `run_build_docker wf188_tip` before the build.
- **FR-013**: The `server-build` skill MUST accept a profile name as argument,
  validate it against `profiles/` in the repository, then run
  `./scripts/gen_config.py <profile>` and `make -j$(nproc)` inside the
  remote `openwrt/` directory.
- **FR-014**: The `server-build` skill MUST capture build output and report
  the path of the generated firmware image on success, or the last 50 lines
  of build log on failure.
- **FR-015**: The `server-build` skill MUST NOT store SSH passwords in files;
  credentials are passed via `sshpass` or prompted interactively each session.

#### Group E: local-build (new)

- **FR-016**: The `local-build` skill MUST validate that `openwrt/` exists
  before running any build command, and provide actionable guidance if absent.
- **FR-017**: The `local-build` skill MUST accept a profile name, validate
  it against `profiles/`, run `./scripts/gen_config.py <profile>`, and then
  `make -j$(nproc)` inside the local `openwrt/` directory.
- **FR-018**: The `local-build` skill MUST report firmware output path on
  success, and the build error summary on failure.

#### Group F: upgrade-dut (new)

- **FR-019**: The `upgrade-dut` skill MUST SCP the specified firmware image to
  the DUT at `/tmp/` before issuing any upgrade command.
- **FR-020**: The `upgrade-dut` skill MUST issue `sysupgrade -n /tmp/<filename>`
  on the DUT via SSH.
- **FR-021**: The `upgrade-dut` skill MUST accept an optional DUT IP parameter
  (default: 192.168.1.1) and use it for both SCP and SSH.
- **FR-022**: The `upgrade-dut` skill MUST auto-discover firmware images from
  `openwrt/bin/` when no image path argument is provided, and present a
  selection if multiple images exist.
- **FR-023**: The `upgrade-dut` skill MUST treat SSH connection drop after
  `sysupgrade` as a normal termination (not an error).

### Key Entities

- **Skill**: A directory under `.github/skills/<name>/` containing a `SKILL.md`
  (invocation instructions and workflow) and any supporting scripts; may also
  include a `spec.md` describing behavior.
- **Knowledge Base (KB)**: A git repository mounted under `refs/<kb-name>/`
  with a `kb.yaml` descriptor; contains `origin/` (source files) and `md/`
  (converted markdown + search indexes).
- **Profile**: A YAML file in `profiles/` that defines the hardware target and
  package selection for an OpenWiFi firmware build.
- **DUT (Device Under Test)**: An OpenWiFi access point reachable via SSH for
  firmware flashing and test validation.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: All 9 adapted skills (kb-add-file, kb-add-batch, kb-remove,
  refs-download, jira-communication, jira-syntax, confluence, bkt,
  detect-wsl) are present under `.github/skills/` in the OpenWiFi repository
  and pass their respective acceptance scenarios without modification errors.
- **SC-002**: A developer can add a reference PDF to the knowledge base using
  `/kb-add-file` in under 3 minutes, from invocation to committed artifact.
- **SC-003**: A developer can trigger a full remote firmware build for any
  target in `profiles/` using `/server-build <profile>` without any manual
  SSH steps.
- **SC-004**: A developer can trigger a local firmware build in under 30
  seconds of elapsed wall-clock time from invocation to `make` starting.
- **SC-005**: A developer can flash a DUT with a firmware image using
  `/upgrade-dut` and the DUT begins rebooting within 60 seconds of
  invocation, assuming a stable network connection.
- **SC-006**: All three new skills (server-build, local-build, upgrade-dut)
  handle the "build server unreachable", "bad profile name", "DUT
  unreachable", and "image not found" error cases with clear, actionable
  messages rather than raw shell error output.

## Assumptions

- The OpenWiFi team uses Jira, Confluence, and Bitbucket as its project collaboration suite; exact server URLs are configured per-user at first run.
- The dedicated build server at 192.168.20.30 is persistently available and already has the Docker build environment (`run_build_docker`) installed.
- The DUT default IP 192.168.1.1 is the standard factory management IP for all OpenWiFi access points; developers override it when needed.
- All skill files are placed under `.github/skills/` to be discoverable by the VS Code Copilot agent configuration.
- KB skills require `pymupdf4llm` installed on the machine where `/kb-add-file` or `/kb-add-batch` is invoked (same prerequisite as gw3).
- The `bkt` Bitbucket CLI skill requires no OpenWiFi-specific modifications; server URL and credentials are configured once per user.
- SSH password for the build server and DUT are not stored in the repository; they are either provided interactively or via user-level credential helpers.
