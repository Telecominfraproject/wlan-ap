#!/bin/sh
# SPDX-License-Identifier: BSD-3-Clause
#
# tisbl.sh -- verify / flash the TI BLE MCU firmware through the ROM serial
#             bootloader (SBL).
#
# Usage:
#   tisbl.sh <tty> <baud> <tichip> <firmware> <resetpin> <backdoorpin> [<sw_rstpin>]
#
# Shared by every board carrying a CC2652R1 (eap101, eap104, oap101, ...).
# No per-model branching: everything board-specific arrives as an argument and
# lives in /etc/config/ble_chip under a section named after the model.
#
# COMPATIBILITY RULE. The previous revision of this script could not fail --
# when a GPIO path did not exist it wrote into nothing and carried on. Any board
# that works today may be relying on that. So nothing here aborts over a GPIO
# problem: it warns and continues, and the worst case is the same no-op as
# before, with a log line explaining it. The only hard failures are an
# unreadable firmware file and a non-zero exit from tisbl itself.
#
# Pin arguments accept a /sys/class/gpio name or a Linux GPIO number.
#
# PHYSICAL levels (verify on a new board; /sys/kernel/debug/gpio is the only
# file that shows the truth):
#     reset     LOW = held in reset       HIGH = running
#     backdoor  LOW = ROM serial loader   HIGH = run the application
# Levels are written physically; active_low is read back and compensated for.

set -u

GPIO_DIR=/sys/class/gpio
PINMUX_GLOB=/sys/kernel/debug/pinctrl/*/pinmux-pins
LOCK=/var/lock/ble.lock
DAEMON=/etc/init.d/ble-provision

die()  { echo "tisbl.sh: $*" >&2; exit 1; }
warn() { echo "tisbl.sh: $*" >&2; }

[ $# -ge 6 ] || die \
	"usage: $0 <tty> <baud> <tichip> <firmware> <resetpin> <backdoorpin> [<sw_rstpin>]"

tty=$1
baudrate=$2
tichip=$3
firmware=$4
resetpin=$5
backdoorpin=$6
sw_rstpin=${7:-}

[ -r "$firmware" ] || die "firmware '$firmware' is not readable"

# ------------------------------------------------------------------- gpio -----

# Print the pinmux owner of a Linux GPIO number. Lines look like
#   pin 36 (GPIO_36): 90000.mdio (GPIO UNCLAIMED) function mdc group gpio36
#   pin 31 (GPIO_31): (MUX UNCLAIMED) 1000000.pinctrl:496
# and the pin numbers there are TLMM-relative, so the gpiochip base comes off
# the Linux number first.
gpio_mux_owner() {
	_pm=""
	for _f in $PINMUX_GLOB; do
		[ -r "$_f" ] && { _pm=$_f; break; }
	done
	[ -n "$_pm" ] || return 1

	_base=""
	for _f in "$GPIO_DIR"/gpiochip*/base; do
		[ -r "$_f" ] && { _base=$(cat "$_f"); break; }
	done
	[ -n "$_base" ] || return 1

	_rel=$(( $1 - _base ))
	[ "$_rel" -ge 0 ] || return 1

	_line=$(grep "^pin $_rel (" "$_pm" 2>/dev/null) || return 1
	[ -n "$_line" ] || return 1

	_rest=${_line#*): }
	echo "${_rest%% *}"
}

# True when no peripheral driver has muxed this pin.
#
# This is the check /sys/class/gpio/export cannot do: pinctrl-msm is not strict,
# so exporting a pin another driver owns SUCCEEDS and then silently overrides
# its mux. On EAP104 sw_rstpin was 501 = TLMM 36 = MDC, the MDIO clock for the
# QCA8337 switch and its PHYs -- export worked, MDIO stopped, ethernet died and
# shutdown hung. "(MUX UNCLAIMED)" is free; a "*.pinctrl" owner is a device-tree
# hog, i.e. our own mcu_pins node, not a competitor.
gpio_is_free() {
	case "$1" in
	''|*[!0-9]*) return 0 ;;   # a name, not a number: nothing to look up
	esac

	_owner=$(gpio_mux_owner "$1") || return 0   # cannot tell; do not block

	case "$_owner" in
	''|'(MUX'|*.pinctrl) return 0 ;;
	esac

	warn "GPIO $1 is muxed by '$_owner' -- not touching it"
	return 1
}

# Resolve a pin spec to a /sys/class/gpio directory, or print nothing.
gpio_dir() {
	[ -d "$GPIO_DIR/$1" ]     && { echo "$GPIO_DIR/$1";     return 0; }
	[ -d "$GPIO_DIR/gpio$1" ] && { echo "$GPIO_DIR/gpio$1"; return 0; }

	case "$1" in
	''|*[!0-9]*) return 1 ;;
	esac
	echo "$1" > "$GPIO_DIR/export" 2>/dev/null || return 1
	[ -d "$GPIO_DIR/gpio$1" ] || return 1
	echo "$GPIO_DIR/gpio$1"
}

