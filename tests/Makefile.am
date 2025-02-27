TEST_EXTENSIONS=.sh

if ENABLE_TESTBENCH

CLEANFILES=\
	*_*.conf \
	rsyslog*.started work-*.conf rsyslog.random.data \
	rsyslog*.pid.save xlate*.lkp_tbl \
	log log* *.log \
	work \
	test-spool test-logdir stat-file1 \
	rsyslog.pipe rsyslog.input.* \
	rsyslog.input rsyslog.input.* imfile-state:* omkafka-failed.data \
	rsyslog.input-symlink.log rsyslog-link.*.log targets \
	HOSTNAME \
	rstb_* \
	zookeeper.pid \
	tmp.qi nocert

CLEANFILES+= \
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
check_PROGRAMS = $(TESTRUNS) ourtail tcpflood chkseq msleep randomgen \
	diagtalker uxsockrcvr syslog_caller inputfilegen minitcpsrv \
	omrelp_dflt_port \
	mangle_qi \
	have_relpSrvSetOversizeMode \
	have_relpEngineSetTLSLibByName \
	have_relpSrvSetTlsConfigCmd \
	check_relpEngineVersion \
	test_id
if ENABLE_JOURNAL_TESTS
if ENABLE_IMJOURNAL
check_PROGRAMS += journal_print
endif
endif # if ENABLE_JOURNAL_TESTS
TESTS = $(TESTRUNS)

if ENABLE_ELASTICSEARCH_TESTS_MINIMAL
TESTS +=  \
	es-duplicated-ruleset.sh
# the following test need to be serialized:
TESTS +=  \
	es-basic-es7.14.sh \
	es-basic.sh \
	es-basic-bulk.sh
es-basic.log: es-basic-es7.14.log
es-basic-bulk.log: es-basic.log
es-basic-server.log: es-basic-bulk.log

# special "test" for stopping ES once all ES tests are done
TESTS += elasticsearch-stop.sh
elasticsearch-stop.log: es-basic-bulk.log


if HAVE_VALGRIND
TESTS += \
	es-duplicated-ruleset-vg.sh \
	es-basic-vg.sh
es-basic-vg.log: es-basic-bulk.log
# for next if block:
es-basic-server.log: es-basic-vg.log
elasticsearch-stop.log: es-basic-vg.log
if ENABLE_HELGRIND
TESTS += \
	es-basic-vgthread.sh
es-basic-vgthread.log: es-basic-vg.log
# for next if block:
es-basic-server.log: es-basic-vgthread.log
elasticsearch-stop.log: es-basic-vgthread.log
endif # ENABLE_HELGRIND
endif # HAVE_VALGRIND
endif # ENABLE_ELASTICSEARCH_TESTS_MINIMAL

if ENABLE_ELASTICSEARCH_TESTS
TESTS +=  \
	es-basic-server.sh \
	es-execOnlyWhenPreviousSuspended.sh \
	es-maxbytes-bulk.sh \
	es-basic-errfile-empty.sh \
	es-basic-errfile-popul.sh \
	es-bulk-errfile-empty.sh \
	es-bulk-errfile-popul.sh \
	es-searchType-empty.sh \
	diskqueue-multithread-es.sh \
	es-writeoperation.sh

es-basic-server.log: es-basic-bulk.log
es-execOnlyWhenPreviousSuspended.log: es-basic-server.log
es-maxbytes-bulk.log: es-execOnlyWhenPreviousSuspended.log
es-basic-errfile-empty.log: es-maxbytes-bulk.log
es-basic-errfile-popul.log: es-basic-errfile-empty.log
es-bulk-errfile-empty.log: es-basic-errfile-popul.log
es-bulk-errfile-popul.log: es-bulk-errfile-empty.log
es-searchType-empty.log: es-bulk-errfile-popul.log
diskqueue-multithread-es.log: es-searchType-empty.log
es-writeoperation.log: diskqueue-multithread-es.log
elasticsearch-stop.log: es-writeoperation.log

if ENABLE_IMPSTATS
TESTS +=  \
	es-basic-ha.sh \
	es-bulk-retry.sh

es-basic-ha.log: es-writeoperation.log
es-bulk-retry.log: es-basic-ha.log
elasticsearch-stop.log: es-bulk-retry.log
endif
if ENABLE_IMFILE
TESTS +=  \
	es-bulk-errfile-popul-def-format.sh \
	es-bulk-errfile-popul-erronly.sh \
	es-bulk-errfile-popul-erronly-interleaved.sh \
	es-bulk-errfile-popul-def-interleaved.sh

es-bulk-errfile-popul-def-format.log: es-bulk-retry.log
es-bulk-errfile-popul-erronly.log: es-bulk-errfile-popul-def-format.log
es-bulk-errfile-popul-erronly-interleaved.log: es-bulk-errfile-popul-erronly.log
es-bulk-errfile-popul-def-interleaved.log: es-bulk-errfile-popul-erronly-interleaved.log
elasticsearch-stop.log: es-bulk-errfile-popul-def-interleaved.log
endif
if HAVE_VALGRIND
TESTS +=  \
	es-basic-bulk-vg.sh \
	es-basic-ha-vg.sh

es-basic-bulk-vg.log: es-writeoperation.log
es-basic-bulk-vg.log: es-bulk-retry.log
es-basic-bulk-vg.log: es-bulk-errfile-popul-def-interleaved.log
es-basic-ha-vg.log: es-basic-bulk-vg.log
elasticsearch-stop.log: es-basic-ha-vg.log
endif
endif # if ENABLE_ELASTICSEARCH_TESTS



if ENABLE_DEFAULT_TESTS
TESTS +=  \
	immark.sh \
	immark-inputname.sh \
	immark-ruleset.sh \
	immark-ruleset-custom-msg.sh \
	operatingstate-basic.sh \
	operatingstate-empty.sh \
	operatingstate-unclean.sh \
	smtradfile.sh \
	loadbalance.sh \
	empty-hostname.sh \
	timestamp-3164.sh \
	timestamp-3339.sh \
	timestamp-isoweek.sh \
	timestamp-mysql.sh \
	timestamp-pgsql.sh \
	timestamp-subseconds.sh \
	msleep_usage_output.sh \
	mangle_qi_usage_output.sh \
	minitcpsrv_usage_output.sh \
	test_id_usage_output.sh \
	prop-programname.sh \
	prop-programname-with-slashes.sh \
	hostname-with-slash-pmrfc5424.sh \
	hostname-with-slash-pmrfc3164.sh \
	hostname-with-slash-dflt-invld.sh \
	func-substring-invld-startpos.sh \
	func-substring-large-endpos.sh \
	func-substring-large-neg-endpos.sh \
	func-substring-relative-endpos.sh \
	hostname-with-slash-dflt-slash-valid.sh \
	empty-app-name.sh \
	stop-localvar.sh \
	stop-msgvar.sh \
	glbl-ruleset-queue-defaults.sh \
	glbl-internalmsg_severity-info-shown.sh \
	glbl-internalmsg_severity-debug-shown.sh \
	glbl-internalmsg_severity-debug-not_shown.sh \
	glbl-umask.sh \
	glbl-unloadmodules.sh \
	glbl-invld-param.sh \
	glbl_setenv_2_vars.sh \
	glbl_setenv_err.sh \
	glbl_setenv_err_too_long.sh \
	glbl_setenv.sh \
	mmexternal-SegFault.sh \
	nested-call-shutdown.sh \
	dnscache-TTL-0.sh \
	invalid_nested_include.sh \
	omfwd-lb-1target-retry-full_buf.sh \
	omfwd-lb-1target-retry-1_byte_buf.sh \
	omfwd-lb-1target-retry-1_byte_buf-TargetFail.sh \
	omfwd-lb-susp.sh \
	omfwd-lb-2target-basic.sh \
	omfwd-lb-2target-retry.sh \
	omfwd-lb-2target-one_fail.sh \
	omfwd-tls-invalid-permitExpiredCerts.sh \
	omfwd-keepalive.sh \
	omusrmsg-errmsg-no-params.sh \
	omusrmsg-noabort.sh \
	omfile-module-params.sh \
	omfile-read-only-errmsg.sh \
	omfile-null-filename.sh \
	omfile-whitespace-filename.sh \
	omfile-read-only.sh \
	omfile-outchannel.sh \
	omfile_both_files_set.sh \
	omfile_hup.sh \
	msgvar-concurrency.sh \
	localvar-concurrency.sh \
	exec_tpl-concurrency.sh \
	rscript_privdropuser.sh \
	rscript_privdropuserid.sh \
	rscript_privdropgroup.sh \
	rscript_privdropgroupid.sh \
	privdropuser.sh \
	privdropuserid.sh \
	privdropgroup.sh \
	privdropgroupid.sh \
	privdropabortonidfail.sh \
	privdropabortonidfaillegacy.sh \
	json-nonstring.sh \
        json-onempty-at-end.sh \
	template-json.sh \
	template-pure-json.sh \
	template-pos-from-to.sh \
	template-pos-from-to-lowercase.sh \
	template-pos-from-to-oversize.sh \
	template-pos-from-to-oversize-lowercase.sh \
	template-pos-from-to-missing-jsonvar.sh \
	template-const-jsonf.sh \
	template-topos-neg.sh \
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
	rfc5424parser-sp_at_msg_start.sh \
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
	mainq_actq_DA.sh \
	queue_warnmsg-oversize.sh \
	queue-minbatch.sh \
	queue-minbatch-queuefull.sh \
	queue-direct-with-no-params.sh \
	queue-direct-with-params-given.sh \
	arrayqueue.sh \
	global_vars.sh \
	no-parser-errmsg.sh \
	da-mainmsg-q.sh \
	validation-run.sh \
	msgdup.sh \
	msgdup_props.sh \
	empty-ruleset.sh \
	ruleset-direct-queue.sh \
	imtcp-listen-port-file-2.sh \
	allowed-sender-tcp-ok.sh \
	allowed-sender-tcp-fail.sh \
	allowed-sender-tcp-hostname-ok.sh \
	allowed-sender-tcp-hostname-fail.sh \
	imtcp-discard-truncated-msg.sh \
	da-queue-persist.sh \
	daqueue-persist.sh \
	daqueue-invld-qi.sh \
	daqueue-dirty-shutdown.sh \
	diskq-rfc5424.sh \
	diskqueue.sh \
	diskqueue-fsync.sh \
	diskqueue-full.sh \
	diskqueue-fail.sh \
	diskqueue-non-unique-prefix.sh \
	rulesetmultiqueue.sh \
	rulesetmultiqueue-v6.sh \
	manytcp.sh \
	rsf_getenv.sh \
	msg-deadlock-headerless-noappname.sh \
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
	asynwr_dynfile_flushtxend-off.sh \
	abort-uncleancfg-goodcfg.sh \
	abort-uncleancfg-goodcfg-check.sh \
	abort-uncleancfg-badcfg-check.sh \
	abort-uncleancfg-badcfg-check_1.sh \
	variable_leading_underscore.sh \
	gzipwr_hup_multi_file.sh \
	gzipwr_hup_single_file.sh \
	gzipwr_rscript.sh \
	gzipwr_flushInterval.sh \
	gzipwr_flushOnTXEnd.sh \
	gzipwr_large.sh \
	gzipwr_large_dynfile.sh \
	gzipwr_hup.sh \
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
	imuxsock_legacy.sh \
	imuxsock_logger.sh \
	imuxsock_logger_ratelimit.sh \
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
	suspend-via-file.sh \
	suspend-omfwd-via-file.sh \
	externalstate-failed-rcvr.sh \
	rcvr_fail_restore.sh \
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
	rscript_re_extract_i.sh \
	rscript_re_extract.sh \
	rscript_re_match_i.sh \
	rscript_re_match.sh \
	rscript_re_match-dbl_quotes.sh \
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
	rscript_number_comparison_LE.sh \
	rscript_number_comparison_LT.sh \
	rscript_compare_str-numstr.sh \
	rscript_compare_str-num.sh \
	rscript_compare_numstr-str.sh \
	rscript_compare_num-str.sh \
	rscript_compare_numstr-numstr.sh \
	rscript_compare_numstr-num.sh \
	rscript_compare_num-numstr.sh \
	rscript_compare_num-num.sh \
	rscript_compare_str-str.sh \
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
	rscript_exists-yes.sh \
	rscript_exists-yes2.sh \
	rscript_exists-not1.sh \
	rscript_exists-not2.sh \
	rscript_exists-not3.sh \
	rscript_exists-not4.sh \
	rscript-config_enable-on.sh \
	rscript_get_property.sh \
	config_output-o-option.sh \
	rs-cnum.sh \
	rs-substring.sh \
	rs-int2hex.sh \
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
	lookup_table-hup-backgrounded.sh \
	lookup_table_no_hup_reload.sh \
	key_dereference_on_uninitialized_variable_space.sh \
	array_lookup_table.sh \
	sparse_array_lookup_table.sh \
	lookup_table_bad_configs.sh \
	lookup_table_rscript_reload.sh \
	lookup_table_rscript_reload_without_stub.sh \
	config_multiple_include.sh \
	include-obj-text-from-file.sh \
	include-obj-text-from-file-noexist.sh \
	multiple_lookup_tables.sh \
	parsertest-parse1.sh \
	parsertest-parse1-udp.sh \
	parsertest-parse2.sh \
	parsertest-parse2-udp.sh \
	parsertest-parse_8bit_escape.sh \
	parsertest-parse_8bit_escape-udp.sh \
	parsertest-parse3.sh \
	parsertest-parse3-udp.sh \
	parsertest-parse_invld_regex.sh \
	parsertest-parse_invld_regex-udp.sh \
	parsertest-parse-3164-buggyday.sh \
	parsertest-parse-3164-buggyday-udp.sh \
	parsertest-parse-nodate.sh \
	parsertest-parse-nodate-udp.sh \
	parsertest-snare_ccoff_udp.sh \
	parsertest-snare_ccoff_udp2.sh

if ENABLE_LIBZSTD
TESTS +=  \
	zstd.sh
if HAVE_VALGRIND
TESTS +=  \
	zstd-vg.sh
endif # if HAVE_VALGRIND
endif

