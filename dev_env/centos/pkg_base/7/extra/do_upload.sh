!#/bin/sh
# Definitions common to these scripts
source $(dirname "$0")/config.sh

BRANCHES="v5-stable v7-stable v8-stable testing"

echo "-------------------------------------"
echo "--- Upload RPM Packages               ---"
echo "-------------------------------------"
echo "Which BRANCH do you want to upload?--"
select szBranch in $BRANCHES
do
        echo "Uploading Branch '$szBranch'
        "
        break;
done

rsync -au -e "ssh -i /private-files/.ssh/id_rsa" --progress $szYumRepoDir/$szBranch/* $REPOUSERNAME@$REPOURL/$szBranch/
