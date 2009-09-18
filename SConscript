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
f = open('config.h', 'w')
f.write('/* This file is automatically-generated, do not edit. */\n\n')
for (k, v) in config.items():
	if isinstance(v, str):
		f.write("#define CONFIG_%s \"%s\"\n" % (k, v))
	elif isinstance(v, bool) or isinstance(v, int):
		f.write("#define CONFIG_%s %d\n" % (k, int(v)))
	else:
		raise Exception, "Unsupported type %s in build.conf" % (type(v))
f.close()

# Create the distribution environment.
dist = envmgr.Create('dist')
dist['BOOTDATA'] = {}

# Visit subdirectories.
SConscript(dirs=['source'])

# Set build defaults.
Default(Alias('kernel', dist['KERNEL']))
Default(Alias('modules', envmgr['module']['MODULES']))
Default(Alias('libraries', envmgr['uspace']['LIBRARIES']))
Default(Alias('binaries', envmgr['uspace']['BINS']))
Default(Alias('bootimg', dist['BOOTIMG']))

# Create the ISO image.
Default(Alias('cdrom', dist.ISOImage('cdrom.iso', [dist['KERNEL'], dist['BOOTIMG']])))

# Run generated ISO image in QEMU.
dist.Alias('qtest', dist.Command('qtest', ['cdrom.iso'],
           Action(config['QEMU_BINARY'] + ' -cdrom $SOURCE -boot d ' + config['QEMU_OPTS'], None)))
