/**
 * A UDP transport implementation of a syslog sender.
 *
 * Note that there is an anomaly in this version of the code: we query the remote system
 * address only once during the connection setup and resue it. If we potentially run for
 * an extended period of time, the remote address may change, what we do not reflect. For
 * the current use case, this is acceptable, but if this code is put into more wide-spread
 * use outside of debugging, a periodic requery should be added.
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
import java.net.*;

public class UDPSyslogSender extends SyslogSender {

	private final int port = 514; // TODO: take from target!
	private InetAddress targetAddr;

	/** the socket to communicate over with the remote system. */
	private DatagramSocket sock;

	/** Constructs Sender, sets target system.
	 * @param target the system to connect to. Syntax of target is depending
	 * 		 on the underlying transport.
	 */
	public UDPSyslogSender(String target) throws Exception {
		super(target);
	}

	/** send a message on the wire.
	 * This needs a complete formatted message, which will be extended by
	 * the transport framing, if necessary.
	 *
	 * @param MSG a validly formatted syslog message as of the RFC (all parts)
	 * @throws Exception (depending on transport)
	 */
	protected void sendTransport(String MSG) throws Exception {
		byte msg[] = MSG.getBytes();
		DatagramPacket pkt = new DatagramPacket(msg, msg.length, targetAddr, port);
		sock.send(pkt);
	}


	/** connect to the target. 
	 * For UDP, this means we create the socket.
	 */
	public void connect() throws Exception {
		super.connect();
		sock = new DatagramSocket();

		// TODO: we should extract the actual hostname & port!
		targetAddr = InetAddress.getByName(getTarget());
	}

}
