# wait until main message queue is empty. This is currently done in
# a separate shell script so that we can change the implementation
# at some later point. -- rgerhards, 2009-05-25
echo WaitMainQueueEmpty | nc 127.0.0.1 13500
