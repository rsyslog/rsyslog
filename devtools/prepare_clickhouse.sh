#!/bin/bash
# this script prepares a clickhouse instance for use by the rsyslog testbench


clickhouse-client --query="CREATE DATABASE rsyslog"
echo clickouse create database RETURN STATE: $?

# At the moment only the database is created for preparation.
# Every test creates a table for itself and drops it afterwards.
# This could look something like this:

#clickhouse-client --query="CREATE TABLE IF NOT EXISTS rsyslog.test ( id Int32, severity Int8, facility Int8, timestamp DateTime, ipaddress String, tag String, message String ) ENGINE = MergeTree() PARTITION BY severity Order By id"
#clickhouse-client --query="DROP TABLE rsyslog.test"
