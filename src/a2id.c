/*
 * Copyright (c) 2018, 2019 Tim Kuijsten
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

#include <assert.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/*
 * ARPA2 ID library
 *
 * The ARPA2 ID is the identifier used in the ARPA2 Identity infrastructure.
 */

#include "a2id.h"

/*
 * Copy an a2id structure.
 *
 * Note: domain, localpart, basename, firstopt and sigflags all point into the
 * complete A2ID string representation.
 *
 * Note2: The caller is responsible for allocating an a2id structure.
 *
 * Return 0 on success, -1 on failure with errno set.
 */
int
a2id_copy(struct a2id *out, const struct a2id *id)
{
	if (out == NULL || id == NULL) {
		errno = EINVAL;
		return -1;
	}

	if (id->localpartlen < id->basenamelen ||
	    id->localpartlen < id->firstoptlen ||
	    id->localpartlen < id->sigflagslen ||
	    id->localpartlen > A2ID_MAXLEN ||
	    id->domainlen > A2ID_MAXLEN ||
	    A2ID_MAXLEN - id->domainlen < id->localpartlen
	) {
		errno = EINVAL;
		return -1;
	}

	memset(out, 0, sizeof(*out));

	out->idlen = id->localpartlen + id->domainlen;
	out->localpartlen = id->localpartlen;
	out->basenamelen = id->basenamelen;
	out->firstoptlen = id->firstoptlen;
	out->sigflagslen = id->sigflagslen;
	out->domainlen = id->domainlen;

	if (id->localpartlen)
		memcpy(out->_str, id->localpart, id->localpartlen);

	memcpy(&out->_str[id->localpartlen], id->domain,
	    id->domainlen);

	if (id->localpartlen > 0)
		out->localpart = out->_str;
	else /* point to trailing 0 */
		out->localpart = &out->_str[sizeof(out->_str) - 1];

	if (id->basenamelen > 0)
		out->basename = id->basename;
	else /* point to trailing 0 */
		out->basename = &out->_str[sizeof(out->_str) - 1];

	if (id->firstoptlen > 0)
		out->firstopt = id->firstopt;
	else /* point to trailing 0 */
		out->firstopt = &out->_str[sizeof(out->_str) - 1];

	if (id->sigflagslen > 0)
		out->sigflags = id->sigflags;
	else /* point to trailing 0 */
		out->sigflags = &out->_str[sizeof(out->_str) - 1];

	if (id->domainlen > 0)
		out->domain = (char *)id->_str;
	else /* point to trailing 0 */
		out->domain = &out->_str[sizeof(out->_str) - 1];

	out->type = id->type;
	out->hassig = id->hassig;
	out->nropts = id->nropts;

	return 0;
}

static const char basechar[256] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	    0, 0, 0, 0, 0, 0, 0, 0, 0,
	1, /* ! */
	1, /* " */
	1, /* # */
	1, /* $ */
	1, /* % */
	1, /* & */
	1, /* ' */
	1, /* ( */
	1, /* ) */
	1, /* * */
	0, /* "+" PLUS is special */
	1, /* , */
	1, /* - */
	0, /* "." DOT is special */
	1, /* / */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, /* 0-9 */
	1, /* : */
	1, /* ; */
	1, /* < */
	1, /* = */
	1, /* > */
	1, /* ? */
	0, /* "@" AT is special */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	    1, 1, /* A-Z */
	1, /* [ */
	1, /* \ */
	1, /* ] */
	1, /* ^ */
	1, /* _ */
	1, /* ` */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	    1, 1, /* a-z */
	1, /* { */
	1, /* | */
	1, /* } */
	1  /* ~ */
	/* let rest of static array initialize to 0 */
};

/*
 * Static ARPA2 ID parser.
 *
 * Parse the string "in" and writes the result in "out". "in" must be a nul
 * terminated string. "selector" is a boolean that indicates wheter or not the
 * input should be parsed as a selector. A selector is a generalization of an
 * A2ID.
 *
 * On success the following fiels of the "out" structure are set:
 *
 *	type
 *	hassig	whether the ID has a signature or not
 *	nropts	total number of options
 *	localpart	points to the first character of the ID
 *	localpartlen	length, 0 if there is no localpart
 *	basename	points to the first character of the name
 *	basenamelen	length, 0 if there is no basename
 *	firstopt	points to leading '+' if it exists
 *	firstoptlen	length including leading '+', 0 if there is no firstopt
 *	sigflags	points to leading '+' if it exists
 *	sigflagslen	length including leading '+', 0 if there are no sigflags
 *	domain	points to leading '@', always exists in a valid ID
 *	domainlen	length including '@', every valid ID requires a domain
 *	idlen	total length of the ID
 *
 * "localpart", "basename", "firstopt", "sigflags" and "domain" are not
 * guaranteed to be nul terminated.
 *
 * On error "idlen" contains the length of the string up to but not including
 * the first erroneous character in "in". Another way to read this is, on error
 * "idlen" contains the index of the first erroneaous character in "in".
 *
 * Return 0 if "in" is a valid A2ID and could be parsed, -1 otherwise.
 */
