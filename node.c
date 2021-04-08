/* $Id: node.c,v 1.28 2019/05/01 12:52:05 schwarze Exp $ */
/*
 * Copyright (c) 2014 Kristaps Dzonsons <kristaps@bsd.lv>
 * Copyright (c) 2019 Ingo Schwarze <schwarze@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHORS DISCLAIM ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "xmalloc.h"
#include "node.h"

/*
 * The implementation of the DocBook syntax tree.
 */

struct	nodeprop {
	const char	*name;
	enum nodeclass	 class;
};

static	const struct nodeprop properties[] = {
	{ "abstract",		CLASS_BLOCK },
	{ "appendix",		CLASS_BLOCK },
	{ "arg",		CLASS_ENCL },
	{ "author",		CLASS_LINE },
	{ "authorgroup",	CLASS_BLOCK },
	{ "blockquote",		CLASS_BLOCK },
	{ "bookinfo",		CLASS_BLOCK },
	{ "caution",		CLASS_BLOCK },
	{ "citerefentry",	CLASS_LINE },
	{ "citetitle",		CLASS_LINE },
	{ "cmdsynopsis",	CLASS_TRANS },
	{ "colspec",		CLASS_VOID },
	{ "command",		CLASS_LINE },
	{ "constant",		CLASS_LINE },
	{ "contrib",		CLASS_TRANS },
	{ "copyright",		CLASS_LINE },
	{ "date",		CLASS_TRANS },
	{ "!DOCTYPE",		CLASS_VOID },
	{ "editor",		CLASS_LINE },
	{ "email",		CLASS_ENCL },
	{ "emphasis",		CLASS_LINE },
	{ "!ENTITY",		CLASS_VOID },
	{ "entry",		CLASS_ENCL },
	{ "envar",		CLASS_LINE },
	{ "errorname",		CLASS_LINE },
	{ "fieldsynopsis",	CLASS_TRANS },
	{ "filename",		CLASS_LINE },
	{ "firstterm",		CLASS_LINE },
	{ "footnote",		CLASS_BLOCK },
	{ "funcdef",		CLASS_BLOCK },
	{ "funcparams",		CLASS_LINE },
	{ "funcprototype",	CLASS_BLOCK },
	{ "funcsynopsis",	CLASS_TRANS },
	{ "funcsynopsisinfo",	CLASS_LINE },
	{ "function",		CLASS_LINE },
	{ "glossterm",		CLASS_LINE },
	{ "group",		CLASS_ENCL },
	{ "imagedata",		CLASS_TEXT },
	{ "xi:include",		CLASS_VOID },
	{ "index",		CLASS_TRANS },
	{ "info",		CLASS_TRANS },
	{ "informalequation",	CLASS_BLOCK },
	{ "inlineequation",	CLASS_BLOCK },
	{ "itemizedlist",	CLASS_BLOCK },
	{ "keysym",		CLASS_LINE },
	{ "legalnotice",	CLASS_BLOCK },
	{ "link",		CLASS_ENCL },
	{ "listitem",		CLASS_TRANS },
	{ "literal",		CLASS_ENCL },
	{ "literallayout",	CLASS_NOFILL },
	{ "manvolnum",		CLASS_TRANS },
	{ "markup",		CLASS_LINE },
	{ "member",		CLASS_LINE },
	{ "mml:math",		CLASS_LINE },
	{ "mml:mfenced",	CLASS_LINE },
	{ "mml:mfrac",		CLASS_LINE },
	{ "mml:mi",		CLASS_LINE },
	{ "mml:mn",		CLASS_LINE },
	{ "mml:mo",		CLASS_LINE },
	{ "mml:mrow",		CLASS_LINE },
	{ "mml:msub",		CLASS_LINE },
	{ "mml:msup",		CLASS_LINE },
	{ "modifier",		CLASS_LINE },
	{ "note",		CLASS_BLOCK },
	{ "olink",		CLASS_ENCL },
	{ "option",		CLASS_LINE },
	{ "orderedlist",	CLASS_BLOCK },
	{ "para",		CLASS_BLOCK },
	{ "paramdef",		CLASS_LINE },
	{ "parameter",		CLASS_LINE },
	{ "personname",		CLASS_TRANS },
	{ "preface",		CLASS_BLOCK },
	{ "productname",	CLASS_LINE },
	{ "programlisting",	CLASS_NOFILL },
	{ "prompt",		CLASS_TRANS },
	{ "pubdate",		CLASS_TRANS },
	{ "quote",		CLASS_ENCL },
	{ "refclass",		CLASS_TRANS },
	{ "refdescriptor",	CLASS_TRANS },
	{ "refentry",		CLASS_TRANS },
	{ "refentryinfo",	CLASS_VOID },
	{ "refentrytitle",	CLASS_TRANS },
	{ "refmeta",		CLASS_TRANS },
	{ "refmetainfo",	CLASS_TRANS },
	{ "refmiscinfo",	CLASS_TRANS },
	{ "refname",		CLASS_LINE },
	{ "refnamediv",		CLASS_BLOCK },
	{ "refpurpose",		CLASS_LINE },
	{ "refsynopsisdiv",	CLASS_BLOCK },
	{ "replaceable",	CLASS_LINE },
	{ "row",		CLASS_BLOCK },
	{ "sbr",		CLASS_BLOCK },
	{ "screen",		CLASS_NOFILL },
	{ "section",		CLASS_BLOCK },
	{ "simplelist",		CLASS_TRANS },
	{ "simplesect",		CLASS_BLOCK },
	{ "spanspec",		CLASS_TRANS },
	{ "subscript",		CLASS_TEXT },
	{ "subtitle",		CLASS_BLOCK },
	{ "superscript",	CLASS_TEXT },
	{ "synopsis",		CLASS_NOFILL },
	{ "systemitem",		CLASS_LINE },
	{ "table",		CLASS_TRANS },
	{ "tbody",		CLASS_TRANS },
	{ "term",		CLASS_LINE },
	{ "tfoot",		CLASS_TRANS },
	{ "tgroup",		CLASS_BLOCK },
	{ "thead",		CLASS_TRANS },
	{ "tip",		CLASS_BLOCK },
	{ "title",		CLASS_BLOCK },
	{ "type",		CLASS_LINE },
	{ "variablelist",	CLASS_BLOCK },
	{ "varlistentry",	CLASS_BLOCK },
	{ "varname",		CLASS_LINE },
	{ "void",		CLASS_TEXT },
	{ "warning",		CLASS_BLOCK },
	{ "wordasword",		CLASS_TRANS },
	{ "xref",		CLASS_LINE },
	{ "year",		CLASS_TRANS },
	{ "[UNKNOWN]",		CLASS_VOID },
	{ "(t)",		CLASS_TEXT },
	{ "(e)",		CLASS_TEXT }
};

