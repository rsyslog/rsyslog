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
