$RepeatedMsgReduction
---------------------

**Type:** global configuration parameter

**Default:** off

Description
^^^^^^^^^^^

This parameter models old sysklogd legacy. **Note that many people,
including the rsyslog authors, consider this to be a misfeature.** See
*Discussion* below to learn why.

This parameter specifies whether or not repeated messages should be
reduced (this is the "Last line repeated n times" feature). If set to
*on*, repeated messages are reduced. If kept at *off*, every message is
logged. In very early versions of rsyslog, this was controlled by the
*-e* command line option.

What is a repeated message
^^^^^^^^^^^^^^^^^^^^^^^^^^

For a message to be classified as repeated, the following properties
must be **identical**:

* msg
* hostname
* procid
* appname

Note that rate-limiters are usually applied to specific input sources
or processes. So first and foremost the input source must be the same
to classify a messages as a duplicated.

You may want to check out
`testing rsyslog ratelimiting <http://www.rsyslog.com/first-try-to-test-rate-limiting/>`_
for some extra information on the per-process ratelimiting.

Discussion
^^^^^^^^^^

* Very old versions of rsyslog did not have the ability to include the
  repeated message itself within the repeat message.

* Versions before 7.3.2 applied repeat message reduction to the output
  side. This had some implications:

  - they did not account for the actual message origin, so two processes
    emitting an equally-looking message triggered the repeated message
    reduction code

  - repeat message processing could be set on a per-action basis, which
    has switched to per-input basis for 7.3.2 and above

* While turning this feature on can save some space in logs, most log analysis
  tools need to see the repeated messages, they can't handle the
  "last message repeated" format.
* This is a feature that worked decades ago when logs were small and reviewed
  by a human, it fails badly on high volume logs processed by tools.

Sample
^^^^^^

This turns on repeated message reduction (**not** recommended):

::

 $RepeatedMsgReduction on    # do not log repeated messages

