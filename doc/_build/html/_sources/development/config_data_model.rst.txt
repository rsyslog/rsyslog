The rsyslog config data model
=============================

This document describes the config data model on a high layer.
For details, it is suggested to review the actual source code.
The aim of this document is to provide general understanding for
both rsyslog developers as well as developers writing config
management systems.

Objects
=======
Most config objects live in a flat space and are global to rsyslog.
However, actual rule processing is done via a script-like language.
These config scripts need to be represented via a tree structure.

Note that the language as currently implemented is Turing-complete
if the user makes use of very tricky constructs. It was never our
intention to provide a Turing-complete language and we will probably
try to disable these tricks in the future. However, this is not a
priority for us, as these users get what they deserve. For someone
involved with the config, it probably is sufficient to know that
loops are **not** supported by the config language (even though you
can create loop-like structures). Thus, a tree is fully sufficient
to represent any configuration.

In the following sections, we'll quickly describe variables/properties,
flat structure elements and the execution tree.

Variables/Properties
--------------------
Rsyslog supports

* traditional syslog (RFC-based) message properties
* structured data content, including any non-syslog properties
* Variables

  - global
  - local
  - message-enhancing (like message properties)

A description of these properties and variables is available elsewhere. As
far as a config processor is concerned, the important thing to know is
that they be used during template definitions and script operations.

Flat Elements
-------------

Global Parameters
^^^^^^^^^^^^^^^^^
This element must contain all global parameters settable by rsyslog. 
This includes elements from the global() as well as main_queue() config
statements. As of this writing, some global parameter can only be set
by legacy statements.

Note that main_queue() actually is a full queue definition.

Modules
^^^^^^^
This contains all loaded modules, among others:

* input modules
* output modules
* message modification modules
* message parsers

Note that for historical reasons some output modules are directly linked
into rsyslog and must not be specified.

Each module must be given only once. The data object must contain all
module-global parameters.

Inputs
^^^^^^
Describes all defined inputs with their parameters. Is build from the
input() statement or its legacy equivalent (ugly). Contains links to

* module used for input
* ruleset used for processing

Rulesets
^^^^^^^^
They contain the tree-like execution structure. However, rulesets
itself are flat and cannot be nested. Note that there exists statements
that permit rulesets to call into each other, but all rulesets are in
the same flat top-level space.

Note that a ruleset has an associated queue object which (by default)
operates in direct mode. As a reminder, direct queues do not queue or
buffer any of the queue elements. In most cases this is sufficient,
but if the ruleset is bound to an input or is used to run
multiple actions independently (e.g., forwarding messages to two
destinations), then you should configure the associated queue object
as a real queue.

See the :doc:`Understanding rsyslog Queues <../concepts/queues>` or
:doc:`Turning Lanes and Rsyslog Queues <../whitepapers/queues_analogy>` docs
for more information.

Hierarchical Elements
---------------------
These are used for rule execution. They are somewhat hard to fit into a
traditional config scheme, as they provide full tree-like branching
structure.

Basically, a tree consists of statements and evaluations. Consider the
ruleset to be the root of the execution tree. It is rather common that
the tree's main level is a long linked list, with only actions being
branched out. This, for example, happens with a traditional
rsyslog.conf setting, which only contains files to be written based
on some priority filters. However, one must not be tricked into
thinking that this basic case is sufficient to support as enterprise
users typically create far more complex cases.

In essence, rsyslog walks the tree, and executes statements while it
does so. Usually, a filter needs to be evaluated and execution branches
based on the filter outcome. The tree actually **is** an AST.

Execution Statements
^^^^^^^^^^^^^^^^^^^^
These are most easy to implement as they are end nodes (and as such
nothing can be nested under them). They are most importantly created by
the action() config object, but also with statements like "set"
and "unset". Note that "call" is also considered a terminal node, even
though it executes *another* ruleset.

Note that actions have associated queues, so a queue object and its
parameter need to be present. When building configurations interactively,
it is suggested that the default is either not to configure queue parameters
by default or to do this only for actions where it makes sense (e.g.
connection to remote systems which may go offline).

Expression Evaluation
^^^^^^^^^^^^^^^^^^^^^
A full expression evaluation engine is available who does the typical
programming-language type of expression processing. The usual mathematical,
boolean and string operations are supported, as well as functions. As of
this writing, functions are hard-coded into rsyslog but may in the future
be part of a loadable module. Evaluations can access all rsyslog properties
and variables. They may be nested arbitrarily deep.

Control-of-Flow Statements
^^^^^^^^^^^^^^^^^^^^^^^^^^
Remember that rsyslog does intentionally not support loop statements. So
control-of-flow boils down to

* conditional statements

  - "if ... then ... else ..."
  - syslog PRI-based filters
  - property-based filters

* stop

Where "stop" terminates processing of this message. The conditional statements 
contain subbranches, where "if" contains both "then" and "else" subbranches
and the other two only the "then" subbranch (Note: inside the execution
engine, the others may also have "else" branches, but these are result 
of the rsyslog config optimizer run and cannot configured by the user).

When executing a config script, rsyslog executes the subbranch in question
and then continues to evaluate the next statement in the currently
executing branch that contained the conditional statement. If there is no
next statement, it goes up one layer. This is continued until the last
statement of the root statement list is reached. At that point execution
of the message is terminated and the message object destructed.
Again, think AST, as this is exactly what it is.

Note on Queue Objects
---------------------
Queue objects are **not** named objects inside the rsyslog configuration.
So their data is always contained with the object that uses the queue
(action(), ruleset(), main_queue()). From a UI perspective, this
unfortunately tends to complicate a config builder a bit.
