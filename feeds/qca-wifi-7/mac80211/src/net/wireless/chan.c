// SPDX-License-Identifier: GPL-2.0
/*
 * This file contains helper code to handle channel
 * settings and keeping track of what is possible at
 * any point in time.
 *
 * Copyright 2009	Johannes Berg <johannes@sipsolutions.net>
 * Copyright 2013-2014  Intel Mobile Communications GmbH
 * Copyright 2018-2025	Intel Corporation
 */

#include <linux/export.h>
#include <linux/bitfield.h>
#include <net/cfg80211.h>
#include "core.h"
#include "rdev-ops.h"

/* 5GHz 320MHz support */
#define FIXED_PUNCTURE_PATTERN 0xF000
#define CENTER_FREQ_5G_240MHZ  5650
#define DISABLED_SUB_CHAN(freq, start_freq, punctured) \
 ((1 << (freq - start_freq)/MHZ_TO_KHZ(20)) & punctured)

static bool enable_dfs_test_mode;
module_param(enable_dfs_test_mode, bool, 0644);
MODULE_PARM_DESC(enable_dfs_test_mode, "Enable DFS Test mode");

static bool cfg80211_valid_60g_freq(u32 freq)
{
	return freq >= 58320 && freq <= 70200;
}

void cfg80211_chandef_create(struct cfg80211_chan_def *chandef,
			     struct ieee80211_channel *chan,
			     enum nl80211_channel_type chan_type)
{
	if (WARN_ON(!chan))
		return;

	*chandef = (struct cfg80211_chan_def) {
		.chan = chan,
		.freq1_offset = chan->freq_offset,
		.width_device = NL80211_CHAN_WIDTH_20_NOHT,
		.center_freq_device = 0,
	};

	switch (chan_type) {
	case NL80211_CHAN_NO_HT:
		chandef->width = NL80211_CHAN_WIDTH_20_NOHT;
		chandef->center_freq1 = chan->center_freq;
		break;
	case NL80211_CHAN_HT20:
		chandef->width = NL80211_CHAN_WIDTH_20;
		chandef->center_freq1 = chan->center_freq;
		break;
	case NL80211_CHAN_HT40PLUS:
		chandef->width = NL80211_CHAN_WIDTH_40;
		chandef->center_freq1 = chan->center_freq + 10;
		break;
	case NL80211_CHAN_HT40MINUS:
		chandef->width = NL80211_CHAN_WIDTH_40;
		chandef->center_freq1 = chan->center_freq - 10;
		break;
	default:
		WARN_ON(1);
	}
}
EXPORT_SYMBOL(cfg80211_chandef_create);

static int cfg80211_chandef_get_width(const struct cfg80211_chan_def *c)
{
	return nl80211_chan_width_to_mhz(c->width);
}

u32 cfg80211_get_start_freq(const struct cfg80211_chan_def *chandef, u32 cf)
{
	u32 start_freq, center_freq, bandwidth;

	center_freq = MHZ_TO_KHZ((cf == 1) ?
			chandef->center_freq1 : chandef->center_freq2);
	bandwidth = MHZ_TO_KHZ(cfg80211_chandef_get_width(chandef));

	if (bandwidth <= MHZ_TO_KHZ(20))
		start_freq = center_freq;
	else
		start_freq = center_freq - bandwidth / 2 + MHZ_TO_KHZ(10);

	return start_freq;
}
EXPORT_SYMBOL(cfg80211_get_start_freq);

static u32 cfg80211_get_end_freq(const struct cfg80211_chan_def *chandef,
				 u32 cf)
{
	u32 end_freq, center_freq, bandwidth;

	center_freq = MHZ_TO_KHZ((cf == 1) ?
			chandef->center_freq1 : chandef->center_freq2);
	bandwidth = MHZ_TO_KHZ(cfg80211_chandef_get_width(chandef));

	if (bandwidth <= MHZ_TO_KHZ(20))
		end_freq = center_freq;
	else
		end_freq = center_freq + bandwidth / 2 - MHZ_TO_KHZ(10);

	return end_freq;
}

#define for_each_subchan(chandef, freq, cf)				\
	for (u32 punctured = chandef->punctured,			\
	     cf = 1, freq = cfg80211_get_start_freq(chandef, cf);	\
	     freq <= cfg80211_get_end_freq(chandef, cf);		\
	     freq += MHZ_TO_KHZ(20),					\
	     ((cf == 1 && chandef->center_freq2 != 0 &&			\
	       freq > cfg80211_get_end_freq(chandef, cf)) ?		\
	      (cf++, freq = cfg80211_get_start_freq(chandef, cf),	\
	       punctured = 0) : (punctured >>= 1)))			\
		if (!(punctured & 1))

struct cfg80211_per_bw_puncturing_values {
	u8 len;
	const u16 *valid_values;
};

static const u16 puncturing_values_80mhz[] = {
	0x8, 0x4, 0x2, 0x1
};

static const u16 puncturing_values_160mhz[] = {
	 0x80, 0x40, 0x20, 0x10, 0x8, 0x4, 0x2, 0x1, 0xc0, 0x30, 0xc, 0x3
};

static const u16 puncturing_values_320mhz[] = {
	0xc000, 0x3000, 0xc00, 0x300, 0xc0, 0x30, 0xc, 0x3, 0xf000, 0xf00,
	0xf0, 0xf, 0xfc00, 0xf300, 0xf0c0, 0xf030, 0xf00c, 0xf003, 0xc00f,
	0x300f, 0xc0f, 0x30f, 0xcf, 0x3f
};

