#
# Copyright (C) 2023 Mediatek Ltd.
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

from algo import ALGO
from algo import parse_entry
import os

def ccm_select_mode(line):
    mode = ""
    t = ""
    if line.find('DVPT') != -1:
        mode = "-d"
        t = "DVTP"
    elif line.find("VADT") != -1:
        mode = "-e"
        t = "VADT"
    elif line.find("VNT") != -1:
        mode = "-e"
        t = "VNT"
    elif line.find("VPT") != -1:
        mode = "-e"
        t = "VPT"
    elif line.find("VTT" ) != -1:
        mode = "-e"
        t = "VTT"
    return mode, t

class CCM(ALGO):
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

        with open(rsp_file, 'r') as f:
            while True:
                line = f.readline()
                if line == '':
                    break

                if line == '\n' or line == '\r\n':
                    continue

                if line.startswith('#'):
                    if line.find('CCM') != -1:
                        algo = "ccm"

                    if algo == "ccm" and mode == "":
                        mode, t = ccm_select_mode(line)

                    continue


                line = line.strip()

                if line.startswith('['):
                    if test != {}:
                        suite['Tests'].append(test)
                    items= line[1:-1].split(",")
                    suite = {'Key': "", 'Tests': []}
                    for e in items:
                        key, val = parse_entry(e)
                        suite[key] = val
                    suite['Algorithm'] = algo
                    suite['mode'] = mode
                    suite['type'] = t
                    suite.update(self.type_len)
                    test_suites.append(suite)
                    test = {}
                    continue

                if line.find("Key = ") != -1:
                    key, val = parse_entry(line)
                    suite[key] = val
                elif line.find("Plen = ") != -1 or line.find("Nlen = ") != -1 or \
                        line.find("Tlen = ") != -1 or line.find("Alen = ") != -1:
                    key, val = parse_entry(line)
                    self.type_len[key] = val
                elif line.find("Nonce") != -1 and \
                        (t == "VPT" or t == "VTT" or t =="VADT"):
                    key, val = parse_entry(line)
                    suite[key] = val
                elif line.startswith('Count'):
                    if test != {}:
                        suite['Tests'].append(test)
                    test = {}
                    continue
                else:
                    key, val = parse_entry(line)
                    if key in test:
                        key = key + '2'
                    test[key] = val
            suite['Tests'].append(test)
        self.type_len = {}
        self.test_suites = test_suites


    def hw_de_command(self, suite, test, algo):
        add = ""
        cipher = ""
        payload = ""
        ret = ""
        l = 0

        if suite['Alen'] != "0":
            add = " -f " + test['Adata']
        if suite['Plen'] != "0":
            l = len(test['CT']) - int(suite['Tlen']) * 2
            cipher = " -c " + test['CT'][0:l]
        command = "openssl-fips-ext " + suite['mode'] + ' -k ' + suite['Key'] + " -n " + test['Nonce'] + add + cipher + " -t " + test['CT'][l:len(test['CT'])] + " " + algo
        result = os.popen(command).read().replace(" ", "")

        tmp = result.split("\n")
        if "Pass" in tmp[0]:
            ret = "Pass"
            if suite['Plen'] != "0":
                payload = tmp[1].split(":")[1]
            else:
                payload = "00"
        else:
            ret = "Fail"
            payload = ""
        return [ret, payload]

    def hw_en_command(self, suite, test, algo):
        nonce = ""
        add = ""
        payload = ""
        tag = ""
        ret = ""

        if 'Nonce' in suite:
            nonce = suite['Nonce']
        else:
            nonce = test['Nonce']

        if suite['Alen'] != "0":
            add = " -f " + test['Adata']
        if suite['Plen'] != "0":
            payload = " -l " + test['Payload']
        if suite['Tlen'] != "0":
            tag = " -t " + suite['Tlen']

        command = "openssl-fips-ext " + suite['mode'] + ' -k ' + \
                suite['Key'] + " -n " + nonce + add + tag + payload +\
                " " + algo
        result = os.popen(command).read().replace(" ", "")
        tmp = result.split("\n")
        for i in tmp:
            if(i == ''):
                continue
            ret = ret + i.split(":")[1]
        return ret


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
            if test['Result'] == 'Fail':
                return [test['Result'], ""]
            else:
                return [test['Result'], test['Payload']]
        else:
            return test['CT']
