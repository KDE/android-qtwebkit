/*
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004 Apple Computer, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

//#define CSS_DEBUG
// #define TOKEN_DEBUG
#define YYDEBUG 0

#include <kdebug.h>
#include <kurl.h>

#include "cssparser.h"
#include "css_valueimpl.h"
#include "css_ruleimpl.h"
#include "css_stylesheetimpl.h"
#include "cssproperties.h"
#include "cssvalues.h"
#include "misc/helper.h"
#include "xml/dom_docimpl.h"
#include "csshelper.h"
using namespace DOM;

#include <stdlib.h>

#if APPLE_CHANGES
void qFatal ( const char * msg ) {}
#endif

ValueList::ValueList()
{
    values = (Value *) malloc( 16 * sizeof ( Value ) );
    numValues = 0;
    currentValue = 0;
    maxValues = 16;
}

ValueList::~ValueList()
{
    for ( int i = 0; i < numValues; i++ ) {
#ifdef CSS_DEBUG
	kdDebug( 6080 ) << "       value: (unit=" << values[i].unit <<")"<< endl;
#endif
	if ( values[i].unit == Value::Function )
	    delete values[i].function;
    }
    free( values );
}

void ValueList::addValue( const Value &val )
{
    if ( numValues >= maxValues ) {
	maxValues += 16;
	values = (Value *) realloc( values, maxValues*sizeof( Value ) );
    }
    values[numValues++] = val;
}


using namespace DOM;

#if YYDEBUG > 0
extern int cssyydebug;
#endif

extern int cssyyparse( void * parser );

CSSParser *CSSParser::currentParser = 0;

CSSParser::CSSParser( bool strictParsing )
{
#ifdef CSS_DEBUG
    kdDebug( 6080 ) << "CSSParser::CSSParser this=" << this << endl;
#endif
    strict = strictParsing;

    parsedProperties = (CSSProperty **) malloc( 32 * sizeof( CSSProperty * ) );
    numParsedProperties = 0;
    maxParsedProperties = 32;

    data = 0;
    valueList = 0;
    rule = 0;
    id = 0;
    important = false;
    inParseShortHand = false;
    
    defaultNamespace = anyNamespace;
    
    yy_start = 1;

#if YYDEBUG > 0
    cssyydebug = 1;
#endif

}

CSSParser::~CSSParser()
{
    if ( numParsedProperties )
	clearProperties();
    free( parsedProperties );

    delete valueList;

#ifdef CSS_DEBUG
    kdDebug( 6080 ) << "CSSParser::~CSSParser this=" << this << endl;
#endif

    free( data );

}

void ParseString::lower()
{
    for (int i = 0; i < length; i++)
        string[i] = QChar(string[i]).lower().unicode();
}

void CSSParser::setupParser(const char *prefix, const DOMString &string, const char *suffix)
{
    int length = string.length() + strlen(prefix) + strlen(suffix) + 2;
    
    data = (unsigned short *)malloc( length *sizeof( unsigned short ) );
    for ( unsigned int i = 0; i < strlen(prefix); i++ )
	data[i] = prefix[i];
    
    memcpy( data + strlen( prefix ), string.unicode(), string.length()*sizeof( unsigned short) );

    unsigned int start = strlen( prefix ) + string.length();
    unsigned int end = start + strlen(suffix);
    for ( unsigned int i = start; i < end; i++ )
        data[i] = suffix[i-start];

    data[length-1] = 0;
    data[length-2] = 0;

    yy_hold_char = 0;
    yyleng = 0;
    yytext = yy_c_buf_p = data;
    yy_hold_char = *yy_c_buf_p;
}

void CSSParser::parseSheet( CSSStyleSheetImpl *sheet, const DOMString &string )
{
    styleElement = sheet;
    defaultNamespace = anyNamespace; // Reset the default namespace.
    
    setupParser ("", string, "");

#ifdef CSS_DEBUG
    kdDebug( 6080 ) << ">>>>>>> start parsing style sheet" << endl;
#endif
    CSSParser *old = currentParser;
    currentParser = this;
    cssyyparse( this );
    currentParser = old;
#ifdef CSS_DEBUG
    kdDebug( 6080 ) << "<<<<<<< done parsing style sheet" << endl;
#endif

    delete rule;
    rule = 0;
}

CSSRuleImpl *CSSParser::parseRule( DOM::CSSStyleSheetImpl *sheet, const DOM::DOMString &string )
{
    styleElement = sheet;
    
    setupParser ("@-khtml-rule{", string, "} ");

    CSSParser *old = currentParser;
    currentParser = this;
    cssyyparse( this );
    currentParser = old;

    CSSRuleImpl *result = rule;
    rule = 0;
    
    return result;
}

bool CSSParser::parseValue( CSSMutableStyleDeclarationImpl *declaration, int _id, const DOMString &string,
			    bool _important)
{
#ifdef CSS_DEBUG
    kdDebug( 6080 ) << "CSSParser::parseValue: id=" << _id << " important=" << _important
		    << " value='" << string.string() << "'" << endl;
#endif

    styleElement = declaration->stylesheet();

    setupParser ("@-khtml-value{", string, "} ");

    id = _id;
    important = _important;
    
    CSSParser *old = currentParser;
    currentParser = this;
    cssyyparse( this );
    currentParser = old;
    
    delete rule;
    rule = 0;

    bool ok = false;
    if ( numParsedProperties ) {
	ok = true;
        declaration->addParsedProperties(parsedProperties, numParsedProperties);
        clearProperties();
    }

    return ok;
}

QRgb CSSParser::parseColor( const DOM::DOMString &string )
{
    QRgb color = 0;
    CSSMutableStyleDeclarationImpl *dummyStyleDeclaration = new CSSMutableStyleDeclarationImpl;
    
    dummyStyleDeclaration->ref();

    DOM::CSSParser parser(true);

    // First try creating a color specified by name or the "#" syntax.
    if (!parser.parseColor(string.string(), color)) {
    
        // Now try to create a color from the rgb() or rgba() syntax.
        bool ok = parser.parseColor(dummyStyleDeclaration, string);
        if ( ok ) {
            CSSValueImpl *value = parser.parsedProperties[0]->value();
            if (value->cssValueType() == DOM::CSSValue::CSS_PRIMITIVE_VALUE) {
                DOM::CSSPrimitiveValueImpl *primitiveValue = static_cast<DOM::CSSPrimitiveValueImpl *>(value);
                color = primitiveValue->getRGBColorValue();
            }
        }
    
    }
    
    dummyStyleDeclaration->deref();
    
    return color;
}

bool CSSParser::parseColor( CSSMutableStyleDeclarationImpl *declaration, const DOMString &string )
{
    styleElement = declaration->stylesheet();

    setupParser ( "@-khtml-decls{color:", string, "} ");

    CSSParser *old = currentParser;
    currentParser = this;
    cssyyparse( this );
    currentParser = old;

    delete rule;
    rule = 0;

    bool ok = false;
    if ( numParsedProperties && parsedProperties[0]->m_id == CSS_PROP_COLOR) {
	ok = true;
    }

    return ok;
}

bool CSSParser::parseDeclaration( CSSMutableStyleDeclarationImpl *declaration, const DOMString &string )
{
#ifdef CSS_DEBUG
    kdDebug( 6080 ) << "CSSParser::parseDeclaration:value='" << string.string() << "'" << endl;
#endif

    styleElement = declaration->stylesheet();

    setupParser ( "@-khtml-decls{", string, "} ");

    CSSParser *old = currentParser;
    currentParser = this;
    cssyyparse( this );
    currentParser = old;

    delete rule;
    rule = 0;

    bool ok = false;
    if ( numParsedProperties ) {
	ok = true;
        declaration->addParsedProperties(parsedProperties, numParsedProperties);
        clearProperties();
    }

    return ok;
}


void CSSParser::addProperty( int propId, CSSValueImpl *value, bool important )
{
    CSSProperty *prop = new CSSProperty;
    prop->m_id = propId;
    prop->setValue( value );
    prop->m_bImportant = important;

    if ( numParsedProperties >= maxParsedProperties ) {
	maxParsedProperties += 32;
	parsedProperties = (CSSProperty **) realloc( parsedProperties,
						    maxParsedProperties*sizeof( CSSProperty * ) );
    }
    parsedProperties[numParsedProperties++] = prop;
}

CSSMutableStyleDeclarationImpl *CSSParser::createStyleDeclaration( CSSStyleRuleImpl *rule )
{
    CSSMutableStyleDeclarationImpl *result = new CSSMutableStyleDeclarationImpl(rule, parsedProperties, numParsedProperties);
    clearProperties();
    return result;
}

void CSSParser::clearProperties()
{
    for ( int i = 0; i < numParsedProperties; i++ )
	delete parsedProperties[i];
    numParsedProperties = 0;
}

DOM::DocumentImpl *CSSParser::document() const
{
    StyleBaseImpl *root = styleElement;
    DocumentImpl *doc = 0;
    while (root->parent())
	root = root->parent();
    if (root->isCSSStyleSheet())
	doc = static_cast<CSSStyleSheetImpl*>(root)->doc();
    return doc;
}


// defines units allowed for a certain property, used in parseUnit
enum Units
{
    FUnknown   = 0x0000,
    FInteger   = 0x0001,
    FNumber    = 0x0002,  // Real Numbers
    FPercent   = 0x0004,
    FLength    = 0x0008,
    FAngle     = 0x0010,
    FTime      = 0x0020,
    FFrequency = 0x0040,
    FRelative  = 0x0100,
    FNonNeg    = 0x0200
};

static bool validUnit( Value *value, int unitflags, bool strict )
{
    if ( unitflags & FNonNeg && value->fValue < 0 )
	return false;

    bool b = false;
    switch( value->unit ) {
    case CSSPrimitiveValue::CSS_NUMBER:
	b = (unitflags & FNumber);
	if ( !b && ( (unitflags & FLength) && (value->fValue == 0 || !strict ) ) ) {
	    value->unit = CSSPrimitiveValue::CSS_PX;
	    b = true;
	}
	if ( !b && ( unitflags & FInteger ) &&
	     (value->fValue - (int)value->fValue) < 0.001 )
	    b = true;
	break;
    case CSSPrimitiveValue::CSS_PERCENTAGE:
	b = (unitflags & FPercent);
	break;
    case Value::Q_EMS:
    case CSSPrimitiveValue::CSS_EMS:
    case CSSPrimitiveValue::CSS_EXS:
    case CSSPrimitiveValue::CSS_PX:
    case CSSPrimitiveValue::CSS_CM:
    case CSSPrimitiveValue::CSS_MM:
    case CSSPrimitiveValue::CSS_IN:
    case CSSPrimitiveValue::CSS_PT:
    case CSSPrimitiveValue::CSS_PC:
	b = (unitflags & FLength);
	break;
    case CSSPrimitiveValue::CSS_MS:
    case CSSPrimitiveValue::CSS_S:
        b = (unitflags & FTime);
        break;
    case CSSPrimitiveValue::CSS_DEG:
    case CSSPrimitiveValue::CSS_RAD:
    case CSSPrimitiveValue::CSS_GRAD:
    case CSSPrimitiveValue::CSS_HZ:
    case CSSPrimitiveValue::CSS_KHZ:
    case CSSPrimitiveValue::CSS_DIMENSION:
    default:
	break;
    }
    return b;
}

bool CSSParser::parseValue( int propId, bool important )
{
    if ( !valueList ) return false;

    Value *value = valueList->current();

    if ( !value )
	return false;

    int id = 0;
    id = value->id;

    if (id == CSS_VAL_INHERIT) {
	addProperty(propId, new CSSInheritedValueImpl(), important);
	return true;
    }
    else if (id == CSS_VAL_INITIAL) {
        addProperty(propId, new CSSInitialValueImpl(), important);
        return true;
    }

    bool valid_primitive = false;
    CSSValueImpl *parsedValue = 0;

    switch(propId) {
	/* The comment to the left defines all valid value of this properties as defined
	 * in CSS 2, Appendix F. Property index
	 */

	/* All the CSS properties are not supported by the renderer at the moment.
	 * Note that all the CSS2 Aural properties are only checked, if CSS_AURAL is defined
	 * (see parseAuralValues). As we don't support them at all this seems reasonable.
	 */

    case CSS_PROP_SIZE:                 // <length>{1,2} | auto | portrait | landscape | inherit
    case CSS_PROP_QUOTES:               // [<string> <string>]+ | none | inherit
