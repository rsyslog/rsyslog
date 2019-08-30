#!/bin/sh
# Definitions common to these scripts
source $(dirname "$0")/config.sh

echo "---------------------------------------------"
echo "--- Copy private files to their locations ---"
ln -s /private-files/passfile.txt $szBaseDir/passfile.txt
cp -rf /private-files/.gnupg /home/pkg/.gnupg/
cp -rf /private-files/.gnupg /root/.gnupg/
cp -rf /private-files/.ssh /root/.ssh/
cp -rf /private-files/.ssh /home/pkg/.ssh/
echo "---------------------------------------------"

echo "--------------------------------"
echo "--- Setting file permissions ---"
chmod -R 600 /home/pkg/.gnupg/
chmod -R 600 /root/.gnupg/
chown pkg:pkg /private-files/* 
chown pkg:pkg /private-files/.*
chmod -R 0600 /private-files/.ssh/id_rsa 

echo "--------------------------------"

echo "--------------------------------"
echo "--- Sync RPM REPO and update GIT "
./sync_remote.sh
git pull && yes | cp -rf etc-mock/* /etc/mock/
chown -R pkg ./ 

# TODO: Get rsyslog source for testing
#wget https://www.rsyslog.com/files/download/rsyslog/rsyslog-8.1908.0.tar.gz
#mv rsyslog-8.1908.0.tar.gz rpmbuild/SOURCES/
