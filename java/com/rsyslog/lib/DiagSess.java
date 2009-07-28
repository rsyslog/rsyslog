/* The diagnostic session to an imdiag module (running inside rsyslogd).
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
package com.rsyslog.lib;
import java.io.*;
import java.net.*;

public class DiagSess {

	private String host = new String("127.0.0.1");
	private int port = 13500;
	int timeout = 0;
	private PrintWriter out = null;
    private BufferedReader in = null;
	private Socket diagSocket = null;

	/** set connection timeout */
	public void setTimeout(int timeout_) {
		timeout = timeout_;
	}
	
	public DiagSess(String host_, int port_) {
		host = host_;
		port = port_;
	}
	
	/** connect to remote server. Initializes everything for request-reply
	 * processing.
	 * 
	 * @throws IOException
	 */
	public void connect() throws IOException {
        diagSocket = new Socket(host, port);
	    diagSocket.setSoTimeout(timeout);
        out = new PrintWriter(diagSocket.getOutputStream(), true);
        in = new BufferedReader(new InputStreamReader(
                                    diagSocket.getInputStream()));
	
	}
	
	/** end session with remote server. */
	public void disconnect() throws IOException {
		out.close();
		in.close();
		diagSocket.close();
	}

	/** issue a request to imdiag and return its response.
	 * 
	 * @param req request string
	 * @return response string (unparsed)
	 * @throws IOException
	 */
	public String request(String req) throws IOException {
	    out.println(req);
	    String resp = in.readLine();
	    return resp;
	}
	
}
