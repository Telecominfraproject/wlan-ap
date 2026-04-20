========================
Kernel driver for lp5812
========================

* TI/National Semiconductor LP5812 LED Driver
* Datasheet: https://www.ti.com/product/LP5812#tech-docs

Authors: Jared Zhou <jared-zhou@ti.com>

Description
===========

The LP5812 is a 4x3 matrix LED driver with support for both manual and
autonomous animation control. It provides features such as:

- PWM dimming and DC current control
- Slope time configuration
- Autonomous Engine Unit (AEU) for LED animation playback
- Flexible scan and drive mode configuration

This driver provides sysfs interfaces to control and configure the LP5812
device and its LED channels.

Sysfs Interface
===============

LP5812 device exposes a chip-level sysfs group:
  /sys/bus/i2c/devices/<i2c-dev-addr>/lp5812_chip_setup/

The following attributes are available at chip level:
  - dev_config: Configure drive mode and scan order (RW)
  - device_command: Issue device-wide commands (WO)
  - sw_reset: Reset the hardware (WO)
  - fault_clear: Clear any device faults (WO)
  - tsd_config_status: Read thermal shutdown config status (RO)

Each LED channel is exposed as:
  /sys/class/leds/led_<id>/

Each LED exposes the following attributes:
  - activate: Activate or deactivate the LED (WO)
  - mode: manual or autonomous mode (WO)
  - led_current: DC current value (0–255) (WO)
  - max_current: maximum DC current bit setting (RO)
  - pwm_dimming_scale: linear or exponential (WO)
  - pwm_phase_align: PWM alignment mode (WO)
  - auto_time_pause_at_start: config start pause time (WO)
  - auto_time_pause_at_stop: config stop pause time (WO)
  - auto_playback_eau_number: Activate AEU number (WO)
  - auto_playback_time: Animation pattern playback times (WO)
  - aeu_playback_time: playback times for the specific AEU (WO)
  - aeu_pwm_<pwm_id>: PWM duty cycle setting for the specific AEU (WO)
  - aeu_slop_time_<st_id>: slop time setting for the specific AEU (WO)
  - lod_lsd: lod and lsd fault detected status (RO)

Example Usage
=============

To control led_A in manual mode::
    echo 1 1 1 > /sys/class/leds/LED_A/activate
    echo manual manual manual > /sys/class/leds/LED_A/mode
    echo 100 100 100 > /sys/class/leds/LED_A/led_current
    echo 50 50 50 > /sys/class/leds/LED_A/multi-intensity

To control led_A in autonomous mode::
    echo 1 1 1 > /sys/bus/i2c/drivers/lp5812/xxxx/led_A/activate
    echo autonomous autonomous autonomous > /sys/class/leds/LED_A/mode
    echo linear exponential linear > /sys/class/leds/led_<id>/pwm_dimming_scale
    echo forward forward backward > /sys/class/leds/led_<id>/pwm_phase_align
    echo 0 0 0 > /sys/class/leds/led_A/auto_playback_eau_number # only use AEU1
    echo 10 10 10 > /sys/class/leds/led_A/auto_time_pause_at_start
    echo 10 10 10 > /sys/class/leds/led_A/auto_time_pause_at_stop
    echo 15 15 15 > /sys/class/leds/led_A/auto_playback_time
    echo aeu1:100 100 100 > /sys/class/leds/led_A/aeu_pwm1
    echo aeu1:100 100 100 > /sys/class/leds/led_A/aeu_pwm2
    echo aeu1:100 100 100 > /sys/class/leds/led_A/aeu_pwm3
    echo aeu1:100 100 100 > /sys/class/leds/led_A/aeu_pwm4
    echo aeu1:100 100 100 > /sys/class/leds/led_A/aeu_pwm5
    echo aeu1:5 5 5 > /sys/class/leds/led_A/aeu_slop_time_t1
    echo aeu1:5 5 5 > /sys/class/leds/led_A/aeu_slop_time_t2
    echo aeu1:5 5 5 > /sys/class/leds/led_A/aeu_slop_time_t3
    echo aeu1:5 5 5 > /sys/class/leds/led_A/aeu_slop_time_t4
    echo aeu1:1 1 1 > /sys/class/leds/led_A/aeu_playback_time
    echo start > /sys/bus/i2c/drivers/lp5812/xxxx/lp5812_chip_setup/device_command
