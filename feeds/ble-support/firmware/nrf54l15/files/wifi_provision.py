#!/usr/bin/env python3
import asyncio
import dataclasses
import hmac
import json
import re
import subprocess
import math
import time
import zlib
import hashlib
import struct
import shutil
import os
import threading
from typing import Optional
from urllib.parse import urlparse

from dbus_fast.aio import MessageBus
from dbus_fast.service import ServiceInterface, method, dbus_property, signal
from dbus_fast.signature import Variant
from dbus_fast.constants import PropertyAccess, BusType

# --------------------------------------------------
# Constants
# --------------------------------------------------
BLUEZ = "org.bluez"
ADAPTER = "/org/bluez/hci0"

BASE_PATH = "/org/bluez/example"
APP_PATH = BASE_PATH + "/app"
ADV_PATH = BASE_PATH + "/advertisement0"

ATT_MTU = 247
ATT_HEADER = 3
CUSTOM_HEADER = 2
MAX_CHUNK = ATT_MTU - ATT_HEADER - CUSTOM_HEADER  # 242 bytes

SERVICE_UUID = "e063c3a0-0748-40be-935e-01ebdeafa814"
READ_UUID    = "e063c3a1-0748-40be-935e-01ebdeafa814"
WRITE_UUID   = "e063c3a2-0748-40be-935e-01ebdeafa814"
RESULT_UUID  = "e063c3a3-0748-40be-935e-01ebdeafa814"
STATUS_UUID  = "e063c3a4-0748-40be-935e-01ebdeafa814"
AUTH_UUID    = "e063c3a5-0748-40be-935e-01ebdeafa814"
MAC_UUID     = "e063c3a6-0748-40be-935e-01ebdeafa814"
UPGRADE_UUID = "e063c3a7-0748-40be-935e-01ebdeafa814"

# Numeric model ID broadcast in BLE adv (last 2 bytes, uint16 LE).
# ecAPP maps this to a friendly name in DeviceModel.displayName(forModelId:).
# Allocations: 1=pi7, 2=doorlock, 3=eap115.
MODEL_ID = 3

# Device status byte in BLE advertisement (byte 3 of manufacturer data).
# Values are mutually exclusive across config apply and firmware upgrade:
#   0  = idle
#   1  = config_applying   2  = config_success   3  = config_failed
#   10 = fw_downloading    11 = fw_verifying     12 = fw_installing
#   13 = fw_rebooting      14 = fw_success       15 = fw_failed
device_status = 0
# Backward-compat alias: Advertisement.ManufacturerData and existing
# WifiProvisionCharacteristic code still reference apply_status. During
# the transition we keep them pointing to the same int value; the
# helper _set_device_status (to be added in Task 2.3) will keep both
# names in sync.
apply_status = device_status

# Status range classification (see Requirement 9.3).
CONFIG_STATUSES = {1, 2, 3}
UPGRADE_ACTIVE_STATUSES = {10, 11, 12, 13}
UPGRADE_TERMINAL_STATUSES = {14, 15}

# Serialize all device_status transitions so config-apply and
# firmware-upgrade paths cannot overlap (Requirement 9.3). Initialised
# lazily at first use so it is bound to the running asyncio loop.
device_status_lock: "asyncio.Lock | None" = None

# Module-level singleton for the firmware upgrade handler. Set in
# main() once the A7 characteristic is instantiated. Referenced by
# WifiProvisionCharacteristic.WriteValue when a cmd=="upgrade" payload
# arrives.
firmware_handler: "FirmwareUpgradeHandler | None" = None

def _set_device_status(v: int) -> None:
    """Atomically update device_status and its backward-compat alias.

    Must be called under `async with device_status_lock`.
    """
    global device_status, apply_status
    device_status = v
    apply_status = v

def get_device_mac():
    """Read device MAC from eth0 (lowercase, no separators, e.g. '94ef97d938d0')."""
    try:
        with open("/sys/class/net/eth0/address") as f:
            return f.read().strip().replace(":", "").lower()
    except Exception:
        return ""


# BLE application-layer auth key — sourced from uci so it can be
# provisioned per-device (planned: ucentral config). The default
# 147258 is bootstrapped into /etc/config/ble_provision on first run
# so it is discoverable via `uci show`.
BLE_KEY_UCI = "ble_provision.config.auth_key"
BLE_KEY_CONFIG_FILE = "/etc/config/ble_provision"
DEFAULT_BLE_AUTH_KEY = "147258"


def get_ble_auth_key():
    """Return the current BLE auth key.

    Reads `uci get ble_provision.config.auth_key` on every call so
    `uci set ... && uci commit ble_provision` takes effect without a
    daemon restart. Falls back to DEFAULT_BLE_AUTH_KEY and, on first
    run, writes the default into /etc/config/ble_provision so the
    value is visible to admins.
    """
    try:
        key = subprocess.check_output(
            ["uci", "get", BLE_KEY_UCI],
            text=True, stderr=subprocess.DEVNULL
        ).strip()
        if key:
            return key
    except subprocess.CalledProcessError:
        pass
    if not os.path.exists(BLE_KEY_CONFIG_FILE):
        try:
            with open(BLE_KEY_CONFIG_FILE, "w") as f:
                f.write(
                    "config provision 'config'\n"
                    f"\toption auth_key '{DEFAULT_BLE_AUTH_KEY}'\n"
                )
        except Exception as e:
            print(f"[BLE] Failed to bootstrap {BLE_KEY_CONFIG_FILE}: {e}",
                  flush=True)
    return DEFAULT_BLE_AUTH_KEY


def get_device_name():
    try:
        return subprocess.check_output(
            ["uci", "get", "system.@system[0].hostname"], text=True
        ).strip()
    except Exception:
        return "EAP115"


