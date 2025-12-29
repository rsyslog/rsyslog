TEST_EXTENSIONS=.sh
EXTRA_DIST = \
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

TESTS_JSON_OMITIFZERO = json-omitifzero.sh json-omitifzero-subtree.sh json-whitespace.sh
EXTRA_DIST += $(TESTS_JSON_OMITIFZERO)

TESTS_IMPTCP_TABESCAPE = \
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

TESTS_ELASTICSEARCH_MINIMAL = \
	es-duplicated-ruleset.sh \
	es-basic-es7.14.sh \
	es-basic.sh \
	es-basic-bulk.sh \
	elasticsearch-stop.sh

TESTS_ELASTICSEARCH_MINIMAL_VALGRIND = \
	es-duplicated-ruleset-vg.sh \
	es-basic-vg.sh

TESTS_ELASTICSEARCH_MINIMAL_HELGRIND = \
	es-basic-vgthread.sh

TESTS_ELASTICSEARCH_SERVER = \
	es-basic-server.sh \
	es-execOnlyWhenPreviousSuspended.sh \
	es-maxbytes-bulk.sh \
	es-basic-errfile-empty.sh \
	es-basic-errfile-popul.sh \
	es-bulk-errfile-empty.sh \
	es-bulk-errfile-popul.sh \
	diskqueue-multithread-es.sh \
	es-writeoperation.sh

TESTS_ELASTICSEARCH_IMPSTATS = \
	es-basic-ha.sh \
	es-bulk-retry.sh

TESTS_ELASTICSEARCH_IMFILE = \
	es-bulk-errfile-popul-def-format.sh \
	es-bulk-errfile-popul-erronly.sh \
	es-bulk-errfile-popul-erronly-interleaved.sh \
	es-bulk-errfile-popul-def-interleaved.sh

TESTS_ELASTICSEARCH_VALGRIND = \
	es-basic-bulk-vg.sh \
	es-basic-ha-vg.sh

TESTS_DEFAULT = \
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
	endswith-basic.sh \
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
	omfwd-subtree-tpl.sh \
	omfile-subtree-jsonf.sh \
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
        template-jsonf-nested.sh \
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
	pmrfc3164-headerless.sh \
	pmrfc3164-drop.sh \
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
	imudp_ratelimit_name.sh \
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
	rscript_b64_decode.sh \
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
	rscript_toupper.sh \
	rscript_format_time.sh \
	rscript_parse_time.sh \
	rscript_is_time.sh \
	rscript_is_in_subnet.sh \
	rscript_script_error.sh \
	rscript_parse_json.sh \
	rscript_parse_json_issue.sh \
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
	rscript_split.sh \
	rscript_append_json.sh \
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
	lookup_sparse_array_ipv4.sh \
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

TESTS_SKELETONS = \
	omfwd-lb-1target-retry-test_skeleton.sh \
	omfwd-lb-1target-retry-test_skeleton-TargetFail.sh

TESTS_INACTIVE = \
	omfwd-errfile-maxsize.sh \
	omfwd-errfile-maxsize-filled.sh \
	prop-all-json-concurrency.sh \
	imuxsock_hostname.sh \
	sndrcv_ossl_cert_chain.sh \
	imtcp-tls-ossl-basic-stress.sh \
	imtcp-tls-ossl-manycon.sh

TESTS_MMSNAREPARSE_MINIMAL = \
	mmsnareparse-basic.sh \
	mmsnareparse-json.sh \
	mmsnareparse-syslog.sh \
	mmsnareparse-comprehensive.sh \
	mmsnareparse-value-types.sh \
	mmsnareparse-enhanced-validation.sh \
	mmsnareparse-sysmon.sh \
	mmsnareparse-kerberos.sh \
	mmsnareparse-custom.sh \
	mmsnareparse-realworld-4624-4634-5140.sh \
	mmsnareparse-trailing-extradata.sh \
	mmsnareparse-trailing-extradata-regex.sh

TESTS_MMSNAREPARSE_VALGRIND = \
	mmsnareparse-comprehensive-vg.sh

TESTS_OMSENDERTRACK = \
        omsendertrack-basic.sh \
	omsendertrack-statefile.sh

TESTS_OMSENDERTRACK_VALGRIND = \
	omsendertrack-basic-vg.sh \
	omsendertrack-statefile-vg.sh

TESTS_LIBZSTD = \
        zstd.sh

TESTS_LIBZSTD_VALGRIND = \
        zstd-vg.sh

TESTS_LIBGCRYPT = \
	queue-encryption-disk.sh \
	queue-encryption-disk_keyfile.sh \
	queue-encryption-disk_keyprog.sh \
	queue-encryption-da.sh

TESTS_GNUTLS_WRONG_OPT = \
	tcpflood_wrong_option_output.sh

TESTS_OSSL_WRONG_OPT = \
	tcpflood_wrong_option_output.sh

TESTS_DEFAULT_VALGRIND = \
	omusrmsg-noabort-vg.sh \
	omfile_hup-vg.sh \
	gzipwr_hup-vg.sh \
	imtcp-octet-framing-too-long-vg.sh \
	smtradfile-vg.sh \
	dnscache-TTL-0-vg.sh \
	include-obj-outside-control-flow-vg.sh \
	include-obj-in-if-vg.sh \
	include-obj-text-vg.sh \
	rscript_b64_decode-vg.sh \
	rscript_parse_json-vg.sh \
	rscript_backticks-vg.sh \
	rscript_backticks_empty_envvar.sh \
	rscript_backticks_static_text.sh \
	rscript_backticks_braces_envvar.sh \
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
	rscript_trim-vg.sh \
	rscript_split-vg.sh \
	rscript_append_json-vg.sh

TESTS_LIBGCRYPT_VALGRIND = \
	queue-encryption-disk_keyfile-vg.sh
TESTS_LIBYAML = \
	ratelimit_hup.sh

TESTS_IMTCP = \
	fromhost-port.sh \
	fromhost-port-tuple.sh \
	fromhost-port-async-ruleset.sh \
	allowed-sender-tcp-ok.sh \
	allowed-sender-tcp-fail.sh \
	allowed-sender-tcp-hostname-ok.sh \
	allowed-sender-tcp-hostname-fail.sh \
	imtcp-discard-truncated-msg.sh \
	imtcp-basic.sh \
	imtcp_framing_regex.sh \
	imtcp-multiline.sh \
	imtcp-basic-hup.sh \
	imtcp-impstats-single-thread.sh \
	imtcp-starvation-0.sh \
	imtcp-starvation-1.sh \
	imtcp-maxFrameSize.sh \
	imtcp-msg-truncation-on-number.sh \
	imtcp-msg-truncation-on-number2.sh \
	imtcp-netns.sh \
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
	sndrcv_udp_nonstdpt_v6.sh
TESTS_IMTCP_YAML = \
	imtcp-persource-ratelimit.sh

TESTS_IMTCP_VALGRIND = \
	imtcp-octet-framing-too-long-vg.sh

TESTS_RATELIMIT = \
	ratelimit_exclusivity.sh \
	ratelimit_name.sh \
	ratelimit_duplicate.sh

TESTS_IMTCP_IMPSTATS = \
	imtcp-impstats.sh

TESTS_SNMP_MINIMAL = \
	omsnmp_errmsg_no_params.sh

TESTS_SNMP_FULL = \
	sndrcv_omsnmpv1_udp.sh \
	sndrcv_omsnmpv1_udp_dynsource.sh \
	sndrcv_omsnmpv1_udp_invalidoid.sh

TESTS_MMUTF8FIX = \
        mmutf8fix_no_error.sh \
        mmutf8fix_tag.sh

TESTS_MMAITAG = \
        mmaitag-basic.sh \
        mmaitag-invalid-key.sh

TESTS_MAIL = \
	ommail_errmsg_no_params.sh

TESTS_MMANON = \
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

TESTS_CLICKHOUSE = \
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
	clickhouse-wrong-insert-syntax.sh \
	clickhouse-stop.sh

TESTS_CLICKHOUSE_VALGRIND = \
	clickhouse-basic-vg.sh \
	clickhouse-load-vg.sh \
	clickhouse-bulk-vg.sh \
	clickhouse-bulk-load-vg.sh

TESTS_LIBFAKETIME = \
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

TESTS_MMNORMALIZE_FAKETIME = \
	mmnormalize_processing_test1.sh \
	mmnormalize_processing_test2.sh \
	mmnormalize_processing_test3.sh \
	mmnormalize_processing_test4.sh

TESTS_PGSQL = \
	pgsql-basic.sh \
	pgsql-basic-cnf6.sh \
	pgsql-basic-threads-cnf6.sh \
	pgsql-template.sh \
	pgsql-template-cnf6.sh \
	pgsql-actq-mt-withpause.sh \
	pgsql-template-threads-cnf6.sh

TESTS_PGSQL_VALGRIND = \
	pgsql-basic-vg.sh \
	pgsql-template-vg.sh \
	pgsql-basic-cnf6-vg.sh \
	pgsql-template-cnf6-vg.sh \
	pgsql-actq-mt-withpause-vg.sh

TESTS_MYSQL = \
	mysqld-start.sh \
	mysql-basic.sh \
	mysql-basic-cnf6.sh \
	mysql-asyn.sh \
	mysql-actq-mt.sh \
	mysql-actq-mt-withpause.sh \
	action-tx-single-processing.sh \
	action-tx-errfile-maxsize.sh \
	action-tx-errfile.sh \
	mysqld-stop.sh

TESTS_MYSQL_VALGRIND = \
	mysql-basic-vg.sh \
	mysql-asyn-vg.sh \
	mysql-actq-mt-withpause-vg.sh

