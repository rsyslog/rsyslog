FROM	alpine
LABEL	maintainer rgerhards@adiscon.com
RUN	apk add --no-cache py-pip git
RUN	pip install sphinx
RUN	adduser -s /bin/ash -D rsyslog rsyslog \
	&& echo "rsyslog ALL=(ALL) NOPASSWD:ALL" >> /etc/sudoers
WORKDIR	/home/appliance
CMD	["build-doc"]
ENTRYPOINT ["/home/appliance/starter.sh"]
VOLUME	/rsyslog-doc
RUN	chmod a+w /rsyslog-doc
ENV	BRANCH="main" \
	FORMAT="html" \
	STRICT="-n -W"
COPY	starter.sh ./
COPY	tools/* ./tools/