if ENABLE_LIBGCRYPT
TESTS +=  \
	queue-encryption-disk.sh \
	queue-encryption-disk_keyfile.sh \
	queue-encryption-disk_keyprog.sh \
	queue-encryption-da.sh
endif # ENABLE_LIBGCRYPT
if HAVE_VALGRIND
TESTS +=  \
	omusrmsg-noabort-vg.sh \
	omfile_hup-vg.sh \
	gzipwr_hup-vg.sh \
	tcpflood_wrong_option_output.sh \
	imtcp-octet-framing-too-long-vg.sh \
	smtradfile-vg.sh \
	dnscache-TTL-0-vg.sh \
	include-obj-outside-control-flow-vg.sh \
	include-obj-in-if-vg.sh \
	include-obj-text-vg.sh \
	rscript_parse_json-vg.sh \
	rscript_backticks-vg.sh \
	rscript_backticks_empty_envvar-vg.sh \
	rscript-config_enable-off-vg.sh \
	rscript_get_property-vg.sh \
	prop-jsonmesg-vg.sh \
	mmexternal-InvldProg-vg.sh \
	internal-errmsg-memleak-vg.sh \
	glbl-oversizeMsg-log-vg.sh \
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
	rscript_number_comparison_LE-vg.sh \
	rscript_compare_str-str-vg.sh \
	rscript_compare_str-num-vg.sh \
	rscript_compare_str-numstr-vg.sh \
	rscript_compare_num-str-vg.sh \
	rscript_compare_numstr-str-vg.sh \
	rscript_compare_numstr-num-vg.sh \
	rscript_compare_numstr-numstr-vg.sh \
	rscript_compare_num-num-vg.sh \
	rscript_compare_num-numstr-vg.sh \
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
	badqi.sh \
	threadingmq.sh \
	threadingmqaq.sh \
	func-substring-invld-startpos-vg.sh \
	rscript_trim-vg.sh
if ENABLE_LIBGCRYPT
TESTS +=  \
	queue-encryption-disk_keyfile-vg.sh
endif # ENABLE_LIBGCRYPT
endif # HAVE_VALGRIND
endif # ENABLE_DEFAULT_TESTS



if ENABLE_IMTCP_TESTS
TESTS +=  \
	imtcp-listen-port-file-2.sh \
	allowed-sender-tcp-ok.sh \
	allowed-sender-tcp-fail.sh \
	allowed-sender-tcp-hostname-ok.sh \
	allowed-sender-tcp-hostname-fail.sh \
	imtcp-discard-truncated-msg.sh \
	imtcp-basic.sh \
	imtcp-basic-hup.sh \
	imtcp-impstats-single-thread.sh \
	imtcp-starvation-0.sh \
	imtcp-starvation-1.sh \
	imtcp-maxFrameSize.sh \
	imtcp-msg-truncation-on-number.sh \
	imtcp-msg-truncation-on-number2.sh \
	imtcp-NUL.sh \
	imtcp-NUL-rawmsg.sh \
	imtcp_incomplete_frame_at_end.sh \
	imtcp-multiport.sh \
	imtcp-bigmessage-octetcounting.sh \
	imtcp-bigmessage-octetstuffing.sh \
	manytcp.sh \
	imtcp_conndrop.sh \
	imtcp_addtlframedelim.sh \
	imtcp_no_octet_counted.sh \
	imtcp_addtlframedelim_on_input.sh \
	imtcp_spframingfix.sh \
	imtcp-connection-msg-recieved.sh \
	sndrcv.sh \
	sndrcv_failover.sh \
	sndrcv_gzip.sh \
	sndrcv_udp_nonstdpt.sh \
	sndrcv_udp_nonstdpt_v6.sh \
	sndrcv_udp_nonstdpt_v6.sh

if HAVE_VALGRIND
TESTS +=  \
	imtcp-octet-framing-too-long-vg.sh
endif # HAVE_VALGRIND
if ENABLE_IMTCP_EPOLL
TESTS +=  \
	imtcp-impstats.sh
endif # ENABLE_IMTCP_EPOLL
endif # ENABLE_IMTCP_TESTS

if ENABLE_SNMP
TESTS +=  \
	omsnmp_errmsg_no_params.sh
if ENABLE_SNMP_TESTS
TESTS +=  \
	sndrcv_omsnmpv1_udp.sh \
	sndrcv_omsnmpv1_udp_dynsource.sh \
	sndrcv_omsnmpv1_udp_invalidoid.sh
endif # if ENABLE_SNMP_TESTS
endif # if ENABLE_SNMP

if ENABLE_MMUTF8FIX
TESTS +=  \
	mmutf8fix_no_error.sh
endif # if ENABLE_MAIL

if ENABLE_MAIL
TESTS +=  \
	ommail_errmsg_no_params.sh
endif # if ENABLE_MAIL

if ENABLE_MMANON
TESTS +=  \
	mmanon_with_debug.sh \
	mmanon_random_32_ipv4.sh \
	mmanon_random_cons_32_ipv4.sh \
	mmanon_recognize_ipv4.sh \
	mmanon_zero_12_ipv4.sh \
	mmanon_zero_33_ipv4.sh \
	mmanon_zero_8_ipv4.sh \
	mmanon_simple_12_ipv4.sh \
	mmanon_simple_33_ipv4.sh \
	mmanon_simple_8_ipv4.sh \
	mmanon_simple_mallformed_ipv4.sh \
	mmanon_random_128_ipv6.sh \
	mmanon_zero_128_ipv6.sh \
	mmanon_zero_96_ipv6.sh \
	mmanon_random_cons_128_ipv6.sh \
	mmanon_zero_50_ipv6.sh \
	mmanon_recognize_ipv6.sh \
	mmanon_zero_64_ipv6.sh \
	mmanon_both_modes_compatible.sh \
	mmanon_recognize_ipembedded.sh \
	mmanon_ipv6_port.sh \
	mmanon_random_cons_128_ipembedded.sh
endif # if ENABLE_MMANON

if ENABLE_CLICKHOUSE_TESTS
TESTS +=  \
	clickhouse-start.sh \
	clickhouse-basic.sh \
	clickhouse-dflt-tpl.sh \
	clickhouse-retry-error.sh \
	clickhouse-load.sh \
	clickhouse-bulk.sh \
	clickhouse-bulk-load.sh \
	clickhouse-limited-batch.sh \
	clickhouse-select.sh \
	clickhouse-errorfile.sh \
	clickhouse-wrong-quotation-marks.sh \
	clickhouse-wrong-template-option.sh \
	clickhouse-wrong-insert-syntax.sh

clickhouse-basic.log: clickhouse-start.log
clickhouse-dflt-tpl.log: clickhouse-basic.log
clickhouse-retry-error.log: clickhouse-dflt-tpl.log
clickhouse-load.log: clickhouse-retry-error.log
clickhouse-bulk.log: clickhouse-load.log
clickhouse-bulk-load.log: clickhouse-bulk.log
clickhouse-limited-batch.log: clickhouse-bulk-load.log
clickhouse-select.log: clickhouse-limited-batch.log
clickhouse-errorfile.log: clickhouse-select.log
clickhouse-wrong-quotation-marks.log: clickhouse-errorfile.log
clickhouse-wrong-template-option.log: clickhouse-wrong-quotation-marks.log
clickhouse-wrong-insert-syntax.log: clickhouse-wrong-template-option.log
clickhouse-stop.log: clickhouse-wrong-insert-syntax.log

if HAVE_VALGRIND
TESTS +=  \
	clickhouse-basic-vg.sh \
	clickhouse-load-vg.sh \
	clickhouse-bulk-vg.sh \
	clickhouse-bulk-load-vg.sh

clickhouse-basic-vg.log: clickhouse-wrong-insert-syntax.log
clickhouse-load-vg.log: clickhouse-basic-vg.log
clickhouse-bulk-vg.log: clickhouse-load-vg.log
clickhouse-bulk-load-vg.log: clickhouse-bulk-vg.log
clickhouse-stop.log: clickhouse-bulk-load-vg.log
endif # VALGRIND

TESTS += clickhouse-stop.sh
endif # CLICKHOUSE_TESTS


if ENABLE_LIBFAKETIME
TESTS +=  \
	now_family_utc.sh \
	now-utc.sh \
	now-utc-ymd.sh \
	now-utc-casecmp.sh \
	now-unixtimestamp.sh \
	timegenerated-ymd.sh \
	timegenerated-uxtimestamp.sh \
	timegenerated-uxtimestamp-invld.sh \
	timegenerated-dateordinal.sh \
	timegenerated-dateordinal-invld.sh \
	timegenerated-utc.sh \
	timegenerated-utc-legacy.sh \
	timereported-utc.sh \
	timereported-utc-legacy.sh
# now come faketime tests that utilize mmnormalize - aka "no endif here"
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

pgsql-basic-cnf6.log: pgsql-basic.log
pgsql-basic-threads-cnf6.log: pgsql-basic-cnf6.log
pgsql-template.log: pgsql-basic-threads-cnf6.log
pgsql-template-cnf6.log: pgsql-template.log
pgsql-actq-mt-withpause.log: pgsql-template-cnf6.log
pgsql-template-threads-cnf6.log: pgsql-actq-mt-withpause.log
if HAVE_VALGRIND
TESTS += \
	pgsql-basic-vg.sh \
	pgsql-template-vg.sh \
	pgsql-basic-cnf6-vg.sh \
	pgsql-template-cnf6-vg.sh \
	pgsql-actq-mt-withpause-vg.sh

pgsql-basic-vg.log: pgsql-template-threads-cnf6.log
pgsql-template-vg.log: pgsql-basic-vg.log
pgsql-basic-cnf6-vg.log: pgsql-template-vg.log
pgsql-template-cnf6-vg.log: pgsql-basic-cnf6-vg.log
pgsql-actq-mt-withpause-vg.log: pgsql-template-cnf6-vg.log
endif
endif
endif

if ENABLE_MYSQL_TESTS
TESTS +=  \
	mysqld-start.sh \
	mysql-basic.sh \
	mysql-basic-cnf6.sh \
	mysql-asyn.sh \
	mysql-actq-mt.sh \
	mysql-actq-mt-withpause.sh \
	action-tx-single-processing.sh \
	action-tx-errfile-maxsize.sh \
	action-tx-errfile.sh

mysql-basic.log: mysqld-start.log
mysql-basic-cnf6.log: mysqld-start.log
mysql-asyn.log: mysqld-start.log
mysql-actq-mt.log: mysqld-start.log
mysql-actq-mt-withpause.log: mysqld-start.log
action-tx-single-processing.log: mysqld-start.log
action-tx-errfile.log: mysqld-start.log

mysqld-stop.log: mysql-basic.log \
	mysql-basic-cnf6.log \
	mysql-asyn.log \
	mysql-actq-mt.log \
	mysql-actq-mt-withpause.log \
	action-tx-single-processing.log \
	action-tx-errfile.log

if HAVE_VALGRIND
TESTS +=  \
	mysql-basic-vg.sh \
	mysql-asyn-vg.sh \
	mysql-actq-mt-withpause-vg.sh
mysql-basic-vg.log: mysqld-start.log
mysql-asyn-vg.log: mysqld-start.log
mysql-actq-mt-withpause-vg.log: mysqld-start.log

mysqld-stop.log: mysql-basic-vg.log \
	mysql-asyn-vg.log \
	mysql-actq-mt-withpause-vg.log
endif

if ENABLE_OMLIBDBI # we piggy-back on MYSQL_TESTS as we need the same environment
TESTS +=  \
	libdbi-basic.sh \
	libdbi-asyn.sh
libdbi-basic.log: mysqld-start.log
libdbi-asyn.log: mysqld-start.log
mysqld-stop.log: libdbi-basic.log \
	libdbi-asyn.log
if HAVE_VALGRIND
TESTS +=  \
	libdbi-basic-vg.sh
libdbi-basic-vg.log: mysqld-start.log
mysqld-stop.log: libdbi-basic-vg.log
endif
endif
TESTS += mysqld-stop.sh
endif #  MYSQL_TESTS

if ENABLE_FMHTTP
TESTS +=  \
	rscript_http_request.sh
if HAVE_VALGRIND
TESTS +=  \
	rscript_http_request-vg.sh
endif # HAVE_VALGRIND
endif # ENABLE_FMHTTP


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
if HAVE_VALGRIND
TESTS +=  \
	mmexternal-SegFault-empty-jroot-vg.sh
endif
endif

# see comments in configure.ac on flaky tests for important details!!!
if ENABLE_FLAKY_TESTS
#TESTS += 
#
if ENABLE_OMHTTP
TESTS +=  \
	omhttp-batch-retry-metadata.sh
if HAVE_VALGRIND
TESTS +=  \
	omhttp-batch-retry-metadata-vg.sh
endif # end if HAVE_VALGRIND
endif # end if ENABLE_OMHTTP
endif # end if ENABLE_FLAKY_TESTS

if ENABLE_JOURNAL_TESTS
if ENABLE_IMJOURNAL
TESTS +=  \
	imjournal-basic.sh \
	imjournal-statefile.sh
if HAVE_VALGRIND
TESTS +=  \
	imjournal-basic-vg.sh \
	imjournal-statefile-vg.sh
endif
endif

if ENABLE_OMJOURNAL
TESTS +=  \
	omjournal-abort-template.sh \
	omjournal-abort-no-template.sh \
	omjournal-basic-template.sh \
	omjournal-basic-no-template.sh
endif
endif #if ENABLE_JOURNAL_TESTS

if ENABLE_IMPROG
TESTS +=  \
	improg_errmsg_no_params.sh \
	improg_prog_simple.sh \
	improg_prog_confirm.sh \
	improg_prog_confirm_killonclose.sh \
	improg_prog_killonclose.sh \
	improg_simple_multi.sh
if HAVE_VALGRIND
TESTS +=  \
	improg_errmsg_no_params-vg.sh \
	improg_prog_simple-vg.sh
endif # ENABLE_IMPROG
endif

if ENABLE_MMDARWIN
TESTS +=  \
	mmdarwin_errmsg_no_params.sh \
	mmdarwin_errmsg_no_sock.sh
if HAVE_VALGRIND
TESTS += \
	mmdarwin_errmsg_no_sock-vg.sh
