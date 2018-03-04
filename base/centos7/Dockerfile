FROM	centos:7
LABEL   maintainer="rgerhards@adiscon.com"
RUN     yum -y install wget \
	&& cd /etc/yum.repos.d/ \
        && wget http://rpms.adiscon.com/v8-stable/rsyslog.repo
RUN	yum -y install rsyslog \
	   rsyslog-elasticsearch \
	   rsyslog-imptcp \
	   rsyslog-imrelp \
	   rsyslog-mmjsonparse \
	   rsyslog-omrelp \
	   rsyslog-omstdout \
	&& rm /etc/rsyslog.d/listen.conf
COPY	rsyslog.conf /etc/rsyslog.conf
