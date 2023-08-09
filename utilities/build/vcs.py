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

import subprocess

# Obtain the revision number from the Git repository.
def revision_id():
    git = subprocess.Popen(['git', 'rev-parse', '--short', 'HEAD'], stdout = subprocess.PIPE, stderr = subprocess.PIPE)
    revision = git.communicate()[0].decode('utf-8').strip()
    if git.returncode != 0:
        return None
    return revision

# Check whether submodules are checked out.
def check_submodules():
    try:
        git = subprocess.Popen(['git', 'submodule', 'status'], stdout = subprocess.PIPE, stderr = subprocess.PIPE)
        modules = git.communicate()[0].decode('utf-8').split('\n')
        for module in modules:
            if len(module) and module[0] != ' ':
                return False
        return True
    except:
        return True
