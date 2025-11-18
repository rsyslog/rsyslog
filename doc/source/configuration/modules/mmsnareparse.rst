mmsnareparse - NXLog Snare Windows Security parser
===================================================

The ``mmsnareparse`` parser module extracts structured metadata from NXLog
Snare-formatted Windows Security events and Microsoft Sysinternals Sysmon events
that are transported inside RFC3164 or RFC5424 envelopes. It was designed using
Windows Server 2016 through Windows Server 2025 samples and preserves the
original payload while exposing a normalized JSON view under a configurable
container (``!win`` by default).

Highlights
----------

* Supports classic tab-delimited ``MSWinEventLog`` payloads as well as the
  Snare JSON variant (``MSWinEventLog\t0\t{...}``).
* Parses Microsoft Sysinternals Sysmon events from the
  ``Microsoft-Windows-Sysmon/Operational`` channel, with support for Event IDs
  1-29 and common Sysmon fields (process creation, network connections, file
  operations, registry changes, and more).
* Derives event-level metadata such as event IDs (with integer promotion when
  possible), provider, NXLog event type, channel, computer, and RFC 3339
  timestamps, and maps high-value event IDs to semantic categories
  (``4624``, ``4625``, ``4672``, ``4688``, ``4768``, ``4769``, ``4771``,
  ``5140``, ``5157``, ``6281``, ``1102``, ``1243``) while populating derived
  ``Category``, ``Subtype``, and ``Outcome`` fields.
* Normalizes free-form keys through priority-ordered pattern tables that apply
  type-aware writers (integer, boolean, JSON block) and event-specific
  overrides so fields such as WDAC PID values or WUFB policy identifiers land
  in the correct JSON section without bespoke conditionals.
* Interprets well-known sections (Subject, Logon Information, New Logon,
  Network Information, Process Information, Detailed Authentication
  Information, Account For Which Logon Failed, Failure Information, Application
  Information, Filter Information, Share Information, Additional Information,
  Certificate Information) and exposes their fields as nested objects such as
  ``!win!Subject!AccountName`` or ``!win!Network!SourceNetworkAddress``.
* Recognizes modern Windows telemetry blocks:

  * **LAPS Context** -> ``!win!LAPS!PolicyVersion``,
    ``!win!LAPS!CredentialRotation``
  * **TLS Inspection** -> ``!win!TLSInspection!Reason``,
    ``!win!TLSInspection!Policy``
  * **WDAC Enforcement** -> ``!win!WDAC!PolicyName``,
    ``!win!WDAC!PolicyVersion``, ``!win!WDAC!EnforcementMode``,
    ``!win!WDAC!User``, ``!win!WDAC!PID`` (with numeric promotion)
  * **WUFB Deployment** -> ``!win!WUFB!PolicyID``, ``!win!WUFB!Ring``,
    ``!win!WUFB!FromService``, ``!win!WUFB!EnforcementResult``
  * Remote Credential Guard surface (``!win!Logon!RemoteCredentialGuard``)

* Performs lookup translation for logon type codes, deriving
  ``LogonTypeName`` strings, and emits boolean interpretations for Remote
  Credential Guard and LAPS credential rotation indicators.
* Stores any unmapped segments in ``!win!Unparsed`` to ensure the payload is
  preserved for later review.
* Emits per-message validation diagnostics in ``!win!Validation!Errors`` and
  parsing counters in ``!win!Stats!ParsingStats`` for easier troubleshooting.
* Supports external JSON overrides via ``runtime.config`` or inline
  ``definition.json`` so new fields can be mapped without recompilation.

Build & Runtime Requirements
-----------------------------

* No external libraries beyond the rsyslog core dependencies (libfastjson is
  already required by rsyslog).
* Tested against NXLog Snare output for Windows Server 2016, 2019, 2022, and the
  2025.
* Runs safely with multiple worker threads; configuration is immutable and each
  worker keeps its own scratch buffers.

JSON Field Overview
-------------------

The module emits a hierarchical JSON document under the configured root path. Key
nodes include:

* ``Event`` — Record metadata (log, record number, event ID, computer, timestamps,
  audit outcome, mapping category/subtype).
* ``EventData`` — General key/value pairs emitted outside named sections,
  including logon/process/network helpers that benefit from the typed matcher.
