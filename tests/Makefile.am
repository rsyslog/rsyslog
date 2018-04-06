if ENABLE_TESTBENCH
CLEANFILES = \
	IN_AUTO_DEBUG
# IN_AUTO_DEBUG should be deleted each time make check is run, but
# there exists no such hook. Se we at least delete it on make clean.


pkglib_LTLIBRARIES =

pkglib_LTLIBRARIES += liboverride_gethostname.la
liboverride_gethostname_la_SOURCES = override_gethostname.c
liboverride_gethostname_la_CFLAGS =
liboverride_gethostname_la_LDFLAGS = -avoid-version -shared

pkglib_LTLIBRARIES += liboverride_gethostname_nonfqdn.la
liboverride_gethostname_nonfqdn_la_SOURCES = override_gethostname_nonfqdn.c
liboverride_gethostname_nonfqdn_la_CFLAGS =
liboverride_gethostname_nonfqdn_la_LDFLAGS = -avoid-version -shared

pkglib_LTLIBRARIES += liboverride_getaddrinfo.la
liboverride_getaddrinfo_la_SOURCES = override_getaddrinfo.c
liboverride_getaddrinfo_la_CFLAGS =
liboverride_getaddrinfo_la_LDFLAGS = -avoid-version -shared

# TODO: reenable TESTRUNS = rt_init rscript
check_PROGRAMS = $(TESTRUNS) ourtail nettester tcpflood chkseq msleep randomgen \
	diagtalker uxsockrcvr syslog_caller inputfilegen minitcpsrv \
	omrelp_dflt_port \
	mangle_qi
if ENABLE_IMJOURNAL
check_PROGRAMS += journal_print
endif
TESTS = $(TESTRUNS)
#TESTS = $(TESTRUNS) cfg.sh

TESTS +=  \
	empty-hostname.sh

if ENABLE_TESTBENCH1
TESTS +=  \
	hostname-getaddrinfo-fail.sh \
	prop-programname.sh \
	prop-programname-with-slashes.sh \
	hostname-with-slash-pmrfc5424.sh \
	hostname-with-slash-pmrfc3164.sh \
	hostname-with-slash-dflt-invld.sh \
	hostname-with-slash-dflt-slash-valid.sh \
	stop-localvar.sh \
	stop-msgvar.sh \
	glbl-umask.sh \
	glbl-unloadmodules.sh \
	glbl-invld-param.sh \
	glbl_setenv_2_vars.sh \
	glbl_setenv_err.sh \
	glbl_setenv_err_too_long.sh \
	glbl_setenv.sh \
	nested-call-shutdown.sh \
	invalid_nested_include.sh \
	omfwd-keepalive.sh \
	omfile-read-only-errmsg.sh \
	omfile-null-filename.sh \
	omfile-whitespace-filename.sh \
	omfile-read-only.sh \
	omfile_both_files_set.sh \
	msgvar-concurrency.sh \
	localvar-concurrency.sh \
	exec_tpl-concurrency.sh \
	privdropuser.sh \
	privdropuserid.sh \
	privdropgroup.sh \
	privdropgroupid.sh \
	template-json.sh \
	template-pure-json.sh \
	template-pos-from-to.sh \
	template-pos-from-to-lowercase.sh \
	template-pos-from-to-oversize.sh \
	template-pos-from-to-oversize-lowercase.sh \
	template-pos-from-to-missing-jsonvar.sh \
	template-const-jsonf.sh \
	fac_authpriv.sh \
	fac_local0.sh \
	fac_local7.sh \
	fac_mail.sh \
	fac_news.sh \
	fac_ftp.sh \
	fac_ntp.sh \
	fac_uucp.sh \
	fac_invld1.sh \
	fac_invld2.sh \
	fac_invld3.sh \
	fac_invld4_rfc5424.sh \
	compresssp.sh \
	compresssp-stringtpl.sh \
	rawmsg-after-pri.sh \
	rfc5424parser.sh \
	pmrfc3164-msgFirstSpace.sh \
	pmrfc3164-AtSignsInHostname.sh \
	pmrfc3164-AtSignsInHostname_off.sh \
	pmrfc3164-tagEndingByColon.sh \
	pmrfc3164-defaultTag.sh \
	pmrfc3164-json.sh \
	tcp_forwarding_tpl.sh \
	tcp_forwarding_dflt_tpl.sh \
	tcp_forwarding_retries.sh \
	arrayqueue.sh \
	global_vars.sh \
	no-parser-errmsg.sh \
	da-mainmsg-q.sh \
	validation-run.sh \
	msgdup.sh \
	empty-ruleset.sh \
	imtcp-discard-truncated-msg.sh \
	imtcp-basic.sh \
	imtcp-maxFrameSize.sh \
	imtcp-msg-truncation-on-number.sh \
	imtcp-msg-truncation-on-number2.sh \
	imtcp-NUL.sh \
	imtcp-NUL-rawmsg.sh \
	imtcp-multiport.sh \
	imtcp_incomplete_frame_at_end.sh \
	daqueue-persist.sh \
	daqueue-invld-qi.sh \
	daqueue-dirty-shutdown.sh \
	diskq-rfc5424.sh \
	diskqueue.sh \
	diskqueue-fsync.sh \
	rulesetmultiqueue.sh \
	rulesetmultiqueue-v6.sh \
	manytcp.sh \
	rsf_getenv.sh \
	imtcp_conndrop.sh \
	imtcp_addtlframedelim.sh \
	imtcp_no_octet_counted.sh \
	imtcp_spframingfix.sh \
	sndrcv.sh \
	sndrcv_failover.sh \
	sndrcv_gzip.sh \
	sndrcv_udp_nonstdpt.sh \
	sndrcv_udp_nonstdpt_v6.sh \
	imudp_thread_hang.sh \
	sndrcv_udp_nonstdpt_v6.sh \
	asynwr_simple.sh \
	asynwr_simple_2.sh \
	asynwr_timeout.sh \
	asynwr_timeout_2.sh \
	asynwr_small.sh \
	asynwr_tinybuf.sh \
	wr_large_async.sh \
	wr_large_sync.sh \
	asynwr_deadlock.sh \
	asynwr_deadlock_2.sh \
	asynwr_deadlock2.sh \
	asynwr_deadlock4.sh \
	abort-uncleancfg-goodcfg.sh \
	abort-uncleancfg-goodcfg-check.sh \
	abort-uncleancfg-badcfg-check.sh \
	abort-uncleancfg-badcfg-check_1.sh \
	variable_leading_underscore.sh \
	gzipwr_rscript.sh \
	gzipwr_flushInterval.sh \
	gzipwr_flushOnTXEnd.sh \
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
	execonlywhenprevsuspended_multiwrkr.sh \
	execonlywhenprevsuspended-queue.sh \
	execonlywhenprevsuspended-nonsusp.sh \
	execonlywhenprevsuspended-nonsusp-queue.sh \
	pipe_noreader.sh \
	dircreate_dflt.sh \
	dircreate_off.sh \
	imuxsock_logger.sh \
	imuxsock_logger_ruleset.sh \
	imuxsock_logger_ruleset_ratelimit.sh \
	imuxsock_logger_err.sh \
	imuxsock_logger_parserchain.sh \
	imuxsock_traillf.sh \
	imuxsock_ccmiddle.sh \
	imuxsock_logger_syssock.sh \
	imuxsock_traillf_syssock.sh \
	imuxsock_ccmiddle_syssock.sh \
	discard-rptdmsg.sh \
	discard-allmark.sh \
	discard.sh \
	stop.sh \
	failover-async.sh \
	failover-double.sh \
	failover-basic.sh \
	failover-rptd.sh \
	failover-no-rptd.sh \
	failover-no-basic.sh \
	rcvr_fail_restore.sh
if ENABLE_LIBFAKETIME
TESTS +=  \
	now_family_utc.sh \
	now-utc.sh \
	now-utc-ymd.sh \
	now-utc-casecmp.sh \
	timegenerated-ymd.sh \
	timegenerated-uxtimestamp.sh \
	timegenerated-uxtimestamp-invld.sh \
	timegenerated-dateordinal.sh \
	timegenerated-dateordinal-invld.sh \
	timegenerated-utc.sh \
	timegenerated-utc-legacy.sh \
	timereported-utc.sh \
	timereported-utc-legacy.sh
if ENABLE_MMNORMALIZE
TESTS += \
	mmnormalize_processing_test1.sh \
	mmnormalize_processing_test2.sh \
	mmnormalize_processing_test3.sh \
	mmnormalize_processing_test4.sh