//     case CSS_PROP_PAGE:                 // <identifier> | auto // ### CHECK
	// ### To be done
	if (id)
	    valid_primitive = true;
	break;
    case CSS_PROP_UNICODE_BIDI:         // normal | embed | bidi-override | inherit
	if ( id == CSS_VAL_NORMAL ||
	     id == CSS_VAL_EMBED ||
	     id == CSS_VAL_BIDI_OVERRIDE )
	    valid_primitive = true;
	break;

    case CSS_PROP_POSITION:             // static | relative | absolute | fixed | inherit
	if ( id == CSS_VAL_STATIC ||
	     id == CSS_VAL_RELATIVE ||
	     id == CSS_VAL_ABSOLUTE ||
 	     id == CSS_VAL_FIXED )
	    valid_primitive = true;
	break;

    case CSS_PROP_PAGE_BREAK_AFTER:     // auto | always | avoid | left | right | inherit
    case CSS_PROP_PAGE_BREAK_BEFORE:    // auto | always | avoid | left | right | inherit
	if ( id == CSS_VAL_AUTO ||
	     id == CSS_VAL_ALWAYS ||
	     id == CSS_VAL_AVOID ||
 	     id == CSS_VAL_LEFT ||
 	     id == CSS_VAL_RIGHT )
	    valid_primitive = true;
	break;

    case CSS_PROP_PAGE_BREAK_INSIDE:    // avoid | auto | inherit
	if ( id == CSS_VAL_AUTO ||
	     id == CSS_VAL_AVOID )
	    valid_primitive = true;
	break;

    case CSS_PROP_EMPTY_CELLS:          // show | hide | inherit
	if ( id == CSS_VAL_SHOW ||
	     id == CSS_VAL_HIDE )
	    valid_primitive = true;
	break;

    case CSS_PROP_CONTENT:              // [ <string> | <uri> | <counter> | attr(X) | open-quote |
	// close-quote | no-open-quote | no-close-quote ]+ | inherit
        return parseContent( propId, important );
        break;

    case CSS_PROP_WHITE_SPACE:          // normal | pre | nowrap | inherit
	if ( id == CSS_VAL_NORMAL ||
	     id == CSS_VAL_PRE ||
	     id == CSS_VAL_NOWRAP )
	    valid_primitive = true;
	break;

    case CSS_PROP_CLIP:                 // <shape> | auto | inherit
	if ( id == CSS_VAL_AUTO )
	    valid_primitive = true;
	else if ( value->unit == Value::Function )
	    return parseShape( propId, important );
	break;

#if APPLE_CHANGES
    case CSS_PROP__APPLE_DASHBOARD_REGION:                 // <dashboard-region> | <dashboard-region> 
	if ( value->unit == Value::Function || id == CSS_VAL_NONE)
	    return parseDashboardRegions( propId, important );
	break;
#endif

    /* Start of supported CSS properties with validation. This is needed for parseShortHand to work
     * correctly and allows optimization in khtml::applyRule(..)
     */
    case CSS_PROP_CAPTION_SIDE:         // top | bottom | left | right | inherit
	if (id == CSS_VAL_LEFT || id == CSS_VAL_RIGHT ||
	    id == CSS_VAL_TOP || id == CSS_VAL_BOTTOM)
	    valid_primitive = true;
	break;

    case CSS_PROP_BORDER_COLLAPSE:      // collapse | separate | inherit
	if ( id == CSS_VAL_COLLAPSE || id == CSS_VAL_SEPARATE )
	    valid_primitive = true;
	break;

    case CSS_PROP_VISIBILITY:           // visible | hidden | collapse | inherit
	if (id == CSS_VAL_VISIBLE || id == CSS_VAL_HIDDEN || id == CSS_VAL_COLLAPSE)
	    valid_primitive = true;
	break;

    case CSS_PROP_OVERFLOW:             // visible | hidden | scroll | auto | marquee | overlay | inherit
	if (id == CSS_VAL_VISIBLE || id == CSS_VAL_HIDDEN || id == CSS_VAL_SCROLL || id == CSS_VAL_AUTO ||
            id == CSS_VAL_MARQUEE || id == CSS_VAL_OVERLAY)
	    valid_primitive = true;
	break;

    case CSS_PROP_LIST_STYLE_POSITION:  // inside | outside | inherit
	if ( id == CSS_VAL_INSIDE || id == CSS_VAL_OUTSIDE )
	    valid_primitive = true;
	break;

    case CSS_PROP_LIST_STYLE_TYPE:
        // disc | circle | square | decimal | decimal-leading-zero | lower-roman |
        // upper-roman | lower-greek | lower-alpha | lower-latin | upper-alpha |
        // upper-latin | hebrew | armenian | georgian | cjk-ideographic | hiragana |
        // katakana | hiragana-iroha | katakana-iroha | none | inherit
	if ((id >= CSS_VAL_DISC && id <= CSS_VAL_KATAKANA_IROHA) || id == CSS_VAL_NONE)
	    valid_primitive = true;
	break;

    case CSS_PROP_DISPLAY:
        // inline | block | list-item | run-in | inline-block | table |
        // inline-table | table-row-group | table-header-group | table-footer-group | table-row |
        // table-column-group | table-column | table-cell | table-caption | box | inline-box | none | inherit
	if ((id >= CSS_VAL_INLINE && id <= CSS_VAL__KHTML_INLINE_BOX) || id == CSS_VAL_NONE)
	    valid_primitive = true;
	break;

    case CSS_PROP_DIRECTION:            // ltr | rtl | inherit
	if ( id == CSS_VAL_LTR || id == CSS_VAL_RTL )
	    valid_primitive = true;
	break;

    case CSS_PROP_TEXT_TRANSFORM:       // capitalize | uppercase | lowercase | none | inherit
	if ((id >= CSS_VAL_CAPITALIZE && id <= CSS_VAL_LOWERCASE) || id == CSS_VAL_NONE)
	    valid_primitive = true;
	break;

    case CSS_PROP_FLOAT:                // left | right | none | inherit + center for buggy CSS
	if ( id == CSS_VAL_LEFT || id == CSS_VAL_RIGHT ||
	     id == CSS_VAL_NONE || id == CSS_VAL_CENTER)
	    valid_primitive = true;
	break;

    case CSS_PROP_CLEAR:                // none | left | right | both | inherit
	if ( id == CSS_VAL_NONE || id == CSS_VAL_LEFT ||
	     id == CSS_VAL_RIGHT|| id == CSS_VAL_BOTH)
	    valid_primitive = true;
	break;

    case CSS_PROP_TEXT_ALIGN:
    	// left | right | center | justify | khtml_left | khtml_right | khtml_center | <string> | inherit
	if ( ( id >= CSS_VAL__KHTML_AUTO && id <= CSS_VAL__KHTML_CENTER ) ||
	     value->unit == CSSPrimitiveValue::CSS_STRING )
	    valid_primitive = true;
	break;

    case CSS_PROP_OUTLINE_STYLE:        // <border-style> | auto | inherit
        if (id == CSS_VAL_AUTO) {
            valid_primitive = true;
            break;
        } // Fall through!
    case CSS_PROP_BORDER_TOP_STYLE:     //// <border-style> | inherit
    case CSS_PROP_BORDER_RIGHT_STYLE:   //   Defined as:    none | hidden | dotted | dashed |
    case CSS_PROP_BORDER_BOTTOM_STYLE:  //   solid | double | groove | ridge | inset | outset
    case CSS_PROP_BORDER_LEFT_STYLE:    ////
	if (id >= CSS_VAL_NONE && id <= CSS_VAL_DOUBLE)
	    valid_primitive = true;
	break;

    case CSS_PROP_FONT_WEIGHT:  // normal | bold | bolder | lighter | 100 | 200 | 300 | 400 |
	// 500 | 600 | 700 | 800 | 900 | inherit
	if (id >= CSS_VAL_NORMAL && id <= CSS_VAL_900) {
	    // Allready correct id
	    valid_primitive = true;
	} else if ( validUnit( value, FInteger|FNonNeg, false ) ) {
	    int weight = (int)value->fValue;
	    if ( (weight % 100) )
		break;
	    weight /= 100;
	    if ( weight >= 1 && weight <= 9 ) {
		id = CSS_VAL_100 + weight - 1;
		valid_primitive = true;
	    }
	}
	break;

    case CSS_PROP_BORDER_SPACING: {
        const int properties[2] = { CSS_PROP__KHTML_BORDER_HORIZONTAL_SPACING,
                                    CSS_PROP__KHTML_BORDER_VERTICAL_SPACING };
        int num = valueList->numValues;
        if (num == 1) {
            if (!parseValue(properties[0], important)) return false;
            CSSValueImpl* value = parsedProperties[numParsedProperties-1]->value();
            addProperty(properties[1], value, important);
            return true;
        }
        else if (num == 2) {
            if (!parseValue(properties[0], important)) return false;
            if (!parseValue(properties[1], important)) return false;
            return true;
        }
        return false;
    }
    case CSS_PROP__KHTML_BORDER_HORIZONTAL_SPACING:
    case CSS_PROP__KHTML_BORDER_VERTICAL_SPACING:
        valid_primitive = validUnit(value, FLength|FNonNeg, strict);
        break;
    case CSS_PROP_SCROLLBAR_FACE_COLOR:         // IE5.5
    case CSS_PROP_SCROLLBAR_SHADOW_COLOR:       // IE5.5
    case CSS_PROP_SCROLLBAR_HIGHLIGHT_COLOR:    // IE5.5
    case CSS_PROP_SCROLLBAR_3DLIGHT_COLOR:      // IE5.5
    case CSS_PROP_SCROLLBAR_DARKSHADOW_COLOR:   // IE5.5
    case CSS_PROP_SCROLLBAR_TRACK_COLOR:        // IE5.5
    case CSS_PROP_SCROLLBAR_ARROW_COLOR:        // IE5.5
	if ( strict )
	    break;
	/* nobreak */
    case CSS_PROP_OUTLINE_COLOR:        // <color> | invert | inherit
	// outline has "invert" as additional keyword.
	if ( propId == CSS_PROP_OUTLINE_COLOR && id == CSS_VAL_INVERT ) {
	    valid_primitive = true;
            break;
	}
	/* nobreak */
    case CSS_PROP_BACKGROUND_COLOR:     // <color> | inherit
    case CSS_PROP_BORDER_TOP_COLOR:     // <color> | inherit
    case CSS_PROP_BORDER_RIGHT_COLOR:   // <color> | inherit
    case CSS_PROP_BORDER_BOTTOM_COLOR:  // <color> | inherit
    case CSS_PROP_BORDER_LEFT_COLOR:    // <color> | inherit
    case CSS_PROP_COLOR:                // <color> | inherit
    case CSS_PROP_TEXT_LINE_THROUGH_COLOR: // CSS3 text decoration colors
    case CSS_PROP_TEXT_UNDERLINE_COLOR:
    case CSS_PROP_TEXT_OVERLINE_COLOR:
        if (id == CSS_VAL__KHTML_TEXT)
            valid_primitive = true; // Always allow this, even when strict parsing is on,
                                    // since we use this in our UA sheets.
	else if ( id >= CSS_VAL_AQUA && id <= CSS_VAL_WINDOWTEXT || id == CSS_VAL_MENU ||
	     (id >= CSS_VAL_GREY && id < CSS_VAL__KHTML_TEXT && !strict) ) {
	    valid_primitive = true;
	} else {
	    parsedValue = parseColor();
	    if ( parsedValue )
		valueList->next();
	}
	break;

    case CSS_PROP_CURSOR:
	//  [ auto | crosshair | default | pointer | progress | move | e-resize | ne-resize |
	// nw-resize | n-resize | se-resize | sw-resize | s-resize | w-resize | text |
	// wait | help ] ] | inherit
    // MSIE 5 compatibility :/
        if ( !strict && id == CSS_VAL_HAND ) {
            id = CSS_VAL_POINTER;
	    valid_primitive = true;
        } else if ( id >= CSS_VAL_AUTO && id <= CSS_VAL_HELP )
	    valid_primitive = true;
	break;

    case CSS_PROP_BACKGROUND_ATTACHMENT:
    case CSS_PROP_BACKGROUND_IMAGE:
    case CSS_PROP_BACKGROUND_POSITION:
    case CSS_PROP_BACKGROUND_POSITION_X:
    case CSS_PROP_BACKGROUND_POSITION_Y:
    case CSS_PROP_BACKGROUND_REPEAT: {
        CSSValueImpl *val1 = 0, *val2 = 0;
        int propId1, propId2;
        if (parseBackgroundProperty(propId, propId1, propId2, val1, val2)) {
            addProperty(propId1, val1, important);
            if (val2)
                addProperty(propId2, val2, important);
            return true;
        }
        return false;
    }
    case CSS_PROP_LIST_STYLE_IMAGE:     // <uri> | none | inherit
        if (id == CSS_VAL_NONE) {
	    parsedValue = new CSSImageValueImpl();
            valueList->next();
        }
        else if (value->unit == CSSPrimitiveValue::CSS_URI) {
	    // ### allow string in non strict mode?
	    DOMString uri = khtml::parseURL( domString( value->string ) );
	    if (!uri.isEmpty()) {
		parsedValue = new CSSImageValueImpl(
		    DOMString(KURL( styleElement->baseURL().string(), uri.string()).url()),
		    styleElement);
	    }
	}
        break;

    case CSS_PROP_OUTLINE_WIDTH:        // <border-width> | inherit
    case CSS_PROP_BORDER_TOP_WIDTH:     //// <border-width> | inherit
    case CSS_PROP_BORDER_RIGHT_WIDTH:   //   Which is defined as
    case CSS_PROP_BORDER_BOTTOM_WIDTH:  //   thin | medium | thick | <length>
    case CSS_PROP_BORDER_LEFT_WIDTH:    ////
	if (id == CSS_VAL_THIN || id == CSS_VAL_MEDIUM || id == CSS_VAL_THICK)
	    valid_primitive = true;
        else
            valid_primitive = ( validUnit( value, FLength, strict ) );
	break;

    case CSS_PROP_LETTER_SPACING:       // normal | <length> | inherit
    case CSS_PROP_WORD_SPACING:         // normal | <length> | inherit
	if ( id == CSS_VAL_NORMAL )
	    valid_primitive = true;
	else
            valid_primitive = validUnit( value, FLength, strict );
	break;

    case CSS_PROP_WORD_WRAP:           // normal | break-word
	if (id == CSS_VAL_NORMAL || id == CSS_VAL_BREAK_WORD)
	    valid_primitive = true;
	break;

    case CSS_PROP__KHTML_FONT_SIZE_DELTA:           // <length>
	   valid_primitive = validUnit( value, FLength, strict );
	break;

    case CSS_PROP__KHTML_NBSP_MODE:     // normal | space
	if (id == CSS_VAL_NORMAL || id == CSS_VAL_SPACE)
	    valid_primitive = true;
	break;

    case CSS_PROP__KHTML_LINE_BREAK:   // normal | after-white-space
	if (id == CSS_VAL_NORMAL || id == CSS_VAL_AFTER_WHITE_SPACE)
	    valid_primitive = true;
	break;

    case CSS_PROP__KHTML_MATCH_NEAREST_MAIL_BLOCKQUOTE_COLOR:   // normal | match
	if (id == CSS_VAL_NORMAL || id == CSS_VAL_MATCH)
	    valid_primitive = true;
	break;

    case CSS_PROP_TEXT_INDENT:          // <length> | <percentage> | inherit
    case CSS_PROP_PADDING_TOP:          //// <padding-width> | inherit
    case CSS_PROP_PADDING_RIGHT:        //   Which is defined as
    case CSS_PROP_PADDING_BOTTOM:       //   <length> | <percentage>
    case CSS_PROP_PADDING_LEFT:         ////
    case CSS_PROP__KHTML_PADDING_START:
	valid_primitive = ( !id && validUnit( value, FLength|FPercent, strict ) );
	break;

    case CSS_PROP_MAX_HEIGHT:           // <length> | <percentage> | none | inherit
    case CSS_PROP_MAX_WIDTH:            // <length> | <percentage> | none | inherit
	if (id == CSS_VAL_NONE || id == CSS_VAL_INTRINSIC || id == CSS_VAL_MIN_INTRINSIC) {
	    valid_primitive = true;
	    break;
	}
	/* nobreak */
    case CSS_PROP_MIN_HEIGHT:           // <length> | <percentage> | inherit
    case CSS_PROP_MIN_WIDTH:            // <length> | <percentage> | inherit
        if (id == CSS_VAL_INTRINSIC || id == CSS_VAL_MIN_INTRINSIC)
            valid_primitive = true;
        else
            valid_primitive = ( !id && validUnit( value, FLength|FPercent|FNonNeg, strict ) );
	break;

    case CSS_PROP_FONT_SIZE:
    	// <absolute-size> | <relative-size> | <length> | <percentage> | inherit
	if (id >= CSS_VAL_XX_SMALL && id <= CSS_VAL_LARGER)
	    valid_primitive = true;
	else
	    valid_primitive = ( validUnit( value, FLength|FPercent, strict ) );
	break;

    case CSS_PROP_FONT_STYLE:           // normal | italic | oblique | inherit
	if ( id == CSS_VAL_NORMAL || id == CSS_VAL_ITALIC || id == CSS_VAL_OBLIQUE)
	    valid_primitive = true;
	break;

    case CSS_PROP_FONT_VARIANT:         // normal | small-caps | inherit
	if ( id == CSS_VAL_NORMAL || id == CSS_VAL_SMALL_CAPS)
	    valid_primitive = true;
	break;

    case CSS_PROP_VERTICAL_ALIGN:
    	// baseline | sub | super | top | text-top | middle | bottom | text-bottom |
	// <percentage> | <length> | inherit

	if ( id >= CSS_VAL_BASELINE && id <= CSS_VAL__KHTML_BASELINE_MIDDLE )
	    valid_primitive = true;
	else
	    valid_primitive = ( !id && validUnit( value, FLength|FPercent, strict ) );
	break;

    case CSS_PROP_HEIGHT:               // <length> | <percentage> | auto | inherit
    case CSS_PROP_WIDTH:                // <length> | <percentage> | auto | inherit
	if (id == CSS_VAL_AUTO || id == CSS_VAL_INTRINSIC || id == CSS_VAL_MIN_INTRINSIC)
	    valid_primitive = true;
	else
	    // ### handle multilength case where we allow relative units
	    valid_primitive = ( !id && validUnit( value, FLength|FPercent|FNonNeg, strict ) );
	break;

    case CSS_PROP_BOTTOM:               // <length> | <percentage> | auto | inherit
    case CSS_PROP_LEFT:                 // <length> | <percentage> | auto | inherit
    case CSS_PROP_RIGHT:                // <length> | <percentage> | auto | inherit
    case CSS_PROP_TOP:                  // <length> | <percentage> | auto | inherit
    case CSS_PROP_MARGIN_TOP:           //// <margin-width> | inherit
    case CSS_PROP_MARGIN_RIGHT:         //   Which is defined as
    case CSS_PROP_MARGIN_BOTTOM:        //   <length> | <percentage> | auto | inherit
    case CSS_PROP_MARGIN_LEFT:          ////
    case CSS_PROP__KHTML_MARGIN_START:
	if ( id == CSS_VAL_AUTO )
	    valid_primitive = true;
	else
	    valid_primitive = ( !id && validUnit( value, FLength|FPercent, strict ) );
	break;

    case CSS_PROP_Z_INDEX:              // auto | <integer> | inherit
	// qDebug("parsing z-index: id=%d, fValue=%f", id, value->fValue );
	if ( id == CSS_VAL_AUTO ) {
            valid_primitive = true;
            break;
	}
	/* nobreak */
    case CSS_PROP_ORPHANS:              // <integer> | inherit
    case CSS_PROP_WIDOWS:               // <integer> | inherit
	// ### not supported later on
	valid_primitive = ( !id && validUnit( value, FInteger, false ) );
	break;

    case CSS_PROP_LINE_HEIGHT:          // normal | <number> | <length> | <percentage> | inherit
	if ( id == CSS_VAL_NORMAL )
	    valid_primitive = true;
	else
	    valid_primitive = ( !id && validUnit( value, FNumber|FLength|FPercent, strict ) );
	break;