def get_uplink_info():
    """Returns (link_status, ip_bytes). link: 0=down, 2=connected.
    Uses a cache to avoid blocking the event loop during network restarts."""
    global _uplink_cache, _uplink_cache_time
    # Return cache if fresh (< 5 seconds)
    now = time.time()
    if now - _uplink_cache_time < 5:
        return _uplink_cache
    try:
        dump = json.loads(run(["ubus", "call", "network.interface", "dump"]))
        for iface in dump.get("interface", []):
            name = iface.get("interface", "")
            if name.startswith("up") and name != "up_none":
                up = iface.get("up", False)
                addrs = iface.get("ipv4-address", [])
                ip = addrs[0].get("address", "") if addrs else ""
                link = 2 if (up and ip) else 0
                ip_bytes = [int(x) for x in ip.split(".")] if ip and "." in ip else [0, 0, 0, 0]
                _uplink_cache = (link, ip_bytes)
                _uplink_cache_time = now
                return _uplink_cache
    except Exception:
        pass
    return _uplink_cache

_uplink_cache = (0, [0, 0, 0, 0])
_uplink_cache_time = 0

WIRELESS_CFG = "/etc/config/wireless"
WIRELESS_BAK = "/tmp/wireless.ble.bak"


# --------------------------------------------------
# Helpers
# --------------------------------------------------
def run(cmd, timeout=3):
    return subprocess.check_output(cmd, text=True, timeout=timeout)


def wifi_is_up(timeout=15):
    end = time.time() + timeout
    while time.time() < end:
        try:
            status = json.loads(run(["ubus", "call", "network.wireless", "status"]))
            for radio in status.values():
                if radio.get("up"):
                    return True
                for iface in radio.get("interfaces", []):
                    if iface.get("up") or iface.get("associated"):
                        return True
        except Exception:
            pass
        time.sleep(1)
    return False


def backup_wireless():
    shutil.copy(WIRELESS_CFG, WIRELESS_BAK)


def rollback_wireless():
    if os.path.exists(WIRELESS_BAK):
        shutil.copy(WIRELESS_BAK, WIRELESS_CFG)
        subprocess.call(["wifi", "reload"])


# --------------------------------------------------
# Device info
# --------------------------------------------------
def get_device_info():
    info = {}
    try:
        board = json.loads(run(["ubus", "call", "system", "board"]))
        info["model"] = board.get("model", "")
        info["ver"] = board.get("release", {}).get("version", "")
    except Exception:
        pass

    # EAP115 prints the eth0 MAC as the device serial number on the
    # label; expose it here so the BLE status payload carries both
    # serial and a colon-formatted MAC (ucentral.state only keeps
    # these at the top level, which get_state() drops).
    mac_nosep = get_device_mac()
    if mac_nosep and len(mac_nosep) == 12:
        info["serial"] = mac_nosep
        info["mac"] = ":".join(
            mac_nosep[i:i + 2] for i in range(0, 12, 2)
        ).upper()
    return info


# --------------------------------------------------
# Wi‑Fi config (Read) — prefer uCentral active config
# --------------------------------------------------
def get_health():
    """Read uCentral health data."""
    try:
        with open("/tmp/ucentral.health", "r") as f:
            return json.loads(f.read())
    except Exception:
        return {}


def get_state():
    """Read uCentral state (statistics) data from file."""
    state_file = "/tmp/ucentral.state"
    try:
        with open(state_file, "r") as f:
            return json.loads(f.read()).get("state", {})
    except Exception:
        return {}


def get_wifi_config():
    """Read uCentral active config (without telemetry)."""
    data = {}

    # Try uCentral active config first
    try:
        with open("/etc/ucentral/ucentral.active", "r") as f:
            data = json.loads(f.read())
    except Exception:
        pass

    if not data.get("interfaces"):
        # No uCentral config, fallback to UCI topology
        return get_wifi_topology()

    return data


def get_current_mode() -> str:
    """Return 'standalone' | 'cloud' | 'unknown'.

    Source of truth is uci `ble_provision.config.mode`. If absent
    (factory-fresh device) we infer from whether the ucentral daemon
    is currently running.
    """
    try:
        m = subprocess.check_output(
            ["uci", "get", "ble_provision.config.mode"],
            text=True, stderr=subprocess.DEVNULL
        ).strip()
        if m in ("standalone", "cloud"):
            return m
    except subprocess.CalledProcessError:
        pass
    try:
        result = subprocess.run(
            ["pidof", "ucentral"], capture_output=True, timeout=2
        )
        return "cloud" if result.returncode == 0 else "standalone"
    except Exception:
        return "unknown"


def get_device_status():
    """Read info + health + state for dashboard display."""
    return {
        "info": get_device_info(),
        "health": get_health(),
        "state": get_state(),
        "mode": get_current_mode(),
    }


def get_wifi_topology():
    data = {"radios": []}
    try:
        status = json.loads(
            subprocess.check_output(
                ["ubus", "call", "network.wireless", "status"],
                text=True
            )
        )
    except Exception:
        return data

    for radio_name, radio in status.items():
        r = {
            "radio": radio_name,
            "band": radio.get("config", {}).get("htmode"),
            "channel": radio.get("config", {}).get("channel"),
            "interfaces": []
        }

        for iface in radio.get("interfaces", {}):
            r["interfaces"].append({
                "section": iface.get("section"),
                "ifname": iface.get("ifname"),
                "ssid": iface.get("config", {}).get("ssid"),
                "mode": iface.get("config", {}).get("mode"),
                "encryption": iface.get("config", {}).get("encryption"),
                "key": iface.get("config", {}).get("key", ""),
                "disabled": iface.get("config", {}).get("disabled", False),
                "up": iface.get("up"),
                "associated": iface.get("associated"),
            })

        data["radios"].append(r)

    return data


# --------------------------------------------------
# ZIP + tail(raw_len + raw_md5) + fragment
# --------------------------------------------------
def zip_and_fragment(data: dict):
    raw = json.dumps(data, separators=(",", ":")).encode("utf-8")
    raw_len = len(raw)
    raw_md5 = hashlib.md5(raw).digest()

    compressed = zlib.compress(raw, level=9)

    payload = compressed + struct.pack(">I", raw_len) + raw_md5

    total = math.ceil(len(payload) / MAX_CHUNK)

    fragments = []

    for idx in range(total):
        start = idx * MAX_CHUNK
        end = start + MAX_CHUNK
        fragments.append(
            bytes([total, idx]) + payload[start:end]
        )

    return fragments


