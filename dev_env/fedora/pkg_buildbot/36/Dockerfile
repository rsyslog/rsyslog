FROM rsyslog/rsyslog_dev_pkg_base_fedora:36
USER root
RUN dnf -y install  \
   python3-devel \
   python3-pip
RUN pip3 install buildbot-worker buildbot-slave
RUN groupadd -r buildbot && useradd -r -g buildbot buildbot
RUN mkdir /worker && chown buildbot:buildbot /worker
# Install your build-dependencies here ...
ENV WORKER_ENVIRONMENT_BLACKLIST=WORKER*
#USER root
USER buildbot
WORKDIR /worker
RUN buildbot-worker create-worker . docker.rsyslog.com docker-fedora36 password
# the following script is directly from buildbot git repo and seems
# to be necessary at the moment.
# see https://github.com/buildbot/buildbot/issues/4179
COPY tpl-buildbot.tac /worker/buildbot.tac
ENTRYPOINT ["/usr/local/bin/buildbot-worker"]
CMD ["start", "--nodaemon"]
VOLUME /worker
USER root
