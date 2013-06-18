if ENABLE_TESTBENCH
# TODO: reenable TESTRUNS = rt_init rscript
check_PROGRAMS = $(TESTRUNS) ourtail nettester tcpflood chkseq msleep randomgen diagtalker uxsockrcvr syslog_caller syslog_inject inputfilegen minitcpsrv
TESTS = $(TESTRUNS) 
#TESTS = $(TESTRUNS) cfg.sh

if ENABLE_IMDIAG
TESTS +=  \
	arrayqueue.sh \
	da-mainmsg-q.sh \
	validation-run.sh \
	imtcp-multiport.sh \
	daqueue-persist.sh \
	diskqueue.sh \
	diskqueue-fsync.sh \
	rulesetmultiqueue.sh \
	manytcp.sh \
	rsf_getenv.sh \
	imtcp_conndrop.sh \
	imtcp_addtlframedelim.sh \
	sndrcv.sh \
	sndrcv_failover.sh \
	sndrcv_gzip.sh \
	sndrcv_udp.sh \
	sndrcv_udp_nonstdpt.sh \
	asynwr_simple.sh \
	asynwr_timeout.sh \
	asynwr_small.sh \
	asynwr_tinybuf.sh \
	wr_large_async.sh \
	wr_large_sync.sh \
	asynwr_deadlock.sh \
	asynwr_deadlock2.sh \
	asynwr_deadlock4.sh \
	gzipwr_large.sh \
	gzipwr_large_dynfile.sh \
	dynfile_invld_async.sh \
	dynfile_invld_sync.sh \
	dynfile_invalid2.sh \
	complex1.sh \
	queue-persist.sh \
	pipeaction.sh \
	execonlyonce.sh \
	execonlywhenprevsuspended.sh \
	execonlywhenprevsuspended2.sh \
	execonlywhenprevsuspended3.sh \
	execonlywhenprevsuspended4.sh \
	pipe_noreader.sh \
	dircreate_dflt.sh \
	dircreate_off.sh \
	imuxsock_logger_root.sh \
	imuxsock_traillf_root.sh \
	imuxsock_ccmiddle_root.sh \
	udp-msgreduc-vg.sh \
	udp-msgreduc-orgmsg-vg.sh \
	queue-persist.sh 
	discard-rptdmsg.sh \
	discard-allmark.sh \
	discard.sh \
	failover-async.sh \
	failover-double.sh \
	failover-basic.sh \
	failover-rptd.sh \
	failover-no-rptd.sh \
	failover-no-basic.sh \
	rcvr_fail_restore.sh \
	rscript_contains.sh \
	rscript_field.sh \
	rscript_stop.sh \
	rscript_stop2.sh \
	rscript_prifilt.sh \
	rscript_optimizer1.sh \
	rscript_ruleset_call.sh \
	cee_simple.sh \
	cee_diskqueue.sh \
	incltest.sh \
	incltest_dir.sh \
	incltest_dir_wildcard.sh \
	incltest_dir_empty_wildcard.sh \
	linkedlistqueue.sh

if HAVE_VALGRIND
TESTS +=  \
	discard-rptdmsg-vg.sh \
	discard-allmark-vg.sh \
	failover-basic-vg.sh \
	failover-rptd-vg.sh \
	failover-no-basic-vg.sh \
	failover-no-rptd-vg.sh \
	tcp-msgreduc-vg.sh
endif # HAVE_VALGRIND
endif # ENABLE_IMDIAG


if ENABLE_MYSQL_TESTS
TESTS +=  \
	mysql-basic.sh \
	mysql-basic-cnf6.sh \
	mysql-asyn.sh 
if ENABLE_OMLIBDBI
TESTS +=  \
	libdbi-basic.sh \
	libdbi-asyn.sh
endif
if HAVE_VALGRIND
TESTS +=  \
	mysql-basic-vg.sh \
	mysql-asyn-vg.sh 
endif
endif

