RainerScript
============

**RainerScript is a scripting language specifically designed and
well-suited for processing network events and configuring event
processors** (with the most prominent sample being syslog). While
RainerScript is theoritically usable with various softwares, it
currently is being used, and developed for, rsyslog. Please note that
RainerScript may not be abreviated as rscript, because that's somebody
elses trademark.

RainerScript is currently under development. It has its first appearance
in rsyslog 3.12.0, where it provides complex expression support.
However, this is only a very partial implementatio of the scripting
language. Due to technical restrictions, the final implementation will
have a slightly different syntax. So while you are invited to use the
full power of expresssions, you unfortunatley need to be prepared to
change your configuration files at some later points. Maintaining
backwards-compatibility at this point would cause us to make too much
compromise. Defering the release until everything is perfect is also not
a good option. So use your own judgement.

A formal definition of the language can be found in `RainerScript
ABNF <rscript_abnf.html>`_. The rest of this document describes the
language from the user's point of view. Please note that this doc is
also currently under development and can (and will) probably improve as
time progresses. If you have questions, use the rsyslog forum. Feedback
is also always welcome.


.. toctree::
   :maxdepth: 2
   
   rscript_abnf
   data_types
   expressions
   functions
   





