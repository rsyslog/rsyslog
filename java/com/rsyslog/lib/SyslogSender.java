/**
 * This class specifies all methods common to syslog senders. It also implements
 * some generic ways to send data. Actual syslog senders (e.g. UDP, TCP, ...) shall
 * be derived from this class.
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

public abstract class SyslogSender {

	/** the rawmsg without the PRI part */
	private String target;

	/** the rawmsg without the PRI part */
	private boolean isConnected = false;

	/** Constructs Sender, sets target system.
	 * @param target the system to connect to. Syntax of target is depending
	 * 		 on the underlying transport.
	 */
	public SyslogSender(String target) {
		this.target = target;
	}


	/** send a message on the wire.
	 * This needs a complete formatted message, which will be extended by
	 * the transport framing, if necessary.
	 *
	 * @param MSG a validly formatted syslog message as of the RFC (all parts)
	 * @throws Exception (depending on transport)
	 */
	protected abstract void sendTransport(String MSG) throws Exception;

	/** send an alread-formatted message.
	 * Sends a preformatted syslog message payload to the target. Connects
	 * to the target if not already connected.
	 *
	 * @param MSG a validly formatted syslog message as of the RFC (all parts)
	 * @throws Exception (depending on transport)
	 */
	public void sendMSG(String MSG) throws Exception {
		if(!isConnected)
			connect();
		sendTransport(MSG);
	}

	/** connect to the target. 
	 * Note that this may be a null operation if there is no session-like entity
	 * in the underlying transport (as is for example in UDP).
	 */
	public void connect() throws Exception {
		/* the default implementation does (almost) nothing */
		isConnected = true;
	}

	/** disconnects from the target. 
	 * Note that this may be a null operation if there is no session-like entity
	 * in the underlying transport (as is for example in UDP).
	 */
	public void disconnect() {
		/* the default implementation does (almost) nothing */
		isConnected = false;
	}

	/** return target of this Sender.
	 * @returns target as initially set
	 */
	public String getTarget() {
		return target;
	}
}
