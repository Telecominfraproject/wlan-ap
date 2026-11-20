#!/bin/sh
# SPDX-License-Identifier: BSD-3-Clause
# Write an escaped byte sequence to a serial device, then wait in tenths of a
# second so the controller can process it.

if [ "$#" -ne 3 ]; then
	echo "usage: $0 <tty> <delay-tenths> <escaped-bytes>" >&2
	exit 2
fi

tty=$1
delay=$2
command=$3

case "$delay" in
	''|*[!0-9]*)
		echo "$0: delay must be a non-negative integer" >&2
		exit 2
		;;
esac

printf '%b' "$command" > "$tty" || exit 1
usleep $((delay * 100000))
