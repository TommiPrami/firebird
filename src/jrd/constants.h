/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		constants.h
 *	DESCRIPTION:	Misc system constants
 *
 * The contents of this file are subject to the Interbase Public
 * License Version 1.0 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy
 * of the License at http://www.Inprise.com/IPL.html
 *
 * Software distributed under the License is distributed on an
 * "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, either express
 * or implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code was created by Inprise Corporation
 * and its predecessors. Portions created by Inprise Corporation are
 * Copyright (C) Inprise Corporation.
 *
 * All Rights Reserved.
 * Contributor(s): ______________________________________.
 *
 * 2001.10.08 Claudio Valderrama: fb_sysflag enum with numbering
 *   for automatically created triggers that aren't system triggers.
 */

#ifndef JRD_CONSTANTS_H
#define JRD_CONSTANTS_H

// BLOb Subtype definitions

/* Subtypes < 0  are user defined
 * Subtype  0    means "untyped"
 * Subtypes > 0  are Firebird defined
 */

// BRS 29-Apr-2004
// replace those constants with public defined ones isc_blob_*
//
//const int BLOB_untyped	= 0;
//
//const int BLOB_text		= 1;
//const int BLOB_blr		= 2;
//const int BLOB_acl		= 3;
//const int BLOB_ranges	= 4;
//const int BLOB_summary	= 5;
//const int BLOB_format	= 6;
//const int BLOB_tra		= 7;
//const int BLOB_extfile	= 8;
//const int BLOB_max_predefined_subtype = 9;
//

// Column Limits (in bytes)

const ULONG MAX_COLUMN_SIZE = 32767;
const ULONG MAX_VARY_COLUMN_SIZE = MAX_COLUMN_SIZE - sizeof(USHORT);

const ULONG MAX_STR_SIZE = 65535;

const int TEMP_STR_LENGTH = 128;

// Metadata constants

const unsigned METADATA_IDENTIFIER_CHAR_LEN	= 63;
const unsigned METADATA_BYTES_PER_CHAR		= 4;

// Misc constant values

const unsigned int USERNAME_LENGTH	= METADATA_IDENTIFIER_CHAR_LEN * METADATA_BYTES_PER_CHAR;

const FB_SIZE_T MAX_SQL_IDENTIFIER_LEN = METADATA_IDENTIFIER_CHAR_LEN * METADATA_BYTES_PER_CHAR;
const FB_SIZE_T MAX_SQL_IDENTIFIER_SIZE = MAX_SQL_IDENTIFIER_LEN + 1;
const FB_SIZE_T MAX_CONFIG_NAME_LEN = 63;

// Every character of the name may be a double-quote needed to be escaped (with another double-quote).
// The name would also need to be enclosed in double-quotes.
// There is two names, separated by a dot.
const FB_SIZE_T MAX_QUALIFIED_NAME_TO_STRING_LEN =
	((METADATA_IDENTIFIER_CHAR_LEN * 2 + 2) * 2 + 1) * METADATA_BYTES_PER_CHAR;

const ULONG MAX_SQL_LENGTH = 10 * 1024 * 1024; // 10 MB - just a safety check

const char* const DB_KEY_NAME = "DB_KEY";
const char* const RDB_DB_KEY_NAME = "RDB$DB_KEY";
const char* const RDB_RECORD_VERSION_NAME = "RDB$RECORD_VERSION";

const char* const NULL_STRING_MARK = "*** null ***";
const char* const UNKNOWN_STRING_MARK = "*** unknown ***";

const char* const ISC_USER = "ISC_USER";
const char* const ISC_PASSWORD = "ISC_PASSWORD";

const char* const NULL_ROLE = "NONE";
#define ADMIN_ROLE "RDB$ADMIN"		// It's used in C-string concatenations

// User name assigned to any user granted USR_locksmith rights.
// If this name is changed, modify also the trigger in
// jrd/grant.gdl (which turns into jrd/trig.h.
const char* const DBA_USER_NAME		= "SYSDBA";

const char* const PRIMARY_KEY		= "PRIMARY KEY";
const char* const FOREIGN_KEY		= "FOREIGN KEY";
const char* const UNIQUE_CNSTRT		= "UNIQUE";
const char* const CHECK_CNSTRT		= "CHECK";
const char* const NOT_NULL_CNSTRT	= "NOT NULL";

