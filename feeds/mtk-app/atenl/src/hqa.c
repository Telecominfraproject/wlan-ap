/* Copyright (C) 2021-2022 Mediatek Inc. */
#include "atenl.h"

#define CHAN(_ch, _freq, _ch_80, _ch_160, ...) {	\
	.ch = _ch,				\
	.freq = _freq,				\
	.ch_80 = _ch_80,			\
	.ch_160 = _ch_160,			\
	__VA_ARGS__				\
}

struct atenl_channel {
	/* ctrl ch */
	u16 ch;
	u16 freq;
	/* center ch */
	u16 ch_80;
	u16 ch_160;
	u16 ch_320_1;
	u16 ch_320_2;
	/* only use for channels that don't have 80 but has 40 */
	u16 ch_40;
};

static const struct atenl_channel atenl_channels_5ghz[] = {
	CHAN(8, 5040, 0, 0),
	CHAN(12, 5060, 0, 0),
	CHAN(16, 5080, 0, 0),

	CHAN(36, 5180, 42, 50),
	CHAN(40, 5200, 42, 50),
	CHAN(44, 5220, 42, 50),
	CHAN(48, 5240, 42, 50),

	CHAN(52, 5260, 58, 50),
	CHAN(56, 5280, 58, 50),
	CHAN(60, 5300, 58, 50),
	CHAN(64, 5320, 58, 50),

	CHAN(68, 5340, 0, 0),
	CHAN(80, 5400, 0, 0),
	CHAN(84, 5420, 0, 0),
	CHAN(88, 5440, 0, 0),
	CHAN(92, 5460, 0, 0),
	CHAN(96, 5480, 0, 0),

	CHAN(100, 5500, 106, 114),
	CHAN(104, 5520, 106, 114),
	CHAN(108, 5540, 106, 114),
	CHAN(112, 5560, 106, 114),
	CHAN(116, 5580, 122, 114),
	CHAN(120, 5600, 122, 114),
	CHAN(124, 5620, 122, 114),
	CHAN(128, 5640, 122, 114),

	CHAN(132, 5660, 138, 0),
	CHAN(136, 5680, 138, 0),
	CHAN(140, 5700, 138, 0),
	CHAN(144, 5720, 138, 0),

	CHAN(149, 5745, 155, 0),
	CHAN(153, 5765, 155, 0),
	CHAN(157, 5785, 155, 0),
	CHAN(161, 5805, 155, 0),
	CHAN(165, 5825, 0, 0, .ch_40 = 167),
	CHAN(169, 5845, 0, 0, .ch_40 = 167),
	CHAN(173, 5865, 0, 0),

	CHAN(184, 4920, 0, 0),
	CHAN(188, 4940, 0, 0),
	CHAN(192, 4960, 0, 0),
	CHAN(196, 4980, 0, 0),
};

