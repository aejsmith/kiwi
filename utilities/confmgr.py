# Kiwi build system
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

import os
import sys
import time
import snack

from SCons.Script import *

# Configuration UI class using Newt/Snack.
class ConfigTextUI:
	def __init__(self, manager):
		self.manager = manager
		self.maxwidth = 70

	# Display an edit menu for a ConfigOption.
	def option_menu(self, entry):
		# Create the entry window.
		(button, values) = snack.EntryWindow(
					self.screen, entry.name, '',
					[('New value:', str(entry.get()))],
					buttons=[('OK', True), ('Cancel', False)],
					entryWidth=self.maxwidth - 20,
					width=self.maxwidth)
		if not button:
			return

		# Try to convert the value to the correct type.
		try:
			value = entry.type(values[0])
		except ValueError:
			return
		entry.set(value)

	# Display an edit menu for a ConfigChoice.
	def choice_menu(self, entry):
		# Create the form that we will be adding things to.
		grid = snack.GridForm(self.screen, entry.name, 1, 2)

		# Work out which choices we are able to display.
		choices = []
		for choice in entry.choices:
			if choice.checkdeps():
				choices.append(choice)

		# Create the listbox and add these choices. Set a height limit
		# to prevent the window from growing too big.
		listbox = snack.Listbox(len(choices), len(choices) >= 3, 1, self.maxwidth - 10)
		for (i, c) in enumerate(choices):
			listbox.append('%s (%s)' % (c.desc, c.value), i)
		grid.add(listbox, 0, 0, padding=(0, 1, 0, 1), growx=1)

		# Set the current listbox entry.
		for (i, c) in enumerate(choices):
			if c == entry.value:
				listbox.setCurrent(i)
				break

		# Create the help text box.
		textbox = snack.TextboxReflowed(self.maxwidth, 'Select the new value above and press <Enter>.')
		grid.add(textbox, 0, 1)

		# Display the menu and set the value.
		grid.runOnce()
		entry.value = choices[listbox.current()]

	# Display the entry edit menu.
	def entry_menu(self, entry):
		# If this is a boolean option, just swap its value it finish.
		if entry.type == bool:
			entry.set(not entry.get())
			return
		elif isinstance(entry, ConfigManager.ConfigOption):
			self.option_menu(entry)
		elif isinstance(entry, ConfigManager.ConfigChoice):
			self.choice_menu(entry)
		else:
			raise Exception, "WTF?"

	# Display the menu for a section.
	def section_menu(self, section, num):
		# Create the form that we will be adding things to.
		grid = snack.GridForm(self.screen, section.desc, 1, 4)

		# Work out which options we are able to display.
		entries = []
		for entry in section.entries:
			if entry.checkdeps():
				entries.append(entry)

		# Create the listbox and add these entries. Set a height limit
		# to prevent the window from growing too big.
		height = len(entries) > 8 and 8 or len(entries)
		listbox = snack.Listbox(height, len(entries) >= 3, 1, self.maxwidth - 10)
		for (i, e) in enumerate(entries):
			listbox.append('%s [%s]' % (e.name, e.get()), i)
		grid.add(listbox, 0, 0, padding=(0, 1, 0, 1), growx=1)

		# Set the current listbox entry.
		listbox.setCurrent(num)

		# Create the text box that displays the item description.
		descbox = snack.TextboxReflowed(self.maxwidth, entries[num].desc)
		grid.add(descbox, 0, 1, padding=(0, 0, 0, 1))

		# Set the list callback to update the item description.
		def list_callback():
			descbox.setText(entries[listbox.current()].desc)
			grid.draw()
			self.screen.refresh()
		listbox.setCallback(list_callback)

		# Create the help text box.
		textbox = snack.TextboxReflowed(
				self.maxwidth,
		                'Select an entry above and press <Enter> to edit its value.\n' +
		                'When you have finished, select Back below to return to the menu.'
		)
		grid.add(textbox, 0, 2, padding=(0, 0, 0, 1))

		# Add the button bar to the form.
		buttonbar = snack.ButtonBar(self.screen, ['Back'])
		grid.add(buttonbar, 0, 3, growx=1)

		# Display the menu.
		ret = grid.runOnce()
		if buttonbar.buttonPressed(ret):
			# Button was pressed, return None to go back to the
			# main menu.
			return (0, None)
		else:
			# An entry was selected, return it.
			return (listbox.current(), entries[listbox.current()])

	# Display the main sections menu.
	def main_menu(self):
		# Create the form that we will be adding things to.
		title = 'Kiwi v%s Configuration' % (self.manager.version['KIWI_VER_STRING'])
		grid = snack.GridForm(self.screen, title, 1, 3)

		# Get the list of sections we are able to display.
		sections = []
		for section in self.manager.sections:
			if section.checkdeps():
				sections.append(section)

		# Create a listbox and add our sections to it.
		listbox = snack.Listbox(len(sections), len(sections) >= 3, 1, self.maxwidth - 10)
		for (i, s) in enumerate(sections):
			listbox.append(s.desc, i)
		grid.add(listbox, 0, 0, padding=(0, 1, 0, 1), growx=1)

		# Create the text box.
		textbox = snack.TextboxReflowed(
				self.maxwidth,
		                'Select a category above and press <Enter> to view options for it.\n' +
		                'When you have finished, select Save below to save the configuration.'
		)
		grid.add(textbox, 0, 1, padding=(0, 0, 0, 1))

		# Add the button bar to the form.
		buttonbar = snack.ButtonBar(self.screen, [('Save', True), ('Cancel', False)])
		grid.add(buttonbar, 0, 2, growx=1)

		# Display the menu and return information about what gets
		# selected.
		ret = grid.runOnce()
		return (buttonbar.buttonPressed(ret), sections[listbox.current()])

	# Main user interface function. Returns True if the configuration
	# should be saved, False if not.
	def run(self):
		self.screen = snack.SnackScreen()
		try:
			# Main loop for the UI, will run until a button is
			# pressed on the main menu.
			while True:
				(button, section) = self.main_menu()
				if button != None:
					return button

				# Display the section menu for the selected
				# section.
				num = 0
				while True:
					(num, entry) = self.section_menu(section, num)
					if not entry:
						break
					self.entry_menu(entry)
		finally:
			self.screen.finish()

