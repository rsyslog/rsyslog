# Definitions common to these scripts
# mybasedir=$(dirname "$0")
mybasedir=$PKGBASEDIR

source $mybasedir/config.sh
cd $mybasedir/
echo "-------------------------------------"
echo "--- Basedir: $szBaseDir"
echo "--- Sync remote repository	---"
echo "-------------------------------------"

rsync -avh --update -e "ssh -i /private-files/.ssh/id_rsa" --progress $REPOUSERNAME@$REPOURL $szBaseDir/
