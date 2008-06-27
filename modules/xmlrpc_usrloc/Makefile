# $Id: Makefile 2 2005-06-13 16:47:24Z bogdan_iancu $
#
# Usrloc module Makefile
#
# 
# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs
auto_gen=
NAME=xmlrpc_usrloc.so
LIBS=
MY_DEFS=-I/Users/elias/src/c/xmlrpc-c/include -D_UNIX
MY_LIBS=-L/opt/local/lib -lwwwxml -lxmltok -lxmlparse -lwwwzip -lwwwinit -lwwwapp -lwwwhtml -lwwwtelnet -lwwwnews -lwwwhttp -lwwwmime -lwwwgopher -lwwwftp -lwwwfile -lwwwdir -lwwwcache -lwwwstream -lwwwmux -lwwwtrans -lwwwcore -lwwwutils -lmd5 -ldl -lz -L/opt/local/lib -lssl -lcrypto -lz -L/opt/local/lib -lcurl -L/opt/local/lib -L/opt/local/lib -lz /Users/elias/src/c/xmlrpc-c/src/.libs/libxmlrpc_client.a /Users/elias/src/c/xmlrpc-c/src/.libs/libxmlrpc.a /Users/elias/src/c/xmlrpc-c/lib/libutil/.libs/libxmlrpc_util.a /Users/elias/src/c/xmlrpc-c/lib/expat/xmlparse/.libs/libxmlrpc_xmlparse.a /Users/elias/src/c/xmlrpc-c/lib/expat/xmltok/.libs/libxmlrpc_xmltok.a

DEFS+=$(MY_DEFS)
LIBS+=$(MY_LIBS)

include ../../Makefile.modules