# --------------------------------------------------
# Object Manager / GATT
# --------------------------------------------------
class ObjectManager(ServiceInterface):
    def __init__(self, managed):
        super().__init__("org.freedesktop.DBus.ObjectManager")
        self.managed = managed

    @method()
    def GetManagedObjects(self) -> "a{oa{sa{sv}}}":
        return self.managed


class GattService(ServiceInterface):
    def __init__(self):
        self.path = f"{APP_PATH}/service0"
        super().__init__("org.bluez.GattService1")

    @dbus_property(access=PropertyAccess.READ)
    def UUID(self) -> "s":
        return SERVICE_UUID

    @dbus_property(access=PropertyAccess.READ)
    def Primary(self) -> "b":
        return True

    @dbus_property(access=PropertyAccess.READ)
    def Characteristics(self) -> "ao":
        return [
            f"{self.path}/char0",
            f"{self.path}/char1",
            f"{self.path}/char2",
            f"{self.path}/char3",
            f"{self.path}/char4",
            f"{self.path}/char5",
            f"{self.path}/char6",
        ]


# --------------------------------------------------
# Read Characteristic
# --------------------------------------------------
class WifiReadCharacteristic(ServiceInterface):
    def __init__(self, service, auth_char):
        self.path = f"{service.path}/char0"
        super().__init__("org.bluez.GattCharacteristic1")
        self._frags = []
        self._idx = 0
        self.auth = auth_char

    @dbus_property(access=PropertyAccess.READ)
    def UUID(self) -> "s":
        return READ_UUID

    @dbus_property(access=PropertyAccess.READ)
    def Service(self) -> "o":
        return f"{APP_PATH}/service0"

    @dbus_property(access=PropertyAccess.READ)
    def Flags(self) -> "as":
        return ["read"]

    @method()
    def ReadValue(self, options: "a{sv}") -> "ay":
        if not self.auth.authenticated:
            # Return minimal bytes so client knows read is blocked
            return b'\x00'
        if not self._frags:
            self._frags = zip_and_fragment(get_wifi_config())
            self._idx = 0

        frag = self._frags[self._idx]
        self._idx += 1
        if self._idx >= len(self._frags):
            self._frags = []
            self._idx = 0

        return frag

    @method()
    def StartNotify(self):
        pass

    @method()
    def StopNotify(self):
        pass


# --------------------------------------------------
# Provisioning Result (Read + Notify)
# --------------------------------------------------
class ProvisionResultCharacteristic(ServiceInterface):
    def __init__(self, service):
        self.path = f"{service.path}/char2"
        super().__init__("org.bluez.GattCharacteristic1")
        self._value = b'{"status":"idle"}'

    @dbus_property(access=PropertyAccess.READ)
    def UUID(self) -> "s":
        return RESULT_UUID

    @dbus_property(access=PropertyAccess.READ)
    def Service(self) -> "o":
        return f"{APP_PATH}/service0"

    @dbus_property(access=PropertyAccess.READ)
    def Flags(self) -> "as":
        return ["read", "notify"]

    @method()
    def ReadValue(self, options: "a{sv}") -> "ay":
        return bytes(self._value)

    def update(self, payload: dict):
        self._value = json.dumps(payload).encode()
        try:
            self.emit_properties_changed(
                {"Value": Variant("ay", bytes(self._value))}
            )
        except Exception as e:
            print(f"[BLE] Notify emit FAILED: {e}", flush=True)


# --------------------------------------------------
# Status Read (info + health + state for dashboard)
# --------------------------------------------------
class StatusReadCharacteristic(ServiceInterface):
    def __init__(self, service, auth_char):
        self.path = f"{service.path}/char3"
        super().__init__("org.bluez.GattCharacteristic1")
        self._frags = []
        self._idx = 0
        self.auth = auth_char

    @dbus_property(access=PropertyAccess.READ)
    def UUID(self) -> "s":
        return STATUS_UUID

    @dbus_property(access=PropertyAccess.READ)
    def Service(self) -> "o":
        return f"{APP_PATH}/service0"

    @dbus_property(access=PropertyAccess.READ)
    def Flags(self) -> "as":
        return ["read"]

    @method()
    def ReadValue(self, options: "a{sv}") -> "ay":
        if not self.auth.authenticated:
            return b'\x00'
        if not self._frags:
            self._frags = zip_and_fragment(get_device_status())
            self._idx = 0

        frag = self._frags[self._idx]
        self._idx += 1
        if self._idx >= len(self._frags):
            self._frags = []
            self._idx = 0

        return frag

    @method()
    def StartNotify(self):
        pass

    @method()
    def StopNotify(self):
        pass


