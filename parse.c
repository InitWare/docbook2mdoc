/* $Id: parse.c,v 1.59 2019/05/02 11:58:18 schwarze Exp $ */
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
#include <sys/types.h>

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "xmalloc.h"
#include "node.h"
#include "parse.h"

/*
 * The implementation of the DocBook parser.
 */

enum	pstate {
	PARSE_ELEM,
	PARSE_TAG,
	PARSE_ARG,
	PARSE_SQ,
	PARSE_DQ
};

/*
 * Global parse state.
 * Keep this as simple and small as possible.
 */
struct	parse {
	const char	*fname;  /* Name of the input file. */
	struct ptree	*tree;   /* Complete parse result. */
	struct pnode	*doctype;
	struct pnode	*cur;	 /* Current node in the tree. */
	enum nodeid	 ncur;   /* Type of the current node. */
	int		 line;   /* Line number in the input file. */
	int		 col;	 /* Column number in the input file. */
	int		 nline;  /* Line number of next token. */
	int		 ncol;   /* Column number of next token. */
	int		 del;    /* Levels of nested nodes being deleted. */
	int		 nofill; /* Levels of open no-fill displays. */
	int		 flags;
#define	PFLAG_WARN	 (1 << 0)  /* Print warning messages. */
#define	PFLAG_LINE	 (1 << 1)  /* New line before the next element. */
#define	PFLAG_SPC	 (1 << 2)  /* Whitespace before the next element. */
#define	PFLAG_ATTR	 (1 << 3)  /* The most recent attribute is valid. */
#define	PFLAG_EEND	 (1 << 4)  /* This element is self-closing. */
};

struct	alias {
	const char	*name;   /* DocBook element name. */
	enum nodeid	 node;   /* Node type to generate. */
};

static	const struct alias aliases[] = {
	{ "acronym",		NODE_IGNORE },
	{ "affiliation",	NODE_IGNORE },
	{ "anchor",		NODE_DELETE },
	{ "application",	NODE_COMMAND },
	{ "article",		NODE_SECTION },
	{ "articleinfo",	NODE_BOOKINFO },
	{ "book",		NODE_SECTION },
	{ "chapter",		NODE_SECTION },
	{ "caption",		NODE_IGNORE },
	{ "code",		NODE_LITERAL },
	{ "computeroutput",	NODE_LITERAL },
	{ "!doctype",		NODE_DOCTYPE },
	{ "figure",		NODE_IGNORE },
	{ "firstname",		NODE_PERSONNAME },
	{ "glossary",		NODE_VARIABLELIST },
	{ "glossdef",		NODE_IGNORE },
	{ "glossdiv",		NODE_IGNORE },
	{ "glossentry",		NODE_VARLISTENTRY },
	{ "glosslist",		NODE_VARIABLELIST },
	{ "holder",		NODE_IGNORE },
	{ "imageobject",	NODE_IGNORE },
	{ "indexterm",		NODE_DELETE },
	{ "informaltable",	NODE_TABLE },
	{ "jobtitle",		NODE_IGNORE },
	{ "keycap",		NODE_KEYSYM },
	{ "keycode",		NODE_IGNORE },
	{ "keycombo",		NODE_IGNORE },
	{ "mediaobject",	NODE_BLOCKQUOTE },
	{ "orgdiv",		NODE_IGNORE },
	{ "orgname",		NODE_IGNORE },
	{ "othercredit",	NODE_AUTHOR },
	{ "othername",		NODE_PERSONNAME },
	{ "part",		NODE_SECTION },
	{ "phrase",		NODE_IGNORE },
	{ "primary",		NODE_DELETE },
	{ "property",		NODE_PARAMETER },
	{ "reference",		NODE_SECTION },
	{ "refsect1",		NODE_SECTION },
	{ "refsect2",		NODE_SECTION },
	{ "refsect3",		NODE_SECTION },
	{ "refsection",		NODE_SECTION },
	{ "releaseinfo",	NODE_IGNORE },
	{ "returnvalue",	NODE_IGNORE },
	{ "secondary",		NODE_DELETE },
	{ "sect1",		NODE_SECTION },
	{ "sect2",		NODE_SECTION },
	{ "sect3",		NODE_SECTION },
	{ "sect4",		NODE_SECTION },
	{ "sgmltag",		NODE_MARKUP },
	{ "simpara",		NODE_PARA },
	{ "structfield",	NODE_PARAMETER },
	{ "structname",		NODE_TYPE },
	{ "surname",		NODE_PERSONNAME },
	{ "symbol",		NODE_CONSTANT },
	{ "tag",		NODE_MARKUP },
	{ "trademark",		NODE_IGNORE },
	{ "ulink",		NODE_LINK },
	{ "userinput",		NODE_LITERAL },
	{ NULL,			NODE_IGNORE }
};

