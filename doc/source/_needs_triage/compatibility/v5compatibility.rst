Compatibility Notes for rsyslog v5
==================================

*Written by* `Rainer Gerhards <https://rainer.gerhards.net/>`_
*(2009-07-15)*

The changes introduced in rsyslog v5 are numerous, but not very
intrusive. This document describes things to keep in mind when moving
from v4 to v5. It does not list enhancements nor does it talk about
compatibility concerns introduced by earlier versions (for this, see
their respective compatibility documents).

HUP processing
--------------

The $HUPisRestart directive is supported by some early v5 versions, but
has been removed in 5.1.3 and above. That means that restart-type HUP
processing is no longer available. This processing was redundant and had
a lot a drawbacks. For details, please see the `rsyslog v4 compatibility
notes <v4compatibility.html>`_ which elaborate on the reasons and the
(few) things you may need to change.

Please note that starting with 5.8.11 HUP will also requery the local
hostname.

Queue on-disk format
--------------------

The queue information file format has been changed. When upgrading from
v4 to v5, make sure that the queue is emptied and no on-disk structure
present. We did not go great length in understanding the old format, as
there was too little demand for that (and it being quite some effort if
done right).

Queue Worker Thread Shutdown
----------------------------

Previous rsyslog versions had the capability to "run" on zero queue
worker if no work was required. This was done to save a very limited
number of resources. However, it came at the price of great complexity.
In v5, we have decided to let a minimum of one worker run all the time.
The additional resource consumption is probably not noticeable at all,
however, this enabled us to do some important code cleanups, resulting
in faster and more reliable code (complex code is hard to maintain and
error-prone). From the regular user's point of view, this change should
be barely noticeable. I am including the note for expert users, who will
notice it in rsyslog debug output and other analysis tools. So it is no
error if each queue in non-direct mode now always runs at least one
worker thread.
