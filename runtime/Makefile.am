sbin_PROGRAMS =
man_MANS =
noinst_LIBRARIES =
noinst_LTLIBRARIES = librsyslog.la
pkglib_LTLIBRARIES =
#pkglib_LTLIBRARIES = librsyslog.la

librsyslog_la_SOURCES = \
	rsyslog.c \
	rsyslog.h \
	typedefs.h \
	dnscache.c \
	dnscache.h \
	unicode-helper.h \
	atomic.h \
	batch.h \
	syslogd-types.h \
	module-template.h \
	im-helper.h \
	obj-types.h \
	sigprov.h \
	cryprov.h \
	nsd.h \
	glbl.h \
	glbl.c \
	unlimited_select.h \
	conf.c \
	conf.h \
	janitor.c \
	janitor.h \
	rsconf.c \
	rsconf.h \
	parser.h \
	parser.c \
	strgen.h \
	strgen.c \
	msg.c \
	msg.h \
	linkedlist.c \
	linkedlist.h \
	objomsr.c \
	objomsr.h \
	stringbuf.c \
	stringbuf.h \
	datetime.c \
	datetime.h \
	srutils.c \
	srUtils.h \
	errmsg.c \
	errmsg.h \
	operatingstate.c \
	operatingstate.h \
	debug.c \
	debug.h \
	obj.c \
	obj.h \
	modules.c \
	modules.h \
	statsobj.c \
	statsobj.h \
	dynstats.c \
	dynstats.h \
	perctile_ringbuf.c \
	perctile_ringbuf.h \
	perctile_stats.c \
	perctile_stats.h \
	statsobj.h \
	stream.c \
	stream.h \
	var.c \
	var.h \
	wtp.c \
	wtp.h \
	wti.c \
	wti.h \
	queue.c \
	queue.h \
	ruleset.c \
	ruleset.h \
	prop.c \
	prop.h \
	ratelimit.c \
	ratelimit.h \
	lookup.c \
	lookup.h \
	cfsysline.c \
	cfsysline.h \
	\
	../action.h \
	../action.c \
	../threads.c \
	../threads.h \
	\
	../parse.c \
	../parse.h \
	\
	hashtable.c \
	hashtable.h \
	hashtable_itr.c \
	hashtable_itr.h \
	hashtable_private.h \
	\
	../outchannel.c \
	../outchannel.h \
	../template.c \
	../template.h \
	timezones.c \
	timezones.h
# the files with ../ we need to work on - so that they either become part of the
# runtime or will no longer be needed. -- rgerhards, 2008-06-13
#
#if OS_LINUX
#librsyslog_la_SOURCES += \
#endif

if WITH_MODDIRS
librsyslog_la_CPPFLAGS = -DSD_EXPORT_SYMBOLS -D_PATH_MODDIR=\"$(pkglibdir)/:$(moddirs)\"
else
librsyslog_la_CPPFLAGS = -DSD_EXPORT_SYMBOLS -D_PATH_MODDIR=\"$(pkglibdir)/\" -I\$(top_srcdir) -I\$(top_srcdir)/grammar
endif
#librsyslog_la_LDFLAGS = -module -avoid-version
librsyslog_la_CPPFLAGS += $(PTHREADS_CFLAGS) $(RSRT_CFLAGS) $(LIBUUID_CFLAGS) $(LIBFASTJSON_CFLAGS) ${LIBESTR_CFLAGS}
librsyslog_la_LIBADD =  $(DL_LIBS) $(RT_LIBS) $(LIBUUID_LIBS) $(LIBFASTJSON_LIBS) ${LIBESTR_LIBS} -lm

if ENABLE_LIBLOGGING_STDLOG
librsyslog_la_CPPFLAGS += ${LIBLOGGING_STDLOG_CFLAGS}
librsyslog_la_LIBADD += $(LIBLOGGING_STDLOG_LIBS)
endif

librsyslog_la_CPPFLAGS += -I\$(top_srcdir)/tools

#
# regular expression support
#
if ENABLE_REGEXP
pkglib_LTLIBRARIES += lmregexp.la
lmregexp_la_SOURCES = regexp.c regexp.h
lmregexp_la_CPPFLAGS = $(PTHREADS_CFLAGS) $(RSRT_CFLAGS)
lmregexp_la_LDFLAGS = -module -avoid-version
lmregexp_la_LIBADD =

if ENABLE_LIBLOGGING_STDLOG
lmregexp_la_CPPFLAGS += $(LIBLOGGING_STDLOG_CFLAGS)
lmregexp_la_LDFLAGS += $(LIBLOGGING_STDLOG_LIBS)
endif

endif

