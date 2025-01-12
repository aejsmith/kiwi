#
# SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
# SPDX-License-Identifier: ISC
#

import SCons.Defaults
from SCons.Script import *
from functools import reduce

# Helpers for creating source lists with certain files only enabled by config
# settings.
def feature_sources(config, files):
    output = []
    for f in files:
        if type(f) is tuple:
            if reduce(lambda x, y: x or y, [config[x] for x in f[0:-1]]):
                output.append(File(f[-1]))
        else:
            output.append(File(f))
    return output
def feature_dirs(config, dirs):
    output = []
    for f in dirs:
        if type(f) is tuple:
            if reduce(lambda x, y: x or y, [config[x] for x in f[0:-1]]):
                output.append(Dir(f[-1]))
        else:
            output.append(Dir(f))
    return output

# KBoot compatibility
FeatureSources = feature_sources

# Raise an error if a certain target is not specified.
def require_target(target, error):
    if GetOption('help') or target in COMMAND_LINE_TARGETS:
        return
    raise SCons.Errors.StopError(error)

# Copy a file.
def Copy(env, dest, src):
    # This silences the output of the command as opposed to using Copy directly.
    return env.Command(
        dest, src,
        Action(lambda target, source, env: SCons.Defaults.copy_func(target[0], source[0]), None))

# Phony command.
def Phony(env, name, dependencies, action):
    return Alias(name, env.Command('__%s' % (name), dependencies, action))