const char* const REL_SCOPE_PERSISTENT		= "persistent table %s";
const char* const REL_SCOPE_GTT_PRESERVE	= "global temporary table %s of type ON COMMIT PRESERVE ROWS";
const char* const REL_SCOPE_GTT_DELETE		= "global temporary table %s of type ON COMMIT DELETE ROWS";
const char* const REL_SCOPE_EXTERNAL		= "external table %s";
const char* const REL_SCOPE_VIEW			= "view %s";
const char* const REL_SCOPE_VIRTUAL			= "virtual table %s";

// literal strings in rdb$ref_constraints to be used to identify
// the cascade actions for referential constraints. Used
// by isql/show and isql/extract for now.

const char* const RI_ACTION_CASCADE = "CASCADE";
const char* const RI_ACTION_NULL    = "SET NULL";
const char* const RI_ACTION_DEFAULT = "SET DEFAULT";
const char* const RI_ACTION_NONE    = "NO ACTION";
const char* const RI_RESTRICT       = "RESTRICT";

// Automatically created domains for fields with direct data type.
// Also, automatically created indices that are unique or non-unique, but not PK.
const char* const IMPLICIT_DOMAIN_PREFIX = "RDB$";
const int IMPLICIT_DOMAIN_PREFIX_LEN = 4;

// Automatically created indices for PKs.
const char* const IMPLICIT_PK_PREFIX = "RDB$PRIMARY";
const int IMPLICIT_PK_PREFIX_LEN = 11;

// The invisible "id zero" generator.
const char* const MASTER_GENERATOR = ""; //Was "RDB$GENERATORS";

constexpr const char* SYSTEM_SCHEMA = "SYSTEM";
constexpr const char* PUBLIC_SCHEMA = "PUBLIC";
constexpr const char* PLG_LEGACY_SEC_SCHEMA = "PLG$LEGACY_SEC";

// Automatically created security classes for SQL objects.
// Keep in sync with trig.h
const char* const DEFAULT_CLASS				= "SQL$DEFAULT";
const char* const SQL_SECCLASS_GENERATOR	= "RDB$SECURITY_CLASS";
const char* const SQL_SECCLASS_PREFIX		= "SQL$";
const int SQL_SECCLASS_PREFIX_LEN			= 4;
const char* const SQL_FLD_SECCLASS_PREFIX	= "SQL$GRANT";
const int SQL_FLD_SECCLASS_PREFIX_LEN		= 9;

const char* const SQL_DDL_SECCLASS_FORMAT	= "SQL$D%02d%s";
const int SQL_DDL_SECCLASS_PREFIX_LEN		= 7;

const char* const GEN_SECCLASS_PREFIX		= "GEN$";
const int GEN_SECCLASS_PREFIX_LEN			= 4;

const char* const PROCEDURES_GENERATOR = "RDB$PROCEDURES";
const char* const FUNCTIONS_GENERATOR = "RDB$FUNCTIONS";

// Automatically created check constraints for unnamed PRIMARY and UNIQUE declarations.
const char* const IMPLICIT_INTEGRITY_PREFIX = "INTEG_";
const int IMPLICIT_INTEGRITY_PREFIX_LEN = 6;

// Default publication name
const char* const DEFAULT_PUBLICATION = "RDB$DEFAULT";

//*****************************************
// System flag meaning - mainly Firebird.
//*****************************************

enum fb_sysflag {
	fb_sysflag_user = 0,
	fb_sysflag_system = 1,
	fb_sysflag_qli = 2,
	fb_sysflag_check_constraint = 3,
	fb_sysflag_referential_constraint = 4,
	fb_sysflag_view_check = 5,
	fb_sysflag_identity_generator = 6
};

enum ViewContextType {
	VCT_TABLE,
	VCT_VIEW,
	VCT_PROCEDURE
};

enum IdentityType {
	IDENT_TYPE_ALWAYS,
	IDENT_TYPE_BY_DEFAULT
};

enum SubRoutineType
{
	SUB_ROUTINE_TYPE_PSQL
};

// UDF Arguments are numbered from 0 to MAX_UDF_ARGUMENTS --
// argument 0 is reserved for the return-type of the UDF

const unsigned MAX_UDF_ARGUMENTS	= 15;

// Maximum length of single line returned from pretty printer
const int PRETTY_BUFFER_SIZE = 1024;

const int MAX_INDEX_SEGMENTS = 16;

// Maximum index key length (must be in sync with MAX_PAGE_SIZE in ods.h)
const ULONG MAX_KEY			= 8192;		// Maximum page size possible divide by 4 (MAX_PAGE_SIZE / 4)

