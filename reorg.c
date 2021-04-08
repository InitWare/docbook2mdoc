/* $Id: reorg.c,v 1.6 2019/05/01 11:03:31 schwarze Exp $ */
/*
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
#include "string.h"

#include "node.h"
#include "reorg.h"

/*
 * The implementation of the tree reorganizer.
 */

static void
reorg_root(struct pnode *root, const char *sec)
{
	struct pnode	*date, *info, *name, *vol, *nc;

	if (root == NULL)
		return;

	/* Collect prologue information. */

	if ((date = pnode_takefirst(root, NODE_PUBDATE)) == NULL &&
	    (date = pnode_takefirst(root, NODE_DATE)) == NULL) {
		date = pnode_alloc(NULL);
		pnode_alloc_text(date, "$Mdocdate" "$");
	}
	date->node = NODE_DATE;
	date->parent = root;

	name = vol = NULL;
	if ((nc = pnode_findfirst(root, NODE_REFMETA)) != NULL) {
		name = pnode_takefirst(nc, NODE_REFENTRYTITLE);
		vol = pnode_takefirst(nc, NODE_MANVOLNUM);
	}
	if (name == NULL) {
		name = pnode_alloc(NULL);
		name->node = NODE_REFENTRYTITLE;
		name->parent = root;
		pnode_alloc_text(name,
		    pnode_getattr_raw(root, ATTRKEY_ID, "UNKNOWN"));
	}
	if (vol == NULL || sec != NULL) {
		pnode_unlink(vol);
		vol = pnode_alloc(NULL);
		vol->node = NODE_MANVOLNUM;
		vol->parent = root;
		pnode_alloc_text(vol, sec == NULL ? "1" : sec);
	}

	/* Insert prologue information at the beginning. */

	if (pnode_findfirst(root, NODE_REFNAMEDIV) == NULL &&
	    ((info = pnode_findfirst(root, NODE_BOOKINFO)) != NULL ||
	     (info = pnode_findfirst(root, NODE_REFENTRYINFO)) != NULL)) {
		if ((nc = pnode_takefirst(info, NODE_ABSTRACT)) != NULL)
			TAILQ_INSERT_HEAD(&root->childq, nc, child);
		if ((nc = pnode_takefirst(info, NODE_TITLE)) != NULL)
			TAILQ_INSERT_HEAD(&root->childq, nc, child);
	}
	TAILQ_INSERT_HEAD(&root->childq, vol, child);
	TAILQ_INSERT_HEAD(&root->childq, name, child);
	TAILQ_INSERT_HEAD(&root->childq, date, child);
}

static void
reorg_refentry(struct pnode *n)
{
	struct pnode	*info, *meta, *nc, *title;
	struct pnode	*match, *later;

	/* Collect nodes that remained behind from the prologue. */

	meta = NULL;
	info = pnode_takefirst(n, NODE_BOOKINFO);
	if (info != NULL && TAILQ_FIRST(&info->childq) == NULL) {
		pnode_unlink(info);
		info = NULL;
	}
	if (info == NULL) {
		info = pnode_takefirst(n, NODE_REFENTRYINFO);
		if (info != NULL && TAILQ_FIRST(&info->childq) == NULL) {
			pnode_unlink(info);
			info = NULL;
		}
		if (info == NULL)
			info = pnode_takefirst(n, NODE_INFO);
		meta = pnode_takefirst(n, NODE_REFMETA);
		if (meta != NULL && TAILQ_FIRST(&meta->childq) == NULL) {
			pnode_unlink(meta);
			meta = NULL;
		}
	}
	if (info == NULL && meta == NULL)
		return;

	/*
	 * Find the best place to put this information.
	 * Use the last existing AUTHORS node, if any.
	 * Otherwise, put it behind all standard sections that
	 * conventionally precede AUTHORS, and also behind any
	 * non-standard sections that follow the last of these,
	 * but before the next standard section.
	 */

	match = later = NULL;
	TAILQ_FOREACH(nc, &n->childq, child) {
		switch (nc->node) {
		case NODE_REFENTRY:
		case NODE_REFNAMEDIV:
		case NODE_REFSYNOPSISDIV:
			later = NULL;
			continue;
		case NODE_APPENDIX:
		case NODE_INDEX:
			if (later == NULL)
				later = nc;
			continue;
		default:
			break;
		}
		if ((title = pnode_findfirst(nc, NODE_TITLE)) == NULL ||
		    (title = TAILQ_FIRST(&title->childq)) == NULL ||
		    title->node != NODE_TEXT)
			continue;
		if (strcasecmp(title->b, "AUTHORS") == 0 ||
		    strcasecmp(title->b, "AUTHOR") == 0)
			match = nc;
		else if (strcasecmp(title->b, "NAME") == 0 ||
		    strcasecmp(title->b, "SYNOPSIS") == 0 ||
		    strcasecmp(title->b, "DESCRIPTION") == 0 ||
		    strcasecmp(title->b, "RETURN VALUES") == 0 ||
		    strcasecmp(title->b, "ENVIRONMENT") == 0 ||
		    strcasecmp(title->b, "FILES") == 0 ||
		    strcasecmp(title->b, "EXIT STATUS") == 0 ||
		    strcasecmp(title->b, "EXAMPLES") == 0 ||
		    strcasecmp(title->b, "DIAGNOSTICS") == 0 ||
		    strcasecmp(title->b, "ERRORS") == 0 ||
		    strcasecmp(title->b, "SEE ALSO") == 0 ||
		    strcasecmp(title->b, "STANDARDS") == 0 ||
		    strcasecmp(title->b, "HISTORY") == 0)
			later = NULL;
		else if ((strcasecmp(title->b, "CAVEATS") == 0 ||
		    strcasecmp(title->b, "BUGS") == 0) &&
		    later == NULL)
			later = nc;
	}

	/*
	 * If no AUTHORS section was found, create one from scratch,
	 * and insert that at the place selected earlier.
	 */

	if (match == NULL) {
		match = pnode_alloc(NULL);
		match->node = NODE_SECTION;
		match->flags |= NFLAG_SPC;
		match->parent = n;
		nc = pnode_alloc(match);
		nc->node = NODE_TITLE;
		nc->flags |= NFLAG_SPC;
		nc = pnode_alloc_text(nc, "AUTHORS");
		nc->flags |= NFLAG_SPC;
		if (later == NULL)
			TAILQ_INSERT_TAIL(&n->childq, match, child);
		else
			TAILQ_INSERT_BEFORE(later, match, child);
	}

	/*
	 * Dump the stuff excised at the beginning
	 * into this AUTHORS section.
	 */

	if (info != NULL)
		TAILQ_INSERT_TAIL(&match->childq, info, child);
	if (meta != NULL)
		TAILQ_INSERT_TAIL(&match->childq, meta, child);
}

