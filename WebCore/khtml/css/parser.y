%{

/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 2002-2003 Lars Knoll (knoll@kde.org)
 *  Copyright (C) 2004 Apple Computer, Inc.
 * 
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <string.h>
#include <stdlib.h>

#include <dom/dom_string.h>
#include <xml/dom_docimpl.h>
#include <css/css_ruleimpl.h>
#include <css/css_stylesheetimpl.h>
#include <css/css_valueimpl.h>
#include <misc/htmlhashes.h>
#include "cssparser.h"

#include "xml_namespace_table.h"
    
#include <assert.h>
#include <kdebug.h>
// #define CSS_DEBUG

using namespace DOM;

//
// The following file defines the function
//     const struct props *findProp(const char *word, int len)
//
// with 'props->id' a CSS property in the range from CSS_PROP_MIN to
// (and including) CSS_PROP_TOTAL-1

// turn off inlining to void warning with newer gcc
#undef __inline
#define __inline
#include "cssproperties.c"
#include "cssvalues.c"
#undef __inline

int DOM::getPropertyID(const char *tagStr, int len)
{
    const struct props *propsPtr = findProp(tagStr, len);
    if (!propsPtr)
        return 0;

    return propsPtr->id;
}

static inline int getValueID(const char *tagStr, int len)
{
    const struct css_value *val = findValue(tagStr, len);
    if (!val)
        return 0;

    return val->id;
}


#define YYDEBUG 0
#define YYMAXDEPTH 0
#define YYPARSE_PARAM parser
%}

%pure_parser

%union {
    CSSRuleImpl *rule;
    CSSSelector *selector;
    bool ok;
    MediaListImpl *mediaList;
    CSSMediaRuleImpl *mediaRule;
    CSSRuleListImpl *ruleList;
    ParseString string;
    float val;
    int prop_id;
    int attribute;
    int element;
    CSSSelector::Relation relation;
    bool b;
    char tok;
    Value value;
    ValueList *valueList;
}

%{

static inline int cssyyerror(const char *x ) {
#ifdef CSS_DEBUG
    qDebug( x );
#else
    Q_UNUSED( x );
#endif
    return 1;
}

static int cssyylex( YYSTYPE *yylval ) {
    return CSSParser::current()->lex( yylval );
}


%}

%expect 12

%token WHITESPACE SGML_CD

%token INCLUDES
%token DASHMATCH
%token BEGINSWITH
%token ENDSWITH
%token CONTAINS

%token <string> STRING

%token <string> IDENT

%token <string> HASH
%token ':'
%token '.'
%token '['

%token IMPORT_SYM
%token PAGE_SYM
%token MEDIA_SYM
%token FONT_FACE_SYM
%token CHARSET_SYM
%token NAMESPACE_SYM
%token KHTML_RULE_SYM
%token KHTML_DECLS_SYM
%token KHTML_VALUE_SYM

%token IMPORTANT_SYM

%token <val> QEMS
%token <val> EMS
%token <val> EXS
%token <val> PXS
%token <val> CMS
%token <val> MMS
%token <val> INS
%token <val> PTS
%token <val> PCS
%token <val> DEGS
%token <val> RADS
%token <val> GRADS
%token <val> MSECS
%token <val> SECS
%token <val> HERZ
%token <val> KHERZ
%token <string> DIMEN
%token <val> PERCENTAGE
%token <val> NUMBER

%token <string> URI
%token <string> FUNCTION

%token <string> UNICODERANGE

%type <relation> combinator

%type <rule> ruleset
%type <rule> media
%type <rule> import
%type <rule> page
%type <rule> font_face
%type <rule> invalid_rule
%type <rule> invalid_at
%type <rule> invalid_import
%type <rule> rule

%type <string> maybe_ns_prefix

%type <string> namespace_selector

%type <string> string_or_uri
%type <string> ident_or_string
%type <string> medium
%type <string> hexcolor

%type <mediaList> media_list
%type <mediaList> maybe_media_list