#define CFG80211_PER_BW_VALID_PUNCTURING_VALUES(_bw) \
	{ \
		.len = ARRAY_SIZE(puncturing_values_ ## _bw ## mhz), \
		.valid_values = puncturing_values_ ## _bw ## mhz \
	}

static const struct cfg80211_per_bw_puncturing_values per_bw_puncturing[] = {
	CFG80211_PER_BW_VALID_PUNCTURING_VALUES(80),
	CFG80211_PER_BW_VALID_PUNCTURING_VALUES(160),
	CFG80211_PER_BW_VALID_PUNCTURING_VALUES(320)
};

bool valid_puncturing_bitmap(const struct cfg80211_chan_def *chandef)
{
	u32 idx, i, start_freq, primary_center = chandef->chan->center_freq;

	switch (chandef->width) {
	case NL80211_CHAN_WIDTH_80:
		idx = 0;
		start_freq = chandef->center_freq1 - 40;
		break;
	case NL80211_CHAN_WIDTH_160:
		idx = 1;
		start_freq = chandef->center_freq1 - 80;
		break;
	case NL80211_CHAN_WIDTH_320:
		idx = 2;
		start_freq = chandef->center_freq1 - 160;
		break;
	default:
		return chandef->punctured == 0;
	}

	if (!chandef->punctured)
		return true;

	/* check if primary channel is punctured */
	if (chandef->punctured & (u16)BIT((primary_center - start_freq) / 20))
		return false;

	for (i = 0; i < per_bw_puncturing[idx].len; i++) {
		if (per_bw_puncturing[idx].valid_values[i] == chandef->punctured)
			return true;
	}

	return false;
}
EXPORT_SYMBOL(valid_puncturing_bitmap);

static bool cfg80211_edmg_chandef_valid(const struct cfg80211_chan_def *chandef)
{
	int max_contiguous = 0;
	int num_of_enabled = 0;
	int contiguous = 0;
	int i;

	if (!chandef->edmg.channels || !chandef->edmg.bw_config)
		return false;

	if (!cfg80211_valid_60g_freq(chandef->chan->center_freq))
		return false;

	for (i = 0; i < 6; i++) {
		if (chandef->edmg.channels & BIT(i)) {
			contiguous++;
			num_of_enabled++;
		} else {
			contiguous = 0;
		}

		max_contiguous = max(contiguous, max_contiguous);
	}
	/* basic verification of edmg configuration according to
	 * IEEE P802.11ay/D4.0 section 9.4.2.251
	 */
	/* check bw_config against contiguous edmg channels */
	switch (chandef->edmg.bw_config) {
	case IEEE80211_EDMG_BW_CONFIG_4:
	case IEEE80211_EDMG_BW_CONFIG_8:
	case IEEE80211_EDMG_BW_CONFIG_12:
		if (max_contiguous < 1)
			return false;
		break;
	case IEEE80211_EDMG_BW_CONFIG_5:
	case IEEE80211_EDMG_BW_CONFIG_9:
	case IEEE80211_EDMG_BW_CONFIG_13:
		if (max_contiguous < 2)
			return false;
		break;
	case IEEE80211_EDMG_BW_CONFIG_6:
	case IEEE80211_EDMG_BW_CONFIG_10:
	case IEEE80211_EDMG_BW_CONFIG_14:
		if (max_contiguous < 3)
			return false;
		break;
	case IEEE80211_EDMG_BW_CONFIG_7:
	case IEEE80211_EDMG_BW_CONFIG_11:
	case IEEE80211_EDMG_BW_CONFIG_15:
		if (max_contiguous < 4)
			return false;
		break;

	default:
		return false;
	}

	/* check bw_config against aggregated (non contiguous) edmg channels */
	switch (chandef->edmg.bw_config) {
	case IEEE80211_EDMG_BW_CONFIG_4:
	case IEEE80211_EDMG_BW_CONFIG_5:
	case IEEE80211_EDMG_BW_CONFIG_6:
	case IEEE80211_EDMG_BW_CONFIG_7:
		break;
	case IEEE80211_EDMG_BW_CONFIG_8:
	case IEEE80211_EDMG_BW_CONFIG_9:
	case IEEE80211_EDMG_BW_CONFIG_10:
	case IEEE80211_EDMG_BW_CONFIG_11:
		if (num_of_enabled < 2)
			return false;
		break;
	case IEEE80211_EDMG_BW_CONFIG_12:
	case IEEE80211_EDMG_BW_CONFIG_13:
	case IEEE80211_EDMG_BW_CONFIG_14:
	case IEEE80211_EDMG_BW_CONFIG_15:
		if (num_of_enabled < 4 || max_contiguous < 2)
			return false;
		break;
	default:
		return false;
	}

	return true;
}

int nl80211_chan_width_to_mhz(enum nl80211_chan_width chan_width)
{
	int mhz;

	switch (chan_width) {
	case NL80211_CHAN_WIDTH_1:
		mhz = 1;
		break;
	case NL80211_CHAN_WIDTH_2:
		mhz = 2;
		break;
	case NL80211_CHAN_WIDTH_4:
		mhz = 4;
		break;
	case NL80211_CHAN_WIDTH_8:
		mhz = 8;
		break;
	case NL80211_CHAN_WIDTH_16:
		mhz = 16;
		break;
	case NL80211_CHAN_WIDTH_5:
		mhz = 5;
		break;
	case NL80211_CHAN_WIDTH_10:
		mhz = 10;
		break;
	case NL80211_CHAN_WIDTH_20:
	case NL80211_CHAN_WIDTH_20_NOHT:
		mhz = 20;
		break;
	case NL80211_CHAN_WIDTH_40:
		mhz = 40;
		break;
	case NL80211_CHAN_WIDTH_80P80:
	case NL80211_CHAN_WIDTH_80:
		mhz = 80;
		break;
	case NL80211_CHAN_WIDTH_160:
		mhz = 160;
		break;
	case NL80211_CHAN_WIDTH_320:
		mhz = 320;
		break;
	default:
		WARN_ON_ONCE(1);
		return -1;
	}
	return mhz;
}
EXPORT_SYMBOL(nl80211_chan_width_to_mhz);

static bool cfg80211_valid_center_freq(u32 center,
				       enum nl80211_chan_width width)
{
	int bw;
	int step;

	/* We only do strict verification on 6 GHz */
	if (center < 5955 || center > 7115)
		return true;

	bw = nl80211_chan_width_to_mhz(width);
	if (bw < 0)
		return false;

	/* Validate that the channels bw is entirely within the 6 GHz band */
	if (center - bw / 2 < 5945 || center + bw / 2 > 7125)
		return false;

	/* With 320 MHz the permitted channels overlap */
	if (bw == 320)
		step = 160;
	else
		step = bw;

	/*
	 * Valid channels are packed from lowest frequency towards higher ones.
	 * So test that the lower frequency aligns with one of these steps.
	 */
	return (center - bw / 2 - 5945) % step == 0;
}

bool cfg80211_chandef_valid(const struct cfg80211_chan_def *chandef)
{
	u32 control_freq, oper_freq;
	int oper_width, control_width;

	if (!chandef)
		return false;

	if (!chandef->chan)
		return false;

	if (chandef->freq1_offset >= 1000)
		return false;

	control_freq = chandef->chan->center_freq;

	switch (chandef->width) {
	case NL80211_CHAN_WIDTH_5:
	case NL80211_CHAN_WIDTH_10:
	case NL80211_CHAN_WIDTH_20:
	case NL80211_CHAN_WIDTH_20_NOHT:
		if (ieee80211_chandef_to_khz(chandef) !=
		    ieee80211_channel_to_khz(chandef->chan))
			return false;
		if (chandef->center_freq2)
			return false;
		break;
	case NL80211_CHAN_WIDTH_1:
	case NL80211_CHAN_WIDTH_2:
	case NL80211_CHAN_WIDTH_4:
	case NL80211_CHAN_WIDTH_8:
	case NL80211_CHAN_WIDTH_16:
		if (chandef->chan->band != NL80211_BAND_S1GHZ)
			return false;

		control_freq = ieee80211_channel_to_khz(chandef->chan);
		oper_freq = ieee80211_chandef_to_khz(chandef);
		control_width = nl80211_chan_width_to_mhz(
					ieee80211_s1g_channel_width(
								chandef->chan));
		oper_width = cfg80211_chandef_get_width(chandef);

		if (oper_width < 0 || control_width < 0)
			return false;
		if (chandef->center_freq2)
			return false;

		if (control_freq + MHZ_TO_KHZ(control_width) / 2 >
		    oper_freq + MHZ_TO_KHZ(oper_width) / 2)
			return false;

		if (control_freq - MHZ_TO_KHZ(control_width) / 2 <
		    oper_freq - MHZ_TO_KHZ(oper_width) / 2)
			return false;
		break;
	case NL80211_CHAN_WIDTH_80P80:
		if (!chandef->center_freq2)
			return false;
		/* adjacent is not allowed -- that's a 160 MHz channel */
		if (chandef->center_freq1 - chandef->center_freq2 == 80 ||
		    chandef->center_freq2 - chandef->center_freq1 == 80)
			return false;
		break;
	default:
		if (chandef->center_freq2)
			return false;
		break;
	}

	switch (chandef->width) {
	case NL80211_CHAN_WIDTH_5:
	case NL80211_CHAN_WIDTH_10:
	case NL80211_CHAN_WIDTH_20:
	case NL80211_CHAN_WIDTH_20_NOHT:
	case NL80211_CHAN_WIDTH_1:
	case NL80211_CHAN_WIDTH_2:
	case NL80211_CHAN_WIDTH_4:
	case NL80211_CHAN_WIDTH_8:
	case NL80211_CHAN_WIDTH_16:
		/* all checked above */
		break;
	case NL80211_CHAN_WIDTH_320:
		if (chandef->center_freq1 == control_freq + 150 ||
		    chandef->center_freq1 == control_freq + 130 ||
		    chandef->center_freq1 == control_freq + 110 ||
		    chandef->center_freq1 == control_freq + 90 ||
		    chandef->center_freq1 == control_freq - 90 ||
		    chandef->center_freq1 == control_freq - 110 ||
		    chandef->center_freq1 == control_freq - 130 ||
		    chandef->center_freq1 == control_freq - 150)
			break;
		fallthrough;
	case NL80211_CHAN_WIDTH_160:
		if (chandef->center_freq1 == control_freq + 70 ||
		    chandef->center_freq1 == control_freq + 50 ||
		    chandef->center_freq1 == control_freq - 50 ||
		    chandef->center_freq1 == control_freq - 70)
			break;
		fallthrough;
	case NL80211_CHAN_WIDTH_80P80:
	case NL80211_CHAN_WIDTH_80:
		if (chandef->center_freq1 == control_freq + 30 ||
		    chandef->center_freq1 == control_freq - 30)
			break;
		fallthrough;
	case NL80211_CHAN_WIDTH_40:
		if (chandef->center_freq1 == control_freq + 10 ||
		    chandef->center_freq1 == control_freq - 10)
			break;
		fallthrough;
	default:
		return false;
	}

	if (!cfg80211_valid_center_freq(chandef->center_freq1, chandef->width))
		return false;

	if (chandef->width == NL80211_CHAN_WIDTH_80P80 &&
	    !cfg80211_valid_center_freq(chandef->center_freq2, chandef->width))
		return false;

	/* channel 14 is only for IEEE 802.11b */
	if (chandef->center_freq1 == 2484 &&
	    chandef->width != NL80211_CHAN_WIDTH_20_NOHT)
		return false;

	if (cfg80211_chandef_is_edmg(chandef) &&
	    !cfg80211_edmg_chandef_valid(chandef))
		return false;

	return valid_puncturing_bitmap(chandef);
}
EXPORT_SYMBOL(cfg80211_chandef_valid);

void cfg80211_chandef_primary_freqs(const struct cfg80211_chan_def *c,
				    u32 *pri40, u32 *pri80, u32 *pri160)
{
	int tmp;

	switch (c->width) {
	case NL80211_CHAN_WIDTH_40:
		*pri40 = c->center_freq1;
		*pri80 = 0;
		*pri160 = 0;
		break;
	case NL80211_CHAN_WIDTH_80:
	case NL80211_CHAN_WIDTH_80P80:
		*pri160 = 0;
		*pri80 = c->center_freq1;
		/* n_P20 */
		tmp = (30 + c->chan->center_freq - c->center_freq1)/20;
		/* n_P40 */
		tmp /= 2;
		/* freq_P40 */
		*pri40 = c->center_freq1 - 20 + 40 * tmp;
		break;
	case NL80211_CHAN_WIDTH_160:
		*pri160 = c->center_freq1;
		/* n_P20 */
		tmp = (70 + c->chan->center_freq - c->center_freq1)/20;
		/* n_P40 */
		tmp /= 2;
		/* freq_P40 */
		*pri40 = c->center_freq1 - 60 + 40 * tmp;
		/* n_P80 */
		tmp /= 2;
		*pri80 = c->center_freq1 - 40 + 80 * tmp;
		break;
	case NL80211_CHAN_WIDTH_320:
		/* n_P20 */
		tmp = (150 + c->chan->center_freq - c->center_freq1) / 20;
		/* n_P40 */
		tmp /= 2;
		/* freq_P40 */
		*pri40 = c->center_freq1 - 140 + 40 * tmp;
		/* n_P80 */
		tmp /= 2;
		*pri80 = c->center_freq1 - 120 + 80 * tmp;
		/* n_P160 */
		tmp /= 2;
		*pri160 = c->center_freq1 - 80 + 160 * tmp;
		break;
	default:
		WARN_ON_ONCE(1);
	}
}
EXPORT_SYMBOL(cfg80211_chandef_primary_freqs);

int cfg80211_chandef_primary(const struct cfg80211_chan_def *c,
			     enum nl80211_chan_width primary_chan_width,
			     u16 *punctured)
{
	int pri_width = nl80211_chan_width_to_mhz(primary_chan_width);
	int width = cfg80211_chandef_get_width(c);
	u32 control = c->chan->center_freq;
	u32 center = c->center_freq1;
	u16 _punct = 0;

	if (WARN_ON_ONCE(pri_width < 0 || width < 0))
		return -1;

	/* not intended to be called this way, can't determine */
	if (WARN_ON_ONCE(pri_width > width))
		return -1;

	if (!punctured)
		punctured = &_punct;

	*punctured = c->punctured;

	while (width > pri_width) {
		unsigned int bits_to_drop = width / 20 / 2;

		if (control > center) {
			center += width / 4;
			*punctured >>= bits_to_drop;
		} else {
			center -= width / 4;
			*punctured &= (1 << bits_to_drop) - 1;
		}
		width /= 2;
	}

	return center;
}
EXPORT_SYMBOL(cfg80211_chandef_primary);

static const struct cfg80211_chan_def *
check_chandef_primary_compat(const struct cfg80211_chan_def *c1,
			     const struct cfg80211_chan_def *c2,
			     enum nl80211_chan_width primary_chan_width)
{
	u16 punct_c1 = 0, punct_c2 = 0;

	/* check primary is compatible -> error if not */
	if (cfg80211_chandef_primary(c1, primary_chan_width, &punct_c1) !=
	    cfg80211_chandef_primary(c2, primary_chan_width, &punct_c2))
		return ERR_PTR(-EINVAL);

	if (punct_c1 != punct_c2)
		return ERR_PTR(-EINVAL);

	/* assumes c1 is smaller width, if that was just checked -> done */
	if (c1->width == primary_chan_width)
		return c2;

	/* otherwise continue checking the next width */
	return NULL;
}

static const struct cfg80211_chan_def *
_cfg80211_chandef_compatible(const struct cfg80211_chan_def *c1,
			     const struct cfg80211_chan_def *c2)
{
	const struct cfg80211_chan_def *ret;

	/* If they are identical, return */
	if (cfg80211_chandef_identical(c1, c2))
		return c2;

	/* otherwise, must have same control channel */
	if (c1->chan != c2->chan)
		return NULL;

	/*
	 * If they have the same width, but aren't identical,
	 * then they can't be compatible.
	 */
	if (c1->width == c2->width)
		return NULL;

	/*
	 * can't be compatible if one of them is 5/10 MHz or S1G
	 * but they don't have the same width.
	 */
#define NARROW_OR_S1G(width)	((width) == NL80211_CHAN_WIDTH_5 || \
				 (width) == NL80211_CHAN_WIDTH_10 || \
				 (width) == NL80211_CHAN_WIDTH_1 || \
				 (width) == NL80211_CHAN_WIDTH_2 || \
				 (width) == NL80211_CHAN_WIDTH_4 || \
				 (width) == NL80211_CHAN_WIDTH_8 || \
				 (width) == NL80211_CHAN_WIDTH_16)

	if (NARROW_OR_S1G(c1->width) || NARROW_OR_S1G(c2->width))
		return NULL;

	/*
	 * Make sure that c1 is always the narrower one, so that later
	 * we either return NULL or c2 and don't have to check both
	 * directions.
	 */
	if (c1->width > c2->width)
		swap(c1, c2);

	/*
	 * No further checks needed if the "narrower" one is only 20 MHz.
	 * Here "narrower" includes being a 20 MHz non-HT channel vs. a
	 * 20 MHz HT (or later) one.
	 */
	if (c1->width <= NL80211_CHAN_WIDTH_20)
		return c2;

	ret = check_chandef_primary_compat(c1, c2, NL80211_CHAN_WIDTH_40);
	if (ret)
		return ret;

	ret = check_chandef_primary_compat(c1, c2, NL80211_CHAN_WIDTH_80);
	if (ret)
		return ret;

	/*
	 * If c1 is 80+80, then c2 is 160 or higher, but that cannot
	 * match. If c2 was also 80+80 it was already either accepted
	 * or rejected above (identical or not, respectively.)
	 */
	if (c1->width == NL80211_CHAN_WIDTH_80P80)
		return NULL;

	ret = check_chandef_primary_compat(c1, c2, NL80211_CHAN_WIDTH_160);
	if (ret)
		return ret;

	/*
	 * Getting here would mean they're both wider than 160, have the
	 * same primary 160, but are not identical - this cannot happen
	 * since they must be 320 (no wider chandefs exist, at least yet.)
	 */
	WARN_ON_ONCE(1);

	return NULL;
}

