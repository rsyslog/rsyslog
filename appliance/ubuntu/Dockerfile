FROM	ubuntu:16.04
ENV	DEBIAN_FRONTEND=noninteractive
RUN	apt-get -y update \
	&& apt-get -y upgrade \
	&& apt-get -y install software-properties-common curl \
	&& add-apt-repository -y ppa:adiscon/v8-stable \
	&& apt-get -y update \
	&& apt-get -y install libfastjson4 \
	&& apt-get -y install rsyslog \
	&& rm -r /etc/rsyslog.conf
ADD	rsyslog.conf /etc/rsyslog.conf
#VOLUME  /rsyslog-bin
RUN	mkdir /rsyslog-bin \
	&& cp /usr/sbin/rsyslogd /usr/lib/rsyslog/* /rsyslog-bin

EXPOSE	10514

WORKDIR /rsyslog-bin
CMD	["/rsyslog-bin/rsyslogd", "-n", "-f/etc/rsyslog.conf", "-M."]
