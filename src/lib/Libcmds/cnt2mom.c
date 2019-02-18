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
 * @file	cnt2mom.c
 * @brief
 *	Connect to MOM, and if there is an error, print a more
 *	descriptive message.
 *
 * @par	Synopsis:
 *	int cnt2mom( char *momhost )
 *
 *	momhost	The name of the MOM host to connect to. A NULL or null
 *		string for the localhost.
 *
 * @par	Returns:
 *	The connection returned by pbs_connect().
 */

#include <pbs_config.h>   /* the master config generated by configure */

#include <stdio.h>
#include <netdb.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "libpbs.h"
#include "cmds.h"
#include "dis.h"

extern struct connect_handle connection[NCONNECTS];

/**
 * @brief
 *	establishes connection to host
 *
 * @param[in] momhost - The name of the MOM host to connect to.
 *
 * @return	int
 * @retval	connection slot		success
 * @retval	-1			error
 *
 */
static int
pbs_connect2mom(char *momhost)
{
	int conn, sd;
	struct addrinfo *aip, *pai;
	struct addrinfo hints;
	struct sockaddr_in *inp;

	/* initialize the thread context data, if not already initialized */
	if (pbs_client_thread_init_thread_context() != 0)
		return -1;

	if (pbs_loadconf(0) == 0)
		return -1;

	if (pbs_client_thread_lock_conntable() != 0)
		return -1;

	/*
	 * Find an available connection slot.
	 */
	for (conn=1; conn<NCONNECTS; conn++) {
		if (connection[conn].ch_inuse)
			continue;
		break;
	}
	if (conn >= NCONNECTS) {
		pbs_errno = PBSE_NOCONNECTS;
		goto err;
	}

	/*
	 * Ensure we have something valid to connect to.
	 */
	if (!momhost)
		momhost = "localhost";
	if (strlen(momhost) <= 0)
		momhost = "localhost";

	memset(&hints, 0, sizeof(struct addrinfo));
	/*
	 *	Why do we use AF_UNSPEC rather than AF_INET?  Some
	 *	implementations of getaddrinfo() will take an IPv6
	 *	address and map it to an IPv4 one if we ask for AF_INET
	 *	only.  We don't want that - we want only the addresses
	 *	that are genuinely, natively, IPv4 so we start with
	 *	AF_UNSPEC and filter ai_family below.
	 */
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	if (getaddrinfo(momhost, NULL, &hints, &pai) != 0) {
		pbs_errno = PBSE_BADHOST;
		goto err;
	}
	for (aip = pai; aip != NULL; aip = aip->ai_next) {
		/* skip non-IPv4 addresses */
		if (aip->ai_family == AF_INET) {
			inp = (struct sockaddr_in *) aip->ai_addr;
			break;
		}
	}
	if (aip == NULL) {
		pbs_errno = PBSE_BADHOST;
		goto err;
	} else
		inp->sin_port = htons(pbs_conf.mom_service_port);

	/*
	 * Establish a connection.
	 */
	sd = socket(AF_INET, SOCK_STREAM, 0);
	if (sd < 0) {
		pbs_errno = PBSE_PROTOCOL;
		goto err;
	}

	if (connect(sd, aip->ai_addr, aip->ai_addrlen) < 0) {
		close(sd);
		pbs_errno = errno;
		freeaddrinfo(pai);
		goto err;
	}
	freeaddrinfo(pai);

	/*
	 * Setup DIS support routines.
	 */
	DIS_tcp_setup(sd);
	pbs_tcp_timeout = PBS_DIS_TCP_TIMEOUT_VLONG;

	/*
	 * Register the connection slot as in use.
	 */
	connection[conn].ch_inuse = 1;
	connection[conn].ch_errno = 0;
	connection[conn].ch_socket= sd;
	connection[conn].ch_errtxt = NULL;

	/* setup connection level thread context */
	if (pbs_client_thread_init_connect_context(conn) != 0) {
		close(connection[conn].ch_socket);
		connection[conn].ch_inuse = 0;
		/* pbs_errno would be set by the pbs_connect_init_context routine */
		goto err;
	}

	if (pbs_client_thread_unlock_conntable() != 0)
		return -1;
	return conn;

err:
	(void)pbs_client_thread_unlock_conntable();
	return -1;
}

/**
 * @brief
 *	wrapper func for pbs_connect2mom.
 *
 * @param[in] momhost - The name of the MOM host to connect to.
 *
 * @return	int
 * @retval	connection	success
 * @retval	NULL		error
 *
 */
int
cnt2mom(char *momhost)
{
	int connect;

	connect = pbs_connect2mom(momhost);
	return (connect);
}
