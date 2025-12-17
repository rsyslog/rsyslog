sbin_PROGRAMS =
pkglib_LTLIBRARIES =

pkgconfigdir = $(libdir)/pkgconfig

EXTRA_DIST = \
	README.md \
	platform/README \
	platform/freebsd/rsyslogd \
	platform/slackware/rc.rsyslogd \
	platform/redhat/rsyslog.conf \
	contrib/README \
	CONTRIBUTING.md \
	COPYING \
	COPYING.LESSER \
	COPYING.ASL20 \
	contrib/gnutls/ca.pem \
	contrib/gnutls/cert.pem \
	contrib/gnutls/key.pem

# doc files - actual enumration, "pickup script" did not work well.
EXTRA_DIST += \
    doc/.gitignore \
    doc/.travis.yml \
    doc/AGENTS.md \
    doc/BUILDS_README.md \
    doc/CONTRIBUTING.md \
    doc/IMPLEMENTATION_PLAN.md \
    doc/LICENSE \
    doc/Makefile \
    doc/README.md \
    doc/STRATEGY.md \
    doc/ai/README.md \
    doc/ai/authoring_guidelines.md \
    doc/ai/chunking_and_embeddings.md \
    doc/ai/crosslinking_and_nav.md \
    doc/ai/drift_monitoring.md \
    doc/ai/mermaid_rules.md \
    doc/ai/module_map.yaml \
    doc/ai/structure_and_paths.md \
    doc/ai/templates/template-concept.rst \
    doc/ai/templates/template-tutorial.rst \
    doc/ai/terminology.md \
    doc/contrib/README.md \
    doc/contrib/cron_build_all_branches.sh \
    doc/proposed-toc \
    doc/requirements.txt \
    doc/source/_ext/edit_on_github.py \
    doc/source/_ext/rsyslog_lexer.py \
    doc/source/_static/rsyslog.css \
    doc/source/_static/vendor/README.md \
    doc/source/_static/vendor/d3/d3.min.js \
    doc/source/_static/vendor/mermaid/mermaid-layout-elk.esm.min.mjs \
    doc/source/_static/vendor/mermaid/mermaid-layout-elk.min.js \
    doc/source/_static/vendor/mermaid/mermaid.esm.min.mjs \
    doc/source/_static/vendor/mermaid/mermaid.min.js \
    doc/source/_templates/sourcelink.html \
    doc/source/_templates_minimal/README \
    doc/source/_templates_minimal/footer.html \
    doc/source/_templates_minimal/header.html \
    doc/source/_templates_minimal/relbar.html \
    doc/source/about/ai_first.rst \
    doc/source/about/history.rst \
    doc/source/about/index.rst \
    doc/source/about/origins.rst \
    doc/source/about/release_versioning.rst \
    doc/source/community.rst \
    doc/source/concepts/index.rst \
    doc/source/concepts/janitor.rst \
    doc/source/concepts/log_pipeline/design_patterns.rst \
    doc/source/concepts/log_pipeline/example_json_transform.rst \
    doc/source/concepts/log_pipeline/index.rst \
    doc/source/concepts/log_pipeline/stages.rst \
    doc/source/concepts/log_pipeline/troubleshooting.rst \
    doc/source/concepts/messageparser.rst \
    doc/source/concepts/multi_ruleset.rst \
    doc/source/concepts/netstrm_drvr.rst \
    doc/source/concepts/ns_gtls.rst \
    doc/source/concepts/ns_mbedtls.rst \
    doc/source/concepts/ns_ossl.rst \
    doc/source/concepts/ns_ptcp.rst \
    doc/source/concepts/queues.rst \
    doc/source/concepts/rfc5424layers.png \
    doc/source/conf.py \
    doc/source/conf_helpers.py \
    doc/source/configuration/action/index.rst \
    doc/source/configuration/action/rsconf1_actionexeconlywhenpreviousissuspended.rst \
    doc/source/configuration/action/rsconf1_actionresumeinterval.rst \
    doc/source/configuration/action/rsconf1_dirgroup.rst \
    doc/source/configuration/action/rsconf1_dirowner.rst \
    doc/source/configuration/action/rsconf1_dynafilecachesize.rst \
    doc/source/configuration/action/rsconf1_filecreatemode.rst \
    doc/source/configuration/action/rsconf1_filegroup.rst \
    doc/source/configuration/action/rsconf1_fileowner.rst \
    doc/source/configuration/action/rsconf1_gssforwardservicename.rst \
    doc/source/configuration/action/rsconf1_gssmode.rst \
    doc/source/configuration/action/rsconf1_omfileforcechown.rst \
    doc/source/configuration/action/rsconf1_repeatedmsgreduction.rst \
    doc/source/configuration/actions.rst \
    doc/source/configuration/basic_structure.rst \
    doc/source/configuration/conf_formats.rst \
    doc/source/configuration/config_param_types.rst \
    doc/source/configuration/converting_to_new_format.rst \
    doc/source/configuration/cryprov_gcry.rst \
    doc/source/configuration/cryprov_ossl.rst \
    doc/source/configuration/droppriv.rst \
    doc/source/configuration/dyn_stats.rst \
    doc/source/configuration/examples.rst \
    doc/source/configuration/filters.rst \
    doc/source/configuration/global/index.rst \
    doc/source/configuration/global/options/rsconf1_abortonuncleanconfig.rst \
    doc/source/configuration/global/options/rsconf1_debugprintcfsyslinehandlerlist.rst \
    doc/source/configuration/global/options/rsconf1_debugprintmodulelist.rst \
    doc/source/configuration/global/options/rsconf1_debugprinttemplatelist.rst \
    doc/source/configuration/global/options/rsconf1_failonchownfailure.rst \
    doc/source/configuration/global/options/rsconf1_generateconfiggraph.rst \
    doc/source/configuration/global/options/rsconf1_includeconfig.rst \
    doc/source/configuration/global/options/rsconf1_mainmsgqueuesize.rst \
    doc/source/configuration/global/options/rsconf1_maxopenfiles.rst \
    doc/source/configuration/global/options/rsconf1_moddir.rst \
    doc/source/configuration/global/options/rsconf1_modload.rst \
    doc/source/configuration/global/options/rsconf1_resetconfigvariables.rst \
    doc/source/configuration/global/options/rsconf1_umask.rst \
    doc/source/configuration/global/options/rsyslog_confgraph_complex.png \
    doc/source/configuration/global/options/rsyslog_confgraph_std.png \
    doc/source/configuration/index.rst \
    doc/source/configuration/index_directives.rst \
    doc/source/configuration/input.rst \
    doc/source/configuration/input_directives/index.rst \
    doc/source/configuration/input_directives/rsconf1_allowedsender.rst \
    doc/source/configuration/input_directives/rsconf1_controlcharacterescapeprefix.rst \
    doc/source/configuration/input_directives/rsconf1_dropmsgswithmaliciousdnsptrrecords.rst \
    doc/source/configuration/input_directives/rsconf1_droptrailinglfonreception.rst \
    doc/source/configuration/input_directives/rsconf1_escape8bitcharsonreceive.rst \
    doc/source/configuration/input_directives/rsconf1_escapecontrolcharactersonreceive.rst \
    doc/source/configuration/input_directives/rsconf1_markmessageperiod.rst \
    doc/source/configuration/ipv6.rst \
    doc/source/configuration/lookup_tables.rst \
    doc/source/configuration/modules/gssapi.png \
    doc/source/configuration/modules/idx_input.rst \
    doc/source/configuration/modules/idx_library.rst \
    doc/source/configuration/modules/idx_messagemod.rst \
    doc/source/configuration/modules/idx_output.rst \
    doc/source/configuration/modules/idx_parser.rst \
    doc/source/configuration/modules/idx_stringgen.rst \
    doc/source/configuration/modules/im3195.rst \
    doc/source/configuration/modules/imbatchreport.rst \
    doc/source/configuration/modules/imczmq.rst \
    doc/source/configuration/modules/imdiag.rst \
    doc/source/configuration/modules/imdocker.rst \
    doc/source/configuration/modules/imdtls.rst \
    doc/source/configuration/modules/imfile.rst \
    doc/source/configuration/modules/imgssapi.rst \
    doc/source/configuration/modules/imhiredis.rst \
    doc/source/configuration/modules/imhttp.rst \
    doc/source/configuration/modules/imjournal.rst \
    doc/source/configuration/modules/imkafka.rst \
    doc/source/configuration/modules/imklog.rst \
    doc/source/configuration/modules/imkmsg.rst \
    doc/source/configuration/modules/immark.rst \
    doc/source/configuration/modules/impcap.rst \
    doc/source/configuration/modules/improg.rst \
    doc/source/configuration/modules/impstats.rst \
    doc/source/configuration/modules/imptcp.rst \
    doc/source/configuration/modules/imrelp.rst \
    doc/source/configuration/modules/imsolaris.rst \
    doc/source/configuration/modules/imtcp.rst \
    doc/source/configuration/modules/imtuxedoulog.rst \
    doc/source/configuration/modules/imudp.rst \
    doc/source/configuration/modules/imuxsock.rst \
    doc/source/configuration/modules/index.rst \
    doc/source/configuration/modules/mmaitag.rst \
    doc/source/configuration/modules/mmanon.rst \
    doc/source/configuration/modules/mmaudit.rst \
    doc/source/configuration/modules/mmcount.rst \
    doc/source/configuration/modules/mmdarwin.rst \
    doc/source/configuration/modules/mmdblookup.rst \
    doc/source/configuration/modules/mmexternal.rst \
    doc/source/configuration/modules/mmfields.rst \
    doc/source/configuration/modules/mmjsonparse.rst \
    doc/source/configuration/modules/mmjsonrewrite.rst \
    doc/source/configuration/modules/mmjsontransform.rst \
    doc/source/configuration/modules/mmkubernetes.rst \
    doc/source/configuration/modules/mmleefparse.rst \
    doc/source/configuration/modules/mmnormalize.rst \
    doc/source/configuration/modules/mmpstrucdata.rst \
    doc/source/configuration/modules/mmrfc5424addhmac.rst \
    doc/source/configuration/modules/mmrm1stspace.rst \
    doc/source/configuration/modules/mmsequence.rst \
    doc/source/configuration/modules/mmsnareparse.rst \
    doc/source/configuration/modules/mmsnmptrapd.rst \
    doc/source/configuration/modules/mmtaghostname.rst \
    doc/source/configuration/modules/mmutf8fix.rst \
    doc/source/configuration/modules/module_workflow.png \
    doc/source/configuration/modules/omamqp1.rst \
    doc/source/configuration/modules/omazureeventhubs.rst \
    doc/source/configuration/modules/omclickhouse.rst \
    doc/source/configuration/modules/omczmq.rst \
    doc/source/configuration/modules/omdtls.rst \
    doc/source/configuration/modules/omelasticsearch.rst \
    doc/source/configuration/modules/omfile.rst \
    doc/source/configuration/modules/omfwd.rst \
    doc/source/configuration/modules/omfwd/parameters/basic.rst \
    doc/source/configuration/modules/omfwd/parameters/module.rst \
    doc/source/configuration/modules/omgssapi.rst \
    doc/source/configuration/modules/omhdfs.rst \
    doc/source/configuration/modules/omhiredis.rst \
    doc/source/configuration/modules/omhttp.rst \
    doc/source/configuration/modules/omhttpfs.rst \
    doc/source/configuration/modules/omjournal.rst \
    doc/source/configuration/modules/omkafka.rst \
    doc/source/configuration/modules/omlibdbi.rst \
    doc/source/configuration/modules/ommail.rst \
    doc/source/configuration/modules/ommongodb.rst \
    doc/source/configuration/modules/ommysql.rst \
    doc/source/configuration/modules/omoracle.rst \
    doc/source/configuration/modules/ompgsql.rst \
    doc/source/configuration/modules/ompipe.rst \
    doc/source/configuration/modules/omprog.rst \
    doc/source/configuration/modules/omrabbitmq.rst \
    doc/source/configuration/modules/omrelp.rst \
    doc/source/configuration/modules/omruleset.rst \
    doc/source/configuration/modules/omsendertrack.rst \
    doc/source/configuration/modules/omsnmp.rst \
    doc/source/configuration/modules/omstdout.rst \
    doc/source/configuration/modules/omudpspoof.rst \
    doc/source/configuration/modules/omusrmsg.rst \
    doc/source/configuration/modules/omuxsock.rst \
    doc/source/configuration/modules/pmciscoios.rst \
    doc/source/configuration/modules/pmdb2diag.rst \
    doc/source/configuration/modules/pmlastmsg.rst \
    doc/source/configuration/modules/pmnormalize.rst \
    doc/source/configuration/modules/pmnull.rst \
    doc/source/configuration/modules/pmrfc3164.rst \
    doc/source/configuration/modules/pmrfc3164sd.rst \
    doc/source/configuration/modules/pmrfc5424.rst \
    doc/source/configuration/modules/sigprov_gt.rst \
    doc/source/configuration/modules/sigprov_ksi.rst \
    doc/source/configuration/modules/sigprov_ksi12.rst \
    doc/source/configuration/modules/workflow.rst \
    doc/source/configuration/nomatch.rst \
    doc/source/configuration/output_channels.rst \
    doc/source/configuration/parser.rst \
    doc/source/configuration/percentile_stats.rst \
    doc/source/configuration/properties.rst \
    doc/source/configuration/property_replacer.rst \
    doc/source/configuration/rsyslog-example.conf \
    doc/source/configuration/rsyslog_statistic_counter.rst \
    doc/source/configuration/ruleset/index.rst \
    doc/source/configuration/ruleset/rsconf1_rulesetcreatemainqueue.rst \
    doc/source/configuration/ruleset/rsconf1_rulesetparser.rst \
    doc/source/configuration/sysklogd_format.rst \
    doc/source/configuration/templates.rst \
    doc/source/configuration/timezone.rst \
    doc/source/containers/collector.rst \
    doc/source/containers/development_images.rst \
    doc/source/containers/dockerlogs.rst \
    doc/source/containers/index.rst \
    doc/source/containers/minimal.rst \
    doc/source/containers/standard.rst \
    doc/source/containers/user_images.rst \
    doc/source/dataflow.png \
    doc/source/development/coding_practices.rst \
    doc/source/development/coding_practices/embedded_ipv4_tail_helper.rst \
    doc/source/development/coding_practices/error_logging_with_errno.rst \
    doc/source/development/coding_practices/externalized_helper_contracts.rst \
    doc/source/development/config_data_model.rst \
    doc/source/development/debugging.rst \
    doc/source/development/dev_action_threads.rst \
    doc/source/development/dev_codestyle.rst \
    doc/source/development/dev_oplugins.rst \
    doc/source/development/dev_queue.rst \
    doc/source/development/dev_testbench.rst \
    doc/source/development/doc_reference_section_guidelines.rst \
    doc/source/development/doc_style_guide.rst \
    doc/source/development/engine_overview.rst \
    doc/source/development/generic_design.rst \
    doc/source/development/index.rst \
    doc/source/development/internal_tooling.rst \
    doc/source/development/queueWorkerLogic.jpg \
    doc/source/direct_queue0.png \
    doc/source/direct_queue1.png \
    doc/source/direct_queue2.png \
    doc/source/direct_queue3.png \
    doc/source/direct_queue_directq.png \
    doc/source/direct_queue_rsyslog.png \
    doc/source/direct_queue_rsyslog2.png \
    doc/source/faq/common-config-mistakes.rst \
    doc/source/faq/difference_queues.rst \
    doc/source/faq/does-rsyslog-run-under-windows.rst \
    doc/source/faq/encrypt_mysql_traffic_ommysql.rst \
    doc/source/faq/etl_tool.rst \
    doc/source/faq/faq_overall.rst \
    doc/source/faq/imtcp-tls-gibberish.rst \
    doc/source/faq/imudp-packet-loss.rst \
    doc/source/faq/index.rst \
    doc/source/faq/vespa-ai-integration.rst \
    doc/source/features.rst \
    doc/source/getting_started/ai-assistants.rst \
    doc/source/getting_started/basic_configuration.rst \
    doc/source/getting_started/beginner_tutorials/01-installation.rst \
    doc/source/getting_started/beginner_tutorials/02-first-config.rst \
    doc/source/getting_started/beginner_tutorials/03-default-config.rst \
    doc/source/getting_started/beginner_tutorials/04-message-pipeline.rst \
    doc/source/getting_started/beginner_tutorials/05-order-matters.rst \
    doc/source/getting_started/beginner_tutorials/06-remote-server.rst \
    doc/source/getting_started/beginner_tutorials/index.rst \
    doc/source/getting_started/forwarding_logs.rst \
    doc/source/getting_started/index.rst \
    doc/source/getting_started/installation.rst \
    doc/source/getting_started/next_steps.rst \
    doc/source/getting_started/understanding_default_config.rst \
    doc/source/gssapi.png \
    doc/source/historical/high_performance.rst \
    doc/source/historical/index.rst \
    doc/source/historical/module_devel.rst \
    doc/source/historical/multi_ruleset_legacy_format_samples.rst \
    doc/source/historical/php_syslog_ng.rst \
    doc/source/historical/v3compatibility.rst \
    doc/source/historical/v4compatibility.rst \
    doc/source/historical/v5compatibility.rst \
    doc/source/historical/v6compatibility.rst \
    doc/source/historical/v7compatibility.rst \
    doc/source/historical/v8compatibility.rst \
    doc/source/history.rst \
    doc/source/how2help.rst \
    doc/source/idx_reference.rst \
    doc/source/includes/container_dev_env.inc.rst \
    doc/source/includes/footer.inc.rst \
    doc/source/includes/substitution_definitions.inc.rst \
    doc/source/index.rst \
    doc/source/installation/build_from_repo.rst \
    doc/source/installation/index.rst \
    doc/source/installation/install_from_source.rst \
    doc/source/installation/packages.rst \
    doc/source/installation/rsyslog_docker.rst \
    doc/source/licensing.rst \
    doc/source/module_workflow.png \
    doc/source/proposals/index.rst \
    doc/source/queue_analogy_tv.png \
    doc/source/rainerscript/configuration_objects.rst \
    doc/source/rainerscript/constant_strings.rst \
    doc/source/rainerscript/control_structures.rst \
    doc/source/rainerscript/data_types.rst \
    doc/source/rainerscript/expressions.rst \
    doc/source/rainerscript/functions/idx_built-in_functions.rst \
    doc/source/rainerscript/functions/idx_module_functions.rst \
    doc/source/rainerscript/functions/index.rst \
    doc/source/rainerscript/functions/mo-ffaup.rst \
    doc/source/rainerscript/functions/mo-hashXX.rst \
    doc/source/rainerscript/functions/mo-hashXXmod.rst \
    doc/source/rainerscript/functions/mo-http_request.rst \
    doc/source/rainerscript/functions/mo-unflatten.rst \
    doc/source/rainerscript/functions/rs-cnum.rst \
    doc/source/rainerscript/functions/rs-cstr.rst \
    doc/source/rainerscript/functions/rs-dyn_inc.rst \
    doc/source/rainerscript/functions/rs-exec_template.rst \
    doc/source/rainerscript/functions/rs-exists.rst \
    doc/source/rainerscript/functions/rs-field.rst \
    doc/source/rainerscript/functions/rs-format_time.rst \
    doc/source/rainerscript/functions/rs-get_property.rst \
    doc/source/rainerscript/functions/rs-getenv.rst \
    doc/source/rainerscript/functions/rs-int2hex.rst \
    doc/source/rainerscript/functions/rs-ipv4convert.rst \
    doc/source/rainerscript/functions/rs-is_time.rst \
    doc/source/rainerscript/functions/rs-lookup.rst \
    doc/source/rainerscript/functions/rs-parse_json.rst \
    doc/source/rainerscript/functions/rs-parse_time.rst \
    doc/source/rainerscript/functions/rs-percentile_observe.rst \
    doc/source/rainerscript/functions/rs-previous_action_suspended.rst \
    doc/source/rainerscript/functions/rs-prifilt.rst \
    doc/source/rainerscript/functions/rs-random.rst \
    doc/source/rainerscript/functions/rs-re_extract.rst \
    doc/source/rainerscript/functions/rs-re_extract_i.rst \
    doc/source/rainerscript/functions/rs-re_match.rst \
    doc/source/rainerscript/functions/rs-re_match_i.rst \
    doc/source/rainerscript/functions/rs-replace.rst \
    doc/source/rainerscript/functions/rs-script_error.rst \
    doc/source/rainerscript/functions/rs-strlen.rst \
    doc/source/rainerscript/functions/rs-substring.rst \
    doc/source/rainerscript/functions/rs-tolower.rst \
    doc/source/rainerscript/functions/rs-toupper.rst \
    doc/source/rainerscript/functions/rs-trim.rst \
    doc/source/rainerscript/functions/rs-wrap.rst \
    doc/source/rainerscript/global.rst \
    doc/source/rainerscript/include.rst \
    doc/source/rainerscript/index.rst \
    doc/source/rainerscript/lookup_tables.rst \
    doc/source/rainerscript/queue_parameters.rst \
    doc/source/rainerscript/rainerscript_call.rst \
    doc/source/rainerscript/rainerscript_call_indirect.rst \
    doc/source/rainerscript/variable_property_types.rst \
    doc/source/reference/parameters/im3195-input3195listenport.rst \
    doc/source/reference/parameters/imdiag-aborttimeout.rst \
    doc/source/reference/parameters/imdiag-injectdelaymode.rst \
    doc/source/reference/parameters/imdiag-listenportfilename.rst \
    doc/source/reference/parameters/imdiag-maxsessions.rst \
    doc/source/reference/parameters/imdiag-serverinputname.rst \
    doc/source/reference/parameters/imdiag-serverrun.rst \
    doc/source/reference/parameters/imdiag-serverstreamdriverauthmode.rst \
    doc/source/reference/parameters/imdiag-serverstreamdrivermode.rst \
    doc/source/reference/parameters/imdiag-serverstreamdriverpermittedpeer.rst \
    doc/source/reference/parameters/imdocker-apiversionstr.rst \
    doc/source/reference/parameters/imdocker-defaultfacility.rst \
    doc/source/reference/parameters/imdocker-defaultseverity.rst \
    doc/source/reference/parameters/imdocker-dockerapiunixsockaddr.rst \
    doc/source/reference/parameters/imdocker-escapelf.rst \
    doc/source/reference/parameters/imdocker-getcontainerlogoptions.rst \
    doc/source/reference/parameters/imdocker-listcontainersoptions.rst \
    doc/source/reference/parameters/imdocker-pollinginterval.rst \
    doc/source/reference/parameters/imdocker-retrievenewlogsfromstart.rst \
    doc/source/reference/parameters/imdtls-address.rst \
    doc/source/reference/parameters/imdtls-name.rst \
    doc/source/reference/parameters/imdtls-port.rst \
    doc/source/reference/parameters/imdtls-ruleset.rst \
    doc/source/reference/parameters/imdtls-timeout.rst \
    doc/source/reference/parameters/imdtls-tls-authmode.rst \
    doc/source/reference/parameters/imdtls-tls-cacert.rst \
    doc/source/reference/parameters/imdtls-tls-mycert.rst \
    doc/source/reference/parameters/imdtls-tls-myprivkey.rst \
    doc/source/reference/parameters/imdtls-tls-permittedpeer.rst \
    doc/source/reference/parameters/imdtls-tls-tlscfgcmd.rst \
    doc/source/reference/parameters/imfile-addceetag.rst \
    doc/source/reference/parameters/imfile-addmetadata.rst \
    doc/source/reference/parameters/imfile-deletestateonfiledelete.rst \
    doc/source/reference/parameters/imfile-deletestateonfilemove.rst \
    doc/source/reference/parameters/imfile-discardtruncatedmsg.rst \
    doc/source/reference/parameters/imfile-endmsg-regex.rst \
    doc/source/reference/parameters/imfile-escapelf-replacement.rst \
    doc/source/reference/parameters/imfile-escapelf.rst \
    doc/source/reference/parameters/imfile-facility.rst \
    doc/source/reference/parameters/imfile-file.rst \
    doc/source/reference/parameters/imfile-freshstarttail.rst \
    doc/source/reference/parameters/imfile-ignoreolderthan.rst \
    doc/source/reference/parameters/imfile-maxbytesperminute.rst \
    doc/source/reference/parameters/imfile-maxlinesatonce.rst \
    doc/source/reference/parameters/imfile-maxlinesperminute.rst \
    doc/source/reference/parameters/imfile-maxsubmitatonce.rst \
    doc/source/reference/parameters/imfile-mode.rst \
    doc/source/reference/parameters/imfile-msgdiscardingerror.rst \
    doc/source/reference/parameters/imfile-needparse.rst \
    doc/source/reference/parameters/imfile-persiststateaftersubmission.rst \
    doc/source/reference/parameters/imfile-persiststateinterval.rst \
    doc/source/reference/parameters/imfile-pollinginterval.rst \
    doc/source/reference/parameters/imfile-readmode.rst \
    doc/source/reference/parameters/imfile-readtimeout.rst \
    doc/source/reference/parameters/imfile-reopenontruncate.rst \
    doc/source/reference/parameters/imfile-ruleset.rst \
    doc/source/reference/parameters/imfile-severity.rst \
    doc/source/reference/parameters/imfile-sortfiles.rst \
    doc/source/reference/parameters/imfile-startmsg-regex.rst \
    doc/source/reference/parameters/imfile-statefile-directory.rst \
    doc/source/reference/parameters/imfile-statefile.rst \
    doc/source/reference/parameters/imfile-tag.rst \
    doc/source/reference/parameters/imfile-timeoutgranularity.rst \
    doc/source/reference/parameters/imfile-trimlineoverbytes.rst \
    doc/source/reference/parameters/imgssapi-inputgsslistenportfilename.rst \
    doc/source/reference/parameters/imgssapi-inputgssserverkeepalive.rst \
    doc/source/reference/parameters/imgssapi-inputgssservermaxsessions.rst \
    doc/source/reference/parameters/imgssapi-inputgssserverpermitplaintcp.rst \
    doc/source/reference/parameters/imgssapi-inputgssserverrun.rst \
    doc/source/reference/parameters/imgssapi-inputgssserverservicename.rst \
    doc/source/reference/parameters/imjournal-defaultfacility.rst \
    doc/source/reference/parameters/imjournal-defaultseverity.rst \
    doc/source/reference/parameters/imjournal-defaulttag.rst \
    doc/source/reference/parameters/imjournal-filecreatemode.rst \
    doc/source/reference/parameters/imjournal-fsync.rst \
    doc/source/reference/parameters/imjournal-ignorenonvalidstatefile.rst \
    doc/source/reference/parameters/imjournal-ignorepreviousmessages.rst \
    doc/source/reference/parameters/imjournal-main.rst \
    doc/source/reference/parameters/imjournal-persiststateinterval.rst \
    doc/source/reference/parameters/imjournal-ratelimit-burst.rst \
    doc/source/reference/parameters/imjournal-ratelimit-interval.rst \
    doc/source/reference/parameters/imjournal-remote.rst \
    doc/source/reference/parameters/imjournal-statefile.rst \
    doc/source/reference/parameters/imjournal-usepid.rst \
    doc/source/reference/parameters/imjournal-usepidfromsystem.rst \
    doc/source/reference/parameters/imjournal-workaroundjournalbug.rst \
    doc/source/reference/parameters/imkafka-broker.rst \
    doc/source/reference/parameters/imkafka-confparam.rst \
    doc/source/reference/parameters/imkafka-consumergroup.rst \
    doc/source/reference/parameters/imkafka-parsehostname.rst \
    doc/source/reference/parameters/imkafka-ruleset.rst \
    doc/source/reference/parameters/imkafka-topic.rst \
    doc/source/reference/parameters/imklog-consoleloglevel.rst \
    doc/source/reference/parameters/imklog-internalmsgfacility.rst \
    doc/source/reference/parameters/imklog-keepkerneltimestamp.rst \
    doc/source/reference/parameters/imklog-logpath.rst \
    doc/source/reference/parameters/imklog-parsekerneltimestamp.rst \
    doc/source/reference/parameters/imklog-permitnonkernelfacility.rst \
    doc/source/reference/parameters/imklog-ratelimitburst.rst \
    doc/source/reference/parameters/imklog-ratelimitinterval.rst \
    doc/source/reference/parameters/immark-interval.rst \
    doc/source/reference/parameters/immark-markmessagetext.rst \
    doc/source/reference/parameters/immark-ruleset.rst \
    doc/source/reference/parameters/immark-use-markflag.rst \
    doc/source/reference/parameters/immark-use-syslogcall.rst \
    doc/source/reference/parameters/impstats-bracketing.rst \
    doc/source/reference/parameters/impstats-facility.rst \
    doc/source/reference/parameters/impstats-format.rst \
    doc/source/reference/parameters/impstats-interval.rst \
    doc/source/reference/parameters/impstats-log-file.rst \
    doc/source/reference/parameters/impstats-log-syslog.rst \
    doc/source/reference/parameters/impstats-resetcounters.rst \
    doc/source/reference/parameters/impstats-ruleset.rst \
    doc/source/reference/parameters/impstats-severity.rst \
    doc/source/reference/parameters/imptcp-address.rst \
    doc/source/reference/parameters/imptcp-addtlframedelimiter.rst \
    doc/source/reference/parameters/imptcp-compression-mode.rst \
    doc/source/reference/parameters/imptcp-defaulttz.rst \
    doc/source/reference/parameters/imptcp-discardtruncatedmsg.rst \
    doc/source/reference/parameters/imptcp-failonchownfailure.rst \
    doc/source/reference/parameters/imptcp-filecreatemode.rst \
    doc/source/reference/parameters/imptcp-filegroup.rst \
    doc/source/reference/parameters/imptcp-filegroupnum.rst \
    doc/source/reference/parameters/imptcp-fileowner.rst \
    doc/source/reference/parameters/imptcp-fileownernum.rst \
    doc/source/reference/parameters/imptcp-flowcontrol.rst \
    doc/source/reference/parameters/imptcp-framing-delimiter-regex.rst \
    doc/source/reference/parameters/imptcp-framingfix-cisco-asa.rst \
    doc/source/reference/parameters/imptcp-keepalive-interval.rst \
    doc/source/reference/parameters/imptcp-keepalive-probes.rst \
    doc/source/reference/parameters/imptcp-keepalive-time.rst \
    doc/source/reference/parameters/imptcp-keepalive.rst \
    doc/source/reference/parameters/imptcp-listenportfilename.rst \
    doc/source/reference/parameters/imptcp-maxframesize.rst \
    doc/source/reference/parameters/imptcp-maxsessions.rst \
    doc/source/reference/parameters/imptcp-multiline.rst \
    doc/source/reference/parameters/imptcp-name.rst \
    doc/source/reference/parameters/imptcp-notifyonconnectionclose.rst \
    doc/source/reference/parameters/imptcp-notifyonconnectionopen.rst \
    doc/source/reference/parameters/imptcp-path.rst \
    doc/source/reference/parameters/imptcp-port.rst \
    doc/source/reference/parameters/imptcp-processonpoller.rst \
    doc/source/reference/parameters/imptcp-ratelimit-burst.rst \
    doc/source/reference/parameters/imptcp-ratelimit-interval.rst \
    doc/source/reference/parameters/imptcp-ruleset.rst \
    doc/source/reference/parameters/imptcp-socketbacklog.rst \
    doc/source/reference/parameters/imptcp-supportoctetcountedframing.rst \
    doc/source/reference/parameters/imptcp-threads.rst \
    doc/source/reference/parameters/imptcp-unlink.rst \
    doc/source/reference/parameters/imrelp-address.rst \
    doc/source/reference/parameters/imrelp-flowcontrol.rst \
    doc/source/reference/parameters/imrelp-keepalive-interval.rst \
    doc/source/reference/parameters/imrelp-keepalive-probes.rst \
    doc/source/reference/parameters/imrelp-keepalive-time.rst \
    doc/source/reference/parameters/imrelp-keepalive.rst \
    doc/source/reference/parameters/imrelp-maxdatasize.rst \
    doc/source/reference/parameters/imrelp-name.rst \
    doc/source/reference/parameters/imrelp-oversizemode.rst \
    doc/source/reference/parameters/imrelp-port.rst \
    doc/source/reference/parameters/imrelp-ruleset-input.rst \
    doc/source/reference/parameters/imrelp-ruleset.rst \
    doc/source/reference/parameters/imrelp-tls-authmode.rst \
    doc/source/reference/parameters/imrelp-tls-cacert.rst \
    doc/source/reference/parameters/imrelp-tls-compression.rst \
    doc/source/reference/parameters/imrelp-tls-dhbits.rst \
    doc/source/reference/parameters/imrelp-tls-mycert.rst \
    doc/source/reference/parameters/imrelp-tls-myprivkey.rst \
    doc/source/reference/parameters/imrelp-tls-permittedpeer.rst \
    doc/source/reference/parameters/imrelp-tls-prioritystring.rst \
    doc/source/reference/parameters/imrelp-tls-tlscfgcmd.rst \
    doc/source/reference/parameters/imrelp-tls-tlslib.rst \
    doc/source/reference/parameters/imrelp-tls.rst \
    doc/source/reference/parameters/imsolaris-imsolarislogsocketname.rst \
    doc/source/reference/parameters/imtcp-address.rst \
    doc/source/reference/parameters/imtcp-addtlframedelimiter.rst \
    doc/source/reference/parameters/imtcp-disablelfdelimiter.rst \
    doc/source/reference/parameters/imtcp-discardtruncatedmsg.rst \
    doc/source/reference/parameters/imtcp-flowcontrol.rst \
    doc/source/reference/parameters/imtcp-gnutlsprioritystring.rst \
    doc/source/reference/parameters/imtcp-keepalive-interval.rst \
    doc/source/reference/parameters/imtcp-keepalive-probes.rst \
    doc/source/reference/parameters/imtcp-keepalive-time.rst \
    doc/source/reference/parameters/imtcp-keepalive.rst \
    doc/source/reference/parameters/imtcp-listenportfilename.rst \
    doc/source/reference/parameters/imtcp-maxframesize.rst \
    doc/source/reference/parameters/imtcp-maxlisteners.rst \
    doc/source/reference/parameters/imtcp-maxsessions.rst \
    doc/source/reference/parameters/imtcp-name.rst \
    doc/source/reference/parameters/imtcp-networknamespace.rst \
    doc/source/reference/parameters/imtcp-notifyonconnectionclose.rst \
    doc/source/reference/parameters/imtcp-notifyonconnectionopen.rst \
    doc/source/reference/parameters/imtcp-permittedpeer.rst \
    doc/source/reference/parameters/imtcp-port.rst \
    doc/source/reference/parameters/imtcp-preservecase.rst \
    doc/source/reference/parameters/imtcp-ratelimit-burst.rst \
    doc/source/reference/parameters/imtcp-ratelimit-interval.rst \
    doc/source/reference/parameters/imtcp-ruleset.rst \
    doc/source/reference/parameters/imtcp-socketbacklog.rst \
    doc/source/reference/parameters/imtcp-starvationprotection-maxreads.rst \
    doc/source/reference/parameters/imtcp-streamdriver-authmode.rst \
    doc/source/reference/parameters/imtcp-streamdriver-cafile.rst \
    doc/source/reference/parameters/imtcp-streamdriver-certfile.rst \
    doc/source/reference/parameters/imtcp-streamdriver-checkextendedkeypurpose.rst \
    doc/source/reference/parameters/imtcp-streamdriver-crlfile.rst \
    doc/source/reference/parameters/imtcp-streamdriver-keyfile.rst \
    doc/source/reference/parameters/imtcp-streamdriver-mode.rst \
    doc/source/reference/parameters/imtcp-streamdriver-name.rst \
    doc/source/reference/parameters/imtcp-streamdriver-permitexpiredcerts.rst \
    doc/source/reference/parameters/imtcp-streamdriver-prioritizesan.rst \
    doc/source/reference/parameters/imtcp-streamdriver-tlsverifydepth.rst \
    doc/source/reference/parameters/imtcp-supportoctetcountedframing.rst \
    doc/source/reference/parameters/imtcp-workerthreads.rst \
    doc/source/reference/parameters/imudp-address.rst \
    doc/source/reference/parameters/imudp-batchsize.rst \
    doc/source/reference/parameters/imudp-defaulttz.rst \
    doc/source/reference/parameters/imudp-device.rst \
    doc/source/reference/parameters/imudp-ipfreebind.rst \
    doc/source/reference/parameters/imudp-name-appendport.rst \
    doc/source/reference/parameters/imudp-name.rst \
    doc/source/reference/parameters/imudp-port.rst \
    doc/source/reference/parameters/imudp-preservecase.rst \
    doc/source/reference/parameters/imudp-ratelimit-burst.rst \
    doc/source/reference/parameters/imudp-ratelimit-interval.rst \
    doc/source/reference/parameters/imudp-rcvbufsize.rst \
    doc/source/reference/parameters/imudp-ruleset.rst \
    doc/source/reference/parameters/imudp-schedulingpolicy.rst \
    doc/source/reference/parameters/imudp-schedulingpriority.rst \
    doc/source/reference/parameters/imudp-threads.rst \
    doc/source/reference/parameters/imudp-timerequery.rst \
    doc/source/reference/parameters/imuxsock-annotate.rst \
    doc/source/reference/parameters/imuxsock-createpath.rst \
    doc/source/reference/parameters/imuxsock-flowcontrol.rst \
    doc/source/reference/parameters/imuxsock-hostname.rst \
    doc/source/reference/parameters/imuxsock-ignoreownmessages.rst \
    doc/source/reference/parameters/imuxsock-ignoretimestamp.rst \
    doc/source/reference/parameters/imuxsock-parsehostname.rst \
    doc/source/reference/parameters/imuxsock-parsetrusted.rst \
    doc/source/reference/parameters/imuxsock-ratelimit-burst.rst \
    doc/source/reference/parameters/imuxsock-ratelimit-interval.rst \
    doc/source/reference/parameters/imuxsock-ratelimit-severity.rst \
    doc/source/reference/parameters/imuxsock-ruleset.rst \
    doc/source/reference/parameters/imuxsock-socket.rst \
    doc/source/reference/parameters/imuxsock-syssock-annotate.rst \
    doc/source/reference/parameters/imuxsock-syssock-flowcontrol.rst \
    doc/source/reference/parameters/imuxsock-syssock-ignoreownmessages.rst \
    doc/source/reference/parameters/imuxsock-syssock-ignoretimestamp.rst \
    doc/source/reference/parameters/imuxsock-syssock-name.rst \
    doc/source/reference/parameters/imuxsock-syssock-parsehostname.rst \
    doc/source/reference/parameters/imuxsock-syssock-parsetrusted.rst \
    doc/source/reference/parameters/imuxsock-syssock-ratelimit-burst.rst \
    doc/source/reference/parameters/imuxsock-syssock-ratelimit-interval.rst \
    doc/source/reference/parameters/imuxsock-syssock-ratelimit-severity.rst \
    doc/source/reference/parameters/imuxsock-syssock-unlink.rst \
    doc/source/reference/parameters/imuxsock-syssock-use.rst \
    doc/source/reference/parameters/imuxsock-syssock-usepidfromsystem.rst \
    doc/source/reference/parameters/imuxsock-syssock-usespecialparser.rst \
    doc/source/reference/parameters/imuxsock-syssock-usesystimestamp.rst \
    doc/source/reference/parameters/imuxsock-unlink.rst \
    doc/source/reference/parameters/imuxsock-usepidfromsystem.rst \
    doc/source/reference/parameters/imuxsock-usespecialparser.rst \
    doc/source/reference/parameters/imuxsock-usesystimestamp.rst \
    doc/source/reference/parameters/mmaitag-apikey.rst \
    doc/source/reference/parameters/mmaitag-apikey_file.rst \
    doc/source/reference/parameters/mmaitag-expert-initialprompt.rst \
    doc/source/reference/parameters/mmaitag-inputproperty.rst \
    doc/source/reference/parameters/mmaitag-model.rst \
    doc/source/reference/parameters/mmaitag-provider.rst \
    doc/source/reference/parameters/mmaitag-tag.rst \
    doc/source/reference/parameters/mmanon-embeddedipv4-anonmode.rst \
    doc/source/reference/parameters/mmanon-embeddedipv4-bits.rst \
    doc/source/reference/parameters/mmanon-embeddedipv4-enable.rst \
    doc/source/reference/parameters/mmanon-ipv4-bits.rst \
    doc/source/reference/parameters/mmanon-ipv4-enable.rst \
    doc/source/reference/parameters/mmanon-ipv4-mode.rst \
    doc/source/reference/parameters/mmanon-ipv4-replacechar.rst \
    doc/source/reference/parameters/mmanon-ipv6-anonmode.rst \
    doc/source/reference/parameters/mmanon-ipv6-bits.rst \
    doc/source/reference/parameters/mmanon-ipv6-enable.rst \
    doc/source/reference/parameters/mmcount-appname.rst \
    doc/source/reference/parameters/mmcount-key.rst \
    doc/source/reference/parameters/mmcount-value.rst \
    doc/source/reference/parameters/mmdarwin-container.rst \
    doc/source/reference/parameters/mmdarwin-fields.rst \
    doc/source/reference/parameters/mmdarwin-filtercode.rst \
    doc/source/reference/parameters/mmdarwin-key.rst \
    doc/source/reference/parameters/mmdarwin-response.rst \
    doc/source/reference/parameters/mmdarwin-send_partial.rst \
    doc/source/reference/parameters/mmdarwin-socketpath.rst \
    doc/source/reference/parameters/mmdblookup-container.rst \
    doc/source/reference/parameters/mmdblookup-fields.rst \
    doc/source/reference/parameters/mmdblookup-key.rst \
    doc/source/reference/parameters/mmdblookup-mmdbfile.rst \
    doc/source/reference/parameters/mmdblookup-reloadonhup.rst \
    doc/source/reference/parameters/mmexternal-binary.rst \
    doc/source/reference/parameters/mmexternal-forcesingleinstance.rst \
    doc/source/reference/parameters/mmexternal-interface-input.rst \
    doc/source/reference/parameters/mmexternal-output.rst \
    doc/source/reference/parameters/mmfields-jsonroot.rst \
    doc/source/reference/parameters/mmfields-separator.rst \
    doc/source/reference/parameters/mmjsonparse-allow_trailing.rst \
    doc/source/reference/parameters/mmjsonparse-container.rst \
    doc/source/reference/parameters/mmjsonparse-cookie.rst \
    doc/source/reference/parameters/mmjsonparse-max_scan_bytes.rst \
    doc/source/reference/parameters/mmjsonparse-mode.rst \
    doc/source/reference/parameters/mmjsonparse-userawmsg.rst \
    doc/source/reference/parameters/mmjsonrewrite-input.rst \
    doc/source/reference/parameters/mmjsonrewrite-output.rst \
    doc/source/reference/parameters/mmjsontransform-input.rst \
    doc/source/reference/parameters/mmjsontransform-mode.rst \
    doc/source/reference/parameters/mmjsontransform-output.rst \
    doc/source/reference/parameters/mmkubernetes-allowunsignedcerts.rst \
    doc/source/reference/parameters/mmkubernetes-annotation-match.rst \
    doc/source/reference/parameters/mmkubernetes-busyretryinterval.rst \
    doc/source/reference/parameters/mmkubernetes-cacheentryttl.rst \
    doc/source/reference/parameters/mmkubernetes-cacheexpireinterval.rst \
    doc/source/reference/parameters/mmkubernetes-containerrulebase.rst \
    doc/source/reference/parameters/mmkubernetes-containerrules.rst \
    doc/source/reference/parameters/mmkubernetes-de-dot-separator.rst \
    doc/source/reference/parameters/mmkubernetes-de-dot.rst \
    doc/source/reference/parameters/mmkubernetes-dstmetadatapath.rst \
    doc/source/reference/parameters/mmkubernetes-filenamerulebase.rst \
    doc/source/reference/parameters/mmkubernetes-filenamerules.rst \
    doc/source/reference/parameters/mmkubernetes-kubernetesurl.rst \
    doc/source/reference/parameters/mmkubernetes-skipverifyhost.rst \
    doc/source/reference/parameters/mmkubernetes-srcmetadatapath.rst \
    doc/source/reference/parameters/mmkubernetes-sslpartialchain.rst \
    doc/source/reference/parameters/mmkubernetes-tls-cacert.rst \
    doc/source/reference/parameters/mmkubernetes-tls-mycert.rst \
    doc/source/reference/parameters/mmkubernetes-tls-myprivkey.rst \
    doc/source/reference/parameters/mmkubernetes-token.rst \
    doc/source/reference/parameters/mmkubernetes-tokenfile.rst \
    doc/source/reference/parameters/mmnormalize-allowregex.rst \
    doc/source/reference/parameters/mmnormalize-path.rst \
    doc/source/reference/parameters/mmnormalize-rule.rst \
    doc/source/reference/parameters/mmnormalize-rulebase.rst \
    doc/source/reference/parameters/mmnormalize-userawmsg.rst \
    doc/source/reference/parameters/mmnormalize-variable.rst \
    doc/source/reference/parameters/mmpstrucdata-jsonroot.rst \
    doc/source/reference/parameters/mmpstrucdata-sd_name.lowercase.rst \
    doc/source/reference/parameters/mmrfc5424addhmac-hashfunction.rst \
    doc/source/reference/parameters/mmrfc5424addhmac-key.rst \
    doc/source/reference/parameters/mmrfc5424addhmac-sd-id.rst \
    doc/source/reference/parameters/mmsnmptrapd-severitymapping.rst \
    doc/source/reference/parameters/mmsnmptrapd-tag.rst \
    doc/source/reference/parameters/mmtaghostname-forcelocalhostname.rst \
    doc/source/reference/parameters/mmtaghostname-tag.rst \
    doc/source/reference/parameters/mmutf8fix-mode.rst \
    doc/source/reference/parameters/mmutf8fix-replacementchar.rst \
    doc/source/reference/parameters/omazureeventhubs-amqp_address.rst \
    doc/source/reference/parameters/omazureeventhubs-azure_key.rst \
    doc/source/reference/parameters/omazureeventhubs-azure_key_name.rst \
    doc/source/reference/parameters/omazureeventhubs-azurehost.rst \
    doc/source/reference/parameters/omazureeventhubs-azureport.rst \
    doc/source/reference/parameters/omazureeventhubs-container.rst \
    doc/source/reference/parameters/omazureeventhubs-eventproperties.rst \
    doc/source/reference/parameters/omazureeventhubs-statsname.rst \
    doc/source/reference/parameters/omazureeventhubs-template.rst \
    doc/source/reference/parameters/omclickhouse-allowunsignedcerts.rst \
    doc/source/reference/parameters/omclickhouse-bulkmode.rst \
    doc/source/reference/parameters/omclickhouse-errorfile.rst \
    doc/source/reference/parameters/omclickhouse-healthchecktimeout.rst \
    doc/source/reference/parameters/omclickhouse-maxbytes.rst \
    doc/source/reference/parameters/omclickhouse-port.rst \
    doc/source/reference/parameters/omclickhouse-pwd.rst \
    doc/source/reference/parameters/omclickhouse-server.rst \
    doc/source/reference/parameters/omclickhouse-skipverifyhost.rst \
    doc/source/reference/parameters/omclickhouse-template.rst \
    doc/source/reference/parameters/omclickhouse-timeout.rst \
    doc/source/reference/parameters/omclickhouse-usehttps.rst \
    doc/source/reference/parameters/omclickhouse-user.rst \
    doc/source/reference/parameters/omdtls-port.rst \
    doc/source/reference/parameters/omdtls-statsname.rst \
    doc/source/reference/parameters/omdtls-target.rst \
    doc/source/reference/parameters/omdtls-template.rst \
    doc/source/reference/parameters/omdtls-tls-authmode.rst \
    doc/source/reference/parameters/omdtls-tls-cacert.rst \
    doc/source/reference/parameters/omdtls-tls-mycert.rst \
    doc/source/reference/parameters/omdtls-tls-myprivkey.rst \
    doc/source/reference/parameters/omdtls-tls-tlscfgcmd.rst \
    doc/source/reference/parameters/omelasticsearch-allowunsignedcerts.rst \
    doc/source/reference/parameters/omelasticsearch-apikey.rst \
    doc/source/reference/parameters/omelasticsearch-asyncrepl.rst \
    doc/source/reference/parameters/omelasticsearch-bulkid.rst \
    doc/source/reference/parameters/omelasticsearch-bulkmode.rst \
    doc/source/reference/parameters/omelasticsearch-dynbulkid.rst \
    doc/source/reference/parameters/omelasticsearch-dynparent.rst \
    doc/source/reference/parameters/omelasticsearch-dynpipelinename.rst \
    doc/source/reference/parameters/omelasticsearch-dynsearchindex.rst \
    doc/source/reference/parameters/omelasticsearch-dynsearchtype.rst \
    doc/source/reference/parameters/omelasticsearch-errorfile.rst \
    doc/source/reference/parameters/omelasticsearch-esversion-major.rst \
    doc/source/reference/parameters/omelasticsearch-healthchecktimeout.rst \
    doc/source/reference/parameters/omelasticsearch-indextimeout.rst \
    doc/source/reference/parameters/omelasticsearch-maxbytes.rst \
    doc/source/reference/parameters/omelasticsearch-parent.rst \
    doc/source/reference/parameters/omelasticsearch-pipelinename.rst \
    doc/source/reference/parameters/omelasticsearch-pwd.rst \
    doc/source/reference/parameters/omelasticsearch-ratelimit-burst.rst \
    doc/source/reference/parameters/omelasticsearch-ratelimit-interval.rst \
    doc/source/reference/parameters/omelasticsearch-rebindinterval.rst \
    doc/source/reference/parameters/omelasticsearch-retryfailures.rst \
    doc/source/reference/parameters/omelasticsearch-retryruleset.rst \
    doc/source/reference/parameters/omelasticsearch-searchindex.rst \
    doc/source/reference/parameters/omelasticsearch-searchtype.rst \
    doc/source/reference/parameters/omelasticsearch-server.rst \
    doc/source/reference/parameters/omelasticsearch-serverport.rst \
    doc/source/reference/parameters/omelasticsearch-skippipelineifempty.rst \
    doc/source/reference/parameters/omelasticsearch-skipverifyhost.rst \
    doc/source/reference/parameters/omelasticsearch-template.rst \
    doc/source/reference/parameters/omelasticsearch-timeout.rst \
    doc/source/reference/parameters/omelasticsearch-tls-cacert.rst \
    doc/source/reference/parameters/omelasticsearch-tls-mycert.rst \
    doc/source/reference/parameters/omelasticsearch-tls-myprivkey.rst \
    doc/source/reference/parameters/omelasticsearch-uid.rst \
    doc/source/reference/parameters/omelasticsearch-usehttps.rst \
    doc/source/reference/parameters/omelasticsearch-writeoperation.rst \
    doc/source/reference/parameters/omfile-addlf.rst \
    doc/source/reference/parameters/omfile-asyncwriting.rst \
    doc/source/reference/parameters/omfile-closetimeout.rst \
    doc/source/reference/parameters/omfile-compression-driver.rst \
    doc/source/reference/parameters/omfile-compression-zstd-workers.rst \
    doc/source/reference/parameters/omfile-createdirs.rst \
    doc/source/reference/parameters/omfile-cry-provider.rst \
    doc/source/reference/parameters/omfile-dircreatemode.rst \
    doc/source/reference/parameters/omfile-dirgroup.rst \
    doc/source/reference/parameters/omfile-dirgroupnum.rst \
    doc/source/reference/parameters/omfile-dirowner.rst \
    doc/source/reference/parameters/omfile-dirownernum.rst \
    doc/source/reference/parameters/omfile-dynafile-donotsuspend.rst \
    doc/source/reference/parameters/omfile-dynafile.rst \
    doc/source/reference/parameters/omfile-dynafilecachesize.rst \
    doc/source/reference/parameters/omfile-failonchownfailure.rst \
    doc/source/reference/parameters/omfile-file.rst \
    doc/source/reference/parameters/omfile-filecreatemode.rst \
    doc/source/reference/parameters/omfile-filegroup.rst \
    doc/source/reference/parameters/omfile-filegroupnum.rst \
    doc/source/reference/parameters/omfile-fileowner.rst \
    doc/source/reference/parameters/omfile-fileownernum.rst \
    doc/source/reference/parameters/omfile-flushinterval.rst \
    doc/source/reference/parameters/omfile-flushontxend.rst \
    doc/source/reference/parameters/omfile-iobuffersize.rst \
    doc/source/reference/parameters/omfile-rotation-sizelimit.rst \
    doc/source/reference/parameters/omfile-rotation-sizelimitcommand.rst \
    doc/source/reference/parameters/omfile-sig-provider.rst \
    doc/source/reference/parameters/omfile-sync.rst \
    doc/source/reference/parameters/omfile-template.rst \
    doc/source/reference/parameters/omfile-veryrobustzip.rst \
    doc/source/reference/parameters/omfile-ziplevel.rst \
    doc/source/reference/parameters/omgssapi-actiongssforwarddefaulttemplate.rst \
    doc/source/reference/parameters/omgssapi-gssforwardservicename.rst \
    doc/source/reference/parameters/omgssapi-gssmode.rst \
    doc/source/reference/parameters/omhdfs-omhdfsdefaulttemplate.rst \
    doc/source/reference/parameters/omhdfs-omhdfsfilename.rst \
    doc/source/reference/parameters/omhdfs-omhdfshost.rst \
    doc/source/reference/parameters/omhdfs-omhdfsport.rst \
    doc/source/reference/parameters/omhttp-allowunsignedcerts.rst \
    doc/source/reference/parameters/omhttp-batch-format.rst \
    doc/source/reference/parameters/omhttp-batch-maxbytes.rst \
    doc/source/reference/parameters/omhttp-batch-maxsize.rst \
    doc/source/reference/parameters/omhttp-batch.rst \
    doc/source/reference/parameters/omhttp-checkpath.rst \
    doc/source/reference/parameters/omhttp-compress-level.rst \
    doc/source/reference/parameters/omhttp-compress.rst \
    doc/source/reference/parameters/omhttp-dynrestpath.rst \
    doc/source/reference/parameters/omhttp-errorfile.rst \
    doc/source/reference/parameters/omhttp-healthchecktimeout.rst \
    doc/source/reference/parameters/omhttp-httpcontenttype.rst \
    doc/source/reference/parameters/omhttp-httpheaderkey.rst \
    doc/source/reference/parameters/omhttp-httpheaders.rst \
    doc/source/reference/parameters/omhttp-httpheadervalue.rst \
    doc/source/reference/parameters/omhttp-httpignorablecodes.rst \
    doc/source/reference/parameters/omhttp-httpretrycodes.rst \
    doc/source/reference/parameters/omhttp-proxyhost.rst \
    doc/source/reference/parameters/omhttp-proxyport.rst \
    doc/source/reference/parameters/omhttp-pwd.rst \
    doc/source/reference/parameters/omhttp-ratelimit-burst.rst \
    doc/source/reference/parameters/omhttp-ratelimit-interval.rst \
    doc/source/reference/parameters/omhttp-reloadonhup.rst \
    doc/source/reference/parameters/omhttp-restpath.rst \
    doc/source/reference/parameters/omhttp-restpathtimeout.rst \
    doc/source/reference/parameters/omhttp-retry-addmetadata.rst \
    doc/source/reference/parameters/omhttp-retry-ruleset.rst \
    doc/source/reference/parameters/omhttp-retry.rst \
    doc/source/reference/parameters/omhttp-server.rst \
    doc/source/reference/parameters/omhttp-serverport.rst \
    doc/source/reference/parameters/omhttp-skipverifyhost.rst \
    doc/source/reference/parameters/omhttp-statsname.rst \
    doc/source/reference/parameters/omhttp-template.rst \
    doc/source/reference/parameters/omhttp-tls-cacert.rst \
    doc/source/reference/parameters/omhttp-tls-mycert.rst \
    doc/source/reference/parameters/omhttp-tls-myprivkey.rst \
    doc/source/reference/parameters/omhttp-uid.rst \
    doc/source/reference/parameters/omhttp-usehttps.rst \
    doc/source/reference/parameters/omjournal-namespace.rst \
    doc/source/reference/parameters/omjournal-template.rst \
    doc/source/reference/parameters/omkafka-broker.rst \
    doc/source/reference/parameters/omkafka-closetimeout.rst \
    doc/source/reference/parameters/omkafka-confparam.rst \
    doc/source/reference/parameters/omkafka-dynakey.rst \
    doc/source/reference/parameters/omkafka-dynatopic-cachesize.rst \
    doc/source/reference/parameters/omkafka-dynatopic.rst \
    doc/source/reference/parameters/omkafka-errorfile.rst \
    doc/source/reference/parameters/omkafka-failedmsgfile.rst \
    doc/source/reference/parameters/omkafka-kafkaheader.rst \
    doc/source/reference/parameters/omkafka-keepfailedmessages.rst \
    doc/source/reference/parameters/omkafka-key.rst \
    doc/source/reference/parameters/omkafka-partitions-auto.rst \
    doc/source/reference/parameters/omkafka-partitions-number.rst \
    doc/source/reference/parameters/omkafka-partitions-usefixed.rst \
    doc/source/reference/parameters/omkafka-resubmitonfailure.rst \
    doc/source/reference/parameters/omkafka-statsfile.rst \
    doc/source/reference/parameters/omkafka-statsname.rst \
    doc/source/reference/parameters/omkafka-template.rst \
    doc/source/reference/parameters/omkafka-topic.rst \
    doc/source/reference/parameters/omkafka-topicconfparam.rst \
    doc/source/reference/parameters/omlibdbi-db.rst \
    doc/source/reference/parameters/omlibdbi-driver.rst \
    doc/source/reference/parameters/omlibdbi-driverdirectory.rst \
    doc/source/reference/parameters/omlibdbi-pwd.rst \
    doc/source/reference/parameters/omlibdbi-server.rst \
    doc/source/reference/parameters/omlibdbi-template.rst \
    doc/source/reference/parameters/omlibdbi-uid.rst \
    doc/source/reference/parameters/ommail-body-enable.rst \
    doc/source/reference/parameters/ommail-mailfrom.rst \
    doc/source/reference/parameters/ommail-mailto.rst \
    doc/source/reference/parameters/ommail-port.rst \
    doc/source/reference/parameters/ommail-server.rst \
    doc/source/reference/parameters/ommail-subject-template.rst \
    doc/source/reference/parameters/ommail-subject-text.rst \
    doc/source/reference/parameters/ommail-template.rst \
    doc/source/reference/parameters/ommysql-db.rst \
    doc/source/reference/parameters/ommysql-mysqlconfig-file.rst \
    doc/source/reference/parameters/ommysql-mysqlconfig-section.rst \
    doc/source/reference/parameters/ommysql-pwd.rst \
    doc/source/reference/parameters/ommysql-server.rst \
    doc/source/reference/parameters/ommysql-serverport.rst \
    doc/source/reference/parameters/ommysql-socket.rst \
    doc/source/reference/parameters/ommysql-template.rst \
    doc/source/reference/parameters/ommysql-uid.rst \
    doc/source/reference/parameters/ompgsql-conninfo.rst \
    doc/source/reference/parameters/ompgsql-db.rst \
    doc/source/reference/parameters/ompgsql-pass.rst \
    doc/source/reference/parameters/ompgsql-port.rst \
    doc/source/reference/parameters/ompgsql-server.rst \
    doc/source/reference/parameters/ompgsql-template.rst \
    doc/source/reference/parameters/ompgsql-user.rst \
    doc/source/reference/parameters/omprog-begintransactionmark.rst \
    doc/source/reference/parameters/omprog-binary.rst \
    doc/source/reference/parameters/omprog-closetimeout.rst \
    doc/source/reference/parameters/omprog-committransactionmark.rst \
    doc/source/reference/parameters/omprog-confirmmessages.rst \
    doc/source/reference/parameters/omprog-confirmtimeout.rst \
    doc/source/reference/parameters/omprog-filecreatemode.rst \
    doc/source/reference/parameters/omprog-forcesingleinstance.rst \
    doc/source/reference/parameters/omprog-hup-signal.rst \
    doc/source/reference/parameters/omprog-killunresponsive.rst \
    doc/source/reference/parameters/omprog-output.rst \
    doc/source/reference/parameters/omprog-reportfailures.rst \
    doc/source/reference/parameters/omprog-signalonclose.rst \
    doc/source/reference/parameters/omprog-template.rst \
    doc/source/reference/parameters/omprog-usetransactions.rst \
    doc/source/reference/parameters/omrelp-conn-timeout.rst \
    doc/source/reference/parameters/omrelp-keepalive.rst \
    doc/source/reference/parameters/omrelp-keepalive-interval.rst \
    doc/source/reference/parameters/omrelp-keepalive-probes.rst \
    doc/source/reference/parameters/omrelp-keepalive-time.rst \
    doc/source/reference/parameters/omrelp-localclientip.rst \
    doc/source/reference/parameters/omrelp-port.rst \
    doc/source/reference/parameters/omrelp-rebindinterval.rst \
    doc/source/reference/parameters/omrelp-target.rst \
    doc/source/reference/parameters/omrelp-template.rst \
    doc/source/reference/parameters/omrelp-timeout.rst \
    doc/source/reference/parameters/omrelp-tls.rst \
    doc/source/reference/parameters/omrelp-tls-authmode.rst \
    doc/source/reference/parameters/omrelp-tls-cacert.rst \
    doc/source/reference/parameters/omrelp-tls-compression.rst \
    doc/source/reference/parameters/omrelp-tls-mycert.rst \
    doc/source/reference/parameters/omrelp-tls-myprivkey.rst \
    doc/source/reference/parameters/omrelp-tls-permittedpeer.rst \
    doc/source/reference/parameters/omrelp-tls-prioritystring.rst \
    doc/source/reference/parameters/omrelp-tls-tlscfgcmd.rst \
    doc/source/reference/parameters/omrelp-tls-tlslib.rst \
    doc/source/reference/parameters/omrelp-windowsize.rst \
    doc/source/reference/parameters/omsendertrack-cmdfile.rst \
    doc/source/reference/parameters/omsendertrack-interval.rst \
    doc/source/reference/parameters/omsendertrack-senderid.rst \
    doc/source/reference/parameters/omsendertrack-statefile.rst \
    doc/source/reference/parameters/omsnmp-community.rst \
    doc/source/reference/parameters/omsnmp-enterpriseoid.rst \
    doc/source/reference/parameters/omsnmp-messageoid.rst \
    doc/source/reference/parameters/omsnmp-port.rst \
    doc/source/reference/parameters/omsnmp-server.rst \
    doc/source/reference/parameters/omsnmp-snmpv1dynsource.rst \
    doc/source/reference/parameters/omsnmp-specifictype.rst \
    doc/source/reference/parameters/omsnmp-transport.rst \
    doc/source/reference/parameters/omsnmp-trapoid.rst \
    doc/source/reference/parameters/omsnmp-traptype.rst \
    doc/source/reference/parameters/omsnmp-version.rst \
    doc/source/reference/parameters/omstdout-ensurelfending.rst \
    doc/source/reference/parameters/omstdout-template.rst \
    doc/source/reference/parameters/omuxsock-abstract.rst \
    doc/source/reference/parameters/omuxsock-networknamespace.rst \
    doc/source/reference/parameters/omuxsock-socketname.rst \
    doc/source/reference/parameters/omuxsock-sockettype.rst \
    doc/source/reference/parameters/omuxsock-template.rst \
    doc/source/reference/parameters/pmrfc3164-detect-headerless.rst \
    doc/source/reference/parameters/pmrfc3164-detect-yearaftertimestamp.rst \
    doc/source/reference/parameters/pmrfc3164-force-tagendingbycolon.rst \
    doc/source/reference/parameters/pmrfc3164-headerless-drop.rst \
    doc/source/reference/parameters/pmrfc3164-headerless-errorfile.rst \
    doc/source/reference/parameters/pmrfc3164-headerless-hostname.rst \
    doc/source/reference/parameters/pmrfc3164-headerless-ruleset.rst \
    doc/source/reference/parameters/pmrfc3164-headerless-tag.rst \
    doc/source/reference/parameters/pmrfc3164-permit-atsignsinhostname.rst \
    doc/source/reference/parameters/pmrfc3164-permit-slashesinhostname.rst \
    doc/source/reference/parameters/pmrfc3164-permit-squarebracketsinhostname.rst \
    doc/source/reference/parameters/pmrfc3164-remove-msgfirstspace.rst \
    doc/source/reference/properties/message-app-name.rst \
    doc/source/reference/properties/message-fromhost-ip.rst \
    doc/source/reference/properties/message-fromhost-port.rst \
    doc/source/reference/properties/message-fromhost.rst \
    doc/source/reference/properties/message-hostname.rst \
    doc/source/reference/properties/message-inputname.rst \
    doc/source/reference/properties/message-iut.rst \
    doc/source/reference/properties/message-jsonmesg.rst \
    doc/source/reference/properties/message-msg.rst \
    doc/source/reference/properties/message-msgid.rst \
    doc/source/reference/properties/message-pri-text.rst \
    doc/source/reference/properties/message-pri.rst \
    doc/source/reference/properties/message-procid.rst \
    doc/source/reference/properties/message-programname.rst \
    doc/source/reference/properties/message-protocol-version.rst \
    doc/source/reference/properties/message-rawmsg-after-pri.rst \
    doc/source/reference/properties/message-rawmsg.rst \
    doc/source/reference/properties/message-source.rst \
    doc/source/reference/properties/message-structured-data.rst \
    doc/source/reference/properties/message-syslogfacility-text.rst \
    doc/source/reference/properties/message-syslogfacility.rst \
    doc/source/reference/properties/message-syslogpriority-text.rst \
    doc/source/reference/properties/message-syslogpriority.rst \
    doc/source/reference/properties/message-syslogseverity-text.rst \
    doc/source/reference/properties/message-syslogseverity.rst \
    doc/source/reference/properties/message-syslogtag.rst \
    doc/source/reference/properties/message-timegenerated.rst \
    doc/source/reference/properties/message-timereported.rst \
    doc/source/reference/properties/message-timestamp.rst \
    doc/source/reference/properties/message-uuid.rst \
    doc/source/reference/properties/system-bom.rst \
    doc/source/reference/properties/system-myhostname.rst \
    doc/source/reference/properties/system-time-day.rst \
    doc/source/reference/properties/system-time-hhour.rst \
    doc/source/reference/properties/system-time-hour.rst \
    doc/source/reference/properties/system-time-minute.rst \
    doc/source/reference/properties/system-time-month.rst \
    doc/source/reference/properties/system-time-now-unixtimestamp.rst \
    doc/source/reference/properties/system-time-now.rst \
    doc/source/reference/properties/system-time-qhour.rst \
    doc/source/reference/properties/system-time-wday.rst \
    doc/source/reference/properties/system-time-year.rst \
    doc/source/reference/templates/templates-examples.rst \
    doc/source/reference/templates/templates-legacy.rst \
    doc/source/reference/templates/templates-options.rst \
    doc/source/reference/templates/templates-reserved-names.rst \
    doc/source/reference/templates/templates-statement-constant.rst \
    doc/source/reference/templates/templates-statement-property.rst \
    doc/source/reference/templates/templates-type-list.rst \
    doc/source/reference/templates/templates-type-plugin.rst \
    doc/source/reference/templates/templates-type-string.rst \
    doc/source/reference/templates/templates-type-subtree.rst \
    doc/source/rsyslog-vers.png \
    doc/source/rsyslog_confgraph_complex.png \
    doc/source/rsyslog_confgraph_std.png \
    doc/source/tls_cert.jpg \
    doc/source/tls_cert_100.jpg \
    doc/source/tls_cert_ca.jpg \
    doc/source/troubleshooting/debug.rst \
    doc/source/troubleshooting/file_not_written.rst \
    doc/source/troubleshooting/howtodebug.rst \
    doc/source/troubleshooting/index.rst \
    doc/source/troubleshooting/selinux.rst \
    doc/source/troubleshooting/troubleshoot.rst \
    doc/source/tutorials/cert-script.tar.gz \
    doc/source/tutorials/database.rst \
    doc/source/tutorials/failover_syslog_server.rst \
    doc/source/tutorials/gelf_forwarding.rst \
    doc/source/tutorials/hash_sampling.rst \
    doc/source/tutorials/high_database_rate.rst \
    doc/source/tutorials/index.rst \
    doc/source/tutorials/log_rotation_fix_size.rst \
    doc/source/tutorials/log_sampling.rst \
    doc/source/tutorials/random_sampling.rst \
    doc/source/tutorials/recording_pri.rst \
    doc/source/tutorials/reliable_forwarding.rst \
    doc/source/tutorials/tls.rst \
    doc/source/tutorials/tls_cert.jpg \
    doc/source/tutorials/tls_cert_100.jpg \
    doc/source/tutorials/tls_cert_ca.jpg \
    doc/source/tutorials/tls_cert_ca.rst \
    doc/source/tutorials/tls_cert_client.rst \
    doc/source/tutorials/tls_cert_errmsgs.rst \
    doc/source/tutorials/tls_cert_machine.rst \
    doc/source/tutorials/tls_cert_scenario.rst \
    doc/source/tutorials/tls_cert_script.rst \
    doc/source/tutorials/tls_cert_server.rst \
    doc/source/tutorials/tls_cert_summary.rst \
    doc/source/tutorials/tls_cert_udp_relay.rst \
    doc/source/whitepapers/dataflow.png \
    doc/source/whitepapers/direct_queue0.png \
    doc/source/whitepapers/direct_queue1.png \
    doc/source/whitepapers/direct_queue2.png \
    doc/source/whitepapers/direct_queue3.png \
    doc/source/whitepapers/direct_queue_directq.png \
    doc/source/whitepapers/direct_queue_rsyslog.png \
    doc/source/whitepapers/direct_queue_rsyslog2.png \
    doc/source/whitepapers/index.rst \
    doc/source/whitepapers/preserve_in_nat.rst \
    doc/source/whitepapers/queue_analogy_tv.png \
    doc/source/whitepapers/queues_analogy.rst \
    doc/source/whitepapers/reliable_logging.rst \
    doc/source/whitepapers/syslog_parsing.rst \
    doc/source/whitepapers/syslog_protocol.rst \
    doc/tools/README.md \
    doc/tools/build-doc-linux.sh \
    doc/tools/build-doc-windows.ps1 \
    doc/tools/buildenv/.dockerignore \
    doc/tools/buildenv/Dockerfile \
    doc/tools/buildenv/README.md \
    doc/tools/buildenv/starter.sh \
    doc/tools/buildenv/tools/bash \
    doc/tools/buildenv/tools/build-doc \
    doc/tools/buildenv/tools/git-clone \
    doc/tools/buildenv/tools/help \
    doc/tools/buildenv/tools/version-info \
    doc/tools/fix-mermaid-offline.py \
    doc/tools/release_build.sh \
    doc/utils/release_build.sh