int
a2id_parsestr(struct a2id *out, const char *in, int selector)
{
	enum states { S, SERVICE, LOCALPART, OPTION, NEWLABEL, DOMAIN } state;
	char *curopt, *prevopt, *secondopt;
	size_t i;
	unsigned char c;

	if (in == NULL || out == NULL)
		return -1;

	secondopt = prevopt = curopt = NULL;

	out->generalized = 0;
	out->hassig = 0;
	out->nropts = 0;
	out->localpart = NULL;
	out->basename = NULL;
	out->firstopt = NULL;
	out->sigflags = NULL;
	out->domain = NULL;
	out->type = A2IDT_GENERIC;
	out->localpartlen = 0;
	out->basenamelen = 0;
	out->firstoptlen = 0;
	out->sigflagslen = 0;
	out->domainlen = 0;
	out->idlen = 0;

	state = S;
	for (i = 0; i < A2ID_MAXLEN && in[i] != '\0'; i++) {
		c = in[i];

		/* Copy string. */
		out->_str[i] = c;

		switch (state) {
		case S:
			if (basechar[c] || c == '.') {
				out->localpart = &out->_str[i];
				out->basename = &out->_str[i];
				state = LOCALPART;
			} else if (c == '+') {
				out->localpart = &out->_str[i];
				state = SERVICE;
			} else if (c == '@') {
				out->domain = &out->_str[i];
				state = NEWLABEL;
			} else
				goto done;
			break;
		case SERVICE:
			if (basechar[c] || c == '.') {
				out->basename = &out->_str[i];
				state = LOCALPART;
			} else if (selector && c == '@') {
				out->domain = &out->_str[i];
				state = NEWLABEL;
			} else if (selector && c == '+') {
				curopt = &out->_str[i];
				out->firstopt = &out->_str[i];
				out->nropts++;
				state = OPTION;
			} else
				goto done;
			break;
		case LOCALPART:
			if (basechar[c] || c == '.') {
				/* keep going */
			} else if (c == '+') {
				prevopt = curopt;
				curopt = &out->_str[i];
				if (out->firstopt == NULL) {
					out->firstopt = &out->_str[i];
				} else if (secondopt == NULL) {
					secondopt = &out->_str[i];
				}

				out->nropts++;
				state = OPTION;
			} else if (c == '@') {
				out->domain = &out->_str[i];
				state = NEWLABEL;
			} else
				goto done;
			break;
		case OPTION:
			if (basechar[c] || c == '.') {
				state = LOCALPART;
			} else if (c == '+') {
				prevopt = curopt;
				curopt = &out->_str[i];
				if (secondopt == NULL) {
					secondopt = &out->_str[i];
				}
				out->nropts++;
			} else if (c == '@') {
				out->domain = &out->_str[i];
				state = NEWLABEL;
			} else
				goto done;
			break;
		case DOMAIN:
			if (basechar[c]) {
				/* keep going */
			} else if (c == '.') {
				state = NEWLABEL;
			} else
				goto done;
			break;
		case NEWLABEL:
			if (basechar[c]) {
				state = DOMAIN;
			} else if (selector && c == '.') {
				/* keep going */
			} else
				goto done;
			break;
		default:
			abort();
		}
	}

done:
	/* Ensure termination. */
	out->idlen = i;
	out->_str[i] = '\0';

	out->generalized = 0;

	/*
	 * Make sure the end of the input is reached and the state is one of the
	 * final states.
	 */
	if (in[i] != '\0')
		return -1;

	if (selector) {
		if (state != DOMAIN && state != NEWLABEL)
			return -1;
	} else {
		if (state != DOMAIN)
			return -1;
	}

	/* Determine type. */
	if (out->localpart) {
		if (*out->localpart == '+')
			out->type = A2IDT_SERVICE;
		else
			out->type = A2IDT_GENERIC;
	} else {
		out->type = A2IDT_DOMAINONLY;
	}

	/* Calculate lengths and point to trailing '\0' if length is 0. */

	out->domainlen = &out->_str[i] - out->domain;
	assert(out->domainlen > 0);

	out->localpartlen = out->domain - out->_str;

	if (out->localpartlen == 0)
		out->localpart = &out->_str[i];

	/* First determine if there was a signature. */
	if (curopt && prevopt && curopt + 1 == out->domain) {
		out->hassig = 1;
		out->sigflags = prevopt;
		out->sigflagslen = curopt - prevopt;

		/*
		 * Undo the signature which has a leading and trailing '+' that
		 * are both counted as an option.
		 */
		out->nropts -= 2;
		if (out->nropts == 0) {
			out->firstopt = NULL;
			out->firstoptlen = 0;
		}
	} else {
		out->hassig = 0;
		out->sigflagslen = 0;
		out->sigflags = &out->_str[i];
	}

	if (out->firstopt) {
		if (secondopt) {
			out->firstoptlen = secondopt - out->firstopt;
		} else if (out->sigflagslen)
			out->firstoptlen = out->sigflags - out->firstopt;
		else
			out->firstoptlen = out->domain - out->firstopt;
	} else {
		out->firstoptlen = 0;
		out->firstopt = &out->_str[i];
	}

	if (out->basename) {
		if (out->firstoptlen) {
			out->basenamelen = out->firstopt - out->basename;
		} else if (out->sigflagslen) {
			out->basenamelen = out->sigflags - out->basename;
		} else {
			out->basenamelen = out->domain - out->basename;
		}
	} else {
		out->basenamelen = 0;
		out->basename = &out->_str[i];
	}

	return 0;
}

