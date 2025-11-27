#!/usr/bin/env python3

from pathlib import Path
from subprocess import run
import os
import sys
import yaml
import getopt

def clone_tree():
	try:
		makefile = openwrt +"/Makefile"
		if Path(makefile).is_file():
			print("### OpenWrt checkout is already present. Please run --rebase")
			sys.exit(-1)

		print("### Cloning tree")
		Path(openwrt).mkdir(exist_ok=True, parents=True)
		if git_ref != "":
			run(["git", "clone", "--reference", git_ref, config["repo"], openwrt], check=True)
		else:
			run(["git", "clone", config["repo"], openwrt], check=True)
		print("### Clone done")
	except:
		print("### Cloning the tree failed")
		sys.exit(1)

def fetch_tree():
	try:
		makefile = openwrt +"/Makefile"
		if not Path(makefile).is_file():
			print("### OpenWrt checkout is not present. Please run --setup")
			sys.exit(-1)

		print("### Fetch tree")
		os.chdir(openwrt)
		run(["git", "fetch"], check=True)
		print("### Fetch done")
	except:
		print("### Fetching the tree failed")
		sys.exit(1)
	finally:
		os.chdir(base_dir)

def reset_tree():
	try:
		print("### Resetting tree")
		os.chdir(openwrt)
		run(
			["git", "checkout", config["branch"]], check=True,
		)
		run(
			["git", "reset", "--hard", config.get("revision", config["branch"])],
			check=True,
		)
		run(
			["rm", "-r", "profiles"],
		)
		print("### Reset done")
	except:
		print("### Resetting tree failed")
		sys.exit(1)
	finally:
		os.chdir(base_dir)

def setup_tree():
	try:
		print("### Applying patches")

		patches = []
		for folder in config.get("patch_folders", []):
			patch_folder = base_dir / folder
			if not patch_folder.is_dir():
				print(f"Patch folder {patch_folder} not found")
				sys.exit(-1)

			print(f"Adding patches from {patch_folder}")

			patches.extend(
				sorted(list((base_dir / folder).glob("*.patch")), key=os.path.basename)
		)

		print(f"Found {len(patches)} patches")

		os.chdir(openwrt)

		for patch in patches:
			run(["git", "am", "-3", str(base_dir / patch)], check=True)
		run(
			["ln", "-rs", profiles, "profiles"], check=True,
		)
		print("### Patches done")
	except:
		print("### Setting up the tree failed")
		sys.exit(1)
	finally:
		os.chdir(base_dir)

def update_patches():
	try:
		print("### Updating patches")
		run(
			["rm", "-r", "patches"], check=True,
		)
		os.chdir(openwrt)
		run(
			["git", "format-patch", config.get("revision", config["branch"]), "-o", base_dir / "patches"],
			check=True,
		)
		print("### Updating done")
	except:
		print("### updating failed failed")
		sys.exit(1)
	finally:
		os.chdir(base_dir)


base_dir = Path.cwd().absolute()
setup = False
update = False
rebase = False
config = "config.yml"
profiles = base_dir / "profiles"
openwrt = "openwrt"
git_ref = ""

try:
	opts, args = getopt.getopt(sys.argv[1:], "srd:c:f:u2", ["setup", "rebase", "config=", "folder=", "reference=", "update", "20x", "directory=" ])
except getopt.GetoptError as err:
	print(err)
	sys.exit(2)


for o, a in opts:
	if o in ("-s", "--setup"):
		setup = True
	elif o in ("-r", "--rebase"):
		rebase = True
	elif o in ("-u", "--update"):
		update = True
	elif o in ("-c", "--config"):
		config = a
	elif o in ("--reference"):
		git_ref = a
	elif o in ("-d", "--directory"):
		openwrt = a
	else:
		assert False, "unhandled option"

if not Path(config).is_file():
	print(f"Missing {config}")
	sys.exit(1)
config = yaml.safe_load(open(config))

if setup:
	clone_tree()
	reset_tree()
	setup_tree()
elif rebase:
	fetch_tree()
	reset_tree()
	setup_tree()
elif update:
	update_patches()
