
fmpansanitizer
**********************************************************************
:date: 2025-06-30
:version: 1.0


The `fmpansanitizer` module provides a high-performance function `maskpan` for **detecting and masking Primary Account Numbers (PANs)** in syslog messages. 
This function is designed to support **PCI-DSS Requirement 3.4.1**, ensuring that PANs are not stored or displayed in cleartext in logs.


.. function:: maskpan(variable, pattern, replacement)

   To reduce masking false positives (non-PAN 16-digit numbers), the function includes Luhn algorithm validation:


   :param variable: The message string to process (e.g., `$msg` in rsyslog).
   :type variable: str
   :param pattern: Regular expression used to detect PAN candidates.
   :type pattern: str
   :param replacement: Replacement pattern (e.g., `\$1******\$2`) used to mask the PAN.
   :type replacement: str
   :returns: A new string with PANs masked if valid.
   :rtype: str


Usage
=====




.. code-block:: none

    module(load="fmpansanitizer")
    
    # Example log message containing a PAN
    set $msg = "Transaction completed with card 4000000000000002 at 2023-10-01."
    
    # Mask the PAN in the log message
    # The regex pattern matches 16-digit PANs that begin with a 4 (Visa) or with 51 to 55 (MasterCard), 
    # and are validated using the Luhn checksum.
    # The regular expression pattern must be customized to match the specific 
    # PAN formats relevant to your environment or data sources.
    set $.masked = maskpan($msg, "(4\\d{5}|5[12345]\\d{4})\\d{6}(\\d{4})", "\$1******\$2")
    
    # Update the original message with the masked version
    set $msg = $.masked
    
    # Output the sanitized message
    # "Transaction completed with card 400000******0002 at 2023-10-01."
    action(type="omfile" file="/var/log/sanitized.log")



Requirements
============
This module relies on `PCRE2 library <https://www.pcre.org/>`.


PCI-DSS v4.0 Compliance
=======================
- Requirement 3.4.1: This module helps enforce the masking of PANs in log data, reducing the exposure of sensitive cardholder information.
- Only displays the first 6 and last 4 digits, as explicitly allowed by the standard.
- Designed for use in environments where logging of transactional or payment-related data may inadvertently expose PANs.


Use case
========
- SANITIZE logs before storage or forwarding.
- Integrate into payment gateways, POS systems, or any infrastructure logging PANs.
- Reduce scope for PCI-DSS audits by preventing log-based data leaks.