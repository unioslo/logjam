/*-
 * Copyright (c) 2016-2017 Dag-Erling Smørgrav
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
#include <netdb.h>
#include <stdio.h>
#include <string.h>

#include <logjam/resolve.h>
#include <logjam/strchrnul.h>

/*
 * From FreeBSD's libfetch
 */

static struct addrinfo ai_hints = {
	.ai_family	 = AF_UNSPEC,
	.ai_socktype	 = SOCK_STREAM,
	.ai_flags	 = AI_ADDRCONFIG,
	.ai_protocol	 = PF_UNSPEC,
};

/*
 * Resolve an address.
 */
struct addrinfo *
lj_resolve(const char *addr, int port, int af)
{
	char hbuf[256], sbuf[8];
	struct addrinfo hints, *res;
	const char *hb, *he, *sep;
	const char *host, *service;
	int err, len;

	/* first, check for a bracketed IPv6 address */
	if (*addr == '[') {
		hb = addr + 1;
		if ((sep = strchr(hb, ']')) == NULL) {
			errno = EINVAL;
			goto syserr;
		}
		he = sep++;
	} else {
		hb = addr;
		sep = strchrnul(hb, ':');
		he = sep;
	}

	/* see if we need to copy the host name */
	if (*he != '\0') {
		len = snprintf(hbuf, sizeof(hbuf),
		    "%.*s", (int)(he - hb), hb);
		if (len < 0)
			goto syserr;
		if (len >= (int)sizeof(hbuf)) {
			errno = ENAMETOOLONG;
			goto syserr;
		}
		host = hbuf;
	} else {
		host = hb;
	}

	/* was it followed by a service name? */
	if (*sep == '\0' && port != 0) {
		if (port < 1 || port > 65535) {
			errno = EINVAL;
			goto syserr;
		}
		if (snprintf(sbuf, sizeof(sbuf), "%d", port) < 0)
			goto syserr;
		service = sbuf;
	} else if (*sep != '\0') {
		service = sep + 1;
	} else {
		service = NULL;
	}

	/* resolve */
	hints = ai_hints;
	if (af != 0)
		hints.ai_family = af;
	if ((err = getaddrinfo(host, service, &hints, &res)) != 0) {
		// netdb_seterr(err);
		return (NULL);
	}
	return (res);
syserr:
	// lj_syserr();
	return (NULL);
}
