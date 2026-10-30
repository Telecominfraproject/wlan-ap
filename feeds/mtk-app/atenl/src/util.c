/* Copyright (C) 2021-2022 Mediatek Inc. */

#include "atenl.h"

int atenl_reg_read(struct atenl *an, u32 offset, u32 *res)
{
	char dir[64], buf[16];
	unsigned long val;
	int fd, ret;

	/* write offset into regidx */
	ret = snprintf(dir, sizeof(dir),
		       "/sys/kernel/debug/ieee80211/phy%d/mt76/regidx",
		       an->main_phy_idx);
	if (snprintf_error(sizeof(dir), ret))
		return ret;

	fd = open(dir, O_WRONLY);
	if (fd < 0)
		return fd;

	ret = snprintf(buf, sizeof(buf), "0x%x", offset);
	if (snprintf_error(sizeof(buf), ret))
		goto out;

	lseek(fd, 0, SEEK_SET);
	write(fd, buf, sizeof(buf));
	close(fd);

	/* read value from regval */
	ret = snprintf(dir, sizeof(dir),
		       "/sys/kernel/debug/ieee80211/phy%d/mt76/regval",
		       an->main_phy_idx);
	if (snprintf_error(sizeof(dir), ret))
		return ret;

	fd = open(dir, O_RDONLY);
	if (fd < 0)
		return fd;

	ret = read(fd, buf, sizeof(buf) - 1);
	if (ret < 0)
		goto out;
	buf[ret] = 0;

	val = strtoul(buf, NULL, 16);
	if (val > (u32) -1)
		return -EINVAL;

	*res = val;
	ret = 0;
out:
	close(fd);

	return ret;
}

int atenl_reg_write(struct atenl *an, u32 offset, u32 val)
{
	char dir[64], buf[16];
	int fd, ret;

	/* write offset into regidx */
	ret = snprintf(dir, sizeof(dir),
		       "/sys/kernel/debug/ieee80211/phy%d/mt76/regidx",
		       an->main_phy_idx);
	if (snprintf_error(sizeof(dir), ret))
		return ret;

	fd = open(dir, O_WRONLY);
	if (fd < 0)
		return fd;

	ret = snprintf(buf, sizeof(buf), "0x%x", offset);
	if (snprintf_error(sizeof(buf), ret))
		goto out;

	lseek(fd, 0, SEEK_SET);
	write(fd, buf, sizeof(buf));
	close(fd);

	/* write value into regval */
	ret = snprintf(dir, sizeof(dir),
		       "/sys/kernel/debug/ieee80211/phy%d/mt76/regval",
		       an->main_phy_idx);
	if (snprintf_error(sizeof(dir), ret))
		return ret;

	fd = open(dir, O_WRONLY);
	if (fd < 0)
		return fd;

	ret = snprintf(buf, sizeof(buf), "0x%x", val);
	if (snprintf_error(sizeof(buf), ret))
		goto out;
	buf[ret] = 0;

	lseek(fd, 0, SEEK_SET);
	write(fd, buf, sizeof(buf));
	ret = 0;
out:
	close(fd);

	return ret;
}

int atenl_rf_read(struct atenl *an, u32 wf_sel, u32 offset, u32 *res)
{
	char dir[64], buf[16];
	unsigned long val;
	int fd, ret;
	u32 regidx;

	/* merge wf_sel and offset into regidx */
	regidx = FIELD_PREP(GENMASK(31, 28), wf_sel) |
		 FIELD_PREP(GENMASK(27, 0), offset);

	/* write regidx */
	ret = snprintf(dir, sizeof(dir),
		       "/sys/kernel/debug/ieee80211/phy%d/mt76/regidx",
		       an->main_phy_idx);
	if (snprintf_error(sizeof(dir), ret))
		return ret;

	fd = open(dir, O_WRONLY);
	if (fd < 0)
		return fd;

	ret = snprintf(buf, sizeof(buf), "0x%x", regidx);
	if (snprintf_error(sizeof(buf), ret))
		goto out;

	lseek(fd, 0, SEEK_SET);
	write(fd, buf, sizeof(buf));
	close(fd);

	/* read from rf_regval */
	ret = snprintf(dir, sizeof(dir),
		       "/sys/kernel/debug/ieee80211/phy%d/mt76/rf_regval",
		       an->main_phy_idx);
	if (snprintf_error(sizeof(dir), ret))
		return ret;

	fd = open(dir, O_RDONLY);
	if (fd < 0)
		return fd;

	ret = read(fd, buf, sizeof(buf) - 1);
	if (ret < 0)
		goto out;
	buf[ret] = 0;

	val = strtoul(buf, NULL, 16);
	if (val > (u32) -1)
		return -EINVAL;

	*res = val;
	ret = 0;
out:
	close(fd);

	return ret;
}

int atenl_rf_write(struct atenl *an, u32 wf_sel, u32 offset, u32 val)
{
	char dir[64], buf[16];
	int fd, ret;
	u32 regidx;

	/* merge wf_sel and offset into regidx */
	regidx = FIELD_PREP(GENMASK(31, 28), wf_sel) |
		 FIELD_PREP(GENMASK(27, 0), offset);

	/* write regidx */
	ret = snprintf(dir, sizeof(dir),
		       "/sys/kernel/debug/ieee80211/phy%d/mt76/regidx",
		       an->main_phy_idx);
	if (snprintf_error(sizeof(dir), ret))
		return ret;

	fd = open(dir, O_WRONLY);
	if (fd < 0)
		return fd;

	ret = snprintf(buf, sizeof(buf), "0x%x", regidx);
	if (snprintf_error(sizeof(buf), ret))
		goto out;

	lseek(fd, 0, SEEK_SET);
	write(fd, buf, sizeof(buf));
	close(fd);

	/* write value into rf_val */
	ret = snprintf(dir, sizeof(dir),
		       "/sys/kernel/debug/ieee80211/phy%d/mt76/rf_regval",
		       an->main_phy_idx);
	if (snprintf_error(sizeof(dir), ret))
		return ret;

	fd = open(dir, O_WRONLY);
	if (fd < 0)
		return fd;

	ret = snprintf(buf, sizeof(buf), "0x%x", val);
	if (snprintf_error(sizeof(buf), ret))
		goto out;
	buf[ret] = 0;

	lseek(fd, 0, SEEK_SET);
	write(fd, buf, sizeof(buf));
	ret = 0;
out:
	close(fd);

	return ret;
}
