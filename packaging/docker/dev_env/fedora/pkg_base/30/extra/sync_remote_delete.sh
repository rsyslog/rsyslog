#!/bin/sh
# Definitions common to these scripts
source $(dirname "$0")/config.sh

echo "-------------------------------------"
echo "--- SYNC REMOTE REPO              ---"
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

echo "SYNC DELETE Branch '$REPOUSERNAME@$REPOURL/$szSubRepo/' to $szYumRepoDir/$szSubRepo/
"

FILESTOREMOVE=`rsync -au --delete --dry-run -e "ssh -i /private-files/.ssh/id_rsa" --progress $REPOUSERNAME@$REPOURL/$szSubRepo/* $szYumRepoDir/$szSubRepo/ | grep deleting | wc -l`

read -p "ARE YOU SURE TO REMOVE $FILESTOREMOVE files from $REPOUSERNAME@$REPOURL/$szSubRepo/ " -n 1 -r
echo ""
if [[ ! $REPLY =~ ^[Yy]$ ]]
then
        exit 1;
fi
exit

# DO IT
rsync -au --delete -e "ssh -i /private-files/.ssh/id_rsa" --progress $REPOUSERNAME@$REPOURL/$szSubRepo/* $szYumRepoDir/$szSubRepo/ 