/*
 * Match an ARPA2 ID with an ARPA2 ID Selector.
 *
 * Return 1 if the subject matches the selector, 0 otherwise. If the localpart
 * and/or domain in the selector are an empty string it is considered to be a
 * match to the respective part in the subject.
 */
int
a2id_match(const struct a2id *subject, const struct a2id *selector)
{
	char *selp, *subp;
	size_t selplen, subplen;
	int n;

	assert(subject && selector);

	if (selector->localpartlen > 0) {
		if (selector->localpartlen > subject->localpartlen)
			return 0;

		if (selector->hassig) {
			if (!subject->hassig)
				return 0;

			/* if sigflags selector is not empty it must match */
			if (selector->sigflagslen > 1 &&
			    selector->sigflagslen != subject->sigflagslen)
					return 0;

			if (selector->sigflagslen > 1 &&
			    strncmp(selector->sigflags, subject->sigflags,
			    selector->sigflagslen) != 0)
				return 0;
		}

		/* compare each segment */
		selp = selector->localpart;
		subp = subject->localpart;

		if (selector->type == A2IDT_SERVICE) {
			if (subject->type != A2IDT_SERVICE)
				return 0;

			/* skip leading '+' */
			selp++;
			subp++;
		}

		if (selector->nropts > subject->nropts)
			return 0;

		/* Compare base and option segments. */
		for (n = -1; n < selector->nropts; n++) {
			/* lock-step comparison up till next separator */
			for (selplen = subplen = 0;
			    *selp != '+' && *selp != '@' && *selp != '\0' &&
			    *subp != '+' && *subp != '@' && *subp != '\0';
			    selplen++, selp++,
			    subplen++, subp++) {
				if (tolower(*selp) != tolower(*subp))
					break;
			}

			/* selector must have reached a separator */
			if (*selp != '+' && *selp != '@' && *selp != '\0')
				return 0;

			/*
			 * If there is no text in the segment of the selector
			 * after the last '+', any segment in the subject will
			 * do, as long as it exists (except for a leading or
			 * trailing '+' which indicates a service or a
			 * signature, respectively).
			 */
			if (selplen == 0) {
				if (*subp == '+' ||
				    *subp == '@' ||
				    *subp == '\0')
					return 0;

				while (*subp != '+' && *subp != '@' && *subp != '\0')
					subp++;
			}

			if (*subp != '+' && *subp != '@' && *subp != '\0')
				return 0;

			if (*selp == '@' || *selp == '\0')
				break; /* done, every selector segment matches */

			if (*subp != '+')
				return 0;

			selp++;
			subp++;
		}
	}

	if (selector->domainlen > 0) {
		/*
		 * Can't compare domain lengths because of optional trailing
		 * dots and possibly empty labels in the selector.
		 */
		if (subject->domainlen < 1)
			return 0;

		/* compare each label, starting from the back */
		assert(*selector->domain == '@' && *subject->domain == '@');
		selp = &selector->domain[selector->domainlen - 1];
		subp = &subject->domain[subject->domainlen - 1];

		for (;;) {
			/* Step over leading dot of current label. */
			if (*selp == '.') /* ROOT dot is optional */
				selp--;

			if (*subp == '.')
				subp--;

			/* lock-step comparison */
			for (selplen = subplen = 0;
			    *selp != '@' && *selp != '.' &&
			    *subp != '@' && *subp != '.';
			    selplen++, selp--,
			    subplen++, subp--) {
				if (tolower(*selp) != tolower(*subp))
					break;
			}

			if (*selp != '@' && *selp != '.')
				return 0;

			if (selplen == 0) {
				/*
				 * A dot without label means there must be a
				 * label in the subject, no matter what the
				 * content.
				 */
				if (*subp == '@' || *subp == '.')
					return 0;

				/*
				 * Jump over the label now that we now it's
				 * there.
				 */
				while (*subp != '@' && *subp != '.')
					subp--;
			}

			if (*subp != '@' && *subp != '.')
				return 0;

			if (*selp == '@')
				break; /* done, every selector label matches */
		}
	}

	/* Match if we made it this far. */

	return 1;
}