SUBDIRS = compat runtime grammar . plugins/immark plugins/imuxsock plugins/imtcp plugins/imudp plugins/omtesting
# external plugin driver is always enabled (core component)
SUBDIRS += plugins/mmexternal

if ENABLE_RSYSLOGD
SUBDIRS += tools
endif

if ENABLE_IMKLOG
SUBDIRS += plugins/imklog
endif

if ENABLE_IMKMSG
SUBDIRS += contrib/imkmsg
endif

if ENABLE_IMPSTATS
SUBDIRS += plugins/impstats
endif

if ENABLE_IMSOLARIS
SUBDIRS += plugins/imsolaris
endif

if ENABLE_GSSAPI
SUBDIRS += plugins/omgssapi plugins/imgssapi
endif

if ENABLE_RELP
SUBDIRS += plugins/omrelp plugins/imrelp
endif

if ENABLE_OMFILE_HARDENED
SUBDIRS += contrib/omfile-hardened
endif

if ENABLE_MYSQL
SUBDIRS += plugins/ommysql
endif

if ENABLE_OMLIBDBI
SUBDIRS += plugins/omlibdbi
endif

if ENABLE_PGSQL
SUBDIRS += plugins/ompgsql
endif

if ENABLE_SNMP
SUBDIRS += plugins/omsnmp
endif

