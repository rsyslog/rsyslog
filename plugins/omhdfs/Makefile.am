pkglib_LTLIBRARIES = omhdfs.la

omhdfs_la_SOURCES = omhdfs.c
#omhdfs_la_CPPFLAGS = -I$(top_srcdir) $(PTHREADS_CFLAGS) $(RSRT_CFLAGS) -I/usr/lib/jvm/java-6-sun/include -I/usr/lib/jvm/java-6-sun/include/linux
omhdfs_la_CPPFLAGS = -I$(top_srcdir) $(PTHREADS_CFLAGS) $(RSRT_CFLAGS) $(JAVA_INCLUDES)
omhdfs_la_LDFLAGS = -module -avoid-version -lhdfs $(JAVA_LIBS)
omhdfs_la_LIBADD = 
