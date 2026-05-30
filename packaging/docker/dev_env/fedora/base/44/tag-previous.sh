#!/bin/sh
docker tag rsyslog/rsyslog_dev_base_fedora:44 rsyslog/rsyslog_dev_base_fedora:44_previous
docker push rsyslog/rsyslog_dev_base_fedora:44_previous