#
# zlib support
#
pkglib_LTLIBRARIES += lmzlibw.la
lmzlibw_la_SOURCES = zlibw.c zlibw.h
lmzlibw_la_CPPFLAGS = $(PTHREADS_CFLAGS) $(RSRT_CFLAGS)
lmzlibw_la_LDFLAGS = -module -avoid-version $(ZLIB_LIBS)
lmzlibw_la_LIBADD =

if ENABLE_LIBLOGGING_STDLOG
lmzlibw_la_CPPFLAGS += $(LIBLOGGING_STDLOG_CFLAGS)
lmzlibw_la_LDFLAGS += $(LIBLOGGING_STDLOG_LIBS)
endif


#
# basic network support, needed for rsyslog startup (e.g. our own system name)
#
pkglib_LTLIBRARIES += lmnet.la
lmnet_la_SOURCES = net.c net.h netns_socket.c netns_socket.h
lmnet_la_CPPFLAGS = $(PTHREADS_CFLAGS) $(RSRT_CFLAGS)
lmnet_la_LDFLAGS = -module -avoid-version ../compat/compat_la-getifaddrs.lo
lmnet_la_LIBADD =

if ENABLE_INET
pkglib_LTLIBRARIES += lmnetstrms.la
if ENABLE_LIBLOGGING_STDLOG
lmnet_la_CPPFLAGS += $(LIBLOGGING_STDLOG_CFLAGS)
lmnet_la_LDFLAGS += $(LIBLOGGING_STDLOG_LIBS)
endif

# network stream master class and stream factory
lmnetstrms_la_SOURCES = netstrms.c netstrms.h \
			netstrm.c netstrm.h
lmnetstrms_la_CPPFLAGS = $(PTHREADS_CFLAGS) $(RSRT_CFLAGS)
lmnetstrms_la_LDFLAGS = -module -avoid-version
lmnetstrms_la_LIBADD =

if ENABLE_LIBLOGGING_STDLOG
lmnetstrms_la_CPPFLAGS += $(LIBLOGGING_STDLOG_CFLAGS)
lmnetstrms_la_LDFLAGS += $(LIBLOGGING_STDLOG_LIBS)
endif

# netstream drivers

# plain tcp driver - main driver
pkglib_LTLIBRARIES += lmnsd_ptcp.la
lmnsd_ptcp_la_SOURCES = nsd_ptcp.c nsd_ptcp.h
lmnsd_ptcp_la_CPPFLAGS = $(PTHREADS_CFLAGS) $(RSRT_CFLAGS)
lmnsd_ptcp_la_LDFLAGS = -module -avoid-version
lmnsd_ptcp_la_LIBADD =

if ENABLE_LIBLOGGING_STDLOG
lmnsd_ptcp_la_CPPFLAGS += $(LIBLOGGING_STDLOG_CFLAGS)
lmnsd_ptcp_la_LDFLAGS += $(LIBLOGGING_STDLOG_LIBS)
endif

endif # if ENABLE_INET

#
# openssl base and netstream driver
#
if ENABLE_OPENSSL
# noinst_LTLIBRARIES += lmnsd_ossl.la
pkglib_LTLIBRARIES += lmnsd_ossl.la
lmnsd_ossl_la_SOURCES = net_ossl.c net_ossl.h nsd_ossl.c nsd_ossl.h
lmnsd_ossl_la_CPPFLAGS = $(PTHREADS_CFLAGS) $(RSRT_CFLAGS) $(OPENSSL_CFLAGS)
lmnsd_ossl_la_LDFLAGS = -module -avoid-version
lmnsd_ossl_la_LIBADD = $(OPENSSL_LIBS)

endif

#
# GnuTLS netstream driver
#
if ENABLE_GNUTLS
pkglib_LTLIBRARIES += lmnsd_gtls.la
lmnsd_gtls_la_SOURCES = nsd_gtls.c nsd_gtls.h
lmnsd_gtls_la_CPPFLAGS = $(PTHREADS_CFLAGS) $(RSRT_CFLAGS) $(GNUTLS_CFLAGS)
lmnsd_gtls_la_LDFLAGS = -module -avoid-version
lmnsd_gtls_la_LIBADD = $(GNUTLS_LIBS)
endif

#
# Mbed TLS netstream driver
#
if ENABLE_MBEDTLS
pkglib_LTLIBRARIES += lmnsd_mbedtls.la
lmnsd_mbedtls_la_SOURCES = nsd_mbedtls.c nsd_mbedtls.h
lmnsd_mbedtls_la_CPPFLAGS = $(PTHREADS_CFLAGS) $(RSRT_CFLAGS) $(MBEDTLS_CFLAGS)
lmnsd_mbedtls_la_LDFLAGS = -module -avoid-version
lmnsd_mbedtls_la_LIBADD = $(MBEDTLS_LIBS)
endif

