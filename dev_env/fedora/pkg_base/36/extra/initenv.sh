#!/bin/sh
# Definitions common to these scripts
source $(dirname "$0")/config.sh
if [ -z $PKGGITBRANCH ]; then 
        echo "initenv: use master (default)"
	gitbranch=master
else 
        echo "initenv: set to $PKGGITBRANCH"
        echo "SPEC is set to '$RPM_SPEC'"
	gitbranch=$PKGGITBRANCH
fi

echo "---------------------------------------------"
echo "--- Copy private files to their locations ---"
cp -rf /home/pkg/.rpmmacros /root/
ln -s /private-files/passfile.txt $szBaseDir/passfile.txt
cp -rf /private-files/.gnupg /home/pkg/.gnupg/
cp -rf /private-files/.gnupg/* /home/pkg/.gnupg/
cp -rf /private-files/.gnupg /root/.gnupg/
cp -rf /private-files/.gnupg/* /root/.gnupg/
cp -rf /private-files/.ssh /root/
cp -rf /private-files/.ssh /home/pkg/
# Copy entitlements and subscription manager configurations
cp -rf /private-files/rhel/etc-pki-entitlement/* /etc/pki/entitlement/
cp -rf /private-files/rhel/rhsm-conf /etc/rhsm/
cp -rf /private-files/rhel/rhsm-ca /etc/rhsm/ca/
echo "---------------------------------------------"

echo "--------------------------------"
echo "--- Setting file permissions ---"
chmod -R 600 /home/pkg/.gnupg/
chmod -R 600 /root/.gnupg/
chown pkg:pkg /private-files/*
chown pkg:pkg /private-files/.*
chmod -R 0600 /private-files/.ssh/id_rsa
chmod -R 0600 /private-files/.ssh/id_dsa
chmod -R 0600 /home/pkg/.ssh/id_rsa
chmod -R 0600 /home/pkg/.ssh/id_dsa
chmod -R 0600 /root/.ssh/id_rsa
chmod -R 0600 /root/.ssh/id_dsa
echo "--------------------------------"

echo "--------------------------------"
echo "--- Sync RPM REPO"
./sync_remote.sh
echo "--------------------------------"
echo "--- Sync GIT and change branch"
git fetch --all 

if test "$gitbranch" = "master"; then 
	if test -z "$gitdebugbranch"; then
		echo "initenv: using master branch"
		git checkout -f $gitbranch
		git pull
	else
		echo "initenv: using custom branch $gitdebugbranch"
		git pull
		git checkout $gitdebugbranch
	fi
else
        echo "initenv: switch to PR branch"
	git fetch -t https://github.com/rsyslog/rsyslog-pkg-rhel-centos.git $gitbranch
	git reset --hard FETCH_HEAD --
	git checkout -B $gitbranch
	git rev-parse HEAD
fi

# Set PKGNOREPLACEMOCKFILES env variable to anything to use system mock files
if [ -z $PKGNOREPLACEMOCKFILES ]; then
        echo "initenv: replace MOCK files with our own (default)"
        cp -rf etc-mock/* /etc/mock/
        chown -R pkg ./ >> /dev/null 2>&1   
else
        echo "initenv: use system MOCK files"
fi

