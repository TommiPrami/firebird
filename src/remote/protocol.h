/*
 *	PROGRAM:	JRD Remote Interface/Server
 *	MODULE:		protocol.h
 *	DESCRIPTION:	Protocol Definition
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
 * 2002.02.15 Sean Leyne - This module needs to be cleanedup to remove obsolete ports/defines:
 *                            - "EPSON", "XENIX" +++
 *
 * 2002.10.27 Sean Leyne - Code Cleanup, removed obsolete "Ultrix/MIPS" port
 *
 * 2002.10.28 Sean Leyne - Code cleanup, removed obsolete "MPEXL" port
 * 2002.10.28 Sean Leyne - Code cleanup, removed obsolete "DecOSF" port
 *
 */

#ifndef REMOTE_PROTOCOL_H
#define REMOTE_PROTOCOL_H

// forward
namespace Firebird {
	class DynamicStatusVector;
}

class RemBlobBuffer;	// see remote.h


// dimitr: ask for asymmetric protocols only.
// Comment it out to return back to FB 1.0 behaviour.
#define ASYMMETRIC_PROTOCOLS_ONLY

// The protocol is defined blocks, rather than messages, to
// separate the protocol from the transport layer.

// p_cnct_cversion
inline constexpr USHORT CONNECT_VERSION3 = 3;

// Protocol 10 includes support for warnings and removes the requirement for
// encoding and decoding status codes

inline constexpr USHORT PROTOCOL_VERSION10	= 10;

// Since protocol 11 we must be separated from Borland Interbase.
// Therefore always set highmost bit in protocol version to 1.
// For unsigned protocol version this does not break version's compare.

inline constexpr USHORT FB_PROTOCOL_FLAG = 0x8000;
inline constexpr USHORT FB_PROTOCOL_MASK = static_cast<USHORT>(~FB_PROTOCOL_FLAG);

// Protocol 11 has support for user authentication related
// operations (op_update_account_info, op_authenticate_user and
// op_trusted_auth). When specific operation is not supported,
// we say "sorry".

inline constexpr USHORT PROTOCOL_VERSION11	= (FB_PROTOCOL_FLAG | 11);

// Protocol 12 has support for asynchronous call op_cancel.
// Currently implemented asynchronously only for TCP/IP.

inline constexpr USHORT PROTOCOL_VERSION12	= (FB_PROTOCOL_FLAG | 12);

// Protocol 13 has support for authentication plugins (op_cont_auth).
// It also transfers SQL messages in the packed (null aware) format.

inline constexpr USHORT PROTOCOL_VERSION13	= (FB_PROTOCOL_FLAG | 13);

// Protocol 14:
//	- fixes a bug in database crypt key callback

inline constexpr USHORT PROTOCOL_VERSION14	= (FB_PROTOCOL_FLAG | 14);

// Protocol 15:
//	- supports crypt key callback at connect phase

inline constexpr USHORT PROTOCOL_VERSION15 = (FB_PROTOCOL_FLAG | 15);

// Protocol 16:
//	- supports statement timeouts

inline constexpr USHORT PROTOCOL_VERSION16 = (FB_PROTOCOL_FLAG | 16);
inline constexpr USHORT PROTOCOL_STMT_TOUT = PROTOCOL_VERSION16;

// Protocol 17:
//	- supports op_batch_sync, op_info_batch

inline constexpr USHORT PROTOCOL_VERSION17 = (FB_PROTOCOL_FLAG | 17);

// Protocol 18:
//	- supports op_fetch_scroll

inline constexpr USHORT PROTOCOL_VERSION18 = (FB_PROTOCOL_FLAG | 18);
inline constexpr USHORT PROTOCOL_FETCH_SCROLL = PROTOCOL_VERSION18;

// Protocol 19:
//	- supports op_inline_blob

inline constexpr USHORT PROTOCOL_VERSION19 = (FB_PROTOCOL_FLAG | 19);
inline constexpr USHORT PROTOCOL_INLINE_BLOB = PROTOCOL_VERSION19;

// Protocol 20:
//	- supports passing flags to IStatement::prepare

inline constexpr USHORT PROTOCOL_VERSION20 = (FB_PROTOCOL_FLAG | 20);
inline constexpr USHORT PROTOCOL_PREPARE_FLAG = PROTOCOL_VERSION20;

// Architecture types

