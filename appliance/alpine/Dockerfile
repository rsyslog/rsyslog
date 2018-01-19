FROM	alpine:3.7
ADD	rsyslog@lists.adiscon.com-5a55e598.rsa.pub /etc/apk/keys/rsyslog@lists.adiscon.com-5a55e598.rsa.pub
RUN	echo "http://build.rsyslog.com/alpine/3.7/unstable" >> /etc/apk/repositories \
	&& apk update  \
	&& apk add rsyslog rsyslog-elasticsearch
ADD	rsyslog.conf /etc/rsyslog.conf

CMD	["/usr/sbin/rsyslogd", "-n", "-f/etc/rsyslog.conf"]
