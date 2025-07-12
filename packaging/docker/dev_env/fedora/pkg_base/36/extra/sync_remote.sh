# Definitions common to these scripts
# szBaseDir=$(dirname "$0")
szBaseDir=$PKGBASEDIR

source $szBaseDir/config.sh
cd $szBaseDir/
echo "-------------------------------------"
echo "--- Basedir: $szBaseDir"
echo "--- Sync remote repository        ---"
echo "-------------------------------------"

if [ -z $RPM_REPO ]; then
        echo "sync_remote: RPM_REPO not set"
        addrepo=
        addplatform=
        addarch=
else
        echo "sync_remote: RPM_REPO set to              $RPM_REPO"
        addrepo=$RPM_REPO/
        if [ -z $RPM_PLATFORM ]; then
                echo "sync_remote: RPM_PLATFORM not set"
                addplatform=
                addarch=
        else
                echo "sync_remote: RPM_PLATFORM set to  $RPM_PLATFORM"
                addplatform=$RPM_PLATFORM/
                if [ -z $RPM_ARCH ]; then
                        echo "sync_remote: RPM_ARCH not set"
                        addarch=
                else
                        echo "sync_remote: RPM_ARCH set to              $RPM_ARCH"
                        addarch=$RPM_ARCH/
                fi
        fi
fi

# Set default SSH port to 22 if REPOSSHPORT is not set
REPOSSHPORT=${REPOSSHPORT:-22}

echo "rsync $REPOUSERNAME@$REPOURL/$addrepo$addplatform$addarch to $szBaseDir/yumrepo/$addrepo$addplatform$addarch"
rsync -avh --update -e "ssh -i /private-files/.ssh/id_rsa -p $REPOSSHPORT" --progress $REPOUSERNAME@$REPOURL/$addrepo$addplatform$addarch $szBaseDir/yumrepo/$addrepo$addplatform$addarch
