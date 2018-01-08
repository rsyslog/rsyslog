FROM alpine:latest
RUN	apk update && \
	apk add rsyslog
ADD	rsyslog.conf /etc/rsyslog.conf

CMD	["/usr/sbin/rsyslogd", "-n", "-f/etc/rsyslog.conf"]
