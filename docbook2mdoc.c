/* $Id: docbook2mdoc.c,v 1.148 2019/05/02 04:15:40 schwarze Exp $ */
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
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "xmalloc.h"
#include "node.h"
#include "macro.h"
#include "format.h"

/*
 * The implementation of the mdoc(7) formatter.
 */

static void	 pnode_print(struct format *, struct pnode *);


static void
pnode_printtext(struct format *f, struct pnode *n)
{
	struct pnode	*nn;
	char		*cp;
	int		 accept_arg;

	para_check(f);
	cp = n->b;
	accept_arg = f->flags & FMT_ARG;
	if (f->linestate == LINE_MACRO && !accept_arg &&
	    (n->flags & NFLAG_SPC) == 0) {
		for (;;) {
			if (*cp == '\0')
				return;
			if (strchr("!),.:;?]", *cp) == NULL)
				break;
			printf(" %c", *cp++);
		}
		if (isspace((unsigned char)*cp)) {
			while (isspace((unsigned char)*cp))
				cp++;
			n->flags |= NFLAG_SPC;
		} else {
			f->flags &= ~FMT_NOSPC;
			f->flags |= FMT_CHILD;
			macro_open(f, "Ns");
			f->flags &= ~FMT_ARG;
			f->flags |= FMT_CHILD;
			accept_arg = 1;
		}
	}
	if (f->linestate == LINE_MACRO && f->nofill == 0 &&
	    !accept_arg && (f->flags & FMT_IMPL) == 0)
		macro_close(f);

	/*
	 * Text preceding a macro without intervening whitespace
	 * requires a .Pf macro.
	 * Set the spacing flag to avoid a redundant .Ns macro.
	 */

	if ((f->nofill || f->linestate != LINE_MACRO) &&
	    (nn = TAILQ_NEXT(n, child)) != NULL &&
	     (nn->flags & NFLAG_SPC) == 0) {
		switch (pnode_class(nn->node)) {
		case CLASS_LINE:
		case CLASS_ENCL:
			macro_open(f, "Pf");
			accept_arg = 1;
			f->flags |= FMT_CHILD;
			nn->flags |= NFLAG_SPC;
			break;
		default:
			break;
		}
	}

	switch (f->linestate) {
	case LINE_NEW:
		break;
	case LINE_TEXT:
		if (n->flags & NFLAG_SPC) {
			if (n->flags & NFLAG_LINE &&
			    pnode_class(n->node) == CLASS_TEXT)
				macro_close(f);
			else
				putchar(' ');
		}
		break;
	case LINE_MACRO:
		if (accept_arg == 0) {
			if (f->nofill) {
				f->flags &= ~FMT_NOSPC;
				f->flags |= FMT_CHILD;
				macro_open(f, "No ");
				f->flags &= ~FMT_ARG;
				f->flags |= FMT_CHILD;
			} else
				macro_close(f);
		} else if (n->flags & NFLAG_SPC ||
		    (f->flags & FMT_ARG) == 0 ||
		    (nn = TAILQ_PREV(n, pnodeq, child)) == NULL ||
		    pnode_class(nn->node) != CLASS_TEXT)
			putchar(' ');
		break;
	}

	if (n->node == NODE_ESCAPE) {
		fputs(n->b, stdout);
		if (f->linestate == LINE_NEW)
			f->linestate = LINE_TEXT;
		return;
	}

	/*
	 * Remove the prefix '-' from <option> elements
	 * because the arguments of .Fl macros do not need it.
	 */

	if (n->parent != NULL && n->parent->node == NODE_OPTION && *cp == '-')
		cp++;

	if (f->linestate == LINE_MACRO)
		macro_addarg(f, cp, 0);
	else
		print_text(f, cp, 0);
}

static void
pnode_printimagedata(struct format *f, struct pnode *n)
{
	const char	*cp;

	if ((cp = pnode_getattr_raw(n, ATTRKEY_FILEREF, NULL)) == NULL)
		cp = pnode_getattr_raw(n, ATTRKEY_ENTITYREF, NULL);
	if (cp != NULL) {
		print_text(f, "[image:", ARG_SPACE);
		print_text(f, cp, ARG_SPACE);
		print_text(f, "]", 0);
	} else
		print_text(f, "[image]", ARG_SPACE);
}

static void
pnode_printrefnamediv(struct format *f, struct pnode *n)
{
	struct pnode	*nc, *nn;
	int		 comma;

	f->parastate = PARA_HAVE;
	macro_line(f, "Sh NAME");
	f->parastate = PARA_HAVE;
	comma = 0;
	TAILQ_FOREACH_SAFE(nc, &n->childq, child, nn) {
		if (nc->node != NODE_REFNAME)
			continue;
		if (comma)
			macro_addarg(f, ",", ARG_SPACE);
		macro_open(f, "Nm");
		macro_addnode(f, nc, ARG_SPACE);
		pnode_unlink(nc);
		comma = 1;
	}
	macro_close(f);
}

/*
 * If the SYNOPSIS macro has a superfluous title, kill it.
 */
