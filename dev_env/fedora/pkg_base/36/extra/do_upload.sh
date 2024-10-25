#!/bin/sh
# Definitions common to these scripts
source $(dirname "$0")/config.sh

echo "-------------------------------------"
echo "--- Upload RPM Packages               ---"
echo "-------------------------------------"

if [ -z $RPM_REPO ]; then
	echo "Which REPO do you want to upload?--"
	select szSubRepo in $REPOOPTIONS
	do
		break;
	done
else
	echo "REPO is set to '$RPM_REPO'"
	szSubRepo=$RPM_REPO
fi

echo "Uploading Branch '$szYumRepoDir/$szSubRepo/' to $REPOUSERNAME@$REPOURL/$szSubRepo/
"

# Set default SSH port to 22 if REPOSSHPORT is not set
REPOSSHPORT=${REPOSSHPORT:-22}

rsync -au -e "ssh -i /private-files/.ssh/id_rsa -p $REPOSSHPORT" --progress $szYumRepoDir/$szSubRepo/* $REPOUSERNAME@$REPOURL/$szSubRepo/
