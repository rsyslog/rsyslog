/** A syslog message consumer for the simple syslog server.
 * 
 * @author Rainer Gerhards
 * 
 * Copyright 2009 Rainer Gerhards and Adiscon GmbH.
 *
 * This file is part of rsyslog.
 *
 * Rsyslog is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Rsyslog is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Rsyslog.  If not, see http://www.gnu.org/licenses/.
 *
 * A copy of the GPL can be found in the file "COPYING" in this distribution.
 */
package com.rsyslog.gui.simpServ;
import com.rsyslog.lib.*;

class simpServConsumer implements SyslogMsgConsumer {
	public void consumeMsg(String ln) {
		SyslogMessage msg = new SyslogMessage(ln);
		System.out.println("Line received '" + msg.getRawMsgAfterPRI() + "'\n");
	}
}
