/*-
 * Copyright (c) 2017 The University of Oslo
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <logjam/logjam.h>
#include <logjam/config.h>
#include <logjam/log.h>
#include <logjam/pidfile.h>

static const char *lj_pidfile = "/var/run/logjam.pid";
static int lj_foreground = 0;

static void
daemonize(void)
{
	struct lj_pidfh *pidfh;
	pid_t pid;

	if ((pidfh = lj_pidfile_open(lj_pidfile, 0600, &pid)) == NULL) {
		if (errno == EEXIST) {
			lj_fatal("already running with PID %lu",
			    (unsigned long)pid);
		} else {
			lj_fatal("unable to open or create pidfile %s: %s",
			    lj_pidfile, strerror(errno));
		}
	}
	if (daemon(0, 0) != 0)
		lj_fatal("unable to daemonize: %s", strerror(errno));
	lj_pidfile_write(pidfh);
}

static void
usage(void)
{

	fprintf(stderr, "usage: "
	    "logjam [-dfv] [-c config] [-l logspec] [-p pidfile]\n");
	exit(1);
}

int
main(int argc, char *argv[])
{
	const char *logspec = NULL;
	int opt, ret;

	while ((opt = getopt(argc, argv, "c:dfl:p:v")) != -1)
		switch (opt) {
		case 'c':
			lj_config_file = optarg;
			break;
		case 'd':
			if (lj_log_level > LJ_LOG_LEVEL_DEBUG)
				lj_log_level = LJ_LOG_LEVEL_DEBUG;
			break;
		case 'f':
			++lj_foreground;
			break;
		case 'l':
			logspec = optarg;
			break;
		case 'p':
			lj_pidfile = optarg;
			break;
		case 'v':
			if (lj_log_level > LJ_LOG_LEVEL_VERBOSE)
				lj_log_level = LJ_LOG_LEVEL_VERBOSE;
			break;
		default:
			usage();
		}

	argc -= optind;
	argv += optind;

	if (argc > 0)
		usage();

	if (!lj_foreground)
		daemonize();

        lj_log_init("logjam", logspec ? logspec :
	    lj_foreground ? NULL : "syslog:");
        ret = logjam();
        lj_log_exit();

        exit(ret == 0 ? 0 : 1);
}
