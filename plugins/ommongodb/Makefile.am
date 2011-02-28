mongodir = ./mongo-c-driver/src
pkglib_LTLIBRARIES = ommongodb.la

ommongodb_la_SOURCES = ommongodb.c
ommongodb_la_SOURCES += $(mongodir)/bson.c $(mongodir)/mongo.c $(mongodir)/md5.c $(mongodir)/numbers.c

ommongodb_la_CPPFLAGS =  -DMONGO_HAVE_STDINT -Imongo-c-driver/src  $(RSRT_CFLAGS) $(PTHREADS_CFLAGS) 
ommongodb_la_LDFLAGS = -module -avoid-version
ommongodb_la_LIBADD = 

EXTRA_DIST = 
