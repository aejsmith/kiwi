#!/usr/bin/env python

# Kiwi symbol table generator
# Copyright (C) 2009 Alex Smith
#
# Kiwi is open source software, released under the terms of the Non-Profit
# Open Software License 3.0. You should have received a copy of the
# licensing information along with the source code distribution. If you
# have not received a copy of the license, please refer to the Kiwi
# project website.
#
# Please note that if you modify this file, the license requires you to
# ADD your name to the list of contributors. This boilerplate is not the
# license itself; please refer to the copy of the license you have received
# for complete terms.

import sys

if len(sys.argv) != 2:
	print 'Usage: %s <table name>' % (sys.argv[0])
	sys.exit(0)

print '/* This file is auto-generated, changes will be overwritten. */'
print
print '#include <ksym.h>'
print
print 'static ksym_t %s_array[] = {' % (sys.argv[1])

count = 0

for line in sys.stdin.readlines():
	split = line.split(' ')
	if len(split) == 4:
		isglobal = not split[2].islower()
		print '	{ 0x%s, 0x%s, "%s", %d, true },' \
			% (split[0], split[1], split[3].strip(), isglobal)
		count += 1
	elif len(split) == 3:
		isglobal = not split[1].islower()
		print '	{ 0x%s, 0x0, "%s", %d, true },' \
			% (split[0], split[2].strip(), isglobal)
		count += 1

print '};'
print "ksym_table_t %s = { .symbols = %s_array, .count = %d };" % (sys.argv[1], sys.argv[1], count)
