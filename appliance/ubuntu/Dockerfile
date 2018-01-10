FROM	ubuntu:16.04
ENV	DEBIAN_FRONTEND=noninteractive
RUN	apt-get -y update \
	&& apt-get -y upgrade \
	&& apt-get -y install rsyslog
ADD	rsyslog.conf /etc/rsyslog.conf

CMD	["/usr/sbin/rsyslogd", "-n", "-f/etc/rsyslog.conf"]
