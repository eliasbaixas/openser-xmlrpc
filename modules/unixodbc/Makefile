# $Id: Makefile 470 2005-12-13 17:24:11Z bogdan_iancu $
#
# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs
auto_gen=
NAME=unixodbc.so

# sql.h locations (freebsd,openbsd  solaris)
DEFS += -I$(LOCALBASE)/include

# libodbc locations on RH/Suse, Solaris /OpenBSD, FreeBSD
# (Debian does the right thing and puts it in /usr/lib)
LIBS= -L$(LOCALBASE)/lib -lodbc -lz

include ../../Makefile.modules
