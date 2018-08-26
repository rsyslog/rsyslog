# warning: CentOS 6 is special in that Python is too old for buildbot! 
# So we build Python from source...
FROM rsyslog/rsyslog_dev_base_centos:6
USER root
RUN	yum -y install  \
	centos-release-scl
RUN	yum -y install \
	xz \
	bzip2-devel
# Note: bzip2-devel is absolutely necessary to build python and use pip, albeit it is
#       not checked by the build process! If it is not there, pip will find no packages,
#       as they are stored as bz2.
#       see also: https://github.com/pypa/warehouse/issues/2036
RUN	wget https://www.python.org/ftp/python/3.6.0/Python-3.6.0.tar.xz && \
	tar xf Python-3.6.0.tar.xz && \
	cd Python-3.6.0 && \
	./configure --prefix=/usr/local --enable-shared LDFLAGS="-Wl,-rpath /usr/local/lib" && \
	make -j && \
	make install
RUN 	curl https://bootstrap.pypa.io/get-pip.py | python3.6 -
RUN	yum -y install bzip2
#RUN	pip --version &&  \
#	pip install --verbose Twisted
RUN	pip install buildbot-worker buildbot-slave
RUN	groupadd -r buildbot && useradd -r -g buildbot buildbot
RUN	mkdir /worker && chown buildbot:buildbot /worker
# Install your build-dependencies here ...
ENV WORKER_ENVIRONMENT_BLACKLIST=WORKER*
USER buildbot
WORKDIR /worker
RUN buildbot-worker create-worker . docker.rsyslog.com docker-centos6 password
# the following script is directly from buildbot git repo and seems
# to be necessary at the moment.
# see https://github.com/buildbot/buildbot/issues/4179
COPY tpl-buildbot.tac /worker/buildbot.tac
ENTRYPOINT ["/usr/local/bin/buildbot-worker"]
CMD ["start", "--nodaemon"]
VOLUME /worker
