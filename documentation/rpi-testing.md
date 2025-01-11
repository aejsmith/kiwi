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

Remove any copied `kernel8.img` and `initramfs8`, and then symlink these into
your Kiwi build tree:

    rm /tftpboot/kernel8.img /tftpboot/initramfs8
    ln -s /path/to/kiwi/images/arm64/kboot.bin /tftpboot/kernel8.img
    ln -s /path/to/kiwi/images/arm64/boot.tar /tftpboot/initramfs8

Build Kiwi:

    scons boot_archive

Each time you run this, this will now update the kernel and boot image that
are set up for your Raspberry Pi to boot from.

Raspberry Pi 3
--------------

KBoot and the Kiwi kernel will output their log to the mini UART on the GPIO
header. Set the following `config.txt` options:

    enable_uart=1
    uart_2ndstage=1

You can then connect a USB UART adapter to these pins to receive log output
and interact with KDB.
