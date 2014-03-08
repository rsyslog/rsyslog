Basic Structure
===============
Rsyslog supports standard sysklogd's configuration file format and
extends it. So in general, you can take a "normal" syslog.conf and use
it together with rsyslogd. It will understand everything. However, to
use most of rsyslogd's unique features, you need to add extended
configuration directives.

Rsyslogd supports the classical, selector-based rule lines. They are
still at the heart of it and all actions are initiated via rule lines. A
rule lines is any line not starting with a $ or the comment sign (#).
Lines starting with $ carry rsyslog-specific directives.

Every rule line consists of two fields, a selector field and an action
field. These two fields are separated by one or more spaces or tabs. The
selector field specifies a pattern of facilities and priorities
belonging to the specified action.

Lines starting with a hash mark ("#'') and empty lines are ignored.
 
Configuration File Syntax Differences
-------------------------------------
Rsyslogd uses a slightly different syntax for its configuration file
than the original BSD sources. Originally all messages of a specific
priority and above were forwarded to the log file. The modifiers "='',
"!'' and "!-'' were added to make rsyslogd more flexible and to use it
in a more intuitive manner.

The original BSD syslogd doesn't understand spaces as separators
between the selector and the action field.

When compared to syslogd from sysklogd package, rsyslogd offers
additional `features <features.html>`_ (like template and database
support). For obvious reasons, the syntax for defining such features is
available in rsyslogd, only.

Basic rsyslog.conf Structure
============================

This is a part of the rsyslog.conf documentation.

Rsyslog supports three different types of configuration statements
concurrently:

-  **sysklogd** - this is the plain old format, thaught everywhere and
   still pretty useful for simple use cases. Note that some very few
   constructs are no longer supported because they are incompatible with
   newer features. These are mentioned in the compatibility docs.
-  **legacy rsyslog** - these are statements that begin with a dollar
   sign. They set some configuration parameters and modify e.g. the way
   actions operate. This is the only format supported in pre-v6 versions
   of rsyslog. It is still fully supported in v6 and above. Note that
   some plugins and features may still only be available through legacy
   format (because plugins need to be explicitely upgraded to use the
   new style format, and this hasn't happened to all plugins).
-  **RainerScript** - the new style format. This is the best and most
   precise format to be used for more complex cases. The rest of this
   page assumes RainerScript based rsyslog.conf.

The rsyslog.conf files consists of statements. For old style (sysklogd &
legacy rsyslog), lines do matter. For new style (RainerScript) line
spacing is irrelevant. Most importantly, this means with new style
actions and all other objects can split across lines as users want to.

Comments
--------

There are two types of comments:

-  **#-Comments** - start with a hash sign (#) and run to the end of the
   line
-  **C-style Comments** - start with /\* and end with \*/, just like in
   the C programming language. They can be used to comment out multiple
   lines at one. Comment nesting is not supported, but #-Comments can be
   contained inside a C-style comment.

Processing Order
----------------

Directives are processed from the top of rsyslog.conf to the bottom.
Sequence matters. For example, if you stop processing of a message,
obviously all statements after the stop statement are never evaluated.

Flow Control Statements
~~~~~~~~~~~~~~~~~~~~~~~

-  **if expr then ... else ...** - conditional execution
-  **stop** - stops processing the current message
-  **`call <rainerscript_call.html>`_** - calls a ruleset (just like a
   subroutine call)
-  **continue** - a NOP, useful e.g. inside the then part of an if

Data Manipulation Statements
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

-  **set** -
   `sets <http://www.rsyslog.com/how-to-set-variables-in-rsyslog-v7/>`_
   a user variable
-  **unset** - deletes a previously set user variable

Inputs
------

Every input requires an input module to be loaded and a listener defined
for it. Full details can be found inside the `rsyslog
modules <rsyslog_conf_modules.html>`_ documentation. Once loaded, inputs
are defined via the **input()** object.

Outputs
-------

Outputs are also called "actions". A small set of actions is pre-loaded
(like the output file writer, which is used in almost every
rsyslog.conf), others must be loaded just like inputs.

An action is invoked via the **action(type="type" ...)** object. Type is
mandatory and must contain the name of the plugin to be called (e.g.
"omfile" or "ommongodb"). Other paramters may be present. Their type and
use depends on the output plugin in question.

Rulesets and Rules
------------------

Rulesets and rules form the basis of rsyslog processing. In short, a
rule is a way how rsyslog shall process a specific message. Usually,
there is a type of filter (if-statement) in front of the rule. Complex
nesting of rules is possible, much like in a programming language.

Rulesets are containers for rules. A single ruleset can contain many
rules. In the programming language analogy, one may think of a ruleset
like being a program. A ruleset can be "bound" (assigned) to a specific
input. In the analogy, this means that when a message comes in via that
input, the "program" (ruleset) bound to it will be executed (but not any
other!).

There is detail documentation available for `rsyslog
rulesets <multi_ruleset.html>`_.

For quick reference, rulesets are defined as follows:

::

    ruleset(name="rulesetname") {
        action(type="omfile" file="/path/to/file")
        action(type="..." ...)
        /* and so on... */
    }

