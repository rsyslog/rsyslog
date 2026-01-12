**************
is_in_subnet()
**************

Purpose
=======

is_in_subnet(ip, cidr)

Checks if the provided IP address matches the specified CIDR subnet.
Returns 1 if the IP is in the subnet, 0 otherwise.

Parameters
==========

* **ip** - The IP address to check (IPv4 or IPv6 string).
* **cidr** - The subnet in CIDR notation (e.g., "192.168.1.0/24" or "2001:db8::/32").

Return Value
============

* **1** - If the IP address is valid and belongs to the subnet.
* **0** - If the IP address is valid but does not belong to the subnet, or if inputs are invalid.

Example
=======

.. code-block:: none

   if is_in_subnet($fromhost-ip, "192.168.0.0/16") then {
      action(type="omfile" file="/var/log/local_net.log")
   }