# --------------------------------------------------
# Provisioning Write
# --------------------------------------------------
class WifiProvisionCharacteristic(ServiceInterface):
    def __init__(self, service, result_char, auth_char):
        self.path = f"{service.path}/char1"
        super().__init__("org.bluez.GattCharacteristic1")
        self._chunks = {}
        self._total = None
        self.result = result_char
        self.auth = auth_char

    @dbus_property(access=PropertyAccess.READ)
    def UUID(self) -> "s":
        return WRITE_UUID

    @dbus_property(access=PropertyAccess.READ)
    def Service(self) -> "o":
        return f"{APP_PATH}/service0"

    @dbus_property(access=PropertyAccess.READ)
    def Flags(self) -> "as":
        return ["write"]

    @method()
    def StartNotify(self):
        pass

    @method()
    def StopNotify(self):
        pass

    @method()
    def WriteValue(self, value: "ay", options: "a{sv}"):
        data = bytes(value)

        # JSON commands (upgrade, upgrade_ack) are sent as plain UTF-8
        # — detect by leading '{'. Everything else goes through the
        # existing chunked/compressed config-apply path.
        if data and data[0:1] == b"{":
            try:
                obj = json.loads(data.decode("utf-8"))
            except Exception:
                obj = {}
            cmd_type = obj.get("cmd", "")

            # upgrade_ack is allowed without auth — it only clears a
            # stale advertisement status and poses no security risk.
            if cmd_type == "upgrade_ack":
                if firmware_handler is None:
                    return
                try:
                    asyncio.get_event_loop().create_task(
                        firmware_handler.on_ack()
                    )
                except Exception as e:
                    print(f"[BLE] Dispatch upgrade_ack failed: {e}", flush=True)
                return

            # All other JSON commands require auth.
            if not self.auth.authenticated:
                print("[BLE] Write rejected: not authenticated", flush=True)
                self.result.update({"status": "failed", "reason": "not_authenticated"})
                return

            # Device-action commands (reboot / factory_reset / set_mode)
            # — handled inline, do not go through firmware_handler.
            if cmd_type in ("reboot", "factory_reset", "set_mode"):
                try:
                    asyncio.get_event_loop().create_task(
                        self._handle_device_action(obj)
                    )
                except Exception as e:
                    print(f"[BLE] Dispatch device action {cmd_type} failed: {e}",
                          flush=True)
                return

            if firmware_handler is None:
                print("[BLE] Upgrade cmd arrived but handler not ready",
                      flush=True)
                return
            try:
                asyncio.get_event_loop().create_task(
                    firmware_handler.on_command(obj)
                )
            except Exception as e:
                print(f"[BLE] Dispatch upgrade cmd failed: {e}", flush=True)
            return

        # Chunked config-apply path requires auth.
        if not self.auth.authenticated:
            print("[BLE] Write rejected: not authenticated", flush=True)
            self.result.update({"status": "failed", "reason": "not_authenticated"})
            return

        total, idx = data[0], data[1]
        self._chunks[idx] = data[2:]
        self._total = total

        if len(self._chunks) != self._total:
            return

        try:
            full = b"".join(self._chunks[i] for i in range(total))
            raw_len = struct.unpack(">I", full[-20:-16])[0]
            raw_md5 = full[-16:]
            raw = zlib.decompress(full[:-20])

            if len(raw) != raw_len or hashlib.md5(raw).digest() != raw_md5:
                raise ValueError("verify_failed")

            config = json.loads(raw.decode())
            print(f"[BLE] Received config ({len(raw)} bytes)", flush=True)

            # Schedule apply on event loop so WriteValue returns immediately.
            # Status transition + A3 notify are sequenced together inside
            # _async_apply under the shared device_status_lock.
            self.result.update({"status": "applying"})
            asyncio.get_event_loop().create_task(self._async_apply(config))

        except Exception as e:
            print(f"[BLE] WriteValue exception: {e}", flush=True)
            # Failure during parse/decompress: we have not taken the
            # device_status lock yet so we cannot touch it here; report via
            # A3 only and leave device_status unchanged.
            self.result.update({"status": "failed", "reason": str(e)})

    async def _async_apply(self, config):
        """Run blocking apply in executor, then notify result."""
        loop = asyncio.get_event_loop()
        assert device_status_lock is not None, (
            "device_status_lock must be initialised in main()"
        )
        # Serialize with any firmware-upgrade task that may be running.
        async with device_status_lock:
            if device_status in UPGRADE_ACTIVE_STATUSES:
                # Busy: an upgrade is in progress. Reject this config
                # apply without mutating device_status (Requirement 9.2).
                self.result.update({"status": "failed",
                                    "reason": "busy: upgrading"})
                return
            _set_device_status(1)  # config_applying
        try:
            result = await loop.run_in_executor(None, self._do_apply, config)
            async with device_status_lock:
                _set_device_status(2 if result.get("status") == "success" else 3)
            self.result.update(result)
        except Exception as e:
            print(f"[BLE] Async apply error: {e}", flush=True)
            async with device_status_lock:
                _set_device_status(3)  # config_failed
            self.result.update({"status": "failed", "reason": str(e)})
        # Reset device_status to idle after 10 seconds (only if still
        # in a config terminal state; an upgrade may have started in the
        # meantime, and we must not stomp over its status).
        await asyncio.sleep(10)
        async with device_status_lock:
            if device_status in (2, 3):
                _set_device_status(0)

    async def _handle_device_action(self, obj):
        """Dispatch reboot / factory_reset / set_mode device actions.

        All three already passed auth at the WriteValue gate. Results
        go out via the existing A3 (RESULT) notify channel.
        """
        cmd = obj.get("cmd", "")

        if cmd == "reboot":
            # TODO: enable real reboot once QA flow + recovery procedure
            # are agreed. For now log + reply so the ecAPP UI path can
            # be verified without dropping the BLE connection.
            #print("[BLE] Action: reboot (STUBBED — not rebooting)",
            #      flush=True)
            print("[BLE] Action: reboot ...", flush=True)
            self.result.update({"status": "rebooting"})
            await asyncio.sleep(3)
            subprocess.Popen(["reboot"])
            return

        if cmd == "factory_reset":
            # TODO: enable real factory_reset once recovery procedure
            # is in place. `jffs2reset -y` would wipe /overlay,
            # erasing /root/wifi_conf.py and /etc/config/ble_provision,
            # leaving the device with no BLE provisioning daemon.
            # print("[BLE] Action: factory_reset (STUBBED — not wiping)",
            #      flush=True)
            print("[BLE] Action: factory_reset ...", flush=True)
            self.result.update({"status": "factory_resetting"})
            await asyncio.sleep(3)
            subprocess.Popen("jffs2reset -y && reboot", shell=True)
            return

        if cmd == "set_mode":
            mode = obj.get("mode", "")
            if mode not in ("standalone", "cloud"):
                self.result.update({"status": "failed",
                                    "reason": f"invalid mode: {mode}"})
                return

            # Persist the mode flag in UCI
            try:
                subprocess.run(
                    ["uci", "set",
                     f"ble_provision.config.mode={mode}"],
                    check=True, stderr=subprocess.DEVNULL
                )
                subprocess.run(
                    ["uci", "commit", "ble_provision"], check=True
                )
            except Exception as e:
                self.result.update({"status": "failed", "reason": str(e)})
                return

            if mode == "standalone":
                # Stops ucentral and persists the standalone flag.
                # Deferred via call_later so the BLE notification flushes
                # before the script tears down services.
                print("[BLE] Action: set_mode=standalone (enable_standalone)", flush=True)
                self.result.update({"status": "success", "mode": mode})
                asyncio.get_event_loop().call_later(
                    1,
                    lambda: subprocess.Popen(["/usr/sbin/enable_standalone.sh"]),
                )
            else:
                # Restores ucentral and reboots the AP. Deferred so the
                # BLE app receives the notification before the reboot
                # tears down the BLE link.
                print("[BLE] Action: set_mode=cloud (disable_standalone, will reboot)", flush=True)
                self.result.update({
                    "status": "success",
                    "mode": mode,
                    "message": "AP will be rebooted now. Please do not disconnect power",
                })
                asyncio.get_event_loop().call_later(
                    3,
                    lambda: subprocess.Popen(["/usr/sbin/disable_standalone.sh"]),
                )
            return

        self.result.update({"status": "failed",
                            "reason": f"unknown action: {cmd}"})

    def _is_ucentral_format(self, config):
        """Detect uCentral config format (has top-level 'interfaces' with 'ssids' inside)."""
        if "interfaces" in config and isinstance(config["interfaces"], list):
            for iface in config["interfaces"]:
                if "ssids" in iface:
                    return True
        return False

    def _do_apply(self, config):
        """Blocking apply work — runs in thread pool."""
        print("[BLE] Applying config...", flush=True)

        if self._is_ucentral_format(config):
            return self._apply_ucentral(config)
        else:
            return self._apply_uci(config)

    def _apply_ucentral(self, config):
        """Apply uCentral format config directly using ucentral.uc script.
        This works in standalone mode without the ucentral daemon running."""
        try:
            # Generate a uuid based on timestamp
            uuid = int(time.time())
            config["uuid"] = uuid
            cfg_path = f"/etc/ucentral/ucentral.cfg.{uuid:010d}"

            print(f"[BLE] uCentral format detected, writing to {cfg_path}", flush=True)
            with open(cfg_path, "w") as f:
                json.dump(config, f, separators=(",", ":"))

            # Apply config directly via ucentral.uc script (no daemon needed)
            print("[BLE] Applying config via ucentral.uc...", flush=True)
            result = subprocess.run(
                ["/usr/bin/ucode", "/usr/share/ucentral/ucentral.uc", cfg_path],
                capture_output=True, text=True, timeout=60
            )

            # Check 1: exit code from ucentral.uc
            #   0 = success, 1 = partial (rejects), >1 = fatal failure.
            # The script returns 0 for both clean and "applied with rejects"
            # cases, so a non-zero rc is always a hard failure.
            if result.returncode != 0:
                print(f"[BLE] ucentral.uc exited with code {result.returncode}", flush=True)
                if result.stderr:
                    print(f"[BLE] stderr: {result.stderr}", flush=True)
                return {"status": "failed", "reason": f"ucentral.uc exit code {result.returncode}",
                        "log": result.stdout[-200:] if result.stdout else ""}

            # Check 2: ucentral.active symlink must point at our new file.
            # ucentral.uc only updates the symlink at the end of a successful
            # apply, so this confirms the config was committed (not just that
            # the script exited cleanly with all rules rejected).
            active_link = "/etc/ucentral/ucentral.active"
            try:
                link_target = os.readlink(active_link)
            except OSError as e:
                print(f"[BLE] active symlink unreadable: {e}", flush=True)
                return {"status": "failed",
                        "reason": f"active symlink missing: {e}"}

            # readlink may return either an absolute path or just the basename
            # depending on how ucentral.uc created it — compare basenames.
            if os.path.basename(link_target) != os.path.basename(cfg_path):
                print(f"[BLE] active symlink mismatch: expected {cfg_path}, "
                      f"got {link_target}", flush=True)
                return {"status": "failed",
                        "reason": "active symlink not updated",
                        "expected": os.path.basename(cfg_path),
                        "got": os.path.basename(link_target)}

            print(f"[BLE] Config applied successfully (uuid={uuid})", flush=True)
            return {"status": "success", "uuid": uuid}

        except subprocess.TimeoutExpired:
            print("[BLE] ucentral.uc timed out", flush=True)
            return {"status": "failed", "reason": "apply timeout (60s)"}
        except Exception as e:
            print(f"[BLE] uCentral apply failed: {e}", flush=True)
            return {"status": "failed", "reason": str(e)}

    def _apply_uci(self, config):
        """Apply UCI topology format config via uci set commands."""
        try:
            backup_wireless()

            uci_count = 0
            for r in config.get("radios", []):
                radio_name = r.get("radio", "?")
                interfaces = r.get("interfaces", [])
                print(f"[BLE] Radio: {radio_name}, interfaces: {len(interfaces)}", flush=True)
                for i in interfaces:
                    sec = i.get("section", "")
                    if not sec:
                        print(f"[BLE]   Skipping interface (no section): {i}", flush=True)
                        continue
                    for k in ("ssid", "encryption", "key"):
                        if k in i:
                            cmd = f"wireless.{sec}.{k}={i[k]}"
                            print(f"[BLE]   uci set {cmd}", flush=True)
                            subprocess.check_call(["uci", "set", cmd])
                            uci_count += 1

            print(f"[BLE] {uci_count} uci set commands executed", flush=True)

            if uci_count == 0:
                print("[BLE] WARNING: No uci changes made", flush=True)
                return {"status": "failed", "reason": "no changes to apply"}

            subprocess.check_call(["uci", "commit", "wireless"])
            print("[BLE] Config committed to flash", flush=True)

            print("[BLE] Running wifi reload...", flush=True)
            subprocess.call(["wifi", "reload"])

            print("[BLE] Config applied successfully", flush=True)
            return {"status": "success"}

        except Exception as e:
            rollback_wireless()
            print(f"[BLE] Config failed: {e}", flush=True)
            return {"status": "failed", "reason": str(e)}


