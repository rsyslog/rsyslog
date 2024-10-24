Writing Rsyslog Output Plugins
==============================

This page is the begin of some developer documentation for writing
output plugins. Doing so is quite easy (and that was a design goal), but
there currently is only sparse documentation on the process available. I
was tempted NOT to write this guide here because I know I will most
probably not be able to write a complete guide.

However, I finally concluded that it may be better to have some
information and pointers than to have nothing.

Getting Started and Samples
---------------------------

The best to get started with rsyslog plugin development is by looking at
existing plugins. All that start with "om" are **o**\ utput
**m**\ odules. That means they are primarily thought of being message
sinks. In theory, however, output plugins may aggregate other
functionality, too. Nobody has taken this route so far so if you would
like to do that, it is highly suggested to post your plan on the rsyslog
mailing list, first (so that we can offer advice).

The rsyslog distribution tarball contains the omstdout plugin which is
extremely well targeted for getting started. Just note that this plugin
itself is not meant for production use. But it is very simplistic and so
a really good starting point to grasp the core ideas.

In any case, you should also read the comments in
./runtime/module-template.h. Output plugins are built based on a large
set of code-generating macros. These macros handle most of the plumbing
needed by the interface. As long as no special callback to rsyslog is
needed (it typically is not), an output plugin does not really need to
be aware that it is executed by rsyslog. As a plug-in programmer, you
can (in most cases) "code as usual". However, all macros and entry
points need to be provided and thus reading the code comments in the
files mentioned is highly suggested.

For testing, you need rsyslog's debugging support. Some useful
information is given in "`troubleshooting rsyslog <troubleshoot.html>`_
from the doc set.

Special Topics
--------------

Threading
~~~~~~~~~

Rsyslog uses massive parallel processing and multithreading. However, a
plugin's entry points are guaranteed to be never called concurrently
**for the same action**. That means your plugin must be able to be
called concurrently by two or more threads, but you can be sure that for
the same instance no concurrent calls happen. This is guaranteed by the
interface specification and the rsyslog core guards against multiple
concurrent calls. An instance, in simple words, is one that shares a
single instanceData structure.

So as long as you do not mess around with global data, you do not need
to think about multithreading (and can apply a purely sequential
programming methodology).

Please note that during the configuration parsing stage of execution,
access to global variables for the configuration system is safe. In that
stage, the core will only call sequentially into the plugin.

Getting Message Data
~~~~~~~~~~~~~~~~~~~~

The doAction() entry point of your plugin is provided with messages to
be processed. It will only be activated after filtering and all other
conditions, so you do not need to apply any other conditional but can
simply process the message.

Note that you do NOT receive the full internal representation of the
message object. There are various (including historical) reasons for
this and, among others, this is a design decision based on security.

Your plugin will only receive what the end user has configured in a
$template statement. However, starting with 4.1.6, there are two ways of
receiving the template content. The default mode, and in most cases
sufficient and optimal, is to receive a single string with the expanded
template. As I said, this is usually optimal, think about writing things
to files, emailing content or forwarding it.

The important philosophy is that a plugin should **never** reformat any
of such strings - that would either remove the user's ability to fully
control message formats or it would lead to duplicating code that is
already present in the core. If you need some formatting that is not yet
present in the core, suggest it to the rsyslog project, best done by
sending a patch ;), and we will try hard to get it into the core (so
far, we could accept all such suggestions - no promise, though).