const struct cfg80211_chan_def *
cfg80211_chandef_compatible(const struct cfg80211_chan_def *c1,
			    const struct cfg80211_chan_def *c2)
{
	const struct cfg80211_chan_def *ret;

	ret = _cfg80211_chandef_compatible(c1, c2);
	if (IS_ERR(ret))
		return NULL;
	return ret;
}
EXPORT_SYMBOL(cfg80211_chandef_compatible);

static void cfg80211_set_chans_dfs_state(struct wiphy *wiphy, u32 center_freq,
					 u32 bandwidth,
					 enum nl80211_dfs_state dfs_state,
					 u16 radar_bitmap)
{
	struct ieee80211_channel *c;
	u32 freq;
	int i;

	for (i = 0, freq = center_freq - bandwidth / 2 + 10;
		freq <= center_freq + bandwidth / 2 - 10;
		freq += 20, i++) {
		c = ieee80211_get_channel(wiphy, freq);
		if (!c || !(c->flags & IEEE80211_CHAN_RADAR) )
			continue;

		if (radar_bitmap && dfs_state == NL80211_DFS_UNAVAILABLE) {
			if (radar_bitmap & 1 << i) {
				c->dfs_state = dfs_state;
				c->dfs_state_entered = jiffies;
			}
		}
		else {
			c->dfs_state = dfs_state;
			c->dfs_state_entered = jiffies;
			c->dfs_state_last_available = jiffies;
		}
	}
}

void cfg80211_set_dfs_state(struct wiphy *wiphy,
			    const struct cfg80211_chan_def *chandef,
			    enum nl80211_dfs_state dfs_state)
{
	struct ieee80211_channel *c;
	int width;

	if (WARN_ON(!cfg80211_chandef_valid(chandef)))
		return;

	if (cfg80211_chandef_device_present(chandef)) {
		width = nl80211_chan_width_to_mhz(chandef->width_device);
		if (width < 0)
			return;

		cfg80211_set_chans_dfs_state(wiphy, chandef->center_freq_device,
					     width, dfs_state,
					     chandef->radar_bitmap);
		return;
	}

	width = cfg80211_chandef_get_width(chandef);
	if (width < 0)
		return;

	cfg80211_set_chans_dfs_state(wiphy, chandef->center_freq1,
				     width, dfs_state,
				     chandef->radar_bitmap);

	/* WAR: To avoid setting all sub channels as NOL */
	if (chandef->radar_bitmap)
		return;

	for_each_subchan(chandef, freq, cf) {
		c = ieee80211_get_channel_khz(wiphy, freq);
		if (!c || !(c->flags & IEEE80211_CHAN_RADAR))
			continue;

		c->dfs_state = dfs_state;
		c->dfs_state_entered = jiffies;
	}
}

static bool
cfg80211_dfs_permissive_check_wdev(struct cfg80211_registered_device *rdev,
				   enum nl80211_iftype iftype,
				   struct wireless_dev *wdev,
				   struct ieee80211_channel *chan)
{
	unsigned int link_id;

	for_each_valid_link(wdev, link_id) {
		struct ieee80211_channel *other_chan = NULL;
		struct cfg80211_chan_def chandef = {};
		int ret;

		/* In order to avoid daisy chaining only allow BSS STA */
		if (wdev->iftype != NL80211_IFTYPE_STATION ||
		    !wdev->links[link_id].client.current_bss)
			continue;

		other_chan =
			wdev->links[link_id].client.current_bss->pub.channel;

		if (!other_chan)
			continue;

		if (chan == other_chan)
			return true;

		/* continue if we can't get the channel */
		ret = rdev_get_channel(rdev, wdev, link_id, &chandef);
		if (ret)
			continue;

		if (cfg80211_is_sub_chan(&chandef, chan, false))
			return true;
	}

	return false;
}

/*
 * Check if P2P GO is allowed to operate on a DFS channel
 */
static bool cfg80211_dfs_permissive_chan(struct wiphy *wiphy,
					 enum nl80211_iftype iftype,
					 struct ieee80211_channel *chan)
{
	struct wireless_dev *wdev;
	struct cfg80211_registered_device *rdev = wiphy_to_rdev(wiphy);

	lockdep_assert_held(&rdev->wiphy.mtx);

	if (!wiphy_ext_feature_isset(&rdev->wiphy,
				     NL80211_EXT_FEATURE_DFS_CONCURRENT) ||
	    !(chan->flags & IEEE80211_CHAN_DFS_CONCURRENT))
		return false;

