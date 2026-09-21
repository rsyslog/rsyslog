EXTRA_PROGRAMS = \
	fuzz_rsyslog_parsers \
	fuzz_rsyslog_parsers_libfuzzer \
	fuzz_driver_regression \
	fuzz_state_regression

fuzz_runtime_files = \
	fuzzapi.c \
	fuzzapi.h \
	fuzzdriver.c \
	fuzzdriver.h \
	../tools/omdiscard.c \
	../tools/omfile.c \
	../tools/omfwd.c \
	../tools/iminternal.c \
	../tools/ompipe.c \
	../tools/omshell.c \
	../tools/omusrmsg.c \
	../tools/pmrfc3164.c \
	../tools/pmrfc5424.c \
	../tools/smfwd.c \
	../tools/smfile.c \
	../tools/smtradfile.c \
	../tools/smtradfwd.c

fuzz_rsyslog_parsers_SOURCES = \
	fuzz.c \
	$(fuzz_runtime_files)

fuzz_rsyslog_parsers_CPPFLAGS = -DSD_EXPORT_SYMBOLS -I$(top_srcdir)/tools $(PTHREADS_CFLAGS) $(RSRT_CFLAGS) \
	-U_PATH_MODDIR -D_PATH_MODDIR=\"$(abs_top_builddir)/runtime/.libs/\"
if ENABLE_RELP
fuzz_rsyslog_parsers_SOURCES += ../plugins/omrelp/omrelp.c
fuzz_rsyslog_parsers_CPPFLAGS += -DFUZZ_HAVE_OMRELP -I$(top_srcdir)/plugins/omrelp $(RELP_CFLAGS)
endif
FUZZ_CFLAGS =
FUZZ_LDFLAGS =
FUZZ_SANITIZERS =
FUZZ_NO_SANITIZE_FLAGS =
FUZZ_COVERAGE = 1
FUZZ_COVERAGE_FLAGS = $(if $(filter 1 yes true,$(FUZZ_COVERAGE)),--coverage,)
FUZZ_SANITIZER_FLAGS = $(if $(FUZZ_SANITIZERS),-fsanitize=$(FUZZ_SANITIZERS) -fno-omit-frame-pointer,)
FUZZ_COMMON_CFLAGS = $(FUZZ_CFLAGS) $(FUZZ_SANITIZER_FLAGS) $(FUZZ_NO_SANITIZE_FLAGS) $(FUZZ_COVERAGE_FLAGS)
FUZZ_COMMON_LDFLAGS = $(FUZZ_LDFLAGS) $(FUZZ_SANITIZER_FLAGS) $(FUZZ_NO_SANITIZE_FLAGS) $(FUZZ_COVERAGE_FLAGS)
FUZZ_LIBFUZZER_LIBS =
fuzz_rsyslog_parsers_CFLAGS = $(AM_CFLAGS) $(FUZZ_COMMON_CFLAGS)
fuzz_rsyslog_parsers_LDFLAGS = $(FUZZ_COMMON_LDFLAGS) -export-dynamic
fuzz_rsyslog_parsers_LDADD = \
	$(top_builddir)/grammar/libgrammar.la \
	$(top_builddir)/runtime/librsyslog.la \
	$(top_builddir)/compat/compat.la \
	$(ZLIB_LIBS) \
	$(PTHREADS_LIBS) \
	$(RSRT_LIBS) \
	$(SOL_LIBS) \
	$(LIBUUID_LIBS) \
	$(HASH_XXHASH_LIBS)
if ENABLE_RELP
fuzz_rsyslog_parsers_LDADD += $(RELP_LIBS)
endif

fuzz_rsyslog_parsers_DEPENDENCIES = \
	$(top_builddir)/grammar/libgrammar.la \
	$(top_builddir)/runtime/librsyslog.la \
	$(top_builddir)/runtime/lmnet.la \
	$(top_builddir)/runtime/lmnetstrms.la \
	$(top_builddir)/runtime/lmtcpsrv.la \
	$(top_builddir)/compat/compat.la