#if 0
	// removed from CSS 2.1
    case CSS_PROP_COUNTER_INCREMENT:    // [ <identifier> <integer>? ]+ | none | inherit
    case CSS_PROP_COUNTER_RESET:        // [ <identifier> <integer>? ]+ | none | inherit
	if ( id == CSS_VAL_NONE )
	    valid_primitive = true;
	else {
            CSSValueListImpl *list = new CSSValueListImpl;
            int pos=0, pos2;
            while( 1 )
	    {
                pos2 = value.find(',', pos);
                QString face = value.mid(pos, pos2-pos);
                face = face.stripWhiteSpace();
                if(face.length() == 0) break;
                // ### single quoted is missing...
                if(face[0] == '\"') face.remove(0, 1);
                if(face[face.length()-1] == '\"') face = face.left(face.length()-1);
                //kdDebug( 6080 ) << "found face '" << face << "'" << endl;
                list->append(new CSSPrimitiveValueImpl(DOMString(face), CSSPrimitiveValue::CSS_STRING));
                pos = pos2 + 1;
                if(pos2 == -1) break;
	    }
            //kdDebug( 6080 ) << "got " << list->length() << " faces" << endl;
            if(list->length()) {
		parsedValue = list;
		valueList->next();
            } else
		delete list;
            break;
	}
#endif
    case CSS_PROP_FONT_FAMILY:
    	// [[ <family-name> | <generic-family> ],]* [<family-name> | <generic-family>] | inherit
    {
	parsedValue = parseFontFamily();
	break;
    }

    case CSS_PROP_TEXT_DECORATION:
    case CSS_PROP__KHTML_TEXT_DECORATIONS_IN_EFFECT:
    	// none | [ underline || overline || line-through || blink ] | inherit
	if (id == CSS_VAL_NONE) {
	    valid_primitive = true;
	} else {
	    CSSValueListImpl *list = new CSSValueListImpl;
	    bool is_valid = true;
	    while( is_valid && value ) {
		switch ( value->id ) {
		case CSS_VAL_BLINK:
		    break;
		case CSS_VAL_UNDERLINE:
		case CSS_VAL_OVERLINE:
		case CSS_VAL_LINE_THROUGH:
		    list->append( new CSSPrimitiveValueImpl( value->id ) );
		    break;
		default:
		    is_valid = false;
		}
		value = valueList->next();
	    }
	    //kdDebug( 6080 ) << "got " << list->length() << "d decorations" << endl;
	    if(list->length() && is_valid) {
                parsedValue = list;
		valueList->next();
	    } else {
                delete list;
	    }
	}
	break;

    case CSS_PROP_TABLE_LAYOUT:         // auto | fixed | inherit
	if ( id == CSS_VAL_AUTO || id == CSS_VAL_FIXED )
	    valid_primitive = true;
	break;

    /* CSS3 properties */
    case CSS_PROP__KHTML_BINDING:
#ifndef KHTML_NO_XBL
        if (id == CSS_VAL_NONE)
            valid_primitive = true;
        else {
            CSSValueListImpl* values = new CSSValueListImpl();
            Value* val;
            CSSValueImpl* parsedValue = 0;
            while ((val = valueList->current())) {
                if (val->unit == CSSPrimitiveValue::CSS_URI) {
                    DOMString value = khtml::parseURL(domString(val->string));
                    parsedValue = new CSSPrimitiveValueImpl(
                                    DOMString(KURL(styleElement->baseURL().string(), value.string()).url()), 
                                    CSSPrimitiveValue::CSS_URI);
                } 
                
                if (parsedValue)
                    values->append(parsedValue);
                else
                    break;
                valueList->next();
            }
            if ( values->length() ) {
                addProperty( propId, values, important );
                valueList->next();
                return true;
            }
            delete values;
            return false;
        }
