FROM rsyslog/rsyslog_dev_base_ubuntu:19.04
USER root
RUN apt-get update && apt-get install -y \
   python-dev \
   python-pip
RUN pip install buildbot-worker buildbot-slave
RUN groupadd -r buildbot && useradd -r -g buildbot buildbot
RUN mkdir /worker && chown buildbot:buildbot /worker
# Install your build-dependencies here ...
ENV WORKER_ENVIRONMENT_BLACKLIST=WORKER*
USER buildbot
WORKDIR /worker
RUN buildbot-worker create-worker . docker.rsyslog.com docker-ubuntu16 password
# the following script is directly from buildbot git repo and seems
# to be necessary at the moment.
# see https://github.com/buildbot/buildbot/issues/4179
COPY tpl-buildbot.tac /worker/buildbot.tac
ENTRYPOINT ["/usr/local/bin/buildbot-worker"]
CMD ["start", "--nodaemon"]
VOLUME /worker