TESTS_LIBDBI = \
	libdbi-basic.sh \
	libdbi-asyn.sh

TESTS_LIBDBI_VALGRIND = \
	libdbi-basic-vg.sh

TESTS_FMHTTP = \
	rscript_http_request.sh

TESTS_FMHTTP_VALGRIND = \
	rscript_http_request-vg.sh

TESTS_ROOT = \
	sndrcv_udp.sh \
	imuxsock_logger_root.sh \
	imuxsock_traillf_root.sh \
	imuxsock_ccmiddle_root.sh \
	imklog_permitnonkernelfacility_root.sh

TESTS_ROOT_IP = \
	tcp_forwarding_ns_tpl.sh

TESTS_ROOT_VALGRIND = \
	mmexternal-SegFault-empty-jroot-vg.sh

TESTS_OMHTTP_FLAKY = \
	omhttp-batch-retry-metadata.sh

TESTS_OMHTTP_FLAKY_VALGRIND = \
	omhttp-batch-retry-metadata-vg.sh

TESTS_IMJOURNAL = \
	imjournal-basic.sh \
	imjournal-statefile.sh

TESTS_IMJOURNAL_VALGRIND = \
	imjournal-basic-vg.sh \
	imjournal-statefile-vg.sh

TESTS_OMJOURNAL = \
	omjournal-abort-template.sh \
	omjournal-abort-no-template.sh \
	omjournal-basic-template.sh \
	omjournal-basic-no-template.sh

TESTS_IMPROG = \
	improg_errmsg_no_params.sh \
	improg_prog_simple.sh \
	improg_prog_confirm.sh \
	improg_prog_confirm_killonclose.sh \
	improg_prog_killonclose.sh \
	improg_simple_multi.sh

TESTS_IMPROG_VALGRIND = \
	improg_errmsg_no_params-vg.sh \
	improg_prog_simple-vg.sh

TESTS_MMDARWIN = \
	mmdarwin_errmsg_no_params.sh \
	mmdarwin_errmsg_no_sock.sh

TESTS_MMDARWIN_VALGRIND = \
	mmdarwin_errmsg_no_sock-vg.sh

TESTS_OMPROG = \
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

TESTS_OMPROG_VALGRIND = \
	omprog-defaults-vg.sh \
	omprog-output-capture-vg.sh \
	omprog-feedback-vg.sh \
	omprog-close-unresponsive-vg.sh \
	omprog-restart-terminated-vg.sh \
	omprog-single-instance-vg.sh \
	omprog-transactions-vg.sh

TESTS_OMHTTP = \
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
	omhttp-profile-loki.sh \
	omhttp-batch-newline.sh \
	omhttp-retry.sh \
	omhttp-retry-timeout.sh \
	omhttp-httpheaderkey.sh \
	omhttp-multiplehttpheaders.sh \
	omhttp-dynrestpath.sh \
	omhttp-batch-dynrestpath.sh

TESTS_OMHTTP_VALGRIND = \
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

TESTS_OMOTEL = \
        omotel-http-batch.sh \
        omotel-basic.sh \
        omotel-batch.sh \
        omotel-compression.sh \
        omotel-proxy.sh \
        omotel-tls.sh \
        omotel-trace-correlation.sh

TESTS_KAFKA = \
	kafka-selftest.sh \
	omkafka.sh \
	omkafkadynakey.sh \
	omkafka-headers.sh \
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

TESTS_KAFKA_VALGRIND = \
	omkafka-vg.sh \
	imkafka-vg.sh

TESTS_OMAZUREEVENTHUBS = \
	omazureeventhubs-basic.sh \
	omazureeventhubs-basic-url.sh \
	omazureeventhubs-list.sh \
	omazureeventhubs-stress.sh \
	omazureeventhubs-interrupt.sh

TESTS_OMAZUREEVENTHUBS_VALGRIND = \
	omazureeventhubs-basic-vg.sh \
	omazureeventhubs-interrupt-vg.sh

TESTS_IMDOCKER = \
	imdocker-basic.sh \
	imdocker-image-name.sh \
	imdocker-long-logline.sh \
	imdocker-new-logs-from-start.sh \
	imdocker-multi-line.sh

TESTS_IMDOCKER_VALGRIND = \
	imdocker-basic-vg.sh \
	imdocker-image-name-vg.sh \
	imdocker-long-logline-vg.sh \
	imdocker-new-logs-from-start-vg.sh \
	imdocker-multi-line-vg.sh

TESTS_IMHTTP = \
	imhttp-post-payload.sh \
	imhttp-post-payload-basic-auth.sh \
	imhttp-post-payload-query-params.sh \
	imhttp-post-payload-large.sh \
	imhttp-post-payload-multi.sh \
	imhttp-post-payload-multi-lf.sh \
	imhttp-post-payload-compress.sh \
	imhttp-getrequest-file.sh \
	imhttp-healthcheck.sh \
	imhttp-metrics.sh

TESTS_IMHTTP_VALGRIND = \
	imhttp-post-payload-vg.sh \
	imhttp-post-payload-basic-auth-vg.sh \
	imhttp-post-payload-query-params-vg.sh \
	imhttp-post-payload-large-vg.sh \
	imhttp-post-payload-multi-vg.sh \
	imhttp-post-payload-multi-lf-vg.sh \
	imhttp-post-payload-compress-vg.sh \
	imhttp-getrequest-file-vg.sh

TESTS_OMRABBITMQ = \
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

TESTS_OMRABBITMQ_VALGRIND = \
	omrabbitmq_data_1server-vg.sh

TESTS_IMHIREDIS = \
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

TESTS_IMHIREDIS_VALGRIND = \
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

TESTS_OMHIREDIS = \
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

TESTS_OMHIREDIS_VALGRIND = \
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

TESTS_IMPSTATS = \
	impstats-hup.sh \
	impstats-overwrite.sh \
	impstats-no-overwrite.sh \
	perctile-simple.sh \
	dynstats.sh \
	dynstats_overflow.sh \
	dynstats_reset.sh \
	dynstats_ctr_reset.sh \
	dynstats_nometric.sh \
	no-dynstats-json.sh \
	no-dynstats.sh \
	stats-json.sh \
	stats-prometheus.sh \
	dynstats-json.sh \
	stats-cee.sh \
	stats-json-es.sh \
	dynstats_reset_without_pstats_reset.sh \
	dynstats_prevent_premature_eviction.sh \
	dynstats-persist.sh \
	omfwd-lb-2target-impstats.sh \
	omfwd_fast_imuxsock.sh \
	omfwd_impstats-udp.sh \
	omfwd_impstats-tcp.sh

TESTS_IMPSTATS_VALGRIND = \
	perctile-simple-vg.sh \
	dynstats-vg.sh \
	dynstats_reset-vg.sh \
	dynstats_overflow-vg.sh \
	dynstats-json-vg.sh \
	stats-json-vg.sh \
	stats-cee-vg.sh \
	dynstats_prevent_premature_eviction-vg.sh \
	dynstats-persist-vg.sh

TESTS_IMPTCP = \
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

TESTS_IMPTCP_VALGRIND = \
	imptcp-octet-framing-too-long-vg.sh \
	imptcp_conndrop-vg.sh

TESTS_IMPTCP_FMHASH_VALGRIND = \
	rscript_hash32-vg.sh \
	rscript_hash64-vg.sh

TESTS_IMPTCP_FMUNFLATTEN = \
	rscript_unflatten_arg1_unsuitable.sh \
	rscript_unflatten_arg2_invalid.sh \
	rscript_unflatten_conflict1.sh \
	rscript_unflatten_conflict2.sh \
	rscript_unflatten_conflict3.sh \
	rscript_unflatten_key_truncated.sh \
	rscript_unflatten_non_object.sh \
	rscript_unflatten_object.sh \
	rscript_unflatten_object_exclamation.sh

TESTS_IMPTCP_FMUNFLATTEN_VALGRIND = \
	rscript_unflatten_arg1_unsuitable-vg.sh \
	rscript_unflatten_arg2_invalid-vg.sh \
	rscript_unflatten_conflict1-vg.sh \
	rscript_unflatten_conflict2-vg.sh \
	rscript_unflatten_conflict3-vg.sh \
	rscript_unflatten_key_truncated-vg.sh \
	rscript_unflatten_non_object-vg.sh \
	rscript_unflatten_object-vg.sh \
	rscript_unflatten_object_exclamation-vg.sh

TESTS_IMPTCP_FFAUP = \
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

TESTS_IMPTCP_FFAUP_VALGRIND = \
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

TESTS_FMPCRE = \
	ffmpcre-basic.sh

TESTS_MMPSTRUCDATA = \
	mmpstrucdata.sh \
	mmpstrucdata-escaping.sh \
	mmpstrucdata-case.sh

TESTS_MMPSTRUCDATA_VALGRIND = \
	mmpstrucdata-vg.sh \
	mmpstrucdata-invalid-vg.sh

TESTS_MMRM1STSPACE = \
	mmrm1stspace-basic.sh

TESTS_PMNULL = \
	pmnull-basic.sh \
	pmnull-withparams.sh

TESTS_OMSTDOUT = \
	omstdout-basic.sh

TESTS_PMNORMALIZE = \
	pmnormalize-basic.sh \
	pmnormalize-invld-rulebase.sh \
	pmnormalize-rule.sh \
	pmnormalize-rule_and_rulebase.sh \
	pmnormalize-neither_rule_rulebase.sh \
	pmnormalize-rule_invld-data.sh

