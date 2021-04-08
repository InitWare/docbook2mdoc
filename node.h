/* $Id: node.h,v 1.37 2019/05/01 12:52:05 schwarze Exp $ */
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
#include <sys/queue.h>

/*
 * The interface of the DocBook syntax tree.
 */

/*
 * How the output from each node behaves
 * with respect to mdoc(7) structure.
 */
enum	nodeclass {
	CLASS_VOID = 0,	/* No output at all, children are skipped. */
	CLASS_TEXT,	/* Node prints only text, no macros. */
	CLASS_TRANS,	/* Transparent: only the children are shown. */
	CLASS_LINE,	/* Generates only simple in-line macros. */
	CLASS_ENCL,	/* Explicit or implicit partial blocks. */
	CLASS_BLOCK,	/* Output linebreak before and after. */
	CLASS_NOFILL	/* Block preserving leading whitespace. */
};

/*
 * All node types used for formatting purposes.
 * More DocBook XML elements are recognized, but remapped or discarded.
 */
enum	nodeid {
	NODE_ABSTRACT,
	NODE_APPENDIX,
	NODE_ARG,
	NODE_AUTHOR,
	NODE_AUTHORGROUP,
	NODE_BLOCKQUOTE,
	NODE_BOOKINFO,
	NODE_CAUTION,
	NODE_CITEREFENTRY,
	NODE_CITETITLE,
	NODE_CMDSYNOPSIS,
	NODE_COLSPEC,
	NODE_COMMAND,
	NODE_CONSTANT,
	NODE_CONTRIB,
	NODE_COPYRIGHT,
	NODE_DATE,
	NODE_DOCTYPE,
	NODE_EDITOR,
	NODE_EMAIL,
	NODE_EMPHASIS,
	NODE_ENTITY,
	NODE_ENTRY,
	NODE_ENVAR,
	NODE_ERRORNAME,
	NODE_FIELDSYNOPSIS,
	NODE_FILENAME,
	NODE_FIRSTTERM,
	NODE_FOOTNOTE,
	NODE_FUNCDEF,
	NODE_FUNCPARAMS,
	NODE_FUNCPROTOTYPE,
	NODE_FUNCSYNOPSIS,
	NODE_FUNCSYNOPSISINFO,
	NODE_FUNCTION,
	NODE_GLOSSTERM,
	NODE_GROUP,
	NODE_IMAGEDATA,
	NODE_INCLUDE,
	NODE_INDEX,
	NODE_INFO,
	NODE_INFORMALEQUATION,
	NODE_INLINEEQUATION,
	NODE_ITEMIZEDLIST,
	NODE_KEYSYM,
	NODE_LEGALNOTICE,
	NODE_LINK,
	NODE_LISTITEM,
	NODE_LITERAL,
	NODE_LITERALLAYOUT,
	NODE_MANVOLNUM,
	NODE_MARKUP,
	NODE_MEMBER,
	NODE_MML_MATH,
	NODE_MML_MFENCED,
	NODE_MML_MFRAC,
	NODE_MML_MI,
	NODE_MML_MN,
	NODE_MML_MO,
	NODE_MML_MROW,
	NODE_MML_MSUB,
	NODE_MML_MSUP,
	NODE_MODIFIER,
	NODE_NOTE,
	NODE_OLINK,
	NODE_OPTION,
	NODE_ORDEREDLIST,
	NODE_PARA,
	NODE_PARAMDEF,
	NODE_PARAMETER,
	NODE_PERSONNAME,
	NODE_PREFACE,
	NODE_PRODUCTNAME,
	NODE_PROGRAMLISTING,
	NODE_PROMPT,
	NODE_PUBDATE,
	NODE_QUOTE,
	NODE_REFCLASS,
	NODE_REFDESCRIPTOR,
	NODE_REFENTRY,
	NODE_REFENTRYINFO,
	NODE_REFENTRYTITLE,
	NODE_REFMETA,
	NODE_REFMETAINFO,
	NODE_REFMISCINFO,
	NODE_REFNAME,
	NODE_REFNAMEDIV,
	NODE_REFPURPOSE,
	NODE_REFSYNOPSISDIV,
	NODE_REPLACEABLE,
	NODE_ROW,
	NODE_SBR,
	NODE_SCREEN,
	NODE_SECTION,
	NODE_SIMPLELIST,
	NODE_SIMPLESECT,
	NODE_SPANSPEC,
	NODE_SUBSCRIPT,
	NODE_SUBTITLE,
	NODE_SUPERSCRIPT,
	NODE_SYNOPSIS,
	NODE_SYSTEMITEM,
	NODE_TABLE,
	NODE_TBODY,
	NODE_TERM,
	NODE_TFOOT,
	NODE_TGROUP,
	NODE_THEAD,
	NODE_TIP,
	NODE_TITLE,
	NODE_TYPE,
	NODE_VARIABLELIST,
	NODE_VARLISTENTRY,
	NODE_VARNAME,
	NODE_VOID,
	NODE_WARNING,
	NODE_WORDASWORD,
	NODE_XREF,
	NODE_YEAR,
	NODE_UNKNOWN,
	NODE_TEXT,
	NODE_ESCAPE,
	NODE_IGNORE,
	NODE_DELETE,
	NODE_DELETE_WARN
};