static void
pnode_printrefsynopsisdiv(struct format *f, struct pnode *n)
{
	struct pnode	*nc, *nn;

	TAILQ_FOREACH_SAFE(nc, &n->childq, child, nn)
		if (nc->node == NODE_TITLE)
			pnode_unlink(nc);

	f->parastate = PARA_HAVE;
	macro_line(f, "Sh SYNOPSIS");
	f->parastate = PARA_HAVE;
}

/*
 * Start a hopefully-named `Sh' section.
 */
static void
pnode_printsection(struct format *f, struct pnode *n)
{
	struct pnode	*nc, *ncc;
	int		 flags, level;

	if (n->parent == NULL)
		return;

	level = ++f->level;
	flags = ARG_SPACE;
	switch (n->node) {
	case NODE_SECTION:
	case NODE_APPENDIX:
		if (level == 1)
			flags |= ARG_UPPER;
		break;
	case NODE_SIMPLESECT:
		if (level < 2)
			level = 2;
		break;
	case NODE_NOTE:
		if (level < 3)
			level = 3;
		break;
	default:
		abort();
	}

	TAILQ_FOREACH(nc, &n->childq, child)
		if (nc->node == NODE_TITLE)
			break;

	switch (level) {
	case 1:
		macro_close(f);
		f->parastate = PARA_HAVE;
		macro_open(f, "Sh");
		break;
	case 2:
		macro_close(f);
		f->parastate = PARA_HAVE;
		macro_open(f, "Ss");
		break;
	default:
		if (f->parastate == PARA_MID)
			f->parastate = PARA_WANT;
		macro_open(f, "Sy");
		break;
	}
	macro_addnode(f, nc, flags);
	macro_close(f);

	/*
	 * DocBook has no equivalent for -split mode,
	 * so just switch the default in the AUTHORS section.
	 */

	if (nc != NULL) {
		if (level == 1 &&
		    (ncc = TAILQ_FIRST(&nc->childq)) != NULL &&
		    ncc->node == NODE_TEXT &&
		    strcasecmp(ncc->b, "AUTHORS") == 0)
			macro_line(f, "An -nosplit");
		pnode_unlink(nc);
	}
	f->parastate = level > 2 ? PARA_WANT : PARA_HAVE;
}

/*
 * Start a reference, extracting the title and volume.
 */
static void
pnode_printciterefentry(struct format *f, struct pnode *n)
{
	struct pnode	*nc, *title, *manvol;

	title = manvol = NULL;
	TAILQ_FOREACH(nc, &n->childq, child) {
		if (nc->node == NODE_MANVOLNUM)
			manvol = nc;
		else if (nc->node == NODE_REFENTRYTITLE)
			title = nc;
	}
	macro_open(f, "Xr");
	if (title == NULL)
		macro_addarg(f, "unknown", ARG_SPACE);
	else
		macro_addnode(f, title, ARG_SPACE | ARG_SINGLE);
	if (manvol == NULL)
		macro_addarg(f, "1", ARG_SPACE);
	else
		macro_addnode(f, manvol, ARG_SPACE | ARG_SINGLE);
	pnode_unlinksub(n);
}

/*
 * The <mml:mfenced> node is a little peculiar.
 * First, it can have arbitrary open and closing tokens, which default
 * to parentheses.
 * Second, >1 arguments are separated by commas.
 */
static void
pnode_printmathfenced(struct format *f, struct pnode *n)
{
	struct pnode	*nc;

	printf("left %s ", pnode_getattr_raw(n, ATTRKEY_OPEN, "("));

	nc = TAILQ_FIRST(&n->childq);
	pnode_print(f, nc);

	while ((nc = TAILQ_NEXT(nc, child)) != NULL) {
		putchar(',');
		pnode_print(f, nc);
	}
	printf("right %s ", pnode_getattr_raw(n, ATTRKEY_CLOSE, ")"));
	pnode_unlinksub(n);
}

/*
 * These math nodes require special handling because they have infix
 * syntax, instead of the usual prefix or prefix.
 * So we need to break up the first and second child node with a
 * particular eqn(7) word.
 */
static void
pnode_printmath(struct format *f, struct pnode *n)
{
	struct pnode	*nc;

	nc = TAILQ_FIRST(&n->childq);
	pnode_print(f, nc);

	switch (n->node) {
	case NODE_MML_MSUP:
		fputs(" sup ", stdout);
		break;
	case NODE_MML_MFRAC:
		fputs(" over ", stdout);
		break;
	case NODE_MML_MSUB:
		fputs(" sub ", stdout);
		break;
	default:
		break;
	}

	nc = TAILQ_NEXT(nc, child);
	pnode_print(f, nc);
	pnode_unlinksub(n);
}