* ``Subject``, ``Logon``, ``NewLogon`` — Authentication identity details.
* ``Network`` — Source/destination addresses and ports (supports client address
  fields in Kerberos events).
* ``Process`` and ``Application`` — Process identifiers, names, command line data.
* ``Authentication`` — Logon process, packages, Remote Credential Guard status.
* ``Failure`` — Failure reason, status, sub-status for rejected logons.
* ``Privileges`` — Array of privileges assigned to special logon events.
* ``Kerberos`` — Account, service, additional, and certificate information for
  Kerberos ticket events.
* ``LAPS``, ``TLS``, ``WDAC``, ``WUFB`` — Dedicated blocks for modern telemetry
  (LAPS context, TLS inspection, Windows Defender Application Control, Windows
  Update for Business deployment events).
* ``Validation`` — Per-message warnings raised by strict or moderate validation
  policies.
* ``Stats`` — Parsing counters (``ParsingStats``) summarising how many fields
  were extracted successfully.
* ``Raw`` / ``RawJSON`` — Optional copies of the original Snare payload when
  ``emit.rawpayload="on"``.
* ``Unparsed`` — Catch-all array for any sections the module could not map with
  the current release (or an empty array when ``emit.debugjson="on"``).

Event ID Mapping
----------------

.. csv-table::
   :header: "Event ID", "Category", "Subtype", "Outcome"
   :widths: auto

   "4624", "Logon", "Success", "success"
   "4625", "Logon", "Failure", "failure"
   "4672", "Privilege", "Assignment", "success"
   "4688", "Process", "Creation", "success"
   "4768", "Kerberos", "TGTRequest", ""
   "4769", "Kerberos", "ServiceTicket", ""
   "4771", "Kerberos", "PreAuthFailure", ""
   "5140", "FileShare", "Access", ""
   "5157", "FilteringPlatform", "PacketDrop", "failure"
   "6281", "WDAC", "Enforcement", ""
   "1102", "Audit", "LogCleared", ""
   "1243", "WindowsUpdate", "Deployment", ""

Event IDs 4624, 4625, 4672, 4688, 4768–4771, 5140, 5157, 6281, 1102, 1243
and others are mapped to ``Event.Category``, ``Event.Subtype``, and
``Event.Outcome`` for quick filtering.

* Logon type codes are translated to ``Logon.TypeName``.
* NTSTATUS and Kerberos result codes are preserved in hex form; additional maps
  can be added easily in ``mmsnareparse.c``.
* Timestamps are normalised to ISO 8601 UTC using the rsyslog message timestamp
  if the payload does not contain a parseable value.

Error Handling & Observability
------------------------------

* Invalid or partial payloads are routed to ``Unparsed`` and flagged via
  the ``partial`` counter in the instance's impstats object.
* Parse failures increment the ``failed`` counter and can be redirected by a
  secondary action when ``$parsesuccess`` evaluates to anything other than ``OK``.
* Enable ``emit.debugjson="on"`` to force-create ``!win!Unparsed`` (even when
  empty) so assertions and log collection pipelines can detect previously
  unseen sections.
* ``!win!Validation!Errors`` captures parse-time warnings when ``validation.mode``
  is ``moderate`` or ``strict`` and ``!win!Stats!ParsingStats`` exposes
  ``total_fields``, ``successful_parses`` and ``failed_parses`` for telemetry.
* Placeholder values such as ``-`` or ``N/A`` are ignored and therefore neither
  counted as stored fields nor as parse failures in the telemetry counters.

Configuration
-------------

Basic Configuration with Error Handling
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: none

   module(load="imtcp")
   module(load="omfile")
   module(load="mmsnareparse")

   template(name="snareWin" type="string" string="%!win%\n")
   input(type="imtcp" port="5514")

   action(type="mmsnareparse"
          container="!win"
          enable.network="on"
          enable.laps="on"
          enable.tls="on"
          enable.wdac="on")
   if $parsesuccess == "OK" then {
       action(type="omfile" file="/var/log/winsec.json" template="snareWin")
   } else {
       action(type="omfile" file="/var/log/winsec.parsefail" template="RSYSLOG_DebugFormat")
   }

JSON Template Output for SIEM Integration
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

This configuration extracts specific fields into a structured JSON format suitable for SIEM platforms:

