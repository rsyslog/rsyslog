***************
exec_template()
***************

Purpose
=======

exec_template(str)

Sets a variable through the execution of a template. Basically this permits to easily
extract some part of a property and use it later as any other variable.

**Read more about it here :** `<http://www.rsyslog.com/how-to-use-set-variable-and-exec_template>`_

Example
=======

The following example shows the template being used to extract a part of the message.

.. code-block:: none

   template(name="extract" type="string" string="%msg:F:5%")
   set $!xyz = exec_template("extract");


