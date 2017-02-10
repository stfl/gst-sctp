# Note: plugindir is set in configure

plugin_LTLIBRARIES = libgstsctpsink.la libgstsctpsrc.la

# sources used to compile this plug-in
libgstsctpsink_la_SOURCES = gstsctpsink.c gstsctpsink.h

# compiler and linker flags used to compile this plugin, set in configure.ac
libgstsctpsink_la_CFLAGS = $(GST_CFLAGS)
libgstsctpsink_la_LIBADD = $(GST_LIBS)
libgstsctpsink_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
libgstsctpsink_la_LIBTOOLFLAGS = --tag=disable-static


# sources used to compile this plug-in
libgstsctpsrc_la_SOURCES = gstsctpsink.c gstsctpsink.h

# compiler and linker flags used to compile this plugin, set in configure.ac
libgstsctpsrc_la_CFLAGS = $(GST_CFLAGS)
libgstsctpsrc_la_LIBADD = $(GST_LIBS)
libgstsctpsrc_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
libgstsctpsrc_la_LIBTOOLFLAGS = --tag=disable-static

# headers we need but don't want installed
noinst_HEADERS =