# Class to manage build configuration.
# @todo		Handle dependencies on what we import.
class ConfigManager:
	# Class defining a configuration Section.
	class ConfigSection:
		def __init__(self, manager, desc, depends):
			self.manager = manager
			self.desc = desc
			self.depends = depends
			self.entries = []

		# Check dependencies of the section.
		def checkdeps(self):
			for (k, f) in self.depends.items():
				if not k in self.manager or not f(self.manager[k]):
					return False
			return True

		# Write out all of the configuration entries in this section.
		def write(self, f):
			f.write('\n# %s\n\n' % (self.desc))
			for entry in self.entries:
				if not entry.checkdeps():
					continue
				f.write('config[%s] = %s\n' % (repr(entry.name), repr(entry.get())))

	# Class defining a configuration Option.
	class ConfigOption:
		def __init__(self, section, name, desc, default, depends):
			self.section = section
			self.name = name
			self.desc = desc
			self.value = default
			self.depends = depends
			self.type = type(default)

		def checkdeps(self):
			for (k, f) in self.depends.items():
				if not k in self.section.manager or not f(self.section.manager[k]):
					return False
			return True

		def get(self):
			if not self.checkdeps():
				raise KeyError, "Accessing '%s' which has unmet dependencies." % (self.name)
			return self.value

		def set(self, value):
			if not self.checkdeps():
				raise KeyError, "Setting '%s' which has unmet dependencies." % (self.name)
			elif type(value) != self.type:
				raise TypeError, "Setting '%s' with incorrect type." % (self.name)
			self.value = value

	# Class defining a configuration Choice.
	class ConfigChoice:
		# Class defining a single option for a Choice.
		class Choice:
			def __init__(self, parent, value, desc, depends={}):
				self.parent = parent
				self.value = value
				self.desc = desc
				self.depends = depends

			def checkdeps(self):
				for (k, f) in self.depends.items():
					if not k in self.parent.section.manager:
						return False
					if not f(self.parent.section.manager[k]):
						return False
				return True

		def __init__(self, section, name, desc, choices, default, depends):
			self.section = section
			self.name = name
			self.desc = desc
			self.choices = []
			self.depends = depends
			self.type = type(default)
			self.value = None

			# Add the choices to the list, check that they are all
			# the correct type and that the default value is valid.
			for c in choices:
				if type(c[0]) != self.type:
					raise Exception, 'Choice %s is incorrect type' % (c[0])

				# Add the choice.
				self.choices.append(self.Choice(self, c[0], c[1], len(c) >= 3 and c[2] or {}))
				if c[0] == default:
					self.value = self.choices[-1]
			if self.value == None:
				raise Exception, 'Default value %s is not in choices' % (default)

		def checkdeps(self):
			for (k, f) in self.depends.items():
				if not k in self.section.manager or not f(self.section.manager[k]):
					return False

			# Check if we actually have any choices we can show.
			for c in self.choices:
				if c.checkdeps():
					return True
			return False

		def get(self):
			return self.value.value

		def set(self, value):
			if type(value) != self.type:
				raise TypeError, "Setting '%s' with incorrect type." % (self.name)
			for i, c in enumerate(self.choices):
				if c.value == value:
					self.value = c
					return
			raise KeyError, "Setting '%s' with invalid choice." % (self.name)

	##################
	# Main functions #
	##################

	# Import the build configuration into the configuration manager.
	def __init__(self, template, cfgfile, version):
		# Initialize variables.
		self.sections = []
		self.extras = {}
		self.curr_section = None
		self.postconf_func = None
		self.outfile = cfgfile
		self.version = version

		# List of directives that can be used by a template.
		directives = {
			'Section': self.SectionDirective,
			'Option': self.OptionDirective,
			'Choice': self.ChoiceDirective,
			'PostConfig': self.PostConfigDirective,
		}

		# Read in the configuration template.
		execfile(template, directives, None)

		# If we're configured, then import the existing configuration.
		if self.configured():
			array = {}
			execfile(cfgfile, {'config': array}, None)

			# Bring the values into the configuration.
			for (k, v) in array.items():
				try:
					self[k] = v
				except (TypeError, KeyError):
					continue

			# This should have done dependency checks, etc, and
			# not added anything that doesn't exist. Rewrite the
			# configuration.
			self.writeconfig()

		# Run any post-configuration function.
		if self.postconf_func:
			self.postconf_func(self)

	# Write out the configuration.
	def writeconfig(self):
		f = open(self.outfile, 'w')
		f.write('# This file is automatically generated. Do not edit.\n')
		f.write('# Generated on %s.\n' % (time.strftime('%c')))

		# Iterate over each section and write it.
		for section in self.sections:
			if section.checkdeps():
				section.write(f)
		f.close()

	# Check if the configuration exists.
	def configured(self):
		return os.path.exists(self.outfile)

	# Run the configuration UI to create the build configuration.
	def configure(self, target, source, env):
		if ConfigTextUI(self).run():
			# Write out the modified configuration.
			self.writeconfig()

	##############################
	# Dictionary-style functions #
	##############################

	def __getitem__(self, key):
		for section in self.sections:
			if not section.checkdeps():
				continue
			for entry in section.entries:
				if entry.name != key:
					continue
				return entry.get()
		if key in self.extras:
			return self.extras[key]
		raise KeyError, repr(key)

	def __setitem__(self, key, value):
		for section in self.sections:
			if not section.checkdeps():
				continue
			for entry in section.entries:
				if entry.name != key:
					continue
				entry.set(value)
				return
		self.extras[key] = value

	def __contains__(self, key):
		for section in self.sections:
			if not section.checkdeps():
				continue
			for entry in section.entries:
				if entry.name != key:
					continue
				elif not entry.checkdeps():
					return False
				return True
		return key in self.extras

	def __iter__(self):
		return iter(self.items())
		
	def items(self):
		items = []
		for section in self.sections:
			if not section.checkdeps():
				continue
			for entry in section.entries:
				if not entry.checkdeps():
					continue
				items.append((entry.name, entry.get()))
		return items + self.extras.items()

	#######################
	# Template directives #
	#######################

	# Start a new configuration section.
	def SectionDirective(self, desc, depends={}):
		section = self.ConfigSection(self, desc, depends)
		self.sections.append(section)
		self.curr_section = section

	# Add an option to the current section.
	def OptionDirective(self, name, desc, default, depends={}):
		if not self.curr_section:
			raise Exception, 'Option outside of Section'
		elif name in self:
			raise Exception, 'Entry %s already exists' % (name)

		# Create the option object and add it to the section.
		option = self.ConfigOption(self.curr_section, name, desc, default, depends)
		self.curr_section.entries.append(option)

	# Add a multiple choice option to the current section.
	def ChoiceDirective(self, name, desc, choices, default, depends={}):
		if not self.curr_section:
			raise Exception, 'Choice outside of Section'
		elif name in self:
			raise Exception, 'Entry %s already exists' % (name)

		# Create the choice object and add it to the section.
		option = self.ConfigChoice(self.curr_section, name, desc, choices, default, depends)
		self.curr_section.entries.append(option)

	# Set the post-configuration function.
	def PostConfigDirective(self, f):
		self.postconf_func = f
		return f
