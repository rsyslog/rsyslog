//package com.rsyslog.diag;
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

	while ((userInput = stdIn.readLine()) != null) {
	    out.println(userInput);
	    System.out.println("imdiag returns: " + in.readLine());
	}

	out.close();
	in.close();
	stdIn.close();
	diagSocket.close();
    }
}