%type <ruleList> ruleset_list

%type <prop_id> property

%type <selector> specifier
%type <selector> specifier_list
%type <selector> simple_selector
%type <selector> selector
%type <selector> selector_list
%type <selector> class
%type <selector> attrib
%type <selector> pseudo

%type <ok> declaration_list
%type <ok> decl_list
%type <ok> declaration

%type <b> prio

%type <val> match
%type <val> unary_operator
%type <tok> operator

%type <valueList> expr
%type <value> term
%type <value> unary_term
%type <value> function

%type <element> element_name

%type <attribute> attrib_id

%%

stylesheet:
    maybe_charset maybe_sgml import_list namespace_list rule_list
  | khtml_rule maybe_space
  | khtml_decls maybe_space
  | khtml_value maybe_space
  ;

khtml_rule:
    KHTML_RULE_SYM '{' maybe_space ruleset maybe_space '}' {
        CSSParser *p = static_cast<CSSParser *>(parser);
        p->rule = $4;
    }
;

khtml_decls:
    KHTML_DECLS_SYM '{' maybe_space declaration_list '}' {
	/* can be empty */
    }
;

khtml_value:
    KHTML_VALUE_SYM '{' maybe_space expr '}' {
	CSSParser *p = static_cast<CSSParser *>(parser);
	if ( $4 ) {
	    p->valueList = $4;
#ifdef CSS_DEBUG
	    kdDebug( 6080 ) << "   got property for " << p->id <<
		(p->important?" important":"")<< endl;
	    bool ok =
#endif
		p->parseValue( p->id, p->important );
#ifdef CSS_DEBUG
	    if ( !ok )
		kdDebug( 6080 ) << "     couldn't parse value!" << endl;
#endif
	}
#ifdef CSS_DEBUG
	else
	    kdDebug( 6080 ) << "     no value found!" << endl;
#endif
	delete p->valueList;
	p->valueList = 0;
    }
;

maybe_space:
    /* empty */
  | maybe_space WHITESPACE
  ;

maybe_sgml:
    /* empty */
  | maybe_sgml SGML_CD
  | maybe_sgml WHITESPACE
  ;

maybe_charset:
   /* empty */
 | CHARSET_SYM maybe_space STRING maybe_space ';' {
#ifdef CSS_DEBUG
     kdDebug( 6080 ) << "charset rule: " << qString($3) << endl;
#endif
 }
  | CHARSET_SYM error invalid_block
  | CHARSET_SYM error ';'
 ;

import_list:
 /* empty */
 | import_list import maybe_sgml {
     CSSParser *p = static_cast<CSSParser *>(parser);
     if ( $2 && p->styleElement && p->styleElement->isCSSStyleSheet() ) {
	 p->styleElement->append( $2 );
     } else {
	 delete $2;
     }
 }
 ;

namespace_list:
/* empty */
| namespace_list namespace maybe_sgml
;

rule_list:
   /* empty */
 | rule_list rule maybe_sgml {
     CSSParser *p = static_cast<CSSParser *>(parser);
     if ( $2 && p->styleElement && p->styleElement->isCSSStyleSheet() ) {
	 p->styleElement->append( $2 );
     } else {
	 delete $2;
     }
 }
 ;

rule:
    ruleset
  | media
  | page
  | font_face
  | invalid_rule
  | invalid_at
  | invalid_import
    ;

import:
    IMPORT_SYM maybe_space string_or_uri maybe_space maybe_media_list ';' {
#ifdef CSS_DEBUG
	kdDebug( 6080 ) << "@import: " << qString($3) << endl;
#endif
	CSSParser *p = static_cast<CSSParser *>(parser);
	if ( $5 && p->styleElement && p->styleElement->isCSSStyleSheet() )
	    $$ = new CSSImportRuleImpl( p->styleElement, domString($3), $5 );
	else {
	    $$ = 0;
            delete $5;
        }
    }
  | IMPORT_SYM error invalid_block {
        $$ = 0;
    }
  | IMPORT_SYM error ';' {
        $$ = 0;
    }
  ;

