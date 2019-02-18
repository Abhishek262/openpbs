/*
 * Copyright (C) 1994-2019 Altair Engineering, Inc.
 * For more information, contact Altair at www.altair.com.
 *
 * This file is part of the PBS Professional ("PBS Pro") software.
 *
 * Open Source License Information:
 *
 * PBS Pro is free software. You can redistribute it and/or modify it under the
 * terms of the GNU Affero General Public License as published by the Free
 * Software Foundation, either version 3 of the License, or (at your option) any
 * later version.
 *
 * PBS Pro is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.
 * See the GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Commercial License Information:
 *
 * For a copy of the commercial license terms and conditions,
 * go to: (http://www.pbspro.com/UserArea/agreement.html)
 * or contact the Altair Legal Department.
 *
 * Altair’s dual-license business model allows companies, individuals, and
 * organizations to create proprietary derivative works of PBS Pro and
 * distribute them - whether embedded or bundled with other software -
 * under a commercial license agreement.
 *
 * Use of Altair’s trademarks, including but not limited to "PBS™",
 * "PBS Professional®", and "PBS Pro™" and Altair’s logos is subject to Altair's
 * trademark licensing policies.
 *
 */
/**
 * @file	pbs_defschreply.c
 * @brief
 *		Deferred reply from the Scheduler to the Server
 */

#include <pbs_config.h>   /* the master config generated by configure */

#include <string.h>
#include <stdio.h>
#include "libpbs.h"
#include "dis.h"
#include "pbs_error.h"
#include "pbs_ecl.h"


/**
 * @brief
 *	- Deferred reply from the Scheduler to the Server
 *
 * @param[in] c - connection handler
 * @param[in] cmd - command
 * @param[in] id - job id
 * @param[in] err - error number
 * @param[in] txt - message
 * @param[in] extend - extend string for encoding req
 *
 * @return	int
 * @retval	0	success
 * @retval	!0	error
 *
 */
int
pbs_defschreply(int c, int cmd, char *id, int err, char *txt, char *extend)
{
	int	rc;
	struct batch_reply   *reply;
	int	sock;
	int	has_txt = 0;

	if ((id == NULL) || (*id == '\0'))
		return (pbs_errno = PBSE_IVALREQ);
	if ((txt != NULL) && (*txt != '\0'))
		has_txt = 1;

	sock = connection[c].ch_socket;

	/* initialize the thread context data, if not already initialized */
	if (pbs_client_thread_init_thread_context() != 0)
		return pbs_errno;

	/* lock pthread mutex here for this connection */
	/* blocking call, waits for mutex release */
	if (pbs_client_thread_lock_connection(c) != 0)
		return pbs_errno;

	/* setup DIS support routines for following DIS calls */

	DIS_tcp_setup(sock);

	/* encode request */

	if ((rc = encode_DIS_ReqHdr(sock, PBS_BATCH_DefSchReply,
		pbs_current_user)) ||
		(rc = diswui(sock, cmd)  != 0)                        ||
		(rc = diswst(sock, id)  != 0)                        ||
		(rc = diswui(sock, err)  != 0)) {
		connection[c].ch_errtxt = strdup(dis_emsg[rc]);
		if (connection[c].ch_errtxt == NULL) {
			pbs_errno = PBSE_SYSTEM;
		} else {
			pbs_errno = PBSE_PROTOCOL;
		}
		(void)pbs_client_thread_unlock_connection(c);
		return pbs_errno;
	}
	rc = diswsi(sock, has_txt);
	if ((has_txt == 1) && (rc == 0)) {
		rc = diswst(sock, txt);
	}
	if (rc == 0)
		rc = encode_DIS_ReqExtend(sock, extend);
	if (rc) {
		connection[c].ch_errtxt = strdup(dis_emsg[rc]);
		if (connection[c].ch_errtxt == NULL) {
			pbs_errno = PBSE_SYSTEM;
		} else {
			pbs_errno = PBSE_PROTOCOL;
		}
		(void)pbs_client_thread_unlock_connection(c);
		return pbs_errno;
	}

	if (DIS_tcp_wflush(sock)) {
		pbs_errno = PBSE_PROTOCOL;
		(void)pbs_client_thread_unlock_connection(c);
		return pbs_errno;
	}

	/* get reply */

	reply = PBSD_rdrpy(c);
	rc = connection[c].ch_errno;

	PBSD_FreeReply(reply);

	/* unlock the thread lock and update the thread context data */
	if (pbs_client_thread_unlock_connection(c) != 0)
		return pbs_errno;

	return rc;
}