TESTS_PMNORMALIZE_VALGRIND = \
	pmnormalize-basic-vg.sh \
	pmnormalize-invld-rulebase-vg.sh \
	pmnormalize-rule-vg.sh \
	pmnormalize-rule_and_rulebase-vg.sh \
	pmnormalize-neither_rule_rulebase-vg.sh \
	pmnormalize-rule_invld-data-vg.sh

TESTS_MMNORMALIZE = \
        msgvar-concurrency-array.sh \
        msgvar-concurrency-array-event.tags.sh \
        mmnormalize_rule_from_string.sh \
        mmnormalize_rule_from_array.sh \
        mmnormalize_parsesuccess.sh

TESTS_MMNORMALIZE_VALGRIND = \
	mmnormalize_parsesuccess-vg.sh

TESTS_MMNORMALIZE_IMPTCP = \
	mmnormalize_regex_defaulted.sh \
	mmnormalize_regex_disabled.sh \
	mmnormalize_variable.sh \
	mmnormalize_tokenized.sh

TESTS_MMNORMALIZE_REGEX = \
	mmnormalize_regex.sh

TESTS_MMLEEFPARSE = \
	mmleefparse_basic.sh

TESTS_MMJSONPARSE_PART1 = \
	mmjsonparse_qradar.sh \
        mmjsonparse-w-o-cookie.sh \
        mmjsonparse-w-o-cookie-multi-spaces.sh \
        mmjsonparse-find-json-basic.sh \
        mmjsonparse-find-json-trailing.sh \
        mmjsonparse-find-json-scan-limit.sh \
        mmjsonparse-find-json-invalid.sh \
        mmjsonparse-find-json-invalid-mode.sh \
        mmjsonparse-find-json-parser-validation.sh

TESTS_MMJSONPARSE_IMPSTATS = \
	mmjsonparse-invalid-containerName.sh \
	wtpShutdownAll-assertionFailure.sh

TESTS_MMJSONPARSE_IMPTCP = \
	mmjsonparse_simple.sh \
	imptcp-oversize-message-display.sh \
	imptcp-msg-truncation-on-number.sh \
	imptcp-msg-truncation-on-number2.sh \
	imtcp-netns.sh \
	imptcp-maxFrameSize-parameter.sh \
	mmjsonparse_cim.sh \
	mmjsonparse_cim2.sh \
	mmjsonparse_localvar.sh \
	json_array_subscripting.sh \
	json_array_looping.sh \
	json_object_looping.sh \
	json_nonarray_looping.sh

TESTS_MMJSONPARSE_VALGRIND = \
	mmjsonparse_extra_data-vg.sh \
	json_null_array-vg.sh \
	json_object_looping-vg.sh \
	json_array_looping-vg.sh \
	json_object_suicide_in_loop-vg.sh \
	json_null-vg.sh

TESTS_MMJSONPARSE_PART2 = \
	stop_when_array_has_element.sh \
	json_null_array.sh \
	json_null.sh \
        json_var_cmpr.sh \
        json_var_case.sh

TESTS_MMJSONTRANSFORM_IMTCP = \
        mmjsontransform-basic.sh

TESTS_MMJSONTRANSFORM_MMJSONPARSE = \
	data_pipeline-qradar.sh

TESTS_MMDBLOOKUP = \
    mmdb.sh \
    mmdb-space.sh \
    mmdb-container.sh \
    mmdb-container-empty.sh

TESTS_MMDBLOOKUP_VALGRIND = \
    mmdb-vg.sh \
    mmdb-multilevel-vg.sh

TESTS_GNUTLS = \
	imtcp-tls-basic.sh \
	imtcp-tls-input-basic.sh \
	imtcp-tls-input-2certs.sh \
	imtcp-tls-basic-verifydepth.sh \
	imtcp-tls-gtls-revocationcheck-error.sh \
	imtcp_conndrop_tls.sh \
	imtcp-tls-gibberish.sh \
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
	omfwd-tls-gtls-no-sni.sh \
	omfwd-tls-gtls-auto-sni.sh \
	omfwd-tls-gtls-configured-sni.sh \
	imtcp-drvr-in-input-basic.sh \
	imtcp-multi-drvr-basic.sh \
	imtcp-multi-drvr-basic-parallel.sh

TESTS_GNUTLS_VALGRIND = \
	imtcp-tls-basic-vg.sh \
	imtcp_conndrop_tls-vg.sh \
	manytcp-too-few-tls-vg.sh

TESTS_GNUTLS_OPENSSL = \
	imtcp-multi-drvr-basic-ptcp_gtls_ossl.sh

TESTS_MBEDTLS = \
	imtcp-multi-drvr-basic-ptcp_gtls_mbedtls.sh \
	imtcp-tls-mbedtls-basic.sh \
	imtcp-tls-mbedtls-basic-verifydepth.sh \
	imtcp-tls-mbedtls-error-ca.sh \
	imtcp-tls-mbedtls-error-cert.sh \
	imtcp-tls-mbedtls-error-key2.sh \
	imtcp-tls-mbedtls-error-key.sh \
	imtcp-tls-mbedtls-input-basic.sh \
	imtcp-tls-mbedtls-x509invalid.sh \
	imtcp-tls-mbedtls-x509name_invalid.sh \
	imtcp-tls-mbedtls-x509name.sh \
	imtcp-tls-mbedtls-x509valid.sh \
	imtcp-tls-mbedtls-x509fingerprint-invld.sh \
	imtcp-tls-mbedtls-x509fingerprint.sh \
	imtcp-tls-mbedtls-x509name-multi-wildcard.sh \
	sndrcv_tls_certless_clientonly_mbedtls.sh \
	sndrcv_tls_gtls_serveranon_mbedtls_clientanon.sh \
	sndrcv_tls_gtls_servercert_mbedtls_clientanon.sh \
	sndrcv_tls_mbedtls_certvalid_action_level.sh \
	sndrcv_tls_mbedtls_certvalid_expired.sh \
	sndrcv_tls_mbedtls_certvalid_revoked.sh \
	sndrcv_tls_mbedtls_certvalid.sh \
	sndrcv_tls_mbedtls_servercert_clientcert.sh \
	sndrcv_tls_mbedtls_servercert_mbedtls_clientanon.sh \
	sndrcv_tls_servercert_mbedtls_clientanon.sh \
	sndrcv_tls_mbedtls_certvalid_expired_autrzd.sh \
	sndrcv_tls_mbedtls_gnutlspriorityString_no_suite_in_common.sh \
	sndrcv_tls_mbedtls_gnutlspriorityString_suite_in_common.sh \
	sndrcv_tls_servercert_bad_eku_chk_mbedtls_clientanon.sh \
	sndrcv_tls_servercert_bad_eku_no_chk_mbedtls_clientanon.sh

TESTS_MBEDTLS_VALGRIND = \
	imtcp-tls-mbedtls-basic-vg.sh \
	imtcp-tls-mbedtls-basic-brokenhandshake-vg.sh

TESTS_OSSL = \
	imtcp-tls-ossl-input-2certs.sh \
	imtcp-tls-ossl-basic-verifydepth.sh \
	imtcp-tls-ossl-revocationcheck-off.sh \
	imtcp-tls-ossl-invalid-verifydepth.sh \
	sndrcv_tls_ossl_certvalid.sh \
	sndrcv_tls_ossl_certvalid_action_level.sh \
	sndrcv_tls_ossl_certvalid_tlscommand.sh \
	sndrcv_tls_ossl_certvalid_ciphers.sh \
	sndrcv_tls_ossl_intermediate_ca_chain.sh \
	imtcp-tls-ossl-x509valid.sh \
	imtcp-tls-ossl-x509name.sh \
	imtcp-tls-ossl-x509fingerprint.sh \
	imtcp-tls-ossl-error-ca.sh \
	imtcp-tls-ossl-error-cert.sh \
	imtcp-tls-ossl-error-key.sh

TESTS_OSSL_OPENSSL = \
	imtcp-tls-ossl-basic.sh \
	imtcp-tls-ossl-input-basic.sh \
	imtcp-tls-ossl-basic-tlscommands.sh \
	imtcp-tls-ossl-error-key2.sh \
	sndrcv_tls_ossl_anon_ipv4.sh \
	sndrcv_tls_ossl_anon_ipv6.sh \
	sndrcv_tls_ossl_anon_rebind.sh \
	sndrcv_tls_ossl_anon_ciphers.sh \
	sndrcv_tls_ossl_serveranon_ossl_clientanon.sh \
	sndrcv_tls_ossl_servercert_ossl_clientanon.sh \
	sndrcv_tls_ossl_certvalid_expired.sh \
	sndrcv_tls_ossl_certvalid_revoked.sh

TESTS_OSSL_OPENSSL_VALGRIND = \
	imtcp-tls-ossl-basic-vg.sh \
	imtcp-tls-ossl-basic-brokenhandshake-vg.sh

TESTS_OSSL_OPENSSL_SNI = \
	omfwd-tls-ossl-no-sni.sh \
	omfwd-tls-ossl-auto-sni.sh \
	omfwd-tls-ossl-configured-sni.sh

TESTS_GNUTLS_OPENSSL_INTEROP = \
	sndrcv_tls_ossl_servercert_gtls_clientanon.sh \
	sndrcv_tls_ossl_serveranon_gtls_clientanon.sh \
	sndrcv_tls_gtls_servercert_ossl_clientanon.sh \
	sndrcv_tls_gtls_serveranon_ossl_clientanon.sh

TESTS_OMUXSOCK = \
	uxsock_simple.sh \
	 uxsock_simple_abstract.sh \
	 uxsock_multiple.sh \
	 uxsock_multiple_netns.sh

