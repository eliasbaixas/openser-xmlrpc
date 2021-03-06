#
# $Id: OpenSER.pm 1992 2007-04-12 16:13:07Z bastian $
#
# Perl module for OpenSER
#
# Copyright (C) 2006 Collax GmbH
#                    (Bastian Friedrich <bastian.friedrich@collax.com>)
#
# This file is part of openser, a free SIP server.
#
# openser is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version
#
# openser is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
#

package OpenSER;
require Exporter;
require DynaLoader;

@ISA = qw(Exporter DynaLoader);
@EXPORT = qw ( t );
@EXPORT_OK = qw ( log );

use OpenSER::Message;
use OpenSER::Constants;
use OpenSER::Utils::Debug;

bootstrap OpenSER;


BEGIN {
	$SIG{'__DIE__'} = sub {
		OpenSER::Message::log(undef, L_ERR, "perl error: $_[0]\n");
        };
	$SIG{'__WARN__'} = sub {
		OpenSER::Message::log(undef, L_ERR, "perl warning: $_[0]\n");
        };
}

1;

