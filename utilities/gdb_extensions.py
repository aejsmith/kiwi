#
# Copyright (C) 2009-2022 Alex Smith
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

from importlib.resources import path
import os
import sys
sys.path = [os.path.abspath(os.path.join('utilities', 'build'))] + sys.path

import gdb
import json
from kconfig import ConfigParser
config = ConfigParser('.config')

if not config.configured():
    raise gdb.GdbError('Build is not configured. Kiwi extensions will be unavailable')

class KiwiManifest:
    def __init__(self):
        self.manifest = None

    def load(self, path):
        with open(path, 'r') as f:
            self.manifest = json.loads(f.read())

    # Given a target filesystem path, get the path to the file in the build
    # tree. Returns None if not found or is not a file.
    def get_source_path(self, path):
        if not self.manifest:
            raise gdb.GdbError('Manifest has not been loaded')

        path = os.path.normpath(path).lstrip('/')
        if path in self.manifest:
            entry = self.manifest[path]
            if entry['type'] == 'File':
                return entry['source']

        return None

kiwi_manifest = KiwiManifest()

ELF_SHT_PROGBITS = 1
ELF_SHT_NOBITS = 8

ELF_SHF_ALLOC = 0x2

class KiwiImage:
    name        = ''
    path        = ''
    load_base   = 0
    ehdr        = None
    shdrs       = None

    def __init__(self, elf_image):
        self.name      = elf_image['name'].string() if int(elf_image['name']) != 0 else ''
        self.path      = elf_image['path'].string() if int(elf_image['path']) != 0 else ''
        self.load_base = int(elf_image['load_base'])
        self.ehdr      = elf_image['ehdr']
        self.shdrs     = elf_image['shdrs']

    # Look up the source file.
    def find_source_path(self, search_path = ''):
        path = self.path
        if not path:
            path = os.path.join(search_path, self.name)
        return kiwi_manifest.get_source_path(path)

    # Get a map of sections to their load address.
    def get_sections(self, types):
        e_shnum    = int(self.ehdr['e_shnum'])
        e_shstrndx = int(self.ehdr['e_shstrndx'])
        shstrtab   = self.shdrs[e_shstrndx]['sh_addr'].cast(gdb.lookup_type('char').pointer())

        sections = {}

        for i in range(0, e_shnum):
            shdr     = self.shdrs[i]
            sh_name  = shdr['sh_name']
            sh_type  = shdr['sh_type']
            sh_flags = shdr['sh_flags']

            if sh_flags & ELF_SHF_ALLOC and sh_type in types and int(sh_name) != 0:
                name = shstrtab[sh_name].address.string()
                if name:
                    sections[name] = int(shdr['sh_addr'])

        return sections

class KiwiTarget:
    def get_curr_proc(self):
        thread = self.get_curr_thread()
        return thread.dereference()['owner']

    def get_kernel_proc(self):
        frame = gdb.newest_frame()
        return frame.read_var('kernel_proc')

    # Get a list of loaded images for a process.
    def get_images(self, proc):
        images = []

        head = proc.dereference()['images'].address
        iter = head.dereference()['next']
        while iter != head:
            elf_image = iter.cast(gdb.lookup_type('elf_image_t').pointer()).dereference()

            image = KiwiImage(elf_image)
            images.append(image)

            iter = iter.dereference()['next']

        return images

    # Load symbol files for all kernel modules.
    def load_kernel_modules(self):
        images = self.get_images(self.get_kernel_proc())
        for image in images:
            # Ignore the kernel.
            if image.name == 'kernel':
                continue

            # Try to find the file. Kernel doesn't track paths for modules so
            # look up in the modules directory.
            source_path = image.find_source_path('system/kernel/modules')
            if not source_path:
                print("Could not find file for module '%s'!" % (image.name))
                continue

            # Use the unstripped version from the build.
            source_path += '-unstripped'

            # Get loadable sections.
            sections = image.get_sections([ELF_SHT_NOBITS, ELF_SHT_PROGBITS])
            if not '.text' in sections:
                print("Could not find text address for module '%s'!" % (image.name))
                continue

            text_addr = sections['.text']

            cmd = 'add-symbol-file %s 0x%x' % (source_path, text_addr)
            for (name, addr) in sections.items():
                if name != '.text':
                    cmd += ' -s %s 0x%x' % (name, addr)

            gdb.execute(cmd)

class KiwiAMD64Target(KiwiTarget):
    gdb_arch = 'i386:x86-64:intel'

    def get_curr_arch_thread(self):
        frame = gdb.newest_frame()
        register = frame.read_register('gs_base')
        return register.cast(gdb.lookup_type('arch_thread_t').pointer())

    def get_curr_thread(self):
        arch_thread = self.get_curr_arch_thread()
        return arch_thread.dereference()['parent']

    def get_curr_cpu(self):
        arch_thread = self.get_curr_arch_thread()
        return arch_thread.dereference()['cpu']

class KiwiARM64Target(KiwiTarget):
    gdb_arch = 'aarch64'

    def get_curr_arch_thread():
        raise gdb.GdbError('get_curr_arch_thread() unimplemented')

    def get_curr_thread(self):
        raise gdb.GdbError('get_curr_thread() unimplemented')

    def get_curr_cpu(self):
        raise gdb.GdbError('get_curr_cpu() unimplemented')

targets = {
    'amd64': KiwiAMD64Target(),
    'arm64': KiwiARM64Target(),
}

try:
    kiwi_target = targets[config['ARCH']]
except:
    raise gdb.GdbError("Unrecognised architecture '%s'" % (config['ARCH']))

class KiwiConnectCommand(gdb.Command):
    """Connects to a Kiwi target based on the current configuration."""

    def invoke(self, args, from_tty):
        argv = gdb.string_to_argv(args)
        if len(argv) != 2:
            raise gdb.GdbError('Arguments required (target, manifest)')

        kiwi_manifest.load(argv[1])

        kernel_path = os.path.join('build', '%s-%s' % (config['ARCH'], config['BUILD']), 'kernel', 'kernel-unstripped')

        gdb.execute('file "%s"' % (kernel_path))
        gdb.execute('set architecture %s' % (kiwi_target.gdb_arch))
        gdb.execute('target remote %s' % (argv[0]))

        kiwi_target.load_kernel_modules()

        gdb.execute('frame')

KiwiConnectCommand("kiwi-connect", gdb.COMMAND_USER)

class KiwiCurrArchThreadFunction(gdb.Function):
    """Return the current arch_thread_t."""

    def invoke(self):
        return kiwi_target.get_curr_arch_thread()

KiwiCurrArchThreadFunction('curr_arch_thread')

class KiwiCurrThreadFunction(gdb.Function):
    """Return the current thread_t."""

    def invoke(self):
        return kiwi_target.get_curr_thread()

KiwiCurrThreadFunction('curr_thread')

class KiwiCurrProcFunction(gdb.Function):
    """Return the current process_t."""

    def invoke(self):
        return kiwi_target.get_curr_proc()

KiwiCurrProcFunction('curr_proc')

class KiwiCurrCPUFunction(gdb.Function):
    """Return the current cpu_t."""

    def invoke(self):
        return kiwi_target.get_curr_cpu()

KiwiCurrCPUFunction('curr_cpu')
