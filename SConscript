# Copyright (C) 2009-2010 Alex Smith
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

Import('config', 'envmgr')
import tarfile, glob, os, tempfile, shutil, sys

# Generate the configuration header. This is a little bit of a hack to get
# the header updated no matter what.
target = envmgr['host'].ConfigHeader('config.h', ['../../Kconfig'])
AlwaysBuild(target)
BUILD_TARGETS.insert(0, target[0])

# Create the distribution environment.
dist = envmgr.Create('dist', {
	'DATA': {},
	'MODULES': [],
	'LIBRARIES': [],
	'BINARIES': [],
	'SERVICES': [],
})

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
	for bin in env['BINARIES']:
		shutil.copy(str(bin), os.path.join(tmpdir, 'system', 'binaries'))
	for svc in env['SERVICES']:
		shutil.copy(str(svc), os.path.join(tmpdir, 'system', 'services'))
	for lib in env['LIBRARIES']:
		shutil.copy(str(lib), os.path.join(tmpdir, 'system', 'libraries'))
	for mod in env['MODULES']:
		shutil.copy(str(mod), os.path.join(tmpdir, 'system', 'modules'))
	for (app, files) in env['DATA'].items():
		os.makedirs(os.path.join(tmpdir, 'system', 'data', app))
		for f in files:
			shutil.copy(str(f), os.path.join(tmpdir, 'system', 'data', app))

	# Copy extras.
	if len(config['EXTRA_FSIMAGE']) > 0:
		os.system('cp -R ' + os.path.join(config['EXTRA_FSIMAGE'], '*') + ' ' + tmpdir)

	# Create the TAR file.
	tar = tarfile.open(str(target[0]), 'w')
	cwd = os.getcwd()
	os.chdir(tmpdir)
	for f in glob.glob('*'):
		tar.add(f)
	os.chdir(cwd)
	tar.close()

	# Clean up.
	shutil.rmtree(tmpdir)
dist.Command(
	'fsimage.tar',
	dist['MODULES'] + dist['LIBRARIES'] + dist['BINARIES'] + dist['SERVICES'],
	Action(fs_image_func, '$GENCOMSTR')
)
dist['FSIMAGE'] = File('fsimage.tar')

# Add aliases and set the default target.
Alias('loader', dist['LOADER'])
Alias('kernel', dist['KERNEL'])
Alias('modules', dist['MODULES'])
Alias('libraries', dist['LIBRARIES'])
Alias('binaries', dist['BINARIES'])
Alias('services', dist['SERVICES'])
Alias('fsimage', dist['FSIMAGE'])
Default(Alias('cdrom', dist.ISOImage('cdrom.iso', [])))

# Target to run in QEMU.
Alias('qtest', dist.Command('qtest', ['cdrom.iso'], Action(
	config['QEMU_BINARY'] + ' -cdrom $SOURCE -boot d ' + config['QEMU_OPTS'],
	None
)))
