build:

docker build  -t rsyslog/rsyslog_doc_gen .

where rsyslog/rsyslog_doc_gen is the tag and the location on dockerhub. Obviously,
you need to use something else if you are not a maintainer of that repository.

ENV VARS
DEBUG - turn on some (minimal) container debugging