# --------------------------------------------------
# Advertisement ✅ 加回
# --------------------------------------------------
class Advertisement(ServiceInterface):
    def __init__(self):
        self.path = ADV_PATH
        super().__init__("org.bluez.LEAdvertisement1")

    @dbus_property(access=PropertyAccess.READ)
    def Type(self) -> "s":
        return "peripheral"

    @dbus_property(access=PropertyAccess.READ)
    def LocalName(self) -> "s":
        return get_device_name()

    @dbus_property(access=PropertyAccess.READ)
    def ServiceUUIDs(self) -> "as":
        return [SERVICE_UUID]

    @dbus_property(access=PropertyAccess.READ)
    def ManufacturerData(self) -> "a{qv}":
        link, _ip = get_uplink_info()
        # Layout: [link, apply_status, model_id_lo, model_id_hi].
        # IP was dropped (privacy: leaked topology to passive scanners);
        # ecAPP reads it from GATT status after connect instead. The last
        # 2 bytes are MODEL_ID (uint16 LE) which ecAPP uses to label the
        # device in the scan list. apply_status (byte2) tracks both config
        # apply (1-3) and firmware upgrade (10-15) -- see _set_device_status.
        data = bytes([link, apply_status, MODEL_ID & 0xFF, (MODEL_ID >> 8) & 0xFF])
        return {0xFFFF: Variant("ay", data)}

    @method()
    def Release(self):
        pass


