#!/usr/bin/env python

# Kiwi symbol table generator
# Copyright (C) 2009 Alex Smith
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of version 2 of the GNU General Public License
# as published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License along
# with this program; if not, write to the Free Software Foundation, Inc.,
# 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

import sys

if len(sys.argv) != 2:
	print 'Usage: %s <table name>' % (sys.argv[0])
	sys.exit(0)

print '/* This file is auto-generated, changes will be overwritten. */'
print
print '#include <symtab.h>'
print
print 'SymbolTable::Symbol %s[] = {' % (sys.argv[1])

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
print 'size_t %s_count = %d;' % (sys.argv[1], count)
