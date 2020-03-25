#!/usr/bin/env python3

import argparse
import configparser
import copy
import logging
import os
import stat
import sys
import lib.fsm_config as fsm_config
import lib.tags as tag
import lib.iface as tap
import lib.flows as flow

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
    taps = tap.TapConfEntry(config)
    flows = flow.OpenflowConfEntry(config)
    tags = tag.TagsConfEntry(config)
    fsm_plugins = fsm_config.FsmConfEntry(config)

    insert_cmds = taps.get_insert_cmds()
    insert_cmds += flows.get_insert_cmds()
    insert_cmds += tags.get_insert_cmds()
    insert_cmds += fsm_plugins.get_insert_cmds()
    generate_script(insert_cmds, args.plugin_insert)

    delete_cmds = fsm_plugins.get_delete_cmds()
    delete_cmds += tags.get_delete_cmds()
    delete_cmds += flows.get_delete_cmds()
    delete_cmds += taps.get_delete_cmds()


    generate_script(delete_cmds, args.plugin_delete)

def main():
    logger.setLevel(logging.DEBUG)
    parser = argparse.ArgumentParser()
    parser.add_argument("--log-level", dest="loglevel",
                        choices=['INFO', 'DEBUG'],
                        default='INFO')
    parser.add_argument("--set-script", dest="plugin_insert", type=str,
                        default="create_fsm_plugin.sh",
                        help="where to save the plugin insert generated script")
    parser.add_argument("--del-script", dest="plugin_delete", type=str,
                        default="delete_fsm_plugin.sh",
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
