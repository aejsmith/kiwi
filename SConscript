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

Import('config', 'envmgr')

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

# Create the distribution environment.
dist = envmgr.Create('dist')

# Visit subdirectories.
SConscript(dirs=['source'])

# Create the ISO image.
iso = Alias('cdrom', dist.ISOImage('cdrom.iso', [dist['KERNEL']]))
Default(iso)

# Run generated ISO image in QEMU.
dist.Alias('qtest', dist.Command('qtest', ['cdrom.iso'],
           Action(config['QEMU_BINARY'] + ' -cdrom $SOURCE -serial stdio ' + \
                  config['QEMU_OPTS'], None)))
