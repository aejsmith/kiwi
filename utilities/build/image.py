#
# Copyright (C) 2009-2021 Alex Smith
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
import io, tarfile, glob, os, tempfile, shutil, time

class TARArchive:
    def __init__(self, path):
        self.tar = tarfile.open(path, 'w')

    def finish(self):
        self.tar.close()

    def make_dir(self, name):
        if len(name) == 0:
            return
        try:
            self.tar.getmember(name)
        except KeyError:
            self.make_dir(os.path.dirname(name))

            tarinfo = tarfile.TarInfo(name)
            tarinfo.type  = tarfile.DIRTYPE
            tarinfo.mtime = int(time.time())
            tarinfo.mode  = 0o755
            tarinfo.uid   = 0
            tarinfo.gid   = 0
            tarinfo.uname = "root"
            tarinfo.gname = "root"

            self.tar.addfile(tarinfo)

    def make_file(self, name, data):
        self.make_dir(os.path.dirname(name))

        file = io.BytesIO(data.encode('utf-8'))

        tarinfo = tarfile.TarInfo(name)
        tarinfo.type     = tarfile.REGTYPE
        tarinfo.mtime    = int(time.time())
        tarinfo.mode     = 0o644
        tarinfo.uid      = 0
        tarinfo.gid      = 0
        tarinfo.uname    = "root"
        tarinfo.gname    = "root"
        tarinfo.size     = len(data)

        self.tar.addfile(tarinfo, file)

    def add_files(self, files):
        for (path, target) in files:
            while path[0] == '/':
                path = path[1:]

            self.make_dir(os.path.dirname(path))

            with open(str(target), 'rb') as file:
                tarinfo = self.tar.gettarinfo(None, path, file)
                tarinfo.uid   = 0
                tarinfo.gid   = 0
                tarinfo.uname = "root"
                tarinfo.gname = "root"

                self.tar.addfile(tarinfo, file)

    def add_links(self, links):
        for (path, target) in links:
            while path[0] == '/':
                path = path[1:]

            self.make_dir(os.path.dirname(path))

            tarinfo = tarfile.TarInfo(path)
            tarinfo.type     = tarfile.SYMTYPE
            tarinfo.linkname = target
            tarinfo.mtime    = int(time.time())
            tarinfo.mode     = 0o644
            tarinfo.uid      = 0
            tarinfo.gid      = 0
            tarinfo.uname    = "root"
            tarinfo.gname    = "root"

            self.tar.addfile(tarinfo)

    def add_dir_tree(self, path):
        cwd = os.getcwd()
        os.chdir(path)
        for f in glob.glob('*'):
            self.tar.add(f)
        os.chdir(cwd)

# Create a TAR archive containing the filesystem tree.
def fs_image_func(target, source, env):
    config = env['_CONFIG']

    tar = TARArchive(str(target[0]))

    tar.add_files(env['FILES'])
    tar.add_links(env['LINKS'])
    tar.add_dir_tree(str(Dir('#/data')))

    # Add in extra stuff from the directory specified in the configuration.
    if len(config['EXTRA_FSIMAGE']) > 0:
        tar.add_dir_tree(config['EXTRA_FSIMAGE'])

    tar.finish()
    return 0
def fs_image_emitter(target, source, env):
    # We must depend on every file that goes into the image.
    deps = [f for (p, f) in env['FILES']]
    return (target, source + deps)
fs_image_builder = Builder(action = Action(fs_image_func, '$GENCOMSTR'), emitter = fs_image_emitter)

# Create a boot image.
def boot_image_func(target, source, env):
    config = env['_CONFIG']

    tar = TARArchive(str(target[0]))

    files = [
        ('kernel', env['KERNEL']),
        ('modules/fsimage.tar', env['FSIMAGE']),
    ]

    for mod in env['MODULES']:
        files += [('modules/' + os.path.basename(str(mod)), mod)]

    tar.add_files(files)

    kboot_cfg = 'kboot "/kernel" "/modules"\n'
    tar.make_file('kboot.cfg', kboot_cfg)

    tar.finish()
    return 0
def boot_image_emitter(target, source, env):
    return (target, [env['KERNEL'], env['FSIMAGE']] + env['KBOOT'] + env['MODULES'])
boot_image_builder = Builder(action = Action(boot_image_func, '$GENCOMSTR'), emitter = boot_image_emitter)

# Function to generate an ISO image.
def iso_image_func(target, source, env):
    config = env['_CONFIG']

    fsimage = str(env['FSIMAGE'])
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
        '%s --bin-dir=build/%s-%s-%s/boot/bin --targets="%s" --label="Kiwi CDROM" %s %s %s' %
            (env['KBOOT_MKISO'], config['ARCH'], config['PLATFORM'], config['BUILD'],
                config['KBOOT_TARGETS'], target[0], tmpdir, verbose))

    shutil.rmtree(tmpdir)
    return ret
def iso_image_emitter(target, source, env):
    return (target, [env['KERNEL'], env['FSIMAGE'], env['KBOOT_MKISO']] + env['KBOOT'] + env['MODULES'])
iso_image_builder = Builder(action = Action(iso_image_func, '$GENCOMSTR'), emitter = iso_image_emitter)
