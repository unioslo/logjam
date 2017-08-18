/*-
 * Copyright (s) 2017 The University of Oslo
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
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#if HAVE_GNUTLS
#include <gnutls/gnutls.h>
#endif

#include <logjam/connect.h>
#include <logjam/socket.h>
#include <logjam/strlcpy.h>

struct lj_socket {
	char	 target[256];
	int	 sd;
	int	 lasterr;
#if HAVE_GNUTLS
	struct {
		enum { failed = -1, disabled = 0, enabled, connected } state;
		gnutls_session_t session;
		gnutls_certificate_credentials_t cred;
	} tls;
#endif
};

lj_socket *
sock_create(const char *target)
{
	lj_socket *s;

	if ((s = calloc(1, sizeof *s)) == NULL)
		return (NULL);
	if (strlcpy(s->target, target, sizeof s->target) >= sizeof s->target) {
		free(s);
		errno = EINVAL;
		return (NULL);
	}
	s->sd = -1;
	return (s);
}

int
sock_use_tls(lj_socket *s)
{

#if HAVE_GNUTLS
	if (s->sd >= 0 || s->tls.state != disabled)
		return (0);
	s->tls.state = failed;
	if (gnutls_certificate_allocate_credentials(&s->tls.cred) >= 0 &&
	    gnutls_certificate_set_x509_system_trust(s->tls.cred) >= 0) {
		s->tls.state = enabled;
		return (0);
	}
	if (s->tls.cred != NULL)
		gnutls_certificate_free_credentials(s->tls.cred);
	memset(&s->tls, 0, sizeof s->tls);
	return (-1);
#else
	(void)s;
	return (-1);
#endif
}

int
sock_use_cert(lj_socket *s, const char *certfile)
{

#if HAVE_GNUTLS
	if (s->sd >= 0 || s->tls.state != enabled)
		return (-1);
	(void)certfile;
	return (-1);
#else
	(void)s;
	(void)certfile;
	return (-1);
#endif
}

int
sock_open(lj_socket *s)
{
	int ret;

	if (s->sd >= 0)
		return (-1);

	/* resolve and connect */
	if ((s->sd = lj_connect(s->target, 0, 0)) < 0) {
		s->lasterr = errno;
		goto fail;
	}

#if HAVE_GNUTLS
	/* establish TLS */
	if (s->tls.state == enabled) {
		s->lasterr = EPROTO;
		s->tls.state = failed;
		if (gnutls_init(&s->tls.session, GNUTLS_CLIENT) != 0 ||
		    gnutls_set_default_priority(s->tls.session) != 0 ||
		    gnutls_credentials_set(s->tls.session, GNUTLS_CRD_CERTIFICATE, s->tls.cred) != 0)
			goto fail;
		/* SNI? */
		gnutls_transport_set_int(s->tls.session, s->sd);
		gnutls_handshake_set_timeout(s->tls.session, GNUTLS_DEFAULT_HANDSHAKE_TIMEOUT);
		while ((ret = gnutls_handshake(s->tls.session)) != 0) {
			if (gnutls_error_is_fatal(ret))
				goto fail;
		}
		s->tls.state = connected;
	}
#endif
	s->lasterr = 0;
	return (0);
fail:
#if HAVE_GNUTLS
	if (s->tls.session != NULL) {
		gnutls_credentials_clear(s->tls.session);
		gnutls_deinit(s->tls.session);
		s->tls.session = NULL;
	}
#endif
	if (s->sd >= 0) {
		close(s->sd);
		s->sd = -1;
	}
	return (-1);
}

int
sock_reopen(lj_socket *s)
{

	sock_close(s);
	return (sock_open(s));
}

void
sock_close(lj_socket *s)
{

#if HAVE_GNUTLS
	if (s->tls.session != NULL) {
		if (s->tls.state == connected) {
			/* XXX can return error code */
			gnutls_bye(s->tls.session, GNUTLS_SHUT_RDWR);
		}
		gnutls_credentials_clear(s->tls.session);
		gnutls_deinit(s->tls.session);
		s->tls.session = NULL;
	}
	if (s->tls.state != disabled)
		s->tls.state = enabled;
#endif
	if (s->sd >= 0) {
		close(s->sd);
		s->sd = -1;
	}
	s->lasterr = 0;
}

ssize_t
sock_write(lj_socket *s, const void *buf, size_t len)
{
	ssize_t ret, sent;

	/* assert(len < SSIZE_MAX) */
	sent = 0;
	while (sent < (ssize_t)len) {
#if HAVE_GNUTLS
		if (s->tls.state != disabled) {
			ret = gnutls_record_send(s->tls.session,
			    buf + sent, len - sent);
			if (ret >= 0) {
				sent += ret;
			} else if (ret == GNUTLS_E_INTERRUPTED ||
			    ret == GNUTLS_E_AGAIN) {
				/* retry */
			} else {
				s->tls.state = failed;
				s->lasterr = EPROTO;
				return (-1);
			}
			continue;
		}
#else
		ret = write(s->sd, buf + sent, len - sent);
		if (ret >= 0) {
			sent += ret;
		} else if (errno == EINTR ||
		    errno == EAGAIN) {
			/* retry */
		} else {
			s->lasterr = errno;
			return (-1);
		}
#endif
	}
	return (sent);
}

ssize_t
sock_read(lj_socket *s, void *buf, size_t len)
{

	/* XXX */
	(void)s;
	(void)buf;
	(void)len;
	return (-1);
}

void
sock_destroy(lj_socket *s)
{

	sock_close(s);
	free(s);
}

int
sock_connected(lj_socket *s)
{

	if (s->sd < 0 || s->lasterr != 0)
		return (0);
#if HAVE_GNUTLS
	if (s->tls.state != disabled && s->tls.state != connected)
		return (0);
#endif
	return (1);
}
