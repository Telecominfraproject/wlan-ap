#!/bin/sh
#
# Copyright (c) 2019, The Linux Foundation. All rights reserved.
#
# Permission to use, copy, modify, and/or distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
# ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
# ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
# OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
[ -e /lib/ipq806x.sh ] && . /lib/ipq806x.sh
. /lib/functions.sh

type ipq806x_board_name &>/dev/null  || ipq806x_board_name() {
        echo $(board_name) | sed 's/^\([^-]*-\)\{1\}//g'
}

enable_affinity_hk10_c2() {

	# Enable smp affinity for PCIE attach
	#pci 0

	#assign 3 tcl completions to last 3 CPUs from reverse
	irq_affinity_num=`grep -E -m1 'pci0_wbm2host_tx_completions_ring1' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 8 > /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=`grep -E -m1 'pci0_wbm2host_tx_completions_ring2' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 4 > /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=`grep -E -m1 'pci0_wbm2host_tx_completions_ring3' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 2 > /proc/irq/$irq_affinity_num/smp_affinity

	#assign lmac,reo err,release interrupts are mapped to one core alone
	irq_affinity_num=`grep -E -m1 'pci0_lmac_reo_misc_irq' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 8 > /proc/irq/$irq_affinity_num/smp_affinity

	#assign 4 rx interrupts to each cores
	irq_affinity_num=`grep -E -m1 'pci0_reo2host_destination_ring1' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 1 > /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=`grep -E -m1 'pci0_reo2host_destination_ring2' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 2 > /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=`grep -E -m1 'pci0_reo2host_destination_ring3' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 4 > /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=`grep -E -m1 'pci0_reo2host_destination_ring4' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 8 > /proc/irq/$irq_affinity_num/smp_affinity

	#pci 1
	#assign 3 tcl completions to last 3 CPUS from reverse
	irq_affinity_num=`grep -E -m1 'pci1_wbm2host_tx_completions_ring1' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 8 > /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=`grep -E -m1 'pci1_wbm2host_tx_completions_ring2' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 4 > /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=`grep -E -m1 'pci1_wbm2host_tx_completions_ring3' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 2 > /proc/irq/$irq_affinity_num/smp_affinity

	#assign lmac,reo err,release interrupts are mapped to one core alone
	irq_affinity_num=`grep -E -m1 'pci1_lmac_reo_misc_irq' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 8 > /proc/irq/$irq_affinity_num/smp_affinity

	#assign 4 rx interrupts to each cores
	irq_affinity_num=`grep -E -m1 'pci1_reo2host_destination_ring1' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 1 > /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=`grep -E -m1 'pci1_reo2host_destination_ring2' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 2 > /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=`grep -E -m1 'pci1_reo2host_destination_ring3' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 4 > /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=`grep -E -m1 'pci1_reo2host_destination_ring4' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 8 > /proc/irq/$irq_affinity_num/smp_affinity
}