	/* only valid for P2P GO */
	if (iftype != NL80211_IFTYPE_P2P_GO)
		return false;

	/*
	 * Allow only if there's a concurrent BSS
	 */
	list_for_each_entry(wdev, &rdev->wiphy.wdev_list, list) {
		bool ret = cfg80211_dfs_permissive_check_wdev(rdev, iftype,
							      wdev, chan);
		if (ret)
			return ret;
	}

	return false;
}

u32 cfg80211_get_start_freq_device(const struct cfg80211_chan_def *chandef)
{
	u32 start_freq, center_freq;
	int bandwidth = nl80211_chan_width_to_mhz(chandef->width_device);

	if (bandwidth < 0)
		return 0;

	center_freq = MHZ_TO_KHZ(chandef->center_freq_device);
	bandwidth = MHZ_TO_KHZ(bandwidth);

	if (bandwidth <= MHZ_TO_KHZ(20))
		start_freq = center_freq;
	else
		start_freq = center_freq - bandwidth / 2 + MHZ_TO_KHZ(10);

	return start_freq;
}

u32 cfg80211_get_end_freq_device(const struct cfg80211_chan_def *chandef)
{
	u32 end_freq, center_freq;
	int bandwidth = nl80211_chan_width_to_mhz(chandef->width_device);

	if (bandwidth < 0)
		return 0;

	center_freq = MHZ_TO_KHZ(chandef->center_freq_device);
	bandwidth = MHZ_TO_KHZ(bandwidth);

	if (bandwidth <= MHZ_TO_KHZ(20))
		end_freq = center_freq;
	else
		end_freq = center_freq + bandwidth / 2 - MHZ_TO_KHZ(10);

	return end_freq;
}

bool cfg80211_chandef_device_valid(const struct cfg80211_chan_def *chandef)
{
	int start_freq_device, end_freq_device, start_freq_oper, end_freq_oper;

	if ((chandef->width_device == NL80211_CHAN_WIDTH_20_NOHT &&
	     chandef->center_freq_device == 0) ||
	    (chandef->width_device == chandef->width &&
	     chandef->center_freq_device == chandef->center_freq1))
		return true;

	if (chandef->center_freq_device == 0 ||
	    chandef->width_device == NL80211_CHAN_WIDTH_20_NOHT)
		return false;

	switch (chandef->width_device) {
	case NL80211_CHAN_WIDTH_320:
		if (chandef->width != NL80211_CHAN_WIDTH_160)
			return false;
		break;
	case NL80211_CHAN_WIDTH_160:
		if (chandef->width != NL80211_CHAN_WIDTH_80)
			return false;
		break;
	case NL80211_CHAN_WIDTH_80:
		if (chandef->width != NL80211_CHAN_WIDTH_40)
			return false;
		break;
	case NL80211_CHAN_WIDTH_40:
		if (chandef->width != NL80211_CHAN_WIDTH_20)
			return false;
		break;
	default:
		return false;
	}
	start_freq_device = cfg80211_get_start_freq_device(chandef);
	end_freq_device = cfg80211_get_end_freq_device(chandef);

	start_freq_oper = cfg80211_get_start_freq(chandef,
						  1);
	end_freq_oper = cfg80211_get_end_freq(chandef,
					      1);

	if (start_freq_device <= start_freq_oper && end_freq_oper <= end_freq_device)
		return true;

	return false;
}

bool cfg80211_is_freq_device_non_oper(const struct cfg80211_chan_def *chandef,
				      u32 freq)
{
	int width = nl80211_chan_width_to_mhz(chandef->width);

	if (freq < cfg80211_get_start_freq_device(chandef) ||
	    freq > cfg80211_get_end_freq_device(chandef))
		return false;

	if (width < 0)
		return false;

	if (freq >= cfg80211_get_start_freq(chandef, 1) &&
	    freq <= cfg80211_get_end_freq(chandef, 1))
		return false;

	return true;
}

static bool cfg80211_valid_240mhz_freq_device(const struct cfg80211_chan_def *chandef)
{
	if (chandef->width_device == NL80211_CHAN_WIDTH_320 &&
	    chandef->center_freq_device == CENTER_FREQ_5G_240MHZ) {
		return true;
	}
	return false;
}

static u32 cfg80211_set_punctured_device(struct wiphy *wiphy,
					 const struct cfg80211_chan_def *chandef)
{
	u32 freq, start_freq, end_freq;
	u32 device_punctured = chandef->punctured;
	u32 set_punctured = 0;
	u32 chan_disable_bit = 0, count_non_oper_chan = 0;
	struct ieee80211_channel *c;

	if (chandef->center_freq1 == chandef->center_freq_device)
		return device_punctured;

	/* Note: The condition is applicable when AP is
	 * enabled on 5G band when operating center frequency
	 * is less than device center frequency and bandwidth.
	 * is set to 320 MHz.
	 */
	if (cfg80211_valid_240mhz_freq_device(chandef) &&
	    chandef->center_freq1 < chandef->center_freq_device)
		device_punctured = chandef->punctured | FIXED_PUNCTURE_PATTERN;

	start_freq = cfg80211_get_start_freq_device(chandef);
	end_freq = cfg80211_get_end_freq_device(chandef);

	/* Check disabled channels among the non-operating channels present
	 * in the device bandwidth to set ru puncture bitmap if a channel is
	 * disabled
	 */
	if (chandef->center_freq1 < chandef->center_freq_device)
		start_freq = MHZ_TO_KHZ(chandef->center_freq_device + 10);
	else
		end_freq = MHZ_TO_KHZ(chandef->center_freq_device - 10);

	for (freq = start_freq; freq <= end_freq;
	     freq += MHZ_TO_KHZ(20), count_non_oper_chan++) {
		if (count_non_oper_chan > 3 &&
		    chandef->width_device == NL80211_CHAN_WIDTH_320)
			continue;

		c = ieee80211_get_channel_khz(wiphy, freq);

		if (!c)
			return -EINVAL;

		if (c->flags & IEEE80211_CHAN_DISABLED)
			set_punctured |= BIT(chan_disable_bit);

		chan_disable_bit++;
	}

	if (chandef->width_device == NL80211_CHAN_WIDTH_80) {
		if (chandef->center_freq1 > chandef->center_freq_device)
			device_punctured = device_punctured << 2;
		else
			set_punctured = set_punctured << 2;
		return (device_punctured | set_punctured);
	} else if (chandef->width_device == NL80211_CHAN_WIDTH_160) {
		if (chandef->center_freq1 > chandef->center_freq_device)
			device_punctured = device_punctured << 4;
		else
			set_punctured = set_punctured << 4;
		return (device_punctured | set_punctured);
	} else if (chandef->width_device == NL80211_CHAN_WIDTH_320) {
		if (chandef->center_freq1 > chandef->center_freq_device)
			device_punctured = device_punctured << 8;
		else
			set_punctured = set_punctured << 8;

		return (device_punctured | set_punctured);
	}

	pr_err("Invalid parameters are passed");
	return device_punctured;
}


static int cfg80211_get_chans_dfs_required(struct wiphy *wiphy,
					   const struct cfg80211_chan_def *chandef,
					   enum nl80211_iftype iftype)
{
	struct ieee80211_channel *c;

	for_each_subchan(chandef, freq, cf) {
		c = ieee80211_get_channel_khz(wiphy, freq);
		if (!c)
			return -EINVAL;

		if (c->flags & IEEE80211_CHAN_RADAR &&
		    !cfg80211_dfs_permissive_chan(wiphy, iftype, c))
			return 1;
	}

	return 0;
}

static int cfg80211_get_chans_dfs_required_device(struct wiphy *wiphy,
						  const struct cfg80211_chan_def *chandef)
{
        struct ieee80211_channel *c;
        u32 freq, start_freq, end_freq, bandwidth, punctured;

	bandwidth =  nl80211_chan_width_to_mhz(chandef->width_device);
	if (bandwidth < 0)
		return -EINVAL;

        start_freq = cfg80211_get_start_freq_device(chandef);
        end_freq = cfg80211_get_end_freq_device(chandef);

	punctured = cfg80211_set_punctured_device(wiphy, chandef);
        for (freq = start_freq; freq <= end_freq; freq += MHZ_TO_KHZ(20)) {
                if (DISABLED_SUB_CHAN(freq, start_freq, punctured))
                        continue;
                c = ieee80211_get_channel_khz(wiphy, freq);
                if (!c)
                        return -EINVAL;

                if (c->flags & IEEE80211_CHAN_RADAR)
                        return 1;
        }
        return 0;
}

int cfg80211_chandef_dfs_required(struct wiphy *wiphy,
				  const struct cfg80211_chan_def *chandef,
				  enum nl80211_iftype iftype)
{
	int width;
	int ret;

	if (WARN_ON(!cfg80211_chandef_valid(chandef)))
		return -EINVAL;

