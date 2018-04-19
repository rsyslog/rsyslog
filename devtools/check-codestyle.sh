#!/bin/bash
# this checks the rsyslog codestyle. It expects that
# rsyslog_stylecheck
# is already installed inside the system
# if in doubt, run it on one of the development containers
set -e
find -name "*.[ch]" | xargs rsyslog_stylecheck -w -f -l 120
# Note: we do stricter checks for some code sources that have been
# sufficiently cleaned up.
rsyslog_stylecheck -l 120 compat/ifaddrs.h \
	tests/override_getaddrinfo.c \
	tests/ourtail.c \
	tests/filewriter.c \
	tests/omrelp_dflt_port.c \
	tests/chkseq.c \
	tests/getline.c \
	tests/rt-init.c \
	tests/override_gethostname_nonfqdn.c \
	tests/inputfilegen.c \
	tests/msleep.c \
	tests/testconfgen.c \
	plugins/imfile/imfile.c \
	plugins/imrelp/imrelp.c \
	plugins/omrelp/omrelp.c \
	plugins/external/skeletons/C/c_sample.c \
	plugins/imsolaris/imsolaris.h \
	plugins/imsolaris/sun_cddl.h \
	plugins/imklog/solaris_cddl.h \
	grammar/testdriver.c \
	runtime/netstrms.h \
	runtime/lib_ksi_queue.h \
	runtime/var.c \
	runtime/glbl.h \
	runtime/nsdsel_ptcp.h \
	runtime/strgen.h \
	runtime/rsyslog.h \
	runtime/strgen.c \
	runtime/dynstats.h \
	runtime/nsdpoll_ptcp.h \
	runtime/im-helper.h \
	runtime/queue.h