enable_affinity_hk_cp01_c1() {

	#assign 4 rx interrupts to each cores
	irq_affinity_num=`grep -E -m1 'reo2host-destination-ring4' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 8 > /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=`grep -E -m1 'reo2host-destination-ring3' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 4 > /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=`grep -E -m1 'reo2host-destination-ring2' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 2 > /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=`grep -E -m1 'reo2host-destination-ring1' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
        [ -n "$irq_affinity_num" ] && echo 1 > /proc/irq/$irq_affinity_num/smp_affinity

	#assign 3 tcl completions to 3 CPUs
	irq_affinity_num=`grep -E -m1 'wbm2host-tx-completions-ring3' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 4 > /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=`grep -E -m1 'wbm2host-tx-completions-ring2' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 2 > /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=`grep -E -m1 'wbm2host-tx-completions-ring1' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 1 > /proc/irq/$irq_affinity_num/smp_affinity
}

enable_affinity_hk10() {
	# Enable smp affinity for PCIE attach
	#pci 0

	#assign 3 tcl completions to first 3 CPUs
	irq_affinity_num=`grep -E -m1 'pci0_wbm2host_tx_completions_ring1' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 1 > /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=`grep -E -m1 'pci0_wbm2host_tx_completions_ring2' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 2 > /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=`grep -E -m1 'pci0_wbm2host_tx_completions_ring3' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 4 > /proc/irq/$irq_affinity_num/smp_affinity

	#assign lmac,reo err,release interrupts are mapped to one core alone
	irq_affinity_num=`grep -E -m1 'pci0_lmac_reo_misc_irq' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 8 > /proc/irq/$irq_affinity_num/smp_affinity

	#assign 4 rx interrupts to each cores
	irq_affinity_num=`grep -E -m1 'pci0_reo2host_destination_ring1' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 1 > /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=`grep -E -m1 'pci0_reo2host_destination_ring2' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 2 > /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=`grep -E -m1 'pci0_reo2host_destination_ring3' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 4 > /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=`grep -E -m1 'pci0_reo2host_destination_ring4' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 8 > /proc/irq/$irq_affinity_num/smp_affinity

	#pci 1
	#assign 3 tcl completions to first 3 CPUS
	irq_affinity_num=`grep -E -m1 'pci1_wbm2host_tx_completions_ring1' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 1 > /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=`grep -E -m1 'pci1_wbm2host_tx_completions_ring2' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 2 > /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=`grep -E -m1 'pci1_wbm2host_tx_completions_ring3' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 4 > /proc/irq/$irq_affinity_num/smp_affinity

	#assign lmac,reo err,release interrupts are mapped to one core alone
	irq_affinity_num=`grep -E -m1 'pci1_lmac_reo_misc_irq' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 8 > /proc/irq/$irq_affinity_num/smp_affinity

	#assign 4 rx interrupts to each cores
	irq_affinity_num=`grep -E -m1 'pci1_reo2host_destination_ring1' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 1 > /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=`grep -E -m1 'pci1_reo2host_destination_ring2' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 2 > /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=`grep -E -m1 'pci1_reo2host_destination_ring3' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 4 > /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=`grep -E -m1 'pci1_reo2host_destination_ring4' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 8 > /proc/irq/$irq_affinity_num/smp_affinity
}

enable_affinity_al02_c1() {

	#assign 4 rx interrupts to each cores
	irq_affinity_num=`grep -E -m1 'reo2host-destination-ring4' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 8 > /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=`grep -E -m1 'reo2host-destination-ring3' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 4 > /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=`grep -E -m1 'reo2host-destination-ring2' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 2 > /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=`grep -E -m1 'reo2host-destination-ring1' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 1 > /proc/irq/$irq_affinity_num/smp_affinity

	#assign 3 tcl completions to last 3 CPUs
	irq_affinity_num=`grep -E -m1 'wbm2host-tx-completions-ring3' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 8 > /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=`grep -E -m1 'wbm2host-tx-completions-ring2' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 4 > /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=`grep -E -m1 'wbm2host-tx-completions-ring1' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 2 > /proc/irq/$irq_affinity_num/smp_affinity

	# Enable smp affinity for PCIE attach
	#pci 3

	#assign 3 tcl completions to last 3 CPUs
	irq_affinity_num=`grep -E -m1 'pci3_wbm2host_tx_completions_ring1' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 2 > /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=`grep -E -m1 'pci3_wbm2host_tx_completions_ring2' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 4 > /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=`grep -E -m1 'pci3_wbm2host_tx_completions_ring3' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 8 > /proc/irq/$irq_affinity_num/smp_affinity

	#assign lmac,reo err,release interrupts are mapped to one core alone
        irq_affinity_num=`grep -E -m1 'pci3_lmac_reo_misc_irq' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 8 > /proc/irq/$irq_affinity_num/smp_affinity

	#assign 4 rx interrupts to each cores
	irq_affinity_num=`grep -E -m1 'pci3_reo2host_destination_ring1' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 1 > /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=`grep -E -m1 'pci3_reo2host_destination_ring2' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 2 > /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=`grep -E -m1 'pci3_reo2host_destination_ring3' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 4 > /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=`grep -E -m1 'pci3_reo2host_destination_ring4' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 8 > /proc/irq/$irq_affinity_num/smp_affinity

	#pci 4
	#assign 3 tcl completions to last 3 CPUs
	irq_affinity_num=`grep -E -m1 'pci4_wbm2host_tx_completions_ring1' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 2 > /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=`grep -E -m1 'pci4_wbm2host_tx_completions_ring2' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 4 > /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=`grep -E -m1 'pci4_wbm2host_tx_completions_ring3' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 8 > /proc/irq/$irq_affinity_num/smp_affinity

	#assign lmac,reo err,release interrupts are mapped to one core alone
	irq_affinity_num=`grep -E -m1 'pci4_lmac_reo_misc_irq' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 8 > /proc/irq/$irq_affinity_num/smp_affinity

	#assign 4 rx interrupts to each cores from reverse
	irq_affinity_num=`grep -E -m1 'pci4_reo2host_destination_ring1' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 8 > /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=`grep -E -m1 'pci4_reo2host_destination_ring2' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 4 > /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=`grep -E -m1 'pci4_reo2host_destination_ring3' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 2 > /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=`grep -E -m1 'pci4_reo2host_destination_ring4' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 1 > /proc/irq/$irq_affinity_num/smp_affinity
}
enable_affinity_hk14() {
	# Enable smp affinity for PCIE attach
	#pci 0

	#assign 3 tcl completions to first 3 CPUs
	irq_affinity_num=`grep -E -m1 'pci0_wbm2host_tx_completions_ring1' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 8 > /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=`grep -E -m1 'pci0_wbm2host_tx_completions_ring2' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 2 > /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=`grep -E -m1 'pci0_wbm2host_tx_completions_ring3' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 4 > /proc/irq/$irq_affinity_num/smp_affinity

	#assign lmac,reo err,release interrupts are mapped to one core alone
	irq_affinity_num=`grep -E -m1 'pci0_lmac_reo_misc_irq' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 8 > /proc/irq/$irq_affinity_num/smp_affinity

	#assign 4 rx interrupts to each cores
	irq_affinity_num=`grep -E -m1 'pci0_reo2host_destination_ring3' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 4 > /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=`grep -E -m1 'pci0_reo2host_destination_ring4' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 8 > /proc/irq/$irq_affinity_num/smp_affinity


	#assign 4 rx interrupts to each cores
	irq_affinity_num=`grep -E -m1 'reo2host-destination-ring2' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 1 > /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=`grep -E -m1 'reo2host-destination-ring1' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
        [ -n "$irq_affinity_num" ] && echo 2 > /proc/irq/$irq_affinity_num/smp_affinity

	#assign 3 tcl completions to 3 CPUs
	irq_affinity_num=`grep -E -m1 'wbm2host-tx-completions-ring3' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 1 > /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=`grep -E -m1 'wbm2host-tx-completions-ring2' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 1 > /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=`grep -E -m1 'wbm2host-tx-completions-ring1' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 1 > /proc/irq/$irq_affinity_num/smp_affinity

	# rx ring 3 and 4 are mapped for 6G pine radio and disabled rx_hash for 2G and 5G
	echo 0x9246db > /sys/kernel/debug/ath11k/qcn9074\ hw1.0_0000\:01\:00.0/rx_hash
	echo 0 > /sys/kernel/debug/ath11k/ipq8074\ hw2.0/rx_hash
}

enable_affinity_mpc1() {
	# NSS DP gmac interrupts
	irq_affinity_num=`grep -E -m1 'nss-dp-gmac' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 1 > /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=`grep -E -m2 'nss-dp-gmac' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 1 > /proc/irq/$irq_affinity_num/smp_affinity

	#affinity for 6G radio
	irq_affinity_num=`grep -E -m1 'pci_wbm2host_tx_completions_ring1' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 2 > /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=`grep -E -m1 'pci_wbm2host_tx_completions_ring2' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 1 > /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=`grep -E -m1 'pci_reo2host_destination_ring1' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 2 > /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=`grep -E -m1 'pci_reo2host_destination_ring2' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 2 > /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=`grep -E -m1 'pci_reo2host_destination_ring3' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 2 > /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=`grep -E -m1 'pci_reo2host_destination_ring4' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 2 > /proc/irq/$irq_affinity_num/smp_affinity

	#affinity for 5G radio
	irq_affinity_num=`grep -E -m1 'pci1_wbm2host_tx_completions_ring1' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 2 > /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=`grep -E -m1 'pci1_wbm2host_tx_completions_ring2' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 1 > /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=`grep -E -m1 'pci1_reo2host_destination_ring1' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 2 > /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=`grep -E -m1 'pci1_reo2host_destination_ring2' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 2 > /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=`grep -E -m1 'pci1_reo2host_destination_ring3' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 2 > /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=`grep -E -m1 'pci1_reo2host_destination_ring4' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 2 > /proc/irq/$irq_affinity_num/smp_affinity

	#affinity for 2G radio
	irq_affinity_num=`grep -E -m1 'wbm2host-tx-completions-ring1' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 2 > /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=`grep -E -m1 'wbm2host-tx-completions-ring2' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 1 > /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=`grep -E -m1 'reo2host-destination-ring1' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 2 > /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=`grep -E -m1 'reo2host-destination-ring2' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 2 > /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=`grep -E -m1 'reo2host-destination-ring3' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 2 > /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=`grep -E -m1 'reo2host-destination-ring4' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 2 > /proc/irq/$irq_affinity_num/smp_affinity
}

enable_affinity_al02_c4() {

	#pci 3
	#assign 4 rx interrupts to each cores
	irq_affinity_num=`grep -E -m1 'pci3_wlan_dp_4' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 1 > /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=`grep -E -m1 'pci3_wlan_dp_5' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 2 > /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=`grep -E -m1 'pci3_wlan_dp_6' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 4 > /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=`grep -E -m1 'pci3_wlan_dp_7' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 8 > /proc/irq/$irq_affinity_num/smp_affinity

	#assign 3 tcl completions to last 3 CPUs
	irq_affinity_num=`grep -E -m1 'pci3_wlan_dp_0' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 1 > /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=`grep -E -m1 'pci3_wlan_dp_1' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 2 > /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=`grep -E -m1 'pci3_wlan_dp_2' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 4 > /proc/irq/$irq_affinity_num/smp_affinity

	# assign lmac,reo err,release interrupts are mapped to one core alone
	irq_affinity_num=`grep -E -m1 'pci3_wlan_dp_3' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 8 > /proc/irq/$irq_affinity_num/smp_affinity

	# assign 4th tcl completion ring interrupt to core 3
	irq_affinity_num=`grep -E -m1 'pci3_wlan_dp_11' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 8 > /proc/irq/$irq_affinity_num/smp_affinity

	# Enable smp affinity for PCIE attach
	#pci 2

	#assign 4 rx interrupts to each cores
	irq_affinity_num=`grep -E -m1 'pci2_wlan_dp_4' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 1 > /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=`grep -E -m1 'pci2_wlan_dp_5' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 2 > /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=`grep -E -m1 'pci2_wlan_dp_6' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 4 > /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=`grep -E -m1 'pci2_wlan_dp_7' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 8 > /proc/irq/$irq_affinity_num/smp_affinity

	#assign 3 tcl completions to last 3 CPUs
	irq_affinity_num=`grep -E -m1 'pci2_wlan_dp_0' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 1 > /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=`grep -E -m1 'pci2_wlan_dp_1' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 2 > /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=`grep -E -m1 'pci2_wlan_dp_2' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 4 > /proc/irq/$irq_affinity_num/smp_affinity

	# assign lmac,reo err,release interrupts are mapped to one core alone
        irq_affinity_num=`grep -E -m1 'pci2_wlan_dp_3' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 8 > /proc/irq/$irq_affinity_num/smp_affinity

	# assign 4th tcl completion ring interrupt to core 3
	irq_affinity_num=`grep -E -m1 'pci2_wlan_dp_11' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 8 > /proc/irq/$irq_affinity_num/smp_affinity

	#pci 1
	#assign 4 rx interrupts to each cores from reverse
	irq_affinity_num=`grep -E -m1 'pci1_wlan_dp_4' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 1 > /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=`grep -E -m1 'pci1_wlan_dp_5' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 2 > /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=`grep -E -m1 'pci1_wlan_dp_6' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 4 > /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=`grep -E -m1 'pci1_wlan_dp_7' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 8 > /proc/irq/$irq_affinity_num/smp_affinity

	#assign 3 tcl completions to last 3 CPUs
	irq_affinity_num=`grep -E -m1 'pci1_wlan_dp_0' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 1 > /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=`grep -E -m1 'pci1_wlan_dp_1' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 2 > /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=`grep -E -m1 'pci1_wlan_dp_2' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 4 > /proc/irq/$irq_affinity_num/smp_affinity

	#assign lmac,reo err,release interrupts are mapped to one core alone
	irq_affinity_num=`grep -E -m1 'pci1_wlan_dp_3' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 8 > /proc/irq/$irq_affinity_num/smp_affinity

	# assign 4th tcl completion ring interrupt to core 3
	irq_affinity_num=`grep -E -m1 'pci1_wlan_dp_11' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 8 > /proc/irq/$irq_affinity_num/smp_affinity

	#For monitor interrupts
	irq_affinity_num=`grep -E -m1 'pci2_wlan_dp_8' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 1 > /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=`grep -E -m1 'pci3_wlan_dp_8' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 2 > /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=`grep -E -m1 'pci1_wlan_dp_8' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 4 > /proc/irq/$irq_affinity_num/smp_affinity

	enable_affinity_for_ds=$(cat /sys/module/cfg80211/parameters/g_bonded_interface_model)
	if [ -n "$enable_affinity_for_ds" ] && [ $enable_affinity_for_ds == 'Y' ]; then
		echo "Configure Affinity for PPE DS for al02_c4" > /dev/ttyMSM0

		#RDP433  - Assign 5G and 2G bands to core 2 and
		#          6G alone to core 1 to support upto 320MHz BW
		############## affinity for 2G band - pci2 ########################
		irq_num=`grep pci2_ppe2tcl /proc/interrupts | cut -d ':' -f 1 | tr -d ' '`
		[ -n "$irq_num" ] && echo 4 > /proc/irq/$irq_num/smp_affinity

		irq_num=`grep pci2_reo2ppe /proc/interrupts | cut -d ':' -f 1 | tr -d ' '`
		[ -n "$irq_num" ] && echo 4 > /proc/irq/$irq_num/smp_affinity

		irq_num=`grep pci2_ppe_wbm_rel /proc/interrupts | cut -d ':' -f 1 | tr -d ' '`
		[ -n "$irq_num" ] && echo 4 > /proc/irq/$irq_num/smp_affinity

		##Extract the ppeds Node seq number and use same affinity to Node corresponding edma_ppeds interrupts
		ppeds_node=`grep pci2_reo2ppe_ /proc/interrupts | sed -n 's/.*pci2_reo2ppe_\([0-9]*\).*/\1/p'`
		irq_num=`grep edma_ppeds_rxfill_$ppeds_node /proc/interrupts | cut -d ':' -f 1 | tr -d ' '`
		[ -n "$irq_num" ] && echo 4 > /proc/irq/$irq_num/smp_affinity

		irq_num=`grep edma_ppeds_txcmpl_$ppeds_node /proc/interrupts | cut -d ':' -f 1 | tr -d ' '`
		[ -n "$irq_num" ] && echo 4 > /proc/irq/$irq_num/smp_affinity


		############## affinity for 6G band - pci3 ########################
		irq_num=`grep pci3_ppe2tcl /proc/interrupts | cut -d ':' -f 1 | tr -d ' '`
		[ -n "$irq_num" ] && echo 2 > /proc/irq/$irq_num/smp_affinity

		irq_num=`grep pci3_reo2ppe /proc/interrupts | cut -d ':' -f 1 | tr -d ' '`
		[ -n "$irq_num" ] && echo 2 > /proc/irq/$irq_num/smp_affinity

		irq_num=`grep pci3_ppe_wbm_rel /proc/interrupts | cut -d ':' -f 1 | tr -d ' '`
		[ -n "$irq_num" ] && echo 2 > /proc/irq/$irq_num/smp_affinity

		##Extract the ppeds Node seq number and use same affinity to Node corresponding edma_ppeds interrupts
		ppeds_node=`grep pci3_reo2ppe_ /proc/interrupts | sed -n 's/.*pci3_reo2ppe_\([0-9]*\).*/\1/p'`
		irq_num=`grep edma_ppeds_rxfill_$ppeds_node /proc/interrupts | cut -d ':' -f 1 | tr -d ' '`
		[ -n "$irq_num" ] && echo 2 > /proc/irq/$irq_num/smp_affinity

		irq_num=`grep edma_ppeds_txcmpl_$ppeds_node /proc/interrupts | cut -d ':' -f 1 | tr -d ' '`
		[ -n "$irq_num" ] && echo 2 > /proc/irq/$irq_num/smp_affinity


		############## affinity for 5G band - pci1 ########################
		irq_num=`grep pci1_ppe2tcl /proc/interrupts | cut -d ':' -f 1 | tr -d ' '`
		[ -n "$irq_num" ] && echo 4 > /proc/irq/$irq_num/smp_affinity

		irq_num=`grep pci1_reo2ppe /proc/interrupts | cut -d ':' -f 1 | tr -d ' '`
		[ -n "$irq_num" ] && echo 4 > /proc/irq/$irq_num/smp_affinity

		irq_num=`grep pci1_ppe_wbm_rel /proc/interrupts | cut -d ':' -f 1 | tr -d ' '`
		[ -n "$irq_num" ] && echo 4 > /proc/irq/$irq_num/smp_affinity

		##Extract the ppeds Node seq number and use same affinity to Node corresponding edma_ppeds interrupts
		ppeds_node=`grep pci1_reo2ppe_ /proc/interrupts | sed -n 's/.*pci4_reo2ppe_\([0-9]*\).*/\1/p'`
		irq_num=`grep edma_ppeds_rxfill_$ppeds_node /proc/interrupts | cut -d ':' -f 1 | tr -d ' '`
		[ -n "$irq_num" ] && echo 4 > /proc/irq/$irq_num/smp_affinity

		irq_num=`grep edma_ppeds_txcmpl_$ppeds_node /proc/interrupts | cut -d ':' -f 1 | tr -d ' '`
		[ -n "$irq_num" ] && echo 4 > /proc/irq/$irq_num/smp_affinity
	fi
}

enable_affinity_al02_c6() {

	#pci 3
	#assign 4 rx interrupts to each cores
	irq_affinity_num=`grep -E -m1 'pci3_wlan_dp_4' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 1 > /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=`grep -E -m1 'pci3_wlan_dp_5' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 2 > /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=`grep -E -m1 'pci3_wlan_dp_6' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 4 > /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=`grep -E -m1 'pci3_wlan_dp_7' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 8 > /proc/irq/$irq_affinity_num/smp_affinity

	#assign 3 tcl completions to last 3 CPUs
	irq_affinity_num=`grep -E -m1 'pci3_wlan_dp_0' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 1 > /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=`grep -E -m1 'pci3_wlan_dp_1' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 2 > /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=`grep -E -m1 'pci3_wlan_dp_2' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 4 > /proc/irq/$irq_affinity_num/smp_affinity

	# assign lmac,reo err,release interrupts are mapped to one core alone
	irq_affinity_num=`grep -E -m1 'pci3_wlan_dp_3' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 8 > /proc/irq/$irq_affinity_num/smp_affinity

	# assign 4th tcl completion ring interrupt to core 3
	irq_affinity_num=`grep -E -m1 'pci3_wlan_dp_11' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 8 > /proc/irq/$irq_affinity_num/smp_affinity

	#pci 4
	#assign 4 rx interrupts to each cores from reverse
	irq_affinity_num=`grep -E -m1 'pci4_wlan_dp_4' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 1 > /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=`grep -E -m1 'pci4_wlan_dp_5' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 2 > /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=`grep -E -m1 'pci4_wlan_dp_6' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 4 > /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=`grep -E -m1 'pci4_wlan_dp_7' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 8 > /proc/irq/$irq_affinity_num/smp_affinity

	#assign 3 tcl completions to last 3 CPUs
	irq_affinity_num=`grep -E -m1 'pci4_wlan_dp_0' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 1 > /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=`grep -E -m1 'pci4_wlan_dp_1' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 2 > /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=`grep -E -m1 'pci4_wlan_dp_2' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 4 > /proc/irq/$irq_affinity_num/smp_affinity

	#assign lmac,reo err,release interrupts are mapped to one core alone
	irq_affinity_num=`grep -E -m1 'pci4_wlan_dp_3' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 8 > /proc/irq/$irq_affinity_num/smp_affinity

	# assign 4th tcl completion ring interrupt to core 3
	irq_affinity_num=`grep -E -m1 'pci4_wlan_dp_11' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 8 > /proc/irq/$irq_affinity_num/smp_affinity

	#assign affinity for 2 GHz Alder
	#assign 4 rx interrupts to each cores
	irq_affinity_num=`grep -E -m1 'reo2host-destination-ring4' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 8 > /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=`grep -E -m1 'reo2host-destination-ring3' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 4 > /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=`grep -E -m1 'reo2host-destination-ring2' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 2 > /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=`grep -E -m1 'reo2host-destination-ring1' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 1 > /proc/irq/$irq_affinity_num/smp_affinity

	#assign 3 tcl completions to last 3 CPUs
	irq_affinity_num=`grep -E -m1 'wbm2host-tx-completions-ring3' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 4 > /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=`grep -E -m1 'wbm2host-tx-completions-ring2' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 2 > /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=`grep -E -m1 'wbm2host-tx-completions-ring1' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 1 > /proc/irq/$irq_affinity_num/smp_affinity

	#For monitor interrupts
	irq_affinity_num=`grep -E -m1 'pci3_wlan_dp_8' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 2 > /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=`grep -E -m1 'pci4_wlan_dp_8' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 4 > /proc/irq/$irq_affinity_num/smp_affinity
}

