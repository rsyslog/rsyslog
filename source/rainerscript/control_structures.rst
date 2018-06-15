******************
Control Structures
******************

Control structures in RainerScript are similar in semantics to a lot
of other mainstream languages such as C, Java, Javascript, Ruby,
Bash etc.
So this section assumes the reader is familiar with semantics of such
structures, and goes about describing RainerScript implementation in
usage-example form rather than by formal-definition and
detailed semantics documentation.

RainerScript supports following control structures:

if
==

.. code-block:: none

   if ($msg contains "important") then {
      if ( $.foo != "" ) then set $.foo = $.bar & $.baz;
      action(type="omfile" file="/var/log/important.log" template="outfmt")
   }


if/else-if/else
===============

.. code-block:: none

   if ($msg contains "important") then {
      set $.foo = $.bar & $.baz;
      action(type="omfile" file="/var/log/important.log" template="outfmt")
   } else if ($msg startswith "slow-query:") then {
      action(type="omfile" file="/var/log/slow_log.log" template="outfmt")
   } else {
      set $.foo = $.quux;
      action(type="omfile" file="/var/log/general.log" template="outfmt")
   }


foreach
=======

Foreach can iterate both arrays and objects. As opposed to array-iteration
(which is ordered), object-iteration accesses key-values in arbitrary order
(is unordered).

For the foreach invocation below:

.. code-block:: none
   
   foreach ($.i in $.collection) do {
      ...
   }


Say ``$.collection`` holds an array ``[1, "2", {"a": "b"}, 4]``, value
of ``$.i`` across invocations would be ``1``, ``"2"``, ``{"a" : "b"}``
and ``4``.

When ``$.collection`` holds an object
``{"a": "b", "c" : [1, 2, 3], "d" : {"foo": "bar"}}``, value of ``$.i``
across invocations would be ``{"key" : "a", "value" : "b"}``,
``{"key" : "c", "value" : [1, 2, 3]}`` and
``{"key" : "d", "value" : {"foo" : "bar"}}`` (not necessarily in the that
order). In this case key and value will need to be accessed as ``$.i!key``
and ``$.i!value`` respectively.



Here is an example of a nested foreach statement:

.. code-block:: none

   foreach ($.quux in $!foo) do {
      action(type="omfile" file="./rsyslog.out.log" template="quux")
      foreach ($.corge in $.quux!bar) do {
         reset $.grault = $.corge;
         action(type="omfile" file="./rsyslog.out.log" template="grault")
         if ($.garply != "") then
             set $.garply = $.garply & ", ";
         reset $.garply = $.garply & $.grault!baz;
      }
   }

Please note that asynchronous-action calls in foreach-statement body should
almost always set ``action.copyMsg`` to ``on``. This is because action calls
within foreach usually want to work with the variable loop populates (in the
above example, ``$.quux`` and ``$.corge``) which causes message-mutation and
async-action must see message as it was in a certain invocation of loop-body,
so they must make a copy to keep it safe from further modification as iteration
continues. For instance, an async-action invocation with linked-list based
queue would look like:

.. code-block:: none

   foreach ($.quux in $!foo) do {
       action(type="omfile" file="./rsyslog.out.log" template="quux
              queue.type="linkedlist" action.copyMsg="on")
   }


call
====

Details here: :doc:`rainerscript_call`


continue
========

A NOP, useful e.g. inside the ``then`` part of an if-structure.