fuzz_rsyslog_parsers_libfuzzer_SOURCES = \
	$(fuzz_rsyslog_parsers_SOURCES) \
	libfuzzer.c
fuzz_rsyslog_parsers_libfuzzer_CPPFLAGS = $(fuzz_rsyslog_parsers_CPPFLAGS) -DRSYSLOG_FUZZ_NO_MAIN
fuzz_rsyslog_parsers_libfuzzer_CFLAGS = $(fuzz_rsyslog_parsers_CFLAGS)
fuzz_rsyslog_parsers_libfuzzer_LDFLAGS = $(FUZZ_COMMON_LDFLAGS) -fsanitize=fuzzer -export-dynamic
fuzz_rsyslog_parsers_libfuzzer_LDADD = $(fuzz_rsyslog_parsers_LDADD) $(FUZZ_LIBFUZZER_LIBS)
fuzz_rsyslog_parsers_libfuzzer_DEPENDENCIES = $(fuzz_rsyslog_parsers_DEPENDENCIES)

fuzz_driver_regression_SOURCES = \
	fuzz.c \
	fuzzapi.h \
	fuzzdriver.c \
	fuzzdriver.h \
	fuzzdriver_regression.c \
	libfuzzer.c
fuzz_driver_regression_CPPFLAGS = $(fuzz_rsyslog_parsers_CPPFLAGS) -DRSYSLOG_FUZZ_NO_MAIN
fuzz_driver_regression_CFLAGS = $(fuzz_rsyslog_parsers_CFLAGS)
fuzz_driver_regression_LDFLAGS = $(FUZZ_COMMON_LDFLAGS)

fuzz_state_regression_SOURCES = \
	fuzz.c \
	fuzzstate_regression.c \
	$(fuzz_runtime_files)
fuzz_state_regression_CPPFLAGS = $(fuzz_rsyslog_parsers_CPPFLAGS) -DRSYSLOG_FUZZ_NO_MAIN
fuzz_state_regression_CFLAGS = $(fuzz_rsyslog_parsers_CFLAGS)
fuzz_state_regression_LDFLAGS = $(fuzz_rsyslog_parsers_LDFLAGS)
fuzz_state_regression_LDADD = $(fuzz_rsyslog_parsers_LDADD)
fuzz_state_regression_DEPENDENCIES = $(fuzz_rsyslog_parsers_DEPENDENCIES)

$(top_builddir)/grammar/libgrammar.la:
	$(AM_V_at)$(MAKE) -C $(top_builddir)/grammar libgrammar.la

$(top_builddir)/runtime/librsyslog.la:
	$(AM_V_at)$(MAKE) -C $(top_builddir)/runtime librsyslog.la

$(top_builddir)/runtime/lmnet.la:
	$(AM_V_at)$(MAKE) -C $(top_builddir)/runtime lmnet.la

$(top_builddir)/runtime/lmnetstrms.la:
	$(AM_V_at)$(MAKE) -C $(top_builddir)/runtime lmnetstrms.la

$(top_builddir)/runtime/lmtcpsrv.la:
	$(AM_V_at)$(MAKE) -C $(top_builddir)/runtime lmtcpsrv.la

$(top_builddir)/compat/compat.la:
	$(AM_V_at)$(MAKE) -C $(top_builddir)/compat compat.la

.PHONY: check-fuzz-regressions validate-fuzz-dicts
check-fuzz-regressions: fuzz_driver_regression fuzz_state_regression
	$(AM_V_at)$(SHELL) $(srcdir)/run_regressions.sh \
		$(abs_builddir)/fuzz_driver_regression \
		$(abs_builddir)/fuzz_state_regression

validate-fuzz-dicts:
	$(AM_V_at)$(SHELL) $(srcdir)/validate_dicts.sh \
		$(srcdir)/rsyslog.dict \
		$(srcdir)/dicts/json.dict \
		$(srcdir)/dicts/parser.dict \
		$(srcdir)/dicts/queue.dict \
		$(srcdir)/dicts/tcp.dict \
		$(srcdir)/dicts/template.dict \
		$(srcdir)/dicts/imfile.dict \
		$(srcdir)/dicts/action.dict