enable_affinity_al02_c9() {

	# Enable smp affinity for PCIE attach
	#pci 1

	#assign 4 rx interrupts to each cores
	irq_affinity_num=`grep -E -m1 'pci0_wlan_dp_4' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 1 > /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=`grep -E -m1 'pci0_wlan_dp_5' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 2 > /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=`grep -E -m1 'pci0_wlan_dp_6' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 4 > /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=`grep -E -m1 'pci0_wlan_dp_7' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 8 > /proc/irq/$irq_affinity_num/smp_affinity

	#assign 3 tcl completions to last 3 CPUs
	irq_affinity_num=`grep -E -m1 'pci0_wlan_dp_0' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 1 > /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=`grep -E -m1 'pci0_wlan_dp_1' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 2 > /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=`grep -E -m1 'pci0_wlan_dp_2' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 4 > /proc/irq/$irq_affinity_num/smp_affinity

	# assign 4th tcl completion ring and
	# lmac,reo err,release interrupts are mapped to one core alone
        irq_affinity_num=`grep -E -m1 'pci0_wlan_dp_3' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 8 > /proc/irq/$irq_affinity_num/smp_affinity

	#For monitor interrupts
	irq_affinity_num=`grep -E -m1 'pci0_wlan_dp_8' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 1 > /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=`grep -E -m1 'pci2_wlan_dp_8' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 2 > /proc/irq/$irq_affinity_num/smp_affinity

	# Enable smp affinity for PCIE attach
	#pci 3

	#assign 4 rx interrupts to each cores
	irq_affinity_num=`grep -E -m1 'pci2_wlan_dp_4' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 1 > /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=`grep -E -m1 'pci2_wlan_dp_5' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 2 > /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=`grep -E -m1 'pci2_wlan_dp_6' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 4 > /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=`grep -E -m1 'pci2_wlan_dp_7' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 8 > /proc/irq/$irq_affinity_num/smp_affinity

	#assign 3 tcl completions to last 3 CPUs
	irq_affinity_num=`grep -E -m1 'pci2_wlan_dp_0' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 1 > /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=`grep -E -m1 'pci2_wlan_dp_1' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 2 > /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=`grep -E -m1 'pci2_wlan_dp_2' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 4 > /proc/irq/$irq_affinity_num/smp_affinity

	# assign 4th tcl completion ring and
	# lmac,reo err,release interrupts are mapped to one core alone
	irq_affinity_num=`grep -E -m1 'pci2_wlan_dp_3' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 8 > /proc/irq/$irq_affinity_num/smp_affinity

	enable_affinity_for_ds=$(cat /sys/module/cfg80211/parameters/g_bonded_interface_model)
	if [ -n "$enable_affinity_for_ds" ] && [ $enable_affinity_for_ds == 'Y' ]; then
		echo "Configure Affinity for PPE DS for al02_c9" > /dev/ttyMSM0

		#For split wifi (RDP 454)  - Assign pci0 to core 2 and
		#                            Assign pci2 to core 1
		############## affinity for pci0 ########################
		irq_num=`grep pci0_ppe2tcl /proc/interrupts | cut -d ':' -f 1 | tr -d ' '`
		[ -n "$irq_num" ] && echo 4 > /proc/irq/$irq_num/smp_affinity

		irq_num=`grep pci0_reo2ppe /proc/interrupts | cut -d ':' -f 1 | tr -d ' '`
		[ -n "$irq_num" ] && echo 4 > /proc/irq/$irq_num/smp_affinity

		irq_num=`grep pci0_ppe_wbm_rel /proc/interrupts | cut -d ':' -f 1 | tr -d ' '`
		[ -n "$irq_num" ] && echo 4 > /proc/irq/$irq_num/smp_affinity

		##Extract the ppeds Node seq number and use same affinity to Node corresponding edma_ppeds interrupts
		ppeds_node=`grep pci0_reo2ppe_ /proc/interrupts | sed -n 's/.*pci0_reo2ppe_\([0-9]*\).*/\1/p'`
		irq_num=`grep edma_ppeds_rxfill_$ppeds_node /proc/interrupts | cut -d ':' -f 1 | tr -d ' '`
		[ -n "$irq_num" ] && echo 4 > /proc/irq/$irq_num/smp_affinity

		irq_num=`grep edma_ppeds_txcmpl_$ppeds_node /proc/interrupts | cut -d ':' -f 1 | tr -d ' '`
		[ -n "$irq_num" ] && echo 4 > /proc/irq/$irq_num/smp_affinity


		############## affinity for pci2 ########################
		irq_num=`grep pci2_ppe2tcl /proc/interrupts | cut -d ':' -f 1 | tr -d ' '`
		[ -n "$irq_num" ] && echo 2 > /proc/irq/$irq_num/smp_affinity

		irq_num=`grep pci2_reo2ppe /proc/interrupts | cut -d ':' -f 1 | tr -d ' '`
		[ -n "$irq_num" ] && echo 2 > /proc/irq/$irq_num/smp_affinity

		irq_num=`grep pci2_ppe_wbm_rel /proc/interrupts | cut -d ':' -f 1 | tr -d ' '`
		[ -n "$irq_num" ] && echo 2 > /proc/irq/$irq_num/smp_affinity

		##Extract the ppeds Node seq number and use same affinity to Node corresponding edma_ppeds interrupts
		ppeds_node=`grep pci2_reo2ppe_ /proc/interrupts | sed -n 's/.*pci2_reo2ppe_\([0-9]*\).*/\1/p'`
		irq_num=`grep edma_ppeds_rxfill_$ppeds_node /proc/interrupts | cut -d ':' -f 1 | tr -d ' '`
		[ -n "$irq_num" ] && echo 2 > /proc/irq/$irq_num/smp_affinity

		irq_num=`grep edma_ppeds_txcmpl_$ppeds_node /proc/interrupts | cut -d ':' -f 1 | tr -d ' '`
		[ -n "$irq_num" ] && echo 2 > /proc/irq/$irq_num/smp_affinity
	fi
}

