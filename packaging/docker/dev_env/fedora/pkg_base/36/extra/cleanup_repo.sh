# Definitions common to these scripts
source $(dirname "$0")/config.sh

echo "-------------------------------------"
echo "--- CLEANUPREPO                   ---"
echo "-------------------------------------"

if [ -z $RPM_PLATFORM ]; then
        echo "Which Linux Platform?:"
        select szDist in $PLATOPTIONS "All"
        do
                case $szDist in "All")
                        szDist=$PLATOPTIONS;
                esac
                echo "Making RPM for EPEL '$szDist'
                "
                break;
        done
else
        echo "PLATFORM is set to '$RPM_PLATFORM'"
        szDist=$RPM_PLATFORM
fi

if [ -z $RPM_ARCH ]; then
        echo "Which Architecture?:"
        select szArch in $ARCHOPTIONS "All"
        do
                case $szArch in "All")
                        szArch=$ARCHOPTIONS;
                esac
                echo "Making RPM for Platforms '$szArch'
                "
                break;
        done
else
        echo "ARCH is set to '$RPM_ARCH'"
        szArch=$RPM_ARCH
fi

if [ -z $RPM_REPO ]; then
        echo "Which YUM repository?":
        select szSubRepo in $REPOOPTIONS "All"
        do
                case $szSubRepo in "All")
                        szSubRepo=$REPOOPTIONS;
                esac
                break;
        done
else
        echo "REPO is set to '$RPM_REPO'"
        szSubRepo=$RPM_REPO
fi

if [ -z $RPM_FILESTOKEEP ]; then
        read -e -p "How many files do you which to keep? " -i "90" RPM_FILESTOKEEP
fi

# Create repo url
export yumrepotoclean=yumrepo/$szSubRepo/$szDist/$szArch/

FILESTOREMOVE=`repomanage --keep=$RPM_FILESTOKEEP --old $yumrepotoclean | wc -l`

read -p "ARE YOU SURE TO REMOVE $FILESTOREMOVE Packages from $yumrepotoclean (Keep last $RPM_FILESTOKEEP packages)? " -n 1 -r
echo ""
if [[ ! $REPLY =~ ^[Yy]$ ]]
then
        exit 1;
fi

repomanage --keep=$RPM_FILESTOKEEP --old $yumrepotoclean | xargs rm -f

echo "DELETED $FILESTOREMOVE files from $yumrepotoclean - rebuild repo now"
createrepo $yumrepotoclean

exit 0;