static	const char *const attrkeys[ATTRKEY__MAX] = {
	"choice",
	"class",
	"close",
	"cols",
	"DEFINITION",
	"endterm",
	"entityref",
	"fileref",
	"href",
	"id",
	"linkend",
	"localinfo",
	"NAME",
	"open",
	"PUBLIC",
	"rep",
	"SYSTEM",
	"targetdoc",
	"targetptr",
	"url",
	"xlink:href"
};

static	const char *const attrvals[ATTRVAL__MAX] = {
	"event",
	"ipaddress",
	"monospaced",
	"norepeat",
	"opt",
	"plain",
	"repeat",
	"req",
	"systemname"
};

enum attrkey
attrkey_parse(const char *name)
{
	enum attrkey	 key;

	for (key = 0; key < ATTRKEY__MAX; key++)
		if (strcmp(name, attrkeys[key]) == 0)
			break;
	return key;
}

const char *
attrkey_name(enum attrkey key)
{
	return attrkeys[key];
}

enum attrval
attrval_parse(const char *name)
{
	enum attrval	 val;

	for (val = 0; val < ATTRVAL__MAX; val++)
		if (strcmp(name, attrvals[val]) == 0)
			break;
	return val;
}

const char *
attr_getval(const struct pattr *a)
{
	return a->val == ATTRVAL__MAX ? a->rawval : attrvals[a->val];
}

