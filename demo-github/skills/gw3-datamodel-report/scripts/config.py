#!/usr/bin/env python3
"""
Shared configuration constants and helpers for the GW3 data model report pipeline.

All scripts in this skill import paths, namespaces, and helper functions from here
instead of hardcoding them. This makes the pipeline adaptable to future TR-181
versions, different build targets, and alternate directory layouts.
"""

import os
import re

# ---------------------------------------------------------------------------
# Project layout
# ---------------------------------------------------------------------------
# scripts/ lives at .github/skills/gw3-datamodel-report/scripts/
# PROJECT_ROOT is four levels up from here.
SCRIPTS_DIR = os.path.dirname(os.path.abspath(__file__))
SKILL_DIR = os.path.dirname(SCRIPTS_DIR)
PROJECT_ROOT = os.path.normpath(os.path.join(SCRIPTS_DIR, '..', '..', '..', '..'))

# ---------------------------------------------------------------------------
# Staging directory — where the cross-compiled AMX root lives after a build
# ---------------------------------------------------------------------------
STAGING_AMX = "staging_dir/target-aarch64_cortex-a55+neon-vfpv4_musl/root-ipq54xx/etc/amx"

# ---------------------------------------------------------------------------
# Build directory — where extracted/compiled package sources live
# ---------------------------------------------------------------------------
BUILD_DIR = "build_dir/target-aarch64_cortex-a55+neon-vfpv4_musl"

# ---------------------------------------------------------------------------
# Output directories
# ---------------------------------------------------------------------------
LOCAL_DATAMODEL_DIR = os.path.join(PROJECT_ROOT, "local", "datamodel")
SPECS_DATAMODEL_DIR = os.path.join(PROJECT_ROOT, "specs", "datamodel")

# ---------------------------------------------------------------------------
# Broadband Forum data-model repository
# ---------------------------------------------------------------------------
BBF_BASE_URL = "https://usp-data-models.broadband-forum.org/"

# ---------------------------------------------------------------------------
# GW3 XML namespace for coverage annotations
# ---------------------------------------------------------------------------
GW3_NAMESPACE = "urn:actiontec:gw3:datamodel-coverage-1-0"
GW3_NS_PREFIX = "gw3"

# ---------------------------------------------------------------------------
# Confluence target page
# ---------------------------------------------------------------------------
CONFLUENCE_PAGE_ID = "4187652158"

# ---------------------------------------------------------------------------
# ODL variable substitutions (used when parsing ODL files)
# ---------------------------------------------------------------------------
VARIABLES = {
    "global_vendor_prefix_": "X_PRPLWARE-COM_",
    "global_custom_vendor_prefix_": "X_ACTIONTEC-COM_",
    "prefix_": "",
}

# ---------------------------------------------------------------------------
# Standard XML namespaces found in BBF data-model files
# ---------------------------------------------------------------------------
BBF_NS = "{urn:broadband-forum-org:cwmp:datamodel-1-15}"
BBF_DMR_NS = "{urn:broadband-forum-org:cwmp:datamodel-report-1-0}"


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def get_version_from_filename(filename):
    """Extract a version tuple from a TR-181 XML filename.

    >>> get_version_from_filename('tr-181-2-20-1-usp.xml')
    (2, 20, 1)
    >>> get_version_from_filename('tr-181-2-19-0-usp-full.xml')
    (2, 19, 0)
    """
    m = re.search(r'tr-181-(\d+)-(\d+)-(\d+)', filename)
    if m:
        return tuple(int(x) for x in m.groups())
    return (0, 0, 0)


def get_version_string(filename):
    """Return a human-readable version string like '2.20.1'."""
    ver = get_version_from_filename(filename)
    return '.'.join(str(v) for v in ver)


def ensure_output_dir(dirpath=None):
    """Create an output directory if it does not exist.  Defaults to LOCAL_DATAMODEL_DIR."""
    target = dirpath or LOCAL_DATAMODEL_DIR
    os.makedirs(target, exist_ok=True)
    return target


def staging_amx_path():
    """Return the absolute path to the AMX staging directory."""
    return os.path.join(PROJECT_ROOT, STAGING_AMX)


def build_dir_path():
    """Return the absolute path to the build directory."""
    return os.path.join(PROJECT_ROOT, BUILD_DIR)
