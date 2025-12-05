.. _error-logging-with-errno:

Error Logging with errno
========================

**Classification**: Pattern

**Context**: Error handling and logging throughout the codebase, specifically when reporting system errors (where ``errno`` is set).

**Why it matters**:
Using ``LogError(errno, ...)`` ensures that system error messages are formatted correctly and consistently. It avoids the pitfalls of manual string formatting with ``strerror`` or ``rs_strerror_r``, such as buffer management issues or thread-safety concerns (though ``rs_strerror_r`` handles the latter). It also simplifies the code by removing the need for temporary error string buffers.

**Steps**:

1.  **Capture errno immediately**: Ensure ``errno`` is captured or used immediately after the failing system call, before any other function call that might reset it.
2.  **Use LogError with errno**: Pass the captured ``errno`` (or ``errno`` directly if safe) as the first argument to ``LogError``.
3.  **Do not format manually**: Do not use ``strerror``, ``gai_strerror`` (unless specifically for getaddrinfo return codes), or ``rs_strerror_r`` to manually format the error string into the message. Let ``LogError`` handle it.

**Example**:

.. code-block:: c

   /* Bad: Manual formatting */
   if (connect(...) < 0) {
       char errStr[1024];
       rs_strerror_r(errno, errStr, sizeof(errStr));
       LogError(0, RS_RET_SUSPENDED, "Connect failed: %s", errStr);
   }

   /* Good: Using LogError with errno */
   if (connect(...) < 0) {
       LogError(errno, RS_RET_SUSPENDED, "Connect failed");
   }

**Antipattern**:
Passing ``0`` as the first argument to ``LogError`` when a system error has occurred is a common antipattern. This forces the developer to manually handle the error string, leading to more verbose and potentially error-prone code.