static const struct atenl_channel atenl_channels_6ghz[] = {
	/* UNII-5 */
	CHAN(1, 5955, 7, 15, .ch_320_1 = 31),
	CHAN(5, 5975, 7, 15, .ch_320_1 = 31),
	CHAN(9, 5995, 7, 15, .ch_320_1 = 31),
	CHAN(13, 6015, 7, 15, .ch_320_1 = 31),
	CHAN(17, 6035, 23, 15, .ch_320_1 = 31),
	CHAN(21, 6055, 23, 15, .ch_320_1 = 31),
	CHAN(25, 6075, 23, 15, .ch_320_1 = 31),
	CHAN(29, 6095, 23, 15, .ch_320_1 = 31),
	CHAN(33, 6115, 39, 47, .ch_320_1 = 31, .ch_320_2 = 63),
	CHAN(37, 6135, 39, 47, .ch_320_1 = 31, .ch_320_2 = 63),
	CHAN(41, 6155, 39, 47, .ch_320_1 = 31, .ch_320_2 = 63),
	CHAN(45, 6175, 39, 47, .ch_320_1 = 31, .ch_320_2 = 63),
	CHAN(49, 6195, 55, 47, .ch_320_1 = 31, .ch_320_2 = 63),
	CHAN(53, 6215, 55, 47, .ch_320_1 = 31, .ch_320_2 = 63),
	CHAN(57, 6235, 55, 47, .ch_320_1 = 31, .ch_320_2 = 63),
	CHAN(61, 6255, 55, 47, .ch_320_1 = 31, .ch_320_2 = 63),
	CHAN(65, 6275, 71, 79, .ch_320_1 = 95, .ch_320_2 = 63),
	CHAN(69, 6295, 71, 79, .ch_320_1 = 95, .ch_320_2 = 63),
	CHAN(73, 6315, 71, 79, .ch_320_1 = 95, .ch_320_2 = 63),
	CHAN(77, 6335, 71, 79, .ch_320_1 = 95, .ch_320_2 = 63),
	CHAN(81, 6355, 87, 79, .ch_320_1 = 95, .ch_320_2 = 63),
	CHAN(85, 6375, 87, 79, .ch_320_1 = 95, .ch_320_2 = 63),
	CHAN(89, 6395, 87, 79, .ch_320_1 = 95, .ch_320_2 = 63),
	CHAN(93, 6415, 87, 79, .ch_320_1 = 95, .ch_320_2 = 63),
	/* UNII-6 */
	CHAN(97, 6435, 103, 111, .ch_320_1 = 95, .ch_320_2 = 127),
	CHAN(101, 6455, 103, 111, .ch_320_1 = 95, .ch_320_2 = 127),
	CHAN(105, 6475, 103, 111, .ch_320_1 = 95, .ch_320_2 = 127),
	CHAN(109, 6495, 103, 111, .ch_320_1 = 95, .ch_320_2 = 127),
	CHAN(113, 6515, 119, 111, .ch_320_1 = 95, .ch_320_2 = 127),
	CHAN(117, 6535, 119, 111, .ch_320_1 = 95, .ch_320_2 = 127),
	/* UNII-7 */
	CHAN(121, 6555, 119, 111, .ch_320_1 = 95, .ch_320_2 = 127),
	CHAN(125, 6575, 119, 111, .ch_320_1 = 95, .ch_320_2 = 127),
	CHAN(129, 6595, 135, 143, .ch_320_1 = 159, .ch_320_2 = 127),
	CHAN(133, 6615, 135, 143, .ch_320_1 = 159, .ch_320_2 = 127),
	CHAN(137, 6635, 135, 143, .ch_320_1 = 159, .ch_320_2 = 127),
	CHAN(141, 6655, 135, 143, .ch_320_1 = 159, .ch_320_2 = 127),
	CHAN(145, 6675, 151, 143, .ch_320_1 = 159, .ch_320_2 = 127),
	CHAN(149, 6695, 151, 143, .ch_320_1 = 159, .ch_320_2 = 127),
	CHAN(153, 6715, 151, 143, .ch_320_1 = 159, .ch_320_2 = 127),
	CHAN(157, 6735, 151, 143, .ch_320_1 = 159, .ch_320_2 = 127),
	CHAN(161, 6755, 167, 175, .ch_320_1 = 159, .ch_320_2 = 191),
	CHAN(165, 6775, 167, 175, .ch_320_1 = 159, .ch_320_2 = 191),
	CHAN(169, 6795, 167, 175, .ch_320_1 = 159, .ch_320_2 = 191),
	CHAN(173, 6815, 167, 175, .ch_320_1 = 159, .ch_320_2 = 191),
	CHAN(177, 6835, 183, 175, .ch_320_1 = 159, .ch_320_2 = 191),
	CHAN(181, 6855, 183, 175, .ch_320_1 = 159, .ch_320_2 = 191),
	CHAN(185, 6875, 183, 175, .ch_320_1 = 159, .ch_320_2 = 191),
	/* UNII-8 */
	CHAN(189, 6895, 183, 175, .ch_320_1 = 159, .ch_320_2 = 191),
	CHAN(193, 6915, 199, 207, .ch_320_2 = 191),
	CHAN(197, 6935, 199, 207, .ch_320_2 = 191),
	CHAN(201, 6955, 199, 207, .ch_320_2 = 191),
	CHAN(205, 6975, 199, 207, .ch_320_2 = 191),
	CHAN(209, 6995, 215, 207, .ch_320_2 = 191),
	CHAN(213, 7015, 215, 207, .ch_320_2 = 191),
	CHAN(217, 7035, 215, 207, .ch_320_2 = 191),
	CHAN(221, 7055, 215, 207, .ch_320_2 = 191),
	CHAN(225, 7075, 0, 0, .ch_40 = 227),
	CHAN(229, 7095, 0, 0, .ch_40 = 227),
	CHAN(233, 7115, 0, 0),
};

static int
atenl_hqa_adapter(struct atenl *an, struct atenl_data *data)
{
	char cmd[64];
	u8 i;

	for (i = 0; i < MAX_BAND_NUM; i++) {
		u8 phy = get_band_val(an, i, phy_idx);

		if (!get_band_val(an, i, valid))
			continue;

		if (data->cmd == HQA_CMD_OPEN_ADAPTER) {
			sprintf(cmd, "iw phy phy%u interface add mon%u type monitor", phy, phy);
			system(cmd);
			sprintf(cmd, "iw dev wlan%u del", phy);
			system(cmd);
			sprintf(cmd, "ifconfig mon%u up", phy);
			system(cmd);
			/* set a special-defined country */
			sprintf(cmd, "iw reg set VV");
			system(cmd);
		} else {
			atenl_nl_set_state(an, i, MT76_TM_STATE_OFF);
			sprintf(cmd, "iw reg set 00");
			system(cmd);
			sprintf(cmd, "iw dev mon%u del", phy);
			system(cmd);
			sprintf(cmd, "iw phy phy%u interface add wlan%u type managed", phy, phy);
			system(cmd);
		}
	}

	for (i = 0; i < MAX_BAND_NUM; i++)
		atenl_nl_set_state(an, i, MT76_TM_STATE_IDLE);

	return 0;
}

static int
atenl_hqa_set_rf_mode(struct atenl *an, struct atenl_data *data)
{
	struct atenl_cmd_hdr *hdr = atenl_hdr(data);

	/* The testmode rf mode change applies to all bands */
	set_band_val(an, 0, rf_mode, ntohl(*(u32 *)hdr->data));
	set_band_val(an, 1, rf_mode, ntohl(*(u32 *)hdr->data));

	return 0;
}

static int
atenl_hqa_get_chip_id(struct atenl *an, struct atenl_data *data)
{
	struct atenl_cmd_hdr *hdr = atenl_hdr(data);

	*(u32 *)(hdr->data + 2) = htonl(an->chip_id);

	return 0;
}

static int
atenl_hqa_get_sub_chip_id(struct atenl *an, struct atenl_data *data)
{
	struct atenl_cmd_hdr *hdr = atenl_hdr(data);

	*(u32 *)(hdr->data + 2) = htonl(an->sub_chip_id);

	return 0;
}

