*******************************************
pmrfc3164: Parse RFC3164-formatted messages
*******************************************

===========================  ===========================================================================
**Module Name:**             **pmrfc3164**
**Author:**                  `Rainer Gerhards <https://rainer.gerhards.net/>`_ <rgerhards@adiscon.com>
===========================  ===========================================================================


Purpose
=======

This parser module is for parsing messages according to the traditional/legacy
syslog standard :rfc:`3164`

It is part of the default parser chain.

.. note::
   ``pmrfc3164`` is built into rsyslog and must not be loaded with
   ``module(load="pmrfc3164")``. Configure parameters via the
   ``parser`` directive.

The parser can also be customized to allow the parsing of specific formats,
if they occur.

Configuration Parameters
========================

.. note::
   Parameter names are case-insensitive; CamelCase is recommended for readability.

Parser Parameters
-----------------

.. list-table::
   :widths: 30 70
   :header-rows: 1

   * - Parameter
     - Summary
   * - :ref:`param-pmrfc3164-permit-squarebracketsinhostname`
     - .. include:: ../../reference/parameters/pmrfc3164-permit-squarebracketsinhostname.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-pmrfc3164-permit-slashesinhostname`
     - .. include:: ../../reference/parameters/pmrfc3164-permit-slashesinhostname.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-pmrfc3164-permit-atsignsinhostname`
     - .. include:: ../../reference/parameters/pmrfc3164-permit-atsignsinhostname.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-pmrfc3164-force-tagendingbycolon`
     - .. include:: ../../reference/parameters/pmrfc3164-force-tagendingbycolon.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-pmrfc3164-remove-msgfirstspace`
     - .. include:: ../../reference/parameters/pmrfc3164-remove-msgfirstspace.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-pmrfc3164-detect-yearaftertimestamp`
     - .. include:: ../../reference/parameters/pmrfc3164-detect-yearaftertimestamp.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-pmrfc3164-detect-headerless`
     - .. include:: ../../reference/parameters/pmrfc3164-detect-headerless.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-pmrfc3164-headerless-hostname`
     - .. include:: ../../reference/parameters/pmrfc3164-headerless-hostname.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-pmrfc3164-headerless-tag`
     - .. include:: ../../reference/parameters/pmrfc3164-headerless-tag.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-pmrfc3164-headerless-ruleset`
     - .. include:: ../../reference/parameters/pmrfc3164-headerless-ruleset.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-pmrfc3164-headerless-errorfile`
     - .. include:: ../../reference/parameters/pmrfc3164-headerless-errorfile.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-pmrfc3164-headerless-drop`
     - .. include:: ../../reference/parameters/pmrfc3164-headerless-drop.rst
        :start-after: .. summary-start
        :end-before: .. summary-end

Signal Handling
===============

HUP Signal Support
------------------

This parser module supports the HUP signal for log rotation when using the
``headerless.errorfile`` parameter. When rsyslog receives a HUP signal, the
module will:

1. Close the current headerless error file
2. Automatically reopen it on the next write operation

This allows external log rotation tools (like ``logrotate``) to safely rotate
the headerless error file by moving/renaming it and then sending a HUP signal
to rsyslog.

**Example log rotation configuration:**

.. code-block:: none

   /var/log/rsyslog-headerless.log {
       daily
       rotate 7
       compress
       delaycompress
       postrotate
           /bin/kill -HUP `cat /var/run/rsyslogd.pid 2> /dev/null` 2> /dev/null || true
       endscript
   }


Examples
========

Receiving malformed RFC3164 messages
------------------------------------

We assume a scenario where some of the devices send malformed RFC3164
messages. The parser module will automatically detect the malformed
sections and parse them accordingly.

.. code-block:: none

   module(load="imtcp")

   input(type="imtcp" port="514" ruleset="customparser")

   parser(name="custom.rfc3164"
         type="pmrfc3164"
         permit.squareBracketsInHostname="on"
         detect.YearAfterTimestamp="on")

   ruleset(name="customparser" parser="custom.rfc3164") {
    ... do processing here...
   }

.. toctree::
   :hidden:

   ../../reference/parameters/pmrfc3164-permit-squarebracketsinhostname.rst
   ../../reference/parameters/pmrfc3164-permit-slashesinhostname.rst
   ../../reference/parameters/pmrfc3164-permit-atsignsinhostname.rst
   ../../reference/parameters/pmrfc3164-force-tagendingbycolon.rst
   ../../reference/parameters/pmrfc3164-remove-msgfirstspace.rst
   ../../reference/parameters/pmrfc3164-detect-yearaftertimestamp.rst
   ../../reference/parameters/pmrfc3164-detect-headerless.rst
   ../../reference/parameters/pmrfc3164-headerless-hostname.rst
   ../../reference/parameters/pmrfc3164-headerless-tag.rst
   ../../reference/parameters/pmrfc3164-headerless-ruleset.rst
   ../../reference/parameters/pmrfc3164-headerless-errorfile.rst
   ../../reference/parameters/pmrfc3164-headerless-drop.rst

