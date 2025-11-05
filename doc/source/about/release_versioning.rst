.. _about-release-versioning:
.. index:: version numbers; release cadence; numbering scheme

=================================
rsyslog Release Numbering Scheme
=================================

.. meta::
   :description: Explanation of rsyslog's 8.yymm.0 release numbers and what they
                 communicate about cadence, compatibility, and patch levels.
   :keywords: rsyslog, version numbers, release cadence, numbering scheme

.. summary-start

rsyslog release numbers follow the 8.yymm.0 pattern so users can instantly
judge how current a build is while keeping long-standing compatibility promises
and a predictable release day.
.. summary-end

Overview
========

Since January 2019 rsyslog has used a calendar-based identifier that is easy to
read at a glance:

- ``8`` — the long-running major series. Staying on the same major digit signals
  that no breaking architectural shift has taken place. The project plans to
  keep ``8`` as the visible major until the numbering scheme itself changes or a
  non-backwards-compatible rewrite truly requires a higher digit.
- ``yymm`` — the two-digit year and month when the release was published. For
  example ``8.2306.0`` was issued in June 2023.
- ``.0`` — the patch counter. Regular scheduled releases use ``.0``. Urgent
  corrections (rare) become ``.1``, ``.2``, and so on.

The version string reported by :command:`rsyslogd -v` mirrors this layout and
also prints a human-friendly alias such as ``8.1901.0 (aka 2019.01)``.

Cadence and scheduling
======================

Stable builds are issued roughly every two months. The release team aims for an
even-numbered month (February, April, June, …) on a Tuesday around mid-month.
Monday is reserved for final testing and housekeeping, while the remainder of
the week provides distance from the weekend — a period when operations staff is
thin and attackers are more active. Holiday seasons occasionally shift the
target: for example the December release is typically earlier in the month so
critical updates can be applied before Christmas downtime. Exceptional security
needs or project priorities may also pull a build forward or push it back.

Quick reference for newcomers
=============================

If you are evaluating or deploying rsyslog, keep these checkpoints in mind:

- Treat the ``yymm`` portion as the age of your installation. A value older than
  about a year usually means you are missing new modules, fixes, or performance
  improvements.
- When you see the same ``yymm`` across different systems, they are running the
  same feature level regardless of packaging metadata.
- Patch digits greater than ``0`` indicate that the release was respun to ship a
  focused fix without waiting for the next six-week milestone.
- Major digit changes (for example moving from 8.x to 9.x someday) would signal
  a deliberate compatibility break and be announced separately.

Background and rationale
========================

Before 2019 rsyslog labeled releases as ``8.<seq>.0`` where ``<seq>`` simply
counted every six-week drop. The community noticed that ``8.24`` versus ``8.40``
misleadingly looked like minor point upgrades even though they were years apart.
Switching to the calendar-inspired ``8.yymm.0`` notation keeps the established
major series in place for enterprise distributions while making it obvious how
fresh a package is. The decision was refined on the mailing list and summarized
on Rainer Gerhards' project blog when the first ``8.1901.0`` build shipped in
January 2019. The policy has since evolved from the “roughly six-week” cadence
described in that post to today’s bi-monthly Tuesday rhythm for the reasons
outlined above.

Inspired by the wider ecosystem
===============================

rsyslog’s numbering mirrors other projects that encode calendar information in
their version strings, such as Ubuntu (``24.04``) and openEuler (``25.09``). The
similarity makes it easy for operators familiar with those platforms to parse
``8.2404.0`` at a glance and understand how it fits into their maintenance
cycle.

.. seealso::
   Rainer Gerhards, `rsyslog version numbering change`_

.. _rsyslog version numbering change: https://rainer.gerhards.net/2018/12/rsyslog-version-numbering-change.html
