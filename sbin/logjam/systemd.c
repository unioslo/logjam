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

#include <sys/time.h>

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <systemd/sd-journal.h>

#include <logjam/ctype.h>
#include <logjam/log.h>
#include <logjam/reader.h>

#define TIMESTAMP_FIELD "_SOURCE_REALTIME_TIMESTAMP"
#define MESSAGE_FIELD	"MESSAGE"

typedef struct lj_systemd_ctx {
	struct LJ_READER_CTX;
	sd_journal		*j;
} lj_systemd_ctx;

static lj_reader_ctx *
lj_systemd_init(const char *loc)
{
	char str[256];
	lj_systemd_ctx *ctx;
	int r;

	/* prepare filter string */
	if (snprintf(str, sizeof str, "_SYSTEMD_UNIT=%s", loc) >= (int)sizeof str) {
		errno = EINVAL;
		return (NULL);
	}
	if ((ctx = calloc(1, sizeof *ctx)) == NULL)
		return (NULL);
	ctx->reader = &lj_systemd_reader;
	if ((r = sd_journal_open(&ctx->j,
	    SD_JOURNAL_LOCAL_ONLY|SD_JOURNAL_SYSTEM)) != 0) {
		// fprintf(stderr, "sd_journal_open(): %s\n", strerror(-r));
		goto sd_err;
	}
	if ((r = sd_journal_seek_tail(ctx->j)) != 0) {
		// fprintf(stderr, "sd_journal_seek_tail(): %s\n", strerror(-r));
		goto sd_err;
	}
	if ((r = sd_journal_add_match(ctx->j, str, 0)) != 0) {
		// fprintf(stderr, "sd_journal_add_match(): %s\n", strerror(-r));
		goto sd_err;
	}
	return ((lj_reader_ctx *)ctx);
sd_err:
	free(ctx);
	errno = -r;
	return (NULL);
}

static lj_logline *
lj_systemd_read(lj_reader_ctx *sctx)
{
	lj_systemd_ctx *ctx = (lj_systemd_ctx *)sctx;
	struct timeval tv;
	lj_logline *ll;
	const char *str;
	uintmax_t umax;
	size_t len;
	int r;

	/* request the next log entry */
	if ((r = sd_journal_next(ctx->j)) < 0) {
		errno = -r;
		return (NULL);
	} else if (r == 0) {
		errno = EAGAIN;
		return (NULL);
	}
	/* get text of message first, no point otherwise */
	r = sd_journal_get_data(ctx->j, MESSAGE_FIELD,
	    (const void **)&str, &len);
	if (r != 0) {
		errno = -r;
		return (NULL);
	}
	if ((ll = calloc(1, sizeof *ll)) == NULL)
		return (NULL);
	str += sizeof MESSAGE_FIELD;
	len -= sizeof MESSAGE_FIELD;
	if (len >= sizeof ll->what)
		len = sizeof ll->what - 1;
	memcpy(ll->what, str, len);
	ll->what[len] = '\0';
	/* try to get the timestamp, fall back to current time */
	r = sd_journal_get_data(ctx->j, TIMESTAMP_FIELD,
	    (const void **)&str, &len);
	if (r == 0) {
		str += sizeof TIMESTAMP_FIELD;
		len -= sizeof TIMESTAMP_FIELD;
	} else {
		len = 0;
	}
	/* not terminated, can't use strtoumax */
	for (umax = 0; len > 0 && is_digit(*str); len--, str++) {
		if (umax >= UINTMAX_MAX / 10)
			break;
		umax = umax * 10 + *str - '0';
	}
	if (len == 0 && umax > 0) {
		ll->when = umax;
	} else {
		gettimeofday(&tv, NULL);
		ll->when = tv.tv_sec * 1000000 + tv.tv_usec;
	}
	return (ll);
}

static void
lj_systemd_fini(lj_reader_ctx *sctx)
{
	lj_systemd_ctx *ctx = (lj_systemd_ctx *)sctx;

	sd_journal_close(ctx->j);
	free(ctx);
}

lj_reader lj_systemd_reader = {
	.init	 = lj_systemd_init,
	.read	 = lj_systemd_read,
	.fini	 = lj_systemd_fini,
};
