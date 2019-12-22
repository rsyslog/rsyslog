#!/bin/bash
# add 2019-09-03 by Philippe Duveau, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
# 804/805 fail/sent with space dedup
global(maxmessagesize="800")
module(load="../contrib/imbatchreport/.libs/imbatchreport" pollinginterval="1")
global(localhostname="server")
input(type="imbatchreport" ruleset="ruleset" tag="batch"
      severity="info" facility="local0"
      reports="./'$RSYSLOG_DYNNAME'*.done" deduplicatespace="on"
      programkey="KSH" timestampkey="START"
      delete=".done$ 	.rejected"
      )
ruleset(name="ruleset") {
   action(type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="RSYSLOG_SyslogProtocol23Format")
}
'
{
 echo '164313.149.tst-tmsm1!ARTIMPP_UDB.40042722.1.0: gtrid x0 x5624ee75 x1c88a0f: ECID <000001833E1D4i^5pVl3iY00f02B003UF^>: TRACE:at:    { tpcommit(0x0)'
 echo '164313.150.tst-tmsm1!ARTIMPP_UDB.40042721.1.0: gtrid x0 x5624ee75 x1c88a0f: TRACE:at:    } tpfree'
 echo '164313.151.tst-tmsm1!ARTIMPP_UDB.40042722.1.0: gtrid x0 x5624ee75 x1c88a0f: ECID <000001833E1D4i^5pVl3iY00f02B003UF^>: TRACE:at:    { tpcommit(0x0)'
 echo '164313.152.tst-tmsm1!ARTIMPP_UDB.40042722.1.0: gtrid x0 x5624ee75 x1c88a0f: ECID <000001833E1D4i^5pVl3iY00f02M003UF^>: TRACE:at:    } tpfree'
 echo '164313.153.tst-tmsm1!ARTIMPP_UDB.40042722.1.0: ECID <000001833E1D4i^5pVl3iY00f02M003VF^>: TRACE:at:    { tpacall("ARTIGW_SVC_REPLY_00700_02101", 0x110405698, 0, 0xc)'
 echo '164313.154.tst-tmsm1!ARTIMPP_UDB.40042722.1.0: ECID <000001833E1D4i^5pVl3iY00f02M003SF^>: TRACE:at:    } tpcommit = 1'
} > $RSYSLOG_DYNNAME.dtl.done
case $(uname) in
  FreeBSD)
    datelog=$(date -ur $(stat -f %m $RSYSLOG_DYNNAME.dtl.done) "+%Y-%m-%dT%H:%M:%S")
    ;;
  *)
    datelog=$(date -ud @$(stat -c "%Y" $RSYSLOG_DYNNAME.dtl.done) "+%Y-%m-%dT%H:%M:%S")
    ;;
esac
echo "Batch report to consume ${RSYSLOG_DYNNAME}.dtl.done for ${datelog}"
startup
shutdown_when_empty
wait_shutdown
export EXPECTED='<134>1 '${datelog}'.000000+00:00 server batch - - - File too large : ./'${RSYSLOG_DYNNAME}'.dtl.done'
cmp_exact
if [ -e $RSYSLOG_DYNNAME.dtl.done ] || [ -e $RSYSLOG_DYNNAME.dtl.sent ] || [ ! -e $RSYSLOG_DYNNAME.dtl.rejected ]; then
  echo "The batch report could not be renamed to rejected"
  ls $RSYSLOG_DYNNAME*
  error_exit  1
fi;

exit_test
