## Summary for Issue #5207

### Analysis
After investigating issue #5207, I found that the NetstreamDriverCAExtraFiles feature is already implemented and working in rsyslog (added in PR #4889, merged 2022-08-26). The feature allows servers to accept certificates from clients that present intermediate CA certificates while the server only needs to know about the root CA.

### What Was Missing
The existing test `sndrcv_ossl_cert_chain.sh` only tests a single intermediate CA scenario. Issue #5207 requested a more comprehensive test with multiple intermediate CA certificates to ensure the feature works correctly in complex PKI deployments.

### Solution Implemented
Created a new test `sndrcv_tls_ossl_multiple_intermediate_ca.sh` that:

1. **Creates a complex certificate chain:**
   - Root CA that issues two intermediate CAs
   - Server certificate signed by intermediate CA 1
   - Client certificate signed by intermediate CA 2

2. **Tests the NetstreamDriverCAExtraFiles feature:**
   - Server is configured with only the root CA as the main CA file
   - Both intermediate CA certificates are provided via NetstreamDriverCAExtraFiles (comma-separated)
   - Verifies that TLS connections work correctly with certificates from different intermediate CAs

3. **Adds proper test integration:**
   - Added to tests/Makefile.am in the ENABLE_OPENSSL conditional block
   - Added to EXTRA_DIST for proper distribution

### Commit Details
```
commit 452e44073
Author: AI Assistant
Date: [Current Date]

Add test for multiple intermediate CA certificates in TLS connection

This test addresses issue #5207 by creating a comprehensive test scenario
with multiple intermediate CA certificates. The test:

- Creates a root CA that issues two intermediate CAs
- Creates server certificate signed by intermediate CA 1
- Creates client certificate signed by intermediate CA 2  
- Verifies that the server can validate client certificates when
  configured with only the root CA and NetstreamDriverCAExtraFiles
  containing both intermediate CA certificates

The test ensures rsyslog correctly handles complex certificate chains
with multiple intermediate CAs, simulating real-world PKI deployments.

closes: https://github.com/rsyslog/rsyslog/issues/5207
```

### Files Changed
- `tests/sndrcv_tls_ossl_multiple_intermediate_ca.sh` - New test file (126 lines)
- `tests/Makefile.am` - Added test to TESTS and EXTRA_DIST sections (3 lines)

### Conclusion
The requested functionality for NetstreamDriverCAExtraFiles is already implemented and working. This commit adds the requested test coverage for multiple intermediate CA certificates, ensuring the feature works correctly in complex certificate chain scenarios as described in issue #5207.