const USHORT SQL_MATCH_1_CHAR		= '_';	// Not translatable
const USHORT SQL_MATCH_ANY_CHARS	= '%';	// Not translatable

const size_t MAX_CONTEXT_VARS	= 1000;		// Maximum number of context variables allowed for a single object

// Time precision limits and defaults for TIME/TIMESTAMP values.
// Currently they're applied to CURRENT_TIME[STAMP] expressions only.

// Should be more than 6 as per SQL spec, but we don't support more than 3 yet
const size_t MAX_TIME_PRECISION			= 3;
// Consistent with the SQL spec
const size_t DEFAULT_TIME_PRECISION		= 0;
// Should be 6 as per SQL spec
const size_t DEFAULT_TIMESTAMP_PRECISION	= 3;

const size_t MAX_ARRAY_DIMENSIONS = 16;

const size_t MAX_SORT_ITEMS = 255; // ORDER BY f1,...,f255

const size_t MAX_DB_PER_TRANS = 256; // A multi-db txn can span up to 256 dbs

// relation types

enum rel_t {
	rel_persistent = 0,
	rel_view = 1,
	rel_external = 2,
	rel_virtual = 3,
	rel_global_temp_preserve = 4,
	rel_global_temp_delete = 5
};

// procedure types

enum prc_t {
	prc_legacy = 0,
	prc_selectable = 1,
	prc_executable = 2
};

// procedure parameter mechanism

enum prm_mech_t {
	prm_mech_normal = 0,
	prm_mech_type_of = 1
};

// states

enum mon_state_t {
	mon_state_idle = 0,
	mon_state_active = 1,
	mon_state_stalled = 2
};

// shutdown modes (match hdr_nbak_* in ods.h)

enum shut_mode_t {
	shut_mode_online = 0,
	shut_mode_multi = 1,
	shut_mode_single = 2,
	shut_mode_full = 3
};

// backup states (match hdr_backup_* in ods.h)

enum backup_state_t {
	backup_state_unknown = -1,
	backup_state_normal = 0,
	backup_state_stalled = 1,
	backup_state_merge = 2
};

// transaction isolation levels

enum tra_iso_mode_t {
	iso_mode_consistency = 0,
	iso_mode_concurrency = 1,
	iso_mode_rc_version = 2,
	iso_mode_rc_no_version = 3,
	iso_mode_rc_read_consistency = 4
};

// statistics groups

enum stat_group_t {
	stat_database = 0,
	stat_attachment = 1,
	stat_transaction = 2,
	stat_statement = 3,
	stat_call = 4,
	stat_cmp_statement = 5
};

enum InfoType
{
	INFO_TYPE_CONNECTION_ID = 1,
	INFO_TYPE_TRANSACTION_ID = 2,
	INFO_TYPE_GDSCODE = 3,
	INFO_TYPE_SQLCODE = 4,
	INFO_TYPE_ROWS_AFFECTED = 5,
	INFO_TYPE_TRIGGER_ACTION = 6,
	INFO_TYPE_SQLSTATE = 7,
	INFO_TYPE_EXCEPTION = 8,
	INFO_TYPE_ERROR_MSG = 9,
	INFO_TYPE_SESSION_RESETTING = 10,
	MAX_INFO_TYPE
};

// Replica modes (match hdr_replica_* in ods.h)

enum ReplicaMode
{
	REPLICA_NONE = 0,
	REPLICA_READ_ONLY = 1,
	REPLICA_READ_WRITE = 2
};

enum TriggerType
{
	PRE_STORE_TRIGGER = 1,
	POST_STORE_TRIGGER = 2,
	PRE_MODIFY_TRIGGER = 3,
	POST_MODIFY_TRIGGER = 4,
	PRE_ERASE_TRIGGER = 5,
	POST_ERASE_TRIGGER = 6
};

enum TriggerAction
{
	// Order should be maintained because the numbers are stored in BLR
	// and should be in sync with IExternalTrigger::ACTION_* .
	TRIGGER_INSERT = 1,
	TRIGGER_UPDATE = 2,
	TRIGGER_DELETE = 3,
	TRIGGER_CONNECT = 4,
	TRIGGER_DISCONNECT  = 5,
	TRIGGER_TRANS_START = 6,
	TRIGGER_TRANS_COMMIT = 7,
	TRIGGER_TRANS_ROLLBACK = 8,
	TRIGGER_DDL = 9
};