static void
pnode_printfuncprototype(struct format *f, struct pnode *n)
{
	struct pnode	*fdef, *fps, *ftype, *nc, *nn;

	/*
	 * Extract <funcdef> child and ignore <void> child.
	 * Leave other children in place, to be treated as parameters.
	 */

	fdef = NULL;
	TAILQ_FOREACH_SAFE(nc, &n->childq, child, nn) {
		switch (nc->node) {
		case NODE_FUNCDEF:
			if (fdef == NULL) {
				fdef = nc;
				TAILQ_REMOVE(&n->childq, nc, child);
				nc->parent = NULL;
			}
			break;
		case NODE_VOID:
			pnode_unlink(nc);
			break;
		default:
			break;
		}
	}

	/*
	 * If no children are left, the function is void; use .Fn.
	 * Otherwise, use .Fo.
	 */

	nc = TAILQ_FIRST(&n->childq);
	if (fdef != NULL) {
		ftype = TAILQ_FIRST(&fdef->childq);
		if (ftype != NULL && ftype->node == NODE_TEXT) {
			macro_argline(f, "Ft", ftype->b);
			pnode_unlink(ftype);
		}
		if (nc == NULL) {
			macro_open(f, "Fn");
			macro_addnode(f, fdef, ARG_SPACE | ARG_SINGLE);
			macro_addarg(f, "void", ARG_SPACE);
			macro_close(f);
		} else
			macro_nodeline(f, "Fo", fdef, ARG_SINGLE);
		pnode_unlink(fdef);
	} else if (nc == NULL)
		macro_line(f, "Fn UNKNOWN void");
	else
		macro_line(f, "Fo UNKNOWN");

	if (nc == NULL)
		return;

	while (nc != NULL) {
		if ((fps = pnode_takefirst(nc, NODE_FUNCPARAMS)) != NULL) {
			macro_open(f, "Fa \"");
			macro_addnode(f, nc, ARG_QUOTED);
			macro_addarg(f, "(", ARG_QUOTED);
			macro_addnode(f, fps, ARG_QUOTED);
			macro_addarg(f, ")", ARG_QUOTED);
			putchar('"');
			macro_close(f);
		} else
			macro_nodeline(f, "Fa", nc, ARG_SINGLE);
		pnode_unlink(nc);
		nc = TAILQ_FIRST(&n->childq);
	}
	macro_line(f, "Fc");
}

/*
 * The <arg> element is more complicated than it should be because text
 * nodes are treated like ".Ar foo", but non-text nodes need to be
 * re-sent into the printer (i.e., without the preceding ".Ar").
 * This also handles the case of "repetition" (or in other words, the
 * ellipsis following an argument) and optionality.
 */
static void
pnode_printarg(struct format *f, struct pnode *n)
{
	struct pnode	*nc;
	struct pattr	*a;
	int		 isop, isrep, was_impl;

	isop = 1;
	isrep = was_impl = 0;
	TAILQ_FOREACH(a, &n->attrq, child) {
		if (a->key == ATTRKEY_CHOICE &&
		    (a->val == ATTRVAL_PLAIN || a->val == ATTRVAL_REQ))
			isop = 0;
		else if (a->key == ATTRKEY_REP && a->val == ATTRVAL_REPEAT)
			isrep = 1;
	}
	if (isop) {
		if (f->flags & FMT_IMPL) {
			was_impl = 1;
			macro_open(f, "Oo");
		} else {
			macro_open(f, "Op");
			f->flags |= FMT_IMPL;
		}
	}
	TAILQ_FOREACH(nc, &n->childq, child) {
		if (nc->node == NODE_TEXT)
			macro_open(f, "Ar");
		pnode_print(f, nc);
	}
	if (isrep && f->linestate == LINE_MACRO)
		macro_addarg(f, "...", ARG_SPACE);
	if (isop) {
		if (was_impl)
			macro_open(f, "Oc");
		else
			f->flags &= ~FMT_IMPL;
	}
	pnode_unlinksub(n);
}

static void
pnode_printgroup(struct format *f, struct pnode *n)
{
	struct pnode	*nc;
	struct pattr	*a;
	int		 bar, isop, isrep, was_impl;

	isop = 1;
	isrep = was_impl = 0;
	TAILQ_FOREACH(a, &n->attrq, child) {
		if (a->key == ATTRKEY_CHOICE &&
		    (a->val == ATTRVAL_PLAIN || a->val == ATTRVAL_REQ))
			isop = 0;
		else if (a->key == ATTRKEY_REP && a->val == ATTRVAL_REPEAT)
			isrep = 1;
	}
	if (isop) {
		if (f->flags & FMT_IMPL) {
			was_impl = 1;
			macro_open(f, "Oo");
		} else {
			macro_open(f, "Op");
			f->flags |= FMT_IMPL;
		}
	} else if (isrep) {
		if (f->flags & FMT_IMPL) {
			was_impl = 1;
			macro_open(f, "Bro");
		} else {
			macro_open(f, "Brq");
			f->flags |= FMT_IMPL;
		}
	}
	bar = 0;
	TAILQ_FOREACH(nc, &n->childq, child) {
		if (bar && f->linestate == LINE_MACRO)
			macro_addarg(f, "|", ARG_SPACE);
		pnode_print(f, nc);
		bar = 1;
	}
	if (isop) {
		if (was_impl)
			macro_open(f, "Oc");
		else
			f->flags &= ~FMT_IMPL;
	} else if (isrep) {
		if (was_impl)
			macro_open(f, "Brc");
		else
			f->flags &= ~FMT_IMPL;
	}
	if (isrep && f->linestate == LINE_MACRO)
		macro_addarg(f, "...", ARG_SPACE);
	pnode_unlinksub(n);
}

