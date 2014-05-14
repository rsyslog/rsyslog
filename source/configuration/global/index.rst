Global Configuration Statements
===============================
Global configuration statements, as their name implies, usually affect
some global features. However, some are specific to a set of input
modules. Finally, we also include some directives that modify only
a single input module, which is nowhere else documented.

True Global Directives
----------------------

.. toctree::
   :glob:

   options/rsconf1_abortonuncleanconfig
   options/rsconf1_debugprintcfsyslinehandlerlist
   options/rsconf1_debugprintmodulelist
   options/rsconf1_debugprinttemplatelist
   options/rsconf1_failonchownfailure
   options/rsconf1_generateconfiggraph
   options/rsconf1_includeconfig
   options/rsconf1_mainmsgqueuesize
   options/rsconf1_maxopenfiles
   options/rsconf1_moddir
   options/rsconf1_modload
   options/rsconf1_umask
   options/rsconf1_resetconfigvariables

Directives affecting multiple Input Modules
-------------------------------------------
While these directives only affect input modules, they are global in
the sense that they cannot be overwritten for specific input
instances. So they apply globally for all inputs that support these
directives.

.. toctree::
   :glob:

   options/rsconf1_allowedsender
   options/rsconf1_dropmsgswithmaliciousdnsptrrecords
   options/rsconf1_controlcharacterescapeprefix
   options/rsconf1_droptrailinglfonreception
   options/rsconf1_escape8bitcharsonreceive
   options/rsconf1_escapecontrolcharactersonreceive

immark-specific Directives
--------------------------

.. toctree::
   :glob:

   options/rsconf1_markmessageperiod
