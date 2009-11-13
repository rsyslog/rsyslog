/**
 * Implementation of the syslog message object.
 *
 * This is a limited-capability implementation of a syslog message together
 * with all its properties. It is limit to what is currently needed and may
 * be extended as further need arises.
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
package com.rsyslog.lib;

public class SyslogMessage {

	/** message as received from the wire */
	private String rawmsg;
	/** the rawmsg without the PRI part */
	private String rawMsgAfterPRI;
	/** PRI part */
	private int pri;

	/** a very simple syslog parser. So far, it only parses out the
	 * PRI part of the message. May be extended later. Rawmsg must have
	 * been set before the parser is called. It will populate "all" other
	 * fields.
	 */
	private void parse() {
		int i;
		if(rawmsg.charAt(0) == '<') {
			pri = 0;
			for(i = 1 ; Character.isDigit(rawmsg.charAt(i)) && i < 4 ; ++i) {
				pri = pri * 10 + rawmsg.charAt(i) - '0';
			}
			if(rawmsg.charAt(i) != '>')
				/* not a real cure, but sufficient for the current
				 * mini-parser... */
				--i;
			rawMsgAfterPRI = rawmsg.substring(i + 1);
		} else {
			pri = 116;
			rawMsgAfterPRI = rawmsg;
		}
	}

	public SyslogMessage(String _rawmsg) {
		rawmsg = _rawmsg;
		parse();
	}

	public String getRawMsg() {
		return rawmsg;
	}

	public String getRawMsgAfterPRI() {
		return rawMsgAfterPRI;
	}
}