if ENABLE_OMSTDOUT
SUBDIRS += plugins/omstdout
endif

if ENABLE_OMSENDERTRACK
SUBDIRS += plugins/omsendertrack
endif

if ENABLE_PMCISCONAMES
SUBDIRS += contrib/pmcisconames
endif

if ENABLE_PMCISCOIOS
SUBDIRS += plugins/pmciscoios
endif

if ENABLE_PMNULL
SUBDIRS += plugins/pmnull
endif

if ENABLE_PMNORMALIZE
SUBDIRS += plugins/pmnormalize
endif

if ENABLE_PMAIXFORWARDEDFROM
SUBDIRS += contrib/pmaixforwardedfrom
endif

if ENABLE_PMSNARE
SUBDIRS += contrib/pmsnare
endif

if ENABLE_PMPANNGFW
SUBDIRS += contrib/pmpanngfw
endif

if ENABLE_PMLASTMSG
SUBDIRS += plugins/pmlastmsg
endif

if ENABLE_OMRULESET
SUBDIRS += plugins/omruleset
endif

if ENABLE_OMUDPSPOOF
SUBDIRS += plugins/omudpspoof
endif

if ENABLE_OMMONGODB
SUBDIRS += plugins/ommongodb
endif

if ENABLE_OMHIREDIS
SUBDIRS += contrib/omhiredis
endif

