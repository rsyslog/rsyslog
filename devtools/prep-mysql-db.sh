#!/bin/bash
# this script prepares a mysql instance for use by the rsyslog testbench

mysql -u root -e "CREATE USER 'rsyslog'@'localhost' IDENTIFIED BY 'testbench';"
mysql -u root -e "GRANT ALL PRIVILEGES ON *.* TO 'rsyslog'@'localhost'; FLUSH PRIVILEGES;"