	switch (iftype) {
	case NL80211_IFTYPE_ADHOC:
	case NL80211_IFTYPE_AP:
	case NL80211_IFTYPE_P2P_GO:
	case NL80211_IFTYPE_MESH_POINT:
		if (cfg80211_chandef_device_present(chandef)) {
			ret = cfg80211_get_chans_dfs_required_device(wiphy, chandef);

		} else {
			width = cfg80211_chandef_get_width(chandef);
			if (width < 0)
				return -EINVAL;

			ret = cfg80211_get_chans_dfs_required(wiphy, chandef, iftype);
		}
		return (ret > 0) ? BIT(chandef->width) : ret;
		break;
	case NL80211_IFTYPE_STATION:
	case NL80211_IFTYPE_OCB:
	case NL80211_IFTYPE_P2P_CLIENT:
	case NL80211_IFTYPE_MONITOR:
	case NL80211_IFTYPE_AP_VLAN:
	case NL80211_IFTYPE_P2P_DEVICE:
	case NL80211_IFTYPE_NAN:
		break;
	case NL80211_IFTYPE_WDS:
	case NL80211_IFTYPE_UNSPECIFIED:
	case NUM_NL80211_IFTYPES:
		WARN_ON(1);
	}

	return 0;
}
EXPORT_SYMBOL(cfg80211_chandef_dfs_required);

bool cfg80211_valid_240mhz_freq(const struct cfg80211_chan_def *chandef)
{
	if (chandef->width == NL80211_CHAN_WIDTH_320 &&
	    chandef->center_freq1 == CENTER_FREQ_5G_240MHZ &&
	    ((chandef->punctured & FIXED_PUNCTURE_PATTERN) == FIXED_PUNCTURE_PATTERN)) {
		return true;
	}
	return false;
}

bool cfg80211_chandef_dfs_usable(struct wiphy *wiphy,
				 const struct cfg80211_chan_def *chandef)
{
	struct ieee80211_channel *c;
	int width, count = 0;

	if (WARN_ON(!cfg80211_chandef_valid(chandef)))
		return false;

	width = cfg80211_chandef_get_width(chandef);
	if (width < 0)
		return false;

	/*
	 * Check entire range of channels for the bandwidth.
	 * Check all channels are DFS channels (DFS_USABLE or
	 * DFS_AVAILABLE). Return number of usable channels
	 * (require CAC). Allow DFS and non-DFS channel mix.
	 */
	for_each_subchan(chandef, freq, cf) {
		c = ieee80211_get_channel_khz(wiphy, freq);
		if (!c)
			return false;

		if (c->flags & IEEE80211_CHAN_DISABLED)
			return false;

		if (c->flags & IEEE80211_CHAN_RADAR) {
			if (c->dfs_state == NL80211_DFS_UNAVAILABLE)
				return false;

			if (c->dfs_state == NL80211_DFS_USABLE)
				count++;
		}
	}

	return count > 0;
}
EXPORT_SYMBOL(cfg80211_chandef_dfs_usable);

static int cfg80211_get_chans_dfs_usable_device(struct wiphy *wiphy,
						const struct cfg80211_chan_def *chandef)
{
	struct ieee80211_channel *c;
	u32 freq, start_freq, end_freq, punctured;
	int count = 0;

	start_freq = cfg80211_get_start_freq_device(chandef);
	end_freq = cfg80211_get_end_freq_device(chandef);
	punctured = cfg80211_set_punctured_device(wiphy, chandef);

	/*
	 * Check entire range of channels for the bandwidth.
	 * Check all channels are DFS channels (DFS_USABLE or
	 * DFS_AVAILABLE). Return number of usable channels
	 * (require CAC). Allow DFS and non-DFS channel mix.
	 */
	for (freq = start_freq; freq <= end_freq; freq += MHZ_TO_KHZ(20)) {
		if (DISABLED_SUB_CHAN(freq, start_freq, punctured))
			continue;
		c = ieee80211_get_channel_khz(wiphy, freq);
		if (!c)
			return -EINVAL;

		if (c->flags & IEEE80211_CHAN_DISABLED)
			return -EINVAL;

		if (c->flags & IEEE80211_CHAN_RADAR) {
			if (c->dfs_state == NL80211_DFS_UNAVAILABLE)
				return -EINVAL;

			if (c->dfs_state == NL80211_DFS_USABLE)
				count++;
		}
	}

	return count;
}

bool cfg80211_chandef_dfs_usable_device(struct wiphy *wiphy,
					const struct cfg80211_chan_def *chandef)
{
	int width;
	int r1;

	if (WARN_ON(!cfg80211_chandef_valid(chandef)))
		return false;

	width = nl80211_chan_width_to_mhz(chandef->width_device);
	if (width < 0)
		return false;

	if (!cfg80211_chandef_device_present(chandef))
		return false;

	r1 = cfg80211_get_chans_dfs_usable_device(wiphy, chandef);

	if (r1 > 0)
		return true;

	return false;
}

/*
 * Checks if center frequency of chan falls with in the bandwidth
 * range of chandef.
 */
bool cfg80211_is_sub_chan(struct cfg80211_chan_def *chandef,
			  struct ieee80211_channel *chan,
			  bool primary_only)
{
	int width;
	u32 freq;

	if (!chandef->chan)
		return false;

	if (chandef->chan->center_freq == chan->center_freq)
		return true;

	if (primary_only)
		return false;

	width = cfg80211_chandef_get_width(chandef);
	if (width <= 20)
		return false;

	for (freq = chandef->center_freq1 - width / 2 + 10;
	     freq <= chandef->center_freq1 + width / 2 - 10; freq += 20) {
		if (chan->center_freq == freq)
			return true;
	}

	if (!chandef->center_freq2)
		return false;

	for (freq = chandef->center_freq2 - width / 2 + 10;
	     freq <= chandef->center_freq2 + width / 2 - 10; freq += 20) {
		if (chan->center_freq == freq)
			return true;
	}

	return false;
}

bool cfg80211_beaconing_iface_active(struct wireless_dev *wdev)
{
	unsigned int link;

	lockdep_assert_wiphy(wdev->wiphy);

	switch (wdev->iftype) {
	case NL80211_IFTYPE_AP:
	case NL80211_IFTYPE_P2P_GO:
		for_each_valid_link(wdev, link) {
			if (wdev->links[link].ap.beacon_interval)
				return true;
		}
		break;
	case NL80211_IFTYPE_ADHOC:
		if (wdev->u.ibss.ssid_len)
			return true;
		break;
	case NL80211_IFTYPE_MESH_POINT:
		if (wdev->u.mesh.id_len)
			return true;
		break;
	case NL80211_IFTYPE_STATION:
	case NL80211_IFTYPE_OCB:
	case NL80211_IFTYPE_P2P_CLIENT:
	case NL80211_IFTYPE_MONITOR:
	case NL80211_IFTYPE_AP_VLAN:
	case NL80211_IFTYPE_P2P_DEVICE:
	/* Can NAN type be considered as beaconing interface? */
	case NL80211_IFTYPE_NAN:
		break;
	case NL80211_IFTYPE_UNSPECIFIED:
	case NL80211_IFTYPE_WDS:
	case NUM_NL80211_IFTYPES:
		WARN_ON(1);
	}

	return false;
}

bool cfg80211_wdev_on_sub_chan(struct wireless_dev *wdev,
			       struct ieee80211_channel *chan,
			       bool primary_only)
{
	unsigned int link;

	switch (wdev->iftype) {
	case NL80211_IFTYPE_AP:
	case NL80211_IFTYPE_P2P_GO:
		for_each_valid_link(wdev, link) {
			if (cfg80211_is_sub_chan(&wdev->links[link].ap.chandef,
						 chan, primary_only))
				return true;
		}
		break;
	case NL80211_IFTYPE_ADHOC:
		return cfg80211_is_sub_chan(&wdev->u.ibss.chandef, chan,
					    primary_only);
	case NL80211_IFTYPE_MESH_POINT:
		return cfg80211_is_sub_chan(&wdev->u.mesh.chandef, chan,
					    primary_only);
	default:
		break;
	}

	return false;
}

static bool cfg80211_is_wiphy_oper_chan(struct wiphy *wiphy,
					struct ieee80211_channel *chan)
{
	struct wireless_dev *wdev;

	lockdep_assert_wiphy(wiphy);

	list_for_each_entry(wdev, &wiphy->wdev_list, list) {
		if (!cfg80211_beaconing_iface_active(wdev))
			continue;

		if (cfg80211_wdev_on_sub_chan(wdev, chan, false))
			return true;
	}

	return false;
}

static bool
cfg80211_offchan_chain_is_active(struct cfg80211_registered_device *rdev,
				 struct ieee80211_channel *channel)
{
	if (!rdev->background_radar_wdev)
		return false;

	if (!cfg80211_chandef_valid(&rdev->background_radar_chandef))
		return false;

	return cfg80211_is_sub_chan(&rdev->background_radar_chandef, channel,
				    false);
}

bool cfg80211_any_wiphy_oper_chan(struct wiphy *wiphy,
				  struct ieee80211_channel *chan)
{
	struct cfg80211_registered_device *rdev;

	ASSERT_RTNL();

	if (!(chan->flags & IEEE80211_CHAN_RADAR))
		return false;

	for_each_rdev(rdev) {
		bool found;

		if (!reg_dfs_domain_same(wiphy, &rdev->wiphy))
			continue;

		guard(wiphy)(&rdev->wiphy);

		found = cfg80211_is_wiphy_oper_chan(&rdev->wiphy, chan) ||
			cfg80211_offchan_chain_is_active(rdev, chan);

		if (found)
			return true;
	}

	return false;
}

static bool cfg80211_is_sub_chan_device(struct cfg80211_chan_def *chandef,
					struct ieee80211_channel *chan)
{
	int width;
	u32 start_freq = chandef->center_freq_device;
	u32 end_freq = chandef->center_freq_device;

