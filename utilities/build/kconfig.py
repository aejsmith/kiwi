#
# Copyright (C) 2009-2023 Alex Smith
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

# Class to parse a Kconfig configuration file.
class ConfigParser(dict):
    def __init__(self, path):
        dict.__init__(self)

        # Parse the configuration file. If it doesn't exist, just
        # return - the dictionary will be empty so configured() will
        # return false.
        try:
            f = open(path, 'r')
        except IOError:
            return

        # Read and parse the file contents. We return without adding
        # any values if there is a parse error, this will cause
        # configured() to return false and require the user to reconfig.
        lines = f.readlines()
        f.close()
        values = {}
        for line in lines:
            line = line.strip()

            # Ignore blank lines or comments.
            if not len(line) or line[0] == '#':
                continue

            # Split the line into key/value.
            line = line.split('=', 1)
            if len(line) != 2:
                return
            key = line[0].strip()
            value = line[1].strip()
            if len(key) < 8 or key[0:7] != 'CONFIG_' or not len(value):
                return
            key = line[0].strip()[7:]

            # Work out the correct value.
            if value == 'y':
                value = True
            elif value[0] == '"' and value[-1] == '"':
                value = value[1:-1]
            elif value[0:2] == '0x' and len(value) > 2:
                value = int(value, 16)
            elif value.isdigit():
                value = int(value)
            else:
                print("Unrecognised value type: %s" % (value))
                return

            # Add it to the dictionary.
            values[key] = value

        # Everything was OK, add stuff into the real dictionary.
        for (k, v) in values.items():
            self[k] = v

    # Get a configuration value. This returns None for any accesses to
    # undefined keys.
    def __getitem__(self, key):
        try:
            return dict.__getitem__(self, key)
        except KeyError:
            return None

    # Check whether the build configuration exists.
    def configured(self):
        return len(self) > 0
