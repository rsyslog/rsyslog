FROM	ubuntu:22.04
ENV	DEBIAN_FRONTEND=noninteractive
RUN 	apt-get update && \
	apt-get upgrade -y
RUN	apt-get install -y \
	autoconf \
	autoconf-archive \
	automake \
	autotools-dev \
	net-tools \
	bison \
	curl \
	flex \
	git \
	sudo \
	vim \
	wget \
	python3-docutils  \
	python3-pip \
	python3-pysnmp4
RUN	apt-get install -y \
	texlive-base \
	texlive-latex-base \
	texlive-latex-extra \
	texlive-latex-recommended \
	texlive-extra-utils \
	texlive-fonts-recommended \
	texlive-lang-other \
	texlive-luatex \
	texlive-xetex \
	latexmk
RUN	pip install sphinx sphinx_rtd_theme rst2pdf
VOLUME	/rsyslog
RUN	groupadd rsyslog \
	&& useradd -g rsyslog -s /bin/bash rsyslog \
	&& echo "rsyslog ALL=(ALL) NOPASSWD:ALL" >> /etc/sudoers \
	&& echo "buildbot ALL=(ALL) NOPASSWD:ALL" >> /etc/sudoers
WORKDIR	/rsyslog
USER	rsyslog