endif # HAVE_VALGRIND
endif # ENABLE_IMPROG

if ENABLE_OMPROG
TESTS +=  \
	omprog-defaults.sh \
	omprog-output-capture.sh \
	omprog-output-capture-mt.sh \
	omprog-feedback.sh \
	omprog-feedback-mt.sh \
	omprog-feedback-timeout.sh \
	omprog-close-unresponsive.sh \
	omprog-close-unresponsive-noterm.sh \
	omprog-restart-terminated.sh \
	omprog-restart-terminated-outfile.sh \
	omprog-single-instance.sh \
	omprog-single-instance-outfile.sh \
	omprog-transactions.sh \
	omprog-if-error.sh \
	omprog-transactions-failed-messages.sh \
	omprog-transactions-failed-commits.sh
if HAVE_VALGRIND
TESTS +=  \
	omprog-defaults-vg.sh \
	omprog-output-capture-vg.sh \
	omprog-feedback-vg.sh \
	omprog-close-unresponsive-vg.sh \
	omprog-restart-terminated-vg.sh \
	omprog-single-instance-vg.sh \
	omprog-transactions-vg.sh
endif
endif

if ENABLE_OMHTTP
TESTS +=  \
	omhttp-auth.sh \
	omhttp-basic.sh \
	omhttp-basic-ignorecodes.sh \
	omhttp-batch-fail-with-400.sh \
	omhttp-batch-jsonarray-compress.sh \
	omhttp-batch-jsonarray-retry.sh \
	omhttp-batch-jsonarray.sh \
	omhttp-batch-kafkarest-retry.sh \
	omhttp-batch-kafkarest.sh \
	omhttp-batch-lokirest-retry.sh \
	omhttp-batch-lokirest.sh \
	omhttp-batch-newline.sh \
	omhttp-retry.sh \
	omhttp-retry-timeout.sh \
	omhttp-httpheaderkey.sh \
	omhttp-multiplehttpheaders.sh \
	omhttp-dynrestpath.sh \
	omhttp-batch-dynrestpath.sh
if HAVE_VALGRIND
TESTS +=  \
	omhttp-auth-vg.sh \
	omhttp-basic-vg.sh \
	omhttp-basic-ignorecodes-vg.sh \
	omhttp-batch-jsonarray-compress-vg.sh \
	omhttp-batch-jsonarray-retry-vg.sh \
	omhttp-batch-jsonarray-vg.sh \
	omhttp-batch-kafkarest-retry-vg.sh \
	omhttp-batch-lokirest-retry-vg.sh \
	omhttp-retry-vg.sh \
	omhttp-retry-timeout-vg.sh \
	omhttp-batch-lokirest-vg.sh
endif
endif

if ENABLE_OMKAFKA
if ENABLE_IMKAFKA
if ENABLE_KAFKA_TESTS
TESTS += \
	kafka-selftest.sh \
	omkafka.sh \
	omkafkadynakey.sh \
	imkafka.sh \
	imkafka-backgrounded.sh \
	imkafka-config-err-ruleset.sh \
	imkafka-config-err-param.sh \
	imkafka-hang-on-no-kafka.sh \
	imkafka-hang-other-action-on-no-kafka.sh \
	imkafka_multi_single.sh \
	imkafka_multi_group.sh \
	sndrcv_kafka.sh \
	sndrcv_kafka_multi_topics.sh
# Tests below need to be stable first!
#	sndrcv_kafka_fail.sh \
#	sndrcv_kafka_failresume.sh \
# needs properly to much mempory on arm devices!
#	sndrcv_kafka_multi.sh
omkafka.log: kafka-selftest.log
imkafka.log: omkafka.log
imkafka-backgrounded.log: imkafka.log
imkafka-config-err-ruleset.log: imkafka-backgrounded.log
imkafka-config-err-param.log: imkafka-config-err-ruleset.log
imkafka-hang-on-no-kafka.log: imkafka-config-err-param.log
imkafka-hang-other-action-on-no-kafka.log: imkafka-hang-on-no-kafka.log
imkafka_multi_single.log: imkafka-hang-other-action-on-no-kafka.log
sndrcv_kafka.log: imkafka_multi_single.log
imkafka_multi_group.log: sndrcv_kafka.log
sndrcv_kafka_multi_topics.log: imkafka_multi_group.log
omkafkadynakey.log: sndrcv_kafka_multi_topics.log

if HAVE_VALGRIND
TESTS += \
	omkafka-vg.sh \
	imkafka-vg.sh

omkafka-vg.log: sndrcv_kafka_multi_topics.log
imkafka-vg.log: omkafka-vg.log
endif
endif
endif
endif

if ENABLE_OMAZUREEVENTHUBS
if ENABLE_OMAZUREEVENTHUBS_TESTS
TESTS += \
	omazureeventhubs-basic.sh \
	omazureeventhubs-basic-url.sh \
	omazureeventhubs-list.sh \
	omazureeventhubs-stress.sh \
	omazureeventhubs-interrupt.sh
if HAVE_VALGRIND
TESTS += \
	omazureeventhubs-basic-vg.sh \
	omazureeventhubs-interrupt-vg.sh
endif
endif
endif

if ENABLE_IMDOCKER
if ENABLE_IMDOCKER_TESTS
TESTS += \
	imdocker-basic.sh \
	imdocker-long-logline.sh \
	imdocker-new-logs-from-start.sh \
	imdocker-multi-line.sh
if HAVE_VALGRIND
TESTS +=  \
	imdocker-basic-vg.sh \
	imdocker-long-logline-vg.sh \
	imdocker-new-logs-from-start-vg.sh \
	imdocker-multi-line-vg.sh
endif # HAVE_VALGRIND
endif # ENABLE_IMDOCKER_TESTS
endif # ENABLE_IMDOCKER

if ENABLE_IMHTTP
TESTS += \
	imhttp-post-payload.sh \
	imhttp-post-payload-basic-auth.sh \
	imhttp-post-payload-query-params.sh \
	imhttp-post-payload-large.sh \
	imhttp-post-payload-multi.sh \
	imhttp-post-payload-multi-lf.sh \
	imhttp-post-payload-compress.sh \
	imhttp-getrequest-file.sh
if HAVE_VALGRIND
TESTS +=  \
	imhttp-post-payload-vg.sh \
	imhttp-post-payload-basic-auth-vg.sh \
	imhttp-post-payload-query-params-vg.sh \
	imhttp-post-payload-large-vg.sh \
	imhttp-post-payload-multi-vg.sh \
	imhttp-post-payload-multi-lf-vg.sh \
	imhttp-post-payload-compress-vg.sh \
	imhttp-getrequest-file-vg.sh
endif # HAVE_VALGRIND
endif # ENABLE_IMHTTP

if ENABLE_OMRABBITMQ
check_PROGRAMS += miniamqpsrvr
miniamqpsrvr_SOURCES = miniamqpsrvr.c
miniamqpsrvr_CPPFLAGS = $(PTHREADS_CFLAGS) $(RABBITMQ_CFLAGS) $(RSRT_CFLAGS)
miniamqpsrvr_LDADD = $(SOL_LIBS) $(PTHREADS_LIBS)

TESTS +=  \
	omrabbitmq_no_params.sh \
	omrabbitmq_params_missing0.sh \
	omrabbitmq_params_missing1.sh \
	omrabbitmq_params_missing2.sh \
	omrabbitmq_params_invalid0.sh \
	omrabbitmq_params_invalid1.sh \
	omrabbitmq_params_invalid2.sh \
	omrabbitmq_params_invalid3.sh \
	omrabbitmq_data_1server.sh \
	omrabbitmq_data_2servers.sh \
	omrabbitmq_error_server0.sh \
	omrabbitmq_error_server1.sh \
	omrabbitmq_error_server2.sh \
	omrabbitmq_error_server3.sh \
	omrabbitmq_json.sh \
	omrabbitmq_raw.sh
if HAVE_VALGRIND
TESTS += \
	omrabbitmq_data_1server-vg.sh
endif # HAVE_VALGRIND
endif # ENABLE_OMRABBITMQ

if ENABLE_REDIS_TESTS
if ENABLE_IMHIREDIS
TESTS +=  \
	imhiredis-queue.sh \
	imhiredis-queue-lpop.sh \
	imhiredis-redis-restart.sh \
	imhiredis-redis-start-after.sh \
	imhiredis-subscribe.sh \
	imhiredis-stream.sh \
	imhiredis-stream-from-beginning.sh \
	imhiredis-stream-consumerGroup-ack.sh \
	imhiredis-stream-consumerGroup-noack.sh \
	imhiredis-stream-consumerGroup-reclaim.sh
if HAVE_VALGRIND
TESTS += \
	imhiredis-queue-vg.sh \
	imhiredis-queue-lpop-vg.sh \
	imhiredis-redis-restart-vg.sh \
	imhiredis-redis-start-after-vg.sh \
	imhiredis-subscribe-vg.sh \
	imhiredis-stream-vg.sh \
	imhiredis-stream-from-beginning-vg.sh \
	imhiredis-stream-consumerGroup-ack-vg.sh \
	imhiredis-stream-consumerGroup-noack-vg.sh \
	imhiredis-stream-consumerGroup-reclaim-vg.sh
endif # HAVE_VALGRIND
endif # ENABLE_IMHIREDIS

if ENABLE_OMHIREDIS
TESTS +=  \
	mmdb-reload.sh \
	omhiredis-dynakey.sh \
	omhiredis-publish.sh \
	omhiredis-queue-rpush.sh \
	omhiredis-queue.sh \
	omhiredis-set.sh \
	omhiredis-setex.sh \
	omhiredis-template.sh \
	omhiredis-withpass.sh \
	omhiredis-wrongpass.sh \
	omhiredis-stream-ack.sh \
	omhiredis-stream-capped.sh \
	omhiredis-stream-del.sh \
	omhiredis-stream-dynack.sh \
	omhiredis-stream-outfield.sh \
	omhiredis-stream.sh
if HAVE_VALGRIND
TESTS += \
	mmdb-reload-vg.sh \
	omhiredis-dynakey-vg.sh \
	omhiredis-publish-vg.sh \
	omhiredis-queue-rpush-vg.sh \
	omhiredis-queue-vg.sh \
	omhiredis-set-vg.sh \
	omhiredis-setex-vg.sh \
	omhiredis-template-vg.sh \
	omhiredis-withpass-vg.sh \
	omhiredis-wrongpass-vg.sh \
	omhiredis-stream-ack-vg.sh \
	omhiredis-stream-capped-vg.sh \
	omhiredis-stream-del-vg.sh \
	omhiredis-stream-dynack-vg.sh \
	omhiredis-stream-outfield-vg.sh \
	omhiredis-stream-vg.sh
endif # HAVE_VALGRIND
endif # ENABLE_OMHIREDIS
endif # ENABLE_REDIS_TESTS

if ENABLE_IMPSTATS
TESTS +=  \
	impstats-hup.sh \
	perctile-simple.sh \
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
	dynstats_prevent_premature_eviction.sh \
	omfwd-lb-2target-impstats.sh \
	omfwd_fast_imuxsock.sh \
	omfwd_impstats-udp.sh \
	omfwd_impstats-tcp.sh
if HAVE_VALGRIND
TESTS +=  \
	perctile-simple-vg.sh \
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
	imptcp_framing_regex.sh \
	imptcp_framing_regex-oversize.sh \
	imptcp_large.sh \
	imptcp-connection-msg-disabled.sh \
	imptcp-connection-msg-received.sh \
	imptcp-discard-truncated-msg.sh \
	imptcp_addtlframedelim.sh \
	imptcp_conndrop.sh \
	imptcp_no_octet_counted.sh \
	imptcp_multi_line.sh \
	imptcp_spframingfix.sh \
	imptcp_maxsessions.sh \
	imptcp_nonProcessingPoller.sh \
	imptcp_veryLargeOctateCountedMessages.sh \
	imptcp-basic-hup.sh \
	imptcp-NUL.sh \
	imptcp-NUL-rawmsg.sh \
	rscript_random.sh \
	rscript_hash32.sh \
	rscript_hash64.sh \
	rscript_replace.sh \
	omfile-sizelimitcmd-many.sh \
	omfile-outchannel-many.sh
if HAVE_VALGRIND
TESTS +=  \
	imptcp-octet-framing-too-long-vg.sh \
	imptcp_conndrop-vg.sh
if ENABLE_FMHASH
TESTS +=  \
	rscript_hash32-vg.sh \
	rscript_hash64-vg.sh
endif # ENABLE_FMHASH
endif # HAVE_VALGRIND
if ENABLE_FMUNFLATTEN
TESTS +=  \
	rscript_unflatten_arg1_unsuitable.sh \
	rscript_unflatten_arg2_invalid.sh \
	rscript_unflatten_conflict1.sh \
	rscript_unflatten_conflict2.sh \
	rscript_unflatten_conflict3.sh \
	rscript_unflatten_key_truncated.sh \
	rscript_unflatten_non_object.sh \
	rscript_unflatten_object.sh \
	rscript_unflatten_object_exclamation.sh
if HAVE_VALGRIND
TESTS +=  \
	rscript_unflatten_arg1_unsuitable-vg.sh \
	rscript_unflatten_arg2_invalid-vg.sh \
	rscript_unflatten_conflict1-vg.sh \
	rscript_unflatten_conflict2-vg.sh \
	rscript_unflatten_conflict3-vg.sh \
	rscript_unflatten_key_truncated-vg.sh \
	rscript_unflatten_non_object-vg.sh \
	rscript_unflatten_object-vg.sh \
	rscript_unflatten_object_exclamation-vg.sh
endif # HAVE_VALGRIND
endif # ENABLE_FMUNFLATTEN
if ENABLE_FFAUP
TESTS +=  \
	rscript_faup_all.sh \
	rscript_faup_all_2.sh \
	rscript_faup_all_empty.sh \
	rscript_faup_scheme.sh \
	rscript_faup_credential.sh \
	rscript_faup_subdomain.sh \
	rscript_faup_domain.sh \
	rscript_faup_domain_without_tld.sh \
	rscript_faup_host.sh \
	rscript_faup_tld.sh \
	rscript_faup_port.sh \
	rscript_faup_resource_path.sh \
	rscript_faup_query_string.sh \
	rscript_faup_fragment.sh \
	rscript_faup_mozilla_tld.sh
