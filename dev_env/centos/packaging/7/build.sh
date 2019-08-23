set -e

# Check for extra files
if [ -d "extra/.gnupg" ]; then
	echo "SUCCESS: extra/.gnupg found"
else
	echo "FAIL: extra/.gnupg missing"
	exit 1;
fi
if [ -d "extra/.ssh" ]; then
	echo "SUCCESS: extra/.ssh found"
else
	echo "FAIL: extra/.ssh missing"
	exit 1;
fi
if [ -f "extra/do_upload.sh" ]; then
	echo "SUCCESS: extra/do_upload.sh found"
else
	echo "FAIL: extra/do_upload.sh missing"
	exit 1;
fi
if [ -f "extra/passfile.txt" ]; then
	echo "SUCCESS: extra/passfile.txt found"
else
	echo "FAIL: extra/passfile.txt missing"
	exit 1;
fi
if [ -f "extra/sync_remote.sh" ]; then
	echo "SUCCESS: extra/sync_remote.sh found"
else
	echo "FAIL: extra/sync_remote.sh missing"
	exit 1;
fi

# Use --no-cache to rebuild image
docker build $1 -t rsyslog/rsyslog_dev_pkg_centos:7 --build-arg CACHEBUST=$(date +%s) .
printf "\n\n================== BUILD DONE\n"
