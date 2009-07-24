package com.rsyslog.lib;

public interface SyslogMsgConsumer {
	public void consumeMsg(String msg);
}
