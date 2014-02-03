Message Modification Modules
----------------------------

Message modification modules are used to change the content of messages
being processed. They can be implemented using either the output module
or the parser module interface. From the rsyslog core's point of view,
they actually are output or parser modules, it is their implementation
that makes them special.

Currently, there do not exist any such modules, but could be written
with the methods the engine provides. They could be used, for example,
to:

-  anonymize message content
-  add dynamically computed content to message (fields)

Message modification modules are usually written for one specific task
and thus usually are not generic enough to be reused. However, existing
module's code is probably an excellent starting base for writing a new
module. Currently, the following modules existin inside the source tree

.. toctree::
   :glob:

   mm*

