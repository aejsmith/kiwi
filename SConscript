# Kiwi build system
# Copyright (C) 2009 Alex Smith
#
# Kiwi is open source software, released under the terms of the Non-Profit
# Open Software License 3.0. You should have received a copy of the
# licensing information along with the source code distribution. If you
# have not received a copy of the license, please refer to the Kiwi
# project website.
#
# Please note that if you modify this file, the license requires you to
# ADD your name to the list of contributors. This boilerplate is not the
# license itself; please refer to the copy of the license you have received
# for complete terms.

import os
import sys

Import("config", "envmgr")

# Create the build configuration header.
with open('config.h', 'w') as f:
	f.write('/* This file is automatically-generated. Modify build.conf instead. */\n\n')
	for (k, v) in config.items():
		if isinstance(v, str):
			f.write("#define CONFIG_%s \"%s\"\n" % (k, v))
		elif isinstance(v, bool) or isinstance(v, int):
			f.write("#define CONFIG_%s %d\n" % (k, int(v)))
		else:
			raise Exception, "Unsupported type %s in build.conf" % (type(v))

"""subdirs = ["kernel", "drivers", "uspace"]

KEnv.HeaderFile("config.h", src_dict=Buildconf, prefix="CONFIG_")
KEnv.HeaderFile("build.h", src_dict=Buildinfo, ignore="FLAGS_")

# Include the subdirectory SConscript files
SConscript(dirs=subdirs)

# Generate the boot image
drivers = []
for driver in KEnv["DRIVERS"]:
	drivers += [str(driver)]
apps = []
for app in UEnv["APPS"]:
	apps += [str(app)]
KEnv.Alias("bootimg", KEnv.Command("bootimg-" + Buildconf["ARCH"] + ".tar.gz",
	["drivers", "apps"],
	Action("util/mkbootimg.sh " + Buildconf["ARCH"] + " $TARGET build/" + Buildconf["ARCH"] + "/bootimg "
	"\"%s\" \"%s\"" % (string.join(drivers, " "), string.join(apps, " ")),
	"  BOOTIMG $TARGET")))

# Generate a GRUB config file
def GrubConfFunc(target, source, env):
	file = open(str(target[0]), "w")
	file.write("default 0\n")
	file.write("timeout 10\n\n")
	file.write("title Exclaim - %s\n" % (Buildconf["ARCH"]))
	file.write("	kernel /exclaim.elf vfs.root=ramfs\n")
	file.write("	module /bootimg-" + Buildconf["ARCH"] + ".tar.gz\n")
	file.close()
	return None

KEnv.Command("menu.lst", ["kernel", "bootimg"],
	Action(GrubConfFunc, "  GRUB    $TARGET"))

# Generate an ISO image
KEnv.Alias("cdrom", KEnv.Command("cdrom.iso", ["menu.lst", "kernel", "bootimg"],
	Action("util/mkiso.sh " + Buildconf["ARCH"] + " $TARGET ",
	"  MKISO   $TARGET")))

# Run in QEMU
try: qemu = Buildconf["QEMU"]
except KeyError: qemu = "qemu"

try: qemu_opts = Buildconf["QEMU_OPTS"]
except KeyError: qemu_opts = ""

KEnv.Alias("qtest", KEnv.Command("qtest", ["cdrom.iso"],
	Action(qemu + " -cdrom $SOURCE -serial stdio " + qemu_opts,
	"  QEMU")))
KEnv.Alias("qtest-gdb", KEnv.Command("qtest-gdb", ["cdrom.iso"],
	Action(qemu + " -cdrom $SOURCE -serial stdio -s " + qemu_opts,
	"  QEMU")))"""
