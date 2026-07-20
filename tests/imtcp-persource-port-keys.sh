#!/bin/bash
## Run the shared exact port-key ratelimit test through imtcp.
export PORT_KEY_MODULE=imtcp
. ${srcdir:=.}/ratelimit-persource-port-keys-common.sh
