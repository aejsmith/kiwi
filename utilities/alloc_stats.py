#!/usr/bin/env python
#
# SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
# SPDX-License-Identifier: ISC
#

import sys
from collections import Counter

def usage():
    sys.stderr.write('Usage: %s [--include-slab] <log file>\n' % (sys.argv[0]))
    sys.exit(1)

i = 1
include_slab = False
if len(sys.argv) == 2:
    if sys.argv[1][0:2] == '--':
        usage()
elif len(sys.argv) == 3:
    if sys.argv[1] == '--include-slab':
        include_slab = True
    else:
        usage()
    i = 2
else:
    usage()

allocations = {}
f = open(sys.argv[i], 'r')
for line in f.readlines():
    line = line.strip().split(' ', 3)
    if len(line) != 4 or (line[0] != 'alloc:' and line[0] != 'free:'):
        continue

    if line[0] == 'alloc:':
        allocations[line[1]] = (line[2], line[3])
    elif line[0] == 'free:':
        try:
            del allocations[line[1]]
        except KeyError:
            pass

# At least the column header width.
addr_width = 7
name_width = 6
for (k, v) in allocations.items():
    addr_width = max(addr_width, len(k))
    name_width = max(name_width, len(v[0]))

print("Outstanding")
print("===========")
print()
print("%s %s Caller" % ("Address".ljust(addr_width), "Source".ljust(name_width)))
print("%s %s ------" % ("-------".ljust(addr_width), "------".ljust(name_width)))

for (k, v) in allocations.items():
    slab_caches = ['slab_bufctl_cache', 'slab_mag_cache', 'slab_slab_cache']
    if include_slab or v[0] not in slab_caches:
        print("%s %s %s" % (k.ljust(addr_width), v[0].ljust(name_width), v[1]))

print()
print("Totals")
print("------")
print()
print("Count    %s Caller" % ("Source".ljust(name_width)))
print("-----    %s ------" % ("------".ljust(name_width)))

counter = Counter(allocations.values())
for (v, count) in counter.most_common():
    print("%s %s %s" % (str(count).ljust(8), v[0].ljust(name_width), v[1]))
