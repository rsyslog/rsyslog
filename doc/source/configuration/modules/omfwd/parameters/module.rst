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
