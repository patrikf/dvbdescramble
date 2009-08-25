#!/usr/bin/python

import os.path

def glib_genmarshal_emitter(target, source, env):
    base = os.path.splitext(source[0].get_path())[0]
    return [base + ".c", base + ".h"], source

def glib_genmarshal_generator(source, target, env, for_signature):
    if len(source) != 1:
	raise ValueError("glib-genmarshal expects exactly 1 source")
    if len(target) != 2:
	raise ValueError("glib-genmarshal expects exactly 2 targets")
    return [
	    "glib-genmarshal --body %s > %s" % (source[0], target[0]),
	    "glib-genmarshal --header %s > %s" % (source[0], target[1]),
	]

glib_genmarshal_builder = Builder(generator = glib_genmarshal_generator, suffix = ".c", src_suffix = ".in", emitter = glib_genmarshal_emitter)

env = Environment(CPPPATH = ".")
env.Append(BUILDERS = {'glibMarshal': glib_genmarshal_builder},
	   CFLAGS = "-std=gnu99 -Wall")
if ARGUMENTS.get("debug", False):
    env.Append(CFLAGS = "-g")
if ARGUMENTS.get("stack", False):
    env.Append(CFLAGS = "-fstack-check")

env.ParseConfig("pkg-config --cflags --libs glib-2.0 gobject-2.0")
env.glibMarshal("marshal.in")
env.Program("ca-test", Split("ca-test.c ca-device.c ca-module.c misc.c marshal.c ca-t-c.c ca-resource-manager.c tune.c diseqc.c"));
dvbdescramble = env.Program("dvbdescramble", Split("dvbdescramble.c ca-device.c ca-module.c misc.c marshal.c ca-t-c.c ca-resource-manager.c"));

PREFIX = ARGUMENTS.get("PREFIX", "/usr/local")
BINDIR = ARGUMENTS.get("BINDIR", PREFIX+"/bin")

env.Alias("install", env.Install(BINDIR, dvbdescramble))

