import os

env = Environment()

debug = ARGUMENTS.get("debug", 0)

if env["PLATFORM"] == "win32":
    apachedir = "C:/Program Files/Apache Software Foundation/Apache2.2"

    if int(debug):
        env.Append(CCFLAGS = ["/Zi", "/Od", "/MDd"],
                   LINKFLAGS = ["/DEBUG"])
    else:
        env.Append(CCFLAGS = ["/O2", "/MD"])
    env.Append(CCFLAGS = ["/EHsc", "/W3"],
               CPPDEFINES = ["WIN32"],
               CPPPATH = ["..", apachedir+"/include"],
               LIBPATH = [apachedir+"/lib"],
               LIBS = ["libapr-1.lib"],
               SHLINKCOM=["mt.exe -nologo -manifest ${TARGET}.manifest -outputresource:$TARGET;2"])
    env.SideEffect(["mod_websocket_mosquitto.so.manifest", "mod_websocket_mosquitto.exp", "mod_websocket_mosquitto.lib"], "mod_websocket_mosquitto.so")

    env["no_import_lib"] = "true"

    modulesdir = apachedir+"/modules"
else:
    env.Append(CCFLAGS = ["-Wall", "-pipe"],
               CPPPATH = [".."])
    if int(debug):
        env.Append(CCFLAGS = ["-g"])
    else:
        env.Append(CCFLAGS = ["-O2"])
    if env["PLATFORM"] == "darwin":
        env.Append(CPPDEFINES = ["DARWIN", "SIGPROCMASK_SETS_THREAD_MASK"],
                   CPPPATH = ["/usr/include/httpd", "/usr/include/apr-1"],
                   SHLINKFLAGS = "-undefined dynamic_lookup")
        modulesdir = "/usr/libexec/apache2"
    else:
        env.Append(CPPDEFINES = ["LINUX=2", "_FORTIFY_SOURCE=2", "_GNU_SOURCE", "_LARGEFILE64_SOURCE", "_REENTRANT"],
                   CPPPATH = ["/usr/include/httpd", "/usr/include/apr-1"])
        modulesdir = "/etc/httpd/modules"

mosquitto = env.SharedLibrary(source=["mod_websocket_mosquitto.c"],
                         SHLIBPREFIX="",
                         SHLIBSUFFIX=".so")

env.Install(dir=modulesdir, source=[mosquitto])

# Install

env.Alias("install", modulesdir)
