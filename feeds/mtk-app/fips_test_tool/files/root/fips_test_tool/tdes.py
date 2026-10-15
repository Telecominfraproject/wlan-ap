#
# Copyright (C) 2023 Mediatek Ltd.
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

from algo import ALGO
from algo import parse_entry
import os

class TDES(ALGO):
    def __init__(self, engine, dir_path):
        super().__init__(engine, dir_path)
        self.multi_flag = False

    def parse(self, rsp_file):
        test_suites = []
        suite = {}
        test = {}
        algo = ""
        mode = ""

        with open(rsp_file, 'r') as f:
            while True:
                line = f.readline()
                if line == '':
                    break

                if line == '\n' or line == '\r\n':
                    continue

                if line.startswith('#'):
                    if line.find('CBC') != -1:
                        algo = "-des-ede3-cbc"
                    elif algo == "":
                        algo = "-des-ede3-ecb"

                    if(line.find('Monte') != -1):
                       self.monte_flag = True
                       self.multi_flag = True
                    if(line.find('Multi block Message') != -1):
                       self.multi_flag = True
                    continue

                line = line.strip()

                if line.startswith('['):
                    e = line[1:-1]
                    if not '=' in e:
                        if suite:
                            test['mode'] = mode
                            suite['Tests'].append(test)
                            test_suites.append(suite)
                        suite = {'Algorithm': algo, 'Tests': []}
                        mode = e
                        test = {}
                    else:
                        key, val = parse_entry(e)
                        suite[key] = val
                    continue

                if line.startswith('COUNT'):
                    if test:
                        test['mode'] = mode
                        suite['Tests'].append(test)
                    test = {}
                    continue

                key, val = parse_entry(line)
                if key in test:
                    key = key + '2'
                test[key] = val
            test['mode'] = mode
            suite['Tests'].append(test)
        test_suites.append(suite)
        self.test_suites = test_suites

    def clear(self):
        self.multi_flag = False

    def hw_command(self, suite, test, algo):
        data = ""
        key = ""

        if test['mode'] == 'ENCRYPT':
            algo = " -e " + algo
            data = test['PLAINTEXT']
        elif test['mode'] == 'DECRYPT':
            algo = " -d " + algo
            data = test['CIPHERTEXT']

        if self.multi_flag == True:
            key = test['KEY1'] + test['KEY2'] + test['KEY3']
        else:
            key = test['KEYs']

        e = "echo -n " + "\"" + data + "\"" + " | xxd -r -p | "
        o = "openssl enc" + algo + " -K " + key + " -nopad -engine devcrypto"
        s = " | xxd | cut -c10-50"

        if algo.find("cbc") != -1:
            o = o + " -iv " + test['IV']

        command = e + o + s
        result = os.popen(command).read().replace(" ", "").replace("\n", "")
        return result

    def select_algo(self, suites):
        return suites['Algorithm']

    def get_expect(self, test):
        if test['mode'] == 'ENCRYPT':
            return test['CIPHERTEXT']
        if test['mode'] == 'DECRYPT':
            return test['PLAINTEXT']

    def run_monte_command(self, suite, algo):
        expect = ""
        key = ""
        result = ""
        iv = ""
        count = 0

        for test in suite['Tests']:
            if (test['mode'] == 'ENCRYPT'):
                algo = " -e " + algo
                data = test['PLAINTEXT']
                expect = test['CIPHERTEXT']
            elif test['mode'] == 'DECRYPT':
                algo = " -d " + algo
                data = test['CIPHERTEXT']
                expect = test['PLAINTEXT']

            if algo.find("cbc") != -1:
                iv = test['IV']

            for i in range(10000):
                if self.engine == "sw":
                    result = self.sw_command(suite, test, algo)
                elif self.engine == "hw":
                    result = self.hw_command(suite, test, algo)

                if((i != 0  and test['mode'] == 'ENCRYPT') or test['mode'] == 'DECRYPT'):
                    iv = data
                    test['IV'] = iv
                data = result.replace("\n", "")
                if test['mode'] == 'ENCRYPT':
                    test['PLAINTEXT'] = data
                elif test['mode'] == 'DECRYPT':
                    test['CIPHERTEXT'] = data

            if(result.replace("\n", "") == expect):
                count = count + 1;
            else:
                return False
        return True
