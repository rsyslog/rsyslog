Basic Structure
===============

This section describes how rsyslog configuration basically works. Think
of rsyslog as a big logging and event processing toolset. It can be considered
as a framework with some basic processing that is fixed in the way data flows,
but is highly custumizable in the details of this message flow. During
configuration, this customization is done by defining and customizing
the rsyslog objects.

Quick overview of message flow and objects
------------------------------------------
Messages enter rsyslog with the help of input modules. Then, they are
passed to ruleset, where rules are conditionally applied. When a rule
matches, the message is transferred to an action, which then does 
something to the message, e.g. write it to a file or database or
forward it to a remote host.

Processing Principles
---------------------

- inputs submit received messages to rulesets

  * if the ruleset is not specifically bound, the default ruleset is used

- by default, there is one ruleset (RSYSLOG_DefaultRuleset)

- additional rulesets can be user-defined

- each ruleset contains of zero or many rules

  * while it is permitted to have zero rules inside a ruleset,
    this obviously makes no sense

- a rule consists of a filter and an action list

- filters provide yes/no decisions and thus control-of-flow capability

- if a filter "matches" (filter says "yes"), the corresponding
  action list is executed. If it does not match, nothing special
  happens

- rules are evaluated in sequence from the first to the last rule
  **inside** the given ruleset. No rules from unrelated rulesets are
  ever executed.

- all rules are **always** fully evaluated, no matter if a filter matches
  or not (so we do **not** stop at the first match). If message processing
  shall stop, the "discard" action (represented by the tilde character) must
  explicitely be executed. If discard is executed, message processing 
  immediately stops, without eveluating any further rules.

- an action list constains one or many actions

- inside an action list no further filters are possible

- to have more than one action inside a list, the ampersand character
  must be placed in the position of the filter, and this must immediately
  follow the previous action

- actions consist of the action call itself (e.g. ":omusrmsg:") as
  well as all action-defining configuration statements ($Action... directives)

- if legacy format is used (see below), $Action... directives **must** be
  specified in front of the action they are intended to configure

- some config directives automatically refer to their previous values 
  after being applied, others not. See the respective doc for details. Be
  warned that this is currently not always properly documented.

- in general, rsyslog v5 is heavily outdated and its native config language
  is a pain. The rsyslog project strongly recommends using at least version 7,
  where these problems are solved and configuration is much easier.



Configuration File
------------------
Upon startup, rsyslog reads its configuration from the ``rsyslog.conf``
file by default. This file may contain references to include other
config files.

A different "root" configuration file can be specified via the ``-f <file>``
rsyslogd command line option. This is usually done within some init
script or similiar facility.

Statement Types
---------------
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
-  :doc:`call <../rainerscript/rainerscript_call>` - calls a ruleset
   (just like a subroutine call)
-  **continue** - a NOP, useful e.g. inside the then part of an if

Flow control is also provided by :doc:`filter conditions <filters>`.

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

