.. _conf_formats:

Configuration Formats
=====================

.. meta::
   :description: Overview of rsyslog configuration formats: basic (sysklogd), advanced (RainerScript), YAML, and the obsolete legacy format.
   :keywords: configuration format, rainerscript, yaml, sysklogd, legacy, conf_formats

.. summary-start

rsyslog supports four configuration formats. For new configs use RainerScript (advanced); YAML is a good alternative if you are more comfortable with YAML syntax.

.. summary-end

Rsyslog has evolved over several decades. For this reason, it supports four
different configuration formats:

-  |FmtBasicName| - previously known as the :doc:`sysklogd  <sysklogd_format>`
   format. Best for simple, one-line configurations matching on facility/severity
   and writing to a log file.

-  |FmtAdvancedName| - previously known as the ``RainerScript`` format. The
   recommended format for all non-trivial use cases. First available in rsyslog v6,
   it handles advanced filtering, forwarding, queueing, and custom actions.

-  |FmtYamlName| - an alternative syntax for users more comfortable with
   YAML than with RainerScript. Every YAML key maps directly to the equivalent
   RainerScript parameter, so both formats share the same feature set.
   See :doc:`yaml_config` for full reference.

-  |FmtObsoleteName| - previously known as the ``legacy`` format. This format is
   **obsolete** and must not be used in new configurations.

Which Format Should I Use?
--------------------------

For New Configurations
~~~~~~~~~~~~~~~~~~~~~~

Use the |FmtAdvancedName| format for all new configurations due to its
flexibility, precision, and control. It handles complex use cases such as
advanced filtering, forwarding, and actions with specific parameters.

If You Prefer YAML
~~~~~~~~~~~~~~~~~~

If you are already familiar with YAML and find it more readable than
RainerScript, use the :doc:`YAML format <yaml_config>`.  The two formats
are functionally equivalent — you can also mix them: a YAML main config
may include RainerScript ``.conf`` fragments and vice versa.

Existing Configurations in Basic Format
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Some distributions ship default configurations in |FmtBasicName| format.
Converting them to |FmtAdvancedName| is straightforward and recommended
if you plan to add complex constructs such as rulesets or queued actions.

Retaining Basic Format
~~~~~~~~~~~~~~~~~~~~~~

Continue using the |FmtBasicName| format if there is a strong reliance on
external documentation or many existing configurations in that format.

Example - Basic Format
^^^^^^^^^^^^^^^^^^^^^^

::

    mail.info /var/log/mail.log
    mail.err @@server.example.net

Advanced Use Cases
~~~~~~~~~~~~~~~~~~

For anything beyond basic logging, use the |FmtAdvancedName| format:

- Fine control via advanced parameters
- Easy-to-follow block structure
- Safe for use with include files

Example - Advanced Format
^^^^^^^^^^^^^^^^^^^^^^^^^

::

    mail.err action(type="omfwd" protocol="tcp" queue.type="linkedList")

Deprecated Format
~~~~~~~~~~~~~~~~~

Do not use |FmtObsoleteName| format. **It is obsolete and will make your
life difficult.** It exists solely for backward compatibility with very
old configurations.

Conclusion
----------

Use |FmtAdvancedName| for new configurations, or the :doc:`YAML format
<yaml_config>` if you prefer YAML syntax.  The |FmtBasicName| format is
acceptable for simple, existing configurations.  Never use |FmtObsoleteName|.
