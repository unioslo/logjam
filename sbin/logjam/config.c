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
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include <logjam/config.h>
#include <logjam/flume.h>
#include <logjam/log.h>
#include <logjam/parser.h>
#include <logjam/reader.h>
#include <logjam/sender.h>

#include <jansson.h>

const char *lj_config_file = LJ_DEFAULT_CONFIG;

static lj_reader_ctx *
lj_config_unpack_reader(const char *cfn, json_t *obj)
{
	lj_reader_ctx *rctx;
	const char *key, *str;
	json_t *value;
	void *iter;

	if (json_typeof(obj) != JSON_OBJECT) {
		lj_error("%s: reader must be an object", cfn);
		return (NULL);
	}
	if ((value = json_object_get(obj, "class")) == NULL) {
		lj_error("%s: reader has no class", cfn);
		return (NULL);
	}
	if ((str = json_string_value(value)) == NULL) {
		lj_error("%s: reader class must be a string", cfn);
		return (NULL);
	}
	if (strcmp(str, "file") == 0) {
		if ((rctx = lj_file_reader.init()) == NULL) {
			lj_error("%s: failed to initialize file reader", cfn);
			return (NULL);
		}
#if HAVE_LIBSYSTEMD
	} else if (strcmp(str, "systemd") == 0) {
		if ((rctx = lj_systemd_reader.init()) == NULL) {
			lj_error("%s: failed to initialize systemd reader", cfn);
			return (NULL);
		}
#endif
	} else {
		lj_error("%s: unrecognized reader class '%s'", cfn, str);
		return (NULL);
	}
	json_object_del(obj, "class");
	for (iter = json_object_iter(obj);
	     iter != NULL;
	     iter = json_object_iter_next(obj, iter)) {
		key = json_object_iter_key(iter);
		value = json_object_iter_value(iter);
		if ((str = json_string_value(value)) == NULL) {
			lj_error("%s: reader property '%s' must be a string", cfn, key);
			return (NULL);
		}
		if (rctx->reader->set(rctx, key, str) != 0) {
			lj_error("%s: failed to set reader property '%s'", cfn, key);
			return (NULL);
		}
	}
	return (rctx);
}

static lj_parser_ctx *
lj_config_unpack_parser(const char *cfn, json_t *obj)
{
	lj_parser_ctx *pctx;
	const char *key, *str;
	json_t *value;
	void *iter;

	if (json_typeof(obj) != JSON_OBJECT) {
		lj_error("%s: parser must be an object", cfn);
		return (NULL);
	}
	if ((value = json_object_get(obj, "class")) == NULL) {
		lj_error("%s: parser has no class", cfn);
		return (NULL);
	}
	if ((str = json_string_value(value)) == NULL) {
		lj_error("%s: parser class must be a string", cfn);
		return (NULL);
	}
	if (strcmp(str, "sshd") == 0) {
		if ((pctx = lj_sshd_parser.init()) == NULL) {
			lj_error("%s: failed to initialize sshd parser", cfn);
			return (NULL);
		}
	} else if (strcmp(str, "bind") == 0) {
		if ((pctx = lj_bind_parser.init()) == NULL) {
			lj_error("%s: failed to initialize BIND parser", cfn);
			return (NULL);
		}
	} else {
		lj_error("%s: unrecognized parser class '%s'", cfn, str);
		return (NULL);
	}
	json_object_del(obj, "class");
	for (iter = json_object_iter(obj);
	     iter != NULL;
	     iter = json_object_iter_next(obj, iter)) {
		key = json_object_iter_key(iter);
		value = json_object_iter_value(iter);
		if ((str = json_string_value(value)) == NULL) {
			lj_error("%s: parser property '%s' must be a string", cfn, key);
			return (NULL);
		}
		if (pctx->parser->set(pctx, key, str) != 0) {
			lj_error("%s: failed to set parser property '%s'", cfn, key);
			return (NULL);
		}
	}
	return (pctx);
}

static lj_sender_ctx *
lj_config_unpack_sender(const char *cfn, json_t *obj)
{
	lj_sender_ctx *sctx;
	const char *key, *str;
	json_t *value;
	void *iter;

	if (json_typeof(obj) != JSON_OBJECT) {
		lj_error("%s: sender must be an object", cfn);
		return (NULL);
	}
	if ((value = json_object_get(obj, "class")) == NULL) {
		lj_error("%s: sender has no class", cfn);
		return (NULL);
	}
	if ((str = json_string_value(value)) == NULL) {
		lj_error("%s: sender class must be a string", cfn);
		return (NULL);
	}
	if (strcmp(str, "elk") == 0) {
		if ((sctx = lj_elk_sender.init()) == NULL) {
			lj_error("%s: failed to initialize elk sender", cfn);
			return (NULL);
		}
	} else {
		lj_error("%s: unrecognized sender class '%s'", cfn, str);
		return (NULL);
	}
	json_object_del(obj, "class");
	for (iter = json_object_iter(obj);
	     iter != NULL;
	     iter = json_object_iter_next(obj, iter)) {
		key = json_object_iter_key(iter);
		value = json_object_iter_value(iter);
		if ((str = json_string_value(value)) == NULL) {
			lj_error("%s: sender property '%s' must be a string", cfn, key);
			return (NULL);
		}
		if (sctx->sender->set(sctx, key, str) != 0) {
			lj_error("%s: failed to set sender property '%s'", cfn, key);
			return (NULL);
		}
	}
	return (sctx);
}