if ENABLE_OMCZMQ
SUBDIRS += contrib/omczmq
endif

if ENABLE_OMRABBITMQ
SUBDIRS += contrib/omrabbitmq
endif

if ENABLE_IMCZMQ
SUBDIRS += contrib/imczmq
endif

if ENABLE_OMUXSOCK
SUBDIRS += plugins/omuxsock
endif

if ENABLE_OMHDFS
SUBDIRS += plugins/omhdfs
endif

if ENABLE_OMOTEL
SUBDIRS += plugins/omotel
endif

if ENABLE_OMJOURNAL
SUBDIRS += plugins/omjournal
endif

if ENABLE_IMJOURNAL
SUBDIRS += plugins/imjournal
endif

if ENABLE_ELASTICSEARCH
SUBDIRS += plugins/omelasticsearch
endif

if ENABLE_CLICKHOUSE
SUBDIRS += plugins/omclickhouse
endif

if ENABLE_OMHTTP
SUBDIRS += contrib/omhttp
endif

if ENABLE_SNMP
SUBDIRS += plugins/omsnmp
endif

if ENABLE_MMSNMPTRAPD
SUBDIRS += plugins/mmsnmptrapd
endif

if ENABLE_IMFILE
SUBDIRS += plugins/imfile
endif

if ENABLE_IMDOCKER
SUBDIRS += contrib/imdocker
endif