struct	entity {
	const char	*name;
	const char	*roff;
};

/*
 * XML character entity references found in the wild.
 * Those that don't have an exact mandoc_char(7) representation
 * are approximated, and the desired codepoint is given as a comment.
 * Encoding them as \\[u...] would leave -Tascii out in the cold.
 */
static	const struct entity entities[] = {
	{ "alpha",	"\\(*a" },
	{ "amp",	"&" },
	{ "apos",	"'" },
	{ "auml",	"\\(:a" },
	{ "beta",	"\\(*b" },
	{ "circ",	"^" },      /* U+02C6 */
	{ "copy",	"\\(co" },
	{ "dagger",	"\\(dg" },
	{ "Delta",	"\\(*D" },
	{ "eacute",	"\\('e" },
	{ "emsp",	"\\ " },    /* U+2003 */
	{ "gt",		">" },
	{ "hairsp",	"\\^" },
	{ "kappa",	"\\(*k" },
	{ "larr",	"\\(<-" },
	{ "ldquo",	"\\(lq" },
	{ "le",		"\\(<=" },
	{ "lowbar",	"_" },
	{ "lsqb",	"[" },
	{ "lt",		"<" },
	{ "mdash",	"\\(em" },
	{ "minus",	"\\-" },
	{ "ndash",	"\\(en" },
	{ "nbsp",	"\\ " },
	{ "num",	"#" },
	{ "oslash",	"\\(/o" },
	{ "ouml",	"\\(:o" },
	{ "percnt",	"%" },
	{ "quot",	"\\(dq" },
	{ "rarr",	"\\(->" },
	{ "rArr",	"\\(rA" },
	{ "rdquo",	"\\(rq" },
	{ "reg",	"\\(rg" },
	{ "rho",	"\\(*r" },
	{ "rsqb",	"]" },
	{ "sigma",	"\\(*s" },
	{ "shy",	"\\&" },     /* U+00AD */
	{ "tau",	"\\(*t" },
	{ "tilde",	"\\[u02DC]" },
	{ "times",	"\\[tmu]" },
	{ "uuml",	"\\(:u" },
	{ NULL,		NULL }
};

static size_t	 parse_string(struct parse *, char *, size_t,
			 enum pstate *, int);
static void	 parse_fd(struct parse *, int);


static void
error_msg(struct parse *p, const char *fmt, ...)
{
	va_list		 ap;

	fprintf(stderr, "%s:%d:%d: ERROR: ", p->fname, p->line, p->col);
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fputc('\n', stderr);
	p->tree->flags |= TREE_ERROR;
}

static void
warn_msg(struct parse *p, const char *fmt, ...)
{
	va_list		 ap;

	if ((p->flags & PFLAG_WARN) == 0)
		return;

	fprintf(stderr, "%s:%d:%d: WARNING: ", p->fname, p->line, p->col);
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fputc('\n', stderr);
	p->tree->flags |= TREE_WARN;
}

/*
 * Process a string of characters.
 * If a text node is already open, append to it.
 * Otherwise, create a new one as a child of the current node.
 */
