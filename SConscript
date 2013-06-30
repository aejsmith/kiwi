#
# Copyright (C) 2009-2011 Alex Smith
#
# Permission to use, copy, modify, and/or distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
# ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
# ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
# OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
#

Import('config', 'envmgr')
import tarfile, glob, os, tempfile, shutil, sys

# Generate the configuration header. We don't generate with Kconfig because its
# too much of a pain to get SCons to do it properly.
f = open('config.h', 'w')
f.write('/* This file is automatically-generated, do not edit. */\n\n')
for (k, v) in config.items():
	if isinstance(v, str):
		f.write("#define CONFIG_%s \"%s\"\n" % (k, v))
	elif isinstance(v, bool) or isinstance(v, int):
		f.write("#define CONFIG_%s %d\n" % (k, int(v)))
	else:
		raise Exception, "Unsupported type %s in config" % (type(v))
f.close()

# Create the distribution environment.
dist = envmgr.Create('dist', {
	'DATA': {},
	'LINKS': {},
	'MODULES': [],
	'LIBRARIES': [],
	'BINARIES': [],
	'SERVICES': [],
})

# Visit subdirectories.
SConscript(dirs = ['source'])

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
	os.system('cp -R ' + os.path.join(str(Dir('#/data')), '*') + ' ' + tmpdir)
	for (source, dest) in env['LINKS'].items():
		if source[0] == '/':
			source = source[1:]
		os.symlink(dest, os.path.join(tmpdir, source))

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
data = []
for (k, v) in dist['DATA'].items():
	data += v
target = dist.Command(
	'fsimage.tar',
	dist['MODULES'] + dist['LIBRARIES'] + dist['SERVICES'] + dist['BINARIES'] + data,
	Action(fs_image_func, '$GENCOMSTR')
)
dist['FSIMAGE'] = File('fsimage.tar')

# Always build the filesystem image to make sure stuff is copied into it.
AlwaysBuild(target)

# Add aliases and set the default target.
Alias('loader', dist['LOADER'])
Alias('kernel', dist['KERNEL'])
Alias('modules', dist['MODULES'])
Alias('libraries', dist['LIBRARIES'])
Alias('services', dist['SERVICES'])
Alias('binaries', dist['BINARIES'])
Alias('fsimage', dist['FSIMAGE'])

# Add platform-specific targets to generate bootable images.
if config['PLATFORM'] == 'pc':
	from iso import ISOBuilder
	dist['BUILDERS']['ISOImage'] = ISOBuilder

	Default(Alias('cdrom', dist.ISOImage('cdrom.iso', [])))

	# Target to run in QEMU.
	Alias('qemu', dist.Command('qemu', ['cdrom.iso'], Action(
		config['QEMU_BINARY'] + ' -cdrom $SOURCE -boot d ' + config['QEMU_OPTS'],
		None
	)))