static int
atenl_hqa_get_rf_cap(struct atenl *an, struct atenl_data *data)
{
	struct atenl_cmd_hdr *hdr = atenl_hdr(data);
	u32 band = ntohl(*(u32 *)hdr->data);
	struct atenl_band *anb;

	if (band >= MAX_BAND_NUM)
		return -EINVAL;

	anb = &an->anb[band];
	/* fill tx and rx ant */
	*(u32 *)(hdr->data + 2) = htonl(__builtin_popcount(anb->chainmask));
	*(u32 *)(hdr->data + 2 + 4) = *(u32 *)(hdr->data + 2);

	return 0;
}

static int
atenl_hqa_reset_counter(struct atenl *an, struct atenl_data *data)
{
	struct atenl_band *anb = &an->anb[an->cur_band];

	anb->reset_tx_cnt = true;
	anb->reset_rx_cnt = true;

	memset(&anb->rx_stat, 0, sizeof(anb->rx_stat));

	return 0;
}

static int
atenl_hqa_mac_bbp_reg(struct atenl *an, struct atenl_data *data)
{
	struct atenl_cmd_hdr *hdr = atenl_hdr(data);
	enum atenl_cmd cmd = data->cmd;
	u32 *v = (u32 *)hdr->data;
	u32 offset = ntohl(v[0]), res;
	int ret;

	if (cmd == HQA_CMD_READ_MAC_BBP_REG) {
		u16 num = ntohs(*(u16 *)(hdr->data + 4));
		u32 *ptr = (u32 *)(hdr->data + 2);
		int i;

		if (num > SHRT_MAX) {
			ret = -EINVAL;
			goto out;
		}

		hdr->len = htons(2 + num * 4);
		for (i = 0; i < num && i < sizeof(hdr->data) / 4; i++) {
			ret = atenl_reg_read(an, offset + i * 4, &res);
			if (ret)
				goto out;

			res = htonl(res);
			memcpy(ptr + i, &res, 4);
		}
	} else if (cmd == HQA_CMD_READ_MAC_BBP_REG_QA) {
		ret = atenl_reg_read(an, offset, &res);
		if (ret)
			goto out;

		res = htonl(res);
		memcpy(hdr->data + 2, &res, 4);
	} else {
		u32 val = ntohl(v[1]);

		ret = atenl_reg_write(an, offset, val);
		if (ret)
			goto out;
	}

	ret = 0;
out:
	memset(hdr->data, 0, 2);

	return ret;
}

static int
atenl_hqa_rf_reg(struct atenl *an, struct atenl_data *data)
{
	struct atenl_cmd_hdr *hdr = atenl_hdr(data);
	enum atenl_cmd cmd = data->cmd;
	u32 *v = (u32 *)hdr->data;
	u32 wf_sel = ntohl(v[0]);
	u32 offset = ntohl(v[1]);
	u32 num = ntohl(v[2]);
	int ret, i;

	if (cmd == HQA_CMD_READ_RF_REG) {
		u32 *ptr = (u32 *)(hdr->data + 2);
		u32 res;

		hdr->len = htons(2 + num * 4);
		for (i = 0; i < num && i < sizeof(hdr->data) / 4; i++) {
			ret = atenl_rf_read(an, wf_sel, offset + i * 4, &res);
			if (ret)
				goto out;

			res = htonl(res);
			memcpy(ptr + i, &res, 4);
		}
	} else {
		u32 *ptr = (u32 *)(hdr->data + 12);

		for (i = 0; i < num && i < sizeof(hdr->data) / 4; i++) {
			u32 val = ntohl(ptr[i]);

			ret = atenl_rf_write(an, wf_sel, offset + i * 4, val);
			if (ret)
				goto out;
		}
	}

	ret = 0;
out:
	memset(hdr->data, 0, 2);

	return ret;
}

static int
atenl_hqa_eeprom_bulk(struct atenl *an, struct atenl_data *data)
{
	struct atenl_cmd_hdr *hdr = atenl_hdr(data);
	enum atenl_cmd cmd = data->cmd;

	if (cmd == HQA_CMD_WRITE_BUFFER_DONE) {
		u32 buf_mode = ntohl(*(u32 *)hdr->data);

		switch (buf_mode) {
		case E2P_EFUSE_MODE:
			atenl_nl_write_efuse_all(an);
			break;
		default:
			break;
		}
	} else {
		u16 *v = (u16 *)hdr->data;
		u16 offset = ntohs(v[0]), len = ntohs(v[1]);
		u16 val;
		size_t i;

		if (offset >= an->eeprom_size || (len > sizeof(hdr->data) - 2))
			return -EINVAL;

		if (cmd == HQA_CMD_READ_EEPROM_BULK) {
			hdr->len = htons(len + 2);
			for (i = 0; i < len; i += 2) {
				if (offset + i >= an->eeprom_size)
					val = 0;
				else
					val = ntohs(*(u16 *)(an->eeprom_data + offset + i));
				*(u16 *)(hdr->data + 2 + i) = val;
			}
		} else { /* write eeprom */
			for (i = 0; i < DIV_ROUND_UP(len, 2); i++) {
				val = ntohs(v[i + 2]);
				memcpy(&an->eeprom_data[offset + i * 2], &val, 2);
			}
		}
	}

	return 0;
}

static int
atenl_hqa_get_efuse_free_block(struct atenl *an, struct atenl_data *data)
{
	struct atenl_cmd_hdr *hdr = atenl_hdr(data);
	u32 free_block = htonl(0x14);

	/* TODO */
	*(u32 *)(hdr->data + 2) = free_block;

	return 0;
}

