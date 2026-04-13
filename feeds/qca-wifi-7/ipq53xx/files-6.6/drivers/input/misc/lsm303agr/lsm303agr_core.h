/*
 * STMicroelectronics lsm303agr driver
 *
 * Copyright 2016 STMicroelectronics Inc.
 *
 * Giuseppe Barba <giuseppe.barba@st.com>
 *
 * Licensed under the GPL-2.
 */

#ifndef	__LSM303AGR_H__
#define	__LSM303AGR_H__

#ifdef __KERNEL__

#define LSM303AGR_MAX_SENSORS_NUM	1
#define LSM303AGR_ACC_SENSOR		0 /* only this sensor */
#define LSM303AGR_MAG_SENSOR		0 /* only this sensor */

struct lsm303agr_common_data;

/* specific bus I/O functions */
struct lsm303agr_transfer_function {
	int (*write) (struct device *dev, u8 reg_addr, int len, u8 *data);
	int (*read) (struct device *dev, u8 reg_addr, int len, u8 *data);
};

#if defined(CONFIG_INPUT_LSM303AGR_SPI) || \
    defined(CONFIG_INPUT_LSM303AGR_SPI_MODULE)
#define LSM303AGR_RX_MAX_LENGTH		500
#define LSM303AGR_TX_MAX_LENGTH		500

struct lsm303agr_transfer_buffer {
	u8 rx_buf[LSM303AGR_RX_MAX_LENGTH];
	u8 tx_buf[LSM303AGR_TX_MAX_LENGTH] ____cacheline_aligned;
};
#endif /* CONFIG_INPUT_LSM303AGR_SPI */

/* Sensor data */
struct lsm303agr_sensor_data {
	struct lsm303agr_common_data *cdata;
	const char* name;
	s64 timestamp;
	u8 enabled;
	u32 c_odr;
	u32 c_gain;
	u8 sindex;
	u8 sample_to_discard;
	u32 poll_interval;
	u32 min_interval;
	u8 fs_range;
	u32 sensitivity;
	u16 shift;
	u16 opmode;
	struct input_dev *input_dev;
	struct delayed_work input_work;
	u32 schedule_num; /* Number of time work_input routine is called */
};

struct lsm303agr_common_data {
	const char *name;
	struct mutex lock;
	struct device *dev;
	int hw_initialized;
	atomic_t enabled;
	int on_before_suspend;
	u8 sensor_num;
	u16 bus_type;
	struct lsm303agr_sensor_data sensors[LSM303AGR_MAX_SENSORS_NUM];
	const struct lsm303agr_transfer_function *tf;
#if defined(CONFIG_INPUT_LSM303AGR_SPI) || \
    defined(CONFIG_INPUT_LSM303AGR_SPI_MODULE)
	struct lsm303agr_transfer_buffer tb;
#endif /* CONFIG_INPUT_LSM303AGR_SPI */
};

/* Input events used by lsm303agr driver */
#define INPUT_EVENT_TYPE		EV_MSC
#define INPUT_EVENT_X			MSC_SERIAL
#define INPUT_EVENT_Y			MSC_PULSELED
#define INPUT_EVENT_Z			MSC_GESTURE
#define INPUT_EVENT_TIME_MSB		MSC_SCAN
#define INPUT_EVENT_TIME_LSB		MSC_MAX

static inline s64 lsm303agr_get_time_ns(void)
{
	return ktime_to_ns(ktime_get_boottime());
}

void lsm303agr_acc_remove(struct lsm303agr_common_data *cdata);
int lsm303agr_acc_probe(struct lsm303agr_common_data *cdata);
int lsm303agr_acc_enable(struct lsm303agr_common_data *cdata);
int lsm303agr_acc_disable(struct lsm303agr_common_data *cdata);

void lsm303agr_mag_remove(struct lsm303agr_common_data *cdata);
int lsm303agr_mag_probe(struct lsm303agr_common_data *cdata);
int lsm303agr_mag_enable(struct lsm303agr_common_data *cdata);
int lsm303agr_mag_disable(struct lsm303agr_common_data *cdata);

#endif /* __KERNEL__ */
#endif	/* __LSM303AGR_H__ */



