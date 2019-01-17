/*
 * Copyright (c) 2018 Tim Kuijsten
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
 * OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef A2ACL_H
#define A2ACL_H

#include <sys/types.h>

#include "a2id.h"

#define A2ACL_MAXLEN 500

int a2acl_whichlist(char *, struct a2id *, const struct a2id *);
ssize_t a2acl_fromfile(const char *, char *, size_t);

/*
 * When implementing a new database backend like "dbm" and "dblmdb", the
 * following four functions must be implemented:
 *    a2acl_dbopen: Initialize a database backend.
 *
 *    a2acl_dbclose: Close a database backend.
 *
 *    a2acl_putaclrule: Store a communication ACL rule given a remote and local
 * 	ID. A copy of "aclrule", "remotesel" and "localid" must be made since
 *	these are being free(3)d after this functions returns.
 *
 *    a2acl_getaclrule: Search for a communication ACL rule based on a remote
 *	selector and local ID. "aclrule" must be allocated by the caller.
 *	"aclrulesize" is a value/result parameter. In case no ACL rule is found
 *	then "aclrule" is left untouched, "aclrulesize" is set to 0 and 0 is
 *	returned.
 *
 * All four functions must return 0 on success, and -1 on failure.
 */

int a2acl_dbopen(const char *path);
int a2acl_dbclose(void);
int a2acl_putaclrule(const char *aclrule, size_t aclrulesize,
    const char *remotesel, size_t remoteselsize, const char *localid,
    size_t localidsize);
int a2acl_getaclrule(char *aclrule, size_t *aclrulesize, const char *remotesel,
    size_t remoteselsize, const char *localid, size_t localidsize);

/*
 * What follows are private structures only made public for internal testing.
 */

/*
 * ACL rule segment iterator storage, contains internel state for
 * a2acl_nextsegment(3). Only used internally and for testing.
 */
struct a2aclit {
	enum states { S, SETLIST, LIST, WILDCARD, REQSIGFLAGS, SEGMENTNAME,
	    SUBSEGMENT, POSTSEGMENT, E } state;
	const char *aclrule;
	size_t aclrulesize, n;
	int initialized;	/* magic */
};

struct a2aclseg {
	const char *seg;
	size_t segsize;
	int reqsigflags;
};

#endif /* A2ACL_H */