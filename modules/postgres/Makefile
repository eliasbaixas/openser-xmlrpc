# $Id: Makefile 3084 2007-11-09 16:19:42Z miconda $
#
# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs
auto_gen=
NAME=postgres.so


# set CROSS_COMPILE to true if you want to skip
# the autodetection
# CROSS_COMPILE=true

ifeq ($(CROSS_COMPILE),)
PGCFG=$(shell which pg_config)
endif

ifneq ($(PGCFG),)

	# use autodetection
	DEFS += -I$(shell $(PGCFG) --includedir)
	LIBS = -L$(shell $(PGCFG) --libdir) -lpq

else

	# use standard know paths
	# libpq-fe.h locations
	DEFS +=-I$(LOCALBASE)/include -I$(LOCALBASE)/pgsql/include \
		 -I$(SYSBASE)/include/pgsql -I$(SYSBASE)/include/postgresql \
		 -I$(SYSBASE)/include/postgresql/8.0
	LIBS=-L$(LOCALBASE)/lib -L$(LOCALBASE)/pgsql/lib -L$(LOCALBASE)/lib/pgsql \
		 -lpq

endif

include ../../Makefile.modules
