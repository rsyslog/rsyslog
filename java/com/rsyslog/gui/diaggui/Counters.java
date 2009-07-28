/* Display some basic rsyslogd counter variables.
 * 
 * Please note that this program requires imdiag to be loaded inside rsyslogd.
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
package com.rsyslog.gui.diaggui;
import java.awt.*;
import java.awt.event.*;
import java.io.*;
import java.util.*;

import com.rsyslog.lib.DiagSess;

public class Counters extends Frame {
	
	private TextField MainQItems;
	private TextField RefreshInterval;
	private Checkbox  AutoRefresh;
	private DiagSess diagSess;
	private Timer timer;

	private void createDiagSess() {
		try {
			diagSess = new DiagSess("127.0.0.1", 13500); // TODO: values from GUI
			diagSess.connect();
		}
		catch(IOException e) {
			System.out.println("Exception trying to open diag session:\n" + e.toString());
		}
	}

	private void createGUI() {
		MainQItems = new TextField();
		MainQItems.setColumns(8);
		Panel pCenter = new Panel();
		pCenter.setLayout(new FlowLayout());
		pCenter.add(new Label("MainQ Items:"));
		pCenter.add(MainQItems);
		
		RefreshInterval = new TextField();
		RefreshInterval.setColumns(5);
		RefreshInterval.setText("100");
		AutoRefresh = new Checkbox("Auto Refresh", false);
		AutoRefresh.addItemListener(new ItemListener() {
			public void itemStateChanged(ItemEvent e) {
				setAutoRefresh();
			}
			
		});
		Button b = new Button("Refresh now");
		b.addActionListener(new ActionListener() {
			public void actionPerformed(ActionEvent e) {
				refreshCounters();
			}
		});
		Panel pSouth = new Panel();
		pSouth.setLayout(new FlowLayout());
		pSouth.add(AutoRefresh);
		pSouth.add(new Label("Interval (ms):"));
		pSouth.add(RefreshInterval);
		pSouth.add(b);
		
		pack();
		setTitle("rsyslogd Counters");
		setLayout(new BorderLayout());
		add(pCenter, BorderLayout.CENTER);
		add(pSouth, BorderLayout.SOUTH);
		setSize(400,500);
	}
	
	public Counters() {
		addWindowListener(new WindowAdapter() {
			public void windowClosing(WindowEvent e){
				Counters.this.dispose();
			}
		});
		createGUI();
		createDiagSess();
		setAutoRefresh();
		setVisible(true);
	}


	private void startTimer() {
		timer = new Timer();
		timer.scheduleAtFixedRate(new TimerTask() {
			public void run() {
				refreshCounters();
			}
		}, 0, 100);
	}
	
	private void stopTimer() {
		if(timer != null) {
			timer.cancel();
			timer = null;
		}
	}

	/** set auto-refresh mode. It is either turned on or off, depending on the 
	 *  status of the relevant check box. */
	private void setAutoRefresh() {
		if(AutoRefresh.getState() == true) {
			startTimer();
		} else {
			stopTimer();
		}
	}
	
	/** refresh counter display from rsyslogd data. Does a network round-trip. */
	private void refreshCounters() {
		try {
			String res = diagSess.request("getmainmsgqueuesize");
			MainQItems.setText(res);
		}
		catch(IOException e) {
			System.out.println("Exception during request:\n" + e.toString());
		}
	}
}