static void
pnode_printsystemitem(struct format *f, struct pnode *n)
{
	switch (pnode_getattr(n, ATTRKEY_CLASS)) {
	case ATTRVAL_IPADDRESS:
		break;
	case ATTRVAL_SYSTEMNAME:
		macro_open(f, "Pa");
		break;
	case ATTRVAL_EVENT:
	default:
		macro_open(f, "Sy");
		break;
	}
}

static void
pnode_printauthor(struct format *f, struct pnode *n)
{
	struct pnode	*nc, *nn;
	int		 have_contrib, have_name;

	/*
	 * Print <contrib> children up front, before the .An scope,
	 * and figure out whether we a name of a person.
	 */

	have_contrib = have_name = 0;
	TAILQ_FOREACH_SAFE(nc, &n->childq, child, nn) {
		switch (nc->node) {
		case NODE_CONTRIB:
			if (have_contrib)
				print_text(f, ",", 0);
			print_textnode(f, nc);
			pnode_unlink(nc);
			have_contrib = 1;
			break;
		case NODE_PERSONNAME:
			have_name = 1;
			break;
		default:
			break;
		}
	}
	if (TAILQ_FIRST(&n->childq) == NULL)
		return;

	if (have_contrib)
		print_text(f, ":", 0);

	/*
         * If we have a name, print it in the .An scope and leave
         * all other content for child handlers, to print after the
         * scope.  Otherwise, print everything in the scope.
	 */

	macro_open(f, "An");
	TAILQ_FOREACH_SAFE(nc, &n->childq, child, nn) {
		if (nc->node == NODE_PERSONNAME || have_name == 0) {
			macro_addnode(f, nc, ARG_SPACE);
			pnode_unlink(nc);
		}
	}

	/*
	 * If there is an email address,
	 * print it on the same macro line.
	 */

	if ((nc = pnode_findfirst(n, NODE_EMAIL)) != NULL) {
		f->flags |= FMT_CHILD;
		macro_open(f, "Aq Mt");
		macro_addnode(f, nc, ARG_SPACE);
		pnode_unlink(nc);
	}

	/*
	 * If there are still unprinted children, end the scope
	 * with a comma.  Otherwise, leave the scope open in case
	 * a text node follows that starts with closing punctuation.
	 */

	if (TAILQ_FIRST(&n->childq) != NULL) {
		macro_addarg(f, ",", ARG_SPACE);
		macro_close(f);
	}
}

static void
pnode_printxref(struct format *f, struct pnode *n)
{
	const char	*linkend;

	linkend = pnode_getattr_raw(n, ATTRKEY_LINKEND, NULL);
	if (linkend != NULL) {
		macro_open(f, "Sx");
		macro_addarg(f, linkend, ARG_SPACE);
	}
}

static void
pnode_printlink(struct format *f, struct pnode *n)
{
	struct pnode	*nc;
	const char	*uri, *text;

	uri = pnode_getattr_raw(n, ATTRKEY_LINKEND, NULL);
	if (uri != NULL) {
		if (TAILQ_FIRST(&n->childq) != NULL) {
			TAILQ_FOREACH(nc, &n->childq, child)
				pnode_print(f, nc);
			text = "";
		} else if ((text = pnode_getattr_raw(n,
		    ATTRKEY_ENDTERM, NULL)) != NULL) {
			if (f->linestate == LINE_MACRO && f->flags & FMT_ARG)
				macro_addarg(f, text, ARG_SPACE);
			else
				print_text(f, text, ARG_SPACE);
		}
		if (text != NULL) {
			if (f->flags & FMT_IMPL)
				macro_open(f, "Po");
			else {
				macro_open(f, "Pq");
				f->flags |= FMT_CHILD;
			}
		}
		macro_open(f, "Sx");
		macro_addarg(f, uri, ARG_SPACE);
		if (text != NULL && f->flags & FMT_IMPL)
			macro_open(f, "Pc");
		pnode_unlinksub(n);
		return;
	}
	uri = pnode_getattr_raw(n, ATTRKEY_XLINK_HREF, NULL);
	if (uri == NULL)
		uri = pnode_getattr_raw(n, ATTRKEY_URL, NULL);
	if (uri != NULL) {
		macro_open(f, "Lk");
		macro_addarg(f, uri, ARG_SPACE | ARG_SINGLE);
		if (TAILQ_FIRST(&n->childq) != NULL)
			macro_addnode(f, n, ARG_SPACE | ARG_SINGLE);
		pnode_unlinksub(n);
	}
}

static void
pnode_printolink(struct format *f, struct pnode *n)
{
	const char	*uri, *ptr, *local;

	uri = pnode_getattr_raw(n, ATTRKEY_TARGETDOC, NULL);
	ptr = pnode_getattr_raw(n, ATTRKEY_TARGETPTR, NULL);
	local = pnode_getattr_raw(n, ATTRKEY_LOCALINFO, NULL);
	if (uri == NULL) {
		uri = ptr;
		ptr = NULL;
	}
	if (uri == NULL) {
		uri = local;
		local = NULL;
	}
	if (uri == NULL)
		return;

	macro_open(f, "Lk");
	macro_addarg(f, uri, ARG_SPACE | ARG_SINGLE);
	macro_addnode(f, n, ARG_SPACE | ARG_SINGLE);
	if (ptr != NULL || local != NULL) {
		macro_close(f);
		macro_open(f, "Pq");
		if (ptr != NULL)
			macro_addarg(f, ptr, ARG_SPACE);
		if (local != NULL)
			macro_addarg(f, local, ARG_SPACE);
	}
	pnode_unlinksub(n);
}