enable_affinity_mi01_2() {

	#IPQ5332 2G radio
	#assign 4 rx interrupts to each cores
	irq_affinity_num=`grep -E -m1 'reo2host-destination-ring4' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 8 > /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=`grep -E -m1 'reo2host-destination-ring3' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 4 > /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=`grep -E -m1 'reo2host-destination-ring2' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 2 > /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=`grep -E -m1 'reo2host-destination-ring1' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 1 > /proc/irq/$irq_affinity_num/smp_affinity

	#assign 4 tcl completions to last 4 CPUs
	irq_affinity_num=`grep -E -m1 'wbm2host-tx-completions-ring4' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 8 > /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=`grep -E -m1 'wbm2host-tx-completions-ring3' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 4 > /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=`grep -E -m1 'wbm2host-tx-completions-ring2' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 2 > /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=`grep -E -m1 'wbm2host-tx-completions-ring1' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 1 > /proc/irq/$irq_affinity_num/smp_affinity

        #assign err,release interrupts to core 3
        irq_affinity_num=`grep -E -m1 'reo2ost-exception' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
        [ -n "$irq_affinity_num" ] && echo 8 > /proc/irq/$irq_affinity_num/smp_affinity
        irq_affinity_num=`grep -E -m1 'wbm2host-rx-release' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
        [ -n "$irq_affinity_num" ] && echo 8 > /proc/irq/$irq_affinity_num/smp_affinity
        irq_affinity_num=`grep -E -m1 'reo2host-status' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
        [ -n "$irq_affinity_num" ] && echo 8 > /proc/irq/$irq_affinity_num/smp_affinity

	#QCN9274 WKK 5G radio
	#pci 0
	#assign 4 rx interrupts to each cores
	irq_affinity_num=`grep -E -m1 'pci1_wlan_dp_4' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 1 > /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=`grep -E -m1 'pci1_wlan_dp_5' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 2 > /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=`grep -E -m1 'pci1_wlan_dp_6' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 4 > /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=`grep -E -m1 'pci1_wlan_dp_7' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 8 > /proc/irq/$irq_affinity_num/smp_affinity

	#assign 3 tcl completions to last 3 CPUs
	irq_affinity_num=`grep -E -m1 'pci1_wlan_dp_0' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 1 > /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=`grep -E -m1 'pci1_wlan_dp_1' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 2 > /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=`grep -E -m1 'pci1_wlan_dp_2' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 4 > /proc/irq/$irq_affinity_num/smp_affinity

	# lmac,reo err,release interrupts are mapped to core 3
	irq_affinity_num=`grep -E -m1 'pci1_wlan_dp_3' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 8 > /proc/irq/$irq_affinity_num/smp_affinity

	# assign 4th tcl completion ring interrupt to core 3 
	irq_affinity_num=`grep -E -m1 'pci1_wlan_dp_11' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
        [ -n "$irq_affinity_num" ] && echo 8 > /proc/irq/$irq_affinity_num/smp_affinity

	#QCN9274 WKK 6G radio
	#assign 4 rx interrupts to each cores from reverse
	irq_affinity_num=`grep -E -m1 'pci0_wlan_dp_4' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 1 > /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=`grep -E -m1 'pci0_wlan_dp_5' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 2 > /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=`grep -E -m1 'pci0_wlan_dp_6' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 4 > /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=`grep -E -m1 'pci0_wlan_dp_7' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 8 > /proc/irq/$irq_affinity_num/smp_affinity

	#assign 3 tcl completions to last 3 CPUs
	irq_affinity_num=`grep -E -m1 'pci0_wlan_dp_0' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 1 > /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=`grep -E -m1 'pci0_wlan_dp_1' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 2 > /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=`grep -E -m1 'pci0_wlan_dp_2' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 4 > /proc/irq/$irq_affinity_num/smp_affinity

	#assign 4th tcl completion ring and
	#lmac,reo err,release interrupts are mapped to core 3
	irq_affinity_num=`grep -E -m1 'pci0_wlan_dp_3' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 8 > /proc/irq/$irq_affinity_num/smp_affinity

	# assign 4th tcl completion ring interrupt to core 3 
        irq_affinity_num=`grep -E -m1 'pci0_wlan_dp_11' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
        [ -n "$irq_affinity_num" ] && echo 8 > /proc/irq/$irq_affinity_num/smp_affinity

	#For monitor interrupts
	irq_affinity_num=`grep -E -m1 'pci1_wlan_dp_8' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 2 > /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=`grep -E -m1 'pci0_wlan_dp_8' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 4 > /proc/irq/$irq_affinity_num/smp_affinity

	enable_affinity_for_ds=$(cat /sys/module/cfg80211/parameters/g_bonded_interface_model)
	if [ -n "$enable_affinity_for_ds" ] && [ $enable_affinity_for_ds == 'Y' ]; then
		echo "Configure Affinity for PPE DS for mi01_2" > /dev/ttyMSM0

		# For Miami Board (RDP 441) - Assign pci0(5G) to core 1 and
		#                             Assign pci1(6G) to core 2
		############## affinity for pci0 - 5G ########################
		irq_num=`grep pci0_ppe2tcl /proc/interrupts | cut -d ':' -f 1 | tr -d ' '`
		[ -n "$irq_num" ] && echo 2 > /proc/irq/$irq_num/smp_affinity

		irq_num=`grep pci0_reo2ppe /proc/interrupts | cut -d ':' -f 1 | tr -d ' '`
		[ -n "$irq_num" ] && echo 2 > /proc/irq/$irq_num/smp_affinity

		irq_num=`grep pci0_ppe_wbm_rel /proc/interrupts | cut -d ':' -f 1 | tr -d ' '`
		[ -n "$irq_num" ] && echo 2 > /proc/irq/$irq_num/smp_affinity

		##Extract the ppeds Node seq number and use same affinity to Node corresponding edma_ppeds interrupts
		ppeds_node=`grep pci0_reo2ppe_ /proc/interrupts | sed -n 's/.*pci0_reo2ppe_\([0-9]*\).*/\1/p'`
		irq_num=`grep edma_ppeds_rxfill_$ppeds_node /proc/interrupts | cut -d ':' -f 1 | tr -d ' '`
		[ -n "$irq_num" ] && echo 2 > /proc/irq/$irq_num/smp_affinity

		irq_num=`grep edma_ppeds_txcmpl_$ppeds_node /proc/interrupts | cut -d ':' -f 1 | tr -d ' '`
		[ -n "$irq_num" ] && echo 2 > /proc/irq/$irq_num/smp_affinity


		############## affinity for pci1 - 6G ########################
		irq_num=`grep pci1_ppe2tcl /proc/interrupts | cut -d ':' -f 1 | tr -d ' '`
		[ -n "$irq_num" ] && echo 4 > /proc/irq/$irq_num/smp_affinity

		irq_num=`grep pci1_reo2ppe /proc/interrupts | cut -d ':' -f 1 | tr -d ' '`
		[ -n "$irq_num" ] && echo 4 > /proc/irq/$irq_num/smp_affinity

		irq_num=`grep pci1_ppe_wbm_rel /proc/interrupts | cut -d ':' -f 1 | tr -d ' '`
		[ -n "$irq_num" ] && echo 4 > /proc/irq/$irq_num/smp_affinity

		##Extract the ppeds Node seq number and use same affinity to Node corresponding edma_ppeds interrupts
		ppeds_node=`grep pci1_reo2ppe_ /proc/interrupts | sed -n 's/.*pci1_reo2ppe_\([0-9]*\).*/\1/p'`
		irq_num=`grep edma_ppeds_rxfill_$ppeds_node /proc/interrupts | cut -d ':' -f 1 | tr -d ' '`
		[ -n "$irq_num" ] && echo 4 > /proc/irq/$irq_num/smp_affinity

		irq_num=`grep edma_ppeds_txcmpl_$ppeds_node /proc/interrupts | cut -d ':' -f 1 | tr -d ' '`
		[ -n "$irq_num" ] && echo 4 > /proc/irq/$irq_num/smp_affinity

	fi
}

enable_affinity_mi01_6() {

        #IPQ5332 2G radio
        #assign 4 rx interrupts to each cores
        irq_affinity_num=`grep -E -m1 'reo2host-destination-ring4' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
        [ -n "$irq_affinity_num" ] && echo 8 > /proc/irq/$irq_affinity_num/smp_affinity
        irq_affinity_num=`grep -E -m1 'reo2host-destination-ring3' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
        [ -n "$irq_affinity_num" ] && echo 4 > /proc/irq/$irq_affinity_num/smp_affinity
        irq_affinity_num=`grep -E -m1 'reo2host-destination-ring2' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
        [ -n "$irq_affinity_num" ] && echo 2 > /proc/irq/$irq_affinity_num/smp_affinity
        irq_affinity_num=`grep -E -m1 'reo2host-destination-ring1' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
        [ -n "$irq_affinity_num" ] && echo 1 > /proc/irq/$irq_affinity_num/smp_affinity

        #assign 4 tcl completions to last 4 CPUs
        irq_affinity_num=`grep -E -m1 'wbm2host-tx-completions-ring4' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
        [ -n "$irq_affinity_num" ] && echo 8 > /proc/irq/$irq_affinity_num/smp_affinity
        irq_affinity_num=`grep -E -m1 'wbm2host-tx-completions-ring3' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
        [ -n "$irq_affinity_num" ] && echo 4 > /proc/irq/$irq_affinity_num/smp_affinity
        irq_affinity_num=`grep -E -m1 'wbm2host-tx-completions-ring2' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
        [ -n "$irq_affinity_num" ] && echo 2 > /proc/irq/$irq_affinity_num/smp_affinity
        irq_affinity_num=`grep -E -m1 'wbm2host-tx-completions-ring1' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
        [ -n "$irq_affinity_num" ] && echo 1 > /proc/irq/$irq_affinity_num/smp_affinity

        #assign err,release interrupts to core 3
        irq_affinity_num=`grep -E -m1 'reo2ost-exception' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
        [ -n "$irq_affinity_num" ] && echo 8 > /proc/irq/$irq_affinity_num/smp_affinity
        irq_affinity_num=`grep -E -m1 'wbm2host-rx-release' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
        [ -n "$irq_affinity_num" ] && echo 8 > /proc/irq/$irq_affinity_num/smp_affinity
        irq_affinity_num=`grep -E -m1 'reo2host-status' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
        [ -n "$irq_affinity_num" ] && echo 8 > /proc/irq/$irq_affinity_num/smp_affinity

        #QCN9274 WKK 5G/6G radio
        #pci 1
        #assign 4 rx interrupts to each cores
        irq_affinity_num=`grep -E -m1 'pci1_wlan_dp_4' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
        [ -n "$irq_affinity_num" ] && echo 1 > /proc/irq/$irq_affinity_num/smp_affinity
        irq_affinity_num=`grep -E -m1 'pci1_wlan_dp_5' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
        [ -n "$irq_affinity_num" ] && echo 2 > /proc/irq/$irq_affinity_num/smp_affinity
        irq_affinity_num=`grep -E -m1 'pci1_wlan_dp_6' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
        [ -n "$irq_affinity_num" ] && echo 4 > /proc/irq/$irq_affinity_num/smp_affinity
        irq_affinity_num=`grep -E -m1 'pci1_wlan_dp_7' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
        [ -n "$irq_affinity_num" ] && echo 8 > /proc/irq/$irq_affinity_num/smp_affinity

        #assign 3 tcl completions to last 3 CPUs
        irq_affinity_num=`grep -E -m1 'pci1_wlan_dp_0' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
        [ -n "$irq_affinity_num" ] && echo 1 > /proc/irq/$irq_affinity_num/smp_affinity
        irq_affinity_num=`grep -E -m1 'pci1_wlan_dp_1' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
        [ -n "$irq_affinity_num" ] && echo 2 > /proc/irq/$irq_affinity_num/smp_affinity
        irq_affinity_num=`grep -E -m1 'pci1_wlan_dp_2' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
        [ -n "$irq_affinity_num" ] && echo 4 > /proc/irq/$irq_affinity_num/smp_affinity

        # lmac,reo err,release interrupts are mapped to core 3
        irq_affinity_num=`grep -E -m1 'pci1_wlan_dp_3' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
        [ -n "$irq_affinity_num" ] && echo 8 > /proc/irq/$irq_affinity_num/smp_affinity

	# assign 4th tcl completion ring interrupt to core 3 
        irq_affinity_num=`grep -E -m1 'pci1_wlan_dp_11' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
        [ -n "$irq_affinity_num" ] && echo 8 > /proc/irq/$irq_affinity_num/smp_affinity

	#For monitor interrupts
        irq_affinity_num=`grep -E -m1 'pci1_wlan_dp_8' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
        [ -n "$irq_affinity_num" ] && echo 2 > /proc/irq/$irq_affinity_num/smp_affinity

	enable_affinity_for_ds=$(cat /sys/module/cfg80211/parameters/g_bonded_interface_model)
	if [ -n "$enable_affinity_for_ds" ] && [ $enable_affinity_for_ds == 'Y' ]; then
		echo "Configure Affinity for PPE DS for mi01_6" > /dev/ttyMSM0

		# For Miami Board (RDP 468) - Assign pci1 to Core 2
		############## affinity for pci1  ########################
		irq_num=`grep pci1_ppe2tcl /proc/interrupts | cut -d ':' -f 1 | tr -d ' '`
		[ -n "$irq_num" ] && echo 4 > /proc/irq/$irq_num/smp_affinity

		irq_num=`grep pci1_reo2ppe /proc/interrupts | cut -d ':' -f 1 | tr -d ' '`
		[ -n "$irq_num" ] && echo 4 > /proc/irq/$irq_num/smp_affinity

		irq_num=`grep pci1_ppe_wbm_rel /proc/interrupts | cut -d ':' -f 1 | tr -d ' '`
		[ -n "$irq_num" ] && echo 4 > /proc/irq/$irq_num/smp_affinity

		##Extract the ppeds Node seq number and use same affinity to Node corresponding edma_ppeds interrupts
		ppeds_node=`grep pci1_reo2ppe_ /proc/interrupts | sed -n 's/.*pci1_reo2ppe_\([0-9]*\).*/\1/p'`
		irq_num=`grep edma_ppeds_rxfill_$ppeds_node /proc/interrupts | cut -d ':' -f 1 | tr -d ' '`
		[ -n "$irq_num" ] && echo 4 > /proc/irq/$irq_num/smp_affinity

		irq_num=`grep edma_ppeds_txcmpl_$ppeds_node /proc/interrupts | cut -d ':' -f 1 | tr -d ' '`
		[ -n "$irq_num" ] && echo 4 > /proc/irq/$irq_num/smp_affinity
	fi
}

