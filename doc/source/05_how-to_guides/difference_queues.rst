What is the difference between the main_queue and a queue with a ruleset tied to an input?
==========================================================================================

A queue on a ruleset tied to an input replaces the main queue for that input.
The only difference is the higher default size of the main queue.

If an input bounded ruleset does not have a queue defined, what default does it have?
-------------------------------------------------------------------------------------

Rulesets without a queue on them use the main queue as a default.

What is the recommended way to use several inputs? Should there be a need to define a queue for the rulesets?
-------------------------------------------------------------------------------------------------------------

If you are going to do the same thing with all logs, then they should share a ruleset.

If you are doing different things with different logs, then they should have
different rulesets.

For example take a system where the default ruleset processes things and sends
them over the network to the nearest relay system. All systems have this same
default ruleset.
Then in the relay systems there is a ruleset which is tied to both TCP and UDP
listeners, and it receives the messages from the network, cleans them up,
and sends them on.
There is no mixing of these two processing paths, so having them as completely
separate paths with rulesets tied to the inputs and queues on the rulesets
makes sense.

A queue on a ruleset tied to one or more inputs can be thought of as a separate
instance of rsyslog, which processes those logs.


Credits: davidelang
