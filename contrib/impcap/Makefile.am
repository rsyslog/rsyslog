pkglib_LTLIBRARIES = impcap.la

impcap_la_SOURCES = impcap.c
impcap_la_CPPFLAGS = -I$(top_srcdir) $(PTHREADS_CFLAGS) $(RSRT_CFLAGS)
impcap_la_LDFLAGS = -module -avoid-version
impcap_la_LIBADD = -lpcap

impcap_la_SOURCES += arp_parser.c
impcap_la_SOURCES += eth_parser.c
impcap_la_SOURCES += icmp_parser.c
impcap_la_SOURCES += ipv4_parser.c
impcap_la_SOURCES += ipv6_parser.c
impcap_la_SOURCES += ipx_parser.c
impcap_la_SOURCES += llc_parser.c
impcap_la_SOURCES += udp_parser.c
impcap_la_SOURCES += dns_parser.c
impcap_la_SOURCES += tcp_parser.c
impcap_la_SOURCES += smb_parser.c
impcap_la_SOURCES += ftp_parser.c
impcap_la_SOURCES += http_parser.c

EXTRA_DIST=parsers.h
