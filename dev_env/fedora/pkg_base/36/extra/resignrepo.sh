# Definitions common to these scripts
source $(dirname "$0")/config.sh

echo "-------------------------------------"
echo "--- RPMMaker                      ---"
echo "-------------------------------------"

if [ -z $RPM_PLATFORM ]; then
	echo "Which Linux Plattform?:"
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
		echo "Making RPM for Plattforms '$szArch'
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

for distro in $szDist; do 
	for arch in $szArch; do	
		echo "Updating REPO $distro-$arch-$szSubRepo"

		repo=$szYumRepoDir/$szSubRepo/$distro/$arch;
		if sudo rpm --addsign $repo/RPMS/*.rpm; then
			sudo createrepo -s sha256 -o $repo -d -p $repo;
			sudo rm $repo/repodata/repomd.xml.asc
			sudo gpg --passphrase-file passfile.txt --detach-sign --armor $repo/repodata/repomd.xml
		else
			echo "rpmsign FAILED";
			exit 1;
		fi
	done;
done;

exit 0;

