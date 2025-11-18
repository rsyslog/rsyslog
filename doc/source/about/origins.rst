.. _about-origins:

.. meta::
   :description: Historical background of the syslog ecosystem from 1983 to 2003, including BSD
                 syslogd and sysklogd, which formed the foundation for rsyslog.
   :keywords: syslog, history, BSD syslogd, sysklogd, rsyslog origins, logging

.. summary-start

Origins and background of rsyslog, covering the syslog ecosystem from BSD syslogd (1983) through
sysklogd and the developments leading to the creation of rsyslog.

.. summary-end


Origins of rsyslog (1983–2003)
==============================

This page describes the historical background of the syslog ecosystem before rsyslog was created.
For the project’s evolution from 2004 onward, see :doc:`history`.


BSD syslogd (1983)
------------------

rsyslog’s roots begin with **BSD syslogd (1983)**, one of the most enduring components of Unix
systems. BSD syslogd introduced:

- facility.priority routing,
- a unified system logging service,
- a simple and recognizable configuration language,
- a stable message format.

Its design has persisted for more than **40 years**, with later RFCs refining message formats but
preserving the original semantics. rsyslog explicitly maintains compatibility with these behaviors
while extending them for modern workloads.

sysklogd (1990s)
----------------

Linux adopted `sysklogd` to provide BSD-compatible logging:

- minimal extensions over BSD syslogd,
- limited filtering and transformation,
- no reliable transport (no TCP),
- monolithic, non-modular architecture.

Its stability and compatibility made it the standard syslog daemon on Linux for many years, but
its limitations became increasingly apparent as system and network requirements grew.


Transition toward rsyslog (early 2000s)
---------------------------------------

By the early 2000s, production environments required capabilities that sysklogd could not
deliver:

- reliable message transport (TCP, later RFC 3195),
- structured and flexible output formats,
- database integration for log analytics,
- better performance and extensibility.

These operational needs led to the creation of rsyslog in 2004, extending the traditional syslog
model while preserving its proven foundations.

