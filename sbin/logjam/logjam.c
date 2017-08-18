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
#include <errno.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <logjam/cirq.h>
#include <logjam/log.h>
#include <logjam/reader.h>
#include <logjam/parser.h>
#include <logjam/sender.h>

#define CIRQ_SIZE 1024

static volatile bool quit;

static cirq *ll_cirq;
static cirq *lo_cirq;

static pthread_t reader_thread;
static pthread_t parser_thread;
static pthread_t sender_thread;

static void *
reader_main(void *arg)
{
	lj_reader_ctx *ctx = arg;
	lj_logline *ll;

	while (!quit) {
		if ((ll = ctx->reader->read(ctx)) == NULL) {
			if (errno != EAGAIN)
				break;
			// fprintf(stderr, "reader waiting for events...\n");
			usleep(1000000);
			continue;
		}
		if ((ll = cirq_put(ll_cirq, ll)) != NULL)
			free(ll);
	}
	fprintf(stderr, "reader got signal to quit\n");
	return (NULL);
}

static void *
parser_main(void *arg)
{
	lj_parser_ctx *ctx = arg;
	lj_logline *ll;
	lj_logobj *lo;

	while (!quit) {
		if ((ll = cirq_get(ll_cirq, 1000000)) == NULL) {
			if (errno != ETIMEDOUT)
				break;
			// fprintf(stderr, "parser waiting for events...\n");
			continue;
		}
		if ((lo = ctx->parser->parse(ctx, ll)) != NULL) {
			if ((lo = cirq_put(lo_cirq, lo)) != NULL)
				lj_logobj_destroy(lo);
		}
		free(ll);
	}
	fprintf(stderr, "parser got signal to quit\n");
	return (NULL);
}

static void *
sender_main(void *arg)
{
	lj_sender_ctx *ctx = arg;
	lj_logobj *lo;

	(void)ctx;
	while (!quit) {
		if ((lo = cirq_get(lo_cirq, 1000000)) == NULL) {
			if (errno != ETIMEDOUT)
				break;
			// fprintf(stderr, "sender waiting for events...\n");
			continue;
		}
		ctx->sender->send(ctx, lo);
		lj_logobj_destroy(lo);
	}
	fprintf(stderr, "sender got signal to quit\n");
	return (NULL);
}

void
logjam(void)
{
	lj_reader_ctx *rctx;
	lj_parser_ctx *pctx;
	lj_sender_ctx *sctx;
	int r;

	quit = false;

	if ((ll_cirq = cirq_create(CIRQ_SIZE)) == NULL)
		err(1, "failed to create input cirq");
	if ((lo_cirq = cirq_create(CIRQ_SIZE)) == NULL)
		err(1, "failed to create output cirq");

#if NARGOTHROND
	/* create a reader that gets sshd logs from systemd */
	if ((rctx = lj_systemd_reader.init("sshd.service")) == NULL)
		err(1, "failed to initialize systemd reader");
#else
	/* create a reader that gets named logs from systemd */
	if ((rctx = lj_systemd_reader.init("named.service")) == NULL)
		err(1, "failed to initialize systemd reader");
#endif
	if ((r = pthread_create(&reader_thread, NULL, reader_main, rctx)) != 0) {
		errno = -r;
		err(1, "failed to start reader thread");
	}

#if NARGOTHROND
	/* create a parser that understands SSHD query logs */
	if ((pctx = lj_sshd_parser.init()) == NULL)
		err(1, "failed to initialize SSHD parser");
#else
	/* create a parser that understands BIND query logs */
	if ((pctx = lj_bind_parser.init()) == NULL)
		err(1, "failed to initialize BIND parser");
#endif
	if ((r = pthread_create(&parser_thread, NULL, parser_main, pctx)) != 0) {
		errno = -r;
		err(1, "failed to start parser thread");
	}

	/* creates a sender that submits to an ELK TCP receiver */
	if ((sctx = lj_elk_sender.init("")) == NULL ||
#if NARGOTHROND
	    lj_elk_sender.set(sctx, "server", "localhost:9999") != 0 ||
	    lj_elk_sender.set(sctx, "logowner", "usit-test") != 0 ||
//	    lj_elk_sender.set(sctx, "cert", "server.crt") != 0 ||
	    lj_elk_sender.set(sctx, "application", "sshd") != 0 ||
#else
	    lj_elk_sender.set(sctx, "server", "logstash-prod03.uio.no:40070") != 0 ||
//	    lj_elk_sender.set(sctx, "cert", "/root/elk/tcp.crt") != 0 ||
	    lj_elk_sender.set(sctx, "logowner", "usit-hostmaster") != 0 ||
	    lj_elk_sender.set(sctx, "application", "dns-resolv") != 0 ||
#endif
	    0)
		err(1, "failed to initialize ELK sender");
	if ((r = pthread_create(&sender_thread, NULL, sender_main, sctx)) != 0) {
		errno = -r;
		err(1, "failed to start sender thread");
	}

	while (usleep(10000000) == 0)
		/* nothing */ ;

	quit = true;
	pthread_join(reader_thread, NULL);
	pthread_join(parser_thread, NULL);
	pthread_join(sender_thread, NULL);
	rctx->reader->fini(rctx);
	pctx->parser->fini(pctx);
	sctx->sender->fini(sctx);
}
