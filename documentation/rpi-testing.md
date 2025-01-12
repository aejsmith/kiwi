Raspberry Pi Testing
====================

The best way to test Raspberry Pi during development is via network boot.

Follow https://www.raspberrypi.com/documentation/computers/raspberry-pi.html#network-booting
for details on how to set up network boot on the specific Raspberry Pi device.

You can use a DHCP proxy with dnsmasq on a network with an existing DHCP server.
Add the following options to `dnsmasq.conf`:

    port=0
    dhcp-range=192.168.0.255,proxy
    log-dhcp
    enable-tftp
    tftp-root=/tftpboot
    pxe-service=0,"Raspberry Pi Boot"

Change the address in `dhcp-range` to the broadcast address for your network.
Make sure to remove any `local-service` options in the file, some Linux distros
include this by default which stops the proxy from working.

Create the `/tftpboot` folder in your filesystem and copy the contents of the
boot filesystem from a Raspberry Pi SD card into it.

Add the following options to `config.txt`:

    auto_initramfs=1
    arm_64bit=1

Remove any copied kernel and initramfs from `/tftpboot`, and then symlink these
into the Kiwi build tree. For RPi 3, the paths are `kernel8.img` and
`initramfs8`. For RPi 5, they are `kernel_2712.img` and `initramfs_2712`.

    rm /tftpboot/kernel8.img /tftpboot/initramfs8 /tftpboot/kernel_2712.img /tftpboot/initramfs_2712
    ln -s /path/to/kiwi/images/arm64/kboot.bin /tftpboot/kernel8.img
    ln -s /path/to/kiwi/images/arm64/boot.tar /tftpboot/initramfs8
    ln -s /path/to/kiwi/images/arm64/kboot.bin /tftpboot/kernel_2712.img
    ln -s /path/to/kiwi/images/arm64/boot.tar /tftpboot/initramfs_2712

Build Kiwi:

    scons boot_archive

Each time you run this, this will now update the kernel and boot image that
are set up for your Raspberry Pi to boot from.

KBoot and the Kiwi kernel will output their log to a UART:

* RPi 3: mini UART on the GPIO (pins 8 and 10, GPIO 14 and 15)
* RPi 5: UART on the debug port (requires Raspberry Pi Debug Probe)

Set the following `config.txt` options:

    enable_uart=1
    uart_2ndstage=1

You can then connect to this UART with the appropriate adapter to receive log
output and interact with KDB.