if HAVE_VALGRIND
TESTS +=  \
	rscript_faup_all_vg.sh \
	rscript_faup_all_2_vg.sh \
	rscript_faup_all_empty_vg.sh \
	rscript_faup_scheme_vg.sh \
	rscript_faup_credential_vg.sh \
	rscript_faup_subdomain_vg.sh \
	rscript_faup_domain_vg.sh \
	rscript_faup_domain_without_tld_vg.sh \
	rscript_faup_host_vg.sh \
	rscript_faup_tld_vg.sh \
	rscript_faup_port_vg.sh \
	rscript_faup_resource_path_vg.sh \
	rscript_faup_query_string_vg.sh \
	rscript_faup_fragment_vg.sh \
	rscript_faup_mozilla_tld_vg.sh
endif # HAVE_VALGRIND
endif # ENABLE_FFAUP
endif

if ENABLE_MMPSTRUCDATA
TESTS +=  \
	mmpstrucdata.sh \
	mmpstrucdata-escaping.sh \
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
	pmnormalize-invld-rulebase.sh \
	pmnormalize-rule.sh \
	pmnormalize-rule_and_rulebase.sh \
	pmnormalize-neither_rule_rulebase.sh \
	pmnormalize-rule_invld-data.sh
if HAVE_VALGRIND
TESTS += \
	pmnormalize-basic-vg.sh \
	pmnormalize-invld-rulebase-vg.sh \
	pmnormalize-rule-vg.sh \
	pmnormalize-rule_and_rulebase-vg.sh \
	pmnormalize-neither_rule_rulebase-vg.sh \
	pmnormalize-rule_invld-data-vg.sh
endif
endif

if ENABLE_MMNORMALIZE
TESTS += msgvar-concurrency-array.sh \
	msgvar-concurrency-array-event.tags.sh \
	mmnormalize_rule_from_string.sh \
	mmnormalize_rule_from_array.sh \
	mmnormalize_parsesuccess.sh

if HAVE_VALGRIND
TESTS += \
	mmnormalize_parsesuccess-vg.sh
endif

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
endif # ENABLE_MMNORMALIZE

if ENABLE_MMJSONPARSE
TESTS += \
	mmjsonparse-w-o-cookie.sh \
	mmjsonparse-w-o-cookie-multi-spaces.sh
if ENABLE_IMPSTATS
TESTS += \
	mmjsonparse-invalid-containerName.sh \
	wtpShutdownAll-assertionFailure.sh
endif
if ENABLE_IMPTCP
TESTS +=  \
	mmjsonparse_simple.sh \
	imptcp-oversize-message-display.sh \
	imptcp-msg-truncation-on-number.sh \
	imptcp-msg-truncation-on-number2.sh \
	imptcp-maxFrameSize-parameter.sh \
	mmjsonparse_cim.sh \
	mmjsonparse_cim2.sh \
	mmjsonparse_localvar.sh \
	json_array_subscripting.sh \
	json_array_looping.sh \
	json_object_looping.sh \
	json_nonarray_looping.sh
endif
if HAVE_VALGRIND
TESTS +=  \
	mmjsonparse_extra_data-vg.sh \
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
    mmdb-space.sh \
    mmdb-container.sh \
    mmdb-container-empty.sh
if HAVE_VALGRIND
TESTS += \
    mmdb-vg.sh \
    mmdb-multilevel-vg.sh
endif
endif

if ENABLE_GNUTLS_TESTS
TESTS +=  \
	imtcp-tls-basic.sh \
	imtcp-tls-input-basic.sh \
	imtcp-tls-input-2certs.sh \
	imtcp-tls-basic-verifydepth.sh \
	imtcp_conndrop_tls.sh \
	sndrcv_tls_anon_rebind.sh \
	sndrcv_tls_anon_hostname.sh \
	sndrcv_tls_anon_ipv4.sh \
	sndrcv_tls_anon_ipv6.sh \
	sndrcv_tls_certless_clientonly.sh \
	sndrcv_tls_gtls_servercert_gtls_clientanon.sh \
	sndrcv_tls_gtls_servercert_gtls_clientanon_legacy.sh \
	sndrcv_tls_gtls_serveranon_gtls_clientanon.sh \
	sndrcv_tls_priorityString.sh \
	sndrcv_tls_certvalid.sh \
	sndrcv_tls_certvalid_action_level.sh \
	sndrcv_tls_certvalid_expired.sh \
	sndrcv_tls_certvalid_expired_defaultmode.sh \
	sndrcv_tls_certvalid_revoked.sh \
	sndrcv_tls_client_missing_cert.sh \
	imtcp-tls-no-lstn-startup.sh \
	imtcp-tls-gtls-x509fingerprint-invld.sh \
	imtcp-tls-gtls-x509fingerprint.sh \
	imtcp-tls-gtls-x509name-invld.sh \
	imtcp-tls-gtls-x509name.sh \
	imtcp-tls-gtls-x509name-legacy.sh \
	imtcp-drvr-in-input-basic.sh \
	imtcp-multi-drvr-basic.sh \
	imtcp-multi-drvr-basic-parallel.sh
if HAVE_VALGRIND
TESTS += \
	imtcp-tls-basic-vg.sh \
	imtcp_conndrop_tls-vg.sh \
	manytcp-too-few-tls-vg.sh
endif
if ENABLE_OPENSSL
TESTS +=  \
	imtcp-multi-drvr-basic-ptcp_gtls_ossl.sh
endif
endif

if ENABLE_OPENSSL
TESTS +=  \
	imtcp-tls-ossl-basic.sh \
	imtcp-tls-ossl-input-basic.sh \
	imtcp-tls-ossl-input-2certs.sh \
	imtcp-tls-ossl-basic-tlscommands.sh \
	imtcp-tls-ossl-basic-verifydepth.sh \
	imtcp-tls-ossl-invalid-verifydepth.sh \
	sndrcv_tls_ossl_anon_ipv4.sh \
	sndrcv_tls_ossl_anon_ipv6.sh \
	sndrcv_tls_ossl_anon_rebind.sh \
	sndrcv_tls_ossl_anon_ciphers.sh \
	sndrcv_tls_ossl_serveranon_ossl_clientanon.sh \
	sndrcv_tls_ossl_servercert_ossl_clientanon.sh \
	sndrcv_tls_ossl_certvalid.sh \
	sndrcv_tls_ossl_certvalid_action_level.sh \
	sndrcv_tls_ossl_certvalid_expired.sh \
	sndrcv_tls_ossl_certvalid_tlscommand.sh \
	sndrcv_tls_ossl_certvalid_ciphers.sh \
	sndrcv_tls_ossl_certvalid_revoked.sh \
	imtcp-tls-ossl-x509valid.sh \
	imtcp-tls-ossl-x509name.sh \
	imtcp-tls-ossl-x509fingerprint.sh \
	imtcp-tls-ossl-error-ca.sh \
	imtcp-tls-ossl-error-cert.sh \
	imtcp-tls-ossl-error-key.sh \
	imtcp-tls-ossl-error-key2.sh
#	imtcp-tls-ossl-basic-stress.sh
if HAVE_VALGRIND
TESTS += \
	imtcp-tls-ossl-basic-vg.sh \
	imtcp-tls-ossl-basic-brokenhandshake-vg.sh
endif
endif

if ENABLE_GNUTLS_TESTS
if ENABLE_OPENSSL
TESTS +=  \
	sndrcv_tls_ossl_servercert_gtls_clientanon.sh \
	sndrcv_tls_ossl_serveranon_gtls_clientanon.sh \
	sndrcv_tls_gtls_servercert_ossl_clientanon.sh \
	sndrcv_tls_gtls_serveranon_ossl_clientanon.sh
endif
endif

if ENABLE_OMUXSOCK
TESTS += uxsock_simple.sh
endif

if ENABLE_RELP
TESTS += sndrcv_relp.sh \
	 sndrcv_relp_rebind.sh \
	 omrelp_errmsg_no_connect.sh \
	 imrelp-basic.sh \
	 imrelp-basic-oldstyle.sh \
	 imrelp-basic-hup.sh \
	 imrelp-manyconn.sh \
	 imrelp-maxDataSize-error.sh \
	 imrelp-long-msg.sh \
	 imrelp-oversizeMode-truncate.sh \
	 imrelp-oversizeMode-accept.sh \
	 imrelp-invld-tlslib.sh \
	 imrelp-bigmessage.sh \
	 omrelp-invld-tlslib.sh \
	 sndrcv_relp_dflt_pt.sh \
	 glbl-oversizeMsg-log.sh \
	 glbl-oversizeMsg-truncate.sh \
	 glbl-oversizeMsg-split.sh
if ENABLE_GNUTLS
TESTS += \
         sndrcv_relp_tls.sh \
         sndrcv_relp_tls_certvalid.sh \
	 sndrcv_tls_certvalid_action_level.sh \
         sndrcv_relp_tls_prio.sh \
	 sndrcv_relp_tls_chainedcert.sh \
         relp_tls_certificate_not_found.sh \
	 omrelp_wrong_authmode.sh \
	 imrelp-tls.sh \
	 imrelp-tls-chainedcert.sh \
	 imrelp-tls-mixed-chainedcert.sh \
	 imrelp-tls-mixed-chainedcert2.sh
if USE_RELPENGINESETTLSCFGCMD
TESTS += \
	 imrelp-tls-cfgcmd.sh \
	 sndrcv_relp_tls-cfgcmd.sh
endif # USE_RELPENGINESETTLSCFGCMD
endif # ENABLE_GNUTLS
if HAVE_VALGRIND
TESTS += \
	 imrelp-basic-vg.sh \
	 imrelp-sessionbreak-vg.sh \
	 imrelp-manyconn-vg.sh \
	 sndrcv_relp-vg-rcvr.sh \
	 sndrcv_relp-vg-sender.sh
endif # HAVE_VALGRIND
endif

if ENABLE_IMDTLS
TESTS += \
	imdtls-basic.sh \
	imdtls-basic-tlscommands.sh \
	imdtls-basic-timeout.sh \
	imdtls-error-cert.sh \
	imdtls-sessionbreak.sh
if HAVE_VALGRIND
TESTS += \
	imdtls-basic-vg.sh \
	imdtls-sessionbreak-vg.sh
endif # HAVE_VALGRIND
if ENABLE_OMDTLS
TESTS += \
	sndrcv_dtls_certvalid.sh \
	sndrcv_dtls_anon_ciphers.sh \
	sndrcv_dtls_certvalid_ciphers.sh \
	sndrcv_dtls_certvalid_permitted.sh \
	sndrcv_dtls_certvalid_missing.sh \
	sndrcv_dtls_anon_ciphers.sh
if HAVE_VALGRIND
TESTS += \
	sndrcv_dtls_certvalid-vg.sh
endif # HAVE_VALGRIND
endif # ENABLE_OMDTLS
endif # ENABLE_IMDTLS

if ENABLE_OMUDPSPOOF
TESTS += \
	omudpspoof_errmsg_no_params.sh \
	sndrcv_omudpspoof.sh \
	sndrcv_omudpspoof-bigmsg.sh \
	sndrcv_omudpspoof_nonstdpt.sh
endif

#disabled as array-passing mode is no longer supported:	omod-if-array.sh
#	omod-if-array.sh
#	omod-if-array-udp.sh
if ENABLE_IMPTCP
TESTS += \
	tabescape_dflt.sh \
	tabescape_dflt-udp.sh \
	tabescape_off.sh \
	tabescape_off-udp.sh \
	tabescape_on.sh \
	inputname-imtcp.sh \
	fieldtest.sh \
	fieldtest-udp.sh \
	proprepltest-nolimittag-udp.sh \
	proprepltest-nolimittag.sh \
	proprepltest-rfctag-udp.sh \
	proprepltest-rfctag.sh

endif

if ENABLE_OMRULESET
TESTS += \
	omruleset.sh \
	omruleset-queue.sh
endif

if ENABLE_PMDB2DIAG
TESTS += \
	pmdb2diag_parse.sh
endif

if ENABLE_PMSNARE
TESTS += \
	pmsnare-default.sh \
	pmsnare-default-udp.sh \
	pmsnare-ccoff.sh \
	pmsnare-ccoff-udp.sh \
	pmsnare-ccdefault.sh \
	pmsnare-ccdefault-udp.sh \
	pmsnare-cccstyle.sh \
	pmsnare-cccstyle-udp.sh \
	pmsnare-ccbackslash.sh \
	pmsnare-ccbackslash-udp.sh \
	pmsnare-modoverride.sh \
	pmsnare-modoverride-udp.sh
endif

if ENABLE_PMLASTMSG
TESTS += \
	pmlastmsg.sh \
	pmlastmsg-udp.sh
endif

if ENABLE_EXTENDED_TESTS
# random.sh is temporarily disabled as it needs some work
# to rsyslog core to complete in reasonable time
#TESTS += random.sh
if ENABLE_IMFILE_TESTS
TESTS += \
	imfile-basic-2GB-file.sh \
	imfile-truncate-2GB-file.sh
endif # ENABLE_IMFILE_TESTS
endif

if ENABLE_IMFILE_TESTS
TESTS += \
	imfile-basic.sh \
	imfile-basic-legacy.sh \
	imfile-discard-truncated-line.sh \
	imfile-truncate-line.sh \
	imfile-file-not-found-error.sh \
	imfile-fileNotFoundError-parameter.sh \
	imfile-error-not-repeated.sh \
	imfile-truncate.sh \
	imfile-truncate-multiple.sh \
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
	imfile-endmsg.regex.sh \
	imfile-escapelf.replacement.sh \
	imfile-escapelf.replacement-empty.sh \
	imfile-statefile-no-file_id.sh \
	imfile-statefile-no-file_id-TO-file_id.sh \
	imfile-statefile-directory.sh \
	imfile-statefile-delete.sh \
	imfile-statefile-no-delete.sh \
	imfile-persist-state-1.sh \
	imfile-freshStartTail1.sh \
	imfile-freshStartTail2.sh \
	imfile-freshStartTail3.sh \
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
	imfile-symlink.sh \
	imfile-symlink-multi.sh \
	imfile-symlink-ext-tmp-dir-tree.sh \
	imfile-logrotate.sh \
	imfile-logrotate-async.sh \
	imfile-logrotate-multiple.sh \
	imfile-logrotate-copytruncate.sh \
	imfile-logrotate-nocopytruncate.sh \
	imfile-growing-file-id.sh \
	imfile-ignore-old-file-1.sh \
	imfile-ignore-old-file-2.sh \
	imfile-ignore-old-file-3.sh \
	imfile-ignore-old-file-4.sh \
	imfile-ignore-old-file-5.sh \
	imfile-ignore-old-file-6.sh \
	imfile-ignore-old-file-7.sh \
	glbl-oversizeMsg-truncate-imfile.sh \
	config_enabled-on.sh \
	config_enabled-off.sh