static int
atenl_hqa_get_band(struct atenl *an, struct atenl_data *data)
{
	struct atenl_cmd_hdr *hdr = atenl_hdr(data);
	u32 band = ntohl(*(u32 *)hdr->data);

	if (band >= MAX_BAND_NUM)
		return -EINVAL;

	*(u32 *)(hdr->data + 2) = htonl(an->anb[band].cap);

	return 0;
}

static int
atenl_hqa_get_tx_power(struct atenl *an, struct atenl_data *data)
{
	struct atenl_cmd_hdr *hdr = atenl_hdr(data);
	u32 tx_power = htonl(28);

	memcpy(hdr->data + 6, &tx_power, 4);

	return 0;
}

static int
atenl_hqa_get_freq_offset(struct atenl *an, struct atenl_data *data)
{
	struct atenl_cmd_hdr *hdr = atenl_hdr(data);
	u32 freq_offset = htonl(10);

	/* TODO */
	memcpy(hdr->data + 2, &freq_offset, 4);

	return 0;
}

static int
atenl_hqa_get_cfg(struct atenl *an, struct atenl_data *data)
{
	struct atenl_cmd_hdr *hdr = atenl_hdr(data);
	u32 val = htonl(1);

	/* TODO */
	memcpy(hdr->data + 2, &val, 4);

	return 0;
}

static int
atenl_hqa_read_temperature(struct atenl *an, struct atenl_data *data)
{
	struct atenl_cmd_hdr *hdr = atenl_hdr(data);
	char buf[64], *str;
	int fd, ret;
	u32 temp;
	u8 phy_idx = get_band_val(an, an->cur_band, phy_idx);

	ret = snprintf(buf, sizeof(buf),
		       "/sys/class/ieee80211/phy%u/hwmon%u/temp1_input",
		       phy_idx, phy_idx);
	if (snprintf_error(sizeof(buf), ret))
		return -1;

	fd = open(buf, O_RDONLY);
	if (fd < 0)
		return fd;

	ret = read(fd, buf, sizeof(buf) - 1);
	if (ret < 0)
		goto out;
	buf[ret] = 0;

	str = strchr(buf, ':');
	str += 2;
	temp = strtol(str, NULL, 10);
	/* unit conversion */
	*(u32 *)(hdr->data + 2) = htonl(temp / 1000);

	ret = 0;
out:
	close(fd);

	return ret;
}

static int
atenl_hqa_check_efuse_mode(struct atenl *an, struct atenl_data *data)
{
	struct atenl_cmd_hdr *hdr = atenl_hdr(data);
	bool flash_mode = an->flash_part != NULL;
	enum atenl_cmd cmd = data->cmd;
	u32 mode;

	switch (cmd) {
	case HQA_CMD_CHECK_EFUSE_MODE:
		mode = flash_mode ? 0 : 1;
		break;
	case HQA_CMD_CHECK_EFUSE_MODE_TYPE:
		mode = flash_mode ? E2P_FLASH_MODE : E2P_BIN_MODE;
		break;
	case HQA_CMD_CHECK_EFUSE_MODE_NATIVE:
		mode = flash_mode ? E2P_FLASH_MODE : E2P_EFUSE_MODE;
		break;
	default:
		mode = E2P_BIN_MODE;
		break;
	}

	*(u32 *)(hdr->data + 2) = htonl(mode);

	return 0;
}

static inline u16
atenl_get_freq_by_channel(u8 ch_band, u16 ch)
{
	u16 base;

	if (ch_band == CH_BAND_6GHZ) {
		base = 5950;
	} else if (ch_band == CH_BAND_5GHZ) {
		if (ch >= 184)
			return 4920 + (ch - 184) * 5;

		base = 5000;
	} else {
		base = 2407;
	}

	return base + ch * 5;
}

u16 atenl_get_center_channel(u8 bw, u8 ch_band, u16 ctrl_ch)
{
	const struct atenl_channel *chan = NULL;
	const struct atenl_channel *ch_list;
	u16 center_ch;
	u8 ch_num;
	int i;

	if (ch_band == CH_BAND_2GHZ || bw <= TEST_CBW_40MHZ)
		return 0;

	if (ch_band == CH_BAND_6GHZ) {
		ch_list = atenl_channels_6ghz;
		ch_num = ARRAY_SIZE(atenl_channels_6ghz);
	} else {
		ch_list = atenl_channels_5ghz;
		ch_num = ARRAY_SIZE(atenl_channels_5ghz);
	}

	for (i = 0; i < ch_num; i++) {
		if (ctrl_ch == ch_list[i].ch) {
			chan = &ch_list[i];
			break;
		}
	}

	if (!chan)
		return 0;

	switch (bw) {
	case TEST_CBW_160MHZ:
		center_ch = chan->ch_160;
		break;
	case TEST_CBW_80MHZ:
		center_ch = chan->ch_80;
		break;
	default:
		center_ch = 0;
		break;
	}

	return center_ch;
}

static void atenl_get_bw_string(u8 bw, char *buf)
{
	switch (bw) {
	case TEST_CBW_320MHZ:
		sprintf(buf, "320");
		break;
	case TEST_CBW_160MHZ:
		sprintf(buf, "160");
		break;
	case TEST_CBW_80MHZ:
		sprintf(buf, "80");
		break;
	case TEST_CBW_40MHZ:
		sprintf(buf, "40");
		break;
	default:
		sprintf(buf, "20");
		break;
	}
}