/*
 * All recognised attribute keys.
 * Other attributes are discarded.
 */
enum	attrkey {
	/* Alpha-order... */
	ATTRKEY_CHOICE = 0,
	ATTRKEY_CLASS,
	ATTRKEY_CLOSE,
	ATTRKEY_COLS,
	ATTRKEY_DEFINITION,
	ATTRKEY_ENDTERM,
	ATTRKEY_ENTITYREF,
	ATTRKEY_FILEREF,
	ATTRKEY_HREF,
	ATTRKEY_ID,
	ATTRKEY_LINKEND,
	ATTRKEY_LOCALINFO,
	ATTRKEY_NAME,
	ATTRKEY_OPEN,
	ATTRKEY_PUBLIC,
	ATTRKEY_REP,
	ATTRKEY_SYSTEM,
	ATTRKEY_TARGETDOC,
	ATTRKEY_TARGETPTR,
	ATTRKEY_URL,
	ATTRKEY_XLINK_HREF,
	ATTRKEY__MAX
};

/*
 * All explicitly recognised attribute values.
 * If an attribute has ATTRVAL__MAX, it is treated as free-form.
 */
enum	attrval {
	/* Alpha-order... */
	ATTRVAL_EVENT,
	ATTRVAL_IPADDRESS,
	ATTRVAL_MONOSPACED,
	ATTRVAL_NOREPEAT,
	ATTRVAL_OPT,
	ATTRVAL_PLAIN,
	ATTRVAL_REPEAT,
	ATTRVAL_REQ,
	ATTRVAL_SYSTEMNAME,
	ATTRVAL__MAX
};

TAILQ_HEAD(pnodeq, pnode);
TAILQ_HEAD(pattrq, pattr);

/*
 * One DocBook XML element attribute.
 */
struct	pattr {
	enum attrkey	 key;
	enum attrval	 val;
	char		*rawval;
	TAILQ_ENTRY(pattr) child;
};

/*
 * One DocBook XML element.
 */
struct	pnode {
	enum nodeid	 node;     /* Node type. */
	char		*b;        /* String value. */
	struct pnode	*parent;   /* Parent node or NULL. */
	int		 flags;
#define	NFLAG_LINE	 (1 << 0)  /* New line before this node. */
#define	NFLAG_SPC	 (1 << 1)  /* Whitespace before this node. */
	struct pnodeq	 childq;   /* Queue of children. */
	struct pattrq	 attrq;    /* Attributes of the node. */
	TAILQ_ENTRY(pnode) child;
};

/*
 * The parse result for one complete DocBook XML document.
 */
struct	ptree {
	struct pnode	*root;     /* The document element. */
	int		 flags;
#define	TREE_ERROR	 (1 << 0)  /* A parse error occurred. */
#define	TREE_WARN	 (1 << 1)  /* A parser warning occurred. */
#define	TREE_CLOSED	 (1 << 3)  /* The document element was closed. */
};


enum attrkey	 attrkey_parse(const char *);
const char	*attrkey_name(enum attrkey);
enum attrval	 attrval_parse(const char *);
const char	*attr_getval(const struct pattr *a);
enum nodeid	 pnode_parse(const char *name);
const char	*pnode_name(enum nodeid);
enum nodeclass	 pnode_class(enum nodeid);

struct pnode	*pnode_alloc(struct pnode *);
struct pnode	*pnode_alloc_text(struct pnode *, const char *);
void		 pnode_unlink(struct pnode *);
void		 pnode_unlinksub(struct pnode *);
enum attrval	 pnode_getattr(struct pnode *, enum attrkey);
const char	*pnode_getattr_raw(struct pnode *, enum attrkey, const char *);
struct pnode	*pnode_findfirst(struct pnode *, enum nodeid);
struct pnode	*pnode_takefirst(struct pnode *, enum nodeid);
