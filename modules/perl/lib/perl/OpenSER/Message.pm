#
# $Id: Message.pm 1992 2007-04-12 16:13:07Z bastian $
#
# Perl module for OpenSER
#
# Copyright (C) 2006 Collax GmbH
#		     (Bastian Friedrich <bastian.friedrich@collax.com>)
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

package OpenSER::Message;
require Exporter;
require DynaLoader;

our @ISA = qw(Exporter DynaLoader);
our @EXPORT = qw ( t );
bootstrap OpenSER;

sub AUTOLOAD{
	use vars qw($AUTOLOAD);
	my $a = $AUTOLOAD;
	
	$a =~ s/^OpenSER::Message:://;

	my $l = scalar @_;
	if ($l == 0) {
		croak("Usage: $a(self, param1 = undef, param2 = undef)");
	} elsif ($l == 1) {
		return OpenSER::Message::moduleFunction(@_[0], $a);
	} elsif ($l == 2) {
		return OpenSER::Message::moduleFunction(@_[0], $a, @_[1]);
	} elsif ($l == 3) {
		return OpenSER::Message::moduleFunction(@_[0],
							$a, @_[1], @_[2]);
	} else {
		croak("Usage: $a(self, param1 = undef, param2 = undef)");
	}

}

sub DESTROY {}

1;

