FROM	alpine:3.7
ADD	rsyslog@lists.adiscon.com-5a55e598.rsa.pub /etc/apk/keys/rsyslog@lists.adiscon.com-5a55e598.rsa.pub
WORKDIR /home/appliance
ADD	starter.sh starter.sh
RUN	echo "http://build.rsyslog.com/alpine/3.7/unstable" >> /etc/apk/repositories \
	&& apk update  \
	&& apk add rsyslog rsyslog-elasticsearch rsyslog-mmutf8fix rsyslog-mmjsonparse
ADD	rsyslog.conf /etc/rsyslog.conf

CMD	["/home/appliance/starter.sh"]
