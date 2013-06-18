/**
 * Implementation of a tcp-based syslog server.
 *
 * This is a limited-capability implementation of a syslog tcp server.
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

import com.rsyslog.lib.SyslogMsgConsumer;
import java.io.*;
import java.net.*;


/** a small test consumer */
/*
class TestConsumer implements SyslogMsgConsumer {
	public void consumeMsg(String ln) {
		System.out.println("Line received '" + ln + "'\n");
	}
}
*/

public class SyslogServerTCP extends Thread {
	private ServerSocket lstnSock;
	private boolean contRun;	/* continue processing requests? */
	public SyslogMsgConsumer consumer;

	/** Process a single connection */
	class Session extends Thread {
		private Socket sock;
		private SyslogServerTCP srvr;

		public Session(Socket so, SyslogServerTCP _srvr) {
			sock = so;
			srvr = _srvr;
		}

		public void run() {
			try {
				BufferedReader data = new BufferedReader(
					new InputStreamReader(sock.getInputStream()));

				String ln = data.readLine();
				while(ln != null) {
					srvr.getConsumer().consumeMsg(ln);
					ln = data.readLine();
				}
				System.out.println("End of Session.\n");
				sock.close();
			}
			catch(Exception e) {
				/* we ignore any errors we may have... */
				System.out.println("Session exception " + e.toString());
			}
		}
	}

	/** a small test driver */
/*
	public static void main(String args[]) {
		try {
			SyslogMsgConsumer cons = new TestConsumer();
			System.out.println("Starting server on port " + args[0] + "\n");
			SyslogServerTCP myServ = new
				SyslogServerTCP(Integer.parseInt(args[0]), cons);
			myServ.start();
			System.out.println("Press ctl-c to terminate\n");
		}
		catch(Exception e) {
			System.out.println("Fehler! " + e.toString());
		}
	}
*/

	public SyslogServerTCP(int port, SyslogMsgConsumer cons) throws java.io.IOException {
		if(lstnSock != null)
			terminate();
		lstnSock = new ServerSocket(port);
		consumer = cons;
		contRun = true;
	}

	public void terminate() {
		contRun = false;
	}

	public SyslogMsgConsumer getConsumer() {
		return consumer;
	}

	public void run() {
		try {
			while(contRun) {
				Socket sock = lstnSock.accept();
				System.out.println("New connection request! " + sock.toString());
				Thread sess = new Session(sock, this);
				sock = null;
				sess.start();
			}
		}
		catch(Exception e) {
			System.out.println("Error during server run " + e.toString());
		}
			
	}
}