/*
 * Generalize an A2ID structure by one step. Generalization is the process of
 * removing segments and labels from the localpart and domain, in that order.
 * Each function call represents one generalization. As long as there are
 * segments in the localpart, one segment is removed from the localpart from
 * right to left. As soon as the localpart can't be generalized any further,
 * domain labels are removed from left to right, until no labels are left. "id"
 * is a value/result parameter.
 *
 * Returns 1 if a component is removed from the localpart or the domain.
 * Returns 0 if "id" can not be further generalized.
 *
 * XXX don't move domain by removing every label, just increment the domain
 * pointer now that the memory is allocated statically in the structure.
 */
int
a2id_generalize(struct a2id *id)
{
	char *cp;
	size_t i, n;

	if (id == NULL)
		return 0;

	if (id->sigflagslen > 0) {
		if (id->sigflagslen > 1) {
			/* remove signature data, but leave trailing '+' */
			*(id->sigflags + 1) = '+';
			*(id->sigflags + 2) = '\0';
			id->localpartlen -= id->sigflagslen - 1;
			id->sigflagslen = 1;
		} else {
			/* remove signature and trailing '+' */
			*id->sigflags = '\0';
			id->localpartlen -= id->sigflagslen + 1;
			id->sigflagslen = 0;
			id->hassig = 0;
		}

		id->idlen = id->localpartlen + id->domainlen;
		id->generalized++;
		return 1;
	}

	if (id->nropts > 0) {
		cp = &id->localpart[id->localpartlen - 1];

		/*
		 * Either delete the option data, or the option. If there is
		 * only a trailing '+', remove the option, else everything up
		 * to the previous '+' is removed, exclusive.
		 */

		/* remove the option */
		if (*cp == '+') {
			*cp = '\0';
			id->nropts--;
			id->localpartlen--;

			if (id->nropts == 0) {
				*id->firstopt = '\0';
				id->firstoptlen = 0;
			}
		} else /* remove option data only */
			for (; *cp != '+'; cp--, id->localpartlen--)
				*cp = '\0';

		id->idlen = id->localpartlen + id->domainlen;
		id->generalized++;
		return 1;
	}

	if (id->basenamelen > 0) {
		*id->basename = '\0';
		id->localpartlen -= id->basenamelen;
		id->basenamelen = 0;
		id->idlen = id->localpartlen + id->domainlen;
		id->generalized++;
		return 1;
	}

	/* Strip next label. */

	assert(id->domain[0] == '@');

	cp = id->domain;
	/* step over '@' */
	cp++;
	if (cp[0] != '.' || cp[1] != '\0') {
		/*
		 * Either remove leading dot, or up to, but not including, next
		 * dot.
		 */

		assert(id->domainlen >= 2);

		/* Find the number of characters before the first dot. */
		n = strcspn(cp, ".");

		/* If the first character is a dot, remove the dot itself. */
		if (n == 0)
			n = 1;

		/* move rtl */
		for (i = 0; cp[i + n]; i++)
			cp[i] = cp[i + n];

		/* and terminate */
		cp[i] = '\0';

		id->domainlen -= n;

		/* On end of string, ensure terminating ROOT dot. */
		if (cp[n] == '\0') {
			cp[0] = '.';
			cp[1] = '\0';
			id->domainlen = 2;
		}

		return 1;
	}

	return 0;
}

