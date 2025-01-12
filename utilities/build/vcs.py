#
# SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
# SPDX-License-Identifier: ISC
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