namespace:
NAMESPACE_SYM maybe_space maybe_ns_prefix string_or_uri maybe_space ';' {
#ifdef CSS_DEBUG
    kdDebug( 6080 ) << "@namespace: " << qString($4) << endl;
#endif
    CSSParser *p = static_cast<CSSParser *>(parser);
    if (p->styleElement && p->styleElement->isCSSStyleSheet())
        static_cast<CSSStyleSheetImpl*>(p->styleElement)->addNamespace(p, domString($3), domString($4));
}
| NAMESPACE_SYM error invalid_block
| NAMESPACE_SYM error ';'
;

maybe_ns_prefix:
/* empty */ { $$.string = 0; }
| IDENT WHITESPACE { $$ = $1; }
;

string_or_uri:
STRING
| URI
;

maybe_media_list:
     /* empty */ {
        $$ = new MediaListImpl();
     }
     | media_list
;


media_list:
    /* empty */ {
	$$ = 0;
    }
    | medium {
	$$ = new MediaListImpl();
	$$->appendMedium( domString($1).lower() );
    }
    | media_list ',' maybe_space medium {
	$$ = $1;
        if ($$)
	    $$->appendMedium( domString($4) );
    }
    | media_list error {
        delete $1;
        $$ = 0;
    }
    ;

media:
    MEDIA_SYM maybe_space media_list '{' maybe_space ruleset_list '}' {
	CSSParser *p = static_cast<CSSParser *>(parser);
	if ( $3 && $6 &&
	     p->styleElement && p->styleElement->isCSSStyleSheet() ) {
	    $$ = new CSSMediaRuleImpl( static_cast<CSSStyleSheetImpl*>(p->styleElement), $3, $6 );
	} else {
	    $$ = 0;
	    delete $3;
	    delete $6;
	}
    }
  ;

ruleset_list:
    /* empty */ { $$ = 0; }
  | ruleset_list ruleset maybe_space {
      $$ = $1;
      if ( $2 ) {
	  if ( !$$ ) $$ = new CSSRuleListImpl();
	  $$->append( $2 );
      }
  }
    ;

medium:
  IDENT maybe_space {
      $$ = $1;
  }
  ;

/*
page:
    PAGE_SYM maybe_space IDENT? pseudo_page? maybe_space
    '{' maybe_space declaration [ ';' maybe_space declaration ]* '}' maybe_space
  ;

pseudo_page
  : ':' IDENT
  ;

font_face
  : FONT_FACE_SYM maybe_space
    '{' maybe_space declaration [ ';' maybe_space declaration ]* '}' maybe_space
  ;
*/

page:
    PAGE_SYM error invalid_block {
      $$ = 0;
    }
  | PAGE_SYM error ';' {
      $$ = 0;
    }
    ;

font_face:
    FONT_FACE_SYM error invalid_block {
      $$ = 0;
    }
  | FONT_FACE_SYM error ';' {
      $$ = 0;
    }
;

combinator:
  '+' maybe_space { $$ = CSSSelector::Sibling; }
  | '>' maybe_space { $$ = CSSSelector::Child; }
  | /* empty */ { $$ = CSSSelector::Descendant; }
  ;

unary_operator:
    '-' { $$ = -1; }
  | '+' { $$ = 1; }
  ;

ruleset:
    selector_list '{' maybe_space declaration_list '}' {
#ifdef CSS_DEBUG
	kdDebug( 6080 ) << "got ruleset" << endl << "  selector:" << endl;
#endif
	CSSParser *p = static_cast<CSSParser *>(parser);
	if ( $1 ) {
            CSSStyleRuleImpl *rule = new CSSStyleRuleImpl( p->styleElement );
            CSSMutableStyleDeclarationImpl *decl = p->createStyleDeclaration( rule );
            rule->setSelector( $1 );
            rule->setDeclaration(decl);
            $$ = rule;
	} else {
	    $$ = 0;
	    p->clearProperties();
	}
    }
  ;