enum P_ARCH
{
	arch_generic		= 1,	// Generic -- always use canonical forms
	arch_sun			= 3,
	arch_sun4			= 8,
	arch_sunx86			= 9,
	arch_hpux			= 10,
	arch_rt				= 14,
	arch_intel_32		= 29,	// generic Intel chip w/32-bit compilation
	arch_linux			= 36,
	arch_freebsd		= 37,
	arch_netbsd			= 38,
	arch_darwin_ppc		= 39,
	arch_winnt_64		= 40,
	arch_darwin_x64		= 41,
	arch_darwin_ppc64	= 42,
	arch_arm            = 43,
	arch_winnt_arm64	= 44,
	arch_max			= 45	// Keep this at the end
};

// Protocol Types
// p_acpt_type
//inline constexpr USHORT ptype_page		= 1;	// Page server protocol
//inline constexpr USHORT ptype_rpc			= 2;	// Simple remote procedure call
inline constexpr USHORT ptype_batch_send	= 3;	// Batch sends, no asynchrony
inline constexpr USHORT ptype_out_of_band	= 4;	// Batch sends w/ out of band notification
inline constexpr USHORT ptype_lazy_send		= 5;	// Deferred packets delivery
inline constexpr USHORT ptype_MASK			= 0xFF;	// Mask - up to 255 types of protocol
//
// upper byte is used for protocol flags
inline constexpr USHORT pflag_compress		= 0x100;	// Turn on compression if possible
inline constexpr USHORT pflag_win_sspi_nego	= 0x200;	// Win_SSPI supports Negotiate security package

// Generic object id

typedef USHORT OBJCT;
inline constexpr int MAX_OBJCT_HANDLES	= 65000;
inline constexpr int INVALID_OBJECT = MAX_USHORT;

// Statement flags

//inline constexpr USHORT STMT_BLOB			= 1;
inline constexpr USHORT STMT_NO_BATCH		= 2;
inline constexpr USHORT STMT_DEFER_EXECUTE	= 4;

enum P_FETCH
{
	fetch_next		= 0,
	fetch_prior		= 1,
	fetch_first		= 2,
	fetch_last		= 3,
	fetch_absolute	= 4,
	fetch_relative	= 5
};

inline constexpr P_FETCH fetch_execute = fetch_next;

// Operation (packet) types

enum P_OP
{
	op_void				= 0,	// Packet has been voided
	op_connect			= 1,	// Connect to remote server
	op_exit				= 2,	// Remote end has exitted
	op_accept			= 3,	// Server accepts connection
	op_reject			= 4,	// Server rejects connection
	//op_protocol		= 5,	// Protocol selection
	op_disconnect		= 6,	// Connect is going away
	//op_credit			= 7,	// Grant (buffer) credits
	//op_continuation	= 8,	// Continuation packet
	op_response			= 9,	// Generic response block

	// Page server operations

	//op_open_file		= 10,	// Open file for page service
	//op_create_file	= 11,	// Create file for page service
	//op_close_file		= 12,	// Close file for page service
	//op_read_page		= 13,	// optionally lock and read page
	//op_write_page		= 14,	// write page and optionally release lock
	//op_lock			= 15,	// seize lock
	//op_convert_lock	= 16,	// convert existing lock
	//op_release_lock	= 17,	// release existing lock
	//op_blocking		= 18,	// blocking lock message

	// Full context server operations

	op_attach			= 19,	// Attach database
	op_create			= 20,	// Create database
	op_detach			= 21,	// Detach database
	op_compile			= 22,	// Request based operations
	op_start			= 23,
	op_start_and_send	= 24,
	op_send				= 25,
	op_receive			= 26,
	op_unwind			= 27,	// apparently unused, see protocol.cpp's case op_unwind
	op_release			= 28,

	op_transaction		= 29,	// Transaction operations
	op_commit			= 30,
	op_rollback			= 31,
	op_prepare			= 32,
	op_reconnect		= 33,

	op_create_blob		= 34,	// Blob operations
	op_open_blob		= 35,
	op_get_segment		= 36,
	op_put_segment		= 37,
	op_cancel_blob		= 38,
	op_close_blob		= 39,

	op_info_database	= 40,	// Information services
	op_info_request		= 41,
	op_info_transaction	= 42,
	op_info_blob		= 43,

	op_batch_segments	= 44,	// Put a bunch of blob segments