static void
pnode_printprologue(struct format *f, struct pnode *root)
{
	struct pnode	*name, *nc;

	nc = TAILQ_FIRST(&root->childq);
	assert(nc->node == NODE_DATE);
	macro_nodeline(f, "Dd", nc, 0);
	pnode_unlink(nc);

	macro_open(f, "Dt");
	name = TAILQ_FIRST(&root->childq);
	assert(name->node == NODE_REFENTRYTITLE);
	macro_addnode(f, name, ARG_SPACE | ARG_SINGLE | ARG_UPPER);
	TAILQ_REMOVE(&root->childq, name, child);
	name->parent = NULL;
	nc = TAILQ_FIRST(&root->childq);
	assert (nc->node == NODE_MANVOLNUM);
	macro_addnode(f, nc, ARG_SPACE | ARG_SINGLE);
	pnode_unlink(nc);

	macro_line(f, "Os");

	nc = TAILQ_FIRST(&root->childq);
	if (nc != NULL && nc->node == NODE_TITLE) {
		macro_line(f, "Sh NAME");
		macro_nodeline(f, "Nm", name, ARG_SINGLE);
		macro_nodeline(f, "Nd", nc, 0);
		pnode_unlink(nc);
	}
	pnode_unlink(name);
	f->parastate = PARA_HAVE;
}

/*
 * We can have multiple <term> elements within a <varlistentry>, which
 * we should comma-separate as list headers.
 */
static void
pnode_printvarlistentry(struct format *f, struct pnode *n)
{
	struct pnode	*nc, *nn, *ncc;
	int		 comma;

	macro_open(f, "It");
	f->parastate = PARA_HAVE;
	f->flags |= FMT_IMPL;
	comma = -1;
	TAILQ_FOREACH_SAFE(nc, &n->childq, child, nn) {
		if (nc->node != NODE_TERM && nc->node != NODE_GLOSSTERM)
			continue;
		if (comma != -1) {
			switch (f->linestate) {
			case LINE_NEW:
				break;
			case LINE_TEXT:
				print_text(f, ",", 0);
				break;
			case LINE_MACRO:
				macro_addarg(f, ",", comma);
				break;
			}
		}
		f->parastate = PARA_HAVE;
		comma = (ncc = TAILQ_FIRST(&nc->childq)) == NULL ||
		    pnode_class(ncc->node) == CLASS_TEXT ? 0 : ARG_SPACE;
		pnode_print(f, nc);
		pnode_unlink(nc);
	}
	macro_close(f);
	f->parastate = PARA_HAVE;
	while ((nc = TAILQ_FIRST(&n->childq)) != NULL) {
		pnode_print(f, nc);
		pnode_unlink(nc);
	}
	macro_close(f);
	f->parastate = PARA_HAVE;
}

static void
pnode_printtitle(struct format *f, struct pnode *n)
{
	struct pnode	*nc, *nn;

	TAILQ_FOREACH_SAFE(nc, &n->childq, child, nn) {
		if (nc->node == NODE_TITLE) {
			if (f->parastate == PARA_MID)
				f->parastate = PARA_WANT;
			macro_nodeline(f, "Sy", nc, 0);
			pnode_unlink(nc);
		}
	}
}

static void
pnode_printrow(struct format *f, struct pnode *n)
{
	struct pnode	*nc;

	macro_line(f, "Bl -dash -compact");
	TAILQ_FOREACH(nc, &n->childq, child) {
		macro_line(f, "It");
		pnode_print(f, nc);
	}
	macro_line(f, "El");
	pnode_unlink(n);
}

static void
pnode_printtgroup1(struct format *f, struct pnode *n)
{
	struct pnode	*nc;

	macro_line(f, "Bl -bullet -compact");
	while ((nc = pnode_findfirst(n, NODE_ENTRY)) != NULL) {
		macro_line(f, "It");
		f->parastate = PARA_HAVE;
		pnode_print(f, nc);
		f->parastate = PARA_HAVE;
		pnode_unlink(nc);
	}
	macro_line(f, "El");
	pnode_unlinksub(n);
}

static void
pnode_printtgroup2(struct format *f, struct pnode *n)
{
	struct pnode	*nr, *ne;

	f->parastate = PARA_HAVE;
	macro_line(f, "Bl -tag -width Ds");
	while ((nr = pnode_findfirst(n, NODE_ROW)) != NULL) {
		if ((ne = pnode_findfirst(n, NODE_ENTRY)) == NULL)
			break;
		macro_open(f, "It");
		f->flags |= FMT_IMPL;
		f->parastate = PARA_HAVE;
		pnode_print(f, ne);
		macro_close(f);
		pnode_unlink(ne);
		f->parastate = PARA_HAVE;
		pnode_print(f, nr);
		f->parastate = PARA_HAVE;
		pnode_unlink(nr);
	}
	macro_line(f, "El");
	f->parastate = PARA_WANT;
	pnode_unlinksub(n);
}

