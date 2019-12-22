# imtuxedoulog : Tuxedo ULOG input module

### Module summary

This module is created to consume Tuxedo ULOG file. 
This file has the following characteristics :
- the file name is compose of a prefix and the date of the file.
- the lines of logs are produced by call to userlog() tuxedo method.
- Tuxedo server based on JAVA writes java's exceptions as log lines in
  ULOG file. This version of the module ignores those lines.
- The ECID (Execution Context ID) is extracted and placed as a structured
  data named ECID.

### Why this module instead of using imfile

As the file name is a rotation base of the current date, imfile must use
a glob to manage this rotation. Imfile must use fs notify implement of the
OS. This interface is implemented in imfile for Linux and Solaris but not 
AIX.

Like imfile does, this module is using the same rsyslog core stream and
then gets advantages its evolves.

Primary we tried to integrating in imfile a "glob like" in polling mode,
it increased the module complexity and make it unmaintainable.

So imfile could not be a valid candidate on none fs notification OS like AIX.

### Ulog log samples

This are different case of logs in the ULOG file according to the usage.

Case 1: second fraction precision = 2
```
105211.70.sic-in2-tmsl1!IMSproxiCSFI4EC.26607818.1.0: TSAM_CAT:305:4563628752 ; I ;TPSUCCESS service
          ^host         ^prog               lprog>    ^text
```

Case 2: second fraction precision = 3
```
011458.705.sic-tst-tmsl1!LMS.5243392.772.3: TSAM_CAT:305: WARN: (23498) times logon TSAM Plus manager
           ^host        ^prog     lprog>    ^text
```

Case 3: with ECID
```
105211.704.sic-in2-tmsl1!IMSproxiCSFI4EC.26607818.1.0: ECID <000003GBORvD4iopwSXBiW01xG2M00001n>: 4563628752
           ^host         ^prog               lprog>          ^ecid                                ^text
```

Case 4: with gtrid & ECID
```
164313.151.sic-tst-tmsm1!ARTIMPP_UDB.42722.1.0: gtrid x0 ... a0f: ECID <000001833^5pVl3iY00f003UF^>: TRACE:at
           ^host         ^prog        lprog>                            ^ecid                        ^text
```