const unsigned TRIGGER_TYPE_SHIFT			= 13;
const FB_UINT64 TRIGGER_TYPE_MASK			= (QUADCONST(3) << TRIGGER_TYPE_SHIFT);

const FB_UINT64 TRIGGER_TYPE_DML			= (QUADCONST(0) << TRIGGER_TYPE_SHIFT);
const FB_UINT64 TRIGGER_TYPE_DB				= (QUADCONST(1) << TRIGGER_TYPE_SHIFT);
const FB_UINT64 TRIGGER_TYPE_DDL			= (QUADCONST(2) << TRIGGER_TYPE_SHIFT);

const unsigned DB_TRIGGER_CONNECT			= 0;
const unsigned DB_TRIGGER_DISCONNECT		= 1;
const unsigned DB_TRIGGER_TRANS_START		= 2;
const unsigned DB_TRIGGER_TRANS_COMMIT		= 3;
const unsigned DB_TRIGGER_TRANS_ROLLBACK	= 4;
const unsigned DB_TRIGGER_MAX				= 5;

static const char* const DDL_TRIGGER_ACTION_NAMES[][2] =
{
	{NULL, NULL},
	{"CREATE", "TABLE"},
	{"ALTER", "TABLE"},
	{"DROP", "TABLE"},
	{"CREATE", "PROCEDURE"},
	{"ALTER", "PROCEDURE"},
	{"DROP", "PROCEDURE"},
	{"CREATE", "FUNCTION"},
	{"ALTER", "FUNCTION"},
	{"DROP", "FUNCTION"},
	{"CREATE", "TRIGGER"},
	{"ALTER", "TRIGGER"},
	{"DROP", "TRIGGER"},
	{"", ""}, {"", ""}, {"", ""},	// gap for TRIGGER_TYPE_MASK - 3 bits
	{"CREATE", "EXCEPTION"},
	{"ALTER", "EXCEPTION"},
	{"DROP", "EXCEPTION"},
	{"CREATE", "VIEW"},
	{"ALTER", "VIEW"},
	{"DROP", "VIEW"},
	{"CREATE", "DOMAIN"},
	{"ALTER", "DOMAIN"},
	{"DROP", "DOMAIN"},
	{"CREATE", "ROLE"},
	{"ALTER", "ROLE"},
	{"DROP", "ROLE"},
	{"CREATE", "INDEX"},
	{"ALTER", "INDEX"},
	{"DROP", "INDEX"},
	{"CREATE", "SEQUENCE"},
	{"ALTER", "SEQUENCE"},
	{"DROP", "SEQUENCE"},
	{"CREATE", "USER"},
	{"ALTER", "USER"},
	{"DROP", "USER"},
	{"CREATE", "COLLATION"},
	{"DROP", "COLLATION"},
	{"ALTER", "CHARACTER SET"},
	{"CREATE", "PACKAGE"},
	{"ALTER", "PACKAGE"},
	{"DROP", "PACKAGE"},
	{"CREATE", "PACKAGE BODY"},
	{"DROP", "PACKAGE BODY"},
	{"CREATE", "MAPPING"},
	{"ALTER", "MAPPING"},
	{"DROP", "MAPPING"},
	{"CREATE", "SCHEMA"},
	{"ALTER", "SCHEMA"},
	{"DROP", "SCHEMA"}
};

const int DDL_TRIGGER_BEFORE	= 0;
const int DDL_TRIGGER_AFTER		= 1;

const FB_UINT64 DDL_TRIGGER_ANY					= 0x7FFFFFFFFFFFFFFFULL & ~(FB_UINT64) TRIGGER_TYPE_MASK & ~1ULL;

