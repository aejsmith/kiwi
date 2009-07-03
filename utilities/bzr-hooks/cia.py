# Copyright (C) 2005-2009 Jelmer Vernooij <jelmer@samba.org>
# Copyright (C) 2009 Alex Smith <alex@alex-smith.me.uk>

# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.


"""Hook to submit pushed commits to CIA (http://cia.navi.cx/)

This is a modification of the original CIA bzr plugin by Jelmer Vernooij
that functions as a post_change_branch_tip hook, allowing it to be set up on
a central repo to submit commits to CIA when they are pushed to the central
repo, rather than when they are committed locally.

To enable CIA reporting for a central repo, run
$ bzr cia-project <name> 
inside the repository.
"""

from xml.sax import saxutils
import socket, xmlrpclib, bzrlib
from bzrlib.branch import Branch
from bzrlib.commands import Command, register_command
from bzrlib.option import Option
from bzrlib.trace import info, warning

class CIASubmitter:
	"""Class for submitting branch changes to CIA."""

	class CIADeliverError(Exception):
		def __init__(self, message):
			self.message = message

	def __init__(self, branch, project):
		self.branch = branch
		self.project = project

	def generate_xml(self, revid, revno):
		revision = self.branch.repository.get_revision(revid)
		delta = self.branch.repository.get_revision_delta(revid)
		authors = revision.get_apparent_authors()

		files = []
		[files.append(f) for (f,_,_) in delta.added]
		[files.append(f) for (f,_,_) in delta.removed]
		[files.append(f) for (_,f,_,_,_,_) in delta.renamed]
		[files.append(f) for (f,_,_,_,_) in delta.modified]

		authors_xml = "".join(["      <author>%s</author>\n" % saxutils.escape(author) for author in authors])
		files_xml = "\n".join(["<file>%s</file>" % saxutils.escape(f) for f in files])

		return """
<message>
  <generator> 
    <name>Bzr (KiwiCIA)</name> 
    <version>%s</version> 
    <url>http://kiwi.alex-smith.me.uk</url>
  </generator>
  <source>
    <project>%s</project>
    <module>%s</module>
  </source>
  <timestamp>%d</timestamp>
  <body>
    <commit>
      <revision>%s</revision>
      <files>%s</files>
      %s
      <log>%s</log>
    </commit>
  </body>
</message>
""" % (bzrlib.version_string, self.project, self.branch.nick, revision.timestamp, revno,
       files_xml, authors_xml, saxutils.escape(revision.message))

	def deliver(self, server, msg):
		try:
			server.hub.deliver(msg)
		except xmlrpclib.ProtocolError, e:
			raise CIADeliverError(e.errmsg)
		except socket.gaierror, (_, errmsg):
			raise CIADeliverError(errmsg)
		except socket.error, (_, errmsg):
			raise CIADeliverError(errmsg)

	def submit(self, revid, revno, dry_run):
		info("Submitting revision %d to CIA." % (revno))

		# Generate the message.
		msg = self.generate_xml(revid, revno)

		# If dry run, just print it, else submit.
		if not dry_run:
			try:
				self.deliver(xmlrpclib.ServerProxy('http://cia.navi.cx'), msg)
			except self.CIADeliverError, (error, ):
				warning("Unable to submit revision %d to CIA: %s" % (revno, error))
		else:
			info(msg)

def cia_hook_change_tip(params):
	config = params.branch.get_config()

	# Get project name.
	project = config.get_user_option('cia_project')
	if project is None:
		return

	# Don't do anything if old revno is greater than new revno (i.e.
	# uncommit on central repo).
	if params.new_revno <= params.old_revno:
		return

	# Get dry run setting.
	if config.get_user_option('cia_dry_run'):
		dry_run = True
	else:
		dry_run = False

	# Create the submitter object.
	submitter = CIASubmitter(params.branch, project)

	# Iterate over each revision in the change set.
	revisions = params.branch.iter_merge_sorted_revisions(params.new_revid, params.old_revid, 'exclude', 'forward')
	for revision in revisions:
		# Ignore merge commits.
		if revision[1]:
			continue
		submitter.submit(revision[0], revision[2][0], dry_run)

Branch.hooks.install_named_hook('post_change_branch_tip', cia_hook_change_tip, 'CIA')

#############
# Commands. #
#############

class cmd_cia_project(Command):
	"""Print or set project to submit changes to on CIA."""

	takes_args = ['project?']
	takes_options = []

	def run(self, project=None):
		br = Branch.open('.')

		config = br.get_config()
		if project:
			config.set_user_option('cia_project', project)
		else:
			print config.get_user_option('cia_project')

register_command(cmd_cia_project)