static void
xml_text(struct parse *p, const char *word, int sz)
{
	struct pnode	*n, *np;
	size_t		 oldsz, newsz;
	int		 i;

	assert(sz > 0);
	if (p->del > 0)
		return;

	if ((n = p->cur) == NULL) {
		error_msg(p, "discarding text before document: %.*s",
		    sz, word);
		return;
	}

	/* Append to the current text node, if one is open. */

	if (n->node == NODE_TEXT) {
		oldsz = strlen(n->b);
		newsz = oldsz + sz;
		if (oldsz && (p->flags & PFLAG_SPC))
			newsz++;
		n->b = xrealloc(n->b, newsz + 1);
		if (oldsz && (p->flags & PFLAG_SPC))
			n->b[oldsz++] = ' ';
		memcpy(n->b + oldsz, word, sz);
		n->b[newsz] = '\0';
		p->flags &= ~(PFLAG_LINE | PFLAG_SPC);
		return;
	}

	if (p->tree->flags & TREE_CLOSED && n == p->tree->root)
		warn_msg(p, "text after end of document: %.*s", sz, word);

	/* Create a new text node. */

	n = pnode_alloc(p->cur);
	n->node = NODE_TEXT;
	if (p->flags & PFLAG_LINE && TAILQ_PREV(n, pnodeq, child) != NULL)
		n->flags |= NFLAG_LINE;
	if (p->flags & PFLAG_SPC)
		n->flags |= NFLAG_SPC;
	p->flags &= ~(PFLAG_LINE | PFLAG_SPC);

	/*
	 * If this node follows an in-line macro without intervening
	 * whitespace, keep the text in it as short as possible,
	 * and do not keep it open.
	 */

	np = n->flags & NFLAG_SPC ? NULL : TAILQ_PREV(n, pnodeq, child);
	while (np != NULL) {
		switch (pnode_class(np->node)) {
		case CLASS_VOID:
		case CLASS_TEXT:
		case CLASS_BLOCK:
		case CLASS_NOFILL:
			np = NULL;
			break;
		case CLASS_TRANS:
			np = TAILQ_LAST(&np->childq, pnodeq);
			continue;
		case CLASS_LINE:
		case CLASS_ENCL:
			break;
		}
		break;
	}
	if (np != NULL) {
		i = 0;
		while (i < sz && !isspace((unsigned char)word[i]))
			i++;
		n->b = xstrndup(word, i);
		if (i == sz)
			return;
		while (i < sz && isspace((unsigned char)word[i]))
			i++;
		if (i == sz) {
			p->flags |= PFLAG_SPC;
			return;
		}

		/* Put any remaining text into a second node. */

		n = pnode_alloc(p->cur);
		n->node = NODE_TEXT;
		n->flags |= NFLAG_SPC;
		word += i;
		sz -= i;
	}
	n->b = xstrndup(word, sz);

	/* The new node remains open for later pnode_closetext(). */

	p->cur = n;
}

/*
 * Close out the text node and strip trailing whitespace, if one is open.
 */
static void
pnode_closetext(struct parse *p, int check_last_word)
{
	struct pnode	*n;
	char		*cp, *last_word;

	if ((n = p->cur) == NULL || n->node != NODE_TEXT)
		return;
	p->cur = n->parent;
	for (cp = strchr(n->b, '\0');
	    cp > n->b && isspace((unsigned char)cp[-1]);
	    *--cp = '\0')
		p->flags |= PFLAG_SPC;

	if (p->flags & PFLAG_SPC || !check_last_word)
		return;

	/*
	 * Find the beginning of the last word
	 * and delete whitespace before it.
	 */

	while (cp > n->b && !isspace((unsigned char)cp[-1]))
		cp--;
	last_word = cp;
	while (cp > n->b && isspace((unsigned char)cp[-1]))
		cp--;
	if (cp == n->b)
		return;
	*cp = '\0';

	/* Move the last word into its own node, for use with .Pf. */

	n = pnode_alloc_text(p->cur, last_word);
	n->flags |= NFLAG_SPC;
}

