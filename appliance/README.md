**EXPERIMENTAL** docker containter to run rsyslog

This aims at providing a full-functional docker container with ample features.
Right now, it is under development. We welcome checking out and commenting it,
but **do not use it in production**.

This provides two containers:
- alpine based, this is what you want in production
- ubuntu based, this is primarily for rsyslog developers

more info:
- https://github.com/rsyslog/rsyslog/issues/2368
- https://github.com/rsyslog/rsyslog/projects/5

## projects that provide docker containers:

- https://github.com/deoren/rsyslog-docker (based on @halfer provided files)
- https://github.com/megastef/rsyslog-logsene (logsene-enabled)
- https://github.com/camptocamp/docker-rsyslog-bin/blob/master/Dockerfile (Ubunut xenial)
- https://github.com/jumanjihouse/docker-rsyslog (Alpine Linux)
