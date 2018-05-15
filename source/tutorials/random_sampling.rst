Random sampling
===============

Rsyslog supports various sampling mechanisms. These can be used on client systems to save servers from getting overwhelmed. Here we introduce a new sampling mechanism "Random sampling".

Let's consider a system that is generating logs at rate of 100 logs/sec.
If we want to get 20% of these logs uniformly sampled we use random sampling.

.. code-block:: none

  set $.rand = random(100);
  if ($.rand <= 20) then {
    //send out
  }

Above config will collect 20% of logs generated.