#endif
        break;
    case CSS_PROP_OUTLINE_OFFSET:
        valid_primitive = validUnit(value, FLength, strict);
        break;
    case CSS_PROP_TEXT_SHADOW: // CSS2 property, dropped in CSS2.1, back in CSS3, so treat as CSS3
        if (id == CSS_VAL_NONE)
            valid_primitive = true;
        else
            return parseShadow(propId, important);
        break;
    case CSS_PROP_OPACITY:
        valid_primitive = validUnit(value, FNumber, strict);
        break;
    case CSS_PROP__KHTML_BOX_ALIGN:
        if (id == CSS_VAL_STRETCH || id == CSS_VAL_START || id == CSS_VAL_END ||
            id == CSS_VAL_CENTER || id == CSS_VAL_BASELINE)
            valid_primitive = true;
        break;
    case CSS_PROP__KHTML_BOX_DIRECTION:
        if (id == CSS_VAL_NORMAL || id == CSS_VAL_REVERSE)
            valid_primitive = true;
        break;
    case CSS_PROP__KHTML_BOX_LINES:
        if (id == CSS_VAL_SINGLE || id == CSS_VAL_MULTIPLE)
            valid_primitive = true;
        break;
    case CSS_PROP__KHTML_BOX_ORIENT:
        if (id == CSS_VAL_HORIZONTAL || id == CSS_VAL_VERTICAL ||
            id == CSS_VAL_INLINE_AXIS || id == CSS_VAL_BLOCK_AXIS)
            valid_primitive = true;
        break;
    case CSS_PROP__KHTML_BOX_PACK:
        if (id == CSS_VAL_START || id == CSS_VAL_END ||
            id == CSS_VAL_CENTER || id == CSS_VAL_JUSTIFY)
            valid_primitive = true;
        break;
    case CSS_PROP__KHTML_BOX_FLEX:
        valid_primitive = validUnit(value, FNumber, strict);
        break;
    case CSS_PROP__KHTML_BOX_FLEX_GROUP:
    case CSS_PROP__KHTML_BOX_ORDINAL_GROUP:
        valid_primitive = validUnit(value, FInteger|FNonNeg, true);
        break;
    case CSS_PROP__KHTML_MARQUEE: {
        const int properties[5] = { CSS_PROP__KHTML_MARQUEE_DIRECTION, CSS_PROP__KHTML_MARQUEE_INCREMENT,
                                    CSS_PROP__KHTML_MARQUEE_REPETITION,
                                    CSS_PROP__KHTML_MARQUEE_STYLE, CSS_PROP__KHTML_MARQUEE_SPEED };
        return parseShortHand(properties, 5, important);
    }
    case CSS_PROP__KHTML_MARQUEE_DIRECTION:
        if (id == CSS_VAL_FORWARDS || id == CSS_VAL_BACKWARDS || id == CSS_VAL_AHEAD ||
            id == CSS_VAL_REVERSE || id == CSS_VAL_LEFT || id == CSS_VAL_RIGHT || id == CSS_VAL_DOWN ||
            id == CSS_VAL_UP || id == CSS_VAL_AUTO)
            valid_primitive = true;
        break;
    case CSS_PROP__KHTML_MARQUEE_INCREMENT:
        if (id == CSS_VAL_SMALL || id == CSS_VAL_LARGE || id == CSS_VAL_MEDIUM)
            valid_primitive = true;
        else
            valid_primitive = validUnit(value, FLength|FPercent, strict);
        break;
    case CSS_PROP__KHTML_MARQUEE_STYLE:
        if (id == CSS_VAL_NONE || id == CSS_VAL_SLIDE || id == CSS_VAL_SCROLL || id == CSS_VAL_ALTERNATE ||
            id == CSS_VAL_UNFURL)
            valid_primitive = true;
        break;
    case CSS_PROP__KHTML_MARQUEE_REPETITION:
        if (id == CSS_VAL_INFINITE)
            valid_primitive = true;
        else
            valid_primitive = validUnit(value, FInteger|FNonNeg, strict);
        break;
    case CSS_PROP__KHTML_MARQUEE_SPEED:
        if (id == CSS_VAL_NORMAL || id == CSS_VAL_SLOW || id == CSS_VAL_FAST)
            valid_primitive = true;
        else
            valid_primitive = validUnit(value, FTime|FInteger|FNonNeg, strict);
        break;
    case CSS_PROP__KHTML_USER_DRAG: // auto | none | element
        if (id == CSS_VAL_AUTO || id == CSS_VAL_NONE || id == CSS_VAL_ELEMENT)
            valid_primitive = true;
        break;
    case CSS_PROP__KHTML_USER_MODIFY:	// read-only | read-write
        if (id == CSS_VAL_READ_ONLY || id == CSS_VAL_READ_WRITE)
            valid_primitive = true;
        break;
    case CSS_PROP__KHTML_USER_SELECT: // auto | none | text
        if (id == CSS_VAL_AUTO || id == CSS_VAL_NONE || id == CSS_VAL_TEXT)
            valid_primitive = true;
        break;
    case CSS_PROP_TEXT_OVERFLOW: // clip | ellipsis
        if (id == CSS_VAL_CLIP || id == CSS_VAL_ELLIPSIS)
            valid_primitive = true;
        break;
    case CSS_PROP__KHTML_MARGIN_COLLAPSE: {
        const int properties[2] = { CSS_PROP__KHTML_MARGIN_TOP_COLLAPSE,
            CSS_PROP__KHTML_MARGIN_BOTTOM_COLLAPSE };
        int num = valueList->numValues;
        if (num == 1) {
            if (!parseValue(properties[0], important)) return false;
            CSSValueImpl* value = parsedProperties[numParsedProperties-1]->value();
            addProperty(properties[1], value, important);
            return true;
        }
        else if (num == 2) {
            if (!parseValue(properties[0], important)) return false;
            if (!parseValue(properties[1], important)) return false;
            return true;
        }
        return false;
    }
    case CSS_PROP__KHTML_MARGIN_TOP_COLLAPSE:
    case CSS_PROP__KHTML_MARGIN_BOTTOM_COLLAPSE:
        if (id == CSS_VAL_COLLAPSE || id == CSS_VAL_SEPARATE || id == CSS_VAL_DISCARD)
            valid_primitive = true;
        break;
    case CSS_PROP_TEXT_LINE_THROUGH_MODE:
    case CSS_PROP_TEXT_OVERLINE_MODE:
    case CSS_PROP_TEXT_UNDERLINE_MODE:
        if (id == CSS_VAL_CONTINUOUS || id == CSS_VAL_SKIP_WHITE_SPACE)
            valid_primitive = true;
        break;
    case CSS_PROP_TEXT_LINE_THROUGH_STYLE:
    case CSS_PROP_TEXT_OVERLINE_STYLE:
    case CSS_PROP_TEXT_UNDERLINE_STYLE:
        if (id == CSS_VAL_NONE || id == CSS_VAL_SOLID || id == CSS_VAL_DOUBLE ||
            id == CSS_VAL_DASHED || id == CSS_VAL_DOT_DASH || id == CSS_VAL_DOT_DOT_DASH ||
            id == CSS_VAL_WAVE)
            valid_primitive = true;
        break;
    case CSS_PROP_TEXT_LINE_THROUGH_WIDTH:
    case CSS_PROP_TEXT_OVERLINE_WIDTH:
    case CSS_PROP_TEXT_UNDERLINE_WIDTH:
        if (id == CSS_VAL_AUTO || id == CSS_VAL_NORMAL || id == CSS_VAL_THIN ||
            id == CSS_VAL_MEDIUM || id == CSS_VAL_THICK)
            valid_primitive = true;
        else
	    valid_primitive = !id && validUnit(value, FNumber|FLength|FPercent, strict);
        break;
    
    // End of CSS3 properties

#if APPLE_CHANGES
    // Apple specific properties.  These will never be standardized and are purely to
    // support custom WebKit-based Apple applications.
    case CSS_PROP__APPLE_LINE_CLAMP:
        valid_primitive = (!id && validUnit(value, FPercent, false));
        break;
    case CSS_PROP__APPLE_TEXT_SIZE_ADJUST:
        if (id == CSS_VAL_AUTO || id == CSS_VAL_NONE)
            valid_primitive = true;
        break;
#endif

	/* shorthand properties */
    case CSS_PROP_BACKGROUND:
    	// ['background-color' || 'background-image' ||'background-repeat' ||
	// 'background-attachment' || 'background-position'] | inherit
	return parseBackgroundShorthand(important);
    case CSS_PROP_BORDER:
 	// [ 'border-width' || 'border-style' || <color> ] | inherit
    {
	const int properties[3] = { CSS_PROP_BORDER_WIDTH, CSS_PROP_BORDER_STYLE,
				    CSS_PROP_BORDER_COLOR };
	return parseShortHand(properties, 3, important);
    }
    case CSS_PROP_BORDER_TOP:
    	// [ 'border-top-width' || 'border-style' || <color> ] | inherit
    {
	const int properties[3] = { CSS_PROP_BORDER_TOP_WIDTH, CSS_PROP_BORDER_TOP_STYLE,
				    CSS_PROP_BORDER_TOP_COLOR};
	return parseShortHand(properties, 3, important);
    }
    case CSS_PROP_BORDER_RIGHT:
    	// [ 'border-right-width' || 'border-style' || <color> ] | inherit
    {
	const int properties[3] = { CSS_PROP_BORDER_RIGHT_WIDTH, CSS_PROP_BORDER_RIGHT_STYLE,
				    CSS_PROP_BORDER_RIGHT_COLOR };
	return parseShortHand(properties, 3, important);
    }
    case CSS_PROP_BORDER_BOTTOM:
    	// [ 'border-bottom-width' || 'border-style' || <color> ] | inherit
    {
	const int properties[3] = { CSS_PROP_BORDER_BOTTOM_WIDTH, CSS_PROP_BORDER_BOTTOM_STYLE,
				    CSS_PROP_BORDER_BOTTOM_COLOR };
	return parseShortHand(properties, 3, important);
    }
    case CSS_PROP_BORDER_LEFT:
    	// [ 'border-left-width' || 'border-style' || <color> ] | inherit
    {
	const int properties[3] = { CSS_PROP_BORDER_LEFT_WIDTH, CSS_PROP_BORDER_LEFT_STYLE,
				    CSS_PROP_BORDER_LEFT_COLOR };
	return parseShortHand(properties, 3, important);
    }
    case CSS_PROP_OUTLINE:
    	// [ 'outline-color' || 'outline-style' || 'outline-width' ] | inherit
    {
	const int properties[3] = { CSS_PROP_OUTLINE_WIDTH, CSS_PROP_OUTLINE_STYLE,
				    CSS_PROP_OUTLINE_COLOR };
	return parseShortHand(properties, 3, important);
    }
    case CSS_PROP_BORDER_COLOR:
    	// <color>{1,4} | inherit
    {
	const int properties[4] = { CSS_PROP_BORDER_TOP_COLOR, CSS_PROP_BORDER_RIGHT_COLOR,
				    CSS_PROP_BORDER_BOTTOM_COLOR, CSS_PROP_BORDER_LEFT_COLOR };
	return parse4Values(properties, important);
    }
    case CSS_PROP_BORDER_WIDTH:
    	// <border-width>{1,4} | inherit
    {
	const int properties[4] = { CSS_PROP_BORDER_TOP_WIDTH, CSS_PROP_BORDER_RIGHT_WIDTH,
				    CSS_PROP_BORDER_BOTTOM_WIDTH, CSS_PROP_BORDER_LEFT_WIDTH };
	return parse4Values(properties, important);
    }
    case CSS_PROP_BORDER_STYLE:
    	// <border-style>{1,4} | inherit
    {
	const int properties[4] = { CSS_PROP_BORDER_TOP_STYLE, CSS_PROP_BORDER_RIGHT_STYLE,
				    CSS_PROP_BORDER_BOTTOM_STYLE, CSS_PROP_BORDER_LEFT_STYLE };
	return parse4Values(properties, important);
    }
    case CSS_PROP_MARGIN:
    	// <margin-width>{1,4} | inherit
    {
	const int properties[4] = { CSS_PROP_MARGIN_TOP, CSS_PROP_MARGIN_RIGHT,
				    CSS_PROP_MARGIN_BOTTOM, CSS_PROP_MARGIN_LEFT };
	return parse4Values(properties, important);
    }
    case CSS_PROP_PADDING:
    	// <padding-width>{1,4} | inherit
    {
	const int properties[4] = { CSS_PROP_PADDING_TOP, CSS_PROP_PADDING_RIGHT,
				    CSS_PROP_PADDING_BOTTOM, CSS_PROP_PADDING_LEFT };
	return parse4Values(properties, important);
    }
    case CSS_PROP_FONT:
    	// [ [ 'font-style' || 'font-variant' || 'font-weight' ]? 'font-size' [ / 'line-height' ]?
	// 'font-family' ] | caption | icon | menu | message-box | small-caption | status-bar | inherit
	if ( id >= CSS_VAL_CAPTION && id <= CSS_VAL_STATUS_BAR )
	    valid_primitive = true;
	else
	    return parseFont(important);

    case CSS_PROP_LIST_STYLE:
    {
	const int properties[3] = { CSS_PROP_LIST_STYLE_TYPE, CSS_PROP_LIST_STYLE_POSITION,
				    CSS_PROP_LIST_STYLE_IMAGE };
	return parseShortHand(properties, 3, important);
    }
    default:
