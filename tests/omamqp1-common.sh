#!/bin/bash

# version of qdrouterd to use for test
QDROUTERD_VERSION=${QDROUTERD_VERSION:-1.7.0}
# Use of containers isn't necessary for CI, and might not be
# useful for devs, depending on platform availability.
# If not using a container for the test, the
# package will be installed using the platform
# package manager.
#USE_CONTAINER=${USE_CONTAINER:-false}
#CONTAINER_URL=${CONTAINER_URL:-quay.io/interconnectedcloud/qdrouterd:$QDROUTERD_VERSION}

if ! type -p qdrouterd ; then
	echo no qdrouterd found in PATH $PATH - skipping test
	exit 0
fi

ver=$( qdrouterd --version )
if [ "$ver" = $QDROUTERD_VERSION ] ; then
	echo found qdrouterd version $ver - continuing
else
	echo found qdrouterd version $ver but expected version $QDROUTERD_VERSION - skipping test
	exit 0
fi

AMQP_SIMPLE_RECV=${AMQP_SIMPLE_RECV:-/usr/share/proton-0.28.0/examples/python/simple_recv.py}

if [ -f $AMQP_SIMPLE_RECV ] ; then
	echo found $AMQP_SIMPLE_RECV
else
	echo no amqp client $AMQP_SIMPLE_RECV - skipping test
	exit 0
fi

AMQP_PYTHON=${AMQP_PYTHON:-python}
if $AMQP_PYTHON $AMQP_SIMPLE_RECV --help > /dev/null 2>&1 ; then
	: # good
elif python3 $AMQP_SIMPLE_RECV --help > /dev/null 2>&1 ; then
	AMQP_PYTHON=python3
else
	echo missing python modules for $AMQP_SIMPLE_RECV - skipping test
	exit 0
fi

amqp_simple_recv() {
	# $1 is host:port (or amqp url if applicable)
	# $2 is target (e.g. amq.rsyslogtest)
	# $3 is number of messages to read
	stdbuf -o 0 $AMQP_PYTHON $AMQP_SIMPLE_RECV -a $1/$2 -m $3
}