static void
pnode_printtgroup(struct format *f, struct pnode *n)
{
	struct pnode	*nc;

	switch (atoi(pnode_getattr_raw(n, ATTRKEY_COLS, "0"))) {
	case 1:
		pnode_printtgroup1(f, n);
		return;
	case 2:
		pnode_printtgroup2(f, n);
		return;
	default:
		break;
	}

	f->parastate = PARA_HAVE;
	macro_line(f, "Bl -ohang");
	while ((nc = pnode_findfirst(n, NODE_ROW)) != NULL) {
		macro_line(f, "It Table Row");
		pnode_printrow(f, nc);
	}
	macro_line(f, "El");
	f->parastate = PARA_WANT;
	pnode_unlinksub(n);
}

static void
pnode_printlist(struct format *f, struct pnode *n)
{
	struct pnode	*nc;

	pnode_printtitle(f, n);
	f->parastate = PARA_HAVE;
	macro_argline(f, "Bl",
	    n->node == NODE_ORDEREDLIST ? "-enum" : "-bullet");
	TAILQ_FOREACH(nc, &n->childq, child) {
		macro_line(f, "It");
		f->parastate = PARA_HAVE;
		pnode_print(f, nc);
		f->parastate = PARA_HAVE;
	}
	macro_line(f, "El");
	f->parastate = PARA_WANT;
	pnode_unlinksub(n);
}

static void
pnode_printvariablelist(struct format *f, struct pnode *n)
{
	struct pnode	*nc;

	pnode_printtitle(f, n);
	f->parastate = PARA_HAVE;
	macro_line(f, "Bl -tag -width Ds");
	TAILQ_FOREACH(nc, &n->childq, child) {
		if (nc->node == NODE_VARLISTENTRY)
			pnode_printvarlistentry(f, nc);
		else
			macro_nodeline(f, "It", nc, 0);
	}
	macro_line(f, "El");
	f->parastate = PARA_WANT;
	pnode_unlinksub(n);
}

/*
 * Print a parsed node (or ignore it--whatever).
 * This is a recursive function.
 * FIXME: if we're in a literal context (<screen> or <programlisting> or
 * whatever), don't print inline macros.
 */
