#!/bin/bash
# add 2019-09-03 by Philippe Duveau, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
module(load="../contrib/imtuxedoulog/.libs/imtuxedoulog")
input(type="imtuxedoulog" ruleset="ruleset"
      severity="info" facility="local0"
      maxlinesatonce="100" persiststateinterval="10"
      maxsubmitatonce="100" tag="domain"
      ulogbase="./'$RSYSLOG_DYNNAME'")
ruleset(name="ruleset") {
        action(type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="RSYSLOG_SyslogProtocol23Format")
}
'
{
 echo '164313.15.tst-tmsm1!ARTIMPP_UDB.40042721.1.0: gtrid x0 x5624ee75 x1c88a0f: TRACE:at:    } tpfree'
 echo '164313.151.tst-tmsm1!ARTIMPP_UDB.40042722.1.0: gtrid x0 x5624ee75 x1c88a0f: ECID <000001833E1D4i^5pVl3iY00f02M003UF^>: TRACE:at:    } tpfree'
 echo '164313.152.tst-tmsm1!ARTIMPP_UDB.40042722.1.0: gtrid x0 x5624ee75 x1c88a0f: ECID <000001833E1D4i^5pVl3iY00f02B003UF^>: TRACE:at:    { tpcommit(0x0)'
 echo '164313.153.tst-tmsm1!ARTIMPP_UDB.40042722.1.0: ECID <000001833E1D4i^5pVl3iY00f02M003SF^>: TRACE:at:    } tpcommit = 1'
 echo '164313.154.tst-tmsm1!ARTIMPP_UDB.40042722.1.0: ECID <000001833E1D4i^5pVl3iY00f02M003VF^>: TRACE:at:    { tpacall("ARTIGW_SVC_REPLY_00700_02101", 0x110405698, 0, 0xc)'
} > $RSYSLOG_DYNNAME.$(date "+%m%d%y")
logdate=$(date "+%Y-%m-%d")
startup
shutdown_when_empty
wait_shutdown
{
echo '<134>1 '${logdate}'T16:43:13.15 tst-tmsm1 domain ARTIMPP_UDB.40042721.1 - - TRACE:at:    } tpfree'
echo '<134>1 '${logdate}'T16:43:13.151 tst-tmsm1 domain ARTIMPP_UDB.40042722.1 - [ECID="000001833E1D4i^5pVl3iY00f02M003UF^"]  TRACE:at:    } tpfree'
echo '<134>1 '${logdate}'T16:43:13.152 tst-tmsm1 domain ARTIMPP_UDB.40042722.1 - [ECID="000001833E1D4i^5pVl3iY00f02B003UF^"]  TRACE:at:    { tpcommit(0x0)'
echo '<134>1 '${logdate}'T16:43:13.153 tst-tmsm1 domain ARTIMPP_UDB.40042722.1 - [ECID="000001833E1D4i^5pVl3iY00f02M003SF^"]  TRACE:at:    } tpcommit = 1'
echo '<134>1 '${logdate}'T16:43:13.154 tst-tmsm1 domain ARTIMPP_UDB.40042722.1 - [ECID="000001833E1D4i^5pVl3iY00f02M003VF^"]  TRACE:at:    { tpacall("ARTIGW_SVC_REPLY_00700_02101", 0x110405698, 0, 0xc)'
} | cmp - $RSYSLOG_OUT_LOG
if [ ! $? -eq 0 ]; then
  echo "invalid response generated, $RSYSLOG_OUT_LOG is:"
  cat $RSYSLOG_OUT_LOG
  error_exit  1
fi;

exit_test
