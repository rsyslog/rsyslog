/* A yet very simple tool to talk to imdiag.
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
 * along with Rsyslog.  If not, see <http://www.gnu.org/licenses/>.
 *
 * A copy of the GPL can be found in the file "COPYING" in this distribution.
 */
package com.rsyslog.diag;
import java.io.*;
import java.net.*;

public class DiagTalker {
    public static void main(String[] args) throws IOException {

        Socket diagSocket = null;
        PrintWriter out = null;
        BufferedReader in = null;
	final String host = "127.0.0.1";
	final int port = 13500;

        try {
            diagSocket = new Socket(host, port);
	    diagSocket.setSoTimeout(0); /* wait for lenghty operations */
            out = new PrintWriter(diagSocket.getOutputStream(), true);
            in = new BufferedReader(new InputStreamReader(
                                        diagSocket.getInputStream()));
        } catch (UnknownHostException e) {
            System.err.println("can not resolve " + host + "!");
            System.exit(1);
        } catch (IOException e) {
            System.err.println("Couldn't get I/O for "
                               + "the connection to: " + host + ".");
            System.exit(1);
        }

	BufferedReader stdIn = new BufferedReader(
                                   new InputStreamReader(System.in));
	String userInput;

	try {
		while ((userInput = stdIn.readLine()) != null) {
		    out.println(userInput);
		    System.out.println("imdiag returns: " + in.readLine());
		}
        } catch (SocketException e) {
            System.err.println("We had a socket exception and consider this to be OK: "
	    			+ e.getMessage());
	}

	out.close();
	in.close();
	stdIn.close();
	diagSocket.close();
    }
}

