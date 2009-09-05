#!/usr/bin/env python

import sys

def main():
	if len(sys.argv) != 3:
		print "Usage: %s <array name> <input>" % (sys.argv[0])
		return 1

	print 'unsigned char %s[] = {' % (sys.argv[1])

	f = open(sys.argv[2], 'r')
	str = f.read()
	for i in range(0, len(str)):
		print '0x%x,' % (ord(str[i]))
	print '};'
	print 'unsigned int %s_size = %d;' % (sys.argv[1], len(str))

if __name__ == '__main__':
	sys.exit(main())