const int DDL_TRIGGER_CREATE_TABLE				= 1;
const int DDL_TRIGGER_ALTER_TABLE				= 2;
const int DDL_TRIGGER_DROP_TABLE				= 3;
const int DDL_TRIGGER_CREATE_PROCEDURE			= 4;
const int DDL_TRIGGER_ALTER_PROCEDURE			= 5;
const int DDL_TRIGGER_DROP_PROCEDURE			= 6;
const int DDL_TRIGGER_CREATE_FUNCTION			= 7;
const int DDL_TRIGGER_ALTER_FUNCTION			= 8;
const int DDL_TRIGGER_DROP_FUNCTION				= 9;
const int DDL_TRIGGER_CREATE_TRIGGER			= 10;
const int DDL_TRIGGER_ALTER_TRIGGER				= 11;
const int DDL_TRIGGER_DROP_TRIGGER				= 12;
// gap for TRIGGER_TYPE_MASK - 3 bits
const int DDL_TRIGGER_CREATE_EXCEPTION			= 16;
const int DDL_TRIGGER_ALTER_EXCEPTION			= 17;
const int DDL_TRIGGER_DROP_EXCEPTION			= 18;
const int DDL_TRIGGER_CREATE_VIEW				= 19;
const int DDL_TRIGGER_ALTER_VIEW				= 20;
const int DDL_TRIGGER_DROP_VIEW					= 21;
const int DDL_TRIGGER_CREATE_DOMAIN				= 22;
const int DDL_TRIGGER_ALTER_DOMAIN				= 23;
const int DDL_TRIGGER_DROP_DOMAIN				= 24;
const int DDL_TRIGGER_CREATE_ROLE				= 25;
const int DDL_TRIGGER_ALTER_ROLE				= 26;
const int DDL_TRIGGER_DROP_ROLE					= 27;
const int DDL_TRIGGER_CREATE_INDEX				= 28;
const int DDL_TRIGGER_ALTER_INDEX				= 29;
const int DDL_TRIGGER_DROP_INDEX				= 30;
const int DDL_TRIGGER_CREATE_SEQUENCE			= 31;
const int DDL_TRIGGER_ALTER_SEQUENCE			= 32;
const int DDL_TRIGGER_DROP_SEQUENCE				= 33;
const int DDL_TRIGGER_CREATE_USER				= 34;
const int DDL_TRIGGER_ALTER_USER				= 35;
const int DDL_TRIGGER_DROP_USER					= 36;
const int DDL_TRIGGER_CREATE_COLLATION			= 37;
const int DDL_TRIGGER_DROP_COLLATION			= 38;
const int DDL_TRIGGER_ALTER_CHARACTER_SET		= 39;
const int DDL_TRIGGER_CREATE_PACKAGE			= 40;
const int DDL_TRIGGER_ALTER_PACKAGE				= 41;
const int DDL_TRIGGER_DROP_PACKAGE				= 42;
const int DDL_TRIGGER_CREATE_PACKAGE_BODY		= 43;
const int DDL_TRIGGER_DROP_PACKAGE_BODY			= 44;
const int DDL_TRIGGER_CREATE_MAPPING			= 45;
const int DDL_TRIGGER_ALTER_MAPPING				= 46;
const int DDL_TRIGGER_DROP_MAPPING				= 47;
const int DDL_TRIGGER_CREATE_SCHEMA				= 48;
const int DDL_TRIGGER_ALTER_SCHEMA				= 49;
const int DDL_TRIGGER_DROP_SCHEMA				= 50;

// that's how database trigger action types are encoded
//    (TRIGGER_TYPE_DB | type)

// that's how DDL trigger action types are encoded
//    (TRIGGER_TYPE_DDL | DDL_TRIGGER_{AFTER | BEFORE} [ | DDL_TRIGGER_??? ...])

// switches for username and password used when an username and/or password
// is specified by the client application
#define USERNAME_SWITCH "USER"
#define PASSWORD_SWITCH "PASSWORD"

// The highest transaction number possible
const TraNumber MAX_TRA_NUMBER = 0x0000FFFFFFFFFFFF;	// ~2.8 * 10^14

// Number of streams, conjuncts, indices that will be statically allocated
// in various arrays. Larger numbers will have to be allocated dynamically
const unsigned OPT_STATIC_ITEMS = 16;
const unsigned OPT_STATIC_STREAMS = 64;

#define CURRENT_ENGINE "Engine14"
#define EMBEDDED_PROVIDERS "Providers=" CURRENT_ENGINE

// Features set for current version of engine provider
#define ENGINE_FEATURES {fb_feature_multi_statements, \
						 fb_feature_multi_transactions, \
						 fb_feature_session_reset, \
						 fb_feature_read_consistency, \
						 fb_feature_statement_timeout, \
						 fb_feature_statement_long_life}

const int WITH_GRANT_OPTION = 1;
const int WITH_ADMIN_OPTION = 2;

// Max length of the string returned by ERROR_TEXT context variable
const USHORT MAX_ERROR_MSG_LENGTH = 1024 * METADATA_BYTES_PER_CHAR; // 1024 UTF-8 characters

#endif // JRD_CONSTANTS_H