CLEANFILES = \
	fuzz_rsyslog_parsers*.gcda \
	fuzz_rsyslog_parsers*.gcno \
	fuzz_rsyslog_parsers_libfuzzer*.gcda \
	fuzz_rsyslog_parsers_libfuzzer*.gcno

EXTRA_DIST = \
	fuzz.c \
	fuzzapi.c \
	fuzzapi.h \
	fuzzdriver.c \
	fuzzdriver.h \
	fuzzdriver_regression.c \
	fuzzstate_regression.c \
	libfuzzer.c \
	Dockerfile \
	.gitattributes \
	.gitignore \
	README.md \
	migration-report.md \
	build_libfastjson_sanitized.sh \
	ci_build_and_fuzz.sh \
	ci_casr_triage.sh \
	ci_fuzz_common.sh \
	minimize_corpus.sh \
	replay_crashes.sh \
	run_regressions.sh \
	run_fuzz_smoke.sh \
	rsyslog.dict \
	validate_dicts.sh \
	dicts/action.dict \
	dicts/imfile.dict \
	dicts/json.dict \
	dicts/parser.dict \
	dicts/queue.dict \
	dicts/tcp.dict \
	dicts/template.dict \
	fuzz_rsyslog_parsers_diagram.md \
	testcases/test_inputs/README.md \
	testcases/test_inputs/binary_data.txt \
	testcases/test_inputs/binary_data_bytes.txt \
	testcases/test_inputs/buffer_overflow_attempt.txt \
	testcases/test_inputs/cee_json_event.txt \
	testcases/test_inputs/cfg_legacy_cmd.txt \
	testcases/test_inputs/control_chars_actual.txt \
	testcases/test_inputs/empty_message.txt \
	testcases/test_inputs/empty_test.txt \
	testcases/test_inputs/ipv6_host.txt \
	testcases/test_inputs/json_array_mixed.json \
	testcases/test_inputs/json_big_props.json \
	testcases/test_inputs/json_invalid_frag.json \
	testcases/test_inputs/json_valid_nested.json \
	testcases/test_inputs/legacy_queue_filename.txt \
	testcases/test_inputs/malformed_priority.txt \
	testcases/test_inputs/malformed_structured_data.txt \
	testcases/test_inputs/malformed_timestamp.txt \
	testcases/test_inputs/multiple_messages.txt \
	testcases/test_inputs/no_priority.txt \
	testcases/test_inputs/null_bytes.txt \
	testcases/test_inputs/octet_counted_rfc5424.txt \
	testcases/test_inputs/rfc3164_large_pri.txt \
	testcases/test_inputs/rfc3164_minimal.txt \
	testcases/test_inputs/rfc3164_standard.txt \
	testcases/test_inputs/rfc3164_with_sd.txt \
	testcases/test_inputs/rfc5424_deep_sd.txt \
	testcases/test_inputs/rfc5424_no_structured.txt \
	testcases/test_inputs/rfc5424_sd_complex.txt \
	testcases/test_inputs/rfc5424_structured.txt \
	testcases/test_inputs/short_timestamp_2.txt \
	testcases/test_inputs/short_timestamp_3.txt \
	testcases/test_inputs/short_timestamp_4.txt \
	testcases/test_inputs/simple_test.txt \
	testcases/test_inputs/special_chars.txt \
	testcases/test_inputs/structured_data_escape.txt \
	testcases/test_inputs/template_dynamic_mix.txt \
	testcases/test_inputs/template_like.txt \
	testcases/test_inputs/test_input \
	testcases/test_inputs/test_input_simple.txt \
	testcases/test_inputs/test_simple2.txt \
	testcases/test_inputs/test_syslog.txt \
	testcases/test_inputs/timestamp_broken.txt \
	testcases/test_inputs/timestamp_rfc3339.txt \
	testcases/test_inputs/timestamp_tz_weird.txt \
	testcases/test_inputs/unix_timestamp.txt \
	testcases/test_inputs/very_long_message.txt
