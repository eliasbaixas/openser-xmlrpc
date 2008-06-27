<?xml version='1.0'?>
<!--
 * $Id: sql.xsl 3026 2007-11-06 08:35:21Z henningw $
 *
 * XSL converter script for SQL
 *
 * Copyright (C) 2001-2007 FhG Fokus
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
-->

<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
                version='1.0'
>

    <xsl:import href="common.xsl"/>

    <xsl:template match="/">
	<xsl:variable name="createfile" select="concat($dir, concat('/', concat($prefix, 'create.sql')))"/>
	<xsl:document href="{$createfile}" method="text" indent="no" omit-xml-declaration="yes">
	    <xsl:apply-templates select="/database[1]"/>
	</xsl:document>

    </xsl:template>

<!-- ################ DATABASE ################# -->

<!-- ################ /DATABASE ################# -->


<!-- ################ TABLE  ################# -->

    <xsl:template match="table">
	<xsl:variable name="table.name">
	    <xsl:call-template name="get-name"/>
	</xsl:variable>

	<!-- Create row in version table -->
	<xsl:apply-templates select="version"/>

	<xsl:text>CREATE TABLE </xsl:text>
	<xsl:value-of select="$table.name"/>
	<xsl:text> (&#x0A;</xsl:text>

	<!-- Process all columns -->
	<xsl:apply-templates select="column"/>

	<!-- Process all indexes -->
	<xsl:apply-templates select="index"/>

	<xsl:text>&#x0A;</xsl:text>

	<xsl:call-template name="table.close"/>
    </xsl:template>

    <xsl:template name="table.close">
	<xsl:text>);&#x0A;&#x0A;</xsl:text>
    </xsl:template>

    <xsl:template match="version">
	<xsl:text>INSERT INTO version (table_name, table_version) values ('</xsl:text>
	<xsl:call-template name="get-name">
	    <xsl:with-param name="select" select="parent::table"/>
	</xsl:call-template>
	<xsl:text>','</xsl:text>
	<xsl:value-of select="text()"/>
	<xsl:text>');&#x0A;</xsl:text>
    </xsl:template>
    
<!-- ################ /TABLE ################  -->


<!-- ################ COLUMN ################  -->

    <xsl:template match="column">
	<xsl:text>    </xsl:text>
	<xsl:call-template name="get-name"/>
	<xsl:text> </xsl:text>

	<xsl:call-template name="column.type"/>

	<xsl:variable name="null">
	    <xsl:call-template name="get-null"/>
	</xsl:variable>
	<xsl:if test="$null=0">
	    <xsl:text> NOT NULL</xsl:text>
	</xsl:if>

	<xsl:choose>
	    <xsl:when test="default[@db=$db]">
		<xsl:text> DEFAULT </xsl:text>
		<xsl:choose>
		    <xsl:when test="default[@db=$db]/null">
			<xsl:text>NULL</xsl:text>
		    </xsl:when>
		    <xsl:otherwise>
			<xsl:text>'</xsl:text>
			<xsl:value-of select="default[@db=$db]"/>
			<xsl:text>'</xsl:text>
		    </xsl:otherwise>
		</xsl:choose>
	    </xsl:when>
	    <xsl:when test="default">
		<xsl:text> DEFAULT </xsl:text>
		<xsl:choose>
		    <xsl:when test="default/null">
			<xsl:text>NULL</xsl:text>
		    </xsl:when>
		    <xsl:when test= "string(number(default))='NaN'"><!-- test for string value -->
			<xsl:text>'</xsl:text>
			<xsl:value-of select="default"/>
			<xsl:text>'</xsl:text>
		    </xsl:when>
		    <xsl:otherwise>
			<xsl:value-of select="default"/><!-- ommit the quotes for numbers -->
		    </xsl:otherwise>
		</xsl:choose>
	    </xsl:when>
	</xsl:choose>

	<xsl:if test="not(position()=last())">
	    <xsl:text>,</xsl:text>
	    <xsl:text>&#x0A;</xsl:text>
	</xsl:if>
    </xsl:template>

    <xsl:template name="column.type">
	<!-- FIXME -->
	<xsl:call-template name="get-type"/>
	<xsl:call-template name="column.size"/>
	<xsl:call-template name="column.trailing"/>
    </xsl:template>

    <xsl:template name="column.size">
	<xsl:variable name="size">
	    <xsl:call-template name="get-size"/>
	</xsl:variable>

	<xsl:if test="not($size='')">
	    <xsl:text>(</xsl:text>
	    <xsl:value-of select="$size"/>
	    <xsl:text>)</xsl:text>
	</xsl:if>
    </xsl:template>

    <xsl:template name="column.trailing"/>

<!-- ################ /COLUMN ################  -->

<!-- ################ INDEX ################  -->

    <xsl:template match="index">
	<!-- Translate unique indexes into SQL92 unique constraints -->
	<xsl:if test="unique">
	    <xsl:if test="position()=1">
		<xsl:text>,&#x0A;</xsl:text>
	    </xsl:if>
	    <xsl:text>    </xsl:text>

	    <xsl:call-template name="get-name"/>
	    <xsl:text> UNIQUE (</xsl:text>

	    <xsl:apply-templates match="colref"/>

	    <xsl:text>)</xsl:text>

	    <xsl:if test="not(position()=last())">
		<xsl:text>,</xsl:text>
		<xsl:text>&#x0A;</xsl:text>
	    </xsl:if>
	</xsl:if>
    </xsl:template>

<!-- ################ /INDEX ################  -->

<!-- ################ COLREF ################  -->

    <xsl:template match="colref">
	<xsl:call-template name="get-column-name">
	    <xsl:with-param name="select" select="@linkend"/>
	</xsl:call-template>
	<xsl:if test="not(position()=last())">
	    <xsl:text>, </xsl:text>
	</xsl:if>
    </xsl:template>

<!-- ################ /COLREF ################  -->

</xsl:stylesheet>