# --------------------------------------------------
# Auth Characteristic (write-only, application-layer authentication)
# --------------------------------------------------
class AuthCharacteristic(ServiceInterface):
    def __init__(self, service):
        self.path = f"{service.path}/char4"
        super().__init__("org.bluez.GattCharacteristic1")
        self.authenticated = False

    @dbus_property(access=PropertyAccess.READ)
    def UUID(self) -> "s":
        return AUTH_UUID

    @dbus_property(access=PropertyAccess.READ)
    def Service(self) -> "o":
        return f"{APP_PATH}/service0"

    @dbus_property(access=PropertyAccess.READ)
    def Flags(self) -> "as":
        return ["write"]

    @method()
    def WriteValue(self, value: "ay", options: "a{sv}"):
        key = bytes(value).decode("utf-8", errors="ignore")
        if key == get_ble_auth_key():
            self.authenticated = True
            print("[BLE] Auth: OK", flush=True)
        else:
            self.authenticated = False
            print("[BLE] Auth: FAILED", flush=True)

    @method()
    def StartNotify(self):
        pass

    @method()
    def StopNotify(self):
        pass


# --------------------------------------------------
# MAC Characteristic (read-only, public — used for auth key derivation)
# --------------------------------------------------
class MacCharacteristic(ServiceInterface):
    def __init__(self, service):
        self.path = f"{service.path}/char5"
        super().__init__("org.bluez.GattCharacteristic1")
        self._mac = get_device_mac().encode()

    @dbus_property(access=PropertyAccess.READ)
    def UUID(self) -> "s":
        return MAC_UUID

    @dbus_property(access=PropertyAccess.READ)
    def Service(self) -> "o":
        return f"{APP_PATH}/service0"

    @dbus_property(access=PropertyAccess.READ)
    def Flags(self) -> "as":
        return ["read"]

    @method()
    def ReadValue(self, options: "a{sv}") -> "ay":
        return bytes(self._mac)

    @method()
    def StartNotify(self):
        pass

    @method()
    def StopNotify(self):
        pass


# --------------------------------------------------
# Upgrade Status Characteristic (read + notify)
# --------------------------------------------------
class UpgradeStatusCharacteristic(ServiceInterface):
    """A7 characteristic (char6) for firmware upgrade status reporting.

    Payload is a UTF-8 JSON dict: {"state": <Upgrade_State>, "message": <str>}.
    Valid states: idle | downloading | verifying | installing |
                  rebooting | success | failed.
    Failed state MUST carry a non-empty message (Property 12).
    """

    def __init__(self, service):
        self.path = f"{service.path}/char6"
        super().__init__("org.bluez.GattCharacteristic1")
        self._value = b'{"state":"idle","message":""}'

    @dbus_property(access=PropertyAccess.READ)
    def UUID(self) -> "s":
        return UPGRADE_UUID

    @dbus_property(access=PropertyAccess.READ)
    def Service(self) -> "o":
        return f"{APP_PATH}/service0"

    @dbus_property(access=PropertyAccess.READ)
    def Flags(self) -> "as":
        return ["read", "notify"]

    @method()
    def ReadValue(self, options: "a{sv}") -> "ay":
        return bytes(self._value)

    def update(self, state: str, message: str = ""):
        """Persist new state to _value and notify subscribers.

        Callers must supply a non-empty message when state == "failed"
        (see Property 12). This method does not enforce that invariant;
        FirmwareUpgradeHandler is the sole writer and is responsible.
        """
        self._value = json.dumps({"state": state, "message": message}).encode()
        print(f"[BLE] A7 update: state={state} message={message!r}", flush=True)
        try:
            self.emit_properties_changed(
                {"Value": Variant("ay", bytes(self._value))}
            )
            print(f"[BLE] A7 notify sent OK", flush=True)
        except Exception as e:
            print(f"[BLE] A7 notify FAILED: {e}", flush=True)

    @method()
    def StartNotify(self):
        pass

    @method()
    def StopNotify(self):
        pass


# --------------------------------------------------
# Firmware Upgrade Handler
# --------------------------------------------------
_MD5_RE = re.compile(r"^[0-9a-fA-F]{32}$")


class InvalidUpgradeCommand(Exception):
    """Raised when an upgrade command JSON fails validation."""


class UpgradeError(Exception):
    """Raised by upgrade execution paths to signal a user-visible
    failure message; carried through to A7 `failed` state."""

    def __init__(self, message: str):
        super().__init__(message)
        self.message = message


@dataclasses.dataclass
class UpgradeCommand:
    url: str
    md5: Optional[str]
    manual: bool


# Simulate-mode timing table (seconds). See Requirement 13.4.
SIMULATE_TIMINGS = {
    "downloading": 3,
    "verifying":   1,
    "installing":  3,
    "rebooting":   2,
}

# Auto-reset idle delay after upgrade success (all modes).
UPGRADE_SUCCESS_RESET_SECONDS = 300  # 5 minutes

