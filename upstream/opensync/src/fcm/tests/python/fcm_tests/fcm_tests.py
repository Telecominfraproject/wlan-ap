#!/usr/bin/env python3

import argparse
import configparser
import copy
import logging
import os
import stat
import sys
import lib.fcm_collect as fcm_collect
import lib.fcm_report as fcm_report

log_format = '%(asctime)s - %(name)s - %(levelname)s - %(message)s'
logger = logging.getLogger(__file__)

def generate_script(cmds, file_path):
    with open(file_path, 'w') as f:
        f.write("#!/bin/sh\n")
        for cmd in cmds:
            f.write("{}\n".format(cmd.strip()))
    os.chmod(file_path, stat.S_IXUSR | stat.S_IRUSR | stat.S_IWUSR)
    logger.info("Created file {}".format(file_path))

def doit(config, args):
    fcm_collector = fcm_collect.FcmConfEntry(config)
    fcm_reporter = fcm_report.FcmConfEntry(config)

    insert_cmds = fcm_collector.get_insert_cmds()
    insert_cmds += fcm_reporter.get_insert_cmds()
    generate_script(insert_cmds, args.plugin_insert)

    delete_cmds = fcm_collector.get_delete_cmds()
    delete_cmds += fcm_reporter.get_delete_cmds()
    generate_script(delete_cmds, args.plugin_delete)

def main():
    logger.setLevel(logging.DEBUG)
    parser = argparse.ArgumentParser()
    parser.add_argument("--log-level", dest="loglevel",
                        choices=['INFO', 'DEBUG'],
                        default='INFO')
    parser.add_argument("--set-script", dest="plugin_insert", type=str,
                        default="create_fcm_plugin.sh",
                        help="where to save the plugin insert generated script")
    parser.add_argument("--del-script", dest="plugin_delete", type=str,
                        default="delete_fcm_plugin.sh",
                        help="where to save the plugin insert command")
    parser.add_argument("--conf-file", dest="conf_file", type=str,
                        default="one_plugin.conf",
                        help="configuaration file")

    args = parser.parse_args()
    # Set log level
    if args.loglevel == 'INFO':
        loglvl = logging.INFO
    else:
        loglvl = logging.DEBUG
    logging.basicConfig(level=loglvl, format=log_format)

    config = configparser.ConfigParser()
    with open(args.conf_file, 'r') as f:
        config.read_file(f)

    doit(config, args)

if __name__ == "__main__":
    sys.exit(main())