if ENABLE_IMPTCP
TESTS +=  \
	manyptcp.sh \
	imptcp_large.sh \
	imptcp_addtlframedelim.sh \
	imptcp_conndrop.sh 
endif

if ENABLE_GNUTLS
# TODO: re-enable in newer version
#TESTS +=  \
	#sndrcv_tls_anon.sh \
	#sndrcv_tls_anon_rebind.sh \
	#imtcp-tls-basic.sh
if HAVE_VALGRIND
TESTS += imtcp-tls-basic-vg.sh \
	 imtcp_conndrop_tls-vg.sh 
	 manytcp-too-few-tls-vg.sh 
endif
endif

if ENABLE_OMUXSOCK
TESTS += uxsock_simple.sh
endif

if ENABLE_OMUDPSPOOF
TESTS += sndrcv_omudpspoof.sh \
	 sndrcv_omudpspoof_nonstdpt.sh
endif

if ENABLE_OMSTDOUT
TESTS += omod-if-array.sh \
	 proprepltest.sh \
	 parsertest.sh \
	 timestamp.sh \
	 inputname.sh \
	 threadingmq.sh \
	 threadingmqaq.sh \
	 badqi.sh \
	 tabescape_dflt.sh \
	 tabescape_off.sh \
	 fieldtest.sh
endif

if ENABLE_OMRULESET
if ENABLE_IMDIAG
TESTS += omruleset.sh \
	 omruleset-queue.sh
endif
endif

if ENABLE_EXTENDED_TESTS
# random.sh is temporarily disabled as it needs some work
# to rsyslog core to complete in reasonable time
#TESTS += random.sh 
endif

if ENABLE_IMFILE
TESTS += imfile-basic.sh
if HAVE_VALGRIND
TESTS += imfile-basic-vg.sh
endif
endif

endif # if ENABLE_TESTBENCH

TESTS_ENVIRONMENT = RSYSLOG_MODDIR='$(abs_top_builddir)'/runtime/.libs/
DISTCLEANFILES=rsyslog.pid
test_files = testbench.h runtime-dummy.c