/*
 * Print the different parts of "id" to "fp".
 */
void
a2id_print(FILE *fp, const struct a2id *id)
{
	fprintf(fp, "type %d\n", id->type);
	fprintf(fp, "hassig %d\n", id->hassig);
	fprintf(fp, "nropts %d\n", id->nropts);
	fprintf(fp, "generalized %d\n", id->generalized);

	fprintf(fp, "localpart %zu %.*s\n", id->localpartlen, (int)id->localpartlen,
	    id->localpart);
	fprintf(fp, "basename %zu %.*s\n", id->basenamelen, (int)id->basenamelen,
	    id->basename);
	fprintf(fp, "firstopt %zu %.*s\n", id->firstoptlen, (int)id->firstoptlen,
	    id->firstopt);
	fprintf(fp, "sigflags %zu %.*s\n", id->sigflagslen, (int)id->sigflagslen,
	    id->sigflags);
	fprintf(fp, "domain %zu %.*s\n", id->domainlen, (int)id->domainlen,
	    id->domain);
	fprintf(fp, "str %zu %.*s\n", id->idlen, (int)id->idlen, id->_str);
}

/*
 * Write the core form of "id" as a string into "dst". "dstsize" is a
 * value/result parameter and should be at least A2ID_MAXLEN + 1 bytes to ensure
 * it will not fail.
 *
 * Return 0 on success, -1 if dst is too short.
 */
int
a2id_coreform(char *dst, const struct a2id *id, size_t *dstsize)
{
	size_t r;

	switch (id->type) {
	case A2IDT_GENERIC:
		r = snprintf(dst, *dstsize, "%.*s%.*s", (int)id->basenamelen,
		    id->basename, (int)id->domainlen, id->domain);

		if (r >= *dstsize)
			return -1;
		*dstsize = r;
		return 0;
	case A2IDT_SERVICE:
		r = snprintf(dst, *dstsize, "+%.*s%.*s", (int)id->basenamelen,
		    id->basename, (int)id->domainlen, id->domain);

		if (r >= *dstsize)
			return -1;
		*dstsize = r;
		return 0;
	case A2IDT_DOMAINONLY:
		r = snprintf(dst, *dstsize, "%.*s", (int)id->domainlen,
		    id->domain);

		if (r >= *dstsize)
			return -1;
		*dstsize = r;
		return 0;
	}

	return -1;
}

/*
 * Write the string representation of "id" into "dst". dstsize is a value/result
 * parameter and should be at least A2ID_MAXLEN + 1 bytes to ensure it will not
 * fail.
 *
 * Return 0 on success, -1 if dst is too short.
 */
int
a2id_tostr(char *dst, const struct a2id *id, size_t *dstsize)
{
	size_t r;

	r = snprintf(dst, *dstsize, "%.*s%.*s", (int)id->localpartlen,
	    id->localpart, (int)id->domainlen, id->domain);

	if (r >= *dstsize)
		return -1;

	*dstsize = r;

	return 0;
}

/*
 * Determine the start and length of optional segments, excluding any sigflags
 * segment.
 *
 * "optseg" will be set to point to the first character of the first option,
 * right after it's leading '+'. If "id" has no optional segments then "optseg"
 * is not set.
 *
 * Returns the length of "optseg" or 0 if "optseg" is not set.
 *
 * XXX might want to make this part of a2id_parsestr
 */
size_t
a2id_optsegments(const char **optseg, const struct a2id *id)
{
	size_t s;

	if (id->firstoptlen <= 1)
		return 0;

	*optseg = id->firstopt;
	s = id->localpartlen - id->basenamelen;

	/* Step over leading optseg '+' */
	(*optseg)++;
	s--;

	if (id->type == A2IDT_SERVICE) {
		assert(*id->localpart == '+');
		assert(*id->basename != '+');
		s--;
	}

	if (id->sigflagslen > 0) {
		assert(s > id->sigflagslen);
		s -= id->sigflagslen;
		s--; /* trailing '+' */
	}

	return s;
}