	//op_mgr_set_affinity	= 45,	// Establish server affinity
	//op_mgr_clear_affinity	= 46,	// Break server affinity
	//op_mgr_report			= 47,	// Report on server

	op_que_events		= 48,	// Que event notification request
	op_cancel_events	= 49,	// Cancel event notification request
	op_commit_retaining	= 50,	// Commit retaining (what else)
	op_prepare2			= 51,	// Message form of prepare
	op_event			= 52,	// Completed event request (asynchronous)
	op_connect_request	= 53,	// Request to establish connection
	op_aux_connect		= 54,	// Establish auxiliary connection
	op_ddl				= 55,	// DDL call
	op_open_blob2		= 56,
	op_create_blob2		= 57,
	op_get_slice		= 58,
	op_put_slice		= 59,
	op_slice			= 60,	// Successful response to op_get_slice
	op_seek_blob		= 61,	// Blob seek operation

	// DSQL operations

	op_allocate_statement 	= 62,	// allocate a statement handle
	op_execute				= 63,	// execute a prepared statement
	op_exec_immediate		= 64,	// execute a statement
	op_fetch				= 65,	// fetch a record
	op_fetch_response		= 66,	// response for record fetch
	op_free_statement		= 67,	// free a statement
	op_prepare_statement 	= 68,	// prepare a statement
	op_set_cursor			= 69,	// set a cursor name
	op_info_sql				= 70,

	op_dummy				= 71,	// dummy packet to detect loss of client

	op_response_piggyback 	= 72,	// response block for piggybacked messages
	op_start_and_receive 	= 73,
	op_start_send_and_receive 	= 74,

	op_exec_immediate2		= 75,	// execute an immediate statement with msgs
	op_execute2				= 76,	// execute a statement with msgs
	op_insert				= 77,
	op_sql_response			= 78,	// response from execute, exec immed, insert

	op_transact				= 79,
	op_transact_response 	= 80,
	op_drop_database		= 81,

	op_service_attach		= 82,
	op_service_detach		= 83,
	op_service_info			= 84,
	op_service_start		= 85,

	op_rollback_retaining	= 86,

	// Two following opcode are used in vulcan.
	// No plans to implement them completely for a while, but to
	// support protocol 11, where they are used, have them here.
	op_update_account_info	= 87,
	op_authenticate_user	= 88,

	op_partial				= 89,	// packet is not complete - delay processing
	op_trusted_auth			= 90,

	op_cancel				= 91,

	op_cont_auth			= 92,

	op_ping					= 93,

	op_accept_data			= 94,	// Server accepts connection and returns some data to client

	op_abort_aux_connection	= 95,	// Async operation - stop waiting for async connection to arrive

	op_crypt				= 96,
	op_crypt_key_callback	= 97,
	op_cond_accept			= 98,	// Server accepts connection, returns some data to client
									// and asks client to continue authentication before attach call

	op_batch_create			= 99,
	op_batch_msg			= 100,
	op_batch_exec			= 101,
	op_batch_rls			= 102,
	op_batch_cs				= 103,
	op_batch_regblob		= 104,
	op_batch_blob_stream	= 105,
	op_batch_set_bpb		= 106,

	op_repl_data			= 107,
	op_repl_req				= 108,

	op_batch_cancel			= 109,
	op_batch_sync			= 110,
	op_info_batch			= 111,

	op_fetch_scroll			= 112,
	op_info_cursor			= 113,

	op_inline_blob			= 114,

	op_max
};


// Count String Structure

struct RemoteXdr;

typedef struct cstring
{
	ULONG	cstr_length;
	ULONG	cstr_allocated;
	UCHAR*	cstr_address;

	void	free(RemoteXdr* xdrs = nullptr);
} CSTRING;

// CVC: Only used in p_blob, p_sgmt & p_ddl, to validate constness.
// We want to ensure our original bpb is not overwritten.
// In turn, p_blob is used only to create and open a blob, so it's correct
// to demand that those functions do not change the bpb.
// We want to ensure our incoming segment to be stored isn't overwritten,
// in the case of send/batch commands.
typedef struct cstring_const
{
	ULONG	cstr_length;
	ULONG	cstr_allocated;
	const UCHAR*	cstr_address;
} CSTRING_CONST;


#ifdef DEBUG_XDR_MEMORY

// Debug xdr memory allocations

inline constexpr USHORT P_MALLOC_SIZE	= 64;	// Xdr memory allocations per packet

