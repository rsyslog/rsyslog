Control Structures
==================

Control structures in RainerScript are similar in semantics to a lot 
of other mainstream languages such as C, Java, Javascript, Ruby, 
Bash etc.
So this section assumes the reader is familier with semantics of such 
structures, and goes about describing RainerScript implementation in 
usage-example form rather than by formal-definition and 
detailed semantics documentation.

RainerScript supports following control structures:

**if**:

::

   if ($msg contains "important") then {
      if ( $. foo != "" ) then set $.foo = $.bar & $.baz;
      action(type="omfile" file="/var/log/important.log" template="outfmt")
   }

**if/else-if/else**:

::

   if ($msg contains "important") then {
      set $.foo = $.bar & $.baz;
      action(type="omfile" file="/var/log/important.log" template="outfmt")
   } else if ($msg startswith "slow-query:") then {
      action(type="omfile" file="/var/log/slow_log.log" template="outfmt")
   } else {
      set $.foo = $.quux;
      action(type="omfile" file="/var/log/general.log" template="outfmt")
   }

**foreach**:

::

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
almost always set *action.copyMsg* to *on*. This is because action calls within foreach
usually want to work with the variable loop populates(in the above example, $.quux and $.corge)
which causes message-mutation and async-action must see message as it was in
a certain invocation of loop-body, so they must make a copy to keep it safe
from further modification as iteration continues. For instance, an async-action
invocation with linked-list based queue would look like:

::

   foreach ($.quux in $!foo) do {
      action(type="omfile" file="./rsyslog.out.log" template="quux" queue.type="linkedlist" action.copyMsg="on")
   }


**call**: :doc:`rainerscript_call`
   
**continue**: a NOP, useful e.g. inside the then part of an if
