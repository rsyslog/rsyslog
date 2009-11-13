/**
 * Implementation of a tcp-based syslog server.
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
//import com.rsyslog.gui.*;

public class simpServ {

	public static void main(String args[]) {
		try {
			simpServConsumer cons = new simpServConsumer();
			System.out.println("Starting server on port " + args[0] + "\n");
			SyslogServerTCP myServ = new
				SyslogServerTCP(Integer.parseInt(args[0]), cons);
			myServ.start();
			System.out.println("Press ctl-c to terminate\n");
		}
		catch(Exception e) {
			System.out.println("Error: " + e.toString());
		}
	}
}
