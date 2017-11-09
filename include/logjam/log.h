/*-
 * Copyright (c) 2013-2016 The University of Oslo
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

#ifndef LOGJAM_LOG_H_INCLUDED
#define LOGJAM_LOG_H_INCLUDED

typedef enum {
	LJ_LOG_LEVEL_DEBUG,
	LJ_LOG_LEVEL_VERBOSE,
	LJ_LOG_LEVEL_NOTICE,
	LJ_LOG_LEVEL_WARNING,
	LJ_LOG_LEVEL_ERROR,
	LJ_LOG_LEVEL_MAX
} lj_log_level_t;

#ifdef LJ_LOGV_REQUIRED
void lj_logv(lj_log_level_t, const char *, va_list);
#endif
void lj_log(lj_log_level_t, const char *, ...);
void lj_fatal(const char *, ...);
int lj_log_init(const char *, const char *);
int lj_log_exit(void);

extern lj_log_level_t lj_log_level;

#define lj_log_if(level, ...)						\
	do {								\
		if (level >= lj_log_level)				\
			lj_log(level, __VA_ARGS__);			\
	} while (0)
#define lj_debug(...)							\
	lj_log_if(LJ_LOG_LEVEL_DEBUG, __VA_ARGS__)
#define lj_verbose(...)							\
	lj_log_if(LJ_LOG_LEVEL_VERBOSE, __VA_ARGS__)
#define lj_notice(...)							\
	lj_log_if(LJ_LOG_LEVEL_NOTICE, __VA_ARGS__)
#define lj_warning(...)							\
	lj_log_if(LJ_LOG_LEVEL_WARNING, __VA_ARGS__)
#define lj_error(...)							\
	lj_log_if(LJ_LOG_LEVEL_ERROR, __VA_ARGS__)
#define lj_fatal(...)							\
	lj_fatal(__VA_ARGS__)

#endif