typedef struct p_malloc
{
	P_OP	p_operation;	// Operation/packet type
	ULONG	p_allocated;	// Memory length
	UCHAR*	p_address;		// Memory address
} P_MALLOC;

#endif	// DEBUG_XDR_MEMORY


// Connect Block (Client to server)

// Servers before FB6 (PROTOCOL_VERSION20) uses only first 10 elements of p_cnct_versions
inline constexpr size_t MAX_CNCT_VERSIONS = 11;

typedef struct p_cnct
{
	USHORT	p_cnct_operation;			// unused
	USHORT	p_cnct_cversion;			// Version of connect protocol
	P_ARCH	p_cnct_client;				// Architecture of client
	CSTRING_CONST	p_cnct_file;		// File name
	USHORT	p_cnct_count;				// Protocol versions understood
	CSTRING_CONST	p_cnct_user_id;		// User identification stuff
	struct	p_cnct_repeat
	{
		USHORT	p_cnct_version;			// Protocol version number
		P_ARCH	p_cnct_architecture;	// Architecture of client
		USHORT	p_cnct_min_type;		// Minimum type (unused)
		USHORT	p_cnct_max_type;		// Maximum type
		USHORT	p_cnct_weight;			// Preference weight
	}	p_cnct_versions[MAX_CNCT_VERSIONS];
} P_CNCT;

#ifdef ASYMMETRIC_PROTOCOLS_ONLY
#define REMOTE_PROTOCOL(version, type, weight) \
	{version, arch_generic, 0, type, weight * 2}
#else
#define REMOTE_PROTOCOL(version, type, weight) \
	{version, arch_generic, 0, type, weight * 2}, \
	{version, ARCHITECTURE, 0, type, weight * 2 + 1}
#endif

/* User identification data, if any, is of form:

    <type> <length> <data>

where

     type	is a byte code
     length	is an unsigned byte containing length of data
     data	is 'type' specific

*/

inline constexpr UCHAR CNCT_user		= 1;			// User name
inline constexpr UCHAR CNCT_passwd		= 2;
//inline constexpr UCHAR CNCT_ppo		= 3;			// Apollo person, project, organization. OBSOLETE.
inline constexpr UCHAR CNCT_host		= 4;
inline constexpr UCHAR CNCT_group		= 5;			// Effective Unix group id
inline constexpr UCHAR CNCT_user_verification	= 6;	// Attach/create using this connection
					 							// will use user verification
inline constexpr UCHAR CNCT_specific_data		= 7;	// Some data, needed for user verification on server
inline constexpr UCHAR CNCT_plugin_name			= 8;	// Name of plugin, which generated that data
inline constexpr UCHAR CNCT_login				= 9;	// Same data as isc_dpb_user_name
inline constexpr UCHAR CNCT_plugin_list			= 10;	// List of plugins, available on client
inline constexpr UCHAR CNCT_client_crypt		= 11;	// Client encryption level (DISABLED/ENABLED/REQUIRED)

// Accept Block (Server response to connect block)

typedef struct p_acpt
{
	USHORT	p_acpt_version;			// Protocol version number
	P_ARCH	p_acpt_architecture;	// Architecture for protocol
	USHORT	p_acpt_type;			// Minimum type
} P_ACPT;

// Accept Block with Data (Server response to connect block, start with P.13)

struct p_acpd : public p_acpt
{
	CSTRING	p_acpt_data;			// Returned auth data
	CSTRING	p_acpt_plugin;			// Plugin to continue with
	USHORT	p_acpt_authenticated;	// Auth complete in single step (few! strange...)
	CSTRING p_acpt_keys;			// Keys known to the server
};
typedef p_acpd P_ACPD;

// Generic Response block

typedef struct p_resp
{
	OBJCT		p_resp_object;		// Object id
	SQUAD		p_resp_blob_id;		// Blob id
	CSTRING		p_resp_data;		// Data
	Firebird::DynamicStatusVector* p_resp_status_vector;
} P_RESP;

#define p_resp_partner	p_resp_blob_id.bid_number

// Attach and create database

typedef struct p_atch
{
	OBJCT	p_atch_database;		// Database object id
	CSTRING_CONST	p_atch_file;	// File name
	CSTRING_CONST	p_atch_dpb;		// Database parameter block
} P_ATCH;

// Compile request

