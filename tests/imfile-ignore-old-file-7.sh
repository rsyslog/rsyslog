#! /bin/bash
# Read a file and ignore another one from an old symlink
echo [imfile-ignore-old-file-7.sh]
. ${srcdir:=.}/diag.sh init
. $srcdir/diag.sh check-inotify
generate_conf
add_conf '
module(load="../plugins/imfile/.libs/imfile")
input(type="imfile"
      File="./symlink/'$RSYSLOG_DYNNAME'.input.ignore3.*"
      ignoreolderthan="86400"
      Tag="file:"
      ruleset="ruleset")
template(name="outfmt" type="string" string="%msg:F,58:2%\n")
ruleset(name="ruleset") {
	action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
}
'

# create a log for testing
mkdir ./source
./inputfilegen -m 1000 > ./source/${RSYSLOG_DYNNAME}.input.ignore3.1
touch -m -t 201806010000.00 ./source/${RSYSLOG_DYNNAME}.input.ignore3.1

./inputfilegen -m 1000 -i 1000 > ./source/${RSYSLOG_DYNNAME}.input.ignore3.2

echo "source file date: "
ls -alth ./source/${RSYSLOG_DYNNAME}.input.ignore3.* #DEBUG

# create symlink
ln -sf ./source/ ./symlink

# apply old date to symlink
touch -h -t 201806010000.00 ./symlink

echo "symlink date"
ls -alth ./symlink #DEBUG

echo "File dates:"
ls -alth ./symlink/${RSYSLOG_DYNNAME}.input.ignore3.* #DEBUG

startup
# sleep a little to give rsyslog a chance to begin processing
./msleep 1000
# shut down rsyslogd when done processing messages
shutdown_when_empty 
# we need to wait until rsyslogd is finished!
wait_shutdown
# check log file
seq_check 1000 1999

# cleanup
unlink ./symlink
rm -rf ./source

exit_test