.. code-block:: none

   module(load="mmsnareparse")

   template(name="jsonfmt" type="list" option.jsonf="on") {
     property(outname="EventID" name="$!win!Event!EventID" format="jsonf")
     property(outname="LogonType" name="$!win!LogonInformation!LogonType" format="jsonf")
     property(outname="LogonTypeName" name="$!win!LogonInformation!LogonTypeName" format="jsonf")
     property(outname="LAPSPolicyVersion" name="$!win!LAPS!PolicyVersion" format="jsonf")
     property(outname="LAPSCredentialRotation" name="$!win!LAPS!CredentialRotation" format="jsonf")
     property(outname="TLSReason" name="$!win!TLSInspection!Reason" format="jsonf")
     property(outname="WDACPolicyVersion" name="$!win!WDAC!PolicyVersion" format="jsonf")
     property(outname="WUFBPolicyID" name="$!win!WUFB!PolicyID" format="jsonf")
   }

   action(type="mmsnareparse")
   action(type="omfile" file="/var/log/winsec.json" template="jsonfmt")

Comprehensive Field Extraction with Ruleset
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

This configuration demonstrates comprehensive field extraction using a ruleset approach, suitable for detailed analysis and compliance reporting:

.. code-block:: none

   module(load="imtcp")
   module(load="mmsnareparse")

   # Template to extract comprehensive structured JSON output
   template(name="jsonfmt" type="list" option.jsonf="on") {
       # Event fields
       property(outname="eventid" name="$!win!Event!EventID" format="jsonf")
       property(outname="channel" name="$!win!Event!Channel" format="jsonf")
       property(outname="eventtype" name="$!win!Event!EventType" format="jsonf")
       property(outname="categorytext" name="$!win!Event!CategoryText" format="jsonf")
       property(outname="computer" name="$!win!Event!Computer" format="jsonf")
       property(outname="provider" name="$!win!Event!Provider" format="jsonf")
       
       # Subject fields
       property(outname="subjectsecurityid" name="$!win!Subject!SecurityID" format="jsonf")
       property(outname="subjectaccountname" name="$!win!Subject!AccountName" format="jsonf")
       property(outname="subjectaccountdomain" name="$!win!Subject!AccountDomain" format="jsonf")
       property(outname="subjectlogonid" name="$!win!Subject!LogonID" format="jsonf")
       
       # LogonInformation fields
       property(outname="logontype" name="$!win!LogonInformation!LogonType" format="jsonf")
       property(outname="logontypename" name="$!win!LogonInformation!LogonTypeName" format="jsonf")
       property(outname="restrictedadminmode" name="$!win!LogonInformation!RestrictedAdminMode" format="jsonf")
       property(outname="virtualaccount" name="$!win!LogonInformation!VirtualAccount" format="jsonf")
       property(outname="elevatedtoken" name="$!win!LogonInformation!ElevatedToken" format="jsonf")
       property(outname="impersonationlevel" name="$!win!LogonInformation!ImpersonationLevel" format="jsonf")
       
       # NewLogon fields
       property(outname="newlogonsecurityid" name="$!win!NewLogon!SecurityID" format="jsonf")
       property(outname="newlogonaccountname" name="$!win!NewLogon!AccountName" format="jsonf")
       property(outname="newlogonaccountdomain" name="$!win!NewLogon!AccountDomain" format="jsonf")
       property(outname="newlogonlogonid" name="$!win!NewLogon!LogonID" format="jsonf")
       property(outname="linkedlogonid" name="$!win!NewLogon!LinkedLogonID" format="jsonf")
       property(outname="networkaccountname" name="$!win!NewLogon!NetworkAccountName" format="jsonf")
       property(outname="logonguid" name="$!win!NewLogon!LogonGUID" format="jsonf")
       
       # Process fields
       property(outname="processid" name="$!win!Process!ProcessID" format="jsonf")
       property(outname="processname" name="$!win!Process!ProcessName" format="jsonf")
       property(outname="processcommandline" name="$!win!Process!ProcessCommandLine" format="jsonf")
       property(outname="tokenelevationtype" name="$!win!Process!TokenElevationType" format="jsonf")
       property(outname="mandatorylabel" name="$!win!Process!MandatoryLabel" format="jsonf")
       
       # Network fields
       property(outname="workstationname" name="$!win!Network!WorkstationName" format="jsonf")
       property(outname="sourcenetworkaddress" name="$!win!Network!SourceNetworkAddress" format="jsonf")
       property(outname="sourceport" name="$!win!Network!SourcePort" format="jsonf")
       
       # DetailedAuthentication fields
       property(outname="logonprocess" name="$!win!DetailedAuthentication!LogonProcess" format="jsonf")
       property(outname="authenticationpackage" name="$!win!DetailedAuthentication!AuthenticationPackage" format="jsonf")
       property(outname="transitedservices" name="$!win!DetailedAuthentication!TransitedServices" format="jsonf")
       property(outname="packagename" name="$!win!DetailedAuthentication!PackageName" format="jsonf")
       property(outname="keylength" name="$!win!DetailedAuthentication!KeyLength" format="jsonf")
       
       # Privileges fields
       property(outname="privilegelist" name="$!win!Privileges!PrivilegeList" format="jsonf")
   }

   ruleset(name="winsec") {
       action(type="mmsnareparse")
       action(type="omfile" file="/var/log/winsec.json" template="jsonfmt")
   }

   input(type="imtcp" port="5514" ruleset="winsec")