selector_list:
    selector {
	if ( $1 ) {
	    $$ = $1;
#ifdef CSS_DEBUG
	    kdDebug( 6080 ) << "   got simple selector:" << endl;
	    $1->print();
#endif
	} else {
	    $$ = 0;
	}
    }
    | selector_list ',' maybe_space selector {
	if ( $1 && $4 ) {
	    $$ = $1;
	    $$->append( $4 );
#ifdef CSS_DEBUG
	    kdDebug( 6080 ) << "   got simple selector:" << endl;
	    $4->print();
#endif
	} else {
            delete $1;
            delete $4;
            $$ = 0;
        }
    }
  | selector_list error {
        delete $1;
        $$ = 0;
    }
   ;

selector:
    simple_selector {
	$$ = $1;
    }
    | selector combinator simple_selector {
    	$$ = $3;
        if (!$1) {
            delete $3;
            $$ = 0;
        }
        else if ($$) {
            CSSSelector *end = $$;
            while( end->tagHistory )
                end = end->tagHistory;
            end->relation = $2;
            end->tagHistory = $1;
            if ( $2 == CSSSelector::Descendant ||
                $2 == CSSSelector::Child ) {
                CSSParser *p = static_cast<CSSParser *>(parser);
                DOM::DocumentImpl *doc = p->document();
                if ( doc )
                    doc->setUsesDescendantRules(true);
            }
            else if ($2 == CSSSelector::Sibling) {
                CSSParser *p = static_cast<CSSParser *>(parser);
                DOM::DocumentImpl *doc = p->document();
                if (doc)
                    doc->setUsesSiblingRules(true);
            }
        } else {
            delete $1;
        }
    }
    | selector error {
        delete $1;
        $$ = 0;
    }
    ;

namespace_selector:
    /* empty */ { $$.string = 0; $$.length = 0; }
    | '*' { static unsigned short star = '*'; $$.string = &star; $$.length = 1; }
    | IDENT { $$ = $1; }
;

simple_selector:
    element_name maybe_space {
	$$ = new CSSSelector();
	$$->tag = $1;
    }
    | element_name specifier_list maybe_space {
	$$ = $2;
	if ( $$ )
            $$->tag = $1;
    }
    | specifier_list maybe_space {
	$$ = $1;
        if ($$)
            $$->tag = makeId(static_cast<CSSParser*>(parser)->defaultNamespace, anyLocalName);;
    }
    | namespace_selector '|' element_name maybe_space {
        $$ = new CSSSelector();
        $$->tag = $3;
        CSSParser *p = static_cast<CSSParser *>(parser);
        if (p->styleElement && p->styleElement->isCSSStyleSheet())
            static_cast<CSSStyleSheetImpl*>(p->styleElement)->determineNamespace($$->tag, domString($1));
    }
    | namespace_selector '|' element_name specifier_list maybe_space {
        $$ = $4;
        if ($$) {
            $$->tag = $3;
            CSSParser *p = static_cast<CSSParser *>(parser);
            if (p->styleElement && p->styleElement->isCSSStyleSheet())
                static_cast<CSSStyleSheetImpl*>(p->styleElement)->determineNamespace($$->tag, domString($1));
        }
    }
    | namespace_selector '|' specifier_list maybe_space {
        $$ = $3;
        if ($$) {
            $$->tag = makeId(anyNamespace, anyLocalName);
            CSSParser *p = static_cast<CSSParser *>(parser);
            if (p->styleElement && p->styleElement->isCSSStyleSheet())
                static_cast<CSSStyleSheetImpl*>(p->styleElement)->determineNamespace($$->tag, domString($1));
        }
    }
  ;

