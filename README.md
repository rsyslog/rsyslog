# rsyslog-docker
a playground for rsyslog docker tasks - nothing production yet

see also https://github.com/rsyslog/rsyslog/projects/5

# container structure

* env_dev_bare - a development environment containing required
  system tools but not all dependencies needed by rsyslog (but
  all tools to build them)
* env_dev - a complete development environment
* env\_ci - an environment suitable for CI runs