// #ifdef CSS_DEBUG
// 	kdDebug( 6080 ) << "illegal or CSS2 Aural property: " << val << endl;
// #endif
	break;
    }

    if ( valid_primitive ) {
	if ( id != 0 ) {
	    // qDebug(" new value: id=%d", id );
	    parsedValue = new CSSPrimitiveValueImpl( id );
	} else if ( value->unit == CSSPrimitiveValue::CSS_STRING )
	    parsedValue = new CSSPrimitiveValueImpl( domString( value->string ),
						     (CSSPrimitiveValue::UnitTypes) value->unit );
	else if ( value->unit >= CSSPrimitiveValue::CSS_NUMBER &&
		  value->unit <= CSSPrimitiveValue::CSS_KHZ ) {
	    // qDebug(" new value: value=%.2f, unit=%d", value->fValue, value->unit );
	    parsedValue = new CSSPrimitiveValueImpl( value->fValue,
						     (CSSPrimitiveValue::UnitTypes) value->unit );
	} else if ( value->unit >= Value::Q_EMS ) {
	    // qDebug(" new quirks value: value=%.2f, unit=%d", value->fValue, value->unit );
	    parsedValue = new CSSQuirkPrimitiveValueImpl( value->fValue, CSSPrimitiveValue::CSS_EMS );
	}
	valueList->next();
    }
    if ( parsedValue ) {
	addProperty( propId, parsedValue, important );
	return true;
    }
    return false;
}

void CSSParser::addBackgroundValue(CSSValueImpl*& lval, CSSValueImpl* rval)
{
    if (lval) {
        if (lval->isValueList())
            static_cast<CSSValueListImpl*>(lval)->append(rval);
        else {
            CSSValueImpl* oldVal = lval;
            CSSValueListImpl* list = new CSSValueListImpl();
            lval = list;
            list->append(oldVal);
            list->append(rval);
        }
    }
    else
        lval = rval;
}

bool CSSParser::parseBackgroundShorthand(bool important)
{
    // Position must come before color in this array because a plain old "0" is a legal color
    // in quirks mode but it's usually the X coordinate of a position.
    const int numProperties = 5;
    const int properties[numProperties] = { CSS_PROP_BACKGROUND_IMAGE, CSS_PROP_BACKGROUND_REPEAT,
        CSS_PROP_BACKGROUND_ATTACHMENT, CSS_PROP_BACKGROUND_POSITION, CSS_PROP_BACKGROUND_COLOR };
    
    inParseShortHand = true;
    
    bool parsedProperty[numProperties] = { false }; // compiler will repeat false as necessary
    CSSValueImpl* values[numProperties] = { 0 }; // compiler will repeat 0 as necessary
    CSSValueImpl* positionYValue = 0;
    int i;

    while (valueList->current()) {
        Value* val = valueList->current();
        if (val->unit == Value::Operator && val->iValue == ',') {
            // We hit the end.  Fill in all remaining values with the initial value.
            valueList->next();
            for (i = 0; i < numProperties; ++i) {
                if (properties[i] == CSS_PROP_BACKGROUND_COLOR && parsedProperty[i])
                    // Color is not allowed except as the last item in a list.  Reject the entire
                    // property.
                    goto fail;

                if (!parsedProperty[i] && properties[i] != CSS_PROP_BACKGROUND_COLOR) {
                    addBackgroundValue(values[i], new CSSInitialValueImpl());
                    if (properties[i] == CSS_PROP_BACKGROUND_POSITION)
                        addBackgroundValue(positionYValue, new CSSInitialValueImpl());
                }
                parsedProperty[i] = false;
            }
            if (!valueList->current())
                break;
        }
        
        bool found = false;
        for (i = 0; !found && i < numProperties; ++i) {
            if (!parsedProperty[i]) {
                CSSValueImpl *val1 = 0, *val2 = 0;
                int propId1, propId2;
		if (parseBackgroundProperty(properties[i], propId1, propId2, val1, val2)) {
		    parsedProperty[i] = found = true;
                    addBackgroundValue(values[i], val1);
                    if (properties[i] == CSS_PROP_BACKGROUND_POSITION)
                        addBackgroundValue(positionYValue, val2);
		}
	    }
	}

        // if we didn't find at least one match, this is an
        // invalid shorthand and we have to ignore it
        if (!found)
            goto fail;
    }
    
    // Fill in any remaining properties with the initial value.
    for (i = 0; i < numProperties; ++i) {
        if (!parsedProperty[i]) {
            addBackgroundValue(values[i], new CSSInitialValueImpl());
            if (properties[i] == CSS_PROP_BACKGROUND_POSITION)
                addBackgroundValue(positionYValue, new CSSInitialValueImpl());
        }
    }
    
    // Now add all of the properties we found.
    for (i = 0; i < numProperties; i++) {
        if (properties[i] == CSS_PROP_BACKGROUND_POSITION) {
            addProperty(CSS_PROP_BACKGROUND_POSITION_X, values[i], important);
            addProperty(CSS_PROP_BACKGROUND_POSITION_Y, positionYValue, important);
        }
        else
            addProperty(properties[i], values[i], important);
    }
    
    inParseShortHand = false;
    return true;

fail:
    inParseShortHand = false;
    for (int k = 0; k < numProperties; k++)
        delete values[k];
    delete positionYValue;
    return false;
}

bool CSSParser::parseShortHand( const int *properties, int numProperties, bool important )
{
    /* We try to match as many properties as possible
     * We setup an array of booleans to mark which property has been found,
     * and we try to search for properties until it makes no longer any sense
     */
    inParseShortHand = true;

    bool found = false;
    bool fnd[6]; //Trust me ;)
    for( int i = 0; i < numProperties; i++ )
    	fnd[i] = false;

#ifdef CSS_DEBUG
    kdDebug(6080) << "PSH: numProperties=" << numProperties << endl;
#endif

    while ( valueList->current() ) {
        found = false;
	// qDebug("outer loop" );
        for (int propIndex = 0; !found && propIndex < numProperties; ++propIndex) {
            if (!fnd[propIndex]) {
#ifdef CSS_DEBUG
		kdDebug(6080) << "LOOKING FOR: " << getPropertyName(properties[propIndex]).string() << endl;
#endif
		if ( parseValue( properties[propIndex], important ) ) {
		    fnd[propIndex] = found = true;
#ifdef CSS_DEBUG
		    kdDebug(6080) << "FOUND: " << getPropertyName(properties[propIndex]).string() << endl;
#endif
		}
	    }
	}
        // if we didn't find at least one match, this is an
        // invalid shorthand and we have to ignore it
        if (!found) {
#ifdef CSS_DEBUG
	    qDebug("didn't find anything" );
#endif
	    inParseShortHand = false;
	    return false;
	}
    }
    
    // Fill in any remaining properties with the initial value.
    for (int i = 0; i < numProperties; ++i) {
        if (!fnd[i])
            addProperty(properties[i], new CSSInitialValueImpl(), important);
    }
    
    inParseShortHand = false;
#ifdef CSS_DEBUG
    kdDebug( 6080 ) << "parsed shorthand" << endl;
#endif
    return true;
}

bool CSSParser::parse4Values( const int *properties,  bool important )
{
    /* From the CSS 2 specs, 8.3
     * If there is only one value, it applies to all sides. If there are two values, the top and
     * bottom margins are set to the first value and the right and left margins are set to the second.
     * If there are three values, the top is set to the first value, the left and right are set to the
     * second, and the bottom is set to the third. If there are four values, they apply to the top,
     * right, bottom, and left, respectively.
     */

    int num = inParseShortHand ? 1 : valueList->numValues;
    // qDebug("parse4Values: num=%d", num );

    // the order is top, right, bottom, left
    switch( num ) {
    case 1: {
        if( !parseValue( properties[0], important ) ) return false;
	CSSValueImpl *value = parsedProperties[numParsedProperties-1]->value();
	addProperty( properties[1], value, important );
	addProperty( properties[2], value, important );
	addProperty( properties[3], value, important );
        return true;
    }
    case 2: {

        if( !parseValue( properties[0], important ) ) return false;
        if( !parseValue( properties[1], important ) ) return false;
	CSSValueImpl *value = parsedProperties[numParsedProperties-2]->value();
	addProperty( properties[2], value, important );
	value = parsedProperties[numParsedProperties-2]->value();
	addProperty( properties[3], value, important );
        return true;
    }
    case 3: {
        if( !parseValue( properties[0], important ) ) return false;
        if( !parseValue( properties[1], important ) ) return false;
        if( !parseValue( properties[2], important ) ) return false;
	CSSValueImpl *value = parsedProperties[numParsedProperties-2]->value();
	addProperty( properties[3], value, important );
        return true;
    }
    case 4: {
        if( !parseValue( properties[0], important ) ) return false;
        if( !parseValue( properties[1], important ) ) return false;
        if( !parseValue( properties[2], important ) ) return false;
        if( !parseValue( properties[3], important ) ) return false;
	return true;
    }
    default:
        return false;
    }
}

// [ <string> | <uri> | <counter> | attr(X) | open-quote | close-quote | no-open-quote | no-close-quote ]+ | inherit
// in CSS 2.1 this got somewhat reduced:
// [ <string> | attr(X) | open-quote | close-quote | no-open-quote | no-close-quote ]+ | inherit
bool CSSParser::parseContent( int propId, bool important )
{
    CSSValueListImpl* values = new CSSValueListImpl();

    Value *val;
    CSSValueImpl *parsedValue = 0;
    while ( (val = valueList->current()) ) {
        if ( val->unit == CSSPrimitiveValue::CSS_URI ) {
            // url
	    DOMString value = khtml::parseURL(domString(val->string));
            parsedValue = new CSSImageValueImpl(
		DOMString(KURL( styleElement->baseURL().string(), value.string()).url() ), styleElement );
#ifdef CSS_DEBUG
	    kdDebug( 6080 ) << "content, url=" << value.string() << " base=" << styleElement->baseURL().string() << endl;
#endif
        } else if ( val->unit == Value::Function ) {
	    // attr( X )
            ValueList *args = val->function->args;
            QString fname = qString( val->function->name ).lower();
            if ( fname != "attr(" || !args )
                return false;
            if ( args->numValues != 1)
                return false;
            Value *a = args->current();
            parsedValue = new CSSPrimitiveValueImpl(domString(a->string), CSSPrimitiveValue::CSS_ATTR);
        } else if ( val->unit == CSSPrimitiveValue::CSS_IDENT ) {
            // open-quote
            // close-quote
            // no-open-quote
            // no-close-quote
        } else if ( val->unit == CSSPrimitiveValue::CSS_STRING ) {
            parsedValue = new CSSPrimitiveValueImpl(domString(val->string), CSSPrimitiveValue::CSS_STRING);
        }
        if (parsedValue)
            values->append(parsedValue);
	else
	    break;
	valueList->next();
    }
    if ( values->length() ) {
	addProperty( propId, values, important );
	valueList->next();
	return true;
    }
    delete values;
    return false;
}

CSSValueImpl* CSSParser::parseBackgroundColor()
{
    int id = valueList->current()->id;
    if (id == CSS_VAL__KHTML_TEXT || (id >= CSS_VAL_AQUA && id <= CSS_VAL_WINDOWTEXT) || id == CSS_VAL_MENU ||
        (id >= CSS_VAL_GREY && id < CSS_VAL__KHTML_TEXT && !strict))
       return new CSSPrimitiveValueImpl(id);
    return parseColor();
}

