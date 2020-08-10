#
# Copyright (C) 2009-2013 Alex Smith
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
import tarfile, glob, os, tempfile, shutil, time

# Create a TAR archive containing the filesystem tree.
def fs_image_func(target, source, env):
    config = env['_CONFIG']

    # Create the TAR file.
    tar = tarfile.open(str(target[0]), 'w')

    def make_dir(name):
        if len(name) == 0:
            return
        try:
            tar.getmember(name)
        except KeyError:
            make_dir(os.path.dirname(name))
            tarinfo = tarfile.TarInfo(name)
            tarinfo.type = tarfile.DIRTYPE
            tarinfo.mtime = int(time.time())
            tarinfo.mode = 0o755
            tarinfo.uid = 0
            tarinfo.gid = 0
            tarinfo.uname = "root"
            tarinfo.gname = "root"
            tar.addfile(tarinfo)

    # Copy everything into it.
    for (path, target) in env['FILES']:
        while path[0] == '/':
            path = path[1:]
        make_dir(os.path.dirname(path))
        with open(str(target), 'rb') as file:
            tarinfo = tar.gettarinfo(None, path, file)
            tarinfo.uid = 0
            tarinfo.gid = 0
            tarinfo.uname = "root"
            tarinfo.gname = "root"
            tar.addfile(tarinfo, file)
    for (path, target) in env['LINKS']:
        while path[0] == '/':
            path = path[1:]
        make_dir(os.path.dirname(path))
        tarinfo = tarfile.TarInfo(path)
        tarinfo.type = tarfile.SYMTYPE
        tarinfo.linkname = target
        tarinfo.mtime = int(time.time())
        tarinfo.mode = 0o644
        tarinfo.uid = 0
        tarinfo.gid = 0
        tarinfo.uname = "root"
        tarinfo.gname = "root"
        tar.addfile(tarinfo)

    # Add in extra stuff from the directory specified in the configuration.
    if len(config['EXTRA_FSIMAGE']) > 0:
        cwd = os.getcwd()
        os.chdir(config['EXTRA_FSIMAGE'])
        for f in glob.glob('*'):
            tar.add(f)
        os.chdir(cwd)

    tar.close()
    return 0
def fs_image_emitter(target, source, env):
    # We must depend on every file that goes into the image.
    deps = [f for (p, f) in env['FILES']]
    return (target, source + deps)
fs_image_builder = Builder(action = Action(fs_image_func, '$GENCOMSTR'), emitter = fs_image_emitter)

# Function to generate an ISO image.
def iso_image_func(target, source, env):
    config = env['_CONFIG']

    fsimage = str(source[-1])
    kernel = str(env['KERNEL'])

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
    f = open(os.path.join(tmpdir, 'boot', 'kboot.cfg'), 'w')
    f.write('set "timeout" 5\n')
    f.write('entry "Kiwi" {\n')
    if len(config['FORCE_VIDEO_MODE']) > 0:
        f.write('   set "video_mode" "%s"\n' % (config['FORCE_VIDEO_MODE']))
    f.write('   kboot "/kiwi/kernel" "/kiwi/modules"\n')
    f.write('}\n')
    f.close()

    # Create the ISO.
    verbose = '' if (ARGUMENTS.get('V') == '1') else '>> /dev/null 2>&1'
    config = env['_CONFIG']
    ret = os.system(
        '%s --bin-dir=build/%s-%s/boot/bin --targets="%s" --label="Kiwi CDROM" %s %s %s' %
            (env['KBOOT_MKISO'], config['ARCH'], config['PLATFORM'], config['KBOOT_TARGETS'],
                target[0], tmpdir, verbose))

    shutil.rmtree(tmpdir)
    return ret
def iso_image_emitter(target, source, env):
    assert len(source) == 1
    return (target, [env['KERNEL'], env['KBOOT_MKISO']] + env['KBOOT'] + env['MODULES'] + source)
iso_image_builder = Builder(action = Action(iso_image_func, '$GENCOMSTR'), emitter = iso_image_emitter)