static void
xml_entity(struct parse *p, const char *name)
{
	const struct entity	*entity;
	struct pnode		*n;
	const char		*ccp;
	char			*cp;
	unsigned int		 codepoint;
	enum pstate		 pstate;

	if (p->del > 0)
		return;

	if (p->cur == NULL) {
		error_msg(p, "discarding entity before document: &%s;", name);
		return;
	}

	pnode_closetext(p, 0);

	if (p->tree->flags & TREE_CLOSED && p->cur == p->tree->root)
		warn_msg(p, "entity after end of document: &%s;", name);

	for (entity = entities; entity->name != NULL; entity++)
		if (strcmp(name, entity->name) == 0)
			break;

	if (entity->roff == NULL) {
		if (p->doctype != NULL) {
			TAILQ_FOREACH(n, &p->doctype->childq, child) {
				if ((ccp = pnode_getattr_raw(n,
				     ATTRKEY_NAME, NULL)) == NULL ||
				    strcmp(ccp, name) != 0)
					continue;
				if ((ccp = pnode_getattr_raw(n,
				    ATTRKEY_SYSTEM, NULL)) != NULL) {
					parse_file(p, -1, ccp);
					p->flags &= ~(PFLAG_LINE | PFLAG_SPC);
					return;
				}
				if ((ccp = pnode_getattr_raw(n,
				     ATTRKEY_DEFINITION, NULL)) == NULL)
					continue;
				cp = xstrdup(ccp);
				pstate = PARSE_ELEM;
				parse_string(p, cp, strlen(cp), &pstate, 0);
				p->flags &= ~(PFLAG_LINE | PFLAG_SPC);
				free(cp);
				return;
			}
		}
		if (*name == '#') {
			codepoint = strtonum(name + 1, 0, 0x10ffff, &ccp);
			if (ccp == NULL) {
				n = pnode_alloc(p->cur);
				xasprintf(&n->b, "\\[u%4.4X]", codepoint);
				goto done;
			}
		}
		error_msg(p, "unknown entity &%s;", name);
		return;
	}

	/* Create, append, and close out an entity node. */
	n = pnode_alloc(p->cur);
	n->b = xstrdup(entity->roff);
done:
	n->node = NODE_ESCAPE;
	if (p->flags & PFLAG_LINE && TAILQ_PREV(n, pnodeq, child) != NULL)
		n->flags |= NFLAG_LINE;
	if (p->flags & PFLAG_SPC)
		n->flags |= NFLAG_SPC;
	p->flags &= ~(PFLAG_LINE | PFLAG_SPC);
}

/*
 * Parse an element name.
 */
static enum nodeid
xml_name2node(struct parse *p, const char *name)
{
	const struct alias	*alias;
	enum nodeid		 node;

	if ((node = pnode_parse(name)) < NODE_UNKNOWN)
		return node;

	for (alias = aliases; alias->name != NULL; alias++)
		if (strcmp(alias->name, name) == 0)
			return alias->node;

	return NODE_UNKNOWN;
}

/*
 * Begin an element.
 */