typedef struct p_cmpl
{
	OBJCT	p_cmpl_database;		// Database object id
	CSTRING_CONST	p_cmpl_blr;		// Request blr
} P_CMPL;

// Start Transaction

typedef struct p_sttr
{
	OBJCT	p_sttr_database;		// Database object id
	CSTRING_CONST	p_sttr_tpb;		// Transaction parameter block
} P_STTR;

// Generic release block

typedef struct p_rlse
{
	OBJCT	p_rlse_object;			// Object to be released
} P_RLSE;

// Data block (start, start and send, send, receive)

typedef struct p_data
{
    OBJCT	p_data_request;			// Request object id
    USHORT	p_data_incarnation;		// Incarnation of request
    OBJCT	p_data_transaction;		// Transaction object id
    USHORT	p_data_message_number;	// Message number in request
    USHORT	p_data_messages;		// Number of messages
} P_DATA;

// Execute stored procedure block

typedef struct p_trrq
{
    OBJCT	p_trrq_database;		// Database object id
    OBJCT	p_trrq_transaction;		// Transaction object id
    CSTRING	p_trrq_blr;				// Message blr
    USHORT	p_trrq_messages;		// Number of messages
} P_TRRQ;

// Blob (create/open) and segment blocks

typedef struct p_blob
{
    OBJCT	p_blob_transaction;		// Transaction
    SQUAD	p_blob_id;				// Blob id for open
    CSTRING_CONST	p_blob_bpb;		// Blob parameter block
} P_BLOB;

typedef struct p_sgmt
{
    OBJCT	p_sgmt_blob;			// Blob handle id
    USHORT	p_sgmt_length;			// Length of segment
    CSTRING_CONST	p_sgmt_segment;	// Data segment
} P_SGMT;

typedef struct p_seek
{
    OBJCT	p_seek_blob;		// Blob handle id
    SSHORT	p_seek_mode;		// mode of seek
    SLONG	p_seek_offset;		// Offset of seek
} P_SEEK;

// Information request blocks

typedef struct p_info
{
    OBJCT	p_info_object;				// Object of information
    USHORT	p_info_incarnation;			// Incarnation of object
    CSTRING_CONST	p_info_items;		// Information
    CSTRING_CONST	p_info_recv_items;	// Receive information
    ULONG	p_info_buffer_length;		// Target buffer length
} P_INFO;

// Event request block

typedef struct p_event
{
    OBJCT	p_event_database;			// Database object id
    CSTRING_CONST	p_event_items;		// Event description block
    FPTR_EVENT_CALLBACK p_event_ast;	// Address of ast routine
    SLONG	p_event_arg;				// Argument to ast routine
    SLONG	p_event_rid;				// Client side id of remote event
} P_EVENT;

// Prepare block

typedef struct p_prep
{
    OBJCT	p_prep_transaction;
    CSTRING_CONST	p_prep_data;
} P_PREP;

// Connect request block

typedef struct p_req
{
    USHORT	p_req_type;			// Connection type
    OBJCT	p_req_object;		// Related object
    ULONG	p_req_partner;		// Partner identification
} P_REQ;

// p_req_type
inline constexpr USHORT P_REQ_async	= 1;	// Auxiliary asynchronous port

// DDL request

typedef struct p_ddl
{
     OBJCT	p_ddl_database;		// Database object id
     OBJCT	p_ddl_transaction;	// Transaction
     CSTRING_CONST	p_ddl_blr;	// Request blr
} P_DDL;

// Slice Operation

typedef struct p_slc
{
    OBJCT	p_slc_transaction;	// Transaction
    SQUAD	p_slc_id;			// Slice id
    CSTRING	p_slc_sdl;			// Slice description language
    CSTRING	p_slc_parameters;	// Slice parameters
    lstring	p_slc_slice;		// Slice proper
    ULONG	p_slc_length;		// Number of elements
} P_SLC;

// Response to get_slice

typedef struct p_slr
{
    lstring	p_slr_slice;		// Slice proper
    ULONG	p_slr_length;		// Total length of slice
    UCHAR* p_slr_sdl;			// *** not transferred ***
    USHORT	p_slr_sdl_length;	// *** not transferred ***
} P_SLR;

// DSQL structure definitions