element_name:
    IDENT {
	CSSParser *p = static_cast<CSSParser *>(parser);
	DOM::DocumentImpl *doc = p->document();
	QString tag = qString($1);
	if ( doc ) {
	    if (doc->isHTMLDocument())
		tag = tag.lower();
	    const DOMString dtag(tag);
            $$ = makeId(p->defaultNamespace, doc->tagId(0, dtag.implementation(), false));
	} else {
	    $$ = makeId(p->defaultNamespace, khtml::getTagID(tag.lower().ascii(), tag.length()));
	    // this case should never happen - only when loading
	    // the default stylesheet - which must not contain unknown tags
// 	    assert($$ != 0);
	}
    }
    | '*' {
	$$ = makeId(static_cast<CSSParser*>(parser)->defaultNamespace, anyLocalName);
    }
  ;

specifier_list:
    specifier {
	$$ = $1;
    }
    | specifier_list specifier {
	$$ = $1;
        if ($$) {
            CSSSelector *end = $1;
            while( end->tagHistory )
                end = end->tagHistory;
            end->relation = CSSSelector::SubSelector;
            end->tagHistory = $2;
        }
    }
    | specifier_list error {
        delete $1;
        $$ = 0;
    }
;

specifier:
    HASH {
	$$ = new CSSSelector();
	$$->match = CSSSelector::Id;
	$$->attr = ATTR_ID;
        CSSParser *p = static_cast<CSSParser *>(parser);
        if (!p->strict)
            $1.lower();
	$$->value = atomicString($1);
    }
  | class
  | attrib
  | pseudo
    ;

class:
    '.' IDENT {
        $$ = new CSSSelector();
	$$->match = CSSSelector::Class;
	$$->attr = ATTR_CLASS;
        CSSParser *p = static_cast<CSSParser *>(parser);
        if (!p->strict)
            $2.lower();
	$$->value = atomicString($2);
    }
  ;

attrib_id:
    IDENT maybe_space {
	CSSParser *p = static_cast<CSSParser *>(parser);
	DOM::DocumentImpl *doc = p->document();

	QString attr = qString($1);
	if ( doc ) {
	    if (doc->isHTMLDocument())
		attr = attr.lower();
	    const DOMString dattr(attr);
            $$ = doc->attrId(0, dattr.implementation(), false);
	} else {
	    $$ = khtml::getAttrID(attr.lower().ascii(), attr.length());
	    // this case should never happen - only when loading
	    // the default stylesheet - which must not contain unknown attributes
	    assert($$ != 0);
        }
    }
    ;

attrib:
    '[' maybe_space attrib_id ']' {
	$$ = new CSSSelector();
	$$->attr = $3;
	$$->match = CSSSelector::Set;
    }
    | '[' maybe_space attrib_id match maybe_space ident_or_string maybe_space ']' {
	$$ = new CSSSelector();
	$$->attr = $3;
	$$->match = (CSSSelector::Match)$4;
	$$->value = atomicString($6);
    }
    | '[' maybe_space namespace_selector '|' attrib_id ']' {
        $$ = new CSSSelector();
        $$->attr = $5;
        $$->match = CSSSelector::Set;
        CSSParser *p = static_cast<CSSParser *>(parser);
        if (p->styleElement && p->styleElement->isCSSStyleSheet())
            static_cast<CSSStyleSheetImpl*>(p->styleElement)->determineNamespace($$->attr, domString($3));
    }
    | '[' maybe_space namespace_selector '|' attrib_id match maybe_space ident_or_string maybe_space ']' {
        $$ = new CSSSelector();
        $$->attr = $5;
        $$->match = (CSSSelector::Match)$6;
        $$->value = atomicString($8);
        CSSParser *p = static_cast<CSSParser *>(parser);
        if (p->styleElement && p->styleElement->isCSSStyleSheet())
            static_cast<CSSStyleSheetImpl*>(p->styleElement)->determineNamespace($$->attr, domString($3));
    }
  ;

