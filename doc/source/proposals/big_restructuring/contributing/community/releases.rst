The Release Process
===================

This document explains the Rsyslog release process (Rsyslog being the code
hosted on the main ``rsyslog/rsyslog`` `Git repository`_).

Rsyslog manages its releases through a *time-based model*; a new Rsyslog minor
version comes out every *six weeks*.

.. tip::

    The meaning of "minor" comes from the `Semantic Versioning`_ strategy.

Each minor version sticks to the same very well-defined process where we start
with a development period, followed by a maintenance period.

.. note::

    This release process has been adopted as of Rsyslog 8.2, and all the
    "rules" explained in this document must be strictly followed as of Rsyslog
    8.3.

.. _contributing-release-development:

Development
-----------

The full development period lasts six weeks and is divided into two phases:

* *Development*: *Four weeks* to add new features and to enhance existing
  ones;

* *Stabilisation*: *Two weeks* to fix bugs, prepare the release, and wait
  for the whole Rsyslog ecosystem (third-party libraries, bundles, and
  projects using Rsyslog) to catch up.

During the development phase, any new feature can be reverted if it won't be
finished in time or if it won't be stable enough to be included in the current
final release.


.. _Semantic Versioning: https://semver.org/
.. _Git repository: https://github.com/rsyslog/rsyslog