# Write a PHYSICAL level. Returns non-zero rather than aborting.
gpio_set() {
	[ -n "$1" ] || return 1
	_d=$1
	_v=$2

	_al=0
	[ -r "$_d/active_low" ] && _al=$(cat "$_d/active_low" 2>/dev/null)
	[ "$_al" = "1" ] && _v=$(( 1 - _v ))

	# `direction` is absent unless the device tree node carries
	# gpio-export,direction_may_change, so this is allowed to fail.
	[ -w "$_d/direction" ] && echo out > "$_d/direction" 2>/dev/null

	echo "$_v" > "$_d/value" 2>/dev/null
}

# Resolve each pin, skipping any the mux check rejects. An empty result just
# means the corresponding step is skipped later.
RESET_DIR=""
BACKDOOR_DIR=""
SW_RST_DIR=""

gpio_is_free "$resetpin"    && RESET_DIR=$(gpio_dir "$resetpin")
gpio_is_free "$backdoorpin" && BACKDOOR_DIR=$(gpio_dir "$backdoorpin")
[ -n "$sw_rstpin" ] && gpio_is_free "$sw_rstpin" && SW_RST_DIR=$(gpio_dir "$sw_rstpin")

[ -n "$RESET_DIR" ]    || warn "no usable sysfs path for resetpin '$resetpin'"
[ -n "$BACKDOOR_DIR" ] || warn "no usable sysfs path for backdoorpin '$backdoorpin'"

# ------------------------------------------------------------- chip states ----

ti_reset() {
	[ -n "$RESET_DIR" ] || return 1
	gpio_set "$RESET_DIR" 1 || return 1
	gpio_set "$RESET_DIR" 0 || return 1
	sleep 1
	gpio_set "$RESET_DIR" 1
}

# The backdoor level is sampled by the ROM at reset, so set it first.
ti_enter_bootloader() {
	gpio_set "$BACKDOOR_DIR" 0
	ti_reset || return 1
	sleep 1
}

# Deasserting the backdoor is not enough -- the chip stays in the loader until
# it is reset again. The previous revision stopped at the deassert, so the chip
# only ever left the SBL if tisbl reset it on its own, including on the
# "checksum matched, nothing to do" path. A chip left in the SBL shows up as no
# firmware READY, no ADV_START and a silent radio.
ti_enter_application() {
	gpio_set "$BACKDOOR_DIR" 1
	ti_reset
}

stopped_daemon=

cleanup() {
	_rc=$?
	trap - EXIT INT TERM
	ti_enter_application || warn "could not return the MCU to application mode"
	[ -n "$stopped_daemon" ] && { "$DAEMON" start || warn "could not restart $DAEMON"; }
	exit $_rc
}

# -------------------------------------------------------------------- lock ----

# fd 9, not 100: multi-digit descriptors in redirections are not portable to
# POSIX sh and this is busybox ash. The old `exec 100>` never once worked here --
# /var/lock/ble.lock did not exist on a system booted past S62 -- so the script
# always ran unlocked. Failing to lock is therefore not a regression and must
# not abort.
# The file is created with `touch` first, on purpose. `exec` is a POSIX special
# built-in, and a redirection error on a special built-in makes a non-interactive
# shell exit -- putting `exec 9>$LOCK` in an `if` condition would NOT contain the
# failure, it would kill the script. Proving the path is writable first means the
# exec cannot fail.
if mkdir -p "${LOCK%/*}" 2>/dev/null && touch "$LOCK" 2>/dev/null; then
	exec 9>"$LOCK"
	flock 9 2>/dev/null || warn "could not take the lock on $LOCK -- continuing"
else
	warn "cannot create $LOCK -- continuing unlocked"
fi

# ------------------------------------------------------------------ daemon ----

# ble-provisiond keeps $tty open and reads from it continuously, and takes no
# lock of its own. Two readers on one tty means the kernel gives each byte to
# whoever reads first, so the SBL handshake loses its replies. Not an issue at
# boot, where this runs before the daemon; it matters for a runtime upgrade.
if pgrep ble-provisiond >/dev/null 2>&1 && [ -x "$DAEMON" ]; then
	warn "ble-provisiond holds $tty -- stopping it for the duration"
	if "$DAEMON" stop; then
		stopped_daemon=1
		sleep 1
	else
		warn "could not stop ble-provisiond -- the SBL handshake may fail"
	fi
fi

# -------------------------------------------------------------------- work ----

# From here a failure must not leave the chip in the bootloader.
trap cleanup EXIT INT TERM

[ -n "$SW_RST_DIR" ] && { gpio_set "$SW_RST_DIR" 1 || warn "could not drive sw_rstpin"; }

ti_enter_bootloader || warn "could not drive the MCU into the bootloader -- trying tisbl anyway"

# tisbl compares the on-chip checksum itself and skips flashing when it matches.
tisbl "$tty" "$baudrate" "$tichip" "$firmware"
rc=$?

# Captured into a variable rather than read as $? inside an else branch, where
# what $? refers to is easy to get wrong when anything is added in between.
if [ "$rc" -eq 0 ]; then
	echo "tisbl.sh: tisbl ok (flashed, or the checksum already matched)"
else
	die "tisbl failed with exit code $rc"
fi

exit 0
