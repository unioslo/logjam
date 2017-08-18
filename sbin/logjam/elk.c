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

#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <jansson.h>

#include <logjam/log.h>
#include <logjam/sender.h>
#include <logjam/socket.h>

typedef struct lj_elk_ctx {
	struct LJ_SENDER_CTX;
	json_t *template;
	lj_socket *sock;
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
lj_elk_set_server(lj_sender_ctx *sctx, const char *server)
{
	lj_elk_ctx *ctx = (lj_elk_ctx *)sctx;
	lj_socket *sock;

	if ((sock = sock_create(server)) == NULL)
		return (-1);
	if (sock_use_tls(sock) != 0) {
		sock_destroy(sock);
		return (-1);
	}
	if (ctx->sock != NULL)
		sock_destroy(ctx->sock);
	ctx->sock = sock;
	return (0);
}

static int
lj_elk_set_cert(lj_sender_ctx *sctx, const char *cert)
{
	lj_elk_ctx *ctx = (lj_elk_ctx *)sctx;

	if (ctx->sock == NULL)
		return (-1);
	return (sock_use_cert(ctx->sock, cert));
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
	} else if (strcmp(key, "server") == 0) {
		return (lj_elk_set_server(sctx, value));
	} else if (strcmp(key, "cert") == 0) {
		return (lj_elk_set_cert(sctx, value));
	}
	return (-1);
}

static int
lj_elk_json_callback(const char *buffer, size_t size, void *data)
{
	lj_elk_ctx *ctx = (lj_elk_ctx *)data;

	if (size > SSIZE_MAX)
		return (-1);
#if NARGOTHROND
	fwrite(buffer, size, 1, stderr);
#endif
	if (sock_write(ctx->sock, buffer, size) != (ssize_t)size)
		return (-1);
	return (0);
}

static int
lj_elk_send(lj_sender_ctx *sctx, const lj_logobj *lo)
{
	lj_elk_ctx *ctx = (lj_elk_ctx *)sctx;
	json_t *obj;
	int ret;

	/*
	 * If we aren't already connected, or our existing connection has
	 * failed, attempt to (re)connect.  We should probably have some
	 * sort of exponential backoff here, and perhaps also move this
	 * out of lj_elk_send() and into the sender thread's main loop.
	 */
	if (!sock_connected(ctx->sock) && sock_reopen(ctx->sock) != 0)
		return (-1);

	/*
	 * Create and transmit json object
	 */
	ret = -1;
	if ((obj = json_copy(lo->json)) != NULL &&
	    json_object_update(obj, ctx->template) == 0) {
		if (json_dump_callback(obj, lj_elk_json_callback,
		    ctx, JSON_PRESERVE_ORDER | JSON_COMPACT) == 0)
			ret = 0;
		/*
		 * Jansson's error reporting isn't very good, so we don't
		 * know the exact source of the error.  If it occurred
		 * within Jansson itself rather than at the connection or
		 * encryption level, it is probably recoverable, but there
		 * is a risk that we sent a partial record.  Therefore, we
		 * always send a newline, even if we failed to send the
		 * record itself.
		 */
		if (lj_elk_json_callback("\n", 1, ctx) != 0)
			ret = -1;
	}
	json_decref(obj);
	return (ret);
}

static void
lj_elk_fini(lj_sender_ctx *sctx)
{
	lj_elk_ctx *ctx = (lj_elk_ctx *)sctx;

	json_decref(ctx->template);
	if (ctx->sock != NULL)
		sock_destroy(ctx->sock);
	free(ctx);
}

lj_sender lj_elk_sender = {
	.init	 = lj_elk_init,
	.get	 = lj_elk_get,
	.set	 = lj_elk_set,
	.send	 = lj_elk_send,
	.fini	 = lj_elk_fini,
};
