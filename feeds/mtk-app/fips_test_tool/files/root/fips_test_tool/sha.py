#
# Copyright (C) 2023 Mediatek Ltd.
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

from algo import ALGO
from algo import parse_entry
import os

class SHA(ALGO):
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
                    if line.find('Monte') != -1:
                        self.monte_flag = True
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
                if line.startswith('Len') and self.monte_flag == False:
                    if test:
                        suite['Tests'].append(test)
                if line.startswith('COUNT') and self.monte_flag == True:
                    if test:
                        suite['Tests'].append(test)
                    test = {}

                key, val = parse_entry(line)
                if key in test:
                    key = key + '2'
                test[key] = val
            suite['Tests'].append(test)
        test_suites.append(suite)
        self.test_suites = test_suites

    def hw_command(self, suite, test, algo):
        msg = ""

        if self.monte_flag == True or int(test['Len']) != 0:
            msg = test['Msg']
        e = "echo -n " + "\"" + msg + "\"" + " | xxd -r -p | "
        o = "openssl dgst "  + algo + " -engine devcrypto"
        command = e + o

        result = os.popen(command).read()
        tmp, result = result.split("=")
        return result.replace("\n", "").replace(" ", "")

    def sw_command(self, suite, test, algo):
        output_len = int(suite['L'])
        command = "kcapi-mtk-dgst" + " -n " + algo + " -m " + \
                test['Msg'] + " -l " + str(output_len)

        if self.monte_flag != True and int(test['Len']) == 0:
            command = command + " -e "
        result = os.popen(command).read()
        return result.replace("\n", "")


    def select_algo(self, suites):
        algo = "none"
        if suites['L'] == '20':
            algo = 'sha1'
        elif suites['L'] == '28':
            algo = 'sha224'
        elif suites['L'] == '32':
            algo = 'sha256'
        elif suites['L'] == '48':
            algo = 'sha384'
        elif suites['L'] == '64':
            algo = 'sha512'

        if self.engine == "hw":
            algo = "-" + algo
        return algo

    def get_expect(self, test):
        return test['MD']


    def run_monte_command(self, suite, algo):
        count = 0
        md1 = suite['Tests'][0]['Seed']
        md2 = suite['Tests'][0]['Seed']
        md3 = suite['Tests'][0]['Seed']
        result = suite['Tests'][0]['Seed']

        for test in suite['Tests']:
            if(count == 0):
                count = count + 1
                continue
            else:
                md1 = md3
                md2 = md3
                md3 = md3

            for i in range(1000):
                if self.engine == "sw":
                    test['Msg'] = md1 + md2 + md3
                    result = self.sw_command(suite, test, algo)
                elif self.engine == "hw":
                    test['Msg'] = md1 + md2 + md3
                    result = self.hw_command(suite, test, algo)

                md1 = md2
                md2 = md3
                md3 = result
            if result == test['MD']:
                count = count + 1
            else:
                return False
        return True