TESTS_OMUXSOCK_VALGRIND = \
	uxsock_simple_abstract-vg.sh \
	 uxsock_multiple-vg.sh \
	 uxsock_multiple_netns-vg.sh

TESTS_RELP = \
	sndrcv_relp.sh \
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

TESTS_RELP_GNUTLS = \
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

TESTS_RELP_GNUTLS_CFG = \
	 imrelp-tls-cfgcmd.sh \
	 sndrcv_relp_tls-cfgcmd.sh

TESTS_RELP_VALGRIND = \
	 imrelp-basic-vg.sh \
	 imrelp-sessionbreak-vg.sh \
	 imrelp-manyconn-vg.sh \
	 sndrcv_relp-vg-rcvr.sh \
	 sndrcv_relp-vg-sender.sh

TESTS_IMDTLS = \
	imdtls-basic.sh \
	imdtls-basic-tlscommands.sh \
	imdtls-basic-timeout.sh \
	imdtls-error-cert.sh \
	imdtls-sessionbreak.sh

TESTS_IMDTLS_VALGRIND = \
	imdtls-basic-vg.sh \
	imdtls-sessionbreak-vg.sh

TESTS_IMDTLS_OMDTLS = \
	sndrcv_dtls_certvalid.sh \
	sndrcv_dtls_anon_ciphers.sh \
	sndrcv_dtls_certvalid_ciphers.sh \
	sndrcv_dtls_certvalid_permitted.sh \
	sndrcv_dtls_certvalid_missing.sh \
	sndrcv_dtls_anon_ciphers.sh

TESTS_IMDTLS_OMDTLS_VALGRIND = \
	sndrcv_dtls_certvalid-vg.sh

TESTS_OMUDPSPOOF = \
	omudpspoof_errmsg_no_params.sh \
	sndrcv_omudpspoof.sh \
	sndrcv_omudpspoof-bigmsg.sh \
	sndrcv_omudpspoof_nonstdpt.sh

TESTS_OMRULESET = \
	omruleset.sh \
	omruleset-queue.sh

TESTS_PMDB2DIAG = \
	pmdb2diag_parse.sh

TESTS_PMSNARE = \
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

TESTS_PMLASTMSG = \
	pmlastmsg.sh \
	pmlastmsg-udp.sh

TESTS_IMFILE_EXTENDED = \
	imfile-basic-2GB-file.sh \
	imfile-truncate-2GB-file.sh

TESTS_IMFILE = \
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
	imfile-logrotate-state-files.sh \
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

TESTS_IMFILE_MMNORMALIZE = \
	imfile-endmsg.regex-with-example.sh

TESTS_IMFILE_IMPSTATS = \
	imfile-statistics.sh

TESTS_IMFILE_VALGRIND = \
	imfile-basic-vg.sh \
	imfile-endregex-vg.sh \
	imfile-endmsg.regex-vg.sh \
	imfile-readmode0-vg.sh \
	imfile-readmode2-vg.sh

TESTS_IMFILE_HELGRIND = \
	imfile-basic-vgthread.sh

TESTS_IMFILE_MMNORMALIZE_VALGRIND = \
	imfile-endmsg.regex-with-example-vg.sh

TESTS_IMBATCHREPORT = \
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

TESTS_IMBATCHREPORT_VALGRIND = \
	imbatchreport_errmsg_no_params-vg.sh

TESTS_OMTCL = \
	omtcl.sh

TESTS_MMTAGHOSTNAME = \
	mmtaghostname_tag.sh \
	mmtaghostname_server.sh

TESTS_MMKUBERNETES = \
	mmkubernetes-basic.sh \
	mmkubernetes-cache-expire.sh

TESTS_MMKUBERNETES_VALGRIND = \
	mmkubernetes-basic-vg.sh \
	mmkubernetes-cache-expire-vg.sh

TESTS_IMTUXEDOULOG = \
	imtuxedoulog_errmsg_no_params.sh \
	imtuxedoulog_data.sh

TESTS_IMTUXEDOULOG_VALGRIND = \
	imtuxedoulog_errmsg_no_params-vg.sh

TESTS_OMAMQP1 = \
	omamqp1-basic.sh

TESTS_OMAMQP1_VALGRIND = \
	omamqp1-basic-vg.sh

TESTS_SRV_DISCOVERY = \
	omfwd-srv-discovery-tcp.sh \
	omfwd-srv-discovery-udp.sh \
	omfwd-srv-discovery-errors.sh

TESTS_ENVIRONMENT = RSYSLOG_MODDIR='$(abs_top_builddir)'/runtime/.libs/
TESTS_ENVIRONMENT+= TOP_BUILDDIR='$(top_builddir)'

