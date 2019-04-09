#!/bin/bash
# add 2019-03-09 by Philippe Duveau, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
module(load="../plugins/imfile/.libs/imfile")
module(load="../contrib/pmdb2diag/.libs/pmdb2diag")
input(type="imfile" ruleset="ruleset" tag="in:"
      File="./'$RSYSLOG_DYNNAME'.crio.input"
      Startmsg.regex="^[0-9]{4}-[0-9]{2}-[0-9]{2}"
      Escapelf="on" needparse="on")
template(name="test" type="string" string="time: %TIMESTAMP:::date-rfc3339%, level: %syslogseverity-text%, app: %app-name%, procid: %procid% msg: %msg%\n")
parser(type="pmdb2diag" timeformat="%Y-%m-%d-%H.%M.%S." timepos="0" levelpos="59" pidstarttoprogstartshift="49" name="db2.diag.cust")
ruleset(name="ruleset" parser="db2.diag.cust") {
        action(type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="test")
}
'
{
    echo '2015-05-06-16.53.26.989430+120 E1876227378A1702     LEVEL: Critical'
    echo 'PID     : 4390941              TID : 89500          PROC : db2sysc 0'
    echo 'INSTANCE: db2itst              NODE : 000           DB   : DBTST'
    echo '2015-05-06-16.53.26.989440+120 E1876227378A1702     LEVEL: Alert'
    echo 'PID     : 4390942              TID : 89500          PROC : db2sysc 0'
    echo 'INSTANCE: db2itst              NODE : 000           DB   : DBTST'
    echo '2015-05-06-16.53.26.989450+120 E1876227378A1702     LEVEL: Severe'
    echo 'PID     : 4390943              TID : 89500          PROC : db2sysc 0'
    echo 'INSTANCE: db2itst              NODE : 000           DB   : DBTST'
    echo '2015-05-06-16.53.26.989460+120 E1876227378A1702     LEVEL: Error'
    echo 'PID     : 4390944              TID : 89500          PROC : db2sysc 0'
    echo 'INSTANCE: db2itst              NODE : 000           DB   : DBTST'
    echo '2015-05-06-16.53.26.989470+120 E1876227378A1702     LEVEL: Warning'
    echo 'PID     : 4390945              TID : 89500          PROC : db2sysc 0'
    echo 'INSTANCE: db2itst              NODE : 000           DB   : DBTST'
    echo '2015-05-06-16.53.26.989480+120 E1876227378A1702     LEVEL: Event'
    echo 'PID     : 4390946              TID : 89500          PROC : db2sysc 0'
    echo 'INSTANCE: db2itst              NODE : 000           DB   : DBTST'
    echo '2015-05-06-16.53.26.989490+120 E1876227378A1702     LEVEL: Info'
    echo 'PID     : 4390947              TID : 89500          PROC : db2sysc 0'
    echo 'INSTANCE: db2itst              NODE : 000           DB   : DBTST'
    echo '2015-05-06-16.53.26.989500+120 E1876227378A1702     LEVEL: Debug'
    echo 'PID     : 4390948              TID : 89500          PROC : db2sysc 0'
    echo 'INSTANCE: db2itst              NODE : 000           DB   : DBTST'
    echo '2015-05-06-16.53.26.989510+120 E1876227378A1702     LEVEL: Info'
} > $RSYSLOG_DYNNAME.crio.input
startup
shutdown_when_empty
wait_shutdown
cmp - $RSYSLOG_OUT_LOG <<!end
time: 2015-05-06T16:53:26.989430+02:00, level: emerg, app: db2sysc, procid: 4390941 msg: 2015-05-06-16.53.26.989430+120 E1876227378A1702     LEVEL: Critical\nPID     : 4390941              TID : 89500          PROC : db2sysc 0\nINSTANCE: db2itst              NODE : 000           DB   : DBTST
time: 2015-05-06T16:53:26.989440+02:00, level: alert, app: db2sysc, procid: 4390942 msg: 2015-05-06-16.53.26.989440+120 E1876227378A1702     LEVEL: Alert\nPID     : 4390942              TID : 89500          PROC : db2sysc 0\nINSTANCE: db2itst              NODE : 000           DB   : DBTST
time: 2015-05-06T16:53:26.989450+02:00, level: crit, app: db2sysc, procid: 4390943 msg: 2015-05-06-16.53.26.989450+120 E1876227378A1702     LEVEL: Severe\nPID     : 4390943              TID : 89500          PROC : db2sysc 0\nINSTANCE: db2itst              NODE : 000           DB   : DBTST
time: 2015-05-06T16:53:26.989460+02:00, level: err, app: db2sysc, procid: 4390944 msg: 2015-05-06-16.53.26.989460+120 E1876227378A1702     LEVEL: Error\nPID     : 4390944              TID : 89500          PROC : db2sysc 0\nINSTANCE: db2itst              NODE : 000           DB   : DBTST
time: 2015-05-06T16:53:26.989470+02:00, level: warning, app: db2sysc, procid: 4390945 msg: 2015-05-06-16.53.26.989470+120 E1876227378A1702     LEVEL: Warning\nPID     : 4390945              TID : 89500          PROC : db2sysc 0\nINSTANCE: db2itst              NODE : 000           DB   : DBTST
time: 2015-05-06T16:53:26.989480+02:00, level: notice, app: db2sysc, procid: 4390946 msg: 2015-05-06-16.53.26.989480+120 E1876227378A1702     LEVEL: Event\nPID     : 4390946              TID : 89500          PROC : db2sysc 0\nINSTANCE: db2itst              NODE : 000           DB   : DBTST
time: 2015-05-06T16:53:26.989490+02:00, level: info, app: db2sysc, procid: 4390947 msg: 2015-05-06-16.53.26.989490+120 E1876227378A1702     LEVEL: Info\nPID     : 4390947              TID : 89500          PROC : db2sysc 0\nINSTANCE: db2itst              NODE : 000           DB   : DBTST
time: 2015-05-06T16:53:26.989500+02:00, level: debug, app: db2sysc, procid: 4390948 msg: 2015-05-06-16.53.26.989500+120 E1876227378A1702     LEVEL: Debug\nPID     : 4390948              TID : 89500          PROC : db2sysc 0\nINSTANCE: db2itst              NODE : 000           DB   : DBTST
!end

if [ ! $? -eq 0 ]; then
  echo "invalid response generated, $RSYSLOG_OUT_LOG is:"
  cat $RSYSLOG_OUT_LOG
  error_exit  1
fi;

exit_test
