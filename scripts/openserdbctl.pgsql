# $Id: openserdbctl.pgsql 3580 2008-01-24 16:21:39Z henningw $
#
# Script for adding and dropping OpenSER Postgres tables
#
# History:
# 2006-05-16  added ability to specify MD5 from a configuration file
#             FreeBSD does not have the md5sum function (norm)
# 2006-07-14  Corrected syntax from MySQL to Postgres (norm)
#             moved INDEX creation out of CREATE table statement into 
#                  CREATE INDEX (usr_preferences, trusted)
#             auto_increment isn't valid in Postgres, replaced with 
#                  local AUTO_INCREMENT
#             datetime isn't valid in Postgres, replaced with local DATETIME 
#             split GRANTs for SERWeb tables so that it is only executed 
#                  if SERWeb tables are created
#             added GRANTs for re_grp table
#             added CREATE pdt table (from PDT module)
#             corrected comments to indicate Postgres as opposed to MySQL
#             made last_modified/created stamps consistent to now() using 
#                  local TIMESTAMP
# 2006-10-19  Added address table (bogdan)
# 2006-10-27  subscriber table cleanup; some columns are created only if
#             serweb is installed (bogdan)
# 2007-01-26  added seperate installation routine for presence related tables
#             and fix permissions for the SERIAL sequences.
# 2007-05-21  Move SQL database definitions out of this script (henning)
# 2007-05-31  Move common definitions to openserdbctl.base file (henningw)
#
# 2007-06-11  Use a common control tool for database tasks, like the openserctl

# path to the database schemas
DATA_DIR="/usr/local/share/openser"
if [ -d "$DATA_DIR/postgres" ] ; then
	DB_SCHEMA="$DATA_DIR/postgres"
else
	DB_SCHEMA="./postgres"
fi

#################################################################
# config vars
#################################################################

# full privileges Postgres user
if [ -z "$DBROOTUSER" ]; then
	DBROOTUSER="postgres"
	if [ ! -r ~/.pgpass ]; then
		merr "~./pgpass does not exist, please create this file and support proper credentials for user postgres."
		merr "Note: you need at least postgresql>= 7.3"
		exit 1
	fi
fi

CMD="psql -q -h $DBHOST -U $DBROOTUSER "
DUMP_CMD="pg_dump -h $DBHOST -U $DBROOTUSER -c"
#################################################################