static void
pnode_print(struct format *f, struct pnode *n)
{
	struct pnode	*nc, *nn;
	int		 was_impl;

	if (n == NULL)
		return;

	if (n->flags & NFLAG_LINE &&
	    (f->nofill || (f->flags & (FMT_ARG | FMT_IMPL)) == 0))
		macro_close(f);

	was_impl = f->flags & FMT_IMPL;
	if (n->flags & NFLAG_SPC)
		f->flags &= ~FMT_NOSPC;
	else
		f->flags |= FMT_NOSPC;

	switch (n->node) {
	case NODE_ARG:
		pnode_printarg(f, n);
		break;
	case NODE_AUTHOR:
		pnode_printauthor(f, n);
		break;
	case NODE_AUTHORGROUP:
		macro_line(f, "An -split");
		break;
	case NODE_BLOCKQUOTE:
		f->parastate = PARA_HAVE;
		macro_line(f, "Bd -ragged -offset indent");
		f->parastate = PARA_HAVE;
		break;
	case NODE_CITEREFENTRY:
		pnode_printciterefentry(f, n);
		break;
	case NODE_CITETITLE:
		macro_open(f, "%T");
		break;
	case NODE_COMMAND:
		macro_open(f, "Nm");
		break;
	case NODE_CONSTANT:
		macro_open(f, "Dv");
		break;
	case NODE_COPYRIGHT:
		print_text(f, "Copyright", ARG_SPACE);
		fputs(" \\(co", stdout);
		break;
	case NODE_EDITOR:
		print_text(f, "editor:", ARG_SPACE);
		pnode_printauthor(f, n);
		break;
	case NODE_EMAIL:
		if (was_impl)
			macro_open(f, "Ao Mt");
		else {
			macro_open(f, "Aq Mt");
			f->flags |= FMT_IMPL;
		}
		break;
	case NODE_EMPHASIS:
	case NODE_FIRSTTERM:
	case NODE_GLOSSTERM:
		if ((nc = TAILQ_FIRST(&n->childq)) != NULL &&
		    pnode_class(nc->node) < CLASS_LINE)
			macro_open(f, "Em");
		if (n->node == NODE_GLOSSTERM)
			f->parastate = PARA_HAVE;
		break;
	case NODE_ENVAR:
		macro_open(f, "Ev");
		break;
	case NODE_ERRORNAME:
		macro_open(f, "Er");
		break;
	case NODE_FILENAME:
		macro_open(f, "Pa");
		break;
	case NODE_FOOTNOTE:
		macro_line(f, "Bo");
		f->parastate = PARA_HAVE;
		break;
	case NODE_FUNCTION:
		macro_open(f, "Fn");
		break;
	case NODE_FUNCPROTOTYPE:
		pnode_printfuncprototype(f, n);
		break;
	case NODE_FUNCSYNOPSISINFO:
		macro_open(f, "Fd");
		break;
	case NODE_IMAGEDATA:
		pnode_printimagedata(f, n);
		break;
	case NODE_INFORMALEQUATION:
		f->parastate = PARA_HAVE;
		macro_line(f, "Bd -ragged -offset indent");
		f->parastate = PARA_HAVE;
		/* FALLTHROUGH */
	case NODE_INLINEEQUATION:
		macro_line(f, "EQ");
		break;
	case NODE_ITEMIZEDLIST:
		pnode_printlist(f, n);
		break;
	case NODE_GROUP:
		pnode_printgroup(f, n);
		break;
	case NODE_KEYSYM:
	case NODE_PRODUCTNAME:
		macro_open(f, "Sy");
		break;
	case NODE_LINK:
		pnode_printlink(f, n);
		break;
	case NODE_LITERAL:
		if (n->parent != NULL && n->parent->node == NODE_QUOTE)
			macro_open(f, "Li");
		else if (was_impl)
			macro_open(f, "So Li");
		else {
			macro_open(f, "Ql");
			f->flags |= FMT_IMPL;
		}
		break;
	case NODE_LITERALLAYOUT:
		macro_close(f);
		f->parastate = PARA_HAVE;
		macro_argline(f, "Bd", pnode_getattr(n, ATTRKEY_CLASS) ==
		    ATTRVAL_MONOSPACED ? "-literal" : "-unfilled");
		f->parastate = PARA_HAVE;
		break;
	case NODE_MARKUP:
		macro_open(f, "Ic");
		break;
	case NODE_MML_MFENCED:
		pnode_printmathfenced(f, n);
		break;
	case NODE_MML_MROW:
	case NODE_MML_MI:
	case NODE_MML_MN:
	case NODE_MML_MO:
		if (TAILQ_EMPTY(&n->childq))
			break;
		fputs(" { ", stdout);
		break;
	case NODE_MML_MFRAC:
	case NODE_MML_MSUB:
	case NODE_MML_MSUP:
		pnode_printmath(f, n);
		break;
	case NODE_OLINK:
		pnode_printolink(f, n);
		break;
	case NODE_OPTION:
		if ((nc = TAILQ_FIRST(&n->childq)) != NULL &&
		    pnode_class(nc->node) < CLASS_LINE)
			macro_open(f, "Fl");
		break;
	case NODE_ORDEREDLIST:
		pnode_printlist(f, n);
		break;
	case NODE_PARA:
		if (f->parastate == PARA_MID)
			f->parastate = PARA_WANT;
		break;
	case NODE_PARAMDEF:
	case NODE_PARAMETER:
		/* More often, these appear inside NODE_FUNCPROTOTYPE. */
		macro_open(f, "Fa");
		macro_addnode(f, n, ARG_SPACE | ARG_SINGLE);
		pnode_unlinksub(n);
		break;
	case NODE_QUOTE:
		if ((nc = TAILQ_FIRST(&n->childq)) != NULL &&
		    nc->node == NODE_FILENAME &&
		    TAILQ_NEXT(nc, child) == NULL) {
			if (n->flags & NFLAG_SPC)
				nc->flags |= NFLAG_SPC;
		} else if (was_impl)
			macro_open(f, "Do");
		else {
			macro_open(f, "Dq");
			f->flags |= FMT_IMPL;
		}
		break;
	case NODE_PROGRAMLISTING:
	case NODE_SCREEN:
	case NODE_SYNOPSIS:
		f->parastate = PARA_HAVE;
		macro_line(f, "Bd -literal");
		f->parastate = PARA_HAVE;
		break;
	case NODE_SYSTEMITEM:
		pnode_printsystemitem(f, n);
		break;
	case NODE_REFNAME:
		/* More often, these appear inside NODE_REFNAMEDIV. */
		macro_open(f, "Nm");
		break;
	case NODE_REFNAMEDIV:
		pnode_printrefnamediv(f, n);
		break;
	case NODE_REFPURPOSE:
		macro_open(f, "Nd");
		break;
	case NODE_REFSYNOPSISDIV:
		pnode_printrefsynopsisdiv(f, n);
		break;
	case NODE_SECTION:
	case NODE_SIMPLESECT:
	case NODE_APPENDIX:
	case NODE_NOTE:
		pnode_printsection(f, n);
		break;
	case NODE_REPLACEABLE:
		macro_open(f, "Ar");
		break;
	case NODE_SBR:
		if (f->parastate == PARA_MID)
			macro_line(f, "br");
		break;
	case NODE_SUBSCRIPT:
		if (f->linestate == LINE_MACRO)
			macro_addarg(f, "_", 0);
		else
			print_text(f, "_", 0);
		if ((nc = TAILQ_FIRST(&n->childq)) != NULL)
			nc->flags &= ~(NFLAG_LINE | NFLAG_SPC);
		break;
	case NODE_SUPERSCRIPT:
		fputs("\\(ha", stdout);
		if ((nc = TAILQ_FIRST(&n->childq)) != NULL)
			nc->flags &= ~(NFLAG_LINE | NFLAG_SPC);
		break;
	case NODE_TEXT:
	case NODE_ESCAPE:
		pnode_printtext(f, n);
		break;
	case NODE_TGROUP:
		pnode_printtgroup(f, n);
		break;
	case NODE_TITLE:
	case NODE_SUBTITLE:
		if (f->parastate == PARA_MID)
			f->parastate = PARA_WANT;
		macro_nodeline(f, "Sy", n, 0);
		pnode_unlinksub(n);
		break;
	case NODE_TYPE:
		macro_open(f, "Vt");
		break;
	case NODE_VARIABLELIST:
		pnode_printvariablelist(f, n);
		break;
	case NODE_VARNAME:
		macro_open(f, "Va");
		break;
	case NODE_VOID:
		print_text(f, "void", ARG_SPACE);
		break;
	case NODE_XREF:
		pnode_printxref(f, n);
		break;
	case NODE_CAUTION:
	case NODE_LEGALNOTICE:
	case NODE_PREFACE:
	case NODE_TIP:
	case NODE_WARNING:
		abort();
	default:
		break;
	}

	if (pnode_class(n->node) == CLASS_NOFILL)
		f->nofill++;

	TAILQ_FOREACH(nc, &n->childq, child)
		pnode_print(f, nc);

	switch (n->node) {
	case NODE_EMAIL:
		if (was_impl) {
			f->flags &= ~FMT_NOSPC;
			macro_open(f, "Ac");
		} else
			f->flags &= ~FMT_IMPL;
		break;
	case NODE_ESCAPE:
	case NODE_TERM:
	case NODE_TEXT:
		/* Accept more arguments to the previous macro. */
		return;
	case NODE_FOOTNOTE:
		f->parastate = PARA_HAVE;
		macro_line(f, "Bc");
		break;
	case NODE_GLOSSTERM:
		f->parastate = PARA_HAVE;
		break;
	case NODE_INFORMALEQUATION:
		macro_line(f, "EN");
		macro_line(f, "Ed");
		break;
	case NODE_INLINEEQUATION:
		macro_line(f, "EN");
		break;
	case NODE_LITERAL:
		if (n->parent != NULL && n->parent->node == NODE_QUOTE)
			/* nothing */;
		else if (was_impl) {
			f->flags &= ~FMT_NOSPC;
			macro_open(f, "Sc");
		} else
			f->flags &= ~FMT_IMPL;
		break;
	case NODE_MEMBER:
		if ((nn = TAILQ_NEXT(n, child)) != NULL &&
		    nn->node != NODE_MEMBER)
			nn = NULL;
		switch (f->linestate) {
		case LINE_TEXT:
			if (nn != NULL)
				print_text(f, ",", 0);
			break;
		case LINE_MACRO:
			if (nn != NULL)
				macro_addarg(f, ",", ARG_SPACE);
			macro_close(f);
			break;
		case LINE_NEW:
			break;
		}
		break;
	case NODE_MML_MROW:
	case NODE_MML_MI:
	case NODE_MML_MN:
	case NODE_MML_MO:
		if (TAILQ_EMPTY(&n->childq))
			break;
		fputs(" } ", stdout);
		break;
	case NODE_PARA:
		if (f->parastate == PARA_MID)
			f->parastate = PARA_WANT;
		break;
	case NODE_QUOTE:
		if ((nc = TAILQ_FIRST(&n->childq)) != NULL &&
		    nc->node == NODE_FILENAME &&
		    TAILQ_NEXT(nc, child) == NULL)
			/* nothing */;
		else if (was_impl) {
			f->flags &= ~FMT_NOSPC;
			macro_open(f, "Dc");
		} else
			f->flags &= ~FMT_IMPL;
		break;
	case NODE_SECTION:
	case NODE_SIMPLESECT:
	case NODE_APPENDIX:
	case NODE_NOTE:
		if (n->parent != NULL)
			f->level--;
		break;
	case NODE_BLOCKQUOTE:
	case NODE_LITERALLAYOUT:
	case NODE_PROGRAMLISTING:
	case NODE_SCREEN:
	case NODE_SYNOPSIS:
		f->parastate = PARA_HAVE;
		macro_line(f, "Ed");
		f->parastate = PARA_WANT;
		break;
	case NODE_TITLE:
	case NODE_SUBTITLE:
		f->parastate = PARA_WANT;
		break;
	case NODE_YEAR:
		if ((nn = TAILQ_NEXT(n, child)) != NULL &&
		    nn->node == NODE_YEAR &&
		    f->linestate == LINE_TEXT) {
			print_text(f, ",", 0);
			nn->flags |= NFLAG_SPC;
			if ((nc = TAILQ_FIRST(&nn->childq)) != NULL)
				nc->flags |= NFLAG_SPC;
		}
	default:
		break;
	}
	f->flags &= ~FMT_ARG;
	if (pnode_class(n->node) == CLASS_NOFILL)
		f->nofill--;
}

void
ptree_print_mdoc(struct ptree *tree)
{
	struct format	 formatter;

	formatter.level = formatter.nofill = 0;
	formatter.linestate = LINE_NEW;
	formatter.parastate = PARA_HAVE;
	pnode_printprologue(&formatter, tree->root);
	pnode_print(&formatter, tree->root);
	if (formatter.linestate != LINE_NEW)
		putchar('\n');
}
