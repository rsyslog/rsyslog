.. _practice-embedded-ipv4-tail-helper:

.. meta::
   :description: Antipattern for the embedded IPv4-in-IPv6 tail helper that lacked explicit preconditions; includes the corrected pattern with documented contract.
   :keywords: rsyslog, coding practice, antipattern, IPv6, IPv4 tail, mmanon

.. summary-start

Document the IPv4-in-IPv6 tail helper contract so callers cannot treat it as a general search routine and skip the validations its parent function depends on.

.. summary-end

Antipattern: undocumented IPv4-in-IPv6 tail helper
==================================================

**Classification:** Antipattern (with corrected pattern guidance)

**Context:** ``runtime/mmanon.c`` IPv6 anonymization helpers

**Why it matters:** The helper is logically an internal slice of a larger routine. Without explicit preconditions, callers may treat it as a general search helper and skip the validations the parent routine depends on, leading to undefined behavior on malformed literals.

**Steps (improved pattern):**

#. Document that the caller must pass a substring beginning at the first IPv6 hex group and a dot position that is guaranteed to belong to the embedded IPv4 tail.
#. State that the substring must contain a ``:`` before the dot so the helper can return the IPv4 start offset.
#. Assert in unreachable fallbacks instead of returning ``-1`` to catch precondition violations early while keeping a compiler-placating return.

**Before (antipattern):**

.. code-block:: c

   static size_t findV4Start(const uchar *const __restrict__ buf, size_t dotPos) {
       while (dotPos > 0) {
           if (buf[dotPos] == ':') {
               return dotPos + 1;
           }
           dotPos--;
       }
       return -1;  // should not happen
   }

**After (pattern with explicit contract):**

.. code-block:: c

   /**
    * Locate the start offset of the IPv4 tail inside an embedded IPv4-in-IPv6
    * literal. The caller must pass the substring that begins at the first hex
    * group of the IPv6 address and a dot position that is guaranteed to belong
    * to the IPv4 tail; this routine is not a general-purpose search helper.
    * The substring must contain a ':' before the provided dot index.
    */
   static size_t findV4Start(const uchar *const __restrict__ buf, size_t dotPos) {
       while (dotPos > 0) {
           if (buf[dotPos] == ':') {
               return dotPos + 1;
           }
           dotPos--;
       }
       assert(!"embedded IPv4 must have a ':' before its first '.'");
       return 0;  // placate compilers when assertions are disabled
   }

**References:**

- ``runtime/mmanon.c`` embedded IPv4 parsing helper (to be updated with this contract).
- :ref:`practice-externalized-helper-contracts` for the general pattern of documenting narrow helper contracts and defensive fallbacks.
