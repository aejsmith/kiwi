Kiwi
====

Kiwi is an open source operating system. It uses a custom kernel design/API,
taking inspiration from both POSIX/UNIX and Windows NT. Some POSIX
compatibility is implemented by userspace libraries on top of the native
kernel API, which is currently capable of running some UNIX command line
software such as Bash.

![Terminal](documentation/screenshots/1.png)

More screenshots in the [Screenshot Archive](documentation/screenshots.md).

Supported platforms:

 * 64-bit x86 PCs
 * ARM64 (work-in-progress)
     * QEMU 'virt' machine
     * Raspberry Pi 3/4/5

Features:

 * Custom object-oriented kernel API
     * Processes/threads
     * Virtual memory
     * IPC
     * Event polling/waiting
     * Filesystem
     * Device access
 * Partial POSIX compatibility implemented over the native API
 * Multi-core CPU support
 * Shared library support
 * Networking (IPv4, TCP, UDP, DHCP, DNS)

Planned features (in rough priority order):

 * Common hardware support (disk devices, USB input)
 * Finish ARM64 port
 * Port additional software
 * GUI

Building
--------

To build Kiwi you need the following prerequisites:

 * SCons 4.x
 * ncurses
 * xorriso
 * mtools
 * dosfstools
 * e2fsprogs

Kiwi makes use of git submodules to include various pieces of 3rd party
software it uses. These must be cloned after this repository has been cloned
by running:

    $ git submodule update --init

You must then create a build configuration file by running:

    $ scons config

In that menu you will at least need to change the "Toolchain directory" option
under "Build options", to point to a location in which to install the Kiwi
cross-compiler. This location must be writable by your user. You may also wish
to change the toolchain build jobs option to e.g. the number of CPU cores in
your system to speed up the toolchain build.

After saving the configuration file, build the toolchain (only needs to be done
once) by running:

    $ scons toolchain

Finally, build the system with:

    $ scons

Running
-------

Once the system is built, you can run in QEMU with:

    $ scons qemu

The `qemu` target runs with a persistent disk image that is incrementally
updated with changes to the built system, and preserves other user data in the
image.

You can also build disk images with:

    # Bootable ISO image (BIOS + EFI).
    $ scons images/amd64/cdrom.iso
    
    # Bootable disk image (BIOS + EFI).
    $ scons images/amd64/disk.img

License
-------

Kiwi is licensed under the terms of the [ISC license](documentation/licenses/isc.txt).