EXTRA_DIST= 1.rstest 2.rstest 3.rstest err1.rstest \
	   validation-run.sh \
	   tls-certs/ca-key.pem \
	   tls-certs/ca.pem \
	   tls-certs/cert.pem \
	   tls-certs/key.pem \
	   testsuites/x.509/ca.pem \
	   testsuites/x.509/ca-key.pem \
	   testsuites/x.509/client-cert.pem \
	   testsuites/x.509/client-key.pem \
	   testsuites/x.509/machine-cert.pem \
	   testsuites/x.509/machine-key.pem \
	   testsuites/invalid.conf \
	   testsuites/valid.conf \
	   cfg.sh \
	   cfg1.cfgtest \
	   cfg1.testin \
	   cfg2.cfgtest \
	   cfg2.testin \
	   cfg3.cfgtest \
	   cfg3.testin \
	   cfg4.cfgtest \
	   cfg4.testin \
	   DevNull.cfgtest \
	   err1.rstest \
	   NoExistFile.cfgtest \
	   timestamp.sh \
	   testsuites/ts3164.conf \
	   testsuites/mon1digit.ts3164 \
	   testsuites/mon2digit.ts3164 \
	   testsuites/Jan.ts3164 \
	   testsuites/Feb.ts3164 \
	   testsuites/Mar.ts3164 \
	   testsuites/Apr.ts3164 \
	   testsuites/May.ts3164 \
	   testsuites/Jun.ts3164 \
	   testsuites/Jul.ts3164 \
	   testsuites/Aug.ts3164 \
	   testsuites/Sep.ts3164 \
	   testsuites/Oct.ts3164 \
	   testsuites/Nov.ts3164 \
	   testsuites/Dec.ts3164 \
	   testsuites/ts3339.conf \
	   testsuites/master.ts3339 \
	   testsuites/tsmysql.conf \
	   testsuites/master.tsmysql \
	   testsuites/tspgsql.conf \
	   testsuites/master.tspgsql \
	   testsuites/subsecond.conf \
	   testsuites/master.subsecond \
	   testsuites/parse_8bit_escape.conf \
	   testsuites/8bit.parse_8bit_escape \
	   testsuites/parse1.conf \
	   testsuites/field1.conf \
	   testsuites/1.parse1 \
	   testsuites/2.parse1 \
	   testsuites/3.parse1 \
	   testsuites/4.parse1 \
	   testsuites/mark.parse1 \
	   testsuites/8bit.parse1 \
	   testsuites/empty.parse1 \
	   testsuites/snare.parse1 \
	   testsuites/oversizeTag-1.parse1 \
	   testsuites/weird.parse1 \
	   testsuites/date1.parse1 \
	   testsuites/date2.parse1 \
	   testsuites/date3.parse1 \
	   testsuites/date4.parse1 \
	   testsuites/date5.parse1 \
   	   testsuites/rfc3164.parse1 \
	   testsuites/rfc5424-1.parse1 \
	   testsuites/rfc5424-2.parse1 \
	   testsuites/rfc5424-3.parse1 \
	   testsuites/rfc5424-4.parse1 \
	   testsuites/malformed1.parse1 \
	   testsuites/reallife.parse1 \
	   testsuites/parse2.conf \
	   testsuites/reallife.parse2 \
	   testsuites/parse3.conf \
	   testsuites/reallife.parse3 \
	   testsuites/parse-nodate.conf \
	   testsuites/samples.parse-nodate \
	   testsuites/parse_invld_regex.conf \
	   testsuites/samples.parse_invld_regex \
	   testsuites/parse-3164-buggyday.conf \
	   testsuites/samples.parse-3164-buggyday \
	   testsuites/snare_ccoff_udp.conf \
	   testsuites/samples.snare_ccoff_udp \
	   testsuites/snare_ccoff_udp2.conf \
	   testsuites/samples.snare_ccoff_udp2 \
	   testsuites/omod-if-array.conf \
	   testsuites/1.omod-if-array \
	   testsuites/1.field1 \
	   killrsyslog.sh \
	   parsertest.sh \
	   fieldtest.sh \
	   rsf_getenv.sh \
	   testsuites/rsf_getenv.conf \
	   diskqueue.sh \
	   testsuites/diskqueue.conf \
	   arrayqueue.sh \
	   testsuites/arrayqueue.conf \
	   rscript_contains.sh \
	   testsuites/rscript_contains.conf \
	   rscript_field.sh \
	   testsuites/rscript_field.conf \
	   rscript_stop.sh \
	   testsuites/rscript_stop.conf \
	   rscript_stop2.sh \
	   testsuites/rscript_stop2.conf \
	   rscript_prifilt.sh \
	   testsuites/rscript_prifilt.conf \
	   rscript_optimizer1.sh \
	   testsuites/rscript_optimizer1.conf \
	   rscript_ruleset_call.sh \
	   testsuites/rscript_ruleset_call.conf \
	   cee_simple.sh \
	   testsuites/cee_simple.conf \
	   cee_diskqueue.sh \
	   testsuites/cee_diskqueue.conf \
	   incltest.sh \
	   testsuites/incltest.conf \
	   incltest_dir.sh \
	   testsuites/incltest_dir.conf \
	   incltest_dir_empty_wildcard.sh \
	   testsuites/incltest_dir_empty_wildcard.conf \
	   incltest_dir_wildcard.sh \
	   testsuites/incltest_dir_wildcard.conf \
	   testsuites/incltest.d/include.conf \
	   linkedlistqueue.sh \
	   testsuites/linkedlistqueue.conf \
	   da-mainmsg-q.sh \
	   testsuites/da-mainmsg-q.conf \
	   diskqueue-fsync.sh \
	   testsuites/diskqueue-fsync.conf \
	   imtcp-tls-basic.sh \
	   imtcp-tls-basic-vg.sh \
	   testsuites/imtcp-tls-basic.conf \
	   imtcp-multiport.sh \
	   testsuites/imtcp-multiport.conf \
	   udp-msgreduc-orgmsg-vg.sh \
	   testsuites/udp-msgreduc-orgmsg-vg.conf \
	   udp-msgreduc-vg.sh \
	   testsuites/udp-msgreduc-vg.conf \
	   manytcp-too-few-tls.sh \
	   testsuites/manytcp-too-few-tls.conf \
	   manytcp.sh \
	   testsuites/manytcp.conf \
	   manyptcp.sh \
	   testsuites/manyptcp.conf \
	   imptcp_large.sh \
	   testsuites/imptcp_large.conf \
	   imptcp_addtlframedelim.sh \
	   testsuites/imptcp_addtlframedelim.conf \
	   imptcp_conndrop.sh \
	   testsuites/imptcp_conndrop.conf \
	   imtcp_conndrop.sh \
	   testsuites/imtcp_conndrop.conf \
	   imtcp_conndrop_tls.sh \
	   imtcp_conndrop_tls-vg.sh \
	   testsuites/imtcp_conndrop.conf \
	   imtcp_addtlframedelim.sh \
	   testsuites/imtcp_addtlframedelim.conf \
	   tcp-msgreduc-vg.sh \
	   testsuites/./tcp-msgreduc-vg.conf \
	   inputname.sh \
	   testsuites/inputname_imtcp.conf \
	   testsuites/1.inputname_imtcp_12514 \
	   testsuites/1.inputname_imtcp_12515 \
	   testsuites/1.inputname_imtcp_12516 \
	   omod-if-array.sh \
	   discard.sh \
	   testsuites/discard.conf \
	   failover-no-rptd.sh \
	   failover-no-rptd-vg.sh \
	   testsuites/failover-no-rptd.conf \
	   failover-no-basic.sh \
	   failover-no-basic-vg.sh \
	   testsuites/failover-no-basic.conf \
	   failover-rptd.sh \
	   failover-rptd-vg.sh \
	   testsuites/failover-rptd.conf \
	   failover-basic.sh \
	   failover-basic-vg.sh \
	   testsuites/failover-basic.conf \
	   failover-async.sh \
	   testsuites/failover-async.conf \
	   failover-double.sh \
	   testsuites/failover-double.conf \
	   discard-rptdmsg.sh \
	   discard-rptdmsg-vg.sh \
	   testsuites/discard-rptdmsg.conf \
	   discard-allmark.sh \
	   discard-allmark-vg.sh \
	   testsuites/discard-allmark.conf \
	   diag.sh \
	   testsuites/diag-common.conf \
	   testsuites/diag-common2.conf \
           rcvr_fail_restore.sh \
	   testsuites/rcvr_fail_restore_rcvr.conf \
	   testsuites/rcvr_fail_restore_sender.conf \
           daqueue-persist.sh \
	   daqueue-persist-drvr.sh \
	   queue-persist.sh \
	   queue-persist-drvr.sh \
	   testsuites/queue-persist.conf \
	   threadingmq.sh \
	   testsuites/threadingmq.conf \
	   threadingmqaq.sh \
	   testsuites/threadingmqaq.conf \
	   sndrcv_drvr.sh \
	   sndrcv_drvr_noexit.sh \
	   sndrcv_failover.sh \
	   testsuites/sndrcv_failover_sender.conf \
	   testsuites/sndrcv_failover_rcvr.conf \
	   sndrcv.sh \
	   testsuites/sndrcv_sender.conf \
	   testsuites/sndrcv_rcvr.conf \
	   sndrcv_udp.sh \
	   testsuites/sndrcv_udp_sender.conf \
	   testsuites/sndrcv_udp_rcvr.conf \
	   sndrcv_udp_nonstdpt.sh \
	   testsuites/sndrcv_udp_nonstdpt_sender.conf \
	   testsuites/sndrcv_udp_nonstdpt_rcvr.conf \
	   sndrcv_omudpspoof.sh \
	   testsuites/sndrcv_omudpspoof_sender.conf \
	   testsuites/sndrcv_omudpspoof_rcvr.conf \
	   sndrcv_omudpspoof_nonstdpt.sh \
	   testsuites/sndrcv_omudpspoof_nonstdpt_sender.conf \
	   testsuites/sndrcv_omudpspoof_nonstdpt_rcvr.conf \
	   sndrcv_gzip.sh \
	   testsuites/sndrcv_gzip_sender.conf \
	   testsuites/sndrcv_gzip_rcvr.conf \
	   pipeaction.sh \
	   testsuites/pipeaction.conf \
	   pipe_noreader.sh \
	   testsuites/pipe_noreader.conf \
	   uxsock_simple.sh \
	   testsuites/uxsock_simple.conf \
	   asynwr_simple.sh \
	   testsuites/asynwr_simple.conf \
	   asynwr_timeout.sh \
	   testsuites/asynwr_timeout.conf \
	   asynwr_small.sh \
	   testsuites/asynwr_small.conf \
	   asynwr_tinybuf.sh \
	   testsuites/asynwr_tinybuf.conf \
	   wr_large_async.sh \
	   wr_large_sync.sh \
	   wr_large.sh \
	   testsuites/wr_large.conf \
	   asynwr_deadlock.sh \
	   testsuites/asynwr_deadlock.conf \
	   asynwr_deadlock2.sh \
	   testsuites/asynwr_deadlock2.conf \
	   asynwr_deadlock4.sh \
	   testsuites/asynwr_deadlock4.conf \
	   gzipwr_large.sh \
	   testsuites/gzipwr_large.conf \
	   gzipwr_large_dynfile.sh \
	   testsuites/gzipwr_large_dynfile.conf \
	   complex1.sh \
	   testsuites/complex1.conf \
	   random.sh \
	   testsuites/random.conf \
	   imfile-basic.sh \
	   imfile-basic-vg.sh \
	   testsuites/imfile-basic.conf \
	   dynfile_invld_async.sh \
	   dynfile_invld_sync.sh \
	   dynfile_cachemiss.sh \
	   testsuites/dynfile_cachemiss.conf \
	   dynfile_invalid2.sh \
	   testsuites/dynfile_invalid2.conf \
	   proprepltest.sh \
	   testsuites/rfctag.conf \
	   testsuites/master.rfctag \
	   testsuites/nolimittag.conf \
	   testsuites/master.nolimittag \
	   rulesetmultiqueue.sh \
	   testsuites/rulesetmultiqueue.conf \
	   omruleset.sh \
	   testsuites/omruleset.conf \
	   omruleset-queue.sh \
	   testsuites/omruleset-queue.conf \
	   badqi.sh \
	   testsuites/badqi.conf \
	   bad_qi/dbq.qi \
	   execonlyonce.sh \
	   testsuites/execonlyonce.conf \
	   testsuites/execonlyonce.data \
	   execonlywhenprevsuspended.sh \
	   testsuites/execonlywhenprevsuspended.conf \
	   execonlywhenprevsuspended2.sh \
	   testsuites/execonlywhenprevsuspended2.conf \
	   execonlywhenprevsuspended3.sh \
	   testsuites/execonlywhenprevsuspended3.conf \
	   execonlywhenprevsuspended4.sh \
	   testsuites/execonlywhenprevsuspended4.conf \
	   tabescape_dflt.sh \
	   testsuites/tabescape_dflt.conf \
	   testsuites/1.tabescape_dflt \
	   tabescape_off.sh \
	   testsuites/tabescape_off.conf \
	   testsuites/1.tabescape_off \
	   dircreate_dflt.sh \
	   testsuites/dircreate_dflt.conf \
	   dircreate_off.sh \
	   testsuites/dircreate_off.conf \
	   imuxsock_logger_root.sh \
	   testsuites/imuxsock_logger_root.conf \
	   resultdata/imuxsock_logger.log \
	   imuxsock_traillf_root.sh \
	   testsuites/imuxsock_traillf_root.conf \
	   resultdata/imuxsock_traillf.log \
	   imuxsock_ccmiddle_root.sh \
	   testsuites/imuxsock_ccmiddle_root.conf \
	   resultdata/imuxsock_ccmiddle.log \
	   testsuites/mysql-truncate.sql \
	   testsuites/mysql-select-msg.sql \
	   libdbi-basic.sh \
	   testsuites/libdbi-basic.conf \
	   libdbi-asyn.sh \
	   testsuites/libdbi-asyn.conf \
	   mysql-basic.sh \
	   mysql-basic-cnf6.sh \
	   mysql-basic-vg.sh \
	   testsuites/mysql-basic.conf \
	   testsuites/mysql-basic-cnf6.conf \
	   mysql-asyn.sh \
	   mysql-asyn-vg.sh \
	   testsuites/mysql-asyn.conf \
	   cfg.sh