match:
    '=' {
	$$ = CSSSelector::Exact;
    }
    | INCLUDES {
	$$ = CSSSelector::List;
    }
    | DASHMATCH {
	$$ = CSSSelector::Hyphen;
    }
    | BEGINSWITH {
	$$ = CSSSelector::Begin;
    }
    | ENDSWITH {
	$$ = CSSSelector::End;
    }
    | CONTAINS {
	$$ = CSSSelector::Contain;
    }
    ;

ident_or_string:
    IDENT
  | STRING
    ;

pseudo:
    ':' IDENT {
        $$ = new CSSSelector();
        $$->match = CSSSelector::Pseudo;
        $2.lower();
        $$->value = atomicString($2);
        if ($$->value == "empty" || $$->value == "only-child" ||
            $$->value == "first-child" || $$->value == "last-child") {
            CSSParser *p = static_cast<CSSParser *>(parser);
            DOM::DocumentImpl *doc = p->document();
            if (doc)
                doc->setUsesSiblingRules(true);
        }
    }
    |
    ':' ':' IDENT {
        $$ = new CSSSelector();
        $$->match = CSSSelector::Pseudo;
        $3.lower();
        $$->value = atomicString($3);
    }
    | ':' FUNCTION maybe_space simple_selector maybe_space ')' {
        $$ = new CSSSelector();
        $$->match = CSSSelector::Pseudo;
        $$->simpleSelector = $4;
        $2.lower();
        $$->value = atomicString($2);
    }
  ;

declaration_list:
    declaration {
	$$ = $1;
    }
    | decl_list declaration {
	$$ = $1;
	if ( $2 )
	    $$ = $2;
    }
    | decl_list {
	$$ = $1;
    }
    | error invalid_block_list error {
	$$ = false;
#ifdef CSS_DEBUG
	kdDebug( 6080 ) << "skipping bogus declaration" << endl;
#endif
    }
    | error {
	$$ = false;
#ifdef CSS_DEBUG
	kdDebug( 6080 ) << "skipping all declarations" << endl;
#endif
    }
    ;

decl_list:
    declaration ';' maybe_space {
	$$ = $1;
    }
    | declaration invalid_block_list ';' maybe_space {
        $$ = false;
    }
    | error ';' maybe_space {
	$$ = false;
#ifdef CSS_DEBUG
	kdDebug( 6080 ) << "skipping bogus declaration" << endl;
#endif
    }
    | error invalid_block_list error ';' maybe_space {
	$$ = false;
#ifdef CSS_DEBUG
	kdDebug( 6080 ) << "skipping bogus declaration" << endl;
#endif
    }
    | decl_list declaration ';' maybe_space {
	$$ = $1;
	if ( $2 )
	    $$ = $2;
    }
    | decl_list error ';' maybe_space {
	$$ = $1;
#ifdef CSS_DEBUG
	kdDebug( 6080 ) << "skipping bogus declaration" << endl;
#endif
    }
    | decl_list error invalid_block_list error ';' maybe_space {
	$$ = $1;
#ifdef CSS_DEBUG
	kdDebug( 6080 ) << "skipping bogus declaration" << endl;
#endif
    }
    ;

declaration:
    property ':' maybe_space expr prio {
	$$ = false;
	CSSParser *p = static_cast<CSSParser *>(parser);
	if ( $1 && $4 ) {
	    p->valueList = $4;
#ifdef CSS_DEBUG
	    kdDebug( 6080 ) << "   got property: " << $1 <<
		($5?" important":"")<< endl;
#endif
	    bool ok = p->parseValue( $1, $5 );
	    if ( ok )
		$$ = ok;
#ifdef CSS_DEBUG
	    else
		kdDebug( 6080 ) << "     couldn't parse value!" << endl;
#endif
	} else {
            delete $4;
        }
	delete p->valueList;
	p->valueList = 0;
    }
    |
    property error {
        $$ = false;
    }
    |
    property ':' maybe_space error expr prio {
        /* The default movable type template has letter-spacing: .none;  Handle this by looking for
        error tokens at the start of an expr, recover the expr and then treat as an error, cleaning
        up and deleting the shifted expr.  */
        delete $5;
        $$ = false;
    }
    |
    prio {
        /* Handle this case: div { text-align: center; !important } Just reduce away the stray !important. */
        $$ = false;
    }
    |
    property ':' maybe_space {
        /* div { font-family: } Just reduce away this property with no value. */
        $$ = false;
    }
  ;