static void
xml_elem_start(struct parse *p, const char *name)
{
	struct pnode		*n;

	/*
	 * An ancestor is excluded from the tree;
	 * keep track of the number of levels excluded.
	 */
	if (p->del > 0) {
		if (*name != '!' && *name != '?')
			p->del++;
		return;
	}

	switch (p->ncur = xml_name2node(p, name)) {
	case NODE_DELETE_WARN:
		warn_msg(p, "skipping element <%s>", name);
		/* FALLTHROUGH */
	case NODE_DELETE:
		p->del = 1;
		/* FALLTHROUGH */
	case NODE_IGNORE:
		return;
	case NODE_UNKNOWN:
		if (*name != '!' && *name != '?')
			error_msg(p, "unknown element <%s>", name);
		return;
	default:
		break;
	}

	if (p->tree->flags & TREE_CLOSED && p->cur->parent == NULL)
		warn_msg(p, "element after end of document: <%s>", name);

	switch (pnode_class(p->ncur)) {
	case CLASS_LINE:
	case CLASS_ENCL:
		pnode_closetext(p, 1);
		break;
	default:
		pnode_closetext(p, 0);
		break;
	}

	n = pnode_alloc(p->cur);
	if (p->flags & PFLAG_LINE && p->cur != NULL &&
	    TAILQ_PREV(n, pnodeq, child) != NULL)
		n->flags |= NFLAG_LINE;
	p->flags &= ~PFLAG_LINE;

	/*
	 * Some elements are self-closing.
	 * Nodes that begin a new macro or request line or start by
	 * printing text always want whitespace before themselves.
	 */

	switch (n->node = p->ncur) {
	case NODE_DOCTYPE:
	case NODE_ENTITY:
	case NODE_SBR:
	case NODE_VOID:
		p->flags |= PFLAG_EEND;
		break;
	default:
		break;
	}
	switch (pnode_class(p->ncur)) {
	case CLASS_LINE:
	case CLASS_ENCL:
		if (p->flags & PFLAG_SPC)
			n->flags |= NFLAG_SPC;
		break;
	case CLASS_NOFILL:
		p->nofill++;
		/* FALLTHROUGH */
	default:
		n->flags |= NFLAG_SPC;
		break;
	}
	p->cur = n;
	if (n->node == NODE_DOCTYPE) {
		if (p->doctype == NULL)
			p->doctype = n;
		else
			error_msg(p, "duplicate doctype");
	} else if (n->parent == NULL && p->tree->root == NULL)
		p->tree->root = n;
}

static void
xml_attrkey(struct parse *p, const char *name)
{
	struct pattr	*a;
	const char	*value;
	enum attrkey	 key;

	if (p->del > 0 || p->ncur >= NODE_UNKNOWN || *name == '\0')
		return;

	if ((p->ncur == NODE_DOCTYPE || p->ncur == NODE_ENTITY) &&
	    TAILQ_FIRST(&p->cur->attrq) == NULL) {
		value = name;
		name = "NAME";
	} else
		value = NULL;

	if ((key = attrkey_parse(name)) == ATTRKEY__MAX) {
		p->flags &= ~PFLAG_ATTR;
		return;
	}
	a = xcalloc(1, sizeof(*a));
	a->key = key;
	a->val = ATTRVAL__MAX;
	if (value == NULL) {
		a->rawval = NULL;
		p->flags |= PFLAG_ATTR;
	} else {
		a->rawval = xstrdup(value);
		p->flags &= ~PFLAG_ATTR;
	}
	TAILQ_INSERT_TAIL(&p->cur->attrq, a, child);
	if (p->ncur == NODE_ENTITY && key == ATTRKEY_NAME)
		xml_attrkey(p, "DEFINITION");
}

static void
xml_attrval(struct parse *p, const char *name)
{
	struct pattr	*a;

	if (p->del > 0 || p->ncur >= NODE_UNKNOWN ||
	    (p->flags & PFLAG_ATTR) == 0)
		return;
	if ((a = TAILQ_LAST(&p->cur->attrq, pattrq)) == NULL)
		return;
	if ((a->val = attrval_parse(name)) == ATTRVAL__MAX)
		a->rawval = xstrdup(name);
	p->flags &= ~PFLAG_ATTR;
}

/*
 * Roll up the parse tree.
 * If we're at a text node, roll that one up first.
 */
