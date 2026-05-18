#
# Copyright (C) 2023 Mediatek Ltd.
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

import os
import abc

def parse_entry(line):
    key, val = line.split('=')
    key = key.strip()
    val = val.strip()
    if val == 'True':
        val = True
    elif val == 'False':
        val = False
    return key, val

class ALGO(metaclass=abc.ABCMeta):
    def __init__(self, engine, dir_path):
        self.test_suite = {}
        self.engine = engine
        self.dir_path = dir_path
        self.file = []
        self.monte_flag = False

    @abc.abstractmethod
    def select_algo(self):
        print("get eepext no implement")
        return ""

    @abc.abstractmethod
    def get_expect(self, test):
        return NotImplemented

    @abc.abstractmethod
    def parse(self):
        print("no implement parse")
        return

    def sw_command(self, suite, test, algo):
        print("No sw command implement")
        exit()
        return

    def hw_command(self, suite, test, algo):
        print("No hw command implement")
        exit()
        return

    def run_test(self):
        self.traverse()
        for path in self.file:
            print(path)
            self.parse(path)
            if self.monte_flag == True:
                self.run_monte()
                self.monte_flag = False
            else:
                self.run_command()
            self.clear()

    def traverse(self):
        for filename in os.listdir(self.dir_path):
            path = os.path.join(self.dir_path, filename)
            self.file.append(path)

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
                else:
                    print("No this engine")
                    exit()

                expect = self.get_expect(test)

                if(result == expect):
                    pass_count = pass_count + 1
                    count = count + 1
            total = total + len(suites['Tests'])
        print("\t\t\ttotal case: %d, pass case: %d, rate: %f" %(total, pass_count, pass_count/total))

    def run_monte(self):
        algo = ""
        ret = False
        for suite in self.test_suites:
            algo = self.select_algo(suite)
            ret = self.run_monte_command(suite, algo)
            if ret == True:
                print("%s monte pass" % algo)
            else:
                print("%s monte fail" % algo)

    def clear(self):
        return
