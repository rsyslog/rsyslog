#!/bin/bash
## Run the shared exact port-key ratelimit test through imptcp.
export PORT_KEY_MODULE=imptcp
. ${srcdir:=.}/ratelimit-persource-port-keys-common.sh