if ENABLE_IMPTCP
SUBDIRS += plugins/imptcp
endif

if ENABLE_IMDIAG
SUBDIRS += plugins/imdiag
endif

if ENABLE_MAIL
SUBDIRS += plugins/ommail
endif

if ENABLE_FMHTTP
SUBDIRS += plugins/fmhttp
endif

if ENABLE_FMHASH
SUBDIRS += contrib/fmhash
endif

if ENABLE_FMUNFLATTEN
SUBDIRS += contrib/fmunflatten
endif

if ENABLE_FMPCRE
SUBDIRS += plugins/fmpcre
endif

if ENABLE_FFAUP
SUBDIRS += contrib/ffaup
endif

if ENABLE_OMKAFKA
SUBDIRS += plugins/omkafka
endif

if ENABLE_IMKAFKA
SUBDIRS += plugins/imkafka
endif

if ENABLE_OMAZUREEVENTHUBS
SUBDIRS += plugins/omazureeventhubs
endif

if ENABLE_IMDTLS
SUBDIRS += plugins/imdtls
endif

if ENABLE_OMDTLS
SUBDIRS += plugins/omdtls
endif

if ENABLE_OMPROG
SUBDIRS += plugins/omprog
endif

if ENABLE_RFC3195
SUBDIRS += plugins/im3195
endif

