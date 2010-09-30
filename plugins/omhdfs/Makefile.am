pkglib_LTLIBRARIES = omhdfs.la

omhdfs_la_SOURCES = omhdfs.c
omhdfs_la_CPPFLAGS = -I$(top_srcdir) $(PTHREADS_CFLAGS) $(RSRT_CFLAGS) -I/usr/lib/jvm/java-6-sun/include -I/usr/lib/jvm/java-6-sun/include/linux
omhdfs_la_LDFLAGS = -module -avoid-version -lhdfs -L/usr/lib/jvm/java-6-sun-1.6.0.20/jre/lib/amd64 -L/usr/lib/jvm/java-6-sun-1.6.0.20/jre/lib/amd64/server -ljava -ljvm -lverify
omhdfs_la_LIBADD = 