#
# support library for libgcrypt
#
if ENABLE_LIBGCRYPT
   noinst_LTLIBRARIES += libgcry.la
   libgcry_la_SOURCES = libgcry.c libcry_common.c libcry_common.h libgcry.h
   libgcry_la_CPPFLAGS =  $(RSRT_CFLAGS) $(LIBGCRYPT_CFLAGS)
   pkglib_LTLIBRARIES += lmcry_gcry.la
   lmcry_gcry_la_DEPENDENCIES = libgcry.la
   lmcry_gcry_la_SOURCES = lmcry_gcry.c lmcry_gcry.h
   lmcry_gcry_la_CPPFLAGS = $(RSRT_CFLAGS) $(LIBGCRYPT_CFLAGS)
   lmcry_gcry_la_LDFLAGS = -module -avoid-version
   lmcry_gcry_la_LIBADD = libgcry.la $(LIBGCRYPT_LIBS)
endif

#
# support library for openssl crypto provider
#
if ENABLE_OPENSSL_CRYPTO_PROVIDER
   # Helper
   noinst_LTLIBRARIES += libossl.la
   libossl_la_SOURCES = libossl.c libossl.h libcry_common.c libcry_common.h
   libossl_la_CPPFLAGS = $(PTHREADS_CFLAGS) $(RSRT_CFLAGS) $(OPENSSL_CFLAGS)

   pkglib_LTLIBRARIES += lmcry_ossl.la
   lmcry_ossl_la_DEPENDENCIES = libossl.la
   lmcry_ossl_la_SOURCES = lmcry_ossl.c lmcry_ossl.h
   lmcry_ossl_la_CPPFLAGS = $(RSRT_CFLAGS) $(OPENSSL_FLAGS)
   lmcry_ossl_la_LDFLAGS = -module -avoid-version
   lmcry_ossl_la_LIBADD = libossl.la $(OPENSSL_LIBS)
endif

#
# support library for zstd
#
if ENABLE_LIBZSTD
pkglib_LTLIBRARIES += lmzstdw.la
    lmzstdw_la_SOURCES = zstdw.c zstdw.h
    lmzstdw_la_CPPFLAGS = $(PTHREADS_CFLAGS) $(RSRT_CFLAGS)
    lmzstdw_la_LDFLAGS = -module -avoid-version $(ZSTD_LIBS)
    lmzstdw_la_LIBADD = -lzstd
endif


#
# gssapi support
#
if ENABLE_GSSAPI
pkglib_LTLIBRARIES += lmgssutil.la
lmgssutil_la_SOURCES = gss-misc.c gss-misc.h
lmgssutil_la_CPPFLAGS = $(PTHREADS_CFLAGS) $(RSRT_CFLAGS)
lmgssutil_la_LDFLAGS = -module -avoid-version
lmgssutil_la_LIBADD = $(GSS_LIBS)
endif

pkglib_LTLIBRARIES += lmtcpsrv.la lmtcpclt.la
#
#
# TCP (stream) server support
#
lmtcpsrv_la_SOURCES = \
	tcps_sess.c \
	tcps_sess.h \
	tcpsrv.c \
	tcpsrv.h
lmtcpsrv_la_CPPFLAGS = $(PTHREADS_CFLAGS) $(RSRT_CFLAGS)
lmtcpsrv_la_LDFLAGS = -module -avoid-version
lmtcpsrv_la_LIBADD =

if ENABLE_LIBLOGGING_STDLOG
lmtcpsrv_la_CPPFLAGS += $(LIBLOGGING_STDLOG_CFLAGS)
lmtcpsrv_la_LDFLAGS += $(LIBLOGGING_STDLOG_LIBS)
endif

#
# TCP (stream) client support
#
lmtcpclt_la_SOURCES = \
	tcpclt.c \
	tcpclt.h
lmtcpclt_la_CPPFLAGS = $(PTHREADS_CFLAGS) $(RSRT_CFLAGS)
lmtcpclt_la_LDFLAGS = -module -avoid-version
lmtcpclt_la_LIBADD =

if ENABLE_LIBLOGGING_STDLOG
lmtcpclt_la_CPPFLAGS += $(LIBLOGGING_STDLOG_CFLAGS)
lmtcpclt_la_LDFLAGS += $(LIBLOGGING_STDLOG_LIBS)
endif


#
# support library for Guardtime KSI-LS12
#
if ENABLE_KSI_LS12
   pkglib_LTLIBRARIES += lmsig_ksi_ls12.la
   lmsig_ksi_ls12_la_SOURCES = lmsig_ksi-ls12.c lmsig_ksi-ls12.h lib_ksils12.c \
			lib_ksils12.h lib_ksi_queue.c lib_ksi_queue.h
   lmsig_ksi_ls12_la_CPPFLAGS = $(RSRT_CFLAGS) $(GT_KSI_LS12_CFLAGS)
   lmsig_ksi_ls12_la_LDFLAGS = -module -avoid-version $(GT_KSI_LS12_LIBS)
endif