if ENABLE_MMNORMALIZE
TESTS += \
	imfile-endmsg.regex-with-example.sh
endif

if HAVE_VALGRIND
TESTS += \
	imfile-basic-vg.sh \
	imfile-endregex-vg.sh \
	imfile-endmsg.regex-vg.sh \
	imfile-readmode0-vg.sh \
	imfile-readmode2-vg.sh

if ENABLE_HELGRIND
TESTS += \
	imfile-basic-vgthread.sh
endif #  ENABLE_HELGRIND

if ENABLE_MMNORMALIZE
TESTS += \
	imfile-endmsg.regex-with-example-vg.sh
endif # ENABLE_MMNORMALIZE

endif # HAVE_VALGRIND

endif # ENABLE_IMFILE_TESTS

if ENABLE_IMBATCHREPORT
TESTS +=  \
	imbatchreport_errmsg_no_params.sh \
	imbatchreport_errmsg_glob_not_regular.sh \
	imbatchreport_errmsg_glob_dir_fake.sh \
	imbatchreport_errmsg_glob_dir_not_dir.sh \
	imbatchreport_errmsg_regex.match.reject.sh \
	imbatchreport_errmsg_regex.match.rename.sh \
	imbatchreport_errmsg_regex.nomatch.sh \
	imbatchreport_errmsg_not_supported1.sh \
	imbatchreport_errmsg_not_supported2.sh \
	imbatchreport_errmsg_not_supported3.sh \
	imbatchreport_errmsg_delete_params.sh \
	imbatchreport_errmsg_rename_params.sh \
	imbatchreport_delete_success.sh \
	imbatchreport_delete_structdata.sh \
	imbatchreport_rename_success.sh \
	imbatchreport_delete_toolarge.sh \
	imbatchreport_rename_toolarge.sh
if HAVE_VALGRIND
TESTS +=  \
	imbatchreport_errmsg_no_params-vg.sh
endif # ENABLE_IMBATCHREPORT
endif

if ENABLE_OMTCL
TESTS += \
	 omtcl.sh
endif

if ENABLE_MMTAGHOSTNAME
TESTS += \
	mmtaghostname_tag.sh \
	mmtaghostname_server.sh
endif

if ENABLE_MMKUBERNETES
if ENABLE_MMJSONPARSE
if ENABLE_IMFILE
if ENABLE_IMPSTATS
TESTS += \
	mmkubernetes-basic.sh \
	mmkubernetes-cache-expire.sh
if HAVE_VALGRIND
TESTS += \
	mmkubernetes-basic-vg.sh \
	mmkubernetes-cache-expire-vg.sh
endif
endif
endif
endif
endif

if ENABLE_IMTUXEDOULOG
TESTS +=  \
	imtuxedoulog_errmsg_no_params.sh \
	imtuxedoulog_data.sh
if HAVE_VALGRIND
TESTS +=  \
	imtuxedoulog_errmsg_no_params-vg.sh
endif # ENABLE_IMTUXEDOULOG
endif

if ENABLE_OMAMQP1
TESTS += \
	omamqp1-basic.sh
if HAVE_VALGRIND
TESTS +=  \
	omamqp1-basic-vg.sh
endif
endif # ENABLE_OMAMQP1

endif # if ENABLE_TESTBENCH

TESTS_ENVIRONMENT = RSYSLOG_MODDIR='$(abs_top_builddir)'/runtime/.libs/
TESTS_ENVIRONMENT+= TOP_BUILDDIR='$(top_builddir)'
TESTS_ENVIRONMENT+= TESTTOOL_DIR='$(abs_top_builddir)/tests'
test_files = testbench.h runtime-dummy.c
DISTCLEANFILES=rsyslog.pid

distclean-local:
	rm -rf .dep_cache .dep_wrk