endif
endif

if ENABLE_PGSQL
if ENABLE_PGSQL_TESTS
TESTS += \
	pgsql-basic.sh \
	pgsql-basic-cnf6.sh \
	pgsql-basic-threads-cnf6.sh \
	pgsql-template.sh \
	pgsql-template-cnf6.sh \
	pgsql-actq-mt-withpause.sh \
	pgsql-template-threads-cnf6.sh
if HAVE_VALGRIND
TESTS += \
	pgsql-basic-vg.sh \
	pgsql-template-vg.sh \
	pgsql-basic-cnf6-vg.sh \
	pgsql-template-cnf6-vg.sh \
	pgsql-actq-mt-withpause-vg.sh
endif
endif
endif

if ENABLE_MYSQL_TESTS
TESTS +=  \
	mysql-basic.sh \
	mysql-basic-cnf6.sh \
	mysql-asyn.sh \
	mysql-actq-mt.sh \
	mysql-actq-mt-withpause.sh \
	action-tx-single-processing.sh \
	action-tx-errfile.sh
if HAVE_VALGRIND
TESTS +=  \
	mysql-basic-vg.sh \
	mysql-asyn-vg.sh \
	mysql-actq-mt-withpause-vg.sh
endif
if ENABLE_OMLIBDBI
TESTS +=  \
	libdbi-basic.sh \
	libdbi-asyn.sh
if HAVE_VALGRIND
TESTS +=  \
	libdbi-basic-vg.sh
endif
endif
endif

endif #  ENABLE_TESTBENCH2

if ENABLE_TESTBENCH2
TESTS +=  \
	rscript_contains.sh \
	rscript_bare_var_root.sh \
	rscript_bare_var_root-empty.sh \
	rscript_ipv42num.sh \
	rscript_field.sh \
	rscript_stop.sh \
	rscript_stop2.sh \
	rscript_prifilt.sh \
	rscript_optimizer1.sh \
	rscript_ruleset_call.sh \
	rscript_ruleset_call_indirect-basic.sh \
	rscript_ruleset_call_indirect-var.sh \
	rscript_ruleset_call_indirect-invld.sh \
	rscript_set_unset_invalid_var.sh \
	rscript_set_modify.sh \
	rscript_unaffected_reset.sh \
	rscript_replace_complex.sh \
	rscript_wrap2.sh \
	rscript_wrap3.sh \
	rscript_re_extract.sh \
	rscript_re_match.sh \
	rscript_eq.sh \
	rscript_eq_var.sh \
	rscript_ge.sh \
	rscript_ge_var.sh \
	rscript_gt.sh \
	rscript_gt_var.sh \
	rscript_le.sh \
	rscript_le_var.sh \
	rscript_lt.sh \
	rscript_lt_var.sh \
	rscript_ne.sh \
	rscript_ne_var.sh \
	rscript_num2ipv4.sh \
	rscript_int2Hex.sh \
	rscript_trim.sh \
	rscript_substring.sh \
	rscript_format_time.sh \
	rscript_parse_time.sh \
	rscript_is_time.sh \
	rscript_script_error.sh \
	rscript_parse_json.sh \
	rscript_previous_action_suspended.sh \
	rscript_str2num_negative.sh \
	rscript-config_enable-on.sh \
	rs-cnum.sh \
	rs-substring.sh \
	rs-int2hex.sh \
	mmanon_random_32_ipv4.sh \
	mmanon_random_cons_32_ipv4.sh \
	mmanon_recognize_ipv4.sh \
	mmanon_zero_12_ipv4.sh \
	mmanon_zero_33_ipv4.sh \
	mmanon_zero_8_ipv4.sh \
	mmanon_simple_12_ipv4.sh \
	mmanon_simple_33_ipv4.sh \
	mmanon_simple_8_ipv4.sh \
	mmanon_random_128_ipv6.sh \
	mmanon_zero_128_ipv6.sh \
	mmanon_zero_96_ipv6.sh \
	mmanon_random_cons_128_ipv6.sh \
	mmanon_zero_50_ipv6.sh \
	mmanon_recognize_ipv6.sh \
	mmanon_zero_64_ipv6.sh \
	mmanon_both_modes_compatible.sh \
	mmanon_recognize_ipembedded.sh \
	mmanon_random_cons_128_ipembedded.sh \
	empty-prop-comparison.sh \
	rs_optimizer_pri.sh \
	cee_simple.sh \
	cee_diskqueue.sh \
	incltest.sh \
	incltest_dir.sh \
	incltest_dir_wildcard.sh \
	incltest_dir_empty_wildcard.sh \
	linkedlistqueue.sh \
	lookup_table.sh \
	lookup_table_no_hup_reload.sh \
	key_dereference_on_uninitialized_variable_space.sh \
	array_lookup_table.sh \
	sparse_array_lookup_table.sh \
	lookup_table_bad_configs.sh \
	lookup_table_rscript_reload.sh \
	lookup_table_rscript_reload_without_stub.sh \
	include-obj-text-from-file.sh \
	multiple_lookup_tables.sh
if ENABLE_FMHTTP
TESTS +=  \
	rscript_http_request.sh
endif # ENABLE_FMHTTP
if HAVE_VALGRIND
TESTS +=  \
	include-obj-outside-control-flow-vg.sh \
	include-obj-in-if-vg.sh \
	include-obj-text-vg.sh \
	rscript_parse_json-vg.sh \
	rscript_backticks-vg.sh \
	rscript-config_enable-off-vg.sh \
	prop-jsonmesg-vg.sh
endif # HAVE_VALGRIND

endif # ENABLE_TESTBENCH2

if HAVE_VALGRIND
TESTS +=  \
	mmexternal-SegFault-vg.sh \
	mmexternal-InvldProg-vg.sh \
	internal-errmsg-memleak-vg.sh \
	rscript_set_memleak-vg.sh \
	no-parser-vg.sh \
	discard-rptdmsg-vg.sh \
	discard-allmark-vg.sh \
	failover-basic-vg.sh \
	failover-rptd-vg.sh \
	failover-no-basic-vg.sh \
	failover-no-rptd-vg.sh \
	timereported-utc-vg.sh \
	udp-msgreduc-vg.sh \
	udp-msgreduc-orgmsg-vg.sh \
	tcp-msgreduc-vg.sh \
	rscript_field-vg.sh \
	unused_lookup_table-vg.sh \
	lookup_table-vg.sh \
	lookup_table_no_hup_reload-vg.sh \
	array_lookup_table-vg.sh \
	array_lookup_table_misuse-vg.sh \
	sparse_array_lookup_table-vg.sh \
	lookup_table_bad_configs-vg.sh \
	lookup_table_rscript_reload-vg.sh \
	lookup_table_rscript_reload_without_stub-vg.sh \
	multiple_lookup_tables-vg.sh \
	fac_local0-vg.sh \
	rscript_trim-vg.sh
if ENABLE_FMHTTP
TESTS +=  \
	rscript_http_request-vg.sh
endif # ENABLE_FMHTTP
endif # HAVE_VALGRIND

if ENABLE_ROOT_TESTS
TESTS +=  \
	sndrcv_udp.sh \
	imuxsock_logger_root.sh \
	imuxsock_traillf_root.sh \
	imuxsock_ccmiddle_root.sh \
	imklog_permitnonkernelfacility_root.sh
if ENABLE_IP
TESTS += tcp_forwarding_ns_tpl.sh
endif
if ENABLE_RELP
TESTS += sndrcv_relp_dflt_pt.sh
endif
if HAVE_VALGRIND
TESTS +=  \
	mmexternal-SegFault-empty-jroot-vg.sh
endif
endif

# disable temporarily due to issues
# see: https://github.com/rsyslog/rsyslog/issues/2564
#if ENABLE_IMJOURNAL
#TESTS +=  \
#	imjournal-basic.sh
#if HAVE_VALGRIND
#TESTS +=  \
#	imjournal-basic-vg.sh
#endif
#endif

if ENABLE_OMJOURNAL
TESTS +=  \
	omjournal-abort-template.sh \
	omjournal-abort-no-template.sh \
	omjournal-basic-template.sh \
	omjournal-basic-no-template.sh
endif