property:
    IDENT maybe_space {
	QString str = qString($1);
	$$ = getPropertyID( str.lower().latin1(), str.length() );
    }
  ;

prio:
    IMPORTANT_SYM maybe_space { $$ = true; }
    | /* empty */ { $$ = false; }
  ;

expr:
    term {
	$$ = new ValueList;
	$$->addValue( $1 );
    }
    | expr operator term {
        $$ = $1;
	if ( $$ ) {
            if ( $2 ) {
                Value v;
                v.id = 0;
                v.unit = Value::Operator;
                v.iValue = $2;
                $$->addValue( v );
            }
            $$->addValue( $3 );
        }
    }
    | expr error {
        delete $1;
        $$ = 0;
    }
  ;

operator:
    '/' maybe_space {
	$$ = '/';
    }
  | ',' maybe_space {
	$$ = ',';
    }
  | /* empty */ {
        $$ = 0;
  }
  ;

term:
  unary_term { $$ = $1; }
  | unary_operator unary_term { $$ = $2; $$.fValue *= $1; }
  | STRING maybe_space { $$.id = 0; $$.string = $1; $$.unit = CSSPrimitiveValue::CSS_STRING; }
  | IDENT maybe_space {
      QString str = qString( $1 );
      $$.id = getValueID( str.lower().latin1(), str.length() );
      $$.unit = CSSPrimitiveValue::CSS_IDENT;
      $$.string = $1;
  }
  /* We might need to actually parse the number from a dimension, but we can't just put something that uses $$.string into unary_term. */
  | DIMEN maybe_space { $$.id = 0; $$.string = $1; $$.unit = CSSPrimitiveValue::CSS_DIMENSION }
  | unary_operator DIMEN maybe_space { $$.id = 0; $$.string = $2; $$.unit = CSSPrimitiveValue::CSS_DIMENSION }
  | URI maybe_space { $$.id = 0; $$.string = $1; $$.unit = CSSPrimitiveValue::CSS_URI; }
  | UNICODERANGE maybe_space { $$.id = 0; $$.iValue = 0; $$.unit = CSSPrimitiveValue::CSS_UNKNOWN;/* ### */ }
  | hexcolor { $$.id = 0; $$.string = $1; $$.unit = CSSPrimitiveValue::CSS_RGBCOLOR; }
  | '#' maybe_space { $$.id = 0; $$.string = ParseString(); $$.unit = CSSPrimitiveValue::CSS_RGBCOLOR; } /* Handle error case: "color: #;" */
/* ### according to the specs a function can have a unary_operator in front. I know no case where this makes sense */
  | function {
      $$ = $1;
  }
  ;

