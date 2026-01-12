.. meta::
   :description: FAQ explaining why blocking actions stall queues and how to fix it using action queues
   :keywords: blocking, action, queue, stall, hang, decouple, performance, rsyslog

.. summary-start
   Explains why a slow or suspended output action can block the entire processing queue and how to solve this by configuring a dedicated action queue.
.. summary-end

Why does a blocking action stop all log processing?
===================================================

**Q: I have an output action (like writing to a database or remote server) that is slow or unreachable. Why does this stop all my other logs from being processed, even those going to local files?**

**A:** This happens because of how rsyslog processes messages from a queue.

By default, when rsyslog picks a batch of messages from a queue (like the main message queue), it processes the ruleset sequentially. For each message, it executes the configured actions one by one.

Critically, the action is executed **by the queue worker thread itself**.

If an action blocks (for example, waiting for a database connection timeout or a TCP acknowledgement), the queue worker thread is blocked waiting for that action to complete. It cannot proceed to the next action, nor can it pick up the next message from the queue.

If you have multiple actions defined (e.g., one to a file, one to a remote server), and the remote server action blocks, the file action will also stop receiving messages because the shared queue worker is stuck.

**The Solution: Action Queues**

To prevent a single slow or failing action from affecting others, you should **decouple** it from the main processing flow. You do this by configuring a dedicated **Action Queue**.

When an action has its own queue:
1. The main queue worker places the message into the action's queue and immediately moves on to the next step.
2. A separate worker thread (managed by the action queue) picks up the message and executes the potentially blocking operation.
3. If the action blocks, only the action's specific worker is stalled. The main queue keeps running, and other actions continue to receive logs.

**Example Configuration**

Here is an example of how to add a disk-assisted queue to an action (in this case, forwarding to a remote server) so that it runs asynchronously:

.. code-block:: text

   action(type="omfwd" target="192.0.2.1" port="514" protocol="tcp"
          queue.type="LinkedList"
          queue.filename="fwd_queue"
          queue.maxDiskSpace="1g"
          queue.saveOnShutdown="on"
          action.resumeRetryCount="-1"
   )

In this example:
* ``queue.type="LinkedList"`` enables the separate queue (making the action asynchronous).
* The other ``queue.*`` parameters configure it to be disk-assisted, ensuring logs are preserved even if the remote server is down for a long time.
* ``action.resumeRetryCount="-1"`` ensures rsyslog keeps trying to connect forever.

See Also
--------

* :doc:`../concepts/queues`
