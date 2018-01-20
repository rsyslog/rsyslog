FROM	alpine:3.7
LABEL   maintainer="rgerhards@adiscon.com"
ADD	rsyslog@lists.adiscon.com-5a55e598.rsa.pub /etc/apk/keys/rsyslog@lists.adiscon.com-5a55e598.rsa.pub
RUN	echo "http://build.rsyslog.com/alpine/3.7/unstable" >> /etc/apk/repositories \
	&& apk update  \
	&& apk add rsyslog rsyslog-elasticsearch rsyslog-mmutf8fix rsyslog-mmjsonparse
ADD	rsyslog.conf /etc/rsyslog.conf
# done base system setup

WORKDIR /home/appliance
ADD	starter.sh CONTAINER.* ./
RUN	echo "`date +%F` (`date +%s`)" > CONTAINER.release
CMD	["/home/appliance/starter.sh"]
