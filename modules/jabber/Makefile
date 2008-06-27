# $Id: Makefile 2 2005-06-13 16:47:24Z bogdan_iancu $
#
# example module makefile
#
# 
# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs
auto_gen=
NAME=jabber.so

# extra debug messages
DEFS+=-DXJ_EXTRA_DEBUG
# -DHAVE_IHTTP

# expat.h location
DEFS+=-I$(LOCALBASE)/include
LIBS=-L$(LOCALBASE)/lib -lexpat

include ../../Makefile.modules