void atenl_set_channel(struct atenl *an, u8 bw, u8 ch_band,
		       u16 ch, u16 center_ch1, u16 center_ch2)
{
	char bw_str[8] = {};
	char cmd[128];
	u16 freq = atenl_get_freq_by_channel(ch_band, ch);
	u16 freq_center1 = atenl_get_freq_by_channel(ch_band, center_ch1);
	int ret;

	if (bw > TEST_CBW_MAX)
		return;

	atenl_get_bw_string(bw, bw_str);

	if (bw == TEST_CBW_20MHZ)
		ret = snprintf(cmd, sizeof(cmd), "iw dev mon%d set freq %u %s",
						 get_band_val(an, an->cur_band, phy_idx),
						 freq, bw_str);
	else
		ret = snprintf(cmd, sizeof(cmd), "iw dev mon%d set freq %u %s %u",
						 get_band_val(an, an->cur_band, phy_idx),
						 freq, bw_str, freq_center1);
	if (snprintf_error(sizeof(cmd), ret))
		return;

	atenl_dbg("%s: cmd: %s\n", __func__, cmd);

	system(cmd);
}

static int
atenl_hqa_set_channel(struct atenl *an, struct atenl_data *data)
{
	struct atenl_cmd_hdr *hdr = atenl_hdr(data);
	u32 *v = (u32 *)hdr->data;
	u8 band = ntohl(v[2]);
	u16 ch1 = ntohl(v[3]);	/* center */
	u16 ch2 = ntohl(v[4]);
	u8 bw = ntohl(v[5]);
	u8 pri_sel = ntohl(v[7]);
	u8 ch_band = ntohl(v[9]);
	u16 ctrl_ch = 0;

	if (band >= MAX_BAND_NUM)
		return -EINVAL;

	if ((bw == TEST_CBW_320MHZ && pri_sel > 15) ||
	    (bw == TEST_CBW_160MHZ && pri_sel > 7) ||
	    (bw == TEST_CBW_80MHZ && pri_sel > 3) ||
	    (bw == TEST_CBW_40MHZ && pri_sel > 1)) {
		atenl_err("%s: ctrl channel select error\n", __func__);
		return -EINVAL;
	}

	an->cur_band = band;

	if (ch_band == CH_BAND_2GHZ) {
		ctrl_ch = ch1;
		switch (bw) {
		case TEST_CBW_40MHZ:
			if (pri_sel == 1)
				ctrl_ch += 2;
			else
				ctrl_ch -= 2;
			break;
		default:
			break;
		}

		atenl_set_channel(an, bw, CH_BAND_2GHZ, ctrl_ch, ch1, 0);
	} else {
		const struct atenl_channel *chan = NULL;
		const struct atenl_channel *ch_list;
		u8 ch_num;
		int i;

		if (ch_band == CH_BAND_6GHZ) {
			ch_list = atenl_channels_6ghz;
			ch_num = ARRAY_SIZE(atenl_channels_6ghz);
		} else {
			ch_list = atenl_channels_5ghz;
			ch_num = ARRAY_SIZE(atenl_channels_5ghz);
		}

		if (bw == TEST_CBW_320MHZ) {
			for (i = 0; i < ch_num; i++) {
				if (ch1 == ch_list[i].ch_320_1) {
					chan = &ch_list[i];
					break;
				} else if (ch1 == ch_list[i].ch_320_2) {
					chan = &ch_list[i];
					break;
				}
			}
		} else if (bw == TEST_CBW_160MHZ) {
			for (i = 0; i < ch_num; i++) {
				if (ch1 == ch_list[i].ch_160) {
					chan = &ch_list[i];
					break;
				} else if (ch1 < ch_list[i].ch_160) {
					chan = &ch_list[i - 1];
					break;
				}
			}
		} else if (bw == TEST_CBW_80MHZ) {
			for (i = 0; i < ch_num; i++) {
				if (ch1 == ch_list[i].ch_80) {
					chan = &ch_list[i];
					break;
				} else if (ch1 < ch_list[i].ch_80) {
					chan = &ch_list[i - 1];
					break;
				}
			}
		} else {
			for (i = 0; i < ch_num; i++) {
				if (ch1 <= ch_list[i].ch) {
					if (ch1 == ch_list[i].ch)
						chan = &ch_list[i];
					else
						chan = &ch_list[i - 1];
					break;
				}
			}
		}

		if (!chan)
			return -EINVAL;

		if (bw != TEST_CBW_20MHZ) {
			chan += pri_sel;
			if (chan > &ch_list[ch_num - 1])
				return -EINVAL;
		}
		ctrl_ch = chan->ch;

		atenl_set_channel(an, bw, ch_band, ctrl_ch, ch1, ch2);
	}

	*(u32 *)(hdr->data + 2) = data->ext_id;

	atenl_nl_set_aid(an, band, 0);

	return 0;
}

static int
atenl_hqa_tx_time_option(struct atenl *an, struct atenl_data *data)
{
	struct atenl_cmd_hdr *hdr = atenl_hdr(data);
	u32 *v = (u32 *)hdr->data;
	u8 band = ntohl(v[1]);
	u32 option = ntohl(v[2]);

	if (band >= MAX_BAND_NUM)
		return -EINVAL;

	set_band_val(an, band, use_tx_time, !!option);
	*(u32 *)(hdr->data + 2) = data->ext_id;

	return 0;
}

