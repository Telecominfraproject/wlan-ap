// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2016-2018,2020 The Linux Foundation. All rights reserved.
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/pinctrl/pinctrl.h>

#include "pinctrl-msm.h"

#define FUNCTION(fname)			                \
	[msm_mux_##fname] = {		                \
		.name = #fname,				\
		.groups = fname##_groups,               \
		.ngroups = ARRAY_SIZE(fname##_groups),	\
	}

#define REG_SIZE 0x1000
#define PINGROUP(id, f1, f2, f3, f4, f5, f6, f7, f8, f9)	\
	{					        \
		.name = "gpio" #id,			\
		.pins = gpio##id##_pins,		\
		.npins = (unsigned int)ARRAY_SIZE(gpio##id##_pins),	\
		.funcs = (int[]){			\
			msm_mux_gpio, /* gpio mode */	\
			msm_mux_##f1,			\
			msm_mux_##f2,			\
			msm_mux_##f3,			\
			msm_mux_##f4,			\
			msm_mux_##f5,			\
			msm_mux_##f6,			\
			msm_mux_##f7,			\
			msm_mux_##f8,			\
			msm_mux_##f9			\
		},				        \
		.nfuncs = 10,				\
		.ctl_reg = REG_SIZE * id,	        \
		.io_reg = 0x4 + REG_SIZE * id,		\
		.intr_cfg_reg = 0x8 + REG_SIZE * id,	\
		.intr_status_reg = 0xc + REG_SIZE * id,	\
		.intr_target_reg = 0x8 + REG_SIZE * id,	\
		.mux_bit = 2,			\
		.pull_bit = 0,			\
		.drv_bit = 6,			\
		.oe_bit = 9,			\
		.in_bit = 0,			\
		.out_bit = 1,			\
		.intr_enable_bit = 0,		\
		.intr_status_bit = 0,		\
		.intr_target_bit = 5,		\
		.intr_target_kpss_val = 3,      \
		.intr_raw_status_bit = 4,	\
		.intr_polarity_bit = 1,		\
		.intr_detection_bit = 2,	\
		.intr_detection_width = 2,	\
	}

static const struct pinctrl_pin_desc devsoc_pins[] = {
	PINCTRL_PIN(0, "GPIO_0"),
	PINCTRL_PIN(1, "GPIO_1"),
	PINCTRL_PIN(2, "GPIO_2"),
	PINCTRL_PIN(3, "GPIO_3"),
	PINCTRL_PIN(4, "GPIO_4"),
	PINCTRL_PIN(5, "GPIO_5"),
	PINCTRL_PIN(6, "GPIO_6"),
	PINCTRL_PIN(7, "GPIO_7"),
	PINCTRL_PIN(8, "GPIO_8"),
	PINCTRL_PIN(9, "GPIO_9"),
	PINCTRL_PIN(10, "GPIO_10"),
	PINCTRL_PIN(11, "GPIO_11"),
	PINCTRL_PIN(12, "GPIO_12"),
	PINCTRL_PIN(13, "GPIO_13"),
	PINCTRL_PIN(14, "GPIO_14"),
	PINCTRL_PIN(15, "GPIO_15"),
	PINCTRL_PIN(16, "GPIO_16"),
	PINCTRL_PIN(17, "GPIO_17"),
	PINCTRL_PIN(18, "GPIO_18"),
	PINCTRL_PIN(19, "GPIO_19"),
	PINCTRL_PIN(20, "GPIO_20"),
	PINCTRL_PIN(21, "GPIO_21"),
	PINCTRL_PIN(22, "GPIO_22"),
	PINCTRL_PIN(23, "GPIO_23"),
	PINCTRL_PIN(24, "GPIO_24"),
	PINCTRL_PIN(25, "GPIO_25"),
	PINCTRL_PIN(26, "GPIO_26"),
	PINCTRL_PIN(27, "GPIO_27"),
	PINCTRL_PIN(28, "GPIO_28"),
	PINCTRL_PIN(29, "GPIO_29"),
	PINCTRL_PIN(30, "GPIO_30"),
	PINCTRL_PIN(31, "GPIO_31"),
	PINCTRL_PIN(32, "GPIO_32"),
	PINCTRL_PIN(33, "GPIO_33"),
	PINCTRL_PIN(34, "GPIO_34"),
	PINCTRL_PIN(35, "GPIO_35"),
	PINCTRL_PIN(36, "GPIO_36"),
	PINCTRL_PIN(37, "GPIO_37"),
	PINCTRL_PIN(38, "GPIO_38"),
	PINCTRL_PIN(39, "GPIO_39"),
	PINCTRL_PIN(40, "GPIO_40"),
	PINCTRL_PIN(41, "GPIO_41"),
	PINCTRL_PIN(42, "GPIO_42"),
	PINCTRL_PIN(43, "GPIO_43"),
	PINCTRL_PIN(44, "GPIO_44"),
	PINCTRL_PIN(45, "GPIO_45"),
	PINCTRL_PIN(46, "GPIO_46"),
	PINCTRL_PIN(47, "GPIO_47"),
	PINCTRL_PIN(48, "GPIO_48"),
	PINCTRL_PIN(49, "GPIO_49"),
};

#define DECLARE_MSM_GPIO_PINS(pin) \
	static const unsigned int gpio##pin##_pins[] = { pin }
DECLARE_MSM_GPIO_PINS(0);
DECLARE_MSM_GPIO_PINS(1);
DECLARE_MSM_GPIO_PINS(2);
DECLARE_MSM_GPIO_PINS(3);
DECLARE_MSM_GPIO_PINS(4);
DECLARE_MSM_GPIO_PINS(5);
DECLARE_MSM_GPIO_PINS(6);
DECLARE_MSM_GPIO_PINS(7);
DECLARE_MSM_GPIO_PINS(8);
DECLARE_MSM_GPIO_PINS(9);
DECLARE_MSM_GPIO_PINS(10);
DECLARE_MSM_GPIO_PINS(11);
DECLARE_MSM_GPIO_PINS(12);
DECLARE_MSM_GPIO_PINS(13);
DECLARE_MSM_GPIO_PINS(14);
DECLARE_MSM_GPIO_PINS(15);
DECLARE_MSM_GPIO_PINS(16);
DECLARE_MSM_GPIO_PINS(17);
DECLARE_MSM_GPIO_PINS(18);
DECLARE_MSM_GPIO_PINS(19);
DECLARE_MSM_GPIO_PINS(20);
DECLARE_MSM_GPIO_PINS(21);
DECLARE_MSM_GPIO_PINS(22);
DECLARE_MSM_GPIO_PINS(23);
DECLARE_MSM_GPIO_PINS(24);
DECLARE_MSM_GPIO_PINS(25);
DECLARE_MSM_GPIO_PINS(26);
DECLARE_MSM_GPIO_PINS(27);
DECLARE_MSM_GPIO_PINS(28);
DECLARE_MSM_GPIO_PINS(29);
DECLARE_MSM_GPIO_PINS(30);
DECLARE_MSM_GPIO_PINS(31);
DECLARE_MSM_GPIO_PINS(32);
DECLARE_MSM_GPIO_PINS(33);
DECLARE_MSM_GPIO_PINS(34);
DECLARE_MSM_GPIO_PINS(35);
DECLARE_MSM_GPIO_PINS(36);
DECLARE_MSM_GPIO_PINS(37);
DECLARE_MSM_GPIO_PINS(38);
DECLARE_MSM_GPIO_PINS(39);
DECLARE_MSM_GPIO_PINS(40);
DECLARE_MSM_GPIO_PINS(41);
DECLARE_MSM_GPIO_PINS(42);
DECLARE_MSM_GPIO_PINS(43);
DECLARE_MSM_GPIO_PINS(44);
DECLARE_MSM_GPIO_PINS(45);
DECLARE_MSM_GPIO_PINS(46);
DECLARE_MSM_GPIO_PINS(47);
DECLARE_MSM_GPIO_PINS(48);
DECLARE_MSM_GPIO_PINS(49);

enum devsoc_functions {
	msm_mux_atest_char,
	msm_mux_atest_char0,
	msm_mux_atest_char1,
	msm_mux_atest_char2,
	msm_mux_atest_char3,
	msm_mux_atest_tic,
	msm_mux_audio_pri,
	msm_mux_audio_pri0,
	msm_mux_audio_pri1,
	msm_mux_audio_sec,
	msm_mux_audio_sec0,
	msm_mux_audio_sec1,
	msm_mux_burn_in,
	msm_mux_burn_in0,
	msm_mux_burn_in1,
	msm_mux_core_voltage,
	msm_mux_cri_trng0,
	msm_mux_cri_trng1,
	msm_mux_cri_trng2,
	msm_mux_cri_trng3,
	msm_mux_cxc_clk,
	msm_mux_cxc_data,
	msm_mux_dbg_out,
	msm_mux_gcc_plltest,
	msm_mux_gcc_tlmm,
	msm_mux_gpio,
	msm_mux_i2c0_scl,
	msm_mux_i2c0_sda,
	msm_mux_i2c1_scl,
	msm_mux_i2c1_sda,
	msm_mux_i2c11,
	msm_mux_mac0,
	msm_mux_mac1,
	msm_mux_mdc_mst,
	msm_mux_mdc_slv,
	msm_mux_mdio_mst,
	msm_mux_mdio_slv,
	msm_mux_pcie0_clk,
	msm_mux_pcie0_wake,
	msm_mux_pcie1_clk,
	msm_mux_pcie1_wake,
	msm_mux_pcie2_clk,
	msm_mux_pcie2_wake,
	msm_mux_pcie3_clk,
	msm_mux_pcie3_wake,
	msm_mux_pll_test,
	msm_mux_prng_rosc0,
	msm_mux_prng_rosc1,
	msm_mux_prng_rosc2,
	msm_mux_prng_rosc3,
	msm_mux_PTA_0,
	msm_mux_PTA_1,
	msm_mux_PTA_2,
	msm_mux_PTA10,
	msm_mux_PTA11,
	msm_mux_pwm0,
	msm_mux_pwm1,
	msm_mux_pwm2,
	msm_mux_qdss_cti_trig_in_a0,
	msm_mux_qdss_cti_trig_out_a0,
	msm_mux_qdss_cti_trig_out_a1,
	msm_mux_qdss_cti_trig_out_b1,
	msm_mux_qdss_traceclk_a,
	msm_mux_qdss_tracectl_a,
	msm_mux_qdss_tracedata_a,
	msm_mux_qspi_clk,
	msm_mux_qspi_cs,
	msm_mux_qspi_data,
	msm_mux_resout,
	msm_mux_rx0,
	msm_mux_rx1,
	msm_mux_sdc_clk,
	msm_mux_sdc_cmd,
	msm_mux_sdc_data,
	msm_mux_spi0_clk,
	msm_mux_spi0_cs,
	msm_mux_spi0_miso,
	msm_mux_spi0_mosi,
	msm_mux_spi1_clk,
	msm_mux_spi1_cs,
	msm_mux_spi1_cs1,
	msm_mux_spi1_cs2,
	msm_mux_spi1_cs3,
	msm_mux_spi1_miso,
	msm_mux_spi1_mosi,
	msm_mux_tsens_max,
	msm_mux_uart0_cts,
	msm_mux_uart0_rfr,
	msm_mux_uart0_rx,
	msm_mux_uart0_tx,
	msm_mux_uart1_rx,
	msm_mux_uart1_tx,
	msm_mux_wci0,
	msm_mux_wci1,
	msm_mux_wci2,
	msm_mux_wci3,
	msm_mux_wci4,
	msm_mux_wci5,
	msm_mux_wci6,
	msm_mux__,
};

static const char * const gpio_groups[] = {
        "gpio0", "gpio1", "gpio2", "gpio3", "gpio4", "gpio5", "gpio6", "gpio7",
        "gpio8", "gpio9", "gpio10", "gpio11", "gpio12", "gpio13", "gpio14",
        "gpio15", "gpio16", "gpio17", "gpio18", "gpio19", "gpio20", "gpio21",
        "gpio22", "gpio23", "gpio24", "gpio25", "gpio26", "gpio27", "gpio28",
        "gpio29", "gpio30", "gpio31", "gpio32", "gpio33", "gpio34", "gpio35",
        "gpio36", "gpio37", "gpio38", "gpio39", "gpio40", "gpio41", "gpio42",
        "gpio43", "gpio44", "gpio45", "gpio46", "gpio47", "gpio48", "gpio49",
};

static const char * const sdc_data_groups[] = {
	"gpio0", "gpio1", "gpio2", "gpio3",
};

static const char * const qspi_data_groups[] = {
	"gpio0", "gpio1", "gpio2", "gpio3",
};

static const char * const pwm2_groups[] = {
	"gpio0", "gpio1", "gpio2", "gpio3",
};

static const char * const wci0_groups[] = {
	"gpio0", "gpio0",
};

static const char * const wci1_groups[] = {
	"gpio1", "gpio1",
};

static const char * const sdc_cmd_groups[] = {
	"gpio4",
};

static const char * const qspi_cs_groups[] = {
	"gpio4",
};

static const char * const qdss_cti_trig_out_a1_groups[] = {
	"gpio4",
};

static const char * const sdc_clk_groups[] = {
	"gpio5",
};

static const char * const qspi_clk_groups[] = {
	"gpio5",
};

static const char * const spi0_clk_groups[] = {
	"gpio6",
};

static const char * const pwm1_groups[] = {
	"gpio6", "gpio7", "gpio8", "gpio9",
};

static const char * const cri_trng0_groups[] = {
	"gpio6",
};

static const char * const qdss_tracedata_a_groups[] = {
	"gpio6", "gpio7", "gpio8", "gpio9", "gpio10", "gpio11", "gpio12",
	"gpio13", "gpio14", "gpio15", "gpio20", "gpio21", "gpio36", "gpio37",
	"gpio38", "gpio39",
};

static const char * const spi0_cs_groups[] = {
	"gpio7",
};

static const char * const cri_trng1_groups[] = {
	"gpio7",
};

static const char * const spi0_miso_groups[] = {
	"gpio8",
};

static const char * const wci2_groups[] = {
	"gpio8", "gpio8",
};

static const char * const cri_trng2_groups[] = {
	"gpio8",
};

static const char * const spi0_mosi_groups[] = {
	"gpio9",
};

static const char * const cri_trng3_groups[] = {
	"gpio9",
};

static const char * const uart0_rfr_groups[] = {
	"gpio10",
};

static const char * const pwm0_groups[] = {
	"gpio10", "gpio11", "gpio12", "gpio13",
};

static const char * const wci3_groups[] = {
	"gpio10", "gpio10",
};

static const char * const uart0_cts_groups[] = {
	"gpio11",
};

static const char * const wci4_groups[] = {
	"gpio11", "gpio11",
};

static const char * const uart0_rx_groups[] = {
	"gpio12",
};

static const char * const prng_rosc0_groups[] = {
	"gpio12",
};

static const char * const uart0_tx_groups[] = {
	"gpio13",
};

static const char * const prng_rosc1_groups[] = {
	"gpio13",
};

static const char * const i2c0_scl_groups[] = {
	"gpio14",
};

static const char * const tsens_max_groups[] = {
	"gpio14",
};

static const char * const prng_rosc2_groups[] = {
	"gpio14",
};

static const char * const i2c0_sda_groups[] = {
	"gpio15",
};

static const char * const prng_rosc3_groups[] = {
	"gpio15",
};

static const char * const core_voltage_groups[] = {
	"gpio16", "gpio17",
};

static const char * const i2c1_scl_groups[] = {
	"gpio16",
};

static const char * const i2c1_sda_groups[] = {
	"gpio17",
};

static const char * const mdc_slv_groups[] = {
	"gpio20",
};

static const char * const atest_char0_groups[] = {
	"gpio20",
};

static const char * const mdio_slv_groups[] = {
	"gpio21",
};

static const char * const atest_char1_groups[] = {
	"gpio21",
};

static const char * const mdc_mst_groups[] = {
	"gpio22",
};

static const char * const atest_char2_groups[] = {
	"gpio22",
};

static const char * const mdio_mst_groups[] = {
	"gpio23",
};

static const char * const atest_char3_groups[] = {
	"gpio23",
};

static const char * const pcie0_clk_groups[] = {
	"gpio24",
};

static const char * const PTA10_groups[] = {
	"gpio24", "gpio25", "gpio27", "gpio30",
};

static const char * const mac0_groups[] = {
	"gpio24", "gpio25",
};

static const char * const atest_char_groups[] = {
	"gpio24", "gpio43",
};

static const char * const pcie0_wake_groups[] = {
	"gpio26",
};

static const char * const pcie1_clk_groups[] = {
	"gpio27",
};

static const char * const i2c11_groups[] = {
	"gpio27", "gpio29",
};

static const char * const pcie1_wake_groups[] = {
	"gpio29",
};

static const char * const pcie2_clk_groups[] = {
	"gpio30",
};

static const char * const mac1_groups[] = {
	"gpio30", "gpio32",
};

static const char * const pcie2_wake_groups[] = {
	"gpio32",
};

static const char * const PTA11_groups[] = {
	"gpio32", "gpio33",
};

static const char * const audio_pri0_groups[] = {
	"gpio32", "gpio32",
};

static const char * const pcie3_clk_groups[] = {
	"gpio33",
};

static const char * const audio_pri1_groups[] = {
	"gpio33", "gpio33",
};

static const char * const pcie3_wake_groups[] = {
	"gpio35",
};

static const char * const audio_sec1_groups[] = {
	"gpio35", "gpio35",
};

static const char * const audio_pri_groups[] = {
	"gpio36", "gpio37", "gpio38", "gpio39",
};

static const char * const spi1_cs1_groups[] = {
	"gpio36",
};

static const char * const audio_sec0_groups[] = {
	"gpio36", "gpio36",
};

static const char * const spi1_cs2_groups[] = {
	"gpio37",
};

static const char * const rx1_groups[] = {
	"gpio37", "gpio38", "gpio39",
};

static const char * const spi1_cs3_groups[] = {
	"gpio38",
};

static const char * const pll_test_groups[] = {
	"gpio38",
};

static const char * const dbg_out_groups[] = {
	"gpio38",
};

static const char * const PTA_0_groups[] = {
	"gpio40",
};

static const char * const wci5_groups[] = {
	"gpio40", "gpio40",
};

static const char * const atest_tic_groups[] = {
	"gpio40",
};

static const char * const PTA_1_groups[] = {
	"gpio41",
};

static const char * const wci6_groups[] = {
	"gpio41", "gpio41",
};

static const char * const cxc_data_groups[] = {
	"gpio41",
};

static const char * const PTA_2_groups[] = {
	"gpio42",
};

static const char * const cxc_clk_groups[] = {
	"gpio42",
};

static const char * const uart1_rx_groups[] = {
	"gpio43",
};

static const char * const audio_sec_groups[] = {
	"gpio43", "gpio44", "gpio45", "gpio46",
};

static const char * const gcc_plltest_groups[] = {
	"gpio43", "gpio45",
};

static const char * const uart1_tx_groups[] = {
	"gpio44",
};

static const char * const gcc_tlmm_groups[] = {
	"gpio44",
};

static const char * const qdss_cti_trig_out_b1_groups[] = {
	"gpio44",
};

static const char * const spi1_clk_groups[] = {
	"gpio45",
};

static const char * const rx0_groups[] = {
	"gpio45", "gpio46", "gpio47",
};

static const char * const qdss_traceclk_a_groups[] = {
	"gpio45",
};

static const char * const burn_in_groups[] = {
	"gpio45",
};

static const char * const spi1_cs_groups[] = {
	"gpio46",
};

static const char * const qdss_tracectl_a_groups[] = {
	"gpio46",
};

static const char * const burn_in0_groups[] = {
	"gpio46",
};

static const char * const spi1_miso_groups[] = {
	"gpio47",
};

static const char * const qdss_cti_trig_out_a0_groups[] = {
	"gpio47",
};

static const char * const burn_in1_groups[] = {
	"gpio47",
};

static const char * const spi1_mosi_groups[] = {
	"gpio48",
};

static const char * const qdss_cti_trig_in_a0_groups[] = {
	"gpio48",
};

static const char * const resout_groups[] = {
	"gpio49",
};

static const struct msm_function devsoc_functions[] = {
	FUNCTION(atest_char),
	FUNCTION(atest_char0),
	FUNCTION(atest_char1),
	FUNCTION(atest_char2),
	FUNCTION(atest_char3),
	FUNCTION(atest_tic),
	FUNCTION(audio_pri),
	FUNCTION(audio_pri0),
	FUNCTION(audio_pri1),
	FUNCTION(audio_sec),
	FUNCTION(audio_sec0),
	FUNCTION(audio_sec1),
	FUNCTION(burn_in),
	FUNCTION(burn_in0),
	FUNCTION(burn_in1),
	FUNCTION(core_voltage),
	FUNCTION(cri_trng0),
	FUNCTION(cri_trng1),
	FUNCTION(cri_trng2),
	FUNCTION(cri_trng3),
	FUNCTION(cxc_clk),
	FUNCTION(cxc_data),
	FUNCTION(dbg_out),
	FUNCTION(gcc_plltest),
	FUNCTION(gcc_tlmm),
	FUNCTION(gpio),
	FUNCTION(i2c0_scl),
	FUNCTION(i2c0_sda),
	FUNCTION(i2c1_scl),
	FUNCTION(i2c1_sda),
	FUNCTION(i2c11),
	FUNCTION(mac0),
	FUNCTION(mac1),
	FUNCTION(mdc_mst),
	FUNCTION(mdc_slv),
	FUNCTION(mdio_mst),
	FUNCTION(mdio_slv),
	FUNCTION(pcie0_clk),
	FUNCTION(pcie0_wake),
	FUNCTION(pcie1_clk),
	FUNCTION(pcie1_wake),
	FUNCTION(pcie2_clk),
	FUNCTION(pcie2_wake),
	FUNCTION(pcie3_clk),
	FUNCTION(pcie3_wake),
	FUNCTION(pll_test),
	FUNCTION(prng_rosc0),
	FUNCTION(prng_rosc1),
	FUNCTION(prng_rosc2),
	FUNCTION(prng_rosc3),
	FUNCTION(PTA_0),
	FUNCTION(PTA_1),
	FUNCTION(PTA_2),
	FUNCTION(PTA10),
	FUNCTION(PTA11),
	FUNCTION(pwm0),
	FUNCTION(pwm1),
	FUNCTION(pwm2),
	FUNCTION(qdss_cti_trig_in_a0),
	FUNCTION(qdss_cti_trig_out_a0),
	FUNCTION(qdss_cti_trig_out_a1),
	FUNCTION(qdss_cti_trig_out_b1),
	FUNCTION(qdss_traceclk_a),
	FUNCTION(qdss_tracectl_a),
	FUNCTION(qdss_tracedata_a),
	FUNCTION(qspi_clk),
	FUNCTION(qspi_cs),
	FUNCTION(qspi_data),
	FUNCTION(resout),
	FUNCTION(rx0),
	FUNCTION(rx1),
	FUNCTION(sdc_clk),
	FUNCTION(sdc_cmd),
	FUNCTION(sdc_data),
	FUNCTION(spi0_clk),
	FUNCTION(spi0_cs),
	FUNCTION(spi0_miso),
	FUNCTION(spi0_mosi),
	FUNCTION(spi1_clk),
	FUNCTION(spi1_cs),
	FUNCTION(spi1_cs1),
	FUNCTION(spi1_cs2),
	FUNCTION(spi1_cs3),
	FUNCTION(spi1_miso),
	FUNCTION(spi1_mosi),
	FUNCTION(tsens_max),
	FUNCTION(uart0_cts),
	FUNCTION(uart0_rfr),
	FUNCTION(uart0_rx),
	FUNCTION(uart0_tx),
	FUNCTION(uart1_rx),
	FUNCTION(uart1_tx),
	FUNCTION(wci0),
	FUNCTION(wci1),
	FUNCTION(wci2),
	FUNCTION(wci3),
	FUNCTION(wci4),
	FUNCTION(wci5),
	FUNCTION(wci6),
};

static const struct msm_pingroup devsoc_groups[] = {
	PINGROUP(0, sdc_data, qspi_data, pwm2, wci0, wci0, _, _, _, _),
	PINGROUP(1, sdc_data, qspi_data, pwm2, wci1, wci1, _, _, _, _),
	PINGROUP(2, sdc_data, qspi_data, pwm2, _, _, _, _, _, _),
	PINGROUP(3, sdc_data, qspi_data, pwm2, _, _, _, _, _, _),
	PINGROUP(4, sdc_cmd, qspi_cs, _, qdss_cti_trig_out_a1, _, _, _, _,
		 _),
	PINGROUP(5, sdc_clk, qspi_clk, _, _, _, _, _, _, _),
	PINGROUP(6, spi0_clk, pwm1, _, cri_trng0, qdss_tracedata_a, _, _,
		 _, _),
	PINGROUP(7, spi0_cs, pwm1, _, cri_trng1, qdss_tracedata_a, _, _, _,
		 _),
	PINGROUP(8, spi0_miso, pwm1, wci2, wci2, _, cri_trng2,
		 qdss_tracedata_a, _, _),
	PINGROUP(9, spi0_mosi, pwm1, _, cri_trng3, qdss_tracedata_a, _, _,
		 _, _),
	PINGROUP(10, uart0_rfr, pwm0, _, wci3, wci3, _, qdss_tracedata_a, _,
		 _),
	PINGROUP(11, uart0_cts, pwm0, _, wci4, wci4, _, qdss_tracedata_a, _,
		 _),
	PINGROUP(12, uart0_rx, pwm0, _, prng_rosc0, qdss_tracedata_a, _, _,
		 _, _),
	PINGROUP(13, uart0_tx, pwm0, _, prng_rosc1, qdss_tracedata_a, _, _,
		 _, _),
	PINGROUP(14, i2c0_scl, tsens_max, _, prng_rosc2, qdss_tracedata_a, _,
		 _, _, _),
	PINGROUP(15, i2c0_sda, _, prng_rosc3, qdss_tracedata_a, _, _, _,
		 _, _),
	PINGROUP(16, core_voltage, i2c1_scl, _, _, _, _, _, _, _),
	PINGROUP(17, core_voltage, i2c1_sda, _, _, _, _, _, _, _),
	PINGROUP(18, _, _, _, _, _, _, _, _, _),
	PINGROUP(19, _, _, _, _, _, _, _, _, _),
	PINGROUP(20, mdc_slv, atest_char0, _, qdss_tracedata_a, _, _, _,
		 _, _),
	PINGROUP(21, mdio_slv, atest_char1, _, qdss_tracedata_a, _, _, _,
		 _, _),
	PINGROUP(22, mdc_mst, atest_char2, _, _, _, _, _, _, _),
	PINGROUP(23, mdio_mst, atest_char3, _, _, _, _, _, _, _),
	PINGROUP(24, pcie0_clk, PTA10, mac0, _, _, atest_char, _, _, _),
	PINGROUP(25, _, PTA10, mac0, _, _, _, _, _, _),
	PINGROUP(26, pcie0_wake, _, _, _, _, _, _, _, _),
	PINGROUP(27, pcie1_clk, i2c11, PTA10, _, _, _, _, _, _),
	PINGROUP(28, _, _, _, _, _, _, _, _, _),
	PINGROUP(29, pcie1_wake, i2c11, _, _, _, _, _, _, _),
	PINGROUP(30, pcie2_clk, PTA10, mac1, _, _, _, _, _, _),
	PINGROUP(31, _, _, _, _, _, _, _, _, _),
	PINGROUP(32, pcie2_wake, PTA11, mac1, audio_pri0, audio_pri0, _, _,
		 _, _),
	PINGROUP(33, pcie3_clk, PTA11, audio_pri1, audio_pri1, _, _, _, _,
		 _),
	PINGROUP(34, _, _, _, _, _, _, _, _, _),
	PINGROUP(35, pcie3_wake, audio_sec1, audio_sec1, _, _, _, _, _,
		 _),
	PINGROUP(36, audio_pri, spi1_cs1, audio_sec0, audio_sec0,
		 qdss_tracedata_a, _, _, _, _),
	PINGROUP(37, audio_pri, spi1_cs2, rx1, qdss_tracedata_a, _, _, _,
		 _, _),
	PINGROUP(38, audio_pri, spi1_cs3, pll_test, rx1, qdss_tracedata_a,
		 dbg_out, _, _, _),
	PINGROUP(39, audio_pri, rx1, _, qdss_tracedata_a, _, _, _, _, _),
	PINGROUP(40, PTA_0, wci5, wci5, _, atest_tic, _, _, _, _),
	PINGROUP(41, PTA_1, wci6, wci6, cxc_data, _, _, _, _, _),
	PINGROUP(42, PTA_2, cxc_clk, _, _, _, _, _, _, _),
	PINGROUP(43, uart1_rx, audio_sec, gcc_plltest, _, atest_char, _, _,
		 _, _),
	PINGROUP(44, uart1_tx, audio_sec, gcc_tlmm, _, qdss_cti_trig_out_b1,
		 _, _, _, _),
	PINGROUP(45, spi1_clk, rx0, audio_sec, gcc_plltest, _,
		 qdss_traceclk_a, burn_in, _, _),
	PINGROUP(46, spi1_cs, rx0, audio_sec, _, qdss_tracectl_a, burn_in0,
		 _, _, _),
	PINGROUP(47, spi1_miso, rx0, qdss_cti_trig_out_a0, burn_in1, _, _,
		 _, _, _),
	PINGROUP(48, spi1_mosi, qdss_cti_trig_in_a0, _, _, _, _, _, _,
		 _),
	PINGROUP(49, resout, _, _, _, _, _, _, _, _),
};

static const struct msm_pinctrl_soc_data devsoc_pinctrl = {
	.pins = devsoc_pins,
	.npins = ARRAY_SIZE(devsoc_pins),
	.functions = devsoc_functions,
	.nfunctions = ARRAY_SIZE(devsoc_functions),
	.groups = devsoc_groups,
	.ngroups = ARRAY_SIZE(devsoc_groups),
	.ngpios = 50,
};

static int devsoc_pinctrl_probe(struct platform_device *pdev)
{
	return msm_pinctrl_probe(pdev, &devsoc_pinctrl);
}

static const struct of_device_id devsoc_pinctrl_of_match[] = {
	{ .compatible = "qcom,devsoc-tlmm", },
	{ },
};
MODULE_DEVICE_TABLE(of, devsoc_pinctrl_of_match);

static struct platform_driver devsoc_pinctrl_driver = {
	.driver = {
		.name = "devsoc-tlmm",
		.owner = THIS_MODULE,
		.of_match_table = devsoc_pinctrl_of_match,
	},
	.probe = devsoc_pinctrl_probe,
	.remove = msm_pinctrl_remove,
};

static int __init devsoc_pinctrl_init(void)
{
	return platform_driver_register(&devsoc_pinctrl_driver);
}
arch_initcall(devsoc_pinctrl_init);

static void __exit devsoc_pinctrl_exit(void)
{
	platform_driver_unregister(&devsoc_pinctrl_driver);
}
module_exit(devsoc_pinctrl_exit);

MODULE_DESCRIPTION("QTI DEVSOC TLMM driver");
MODULE_LICENSE("GPL");