static void
default_title(struct pnode *n, const char *title)
{
	struct pnode	*nc;

	if (n->parent == NULL)
		return;

	TAILQ_FOREACH(nc, &n->childq, child)
		if (nc->node == NODE_TITLE)
			return;

	nc = pnode_alloc(NULL);
	nc->node = NODE_TITLE;
	nc->parent = n;
	TAILQ_INSERT_HEAD(&n->childq, nc, child);
	pnode_alloc_text(nc, title);
}

static void
reorg_function(struct pnode *n)
{
	struct pnode	*nc;
	size_t		 sz;

	if ((nc = TAILQ_FIRST(&n->childq)) != NULL &&
	    nc->node == NODE_TEXT &&
	    TAILQ_NEXT(nc, child) == NULL &&
	    (sz = strlen(nc->b)) > 2 &&
	    nc->b[sz - 2] == '(' && nc->b[sz - 1] == ')')
		nc->b[sz - 2] = '\0';
}

static void
reorg_recurse(struct pnode *n)
{
	struct pnode	*nc;

	if (n == NULL)
		return;

	switch (n->node) {
	case NODE_ABSTRACT:
		default_title(n, "Abstract");
		n->node = NODE_SECTION;
		break;
	case NODE_APPENDIX:
		if (n->parent == NULL)
			reorg_refentry(n);
		default_title(n, "Appendix");
		break;
	case NODE_CAUTION:
		default_title(n, "Caution");
		n->node = NODE_NOTE;
		break;
	case NODE_FUNCTION:
		reorg_function(n);
		break;
	case NODE_LEGALNOTICE:
		default_title(n, "Legal Notice");
		n->node = NODE_SIMPLESECT;
		break;
	case NODE_NOTE:
		default_title(n, "Note");
		break;
	case NODE_PREFACE:
		if (n->parent == NULL)
			reorg_refentry(n);
		default_title(n, "Preface");
		n->node = NODE_SECTION;
		break;
	case NODE_REFENTRY:
		reorg_refentry(n);
		break;
	case NODE_SECTION:
		if (n->parent == NULL)
			reorg_refentry(n);
		/* FALLTHROUGH */
	case NODE_SIMPLESECT:
		default_title(n, "Untitled");
		break;
	case NODE_TIP:
		default_title(n, "Tip");
		n->node = NODE_NOTE;
		break;
	case NODE_WARNING:
		default_title(n, "Warning");
		n->node = NODE_NOTE;
		break;
	default:
		break;
	}

	TAILQ_FOREACH(nc, &n->childq, child)
		reorg_recurse(nc);
}

void
ptree_reorg(struct ptree *tree, const char *sec)
{
	reorg_root(tree->root, sec);
	reorg_recurse(tree->root);
}