CSSValueImpl* CSSParser::parseBackgroundImage()
{
    if (valueList->current()->id == CSS_VAL_NONE)
        return new CSSImageValueImpl();
    if (valueList->current()->unit == CSSPrimitiveValue::CSS_URI) {
        DOMString uri = khtml::parseURL(domString(valueList->current()->string));
        if (!uri.isEmpty())
            return new CSSImageValueImpl(DOMString(KURL(styleElement->baseURL().string(), uri.string()).url()), 
                                         styleElement);
    }
    return 0;
}

CSSValueImpl* CSSParser::parseBackgroundPositionXY(bool& xFound, bool& yFound)
{
    int id = valueList->current()->id;
    if (id == CSS_VAL_LEFT || id == CSS_VAL_TOP || id == CSS_VAL_RIGHT || id == CSS_VAL_BOTTOM || id == CSS_VAL_CENTER) {
        int percent = 0;
        if (id == CSS_VAL_LEFT || id == CSS_VAL_RIGHT) {
            if (xFound)
                return 0;
            xFound = true;
            if (id == CSS_VAL_RIGHT)
                percent = 100;
        }
        else if (id == CSS_VAL_TOP || id == CSS_VAL_BOTTOM) {
            if (yFound)
                return 0;
            yFound = true;
            if (id == CSS_VAL_BOTTOM)
                percent = 100;
        }
        else if (id == CSS_VAL_CENTER)
            // Center is ambiguous, so we're not sure which position we've found yet, an x or a y.
            percent = 50;
        return new CSSPrimitiveValueImpl(percent, CSSPrimitiveValue::CSS_PERCENTAGE);
    }
    if (validUnit(valueList->current(), FPercent|FLength, strict))
        return new CSSPrimitiveValueImpl(valueList->current()->fValue,
                                         (CSSPrimitiveValue::UnitTypes)valueList->current()->unit);
                
    return 0;
}

void CSSParser::parseBackgroundPosition(CSSValueImpl*& value1, CSSValueImpl*& value2)
{
    value1 = value2 = 0;
    Value* value = valueList->current();
    
    // Parse the first value.  We're just making sure that it is one of the valid keywords or a percentage/length.
    bool value1IsX = false, value1IsY = false;
    value1 = parseBackgroundPositionXY(value1IsX, value1IsY);
    if (!value1)
        return;
    
    // It only takes one value for background-position to be correctly parsed if it was specified in a shorthand (since we
    // can assume that any other values belong to the rest of the shorthand).  If we're not parsing a shorthand, though, the
    // value was explicitly specified for our property.
    value = valueList->next();
    
    // First check for the comma.  If so, we are finished parsing this value or value pair.
    if (value && value->unit == Value::Operator && value->iValue == ',')
        value = 0;
    
    bool value2IsX = false, value2IsY = false;
    if (value) {
        value2 = parseBackgroundPositionXY(value2IsX, value2IsY);
        if (value2)
            valueList->next();
        else {
            if (!inParseShortHand) {
                delete value1;
                value1 = 0;
                return;
            }
        }
    }
    
    if (!value2)
        // Only one value was specified.  If that value was not a keyword, then it sets the x position, and the y position
        // is simply 50%.  This is our default.
        // For keywords, the keyword was either an x-keyword (left/right), a y-keyword (top/bottom), or an ambiguous keyword (center).
        // For left/right/center, the default of 50% in the y is still correct.
        value2 = new CSSPrimitiveValueImpl(50, CSSPrimitiveValue::CSS_PERCENTAGE);

    if (value1IsY || value2IsX) {
        // Swap our two values.
        CSSValueImpl* val = value2;
        value2 = value1;
        value1 = val;
    }
}

bool CSSParser::parseBackgroundProperty(int propId, int& propId1, int& propId2, 
                                        CSSValueImpl*& retValue1, CSSValueImpl*& retValue2)
{
    CSSValueListImpl *values = 0, *values2 = 0;
    Value* val;
    CSSValueImpl *value = 0, *value2 = 0;
    bool allowComma = false;
    
    retValue1 = retValue2 = 0;
    propId1 = propId;
    propId2 = propId;
    if (propId == CSS_PROP_BACKGROUND_POSITION) {
        propId1 = CSS_PROP_BACKGROUND_POSITION_X;
        propId2 = CSS_PROP_BACKGROUND_POSITION_Y;
    }

    while ((val = valueList->current())) {
        CSSValueImpl *currValue = 0, *currValue2 = 0;
        if (allowComma) {
            if (val->unit != Value::Operator || val->iValue != ',')
                goto failed;
            valueList->next();
            allowComma = false;
        }
        else {
            switch (propId) {
                case CSS_PROP_BACKGROUND_ATTACHMENT:
                    if (val->id == CSS_VAL_SCROLL || val->id == CSS_VAL_FIXED) {
                        currValue = new CSSPrimitiveValueImpl(val->id);
                        valueList->next();
                    }
                    break;
                case CSS_PROP_BACKGROUND_COLOR:
                    currValue = parseBackgroundColor();
                    if (currValue)
                        valueList->next();
                    break;
                case CSS_PROP_BACKGROUND_IMAGE:
                    currValue = parseBackgroundImage();
                    if (currValue)
                        valueList->next();
                    break;
                case CSS_PROP_BACKGROUND_POSITION:
                    parseBackgroundPosition(currValue, currValue2);
                    // unlike the other functions, parseBackgroundPosition advances the valueList pointer
                    break;
                case CSS_PROP_BACKGROUND_POSITION_X: {
                    bool xFound = false, yFound = true;
                    currValue = parseBackgroundPositionXY(xFound, yFound);
                    if (currValue)
                        valueList->next();
                    break;
                }
                case CSS_PROP_BACKGROUND_POSITION_Y: {
                    bool xFound = true, yFound = false;
                    currValue = parseBackgroundPositionXY(xFound, yFound);
                    if (currValue)
                        valueList->next();
                    break;
                }
                case CSS_PROP_BACKGROUND_REPEAT:
                    if (val->id >= CSS_VAL_REPEAT && val->id <= CSS_VAL_NO_REPEAT) {
                        currValue = new CSSPrimitiveValueImpl(val->id);
                        valueList->next();
                    }
                    break;
            }
            
            if (!currValue)
                goto failed;
            
            if (value && !values) {
                values = new CSSValueListImpl();
                values->append(value);
                value = 0;
            }
            
            if (value2 && !values2) {
                values2 = new CSSValueListImpl();
                values2->append(value2);
                value2 = 0;
            }
            
            if (values)
                values->append(currValue);
            else
                value = currValue;
            if (currValue2) {
                if (values2)
                    values2->append(currValue2);
                else
                    value2 = currValue2;
            }
            allowComma = true;
        }
        
        // When parsing the 'background' shorthand property, we let it handle building up the lists for all
        // properties.
        if (inParseShortHand)
            break;
    }
    
    if (values && values->length()) {
        retValue1 = values;
        if (values2 && values2->length())
            retValue2 = values2;
        return true;
    }
    if (value) {
        retValue1 = value;
        retValue2 = value2;
        return true;
    }

failed:
    delete values; delete values2;
    delete value; delete value2;
    return false;
}

#define DASHBOARD_REGION_NUM_PARAMETERS  6
#define DASHBOARD_REGION_SHORT_NUM_PARAMETERS  2

static Value *skipCommaInDashboardRegion (ValueList *args)
{
    if ( args->numValues == (DASHBOARD_REGION_NUM_PARAMETERS*2-1) ||
         args->numValues == (DASHBOARD_REGION_SHORT_NUM_PARAMETERS*2-1)) {
        Value *current = args->current();
        if (current->unit == Value::Operator && current->iValue == ',' )
            return args->next();
    }
    return args->current();
}

#if APPLE_CHANGES
bool CSSParser::parseDashboardRegions( int propId, bool important )
{
    bool valid = true;
    
    Value *value = valueList->current();

    if (value->id == CSS_VAL_NONE) {
        addProperty( propId, new CSSPrimitiveValueImpl( value->id ), important );
        return valid;
    }
        
    DashboardRegionImpl *firstRegion = new DashboardRegionImpl(), *region = 0;

    while (value) {
        if (region == 0) {
            region = firstRegion;
        }
        else {
            DashboardRegionImpl *nextRegion = new DashboardRegionImpl();
            region->setNext (nextRegion);
            region = nextRegion;
        }
        
        if ( value->unit != Value::Function) {
            valid = false;
            break;
        }
            
        // Commas count as values, so allow:
        // dashboard-region(label, type, t, r, b, l) or dashboard-region(label type t r b l)
        // dashboard-region(label, type, t, r, b, l) or dashboard-region(label type t r b l)
        // also allow
        // dashboard-region(label, type) or dashboard-region(label type)
        // dashboard-region(label, type) or dashboard-region(label type)
        ValueList *args = value->function->args;
        int numArgs = value->function->args->numValues;
        if ((numArgs != DASHBOARD_REGION_NUM_PARAMETERS && numArgs != (DASHBOARD_REGION_NUM_PARAMETERS*2-1)) &&
            (numArgs != DASHBOARD_REGION_SHORT_NUM_PARAMETERS && numArgs != (DASHBOARD_REGION_SHORT_NUM_PARAMETERS*2-1))){
            valid = false;
            break;
        }
        
        QString fname = qString( value->function->name ).lower();
        if (fname != "dashboard-region(" ) {
            valid = false;
            break;
        }
            
        // First arg is a label.
        Value *arg = args->current();
        if (arg->unit != CSSPrimitiveValue::CSS_IDENT) {
            valid = false;
            break;
        }
            
        region->m_label = qString(arg->string);

        // Second arg is a type.
        arg = args->next();
        arg = skipCommaInDashboardRegion (args);
        if (arg->unit != CSSPrimitiveValue::CSS_IDENT) {
            valid = false;
            break;
        }

        QString geometryType = qString(arg->string).lower();
        if (geometryType == "circle")
            region->m_isCircle = true;
        else if (geometryType == "rectangle")
            region->m_isRectangle = true;
        else {
            valid = false;
            break;
        }
            
        region->m_geometryType = qString(arg->string);

        if (numArgs == DASHBOARD_REGION_SHORT_NUM_PARAMETERS || numArgs == (DASHBOARD_REGION_SHORT_NUM_PARAMETERS*2-1)) {
            CSSPrimitiveValueImpl *amount = arg->id == CSS_VAL_AUTO ?
                new CSSPrimitiveValueImpl(CSS_VAL_AUTO) :
                new CSSPrimitiveValueImpl((double)0, (CSSPrimitiveValue::UnitTypes) arg->unit );
                
            region->setTop( amount );
            region->setRight( amount );
            region->setBottom( amount );
            region->setLeft( amount );
        }
        else {
            // Next four arguments must be offset numbers
            int i;
            for (i = 0; i < 4; i++) {
                arg = args->next();
                arg = skipCommaInDashboardRegion (args);

                valid = arg->id == CSS_VAL_AUTO || validUnit( arg, FLength, strict );
                if ( !valid )
                    break;
                    
                CSSPrimitiveValueImpl *amount = arg->id == CSS_VAL_AUTO ?
                    new CSSPrimitiveValueImpl(CSS_VAL_AUTO) :
                    new CSSPrimitiveValueImpl(arg->fValue, (CSSPrimitiveValue::UnitTypes) arg->unit );
                    
                if ( i == 0 )
                    region->setTop( amount );
                else if ( i == 1 )
                    region->setRight( amount );
                else if ( i == 2 )
                    region->setBottom( amount );
                else
                    region->setLeft( amount );
            }
        }

        value = valueList->next();
    }

    if (valid)
        addProperty( propId, new CSSPrimitiveValueImpl( firstRegion ), important );
    else
        delete firstRegion;
        
    return valid;
}
#endif