if ENABLE_OMPROG
# see https://github.com/rsyslog/rsyslog/issues/2403
# TODO: re-enable!
#	omprog-cleanup.sh
#	omprog-noterm-cleanup.sh
#	omprog-noterm-default.sh
#	omprog-cleanup-with-outfile.sh
# linux:
#	omprog-noterm-unresponsive.sh
# valgrind:
#	omprog-noterm-cleanup-vg.sh
#	omprog-cleanup-vg.sh
TESTS +=  \
	omprog-feedback.sh \
	omprog-transactions.sh \
	omprog-transactions-failed-messages.sh \
	omprog-transactions-failed-commits.sh
#if OS_LINUX
#TESTS += \
#	omprog-cleanup-when-unresponsive.sh
#endif
if HAVE_VALGRIND
TESTS +=  \
	omprog-multiparam-vg.sh
#if OS_LINUX
#TESTS += \
	#omprog-cleanup-when-unresponsive-vg.sh
#endif
endif
endif

if ENABLE_OMKAFKA
if ENABLE_IMKAFKA
if ENABLE_KAFKA_TESTS
TESTS += \
	sndrcv_kafka.sh
# Tests below need to be stable first!
#	sndrcv_kafka_multi.sh
#	sndrcv_kafka_fail.sh
#	sndrcv_kafka_failresume.sh
if HAVE_VALGRIND
TESTS += \
	sndrcv_kafka-vg-rcvr.sh
# re-enable: (see https://github.com/rsyslog/rsyslog/issues/2434)
# sndrcv_kafka-vg-sender.sh
endif
endif
endif
endif
if ENABLE_IMPSTATS
TESTS +=  \
	impstats-hup.sh \
	dynstats.sh \
	dynstats_overflow.sh \
	dynstats_reset.sh \
	dynstats_ctr_reset.sh \
	dynstats_nometric.sh \
	no-dynstats-json.sh \
	no-dynstats.sh \
	stats-json.sh \
	dynstats-json.sh \
	stats-cee.sh \
	stats-json-es.sh \
	dynstats_reset_without_pstats_reset.sh \
	dynstats_prevent_premature_eviction.sh
if HAVE_VALGRIND
TESTS +=  \
	dynstats-vg.sh \
	dynstats_reset-vg.sh \
	dynstats_overflow-vg.sh \
	dynstats-json-vg.sh \
	stats-json-vg.sh \
	stats-cee-vg.sh \
	dynstats_prevent_premature_eviction-vg.sh
endif
endif

if ENABLE_IMPTCP
# note that some tests simply USE imptcp, but they also
# need to be disabled if we do not have this module
TESTS +=  \
	manyptcp.sh \
	imptcp_large.sh \
	imptcp-connection-msg-disabled.sh \
	imptcp-connection-msg-received.sh \
	imptcp-discard-truncated-msg.sh \
	imptcp_addtlframedelim.sh \
	imptcp_conndrop.sh \
	imptcp_no_octet_counted.sh \
	imptcp_multi_line.sh \
	imptcp_spframingfix.sh \
	imptcp_nonProcessingPoller.sh \
	imptcp_veryLargeOctateCountedMessages.sh \
	imptcp-NUL.sh \
	imptcp-NUL-rawmsg.sh \
	rscript_random.sh \
	rscript_replace.sh
if HAVE_VALGRIND
TESTS +=  \
	imptcp_conndrop-vg.sh
endif
endif

if ENABLE_ELASTICSEARCH_TESTS
TESTS +=  \
	es-basic.sh \
	es-basic-es6.0.sh \
	es-basic-server.sh \
	es-basic-ha.sh \
	es-basic-bulk.sh \
	es-maxbytes-bulk.sh \
	es-basic-errfile-empty.sh \
	es-basic-errfile-popul.sh \
	es-bulk-errfile-empty.sh \
	es-bulk-errfile-popul.sh
if HAVE_VALGRIND
TESTS += \
	es-basic-vgthread.sh
endif
if ENABLE_IMFILE
TESTS +=  \
	es-bulk-errfile-popul-def-format.sh \
	es-bulk-errfile-popul-erronly.sh \
	es-bulk-errfile-popul-erronly-interleaved.sh \
	es-bulk-errfile-popul-def-interleaved.sh
endif
if HAVE_VALGRIND
TESTS +=  \
	es-basic-vg.sh \
	es-basic-bulk-vg.sh \
	es-basic-ha-vg.sh
endif
endif

if ENABLE_MMPSTRUCDATA
TESTS +=  \
	mmpstrucdata.sh \
	mmpstrucdata-case.sh
if HAVE_VALGRIND
TESTS +=  \
	mmpstrucdata-vg.sh \
	mmpstrucdata-invalid-vg.sh
endif
endif

if ENABLE_MMRM1STSPACE
TESTS +=  \
	mmrm1stspace-basic.sh
endif

if ENABLE_PMNULL
TESTS +=  \
	pmnull-basic.sh \
	pmnull-withparams.sh
endif

if ENABLE_OMSTDOUT
TESTS +=  \
	omstdout-basic.sh
endif

if ENABLE_PMNORMALIZE
TESTS +=  \
	pmnormalize-basic.sh \
	pmnormalize-rule.sh
if HAVE_VALGRIND
TESTS += \
	pmnormalize-rule-vg.sh
endif
endif

if ENABLE_MMNORMALIZE
TESTS += msgvar-concurrency-array.sh \
	msgvar-concurrency-array-event.tags.sh \
	mmnormalize_rule_from_string.sh \
	mmnormalize_rule_from_array.sh

if ENABLE_IMPTCP
TESTS +=  \
	mmnormalize_regex_defaulted.sh \
	mmnormalize_regex_disabled.sh \
	mmnormalize_variable.sh \
	mmnormalize_tokenized.sh
endif

if LOGNORM_REGEX_SUPPORTED
TESTS += \
	mmnormalize_regex.sh
endif
endif

if ENABLE_MMJSONPARSE
TESTS += \
	mmjsonparse-w-o-cookie.sh \
	mmjsonparse-w-o-cookie-multi-spaces.sh
if ENABLE_IMPTCP
TESTS +=  \
	mmjsonparse_simple.sh \
	imptcp-oversize-message-display.sh \
	imptcp-msg-truncation-on-number.sh \
	imptcp-msg-truncation-on-number2.sh \
	imptcp-maxFrameSize-parameter.sh \
	mmjsonparse_cim.sh \
	json_array_subscripting.sh \
	json_array_looping.sh \
	json_object_looping.sh \
	json_nonarray_looping.sh
endif
if HAVE_VALGRIND
TESTS +=  \
	json_null_array-vg.sh \
	json_object_looping-vg.sh \
	json_array_looping-vg.sh \
	json_object_suicide_in_loop-vg.sh \
	json_null-vg.sh
endif
TESTS +=  \
	stop_when_array_has_element.sh \
	json_null_array.sh \
	json_null.sh \
	json_var_cmpr.sh \
	json_var_case.sh
endif

if ENABLE_MMDBLOOKUP
TESTS += \
    mmdb.sh \
    mmdb-container.sh \
    mmdb-container-empty.sh
if HAVE_VALGRIND
TESTS += \
    mmdb-vg.sh \
    mmdb-multilevel-vg.sh
endif
endif

if ENABLE_GNUTLS
TESTS +=  \
	imtcp_conndrop_tls.sh \
	sndrcv_tls_anon_hostname.sh \
	sndrcv_tls_anon_ipv4.sh \
	sndrcv_tls_priorityString.sh \
	imtcp-tls-basic.sh \
	sndrcv_tls_anon_rebind.sh
if HAVE_VALGRIND
TESTS += \
	imtcp-tls-basic-vg.sh \
	imtcp_conndrop_tls-vg.sh
if !ENABLE_DISTCHECK_WORKAROUND
TESTS += \
	manytcp-too-few-tls-vg.sh
endif
endif
endif

if ENABLE_OMUXSOCK
TESTS += uxsock_simple.sh
endif

if ENABLE_RELP
TESTS += sndrcv_relp.sh \
	 sndrcv_relp_rebind.sh \
	 imrelp-basic.sh \
	 imrelp-manyconn.sh
if ENABLE_GNUTLS
TESTS += \
         sndrcv_relp_tls.sh \
         relp_tls_certificate_not_found.sh
endif
endif

if ENABLE_OMUDPSPOOF
TESTS += \
	sndrcv_omudpspoof.sh \
	sndrcv_omudpspoof_nonstdpt.sh
endif

if ENABLE_OMSTDOUT
TESTS += \
	omod-if-array.sh \
	threadingmq.sh \
	threadingmqaq.sh \
	badqi.sh