# execute sql command with optional db name
sql_query()
{
	if [ $# -gt 1 ] ; then
		if [ -n "$1" ]; then
			DB="$1"
		else
			DB=""
		fi
		shift
		$CMD -d $DB -c "$@"
	else
		$CMD "$@"
	fi
}


openser_drop()  # pars: <database name>
{
if [ $# -ne 1 ] ; then
	merr "openser_drop function takes two params"
	exit 1
fi

# postgresql users are not dropped automatically
sql_query "template1" "drop database \"$1\"; drop user \"$DBRWUSER\"; drop user \"$DBROUSER\";"

if [ $? -ne 0 ] ; then
	merr "Dropping database $1 failed!"
	exit 1
fi
minfo "Database $1 dropped"
} #openser_drop


openser_create () # pars: <database name>
{
if [ $# -ne 1 ] ; then
	merr "openser_create function takes one param"
	exit 1
fi

minfo "creating database $1 ..."

sql_query "template1" "create database \"$1\";"
if [ $? -ne 0 ] ; then
	merr "Creating database failed!"
	exit 1
fi

sql_query "$1" "CREATE FUNCTION "concat" (text,text) RETURNS text AS 'SELECT \$1 || \$2;' LANGUAGE 'sql';
	        CREATE FUNCTION "rand" () RETURNS double precision AS 'SELECT random();' LANGUAGE 'sql';"
# emulate mysql proprietary functions used by the lcr module in postgresql

if [ $? -ne 0 ] ; then
	merr "Creating mysql emulation functions failed!"
	exit 1
fi

for TABLE in $STANDARD_MODULES; do
    mdbg "Creating core table: $TABLE"
    sql_query "$1" < $DB_SCHEMA/$TABLE-create.sql
    if [ $? -ne 0 ] ; then
	merr "Creating core tables failed!"
	exit 1
    fi
done

sql_query "$1" "CREATE USER $DBRWUSER WITH PASSWORD '$DBRWPW';
		CREATE USER $DBROUSER WITH PASSWORD '$DBROPW';"
if [ $? -ne 0 ] ; then
	mwarn "Create user in database failed, perhaps they allready exist? Try to continue.."
fi

for TABLE in $STANDARD_TABLES; do
	sql_query "$1" "GRANT ALL PRIVILEGES ON TABLE $TABLE TO $DBRWUSER;"
	sql_query "$1" "GRANT SELECT ON TABLE $TABLE TO $DBROUSER;"
	if [ $TABLE != "version" -a $TABLE != "gw_grp" ] ; then
		sql_query "$1" "GRANT ALL PRIVILEGES ON TABLE "$TABLE"_id_seq TO $DBRWUSER;"
    	sql_query "$1" "GRANT SELECT ON TABLE "$TABLE"_id_seq TO $DBROUSER;"
	fi

	if [ $? -ne 0 ] ; then
		merr "Grant privileges to standard tables failed!"
		exit 1
	fi
done

if [ -e $DB_SCHEMA/extensions-create.sql ]
then
	minfo "Creating custom extensions tables"
	sql_query $1 < $DB_SCHEMA/extensions-create.sql
	if [ $? -ne 0 ] ; then
	merr "Creating custom extensions tables failed!"
	exit 1
	fi
fi

minfo "Core OpenSER tables succesfully created."

get_answer $INSTALL_PRESENCE_TABLES "Install presence related tables? (y/n): "
if [ "$ANSWER" = "y" ]; then
	presence_create $1
fi

get_answer $INSTALL_EXTRA_TABLES "Install tables for $EXTRA_MODULES? (y/n): "
if [ "$ANSWER" = "y" ]; then
	extra_create $1
fi

get_answer $INSTALL_SERWEB_TABLES "Install SERWEB related tables? (y/n): "
if [ "$ANSWER" = "y" ]; then
	serweb_create $1
fi
} # openser_create


presence_create () # pars: <database name>
{
if [ $# -ne 1 ] ; then
	merr "presence_create function takes one param"
	exit 1
fi

minfo "creating presence tables into $1 ..."

sql_query "$1" < $DB_SCHEMA/presence-create.sql

if [ $? -ne 0 ] ; then
	merr "Failed to create presence tables!"
	exit 1
fi

sql_query "$1" < $DB_SCHEMA/rls-create.sql

if [ $? -ne 0 ] ; then
	merr "Failed to create rls-presence tables!"
	exit 1
fi

for TABLE in $PRESENCE_TABLES; do
	sql_query "$1" "GRANT ALL PRIVILEGES ON TABLE $TABLE TO $DBRWUSER;"
	sql_query "$1" "GRANT SELECT ON TABLE $TABLE TO $DBROUSER;"
	sql_query "$1" "GRANT ALL PRIVILEGES ON TABLE "$TABLE"_id_seq TO $DBRWUSER;"
    sql_query "$1" "GRANT SELECT ON TABLE "$TABLE"_id_seq TO $DBROUSER;"
	if [ $? -ne 0 ] ; then
		merr "Grant privileges to presence tables failed!"
		exit 1
	fi
done

minfo "Presence tables succesfully created."
}  # end presence_create


extra_create () # pars: <database name>
{
if [ $# -ne 1 ] ; then
	merr "extra_create function takes one param"
	exit 1
fi

minfo "creating extra tables into $1 ..."

for TABLE in $EXTRA_MODULES; do
    mdbg "Creating extra table: $TABLE"
    sql_query "$1" < $DB_SCHEMA/$TABLE-create.sql
    if [ $? -ne 0 ] ; then
	merr "Creating extra tables failed!"
	exit 1
    fi
done

for TABLE in $EXTRA_TABLES; do
	sql_query "$1" "GRANT ALL PRIVILEGES ON TABLE $TABLE TO $DBRWUSER;"
	sql_query "$1" "GRANT SELECT ON TABLE $TABLE TO $DBROUSER;"
	if [ $TABLE != "route_tree" ] ; then
		sql_query "$1" "GRANT ALL PRIVILEGES ON TABLE "$TABLE"_id_seq TO $DBRWUSER;"
	    sql_query "$1" "GRANT SELECT ON TABLE "$TABLE"_id_seq TO $DBROUSER;"
	fi
	if [ $? -ne 0 ] ; then
		merr "Grant privileges to extra tables failed!"
		exit 1
	fi
done

minfo "Extra tables succesfully created."
}  # end extra_create


serweb_create () # pars: <database name>
{
if [ $# -ne 1 ] ; then
	merr "serweb_create function takes one param"
	exit 1
fi

minfo "creating serweb tables into $1 ..."

# Extend table 'subscriber' with serweb specific columns
# It would be easier to drop the table and create a new one,
# but in case someone want to add serweb and has already
# a populated subscriber table, we show here how this
# can be done without deleting the existing data.
# Tested with postgres 7.4.7

sql_query "$1" "ALTER TABLE subscriber ADD COLUMN phplib_id varchar(32);
		ALTER TABLE subscriber ADD COLUMN phone varchar(15);
		ALTER TABLE subscriber ADD COLUMN datetime_modified timestamp;
		ALTER TABLE subscriber ADD COLUMN confirmation varchar(64);
		ALTER TABLE subscriber ADD COLUMN flag char(1);
		ALTER TABLE subscriber ADD COLUMN sendnotification varchar(50);
		ALTER TABLE subscriber ADD COLUMN greeting varchar(50);
		ALTER TABLE subscriber ADD COLUMN allow_find char(1);
		ALTER TABLE subscriber ADD CONSTRAINT phplib_id_key unique (phplib_id);

		ALTER TABLE subscriber ALTER phplib_id SET NOT NULL;
		ALTER TABLE subscriber ALTER phplib_id SET DEFAULT '';
		ALTER TABLE subscriber ALTER phone SET NOT NULL;
		ALTER TABLE subscriber ALTER phone SET DEFAULT '';
		ALTER TABLE subscriber ALTER datetime_modified SET NOT NULL;
		ALTER TABLE subscriber ALTER datetime_modified SET DEFAULT NOW();
		ALTER TABLE subscriber ALTER confirmation SET NOT NULL;
		ALTER TABLE subscriber ALTER confirmation SET DEFAULT '';
		ALTER TABLE subscriber ALTER flag SET NOT NULL;
		ALTER TABLE subscriber ALTER flag SET DEFAULT 'o';
		ALTER TABLE subscriber ALTER sendnotification SET NOT NULL;
		ALTER TABLE subscriber ALTER sendnotification SET DEFAULT '';
		ALTER TABLE subscriber ALTER greeting SET NOT NULL;
		ALTER TABLE subscriber ALTER greeting SET DEFAULT '';
		ALTER TABLE subscriber ALTER allow_find SET NOT NULL;
		ALTER TABLE subscriber ALTER allow_find SET DEFAULT '0';

		UPDATE subscriber SET phplib_id = DEFAULT, phone = DEFAULT, datetime_modified = DEFAULT,
		confirmation = DEFAULT, flag = DEFAULT, sendnotification = DEFAULT, 
		greeting = DEFAULT, allow_find = DEFAULT;"

if [ $? -ne 0 ] ; then
	merr "Failed to alter subscriber table for serweb!"
	exit 1
fi

sql_query "$1" < $DB_SCHEMA/serweb-create.sql

if [ $? -ne 0 ] ; then
	merr "Failed to create presence tables!"
	exit 1
fi

for TABLE in $SERWEB_TABLES; do
	sql_query "$1" "GRANT ALL PRIVILEGES ON TABLE $TABLE TO $DBRWUSER;"
	sql_query "$1" "GRANT SELECT ON TABLE $TABLE TO $DBROUSER;"
	sql_query "$1" "GRANT ALL PRIVILEGES ON TABLE phonebook_id_seq TO $DBRWUSER;"
	sql_query "$1" "GRANT SELECT ON TABLE phonebook_id_seq TO $DBROUSER;"
	sql_query "$1" "GRANT ALL PRIVILEGES ON TABLE pending_id_seq TO $DBRWUSER;"
	sql_query "$1" "GRANT SELECT ON TABLE pending_id_seq TO $DBROUSER;"
	if [ $? -ne 0 ] ; then
		merr "Grant privileges to serweb tables failed!"
		exit 1
	fi
done

if [ -z "$NO_USER_INIT" ] ; then
	if [ -z "$SIP_DOMAIN" ] ; then
		prompt_realm
	fi
	credentials
	sql_query "$1" "INSERT INTO subscriber ($USERCOL, password, first_name, last_name,
			phone, email_address, datetime_created, datetime_modified, confirmation,
			flag, sendnotification, greeting, ha1, domain, ha1b, phplib_id )
			VALUES ( 'admin', '$DBRWPW', 'Initial', 'Admin', '123', 'root@localhost', 
			'2002-09-04 19:37:45', '1900-01-01 00:00:01', '57DaSIPuCm52UNe54LF545750cfdL48OMZfroM53',
			'o', '', '', '$HA1', '$SIP_DOMAIN', '$HA1B', '$PHPLIB_ID' );
			INSERT INTO admin_privileges ($USERCOL, domain, priv_name, priv_value)
			VALUES ('admin', '$SIP_DOMAIN', 'is_admin', '1');
			INSERT INTO admin_privileges ($USERCOL, domain, priv_name, priv_value)
			VALUES ('admin', '$SIP_DOMAIN', 'change_privileges', '1');"

	if [ $? -ne 0 ] ; then
		merr "Failed to create serweb credentials tables!"
		exit 1
	fi
	serweb_message
fi

# emulate mysql proprietary functions used by the serweb in postgresql
sql_query "$1" "CREATE FUNCTION truncate(numeric, int) RETURNS numeric AS 'SELECT trunc(\$1,\$2);' LANGUAGE 'sql';
		CREATE FUNCTION unix_timestamp(timestamp) RETURNS integer AS 'SELECT date_part(''epoch'', \$1)::int4 
		AS result' LANGUAGE 'sql';"

if [ $? -ne 0 ] ; then
	merr "Failed to create mysql emulation functions for serweb!"
	exit 1
fi

minfo "SERWEB tables succesfully created."
}  # end serweb_create
