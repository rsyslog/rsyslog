# This directory is meant to be used by packages that need to augment the
# existing rsyslogd profile with extra rules.  All files in here will be
# included by the /etc/apparmor.d/usr.sbin.rsyslogd profile, subject to the
# exclusion rules defined in
#
# https://sources.debian.org/src/apparmor/3.0.8-2/libraries/libapparmor/src/private.c/#L65
#
# and
#
# https://sources.debian.org/src/apparmor/3.0.8-2/libraries/libapparmor/src/private.c/#L132
#
# Please check the README.apparmor file in the documentation directory of the
# rsyslog package for more information.
#
# For the usual overrides and other additions by local administrators, please
# use the /etc/apparmor.d/local/ mechanism.
