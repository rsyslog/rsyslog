cd tests
export srcdir=&(pwd)
# add tests here below
# ./omfile-read-only.sh
exit # currently not needed
export RSYSLOG_DEBUG="debug nologfuncflow noprintmutexaction"
./msgvar-concurrency.sh
./localvar-concurrency.sh
./exec_tpl-concurrency.sh
./privdropuser.sh
./privdropuserid.sh
./privdropgroup.sh
./privdropgroupid.sh
./template-json.sh
./template-pos-from-to.sh
./template-pos-from-to-lowercase.sh
./template-pos-from-to-oversize.sh
./template-pos-from-to-oversize-lowercase.sh
./template-pos-from-to-missing-jsonvar.sh
./fac_authpriv.sh
