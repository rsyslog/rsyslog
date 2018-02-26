FROM	alpine:3.7
LABEL   maintainer="rgerhards@adiscon.com"
COPY	rsyslog@lists.adiscon.com-5a55e598.rsa.pub /etc/apk/keys/rsyslog@lists.adiscon.com-5a55e598.rsa.pub
RUN	echo "http://alpine.rsyslog.com/3.7/stable" >> /etc/apk/repositories \
	&& apk --no-cache update  \
	&& apk add --no-cache \
	   rsyslog \
	   rsyslog-elasticsearch \
	   rsyslog-imptcp \
	   rsyslog-imrelp \
	   rsyslog-mmjsonparse \
	   rsyslog-mmutf8fix \
	   rsyslog-omrelp \
	   rsyslog-omstdout
RUN	adduser -s /bin/ash -D rsyslog rsyslog \
	&& echo "rsyslog ALL=(ALL) NOPASSWD:ALL" >> /etc/sudoers
VOLUME	/config /work /logs
CMD	["rsyslog"]
ENTRYPOINT ["/home/appliance/starter.sh"]
COPY	rsyslog.conf /etc/rsyslog.conf
COPY	rsyslog.conf.d/*.conf /etc/rsyslog.conf.d/
# done base system setup

WORKDIR /home/appliance
COPY	starter.sh CONTAINER.* ./
COPY	internal/* ./internal/
COPY	tools/* ./tools/
RUN	echo "`date +%F` (`date +%s`)" > CONTAINER.release \
	&& chown -R rsyslog:rsyslog *
