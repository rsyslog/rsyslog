#!/bin/bash
# With B=3, a live dedicated lease and borrowed lease leave one free slot.
# A two-message external producer blocks after its accepted prefix. Only
# borrowed completion can release capacity while the dedicated callback is
# still held; the fixture joins that producer and checks exact ordered IDs.
export LOCAL_QUEUE_HELP_BE_CAPACITY=3
. ${srcdir:=.}/local-queue-helping.sh
