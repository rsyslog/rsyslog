Modules
=======

Rsyslog has a modular design. This enables functionality to be
dynamically loaded from modules, which may also be written by any third
party. Rsyslog itself offers all non-core functionality as modules.
Consequently, there is a growing number of modules. Here is the entry
point to their documentation and what they do (list is currently not
complete)

Please note that each module provides (case-insensitive) configuration
parameters, which are NOT necessarily being listed below. Also remember,
that a modules configuration parameter (and functionality) is only
available if it has been loaded.

It is relatively easy to write a rsyslog module. If none of the
provided modules solve your need, you may consider writing one or have
one written for you by `Adiscon's professional services for
rsyslog <http://www.rsyslog.com/professional-services>`_ (this often
is a very cost-effective and efficient way of getting what you need).

There exist different classes of loadable modules:

.. toctree::
   :maxdepth: 1

   workflow
