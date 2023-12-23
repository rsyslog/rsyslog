Converting older formats to |FmtAdvancedName|
=============================================

First of all, converting of older formats is not strictly necessary. All
formats can be mixed and matched and play well together.

There are stil a number of reasons to convert older formats:

* existing simple constructs need to be enhanced and become more complex
* aid future extensions
* ensure no side-effects accidentally occur
* unify rsyslog.conf language

Do not overdo conversion
~~~~~~~~~~~~~~~~~~~~~~~~

Note: simple facility and severity based filters which trigger writing to
files can actually be very well expressd in |FmtBasicName|. So if you have
something like::

    mail.info   /var/log/maillog

We suggest you leave it as-is without conversion. Equally, in our opinion it is
also fine to add new rules like the above. If you still want to convert, the
line may look as follows (completely in new format)::

    if prifilt("mail.info") then {
         action(type="omfile" file="/var/log/maillog")
    }

More compact, this can also be written like::

    if prifilt("mail.info") then action(type="omfile" file="/var/log/maillog")

The older-selector-style filter is well-known, so this may also write it as::

    mail.info action(type="omfile" file="/var/log/maillog")

There are ample additional possibilities. We suggest to keep things consistent.

Converting Module Load
~~~~~~~~~~~~~~~~~~~~~~
This is very straight-forward. In |FmtObsoleteName| format we use::

    $ModLoad module-name

This is very simply converted to::

    module(load="module-name")

Sometimes modules provide global settings. In |FmtObsoleteName| format these are given in
individual lines **after** the \$ModLoad. In |FmtAdvancedName| format they are given inside
the module object. This makes it much clearer which module they belong to and
that they actually are global parameters (in contrast to per-action or per-listener
parameters). A typical example is `imtcp`::

   $ModLoad imtcp
   $InputTCPMaxSession 500

This is converted to::

   module(load="imtcp" maxSessions="500")

Note: in |FmtObsoleteName| format it is possible to provide global parameters more than once.
In this case it is unclear which one actually applies. For example::

   $ModLoad imtcp
   $InputTCPMaxSession 500
   ...
   *.* /var/log/messages
   ...
   $InputTCPMaxSession 200

This is especially problematic if module-global parameters are used multiple times
in include files.

In |FmtAdvancedName| format this is no longer possible. Module-global parameters can only
be applied once when the module is loaded. Any attempt to change them afterwards
results in an error message and will be ignored. The error messages will help you
find and fix multiple settings. Let us assume "200" is the setting actually intended
in above config snippet. So it would be converted to::

   module(load="imtcp" maxSessions="200")
   ...
   *.* /var/log/messages
   ...


Converting Actions
~~~~~~~~~~~~~~~~~~
In general, you have lines like::

    filter action

where *filter* is any of the filters and *action* is ... the action to be
carried out. As could be seen above, the filter does **not** necessarily
need to be changed in order to convert the action. All filters also work
with all config formats. It often is best to keep existing filters, at
least while working on the conversion (do not change too many things at once).

The following table lists traditional action syntax and how it can be
converted to new-style ``action()`` objects. The link will bring you to
detail documentation. In these detail documentations all parameters are given.
It is also specified which |FmtObsoleteName| directives map to |FmtAdvancedName|
properties.
This table is not conclusive but covers the most commonly used actions.

.. csv-table::
   :header: "|FmtBasicName|", "|FmtAdvancedName|"
   :widths: auto
   :class: parameter-table

   "file path (`/var/log/...`)", "action(type="":doc:`omfile <modules/omfile>`"" file=""/var/log.../"" ...)"
   "UDP forwarding (`@remote`)", "action(type="":doc:`omfwd <modules/omfwd>`"" target=""remote"" protocol=""udp"" ...)"
   "TCP forwarding (`@@remote`)", "action(type="":doc:`omfwd <modules/omfwd>`"" target=""remote"" protocol=""tcp"" ...)"
   "user notify (``:omusrmsg:user``)", "action(type="":doc:`omusrmsg <modules/omusrmsg>`"" users=""user"" ...)"
   "module name (``:omxxx:..``)", "action(type="":doc:`omxxx <modules/idx_output>`"" ...)"

Some concrete examples::

  OLD: :hostname, contains, "remote-sender" @@central
  NEW: :hostname, contains, "remote-sender" action(type="omfwd" target="central" protocol="tcp")

  OLD: if $msg contains "error" then @central
  NEW: if $msg contains "error" then action(type="omfwd" target="central" protocol="udp")

  OLD: *.emerg :omusrmsg:*
  NEW: *.emerg action(type="omusrmsg" users="*")

**NOTE:** Some actions do not have a |FmtBasicName| format configuration line. They may
only be called via the ``action()`` syntax. Similarly, some very few actions,
mostly contributed, do not support ``action()`` syntax and thus can only be
configured via |FmtBasicName| and |FmtObsoleteName|. See module doc for details.


Action with Multiple Parameters
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
In many cases, actions have additional parameters, which also need to be converted. In
|FmtObsoleteName| format the action parameters are given before the actual action call.
To convert such constructs, you need to map all |FmtObsoleteName| parameters to |FmtAdvancedName|
ones. To look these up, you need to turn to three different documentation pages:

* the :doc:`action object <actions>` itself
* the :doc:`output module <modules/idx_output>` that is called in the action (e.g. omfwd)
* the :doc:`queue documentation <../rainerscript/queue_parameters>` (if an action queue is used)

To find the parameter in question, you can other do an on-page search via the browser on these
pages. Often it is very convenient to just use the
`rsyslog doc search engine <https://www.rsyslog.com/doc/master/search.html?q=>`_:
Type the |FmtObsoleteName| format statement into the search box. Most often, one of the first
search results is the matching object description doc page.

Converting Action Chains
~~~~~~~~~~~~~~~~~~~~~~~~

Actions can be chained via the ampersand character ('``&``'). In |FmtAdvancedName|
format this has been replaced by blocks. For example::

   *.error /var/log/errorlog
   &       @remote

becomes::

   *.error {
           action(type="omfile" file="/var/log/errorlog")
	   action(type="omfwd" target="remote" protocol="udp")
   }

The latter is much easier to understand and less error-prone when extended.

A common construct is to send messages to a remote host based on some message
content and then not further process it. This involves the ``stop`` statement
(or it's very old-time equivalent tilde ('``~``'). It may be specfied as such::

   :msg, contains, "error" @remote
   & ~

which is equavalent to::

   :msg, contains, "error" @remote
   & stop

This format is often found in more modern distro's rsyslog.conf. It again is
fully equivalent to::

   :msg, contains, "error" {
	   action(type="omfwd" target="remote" protocol="udp")
           stop
   }

And, just to prove the point, this is also exactly the same like::

   if $msg contains "error" then {
	   action(type="omfwd" target="remote" protocol="udp")
           stop
   }