# Simulate-mode uses a shorter reset for faster test cycles.
SIMULATE_IDLE_RESET_SECONDS = 30


class FirmwareUpgradeHandler:
    """Owns the firmware-upgrade state machine, writes to the
    UpgradeStatusCharacteristic (A7), and coordinates the long-running
    download/verify/install asyncio task.

    mode ∈ {"real", "simulate"} — selected at startup from
    $EAP_FW_UPGRADE_MODE (see Requirement 13.3).
    """

    def __init__(self, upgrade_char: "UpgradeStatusCharacteristic",
                 mode: str = "real"):
        self.a7 = upgrade_char
        self.mode = mode if mode in ("real", "simulate") else "real"
        self.current_task: Optional[asyncio.Task] = None

    # -- Parsing --------------------------------------------------------

    def _parse(self, obj: dict) -> UpgradeCommand:
        """Validate and parse an upgrade command dict (pre-parsed JSON).

        Raises InvalidUpgradeCommand on any violation of Requirement 5.5
        / 12.2 / 12.5.
        """
        if not isinstance(obj, dict):
            raise InvalidUpgradeCommand("not an object")
        if obj.get("cmd") != "upgrade":
            raise InvalidUpgradeCommand("cmd != upgrade")
        url = obj.get("url")
        if not isinstance(url, str) or not url:
            raise InvalidUpgradeCommand("missing url")

        manual = obj.get("manual", False)
        if manual is not False and manual is not True:
            raise InvalidUpgradeCommand("manual not bool")

        # Only https is accepted (both real and simulate modes).
        # Simulate mode branches on a marker substring inside the URL
        # path — "simulate-fail-download" or "simulate-fail-verify"
        # — rather than on a custom scheme, so clients can always
        # send real-looking https URLs.
        if not url.startswith("https://"):
            raise InvalidUpgradeCommand("bad url scheme")
        parsed = urlparse(url)
        if parsed.scheme != "https" or not parsed.netloc:
            raise InvalidUpgradeCommand("bad url scheme")

        sha256_field = obj.get("sha256")
        md5_field = obj.get("md5")
        # Accept only `md5` (pivot v2); reject legacy `sha256` payloads
        # explicitly so mis-configured clients don't silently skip
        # checksum verification.
        if sha256_field is not None:
            raise InvalidUpgradeCommand("sha256 no longer supported, use md5")
        if md5_field is not None:
            if not isinstance(md5_field, str) or not _MD5_RE.match(md5_field):
                raise InvalidUpgradeCommand("bad md5")

        return UpgradeCommand(url=url, md5=md5_field, manual=bool(manual))

    # -- Entry points ---------------------------------------------------

    async def on_command(self, obj: dict) -> None:
        """Called from WifiProvisionCharacteristic when cmd == "upgrade".

        Requirement 5.6: auth is assumed to be verified by the caller.
        Requirement 9.1: reject when device is busy.
        """
        try:
            cmd = self._parse(obj)
        except InvalidUpgradeCommand as e:
            # Req 5.5: no state change, just report via A7.
            print(f"[BLE] Upgrade command rejected: {e}", flush=True)
            self.a7.update("failed", "invalid upgrade command")
            return

        # Test-only visibility: log the parsed command so we can
        # confirm the iOS app is sending the right URL / md5 / manual
        # flag without needing to instrument the BLE layer.
        print(
            f"[BLE] Upgrade command received: mode={self.mode} "
            f"manual={cmd.manual} md5={cmd.md5} url={cmd.url}",
            flush=True,
        )

        assert device_status_lock is not None
        async with device_status_lock:
            if device_status != 0:
                self.a7.update("failed", "busy: device not idle")
                return
            _set_device_status(10)  # fw_downloading
            self.a7.update("downloading", "")

        self.current_task = asyncio.create_task(self._run(cmd))

    async def _run(self, cmd: UpgradeCommand) -> None:
        try:
            if self.mode == "simulate":
                await self._run_simulate(cmd)
            else:
                await self._run_real(cmd)
        except UpgradeError as e:
            await self._fail(e.message)
            return
        except Exception as e:
            await self._fail(f"unexpected: {e}")
            return

        # Success path: schedule auto-reset to idle.
        reset_delay = (SIMULATE_IDLE_RESET_SECONDS if self.mode == "simulate"
                       else UPGRADE_SUCCESS_RESET_SECONDS)
        await asyncio.sleep(reset_delay)
        async with device_status_lock:
            if device_status == 14:
                _set_device_status(0)
                self.a7.update("idle", "")

    async def on_ack(self) -> None:
        """Called when the app sends {"cmd":"upgrade_ack"} to acknowledge
        a terminal upgrade state. Resets device_status from 14/15 to 0
        so the advertisement no longer shows stale fw_success/fw_failed."""
        assert device_status_lock is not None
        async with device_status_lock:
            if device_status in UPGRADE_TERMINAL_STATUSES:
                print(f"[BLE] Upgrade ack: resetting status {device_status} -> 0",
                      flush=True)
                _set_device_status(0)
                self.a7.update("idle", "")

    async def _fail(self, message: str) -> None:
        assert device_status_lock is not None
        async with device_status_lock:
            _set_device_status(15)  # fw_failed
        self.a7.update("failed", message)

    # -- Simulate mode --------------------------------------------------

    async def _run_simulate(self, cmd: UpgradeCommand) -> None:
        assert device_status_lock is not None

        # Simulate-mode failure triggers: look for marker substrings
        # in the URL path so callers can send real-looking https URLs
        # (no custom scheme required).
        fail_download = "simulate-fail-download" in cmd.url
        fail_verify = "simulate-fail-verify" in cmd.url

        # downloading (device_status=10 already set in on_command)
        await asyncio.sleep(SIMULATE_TIMINGS["downloading"])
        if fail_download:
            raise UpgradeError("simulated download failure")

        # verifying — only when md5 present (manual mode skips it)
        if cmd.md5 is not None:
            async with device_status_lock:
                _set_device_status(11)
            self.a7.update("verifying", "")
            await asyncio.sleep(SIMULATE_TIMINGS["verifying"])
            if fail_verify:
                raise UpgradeError("simulated checksum mismatch")

        # installing
        async with device_status_lock:
            _set_device_status(12)
        self.a7.update("installing", "")
        await asyncio.sleep(SIMULATE_TIMINGS["installing"])

        # rebooting
        async with device_status_lock:
            _set_device_status(13)
        self.a7.update("rebooting", "")
        await asyncio.sleep(SIMULATE_TIMINGS["rebooting"])

        # success (do NOT actually reboot in simulate mode)
        async with device_status_lock:
            _set_device_status(14)
        self.a7.update("success", "")

    # -- Real mode ------------------------------------------------------

    async def _run_real(self, cmd: UpgradeCommand) -> None:
        try:
            ret_msg = subprocess.run(
                ["sysupgrade", "-T", cmd.url],
                capture_output=True,
                text=True
            )

            if ret_msg.stderr.find("HTTP error 404") != -1:
                raise UpgradeError("Image does not exist")

            if ret_msg.stderr.find("Operation not permitted") != -1:
                raise UpgradeError("URL failure")

            if ret_msg.stderr.find("Image check failed") != -1:
                raise UpgradeError("Image file download and verification error")

            async with device_status_lock:
                _set_device_status(14)
            self.a7.update("success", "")
            await asyncio.sleep(SIMULATE_TIMINGS["installing"])

            ret_msg = subprocess.run(
                ["sysupgrade", "/var/sysupgrade.img"],
                capture_output=True,
                text=True
            )
        except subprocess.CalledProcessError:
            pass


