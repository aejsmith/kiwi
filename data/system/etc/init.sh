#!/bin/bash
#
# SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
# SPDX-License-Identifier: ISC
#

#
# System initialisation script. This is spawned by the service manager after
# service registration. One day this should be replaced with service manager
# functionality.
#

# Mount a ramfs at /users/admin as a stopgap until we have ext2 write support.
mkdir -p /users/admin
mount -t ramfs /users/admin
shopt -s dotglob
cp -R /users/template/* /users/admin/

# Initialise network with DHCP.
net_control dhcp /class/net/0

# Run a terminal.
terminal
