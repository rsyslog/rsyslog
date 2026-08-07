Module Parameters
#################

The following parameters define **default settings** for the `omfwd` module.  
These values are used by all `omfwd` actions unless explicitly overridden 
by action parameters.

.. note::
   - Module parameters are configured by loading the built-in module with 
     ``module(load="builtin:omfwd" ...)``.
   - Action parameters (inside ``action(type="omfwd" ...)``) take precedence 
     over these defaults when both are set.

List of Module Parameters
=========================

StreamDriver.CAFile
^^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "string", "none", "no", "none"

Sets the default TLS CA file for ``omfwd`` actions that do not set
``StreamDriver.CAFile`` on the action itself. If the value is a strict
``pkcs11:`` URI, that module default is inherited only by actions whose
effective stream driver is ``ossl``.

StreamDriver.CRLFile
^^^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "string", "none", "no", "none"

Sets the default TLS certificate revocation list file for ``omfwd`` actions
that do not set ``StreamDriver.CRLFile`` on the action itself.

StreamDriver.CAExtraFiles
^^^^^^^^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "string", "none", "no", "none"

Sets the default additional CA certificate file list or strict ``pkcs11:``
URI list for ``omfwd`` actions that use the ``ossl`` stream driver and do not
set ``StreamDriver.CAExtraFiles`` on the action itself. Multiple entries use
the same comma-separated format as ``NetstreamDriverCAExtraFiles``. Module
defaults for this parameter are inherited only by actions whose effective
stream driver is ``ossl``.

StreamDriver.KeyFile
^^^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "string", "none", "no", "none"

Sets the default TLS private-key file for ``omfwd`` actions that do not set
``StreamDriver.KeyFile`` on the action itself. If the value is a strict
``pkcs11:`` URI, that module default is inherited only by actions whose
effective stream driver is ``ossl``.

StreamDriver.CertFile
^^^^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "string", "none", "no", "none"

Sets the default TLS certificate file for ``omfwd`` actions that do not set
``StreamDriver.CertFile`` on the action itself. If the value is a strict
``pkcs11:`` URI, that module default is inherited only by actions whose
effective stream driver is ``ossl``.

Template
^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "RSYSLOG_TraditionalForwardFormat", "no", "``$ActionForwardDefaultTemplate``"

Sets a custom default template for this module.

iobuffer.maxSize
^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "full size", "no", "none"

The iobuffer.maxSize parameter sets the maximum size of the I/O buffer
used by rsyslog when submitting messages to the TCP send API. This
parameter allows limiting the buffer size to a specific value and is
primarily intended for testing purposes, such as within an automated
testbench. By default, the full size of the I/O buffer is used, which
depends on the rsyslog version. If the specified size is too large, an
error is emitted, and rsyslog reverts to using the full size.

.. note::
    The I/O buffer has a fixed upper size limit for performance reasons. This limitation
    allows saving one ``malloc()`` call and indirect addressing. Therefore, the ``iobuffer.maxSize``
    parameter cannot be set to a value higher than this fixed limit.

.. note::
    This parameter should usually not be used in production environments.

Example
.......

.. code-block:: rsyslog

  module(load="builtin:omfwd" iobuffer.maxSize="8")

In this example, a very small buffer size is used. This setting helps
force rsyslog to execute code paths that are rarely used in normal
operations. It allows testing edge cases that typically cannot be
tested automatically.

**Note that contrary to most other modules, omfwd is a built-in module. As such,
you cannot "normally" load it just by name but need to prefix it with
"builtin:" as can be seen above!**