enable_affinity_mi01_3() {

        #IPQ5332 2G radio
        #assign 4 rx interrupts to each cores
        irq_affinity_num=`grep -E -m1 'reo2host-destination-ring4' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
        [ -n "$irq_affinity_num" ] && echo 8 > /proc/irq/$irq_affinity_num/smp_affinity
        irq_affinity_num=`grep -E -m1 'reo2host-destination-ring3' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
        [ -n "$irq_affinity_num" ] && echo 4 > /proc/irq/$irq_affinity_num/smp_affinity
        irq_affinity_num=`grep -E -m1 'reo2host-destination-ring2' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
        [ -n "$irq_affinity_num" ] && echo 2 > /proc/irq/$irq_affinity_num/smp_affinity
        irq_affinity_num=`grep -E -m1 'reo2host-destination-ring1' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
        [ -n "$irq_affinity_num" ] && echo 1 > /proc/irq/$irq_affinity_num/smp_affinity

        #assign 4 tcl completions to last 4 CPUs
        irq_affinity_num=`grep -E -m1 'wbm2host-tx-completions-ring4' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
        [ -n "$irq_affinity_num" ] && echo 8 > /proc/irq/$irq_affinity_num/smp_affinity
        irq_affinity_num=`grep -E -m1 'wbm2host-tx-completions-ring3' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
        [ -n "$irq_affinity_num" ] && echo 4 > /proc/irq/$irq_affinity_num/smp_affinity
        irq_affinity_num=`grep -E -m1 'wbm2host-tx-completions-ring2' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
        [ -n "$irq_affinity_num" ] && echo 2 > /proc/irq/$irq_affinity_num/smp_affinity
        irq_affinity_num=`grep -E -m1 'wbm2host-tx-completions-ring1' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
        [ -n "$irq_affinity_num" ] && echo 1 > /proc/irq/$irq_affinity_num/smp_affinity

        #assign err,release interrupts to core 3
        irq_affinity_num=`grep -E -m1 'reo2ost-exception' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
        [ -n "$irq_affinity_num" ] && echo 8 > /proc/irq/$irq_affinity_num/smp_affinity
        irq_affinity_num=`grep -E -m1 'wbm2host-rx-release' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
        [ -n "$irq_affinity_num" ] && echo 8 > /proc/irq/$irq_affinity_num/smp_affinity
        irq_affinity_num=`grep -E -m1 'reo2host-status' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
        [ -n "$irq_affinity_num" ] && echo 8 > /proc/irq/$irq_affinity_num/smp_affinity

        #QCN6432 5G/6G radio
        #pci 1
        #assign 3 rx interrupts to each cores
        #rx interrupt,lmac,reo err,release interrupts are mapped to core 1
        irq_affinity_num=`grep -E -m1 'pcic1_wlan_dp_3' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
        [ -n "$irq_affinity_num" ] && echo 1 > /proc/irq/$irq_affinity_num/smp_affinity
	    irq_affinity_num=`grep -E -m1 'pcic1_wlan_dp_4' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
        [ -n "$irq_affinity_num" ] && echo 2 > /proc/irq/$irq_affinity_num/smp_affinity
        irq_affinity_num=`grep -E -m1 'pcic1_wlan_dp_5' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
        [ -n "$irq_affinity_num" ] && echo 4 > /proc/irq/$irq_affinity_num/smp_affinity

        #assign 3 tcl completions to last 3 CPUs
        irq_affinity_num=`grep -E -m1 'pcic1_wlan_dp_0' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
        [ -n "$irq_affinity_num" ] && echo 1 > /proc/irq/$irq_affinity_num/smp_affinity
        irq_affinity_num=`grep -E -m1 'pcic1_wlan_dp_1' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
        [ -n "$irq_affinity_num" ] && echo 2 > /proc/irq/$irq_affinity_num/smp_affinity
        irq_affinity_num=`grep -E -m1 'pcic1_wlan_dp_2' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
        [ -n "$irq_affinity_num" ] && echo 4 > /proc/irq/$irq_affinity_num/smp_affinity

	#QCN6432 6G radio
        #pci 2
	#assign 3 rx interrupts to each cores
	# rx interrupt,lmac,reo err,release interrupts are mapped to core 1
        irq_affinity_num=`grep -E -m1 'pcic2_wlan_dp_3' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
        [ -n "$irq_affinity_num" ] && echo 1 > /proc/irq/$irq_affinity_num/smp_affinity
        irq_affinity_num=`grep -E -m1 'pcic2_wlan_dp_4' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
        [ -n "$irq_affinity_num" ] && echo 2 > /proc/irq/$irq_affinity_num/smp_affinity
        irq_affinity_num=`grep -E -m1 'pcic2_wlan_dp_5' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
        [ -n "$irq_affinity_num" ] && echo 4 > /proc/irq/$irq_affinity_num/smp_affinity

        #assign 3 tcl completions to last 3 CPUs
        irq_affinity_num=`grep -E -m1 'pcic2_wlan_dp_0' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
        [ -n "$irq_affinity_num" ] && echo 1 > /proc/irq/$irq_affinity_num/smp_affinity
        irq_affinity_num=`grep -E -m1 'pcic2_wlan_dp_1' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
        [ -n "$irq_affinity_num" ] && echo 2 > /proc/irq/$irq_affinity_num/smp_affinity
        irq_affinity_num=`grep -E -m1 'pcic2_wlan_dp_2' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
        [ -n "$irq_affinity_num" ] && echo 4 > /proc/irq/$irq_affinity_num/smp_affinity



	enable_affinity_for_ds=$(cat /sys/module/cfg80211/parameters/g_bonded_interface_model)
	if [ -n "$enable_affinity_for_ds" ] && [ $enable_affinity_for_ds == 'Y' ]; then
		echo "Configure Affinity for PPE DS for mi01_3" > /dev/ttyMSM0

		#Assign 5G band to core 2 and
		#6G alone to core 1 to support upto 320MHz BW
		############## affinity for 5G band - pci1 ########################
		irq_num=`grep pci1_ppe2tcl /proc/interrupts | cut -d ':' -f 1 | tr -d ' '`
		[ -n "$irq_num" ] && echo 4 > /proc/irq/$irq_num/smp_affinity

		irq_num=`grep pci1_reo2ppe /proc/interrupts | cut -d ':' -f 1 | tr -d ' '`
		[ -n "$irq_num" ] && echo 4 > /proc/irq/$irq_num/smp_affinity

		irq_num=`grep pci1_ppe_wbm_rel /proc/interrupts | cut -d ':' -f 1 | tr -d ' '`
		[ -n "$irq_num" ] && echo 4 > /proc/irq/$irq_num/smp_affinity

		##Extract the ppeds Node seq number and use same affinity to Node corresponding edma_ppeds interrupts
		ppeds_node=`grep pci1_reo2ppe_ /proc/interrupts | sed -n 's/.*pci1_reo2ppe_\([0-9]*\).*/\1/p'`
		irq_num=`grep edma_ppeds_rxfill_$ppeds_node /proc/interrupts | cut -d ':' -f 1 | tr -d ' '`
		[ -n "$irq_num" ] && echo 4 > /proc/irq/$irq_num/smp_affinity

		irq_num=`grep edma_ppeds_txcmpl_$ppeds_node /proc/interrupts | cut -d ':' -f 1 | tr -d ' '`
		[ -n "$irq_num" ] && echo 4 > /proc/irq/$irq_num/smp_affinity


		############## affinity for 6G band - pci2 ########################
		irq_num=`grep pci2_ppe2tcl /proc/interrupts | cut -d ':' -f 1 | tr -d ' '`
		[ -n "$irq_num" ] && echo 2 > /proc/irq/$irq_num/smp_affinity

		irq_num=`grep pci2_reo2ppe /proc/interrupts | cut -d ':' -f 1 | tr -d ' '`
		[ -n "$irq_num" ] && echo 2 > /proc/irq/$irq_num/smp_affinity

		irq_num=`grep pci2_ppe_wbm_rel /proc/interrupts | cut -d ':' -f 1 | tr -d ' '`
		[ -n "$irq_num" ] && echo 2 > /proc/irq/$irq_num/smp_affinity

		##Extract the ppeds Node seq number and use same affinity to Node corresponding edma_ppeds interrupts
		ppeds_node=`grep pci2_reo2ppe_ /proc/interrupts | sed -n 's/.*pci2_reo2ppe_\([0-9]*\).*/\1/p'`
		irq_num=`grep edma_ppeds_rxfill_$ppeds_node /proc/interrupts | cut -d ':' -f 1 | tr -d ' '`
		[ -n "$irq_num" ] && echo 2 > /proc/irq/$irq_num/smp_affinity

		irq_num=`grep edma_ppeds_txcmpl_$ppeds_node /proc/interrupts | cut -d ':' -f 1 | tr -d ' '`
		[ -n "$irq_num" ] && echo 2 > /proc/irq/$irq_num/smp_affinity
	fi
}
enable_affinity_mi01_3_c2() {

        #IPQ5332 2G radio
        #assign 4 rx interrupts to each cores
        irq_affinity_num=`grep -E -m1 'reo2host-destination-ring4' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
        [ -n "$irq_affinity_num" ] && echo 8 > /proc/irq/$irq_affinity_num/smp_affinity
        irq_affinity_num=`grep -E -m1 'reo2host-destination-ring3' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
        [ -n "$irq_affinity_num" ] && echo 4 > /proc/irq/$irq_affinity_num/smp_affinity
        irq_affinity_num=`grep -E -m1 'reo2host-destination-ring2' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
        [ -n "$irq_affinity_num" ] && echo 2 > /proc/irq/$irq_affinity_num/smp_affinity
        irq_affinity_num=`grep -E -m1 'reo2host-destination-ring1' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
        [ -n "$irq_affinity_num" ] && echo 1 > /proc/irq/$irq_affinity_num/smp_affinity

        #assign 4 tcl completions to last 4 CPUs
        irq_affinity_num=`grep -E -m1 'wbm2host-tx-completions-ring4' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
        [ -n "$irq_affinity_num" ] && echo 8 > /proc/irq/$irq_affinity_num/smp_affinity
        irq_affinity_num=`grep -E -m1 'wbm2host-tx-completions-ring3' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
        [ -n "$irq_affinity_num" ] && echo 4 > /proc/irq/$irq_affinity_num/smp_affinity
        irq_affinity_num=`grep -E -m1 'wbm2host-tx-completions-ring2' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
        [ -n "$irq_affinity_num" ] && echo 2 > /proc/irq/$irq_affinity_num/smp_affinity
        irq_affinity_num=`grep -E -m1 'wbm2host-tx-completions-ring1' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
        [ -n "$irq_affinity_num" ] && echo 1 > /proc/irq/$irq_affinity_num/smp_affinity

        #assign err,release interrupts to core 3
        irq_affinity_num=`grep -E -m1 'reo2ost-exception' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
        [ -n "$irq_affinity_num" ] && echo 8 > /proc/irq/$irq_affinity_num/smp_affinity
        irq_affinity_num=`grep -E -m1 'wbm2host-rx-release' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
        [ -n "$irq_affinity_num" ] && echo 8 > /proc/irq/$irq_affinity_num/smp_affinity
        irq_affinity_num=`grep -E -m1 'reo2host-status' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
        [ -n "$irq_affinity_num" ] && echo 8 > /proc/irq/$irq_affinity_num/smp_affinity

        #QCN6432 5G radio
        #pci 1
        #assign 3 rx interrupts to each cores
        #rx interrupt,lmac,reo err,release interrupts are mapped to core 1

        irq_affinity_num=`grep -E -m1 'pcic1_wlan_dp_3' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
        [ -n "$irq_affinity_num" ] && echo 1 > /proc/irq/$irq_affinity_num/smp_affinity
        irq_affinity_num=`grep -E -m1 'pcic1_wlan_dp_4' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
        [ -n "$irq_affinity_num" ] && echo 2 > /proc/irq/$irq_affinity_num/smp_affinity
        irq_affinity_num=`grep -E -m1 'pcic1_wlan_dp_5' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
        [ -n "$irq_affinity_num" ] && echo 4 > /proc/irq/$irq_affinity_num/smp_affinity

        #assign 3 tcl completions to last 3 CPUs
        irq_affinity_num=`grep -E -m1 'pcic1_wlan_dp_0' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
        [ -n "$irq_affinity_num" ] && echo 1 > /proc/irq/$irq_affinity_num/smp_affinity
        irq_affinity_num=`grep -E -m1 'pcic1_wlan_dp_1' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
        [ -n "$irq_affinity_num" ] && echo 2 > /proc/irq/$irq_affinity_num/smp_affinity
        irq_affinity_num=`grep -E -m1 'pcic1_wlan_dp_2' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
        [ -n "$irq_affinity_num" ] && echo 4 > /proc/irq/$irq_affinity_num/smp_affinity

	enable_affinity_for_ds=$(cat /sys/module/cfg80211/parameters/g_bonded_interface_model)
	if [ -n "$enable_affinity_for_ds" ] && [ $enable_affinity_for_ds == 'Y' ]; then
		echo "Configure Affinity for PPE DS for mi01_3_c2" > /dev/ttyMSM0

		# For Miami Board (RDP 477 / RDP 478) - Assign pci1(5G) to core 2
		############## affinity for pci1 - 5G ########################
		irq_num=`grep pci1_ppe2tcl /proc/interrupts | cut -d ':' -f 1 | tr -d ' '`
		[ -n "$irq_num" ] && echo 4 > /proc/irq/$irq_num/smp_affinity

		irq_num=`grep pci1_reo2ppe /proc/interrupts | cut -d ':' -f 1 | tr -d ' '`
		[ -n "$irq_num" ] && echo 4 > /proc/irq/$irq_num/smp_affinity

		irq_num=`grep pci1_ppe_wbm_rel /proc/interrupts | cut -d ':' -f 1 | tr -d ' '`
		[ -n "$irq_num" ] && echo 4 > /proc/irq/$irq_num/smp_affinity

		##Extract the ppeds Node seq number and use same affinity to Node corresponding edma_ppeds interrupts
		ppeds_node=`grep pci1_reo2ppe_ /proc/interrupts | sed -n 's/.*pci1_reo2ppe_\([0-9]*\).*/\1/p'`
		irq_num=`grep edma_ppeds_rxfill_$ppeds_node /proc/interrupts | cut -d ':' -f 1 | tr -d ' '`
		[ -n "$irq_num" ] && echo 4 > /proc/irq/$irq_num/smp_affinity

		irq_num=`grep edma_ppeds_txcmpl_$ppeds_node /proc/interrupts | cut -d ':' -f 1 | tr -d ' '`
		[ -n "$irq_num" ] && echo 4 > /proc/irq/$irq_num/smp_affinity

	fi

}
enable_affinity_mi01_9() {
	#pci 0
	#assign 4 rx interrupts to each cores
	irq_affinity_num=`grep -E -m1 'pci0_wlan_dp_4' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 1 > /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=`grep -E -m1 'pci0_wlan_dp_5' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 2 > /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=`grep -E -m1 'pci0_wlan_dp_6' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 4 > /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=`grep -E -m1 'pci0_wlan_dp_7' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 8 > /proc/irq/$irq_affinity_num/smp_affinity

	#assign 3 tcl completions to last 3 CPUs
	irq_affinity_num=`grep -E -m1 'pci0_wlan_dp_0' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 1 > /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=`grep -E -m1 'pci0_wlan_dp_1' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 2 > /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=`grep -E -m1 'pci0_wlan_dp_2' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 4 > /proc/irq/$irq_affinity_num/smp_affinity

	# assign lmac,reo err,release interrupts are mapped to one core alone
	irq_affinity_num=`grep -E -m1 'pci0_wlan_dp_3' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 8 > /proc/irq/$irq_affinity_num/smp_affinity

	# assign 4th tcl completion ring interrupt to core 3
	irq_affinity_num=`grep -E -m1 'pci0_wlan_dp_11' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 8 > /proc/irq/$irq_affinity_num/smp_affinity

	# Enable smp affinity for PCIE attach
	#pci 1

	#assign 4 rx interrupts to each cores
	irq_affinity_num=`grep -E -m1 'pci1_wlan_dp_4' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 1 > /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=`grep -E -m1 'pci1_wlan_dp_5' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 2 > /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=`grep -E -m1 'pci1_wlan_dp_6' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 4 > /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=`grep -E -m1 'pci1_wlan_dp_7' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 8 > /proc/irq/$irq_affinity_num/smp_affinity

	#assign 3 tcl completions to last 3 CPUs
	irq_affinity_num=`grep -E -m1 'pci1_wlan_dp_0' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 1 > /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=`grep -E -m1 'pci1_wlan_dp_1' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 2 > /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=`grep -E -m1 'pci1_wlan_dp_2' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 4 > /proc/irq/$irq_affinity_num/smp_affinity

	# assign lmac,reo err,release interrupts are mapped to one core alone
	irq_affinity_num=`grep -E -m1 'pci1_wlan_dp_3' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 8 > /proc/irq/$irq_affinity_num/smp_affinity

	# assign 4th tcl completion ring interrupt to core 3
	irq_affinity_num=`grep -E -m1 'pci1_wlan_dp_11' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 8 > /proc/irq/$irq_affinity_num/smp_affinity

}