bool CSSParser::parseShape( int propId, bool important )
{
    Value *value = valueList->current();
    ValueList *args = value->function->args;
    QString fname = qString( value->function->name ).lower();
    if ( fname != "rect(" || !args )
	return false;

    // rect( t, r, b, l ) || rect( t r b l )
    if ( args->numValues != 4 && args->numValues != 7 )
	return false;
    RectImpl *rect = new RectImpl();
    bool valid = true;
    int i = 0;
    Value *a = args->current();
    while ( a ) {
	valid = a->id == CSS_VAL_AUTO || validUnit( a, FLength, strict );
	if ( !valid )
	    break;
	CSSPrimitiveValueImpl *length = a->id == CSS_VAL_AUTO ?
            new CSSPrimitiveValueImpl(CSS_VAL_AUTO) :
	    new CSSPrimitiveValueImpl( a->fValue, (CSSPrimitiveValue::UnitTypes) a->unit );
	if ( i == 0 )
	    rect->setTop( length );
	else if ( i == 1 )
	    rect->setRight( length );
	else if ( i == 2 )
	    rect->setBottom( length );
	else
	    rect->setLeft( length );
	a = args->next();
	if ( a && args->numValues == 7 ) {
	    if ( a->unit == Value::Operator && a->iValue == ',' ) {
		a = args->next();
	    } else {
		valid = false;
		break;
	    }
	}
	i++;
    }
    if ( valid ) {
	addProperty( propId, new CSSPrimitiveValueImpl( rect ), important );
	valueList->next();
	return true;
    }
    delete rect;
    return false;
}

// [ 'font-style' || 'font-variant' || 'font-weight' ]? 'font-size' [ / 'line-height' ]? 'font-family'
bool CSSParser::parseFont( bool important )
{
//     kdDebug(6080) << "parsing font property current=" << valueList->currentValue << endl;
    bool valid = true;
    Value *value = valueList->current();
    FontValueImpl *font = new FontValueImpl;
    // optional font-style, font-variant and font-weight
    while ( value ) {
// 	kdDebug( 6080 ) << "got value " << value->id << " / " << (value->unit == CSSPrimitiveValue::CSS_STRING ||
	// 				   value->unit == CSSPrimitiveValue::CSS_IDENT ? qString( value->string ) : QString::null )
// 			<< endl;
	int id = value->id;
	if ( id ) {
	    if ( id == CSS_VAL_NORMAL ) {
		// do nothing, it's the inital value for all three
	    }
	    /*
	      else if ( id == CSS_VAL_INHERIT ) {
	      // set all non set ones to inherit
	      // This is not that simple as the inherit could also apply to the following font-size.
	      // very ahrd to tell without looking ahead.
	      inherit = true;
		} */
	    else if ( id == CSS_VAL_ITALIC || id == CSS_VAL_OBLIQUE ) {
		if ( font->style )
		    goto invalid;
		font->style = new CSSPrimitiveValueImpl( id );
	    } else if ( id == CSS_VAL_SMALL_CAPS ) {
		if ( font->variant )
		    goto invalid;
		font->variant = new CSSPrimitiveValueImpl( id );
	    } else if ( id >= CSS_VAL_BOLD && id <= CSS_VAL_LIGHTER ) {
		if ( font->weight )
		    goto invalid;
		font->weight = new CSSPrimitiveValueImpl( id );
	    } else {
		valid = false;
	    }
	} else if ( !font->weight && validUnit( value, FInteger|FNonNeg, true ) ) {
	    int weight = (int)value->fValue;
	    int val = 0;
	    if ( weight == 100 )
		val = CSS_VAL_100;
	    else if ( weight == 200 )
		val = CSS_VAL_200;
	    else if ( weight == 300 )
		val = CSS_VAL_300;
	    else if ( weight == 400 )
		val = CSS_VAL_400;
	    else if ( weight == 500 )
		val = CSS_VAL_500;
	    else if ( weight == 600 )
		val = CSS_VAL_600;
	    else if ( weight == 700 )
		val = CSS_VAL_700;
	    else if ( weight == 800 )
		val = CSS_VAL_800;
	    else if ( weight == 900 )
		val = CSS_VAL_900;

	    if ( val )
		font->weight = new CSSPrimitiveValueImpl( val );
	    else
		valid = false;
	} else {
	    valid = false;
	}
	if ( !valid )
	    break;
	value = valueList->next();
    }
    if ( !value )
	goto invalid;

    // set undefined values to default
    if ( !font->style )
	font->style = new CSSPrimitiveValueImpl( CSS_VAL_NORMAL );
    if ( !font->variant )
	font->variant = new CSSPrimitiveValueImpl( CSS_VAL_NORMAL );
    if ( !font->weight )
	font->weight = new CSSPrimitiveValueImpl( CSS_VAL_NORMAL );

//     kdDebug( 6080 ) << "  got style, variant and weight current=" << valueList->currentValue << endl;

    // now a font size _must_ come
    // <absolute-size> | <relative-size> | <length> | <percentage> | inherit
    if ( value->id >= CSS_VAL_XX_SMALL && value->id <= CSS_VAL_LARGER )
	font->size = new CSSPrimitiveValueImpl( value->id );
    else if ( validUnit( value, FLength|FPercent, strict ) ) {
	font->size = new CSSPrimitiveValueImpl( value->fValue, (CSSPrimitiveValue::UnitTypes) value->unit );
    }
    value = valueList->next();
    if ( !font->size || !value )
	goto invalid;

    // kdDebug( 6080 ) << "  got size" << endl;

    if ( value->unit == Value::Operator && value->iValue == '/' ) {
	// line-height
	value = valueList->next();
	if ( !value )
	    goto invalid;
	if ( value->id == CSS_VAL_NORMAL ) {
	    // default value, nothing to do
	} else if ( validUnit( value, FNumber|FLength|FPercent, strict ) ) {
	    font->lineHeight = new CSSPrimitiveValueImpl( value->fValue, (CSSPrimitiveValue::UnitTypes) value->unit );
	} else {
	    goto invalid;
	}
	value = valueList->next();
	if ( !value )
	    goto invalid;
    }
    
    if (!font->lineHeight)
	font->lineHeight = new CSSPrimitiveValueImpl( CSS_VAL_NORMAL );

//     kdDebug( 6080 ) << "  got line height current=" << valueList->currentValue << endl;
    // font family must come now
    font->family = parseFontFamily();

    if ( valueList->current() || !font->family )
	goto invalid;
//     kdDebug( 6080 ) << "  got family, parsing ok!" << endl;

    addProperty( CSS_PROP_FONT, font, important );
    return true;

 invalid:
//     kdDebug(6080) << "   -> invalid" << endl;
    delete font;
    return false;
}

CSSValueListImpl *CSSParser::parseFontFamily()
{
//     kdDebug( 6080 ) << "CSSParser::parseFontFamily current=" << valueList->currentValue << endl;
    CSSValueListImpl *list = new CSSValueListImpl;
    Value *value = valueList->current();
    FontFamilyValueImpl* currFamily = 0;
    while ( value ) {
// 	kdDebug( 6080 ) << "got value " << value->id << " / "
// 			<< (value->unit == CSSPrimitiveValue::CSS_STRING ||
// 			    value->unit == CSSPrimitiveValue::CSS_IDENT ? qString( value->string ) : QString::null )
// 			<< endl;
        Value* nextValue = valueList->next();
        bool nextValBreaksFont = !nextValue ||
                                 (nextValue->unit == Value::Operator && nextValue->iValue == ',');
        bool nextValIsFontName = nextValue &&
            ((nextValue->id >= CSS_VAL_SERIF && nextValue->id <= CSS_VAL__KHTML_BODY) ||
            (nextValue->unit == CSSPrimitiveValue::CSS_STRING || nextValue->unit == CSSPrimitiveValue::CSS_IDENT));

        if (value->id >= CSS_VAL_SERIF && value->id <= CSS_VAL__KHTML_BODY) {
            if (currFamily) {
                currFamily->parsedFontName += ' ';
                currFamily->parsedFontName += qString(value->string);
            }
            else if (nextValBreaksFont || !nextValIsFontName)
                list->append(new CSSPrimitiveValueImpl(value->id));
            else
                list->append(currFamily = new FontFamilyValueImpl(qString(value->string)));
        }
        else if (value->unit == CSSPrimitiveValue::CSS_STRING) {
            // Strings never share in a family name.
            currFamily = 0;
            list->append(new FontFamilyValueImpl(qString( value->string)));
        }
        else if (value->unit == CSSPrimitiveValue::CSS_IDENT) {
            if (currFamily) {
                currFamily->parsedFontName += ' ';
                currFamily->parsedFontName += qString(value->string);
            }
            else if (nextValBreaksFont || !nextValIsFontName)
                list->append(new FontFamilyValueImpl(qString( value->string)));
            else
                list->append(currFamily = new FontFamilyValueImpl(qString(value->string)));
        }
	else {
// 	    kdDebug( 6080 ) << "invalid family part" << endl;
	    break;
	}
	
        if (!nextValue)
            break;

        if (nextValBreaksFont) {
            value = valueList->next();
            currFamily = 0;
        }
        else if (nextValIsFontName)
            value = nextValue;
        else
            break;
    }
    if ( !list->length() ) {
	delete list;
	list = 0;
    }
    return list;
}


bool CSSParser::parseColor(const QString &name, QRgb& rgb)
{
    int len = name.length();

    if ( !len )
        return false;


    bool ok;

    if ( len == 3 || len == 6 ) {
	int val = name.toInt(&ok, 16);
	if ( ok ) {
            if (len == 6) {
		rgb =  (0xff << 24) | val;
                return true;
            }
            else if ( len == 3 ) {
		// #abc converts to #aabbcc according to the specs
		rgb = (0xff << 24) |
		    (val&0xf00)<<12 | (val&0xf00)<<8 |
		    (val&0xf0)<<8 | (val&0xf0)<<4 |
		    (val&0xf)<<4 | (val&0xf);
                return true;
            }
	}
    }

    // try a little harder
    QColor tc;
    tc.setNamedColor(name.lower());
    if (tc.isValid()) {
        rgb = tc.rgb();
        return true;
    }

    return false;
}


CSSPrimitiveValueImpl *CSSParser::parseColor()
{
    return parseColorFromValue(valueList->current());
}

CSSPrimitiveValueImpl *CSSParser::parseColorFromValue(Value* value)
{
    QRgb c = khtml::transparentColor;
    if ( !strict && value->unit == CSSPrimitiveValue::CSS_NUMBER &&
        value->fValue >= 0. && value->fValue < 1000000. ) {
        QString str;
        str.sprintf( "%06d", (int)(value->fValue+.5) );
        if (!CSSParser::parseColor( str, c ))
            return 0;
    } else if (value->unit == CSSPrimitiveValue::CSS_RGBCOLOR ||
                value->unit == CSSPrimitiveValue::CSS_IDENT ||
                (!strict && value->unit == CSSPrimitiveValue::CSS_DIMENSION)) {
	if (!CSSParser::parseColor( qString( value->string ), c))
            return 0;
    }
    else if ( value->unit == Value::Function &&
		value->function->args != 0 &&
		value->function->args->numValues == 5 /* rgb + two commas */ &&
		qString( value->function->name ).lower() == "rgb(" ) {
	ValueList *args = value->function->args;
	Value *v = args->current();
	if ( !validUnit( v, FInteger|FPercent, true ) )
	    return 0;
	int r = (int) ( v->fValue * (v->unit == CSSPrimitiveValue::CSS_PERCENTAGE ? 256./100. : 1.) );
	v = args->next();
	if ( v->unit != Value::Operator && v->iValue != ',' )
	    return 0;
	v = args->next();
	if ( !validUnit( v, FInteger|FPercent, true ) )
	    return 0;
	int g = (int) ( v->fValue * (v->unit == CSSPrimitiveValue::CSS_PERCENTAGE ? 256./100. : 1.) );
	v = args->next();
	if ( v->unit != Value::Operator && v->iValue != ',' )
	    return 0;
	v = args->next();
	if ( !validUnit( v, FInteger|FPercent, true ) )
	    return 0;
	int b = (int) ( v->fValue * (v->unit == CSSPrimitiveValue::CSS_PERCENTAGE ? 256./100. : 1.) );
	r = QMAX( 0, QMIN( 255, r ) );
	g = QMAX( 0, QMIN( 255, g ) );
	b = QMAX( 0, QMIN( 255, b ) );
	c = qRgb( r, g, b );
    }
    else if ( value->unit == Value::Function &&
              value->function->args != 0 &&
              value->function->args->numValues == 7 /* rgba + three commas */ &&
              qString( value->function->name ).lower() == "rgba(" ) {
        ValueList *args = value->function->args;
        Value *v = args->current();
        if ( !validUnit( v, FInteger|FPercent, true ) )
            return 0;
        int r = (int) ( v->fValue * (v->unit == CSSPrimitiveValue::CSS_PERCENTAGE ? 256./100. : 1.) );
        v = args->next();
        if ( v->unit != Value::Operator && v->iValue != ',' )
            return 0;
        v = args->next();
        if ( !validUnit( v, FInteger|FPercent, true ) )
            return 0;
        int g = (int) ( v->fValue * (v->unit == CSSPrimitiveValue::CSS_PERCENTAGE ? 256./100. : 1.) );
        v = args->next();
        if ( v->unit != Value::Operator && v->iValue != ',' )
            return 0;
        v = args->next();
        if ( !validUnit( v, FInteger|FPercent, true ) )
            return 0;
        int b = (int) ( v->fValue * (v->unit == CSSPrimitiveValue::CSS_PERCENTAGE ? 256./100. : 1.) );
        v = args->next();
        if ( v->unit != Value::Operator && v->iValue != ',' )
            return 0;
        v = args->next();
        if ( !validUnit( v, FNumber, true ) )
            return 0;
        r = QMAX( 0, QMIN( 255, r ) );
        g = QMAX( 0, QMIN( 255, g ) );
        b = QMAX( 0, QMIN( 255, b ) );
        int a = (int)(QMAX( 0, QMIN( 1.0f, v->fValue ) ) * 255);
        c = qRgba( r, g, b, a );
    }
    else
        return 0;

    return new CSSPrimitiveValueImpl(c);
}

