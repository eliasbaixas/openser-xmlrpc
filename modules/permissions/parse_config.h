/*
 * $Id: parse_config.h 2 2005-06-13 16:47:24Z bogdan_iancu $
 *
 * PERMISSIONS module
 *
 * Copyright (C) 2003 Mikl�s Tirp�k (mtirpak@sztaki.hu)
 *
 * This file is part of openser, a free SIP server.
 *
 * openser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * openser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifndef PARSE_CONFIG_H
#define PARSE_CONFIG_H 1

#include "rule.h"

rule *parse_config_file(char *filename);

#endif /* PARSE_CONFIG_H */
