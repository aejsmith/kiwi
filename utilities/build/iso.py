#
# Copyright (C) 2011 Alex Smith
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

from SCons.Script import *

# Function to generate an ISO image.
def iso_image_func(target, source, env):
	import os, sys, tempfile, shutil
	config = env['CONFIG']

	cdboot = str(env['CDBOOT'])
	loader = str(env['LOADER'])
	kernel = str(env['KERNEL'])
	fsimage = str(env['FSIMAGE'])

	# Create the work directory.
	tmpdir = tempfile.mkdtemp('.kiwiiso')
	os.makedirs(os.path.join(tmpdir, 'boot'))
	os.makedirs(os.path.join(tmpdir, 'kiwi', 'modules'))

	# Copy stuff into it.
	shutil.copy(kernel, os.path.join(tmpdir, 'kiwi'))
	shutil.copy(fsimage, os.path.join(tmpdir, 'kiwi', 'modules'))
	for mod in env['MODULES']:
		shutil.copy(str(mod), os.path.join(tmpdir, 'kiwi', 'modules'))

	# Write the configuration.
	f = open(os.path.join(tmpdir, 'boot', 'loader.cfg'), 'w')
	f.write('set "timeout" 5\n')
	f.write('entry "Kiwi" {\n')
	if len(config['FORCE_VIDEO_MODE']) > 0:
		f.write('	set "video_mode" "%s"\n' % (config['FORCE_VIDEO_MODE']))
	f.write('	kboot "/kiwi/kernel" "/kiwi/modules"\n')
	f.write('}\n')
	f.close()

	# Create the loader by concatenating the CD boot sector and the loader
	# together.
	f = open(os.path.join(tmpdir, 'boot', 'cdboot.img'), 'w')
	f.write(open(cdboot, 'r').read())
	f.write(open(loader, 'r').read())
	f.close()

	# Create the ISO.
	verbose = (ARGUMENTS.get('V') == '1') and '' or '>> /dev/null 2>&1'
	if os.system('mkisofs -J -R -l -b boot/cdboot.img -V "Kiwi CDROM" ' + \
	             '-boot-load-size 4 -boot-info-table -no-emul-boot ' + \
	             '-o %s %s %s' % (target[0], tmpdir, verbose)) != 0:
		print "Could not find mkisofs! Please ensure that it is installed."
		shutil.rmtree(tmpdir)
		return 1

	# Clean up.
	shutil.rmtree(tmpdir)
	return 0
def iso_image_emitter(target, source, env):
	return (target, source + [env['KERNEL'], env['LOADER'], env['CDBOOT'], env['FSIMAGE']] + env['MODULES'])
ISOBuilder = Builder(action = Action(iso_image_func, '$GENCOMSTR'), emitter = iso_image_emitter)