enum nodeid
pnode_parse(const char *name)
{
	enum nodeid	 node;

	for (node = 0; node < NODE_UNKNOWN; node++)
		if (strcmp(name, properties[node].name) == 0)
			break;
	return node;
}

const char *
pnode_name(enum nodeid node)
{
	assert(node < NODE_IGNORE);
	return properties[node].name;
}

enum nodeclass
pnode_class(enum nodeid node)
{
	assert(node < NODE_IGNORE);
	return properties[node].class;
}

struct pnode *
pnode_alloc(struct pnode *np)
{
	struct pnode	*n;

	n = xcalloc(1, sizeof(*n));
	TAILQ_INIT(&n->childq);
	TAILQ_INIT(&n->attrq);
	if ((n->parent = np) != NULL)
		TAILQ_INSERT_TAIL(&np->childq, n, child);
	return n;
}

struct pnode *
pnode_alloc_text(struct pnode *np, const char *text)
{
	struct pnode	*n;

	n = pnode_alloc(np);
	n->node = NODE_TEXT;
	n->b = xstrdup(text);
	return n;
}

/*
 * Recursively free a node (NULL is ok).
 */
static void
pnode_free(struct pnode *n)
{
	struct pnode	*nc;
	struct pattr	*a;

	if (n == NULL)
		return;

	while ((nc = TAILQ_FIRST(&n->childq)) != NULL) {
		TAILQ_REMOVE(&n->childq, nc, child);
		pnode_free(nc);
	}
	while ((a = TAILQ_FIRST(&n->attrq)) != NULL) {
		TAILQ_REMOVE(&n->attrq, a, child);
		free(a->rawval);
		free(a);
	}
	free(n->b);
	free(n);
}

/*
 * Unlink a node from its parent and pnode_free() it.
 */
void
pnode_unlink(struct pnode *n)
{
	if (n == NULL)
		return;
	if (n->parent != NULL)
		TAILQ_REMOVE(&n->parent->childq, n, child);
	pnode_free(n);
}

/*
 * Unlink all children of a node and pnode_free() them.
 */
void
pnode_unlinksub(struct pnode *n)
{
	while (TAILQ_EMPTY(&n->childq) == 0)
		pnode_unlink(TAILQ_FIRST(&n->childq));
}

/*
 * Retrieve an enumeration attribute from a node.
 * Return ATTRVAL__MAX if the node has no such attribute.
 */
enum attrval
pnode_getattr(struct pnode *n, enum attrkey key)
{
	struct pattr	*a;

	if (n == NULL)
		return ATTRVAL__MAX;
	TAILQ_FOREACH(a, &n->attrq, child)
		if (a->key == key)
			return a->val;
	return ATTRVAL__MAX;
}

/*
 * Retrieve an attribute string from a node.
 * Return defval if the node has no such attribute.
 */
const char *
pnode_getattr_raw(struct pnode *n, enum attrkey key, const char *defval)
{
	struct pattr	*a;

	if (n == NULL)
		return defval;
	TAILQ_FOREACH(a, &n->attrq, child)
		if (a->key == key)
			return a->val != ATTRVAL__MAX ? attrvals[a->val] :
			    a->rawval != NULL ? a->rawval : defval;
	return defval;
}

/*
 * Recursively search and return the first instance of "node".
 */
struct pnode *
pnode_findfirst(struct pnode *n, enum nodeid node)
{
	struct pnode	*nc, *res;

	if (n == NULL)
		return NULL;
	if (n->node == node)
		return n;
	TAILQ_FOREACH(nc, &n->childq, child)
		if ((res = pnode_findfirst(nc, node)) != NULL)
			return res;
	return NULL;
}

/*
 * Like pnode_findfirst(), but also take the node out of the tree.
 */
struct pnode *
pnode_takefirst(struct pnode *n, enum nodeid node)
{
	struct pnode	*nc;

	if ((nc = pnode_findfirst(n, node)) != NULL && nc->parent != NULL)
		TAILQ_REMOVE(&nc->parent->childq, nc, child);
	return nc;
}
