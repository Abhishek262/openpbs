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

#include <pbs_config.h>   /* the master config generated by configure */

#include "cmds.h"

/**
 * @file	parse_stage.c
 */

#define ISNAMECHAR(x)  (((isprint(x)) || (isspace(x))) && ((x) != '@') )
#define ISNAMECHAR2(x) ((isprint(x)) && (! isspace(x))  && ((x) != '@') && ((x) != ':') )

/**
 * @brief
 *	parses the staging file name
 *      syntax:	locat_file@hostname:remote_file
 *		on Windows if remote_file is UNC path then
 *		hostname is optional so syntax can be
 *		local_file@remote_unc_file
 *      Note: The arguments local_name, host_name, remote_name are mandatory and
 *	must be allocated with required memory by the caller.
 *
 * @param[in]       pair        - a staged file name pair
 * @param[in/out]   local_name  - local file name
 * @param[in/out]   host_name   - remote host
 * @param[in/out]   remote_name - remote file namea
 *
 * @return int
 * @retval 0    parsing was successful
 * @retval 1	error in parsing
 */
int
parse_stage_name(char *pair, char *local_name, char *host_name, char *remote_name)
{
	char *c = NULL;
	int l_pos = 0;
	int h_pos = 0;
	int r_pos = 0;

	/* Begin the parse */
	c = pair;
	while (isspace(*c)) c++;

	/* Looking for something before the @ sign */
	while (*c != '\0') {
		if (ISNAMECHAR(*c)) {		/* allow whitespace and stop on '@' */
			if (l_pos >= MAXPATHLEN) return 1;
			local_name[l_pos++]=*c;
		} else
			break;
		c++;
	}
	if (l_pos == 0) return 1;

#ifdef WIN32
	if ((*c == '@') && (c+1 != NULL) && (IS_UNCPATH(c+1))) {
		c++;
		/*
		 * remote_name is UNC path without host part
		 * so skip parsing of host_name and parse
		 * remote_name
		 */
		while (*c != '\0') {
			if (ISNAMECHAR(*c)) {	/* allow whitespace */
				if (r_pos >= MAXPATHLEN) return 1;
				remote_name[r_pos++]=*c;
			} else
				break;
			c++;
		}
	}
#endif
	/* Looking for something between the @ and the : */
	if (*c == '@') {
		c++;
		while (*c != '\0') {
			if (ISNAMECHAR2(*c)) {	/* no whitespace allowed in host */
				if (h_pos >= PBS_MAXSERVERNAME) return 1;
				host_name[h_pos++]=*c;
			} else
				break;
			c++;
		}
		if (h_pos == 0) return 1;
	}

#ifdef WIN32
	/*
	 * h_pos may be 1 if non-UNC path is given
	 * without host part which is not allowed
	 * so return parsing error
	 * example: -Wstagein=C:\testdir@D:\testdir1
	 */
	if (h_pos == 1)
		return 1;
#endif

	/* Looking for something after the : */
	if (*c == ':') {
		c++;
		while (*c != '\0') {
			if (ISNAMECHAR(*c)) {	/* allow whitespace */
				if (r_pos >= MAXPATHLEN) return 1;
				remote_name[r_pos++]=*c;
			} else
				break;
			c++;
		}
	}
	if (r_pos == 0) return 1;

	if (*c != '\0') return 1;


	/* set null chars at end of string */
	local_name[l_pos] = '\0';
	remote_name[r_pos] = '\0';
	host_name[h_pos] = '\0';

	return (0);
}



/**
 * @brief
 * 	parse_stage_list
 *
 * syntax:
 *	local_file@hostname:remote_file [,...]
 *
 * @param[in]	list	List of staged file name pairs.
 *
 * @return	int
 * @retval	0	success
 * @retval	1	error
 *
 */

int
parse_stage_list(char *list)
{
	char *b = NULL;
	char *c = NULL;
	char *s = NULL;
	char *l = NULL;
	int comma = 0;
	char local[MAXPATHLEN+1] = {'\0'};
	char host[PBS_MAXSERVERNAME] = {'\0'};
	char remote[MAXPATHLEN+1] = {'\0'};

	if (strlen(list) == 0) return (1);

	if ((l = (char *)malloc(strlen(list)+1)) == NULL) {
		fprintf(stderr, "Out of memory.\n");
		return 1;
	}
	memset(l, 0, strlen(list)+1);
	strcpy(l, list);
	c = l;
	while (*c != '\0') {
		/* Drop leading white space */
		while (isspace((int) * c)) c++;

		/* Find the next comma */
		s = c;
		while (*c != '\0') {
			if (*c == ',' && (s != c) && *(c - 1) != '\\')
				break;
			c++;
		}

		/* Drop any trailing blanks */
		comma = (*c == ',');
		*c = '\0';
		b = c - 1;
		while (isspace((int) * b)) *b-- = '\0';

		/* Parse the individual list item */
		if (parse_stage_name(s, local, host, remote)) {
			(void) free(l);
			return 1;
		}

		/* Make sure all parts of the item are present */
		if (strlen(local) == 0) {
			(void) free(l);
			return 1;
		}
#ifdef WIN32
		if ((strlen(host) == 0) && (strlen(remote) > 0) && (!IS_UNCPATH(remote)))
#else
		if (strlen(host) == 0)
#endif
		{
			(void) free(l);
			return 1;
		}
		if (strlen(remote) == 0) {
			(void) free(l);
			return 1;
		}

		if (comma) {
			c++;
		}
	}
	if (comma) {
		(void)free(l);
		return 1;
	}

	(void)free(l);

	return 0;
}