if ENABLE_IMPTCP
TESTS += \
	tabescape_dflt.sh \
	tabescape_off.sh \
	timestamp.sh \
	inputname.sh \
	proprepltest.sh \
	parsertest.sh \
	fieldtest.sh
endif
endif

if ENABLE_OMRULESET
TESTS += \
	omruleset.sh \
	omruleset-queue.sh
endif

if ENABLE_PMSNARE
TESTS += \
	pmsnare.sh
endif

if ENABLE_EXTENDED_TESTS
# random.sh is temporarily disabled as it needs some work
# to rsyslog core to complete in reasonable time
#TESTS += random.sh
endif

if ENABLE_IMFILE
TESTS += \
	imfile-basic.sh \
	imfile-discard-truncated-line.sh \
	imfile-truncate-line.sh \
	imfile-file-not-found-error.sh \
	imfile-fileNotFoundError-parameter.sh \
	imfile-error-not-repeated.sh \
	imfile-truncate.sh \
	imfile-readmode2.sh \
	imfile-readmode2-polling.sh \
	imfile-readmode2-with-persists-data-during-stop.sh \
	imfile-readmode2-with-persists.sh \
	imfile-endregex.sh \
	imfile-endregex-save-lf.sh \
	imfile-endregex-save-lf-persist.sh \
	imfile-endregex-timeout-none-polling.sh \
	imfile-endregex-timeout-polling.sh \
	imfile-endregex-timeout.sh \
	imfile-endregex-timeout-none.sh \
	imfile-endregex-timeout-with-shutdown.sh \
	imfile-endregex-timeout-with-shutdown-polling.sh \
	imfile-persist-state-1.sh \
	imfile-wildcards.sh \
	imfile-wildcards-dirs.sh \
	imfile-wildcards-dirs2.sh \
	imfile-wildcards-dirs-multi.sh \
	imfile-wildcards-dirs-multi2.sh \
	imfile-wildcards-dirs-multi3.sh \
	imfile-wildcards-dirs-multi4.sh \
	imfile-wildcards-dirs-multi5.sh \
	imfile-wildcards-dirs-multi5-polling.sh \
	imfile-old-state-file.sh \
	imfile-rename-while-stopped.sh \
	imfile-rename.sh

if HAVE_VALGRIND
TESTS += \
	imfile-basic-vg.sh \
	imfile-endregex-vg.sh \
	imfile-readmode0-vg.sh \
	imfile-readmode2-vg.sh \
	imfile-basic-vgthread.sh
endif
endif

if ENABLE_OMTCL
TESTS += \
	 omtcl.sh
endif

endif # if ENABLE_TESTBENCH

TESTS_ENVIRONMENT = RSYSLOG_MODDIR='$(abs_top_builddir)'/runtime/.libs/
DISTCLEANFILES=rsyslog.pid
test_files = testbench.h runtime-dummy.c

