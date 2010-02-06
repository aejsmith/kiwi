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

"""Hook to write information about the latest commit to a file.

This hook writes revision information about the latest revision pushed to a
repository to a file.

To enable revision information writing for a repo, run
$ bzr revinfo-file <file to write info to>
inside the repository.
"""

import bzrlib
from bzrlib.branch import Branch
from bzrlib.commands import Command, register_command
from bzrlib.option import Option

class RevisionInfoWriter:
	"""Class for writing revision information to a file."""

	def __init__(self, branch, filename):
		self.branch = branch
		self.filename = filename

	def write(self, revid, revno):
		# Get the revision and its first author.
		revision = self.branch.repository.get_revision(revid)
		name, address = bzrlib.config.parse_username(revision.get_apparent_authors()[0])

		# Write out the information.
		f = open(self.filename, 'w')
		f.write('%s<split />%d<split />%s<split />%s' % (revno, revision.timestamp, name, revision.message))
		f.close()

def revinfo_hook_change_tip(params):
	config = params.branch.get_config()

	# Get file name to write to.
	filename = config.get_user_option('revinfo_file')
	if filename is None:
		return

	# Write out the revision.
	writer = RevisionInfoWriter(params.branch, filename).write(params.new_revid, params.new_revno)

Branch.hooks.install_named_hook('post_change_branch_tip', revinfo_hook_change_tip, 'Revinfo')

#############
# Commands. #
#############

class cmd_revinfo_file(Command):
	"""Print or set file to write revision info to."""

	takes_args = ['filename?']
	takes_options = []

	def run(self, filename=None):
		br = Branch.open('.')

		config = br.get_config()
		if filename:
			config.set_user_option('revinfo_file', filename)
		else:
			print config.get_user_option('revinfo_file')

register_command(cmd_revinfo_file)