// This class tracks parsing state for shadow values.  If it goes out of scope (e.g., due to an early return)
// without the allowBreak bit being set, then it will clean up all of the objects and destroy them.
struct ShadowParseContext {
    ShadowParseContext()
    :values(0), x(0), y(0), blur(0), color(0),
     allowX(true), allowY(false), allowBlur(false), allowColor(true),
     allowBreak(true)
    {}

    ~ShadowParseContext() {
        if (!allowBreak) {
            delete values;
            delete x;
            delete y;
            delete blur;
            delete color;
        }
    }

    bool allowLength() { return allowX || allowY || allowBlur; }

    bool failed() { return allowBreak = false; }
    
    void commitValue() {
        // Handle the ,, case gracefully by doing nothing.
        if (x || y || blur || color) {
            if (!values)
                values = new CSSValueListImpl();
            
            // Construct the current shadow value and add it to the list.
            values->append(new ShadowValueImpl(x, y, blur, color));
        }
        
        // Now reset for the next shadow value.
        x = y = blur = color = 0;
        allowX = allowColor = allowBreak = true;
        allowY = allowBlur = false;  
    }

    void commitLength(Value* v) {
        CSSPrimitiveValueImpl* val = new CSSPrimitiveValueImpl(v->fValue,
                                                               (CSSPrimitiveValue::UnitTypes)v->unit);
        if (allowX) {
            x = val;
            allowX = false; allowY = true; allowColor = false; allowBreak = false;
        }
        else if (allowY) {
            y = val;
            allowY = false; allowBlur = true; allowColor = true; allowBreak = true;
        }
        else if (allowBlur) {
            blur = val;
            allowBlur = false;
        }
    }

    void commitColor(CSSPrimitiveValueImpl* val) {
        color = val;
        allowColor = false;
        if (allowX)
            allowBreak = false;
        else
            allowBlur = false;
    }
    
    CSSValueListImpl* values;
    CSSPrimitiveValueImpl* x;
    CSSPrimitiveValueImpl* y;
    CSSPrimitiveValueImpl* blur;
    CSSPrimitiveValueImpl* color;

    bool allowX;
    bool allowY;
    bool allowBlur;
    bool allowColor;
    bool allowBreak;
};

bool CSSParser::parseShadow(int propId, bool important)
{
    ShadowParseContext context;
    Value* val;
    while ((val = valueList->current())) {
        // Check for a comma break first.
        if (val->unit == Value::Operator) {
            if (val->iValue != ',' || !context.allowBreak)
                // Other operators aren't legal or we aren't done with the current shadow
                // value.  Treat as invalid.
                return context.failed();
            
            // The value is good.  Commit it.
            context.commitValue();
        }
        // Check to see if we're a length.
        else if (validUnit(val, FLength, true)) {
            // We required a length and didn't get one. Invalid.
            if (!context.allowLength())
                return context.failed();

            // A length is allowed here.  Construct the value and add it.
            context.commitLength(val);
        }
        else {
            // The only other type of value that's ok is a color value.
            CSSPrimitiveValueImpl* parsedColor = 0;
            bool isColor = (val->id >= CSS_VAL_AQUA && val->id <= CSS_VAL_WINDOWTEXT || val->id == CSS_VAL_MENU ||
                            (val->id >= CSS_VAL_GREY && val->id <= CSS_VAL__KHTML_TEXT && !strict));
            if (isColor) {
                if (!context.allowColor)
                    return context.failed();
                parsedColor = new CSSPrimitiveValueImpl(val->id);
            }

            if (!parsedColor)
                // It's not built-in. Try to parse it as a color.
                parsedColor = parseColorFromValue(val);

            if (!parsedColor || !context.allowColor)
                return context.failed(); // This value is not a color or length and is invalid or
                                         // it is a color, but a color isn't allowed at this point.
            
            context.commitColor(parsedColor);
        }
        
        valueList->next();
    }

    if (context.allowBreak) {
        context.commitValue();
        if (context.values->length()) {
            addProperty(propId, context.values, important);
            valueList->next();
            return true;
        }
    }
    
    return context.failed();
}

static inline int yyerror( const char *str ) {
#ifdef CSS_DEBUG
    kdDebug( 6080 ) << "CSS parse error " << str << endl;
#else
    Q_UNUSED( str );
#endif
    return 1;
}

#define END 0

#include "parser.h"

int DOM::CSSParser::lex( void *_yylval ) {
    YYSTYPE *yylval = (YYSTYPE *)_yylval;
    int token = lex();
    int length;
    unsigned short *t = text( &length );

#ifdef TOKEN_DEBUG
    qDebug("CSSTokenizer: got token %d: '%s'", token, token == END ? "" : QString( (QChar *)t, length ).latin1() );
#endif
    switch( token ) {
    case WHITESPACE:
    case SGML_CD:
    case INCLUDES:
    case DASHMATCH:
	break;

    case URI:
    case STRING:
    case IDENT:
    case HASH:
    case DIMEN:
    case UNICODERANGE:
    case FUNCTION:
	yylval->string.string = t;
	yylval->string.length = length;
	break;

    case IMPORT_SYM:
    case PAGE_SYM:
    case MEDIA_SYM:
    case FONT_FACE_SYM:
    case CHARSET_SYM:
    case NAMESPACE_SYM:

    case IMPORTANT_SYM:
	break;

    case QEMS:
	length--;
    case GRADS:
	length--;
    case DEGS:
    case RADS:
    case KHERZ:
	length--;
    case MSECS:
    case HERZ:
    case EMS:
    case EXS:
    case PXS:
    case CMS:
    case MMS:
    case INS:
    case PTS:
    case PCS:
	length--;
    case SECS:
    case PERCENTAGE:
	length--;
    case NUMBER:
	yylval->val = QString( (QChar *)t, length ).toDouble();
	//qDebug("value = %s, converted=%.2f", QString( (QChar *)t, length ).latin1(), yylval->val );
	break;

    default:
	break;
    }

    return token;
}

static inline int toHex( char c ) {
    if ( '0' <= c && c <= '9' )
	return c - '0';
    if ( 'a' <= c && c <= 'f' )
	return c - 'a' + 10;
    if ( 'A' <= c && c<= 'F' )
	return c - 'A' + 10;
    return 0;
}

unsigned short *DOM::CSSParser::text(int *length)
{
    unsigned short *start = yytext;
    int l = yyleng;
    switch( yyTok ) {
    case STRING:
	l--;
	/* nobreak */
    case HASH:
	start++;
	l--;
	break;
    case URI:
	// "url("{w}{string}{w}")"
	// "url("{w}{url}{w}")"

	// strip "url(" and ")"
	start += 4;
	l -= 5;
	// strip {w}
	while ( l &&
		(*start == ' ' || *start == '\t' || *start == '\r' ||
		 *start == '\n' || *start == '\f' ) ) {
	    start++; l--;
	}
	if ( *start == '"' || *start == '\'' ) {
	    start++; l--;
	}
	while ( l &&
		(start[l-1] == ' ' || start[l-1] == '\t' || start[l-1] == '\r' ||
		 start[l-1] == '\n' || start[l-1] == '\f' ) ) {
	    l--;
	}
	if ( l && (start[l-1] == '\"' || start[l-1] == '\'' ) )
	     l--;

    default:
	break;
    }

    // process escapes
    unsigned short *out = start;
    unsigned short *escape = 0;

    for ( int i = 0; i < l; i++ ) {
        unsigned short *current = start+i;
        if ( escape == current - 1 ) {
            if ( ( *current >= '0' && *current <= '9' ) ||
                 ( *current >= 'a' && *current <= 'f' ) ||
                 ( *current >= 'A' && *current <= 'F' ) )
                continue;
            if ( yyTok == STRING &&
                 ( *current == '\n' || *current == '\r' || *current == '\f' ) ) {
                // ### handle \r\n case
                if ( *current != '\r' )
                    escape = 0;
                continue;
            }
            // in all other cases copy the char to output
            // ###
            *out++ = *current;
            escape = 0;
            continue;
        }
        if ( escape == current - 2 && yyTok == STRING &&
             *(current-1) == '\r' && *current == '\n' ) {
            escape = 0;
            continue;
        }
        if ( escape > current - 7 &&
             ( ( *current >= '0' && *current <= '9' ) ||
               ( *current >= 'a' && *current <= 'f' ) ||
               ( *current >= 'A' && *current <= 'F' ) ) )
            continue;
        if ( escape ) {
            // add escaped char
            int uc = 0;
            escape++;
            while ( escape < current ) {
                // 		qDebug("toHex( %c = %x", (char)*escape, toHex( *escape ) );
                uc *= 16;
                uc += toHex( *escape );
                escape++;
            }
            // 	    qDebug(" converting escape: string='%s', value=0x%x", QString( (QChar *)e, current-e ).latin1(), uc );
            // can't handle chars outside ucs2
            if ( uc > 0xffff )
                uc = 0xfffd;
            *(out++) = (unsigned short)uc;
            escape = 0;
            if ( *current == ' ' ||
                 *current == '\t' ||
                 *current == '\r' ||
                 *current == '\n' ||
                 *current == '\f' )
                continue;
        }
        if ( !escape && *current == '\\' ) {
            escape = current;
            continue;
        }
        *(out++) = *current;
    }
    if ( escape ) {
        // add escaped char
        int uc = 0;
        escape++;
        while ( escape < start+l ) {
            // 		qDebug("toHex( %c = %x", (char)*escape, toHex( *escape ) );
            uc *= 16;
            uc += toHex( *escape );
            escape++;
        }
        // 	    qDebug(" converting escape: string='%s', value=0x%x", QString( (QChar *)e, current-e ).latin1(), uc );
        // can't handle chars outside ucs2
        if ( uc > 0xffff )
            uc = 0xfffd;
        *(out++) = (unsigned short)uc;
    }
    
    *length = out - start;
    return start;
}


#define YY_DECL int DOM::CSSParser::lex()
#define yyconst const
typedef int yy_state_type;
typedef unsigned int YY_CHAR;
// this line makes sure we treat all Unicode chars correctly.
#define YY_SC_TO_UI(c) (c > 0xff ? 0xff : c)
#define YY_DO_BEFORE_ACTION \
	yytext = yy_bp; \
	yyleng = (int) (yy_cp - yy_bp); \
	yy_hold_char = *yy_cp; \
	*yy_cp = 0; \
	yy_c_buf_p = yy_cp;
#define YY_BREAK break;
#define ECHO qDebug( "%s", QString( (QChar *)yytext, yyleng ).latin1() )
#define YY_RULE_SETUP
#define INITIAL 0
#define YY_STATE_EOF(state) (YY_END_OF_BUFFER + state + 1)
#define yyterminate() yyTok = END; return yyTok
#define YY_FATAL_ERROR(a) qFatal(a)
#define BEGIN yy_start = 1 + 2 *
#define COMMENT 1

#include "tokenizer.cpp"
