******************************************
omruleset: ruleset output/including module
******************************************

===========================  ===========================================================================
**Module Name:**             **omruleset**
**Author:**                  `Rainer Gerhards <https://rainer.gerhards.net/>`_ <rgerhards@adiscon.com>
===========================  ===========================================================================

.. warning::

   This module is outdated and only provided to support configurations that
   already use it. **Do no longer use it in new configurations.** It has
   been replaced by the much more efficient `"call" RainerScript
   statement <rainerscript_call.html>`_. The "call" statement supports
   everything omruleset does, but in an easier to use way. 


**Available Since**: 5.3.4

**Deprecated in**: 7.2.0+


Purpose
=======

This is a very special "output" module. It permits to pass a message
object to another rule set. While this is a very simple action, it
enables very complex configurations, e.g. it supports high-speed "and"
conditions, sending data to the same file in a non-racy way,
include-ruleset functionality as well as some high-performance
optimizations (in case the rule sets have the necessary queue
definitions).

While it leads to a lot of power, this output module offers seemingly
easy functionality. The complexity (and capabilities) arise from how
everything can be combined.

With this module, a message can be sent to processing to another
ruleset. This is somewhat similar to a "#include" in the C programming
language. However, one needs to keep on the mind that a ruleset can
contain its own queue and that a queue can run in various modes.

Note that if no queue is defined in the ruleset, the message is enqueued
into the main message queue. This most often is not optimal and means
that message processing may be severely deferred. Also note that when the
ruleset's target queue is full and no free space can be acquired within
the usual timeout, the message object may actually be lost. This is an
extreme scenario, but users building an audit-grade system need to know
this restriction. For regular installations, it should not really be
relevant.

**At minimum, be sure you understand the**
:doc:`$RulesetCreateMainQueue <../ruleset/rsconf1_rulesetcreatemainqueue>`
**directive as well as the importance of statement order in rsyslog.conf
before using omruleset!**

**Recommended Use:**

-  create rulesets specifically for omruleset
-  create these rulesets with their own main queue
-  decent queueing parameters (sizes, threads, etc) should be used for
   the ruleset main queue. If in doubt, use the same parameters as for
   the overall main queue.
-  if you use multiple levels of ruleset nesting, double check for
   endless loops - the rsyslog engine does not detect these


|FmtObsoleteName| directives
============================

.. note::

   Parameter names are case-insensitive; camelCase is recommended for readability.

.. list-table::
   :widths: 30 70
   :header-rows: 1

   * - Parameter
     - Summary
   * - :ref:`param-omruleset-actionomrulesetrulesetname`
     - .. include:: ../../reference/parameters/omruleset-actionomrulesetrulesetname.rst
        :start-after: .. summary-start
        :end-before: .. summary-end


Examples
========

Ruleset for Write-to-file action
--------------------------------

This example creates a ruleset for a write-to-file action. The idea here
is that the same file is written based on multiple filters, problems
occur if the file is used together with a buffer. That is because file
buffers are action-specific, and so some partial buffers would be
written. With omruleset, we create a single action inside its own
ruleset and then pass all messages to it whenever we need to do so. Of
course, such a simple situation could also be solved by a more complex
filter, but the method used here can also be utilized in more complex
scenarios (e.g. with multiple listeners). The example tries to keep it
simple. Note that we create a ruleset-specific main queue (for
simplicity with the default main queue parameters) in order to avoid
re-queueing messages back into the main queue.

.. code-block:: none

   $ModLoad omruleset # define ruleset for commonly written file
   $RuleSet CommonAction
   $RulesetCreateMainQueue on
   *.* /path/to/file.log
 
   #switch back to default ruleset
   $ruleset RSYSLOG_DefaultRuleset
 
   # begin first action
   # note that we must first specify which ruleset to use for omruleset:
   $ActionOmrulesetRulesetName CommonAction
   mail.info :omruleset:
   # end first action
 
   # begin second action
   # note that we must first specify which ruleset to use for omruleset:
   $ActionOmrulesetRulesetName CommonAction
   :FROMHOST, isequal, "myhost.example.com" :omruleset:
   #end second action
 
   # of course, we can have "regular" actions alongside :omrulset: actions
   *.* /path/to/general-message-file.log


High-performance filter condition
---------------------------------

The next example is used to create a high-performance nested and filter
condition. Here, it is first checked if the message contains a string
"error". If so, the message is forwarded to another ruleset which then
applies some filters. The advantage of this is that we can use
high-performance filters where we otherwise would need to use the (much
slower) expression-based filters. Also, this enables pipeline
processing, in that second ruleset is executed in parallel to the first
one.

.. code-block:: none

   $ModLoad omruleset
   # define "second" ruleset
   $RuleSet nested
   $RulesetCreateMainQueue on
   # again, we use our own queue
   mail.* /path/to/mailerr.log
   kernel.* /path/to/kernelerr.log
   auth.* /path/to/autherr.log
 
   #switch back to default ruleset
   $ruleset RSYSLOG_DefaultRuleset
 
   # begin first action - here we filter on "error"
   # note that we must first specify which ruleset to use for omruleset:
   $ActionOmrulesetRulesetName nested
   :msg, contains, "error" :omruleset:
   #end first action
 
   # begin second action - as an example we can do anything else in
   # this processing. Note that these actions are processed concurrently
   # to the ruleset "nested"
   :FROMHOST, isequal, "myhost.example.com" /path/to/host.log
   #end second action
 
   # of course, we can have "regular" actions alongside :omrulset: actions
   *.* /path/to/general-message-file.log
 

Caveats/Known Bugs
==================

The current configuration file language is not really adequate for a
complex construct like omruleset. Unfortunately, more important work is
currently preventing me from redoing the config language. So use extreme
care when nesting rulesets and be sure to test-run your config before
putting it into production, ensuring you have a sufficiently large probe
of the traffic run over it. If problems arise, the `rsyslog debug
log <troubleshoot.html>`_ is your friend.

.. toctree::
   :hidden:

   ../../reference/parameters/omruleset-actionomrulesetrulesetname

