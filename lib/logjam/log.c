/*-
 * Copyright (c) 2013-2017 The University of Oslo
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#define _BSD_SOURCE

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>

#define LJ_LOGV_REQUIRED
#include <logjam/log.h>
#include <logjam/strlcpy.h>

static char lj_prog_name[16];
static FILE *lj_logfile;
lj_log_level_t lj_log_level;

static int
lj_log_level_to_syslog(lj_log_level_t level)
{

	switch (level) {
	case LJ_LOG_LEVEL_DEBUG:
		return (LOG_DEBUG);
	case LJ_LOG_LEVEL_VERBOSE:
		return (LOG_INFO);
	case LJ_LOG_LEVEL_NOTICE:
		return (LOG_NOTICE);
	case LJ_LOG_LEVEL_WARNING:
		return (LOG_WARNING);
	case LJ_LOG_LEVEL_ERROR:
		return (LOG_ERR);
	default:
		return (LOG_INFO);
	}
}

static const char *
lj_log_level_to_string(lj_log_level_t level)
{

	switch (level) {
	case LJ_LOG_LEVEL_DEBUG:
		return ("debug");
	case LJ_LOG_LEVEL_VERBOSE:
		return ("verbose");
	case LJ_LOG_LEVEL_NOTICE:
		return ("notice");
	case LJ_LOG_LEVEL_WARNING:
		return ("warning");
	case LJ_LOG_LEVEL_ERROR:
		return ("error");
	default:
		return ("unknown");
	}
}

void
lj_logv(lj_log_level_t level, const char *fmt, va_list ap)
{

	if (lj_logfile != NULL) {
		fprintf(lj_logfile, "%s: %s: ", lj_prog_name,
		    lj_log_level_to_string(level));
		vfprintf(lj_logfile, fmt, ap);
		fprintf(lj_logfile, "\n");
	} else {
		vsyslog(lj_log_level_to_syslog(level), fmt, ap);
	}
}

/*
 * Log a message if at or above selected log level.
 */
void
lj_log(lj_log_level_t level, const char *fmt, ...)
{
	va_list ap;
	int serrno;

	serrno = errno;
	if (level >= lj_log_level) {
		va_start(ap, fmt);
		lj_logv(level, fmt, ap);
		va_end(ap);
	}
	errno = serrno;
}

void
lj_fatal(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	lj_logv(LJ_LOG_LEVEL_ERROR, fmt, ap);
	va_end(ap);
	exit(1);
}

/*
 * Specify a destination for log messages.  Passing NULL or an empty
 * string resets the log destination to stderr.  Passing "syslog:" causes
 * logs to be sent to syslog with the LOG_DAEMON facility and the
 * specified identifier.
 */
int
lj_log_init(const char *ident, const char *logspec)
{
	FILE *f;

	strlcpy(lj_prog_name, ident, sizeof lj_prog_name);
	if (logspec == NULL) {
		f = stderr;
	} else if (strcmp(logspec, "syslog:") == 0) {
		openlog(lj_prog_name, LOG_NDELAY|LOG_PID, LOG_DAEMON);
		f = NULL;
	} else if ((f = fopen(logspec, "a")) == NULL) {
		lj_error("unable to open log file %s: %s",
		    logspec, strerror(errno));
		return (-1);
	}
	if (lj_logfile != NULL && lj_logfile != stderr)
		fclose(lj_logfile);
	if ((lj_logfile = f) != NULL)
		setlinebuf(lj_logfile);
	return (0);
}

/*
 * Close all log destinations
 */
int
lj_log_exit(void)
{

	if (lj_logfile != NULL && lj_logfile != stderr)
		fclose(lj_logfile);
	lj_logfile = NULL;
	return (0);
}