static void
xml_elem_end(struct parse *p, const char *name)
{
	struct pnode		*n;
	const char		*cp;
	enum nodeid		 node;

	/*
	 * An ancestor is excluded from the tree;
	 * keep track of the number of levels excluded.
	 */
	if (p->del > 1) {
		p->del--;
		return;
	}

	if (p->del == 0)
		pnode_closetext(p, 0);

	n = p->cur;
	node = name == NULL ? p->ncur : xml_name2node(p, name);

	switch (node) {
	case NODE_DELETE_WARN:
	case NODE_DELETE:
		if (p->del > 0)
			p->del--;
		break;
	case NODE_IGNORE:
	case NODE_UNKNOWN:
		break;
	case NODE_INCLUDE:
		p->cur = n->parent;
		cp = pnode_getattr_raw(n, ATTRKEY_HREF, NULL);
		if (cp == NULL)
			error_msg(p, "<xi:include> element "
			    "without href attribute");
		else
			parse_file(p, -1, cp);
		pnode_unlink(n);
		p->flags &= ~(PFLAG_LINE | PFLAG_SPC);
		break;
	case NODE_DOCTYPE:
	case NODE_SBR:
	case NODE_VOID:
		p->flags &= ~PFLAG_EEND;
		/* FALLTHROUGH */
	default:
		if (n == NULL || node != n->node) {
			warn_msg(p, "element not open: </%s>", name);
			break;
		}
		if (pnode_class(node) == CLASS_NOFILL)
			p->nofill--;

		/*
		 * Refrain from actually closing the document element.
		 * If no more content follows, no harm is done, but if
		 * some content still follows, simply processing it is
		 * obviously better than discarding it or crashing.
		 */

		if (n->parent != NULL || node == NODE_DOCTYPE) {
			p->cur = n->parent;
			if (p->cur != NULL)
				p->ncur = p->cur->node;
		} else
			p->tree->flags |= TREE_CLOSED;
		p->flags &= ~(PFLAG_LINE | PFLAG_SPC);

		/* Include a file containing entity declarations. */

		if (node == NODE_ENTITY && strcmp("%",
		    pnode_getattr_raw(n, ATTRKEY_NAME, "")) == 0 &&
		    (cp = pnode_getattr_raw(n, ATTRKEY_SYSTEM, NULL)) != NULL)
			parse_file(p, -1, cp);

		break;
	}
	assert(p->del == 0);
}

struct parse *
parse_alloc(int warn)
{
	struct parse	*p;

	p = xcalloc(1, sizeof(*p));
	p->tree = xcalloc(1, sizeof(*p->tree));
	if (warn)
		p->flags |= PFLAG_WARN;
	else
		p->flags &= ~PFLAG_WARN;
	return p;
}

void
parse_free(struct parse *p)
{
	if (p == NULL)
		return;
	if (p->tree != NULL) {
		pnode_unlink(p->tree->root);
		free(p->tree);
	}
	free(p);
}

static void
increment(struct parse *p, char *b, size_t *pend, int refill)
{
	if (refill) {
		if (b[*pend] == '\n') {
			p->nline++;
			p->ncol = 1;
		} else
			p->ncol++;
	}
	++*pend;
}

/*
 * Advance the pend pointer to the next character in the charset.
 * If the charset starts with a space, it stands for any whitespace.
 * Update the new input file position, used for messages.
 * Do not overrun the buffer b of length rlen.
 * When reaching the end, NUL-terminate the buffer and return 1;
 * otherwise, return 0.
 */
static int
advance(struct parse *p, char *b, size_t rlen, size_t *pend,
    const char *charset, int refill)
{
	int		 space;

	if (*charset == ' ') {
		space = 1;
		charset++;
	} else
		space = 0;

	if (refill) {
		p->nline = p->line;
		p->ncol = p->col;
	}
	while (*pend < rlen) {
		if (space && isspace((unsigned char)b[*pend]))
			break;
		if (strchr(charset, b[*pend]) != NULL)
			break;
		increment(p, b, pend, refill);
	}
	if (*pend == rlen) {
		b[rlen] = '\0';
		return refill;
	} else
		return 0;
}

size_t
parse_string(struct parse *p, char *b, size_t rlen,
    enum pstate *pstate, int refill)
{
	char		*cp;
	size_t		 pws;	/* Parse offset including whitespace. */
	size_t		 poff;  /* Parse offset in b[]. */
	size_t		 pend;  /* Offset of the end of the current word. */
	int		 elem_end;

