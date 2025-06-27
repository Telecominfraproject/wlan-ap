/*
 * User-space daemon formonitoring and managing PoE ports with
 * TI TPS23861 chips. based on the Linux Kernel TPS23861
 * HWMON driver.
*/

#include <stdio.h>		/* Standard input/output definitions */
#include <string.h>		/* String function definitions */
#include <unistd.h>		/* UNIX standard function definitions */
#include <fcntl.h>		/* File control definitions */
#include <errno.h>		/* Error number definitions */
#include <sys/ioctl.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>  /* uapi/linux/i2c-dev.h */

#include <libubox/ulog.h>

#define TPS23861_I2C_ADDR				0x20
#define DETECT_CLASS_RESTART			0x18
#define POWER_ENABLE					0x19
#define POWER_ON_SHIFT					0
#define POWER_OFF_SHIFT					4

typedef unsigned char	u8;

#if defined(PLATFORM_EWW631_B1)
#define TPS23861_NUM_PORTS				1
#endif

#define CONVERT_PORT_NUM(x)				(1 << ((u8)x-1))

unsigned int  PORT_POWER_STATUS[TPS23861_NUM_PORTS];

int i2c_handler = -1;
#define ULOG_DBG(fmt, ...) ulog(LOG_DEBUG, fmt, ## __VA_ARGS__)

int open_device(void)
{
	int fd, fset;

	fd = open("/dev/i2c-0", O_RDWR);
	fset = fcntl(fd, F_SETFL, 0);
	if (fset < 0)
		printf("fcntl failed!\n");

	//if (isatty(STDIN_FILENO) == 0)
	//	printf("standard input is not a terminal device\n");

	return fd;
}

int access_salve(int fd)
{
	int ret;

	if((ret = ioctl(fd, I2C_SLAVE, TPS23861_I2C_ADDR)) < 0)
	{
		printf("%s: Failed to access slave bus[%s]\n",__func__, strerror(errno));
		return -1;
	}
	return(ret);
}

// Write to an I2C slave device's register:
int i2c_write(u8 slave_addr, u8 reg, u8 data)
{
	u8 outbuf[2];

	struct i2c_msg msgs[1];
	struct i2c_rdwr_ioctl_data msgset[1];

	outbuf[0] = reg;
	outbuf[1] = data;

	msgs[0].addr = slave_addr;
	msgs[0].flags = 0;
	msgs[0].len = 2;
	msgs[0].buf = outbuf;

	msgset[0].msgs = msgs;
	msgset[0].nmsgs = 1;

	if (ioctl(i2c_handler, I2C_RDWR, &msgset) < 0) {
		perror("ioctl(I2C_RDWR) in i2c_write");
		return -1;
	}

	return 0;
}

void poe_set_PowerOnOff(u8 port, u8 on_off) {
	u8 value;
	u8 portBit;
	portBit = CONVERT_PORT_NUM(port+1);

	if(on_off == 0) {
		value = (portBit << POWER_OFF_SHIFT);
		PORT_POWER_STATUS[port] = 0;
	} else {
		value = (portBit << POWER_ON_SHIFT);
		PORT_POWER_STATUS[port] = 1;
	}

	ULOG_DBG("set Port%d Power Status [%d] portBit 0x[%x] value 0x[%x]\n", port+1, PORT_POWER_STATUS[port], portBit, value);

	if(i2c_write(TPS23861_I2C_ADDR, POWER_ENABLE, value) < 0)
	{
		ULOG_ERR("Set port%d power on-off error (0x19)\n", port);
	}
}

void RestartPortDetectClass(u8 port)
{
	u8 value;

	value = (1 << port) | (1 << (port + 4));
	ULOG_DBG("RestartPortDetectClass value 0x%x\n", value);
	if(i2c_write(TPS23861_I2C_ADDR, DETECT_CLASS_RESTART, value) < 0) {
			ULOG_ERR("Set port%d detection and class on error\n",port);
	}
}

int usage(const char *progname)
{
	fprintf(stderr, "Usage: %s -p <1-3> -P <on|off> [options]\n"
			"Required options:\n"
			"  -p <1-3>:           Select port number (Only port 1 is supported)\n"
			"  -P <on|off>:        Set PSE function state <on|off>\n"
			"Optional options:\n"
			"  -d                  Enable debug mode\n"
			"\n", progname);
	return 1;
}

static int setPSE(int port ,char *optarg)
{
	int ret = 0;
	i2c_handler = open_device();
	if (i2c_handler < 0) {
		ULOG_ERR("open i2c-0 device error!\n");
		goto EXIT;
	}

	ret = access_salve(i2c_handler);
	if (ret < 0)
	{
		ULOG_ERR("The i2c-0 access error\n");
		goto EXIT;
	}

	if(!strncmp("on", optarg, 2)) {
		printf("Enable port%d PSE function\n", port);
		RestartPortDetectClass(port-1);
	}
	else if (!strncmp("off", optarg, 3)) {
		printf("Disable port%d PSE function\n", port);
		poe_set_PowerOnOff(port-1, 0);
	}
	else {
		ULOG_ERR("[Set] Do not accept this optarg!!!\n");
		ret = 1;
	}
EXIT:
	close(i2c_handler);
	return ret;
}

int main(int argc, char *argv[])
{
	int ch, ret = 0, port = 0;
	char *PSE = NULL;
	if (argc == 1) {
		return usage(argv[0]);
	}


	ulog_open(ULOG_STDIO | ULOG_SYSLOG, LOG_DAEMON, "tps23861");
	ulog_threshold(LOG_INFO);

	while ((ch = getopt(argc, argv, "dp:P:")) != -1) {
		switch (ch) {
		case 'd':
			printf("tps23861-i2c-control ulog_threshold set to debug level\n");
			ulog_threshold(LOG_DEBUG);
			break;

		case 'p':
			port = atoi(optarg);
			break;

		case 'P':
			PSE = optarg;
			break;

		default:
			ret = usage(argv[0]);
			break;
		}
	}

	if (port < 1 || port > 3) {
		ret = usage(argv[0]);
	}
	else {
		if (PSE) {
			setPSE(port, PSE);
		}
		else {
			ret = usage(argv[0]);
		}
	}

	return ret;

}
