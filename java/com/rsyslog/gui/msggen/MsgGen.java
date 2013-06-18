/* A yet very simple syslog message generator
 * 
 * The purpose of this program is to provide a facility that enables
 * to generate complex traffic patterns for testing purposes. It still is
 * in its infancy, but hopefully will evolve.
 *
 * Note that this has been created as a stand-alone application because it
 * was considered useful to have it as a separate program. But it should still
 * be possible to call its class from any other program, specifically the debug
 * GUI.
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
package com.rsyslog.gui.msggen;
import com.rsyslog.lib.*;
import java.awt.*;
import java.awt.event.*;
import javax.swing.*;

public class MsgGen extends Frame {
	private TextField target;
	private TextField message;
	private TextField nummsgs;
	private TextField numthrds;

	public static void main(String args[]) {
		new MsgGen();
	}
	
	/** creates the menu bar INCLUDING all menu handlers */
	private void createMenu() {
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

		addWindowListener(new WindowAdapter() {
			public void windowClosing(WindowEvent e){
				System.exit(0);
			}
		});
		setMenuBar(menuBar);
	}

	/** creates the main GUI */
	private void createGUI() {
		//target = new TextField("127.0.0.1", 32);
		target = new TextField("172.19.3.3", 32);
		message = new TextField(80);
		//message.setText("<161>Test malformed");
		message.setText("<5>iaalog[171652]: AIB|dcu|2009/08/12 14:48:43|mfa challenge|NNNNNNN|XX.XX.XX.XX");
		nummsgs = new TextField("1000", 8);
		numthrds = new TextField("10", 5);
		Panel pCenter = new Panel();

		Panel pnl = new Panel();
		pnl.setLayout(new FlowLayout());
		pnl.add(new Label("Target Host:"));
		pnl.add(target);
		pCenter.add(pnl);

		pnl = new Panel();
		pnl.setLayout(new FlowLayout());
		pnl.add(new Label("Number of Msgs:"));
		pnl.add(nummsgs);
		pCenter.add(pnl);

		pnl = new Panel();
		pnl.setLayout(new FlowLayout());
		pnl.add(new Label("Msg:"));
		pnl.add(message);
		pCenter.add(pnl);

		Panel pSouth = new Panel();
		pSouth.setLayout(new FlowLayout());

		pnl = new Panel();
		pnl.setLayout(new FlowLayout());
		pnl.add(new Label("Number of Threads:"));
		pnl.add(numthrds);
		pSouth.add(pnl);

		Button b = new Button("Start Test");
		b.addActionListener(new ActionListener() {
			public void actionPerformed(ActionEvent e) {
				performTest();
			}
		});
		pSouth.add(b);

		pack();
		setTitle("Syslog Message Generator");
		setLayout(new BorderLayout());
		add(pCenter, BorderLayout.CENTER);
		add(pSouth, BorderLayout.SOUTH);
		setSize(800,400);
	}

	/** perform the test, a potentially complex operation */
	private void performTest() {
		for(short i = 0 ; i < Integer.parseInt(numthrds.getText()) ; ++ i) {
			SyslogTrafficGenerator gen = 
				new SyslogTrafficGenerator(target.getText(), message.getText(),
					Long.parseLong(nummsgs.getText()));
			gen.start();
		}
	}


	/** initialize the GUI. */
	public MsgGen(){
		createMenu();
		createGUI();
		setVisible(true);
	}
}