/* should be placed in order for binary search */
static const struct atenl_ops hqa_ops[] = {
	{
		.cmd = HQA_CMD_OPEN_ADAPTER,
		.cmd_id = 0x1000,
		.resp_len = 2,
		.ops = atenl_hqa_adapter,
	},
	{
		.cmd = HQA_CMD_CLOSE_ADAPTER,
		.cmd_id = 0x1001,
		.resp_len = 2,
		.ops = atenl_hqa_adapter,
	},
	{
		.cmd = HQA_CMD_SET_TX_PATH,
		.cmd_id = 0x100b,
		.resp_len = 2,
		.ops = atenl_nl_process,
	},
	{
		.cmd = HQA_CMD_SET_RX_PATH,
		.cmd_id = 0x100c,
		.resp_len = 2,
		.ops = atenl_nl_process,
	},
	{
		.cmd = HQA_CMD_LEGACY,
		.cmd_id = 0x100d,
		.resp_len = 2,
		.flags = ATENL_OPS_FLAG_SKIP,
	},
	{
		.cmd = HQA_CMD_SET_TX_POWER,
		.cmd_id = 0x1011,
		.resp_len = 2,
		.ops = atenl_nl_process,
	},
	{
		.cmd = HQA_CMD_SET_TX_POWER_MANUAL,
		.cmd_id = 0x1018,
		.resp_len = 2,
		.flags = ATENL_OPS_FLAG_SKIP,
	},
	{
		.cmd = HQA_CMD_LEGACY,
		.cmd_id = 0x1101,
		.resp_len = 2,
		.flags = ATENL_OPS_FLAG_SKIP,
	},
	{
		.cmd = HQA_CMD_LEGACY,
		.cmd_id = 0x1102,
		.resp_len = 2,
		.flags = ATENL_OPS_FLAG_SKIP,
	},
	{
		.cmd = HQA_CMD_SET_TX_BW,
		.cmd_id = 0x1104,
		.resp_len = 2,
		.flags = ATENL_OPS_FLAG_SKIP,
	},
	{
		.cmd = HQA_CMD_SET_TX_PKT_BW,
		.cmd_id = 0x1105,
		.resp_len = 2,
		.flags = ATENL_OPS_FLAG_SKIP,
	},
	{
		.cmd = HQA_CMD_SET_TX_PRI_BW,
		.cmd_id = 0x1106,
		.resp_len = 2,
		.flags = ATENL_OPS_FLAG_SKIP,
	},
	{
		.cmd = HQA_CMD_SET_FREQ_OFFSET,
		.cmd_id = 0x1107,
		.resp_len = 2,
		.ops = atenl_nl_process,
	},
	{
		.cmd = HQA_CMD_SET_TSSI,
		.cmd_id = 0x1109,
		.resp_len = 2,
		.ops = atenl_nl_process,
	},
	{
		.cmd = HQA_CMD_SET_EEPROM_TO_FW,
		.cmd_id = 0x110c,
		.resp_len = 2,
		.flags = ATENL_OPS_FLAG_SKIP,
	},
	{
		.cmd = HQA_CMD_ANT_SWAP_CAP,
		.cmd_id = 0x110d,
		.resp_len = 6,
		.flags = ATENL_OPS_FLAG_SKIP,
	},
	{
		.cmd = HQA_CMD_RESET_TX_RX_COUNTER,
		.cmd_id = 0x1200,
		.resp_len = 2,
		.ops = atenl_hqa_reset_counter,
	},
	{
		.cmd = HQA_CMD_READ_MAC_BBP_REG_QA,
		.cmd_id = 0x1300,
		.resp_len = 6,
		.ops = atenl_hqa_mac_bbp_reg,
	},
	{
		.cmd = HQA_CMD_WRITE_MAC_BBP_REG,
		.cmd_id = 0x1301,
		.resp_len = 2,
		.ops = atenl_hqa_mac_bbp_reg,
	},
	{
		.cmd = HQA_CMD_READ_MAC_BBP_REG,
		.cmd_id = 0x1302,
		.ops = atenl_hqa_mac_bbp_reg,
	},
	{
		.cmd = HQA_CMD_READ_RF_REG,
		.cmd_id = 0x1303,
		.ops = atenl_hqa_rf_reg,
	},
	{
		.cmd = HQA_CMD_WRITE_RF_REG,
		.cmd_id = 0x1304,
		.resp_len = 2,
		.ops = atenl_hqa_rf_reg,
	},
	{
		.cmd = HQA_CMD_WRITE_EEPROM_BULK,
		.cmd_id = 0x1306,
		.resp_len = 2,
		.ops = atenl_hqa_eeprom_bulk,
	},
	{
		.cmd = HQA_CMD_READ_EEPROM_BULK,
		.cmd_id = 0x1307,
		.ops = atenl_hqa_eeprom_bulk,
	},
	{
		.cmd = HQA_CMD_WRITE_EEPROM_BULK,
		.cmd_id = 0x1308,
		.resp_len = 2,
		.ops = atenl_hqa_eeprom_bulk,
	},
	{
		.cmd = HQA_CMD_CHECK_EFUSE_MODE,
		.cmd_id = 0x1309,
		.resp_len = 6,
		.ops = atenl_hqa_check_efuse_mode,
	},
	{
		.cmd = HQA_CMD_GET_EFUSE_FREE_BLOCK,
		.cmd_id = 0x130a,
		.resp_len = 6,
		.ops = atenl_hqa_get_efuse_free_block,
	},
	{
		.cmd = HQA_CMD_GET_TX_POWER,
		.cmd_id = 0x130d,
		.resp_len = 10,
		.ops = atenl_hqa_get_tx_power,
	},
	{
		.cmd = HQA_CMD_SET_CFG,
		.cmd_id = 0x130e,
		.resp_len = 2,
		.ops = atenl_nl_process,
	},
	{
		.cmd = HQA_CMD_GET_FREQ_OFFSET,
		.cmd_id = 0x130f,
		.resp_len = 6,
		.ops = atenl_hqa_get_freq_offset,
	},
	{
		.cmd = HQA_CMD_CONTINUOUS_TX,
		.cmd_id = 0x1311,
		.resp_len = 6,
		.ops = atenl_nl_process,
	},
	{
		.cmd = HQA_CMD_SET_RX_PKT_LEN,
		.cmd_id = 0x1312,
		.resp_len = 2,
		.flags = ATENL_OPS_FLAG_SKIP,
	},
	{
		.cmd = HQA_CMD_GET_TX_INFO,
		.cmd_id = 0x1313,
		.resp_len = 10,
		.ops = atenl_nl_process,
	},
	{
		.cmd = HQA_CMD_GET_CFG,
		.cmd_id = 0x1314,
		.resp_len = 6,
		.ops = atenl_hqa_get_cfg,
	},
	{
		.cmd = HQA_CMD_GET_TX_TONE_POWER,
		.cmd_id = 0x131a,
		.resp_len = 6,
		.flags = ATENL_OPS_FLAG_SKIP,
	},
	{
		.cmd = HQA_CMD_UNKNOWN,
		.cmd_id = 0x131f,
		.resp_len = 1024,
		.flags = ATENL_OPS_FLAG_SKIP,
	},
	{
		.cmd = HQA_CMD_READ_TEMPERATURE,
		.cmd_id = 0x1401,
		.resp_len = 6,
		.ops = atenl_hqa_read_temperature,
	},
	{
		.cmd = HQA_CMD_GET_FW_INFO,
		.cmd_id = 0x1500,
		.resp_len = 32,
		.flags = ATENL_OPS_FLAG_SKIP,
	},
	{
		.cmd_id = 0x1502,
		.flags = ATENL_OPS_FLAG_LEGACY,
	},
	{
		.cmd = HQA_CMD_SET_TSSI,
		.cmd_id = 0x1505,
		.resp_len = 2,
		.ops = atenl_nl_process,
	},
	{
		.cmd = HQA_CMD_SET_RF_MODE,
		.cmd_id = 0x1509,
		.resp_len = 2,
		.ops = atenl_hqa_set_rf_mode,
	},
	{
		.cmd_id = 0x150b,
		.flags = ATENL_OPS_FLAG_LEGACY,
	},
	{
		.cmd = HQA_CMD_WRITE_BUFFER_DONE,
		.cmd_id = 0x1511,
		.resp_len = 2,
		.ops = atenl_hqa_eeprom_bulk,
	},
	{
		.cmd = HQA_CMD_GET_CHIP_ID,
		.cmd_id = 0x1514,
		.resp_len = 6,
		.ops = atenl_hqa_get_chip_id,
	},
	{
		.cmd = HQA_CMD_GET_SUB_CHIP_ID,
		.cmd_id = 0x151b,
		.resp_len = 6,
		.ops = atenl_hqa_get_sub_chip_id,
	},
	{
		.cmd = HQA_CMD_GET_RX_INFO,
		.cmd_id = 0x151c,
		.ops = atenl_nl_process,
	},
	{
		.cmd = HQA_CMD_GET_RF_CAP,
		.cmd_id = 0x151e,
		.resp_len = 10,
		.ops = atenl_hqa_get_rf_cap,
	},
	{
		.cmd = HQA_CMD_CHECK_EFUSE_MODE_TYPE,
		.cmd_id = 0x1522,
		.resp_len = 6,
		.ops = atenl_hqa_check_efuse_mode,
	},
	{
		.cmd = HQA_CMD_CHECK_EFUSE_MODE_NATIVE,
		.cmd_id = 0x1523,
		.resp_len = 6,
		.ops = atenl_hqa_check_efuse_mode,
	},
	{
		.cmd = HQA_CMD_GET_BAND,
		.cmd_id = 0x152d,
		.resp_len = 6,
		.ops = atenl_hqa_get_band,
	},
	{
		.cmd = HQA_CMD_SET_RU,
		.cmd_id = 0x1594,
		.resp_len = 2,
		.ops = atenl_nl_process_many,
	},
};

