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

#include <sys/stat.h>
#include <sys/time.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <logjam/ctype.h>
#include <logjam/log.h>
#include <logjam/logobj.h>
#include <logjam/reader.h>
#include <logjam/strlcat.h>
#include <logjam/strlcpy.h>
#include <logjam/time.h>

typedef struct lj_file_ctx {
	struct LJ_READER_CTX;
	int		 fd;
	struct stat	 st;
	size_t		 pos, endl, len;
	char		 datefmt[64];
	char		 path[1024];
	char		 buf[65536];
} lj_file_ctx;

static lj_reader_ctx *
lj_file_init(void)
{
	lj_file_ctx *ctx;

	if ((ctx = calloc(1, sizeof *ctx)) == NULL)
		return (NULL);
	ctx->reader = &lj_file_reader;
	ctx->fd = -1;
	return ((lj_reader_ctx *)ctx);
}

static const char *
lj_file_get(lj_reader_ctx *rctx, const char *key)
{
	lj_file_ctx *ctx = (lj_file_ctx *)rctx;

	if (strcmp(key, "path") == 0) {
		return (ctx->path);
	}
	return (NULL);
}

static int
lj_file_reopen(lj_file_ctx *ctx, const char *path)
{
	int fd;

	if (path == NULL)
		path = ctx->path;
	if ((fd = open(path, O_RDONLY)) == -1)
		return (-1);
	if (path != ctx->path)
		strlcpy(ctx->path, path, sizeof ctx->path);
	close(ctx->fd);
	ctx->fd = fd;
	fstat(ctx->fd, &ctx->st);
	memset(ctx->buf, 0, sizeof ctx->buf);
	ctx->pos = ctx->endl = ctx->len = 0;
	return (0);
}

static int
lj_file_set_path(lj_file_ctx *ctx, const char *path)
{

	if (strlen(path) >= sizeof ctx->path) {
		errno = ENAMETOOLONG;
		return (-1);
	}
	return (lj_file_reopen(ctx, path));
}

static int
lj_file_set_datefmt(lj_file_ctx *ctx, const char *datefmt)
{

	if (strlen(datefmt) >= sizeof ctx->datefmt) {
		errno = ENAMETOOLONG;
		return (-1);
	}
	strlcpy(ctx->datefmt, datefmt, sizeof ctx->datefmt);
	return (0);
}

static int
lj_file_set(lj_reader_ctx *rctx, const char *key, const char *value)
{
	lj_file_ctx *ctx = (lj_file_ctx *)rctx;

	if (strcmp(key, "path") == 0)
		return (lj_file_set_path(ctx, value));
	if (strcmp(key, "datefmt") == 0)
		return (lj_file_set_datefmt(ctx, value));
	return (-1);
}

static ssize_t
lj_fillbuf(lj_file_ctx *ctx)
{
	struct stat st;
	ssize_t rlen;

	/* make room at end of buffer */
	if (ctx->pos > 0) {
		memmove(ctx->buf, ctx->buf + ctx->pos,
		    ctx->len - ctx->pos);
		ctx->len -= ctx->pos;
		ctx->endl -= ctx->pos;
		ctx->pos = 0;
	}
	/* check if the buffer is full */
	if (ctx->len >= sizeof ctx->buf) {
		errno = ENOBUFS;
		return (-1);
	}
	/* read more data into the buffer */
	rlen = sizeof ctx->buf - ctx->len;
	if ((rlen = read(ctx->fd, ctx->buf + ctx->len, rlen)) < 0) {
		lj_warning("%s: %s", ctx->path, strerror(errno));
		return (rlen);
	}
	/* end of file */
	if (rlen == 0) {
		/* has the file been rotated? */
		if (stat(ctx->path, &st) == 0) {
			if (st.st_dev != ctx->st.st_dev ||
			    st.st_ino != ctx->st.st_ino) {
				lj_verbose("%s has been rotated", ctx->path);
				raise(SIGUSR2);
				if (lj_file_reopen(ctx, NULL) < 0)
					return (-1);
			}
		}
		errno = EAGAIN;
		return (-1);
	}
	ctx->len += rlen;
	return (rlen);
}

static const char *
lj_getline(lj_file_ctx *ctx)
{
	const char *ret;
	ssize_t rlen;

	ctx->endl = ctx->pos;
	for (;;) {
		/* search for EOL in existing data */
		while (ctx->endl < ctx->len) {
			if (ctx->buf[ctx->endl] == '\n') {
				ctx->buf[ctx->endl++] = '\0';
				ret = ctx->buf + ctx->pos;
				ctx->pos = ctx->endl;
				return (ret);
			}
			ctx->endl++;
		}
		/*
		 * If the buffer is full and we still failed to find EOL,
		 * discard the entire buffer before reading in more data.
		 * We will eventually reach the end of the mega-line and
		 * return its truncated tail, but we don't really care, as
		 * it will most likely be discarded downstream.
		 */
		if (ctx->pos == 0 && ctx->endl == sizeof ctx->buf) {
			ctx->endl = ctx->len = 0;
			errno = EMSGSIZE;
			lj_warning("%s: %s", ctx->path, strerror(errno));
			return (NULL);
		}
		/* fill the buffer and loop back, unless error or EOF */
		if ((rlen = lj_fillbuf(ctx)) <= 0)
			return (NULL);
	}
}

static lj_logline *
lj_file_read(lj_reader_ctx *rctx)
{
	lj_file_ctx *ctx = (lj_file_ctx *)rctx;
	struct tm tm;
	struct timeval tv;
	lj_logline *ll;
	const char *str;
	uint64_t when;

	if ((str = lj_getline(ctx)) == NULL)
		return (NULL);

	/* try to get the timestamp, fall back to current time */
	when = 0;
	if (ctx->datefmt != NULL) {
		str = lj_strptime(str, ctx->datefmt, &tm);
		when = timelocal(&tm) * 1000000;
	}
	if ((ll = calloc(1, sizeof *ll)) == NULL)
		return (NULL);
	if (when > 0) {
		ll->when = when;
	} else {
		gettimeofday(&tv, NULL);
		ll->when = tv.tv_sec * 1000000 + tv.tv_usec;
	}
	strlcpy(ll->what, str, sizeof ll->what);
	return (ll);
}

static void
lj_file_fini(lj_reader_ctx *rctx)
{
	lj_file_ctx *ctx = (lj_file_ctx *)rctx;

	close(ctx->fd);
	free(ctx);
}

lj_reader lj_file_reader = {
	.init	 = lj_file_init,
	.get	 = lj_file_get,
	.set	 = lj_file_set,
	.read	 = lj_file_read,
	.fini	 = lj_file_fini,
};
