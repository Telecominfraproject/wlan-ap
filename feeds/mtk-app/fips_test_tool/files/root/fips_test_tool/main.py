#
# Copyright (C) 2023 Mediatek Ltd.
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

import argparse
from gcm import GCM
from ccm import CCM
from cbc import CBC
from ecb import ECB
from drbg import DRBG
from hmac import HMAC
from sha import SHA
from tdes import TDES

dir_path = {"CBC":"./testcase/cbc", "CBC_Monte":"./testcase/cbc_monte", \
        "ECB":"./testcase/ecb", "ECB_Monte":"./testcase/ecb_monte", \
        "CCM":"./testcase/ccm", "GCM":"./testcase/gcm", \
        "DRBG":"./testcase/drbg", "HMAC":"./testcase/hmac", \
        "SHA":"./testcase/sha", "SHA_Monte":"./testcase/sha_monte", \
        "TDES":"./testcase/tdes", "TDES_Monte":"./testcase/tdes_monte"}

def print_list():
    print("List all test command")
    print("\tpython3 main.py -a CBC -e hw        => Test CBC in hw mode")
    print("\tpython3 main.py -a CBC -e sej       => Test CBC in sej mode")
    print("\tpython3 main.py -a CBC -e hw -m     => Test CBC Monte in hw mode")
    print("\tpython3 main.py -a CBC -e sej -m    => Test CBC Monte in sej mode")
    print("\tpython3 main.py -a ECB -e hw        => Test EBC in hw mode")
    print("\tpython3 main.py -a ECB -e sej       => Test EBC in sej mode")
    print("\tpython3 main.py -a ECB -e hw -m     => Test EBC Monte in hw mode")
    print("\tpython3 main.py -a ECB -e sej -m    => Test EBC Monte in sej mode")
    print("\tpython3 main.py -a CCM -e hw        => Test CCM in hw mode")
    print("\tpython3 main.py -a GCM -e hw        => Test GCM in hw mode")
    print("\tpython3 main.py -a DRBG -e sw       => Test DRBG in sw mode")
    print("\tpython3 main.py -a HMAC -e hw       => Test HMAC in hw mode")
    print("\tpython3 main.py -a HMAC -e sw       => Test HMAC in sw mode")
    print("\tpython3 main.py -a SHA -e hw        => Test SHA in hw mode")
    print("\tpython3 main.py -a SHA -e hw -m     => Test SHA Monte in hw mode")
    print("\tpython3 main.py -a SHA -e sw        => Test SHA in sw mode")
    print("\tpython3 main.py -a SHA -e sw -m     => Test SHA Montein hw mode")
    print("\tpython3 main.py -a TDES -e hw       => Test TDES in hw mode")
    print("\tpython3 main.py -a TDES -e hw -m    => Test TDES Monte in hw mode")
    exit()

def parse():
    parser = argparse.ArgumentParser(description="FIPS Test Script")
    parser.add_argument("-a", "--algo", choices=["CBC", "ECB", "CCM", "GCM", "DRBG", "HMAC", "SHA", "TDES"], help="Algorithm to test")
    parser.add_argument("-m", action="store_true", help="Enable Monte test, only use in CBC, EBC, SHA and TDES")
    parser.add_argument("-e", "--engine", choices=["sw", "hw", "sej"],help="Select hw or sw")
    parser.add_argument("-l", action="store_true", help="List all test algo and mode")

    return parser.parse_args()

def select_algo(algo, monte_flag, engine):
    if algo == "CBC":
        if monte_flag == True:
            return CBC(engine, dir_path['CBC_Monte'])
        else:
            return CBC(engine, dir_path['CBC'])
    if algo == "ECB":
        if monte_flag == True:
            return ECB(engine, dir_path['ECB_Monte'])
        else:
            return ECB(engine, dir_path['ECB'])
    if algo == "SHA":
        if monte_flag == True:
            return SHA(engine, dir_path['SHA_Monte'])
        else:
            return SHA(engine, dir_path['SHA'])
    if algo == "TDES":
        if monte_flag == True:
            return TDES(engine, dir_path['TDES_Monte'])
        else:
            return TDES(engine, dir_path['TDES'])
    if algo == "CCM":
        return CCM(engine, dir_path['CCM'])
    if algo == "GCM":
        return GCM(engine, dir_path['GCM'])
    if algo == "DRBG":
        return DRBG(engine, dir_path['DRBG'])
    if algo == "HMAC":
        return HMAC(engine, dir_path['HMAC'])

def main():
    args = parse()

    if args.l == True:
        print_list()

    if args.algo != None and args.engine != None:
        algo = select_algo(args.algo, args.m, args.engine)
        algo.run_test()
    else:
        print_list()
        exit()

if __name__ == "__main__":
    main()
