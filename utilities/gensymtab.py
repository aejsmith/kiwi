#!/usr/bin/env python
#
# Copyright (C) 2009-2011 Alex Smith
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

import sys

if len(sys.argv) != 2:
	print 'Usage: %s <table name>' % (sys.argv[0])
	sys.exit(0)

print '/* This file is auto-generated, changes will be overwritten. */'
print
print '#include <symbol.h>'
print
print 'static symbol_t %s_array[] = {' % (sys.argv[1])

count = 0

for line in sys.stdin.readlines():
	split = line.split(' ')
	if len(split) == 4:
		isglobal = not split[2].islower()
		print '	{ { NULL, NULL }, 0x%s, 0x%s, "%s", %d, true },' \
			% (split[0], split[1], split[3].strip(), isglobal)
		count += 1
	elif len(split) == 3:
		isglobal = not split[1].islower()
		print '	{ { NULL, NULL }, 0x%s, 0x0, "%s", %d, true },' \
			% (split[0], split[2].strip(), isglobal)
		count += 1

print '};'
print "symbol_table_t %s = { .symbols = %s_array, .count = %d };" % (sys.argv[1], sys.argv[1], count)