	pend = pws = 0;
	for (;;) {

		/* Proceed to the next token, skipping whitespace. */

		if (refill) {
			p->line = p->nline;
			p->col = p->ncol;
		}
		if ((poff = pend) == rlen)
			break;
		if (isspace((unsigned char)b[pend])) {
			p->flags |= PFLAG_SPC;
			if (b[pend] == '\n') {
				p->flags |= PFLAG_LINE;
				pws = pend + 1;
			}
			increment(p, b, &pend, refill);
			continue;
		}

		/*
		 * The following four cases (ARG, TAG, and starting an
		 * entity or a tag) all parse a word or quoted string.
		 * If that extends beyond the read buffer and the last
		 * read(2) still got data, they all break out of the
		 * token loop to request more data from the read loop.
		 *
		 * Also, three of them detect self-closing tags, those
		 * ending with "/>", setting the flag elem_end and
		 * calling xml_elem_end() at the very end, after
		 * handling the attribute value, attribute name, or
		 * tag name, respectively.
		 */

		/* Parse an attribute value. */

		if (*pstate >= PARSE_ARG) {
			if (*pstate == PARSE_ARG &&
			    (b[pend] == '\'' || b[pend] == '"')) {
				*pstate = b[pend] == '"' ?
				    PARSE_DQ : PARSE_SQ;
				increment(p, b, &pend, refill);
				continue;
			}
			if (advance(p, b, rlen, &pend,
			    *pstate == PARSE_DQ ? "\"" :
			    *pstate == PARSE_SQ ? "'" : " >", refill))
				break;
			*pstate = PARSE_TAG;
			elem_end = 0;
			if (b[pend] == '>') {
				*pstate = PARSE_ELEM;
				if (pend > 0 && b[pend - 1] == '/') {
					b[pend - 1] = '\0';
					elem_end = 1;
				}
				if (p->flags & PFLAG_EEND)
					elem_end = 1;
			}
			b[pend] = '\0';
			if (pend < rlen)
				increment(p, b, &pend, refill);
			xml_attrval(p, b + poff);
			if (elem_end)
				xml_elem_end(p, NULL);

		/* Look for an attribute name. */

		} else if (*pstate == PARSE_TAG) {
			switch (p->ncur) {
			case NODE_DOCTYPE:
				if (b[pend] == '[') {
					*pstate = PARSE_ELEM;
					increment(p, b, &pend, refill);
					continue;
				}
				/* FALLTHROUGH */
			case NODE_ENTITY:
				if (b[pend] == '"' || b[pend] == '\'') {
					*pstate = PARSE_ARG;
					continue;
				}
				break;
			default:
				break;
			}
			if (advance(p, b, rlen, &pend, " =>", refill))
				break;
			elem_end = 0;
			switch (b[pend]) {
			case '>':
				*pstate = PARSE_ELEM;
				if (pend > 0 && b[pend - 1] == '/') {
					b[pend - 1] = '\0';
					elem_end = 1;
				}
				if (p->flags & PFLAG_EEND)
					elem_end = 1;
				break;
			case '=':
				*pstate = PARSE_ARG;
				break;
			default:
				break;
			}
			b[pend] = '\0';
			if (pend < rlen)
				increment(p, b, &pend, refill);
			xml_attrkey(p, b + poff);
			if (elem_end)
				xml_elem_end(p, NULL);

		/* Begin an opening or closing tag. */

		} else if (b[poff] == '<') {
			if (advance(p, b, rlen, &pend, " >", refill))
				break;
			if (pend > poff + 3 &&
			    strncmp(b + poff, "<!--", 4) == 0) {

				/* Skip a comment. */

				cp = strstr(b + pend - 2, "-->");
				if (cp == NULL) {
					if (refill)
						break;
					cp = b + rlen;
				} else
					cp += 3;
				while (b + pend < cp)
					increment(p, b, &pend, refill);
				continue;
			}
			elem_end = 0;
			if (b[pend] != '>')
				*pstate = PARSE_TAG;
			else if (pend > 0 && b[pend - 1] == '/') {
				b[pend - 1] = '\0';
				elem_end = 1;
			}
			b[pend] = '\0';
			if (pend < rlen)
				increment(p, b, &pend, refill);
			if (b[++poff] == '/') {
				elem_end = 1;
				poff++;
			} else {
				xml_elem_start(p, b + poff);
				if (*pstate == PARSE_ELEM &&
				    p->flags & PFLAG_EEND)
					elem_end = 1;
			}
			if (elem_end)
				xml_elem_end(p, b + poff);

		/* Close a doctype. */

		} else if (p->ncur == NODE_DOCTYPE && b[poff] == ']') {
			*pstate = PARSE_TAG;
			increment(p, b, &pend, refill);

		/* Process an entity. */

		} else if (b[poff] == '&') {
			if (advance(p, b, rlen, &pend, ";", refill))
				break;
			b[pend] = '\0';
			if (pend < rlen)
				increment(p, b, &pend, refill);
			xml_entity(p, b + poff + 1);

		/* Process text up to the next tag, entity, or EOL. */

		} else {
			advance(p, b, rlen, &pend,
			    p->ncur == NODE_DOCTYPE ? "<&]\n" : "<&\n",
			    refill);
			if (p->nofill)
				poff = pws;
			xml_text(p, b + poff, pend - poff);
			if (b[pend] == '\n')
				pnode_closetext(p, 0);
		}
		pws = pend;
	}
	return poff;
}


