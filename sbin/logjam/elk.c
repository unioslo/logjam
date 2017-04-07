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

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <jansson.h>

#include <logjam/log.h>
#include <logjam/sender.h>

typedef struct lj_elk_ctx {
	struct LJ_SENDER_CTX;
	json_t *template;
} lj_elk_ctx;

static lj_sender_ctx *
lj_elk_init(const char *dest)
{
	lj_elk_ctx *ctx;

	(void)dest;
	if ((ctx = calloc(1, sizeof *ctx)) == NULL)
		return (NULL);
	ctx->sender = &lj_elk_sender;
	if ((ctx->template = json_object()) == NULL)
		goto fail;
	return ((lj_sender_ctx *)ctx);
fail:
	if (ctx->template != NULL)
		json_decref(ctx->template);
	return (NULL);
}

static const char *
lj_elk_get(lj_sender_ctx *sctx, const char *key)
{
	lj_elk_ctx *ctx = (lj_elk_ctx *)sctx;

	if (strcmp(key, "logowner") == 0 ||
	    strcmp(key, "application") == 0) {
		return (json_string_value(json_object_get(ctx->template, key)));
	}
	return (NULL);
}

static int
lj_elk_set(lj_sender_ctx *sctx, const char *key, const char *value)
{
	lj_elk_ctx *ctx = (lj_elk_ctx *)sctx;

	if (strcmp(key, "logowner") == 0 ||
	    strcmp(key, "application") == 0) {
		if (json_object_set_new(ctx->template, key, json_string(value)) != 0)
			return (-1);
		return (0);
	}
	return (-1);
}

static int
lj_elk_send(lj_sender_ctx *sctx, const lj_logobj *lo)
{
	lj_elk_ctx *ctx = (lj_elk_ctx *)sctx;
	json_t *obj;
	int ret;

	ret = 0;
	if ((obj = json_copy(lo->json)) == NULL ||
	    json_object_update(obj, ctx->template) != 0 ||
	    json_dumpf(obj, stderr, JSON_PRESERVE_ORDER) ||
	    fprintf(stderr, "\n") < 0)
		ret = -1;
	json_decref(obj);
	return (ret);
}

static void
lj_elk_fini(lj_sender_ctx *sctx)
{
	lj_elk_ctx *ctx = (lj_elk_ctx *)sctx;

	json_decref(ctx->template);
	free(ctx);
}

lj_sender lj_elk_sender = {
	.init	 = lj_elk_init,
	.get	 = lj_elk_get,
	.set	 = lj_elk_set,
	.send	 = lj_elk_send,
	.fini	 = lj_elk_fini,
};