static const struct atenl_ops hqa_ops_ext[] = {
	{
		.cmd = HQA_EXT_CMD_SET_CHANNEL,
		.cmd_id = 0x01,
		.resp_len = 6,
		.ops = atenl_hqa_set_channel,
		.flags = ATENL_OPS_FLAG_EXT_CMD,
	},
	{
		.cmd = HQA_EXT_CMD_SET_TX,
		.cmd_id = 0x02,
		.resp_len = 6,
		.ops = atenl_nl_process,
		.flags = ATENL_OPS_FLAG_EXT_CMD,
	},
	{
		.cmd = HQA_EXT_CMD_START_TX,
		.cmd_id = 0x03,
		.resp_len = 6,
		.ops = atenl_nl_process,
		.flags = ATENL_OPS_FLAG_EXT_CMD,
	},
	{
		.cmd = HQA_EXT_CMD_START_RX,
		.cmd_id = 0x04,
		.resp_len = 6,
		.ops = atenl_nl_process,
		.flags = ATENL_OPS_FLAG_EXT_CMD,
	},
	{
		.cmd = HQA_EXT_CMD_STOP_TX,
		.cmd_id = 0x05,
		.resp_len = 6,
		.ops = atenl_nl_process,
		.flags = ATENL_OPS_FLAG_EXT_CMD,
	},
	{
		.cmd = HQA_EXT_CMD_STOP_RX,
		.cmd_id = 0x06,
		.resp_len = 6,
		.ops = atenl_nl_process,
		.flags = ATENL_OPS_FLAG_EXT_CMD,
	},
	{
		.cmd = HQA_EXT_CMD_IBF_SET_VAL,
		.cmd_id = 0x08,
		.resp_len = 6,
		.ops = atenl_nl_process,
		.flags = ATENL_OPS_FLAG_EXT_CMD,
	},
	{
		.cmd = HQA_EXT_CMD_IBF_GET_STATUS,
		.cmd_id = 0x09,
		.resp_len = 10,
		.ops = atenl_nl_process,
		.flags = ATENL_OPS_FLAG_EXT_CMD,
	},
	{
		.cmd = HQA_EXT_CMD_IBF_PROF_UPDATE_ALL,
		.cmd_id = 0x0c,
		.resp_len = 6,
		.ops = atenl_nl_process,
		.flags = ATENL_OPS_FLAG_EXT_CMD,
	},
	{
		.cmd = HQA_EXT_CMD_SET_TX_TIME_OPT,
		.cmd_id = 0x26,
		.resp_len = 6,
		.ops = atenl_hqa_tx_time_option,
		.flags = ATENL_OPS_FLAG_EXT_CMD,
	},
	{
		.cmd = HQA_EXT_CMD_OFF_CH_SCAN,
		.cmd_id = 0x27,
		.resp_len = 6,
		.ops = atenl_nl_process,
		.flags = ATENL_OPS_FLAG_EXT_CMD,
	},
};