EXTRA_DIST += $(TESTS_ELASTICSEARCH_MINIMAL)
EXTRA_DIST += $(TESTS_ELASTICSEARCH_MINIMAL_VALGRIND)
EXTRA_DIST += $(TESTS_ELASTICSEARCH_MINIMAL_HELGRIND)
EXTRA_DIST += $(TESTS_ELASTICSEARCH_SERVER)
EXTRA_DIST += $(TESTS_ELASTICSEARCH_IMPSTATS)
EXTRA_DIST += $(TESTS_ELASTICSEARCH_IMFILE)
EXTRA_DIST += $(TESTS_ELASTICSEARCH_VALGRIND)
EXTRA_DIST += $(TESTS_DEFAULT)
EXTRA_DIST += $(TESTS_SKELETONS)
EXTRA_DIST += $(TESTS_INACTIVE)
EXTRA_DIST += $(TESTS_MMSNAREPARSE_MINIMAL)
EXTRA_DIST += $(TESTS_MMSNAREPARSE_VALGRIND)
EXTRA_DIST += $(TESTS_OMSENDERTRACK)
EXTRA_DIST += $(TESTS_OMSENDERTRACK_VALGRIND)
EXTRA_DIST += $(TESTS_LIBZSTD)
EXTRA_DIST += $(TESTS_LIBZSTD_VALGRIND)
EXTRA_DIST += $(TESTS_LIBGCRYPT)
EXTRA_DIST += $(TESTS_GNUTLS_WRONG_OPT)
EXTRA_DIST += $(TESTS_OSSL_WRONG_OPT)
EXTRA_DIST += $(TESTS_DEFAULT_VALGRIND)
EXTRA_DIST += $(TESTS_LIBGCRYPT_VALGRIND)
EXTRA_DIST += $(TESTS_LIBYAML)
EXTRA_DIST += $(TESTS_IMTCP)
EXTRA_DIST += $(TESTS_IMTCP_YAML)
EXTRA_DIST += $(TESTS_IMTCP_VALGRIND)
EXTRA_DIST += $(TESTS_IMTCP_IMPSTATS)
EXTRA_DIST += $(TESTS_RATELIMIT)
EXTRA_DIST += $(TESTS_SNMP_MINIMAL)
EXTRA_DIST += $(TESTS_SNMP_FULL)
EXTRA_DIST += $(TESTS_MMUTF8FIX)
EXTRA_DIST += $(TESTS_MMAITAG)
EXTRA_DIST += $(TESTS_MAIL)
EXTRA_DIST += $(TESTS_MMANON)
EXTRA_DIST += $(TESTS_CLICKHOUSE)
EXTRA_DIST += $(TESTS_CLICKHOUSE_VALGRIND)
EXTRA_DIST += $(TESTS_LIBFAKETIME)
EXTRA_DIST += $(TESTS_MMNORMALIZE_FAKETIME)
EXTRA_DIST += $(TESTS_PGSQL)
EXTRA_DIST += $(TESTS_PGSQL_VALGRIND)
EXTRA_DIST += $(TESTS_MYSQL)
EXTRA_DIST += $(TESTS_MYSQL_VALGRIND)
EXTRA_DIST += $(TESTS_LIBDBI)
EXTRA_DIST += $(TESTS_LIBDBI_VALGRIND)
EXTRA_DIST += $(TESTS_FMHTTP)
EXTRA_DIST += $(TESTS_FMHTTP_VALGRIND)
EXTRA_DIST += $(TESTS_ROOT)
EXTRA_DIST += $(TESTS_ROOT_IP)
EXTRA_DIST += $(TESTS_ROOT_VALGRIND)
EXTRA_DIST += $(TESTS_OMHTTP_FLAKY)
EXTRA_DIST += $(TESTS_OMHTTP_FLAKY_VALGRIND)
EXTRA_DIST += $(TESTS_IMJOURNAL)
EXTRA_DIST += $(TESTS_IMJOURNAL_VALGRIND)
EXTRA_DIST += $(TESTS_OMJOURNAL)
EXTRA_DIST += $(TESTS_IMPROG)
EXTRA_DIST += $(TESTS_IMPROG_VALGRIND)
EXTRA_DIST += $(TESTS_MMDARWIN)
EXTRA_DIST += $(TESTS_MMDARWIN_VALGRIND)
EXTRA_DIST += $(TESTS_OMPROG)
EXTRA_DIST += $(TESTS_OMPROG_VALGRIND)
EXTRA_DIST += $(TESTS_OMHTTP)
EXTRA_DIST += $(TESTS_OMHTTP_VALGRIND)
EXTRA_DIST += $(TESTS_OMOTEL)
EXTRA_DIST += $(TESTS_KAFKA)
EXTRA_DIST += $(TESTS_KAFKA_VALGRIND)
EXTRA_DIST += $(TESTS_OMAZUREEVENTHUBS)
EXTRA_DIST += $(TESTS_OMAZUREEVENTHUBS_VALGRIND)
EXTRA_DIST += $(TESTS_IMDOCKER)
EXTRA_DIST += $(TESTS_IMDOCKER_VALGRIND)
EXTRA_DIST += $(TESTS_IMHTTP)
EXTRA_DIST += $(TESTS_IMHTTP_VALGRIND)
EXTRA_DIST += $(TESTS_OMRABBITMQ)
EXTRA_DIST += $(TESTS_OMRABBITMQ_VALGRIND)
EXTRA_DIST += $(TESTS_IMHIREDIS)
EXTRA_DIST += $(TESTS_IMHIREDIS_VALGRIND)
EXTRA_DIST += $(TESTS_OMHIREDIS)
EXTRA_DIST += $(TESTS_OMHIREDIS_VALGRIND)
EXTRA_DIST += $(TESTS_IMPSTATS)
EXTRA_DIST += $(TESTS_IMPSTATS_VALGRIND)
EXTRA_DIST += $(TESTS_IMPTCP)
EXTRA_DIST += $(TESTS_IMPTCP_VALGRIND)
EXTRA_DIST += $(TESTS_IMPTCP_FMHASH_VALGRIND)
EXTRA_DIST += $(TESTS_IMPTCP_FMUNFLATTEN)
EXTRA_DIST += $(TESTS_IMPTCP_FMUNFLATTEN_VALGRIND)
EXTRA_DIST += $(TESTS_IMPTCP_FFAUP)
EXTRA_DIST += $(TESTS_IMPTCP_FFAUP_VALGRIND)
EXTRA_DIST += $(TESTS_FMPCRE)
EXTRA_DIST += $(TESTS_MMPSTRUCDATA)
EXTRA_DIST += $(TESTS_MMPSTRUCDATA_VALGRIND)
EXTRA_DIST += $(TESTS_MMRM1STSPACE)
EXTRA_DIST += $(TESTS_PMNULL)
EXTRA_DIST += $(TESTS_OMSTDOUT)
EXTRA_DIST += $(TESTS_PMNORMALIZE)
EXTRA_DIST += $(TESTS_PMNORMALIZE_VALGRIND)
EXTRA_DIST += $(TESTS_MMNORMALIZE)
EXTRA_DIST += $(TESTS_MMNORMALIZE_VALGRIND)
EXTRA_DIST += $(TESTS_MMNORMALIZE_IMPTCP)
EXTRA_DIST += $(TESTS_MMNORMALIZE_REGEX)
EXTRA_DIST += $(TESTS_MMLEEFPARSE)
EXTRA_DIST += $(TESTS_MMJSONPARSE_PART1)
EXTRA_DIST += $(TESTS_MMJSONPARSE_IMPSTATS)
EXTRA_DIST += $(TESTS_MMJSONPARSE_IMPTCP)
EXTRA_DIST += $(TESTS_MMJSONPARSE_VALGRIND)
EXTRA_DIST += $(TESTS_MMJSONPARSE_PART2)
EXTRA_DIST += $(TESTS_MMJSONTRANSFORM_IMTCP)
EXTRA_DIST += $(TESTS_MMJSONTRANSFORM_MMJSONPARSE)
EXTRA_DIST += $(TESTS_MMDBLOOKUP)
EXTRA_DIST += $(TESTS_MMDBLOOKUP_VALGRIND)
EXTRA_DIST += $(TESTS_GNUTLS)
EXTRA_DIST += $(TESTS_GNUTLS_VALGRIND)
EXTRA_DIST += $(TESTS_GNUTLS_OPENSSL)
EXTRA_DIST += $(TESTS_MBEDTLS)
EXTRA_DIST += $(TESTS_MBEDTLS_VALGRIND)
EXTRA_DIST += $(TESTS_OSSL)
EXTRA_DIST += $(TESTS_OSSL_OPENSSL)
EXTRA_DIST += $(TESTS_OSSL_OPENSSL_VALGRIND)
EXTRA_DIST += $(TESTS_OSSL_OPENSSL_SNI)
EXTRA_DIST += $(TESTS_GNUTLS_OPENSSL_INTEROP)
EXTRA_DIST += $(TESTS_OMUXSOCK)
EXTRA_DIST += $(TESTS_OMUXSOCK_VALGRIND)
EXTRA_DIST += $(TESTS_RELP)
EXTRA_DIST += $(TESTS_RELP_GNUTLS)
EXTRA_DIST += $(TESTS_RELP_GNUTLS_CFG)
EXTRA_DIST += $(TESTS_RELP_VALGRIND)
EXTRA_DIST += $(TESTS_IMDTLS)
EXTRA_DIST += $(TESTS_IMDTLS_VALGRIND)
EXTRA_DIST += $(TESTS_IMDTLS_OMDTLS)
EXTRA_DIST += $(TESTS_IMDTLS_OMDTLS_VALGRIND)
EXTRA_DIST += $(TESTS_OMUDPSPOOF)
EXTRA_DIST += $(TESTS_OMRULESET)
EXTRA_DIST += $(TESTS_PMDB2DIAG)
EXTRA_DIST += $(TESTS_PMSNARE)
EXTRA_DIST += $(TESTS_PMLASTMSG)
EXTRA_DIST += $(TESTS_IMFILE_EXTENDED)
EXTRA_DIST += $(TESTS_IMFILE)
EXTRA_DIST += $(TESTS_IMFILE_MMNORMALIZE)
EXTRA_DIST += $(TESTS_IMFILE_IMPSTATS)
EXTRA_DIST += $(TESTS_IMFILE_VALGRIND)
EXTRA_DIST += $(TESTS_IMFILE_HELGRIND)
EXTRA_DIST += $(TESTS_IMFILE_MMNORMALIZE_VALGRIND)
EXTRA_DIST += $(TESTS_IMBATCHREPORT)
EXTRA_DIST += $(TESTS_IMBATCHREPORT_VALGRIND)
EXTRA_DIST += $(TESTS_OMTCL)
EXTRA_DIST += $(TESTS_MMTAGHOSTNAME)
EXTRA_DIST += $(TESTS_MMKUBERNETES)
EXTRA_DIST += $(TESTS_MMKUBERNETES_VALGRIND)
EXTRA_DIST += $(TESTS_IMTUXEDOULOG)
EXTRA_DIST += $(TESTS_IMTUXEDOULOG_VALGRIND)
EXTRA_DIST += $(TESTS_OMAMQP1)
EXTRA_DIST += $(TESTS_OMAMQP1_VALGRIND)
EXTRA_DIST += $(TESTS_SRV_DISCOVERY)


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
if ENABLE_TESTBENCH
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
TESTS += $(TESTS_ELASTICSEARCH_MINIMAL)

es-basic.log: es-basic-es7.14.log
es-basic-bulk.log: es-basic.log
es-basic-server.log: es-basic-bulk.log

# special "test" for stopping ES once all ES tests are done
elasticsearch-stop.log: es-basic-bulk.log


if HAVE_VALGRIND
TESTS += $(TESTS_ELASTICSEARCH_MINIMAL_VALGRIND)
es-basic-vg.log: es-basic-bulk.log
# for next if block:
es-basic-server.log: es-basic-vg.log
elasticsearch-stop.log: es-basic-vg.log
if ENABLE_HELGRIND

TESTS += $(TESTS_ELASTICSEARCH_MINIMAL_HELGRIND)

es-basic-vgthread.log: es-basic-vg.log
# for next if block:
es-basic-server.log: es-basic-vgthread.log
elasticsearch-stop.log: es-basic-vgthread.log
endif # ENABLE_HELGRIND
endif # HAVE_VALGRIND
endif # ENABLE_ELASTICSEARCH_TESTS_MINIMAL






if ENABLE_ELASTICSEARCH_TESTS
TESTS += $(TESTS_ELASTICSEARCH_SERVER)

es-basic-server.log: es-basic-bulk.log
es-execOnlyWhenPreviousSuspended.log: es-basic-server.log
es-maxbytes-bulk.log: es-execOnlyWhenPreviousSuspended.log
es-basic-errfile-empty.log: es-maxbytes-bulk.log
es-basic-errfile-popul.log: es-basic-errfile-empty.log
es-bulk-errfile-empty.log: es-basic-errfile-popul.log
es-bulk-errfile-popul.log: es-bulk-errfile-empty.log
diskqueue-multithread-es.log: es-bulk-errfile-popul.log
es-writeoperation.log: diskqueue-multithread-es.log
elasticsearch-stop.log: es-writeoperation.log

if ENABLE_IMPSTATS
TESTS += $(TESTS_ELASTICSEARCH_IMPSTATS)

es-basic-ha.log: es-writeoperation.log
es-bulk-retry.log: es-basic-ha.log
elasticsearch-stop.log: es-bulk-retry.log
endif

if ENABLE_IMFILE
TESTS += $(TESTS_ELASTICSEARCH_IMFILE)

es-bulk-errfile-popul-def-format.log: es-bulk-retry.log
es-bulk-errfile-popul-erronly.log: es-bulk-errfile-popul-def-format.log
es-bulk-errfile-popul-erronly-interleaved.log: es-bulk-errfile-popul-erronly.log
es-bulk-errfile-popul-def-interleaved.log: es-bulk-errfile-popul-erronly-interleaved.log
elasticsearch-stop.log: es-bulk-errfile-popul-def-interleaved.log
endif

if HAVE_VALGRIND
TESTS += $(TESTS_ELASTICSEARCH_VALGRIND)

es-basic-bulk-vg.log: es-writeoperation.log
es-basic-bulk-vg.log: es-bulk-retry.log
es-basic-bulk-vg.log: es-bulk-errfile-popul-def-interleaved.log
es-basic-ha-vg.log: es-basic-bulk-vg.log
elasticsearch-stop.log: es-basic-ha-vg.log
endif
endif # if ENABLE_ELASTICSEARCH_TESTS




