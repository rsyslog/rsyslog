# very rough support for compiling the java components of rsyslog
# Some usage notes: you need to use the Sun JDK compiler (jdk-devel)
# with this. At least it didn't work for me with the eclipse compiler.
# There is no real installation support. If you intend to run a program,
# change to the ./java subdirectory and issue
# java -cp . <class>
# e.g.: java -cp . com.rsyslog.gui.diaggui.DiagGUI
# or any equivalent command.
#
# I am very glad to hear suggestions about how to improve this part
# of the build system. -- rgerhards, 2009-08-27

javadir = $(top_builddir)/java
JAVAROOT = $(javadir)
# I don't know why CLASSPATH_ENV works this way, but at least it works...
CLASSPATH_ENV = CLASSPATH=$(javadir):$$CLASSPATH

JAVA_SOURCE_FILES = \
	com/rsyslog/lib/DiagSess.java \
	com/rsyslog/lib/SyslogMessage.java \
	com/rsyslog/lib/SyslogMsgConsumer.java \
	com/rsyslog/lib/SyslogTrafficGenerator.java \
	com/rsyslog/lib/SyslogSender.java \
	com/rsyslog/lib/UDPSyslogSender.java \
	com/rsyslog/diag/DiagTalker.java \
	com/rsyslog/gui/simpServ/simpServ.java \
	com/rsyslog/gui/simpServ/simpServConsumer.java \
	com/rsyslog/gui/msggen/MsgGen.java \
	com/rsyslog/gui/diaggui/Counters.java \
	com/rsyslog/gui/diaggui/DiagGUI.java


java_JAVA = $(JAVA_SOURCE_FILES)

dist_java = $(JAVA_SOURCE_FILES)
