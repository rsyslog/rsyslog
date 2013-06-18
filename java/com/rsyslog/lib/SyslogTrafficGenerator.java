/**
 * This class is a syslog traffic generator. It is primarily intended to be used
 * together with testing tools, but may have some use cases outside that domain.
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

public class SyslogTrafficGenerator extends Thread {

	/** the target host to receive traffic */
	private String target;

	/** the message (template) to be sent */
	private String message;

	/** number of messages to be sent */
	private long nummsgs;

	/** Constructs Sender, sets target system.
	 * @param target the system to connect to. Syntax of target is depending
	 * 		 on the underlying transport.
	 */
	public SyslogTrafficGenerator(String target, String message, long nummsgs) {
		this.target = target;
		this.message = message;
		this.nummsgs = nummsgs;
	}

	/** Generates the traffic. Stops when either called to terminate
	 * or the max number of messages have been sent. Note that all
	 * necessary properties must have been set up before starting the
	 * generator thread!
	 */
	private void performTest() throws Exception {
		int doDisp = 0;
		UDPSyslogSender sender = new UDPSyslogSender(target);
		for(long i = 0 ; i < nummsgs ; ++i) {
			sender.sendMSG(message + " " + Long.toString(i) + " " + this.toString() + "\0");
			if((doDisp++ % 1000) == 0)
				System.out.println(this.toString() + " send message " + Long.toString(i));
			sleep(1);
		}	
	}


/** Wrapper around the real traffic generator, catches exceptions.
 */
	public void run() {
		System.out.println("traffic generator " + this.toString() + " thread started");
		try {
			performTest();
		}
		catch(Exception e) {
			/* at some time, we may find a more intelligent way to
			 * handle this! ;)
			 */
			System.out.println(e.toString());
		}
		System.out.println("traffic generator " + this.toString() + " thread finished");
	}
}
