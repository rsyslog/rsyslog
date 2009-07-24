package com.rsyslog.gui.simpServ;
import com.rsyslog.lib.*;
import com.rsyslog.gui.*;

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
