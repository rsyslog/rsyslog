#!/bin/bash
# This test creates multiple symlinks (all watched by rsyslog via wildcard)
# chained to target files via additional symlinks and checks that all files
# are recorded with correct corresponding metadata (name of symlink 
# matching configuration).
# This is part of the rsyslog testbench, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
. $srcdir/diag.sh check-inotify

# generate input files first. Note that rsyslog processes it as
# soon as it start up (so the file should exist at that point).
generate_conf
add_conf '
# comment out if you need more debug info:
	global( debug.whitelist="on"
		debug.files=["imfile.c"])
module(load="../plugins/imfile/.libs/imfile"
       mode="inotify")
input(type="imfile" File="'$RSYSLOG_DYNNAME'/var/log/containers/*.log" Tag="container")
if $msg contains "msgnum:" then
	action( type="omfile" file="'${RSYSLOG_OUT_LOG}'")
'

mkdir $RSYSLOG_DYNNAME
mkdir $RSYSLOG_DYNNAME/var
mkdir $RSYSLOG_DYNNAME/var/lib
mkdir $RSYSLOG_DYNNAME/var/lib/docker
mkdir $RSYSLOG_DYNNAME/var/lib/docker/containers
mkdir $RSYSLOG_DYNNAME/var/lib/docker/containers/8c3dc2b62b9ba97be7650c6e96d50f4edfcc05a7fb37bc6914f8d8143b8c2b1e
./inputfilegen -m 1 > $RSYSLOG_DYNNAME/var/lib/docker/containers/8c3dc2b62b9ba97be7650c6e96d50f4edfcc05a7fb37bc6914f8d8143b8c2b1e/8c3dc2b62b9ba97be7650c6e96d50f4edfcc05a7fb37bc6914f8d8143b8c2b1e-json.log
mkdir $RSYSLOG_DYNNAME/var/log
mkdir $RSYSLOG_DYNNAME/var/log/pods
mkdir $RSYSLOG_DYNNAME/var/log/pods/f3f1ec70-f942-11e8-8bdc-5254008a6103
mkdir $RSYSLOG_DYNNAME/var/log/pods/f3f1ec70-f942-11e8-8bdc-5254008a6103/coremedia-pipeline-developmentmcaefeeder-mysql
ln -s $RSYSLOG_DYNNAME/var/lib/docker/containers/8c3dc2b62b9ba97be7650c6e96d50f4edfcc05a7fb37bc6914f8d8143b8c2b1e/8c3dc2b62b9ba97be7650c6e96d50f4edfcc05a7fb37bc6914f8d8143b8c2b1e-json.log $RSYSLOG_DYNNAME/var/log/pods/f3f1ec70-f942-11e8-8bdc-5254008a6103/coremedia-pipeline-developmentmcaefeeder-mysql/0.log
mkdir $RSYSLOG_DYNNAME/var/log/containers
ln -s $RSYSLOG_DYNNAME/var/log/pods/f3f1ec70-f942-11e8-8bdc-5254008a6103/coremedia-pipeline-developmentmcaefeeder-mysql/0.log $RSYSLOG_DYNNAME/var/log/containers/coremedia-pipeline-developmentmcaefeeder-mysql-6556c49554-zzzzx_coremedia-pipeline-development_coremedia-pipeline-developmentmcaefeeder-mysql-8c3dc2b62b9ba97be7650c6e96d50f4edfcc05a7fb37bc6914f8d8143b8c2b1e.log

# Start rsyslog now before adding more files
startup

# Content check with timeout
content_check "HEADER msgnum:00000000:"

shutdown_when_empty
wait_shutdown
exit_test
