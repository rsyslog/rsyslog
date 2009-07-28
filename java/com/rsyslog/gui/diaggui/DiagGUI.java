/* A yet very simple diagnostic GUI for rsyslog.
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

public class DiagGUI extends Frame {
	public Counters counterWin;
	public static void main(String args[]) {
		new DiagGUI();
	}
	
	/** show counter window. creates it if not already present */
	public void showCounters() {
		if(counterWin == null) {
			counterWin = new Counters();
		} else {
			counterWin.setVisible(true);
			counterWin.toFront();
		}
	}

		/** initialize the GUI. */
	public DiagGUI(){
		MenuItem item;
		MenuBar menuBar = new MenuBar();
		Menu fileMenu = new Menu("File");
		item = new MenuItem("Exit");
		item.addActionListener(new ActionListener() {
			public void actionPerformed(ActionEvent e) {
				System.exit(0);
			}
		});
		fileMenu.add(item);
		menuBar.add(fileMenu);

		Menu viewMenu = new Menu("View");
		item = new MenuItem("Counters");
		item.addActionListener(new ActionListener() {
			public void actionPerformed(ActionEvent e) {
				showCounters();
			}
		});
		viewMenu.add(item);
		menuBar.add(viewMenu);

		addWindowListener(new WindowAdapter() {
			public void windowClosing(WindowEvent e){
				System.exit(0);
			}
		});
		setMenuBar(menuBar);
		setSize(300,400);
		setVisible(true);
	}
}
