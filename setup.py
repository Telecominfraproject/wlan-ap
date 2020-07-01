from pathlib import Path
from subprocess import run
import os
import sys
import yaml
import getopt

git_am = "am"

def clone_tree():
	try:
		makefile = openwrt +"/Makefile"
		if Path(makefile).is_file():
			print("### OpenWrt checkout is already present. Please run --rebase")
			sys.exit(-1)

		print("### Cloning tree")
		Path(openwrt).mkdir(exist_ok=True, parents=True)
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
			run(["git", git_am, "-3", str(base_dir / patch)], check=True)

		print("### Patches done")
	except:
		print("### Setting up the tree failed")
		sys.exit(1)
	finally:
		os.chdir(base_dir)


base_dir = Path.cwd().absolute()
setup = False
rebase = False
config = "config.yml"
openwrt = "openwrt"

try:
	opts, args = getopt.getopt(sys.argv[1:], "srdc:f:", ["setup", "rebase", "docker", "config=", "folder="])
except getopt.GetoptError as err:
	print(err)
	sys.exit(2)


for o, a in opts:
	if o in ("-s", "--setup"):
		setup = True
	elif o in ("-r", "--rebase"):
		rebase = True
	elif o in ("-c", "--config"):
		config = a
	elif o in ("-d", "--docker"):
		git_am = "apply"
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
