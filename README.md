##  mod_websocket_mosquitto

	This handler is based on an example by `self.disconnect`
	apache-websocket is required please download and install
	$ git clone git://github.com/disconnect/apache-websocket.git


## Building and Installation


	It appears to be working on Centos / Ubuntu and OSX - but your mileage may vary.

	Installing and Running websocket_mosquitto on Ubuntu 11.04:

	$ sudo apt-get install scons apache2-dev

	Compile websockets module:

	$ cd apache-websockets

	Change apache and apr paths in file SConstruct
	----------
	else:
		env.Append(CCFLAGS = ["-Wall", "-pipe"])
		if int(debug):
			env.Append(CCFLAGS = ["-g"])
		else:
			env.Append(CCFLAGS = ["-O2"])
		if env["PLATFORM"] == "darwin":
			env.Append(CPPDEFINES = ["DARWIN", "SIGPROCMASK_SETS_THREAD_MASK"],
					   CPPPATH = ["/usr/include/apache2", "/usr/include/apr-1.0"],
					   SHLINKFLAGS = "-undefined dynamic_lookup")
			modulesdir = "/usr/libexec/apache2"
		else:
			env.Append(CPPDEFINES = ["LINUX=2", "_FORTIFY_SOURCE=2", "_GNU_SOURCE", "_LARGEFILE64_SOURCE", "_REENTRANT"],
					   CPPPATH = ["/usr/include/apache2", "/usr/include/apr-1.0"])
			modulesdir = "/usr/lib/apache2/modules/"
	---------

	Build:
	$ scons

	Install:
	$ sudo scons install


	Compile mosquitto module:

	$ cd mosquitto

	Change paths in file SConstruct
	----------
		else:
			env.Append(CPPDEFINES = ["LINUX=2", "_FORTIFY_SOURCE=2", "_GNU_SOURCE", "_LARGEFILE64_SOURCE", "_REENTRANT"],
					   CPPPATH = ["/usr/include/apache2", "/usr/include/apr-1.0"])
			modulesdir = "/usr/lib/apache2/modules"
	----------

	Build:
	$ scons

	Install:
	$ sudo scons install

	Create apache load file:
	$ sudo vi /etc/apache2/mods-available/websocket.load
	---
	LoadModule websocket_module   /usr/lib/apache2/modules/mod_websocket.so
	LoadModule websocket_draft76_module   /usr/lib/apache2/modules/mod_websocket_draft76.
	---

	Create websocket.conf file:
	$ sudo vi /etc/apache2/mods-available/websocket.conf
	---
	<IfModule mod_websocket.c>
	Loadmodule mod_websocket_mosquitto /usr/lib/apache2/modules/mod_websocket_mosquitto.so
	  <Location /mosquitto>
		MosBroker localhost
		MosPort 1883
		SetHandler websocket-handler
		WebSocketHandler /usr/lib/apache2/modules/mod_websocket_mosquitto.so mosquitto_init
	  </Location>
	</IfModule>
	---

	Fix module file permissions:
	$ ls /usr/lib/apache2/modules
	-rw-r--r-- 1 root root   14344 2012-02-14 20:59 mod_usertrack.so
	-rw-r--r-- 1 root root   10240 2012-02-14 20:59 mod_version.so
	-rw-r--r-- 1 root root   10248 2012-02-14 20:59 mod_vhost_alias.so
	-rwxr-xr-x 1 root root   19109 2012-10-28 20:09 mod_websocket_draft76.so
	-rwxr-xr-x 1 root root   13599 2012-10-29 16:19 mod_websocket_mosquitto.so
	-rwxr-xr-x 1 root root   27675 2012-10-28 20:09 mod_websocket.so

	$ sudo chmod 644 /usr/lib/apache2/modules/mod_websocket*

	Enable apache module:
	$ sudo a2enmod websocket

	Restart apache
	$ sudo /etc/init.d/apache2 restart


## License

Please see the file called LICENSE.
