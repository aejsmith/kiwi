#!/usr/bin/env python

# Kiwi Markdown documentation generator
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

import sys, os, glob
from markdown2 import markdown
from jinja2 import Environment, FileSystemLoader

def main():
	if len(sys.argv) != 3:
		print "Usage: %s <docs dir> <output dir>" % (sys.argv[0])
		return 1

	# Get a list of documents and parse their content.
	documents = []
	for doc in glob.glob(os.path.join(sys.argv[1], '*.txt')):
		# Assume that the first line is the title, and that there are
		# two lines after it before the actual content.
		f = open(doc, 'r')
		title = f.readline().strip()
		f.readline()
		f.readline()

		# Parse the document.
		html = markdown(f.read()).encode(sys.stdout.encoding or "utf-8", 'xmlcharrefreplace')
		f.close()

		# Work out the output file name.
		name = os.path.splitext(os.path.basename(doc))[0] + '.html'

		# Add it to the list.
		documents.append((name, title, html))

	# Create the template loader.
	tpl = Environment(loader=FileSystemLoader(os.path.join(sys.argv[1], 'markdown'))).get_template('template.html')

	# For each document, write the template.
	for doc in documents:
		f = open(os.path.join(sys.argv[2], doc[0]), 'w')
		f.write(tpl.render(name=doc[0], title=doc[1], content=doc[2], docs=documents))
		f.close()

if __name__ == '__main__':
	sys.exit(main())