unary_term:
  NUMBER maybe_space { $$.id = 0; $$.fValue = $1; $$.unit = CSSPrimitiveValue::CSS_NUMBER; }
  | PERCENTAGE maybe_space { $$.id = 0; $$.fValue = $1; $$.unit = CSSPrimitiveValue::CSS_PERCENTAGE; }
  | PXS maybe_space { $$.id = 0; $$.fValue = $1; $$.unit = CSSPrimitiveValue::CSS_PX; }
  | CMS maybe_space { $$.id = 0; $$.fValue = $1; $$.unit = CSSPrimitiveValue::CSS_CM; }
  | MMS maybe_space { $$.id = 0; $$.fValue = $1; $$.unit = CSSPrimitiveValue::CSS_MM; }
  | INS maybe_space { $$.id = 0; $$.fValue = $1; $$.unit = CSSPrimitiveValue::CSS_IN; }
  | PTS maybe_space { $$.id = 0; $$.fValue = $1; $$.unit = CSSPrimitiveValue::CSS_PT; }
  | PCS maybe_space { $$.id = 0; $$.fValue = $1; $$.unit = CSSPrimitiveValue::CSS_PC; }
  | DEGS maybe_space { $$.id = 0; $$.fValue = $1; $$.unit = CSSPrimitiveValue::CSS_DEG; }
  | RADS maybe_space { $$.id = 0; $$.fValue = $1; $$.unit = CSSPrimitiveValue::CSS_RAD; }
  | GRADS maybe_space { $$.id = 0; $$.fValue = $1; $$.unit = CSSPrimitiveValue::CSS_GRAD; }
  | MSECS maybe_space { $$.id = 0; $$.fValue = $1; $$.unit = CSSPrimitiveValue::CSS_MS; }
  | SECS maybe_space { $$.id = 0; $$.fValue = $1; $$.unit = CSSPrimitiveValue::CSS_S; }
  | HERZ maybe_space { $$.id = 0; $$.fValue = $1; $$.unit = CSSPrimitiveValue::CSS_HZ; }
  | KHERZ maybe_space { $$.id = 0; $$.fValue = $1; $$.unit = CSSPrimitiveValue::CSS_KHZ; }
  | EMS maybe_space { $$.id = 0; $$.fValue = $1; $$.unit = CSSPrimitiveValue::CSS_EMS; }
  | QEMS maybe_space { $$.id = 0; $$.fValue = $1; $$.unit = Value::Q_EMS; }
  | EXS maybe_space { $$.id = 0; $$.fValue = $1; $$.unit = CSSPrimitiveValue::CSS_EXS; }
    ;


function:
  FUNCTION maybe_space expr ')' maybe_space {
      Function *f = new Function;
      f->name = $1;
      f->args = $3;
      $$.id = 0;
      $$.unit = Value::Function;
      $$.function = f;
  } |
  FUNCTION maybe_space error {
      Function *f = new Function;
      f->name = $1;
      f->args = 0;
      $$.id = 0;
      $$.unit = Value::Function;
      $$.function = f;
  }
  ;
/*
 * There is a constraint on the color that it must
 * have either 3 or 6 hex-digits (i.e., [0-9a-fA-F])
 * after the "#"; e.g., "#000" is OK, but "#abcd" is not.
 */
hexcolor:
  HASH maybe_space { $$ = $1; }
  ;


/* error handling rules */

invalid_at:
    '@' error invalid_block {
	$$ = 0;
#ifdef CSS_DEBUG
	kdDebug( 6080 ) << "skipped invalid @-rule" << endl;
#endif
    }
  | '@' error ';' {
	$$ = 0;
#ifdef CSS_DEBUG
	kdDebug( 6080 ) << "skipped invalid @-rule" << endl;
#endif
    }
    ;

invalid_import:
    import {
        delete $1;
        $$ = 0;
#ifdef CSS_DEBUG
        kdDebug( 6080 ) << "skipped invalid import" << endl;
#endif
    }
    ;

invalid_rule:
    error invalid_block {
	$$ = 0;
#ifdef CSS_DEBUG
	kdDebug( 6080 ) << "skipped invalid rule" << endl;
#endif
    }
/*
  Seems like the two rules below are trying too much and violating
  http://www.hixie.ch/tests/evil/mixed/csserrorhandling.html

  | error ';' {
	$$ = 0;
#ifdef CSS_DEBUG
	kdDebug( 6080 ) << "skipped invalid rule" << endl;
#endif
    }
  | error '}' {
	$$ = 0;
#ifdef CSS_DEBUG
	kdDebug( 6080 ) << "skipped invalid rule" << endl;
#endif
    }
*/
    ;

invalid_block:
    '{' error invalid_block_list error '}'
  | '{' error '}'
    ;

invalid_block_list:
    invalid_block
  | invalid_block_list error invalid_block
;

%%

