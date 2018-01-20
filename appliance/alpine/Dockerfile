FROM	alpine:3.7
LABEL   maintainer="rgerhards@adiscon.com"
ADD	rsyslog@lists.adiscon.com-5a55e598.rsa.pub /etc/apk/keys/rsyslog@lists.adiscon.com-5a55e598.rsa.pub
RUN	echo "http://build.rsyslog.com/alpine/3.7/unstable" >> /etc/apk/repositories \
	&& apk update  \
	&& apk add rsyslog rsyslog-elasticsearch rsyslog-mmutf8fix rsyslog-mmjsonparse \
	   rsyslog-imrelp rsyslog-omrelp rsyslog-imptcp rsyslog-omstdout
RUN	adduser -s /bin/ash -D rsyslog rsyslog \
	&& echo "rsyslog ALL=(ALL) NOPASSWD:ALL" >> /etc/sudoers
ADD	rsyslog.conf /etc/rsyslog.conf
VOLUME	/config /work /logs
# done base system setup

ADD	container_config/container_config /config/container_config
WORKDIR /home/appliance
ADD	starter.sh CONTAINER.* ./
ADD	tools/* ./tools/
RUN	echo "`date +%F` (`date +%s`)" > CONTAINER.release \
	&& chown rsyslog:rsyslog *
CMD	["/home/appliance/starter.sh"]
