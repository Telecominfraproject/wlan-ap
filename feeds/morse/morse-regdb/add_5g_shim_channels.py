#!/usr/bin/env python3


import csv
import sys

# Fake 5g channels reported by netlink instead of 1g channels.
S1G_TO_5G= {
    1: 132,
    2: 134,
    3: 136,
    4: 'NA',
    5: 36,
    6: 38,
    7: 40,
    8: 42,
    9: 44,
    10: 46,
    11: 48,
    12: 50,
    13: 52,
    14: 54,
    15: 56,
    16: 58,
    17: 60,
    18: 62,
    19: 64,
    20: 'NA',
    21: 100,
    22: 102,
    23: 104,
    24: 106,
    25: 108,
    26: 110,
    27: 112,
    28: 114,
    29: 116,
    30: 118,
    31: 120,
    32: 122,
    33: 124,
    34: 126,
    35: 128,
    36: 'NA',
    37: 149,
    38: 151,
    39: 153,
    40: 155,
    41: 157,
    42: 159,
    43: 161,
    44: 163,
    45: 165,
    46: 167,
    47: 169,
    48: 171,
    49: 173,
    50: 175,
    51: 177,
}

# Japanese channels are... different.
JAPAN_S1G_TO_5G = {
    9: 108,
    13: 36,
    15: 40,
    17: 44,
    19: 48,
    21: 64,
    2: 38,
    6: 46,
    4: 54,
    8: 62,
    36: 42,
    38: 58,
}


dr = csv.DictReader(sys.stdin)
dw = csv.DictWriter(sys.stdout, dr.fieldnames + ['5g_chan'], lineterminator='\n')
dw.writeheader()
for row in dr:
     m = JAPAN_S1G_TO_5G if row['country_code'] == 'JP' else S1G_TO_5G
     row['5g_chan'] = m.get(int(row['s1g_chan']))

     assert row['5g_chan'], f'Missing 5G chan map for Channel {row["s1g_chan"]} in {row["country_code"]}'

     dw.writerow(row)
