/*
 *	PROGRAM:	JRD Remote server  
 *	MODULE:		serve_proto.h
 *	DESCRIPTION:	Prototype Header file for server.c
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
 */

#ifndef REMOTE_SERVE_PROTO_H
#define REMOTE_SERVE_PROTO_H

#ifdef __cplusplus
extern "C" {
#endif


#ifdef WINDOWS_ROUTER
void SRVR_WinMain(struct port *, USHORT, HINSTANCE, HINSTANCE, int);
#else	/* WINDOWS_ROUTER */
void SRVR_main(struct port *, USHORT);
#endif	/* WINDOWS_ROUTER */

#ifdef NO_PORT
#define PORT void*
#endif

void SRVR_multi_thread(struct port *, USHORT);
bool process_packet(PORT, PACKET *, PACKET *, PORT *);
void set_server(PORT, USHORT);
void THREAD_ROUTINE process_connection_thread(PORT);

#ifdef __cplusplus
} // extern "C"
#endif


#endif	// REMOTE_SERVE_PROTO_H