Parameters
----------

.. csv-table::
   :header: "Parameter", "Type", "Default", "Description"
   :widths: auto
   :class: parameter-table

   "``rootpath`` / ``container``", "string", "``!win``", "JSON container path that receives the parsed structure. ``rootpath`` remains a backwards-compatible alias."
   "``enable.network``", "binary", "``on``", "Toggle extraction for ``Network Information`` blocks."
   "``enable.laps``", "binary", "``on``", "Toggle parsing of ``LAPS Context`` sections."
   "``enable.tls``", "binary", "``on``", "Toggle parsing of ``TLS Inspection`` sections."
   "``enable.wdac``", "binary", "``on``", "Toggle WDAC enrichment (``Policy Name``, ``Policy Version``, etc.)."
   "``emit.rawpayload``", "binary", "``on``", "When enabled, stores the original payload in ``!win!Raw`` (or ``!win!RawJSON`` for Snare JSON records)."
   "``emit.debugjson`` / ``debugjson``", "binary", "``off``", "Adds an empty ``Unparsed`` array even when all sections are recognized, simplifying downstream assertions."
   "``definition.file``", "string", "``unset``", "Path to a JSON descriptor that augments or overrides built-in section, field, and event mappings."
   "``definition.json``", "string", "``unset``", "Inline JSON descriptor following the same schema as ``definition.file``. Processed after the file-based overrides."
   "``runtime.config``", "string", "``unset``", "Persistent runtime configuration file. Supports the definition schema plus ``options`` such as ``enable_debug`` and ``enable_fallback``."
   "``validation.mode`` / ``validation_mode``", "string", "``permissive``", "Selects parser strictness: ``permissive`` ignores issues, ``moderate`` records warnings, ``strict`` aborts when thresholds are exceeded."

Extracted fields
----------------

A non-exhaustive list of notable properties exposed by the module:

* ``!win!Event!EventID`` (or ``EventIDRaw`` for non-numeric identifiers),
  ``!win!Event!Provider``, ``!win!Event!EventType``, ``!win!Event!Channel``,
  ``!win!Event!Computer``, ``!win!Event!CategoryText``, ``!win!Event!Category``,
  ``!win!Event!Subtype``, ``!win!Event!Outcome``, ``!win!Event!Level`` (for
  Snare JSON payloads), and ``!win!Event!RecordNumberRaw`` when a
  ``System.EventRecordID`` value is present.
* ``!win!Event!TimeCreated!Normalized`` (derived from the syslog envelope) and
  ``!win!Event!TimeCreated!Raw`` when Snare JSON payloads include an
  ``EventTime``.
* ``!win!Subject!SecurityID``, ``!win!Subject!AccountName``,
  ``!win!Subject!AccountDomain``, ``!win!Subject!LogonID``
* ``!win!LogonInformation!LogonType``, ``!win!LogonInformation!LogonTypeName``,
  ``!win!LogonInformation!VirtualAccount``,
  ``!win!LogonInformation!ElevatedToken``,
  ``!win!LogonInformation!RemoteCredentialGuard`` (with an aggregated
  ``!win!Logon!RemoteCredentialGuard`` boolean)
