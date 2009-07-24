/** A syslog message consumer for the simple syslog server. */
package com.rsyslog.gui.simpServ;
import com.rsyslog.lib.*;

class simpServConsumer implements SyslogMsgConsumer {
	public void consumeMsg(String ln) {
		SyslogMessage msg = new SyslogMessage(ln);
		System.out.println("Line received '" + msg.getRawMsgAfterPRI() + "'\n");
	}
}
