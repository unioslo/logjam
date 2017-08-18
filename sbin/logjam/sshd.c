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

#include <err.h>
#include <regex.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <logjam/log.h>
#include <logjam/parser.h>

const char *sshd_re =
    "^"
    "Failed "
    "([a-z-]+)"			/* 1: method */
    " for "
    "(invalid user |)"		/* 2: ignored */
    "([0-9a-z-]+)"		/* 3: login */
    " from "
    "([0-9A-Fa-f:.]+)"		/* 4: client address */
    " port "
    "([0-9]+)"			/* 5: client port */
    " "
    "ssh([0-9.]+)"		/* 6: protocol version */
    "$";
#define NMATCH 7

typedef struct lj_sshd_ctx {
	struct LJ_PARSER_CTX;
	regex_t re;
} lj_sshd_ctx;

static lj_parser_ctx *lj_sshd_init(void);
static lj_logobj *lj_sshd_parse(lj_parser_ctx *, const lj_logline *);
static void lj_sshd_fini(lj_parser_ctx *);

static lj_parser_ctx *
lj_sshd_init(void)
{
	lj_sshd_ctx *ctx;
	int ret;

	if ((ctx = calloc(1, sizeof *ctx)) == NULL)
		return (NULL);
	ctx->parser = &lj_sshd_parser;
	if ((ret = regcomp(&ctx->re, sshd_re, REG_EXTENDED)) != 0)
		goto fail;
	return ((lj_parser_ctx *)ctx);
fail:
	lj_sshd_fini((lj_parser_ctx *)ctx);
	return (NULL);
}

static lj_logobj *
lj_sshd_parse(lj_parser_ctx *pctx, const lj_logline *ll)
{
	lj_sshd_ctx *ctx = (lj_sshd_ctx *)pctx;
	regmatch_t pmatch[NMATCH];
	lj_logobj *lo;

	if (regexec(&ctx->re, ll->what, NMATCH, pmatch, 0) != 0)
		return (NULL);
	if ((lo = lj_logobj_create()) == NULL)
		return (NULL);
	if (lj_logobj_settime(lo, ll->when) != 0)
		goto fail;
#define LO_SET_STRN(KEY, N)					\
	lj_logobj_setstrn(lo, KEY, ll->what + pmatch[N].rm_so,	\
	    pmatch[N].rm_eo - pmatch[N].rm_so)
	if (LO_SET_STRN("method",        1) != 0 ||
	    LO_SET_STRN("login",         3) != 0 ||
	    LO_SET_STRN("client_addr",   4) != 0 ||
	    LO_SET_STRN("client_port",   5) != 0 ||
	    LO_SET_STRN("protocol",      6) != 0)
		goto fail;
#undef LO_SET_STRN
	return (lo);
fail:
	lj_logobj_destroy(lo);
	return (NULL);
}

static void
lj_sshd_fini(lj_parser_ctx *pctx)
{
	lj_sshd_ctx *ctx = (lj_sshd_ctx *)pctx;

	regfree(&ctx->re);
	free(ctx);
}

lj_parser lj_sshd_parser = {
	.init	 = lj_sshd_init,
	.parse	 = lj_sshd_parse,
	.fini	 = lj_sshd_fini,
};