# Helper skeletons which are required by other active tests but should not be run standalone

# The following tests were found to be distributed (in EXTRA_DIST) but not executed
# (missing from TESTS) in the main branch. They likely have pre-existing issues
# (e.g. expired certificates, timing races) and should be audited before re-enabling.












if ENABLE_DEFAULT_TESTS
TESTS += $(TESTS_DEFAULT)
TESTS += $(TESTS_JSON_OMITIFZERO)

if ENABLE_MMSNAREPARSE
TESTS += $(TESTS_MMSNAREPARSE_MINIMAL)
if HAVE_VALGRIND
TESTS += $(TESTS_MMSNAREPARSE_VALGRIND)
endif
endif

if ENABLE_OMSENDERTRACK
TESTS += $(TESTS_OMSENDERTRACK)
if HAVE_VALGRIND
TESTS += $(TESTS_OMSENDERTRACK_VALGRIND)
endif # if HAVE_VALGRIND
endif

if ENABLE_LIBZSTD
TESTS += $(TESTS_LIBZSTD)
if HAVE_VALGRIND
TESTS += $(TESTS_LIBZSTD_VALGRIND)
endif # if HAVE_VALGRIND
endif

if ENABLE_LIBGCRYPT
TESTS += $(TESTS_LIBGCRYPT)
endif # ENABLE_LIBGCRYPT
if HAVE_VALGRIND
# note if the same test is added multiple times, it still is only executed once!
if ENABLE_GNUTLS_TESTS
TESTS += $(TESTS_GNUTLS_WRONG_OPT)
endif # ENABLE_GNUTLS_TESTS
if ENABLE_OSSL
TESTS += $(TESTS_OSSL_WRONG_OPT)
endif # ENABLE_OSSL
TESTS += $(TESTS_DEFAULT_VALGRIND)
if ENABLE_LIBGCRYPT
TESTS += $(TESTS_LIBGCRYPT_VALGRIND)
endif # ENABLE_LIBGCRYPT
endif # HAVE_VALGRIND
if HAVE_LIBYAML
TESTS += $(TESTS_LIBYAML)
endif # HAVE_LIBYAML
endif # ENABLE_DEFAULT_TESTS






if ENABLE_IMTCP_TESTS
TESTS += $(TESTS_IMTCP)

if HAVE_LIBYAML
TESTS += $(TESTS_IMTCP_YAML)
endif # HAVE_LIBYAML
if ENABLE_IMPTCP
TESTS += $(TESTS_RATELIMIT)
endif # ENABLE_IMPTCP

if HAVE_VALGRIND
TESTS += $(TESTS_IMTCP_VALGRIND)
endif # HAVE_VALGRIND
if ENABLE_IMTCP_EPOLL
if ENABLE_IMPSTATS
TESTS += $(TESTS_IMTCP_IMPSTATS)
endif # ENABLE_IMPSTATS
endif # ENABLE_IMTCP_EPOLL
endif # ENABLE_IMTCP_TESTS



if ENABLE_SNMP
TESTS += $(TESTS_SNMP_MINIMAL)
if ENABLE_SNMP_TESTS
TESTS += $(TESTS_SNMP_FULL)
endif # if ENABLE_SNMP_TESTS
endif # if ENABLE_SNMP


if ENABLE_MMUTF8FIX
TESTS += $(TESTS_MMUTF8FIX)
endif # if ENABLE_MMUTF8FIX


if ENABLE_MMAITAG
TESTS += $(TESTS_MMAITAG)
endif


if ENABLE_MAIL
TESTS += $(TESTS_MAIL)
endif # if ENABLE_MAIL


if ENABLE_MMANON
TESTS += $(TESTS_MMANON)
endif # if ENABLE_MMANON



if ENABLE_CLICKHOUSE_TESTS
TESTS += $(TESTS_CLICKHOUSE)

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
TESTS += $(TESTS_CLICKHOUSE_VALGRIND)

clickhouse-basic-vg.log: clickhouse-wrong-insert-syntax.log
clickhouse-load-vg.log: clickhouse-basic-vg.log
clickhouse-bulk-vg.log: clickhouse-load-vg.log
clickhouse-bulk-load-vg.log: clickhouse-bulk-vg.log
clickhouse-stop.log: clickhouse-bulk-load-vg.log
endif # VALGRIND
endif # CLICKHOUSE_TESTS




if ENABLE_LIBFAKETIME
TESTS += $(TESTS_LIBFAKETIME)

# now come faketime tests that utilize mmnormalize - aka "no endif here"
if ENABLE_MMNORMALIZE
TESTS += $(TESTS_MMNORMALIZE)
endif
endif



if ENABLE_PGSQL
if ENABLE_PGSQL_TESTS
TESTS += $(TESTS_PGSQL)

pgsql-basic-cnf6.log: pgsql-basic.log
pgsql-basic-threads-cnf6.log: pgsql-basic-cnf6.log
pgsql-template.log: pgsql-basic-threads-cnf6.log
pgsql-template-cnf6.log: pgsql-template.log
pgsql-actq-mt-withpause.log: pgsql-template-cnf6.log
pgsql-template-threads-cnf6.log: pgsql-actq-mt-withpause.log
if HAVE_VALGRIND
TESTS += $(TESTS_PGSQL_VALGRIND)

pgsql-basic-vg.log: pgsql-template-threads-cnf6.log
pgsql-template-vg.log: pgsql-basic-vg.log
pgsql-basic-cnf6-vg.log: pgsql-template-vg.log
pgsql-template-cnf6-vg.log: pgsql-basic-cnf6-vg.log
pgsql-actq-mt-withpause-vg.log: pgsql-template-cnf6-vg.log
endif
endif
endif





if ENABLE_MYSQL_TESTS
TESTS += $(TESTS_MYSQL)

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
TESTS += $(TESTS_MYSQL_VALGRIND)

mysql-basic-vg.log: mysqld-start.log
mysql-asyn-vg.log: mysqld-start.log
mysql-actq-mt-withpause-vg.log: mysqld-start.log

mysqld-stop.log: mysql-basic-vg.log \
	mysql-asyn-vg.log \
	mysql-actq-mt-withpause-vg.log
endif

if ENABLE_OMLIBDBI # we piggy-back on MYSQL_TESTS as we need the same environment
TESTS += $(TESTS_LIBDBI)

libdbi-basic.log: mysqld-start.log
libdbi-asyn.log: mysqld-start.log
mysqld-stop.log: libdbi-basic.log \
	libdbi-asyn.log
if HAVE_VALGRIND
TESTS += $(TESTS_LIBDBI_VALGRIND)

libdbi-basic-vg.log: mysqld-start.log
mysqld-stop.log: libdbi-basic-vg.log
endif
endif
endif #  MYSQL_TESTS



if ENABLE_FMHTTP
TESTS += $(TESTS_FMHTTP)
if HAVE_VALGRIND
TESTS += $(TESTS_FMHTTP_VALGRIND)
endif # HAVE_VALGRIND
endif # ENABLE_FMHTTP





if ENABLE_ROOT_TESTS
TESTS += $(TESTS_ROOT)
if ENABLE_IP
TESTS += $(TESTS_ROOT_IP)
endif
if HAVE_VALGRIND
TESTS += $(TESTS_ROOT_VALGRIND)
endif
endif

# see comments in configure.ac on flaky tests for important details!!!


if ENABLE_FLAKY_TESTS
#TESTS +=
#
if ENABLE_OMHTTP
TESTS += $(TESTS_OMHTTP_FLAKY)
if HAVE_VALGRIND
TESTS += $(TESTS_OMHTTP_FLAKY_VALGRIND)
endif # end if HAVE_VALGRIND
endif # end if ENABLE_OMHTTP
endif # end if ENABLE_FLAKY_TESTS




if ENABLE_JOURNAL_TESTS
if ENABLE_IMJOURNAL
TESTS += $(TESTS_IMJOURNAL)
if HAVE_VALGRIND
TESTS += $(TESTS_IMJOURNAL_VALGRIND)
endif
endif

if ENABLE_OMJOURNAL
TESTS += $(TESTS_OMJOURNAL)
endif
endif #if ENABLE_JOURNAL_TESTS



if ENABLE_IMPROG
TESTS += $(TESTS_IMPROG)
if HAVE_VALGRIND
TESTS += $(TESTS_IMPROG_VALGRIND)
endif # ENABLE_IMPROG
endif



if ENABLE_MMDARWIN
TESTS += $(TESTS_MMDARWIN)
if HAVE_VALGRIND
TESTS += $(TESTS_MMDARWIN_VALGRIND)
endif # HAVE_VALGRIND
endif # ENABLE_IMPROG



if ENABLE_OMPROG
TESTS += $(TESTS_OMPROG)
if HAVE_VALGRIND
TESTS += $(TESTS_OMPROG_VALGRIND)
endif
endif



if ENABLE_OMHTTP
TESTS += $(TESTS_OMHTTP)
if HAVE_VALGRIND
TESTS += $(TESTS_OMHTTP_VALGRIND)
endif
endif


if ENABLE_OMOTEL
TESTS += $(TESTS_OMOTEL)
endif



if ENABLE_OMKAFKA
if ENABLE_IMKAFKA
if ENABLE_KAFKA_TESTS
TESTS += $(TESTS_KAFKA)

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
omkafka-headers.log: omkafkadynakey.log

if HAVE_VALGRIND
TESTS += $(TESTS_KAFKA_VALGRIND)

omkafka-vg.log: omkafka-headers.log
imkafka-vg.log: omkafka-vg.log
endif
endif
endif
endif