# --------------------------------------------------
# Main
# --------------------------------------------------
def reset_bluetooth():
    """Kill and restart bluetoothd to clear stale registrations."""
    print("Resetting bluetoothd...")
    subprocess.call(["killall", "bluetoothd"], stderr=subprocess.DEVNULL)
    time.sleep(2)
    subprocess.Popen(["bluetoothd"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    # Wait until hci0 is UP
    for i in range(10):
        time.sleep(1)
        try:
            out = subprocess.check_output(["hciconfig", "hci0"], text=True)
            if "UP RUNNING" in out:
                print("bluetoothd ready")
                return
        except Exception:
            pass
    print("WARNING: bluetoothd may not be ready")

async def main():
    global device_status_lock, firmware_handler
    device_status_lock = asyncio.Lock()

    # Firmware upgrade mode selection (Requirement 13.3).
    mode = os.environ.get("EAP_FW_UPGRADE_MODE", "real").lower()
    if mode not in ("real", "simulate"):
        mode = "real"
    print(f"[BLE] Upgrade mode: {mode}", flush=True)

    reset_bluetooth()
    bus = await MessageBus(bus_type=BusType.SYSTEM).connect()

    service = GattService()
    auth_char = AuthCharacteristic(service)
    read_char = WifiReadCharacteristic(service, auth_char)
    result_char = ProvisionResultCharacteristic(service)
    write_char = WifiProvisionCharacteristic(service, result_char, auth_char)
    status_char = StatusReadCharacteristic(service, auth_char)
    mac_char = MacCharacteristic(service)
    upgrade_char = UpgradeStatusCharacteristic(service)
    firmware_handler = FirmwareUpgradeHandler(upgrade_char, mode=mode)

    managed = {
        service.path: {
            "org.bluez.GattService1": {
                "UUID": Variant("s", SERVICE_UUID),
                "Primary": Variant("b", True),
                "Characteristics": Variant("ao", [
                    read_char.path,
                    write_char.path,
                    result_char.path,
                    status_char.path,
                    auth_char.path,
                    mac_char.path,
                    upgrade_char.path
                ])
            }
        }
    }

    bus.export(APP_PATH, ObjectManager(managed))
    bus.export(service.path, service)
    bus.export(read_char.path, read_char)
    bus.export(write_char.path, write_char)
    bus.export(result_char.path, result_char)
    bus.export(status_char.path, status_char)
    bus.export(auth_char.path, auth_char)
    bus.export(mac_char.path, mac_char)
    bus.export(upgrade_char.path, upgrade_char)

    adv = Advertisement()
    bus.export(adv.path, adv)

    intro = await bus.introspect(BLUEZ, ADAPTER)
    bluez = bus.get_proxy_object(BLUEZ, ADAPTER, intro)

    # Set adapter alias so BlueZ default name matches our device name
    adapter = bluez.get_interface("org.freedesktop.DBus.Properties")
    await adapter.call_set("org.bluez.Adapter1", "Alias", Variant("s", get_device_name()))
    # Disable pairing/bonding — we use application-layer auth instead
    await adapter.call_set("org.bluez.Adapter1", "Pairable", Variant("b", False))

    gatt_mgr = bluez.get_interface("org.bluez.GattManager1")
    adv_mgr = bluez.get_interface("org.bluez.LEAdvertisingManager1")

    await gatt_mgr.call_register_application(APP_PATH, {})
    await adv_mgr.call_register_advertisement(adv.path, {})

    print("✅ BLE GATT + Advertisement ready")
    print(f"   MAC: {get_device_mac()}  Auth key: {get_ble_auth_key()}")
    print("   Press Ctrl+C to stop\n")

    # Monitor for client disconnect and re-register advertisement
    async def watch_advertising():
        """Periodically re-register advertisement to refresh manufacturer data."""
        while True:
            await asyncio.sleep(5)
            try:
                await adv_mgr.call_unregister_advertisement(adv.path)
            except Exception:
                pass
            try:
                await adv_mgr.call_register_advertisement(adv.path, {})
            except Exception:
                pass

    adv_task = asyncio.create_task(watch_advertising())

    try:
        await asyncio.get_running_loop().create_future()
    except asyncio.CancelledError:
        pass
    finally:
        adv_task.cancel()
        print("\nCleaning up BLE...")
        try:
            await adv_mgr.call_unregister_advertisement(adv.path)
        except Exception:
            pass
        try:
            await gatt_mgr.call_unregister_application(APP_PATH)
        except Exception:
            pass
        bus.disconnect()
        print("BLE cleanup done")


if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        pass