/*
 * The read loop.
 * If the previous token was incomplete and asked for more input,
 * we have to enter the read loop once more even on EOF.
 * Once rsz is 0, incomplete tokens will no longer ask for more input
 * but instead use whatever there is, and then exit the read loop.
 * The minus one on the size limit for read(2) is needed such that
 * advance() can set b[rlen] to NUL when needed.
 */
static void
parse_fd(struct parse *p, int fd)
{
	char		 b[4096];
	ssize_t		 rsz;	/* Return value from read(2). */
	size_t		 rlen;	/* Number of bytes in b[]. */
	size_t		 poff;  /* Parse offset in b[]. */
	enum pstate	 pstate;

	rlen = 0;
	pstate = PARSE_ELEM;
	while ((rsz = read(fd, b + rlen, sizeof(b) - rlen - 1)) >= 0 &&
	    (rlen += rsz) > 0) {
		poff = parse_string(p, b, rlen, &pstate, rsz > 0);
		/* Buffer exhausted; shift left and re-fill. */
		assert(poff > 0);
		rlen -= poff;
		memmove(b, b + poff, rlen);
	}
	if (rsz < 0)
		error_msg(p, "read: %s", strerror(errno));
}

/*
 * Open and parse a file.
 */
struct ptree *
parse_file(struct parse *p, int fd, const char *fname)
{
	const char	*save_fname;
	int		 save_line, save_col;

	/* Save and initialize reporting data. */

	save_fname = p->fname;
	save_line = p->nline;
	save_col = p->ncol;
	p->fname = fname;
	p->line = 0;
	p->col = 0;

	/* Open the file, unless it is already open. */

	if (fd == -1 && (fd = open(fname, O_RDONLY, 0)) == -1) {
		error_msg(p, "open: %s", strerror(errno));
		p->fname = save_fname;
		return p->tree;
	}

	/*
	 * After opening the starting file, change to the directory it
	 * is located in, in case it wants to include any further files,
	 * which are typically given with relative paths in DocBook.
	 * Do this on a best-effort basis; don't complain about failure.
	 */

	if (save_fname == NULL && (fname = dirname(fname)) != NULL &&
	    strcmp(fname, ".") != 0)
		(void)chdir(fname);

	/* Run the read loop. */

	p->nline = 1;
	p->ncol = 1;
	parse_fd(p, fd);

	/* On the top level, finalize the parse tree. */

	if (save_fname == NULL) {
		pnode_closetext(p, 0);
		if (p->tree->root == NULL)
			error_msg(p, "empty document");
		else if ((p->tree->flags & TREE_CLOSED) == 0)
			warn_msg(p, "document not closed");
		pnode_unlink(p->doctype);
	}

	/* Clean up. */

	if (fd != STDIN_FILENO)
		close(fd);
	p->fname = save_fname;
	p->nline = save_line;
	p->ncol = save_col;
	return p->tree;
}