enable_affinity_mi01_14() {

	#IPQ5332 2G radio
	#assign 4 rx interrupts to each cores
	irq_affinity_num=`grep -E -m1 'reo2host-destination-ring4' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 8 > /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=`grep -E -m1 'reo2host-destination-ring3' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 4 > /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=`grep -E -m1 'reo2host-destination-ring2' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 2 > /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=`grep -E -m1 'reo2host-destination-ring1' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 1 > /proc/irq/$irq_affinity_num/smp_affinity

	#assign 4 tcl completions to last 4 CPUs
	irq_affinity_num=`grep -E -m1 'wbm2host-tx-completions-ring4' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 8 > /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=`grep -E -m1 'wbm2host-tx-completions-ring3' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 4 > /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=`grep -E -m1 'wbm2host-tx-completions-ring2' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 2 > /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=`grep -E -m1 'wbm2host-tx-completions-ring1' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 1 > /proc/irq/$irq_affinity_num/smp_affinity

        #assign err,release interrupts to core 3
        irq_affinity_num=`grep -E -m1 'reo2ost-exception' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
        [ -n "$irq_affinity_num" ] && echo 8 > /proc/irq/$irq_affinity_num/smp_affinity
        irq_affinity_num=`grep -E -m1 'wbm2host-rx-release' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
        [ -n "$irq_affinity_num" ] && echo 8 > /proc/irq/$irq_affinity_num/smp_affinity
        irq_affinity_num=`grep -E -m1 'reo2host-status' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
        [ -n "$irq_affinity_num" ] && echo 8 > /proc/irq/$irq_affinity_num/smp_affinity

	#QCN6432 5G radio
	#pci 1
	#assign 3 rx interrupts to each cores
	#rx interrupt,lmac,reo err,release interrupts are mapped to core 1
	irq_affinity_num=`grep -E -m1 'pcic1_wlan_dp_3' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 1 > /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=`grep -E -m1 'pcic1_wlan_dp_4' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 2 > /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=`grep -E -m1 'pcic1_wlan_dp_5' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 4 > /proc/irq/$irq_affinity_num/smp_affinity

	#assign 3 tcl completions to last 3 CPUs
	irq_affinity_num=`grep -E -m1 'pcic1_wlan_dp_0' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 1 > /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=`grep -E -m1 'pcic1_wlan_dp_1' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 2 > /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=`grep -E -m1 'pcic1_wlan_dp_2' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 4 > /proc/irq/$irq_affinity_num/smp_affinity

	#QCN9274 WKK 6G radio
	#assign 4 rx interrupts to each cores from reverse
	irq_affinity_num=`grep -E -m1 'pci1_wlan_dp_4' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 1 > /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=`grep -E -m1 'pci1_wlan_dp_5' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 2 > /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=`grep -E -m1 'pci1_wlan_dp_6' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 4 > /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=`grep -E -m1 'pci1_wlan_dp_7' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 8 > /proc/irq/$irq_affinity_num/smp_affinity

	#assign 3 tcl completions to last 3 CPUs
	irq_affinity_num=`grep -E -m1 'pci1_wlan_dp_0' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 1 > /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=`grep -E -m1 'pci1_wlan_dp_1' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 2 > /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=`grep -E -m1 'pci1_wlan_dp_2' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 4 > /proc/irq/$irq_affinity_num/smp_affinity

	#assign 4th tcl completion ring and
	#lmac,reo err,release interrupts are mapped to core 3
	irq_affinity_num=`grep -E -m1 'pci1_wlan_dp_3' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 8 > /proc/irq/$irq_affinity_num/smp_affinity

	# assign 4th tcl completion ring interrupt to core 3 
        irq_affinity_num=`grep -E -m1 'pci1_wlan_dp_11' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
        [ -n "$irq_affinity_num" ] && echo 8 > /proc/irq/$irq_affinity_num/smp_affinity

	#For monitor interrupts
	irq_affinity_num=`grep -E -m1 'pci1_wlan_dp_8' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 4 > /proc/irq/$irq_affinity_num/smp_affinity

	enable_affinity_for_ds=$(cat /sys/module/cfg80211/parameters/g_bonded_interface_model)
	if [ -n "$enable_affinity_for_ds" ] && [ $enable_affinity_for_ds == 'Y' ]; then
		echo "Configure Affinity for PPE DS for mi01_2" > /dev/ttyMSM0

		# For Miami Board (RDP 481) - Assign pcic1(5G) to core 1 and
		#                             Assign pci1(6G) to core 2
		############## affinity for pci0 - 5G ########################
		irq_num=`grep pci0_ppe2tcl /proc/interrupts | cut -d ':' -f 1 | tr -d ' '`
		[ -n "$irq_num" ] && echo 2 > /proc/irq/$irq_num/smp_affinity

		irq_num=`grep pci0_reo2ppe /proc/interrupts | cut -d ':' -f 1 | tr -d ' '`
		[ -n "$irq_num" ] && echo 2 > /proc/irq/$irq_num/smp_affinity

		irq_num=`grep pci0_ppe_wbm_rel /proc/interrupts | cut -d ':' -f 1 | tr -d ' '`
		[ -n "$irq_num" ] && echo 2 > /proc/irq/$irq_num/smp_affinity

		##Extract the ppeds Node seq number and use same affinity to Node corresponding edma_ppeds interrupts
		ppeds_node=`grep pci0_reo2ppe_ /proc/interrupts | sed -n 's/.*pci0_reo2ppe_\([0-9]*\).*/\1/p'`
		irq_num=`grep edma_ppeds_rxfill_$ppeds_node /proc/interrupts | cut -d ':' -f 1 | tr -d ' '`
		[ -n "$irq_num" ] && echo 2 > /proc/irq/$irq_num/smp_affinity

		irq_num=`grep edma_ppeds_txcmpl_$ppeds_node /proc/interrupts | cut -d ':' -f 1 | tr -d ' '`
		[ -n "$irq_num" ] && echo 2 > /proc/irq/$irq_num/smp_affinity


		############## affinity for pci1 - 6G ########################
		irq_num=`grep pci1_ppe2tcl /proc/interrupts | cut -d ':' -f 1 | tr -d ' '`
		[ -n "$irq_num" ] && echo 4 > /proc/irq/$irq_num/smp_affinity

		irq_num=`grep pci1_reo2ppe /proc/interrupts | cut -d ':' -f 1 | tr -d ' '`
		[ -n "$irq_num" ] && echo 4 > /proc/irq/$irq_num/smp_affinity

		irq_num=`grep pci1_ppe_wbm_rel /proc/interrupts | cut -d ':' -f 1 | tr -d ' '`
		[ -n "$irq_num" ] && echo 4 > /proc/irq/$irq_num/smp_affinity

		##Extract the ppeds Node seq number and use same affinity to Node corresponding edma_ppeds interrupts
		ppeds_node=`grep pci1_reo2ppe_ /proc/interrupts | sed -n 's/.*pci1_reo2ppe_\([0-9]*\).*/\1/p'`
		irq_num=`grep edma_ppeds_rxfill_$ppeds_node /proc/interrupts | cut -d ':' -f 1 | tr -d ' '`
		[ -n "$irq_num" ] && echo 4 > /proc/irq/$irq_num/smp_affinity

		irq_num=`grep edma_ppeds_txcmpl_$ppeds_node /proc/interrupts | cut -d ':' -f 1 | tr -d ' '`
		[ -n "$irq_num" ] && echo 4 > /proc/irq/$irq_num/smp_affinity

	fi
}

enable_affinity_mi01_12() {

	#IPQ5332 2G radio
	#assign 4 rx interrupts to each cores
	irq_affinity_num=`grep -E -m1 'reo2host-destination-ring4' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 8 > /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=`grep -E -m1 'reo2host-destination-ring3' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 4 > /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=`grep -E -m1 'reo2host-destination-ring2' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 2 > /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=`grep -E -m1 'reo2host-destination-ring1' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 1 > /proc/irq/$irq_affinity_num/smp_affinity

	#assign 4 tcl completions to last 4 CPUs
	irq_affinity_num=`grep -E -m1 'wbm2host-tx-completions-ring4' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 8 > /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=`grep -E -m1 'wbm2host-tx-completions-ring3' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 4 > /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=`grep -E -m1 'wbm2host-tx-completions-ring2' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 2 > /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=`grep -E -m1 'wbm2host-tx-completions-ring1' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 1 > /proc/irq/$irq_affinity_num/smp_affinity

        #assign err,release interrupts to core 3
        irq_affinity_num=`grep -E -m1 'reo2ost-exception' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
        [ -n "$irq_affinity_num" ] && echo 8 > /proc/irq/$irq_affinity_num/smp_affinity
        irq_affinity_num=`grep -E -m1 'wbm2host-rx-release' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
        [ -n "$irq_affinity_num" ] && echo 8 > /proc/irq/$irq_affinity_num/smp_affinity
        irq_affinity_num=`grep -E -m1 'reo2host-status' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
        [ -n "$irq_affinity_num" ] && echo 8 > /proc/irq/$irq_affinity_num/smp_affinity

	#QCN9274 WKK 5G radio
	#pci 0
	#assign 4 rx interrupts to each cores
	irq_affinity_num=`grep -E -m1 'pci1_wlan_dp_4' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 1 > /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=`grep -E -m1 'pci1_wlan_dp_5' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 2 > /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=`grep -E -m1 'pci1_wlan_dp_6' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 4 > /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=`grep -E -m1 'pci1_wlan_dp_7' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 8 > /proc/irq/$irq_affinity_num/smp_affinity

	#assign 3 tcl completions to last 3 CPUs
	irq_affinity_num=`grep -E -m1 'pci1_wlan_dp_0' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 1 > /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=`grep -E -m1 'pci1_wlan_dp_1' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 2 > /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=`grep -E -m1 'pci1_wlan_dp_2' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 4 > /proc/irq/$irq_affinity_num/smp_affinity

	# lmac,reo err,release interrupts are mapped to core 3
	irq_affinity_num=`grep -E -m1 'pci1_wlan_dp_3' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 8 > /proc/irq/$irq_affinity_num/smp_affinity

	# assign 4th tcl completion ring interrupt to core 3 
	irq_affinity_num=`grep -E -m1 'pci1_wlan_dp_11' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
        [ -n "$irq_affinity_num" ] && echo 8 > /proc/irq/$irq_affinity_num/smp_affinity

	#QCN6432  6G radio
    #pci 2
	#assign 3 rx interrupts to each cores
	# rx interrupt,lmac,reo err,release interrupts are mapped to core 1
    irq_affinity_num=`grep -E -m1 'pcic1_wlan_dp_3' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
    [ -n "$irq_affinity_num" ] && echo 1 > /proc/irq/$irq_affinity_num/smp_affinity
    irq_affinity_num=`grep -E -m1 'pcic1_wlan_dp_4' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
    [ -n "$irq_affinity_num" ] && echo 2 > /proc/irq/$irq_affinity_num/smp_affinity
    irq_affinity_num=`grep -E -m1 'pcic1_wlan_dp_5' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
    [ -n "$irq_affinity_num" ] && echo 4 > /proc/irq/$irq_affinity_num/smp_affinity

    #assign 3 tcl completions to last 3 CPUs
    irq_affinity_num=`grep -E -m1 'pcic1_wlan_dp_0' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
    [ -n "$irq_affinity_num" ] && echo 1 > /proc/irq/$irq_affinity_num/smp_affinity
    irq_affinity_num=`grep -E -m1 'pcic1_wlan_dp_1' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
    [ -n "$irq_affinity_num" ] && echo 2 > /proc/irq/$irq_affinity_num/smp_affinity
    irq_affinity_num=`grep -E -m1 'pcic1_wlan_dp_2' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
    [ -n "$irq_affinity_num" ] && echo 4 > /proc/irq/$irq_affinity_num/smp_affinity

	#For monitor interrupts
	irq_affinity_num=`grep -E -m1 'pci1_wlan_dp_8' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' '`
	[ -n "$irq_affinity_num" ] && echo 2 > /proc/irq/$irq_affinity_num/smp_affinity

	enable_affinity_for_ds=$(cat /sys/module/cfg80211/parameters/g_bonded_interface_model)
	if [ -n "$enable_affinity_for_ds" ] && [ $enable_affinity_for_ds == 'Y' ]; then
		echo "Configure Affinity for PPE DS for mi01_2" > /dev/ttyMSM0

		# For Miami Board (RDP 479) - Assign pci1(5G) to core 1 and
		#                             Assign pcic1(6G) to core 2
		############## affinity for pci1 - 5G ########################
		irq_num=`grep pci0_ppe2tcl /proc/interrupts | cut -d ':' -f 1 | tr -d ' '`
		[ -n "$irq_num" ] && echo 2 > /proc/irq/$irq_num/smp_affinity

		irq_num=`grep pci0_reo2ppe /proc/interrupts | cut -d ':' -f 1 | tr -d ' '`
		[ -n "$irq_num" ] && echo 2 > /proc/irq/$irq_num/smp_affinity

		irq_num=`grep pci0_ppe_wbm_rel /proc/interrupts | cut -d ':' -f 1 | tr -d ' '`
		[ -n "$irq_num" ] && echo 2 > /proc/irq/$irq_num/smp_affinity

		##Extract the ppeds Node seq number and use same affinity to Node corresponding edma_ppeds interrupts
		ppeds_node=`grep pci0_reo2ppe_ /proc/interrupts | sed -n 's/.*pci0_reo2ppe_\([0-9]*\).*/\1/p'`
		irq_num=`grep edma_ppeds_rxfill_$ppeds_node /proc/interrupts | cut -d ':' -f 1 | tr -d ' '`
		[ -n "$irq_num" ] && echo 2 > /proc/irq/$irq_num/smp_affinity

		irq_num=`grep edma_ppeds_txcmpl_$ppeds_node /proc/interrupts | cut -d ':' -f 1 | tr -d ' '`
		[ -n "$irq_num" ] && echo 2 > /proc/irq/$irq_num/smp_affinity


		############## affinity for pcic1 - 6G ########################
		irq_num=`grep pci1_ppe2tcl /proc/interrupts | cut -d ':' -f 1 | tr -d ' '`
		[ -n "$irq_num" ] && echo 4 > /proc/irq/$irq_num/smp_affinity

		irq_num=`grep pci1_reo2ppe /proc/interrupts | cut -d ':' -f 1 | tr -d ' '`
		[ -n "$irq_num" ] && echo 4 > /proc/irq/$irq_num/smp_affinity

		irq_num=`grep pci1_ppe_wbm_rel /proc/interrupts | cut -d ':' -f 1 | tr -d ' '`
		[ -n "$irq_num" ] && echo 4 > /proc/irq/$irq_num/smp_affinity

		##Extract the ppeds Node seq number and use same affinity to Node corresponding edma_ppeds interrupts
		ppeds_node=`grep pci1_reo2ppe_ /proc/interrupts | sed -n 's/.*pci1_reo2ppe_\([0-9]*\).*/\1/p'`
		irq_num=`grep edma_ppeds_rxfill_$ppeds_node /proc/interrupts | cut -d ':' -f 1 | tr -d ' '`
		[ -n "$irq_num" ] && echo 4 > /proc/irq/$irq_num/smp_affinity

		irq_num=`grep edma_ppeds_txcmpl_$ppeds_node /proc/interrupts | cut -d ':' -f 1 | tr -d ' '`
		[ -n "$irq_num" ] && echo 4 > /proc/irq/$irq_num/smp_affinity

	fi
}
enable_smp_affinity_wifi() {

	# set smp_affinity for Lithium(ATH11k) and Beriliyum(ATH12k)
        if [ -d "/sys/kernel/debug/ath11k" ] || [ -d "/sys/kernel/debug/ath12k" ]; then
		local board=$(ipq806x_board_name)

	if [ -e /sys/module/ath11k/parameters/nss_offload ];then
		uni_dp=0
	else
		uni_dp=1
	fi

		case "$board" in
			ap-cp01-c1 | \
			ap-oak03 | \
			ap-hk01-c1)
					#case for rdp393,rdp385,rdp352
					enable_affinity_hk_cp01_c1
					;;
			ap-hk10-c2)
					#case for rdp413
					enable_affinity_hk10_c2
					;;
			ap-hk10-c1)
					#case for rdp412
					enable_affinity_hk10
					enable_affinity_hk_cp01_c1
					;;
			ap-hk14)
					#case for rdp419
					enable_affinity_hk14
					;;
			ap-mp03.5-c1)
					#case for RDP432 UniDP
					if [ "$uni_dp" -eq 1 ];then
						enable_affinity_mpc1
					fi
					;;
			ap-al02-c1)
					#case for rdp418
					enable_affinity_al02_c1
					;;
			ap-al02-c4 | \
			ap-al02-c8 | \
			ap-al02-c10)
					#case for rdp433 (QCN9274 2.4, 5, 6 GHz)
					enable_affinity_al02_c4
					;;
			ap-al02-c6)
					#case for rdp433 (IPQ9574(2.4 GHz) + QCN9274(5 and 6 GHz))
					enable_affinity_al02_c6
					;;
			ap-al02-c9)
					#case for rdp454 (QCN9274 (2.4 and 5 Low) + QCN9274 (5 High and 6 GHz))
					enable_affinity_al02_c9
					;;
			ap-mi01.2)
					#for RDP441 (IPQ5332(2.4GHz) + QCN9274(5 and 6 GHz))
					enable_affinity_mi01_2
					;;
			ap-mi01.6)
                                        #for RDP468 (IPQ5332(2.4GHz) + QCN9274(5/6 GHz))
                                        enable_affinity_mi01_6
                                        ;;
			ap-mi01.3 | \
			ap-mi04.1)
					#for RDP442, RDP446 (IPQ5332(2.4GHz) + QCN6432(5/6 GHz)))
					enable_affinity_mi01_3
					;;
			ap-mi01.3-c2 | \
			ap-mi04.1-c2)
					#for RDP477, 478(IPQ5332(2.4GHz) + QCN6432(5GHz))
					enable_affinity_mi01_3_c2
					;;
			ap-mi01.9)
					#for RDP474(QCN9274 2, 5 GHz)
					enable_affinity_mi01_9
					;;
			ap-mi01.14)
					#for RDP481 (IPQ5332(2.4GHz) + QCN6432(5 GHz) + QCN9274(6GHz))
					enable_affinity_mi01_14
					;;
			ap-mi01.12)
                        		#for RDP479 (IPQ5332(2.4GHz) + QCN9274(5 GHz) + QCN6432(6 GHz))
                                        enable_affinity_mi01_12
                                        ;;
			*)
					#no affinity settings
					;;
		esac
	fi
}
