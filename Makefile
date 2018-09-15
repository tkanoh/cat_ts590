PROG=	ts590
SRCS=	ts590.c
BINDIR=	/usr/local/bin
MKMAN=	no
CLEANFILES=	ts590.core

# tty device
#CFLAGS+=	-DTTY_DEV=\"/dev/ttyCY002\"
#CFLAGS+=	-DTTY_DEV=\"/dev/ttyCY004\"
CFLAGS+=	-DTTY_DEV=\"/dev/ttyU1\"
#CFLAGS+=	-DTTY_DEV=\"/dev/ttyU2\"
# baud rate
CFLAGS+=	-DB_TS590=B9600

.include <bsd.prog.mk>
