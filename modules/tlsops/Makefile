# $Id: Makefile 997 2006-07-10 11:49:10Z bogdan_iancu $
#
# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs
auto_gen=
NAME=tlsops.so
LIBS=

DEFS+= -I$(LOCALBASE)/ssl/include
LIBS+= -L$(LOCALBASE)/lib -L$(LOCALBASE)/ssl/lib -lssl  -lcrypto

include ../../Makefile.modules