if ENABLE_MMNORMALIZE
SUBDIRS += plugins/mmnormalize
endif

if ENABLE_MMLEEFPARSE
SUBDIRS += plugins/mmleefparse
endif

if ENABLE_MMJSONPARSE
SUBDIRS += plugins/mmjsonparse
endif

if ENABLE_MMJSONTRANSFORM
SUBDIRS += plugins/mmjsontransform
endif

if ENABLE_MMGROK
SUBDIRS += contrib/mmgrok
endif

if ENABLE_MMAUDIT
SUBDIRS += plugins/mmaudit
endif

if ENABLE_MMANON
SUBDIRS += plugins/mmanon
endif

if ENABLE_MMRM1STSPACE
SUBDIRS += plugins/mmrm1stspace
endif

if ENABLE_MMUTF8FIX
SUBDIRS += plugins/mmutf8fix
endif

if ENABLE_MMCOUNT
SUBDIRS += contrib/mmcount
endif

if ENABLE_MMSEQUENCE
SUBDIRS += contrib/mmsequence
endif

if ENABLE_MMDBLOOKUP
SUBDIRS += plugins/mmdblookup
endif

if ENABLE_MMDARWIN
SUBDIRS += contrib/mmdarwin
endif

if ENABLE_MMFIELDS
SUBDIRS += plugins/mmfields
endif

