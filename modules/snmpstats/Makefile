# $Id: Makefile 1938 2007-04-02 14:35:02Z bogdan_iancu $
#
# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs

BUILDER = $(shell which net-snmp-config)
ifeq ($(BUILDER),)
	DEFS +=-I$(LOCALBASE)/include
	BUILDAGENTLIBS =-L$(LOCALBASE)/lib -lnetsnmpmibs -lnetsnmpagent \
		-lnetsnmphelpers -lnetsnmp
	INSTALLMIBDIR = $(cfg-prefix)/share/snmp/mibs
else
	BUILDAGENTLIBS = $(shell net-snmp-config --agent-libs)
	INSTALLMIBDIR = $(cfg-prefix)$(shell net-snmp-config --prefix)/share/snmp/mibs
endif 

CFLAGS+=
auto_gen=
NAME=snmpstats.so
LIBS=$(BUILDAGENTLIBS)


include ../../Makefile.modules

install_module_custom:
	echo "installing mibs ..."
	mkdir -p $(INSTALLMIBDIR)
	$(INSTALL_CFG) mibs/OPENSER* $(INSTALLMIBDIR)