EXTRA_DIST= \
	internal-errmsg-memleak-vg.sh \
	empty-hostname.sh \
	hostname-getaddrinfo-fail.sh \
	hostname-with-slash-pmrfc5424.sh \
	hostname-with-slash-pmrfc3164.sh \
	pmrfc3164-msgFirstSpace.sh \
	pmrfc3164-AtSignsInHostname.sh \
	pmrfc3164-AtSignsInHostname_off.sh \
	pmrfc3164-tagEndingByColon.sh \
	pmrfc3164-defaultTag.sh \
	pmrfc3164-json.sh \
	hostname-with-slash-dflt-invld.sh \
	hostname-with-slash-dflt-slash-valid.sh \
	glbl-umask.sh \
	glbl-unloadmodules.sh \
	glbl-invld-param.sh \
	glbl_setenv_2_vars.sh \
	glbl_setenv_err.sh \
	glbl_setenv_err_too_long.sh \
	glbl_setenv.sh \
	mmexternal-SegFault-vg.sh \
	mmexternal-SegFault-empty-jroot-vg.sh \
	testsuites/mmexternal-SegFault-mm-python.py \
	mmexternal-InvldProg-vg.sh \
	nested-call-shutdown.sh \
	1.rstest 2.rstest 3.rstest err1.rstest \
	invalid_nested_include.sh \
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
	tcp_forwarding_tpl.sh \
	tcp_forwarding_ns_tpl.sh \
	testsuites/tcp_forwarding_tpl.conf \
	testsuites/tcp_forwarding_ns_tpl.conf \
	tcp_forwarding_dflt_tpl.sh \
	testsuites/tcp_forwarding_dflt_tpl.conf \
	tcp_forwarding_retries.sh \
	killrsyslog.sh \
	parsertest.sh \
	fieldtest.sh \
	rsf_getenv.sh \
	testsuites/rsf_getenv.conf \
	diskq-rfc5424.sh \
	diskqueue.sh \
	testsuites/diskqueue.conf \
	arrayqueue.sh \
	testsuites/arrayqueue.conf \
	include-obj-text-from-file.sh \
	include-obj-outside-control-flow-vg.sh \
	include-obj-in-if-vg.sh \
	include-obj-text-vg.sh \
	rscript-config_enable-off-vg.sh \
	rscript-config_enable-on.sh \
	rscript_http_request.sh \
	rscript_http_request-vg.sh \
	rscript_bare_var_root.sh \
	rscript_bare_var_root-empty.sh \
	rscript_contains.sh \
	testsuites/rscript_contains.conf \
	rscript_ipv42num.sh \
	rscript_field.sh \
	rscript_field-vg.sh \
	testsuites/rscript_field.conf \
	rscript_stop.sh \
	testsuites/rscript_stop.conf \
	rscript_stop2.sh \
	testsuites/rscript_stop2.conf \
	stop.sh \
	testsuites/stop.conf \
	rscript_le.sh \
	testsuites/rscript_le.conf \
	rscript_le_var.sh \
	testsuites/rscript_le_var.conf \
	rscript_ge.sh \
	testsuites/rscript_ge.conf \
	rscript_ge_var.sh \
	testsuites/rscript_ge_var.conf \
	rscript_lt.sh \
	testsuites/rscript_lt.conf \
	rscript_lt_var.sh \
	testsuites/rscript_lt_var.conf \
	rscript_gt.sh \
	testsuites/rscript_gt.conf \
	rscript_gt_var.sh \
	testsuites/rscript_gt_var.conf \
	rscript_ne.sh \
	testsuites/rscript_ne.conf \
	rscript_ne_var.sh \
	rscript_num2ipv4.sh \
	rscript_int2Hex.sh \
	rscript_trim.sh \
	rscript_substring.sh \
	rscript_format_time.sh \
	rscript_parse_time.sh \
	rscript_parse_time_get-ts.py \
	rscript_is_time.sh \
	rscript_script_error.sh \
	rscript_parse_json.sh \
	rscript_parse_json-vg.sh \
	rscript_backticks-vg.sh \
	rscript_previous_action_suspended.sh \
	rscript_str2num_negative.sh \
	rs-cnum.sh \
	rs-int2hex.sh \
	rs-substring.sh \
	mmanon_random_32_ipv4.sh \
	mmanon_random_cons_32_ipv4.sh \
	mmanon_recognize_ipv4.sh \
	mmanon_zero_12_ipv4.sh \
	mmanon_zero_33_ipv4.sh \
	mmanon_zero_8_ipv4.sh \
	mmanon_simple_12_ipv4.sh \
	mmanon_simple_33_ipv4.sh \
	mmanon_simple_8_ipv4.sh \
	mmanon_random_128_ipv6.sh \
	mmanon_zero_128_ipv6.sh \
	mmanon_zero_96_ipv6.sh \
	mmanon_random_cons_128_ipv6.sh \
	mmanon_zero_50_ipv6.sh \
	mmanon_recognize_ipv6.sh \
	mmanon_zero_64_ipv6.sh \
	mmanon_both_modes_compatible.sh \
	mmanon_recognize_ipembedded.sh \
	mmanon_random_cons_128_ipembedded.sh \
	testsuites/rscript_ne_var.conf \
	rscript_eq.sh \
	testsuites/rscript_eq.conf \
	rscript_eq_var.sh \
	testsuites/rscript_eq_var.conf \
	rscript_set_memleak-vg.sh \
	rscript_set_unset_invalid_var.sh \
	rscript_set_modify.sh \
	testsuites/rscript_set_modify.conf \
	testsuites/rscript_unaffected_reset.conf \
	stop-localvar.sh \
	testsuites/stop-localvar.conf \
	stop-msgvar.sh \
	testsuites/stop-msgvar.conf \
	omfwd-keepalive.sh \
	omfile-read-only-errmsg.sh \
	omfile-null-filename.sh \
	omfile-whitespace-filename.sh \
	omfile-read-only.sh \
	omfile_both_files_set.sh \
	msgvar-concurrency.sh \
	testsuites/msgvar-concurrency.conf \
	msgvar-concurrency-array.sh \
	testsuites/msgvar-concurrency-array.conf \
	testsuites/msgvar-concurrency-array.rulebase \
	msgvar-concurrency-array-event.tags.sh \
	testsuites/msgvar-concurrency-array-event.tags.conf \
	testsuites/msgvar-concurrency-array-event.tags.rulebase \
	localvar-concurrency.sh \
	testsuites/localvar-concurrency.conf \
	exec_tpl-concurrency.sh \
	testsuites/exec_tpl-concurrency.conf \
	prop-jsonmesg-vg.sh \
	prop-all-json-concurrency.sh \
	testsuites/prop-all-json-concurrency.conf \
	no-parser-errmsg.sh \
	global_vars.sh \
	testsuites/global_vars.conf \
	no-parser-errmsg.sh \
	no-parser-vg.sh \
	prop-programname.sh \
	prop-programname-with-slashes.sh \
	rfc5424parser.sh \
	testsuites/rfc5424parser.conf \
	privdrop_common.sh \
	privdropuser.sh \
	privdropuserid.sh \
	privdropgroup.sh \
	privdropgroupid.sh \
	template-json.sh \
	template-pure-json.sh \
	template-pos-from-to.sh \
	template-pos-from-to-lowercase.sh \
	template-pos-from-to-oversize.sh \
	template-pos-from-to-oversize-lowercase.sh \
	template-pos-from-to-missing-jsonvar.sh \
	template-const-jsonf.sh \
	fac_authpriv.sh \
	testsuites/fac_authpriv.conf \
	fac_local0.sh \
	fac_local0-vg.sh \
	testsuites/fac_local0.conf \
	fac_local7.sh \
	testsuites/fac_local7.conf \
	fac_mail.sh \
	testsuites/fac_mail.conf \
	fac_news.sh \
	testsuites/fac_news.conf \
	fac_ftp.sh \
	testsuites/fac_ftp.conf \
	fac_ntp.sh \
	testsuites/fac_ntp.conf \
	fac_uucp.sh \
	testsuites/fac_uucp.conf \
	fac_invld1.sh \
	testsuites/fac_invld1.conf \
	fac_invld2.sh \
	testsuites/fac_invld2.conf \
	fac_invld3.sh \
	testsuites/fac_invld3.conf \
	fac_invld4_rfc5424.sh \
	testsuites/fac_invld4_rfc5424.conf \
	compresssp.sh \
	compresssp-stringtpl.sh \
	now_family_utc.sh \
	testsuites/now_family_utc.conf \
	now-utc-ymd.sh \
	now-utc-casecmp.sh \
	now-utc.sh \
	testsuites/now-utc.conf \
	faketime_common.sh \
	imjournal-basic.sh \
	imjournal-basic-vg.sh \
	omjournal-abort-template.sh \
	omjournal-abort-no-template.sh \
	omjournal-basic-template.sh \
	omjournal-basic-no-template.sh \
	timegenerated-ymd.sh \
	timegenerated-uxtimestamp.sh \
	timegenerated-uxtimestamp-invld.sh \
	timegenerated-dateordinal.sh \
	timegenerated-dateordinal-invld.sh \
	timegenerated-utc.sh \
	timegenerated-utc-legacy.sh \
	timereported-utc.sh \
	timereported-utc-legacy.sh \
	timereported-utc-vg.sh \
	mmrm1stspace-basic.sh \
	mmnormalize_rule_from_string.sh \
	mmnormalize_rule_from_array.sh \
	pmnull-basic.sh \
	pmnull-withparams.sh \
	omstdout-basic.sh \
	testsuites/mmnormalize_processing_tests.rulebase \
	mmnormalize_processing_test1.sh \
	mmnormalize_processing_test2.sh \
	mmnormalize_processing_test3.sh \
	mmnormalize_processing_test4.sh \
	pmnormalize-rule.sh \
	pmnormalize-rule-vg.sh\
	testsuites/pmnormalize-rule-vg.conf \
	testsuites/pmnormalize_basic.rulebase \
	pmnormalize-basic.sh \
	rawmsg-after-pri.sh \
	testsuites/rawmsg-after-pri.conf \
	rs_optimizer_pri.sh \
	testsuites/rs_optimizer_pri.conf \
	rscript_prifilt.sh \
	testsuites/rscript_prifilt.conf \
	rscript_optimizer1.sh \
	testsuites/rscript_optimizer1.conf \
	rscript_ruleset_call.sh \
	testsuites/rscript_ruleset_call.conf \
	rscript_ruleset_call_indirect-basic.sh \
	rscript_ruleset_call_indirect-var.sh \
	rscript_ruleset_call_indirect-invld.sh \
	cee_simple.sh \
	testsuites/cee_simple.conf \
	cee_diskqueue.sh \
	testsuites/cee_diskqueue.conf \
	mmjsonparse-w-o-cookie.sh \
	mmjsonparse-w-o-cookie-multi-spaces.sh \
	mmjsonparse_simple.sh \
	imptcp-oversize-message-display.sh \
	imptcp-msg-truncation-on-number.sh \
	imptcp-msg-truncation-on-number2.sh \
	imptcp-maxFrameSize-parameter.sh \
	testsuites/mmjsonparse_simple.conf \
	mmjsonparse_cim.sh \
	testsuites/mmjsonparse_cim.conf \
	mmdb.sh \
	mmdb.rb \
	test.mmdb \
	mmdb-vg.sh \
	mmdb-container.sh \
	mmdb-container-empty.sh \
	mmdb-multilevel-vg.sh \
	incltest.sh \
	testsuites/incltest.conf \
	incltest_dir.sh \
	testsuites/incltest_dir.conf \
	incltest_dir_empty_wildcard.sh \
	testsuites/incltest_dir_empty_wildcard.conf \
	incltest_dir_wildcard.sh \
	testsuites/incltest_dir_wildcard.conf \
	testsuites/incltest.d/include.conf \
	es-basic-es6.0.sh \
	es-basic.sh \
	es-basic-vgthread.sh \
	testsuites/es-basic.conf \
	es-basic-bulk.sh \
	testsuites/es-basic-bulk.conf \
	es-basic-errfile-empty.sh \
	testsuites/es-basic-errfile-empty.conf \
	es-basic-errfile-popul.sh \
	testsuites/es-basic-errfile-popul.conf \
	es-bulk-errfile-empty.sh \
	testsuites/es-bulk-errfile-empty.conf \
	es-bulk-errfile-popul.sh \
	testsuites/es-bulk-errfile-popul.conf \
	es-bulk-errfile-popul-def-format.sh \
	testsuites/es-bulk-errfile-popul-def-format.conf \
	es-bulk-errfile-popul-erronly.sh \
	testsuites/es-bulk-errfile-popul-erronly.conf \
	es-bulk-errfile-popul-erronly-interleaved.sh \
	testsuites/es-bulk-errfile-popul-erronly-interleaved.conf \
	es-bulk-errfile-popul-def-interleaved.sh \
	testsuites/es-bulk-errfile-popul-def-interleaved.conf \
	es-basic-vg.sh \
	es-basic-bulk-vg.sh \
	es-basic-ha-vg.sh \
	linkedlistqueue.sh \
	testsuites/linkedlistqueue.conf \
	da-mainmsg-q.sh \
	testsuites/da-mainmsg-q.conf \
	diskqueue-fsync.sh \
	testsuites/diskqueue-fsync.conf \
	msgdup.sh \
	empty-ruleset.sh \
	testsuites/empty-ruleset.conf \
	imtcp-discard-truncated-msg.sh \
	imtcp-basic.sh \
	imtcp-maxFrameSize.sh \
	imtcp-msg-truncation-on-number.sh \
	imtcp-msg-truncation-on-number2.sh \
	imtcp-NUL.sh \
	imtcp-NUL-rawmsg.sh \
	imtcp-tls-basic.sh \
	imtcp-tls-basic-vg.sh \
	testsuites/imtcp-tls-basic.conf \
	imtcp_incomplete_frame_at_end.sh \
	imtcp-multiport.sh \
	testsuites/imtcp-multiport.conf \
	udp-msgreduc-orgmsg-vg.sh \
	testsuites/udp-msgreduc-orgmsg-vg.conf \
	udp-msgreduc-vg.sh \
	testsuites/udp-msgreduc-vg.conf \
	manytcp-too-few-tls-vg.sh \
	testsuites/manytcp-too-few-tls.conf \
	manytcp.sh \
	testsuites/manytcp.conf \
	manyptcp.sh \
	testsuites/manyptcp.conf \
	imptcp-NUL.sh \
	imptcp-NUL-rawmsg.sh \
	imptcp_large.sh \
	imptcp-connection-msg-disabled.sh \
	imptcp-connection-msg-received.sh \
	imptcp-discard-truncated-msg.sh \
	testsuites/imptcp_large.conf \
	imptcp_addtlframedelim.sh \
	testsuites/imptcp_addtlframedelim.conf \
	imptcp_conndrop-vg.sh \
	imptcp_conndrop.sh \
	testsuites/imptcp_conndrop.conf \
	imptcp_multi_line.sh \
	testsuites/imptcp_multi_line.testdata \
	imptcp_no_octet_counted.sh \
	testsuites/imptcp_no_octet_counted.conf \
	testsuites/no_octet_counted.testdata \
	imtcp_no_octet_counted.sh \
	testsuites/imtcp_no_octet_counted.conf \
	testsuites/spframingfix.testdata \
	imtcp_spframingfix.sh \
	testsuites/imtcp_spframingfix.conf \
	imptcp_spframingfix.sh \
	testsuites/imptcp_spframingfix.conf \
	imtcp_conndrop.sh \
	testsuites/imtcp_conndrop.conf \
	imtcp_conndrop_tls.sh \
	testsuites/imtcp_conndrop_tls.conf \
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
	daqueue-dirty-shutdown.sh \
	daqueue-invld-qi.sh \
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
	imrelp-basic.sh \
	imrelp-manyconn.sh \
	sndrcv_relp.sh \
	testsuites/sndrcv_relp_sender.conf \
	testsuites/sndrcv_relp_rcvr.conf \
	sndrcv_relp_rebind.sh \
	testsuites/sndrcv_relp_rebind_sender.conf \
	testsuites/sndrcv_relp_rebind_rcvr.conf \
	sndrcv_relp_tls.sh \
	testsuites/sndrcv_relp_tls_sender.conf \
	testsuites/sndrcv_relp_tls_rcvr.conf \
	relp_tls_certificate_not_found.sh \
	sndrcv_relp_dflt_pt.sh \
	testsuites/sndrcv_relp_dflt_pt_rcvr.conf \
	testsuites/sndrcv_relp_dflt_pt_sender.conf \
	sndrcv_udp.sh \
	testsuites/sndrcv_udp_sender.conf \
	testsuites/sndrcv_udp_rcvr.conf \
	imudp_thread_hang.sh \
	testsuites/imudp_thread_hang.conf \
	sndrcv_udp_nonstdpt.sh \
	testsuites/sndrcv_udp_nonstdpt_sender.conf \
	testsuites/sndrcv_udp_nonstdpt_rcvr.conf \
	sndrcv_udp_nonstdpt_v6.sh \
	testsuites/sndrcv_udp_nonstdpt_v6_sender.conf \
	testsuites/sndrcv_udp_nonstdpt_v6_rcvr.conf \
	sndrcv_omudpspoof.sh \
	testsuites/sndrcv_omudpspoof_sender.conf \
	testsuites/sndrcv_omudpspoof_rcvr.conf \
	sndrcv_omudpspoof_nonstdpt.sh \
	testsuites/sndrcv_omudpspoof_nonstdpt_sender.conf \
	testsuites/sndrcv_omudpspoof_nonstdpt_rcvr.conf \
	sndrcv_gzip.sh \
	testsuites/sndrcv_gzip_sender.conf \
	testsuites/sndrcv_gzip_rcvr.conf \
	action-tx-single-processing.sh \
	action-tx-errfile.sh \
	testsuites/action-tx-errfile.result \
	pipeaction.sh \
	testsuites/pipeaction.conf \
	omprog-cleanup.sh \
	omprog-cleanup-vg.sh \
	omprog-cleanup-with-outfile.sh \
	omprog-cleanup-when-unresponsive.sh \
	omprog-cleanup-when-unresponsive-vg.sh \
	omprog-noterm-cleanup.sh \
	omprog-noterm-cleanup-vg.sh \
	omprog-noterm-default.sh \
	omprog-noterm-unresponsive.sh \
	omprog-feedback.sh \
	omprog-transactions.sh \
	omprog-transactions-failed-messages.sh \
	omprog-transactions-failed-commits.sh \
	omprog-multiparam-vg.sh \
	testsuites/omprog-cleanup.conf \
	testsuites/omprog-noterm.conf \
	testsuites/omprog-noterm-default.conf \
	testsuites/omprog-noterm-unresponsive.conf \
	testsuites/omprog-noterm.sh \
	testsuites/omprog-cleanup-outfile.conf \
	testsuites/omprog-cleanup-unresponsive.conf \
	testsuites/omprog-test-bin.sh \
	testsuites/term-ignoring-script.sh \
	testsuites/omprog-feedback.conf \
	testsuites/omprog-feedback-bin.sh \
	testsuites/omprog-transactions-bin.sh \
	testsuites/omprog-transactions.conf \
	testsuites/omprog-transactions-failed-messages.conf \
	testsuites/omprog-transactions-failed-commits.conf \
	pipe_noreader.sh \
	testsuites/pipe_noreader.conf \
	uxsock_simple.sh \
	testsuites/uxsock_simple.conf \
	asynwr_simple.sh \
	testsuites/asynwr_simple.conf \
	asynwr_simple_2.sh \
	testsuites/asynwr_simple_2.conf \
	asynwr_timeout.sh \
	testsuites/asynwr_timeout.conf \
	asynwr_timeout_2.sh \
	testsuites/asynwr_timeout_2.conf \
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
	asynwr_deadlock_2.sh \
	testsuites/asynwr_deadlock_2.conf \
	asynwr_deadlock2.sh \
	testsuites/asynwr_deadlock2.conf \
	asynwr_deadlock4.sh \
	testsuites/asynwr_deadlock4.conf \
	abort-uncleancfg-goodcfg.sh \
	testsuites/abort-uncleancfg-goodcfg.conf \
	abort-uncleancfg-goodcfg-check.sh \
	testsuites/abort-uncleancfg-goodcfg.conf \
	abort-uncleancfg-badcfg-check.sh \
	testsuites/abort-uncleancfg-badcfg.conf \
	abort-uncleancfg-badcfg-check_1.sh \
	testsuites/abort-uncleancfg-badcfg_1.conf \
	variable_leading_underscore.sh \
	testsuites/variable_leading_underscore.conf \
	gzipwr_rscript.sh \
	gzipwr_flushInterval.sh \
	gzipwr_flushOnTXEnd.sh \
	gzipwr_large.sh \
	testsuites/gzipwr_large.conf \
	gzipwr_large_dynfile.sh \
	testsuites/gzipwr_large_dynfile.conf \
	complex1.sh \
	testsuites/complex1.conf \
	random.sh \
	testsuites/random.conf \
	testsuites/imfile-old-state-file_imfile-state_.-rsyslog.input \
	imfile-readmode0-vg.sh \
	imfile-readmode2.sh \
	imfile-readmode2-polling.sh \
	imfile-readmode2-vg.sh \
	imfile-readmode2-with-persists-data-during-stop.sh \
	imfile-readmode2-with-persists.sh \
	testsuites/imfile-readmode2.conf \
	testsuites/imfile-readmode2-with-persists.conf \
	testsuites/imfile-readmode2-with-persists-data-during-stop.conf \
	imfile-endregex-save-lf.sh \
	imfile-endregex-save-lf-persist.sh \
	imfile-endregex.sh \
	imfile-endregex-vg.sh \
	testsuites/imfile-endregex.conf \
	imfile-basic.sh \
	imfile-discard-truncated-line.sh \
	imfile-truncate-line.sh \
	imfile-file-not-found-error.sh \
	imfile-fileNotFoundError-parameter.sh \
	imfile-error-not-repeated.sh \
	imfile-basic-vg.sh \
	imfile-basic-vgthread.sh \
	testsuites/imfile-basic.conf \
	imfile-endregex-timeout-none-polling.sh \
	imfile-endregex-timeout-polling.sh \
	imfile-endregex-timeout.sh \
	imfile-endregex-timeout-none.sh \
	imfile-endregex-timeout-with-shutdown.sh \
	imfile-endregex-timeout-with-shutdown-polling.sh \
	imfile-persist-state-1.sh \
	imfile-truncate.sh \
	imfile-wildcards.sh \
	imfile-wildcards-dirs.sh \
	imfile-wildcards-dirs2.sh \
	imfile-wildcards-dirs-multi.sh \
	imfile-wildcards-dirs-multi2.sh \
	imfile-wildcards-dirs-multi3.sh \
	imfile-wildcards-dirs-multi4.sh \
	imfile-wildcards-dirs-multi5.sh \
	imfile-wildcards-dirs-multi5-polling.sh \
	imfile-old-state-file.sh \
	imfile-rename-while-stopped.sh \
	imfile-rename.sh \
	testsuites/imfile-wildcards-simple.conf \
	testsuites/imfile-wildcards-dirs.conf \
	testsuites/imfile-wildcards-dirs-multi.conf \
	testsuites/imfile-wildcards-dirs-multi2.conf \
	testsuites/imfile-wildcards-dirs-multi3.conf \
	testsuites/imfile-wildcards-dirs-multi4.conf \
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
	rulesetmultiqueue-v6.sh \
	testsuites/rulesetmultiqueue-v6.conf \
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
	execonlywhenprevsuspended_multiwrkr.sh \
	testsuites/execonlywhenprevsuspended_multiwrkr.conf \
	execonlywhenprevsuspended-queue.sh \
	testsuites/execonlywhenprevsuspended-queue.conf \
	execonlywhenprevsuspended-nonsusp.sh \
	testsuites/execonlywhenprevsuspended-nonsusp.conf \
	execonlywhenprevsuspended-nonsusp-queue.sh \
	testsuites/execonlywhenprevsuspended-nonsusp-queue.conf \
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
	imuxsock_logger_parserchain.sh \
	testsuites/imuxsock_logger_parserchain.conf \
	imuxsock_logger.sh \
	testsuites/imuxsock_logger.conf \
	imuxsock_logger_ruleset.sh \
	testsuites/imuxsock_logger_ruleset.conf \
	imuxsock_logger_ruleset_ratelimit.sh \
	testsuites/imuxsock_logger_ruleset_ratelimit.conf \
	imuxsock_logger_err.sh \
	imuxsock_logger_root.sh \
	imuxsock_logger_syssock.sh \
	testsuites/imuxsock_logger_root.conf \
	testsuites/imuxsock_logger_syssock.conf \
	resultdata/imuxsock_logger.log \
	imuxsock_traillf.sh \
	testsuites/imuxsock_traillf.conf \
	imuxsock_traillf_root.sh \
	imuxsock_traillf_syssock.sh \
	testsuites/imuxsock_traillf_root.conf \
	testsuites/imuxsock_traillf_syssock.conf \
	resultdata/imuxsock_traillf.log \
	imuxsock_ccmiddle.sh \
	testsuites/imuxsock_ccmiddle.conf \
	imuxsock_ccmiddle_root.sh \
	imklog_permitnonkernelfacility_root.sh \
	imuxsock_ccmiddle_syssock.sh \
	testsuites/imuxsock_ccmiddle_root.conf \
	testsuites/imuxsock_ccmiddle_syssock.conf \
	resultdata/imuxsock_ccmiddle.log \
	imuxsock_hostname.sh \
	testsuites/imuxsock_hostname.conf \
	resultdata/imuxsock_hostname.log \
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
	mysql-actq-mt.sh \
	mysql-actq-mt-withpause.sh \
	mysql-actq-mt-withpause-vg.sh \
	testsuites/mysql-actq-mt.conf \
	sndrcv_kafka.sh \
	sndrcv_kafka-vg-sender.sh \
	sndrcv_kafka-vg-rcvr.sh \
	sndrcv_kafka_multi.sh \
	sndrcv_kafka_fail.sh \
	sndrcv_kafka_failresume.sh \
	testsuites/sndrcv_kafka_rcvr.conf \
	testsuites/sndrcv_kafka_sender.conf \
	testsuites/sndrcv_kafka_multi_rcvr.conf \
	testsuites/sndrcv_kafka_multi_sender.conf \
	testsuites/kafka-server.properties \
	testsuites/kafka-server.dep_wrk1.properties \
	testsuites/kafka-server.dep_wrk2.properties \
	testsuites/kafka-server.dep_wrk3.properties \
	testsuites/zoo.cfg \
	testsuites/zoo.dep_wrk1.cfg \
	testsuites/zoo.dep_wrk2.cfg \
	testsuites/zoo.dep_wrk3.cfg \
	mmpstrucdata.sh \
	mmpstrucdata-case.sh \
	mmpstrucdata-vg.sh \
	testsuites/mmpstrucdata.conf \
	testsuites/mmpstrucdata-case.conf \
	mmpstrucdata-invalid-vg.sh \
	testsuites/mmpstrucdata-invalid.conf \
	libdbi-basic-vg.sh \
	dynstats_ctr_reset.sh \
	dynstats_reset_without_pstats_reset.sh \
	dynstats_nometric.sh \
	dynstats_overflow.sh \
	dynstats_overflow-vg.sh \
	dynstats_reset.sh \
	dynstats_reset-vg.sh \
	impstats-hup.sh \
	dynstats.sh \
	dynstats-vg.sh \
	dynstats_prevent_premature_eviction.sh \
	dynstats_prevent_premature_eviction-vg.sh \
	testsuites/dynstats.conf \
	testsuites/dynstats_ctr_reset.conf \
	testsuites/dynstats_reset_without_pstats_reset.conf \
	testsuites/dynstats_empty_input \
	testsuites/dynstats_input \
	testsuites/dynstats_input_1 \
	testsuites/dynstats_input_2 \
	testsuites/dynstats_input_3 \
	testsuites/dynstats_input_more_0 \
	testsuites/dynstats_input_more_1 \
	testsuites/dynstats_input_more_2 \
	testsuites/dynstats_nometric.conf \
	testsuites/dynstats_overflow.conf \
	testsuites/dynstats_reset.conf \
	no-dynstats-json.sh \
	testsuites/no-dynstats-json.conf \
	no-dynstats.sh \
	testsuites/no-dynstats.conf \
	stats-json.sh \
	stats-json-vg.sh \
	testsuites/stats-json.conf \
	stats-cee.sh \
	stats-cee-vg.sh \
	testsuites/stats-cee.conf \
	stats-json-es.sh \
	testsuites/stats-json-es.conf \
	dynstats-json.sh \
	dynstats-json-vg.sh \
	testsuites/dynstats-json.conf \
	mmnormalize_variable.sh \
	mmnormalize_tokenized.sh \
	testsuites/mmnormalize_variable.conf \
	testsuites/mmnormalize_variable.rulebase \
	testsuites/date_time_msg \
	testsuites/mmnormalize_tokenized.conf \
	testsuites/mmnormalize_tokenized.rulebase \
	testsuites/tokenized_input \
	rscript_random.sh \
	testsuites/rscript_random.conf \
	rscript_replace.sh \
	testsuites/rscript_replace.conf \
	rscript_replace_complex.sh \
	testsuites/rscript_replace_complex.conf \
	testsuites/complex_replace_input \
	rscript_unaffected_reset.sh \
	rscript_wrap2.sh \
	testsuites/rscript_wrap2.conf \
	rscript_wrap3.sh \
	testsuites/rscript_wrap3.conf \
	testsuites/wrap3_input\
	testsuites/gethostname.conf \
	json_array_subscripting.sh \
	testsuites/json_array_subscripting.conf \
	testsuites/json_array_input \
	testsuites/json_object_input \
	testsuites/json_nonarray_input \
	json_array_looping.sh \
	json_object_looping.sh \
	json_object_looping-vg.sh \
	json_array_looping-vg.sh \
	json_object_suicide_in_loop-vg.sh \
	json_nonarray_looping.sh \
	testsuites/json_array_looping.conf \
	testsuites/json_object_looping.conf \
	testsuites/json_object_suicide_in_loop.conf \
	json_null.sh \
	json_null-vg.sh \
	testsuites/json_null.conf \
	json_null_array.sh \
	json_null_array-vg.sh \
	testsuites/json_null_array.conf \
	mmnormalize_regex.sh \
	testsuites/mmnormalize_regex.conf \
	testsuites/mmnormalize_regex.rulebase \
	testsuites/regex_input \
	mmnormalize_regex_disabled.sh \
	testsuites/mmnormalize_regex_disabled.conf \
	mmnormalize_regex_defaulted.sh \
	testsuites/mmnormalize_regex_defaulted.conf \
	stop_when_array_has_element.sh \
	testsuites/stop_when_array_has_elem_input \
	testsuites/stop_when_array_has_element.conf \
	key_dereference_on_uninitialized_variable_space.sh \
	testsuites/key_dereference_on_uninitialized_variable_space.conf \
	rscript_re_extract.sh \
	testsuites/rscript_re_extract.conf \
	rscript_re_match.sh \
	testsuites/rscript_re_match.conf \
	lookup_table.sh \
	lookup_table_no_hup_reload.sh \
	lookup_table_no_hup_reload-vg.sh \
	lookup_table_rscript_reload.sh \
	lookup_table_rscript_reload_without_stub.sh \
	lookup_table_rscript_reload-vg.sh \
	lookup_table_rscript_reload_without_stub-vg.sh \
	rscript_trim-vg.sh \
	testsuites/lookup_table.conf \
	testsuites/lookup_table_no_hup_reload.conf \
	testsuites/lookup_table_reload_stub.conf \
	testsuites/lookup_table_reload.conf \
	testsuites/xlate.lkp_tbl \
	testsuites/xlate_more.lkp_tbl \
	unused_lookup_table-vg.sh \
	lookup_table-vg.sh \
	testsuites/unused_lookup_table.conf \
	array_lookup_table.sh \
	array_lookup_table-vg.sh \
	array_lookup_table_misuse-vg.sh \
	multiple_lookup_tables.sh \
	multiple_lookup_tables-vg.sh \
	testsuites/array_lookup_table.conf \
	testsuites/xlate_array.lkp_tbl \
	testsuites/xlate_array_more.lkp_tbl \
	testsuites/xlate_array_misuse.lkp_tbl \
	testsuites/xlate_array_more_misuse.lkp_tbl \
	sparse_array_lookup_table.sh \
	sparse_array_lookup_table-vg.sh \
	testsuites/xlate_sparse_array.lkp_tbl \
	testsuites/xlate_sparse_array_more.lkp_tbl \
	lookup_table_bad_configs.sh \
	lookup_table_bad_configs-vg.sh \
	testsuites/lookup_table_all.conf \
	testsuites/xlate_array_empty_table.lkp_tbl \
	testsuites/xlate_array_no_index.lkp_tbl \
	testsuites/xlate_array_no_table.lkp_tbl \
	testsuites/xlate_array_no_value.lkp_tbl \
	testsuites/xlate_empty_file.lkp_tbl \
	testsuites/xlate_incorrect_type.lkp_tbl \
	testsuites/xlate_incorrect_version.lkp_tbl \
	testsuites/xlate_sparseArray_empty_table.lkp_tbl \
	testsuites/xlate_sparseArray_no_index.lkp_tbl \
	testsuites/xlate_sparseArray_no_table.lkp_tbl \
	testsuites/xlate_sparseArray_no_value.lkp_tbl \
	testsuites/xlate_string_empty_table.lkp_tbl \
	testsuites/xlate_string_no_index.lkp_tbl \
	testsuites/xlate_string_no_table.lkp_tbl \
	testsuites/xlate_string_no_value.lkp_tbl \
	testsuites/xlate_invalid_json.lkp_tbl \
	testsuites/xlate_array_more_with_duplicates_and_nomatch.lkp_tbl \
	testsuites/xlate_more_with_duplicates_and_nomatch.lkp_tbl \
	testsuites/xlate_sparse_array_more_with_duplicates_and_nomatch.lkp_tbl \
	testsuites/multiple_lookup_tables.conf \
	json_var_cmpr.sh \
	testsuites/json_var_cmpr.conf \
	imptcp_nonProcessingPoller.sh \
	imptcp_veryLargeOctateCountedMessages.sh \
	testsuites/imptcp_nonProcessingPoller.conf \
	libmaxmindb.supp \
	travis/trusty.supp \
	linux_localtime_r.supp \
	json_var_case.sh \
	testsuites/json_var_case.conf \
	cfg.sh \
	empty-prop-comparison.sh \
	sndrcv_tls_anon_rebind.sh \
	testsuites/sndrcv_tls_anon_rebind_sender.conf \
	testsuites/sndrcv_tls_anon_rebind_rcvr.conf \
	sndrcv_tls_anon_hostname.sh \
	testsuites/sndrcv_tls_anon_hostname_sender.conf \
	testsuites/sndrcv_tls_anon_hostname_rcvr.conf \
	sndrcv_tls_anon_ipv4.sh \
	testsuites/sndrcv_tls_anon_ipv4_sender.conf \
	testsuites/sndrcv_tls_anon_ipv4_rcvr.conf \
	sndrcv_tls_anon_ipv6.sh \
	testsuites/sndrcv_tls_anon_ipv6_sender.conf \
	testsuites/sndrcv_tls_anon_ipv6_rcvr.conf \
	sndrcv_tls_priorityString.sh \
	testsuites/sndrcv_tls_priorityString_sender.conf \
	testsuites/sndrcv_tls_priorityString_rcvr.conf \
	omtcl.sh \
	omtcl.tcl \
	pmsnare.sh \
	testsuites/pmsnare_default.conf \
	testsuites/pmsnare_ccoff.conf \
	testsuites/pmsnare_ccdefault.conf \
	testsuites/pmsnare_cccstyle.conf \
	testsuites/pmsnare_ccbackslash.conf \
	testsuites/pmsnare_modoverride.conf \
	testsuites/sample.pmsnare_default \
	testsuites/sample.pmsnare_ccoff \
	testsuites/sample.pmsnare_ccdefault \
	testsuites/sample.pmsnare_cccstyle \
	testsuites/sample.pmsnare_ccbackslash \
	testsuites/sample.pmsnare_modoverride \
	testsuites/include-std-omfile-action.conf \
	pgsql-basic.sh \
	testsuites/pgsql-basic.sql \
	testsuites/pgsql-basic.conf \
	testsuites/pgsql-select-msg.sql \
	testsuites/pgsql-select-syslogtag.sql \
	pgsql-basic-cnf6.sh \
	pgsql-basic-threads-cnf6.sh \
	pgsql-template.sh \
	testsuites/pgsql-template.conf \
	pgsql-template-cnf6.sh \
	pgsql-actq-mt-withpause.sh \
	pgsql-template-threads-cnf6.sh \
	pgsql-basic-vg.sh \
	pgsql-template-vg.sh \
	pgsql-basic-cnf6-vg.sh \
	pgsql-template-cnf6-vg.sh \
	pgsql-actq-mt-withpause-vg.sh \
	../devtools/prep-mysql-db.sh