	if (cfg80211_is_sub_chan(chandef, chan, false) == true)
		return false;

	width = cfg80211_chandef_get_width(chandef);
	if (chandef->center_freq1 > chandef->center_freq_device)
		start_freq -= width;
	else
		end_freq += width;

	if (chan->center_freq >= start_freq && chan->center_freq <= end_freq)
		return true;

	return false;
}

static bool cfg80211_is_wiphy_non_oper_device_chan(struct wiphy *wiphy,
						   struct ieee80211_channel *chan)
{
	struct wireless_dev *wdev;
	unsigned int link;
	struct cfg80211_chan_def *chandef;

	list_for_each_entry(wdev, &wiphy->wdev_list, list) {
		if (wdev->iftype == NL80211_IFTYPE_AP) {
			for_each_valid_link(wdev, link) {
				chandef = &wdev->links[link].ap.chandef;

				if (!chandef->chan || chandef->chan->band != chan->band ||
				    !cfg80211_chandef_device_present(chandef))
					continue;

				if (cfg80211_is_sub_chan_device(&wdev->links[link].ap.chandef,
								chan)) {
					return true;
				}
			}
		}
	}

       return false;
}

bool cfg80211_any_wiphy_non_oper_device_chan(struct wiphy *wiphy,
					     struct ieee80211_channel *chan)
{
	struct cfg80211_registered_device *rdev;

	ASSERT_RTNL();

	if (!(chan->flags & IEEE80211_CHAN_RADAR))
		return false;

	list_for_each_entry(rdev, &cfg80211_rdev_list, list) {
		if (!reg_dfs_domain_same(wiphy, &rdev->wiphy))
			continue;

		if (cfg80211_is_wiphy_non_oper_device_chan(&rdev->wiphy, chan))
			return true;
	}

	return false;
}

bool cfg80211_chandef_dfs_available(struct wiphy *wiphy,
				    const struct cfg80211_chan_def *chandef)
{
	struct ieee80211_channel *c;
	int width;
	bool dfs_offload;

	if (WARN_ON(!cfg80211_chandef_valid(chandef)))
		return false;

	width = cfg80211_chandef_get_width(chandef);
	if (width < 0)
		return false;

	dfs_offload = wiphy_ext_feature_isset(wiphy,
					      NL80211_EXT_FEATURE_DFS_OFFLOAD);

	/*
	 * Check entire range of channels for the bandwidth.
	 * If any channel in between is disabled or has not
	 * had gone through CAC return false
	 */
	for_each_subchan(chandef, freq, cf) {
		c = ieee80211_get_channel_khz(wiphy, freq);
		if (!c)
			return false;

		if (c->flags & IEEE80211_CHAN_DISABLED)
			return false;

		if ((c->flags & IEEE80211_CHAN_RADAR) &&
		    (c->dfs_state != NL80211_DFS_AVAILABLE) &&
		    !(c->dfs_state == NL80211_DFS_USABLE && dfs_offload))
			return false;
	}

	return true;
}
EXPORT_SYMBOL(cfg80211_chandef_dfs_available);

static unsigned int cfg80211_get_chans_dfs_cac_time_dbw(struct wiphy *wiphy,
							const struct cfg80211_chan_def *chandef)
{
	struct ieee80211_channel *c;
	u32 freq, end_freq;
	unsigned int dfs_cac_ms = 0;

	end_freq = cfg80211_get_end_freq_device(chandef);
	for (freq = cfg80211_get_start_freq_device(chandef);
	     freq <= end_freq; freq += MHZ_TO_KHZ(20)) {
		if (!cfg80211_is_freq_device_non_oper(chandef, freq))
			continue;

		c = ieee80211_get_channel_khz(wiphy, freq);
		if (!c)
			continue;

		if (c->flags & IEEE80211_CHAN_DISABLED)
			continue;

		if (!(c->flags & IEEE80211_CHAN_RADAR))
			continue;

		if (c->dfs_cac_ms > dfs_cac_ms)
			dfs_cac_ms = c->dfs_cac_ms;
	}

	return dfs_cac_ms;
}

unsigned int
cfg80211_chandef_dfs_cac_time(struct wiphy *wiphy,
			      const struct cfg80211_chan_def *chandef,
			      bool is_bgcac, bool is_dbw_cac)
{
	struct ieee80211_channel *c;
	int width;
	unsigned int t1 = 0, t2 = 0, dfs_cac_time;

	if (WARN_ON(!cfg80211_chandef_valid(chandef)))
		return 0;

	if (is_dbw_cac) {
		t1 = cfg80211_get_chans_dfs_cac_time_dbw(wiphy, chandef);
		goto exit;
	}

	width = cfg80211_chandef_get_width(chandef);
	if (width < 0)
		return 0;

	for_each_subchan(chandef, freq, cf) {
		c = ieee80211_get_channel_khz(wiphy, freq);
		if (!c || (c->flags & IEEE80211_CHAN_DISABLED)) {
			if (cf == 1)
				t1 = INT_MAX;
			else
				t2 = INT_MAX;
			continue;
		}

		if (!(c->flags & IEEE80211_CHAN_RADAR))
			continue;

		if (cf == 1 && c->dfs_cac_ms > t1)
			t1 = c->dfs_cac_ms;

		if (cf == 2 && c->dfs_cac_ms > t2)
			t2 = c->dfs_cac_ms;
	}

exit:
	if (t1 == INT_MAX && t2 == INT_MAX)
		return 0;

	if (t1 == INT_MAX)
		return t2;

	if (t2 == INT_MAX)
		return t1;

	dfs_cac_time = max(t1, t2);
	if (is_bgcac) {
		if (regulatory_pre_cac_allowed(wiphy)) {
			/* For ETSI,
			   off-channel CAC time  = 6 * CAC time
			   e.g., off-channel CAC time = (6 * 60) secs = 6 mins
			   weather-radar off-channel CAC time = (6 * 10) mins = 1 hour
			 */
			dfs_cac_time = dfs_cac_time * 6;
		} else {
			/* For FCC,
			   off-channel CAC time = CAC time + 2
			   e.g., off-channel CAC time = (60 + 2) seconds
			*/
			dfs_cac_time = dfs_cac_time + REG_PRE_CAC_EXPIRY_GRACE_MS;
		}
	}

	return dfs_cac_time;
}
EXPORT_SYMBOL(cfg80211_chandef_dfs_cac_time);

/* check if the operating channels are valid and supported */
static bool cfg80211_edmg_usable(struct wiphy *wiphy, u8 edmg_channels,
				 enum ieee80211_edmg_bw_config edmg_bw_config,
				 int primary_channel,
				 struct ieee80211_edmg *edmg_cap)
{
	struct ieee80211_channel *chan;
	int i, freq;
	int channels_counter = 0;

	if (!edmg_channels && !edmg_bw_config)
		return true;

	if ((!edmg_channels && edmg_bw_config) ||
	    (edmg_channels && !edmg_bw_config))
		return false;

	if (!(edmg_channels & BIT(primary_channel - 1)))
		return false;

	/* 60GHz channels 1..6 */
	for (i = 0; i < 6; i++) {
		if (!(edmg_channels & BIT(i)))
			continue;

		if (!(edmg_cap->channels & BIT(i)))
			return false;

		channels_counter++;

		freq = ieee80211_channel_to_frequency(i + 1,
						      NL80211_BAND_60GHZ);
		chan = ieee80211_get_channel(wiphy, freq);
		if (!chan || chan->flags & IEEE80211_CHAN_DISABLED)
			return false;
	}

	/* IEEE802.11 allows max 4 channels */
	if (channels_counter > 4)
		return false;

	/* check bw_config is a subset of what driver supports
	 * (see IEEE P802.11ay/D4.0 section 9.4.2.251, Table 13)
	 */
	if ((edmg_bw_config % 4) > (edmg_cap->bw_config % 4))
		return false;

	if (edmg_bw_config > edmg_cap->bw_config)
		return false;

	return true;
}

