#!/bin/bash
# Drain actual disk spill while FE IDs0/1 remain held. Saving disabled
# must discard exactly those IDs; restart must retain the exact 2..65 inventory.
export LOCAL_QUEUE_S5_ENGINE=disk
export LOCAL_QUEUE_S5_SAVE=off
. ${srcdir:=.}/local-queue-s5-spill.sh