if ENABLE_MMPSTRUCDATA
SUBDIRS += plugins/mmpstrucdata
endif

if ENABLE_MMAITAG
SUBDIRS += plugins/mmaitag
endif

if ENABLE_MMRFC5424ADDHMAC
SUBDIRS += contrib/mmrfc5424addhmac
endif

if ENABLE_MMSNAREPARSE
SUBDIRS += plugins/mmsnareparse
endif

# omhttpfs
if ENABLE_OMHTTPFS
SUBDIRS += contrib/omhttpfs
endif

# omamqp1
if ENABLE_OMAMQP1
SUBDIRS += contrib/omamqp1
endif

# imbatchreport
if ENABLE_IMBATCHREPORT
SUBDIRS += contrib/imbatchreport
endif

# omtcl
if ENABLE_OMTCL
SUBDIRS += contrib/omtcl
endif

# mmkubernetes
if ENABLE_MMKUBERNETES
SUBDIRS += contrib/mmkubernetes
endif

# impcap
if ENABLE_IMPCAP
SUBDIRS += contrib/impcap
endif

# imtuxedoulog
if ENABLE_IMTUXEDOULOG
SUBDIRS += contrib/imtuxedoulog
endif

# improg
if ENABLE_IMPROG
SUBDIRS += contrib/improg
endif