typedef struct p_sqlst
{
    OBJCT	p_sqlst_transaction;		// transaction object
    OBJCT	p_sqlst_statement;			// statement object
    USHORT	p_sqlst_SQL_dialect;		// the SQL dialect
    CSTRING_CONST	p_sqlst_SQL_str;	// statement to be prepared
    ULONG	p_sqlst_buffer_length;		// Target buffer length
    CSTRING_CONST	p_sqlst_items;		// Information
    // This should be CSTRING_CONST
    CSTRING	p_sqlst_blr;				// blr describing message
    USHORT	p_sqlst_message_number;
    USHORT	p_sqlst_messages;			// Number of messages
    CSTRING	p_sqlst_out_blr;			// blr describing output message
    USHORT	p_sqlst_out_message_number;
	USHORT	p_sqlst_flags;				// prepare flags
	ULONG	p_sqlst_inline_blob_size;	// maximum size of inlined blob
} P_SQLST;

typedef struct p_sqldata
{
    OBJCT	p_sqldata_statement;		// statement object
    OBJCT	p_sqldata_transaction;		// transaction object
    // This should be CSTRING_CONST, but fetch() has strange behavior.
    CSTRING	p_sqldata_blr;				// blr describing message
    USHORT	p_sqldata_message_number;
    USHORT	p_sqldata_messages;			// Number of messages
    CSTRING	p_sqldata_out_blr;			// blr describing output message
    USHORT	p_sqldata_out_message_number;
    ULONG	p_sqldata_status;			// final eof status
	ULONG	p_sqldata_timeout;			// statement timeout
	ULONG	p_sqldata_cursor_flags;		// cursor flags
	P_FETCH	p_sqldata_fetch_op;			// Fetch operation
	SLONG	p_sqldata_fetch_pos;		// Fetch position
	ULONG	p_sqldata_inline_blob_size;	// maximum size of inlined blob
} P_SQLDATA;

typedef struct p_sqlfree
{
    OBJCT	p_sqlfree_statement;	// statement object
    USHORT	p_sqlfree_option;		// option
} P_SQLFREE;

typedef struct p_sqlcur
{
    OBJCT	p_sqlcur_statement;				// statement object
    CSTRING_CONST	p_sqlcur_cursor_name;	// cursor name
    USHORT	p_sqlcur_type;					// type of cursor
} P_SQLCUR;

typedef struct p_trau
{
	CSTRING	p_trau_data;					// Context
} P_TRAU;

typedef struct p_auth_continue
{
	CSTRING	p_data;							// Specific data
	CSTRING p_name;							// Plugin name
	CSTRING p_list;							// Plugin list
	CSTRING p_keys;							// Keys available on server
} P_AUTH_CONT;

struct p_update_account
{
    OBJCT			p_account_database;		// Database object id
    CSTRING_CONST	p_account_apb;			// Account parameter block (apb)
};

struct p_authenticate
{
    OBJCT			p_auth_database;		// Database object id
    CSTRING_CONST	p_auth_dpb;				// Database parameter block w/ user credentials
	CSTRING			p_auth_items;			// Information
	CSTRING			p_auth_recv_items;		// Receive information
	USHORT			p_auth_buffer_length;	// Target buffer length (transmitted but not used)
};

typedef struct p_cancel_op
{
    USHORT	p_co_kind;			// Kind of cancelation
} P_CANCEL_OP;

typedef struct p_crypt
{
	CSTRING p_plugin;						// Crypt plugin name
	CSTRING p_key;							// Key name / keys available on server
} P_CRYPT;

typedef struct p_crypt_callback
{
	CSTRING	p_cc_data;						// User's data
	USHORT p_cc_reply;
} P_CRYPT_CALLBACK;


// Batch definitions

typedef struct p_batch_create
{
    OBJCT			p_batch_statement;	// statement object
    CSTRING_CONST	p_batch_blr;		// blr describing input messages
    ULONG			p_batch_msglen;		// explicit message length
    CSTRING_CONST   p_batch_pb;			// parameters block
} P_BATCH_CREATE;

typedef struct p_batch_msg
{
	OBJCT	p_batch_statement;			// statement object
	ULONG	p_batch_messages;			// number of messages
	CSTRING p_batch_data;
} P_BATCH_MSG;

typedef struct p_batch_exec
{
	OBJCT	p_batch_statement;			// statement object
	OBJCT   p_batch_transaction;		// transaction object
} P_BATCH_EXEC;

