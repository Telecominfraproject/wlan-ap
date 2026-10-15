#
# Copyright (C) 2023 Mediatek Ltd.
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

from algo import ALGO
from algo import parse_entry
import os

class HMAC(ALGO):
    def parse(self, rsp_file):
        test_suites = []
        suite = {}
        test = {}

        with open(rsp_file, 'r') as f:
            while True:
                line = f.readline()
                if line == '':
                    break

                if line == '\n' or line == '\r\n':
                    continue

                if line.startswith('#'):
                    continue

                line = line.strip()

                if line.startswith('['):
                    e = line[1:-1]
                    if suite:
                        suite['Tests'].append(test)
                        test_suites.append(suite)
                    suite = {'L': e, 'Tests': []}
                    key, val = parse_entry(e)
                    suite[key] = val
                    test = {}
                    continue
                if line.startswith('Count'):
                    if test:
                        suite['Tests'].append(test)
                    test = {}
                    continue

                key, val = parse_entry(line)
                if key in test:
                    key = key + '2'
                test[key] = val
            suite['Tests'].append(test)
        test_suites.append(suite)
        self.test_suites = test_suites


    def hw_command(self, suite, test, algo):
        e = "echo -n " + "\"" + test['Msg'] + "\"" + " | xxd -r -p | "
        o = "openssl dgst "  + algo + " -mac hmac -macopt hexkey:" + test['Key']
        command = e + o + " -engine devcrypto"
        result = os.popen(command).read()
        key, val = result.split('=')
        f = val.strip()
        return f[0:int(test['Tlen'])*2]

    def sw_command(self, suite, test, algo):
        command = "kcapi-mtk-dgst -k " + test['Key'] + " -n " + algo + " -m " + test['Msg'] + " -l " + str(test['Tlen'])
        result = os.popen(command).read()
        return result.replace("\n", "")

    def select_algo(self, suites):
        algo = "none"
        if suites['L'] == '20':
            algo = '-sha1'
        elif suites['L'] == '28':
            algo = '-sha224'
        elif suites['L'] == '32':
            algo = '-sha256'
        elif suites['L'] == '48':
            algo = '-sha384'
        elif suites['L'] == '64':
            algo = '-sha512'

        if self.engine == "sw":
            algo = "hmac" + algo
        return algo

    def get_expect(self, test):
        return test['Mac']
