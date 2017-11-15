# this perform a travis cron job
echo Travis under cron detected, currently no action!
set -x

# download coverity tool
mkdir coverity
cd coverity
wget http//build.rsyslog.com/CI/cov-analysis.tar.gz
tar xzfv cov*.tar.gz
rm -f cov*.tar.gz
export PATH="$(ls -d Cov*)/bin:$PATH"
cd ..
# Coverity scan tool installed

# prep rsyslog for submission
autoreconf -vfi
./configure
