#
# Copyright (C) 2023 Mediatek Ltd.
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

from algo import ALGO
from algo import parse_entry
import os

class CBC(ALGO):
    def __init__(self, engine, dir_path):
        super().__init__(engine, dir_path)
        self.mode = ""

    def parse(self, rsp_file):
        test_suites = []
        suite = {}
        test = {}
        algo = "cbc"
        key_len = ""

        with open(rsp_file, 'r') as f:
            while True:
                line = f.readline()
                if line == '':
                    break

                if line == '\n' or line == '\r\n':
                    continue

                if line.startswith('#'):
                    if line.find('Key Length : ') != -1:
                        tmp = line.find('Key Length : ') + len('Key Length : ')
                        key_len = line[tmp : -1]

                    if(line.find('MCT') != -1):
                       self.monte_flag = True
                    continue

                line = line.strip()

                if line.startswith('['):
                    e = line[1:-1]
                    if not '=' in e:
                        if suite:
                            suite['Tests'].append(test)
                            test_suites.append(suite)
                        suite = {'mode': e, 'Tests': [], \
                                'Algorithm' : algo, 'keylen':key_len}
                        self.mode = e
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
            suite['Tests'].append(test)
        test_suites.append(suite)
        self.test_suites = test_suites

    def hw_command(self, suite, test, algo):
        data = ""
        mode = ""

        if self.mode == "ENCRYPT":
            data = test['PLAINTEXT']
            mode = " -e "
        elif self.mode == 'DECRYPT':
            data = test['CIPHERTEXT']
            mode = " -d "
        e = "echo -n " + "\"" + data + "\"" + " | xxd -r -p | "
        o = "openssl enc " + algo + mode + " -K " + test['KEY'] + " -iv "+ test['IV'] + " -nopad -engine devcrypto"
        s = " | xxd | cut -c10-50"
        command = e + o + s

        result = os.popen(command).read().replace(" ", "")

        return result.replace("\n", "").replace(" ", "")

    def sej_command(self, suite, test, algo):
        data = ""
        mode = ""

        if self.mode == "ENCRYPT":
            data = test['PLAINTEXT']
            mode = "enc"
        elif self.mode == 'DECRYPT':
            data = test['CIPHERTEXT']
            mode = "dec"
        command = "crypto_test " + mode + " cbc -k " + test['KEY'] + \
                  " -i "+ test['IV']+ " " + data
        result = os.popen(command).read().replace(" ", "")
        return result.replace("\n", "").replace(" ", "")

    def select_algo(self, suites):
        algo = "none"
        if suites['keylen'] == '128':
            algo = '-aes-128-cbc'
        elif suites['keylen'] == '192':
            algo = '-aes-192-cbc'
        elif suites['keylen'] == '256':
            algo = '-aes-256-cbc'

        return algo

    def get_expect(self, test):
        if self.mode == 'ENCRYPT':
            return test['CIPHERTEXT']
        if self.mode == 'DECRYPT':
            return test['PLAINTEXT']

    def run_monte_command(self, suite, algo):
        expect = ""
        result = ""
        iv = ""
        p1 = ""
        p2 = ""
        p3 = ""
        count = 0

        for test in suite['Tests']:
            self.mode = suite['mode']
            p1 = test['IV']
            p2 = test['IV']
            p3 = test['IV']

            if (self.mode == 'ENCRYPT'):
                mode = " -e "
                data = test['PLAINTEXT']
                expect = test['CIPHERTEXT']
            else:
                mode = " -d "
                data = test['CIPHERTEXT']
                expect = test['PLAINTEXT']

            for i in range(1000):
                if self.engine == "sw":
                    result = self.sw_command(suite, test, algo)
                elif self.engine == "hw":
                    result = self.hw_command(suite, test, algo)
                elif self.engine == "sej":
                    result = self.sej_command(suite, test, algo)


                if(self.mode == 'ENCRYPT'):
                    if(i == 0):
                        iv = test['IV']
                    else:
                        iv = data
                    data = result.replace("\n", "")
                    test['PLAINTEXT'] = data
                    test['IV'] = iv
                elif(self.mode == 'DECRYPT'):
                    if(i != 0):
                        iv = data
                    data = result.replace("\n", "")
                    p3 = p2
                    p2 = p1
                    p1 = data
                    if(i == 0):
                        data = test['IV']
                        iv = test['CIPHERTEXT']
                    else:
                        iv = p3
                        data = p2
                    test['CIPHERTEXT'] = data
                    test['IV'] = iv

            if(result.replace("\n", "") == expect):
                count = count + 1;
            else:
                return False
                break
        return True

    def run_command(self):
        result = ""
        expect = ""
        pass_count = 0
        total = 0

        for suites in self.test_suites:
            count = 0
            algo = self.select_algo(suites)
            if(algo == "none"):
                continue

            for test in suites['Tests']:
                if self.engine == "sw":
                    result = self.sw_command(suites, test, algo)
                elif self.engine == "hw":
                    result = self.hw_command(suites, test, algo)
                elif self.engine == "sej":
                    result = self.sej_command(suites, test, algo)

                expect = self.get_expect(test)

                if(result == expect):
                    pass_count = pass_count + 1
                    count = count + 1
            total = total + len(suites['Tests'])
        print("\t\t\ttotal case: %d, pass case: %d, rate: %f" %(total, pass_count, pass_count/total))