typedef struct p_batch_cs				// completion state
{
    OBJCT	p_batch_statement;			// statement object
	ULONG	p_batch_reccount;			// total records
	ULONG	p_batch_updates;			// update counters
	ULONG	p_batch_vectors;			// recnum + status vector pairs
	ULONG	p_batch_errors;				// error's recnums
} P_BATCH_CS;

typedef struct p_batch_blob
{
	OBJCT			p_batch_statement;	// statement object
	CSTRING			p_batch_blob_data;	// data
} P_BATCH_BLOB;

typedef struct p_batch_regblob
{
	OBJCT			p_batch_statement;	// statement object
	SQUAD			p_batch_exist_id;	// id of blob to register
	SQUAD			p_batch_blob_id;	// blob id
} P_BATCH_REGBLOB;

typedef struct p_batch_setbpb
{
	OBJCT			p_batch_statement;	// statement object
	CSTRING_CONST	p_batch_blob_bpb;	// BPB
} P_BATCH_SETBPB;


// Replication support

typedef struct p_replicate
{
     OBJCT			p_repl_database;	// database object id
     CSTRING_CONST	p_repl_data;		// replication data
} P_REPLICATE;

typedef struct p_inline_blob
{
	OBJCT			p_tran_id;			// transaction id
	SQUAD			p_blob_id;			// blob id
	CSTRING			p_blob_info;		// blob info
	RemBlobBuffer*	p_blob_data;		// blob data
} P_INLINE_BLOB;


// Generalize packet (sic!)

typedef struct packet
{
#ifdef DEBUG_XDR_MEMORY
	// When XDR memory debugging is enabled, p_malloc must be
	// the first subpacket and be followed by p_operation (see
	// server.c/zap_packet())

    P_MALLOC	p_malloc [P_MALLOC_SIZE]; // Debug xdr memory allocations
#endif
    P_OP	p_operation;		// Operation/packet type
    P_CNCT	p_cnct;				// Connect block
    P_ACPT	p_acpt;				// Accept connection
    P_ACPD	p_acpd;				// Accept connection with data
    P_RESP	p_resp;				// Generic response to a call
    P_ATCH	p_atch;				// Attach or create database
    P_RLSE	p_rlse;				// Release object
    P_DATA	p_data;				// Data packet
    P_CMPL	p_cmpl;				// Compile request
    P_STTR	p_sttr;				// Start transactions
    P_BLOB	p_blob;				// Create/Open blob
    P_SGMT	p_sgmt;				// Put_segment
    P_INFO	p_info;				// Information
    P_EVENT	p_event;			// Que event
    P_PREP	p_prep;				// New improved prepare
    P_REQ	p_req;				// Connection request
    P_DDL	p_ddl;				// Data definition call
    P_SLC	p_slc;				// Slice operator
    P_SLR	p_slr;				// Slice response
    P_SEEK	p_seek;				// Blob seek
    P_SQLST	p_sqlst;			// DSQL Prepare & Execute immediate
    P_SQLDATA	p_sqldata;		// DSQL Open Cursor, Execute, Fetch
    P_SQLCUR	p_sqlcur;		// DSQL Set cursor name
    P_SQLFREE	p_sqlfree;		// DSQL Free statement
    P_TRRQ	p_trrq;				// Transact request packet
	P_TRAU	p_trau;				// Trusted authentication
	p_update_account p_account_update;
	p_authenticate p_authenticate_user;
	P_CANCEL_OP p_cancel_op;	// Cancel operation
	P_AUTH_CONT p_auth_cont;	// Request more auth data
	P_CRYPT p_crypt;			// Start wire crypt
	P_CRYPT_CALLBACK p_cc;		// Database crypt callback
	P_BATCH_CREATE p_batch_create; // Create batch interface
	P_BATCH_MSG p_batch_msg;	// Add messages to batch
	P_BATCH_EXEC p_batch_exec;	// Run batch
	P_BATCH_CS p_batch_cs;		// Batch completion state
	P_BATCH_BLOB p_batch_blob;	// BLOB stream portion in batch
	P_BATCH_REGBLOB p_batch_regblob;	// Register already existing BLOB in batch
	P_BATCH_SETBPB p_batch_setbpb;		// Set default BPB for batch
	P_REPLICATE p_replicate;	// replicate
	P_INLINE_BLOB p_inline_blob;		// inline blob

public:
	packet() noexcept
	{
		memset(this, 0, sizeof(*this));
	}
} PACKET;

#endif // REMOTE_PROTOCOL_H