bool _cfg80211_chandef_usable(struct wiphy *wiphy,
			      const struct cfg80211_chan_def *chandef,
			      u32 prohibited_flags,
			      u32 permitting_flags)
{
	struct ieee80211_sta_ht_cap *ht_cap;
	struct ieee80211_sta_vht_cap *vht_cap;
	struct ieee80211_edmg *edmg_cap;
	u32 width, control_freq, cap;
	bool ext_nss_cap, support_80_80 = false, support_320 = false;
	const struct ieee80211_sband_iftype_data *iftd;
	struct ieee80211_supported_band *sband;
	struct ieee80211_channel *c;
	int i;
	u8 power_mode = NL80211_REG_NUM_POWER_MODES;

	if (WARN_ON(!cfg80211_chandef_valid(chandef)))
		return false;

	ht_cap = &wiphy->bands[chandef->chan->band]->ht_cap;
	vht_cap = &wiphy->bands[chandef->chan->band]->vht_cap;
	edmg_cap = &wiphy->bands[chandef->chan->band]->edmg_cap;
	ext_nss_cap = __le16_to_cpu(vht_cap->vht_mcs.tx_highest) &
			IEEE80211_VHT_EXT_NSS_BW_CAPABLE;

	if (edmg_cap->channels &&
	    !cfg80211_edmg_usable(wiphy,
				  chandef->edmg.channels,
				  chandef->edmg.bw_config,
				  chandef->chan->hw_value,
				  edmg_cap))
		return false;

	control_freq = chandef->chan->center_freq;

	switch (chandef->width) {
	case NL80211_CHAN_WIDTH_1:
		width = 1;
		break;
	case NL80211_CHAN_WIDTH_2:
		width = 2;
		break;
	case NL80211_CHAN_WIDTH_4:
		width = 4;
		break;
	case NL80211_CHAN_WIDTH_8:
		width = 8;
		break;
	case NL80211_CHAN_WIDTH_16:
		width = 16;
		break;
	case NL80211_CHAN_WIDTH_5:
		width = 5;
		break;
	case NL80211_CHAN_WIDTH_10:
		prohibited_flags |= IEEE80211_CHAN_NO_10MHZ;
		width = 10;
		break;
	case NL80211_CHAN_WIDTH_20:
		if (!ht_cap->ht_supported &&
		    chandef->chan->band != NL80211_BAND_6GHZ)
			return false;
		fallthrough;
	case NL80211_CHAN_WIDTH_20_NOHT:
		prohibited_flags |= IEEE80211_CHAN_NO_20MHZ;
		width = 20;
		break;
	case NL80211_CHAN_WIDTH_40:
		width = 40;
		if (chandef->chan->band == NL80211_BAND_6GHZ)
			break;
		if (!ht_cap->ht_supported)
			return false;
		if (!(ht_cap->cap & IEEE80211_HT_CAP_SUP_WIDTH_20_40) ||
		    ht_cap->cap & IEEE80211_HT_CAP_40MHZ_INTOLERANT)
			return false;
		if (chandef->center_freq1 < control_freq &&
		    chandef->chan->flags & IEEE80211_CHAN_NO_HT40MINUS)
			return false;
		if (chandef->center_freq1 > control_freq &&
		    chandef->chan->flags & IEEE80211_CHAN_NO_HT40PLUS)
			return false;
		break;
	case NL80211_CHAN_WIDTH_80P80:
		cap = vht_cap->cap;
		support_80_80 =
			(cap & IEEE80211_VHT_CAP_SUPP_CHAN_WIDTH_160_80PLUS80MHZ) ||
			(cap & IEEE80211_VHT_CAP_SUPP_CHAN_WIDTH_160MHZ &&
			 cap & IEEE80211_VHT_CAP_EXT_NSS_BW_MASK) ||
			(ext_nss_cap &&
			 u32_get_bits(cap, IEEE80211_VHT_CAP_EXT_NSS_BW_MASK) > 1);
		if (chandef->chan->band != NL80211_BAND_6GHZ && !support_80_80)
			return false;
		fallthrough;
	case NL80211_CHAN_WIDTH_80:
		prohibited_flags |= IEEE80211_CHAN_NO_80MHZ;
		width = 80;
		if (chandef->chan->band == NL80211_BAND_6GHZ)
			break;
		if (!vht_cap->vht_supported)
			return false;
		break;
	case NL80211_CHAN_WIDTH_160:
		prohibited_flags |= IEEE80211_CHAN_NO_160MHZ;
		width = 160;
		if (chandef->chan->band == NL80211_BAND_6GHZ)
			break;
		if (!vht_cap->vht_supported)
			return false;
		cap = vht_cap->cap & IEEE80211_VHT_CAP_SUPP_CHAN_WIDTH_MASK;
		if (cap != IEEE80211_VHT_CAP_SUPP_CHAN_WIDTH_160MHZ &&
		    cap != IEEE80211_VHT_CAP_SUPP_CHAN_WIDTH_160_80PLUS80MHZ &&
		    !(ext_nss_cap &&
		      (vht_cap->cap & IEEE80211_VHT_CAP_EXT_NSS_BW_MASK)))
			return false;
		break;
	case NL80211_CHAN_WIDTH_320:
		prohibited_flags |= IEEE80211_CHAN_NO_320MHZ;
		width = 320;

		if ((chandef->chan->band != NL80211_BAND_6GHZ) &&
		    (!cfg80211_valid_240mhz_freq(chandef)))
			return false;

		if (cfg80211_valid_240mhz_freq(chandef))
			sband = wiphy->bands[NL80211_BAND_5GHZ];
		else
			sband = wiphy->bands[NL80211_BAND_6GHZ];
		if (!sband)
			return false;

		for_each_sband_iftype_data(sband, i, iftd) {
			if (!iftd->eht_cap.has_eht)
				continue;

			if (iftd->eht_cap.eht_cap_elem.phy_cap_info[0] &
			    IEEE80211_EHT_PHY_CAP0_320MHZ_IN_6GHZ) {
				support_320 = true;
				break;
			}
		}

		if (!support_320)
			return false;
		break;
	default:
		WARN_ON_ONCE(1);
		return false;
	}

	/*
	 * TODO: What if there are only certain 80/160/80+80 MHz channels
	 *	 allowed by the driver, or only certain combinations?
	 *	 For 40 MHz the driver can set the NO_HT40 flags, but for
	 *	 80/160 MHz and in particular 80+80 MHz this isn't really
	 *	 feasible and we only have NO_80MHZ/NO_160MHZ so far but
	 *	 no way to cover 80+80 MHz or more complex restrictions.
	 *	 Note that such restrictions also need to be advertised to
	 *	 userspace, for example for P2P channel selection.
	 */

	if (width > 20)
		prohibited_flags |= IEEE80211_CHAN_NO_OFDM;

	/* 5 and 10 MHz are only defined for the OFDM PHY */
	if (width < 20)
		prohibited_flags |= IEEE80211_CHAN_NO_OFDM;

	if (chandef->chan->band == NL80211_BAND_6GHZ)
		power_mode = cfg80211_get_6ghz_power_mode_from_chan(wiphy,
								    chandef->chan);

	for_each_subchan(chandef, freq, cf) {
		if (power_mode != NL80211_REG_NUM_POWER_MODES)
			c = ieee80211_get_6g_channel_khz(wiphy, freq, power_mode);
		else
			c = ieee80211_get_channel_khz(wiphy, freq);

		if (!c)
			return false;
		if (c->flags & permitting_flags)
			continue;
		if (c->flags & prohibited_flags)
			return false;
	}

	return true;
}

int cfg80211_validate_freq_width_for_pwr_mode(struct wiphy *wiphy,
					      struct cfg80211_chan_def *chandef,
					      u8 reg_6ghz_power_mode,
					      u32 prohibited_flags)
{
	struct ieee80211_channel *c;

	for_each_subchan(chandef, freq, cf) {
		c = ieee80211_get_6g_channel_khz(wiphy, freq, reg_6ghz_power_mode);
		if (!c)
			return -EINVAL;

		if (c->flags & prohibited_flags)
			return -EOPNOTSUPP;
	}

	return 0;
}
EXPORT_SYMBOL(cfg80211_validate_freq_width_for_pwr_mode);

int
cfg80211_update_chandef_6ghz_power_mode(const struct net_device *netdev,
					u8 link_id,
					u8 power_mode)
{
	struct wireless_dev *wdev = netdev->ieee80211_ptr;
	struct ieee80211_channel *chan;
	struct cfg80211_chan_def *chandef;
	u32 prohibited_flags;

	lockdep_assert_wiphy(wdev->wiphy);

	chandef = wdev_chandef(wdev, link_id);
	if (!chandef) {
		return -EINVAL;
	}

	prohibited_flags = IEEE80211_CHAN_DISABLED | IEEE80211_CHAN_NO_IR;
	if (cfg80211_validate_freq_width_for_pwr_mode(wdev->wiphy,
						      chandef,
						      power_mode,
						      prohibited_flags)) {
		return -EINVAL;
	}

	chan = ieee80211_get_6g_channel_khz(wdev->wiphy,
					    MHZ_TO_KHZ(chandef->chan->center_freq),
					    power_mode);
	if (!chan) {
		return -EINVAL;
	}

	chandef->chan = chan;
	wdev->links[link_id].reg_6g_power_mode = power_mode;

	return 0;
}
EXPORT_SYMBOL(cfg80211_update_chandef_6ghz_power_mode);

u8
cfg80211_get_6ghz_power_mode_from_chan(const struct wiphy *wiphy,
				       const struct ieee80211_channel *chan)
{
	const struct ieee80211_supported_band *sband = wiphy->bands[chan->band];
	u8 i;

	/* For 6 GHz band, the channel should be in any of the
	 * power mode list.
	 */
	for (i = 0; i < NL80211_REG_NUM_POWER_MODES; i++) {
		struct ieee80211_channel *chan_6ghz;
		int n_chans;

		if (!sband->chan_6g[i])
			continue;
		chan_6ghz = sband->chan_6g[i]->channels;
		n_chans = sband->chan_6g[i]->n_channels;

		if (!chan_6ghz || !n_chans)
			continue;

		if (chan >= chan_6ghz &&
		    chan <= (chan_6ghz + n_chans - 1)) {
			return i;
		}
	}

	return NL80211_REG_NUM_POWER_MODES;
}
EXPORT_SYMBOL(cfg80211_get_6ghz_power_mode_from_chan);

bool cfg80211_chandef_usable(struct wiphy *wiphy,
			     const struct cfg80211_chan_def *chandef,
			     u32 prohibited_flags)
{
	return _cfg80211_chandef_usable(wiphy, chandef, prohibited_flags, 0);
}
EXPORT_SYMBOL(cfg80211_chandef_usable);

static bool cfg80211_ir_permissive_check_wdev(enum nl80211_iftype iftype,
					      struct wireless_dev *wdev,
					      struct ieee80211_channel *chan)
{
	struct ieee80211_channel *other_chan = NULL;
	unsigned int link_id;
	int r1, r2;

