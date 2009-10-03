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

import tarfile, glob, os, tempfile, shutil, sys

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
dist['DATA'] = {}
dist['LIBRARIES'] = []
dist['BINARIES'] = []
dist['SERVICES'] = []

# Visit subdirectories.
SConscript(dirs=['source'])

# Create a TAR archive containing the filesystem tree.
def fs_image_func(target, source, env):
	# Create the work directory.
	tmpdir = tempfile.mkdtemp('.kiwifsimage')
	os.makedirs(os.path.join(tmpdir, 'system', 'binaries'))
	os.makedirs(os.path.join(tmpdir, 'system', 'data'))
	os.makedirs(os.path.join(tmpdir, 'system', 'libraries'))
	os.makedirs(os.path.join(tmpdir, 'system', 'modules'))
	os.makedirs(os.path.join(tmpdir, 'system', 'services'))

	# Copy everything needed into it.
	shutil.copy(str(env['KERNEL']), os.path.join(tmpdir, 'system'))
	for bin in env['BINARIES']:
		shutil.copy(str(bin), os.path.join(tmpdir, 'system', 'binaries'))
	for svc in env['SERVICES']:
		shutil.copy(str(svc), os.path.join(tmpdir, 'system', 'services'))
	for lib in env['LIBRARIES']:
		shutil.copy(str(lib), os.path.join(tmpdir, 'system', 'libraries'))
	for mod in env['MODULES']:
		shutil.copy(str(mod), os.path.join(tmpdir, 'system', 'modules'))
	for app, files in env['DATA'].items():
		os.makedirs(os.path.join(tmpdir, 'system', 'data', app))
		for f in files:
			shutil.copy(str(f), os.path.join(tmpdir, 'system', 'data', app))

	# Create the TAR file.
	tar = tarfile.open(str(target[0]), 'w:gz')
	cwd = os.getcwd()
	os.chdir(tmpdir)
	for f in glob.glob('*'):
		tar.add(f)
	os.chdir(cwd)
	tar.close()

	# Clean up.
	shutil.rmtree(tmpdir)
dist['BUILDERS']['FSImage'] = Builder(action=Action(fs_image_func, '$GENCOMSTR'))
dist.FSImage('fsimage.tar.gz', [dist['KERNEL']] + dist['MODULES'] + dist['LIBRARIES'] + dist['BINARIES'] + dist['SERVICES'])
dist['FSIMAGE'] = File('fsimage.tar.gz')

# Set build defaults.
Default(Alias('kernel', dist['KERNEL']))
Default(Alias('modules', dist['MODULES']))
Default(Alias('libraries', dist['LIBRARIES']))
Default(Alias('binaries', dist['BINARIES']))
Default(Alias('services', dist['SERVICES']))
Default(Alias('fsimage', dist['FSIMAGE']))
Default(Alias('cdrom', dist.ISOImage('cdrom.iso', [dist['KERNEL'], dist['FSIMAGE']])))

# Create the ISO/HD images. Only build HD image on Linux for now.
if os.uname()[0] == 'Linux':
	#Default(Alias('hd', dist.HDImage('hd.img', dist['FSIMAGE'])))
	Alias('hd', dist.HDImage('hd.img', dist['FSIMAGE']))
	#Alias('qtest', dist.Command('qtest', ['hd.img'],
	#      Action(config['QEMU_BINARY'] + ' -hda $SOURCE -boot c ' + config['QEMU_OPTS'], None)))
#else:
Alias('qtest', dist.Command('qtest', ['cdrom.iso'],
      Action(config['QEMU_BINARY'] + ' -cdrom $SOURCE -boot d ' + config['QEMU_OPTS'], None)))
