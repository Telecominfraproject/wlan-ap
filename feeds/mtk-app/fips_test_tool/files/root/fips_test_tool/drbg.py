#
# Copyright (C) 2023 Mediatek Ltd.
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

from algo import ALGO
from algo import parse_entry
import os

class DRBG(ALGO):
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
                    if not '=' in e:
                        if suite:
                            suite['Tests'].append(test)
                            test_suites.append(suite)
                        suite = {'Algorithm': e, 'Tests': []}
                        test = {}
                    else:
                        key, val = parse_entry(e)
                        suite[key] = val
                    continue

                if line.startswith('COUNT'):
                    if test:
                        suite['Tests'].append(test)
                    test = {}
                    continue

                key, val = parse_entry(line)
                if key in test:
                    key = key + '2'
                test[key] = val
        test_suites.append(suite)
        self.test_suites = test_suites

    def sw_command(self, suite, test, algo):
        ret_len = int(int(suite['ReturnedBitsLen'])/8)
        command = "kcapi-drbg --hex -r " + algo
        command = command + " -e " + test['EntropyInput'] + " -n " + test['Nonce'] + " -b " + str(ret_len)
        if(test['PersonalizationString'] != ''):
            command = command + ' -p ' + test['PersonalizationString']
        if(test['AdditionalInput'] != ''):
            command = command + ' -a ' + test['AdditionalInput']
        if(test['AdditionalInput2'] != ''):
            command = command + ' -c ' + test['AdditionalInput2']
        result = os.popen(command).read()
        return result.replace(" ", "").replace("\n", "")

    def select_algo(self, suites):
        algo = "none"
        if suites['Algorithm'] == 'SHA-512':
            algo = 'drbg_nopr_hmac_sha512'
        elif suites['Algorithm'] == 'SHA-384':
            algo = 'drbg_nopr_hmac_sha384'
        elif suites['Algorithm'] == 'SHA-256':
            algo = 'drbg_nopr_hmac_sha256'
        elif suites['Algorithm'] == 'SHA-1':
            algo = 'drbg_nopr_hmac_sha1'
        return algo

    def get_expect(self, test):
        return test['ReturnedBits']