static lj_flume *
lj_config_unpack_flume(const char *cfn, json_t *obj)
{
	lj_flume *flume;
	const char *key;
	json_t *value;
	void *iter;

	if ((flume = lj_flume_init()) == NULL)
		return (NULL);
	if (json_typeof(obj) != JSON_OBJECT) {
		lj_error("%s: flume must be an object", cfn);
		return (NULL);
	}
	/* first, pull the required items */
	if ((value = json_object_get(obj, "reader")) == NULL) {
		lj_error("%s: flume has no reader", cfn);
		return (NULL);
	}
	flume->rctx = lj_config_unpack_reader(cfn, value);
	json_object_del(obj, "reader");
	if ((value = json_object_get(obj, "parser")) == NULL) {
		lj_error("%s: flume has no parser", cfn);
		return (NULL);
	}
	flume->pctx = lj_config_unpack_parser(cfn, value);
	json_object_del(obj, "parser");
	if ((value = json_object_get(obj, "sender")) == NULL) {
		lj_error("%s: flume has no sender", cfn);
		return (NULL);
	}
	flume->sctx = lj_config_unpack_sender(cfn, value);
	json_object_del(obj, "sender");
	/* then iterate over the rest */
	for (iter = json_object_iter(obj);
	     iter != NULL;
	     iter = json_object_iter_next(obj, iter)) {
		key = json_object_iter_key(iter);
		value = json_object_iter_value(iter);
		lj_error("%s: unknown flume property %s", cfn, key);
		/* XXX warn or bail? */
		return (NULL);
	}
	return (flume);
}

static lj_flume *
lj_config_unpack_flumes(const char *cfn, json_t *ary)
{
	lj_flume *flume = NULL;
	unsigned int i, n;
	json_t *obj;

	if (json_typeof(ary) != JSON_ARRAY)
		lj_error("%s: value of 'flumes' must be an array", cfn);
	for (i = 0, n = json_array_size(ary); i < n; ++i) {
		obj = json_array_get(ary, i);
		if (flume != NULL) {
			lj_error("%s: multiple flumes are not yet supported", cfn);
			return (NULL);
		}
		flume = lj_config_unpack_flume(cfn, obj);
	}
	return (flume);
}

static void
lj_config_unpack_log_level(const char *cfn, json_t *value)
{
	const char *str;

	if ((str = json_string_value(value)) == NULL)
		lj_error("%s: log_level must be a string", cfn);
	else if (strcmp(str, "debug") == 0)
		lj_log_level = LJ_LOG_LEVEL_DEBUG;
	else if (strcmp(str, "verbose") == 0)
		lj_log_level = LJ_LOG_LEVEL_VERBOSE;
	else if (strcmp(str, "verbose") == 0)
		lj_log_level = LJ_LOG_LEVEL_VERBOSE;
	else if (strcmp(str, "verbose") == 0)
		lj_log_level = LJ_LOG_LEVEL_VERBOSE;
	else if (strcmp(str, "verbose") == 0)
		lj_log_level = LJ_LOG_LEVEL_VERBOSE;
	else
		lj_error("%s: invalid log_level", cfn);
}

static lj_flume *
lj_config_unpack_root(const char *cfn, json_t *obj)
{
	lj_flume *flume = NULL;
	const char *key;
	json_t *value;
	void *iter;

	if (json_typeof(obj) != JSON_OBJECT) {
		lj_error("%s: root must be an object", cfn);
		return (NULL);
	}
	for (iter = json_object_iter(obj);
	     iter != NULL;
	     iter = json_object_iter_next(obj, iter)) {
		key = json_object_iter_key(iter);
		value = json_object_iter_value(iter);
		if (strcmp(key, "flumes") == 0) {
			if (flume != NULL) {
				lj_error("%s: multiple flumes are not yet supported", cfn);
				goto fail;
			}
			flume = lj_config_unpack_flumes(cfn, value);
		} else if (strcmp(key, "log_level") == 0) {
			lj_config_unpack_log_level(cfn, value);
		} else {
			lj_error("%s: unexpected key %s", cfn, key);
			goto fail;
		}
	}
	if (flume == NULL)
		lj_error("%s: at least one flume is required", cfn);
	return (flume);
fail:
	if (flume != NULL)
		lj_flume_fini(flume);
	return (NULL);
}

lj_flume *
lj_configure(const char *cfn)
{
	json_error_t jsonerr;
	lj_flume *flume;
	json_t *root;

	if ((root = json_load_file(cfn, 0, &jsonerr)) == NULL) {
		lj_warning("%s: %s", cfn, jsonerr.text);
		return (NULL);
	}
	flume = lj_config_unpack_root(cfn, root);
	json_decref(root);
	return (flume);
}
