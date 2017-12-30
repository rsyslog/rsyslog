$DynaFileCacheSize
------------------

**Type:** global configuration parameter

**Default:** 10

**Description:**

This parameter specifies the maximum size of the cache for
dynamically-generated file names. Selector lines with dynamic files
names ('?' indicator) support writing to multiple files with a single
selector line. This setting specifies how many open file handles should
be cached. If, for example, the file name is generated with the hostname
in it and you have 100 different hosts, a cache size of 100 would ensure
that files are opened once and then stay open. This can be a great way
to increase performance. If the cache size is lower than the number of
different files, the least recently used one is discarded (and the file
closed). The hardcoded maximum is 10,000 - a value that we assume should
already be very extreme. Please note that if you expect to run with a
very large number of files, you probably need to reconfigure the kernel
to support such a large number. In practice, we do NOT recommend to use
a cache of more than 1,000 entries. The cache lookup would probably
require more time than the open and close operations. The minimum value
is 1.

Numbers are always in decimal. Leading zeros should be avoided (in some
later version, they may be mis-interpreted as being octal). Multiple
parameters may be given. They are applied to selector lines based on
order of appearance.

**Sample:**

``$DynaFileCacheSize 100    # a cache of 100 files at most``

