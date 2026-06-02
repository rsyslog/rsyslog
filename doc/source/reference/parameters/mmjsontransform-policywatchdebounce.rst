.. meta::
   :description: The mmjsontransform policyWatchDebounce parameter sets the quiet period before watched policy reloads.
   :keywords: rsyslog, mmjsontransform, policyWatchDebounce, debounce, watched reload

.. _param-mmjsontransform-policywatchdebounce:
.. _mmjsontransform.parameter.input.policywatchdebounce:

policyWatchDebounce
===================

.. index::
   single: mmjsontransform; policyWatchDebounce
   single: policyWatchDebounce

.. summary-start

Sets the quiet period used before a watched policy change is reloaded.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/mmjsontransform`.

:Name: policyWatchDebounce
:Scope: input
:Type: time interval
:Default: 5s
:Required?: no
:Introduced: 8.2604.0

Description
-----------

Defines the debounce interval used by :ref:`policyWatch
<param-mmjsontransform-policywatch>`. Each new file event resets the timer, so
rapid updates are coalesced into one later reload.

Supported suffixes are ``ms``, ``s``, ``m``, and ``h``. Bare numbers are
interpreted as seconds.

Input usage
-----------
.. _param-mmjsontransform-policywatchdebounce-usage:
.. _mmjsontransform.parameter.input.policywatchdebounce-usage:

.. code-block:: rsyslog

   action(type="mmjsontransform"
          policy="/etc/rsyslog/mmjsontransform-policy.yaml"
          policyWatch="on"
          policyWatchDebounce="500ms"
          input="$!raw" output="$!normalized")

See also
--------
See also the :doc:`main mmjsontransform module documentation
<../../configuration/modules/mmjsontransform>`.
