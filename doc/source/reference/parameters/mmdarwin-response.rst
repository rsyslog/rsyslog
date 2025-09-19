.. _param-mmdarwin-response:
.. _mmdarwin.parameter.input.response:

response
========

.. index::
   single: mmdarwin; response
   single: response

.. summary-start

Controls whether Darwin returns a score, forwards data, or does both.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/mmdarwin`.

:Name: response
:Scope: input
:Type: word
:Default: input="no"
:Required?: no
:Introduced: at least 8.x, possibly earlier

Description
-----------
Tells the Darwin filter what to do next:

* :json:`"no"`: no response will be sent, nothing will be sent to next
  filter.
* :json:`"back"`: a score for the input will be returned by the filter,
  nothing will be forwarded to the next filter.
* :json:`"darwin"`: the data provided will be forwarded to the next filter
  (in the format specified in the filter's configuration), no response will
  be given to mmdarwin.
* :json:`"both"`: the filter will respond to mmdarwin with the input's score
  AND forward the data (in the format specified in the filter's
  configuration) to the next filter.

.. note::

   Please be mindful when setting this parameter, as the called filter will
   only forward data to the next configured filter if you ask the filter to do
   so with :json:`"darwin"` or :json:`"both"`. If a next filter is configured
   but you ask for a :json:`"back"` response, the next filter **WILL NOT**
   receive anything!

Input usage
-----------
.. _param-mmdarwin-input-response-usage:
.. _mmdarwin.parameter.input.response-usage:

.. code-block:: rsyslog

   action(type="mmdarwin" response="both")

See also
--------
See also :doc:`../../configuration/modules/mmdarwin`.