# TODO: re-enable
#sndrcv_tls_anon_rebind.sh \
#testsuites/sndrcv_tls_anon_rebind_sender.conf \
#testsuites/sndrcv_tls_anon_rebind_rcvr.conf \
#sndrcv_tls_anon.sh \
#testsuites/sndrcv_tls_anon_sender.conf \
#testsuites/sndrcv_tls_anon_rcvr.conf \
#

ourtail_SOURCES = ourtail.c
msleep_SOURCES = msleep.c
chkseq_SOURCES = chkseq.c

uxsockrcvr_SOURCES = uxsockrcvr.c
uxsockrcvr_LDADD = $(SOL_LIBS)

tcpflood_SOURCES = tcpflood.c
tcpflood_CPPFLAGS = $(PTHREADS_CFLAGS) $(GNUTLS_CFLAGS)
tcpflood_LDADD = $(SOL_LIBS) $(PTHREADS_LIBS) $(GNUTLS_LIBS)
if ENABLE_GNUTLS
tcpflood_LDADD += -lgcrypt
endif

minitcpsrv_SOURCES = minitcpsrvr.c
minitcpsrv_LDADD = $(SOL_LIBS)

syslog_caller_SOURCES = syslog_caller.c
syslog_caller_LDADD = $(SOL_LIBS)

