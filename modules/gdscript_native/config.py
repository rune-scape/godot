#!/usr/bin/env python3


def can_build(env, platform):
    env.module_add_dependencies("gdscript_native", ["gdscript"], True)
    return True


def configure(env):
    pass