	for_each_valid_link(wdev, link_id) {
		if (wdev->iftype == NL80211_IFTYPE_STATION &&
		    wdev->links[link_id].client.current_bss)
			other_chan = wdev->links[link_id].client.current_bss->pub.channel;

		/*
		 * If a GO already operates on the same GO_CONCURRENT channel,
		 * this one (maybe the same one) can beacon as well. We allow
		 * the operation even if the station we relied on with
		 * GO_CONCURRENT is disconnected now. But then we must make sure
		 * we're not outdoor on an indoor-only channel.
		 */
		if (iftype == NL80211_IFTYPE_P2P_GO &&
		    wdev->iftype == NL80211_IFTYPE_P2P_GO &&
		    wdev->links[link_id].ap.beacon_interval &&
		    !(chan->flags & IEEE80211_CHAN_INDOOR_ONLY))
			other_chan = wdev->links[link_id].ap.chandef.chan;

		if (!other_chan)
			continue;

		if (chan == other_chan)
			return true;

		if (chan->band != NL80211_BAND_5GHZ &&
		    chan->band != NL80211_BAND_6GHZ)
			continue;

		r1 = cfg80211_get_unii(chan->center_freq);
		r2 = cfg80211_get_unii(other_chan->center_freq);

		if (r1 != -EINVAL && r1 == r2) {
			/*
			 * At some locations channels 149-165 are considered a
			 * bundle, but at other locations, e.g., Indonesia,
			 * channels 149-161 are considered a bundle while
			 * channel 165 is left out and considered to be in a
			 * different bundle. Thus, in case that there is a
			 * station interface connected to an AP on channel 165,
			 * it is assumed that channels 149-161 are allowed for
			 * GO operations. However, having a station interface
			 * connected to an AP on channels 149-161, does not
			 * allow GO operation on channel 165.
			 */
			if (chan->center_freq == 5825 &&
			    other_chan->center_freq != 5825)
				continue;
			return true;
		}
	}

	return false;
}

/*
 * Check if the channel can be used under permissive conditions mandated by
 * some regulatory bodies, i.e., the channel is marked with
 * IEEE80211_CHAN_IR_CONCURRENT and there is an additional station interface
 * associated to an AP on the same channel or on the same UNII band
 * (assuming that the AP is an authorized master).
 * In addition allow operation on a channel on which indoor operation is
 * allowed, iff we are currently operating in an indoor environment.
 */
static bool cfg80211_ir_permissive_chan(struct wiphy *wiphy,
					enum nl80211_iftype iftype,
					struct ieee80211_channel *chan)
{
	struct wireless_dev *wdev;
	struct cfg80211_registered_device *rdev = wiphy_to_rdev(wiphy);

	lockdep_assert_held(&rdev->wiphy.mtx);

	if (!IS_ENABLED(CPTCFG_CFG80211_REG_RELAX_NO_IR) ||
	    !(wiphy->regulatory_flags & REGULATORY_ENABLE_RELAX_NO_IR))
		return false;

	/* only valid for GO and TDLS off-channel (station/p2p-CL) */
	if (iftype != NL80211_IFTYPE_P2P_GO &&
	    iftype != NL80211_IFTYPE_STATION &&
	    iftype != NL80211_IFTYPE_P2P_CLIENT)
		return false;

	if (regulatory_indoor_allowed() &&
	    (chan->flags & IEEE80211_CHAN_INDOOR_ONLY))
		return true;

	if (!(chan->flags & IEEE80211_CHAN_IR_CONCURRENT))
		return false;

	/*
	 * Generally, it is possible to rely on another device/driver to allow
	 * the IR concurrent relaxation, however, since the device can further
	 * enforce the relaxation (by doing a similar verifications as this),
	 * and thus fail the GO instantiation, consider only the interfaces of
	 * the current registered device.
	 */
	list_for_each_entry(wdev, &rdev->wiphy.wdev_list, list) {
		bool ret;

		ret = cfg80211_ir_permissive_check_wdev(iftype, wdev, chan);
		if (ret)
			return ret;
	}

	return false;
}

static bool _cfg80211_reg_can_beacon(struct wiphy *wiphy,
				     struct cfg80211_chan_def *chandef,
				     enum nl80211_iftype iftype,
				     u32 prohibited_flags,
				     u32 permitting_flags)
{
	bool res, check_radar;
	int dfs_required;

	trace_cfg80211_reg_can_beacon(wiphy, chandef, iftype,
				      prohibited_flags,
				      permitting_flags);

	if (!_cfg80211_chandef_usable(wiphy, chandef,
				      IEEE80211_CHAN_DISABLED, 0))
		return false;

	dfs_required = cfg80211_chandef_dfs_required(wiphy, chandef, iftype);
	check_radar = dfs_required != 0;

	if (dfs_required > 0 &&
	    cfg80211_chandef_dfs_available(wiphy, chandef)) {
		/* We can skip IEEE80211_CHAN_NO_IR if chandef dfs available */
		prohibited_flags &= ~IEEE80211_CHAN_NO_IR;
		check_radar = false;
	}

	if (enable_dfs_test_mode)
		check_radar = false;

	if (check_radar &&
	    !_cfg80211_chandef_usable(wiphy, chandef,
				      IEEE80211_CHAN_RADAR, 0))
		return false;

	res = _cfg80211_chandef_usable(wiphy, chandef,
				       prohibited_flags,
				       permitting_flags);

	trace_cfg80211_return_bool(res);
	return res;
}

bool cfg80211_reg_check_beaconing(struct wiphy *wiphy,
				  struct cfg80211_chan_def *chandef,
				  struct cfg80211_beaconing_check_config *cfg)
{
	struct cfg80211_registered_device *rdev = wiphy_to_rdev(wiphy);
	u32 permitting_flags = 0;
	bool check_no_ir = true;

	/*
	 * Under certain conditions suggested by some regulatory bodies a
	 * GO/STA can IR on channels marked with IEEE80211_NO_IR. Set this flag
	 * only if such relaxations are not enabled and the conditions are not
	 * met.
	 */
	if (cfg->relax) {
		lockdep_assert_held(&rdev->wiphy.mtx);
		check_no_ir = !cfg80211_ir_permissive_chan(wiphy, cfg->iftype,
							   chandef->chan);
	}

	if (cfg->reg_power == IEEE80211_REG_VLP_AP)
		permitting_flags |= IEEE80211_CHAN_ALLOW_6GHZ_VLP_AP;

	if ((cfg->iftype == NL80211_IFTYPE_P2P_GO ||
	     cfg->iftype == NL80211_IFTYPE_AP) &&
	    (chandef->width == NL80211_CHAN_WIDTH_20_NOHT ||
	     chandef->width == NL80211_CHAN_WIDTH_20))
		permitting_flags |= IEEE80211_CHAN_ALLOW_20MHZ_ACTIVITY;

	return _cfg80211_reg_can_beacon(wiphy, chandef, cfg->iftype,
					check_no_ir ? IEEE80211_CHAN_NO_IR : 0,
					permitting_flags);
}
EXPORT_SYMBOL(cfg80211_reg_check_beaconing);

int cfg80211_set_monitor_channel(struct cfg80211_registered_device *rdev,
				 struct net_device *dev,
				 struct cfg80211_chan_def *chandef)
{
	if (!rdev->ops->set_monitor_channel)
		return -EOPNOTSUPP;
	if (!(rdev->wiphy.flags &
	      WIPHY_FLAG_SUPPORTS_CONCUR_MONITOR_N_OTHER_VIF) &&
	    !cfg80211_has_monitors_only(rdev))
		return -EBUSY;

	return rdev_set_monitor_channel(rdev, dev, chandef);
}

bool cfg80211_any_usable_channels(struct wiphy *wiphy,
				  unsigned long sband_mask,
				  u32 prohibited_flags)
{
	int idx;

	prohibited_flags |= IEEE80211_CHAN_DISABLED;

	for_each_set_bit(idx, &sband_mask, NUM_NL80211_BANDS) {
		struct ieee80211_supported_band *sband = wiphy->bands[idx];
		int chanidx;

		if (!sband)
			continue;

		for (chanidx = 0; chanidx < sband->n_channels; chanidx++) {
			struct ieee80211_channel *chan;

			chan = &sband->channels[chanidx];

			if (chan->flags & prohibited_flags)
				continue;

			return true;
		}
	}

	return false;
}
EXPORT_SYMBOL(cfg80211_any_usable_channels);

struct cfg80211_chan_def *wdev_chandef(struct wireless_dev *wdev,
				       unsigned int link_id)
{
	lockdep_assert_wiphy(wdev->wiphy);

	WARN_ON(wdev->valid_links && !(wdev->valid_links & BIT(link_id)));
	WARN_ON(!wdev->valid_links && link_id > 0);

	switch (wdev->iftype) {
	case NL80211_IFTYPE_MESH_POINT:
		return &wdev->u.mesh.chandef;
	case NL80211_IFTYPE_ADHOC:
		return &wdev->u.ibss.chandef;
	case NL80211_IFTYPE_OCB:
		return &wdev->u.ocb.chandef;
	case NL80211_IFTYPE_AP:
	case NL80211_IFTYPE_P2P_GO:
		return &wdev->links[link_id].ap.chandef;
	default:
		return NULL;
	}
}
EXPORT_SYMBOL(wdev_chandef);