syslog_inject_SOURCES = syslog_inject.c
syslog_inject_LDADD = $(SOL_LIBS)

diagtalker_SOURCES = diagtalker.c
diagtalker_LDADD = $(SOL_LIBS)

randomgen_SOURCES = randomgen.c
randomgen_LDADD = $(SOL_LIBS)

inputfilegen_SOURCES = inputfilegen.c
inputfilegen_LDADD = $(SOL_LIBS)

nettester_SOURCES = nettester.c getline.c
nettester_LDADD = $(SOL_LIBS)

# rtinit tests disabled for the moment - also questionable if they
# really provide value (after all, everything fails if rtinit fails...)
#rt_init_SOURCES = rt-init.c $(test_files)
#rt_init_CPPFLAGS =  -I$(top_srcdir) $(PTHREADS_CFLAGS) $(RSRT_CFLAGS)
#rt_init_LDADD = $(RSRT_LIBS) $(ZLIB_LIBS) $(PTHREADS_LIBS) $(SOL_LIBS)
#rt_init_LDFLAGS = -export-dynamic

# same for basic rscript tests
#rscript_SOURCES = rscript.c getline.c $(test_files)
#rscript_CPPFLAGS =  -I$(top_srcdir) $(PTHREADS_CFLAGS) $(RSRT_CFLAGS)
#rscript_LDADD = $(RSRT_LIBS) $(ZLIB_LIBS) $(PTHREADS_LIBS) $(SOL_LIBS)
#rscript_LDFLAGS = -export-dynamic