if ENABLE_OMAZUREEVENTHUBS
if ENABLE_OMAZUREEVENTHUBS_TESTS
TESTS += $(TESTS_OMAZUREEVENTHUBS)
if HAVE_VALGRIND
TESTS += $(TESTS_OMAZUREEVENTHUBS_VALGRIND)
endif
endif
endif



if ENABLE_IMDOCKER
if ENABLE_IMDOCKER_TESTS
TESTS += $(TESTS_IMDOCKER)
if HAVE_VALGRIND
TESTS += $(TESTS_IMDOCKER_VALGRIND)
endif # HAVE_VALGRIND
endif # ENABLE_IMDOCKER_TESTS
endif # ENABLE_IMDOCKER



if ENABLE_IMHTTP
TESTS += $(TESTS_IMHTTP)
if HAVE_VALGRIND
TESTS += $(TESTS_IMHTTP_VALGRIND)
endif # HAVE_VALGRIND
endif # ENABLE_IMHTTP



if ENABLE_OMRABBITMQ
check_PROGRAMS += miniamqpsrvr
miniamqpsrvr_SOURCES = miniamqpsrvr.c
miniamqpsrvr_CPPFLAGS = $(PTHREADS_CFLAGS) $(RABBITMQ_CFLAGS) $(RSRT_CFLAGS)
miniamqpsrvr_LDADD = $(SOL_LIBS) $(PTHREADS_LIBS)

TESTS += $(TESTS_OMRABBITMQ)
if HAVE_VALGRIND
TESTS += $(TESTS_OMRABBITMQ_VALGRIND)
endif # HAVE_VALGRIND
endif # ENABLE_OMRABBITMQ





if ENABLE_REDIS_TESTS
if ENABLE_IMHIREDIS
TESTS += $(TESTS_IMHIREDIS)
if HAVE_VALGRIND
TESTS += $(TESTS_IMHIREDIS_VALGRIND)
endif # HAVE_VALGRIND
endif # ENABLE_IMHIREDIS

if ENABLE_OMHIREDIS
TESTS += $(TESTS_OMHIREDIS)
if HAVE_VALGRIND
TESTS += $(TESTS_OMHIREDIS_VALGRIND)
endif # HAVE_VALGRIND
endif # ENABLE_OMHIREDIS
endif # ENABLE_REDIS_TESTS



if ENABLE_IMPSTATS
TESTS += $(TESTS_IMPSTATS)
if HAVE_VALGRIND
TESTS += $(TESTS_IMPSTATS_VALGRIND)
endif
endif









if ENABLE_IMPTCP
# note that some tests simply USE imptcp, but they also
# need to be disabled if we do not have this module
TESTS += $(TESTS_IMPTCP)
if HAVE_VALGRIND
TESTS += $(TESTS_IMPTCP_VALGRIND)
if ENABLE_FMHASH
TESTS += $(TESTS_IMPTCP_FMHASH_VALGRIND)
endif # ENABLE_FMHASH
endif # HAVE_VALGRIND
if ENABLE_FMUNFLATTEN
TESTS += $(TESTS_IMPTCP_FMUNFLATTEN)
if HAVE_VALGRIND
TESTS += $(TESTS_IMPTCP_FMUNFLATTEN_VALGRIND)
endif # HAVE_VALGRIND
endif # ENABLE_FMUNFLATTEN
if ENABLE_FFAUP
TESTS += $(TESTS_IMPTCP_FFAUP)
if HAVE_VALGRIND
TESTS += $(TESTS_IMPTCP_FFAUP_VALGRIND)
endif # HAVE_VALGRIND
endif # ENABLE_FFAUP
endif


if ENABLE_FMPCRE
TESTS += $(TESTS_FMPCRE)
endif



if ENABLE_MMPSTRUCDATA
TESTS += $(TESTS_MMPSTRUCDATA)
if HAVE_VALGRIND
TESTS += $(TESTS_MMPSTRUCDATA_VALGRIND)
endif
endif


if ENABLE_MMRM1STSPACE
TESTS += $(TESTS_MMRM1STSPACE)
endif


if ENABLE_PMNULL
TESTS += $(TESTS_PMNULL)
endif


if ENABLE_OMSTDOUT
TESTS += $(TESTS_OMSTDOUT)
endif



if ENABLE_PMNORMALIZE
TESTS += $(TESTS_PMNORMALIZE)
if HAVE_VALGRIND
TESTS += $(TESTS_PMNORMALIZE_VALGRIND)
endif
endif





if ENABLE_MMNORMALIZE
TESTS += $(TESTS_MMNORMALIZE)

if HAVE_VALGRIND
TESTS += $(TESTS_MMNORMALIZE_VALGRIND)
endif

if ENABLE_IMPTCP
TESTS += $(TESTS_MMNORMALIZE_IMPTCP)
endif

if LOGNORM_REGEX_SUPPORTED
TESTS += $(TESTS_MMNORMALIZE_REGEX)
endif
endif # ENABLE_MMNORMALIZE


if ENABLE_MMLEEFPARSE
if ENABLE_IMPTCP
TESTS += $(TESTS_MMLEEFPARSE)
endif
endif






if ENABLE_MMJSONPARSE
TESTS += $(TESTS_MMJSONPARSE_PART1)
if ENABLE_IMPSTATS
TESTS += $(TESTS_MMJSONPARSE_IMPSTATS)
endif
if ENABLE_IMPTCP
TESTS += $(TESTS_MMJSONPARSE_IMPTCP)
endif
if HAVE_VALGRIND
TESTS += $(TESTS_MMJSONPARSE_VALGRIND)
endif
TESTS += $(TESTS_MMJSONPARSE_PART2)
endif



if ENABLE_MMJSONTRANSFORM
if ENABLE_IMTCP_TESTS
TESTS += $(TESTS_MMJSONTRANSFORM_IMTCP)
endif
if ENABLE_MMJSONPARSE
TESTS += $(TESTS_MMJSONTRANSFORM_MMJSONPARSE)
endif
endif



if ENABLE_MMDBLOOKUP
TESTS += $(TESTS_MMDBLOOKUP)
if HAVE_VALGRIND
TESTS += $(TESTS_MMDBLOOKUP_VALGRIND)
endif
endif




if ENABLE_GNUTLS_TESTS
TESTS += $(TESTS_GNUTLS)
if HAVE_VALGRIND
TESTS += $(TESTS_GNUTLS_VALGRIND)
endif
if ENABLE_OPENSSL
TESTS += $(TESTS_GNUTLS_OPENSSL)
endif
endif



if ENABLE_MBEDTLS
TESTS += $(TESTS_MBEDTLS)
if HAVE_VALGRIND
TESTS += $(TESTS_MBEDTLS_VALGRIND)
endif
endif





if ENABLE_OSSL
TESTS += $(TESTS_OSSL)

if ENABLE_OPENSSL
TESTS += $(TESTS_OSSL_OPENSSL)
if HAVE_VALGRIND
TESTS += $(TESTS_OSSL_OPENSSL_VALGRIND)
endif
endif

if ENABLE_OPENSSL
TESTS += $(TESTS_OSSL_OPENSSL_SNI)
endif
endif


if ENABLE_GNUTLS_TESTS
if ENABLE_OPENSSL
TESTS += $(TESTS_GNUTLS_OPENSSL_INTEROP)
endif
endif



if ENABLE_OMUXSOCK
TESTS += $(TESTS_OMUXSOCK)
if HAVE_VALGRIND
TESTS += $(TESTS_OMUXSOCK_VALGRIND)
endif # HAVE_VALGRIND
endif





if ENABLE_RELP
TESTS += $(TESTS_RELP)
if ENABLE_GNUTLS
TESTS += $(TESTS_RELP_GNUTLS)
if USE_RELPENGINESETTLSCFGCMD
TESTS += $(TESTS_RELP_GNUTLS_CFG)
endif # USE_RELPENGINESETTLSCFGCMD
endif # ENABLE_GNUTLS
if HAVE_VALGRIND
TESTS += $(TESTS_RELP_VALGRIND)
endif # HAVE_VALGRIND
endif





if ENABLE_IMDTLS
TESTS += $(TESTS_IMDTLS)
if HAVE_VALGRIND
TESTS += $(TESTS_IMDTLS_VALGRIND)
endif # HAVE_VALGRIND
if ENABLE_OMDTLS
TESTS += $(TESTS_IMDTLS_OMDTLS)
if HAVE_VALGRIND
TESTS += $(TESTS_IMDTLS_OMDTLS_VALGRIND)
endif # HAVE_VALGRIND
endif # ENABLE_OMDTLS
endif # ENABLE_IMDTLS


if ENABLE_OMUDPSPOOF
TESTS += $(TESTS_OMUDPSPOOF)
endif

#disabled as array-passing mode is no longer supported:	omod-if-array.sh
#	omod-if-array.sh
#	omod-if-array-udp.sh
if ENABLE_IMPTCP
TESTS += $(TESTS_IMPTCP_TABESCAPE)
endif


if ENABLE_OMRULESET
TESTS += $(TESTS_OMRULESET)
endif


if ENABLE_PMDB2DIAG
TESTS += $(TESTS_PMDB2DIAG)
endif


if ENABLE_PMSNARE
TESTS += $(TESTS_PMSNARE)
endif


if ENABLE_PMLASTMSG
TESTS += $(TESTS_PMLASTMSG)
endif


if ENABLE_EXTENDED_TESTS
# random.sh is temporarily disabled as it needs some work
# to rsyslog core to complete in reasonable time
#TESTS += random.sh
if ENABLE_IMFILE_TESTS
TESTS += $(TESTS_IMFILE_EXTENDED)
endif # ENABLE_IMFILE_TESTS
endif