static const struct atenl_ops *
atenl_get_ops(struct atenl_data *data)
{
	const struct atenl_ops *group;
	struct atenl_cmd_hdr *hdr = atenl_hdr(data);
	u16 cmd_id = ntohs(hdr->cmd_id), id = cmd_id;
	int size, low = 0, high;

	switch (cmd_id) {
	case 0x1600:
		group = hqa_ops_ext;
		size = ARRAY_SIZE(hqa_ops_ext);
		break;
	default:
		group = hqa_ops;
		size = ARRAY_SIZE(hqa_ops);
		break;
	}

	if (group[0].flags & ATENL_OPS_FLAG_EXT_CMD)
		id = ntohl(*(u32 *)hdr->data);

	/* binary search */
	high = size - 1;
	while (low <= high) {
		int mid = low + (high - low) / 2;

		if (group[mid].cmd_id == id)
			return &group[mid];
		else if (group[mid].cmd_id > id)
			high = mid - 1;
		else
			low = mid + 1;
	}

	return NULL;
}

static int
atenl_hqa_handler(struct atenl *an, struct atenl_data *data)
{
	struct atenl_cmd_hdr *hdr = atenl_hdr(data);
	const struct atenl_ops *ops = NULL;
	u16 cmd_id = ntohs(hdr->cmd_id);
	u16 status = 0;

	atenl_dbg("handle command: 0x%x\n", cmd_id);

	ops = atenl_get_ops(data);
	if (!ops || (!ops->ops && !ops->flags)) {
		atenl_err("Unknown command id: 0x%x\n", cmd_id);
		goto done;
	}

	data->cmd = ops->cmd;
	data->cmd_id = ops->cmd_id;
	if (ops->flags & ATENL_OPS_FLAG_EXT_CMD) {
		data->ext_cmd = ops->cmd;
		data->ext_id = ops->cmd_id;
	}

	if (ops->flags & ATENL_OPS_FLAG_SKIP)
		goto done;

	atenl_dbg_print_data(data->buf, __func__,
			     ntohs(hdr->len) + ETH_HLEN + RACFG_HLEN);
	if (ops->ops)
		status = htons(ops->ops(an, data));
	if (ops->resp_len)
		hdr->len = htons(ops->resp_len);

done:
	*(u16 *)hdr->data = status;
	data->len = ntohs(hdr->len) + ETH_HLEN + RACFG_HLEN;
	hdr->cmd_type |= ~htons(RACFG_CMD_TYPE_MASK);

	return 0;
}

int atenl_hqa_proc_cmd(struct atenl *an)
{
	struct atenl_data *data;
	struct atenl_cmd_hdr *hdr;
	u16 cmd_type;
	int ret = -EINVAL;

	data = calloc(1, sizeof(struct atenl_data));
	if (!data)
		return -ENOMEM;

	ret = atenl_eth_recv(an, data);
	if (ret)
		goto out;

	hdr = atenl_hdr(data);
	if (ntohl(hdr->magic_no) != RACFG_MAGIC_NO)
		goto out;

	cmd_type = ntohs(hdr->cmd_type);
	if (FIELD_GET(RACFG_CMD_TYPE_MASK, cmd_type) != RACFG_CMD_TYPE_ETHREQ &&
	    FIELD_GET(RACFG_CMD_TYPE_MASK, cmd_type) != RACFG_CMD_TYPE_PLATFORM_MODULE) {
		atenl_err("cmd type error = 0x%x\n", cmd_type);
		goto out;
	}

	ret = atenl_hqa_handler(an, data);
	if (ret)
		goto out;

	ret = atenl_eth_send(an, data);
	if (ret)
		goto out;

	ret = 0;
out:
	free(data);

	return ret;
}