EXTRA_DIST= \
	set-envvars.in \
	urlencode.py \
	dnscache-TTL-0.sh \
	dnscache-TTL-0-vg.sh \
	loadbalance.sh \
	smtradfile.sh \
	smtradfile-vg.sh \
	immark.sh \
	immark-inputname.sh \
	immark-ruleset.sh \
	immark-ruleset-custom-msg.sh \
	operatingstate-basic.sh \
	operatingstate-empty.sh \
	operatingstate-unclean.sh \
	internal-errmsg-memleak-vg.sh \
	glbl-ruleset-queue-defaults.sh \
	glbl-internalmsg_severity-info-shown.sh \
	glbl-internalmsg_severity-debug-shown.sh \
	glbl-internalmsg_severity-debug-not_shown.sh \
	glbl-oversizeMsg-log-vg.sh \
	config_enabled-on.sh \
	config_enabled-off.sh \
	empty-app-name.sh \
	empty-hostname.sh \
	func-substring-invld-startpos.sh \
	func-substring-invld-startpos-vg.sh \
	func-substring-large-endpos.sh \
	func-substring-large-neg-endpos.sh \
	func-substring-relative-endpos.sh \
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
	imtuxedoulog_errmsg_no_params.sh \
	imtuxedoulog_data.sh \
	imtuxedoulog_errmsg_no_params-vg.sh \
	pmdb2diag_parse.sh \
	mmtaghostname_tag.sh \
	mmtaghostname_server.sh \
	imbatchreport_errmsg_no_params.sh \
	imbatchreport_errmsg_glob_not_regular.sh \
	imbatchreport_errmsg_glob_dir_fake.sh \
	imbatchreport_errmsg_glob_dir_not_dir.sh \
	imbatchreport_errmsg_regex.match.reject.sh \
	imbatchreport_errmsg_regex.match.rename.sh \
	imbatchreport_errmsg_regex.nomatch.sh \
	imbatchreport_errmsg_not_supported1.sh \
	imbatchreport_errmsg_not_supported2.sh \
	imbatchreport_errmsg_not_supported3.sh \
	imbatchreport_errmsg_delete_params.sh \
	imbatchreport_errmsg_rename_params.sh \
	imbatchreport_delete_success.sh \
	imbatchreport_delete_structdata.sh \
	imbatchreport_rename_success.sh \
	imbatchreport_delete_toolarge.sh \
	imbatchreport_rename_toolarge.sh \
	imbatchreport_errmsg_no_params-vg.sh \
	mmexternal-SegFault.sh \
	mmexternal-SegFault-empty-jroot-vg.sh \
	testsuites/mmexternal-SegFault-mm-python.py \
	mmexternal-InvldProg-vg.sh \
	nested-call-shutdown.sh \
	1.rstest 2.rstest 3.rstest err1.rstest \
	config_multiple_include.sh \
	testsuites/include-std1-omfile-action.conf \
	testsuites/include-std2-omfile-action.conf \
	invalid_nested_include.sh \
	validation-run.sh \
	tls-certs/ca-key.pem \
	tls-certs/ca.pem \
	tls-certs/cert.pem \
	tls-certs/certchained.pem \
	tls-certs/key.pem \
	tls-certs/ca-fail.pem \
	tls-certs/cert-fail.pem \
	tls-certs/key-fail.pem \
	testsuites/x.509/ca.srl \
	testsuites/x.509/client-cert-new.pem \
	testsuites/x.509/client-new.csr \
	testsuites/x.509/client-revoked-key.pem \
	testsuites/x.509/client-revoked-valid.pem \
	testsuites/x.509/client-revoked.csr \
	testsuites/x.509/client-revoked.pem \
	testsuites/x.509/crl.pem \
	testsuites/x.509/index.txt \
	testsuites/x.509/index.txt.attr \
	testsuites/x.509/newcerts/01.pem \
	testsuites/x.509/newcerts/02.pem \
	testsuites/x.509/newcerts/03.pem \
	testsuites/x.509/newcerts/04.pem \
	testsuites/x.509/openssl-cmds.sh \
	testsuites/x.509/openssl.cnf \
	testsuites/x.509/serial \
	testsuites/x.509/ca.pem \
	testsuites/x.509/ca-key.pem \
	testsuites/x.509/client-cert.pem \
	testsuites/x.509/client-key.pem \
	testsuites/x.509/machine-cert.pem \
	testsuites/x.509/machine-key.pem \
	testsuites/x.509/client-expired-cert.pem \
	testsuites/x.509/client-expired-key.pem \
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
	tcp_forwarding_tpl.sh \
	tcp_forwarding_ns_tpl.sh \
	tcp_forwarding_dflt_tpl.sh \
	tcp_forwarding_retries.sh \
	mainq_actq_DA.sh \
	queue_warnmsg-oversize.sh \
	queue-minbatch.sh \
	queue-minbatch-queuefull.sh \
	queue-direct-with-no-params.sh \
	queue-direct-with-params-given.sh \
	killrsyslog.sh \
	parsertest-parse1.sh \
	parsertest-parse1-udp.sh \
	parsertest-parse2.sh \
	parsertest-parse2-udp.sh \
	parsertest-parse_8bit_escape.sh \
	parsertest-parse_8bit_escape-udp.sh \
	parsertest-parse3.sh \
	parsertest-parse3-udp.sh \
	parsertest-parse_invld_regex.sh \
	parsertest-parse_invld_regex-udp.sh \
	parsertest-parse-3164-buggyday.sh \
	parsertest-parse-3164-buggyday-udp.sh \
	parsertest-parse-nodate.sh \
	parsertest-parse-nodate-udp.sh \
	parsertest-snare_ccoff_udp.sh \
	parsertest-snare_ccoff_udp2.sh \
	fieldtest.sh \
	fieldtest-udp.sh \
	proprepltest-nolimittag-udp.sh \
	proprepltest-nolimittag.sh \
	proprepltest-rfctag-udp.sh \
	proprepltest-rfctag.sh \
	timestamp-3164.sh \
	timestamp-3339.sh \
	timestamp-isoweek.sh \
	timestamp-mysql.sh \
	timestamp-pgsql.sh \
	timestamp-subseconds.sh \
	rsf_getenv.sh \
	diskq-rfc5424.sh \
	rfc5424parser-sp_at_msg_start.sh \
	diskqueue-full.sh \
	diskqueue-fail.sh \
	diskqueue.sh \
	diskqueue-non-unique-prefix.sh \
	arrayqueue.sh \
	include-obj-text-from-file.sh \
	include-obj-text-from-file-noexist.sh \
	include-obj-outside-control-flow-vg.sh \
	include-obj-in-if-vg.sh \
	include-obj-text-vg.sh \
	config_output-o-option.sh \
	rscript-config_enable-off-vg.sh \
	rscript-config_enable-on.sh \
	rscript_http_request.sh \
	rscript_http_request-vg.sh \
	rscript_bare_var_root.sh \
	rscript_bare_var_root-empty.sh \
	rscript_contains.sh \
	rscript_ipv42num.sh \
	rscript_field.sh \
	rscript_field-vg.sh \
	rscript_stop.sh \
	rscript_stop2.sh \
	stop.sh \
	rscript_le.sh \
	rscript_le_var.sh \
	rscript_ge.sh \
	rscript_ge_var.sh \
	rscript_lt.sh \
	rscript_lt_var.sh \
	rscript_gt.sh \
	rscript_gt_var.sh \
	rscript_ne.sh \
	rscript_ne_var.sh \
	rscript_number_comparison_LE.sh \
	rscript_number_comparison_LE-vg.sh \
	rscript_number_comparison_LT.sh \
	rscript_compare_str-numstr.sh \
	rscript_compare_str-num.sh \
	rscript_compare_numstr-str.sh \
	rscript_compare_num-str.sh \
	rscript_compare_numstr-numstr.sh \
	rscript_compare_numstr-num.sh \
	rscript_compare_num-numstr.sh \
	rscript_compare_num-num.sh \
	rscript_compare_str-str.sh \
	rscript_compare_str-str-vg.sh \
	rscript_compare_str-num-vg.sh \
	rscript_compare_str-numstr-vg.sh \
	rscript_compare_num-str-vg.sh \
	rscript_compare_numstr-str-vg.sh \
	rscript_compare_numstr-num-vg.sh \
	rscript_compare_numstr-numstr-vg.sh \
	rscript_compare_num-num-vg.sh \
	rscript_compare_num-numstr-vg.sh \
	rscript_compare-common.sh \
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
	rscript_backticks_empty_envvar-vg.sh \
	rscript_previous_action_suspended.sh \
	rscript_str2num_negative.sh \
	rscript_exists-yes.sh \
	rscript_exists-yes2.sh \
	rscript_exists-not1.sh \
	rscript_exists-not2.sh \
	rscript_exists-not3.sh \
	rscript_exists-not4.sh \
	rscript_unflatten_arg1_unsuitable.sh \
	rscript_unflatten_arg2_invalid.sh \
	rscript_unflatten_conflict1.sh \
	rscript_unflatten_conflict2.sh \
	rscript_unflatten_conflict3.sh \
	rscript_unflatten_key_truncated.sh \
	rscript_unflatten_non_object.sh \
	rscript_unflatten_object_exclamation.sh \
	rscript_unflatten_object.sh \
	rscript_unflatten_arg1_unsuitable-vg.sh \
	rscript_unflatten_arg2_invalid-vg.sh \
	rscript_unflatten_conflict1-vg.sh \
	rscript_unflatten_conflict2-vg.sh \
	rscript_unflatten_conflict3-vg.sh \
	rscript_unflatten_key_truncated-vg.sh \
	rscript_unflatten_non_object-vg.sh \
	rscript_unflatten_object_exclamation-vg.sh \
	rscript_unflatten_object-vg.sh \
	rscript_get_property.sh \
	rscript_get_property-vg.sh \
	rs-cnum.sh \
	rs-int2hex.sh \
	rs-substring.sh \
	omsnmp_errmsg_no_params.sh \
	sndrcv_omsnmpv1_udp.sh \
	sndrcv_omsnmpv1_udp_dynsource.sh \
	sndrcv_omsnmpv1_udp_invalidoid.sh \
	snmptrapreceiver.py \
	ommail_errmsg_no_params.sh \
	mmdarwin_errmsg_no_params.sh \
	mmdarwin_errmsg_no_sock.sh \
	mmdarwin_errmsg_no_sock-vg.sh \
	mmutf8fix_no_error.sh \
	tcpflood_wrong_option_output.sh \
	msleep_usage_output.sh \
	mangle_qi_usage_output.sh \
	minitcpsrv_usage_output.sh \
	test_id_usage_output.sh \
	mmanon_with_debug.sh \
	mmanon_random_32_ipv4.sh \
	mmanon_random_cons_32_ipv4.sh \
	mmanon_recognize_ipv4.sh \
	mmanon_zero_12_ipv4.sh \
	mmanon_zero_33_ipv4.sh \
	mmanon_zero_8_ipv4.sh \
	mmanon_simple_12_ipv4.sh \
	mmanon_simple_33_ipv4.sh \
	mmanon_simple_8_ipv4.sh \
	mmanon_simple_mallformed_ipv4.sh \
	mmanon_random_128_ipv6.sh \
	mmanon_zero_128_ipv6.sh \
	mmanon_zero_96_ipv6.sh \
	mmanon_random_cons_128_ipv6.sh \
	mmanon_zero_50_ipv6.sh \
	mmanon_recognize_ipv6.sh \
	mmanon_zero_64_ipv6.sh \
	mmanon_both_modes_compatible.sh \
	mmanon_recognize_ipembedded.sh \
	mmanon_ipv6_port.sh \
	mmanon_random_cons_128_ipembedded.sh \
	rscript_eq.sh \
	rscript_eq_var.sh \
	rscript_set_memleak-vg.sh \
	rscript_set_unset_invalid_var.sh \
	rscript_set_modify.sh \
	stop-localvar.sh \
	stop-msgvar.sh \
	omfwd-lb-1target-retry-full_buf.sh \
	omfwd-lb-1target-retry-1_byte_buf.sh \
	omfwd-lb-1target-retry-test_skeleton.sh \
	omfwd-lb-1target-retry-1_byte_buf-TargetFail.sh \
	omfwd-lb-1target-retry-test_skeleton-TargetFail.sh \
	omfwd-lb-susp.sh \
	omfwd-lb-2target-basic.sh \
	omfwd-lb-2target-impstats.sh \
	omfwd-lb-2target-retry.sh \
	omfwd-lb-2target-one_fail.sh \
	omfwd-tls-invalid-permitExpiredCerts.sh \
	omfwd-keepalive.sh \
	omfwd_fast_imuxsock.sh \
	omfile_hup-vg.sh \
	zstd.sh \
	zstd-vg.sh \
	gzipwr_hup-vg.sh \
	omusrmsg-errmsg-no-params.sh \
	omusrmsg-noabort.sh \
	omusrmsg-noabort-vg.sh \
	omfile-module-params.sh \
	omfile-read-only-errmsg.sh \
	omfile-null-filename.sh \
	omfile-whitespace-filename.sh \
	omfile-read-only.sh \
	omfile-outchannel.sh \
	omfile-outchannel-many.sh \
	omfile-sizelimitcmd-many.sh \
	omfile_both_files_set.sh \
	omfile_hup.sh \
	omrabbitmq_no_params.sh \
	omrabbitmq_params_missing0.sh \
	omrabbitmq_params_missing1.sh \
	omrabbitmq_params_missing2.sh \
	omrabbitmq_params_invalid0.sh \
	omrabbitmq_params_invalid1.sh \
	omrabbitmq_params_invalid2.sh \
	omrabbitmq_params_invalid3.sh \
	omrabbitmq_data_1server.sh \
	omrabbitmq_data_1server-vg.sh \
	omrabbitmq_data_2servers.sh \
	omrabbitmq_error_server0.sh \
	omrabbitmq_error_server1.sh \
	omrabbitmq_error_server2.sh \
	omrabbitmq_error_server3.sh \
	omrabbitmq_json.sh \
	omrabbitmq_raw.sh \
	imhiredis-queue.sh \
	imhiredis-queue-vg.sh \
	imhiredis-queue-lpop.sh \
	imhiredis-queue-lpop-vg.sh \
	imhiredis-redis-restart.sh \
	imhiredis-redis-restart-vg.sh \
	imhiredis-redis-start-after.sh \
	imhiredis-redis-start-after-vg.sh \
	imhiredis-subscribe.sh \
	imhiredis-subscribe-vg.sh \
	imhiredis-stream.sh \
	imhiredis-stream-vg.sh \
	imhiredis-stream-from-beginning.sh \
	imhiredis-stream-from-beginning-vg.sh \
	imhiredis-stream-consumerGroup-ack.sh \
	imhiredis-stream-consumerGroup-ack-vg.sh \
	imhiredis-stream-consumerGroup-noack.sh \
	imhiredis-stream-consumerGroup-noack-vg.sh \
	imhiredis-stream-consumerGroup-reclaim.sh \
	imhiredis-stream-consumerGroup-reclaim-vg.sh \
	msgvar-concurrency.sh \
	msgvar-concurrency-array.sh \
	testsuites/msgvar-concurrency-array.rulebase \
	msgvar-concurrency-array-event.tags.sh \
	testsuites/msgvar-concurrency-array-event.tags.rulebase \
	localvar-concurrency.sh \
	exec_tpl-concurrency.sh \
	prop-jsonmesg-vg.sh \
	prop-all-json-concurrency.sh \
	no-parser-errmsg.sh \
	global_vars.sh \
	no-parser-errmsg.sh \
	no-parser-vg.sh \
	prop-programname.sh \
	prop-programname-with-slashes.sh \
	rfc5424parser.sh \
	rscript_privdropuser.sh \
	rscript_privdropuserid.sh \
	rscript_privdropgroup.sh \
	rscript_privdropgroupid.sh \
	privdrop_common.sh \
	privdropuser.sh \
	privdropuserid.sh \
	privdropgroup.sh \
	privdropgroupid.sh \
	privdropabortonidfail.sh \
	privdropabortonidfaillegacy.sh \
	json-nonstring.sh \
        json-onempty-at-end.sh \
	template-json.sh \
	template-pure-json.sh \
	template-pos-from-to.sh \
	template-pos-from-to-lowercase.sh \
	template-pos-from-to-oversize.sh \
	template-pos-from-to-oversize-lowercase.sh \
	template-pos-from-to-missing-jsonvar.sh \
	template-const-jsonf.sh \
	template-topos-neg.sh \
	fac_authpriv.sh \
	fac_local0.sh \
	fac_local0-vg.sh \
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
	now_family_utc.sh \
	now-utc-ymd.sh \
	now-utc-casecmp.sh \
	now-utc.sh \
	now-unixtimestamp.sh \
	faketime_common.sh \
	imjournal-basic.sh \
	imjournal-statefile.sh \
	imjournal-statefile-vg.sh \
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
	mmnormalize_parsesuccess.sh \
	mmnormalize_parsesuccess-vg.sh \
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
	pmnormalize-basic.sh \
	pmnormalize-rule.sh \
	pmnormalize-rule_and_rulebase.sh \
	pmnormalize-neither_rule_rulebase.sh \
	pmnormalize-invld-rulebase.sh \
	pmnormalize-rule_invld-data.sh \
	testsuites/pmnormalize_basic.rulebase \
	pmnormalize-basic-vg.sh \
	pmnormalize-rule-vg.sh\
	pmnormalize-rule_and_rulebase-vg.sh \
	pmnormalize-neither_rule_rulebase-vg.sh \
	pmnormalize-invld-rulebase-vg.sh \
	pmnormalize-rule_invld-data-vg.sh \
	rawmsg-after-pri.sh \
	rs_optimizer_pri.sh \
	rscript_prifilt.sh \
	rscript_optimizer1.sh \
	rscript_ruleset_call.sh \
	rscript_ruleset_call_indirect-basic.sh \
	rscript_ruleset_call_indirect-var.sh \
	rscript_ruleset_call_indirect-invld.sh \
	cee_simple.sh \
	cee_diskqueue.sh \
	mmjsonparse-w-o-cookie.sh \
	mmjsonparse-w-o-cookie-multi-spaces.sh \
	mmjsonparse_simple.sh \
	mmjsonparse-invalid-containerName.sh \
	wtpShutdownAll-assertionFailure.sh \
	imptcp-octet-framing-too-long-vg.sh \
	imptcp-oversize-message-display.sh \
	imptcp-msg-truncation-on-number.sh \
	imptcp-msg-truncation-on-number2.sh \
	imptcp-maxFrameSize-parameter.sh \
	mmjsonparse_cim.sh \
	mmjsonparse_cim2.sh \
	mmjsonparse_localvar.sh \
	mmdb.sh \
	mmdb-space.sh \
	mmdb.rb \
	test.mmdb \
	with_space.mmdb \
	mmdb-vg.sh \
	mmdb-container.sh \
	mmdb-container-empty.sh \
	mmdb-multilevel-vg.sh \
	incltest.sh \
	incltest_dir.sh \
	incltest_dir_empty_wildcard.sh \
	incltest_dir_wildcard.sh \
	testsuites/es.yml \
	clickhouse-dflt-tpl.sh \
	clickhouse-retry-error.sh \
	clickhouse-start.sh \
	clickhouse-stop.sh \
	clickhouse-basic.sh \
	clickhouse-load.sh \
	clickhouse-bulk.sh \
	clickhouse-bulk-load.sh \
	clickhouse-limited-batch.sh \
	clickhouse-select.sh \
	clickhouse-wrong-quotation-marks.sh \
	clickhouse-wrong-template-option.sh \
	clickhouse-errorfile.sh \
	clickhouse-wrong-insert-syntax.sh \
	clickhouse-basic-vg.sh \
	clickhouse-load-vg.sh \
	clickhouse-bulk-vg.sh \
	clickhouse-bulk-load-vg.sh \
	es_response_get_msgnum.py \
	elasticsearch-error-format-check.py \
	es-duplicated-ruleset.sh \
	es-duplicated-ruleset-vg.sh \
	es-basic-es7.14.sh \
	es-basic.sh \
	es-basic-vgthread.sh \
	es-basic-server.sh \
	es-execOnlyWhenPreviousSuspended.sh \
	es-basic-ha.sh \
	es-basic-bulk.sh \
	es-basic-errfile-empty.sh \
	es-basic-errfile-popul.sh \
	es-bulk-errfile-empty.sh \
	es-bulk-errfile-popul.sh \
	es-bulk-errfile-popul-def-format.sh \
	es-bulk-errfile-popul-erronly.sh \
	es-bulk-errfile-popul-erronly-interleaved.sh \
	es-bulk-errfile-popul-def-interleaved.sh \
	es-searchType-empty.sh \
	diskqueue-multithread-es.sh \
	es-basic-vg.sh \
	es-basic-bulk-vg.sh \
	es-basic-ha-vg.sh \
	es-maxbytes-bulk.sh \
	es-bulk-retry.sh \
	elasticsearch-stop.sh \
	linkedlistqueue.sh \
	da-mainmsg-q.sh \
	diskqueue-fsync.sh \
	msgdup.sh \
	msgdup_props.sh \
	empty-ruleset.sh \
	ruleset-direct-queue.sh \
	imtcp-listen-port-file-2.sh \
	allowed-sender-tcp-ok.sh \
	allowed-sender-tcp-fail.sh \
	allowed-sender-tcp-hostname-ok.sh \
	allowed-sender-tcp-hostname-fail.sh \
	imtcp-octet-framing-too-long-vg.sh \
	imtcp-discard-truncated-msg.sh \
	imtcp-basic.sh \
	imtcp-basic-hup.sh \
	imtcp-impstats.sh \
	imtcp-impstats-single-thread.sh \
	imtcp-starvation-0.sh \
	imtcp-starvation-1.sh \
	imtcp-maxFrameSize.sh \
	imtcp-msg-truncation-on-number.sh \
	imtcp-msg-truncation-on-number2.sh \
	imtcp-NUL.sh \
	imtcp-NUL-rawmsg.sh \
	imtcp-tls-gtls-x509fingerprint-invld.sh \
	imtcp-tls-gtls-x509fingerprint.sh \
	imtcp-tls-gtls-x509name-invld.sh \
	imtcp-tls-gtls-x509name.sh \
	imtcp-tls-gtls-x509name-legacy.sh \
	imtcp-drvr-in-input-basic.sh \
	imtcp-multi-drvr-basic.sh \
	imtcp-multi-drvr-basic-parallel.sh \
	imtcp-multi-drvr-basic-ptcp_gtls_ossl.sh \
	imtcp-tls-basic.sh \
	imtcp-tls-input-basic.sh \
	imtcp-tls-input-2certs.sh \
	imtcp-tls-basic-verifydepth.sh \
	imtcp-tls-basic-vg.sh \
	imtcp-tls-no-lstn-startup.sh \
	imtcp_incomplete_frame_at_end.sh \
	imtcp-multiport.sh \
	imtcp-bigmessage-octetcounting.sh \
	imtcp-bigmessage-octetstuffing.sh \
	udp-msgreduc-orgmsg-vg.sh \
	udp-msgreduc-vg.sh \
	manytcp-too-few-tls-vg.sh \
	imtcp-tls-ossl-basic.sh \
	imtcp-tls-ossl-input-basic.sh \
	imtcp-tls-ossl-input-2certs.sh \
	imtcp-tls-ossl-basic-tlscommands.sh \
	imtcp-tls-ossl-basic-verifydepth.sh \
	imtcp-tls-ossl-invalid-verifydepth.sh \
	imtcp-tls-ossl-basic-stress.sh \
	sndrcv_tls_ossl_anon_ipv4.sh \
	sndrcv_tls_ossl_anon_ipv6.sh \
	sndrcv_tls_ossl_anon_rebind.sh \
	sndrcv_tls_ossl_anon_ciphers.sh \
	sndrcv_tls_ossl_certvalid.sh \
	sndrcv_tls_ossl_certvalid_action_level.sh \
	sndrcv_tls_ossl_certvalid_expired.sh \
	sndrcv_tls_ossl_certvalid_tlscommand.sh \
	sndrcv_tls_ossl_certvalid_ciphers.sh \
	sndrcv_tls_ossl_certvalid_revoked.sh \
	imtcp-tls-ossl-x509valid.sh \
	imtcp-tls-ossl-x509name.sh \
	imtcp-tls-ossl-x509fingerprint.sh \
	imtcp-tls-ossl-basic-vg.sh \
	imtcp-tls-ossl-basic-brokenhandshake-vg.sh \
	imtcp-tls-ossl-error-ca.sh \
	imtcp-tls-ossl-error-cert.sh \
	imtcp-tls-ossl-error-key.sh \
	imtcp-tls-ossl-error-key2.sh \
	manytcp.sh \
	imtcp-tls-gtls-manycon.sh \
	imtcp-tls-ossl-manycon.sh \
	manyptcp.sh \
	imptcp-basic-hup.sh \
	imptcp-NUL.sh \
	imptcp-NUL-rawmsg.sh \
	imptcp_framing_regex.sh \
	testsuites/imptcp_framing_regex.testdata \
	imptcp_framing_regex-oversize.sh \
	testsuites/imptcp_framing_regex-oversize.testdata \
	imptcp_large.sh \
	imptcp-connection-msg-disabled.sh \
	imptcp-connection-msg-received.sh \
	imptcp-discard-truncated-msg.sh \
	imptcp_addtlframedelim.sh \
	imptcp_conndrop-vg.sh \
	imptcp_conndrop.sh \
	imptcp_multi_line.sh \
	testsuites/imptcp_multi_line.testdata \
	imptcp_no_octet_counted.sh \
	imtcp_addtlframedelim_on_input.sh \
	testsuites/no_octet_counted.testdata \
	imtcp_no_octet_counted.sh \
	testsuites/spframingfix.testdata \
	imtcp_spframingfix.sh \
	imtcp-connection-msg-recieved.sh \
	imptcp_spframingfix.sh \
	msg-deadlock-headerless-noappname.sh \
	imtcp_conndrop.sh \
	imtcp_conndrop_tls.sh \
	imtcp_conndrop_tls-vg.sh \
	imtcp_addtlframedelim.sh \
	tcp-msgreduc-vg.sh \
	inputname-imtcp.sh \
	omod-if-array.sh \
	omod-if-array-udp.sh \
	discard.sh \
	failover-no-rptd.sh \
	failover-no-rptd-vg.sh \
	failover-no-basic.sh \
	failover-no-basic-vg.sh \
	failover-rptd.sh \
	failover-rptd-vg.sh \
	failover-basic.sh \
	failover-basic-vg.sh \
	failover-async.sh \
	failover-double.sh \
	suspend-via-file.sh \
	suspend-omfwd-via-file.sh \
	externalstate-failed-rcvr.sh \
	discard-rptdmsg.sh \
	discard-rptdmsg-vg.sh \
	discard-allmark.sh \
	discard-allmark-vg.sh \
	diag.sh \
	rcvr_fail_restore.sh \
	queue-encryption-disk.sh \
	queue-encryption-disk_keyfile.sh \
	queue-encryption-disk_keyfile-vg.sh \
	queue-encryption-disk_keyprog.sh \
	queue-encryption-da.sh \
	da-queue-persist.sh \
	daqueue-dirty-shutdown.sh \
	daqueue-invld-qi.sh \
	daqueue-persist.sh \
	daqueue-persist-drvr.sh \
	queue-persist.sh \
	queue-persist-drvr.sh \
	threadingmq.sh \
	threadingmqaq.sh \
	sndrcv_drvr.sh \
	sndrcv_drvr_noexit.sh \
	sndrcv_failover.sh \
	sndrcv.sh \
	omrelp_errmsg_no_connect.sh \
	imrelp-basic.sh \
	imrelp-basic-hup.sh \
	imrelp-basic-vg.sh \
	imrelp-basic-oldstyle.sh \
	imrelp-manyconn.sh \
	imrelp-manyconn-vg.sh \
	imrelp-maxDataSize-error.sh \
	imrelp-long-msg.sh \
	imrelp-oversizeMode-truncate.sh \
	imrelp-oversizeMode-accept.sh \
	imrelp-invld-tlslib.sh \
	imrelp-bigmessage.sh \
	imrelp-sessionbreak-vg.sh \
	omrelp-invld-tlslib.sh \
	glbl-oversizeMsg-log.sh \
	glbl-oversizeMsg-truncate.sh \
	glbl-oversizeMsg-split.sh \
	sndrcv_relp.sh \
	sndrcv_relp_rebind.sh \
	sndrcv_relp_tls_prio.sh \
	sndrcv_relp_tls_chainedcert.sh \
	sndrcv_relp_tls.sh \
	sndrcv_relp_tls_certvalid.sh \
	sndrcv_relp-vg-rcvr.sh \
	sndrcv_relp-vg-sender.sh \
	relp_tls_certificate_not_found.sh \
	omrelp_wrong_authmode.sh \
	imrelp-tls.sh \
	imrelp-tls-cfgcmd.sh \
	imrelp-tls-chainedcert.sh \
	imrelp-tls-mixed-chainedcert.sh \
	imrelp-tls-mixed-chainedcert2.sh \
	sndrcv_relp_tls-cfgcmd.sh \
	sndrcv_relp_dflt_pt.sh \
	sndrcv_udp.sh \
	imudp_thread_hang.sh \
	sndrcv_udp_nonstdpt.sh \
	sndrcv_udp_nonstdpt_v6.sh \
	omudpspoof_errmsg_no_params.sh \
	sndrcv_omudpspoof.sh \
	sndrcv_omudpspoof-bigmsg.sh \
	sndrcv_omudpspoof_nonstdpt.sh \
	sndrcv_gzip.sh \
	imdtls-basic.sh \
	imdtls-basic-tlscommands.sh \
	imdtls-basic-timeout.sh \
	imdtls-error-cert.sh \
	imdtls-sessionbreak.sh \
	imdtls-basic-vg.sh \
	imdtls-sessionbreak-vg.sh \
	sndrcv_dtls_certvalid.sh \
	sndrcv_dtls_anon_ciphers.sh \
	sndrcv_dtls_certvalid_ciphers.sh \
	sndrcv_dtls_certvalid_permitted.sh \
	sndrcv_dtls_certvalid_missing.sh \
	sndrcv_dtls_anon_ciphers.sh \
	sndrcv_dtls_certvalid-vg.sh \
	action-tx-single-processing.sh \
	omfwd-errfile-maxsize.sh \
	omfwd-errfile-maxsize-filled.sh \
	action-tx-errfile-maxsize.sh \
	action-tx-errfile.sh \
	testsuites/action-tx-errfile.result \
	pipeaction.sh \
	improg-simul.sh \
	improg-multiline-test.py \
	improg_errmsg_no_params.sh \
	improg_errmsg_no_params-vg.sh \
	improg_prog_simple.sh \
	improg_prog_confirm.sh \
	improg_prog_confirm_killonclose.sh \
	improg_prog_killonclose.sh \
	improg_prog_simple-vg.sh \
	improg_simple_multi.sh \
	imhttp-post-payload.sh \
	imhttp-post-payload-vg.sh \
	imhttp-post-payload-basic-auth.sh \
	imhttp-post-payload-basic-auth-vg.sh \
	imhttp-post-payload-query-params.sh \
	imhttp-post-payload-query-params-vg.sh \
	imhttp-post-payload-large.sh \
	imhttp-post-payload-large-vg.sh \
	testsuites/imhttp-large-data.txt \
	imhttp-post-payload-multi.sh \
	imhttp-post-payload-multi-vg.sh \
	imhttp-getrequest-file.sh \
	imhttp-getrequest-file-vg.sh \
	imhttp-post-payload-multi-lf.sh \
	imhttp-post-payload-multi-lf-vg.sh \
	imhttp-post-payload-compress.sh \
	imhttp-post-payload-compress-vg.sh \
	testsuites/docroot/file.txt \
	testsuites/htpasswd \
	omhttp-auth.sh \
	omhttp-basic.sh \
	omhttp-basic-ignorecodes.sh \
	omhttp-batch-fail-with-400.sh \
	omhttp-batch-jsonarray-compress.sh \
	omhttp-batch-jsonarray-retry.sh \
	omhttp-batch-jsonarray.sh \
	omhttp-batch-kafkarest-retry.sh \
	omhttp-batch-kafkarest.sh \
	omhttp-batch-lokirest-retry.sh \
	omhttp-batch-lokirest.sh \
	omhttp-batch-lokirest-vg.sh \
	omhttp-batch-newline.sh \
	omhttp-batch-retry-metadata.sh \
	omhttp-retry-timeout.sh \
	omhttp-basic-ignorecodes-vg.sh \
	omhttp-batch-retry-metadata-vg.sh \
	omhttp-retry-timeout-vg.sh \
	omhttp-retry.sh \
	omhttp-httpheaderkey.sh \
	omhttp-multiplehttpheaders.sh \
	omhttp-dynrestpath.sh \
	omhttp-batch-dynrestpath.sh \
	omhttp-auth-vg.sh \
	omhttp-basic-vg.sh \
	omhttp-batch-jsonarray-compress-vg.sh \
	omhttp-batch-jsonarray-retry-vg.sh \
	omhttp-batch-jsonarray-vg.sh \
	omhttp-batch-kafkarest-retry-vg.sh \
	omhttp-batch-lokirest-retry-vg.sh \
	omhttp-retry-vg.sh \
	omhttp_server.py \
	omhttp-validate-response.py \
	omprog-defaults.sh \
	omprog-defaults-vg.sh \
	omprog-output-capture.sh \
	omprog-output-capture-mt.sh \
	omprog-output-capture-vg.sh \
	omprog-feedback.sh \
	omprog-feedback-mt.sh \
	omprog-feedback-vg.sh \
	omprog-feedback-timeout.sh \
	omprog-close-unresponsive.sh \
	omprog-close-unresponsive-vg.sh \
	omprog-close-unresponsive-noterm.sh \
	omprog-restart-terminated.sh \
	omprog-restart-terminated-vg.sh \
	omprog-restart-terminated-outfile.sh \
	omprog-single-instance.sh \
	omprog-single-instance-vg.sh \
	omprog-single-instance-outfile.sh \
	omprog-if-error.sh \
	omprog-transactions.sh \
	omprog-transactions-vg.sh \
	omprog-transactions-failed-messages.sh \
	omprog-transactions-failed-commits.sh \
	testsuites/omprog-defaults-bin.sh \
	testsuites/omprog-output-capture-bin.sh \
	testsuites/omprog-output-capture-mt-bin.py \
	testsuites/omprog-feedback-bin.sh \
	testsuites/omprog-feedback-mt-bin.sh \
	testsuites/omprog-feedback-timeout-bin.sh \
	testsuites/omprog-close-unresponsive-bin.sh \
	testsuites/omprog-restart-terminated-bin.sh \
	testsuites/omprog-single-instance-bin.sh \
	testsuites/omprog-transactions-bin.sh \
	pipe_noreader.sh \
	uxsock_simple.sh \
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
	asynwr_dynfile_flushtxend-off.sh \
	abort-uncleancfg-goodcfg.sh \
	abort-uncleancfg-goodcfg-check.sh \
	abort-uncleancfg-badcfg-check.sh \
	abort-uncleancfg-badcfg-check_1.sh \
	variable_leading_underscore.sh \
	gzipwr_hup_multi_file.sh \
	gzipwr_hup_single_file.sh \
	gzipwr_rscript.sh \
	gzipwr_flushInterval.sh \
	gzipwr_flushOnTXEnd.sh \
	gzipwr_large.sh \
	gzipwr_large_dynfile.sh \
	gzipwr_hup.sh \
	complex1.sh \
	random.sh \
	testsuites/imfile-old-state-file_imfile-state_.-rsyslog.input \
	imfile-readmode0-vg.sh \
	imfile-readmode2.sh \
	imfile-readmode2-polling.sh \
	imfile-readmode2-vg.sh \
	imfile-readmode2-with-persists-data-during-stop.sh \
	imfile-readmode2-with-persists.sh \
	imfile-endregex-save-lf.sh \
	imfile-endregex-save-lf-persist.sh \
	imfile-endregex.sh \
	imfile-endregex-vg.sh \
	imfile-basic.sh \
	imfile-basic-legacy.sh \
	imfile-basic-2GB-file.sh \
	imfile-truncate-2GB-file.sh \
	imfile-discard-truncated-line.sh \
	imfile-truncate-line.sh \
	imfile-file-not-found-error.sh \
	imfile-fileNotFoundError-parameter.sh \
	imfile-error-not-repeated.sh \
	imfile-basic-vg.sh \
	imfile-basic-vgthread.sh \
	imfile-endregex-timeout-none-polling.sh \
	imfile-endregex-timeout-polling.sh \
	imfile-endregex-timeout.sh \
	imfile-endregex-timeout-none.sh \
	imfile-endregex-timeout-with-shutdown.sh \
	imfile-endregex-timeout-with-shutdown-polling.sh \
	imfile-escapelf.replacement.sh \
	imfile-escapelf.replacement-empty.sh \
	imfile-endmsg.regex.sh \
	imfile-endmsg.regex-vg.sh \
	imfile-endmsg.regex-with-example.sh \
	imfile-endmsg.regex-with-example-vg.sh \
	imfile-endmsg.regex.crio.rulebase \
	imfile-endmsg.regex.json.rulebase \
	imfile-statefile-no-file_id.sh \
	imfile-statefile-no-file_id-TO-file_id.sh \
	imfile-statefile-directory.sh \
	imfile-statefile-delete.sh \
	imfile-statefile-no-delete.sh \
	imfile-persist-state-1.sh \
	imfile-freshStartTail1.sh \
	imfile-freshStartTail2.sh \
	imfile-freshStartTail3.sh \
	imfile-truncate.sh \
	imfile-truncate-multiple.sh \
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
	imfile-symlink.sh \
	imfile-symlink-multi.sh \
	imfile-symlink-ext-tmp-dir-tree.sh \
	imfile-logrotate.sh \
	imfile-logrotate-async.sh \
	imfile-logrotate-copytruncate.sh \
	imfile-logrotate-nocopytruncate.sh \
	imfile-logrotate-multiple.sh \
	imfile-growing-file-id.sh \
	imfile-ignore-old-file-1.sh \
	imfile-ignore-old-file-2.sh \
	imfile-ignore-old-file-3.sh \
	imfile-ignore-old-file-4.sh \
	imfile-ignore-old-file-5.sh \
	imfile-ignore-old-file-6.sh \
	imfile-ignore-old-file-7.sh \
	glbl-oversizeMsg-truncate-imfile.sh \
	dynfile_invld_async.sh \
	dynfile_invld_sync.sh \
	dynfile_invalid2.sh \
	rulesetmultiqueue.sh \
	rulesetmultiqueue-v6.sh \
	omruleset.sh \
	omruleset-queue.sh \
	badqi.sh \
	bad_qi/dbq.qi \
	execonlyonce.sh \
	execonlywhenprevsuspended.sh \
	execonlywhenprevsuspended2.sh \
	execonlywhenprevsuspended3.sh \
	execonlywhenprevsuspended4.sh \
	execonlywhenprevsuspended_multiwrkr.sh \
	execonlywhenprevsuspended-queue.sh \
	execonlywhenprevsuspended-nonsusp.sh \
	execonlywhenprevsuspended-nonsusp-queue.sh \
	tabescape_dflt.sh \
	tabescape_dflt-udp.sh \
	tabescape_off.sh \
	tabescape_off-udp.sh \
	tabescape_on.sh \
	dircreate_dflt.sh \
	dircreate_off.sh \
	imuxsock_legacy.sh \
	imuxsock_logger_parserchain.sh \
	imuxsock_logger.sh \
	imuxsock_logger_ratelimit.sh \
	imuxsock_logger_ruleset.sh \
	imuxsock_logger_ruleset_ratelimit.sh \
	imuxsock_logger_err.sh \
	imuxsock_logger_root.sh \
	imuxsock_logger_syssock.sh \
	imuxsock_traillf.sh \
	imuxsock_traillf_root.sh \
	imuxsock_traillf_syssock.sh \
	imuxsock_ccmiddle.sh \
	imuxsock_ccmiddle_root.sh \
	imklog_permitnonkernelfacility_root.sh \
	imuxsock_ccmiddle_syssock.sh \
	imuxsock_hostname.sh \
	testsuites/mysql-truncate.sql \
	testsuites/mysql-select-msg.sql \
	libdbi-basic.sh \
	libdbi-asyn.sh \
	mysqld-start.sh \
	mysqld-stop.sh \
	mysql-basic.sh \
	mysql-basic-cnf6.sh \
	mysql-basic-vg.sh \
	mysql-asyn.sh \
	mysql-asyn-vg.sh \
	mysql-actq-mt.sh \
	mysql-actq-mt-withpause.sh \
	mysql-actq-mt-withpause-vg.sh \
	kafka-selftest.sh \
	omkafka.sh \
	omkafkadynakey.sh \
	omkafka-vg.sh \
	imkafka-hang-on-no-kafka.sh \
	imkafka-hang-other-action-on-no-kafka.sh \
	imkafka-backgrounded.sh \
	imkafka-config-err-ruleset.sh \
	imkafka-config-err-param.sh \
	imkafka.sh \
	imkafka-vg.sh \
	imkafka_multi_single.sh \
	imkafka_multi_group.sh \
	sndrcv_kafka.sh \
	sndrcv_kafka_multi_topics.sh \
	testsuites/kafka-server.properties \
	testsuites/kafka-server.dep_wrk1.properties \
	testsuites/kafka-server.dep_wrk2.properties \
	testsuites/kafka-server.dep_wrk3.properties \
	testsuites/zoo.cfg \
	testsuites/zoo.dep_wrk1.cfg \
	testsuites/zoo.dep_wrk2.cfg \
	testsuites/zoo.dep_wrk3.cfg \
	omazureeventhubs-basic.sh \
	omazureeventhubs-basic-url.sh \
	omazureeventhubs-list.sh \
	omazureeventhubs-stress.sh \
	omazureeventhubs-interrupt.sh \
	omazureeventhubs-basic-vg.sh \
	omazureeventhubs-interrupt-vg.sh \
	mmpstrucdata.sh \
	mmpstrucdata-escaping.sh \
	mmpstrucdata-case.sh \
	mmpstrucdata-vg.sh \
	mmpstrucdata-invalid-vg.sh \
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
	testsuites/dynstats_empty_input \
	testsuites/dynstats_input \
	testsuites/dynstats_input_1 \
	testsuites/dynstats_input_2 \
	testsuites/dynstats_input_3 \
	testsuites/dynstats_input_more_0 \
	testsuites/dynstats_input_more_1 \
	testsuites/dynstats_input_more_2 \
	no-dynstats-json.sh \
	no-dynstats.sh \
	omfwd_impstats-udp.sh \
	omfwd_impstats-tcp.sh \
	perctile-simple.sh \
	perctile-simple-vg.sh \
	stats-json.sh \
	stats-json-vg.sh \
	stats-cee.sh \
	stats-cee-vg.sh \
	stats-json-es.sh \
	dynstats-json.sh \
	dynstats-json-vg.sh \
	mmnormalize_variable.sh \
	mmnormalize_tokenized.sh \
	testsuites/mmnormalize_variable.rulebase \
	testsuites/date_time_msg \
	testsuites/mmnormalize_tokenized.rulebase \
	testsuites/tokenized_input \
	rscript_random.sh \
	rscript_hash32.sh \
	rscript_hash32-vg.sh \
	rscript_hash64.sh \
	rscript_hash64-vg.sh \
	rscript_replace.sh \
	rscript_replace_complex.sh \
	testsuites/complex_replace_input \
	rscript_unaffected_reset.sh \
	rscript_wrap2.sh \
	rscript_wrap3.sh \
	testsuites/wrap3_input\
	json_array_subscripting.sh \
	testsuites/json_array_input \
	testsuites/json_object_input \
	testsuites/json_nonarray_input \
	json_array_looping.sh \
	json_object_looping.sh \
	json_object_looping-vg.sh \
	json_array_looping-vg.sh \
	json_object_suicide_in_loop-vg.sh \
	json_nonarray_looping.sh \
	json_null.sh \
	json_null-vg.sh \
	json_null_array.sh \
	json_null_array-vg.sh \
	mmjsonparse_extra_data-vg.sh \
	mmnormalize_regex.sh \
	testsuites/mmnormalize_regex.rulebase \
	testsuites/regex_input \
	mmnormalize_regex_disabled.sh \
	mmnormalize_regex_defaulted.sh \
	stop_when_array_has_element.sh \
	testsuites/stop_when_array_has_elem_input \
	key_dereference_on_uninitialized_variable_space.sh \
	rscript_re_extract_i.sh \
	rscript_re_extract.sh \
	rscript_re_match_i.sh \
	rscript_re_match.sh \
	rscript_re_match-dbl_quotes.sh \
	lookup_table.sh \
	lookup_table-hup-backgrounded.sh \
	lookup_table_no_hup_reload.sh \
	lookup_table_no_hup_reload-vg.sh \
	lookup_table_rscript_reload.sh \
	lookup_table_rscript_reload_without_stub.sh \
	lookup_table_rscript_reload-vg.sh \
	lookup_table_rscript_reload_without_stub-vg.sh \
	rscript_trim-vg.sh \
	testsuites/xlate.lkp_tbl \
	testsuites/xlate_more.lkp_tbl \
	unused_lookup_table-vg.sh \
	lookup_table-vg.sh \
	array_lookup_table.sh \
	array_lookup_table-vg.sh \
	array_lookup_table_misuse-vg.sh \
	multiple_lookup_tables.sh \
	multiple_lookup_tables-vg.sh \
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
	json_var_cmpr.sh \
	imptcp_maxsessions.sh \
	imptcp_nonProcessingPoller.sh \
	imptcp_veryLargeOctateCountedMessages.sh \
	known_issues.supp \
	libmaxmindb.supp \
	travis/trusty.supp \
	linux_localtime_r.supp \
	CI/centos6-9.supp \
	CI/centos7.supp \
	CI/gcov.supp \
	CI/ubuntu20.04.supp \
	json_var_case.sh \
	cfg.sh \
	empty-prop-comparison.sh \
	sndrcv_tls_anon_rebind.sh \
	sndrcv_tls_anon_hostname.sh \
	sndrcv_tls_anon_ipv4.sh \
	sndrcv_tls_anon_ipv6.sh \
	sndrcv_tls_priorityString.sh \
	sndrcv_tls_certvalid.sh \
	sndrcv_tls_certvalid_action_level.sh \
	sndrcv_tls_certvalid_expired.sh \
	sndrcv_tls_certvalid_expired_defaultmode.sh \
	sndrcv_tls_certvalid_revoked.sh \
	sndrcv_tls_certless_clientonly.sh \
	sndrcv_tls_gtls_servercert_gtls_clientanon.sh \
	sndrcv_tls_gtls_servercert_gtls_clientanon_legacy.sh \
	sndrcv_tls_gtls_serveranon_gtls_clientanon.sh \
	sndrcv_tls_ossl_serveranon_ossl_clientanon.sh \
	sndrcv_tls_ossl_servercert_ossl_clientanon.sh \
	sndrcv_tls_ossl_servercert_gtls_clientanon.sh \
	sndrcv_tls_ossl_serveranon_gtls_clientanon.sh \
	sndrcv_tls_gtls_servercert_ossl_clientanon.sh \
	sndrcv_tls_gtls_serveranon_ossl_clientanon.sh \
	sndrcv_tls_client_missing_cert.sh \
	sndrcv_ossl_cert_chain.sh \
	omtcl.sh \
	omtcl.tcl \
	pmsnare-default.sh \
	pmsnare-default-udp.sh \
	pmsnare-ccoff.sh \
	pmsnare-ccoff-udp.sh \
	pmsnare-ccdefault.sh \
	pmsnare-ccdefault-udp.sh \
	pmsnare-cccstyle.sh \
	pmsnare-cccstyle-udp.sh \
	pmsnare-ccbackslash.sh \
	pmsnare-ccbackslash-udp.sh \
	pmsnare-modoverride.sh \
	pmsnare-modoverride-udp.sh \
	pmlastmsg.sh \
	pmlastmsg-udp.sh \
	pgsql-basic.sh \
	testsuites/pgsql-basic.sql \
	testsuites/pgsql-select-msg.sql \
	testsuites/pgsql-select-syslogtag.sql \
	pgsql-basic-cnf6.sh \
	pgsql-basic-threads-cnf6.sh \
	pgsql-template.sh \
	pgsql-template-cnf6.sh \
	pgsql-actq-mt-withpause.sh \
	pgsql-template-threads-cnf6.sh \
	pgsql-basic-vg.sh \
	pgsql-template-vg.sh \
	pgsql-basic-cnf6-vg.sh \
	pgsql-template-cnf6-vg.sh \
	pgsql-actq-mt-withpause-vg.sh \
	../devtools/prep-mysql-db.sh \
	../devtools/prepare_clickhouse.sh \
	mmkubernetes-basic.sh \
	mmkubernetes-basic-vg.sh \
	mmkubernetes_test_server.py \
	mmkubernetes-basic.out.json \
	mmkubernetes-cache-expire.sh \
	mmkubernetes-cache-expire-vg.sh \
	mmkubernetes-cache-expire.out.expected \
	mmkubernetes.supp \
	es-writeoperation.sh \
	imdocker-basic.sh \
	imdocker-basic-vg.sh \
	imdocker-long-logline.sh \
	imdocker-long-logline-vg.sh \
	imdocker-new-logs-from-start.sh \
	imdocker-new-logs-from-start-vg.sh \
	imdocker-multi-line.sh \
	imdocker-multi-line-vg.sh \
	testsuites/incltest.d/include.conf \
	testsuites/abort-uncleancfg-goodcfg.conf \
	testsuites/include-std-omfile-action.conf \
	testsuites/invalid.conf \
	testsuites/valid.conf \
	testsuites/variable_leading_underscore.conf \
	omamqp1-common.sh \
	omamqp1-basic.sh \
	omamqp1-basic-vg.sh

