AUTOMAKE_OPTIONS = foreign

YFLAGS = -d

sbin_PROGRAMS = nat64d #tap802ipd

noinst_LIBRARIES = liblips.a libptrtd.a

liblips_a_SOURCES = if.c ether.c if_tuntap.c if_802ip.c if_uml_sw.c icmp.c \
	pbuf.c buffer.c tcp.c udp.c tcb.c util.c rbtree.c \
	buffer.h defs.h icmp.h pbuf.h if.h ether.h event.h udp.h tcp.h tcb.h \
	util.h rbtree.h

libptrtd_a_SOURCES = ptrtd.c ptrtd-tcp.c ptrtd-udp.c event.c http_status.c \
	buffer.h defs.h icmp.h pbuf.h if.h ether.h event.h tcp.h udp.h tcb.h \
	util.h http_status.h

nat64d_SOURCES = main.c scanner.l grammar.y

nat64d_LDADD = libptrtd.a liblips.a

scanner.c: grammar.c

#tap802ipd_SOURCES = tap802ipd.c if.c ether.c if_tuntap.c if_802ip.c \
#	if_uml_sw.c pbuf.c event.c \
#	defs.h pbuf.h if.h ether.h event.h util.h http_status.h http_status.c

#tap802ipd_LDADD = liblips.a