If a single string seems not suitable for your application, the plugin
can also request access to the template components. The typical use case
seems to be databases, where you would like to access properties via
specific fields. With that mode, you receive a char \*\* array, where
each array element points to one field from the template (from left to
right). Fields start at array index 0 and a NULL pointer means you have
reached the end of the array (the typical Unix "poor man's linked list
in an array" design). Note, however, that each of the individual
components is a string. It is not a date stamp, number or whatever, but
a string. This is because rsyslog processes strings (from a high-level
design look at it) and so this is the natural data type. Feel free to
convert to whatever you need, but keep in mind that malformed packets
may have lead to field contents you'd never expected...

If you like to use the array-based parameter passing method, think that
it is only available in rsyslog 4.1.6 and above. If you can accept that
your plugin will not be working with previous versions, you do not need
to handle pre 4.1.6 cases. However, it would be "nice" if you shut down
yourself in these cases - otherwise the older rsyslog core engine will
pass you a string where you expect the array of pointers, what most
probably results in a segfault. To check whether or not the core
supports the functionality, you can use this code sequence:

::


    BEGINmodInit()
        rsRetVal localRet;
        rsRetVal (*pomsrGetSupportedTplOpts)(unsigned long *pOpts);
        unsigned long opts;
        int bArrayPassingSupported;     /* does core support template passing as an array? */
    CODESTARTmodInit
        *ipIFVersProvided = CURR_MOD_IF_VERSION; /* we only support the current interface specification */
    CODEmodInit_QueryRegCFSLineHdlr
        /* check if the rsyslog core supports parameter passing code */
        bArrayPassingSupported = 0;
        localRet = pHostQueryEtryPt((uchar*)"OMSRgetSupportedTplOpts", &pomsrGetSupportedTplOpts);
        if(localRet == RS_RET_OK) {
            /* found entry point, so let's see if core supports array passing */
            CHKiRet((*pomsrGetSupportedTplOpts)(&opts));
            if(opts & OMSR_TPL_AS_ARRAY)
                bArrayPassingSupported = 1;
        } else if(localRet != RS_RET_ENTRY_POINT_NOT_FOUND) {
            ABORT_FINALIZE(localRet); /* Something else went wrong, what is not acceptable */
        }
        DBGPRINTF("omstdout: array-passing is %ssupported by rsyslog core.\n", bArrayPassingSupported ? "" : "not ");

        if(!bArrayPassingSupported) {
            DBGPRINTF("rsyslog core too old, shutting down this plug-in\n");
            ABORT_FINALIZE(RS_RET_ERR);
        }

The code first checks if the core supports the OMSRgetSupportedTplOpts()
API (which is also not present in all versions!) and, if so, queries the
core if the OMSR\_TPL\_AS\_ARRAY mode is supported. If either does not
exits, the core is too old for this functionality. The sample snippet
above then shuts down, but a plugin may instead just do things
different. In omstdout, you can see how a plugin may deal with the
situation.

**In any case, it is recommended that at least a graceful shutdown is
made and the array-passing capability not blindly be used.** In such
cases, we can not guard the plugin from segfaulting and if the plugin
(as currently always) is run within rsyslog's process space, that
results in a segfault for rsyslog. So do not do this.

Another possible mode is OMSR\_TPL\_AS\_JSON, where instead of the
template a json-c memory object tree is passed to the module. The module
can extract data via json-c API calls. It MUST NOT modify the provided
structure. This mode is primarily aimed at plugins that need to process
tree-like data, as found for example in MongoDB or ElasticSearch.

Batching of Messages
~~~~~~~~~~~~~~~~~~~~

Starting with rsyslog 4.3.x, batching of output messages is supported.
Previously, only a single-message interface was supported.

With the **single message** plugin interface, each message is passed via
a separate call to the plugin. Most importantly, the rsyslog engine
assumes that each call to the plugin is a complete transaction and as
such assumes that messages be properly committed after the plugin returns
to the engine.

With the **batching** interface, rsyslog employs something along the
line of "transactions". Obviously, the rsyslog core can not make
non-transactional outputs to be fully transactional. But what it can is
support that the output tells the core which messages have been committed
by the output and which not yet. The core can than take care of those
uncommitted messages when problems occur. For example, if a plugin has
received 50 messages but not yet told the core that it committed them,
and then returns an error state, the core assumes that all these 50
messages were **not** written to the output. The core then requeues all
50 messages and does the usual retry processing. Once the output plugin
tells the core that it is ready again to accept messages, the rsyslog
core will provide it with these 50 not yet committed messages again
(actually, at this point, the rsyslog core no longer knows that it is
re-submitting the messages). If, in contrary, the plugin had told rsyslog
that 40 of these 50 messages were committed (before it failed), then only
10 would have been requeued and resubmitted.

In order to provide an efficient implementation, there are some (mild)
constraints in that transactional model: first of all, rsyslog itself
specifies the ultimate transaction boundaries. That is, it tells the
plugin when a transaction begins and when it must finish. The plugin is
free to commit messages in between, but it **must** commit all work done
when the core tells it that the transaction ends. All messages passed in
between a begin and end transaction notification are called a batch of
messages. They are passed in one by one, just as without transaction
support. Note that batch sizes are variable within the range of 1 to a
user configured maximum limit. Most importantly, that means that plugins
may receive batches of single messages, so they are required to commit
each message individually. If the plugin tries to be "smarter" than the
rsyslog engine and does not commit messages in those cases (for
example), the plugin puts message stream integrity at risk: once rsyslog
has notified the plugin of transaction end, it discards all messages as
it considers them committed and save. If now something goes wrong, the
rsyslog core does not try to recover lost messages (and keep in mind
that "goes wrong" includes such uncontrollable things like connection
loss to a database server). So it is highly recommended to fully abide
to the plugin interface details, even though you may think you can do it
better. The second reason for that is that the core engine will have
configuration settings that enable the user to tune commit rate to their
use-case specific needs. And, as a relief: why would rsyslog ever decide
to use batches of one? There is a trivial case and that is when we have
very low activity so that no queue of messages builds up, in which case
it makes sense to commit work as it arrives. (As a side-note, there are
some valid cases where a timeout-based commit feature makes sense. This
is also under evaluation and, once decided, the core will offer an
interface plus a way to preserve message stream integrity for
properly-crafted plugins).

The second restriction is that if a plugin makes commits in between
(what is perfectly legal) those commits must be in-order. So if a commit
is made for message ten out of 50, this means that messages one to nine
are also committed. It would be possible to remove this restriction, but
we have decided to deliberately introduce it to simplify things.

Output Plugin Transaction Interface
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

In order to keep compatible with existing output plugins (and because it
introduces no complexity), the transactional plugin interface is build
on the traditional non-transactional one. Well... actually the
traditional interface was transactional since its introduction, in the
sense that each message was processed in its own transaction.

So the current
``doAction() entry point can be considered to have this structure (from the transactional interface point of view):``

::

    doAction()
        {
        beginTransaction()
        ProcessMessage()
        endTransaction()
        }

For the **transactional interface**, we now move these implicit
``beginTransaction()`` and ``endTransaction(()`` call out of the message
processing body, resulting is such a structure:

::

    beginTransaction()
        {
        /* prepare for transaction */
        }

    doAction()
        {
        ProcessMessage()
        /* maybe do partial commits */
        }

    endTransaction()
        {
        /* commit (rest of) batch */
        }

And this calling structure actually is the transactional interface! It
is as simple as this. For the new interface, the core calls a
``beginTransaction()`` entry point inside the plugin at the start of the
batch. Similarly, the core call ``endTransaction()`` at the end of the
batch. The plugin must implement these entry points according to its
needs.

But how does the core know when to use the old or the new calling
interface? This is rather easy: when loading a plugin, the core queries
the plugin for the ``beginTransaction()`` and ``endTransaction()`` entry
points. If the plugin supports these, the new interface is used. If the
plugin does not support them, the old interface is used and rsyslog
implies that a commit is done after each message. Note that there is no
special "downlevel" handling necessary to support this. In the case of
the non-transactional interface, rsyslog considers each completed call
to ``doAction`` as partial commit up to the current message. So
implementation inside the core is very straightforward.

Actually, **we recommend that the transactional entry points only be
defined by those plugins that actually need them**. All others should
not define them in which case the default commit behaviour inside
rsyslog will apply (thus removing complexity from the plugin).

In order to support partial commits, special return codes must be
defined for ``doAction``. All those return codes mean that processing
completed successfully. But they convey additional information about the
commit status as follows:

+----------------------------------+-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------+
| *RS\_RET\_OK*                    | The record and all previous inside the batch has been committed. *Note:* this definition is what makes integrating plugins without the transaction being/end calls so easy - this is the traditional "success" return state and if every call returns it, there is no need for actually calling ``endTransaction()``, because there is no transaction open).      |
+----------------------------------+-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------+
| *RS\_RET\_DEFER\_COMMIT*         | The record has been processed, but is not yet committed. This is the expected state for transactional-aware plugins.                                                                                                                                                                                                                                              |
+----------------------------------+-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------+
| *RS\_RET\_PREVIOUS\_COMMITTED*   | The **previous** record inside the batch has been committed, but the current one not yet. This state is introduced to support sources that fill up buffers and commit once a buffer is completely filled. That may occur halfway in the next record, so it may be important to be able to tell the engine the everything up to the previous record is committed   |
+----------------------------------+-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------+

Note that the typical **calling cycle** is ``beginTransaction()``,
followed by *n* times ``doAction()`` followed by ``endTransaction()``.
However, if either ``beginTransaction()`` or ``doAction()`` return back
an error state (including RS\_RET\_SUSPENDED), then the transaction is
considered aborted. In result, the remaining calls in this cycle (e.g.
``endTransaction()``) are never made and a new cycle (starting with
``beginTransaction()`` is begun when processing resumes. So an output
plugin must expect and handle those partial cycles gracefully.

**The question remains how can a plugin know if the core supports
batching?** First of all, even if the engine would not know it, the
plugin would return with RS\_RET\_DEFER\_COMMIT, what then would be
treated as an error by the engine. This would effectively disable the
output, but cause no further harm (but may be harm enough in itself).

The real solution is to enable the plugin to query the rsyslog core if
this feature is supported or not. At the time of the introduction of
batching, no such query-interface exists. So we introduce it with that
release. What the means is if a rsyslog core can not provide this query
interface, it is a core that was build before batching support was
available. So the absence of a query interface indicates that the
transactional interface is not available. One might now be tempted to
think there is no need to do the actual check, but it is recommended to
ask the rsyslog engine explicitly if the transactional interface is
present and will be honored. This enables us to create versions in the
future which have, for whatever reason we do not yet know, no support
for this interface.

The logic to do these checks is contained in the ``INITChkCoreFeature``
macro, which can be used as follows:

::

    INITChkCoreFeature(bCoreSupportsBatching, CORE_FEATURE_BATCHING);

Here, bCoreSupportsBatching is a plugin-defined integer which after
execution is 1 if batches (and thus the transactional interface) is
supported and 0 otherwise. CORE\_FEATURE\_BATCHING is the feature we are
interested in. Future versions of rsyslog may contain additional
feature-test-macros (you can see all of them in ./runtime/rsyslog.h).

Note that the ompsql output plugin supports transactional mode in a
hybrid way and thus can be considered good example code.

Open Issues
-----------

-  Processing errors handling
-  reliable re-queue during error handling and queue termination

Licensing
~~~~~~~~~

From the rsyslog point of view, plugins constitute separate projects. As
such, we think plugins are not required to be compatible with GPLv3.
However, this is not legal advise. If you intend to release something
under a non-GPLV3 compatible license it is probably best to consult with
your lawyer.

Most importantly, and this is definite, the rsyslog team does not expect
or require you to contribute your plugin to the rsyslog project (but of
course we are happy if you do).
