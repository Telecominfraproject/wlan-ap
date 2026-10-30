#
# Copyright (C) 2023 Mediatek Ltd.
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

from algo import ALGO
from algo import parse_entry
import os

class GCM(ALGO):
    def __init__(self, engine, dir_path):
        super().__init__(engine, dir_path)
        self.type_len = {}

    def parse(self, rsp_file):
        mode = ""
        t = ""
        test_suites = []
        suite = {}
        test = {}
        algo =""
        mode = ""

        with open(rsp_file, 'r') as f:
            while True:
                line = f.readline()
                if line == '':
                    break

                if line == '\n' or line == '\r\n':
                    continue

                if line.startswith('#'):
                    if line.find('GCM') != -1:
                        algo = "gcm"
                    if line.find("Decrypt") != -1:
                        mode = "-d"
                    if line.find("Encrypt") != -1:
                        mode = "-e"
                    continue


                line = line.strip()

                if line.startswith('['):
                    e = line[1:-1]
                    if suite == {}:
                        suite = {'Algorithm': algo, 'Tests': [], 'mode':mode}
                    elif 'Tests' in suite and suite['Tests'] != []:
                        if mode == "-d" and 'Result' not in test:
                            test['Result'] = "PASS"
                        suite['Tests'].append(test)
                        test = {}
                        test_suites.append(suite)
                        suite = {'Algorithm': algo, 'Tests': [], 'mode':mode}
                    key, val = parse_entry(e)
                    suite[key] = val
                    continue

                if line.startswith('Count'):
                    if test != {}:
                        if mode == "-d" and 'Result' not in test:
                            test['Result'] = "PASS"
                        suite['Tests'].append(test)
                    test = {}
                    continue
                elif line.find('FAIL') != -1:
                    test['Result'] = 'FAIL'
                    test['PT'] = ""
                    continue
                key, val = parse_entry(line)
                if key in test:
                    key = key + '2'
                test[key] = val
            if mode == "-d" and 'Result' not in test:
                test['Result'] = "PASS"
            suite['Tests'].append(test)
        test_suites.append(suite)
        self.test_suites = test_suites


    def hw_de_command(self, suite, test, algo):
        tag = ""
        payload = ""
        add = ""
        if suite['AADlen'] != "0":
            add = " -a " + test['AAD']
        if suite['Taglen'] != "0":
            tag = " -t " + test['Tag']
        if suite['PTlen'] != "0":
            payload = " -c " + test['CT']

        command = "openssl-fips-ext " + suite['mode'] + " -k " + test['Key'] + \
                " -i " + test['IV'] + payload + add + tag + " " + algo
        result = os.popen(command).read()
        if 'Fail' in result:
            return ["FAIL", ""]
        elif 'Pass' in result and suite['PTlen'] == "0":
            return ["PASS", ""]
        elif 'Pass' in result:
            p = result.split("\n")[0].split(":")[1].replace(" ", "")
            return ["PASS", p]

    def hw_en_command(self, suite, test, algo):
        tag = ""
        payload = ""
        add = ""

        if suite['AADlen'] != "0":
            add = " -a " + test['AAD']
        if suite['Taglen'] != "0":
            tag = " -g " + str(int(int(suite['Taglen'])/8))
        if suite['PTlen'] != "0":
            payload = " -p " + test['PT']

        command = "openssl-fips-ext " + suite['mode'] + " -k " + test['Key'] + \
                " -i " + test['IV'] + payload + add + tag + " " + algo
        result = os.popen(command).read()

        tmp = result.split("\n")
        cipher = tmp[0].split(":")[1].replace(" ", "")
        tag = tmp[1].split(":")[1].replace(" ", "")
        if(cipher == "none"):
            return ["", tag]
        else:
            return [cipher, tag]


    def hw_command(self, suite, test, algo):
        ret = ""
        if suite['mode'] == "-d":
            ret = self.hw_de_command(suite, test, algo)
        elif suite['mode'] == "-e":
            ret = self.hw_en_command(suite, test, algo)

        return ret

    def select_algo(self, suites):
        return suites['Algorithm']

    def get_expect(self, test):
        if 'Result' in test:
            if test['Result'] == 'FAIL;':
                return [test['Result'], ""]
            else:
                return [test['Result'], test['PT']]
        else:
            return [test['CT'], test['Tag']]