* ``!win!NewLogon!SecurityID``, ``!win!NewLogon!AccountName``,
  ``!win!NewLogon!LogonGUID``
* ``!win!Network!SourceNetworkAddress``, ``!win!Network!SourcePort``,
  ``!win!Network!DestinationAddress``, ``!win!Network!DestinationPort``
* ``!win!Process!ProcessID``, ``!win!Process!ProcessName``
* ``!win!Failure!FailureReason``, ``!win!Failure!Status``,
  ``!win!Failure!SubStatus``
* ``!win!DetailedAuthentication!LogonProcess``,
  ``!win!DetailedAuthentication!AuthenticationPackage``,
  ``!win!DetailedAuthentication!TransitedServices``,
  ``!win!DetailedAuthentication!PackageName``,
  ``!win!DetailedAuthentication!KeyLength``
* ``!win!Privileges`` (retains privilege enumerations for downstream review)
* ``!win!LAPS!PolicyVersion``, ``!win!LAPS!CredentialRotation``
* ``!win!TLSInspection!Reason``, ``!win!TLSInspection!Policy``
* ``!win!WDAC!PolicyName``, ``!win!WDAC!PolicyVersion``,
  ``!win!WDAC!EnforcementMode``, ``!win!WDAC!User``, ``!win!WDAC!PID``
  (and ``PIDRaw`` when Snare reports non-numeric values)
* ``!win!WUFB!PolicyID``, ``!win!WUFB!Ring``, ``!win!WUFB!FromService``,
  ``!win!WUFB!EnforcementResult``

Unknown fragments are preserved under ``!win!Unparsed`` to aid future
normalization efforts.

Error handling and observability
--------------------------------

* Residual tokens and unexpected sections are collected in ``!win!Unparsed``
  for follow-up analysis.
* Messages that do not contain an ``MSWinEventLog`` payload are ignored and
  ``$parsesuccess`` remains ``off``.
* When Snare JSON payloads cannot be parsed, the raw text is stored under
  ``!win!RawJSON`` so downstream tooling can inspect the failure.
* Optional raw payload storage (``emit.rawpayload``) simplifies error triage and
  regression analysis.

Testing
-------

The regression suite (``tests/mmsnareparse-basic.sh``,
``tests/mmsnareparse-json.sh``, ``tests/mmsnareparse-syslog.sh``,
``tests/mmsnareparse-comprehensive.sh``, ``tests/mmsnareparse-custom.sh``,
``tests/mmsnareparse-sysmon.sh``) replays canonical Windows Security samples
and Sysmon events, and injects custom JSON overrides to verify extracted fields
remain stable (for example, 4624 with LAPS, 5157 TLS inspection, 6281 WDAC
enforcement, 1243 WUFB deployment, Sysmon process and network events, and
bespoke definitions supplied at runtime).

Extending Pattern Tables at Runtime
-----------------------------------

``mmsnareparse`` ships with curated defaults for section detection, field
normalisation and event metadata, but environments frequently contain
organisation-specific extensions. The module can import supplemental
definitions at startup using declarative JSON descriptors.

Sysmon Event Support
~~~~~~~~~~~~~~~~~~~~

The module includes built-in support for Microsoft Sysinternals Sysmon events
via a definition file located at ``plugins/mmsnareparse/sysmon_definitions.json``.
This file provides event mappings and field patterns for common Sysmon Event IDs
(1-29), including:

* Process events (creation, termination, access, tampering)
* Network events (connections, DNS queries)
* File events (creation, deletion, time changes, stream hashes)
* Registry events (value sets, key renames, object create/delete)
* System events (service state changes, config changes)
* Driver and image load events
* Pipe events (creation, connections)
* WMI events (filters, consumers, bindings)
* Clipboard changes
* File blocking events

To enable Sysmon parsing, load the definition file:

.. code-block:: none

   module(load="mmsnareparse"
          definition.file="../plugins/mmsnareparse/sysmon_definitions.json")

   action(type="mmsnareparse" container="!win")

The definition file maps Sysmon Event IDs to semantic categories and subtypes,
and provides field patterns for common Sysmon fields such as ``ProcessGuid``,
``ProcessId``, ``Image``, ``CommandLine``, ``SourceIp``, ``DestinationIp``,
``Protocol``, and many others. Fields are automatically routed to appropriate
JSON sections (``EventData``, ``Network``, etc.) based on their type and
context.

