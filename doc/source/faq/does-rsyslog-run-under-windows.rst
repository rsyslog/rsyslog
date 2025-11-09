Does rsyslog run under Windows?
================================

Short answer: **No, rsyslog does not run natively on Microsoft Windows.**
It is developed and supported as a *POSIX / Linux* (and related UNIX-like) system component.

Alternatives
------------

1. Native Windows syslog solution: **WinSyslog**

   `WinSyslog <https://www.winsyslog.com/>`_ is a commercial product by
   `Adiscon <https://www.adiscon.com/>`_, the primary sponsor of rsyslog. It is maintained by the same
   engineering team that builds rsyslog and provides a rich feature set:

   * Extensive syslog receive / forward / store capabilities
   * Powerful filtering and routing similar to rsyslog's flexibility
   * Support for many of the same networking and protocol features
   * Commercial support options; purchasing licenses directly helps fund
     ongoing rsyslog development

   If you need a production-grade, well-supported solution *on native Windows*,
   WinSyslog is the recommended path.

2. Windows event log integration and relay: **rsyslog Windows Agent**
   The rsyslog Windows Agent (by Adiscon) provides integration with the
   native Windows Event Log and can forward (relay) events to rsyslog or
   other syslog endpoints.

   * Focused on collection and forwarding from Windows
   * Can act as a syslog relay to an rsyslog server
   * Not a full syslog server on Windows: it is not intended to write locally
     to files or databases on Windows

3. Run rsyslog via a Linux environment on Windows (WSL)

   You can install a Linux distribution side-by-side with Windows using the
   **`Windows Subsystem for Linux (WSL) <https://learn.microsoft.com/windows/wsl/>`_**. WSL provides a genuine Linux userland
   with real process semantics and networking. Inside that environment rsyslog
   operates just like on a standard Linux host.

   Typical capabilities of WSL relevant to rsyslog:

   * Access to Linux package managers (e.g. apt, dnf) so you can install rsyslog
   * Standard TCP/UDP networking for forwarding and receiving syslog traffic
   * Ability to run background services (WSL2 supports ``systemd`` in recent
     Windows builds; earlier versions can run rsyslog under a service wrapper)

   Caveats:

   * WSL adds an additional layer -- operational maturity with Linux is advised
   * Integration with native Windows event logs still requires a bridging
     component (e.g. the rsyslog Windows Agent or other exporters)
   * Performance for very high-volume logging may differ from a dedicated
     Linux server

   This approach is only recommended when your organization already has Linux
   expertise and you explicitly need rsyslog's Linux-centric feature set.

Summary
-------
rsyslog itself targets Linux/UNIX platforms. For native Windows deployments use
WinSyslog. If your goal is to collect Windows Event Log and relay to rsyslog,
use the rsyslog Windows Agent. Where Linux know-how exists and hybrid setups are
acceptable, running rsyslog inside WSL is a viable alternative that preserves its
full functionality.