ourtail_SOURCES = ourtail.c
msleep_SOURCES = msleep.c
omrelp_dflt_port_SOURCES = omrelp_dflt_port.c
mangle_qi_SOURCES = mangle_qi.c
chkseq_SOURCES = chkseq.c
check_relpEngineVersion = check_relpEngineVersion.c
have_relpSrvSetOversizeMode = have_relpSrvSetOversizeMode.c
have_relpEngineSetTLSLibByName = have_relpEngineSetTLSLibByName.c
have_relpSrvSetTlsConfigCmd = have_relpSrvSetTlsConfigCmd.c
test_id_SOURCES = test_id.c

uxsockrcvr_SOURCES = uxsockrcvr.c
uxsockrcvr_LDADD = $(SOL_LIBS)

tcpflood_SOURCES = tcpflood.c
tcpflood_CFLAGS = $(PTHREADS_CFLAGS) $(RELP_CFLAGS)
tcpflood_CPPFLAGS = $(PTHREADS_CFLAGS) $(RELP_CFLAGS)
tcpflood_LDADD = $(SOL_LIBS) $(PTHREADS_LIBS) $(RELP_LIBS)
if ENABLE_GNUTLS
tcpflood_CFLAGS += $(GNUTLS_CFLAGS)
tcpflood_CPPFLAGS += $(GNUTLS_CFLAGS)
tcpflood_LDADD += $(GNUTLS_LIBS)
endif
if ENABLE_OPENSSL
tcpflood_CFLAGS += $(OPENSSL_CFLAGS)
tcpflood_CPPFLAGS += $(OPENSSL_CFLAGS)
tcpflood_LDADD += $(OPENSSL_LIBS)
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