if ENABLE_IMFILE_TESTS
TESTS += $(TESTS_IMFILE)

if ENABLE_MMNORMALIZE
TESTS += $(TESTS_IMFILE_MMNORMALIZE)
endif
if ENABLE_IMPSTATS
TESTS += $(TESTS_IMFILE_IMPSTATS)
endif

if HAVE_VALGRIND
TESTS += $(TESTS_IMFILE_VALGRIND)

if ENABLE_HELGRIND
TESTS += $(TESTS_IMFILE_HELGRIND)
endif #  ENABLE_HELGRIND

if ENABLE_MMNORMALIZE
TESTS += $(TESTS_IMFILE_MMNORMALIZE_VALGRIND)
endif # ENABLE_MMNORMALIZE

endif # HAVE_VALGRIND

endif # ENABLE_IMFILE_TESTS



if ENABLE_IMBATCHREPORT
TESTS += $(TESTS_IMBATCHREPORT)
if HAVE_VALGRIND
TESTS += $(TESTS_IMBATCHREPORT_VALGRIND)
endif # ENABLE_IMBATCHREPORT
endif


if ENABLE_OMTCL
TESTS += $(TESTS_OMTCL)
endif


if ENABLE_MMTAGHOSTNAME
TESTS += $(TESTS_MMTAGHOSTNAME)
endif



if ENABLE_MMKUBERNETES
if ENABLE_MMJSONPARSE
if ENABLE_IMFILE
if ENABLE_IMPSTATS
TESTS += $(TESTS_MMKUBERNETES)
if HAVE_VALGRIND
TESTS += $(TESTS_MMKUBERNETES_VALGRIND)
endif
endif
endif
endif
endif



if ENABLE_IMTUXEDOULOG
TESTS += $(TESTS_IMTUXEDOULOG)
if HAVE_VALGRIND
TESTS += $(TESTS_IMTUXEDOULOG_VALGRIND)
endif # ENABLE_IMTUXEDOULOG
endif



if ENABLE_OMAMQP1
TESTS += $(TESTS_OMAMQP1)
if HAVE_VALGRIND
TESTS += $(TESTS_OMAMQP1_VALGRIND)
endif
endif # ENABLE_OMAMQP1


if HAVE_SRV_DISCOVERY
TESTS += $(TESTS_SRV_DISCOVERY)
endif # HAVE_SRV_DISCOVERY

if ENABLE_GNUTLS
check_PROGRAMS += gnutls_sni_server
gnutls_sni_server_SOURCES = gnutls_sni_server.c
gnutls_sni_server_CFLAGS = $(GNUTLS_CFLAGS)
gnutls_sni_server_CPPFLAGS = $(GNUTLS_CFLAGS)
gnutls_sni_server_LDADD = $(GNUTLS_LIBS) $(SOL_LIBS)
endif
if ENABLE_OPENSSL
check_PROGRAMS += openssl_sni_server
openssl_sni_server_SOURCES = openssl_sni_server.c
openssl_sni_server_CFLAGS = $(OPENSSL_CFLAGS)
openssl_sni_server_CPPFLAGS = $(OPENSSL_CFLAGS)
openssl_sni_server_LDADD = $(OPENSSL_LIBS) $(SOL_LIBS)
endif

endif # if ENABLE_TESTBENCH

TESTS_ENVIRONMENT+= TESTTOOL_DIR='$(abs_top_builddir)/tests'
test_files = testbench.h runtime-dummy.c
DISTCLEANFILES=rsyslog.pid

distclean-local:
	rm -rf .dep_cache .dep_wrk



EXTRA_DIST += \
	faketime_common.sh \
	privdrop_common.sh \
	rscript_compare-common.sh \
	rscript_parse_time_get-ts.py \
	queue-persist-drvr.sh \
	daqueue-persist-drvr.sh \
	snmptrapreceiverv2.py \
	validate_json_shell.sh \
	tls-check-for-certificate.sh \
	set-envvars.in \
	urlencode.py \
	dns_srv_mock.py \
	omrelp-keepalive.sh \
	improg-simul.sh \
	sndrcv_drvr.sh \
	sndrcv_drvr_noexit.sh \
	diag.sh \
	testsuites/mmexternal-SegFault-mm-python.py \
	testsuites/mmsnareparse/sample-custom-pattern.data \
	testsuites/mmsnareparse/sample-windows2022-security.data \
	testsuites/mmsnareparse/sample-windows2025-security.data \
	testsuites/mmsnareparse/sample-events.data \
	1.rstest 2.rstest 3.rstest err1.rstest \
	testsuites/include-std1-omfile-action.conf \
	testsuites/include-std2-omfile-action.conf \
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
	ratelimit_name.sh \
	test_policy_hup.yaml \
	ratelimit_duplicate.sh \
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
	killrsyslog.sh \
	mmdb.rb \
	test.mmdb \
	with_space.mmdb \
	testsuites/es.yml \
	es_response_get_msgnum.py \
	elasticsearch-error-format-check.py \
	testsuites/imptcp_framing_regex.testdata \
	testsuites/imtcp-multiline.testdata \
	testsuites/imptcp_framing_regex-oversize.testdata \
	testsuites/imptcp_multi_line.testdata \
	testsuites/no_octet_counted.testdata \
	testsuites/spframingfix.testdata \
	bad_qi/dbq.qi \
	testsuites/mysql-truncate.sql \
	testsuites/mysql-select-msg.sql \
	../devtools/prep-mysql-db.sh \
	../devtools/prepare_clickhouse.sh \
	testsuites/kafka-server.properties \
	testsuites/kafka-server.dep_wrk1.properties \
	testsuites/kafka-server.dep_wrk2.properties \
	testsuites/kafka-server.dep_wrk3.properties \
	testsuites/zoo.cfg \
	testsuites/zoo.dep_wrk1.cfg \
	testsuites/zoo.dep_wrk2.cfg \
	testsuites/zoo.dep_wrk3.cfg \
	testsuites/dynstats_empty_input \
	testsuites/dynstats_input \
	testsuites/dynstats_input_1 \
	testsuites/dynstats_input_2 \
	testsuites/dynstats_input_3 \
	testsuites/dynstats_input_more_0 \
	testsuites/dynstats_input_more_1 \
	testsuites/dynstats_input_more_2 \
	testsuites/mmnormalize_variable.rulebase \
	testsuites/date_time_msg \
	testsuites/mmnormalize_tokenized.rulebase \
	testsuites/tokenized_input \
	testsuites/complex_replace_input \
	testsuites/wrap3_input\
	testsuites/json_array_input \
	testsuites/json_object_input \
	testsuites/json_nonarray_input \
	testsuites/mmnormalize_regex.rulebase \
	testsuites/regex_input \
	testsuites/stop_when_array_has_elem_input \
	testsuites/xlate.lkp_tbl \
	testsuites/xlate_more.lkp_tbl \
	testsuites/xlate_array.lkp_tbl \
	testsuites/xlate_array_more.lkp_tbl \
	testsuites/xlate_array_misuse.lkp_tbl \
	testsuites/xlate_array_more_misuse.lkp_tbl \
	testsuites/xlate_sparse_array.lkp_tbl \
	testsuites/lookup_sparse_array_ipv4.lkp_tbl \
	testsuites/xlate_sparse_array_more.lkp_tbl \
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
	known_issues.supp \
	libmaxmindb.supp \
	travis/trusty.supp \
	linux_localtime_r.supp \
	CI/centos6-9.supp \
	CI/centos7.supp \
	CI/gcov.supp \
	CI/ubuntu20.04.supp \
	omtcl.tcl \
	testsuites/pgsql-basic.sql \
	testsuites/pgsql-select-msg.sql \
	testsuites/pgsql-select-syslogtag.sql \
	mmkubernetes_test_server.py \
	mmkubernetes-basic.out.json \
	mmkubernetes-cache-expire.out.expected \
	mmkubernetes.supp \
	testsuites/incltest.d/include.conf \
	testsuites/abort-uncleancfg-goodcfg.conf \
	testsuites/include-std-omfile-action.conf \
	testsuites/invalid.conf \
	testsuites/valid.conf \
	testsuites/variable_leading_underscore.conf \
	testsuites/action-tx-errfile.result \
	improg-multiline-test.py \
	testsuites/imhttp-large-data.txt \
	testsuites/docroot/file.txt \
	testsuites/htpasswd \
	omhttp_server.py \
	omhttp-validate-response.py \
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
	testsuites/imfile-old-state-file_imfile-state_.-rsyslog.input \
	imfile-endmsg.regex.crio.rulebase \
	imfile-endmsg.regex.json.rulebase \
	testsuites/msgvar-concurrency-array.rulebase \
	testsuites/msgvar-concurrency-array-event.tags.rulebase \
	testsuites/mmnormalize_processing_tests.rulebase \
	testsuites/pmnormalize_basic.rulebase \
	validate_json_equality.py \
	testsuites/qradar_json \
	snmptrapreceiver.py \
	omamqp1-common.sh

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
if ENABLE_OSSL
if ENABLE_WOLFSSL
tcpflood_CFLAGS += $(WOLFSSL_CFLAGS)
tcpflood_CPPFLAGS += $(WOLFSSL_CFLAGS)
tcpflood_LDADD += $(WOLFSSL_LIBS)
else
tcpflood_CFLAGS += $(OPENSSL_CFLAGS)
tcpflood_CPPFLAGS += $(OPENSSL_CFLAGS)
tcpflood_LDADD += $(OPENSSL_LIBS)
endif
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