ourtail_SOURCES = ourtail.c
msleep_SOURCES = msleep.c
omrelp_dflt_port_SOURCES = omrelp_dflt_port.c
mangle_qi_SOURCES = mangle_qi.c
chkseq_SOURCES = chkseq.c

uxsockrcvr_SOURCES = uxsockrcvr.c
uxsockrcvr_LDADD = $(SOL_LIBS)

tcpflood_SOURCES = tcpflood.c
tcpflood_CFLAGS = $(PTHREADS_CFLAGS) $(RELP_CFLAGS) $(GNUTLS_CFLAGS)
tcpflood_CPPFLAGS = $(PTHREADS_CFLAGS) $(RELP_CFLAGS) $(GNUTLS_CFLAGS)
tcpflood_LDADD = $(SOL_LIBS) $(PTHREADS_LIBS) $(RELP_LIBS) $(GNUTLS_LIBS)
if ENABLE_GNUTLS
tcpflood_LDADD += -lgcrypt
endif

minitcpsrv_SOURCES = minitcpsrvr.c
minitcpsrv_LDADD = $(SOL_LIBS)

syslog_caller_SOURCES = syslog_caller.c
syslog_caller_CPPFLAGS = $(LIBLOGGING_STDLOG_CFLAGS)
syslog_caller_LDADD = $(SOL_LIBS) $(LIBLOGGING_STDLOG_LIBS)

journal_print_SOURCES = journal_print.c
journal_print_CPPFLAGS = $(LIBSYSTEMD_JOURNAL_CFLAGS)
journal_print_LDADD = $(LIBSYSTEMD_JOURNAL_LIBS)

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
