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

#ifndef LOGJAM_READER_H_INCLUDED
#define LOGJAM_READER_H_INCLUDED

#include <logjam/types.h>

typedef lj_reader_ctx *(*lj_reader_init_f)(void);
typedef int (*lj_reader_set_f)(lj_reader_ctx *, const char *, const char *);
typedef const char *(*lj_reader_get_f)(lj_reader_ctx *, const char *);
typedef lj_logline *(*lj_reader_read_f)(lj_reader_ctx *);
typedef void (*lj_reader_fini_f)(lj_reader_ctx *);

#define LJ_READER_CTX { lj_reader *reader; }
struct lj_reader_ctx LJ_READER_CTX;

struct lj_reader {
	lj_reader_init_f	 init;
	lj_reader_get_f		 get;
	lj_reader_set_f		 set;
	lj_reader_read_f	 read;
	lj_reader_fini_f	 fini;
};

static inline void
lj_reader_fini(lj_reader_ctx *rctx)
{

	rctx->reader->fini(rctx);
}

#if HAVE_LIBSYSTEMD
extern lj_reader lj_systemd_reader;
#endif

#endif