# imhttp
if ENABLE_IMHTTP
SUBDIRS += contrib/imhttp
endif

# mmtaghostname
if ENABLE_MMTAGHOSTNAME
SUBDIRS += contrib/mmtaghostname
endif

# imdb2diag
if ENABLE_PMDB2DIAG
SUBDIRS += contrib/pmdb2diag
endif

# imhiredis
if ENABLE_IMHIREDIS
SUBDIRS += contrib/imhiredis
endif

# tests are added as last element, because tests may need different
# modules that need to be generated first
SUBDIRS += tests



DISTCHECK_CONFIGURE_FLAGS=
# make sure "make distcheck" tries to build all modules. This means that
# a developer must always have an environment where every supporting library
# is available. If that is not the case, the respective configure option may
# temporarily be removed below. The intent behind forcing everthing to compile
# in a make distcheck is so that we detect code that accidently was not updated
# when some global update happened.
DISTCHECK_CONFIGURE_FLAGS+= \
	--enable-silent-rules \
	--enable-rsyslogd \
	--enable-omstdout \
	--enable-omsendertrack \
	--enable-imdiag \
	--enable-testbench \
	--enable-valgrind
# currently not supported in make distcheck:
# --enable-pgsql-tests
# --enable-extended-tests --> should probably never be enabled due to runtime

if ENABLE_DEFAULT_TESTS
DISTCHECK_CONFIGURE_FLAGS+= \
	--enable-diagtools \
	--enable-impstats \
	--enable-imptcp \
	--enable-klog \
	--enable-mail \
	--enable-mmanon \
	--enable-mmaudit \
	--enable-mmcount \
	--enable-mmexternal \
	--enable-mmfields \
	--enable-mmjsonparse \
	--enable-mmpstrucdata \
	--enable-mmrm1stspace \
	--enable-mmsequence \
	--enable-mmutf8fix \
	--enable-omruleset \
	--enable-omuxsock \
	--enable-pmaixforwardedfrom \
	--enable-pmciscoios \
	--enable-pmcisconames \
	--enable-pmlastmsg \
	--enable-pmnull \
	--enable-pmsnare \
	--enable-usertools

else
DISTCHECK_CONFIGURE_FLAGS+= \
	--disable-default-tests
endif  # if ENABLE_DEFAULT_TESTS

if ENABLE_IMPROG
DISTCHECK_CONFIGURE_FLAGS+= --enable-improg
endif

if ENABLE_IMHTTP
DISTCHECK_CONFIGURE_FLAGS+= --enable-imhttp
endif

if ENABLE_OMPROG
DISTCHECK_CONFIGURE_FLAGS+= --enable-omprog
endif

if ENABLE_GSSAPI
DISTCHECK_CONFIGURE_FLAGS+= --enable-gssapi-krb5
endif

if ENABLE_PMNORMALIZE
DISTCHECK_CONFIGURE_FLAGS+= --enable-pmnormalize
endif

if ENABLE_MMDBLOOKUP
DISTCHECK_CONFIGURE_FLAGS+= --enable-mmdblookup
endif

if ENABLE_MMDARWIN
DISTCHECK_CONFIGURE_FLAGS+= --enable-mmdarwin
endif

if ENABLE_MMNORMALIZE
DISTCHECK_CONFIGURE_FLAGS+= --enable-mmnormalize
endif

if ENABLE_OMMONGODB
DISTCHECK_CONFIGURE_FLAGS+= --enable-ommongodb
endif

if ENABLE_OMHIREDIS
DISTCHECK_CONFIGURE_FLAGS+= --enable-omhiredis
endif

if ENABLE_MMSNMPTRAPD
DISTCHECK_CONFIGURE_FLAGS+= --enable-mmsnmptrapd
endif

if ENABLE_OMLIBDBI
DISTCHECK_CONFIGURE_FLAGS+= --enable-libdbi
endif

if ENABLE_LIBGCRYPT
DISTCHECK_CONFIGURE_FLAGS+= --enable-libgcrypt
endif

if ENABLE_OMHTTP
DISTCHECK_CONFIGURE_FLAGS+= --enable-omhttp
endif

if ENABLE_OMHTTPFS
DISTCHECK_CONFIGURE_FLAGS+= --enable-omhttpfs
endif

if ENABLE_OMTCL
DISTCHECK_CONFIGURE_FLAGS+= --enable-omtcl
endif

if ENABLE_SNMP
DISTCHECK_CONFIGURE_FLAGS+= --enable-snmp
endif

if ENABLE_SNMP_TESTS
DISTCHECK_CONFIGURE_FLAGS+= --enable-snmp-tests
endif

if ENABLE_FMHTTP
DISTCHECK_CONFIGURE_FLAGS+= --enable-fmhttp
endif

if ENABLE_OMUDPSPOOF
DISTCHECK_CONFIGURE_FLAGS+= --enable-omudpspoof
endif

if ENABLE_PGSQL
DISTCHECK_CONFIGURE_FLAGS+= --enable-pgsql
endif

if ENABLE_GNUTLS
DISTCHECK_CONFIGURE_FLAGS+= --enable-gnutls
endif

if ENABLE_GNUTLS_TESTS
DISTCHECK_CONFIGURE_FLAGS+= --enable-gnutls-tests
else
DISTCHECK_CONFIGURE_FLAGS+= --disable-gnutls-tests
endif

if ENABLE_OPENSSL
DISTCHECK_CONFIGURE_FLAGS+= --enable-openssl
endif

if ENABLE_MYSQL
DISTCHECK_CONFIGURE_FLAGS+= --enable-mysql
endif
if ENABLE_MYSQL_TESTS
DISTCHECK_CONFIGURE_FLAGS+= --enable-mysql-tests
endif

if ENABLE_ELASTICSEARCH
DISTCHECK_CONFIGURE_FLAGS+= --enable-elasticsearch
endif
if ENABLE_ELASTICSEARCH_TESTS
DISTCHECK_CONFIGURE_FLAGS+= --enable-elasticsearch-tests
endif

if ENABLE_CLICKHOUSE
DISTCHECK_CONFIGURE_FLAGS+= --enable-clickhouse
endif
if ENABLE_CLICKHOUSE_TESTS
DISTCHECK_CONFIGURE_FLAGS+= --enable-clickhouse-tests
endif

if ENABLE_MMGROK
DISTCHECK_CONFIGURE_FLAGS+= --enable-mmgrok
endif

if ENABLE_RELP
DISTCHECK_CONFIGURE_FLAGS+= --enable-relp --enable-omrelp-default-port=13515
endif

if ENABLE_IMKAFKA
DISTCHECK_CONFIGURE_FLAGS+= --enable-imkafka
endif
if ENABLE_OMKAFKA
DISTCHECK_CONFIGURE_FLAGS+= --enable-omkafka
endif
if ENABLE_KAFKA_TESTS
DISTCHECK_CONFIGURE_FLAGS+= --enable-kafka-tests
endif

if ENABLE_OMAZUREEVENTHUBS
DISTCHECK_CONFIGURE_FLAGS+= --enable-omazureeventhubs
endif

if ENABLE_IMDTLS
DISTCHECK_CONFIGURE_FLAGS+= --enable-imdtls
endif

if ENABLE_OMDTLS
DISTCHECK_CONFIGURE_FLAGS+= --enable-omdtls
endif

if ENABLE_IMJOURNAL
DISTCHECK_CONFIGURE_FLAGS+= --enable-imjournal
endif
if ENABLE_OMJOURNAL
DISTCHECK_CONFIGURE_FLAGS+= --enable-omjournal
endif
if ENABLE_JOURNAL_TESTS
DISTCHECK_CONFIGURE_FLAGS+= --enable-journal-tests
endif

if ENABLE_IMCZMQ
DISTCHECK_CONFIGURE_FLAGS+= --enable-imczmq
endif
if ENABLE_OMCZMQ
DISTCHECK_CONFIGURE_FLAGS+= --enable-omczmq
endif

if ENABLE_MMTAGHOSTNAME
DISTCHECK_CONFIGURE_FLAGS+= --enable-mmtaghostname
endif

if ENABLE_IMTUXEDOULOG
DISTCHECK_CONFIGURE_FLAGS+= --enable-imtuxedolog
endif

if ENABLE_PMDB2DIAG
DISTCHECK_CONFIGURE_FLAGS+= --enable-pmdb2diag
endif

if ENABLE_IMBATCHREPORT
DISTCHECK_CONFIGURE_FLAGS+= --enable-imbatchreport
endif

if ENABLE_IMFILE
DISTCHECK_CONFIGURE_FLAGS+= --enable-imfile
endif

if ENABLE_IMFILE_TESTS
DISTCHECK_CONFIGURE_FLAGS+= --enable-imfile-tests
else
DISTCHECK_CONFIGURE_FLAGS+= --disable-imfile-tests
endif

if ENABLE_IMPCAP
DISTCHECK_CONFIGURE_FLAGS+= --enable-impcap
endif

if ENABLE_IMDOCKER
DISTCHECK_CONFIGURE_FLAGS+= --enable-imdocker
endif

if ENABLE_OMRABBITMQ
DISTCHECK_CONFIGURE_FLAGS+= --enable-omrabbitmq
endif

if ENABLE_MMKUBERNETES
DISTCHECK_CONFIGURE_FLAGS+= --enable-mmkubernetes
endif

if ENABLE_OMAMQP1
DISTCHECK_CONFIGURE_FLAGS+= --enable-omamqp1
endif

if ENABLE_DISTCHECK_WORKAROUND
DISTCHECK_CONFIGURE_FLAGS+= --disable-testbench
else
DISTCHECK_CONFIGURE_FLAGS+= --enable-testbench
endif

if ENABLE_IMHIREDIS
DISTCHECK_CONFIGURE_FLAGS+= --enable-imhiredis
endif

dist-hook:
	$(AM_V_GEN)echo $(VERSION) > $(distdir)/.tarball-version

ACLOCAL_AMFLAGS = -I m4

COVERAGE_INFO = $(top_builddir)/coverage.info
COVERAGE_DIR =  coverage

LCOV_FLAGS =	--capture \
	--config-file "$(top_srcdir)/lcovrc" \
	--directory "$(abs_top_builddir)" \
	--ignore-errors mismatch \
	--output-file $(COVERAGE_INFO)

GENHTML_FLAGS = --legend \
	--title "$(PACKAGE_NAME)-$(PACKAGE_VERSION) Code Coverage" \
	--output-directory $(top_builddir)/$(COVERAGE_DIR)

check-coverage: reset-coverage-stats
if ENABLE_COVERAGE
	$(MAKE) check || true
	$(MAKE) coverage

reset-coverage-stats:
	$(LCOV) --zerocounters --directory "$(abs_top_builddir)"

coverage: clean-coverage-local distclean-coverage-local
	@echo "Aggregating coverage files and generating HTML summary..."
	$(LCOV) $(LCOV_FLAGS) > /dev/null
	$(GENHTML) $(GENHTML_FLAGS) $(COVERAGE_INFO) > /dev/null
	@echo "Coverage HTML page result file://$(abs_builddir)/$(COVERAGE_DIR)/index.html"
else
	@echo "Code coverage not enabled; to enable:"
	@echo
	@echo "    ./configure --enable-coverage"
endif

clean-local: clean-coverage-local

clean-coverage-local:
	rm -f $(COVERAGE_INFO)

distclean-local: distclean-coverage-local

distclean-coverage-local:
	rm -rf $(COVERAGE_DIR)