You can extend or customize the Sysmon definitions by creating your own JSON
file that merges with or overrides the built-in patterns. See the
``sysmon_definitions.json`` file for the complete schema and examples.

New module parameters
~~~~~~~~~~~~~~~~~~~~~

``definition.file``
    Absolute or relative path to a JSON file that contains custom definitions.
    The file is loaded during activation and merged with the built-in tables.

``definition.json``
    Inline JSON string with the same schema as ``definition.file``. This is
    convenient for smaller overrides delivered directly in the rsyslog config.
    The value is parsed after ``definition.file`` so inline snippets can adjust
    or replace objects loaded from disk.

``validation.mode``
    Controls how both configuration and runtime parsing react to malformed
    data. ``permissive`` (the default; aliases: ``lenient``, ``default``)
    accepts issues silently, ``moderate`` records warnings under
    ``!win!Validation!Errors`` while continuing, and ``strict`` aborts the
    configuration or message when thresholds are exceeded.

``runtime.config``
    Path to a JSON file containing persistent overrides. The file shares the
    same schema as ``definition.file`` and additionally supports an ``options``
    object (``enable_debug``, ``enable_fallback``) to influence parse-time
    behaviour.

Definition schema
~~~~~~~~~~~~~~~~~

The JSON document accepts the following top-level arrays:

``sections``
    Adds or overrides description section matchers. Each entry supports the
    keys ``pattern`` (required, literal with optional ``*`` wildcard),
    ``canonical`` (default: auto-generated CamelCase), ``behavior`` (``standard``,
    ``inline``, ``semicolon`` or ``list``), ``priority`` (integer, higher wins),
    ``sensitivity`` (``case_sensitive``, ``case_insensitive``, ``canonical``) and
    ``flags`` (array of ``network``, ``laps``, ``tls``, ``wdac``).

``fields``
    Declares global field patterns. Fields map a ``pattern`` to a ``canonical``
    name, optionally assign a ``section`` (``EventData``, ``Logon``, custom),
    override ``priority``, and set ``value_type`` (``string``, ``int64``,
    ``int64_with_raw``, ``bool``, ``json``, ``logon_type``,
    ``remote_credential_guard``, ``privilege_list``) and ``sensitivity``.

``eventFields``
    Supplies event-specific field matchers. Each object requires an
    ``event_id`` and a ``patterns`` array containing the same keys as ``fields``.
    Optional ``required_flags`` gate the override on module toggles (for example
    only when TLS inspection is enabled).

``events``
    Defines or updates the derived ``Event.Category``, ``Event.Subtype`` and
    ``Event.Outcome`` for specific Windows event IDs.

Example: merging custom sections and fields
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: json

   {
     "sections": [
       {
         "pattern": "Custom Block*",
         "canonical": "CustomBlock",
         "behavior": "standard",
         "priority": 250
       }
     ],
     "fields": [
       {
         "pattern": "CustomEventTag",
         "section": "EventData",
         "value_type": "string"
       }
     ],
     "eventFields": [
       {
         "event_id": 9999,
         "patterns": [
           {
             "pattern": "WidgetID",
             "section": "CustomBlock",
             "value_type": "string"
           }
         ]
       }
     ],
     "events": [
       {
         "event_id": 9999,
         "category": "Custom",
         "subtype": "Injected",
         "outcome": "success"
       }
     ]
   }

To activate the overrides:

.. code-block:: none

   module(load="mmsnareparse"
          definition.file="/etc/rsyslog.d/custom-winsec.json"
          validation.mode="strict")

At runtime the module evaluates built-in and custom matchers in priority order
and picks the best fit. The definitions become immutable once the action is
activated, ensuring worker threads share a consistent view.


Troubleshooting
---------------

* Inspect ``$parsesuccess`` and the instance's impstats counters (``recordseen``,
  ``parsed``, ``partial``, ``failed``) to verify parsing behaviour.
* Use ``emit.debugjson="on"`` to guarantee an ``!win!Unparsed`` array is present
  for assertions when new Windows releases add previously unknown sections.
* Extend section handlers or lookup tables in ``plugins/mmsnareparse/mmsnareparse.c`` when Microsoft introduces additional telemetry fields.
