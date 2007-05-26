# Copyright (C) 2006, 2007 Apple Inc. All rights reserved.
# Copyright (C) 2006 Samuel Weinig <sam.weinig@gmail.com>
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1.  Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer. 
# 2.  Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution. 
# 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
#     its contributors may be used to endorse or promote products derived
#     from this software without specific prior written permission. 
#
# THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
# ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
# THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

VPATH = $(WEBCORE_PRIVATE_HEADERS_DIR) $(JAVASCRIPTCORE_PRIVATE_HEADERS_DIR)

INTERNAL_HEADERS_DIR = $(BUILT_PRODUCTS_DIR)/DerivedSources/WebKit
PUBLIC_HEADERS_DIR = $(TARGET_BUILD_DIR)/$(PUBLIC_HEADERS_FOLDER_PATH)
PRIVATE_HEADERS_DIR = $(TARGET_BUILD_DIR)/$(PRIVATE_HEADERS_FOLDER_PATH)

.PHONY : all
all : \
    $(PUBLIC_HEADERS_DIR)/DOM.h \
    $(PUBLIC_HEADERS_DIR)/DOMAbstractView.h \
    $(PUBLIC_HEADERS_DIR)/DOMAttr.h \
    $(PUBLIC_HEADERS_DIR)/DOMCDATASection.h \
    $(PUBLIC_HEADERS_DIR)/DOMCSS.h \
    $(PUBLIC_HEADERS_DIR)/DOMCSSCharsetRule.h \
    $(PUBLIC_HEADERS_DIR)/DOMCSSFontFaceRule.h \
    $(PUBLIC_HEADERS_DIR)/DOMCSSImportRule.h \
    $(PUBLIC_HEADERS_DIR)/DOMCSSMediaRule.h \
    $(PUBLIC_HEADERS_DIR)/DOMCSSPageRule.h \
    $(PUBLIC_HEADERS_DIR)/DOMCSSPrimitiveValue.h \
    $(PUBLIC_HEADERS_DIR)/DOMCSSRule.h \
    $(PUBLIC_HEADERS_DIR)/DOMCSSRuleList.h \
    $(PUBLIC_HEADERS_DIR)/DOMCSSStyleDeclaration.h \
    $(INTERNAL_HEADERS_DIR)/DOMCSSStyleDeclarationInternal.h \
    $(PUBLIC_HEADERS_DIR)/DOMCSSStyleRule.h \
    $(PUBLIC_HEADERS_DIR)/DOMCSSStyleSheet.h \
    $(PUBLIC_HEADERS_DIR)/DOMCSSUnknownRule.h \
    $(PUBLIC_HEADERS_DIR)/DOMCSSValue.h \
    $(PUBLIC_HEADERS_DIR)/DOMCSSValueList.h \
    $(PUBLIC_HEADERS_DIR)/DOMCharacterData.h \
    $(PUBLIC_HEADERS_DIR)/DOMComment.h \
    $(PUBLIC_HEADERS_DIR)/DOMCore.h \
    $(PUBLIC_HEADERS_DIR)/DOMCounter.h \
    $(PUBLIC_HEADERS_DIR)/DOMImplementation.h \
    $(PUBLIC_HEADERS_DIR)/DOMDocument.h \
    $(INTERNAL_HEADERS_DIR)/DOMDocumentInternal.h \
    $(PUBLIC_HEADERS_DIR)/DOMDocumentFragment.h \
    $(PRIVATE_HEADERS_DIR)/DOMDocumentPrivate.h \
    $(PUBLIC_HEADERS_DIR)/DOMDocumentType.h \
    $(PUBLIC_HEADERS_DIR)/DOMElement.h \
    $(INTERNAL_HEADERS_DIR)/DOMElementInternal.h \
    $(PRIVATE_HEADERS_DIR)/DOMElementPrivate.h \
    $(PUBLIC_HEADERS_DIR)/DOMEntity.h \
    $(PUBLIC_HEADERS_DIR)/DOMEntityReference.h \
    $(PUBLIC_HEADERS_DIR)/DOMEvent.h \
    $(PUBLIC_HEADERS_DIR)/DOMEventException.h \
    $(PUBLIC_HEADERS_DIR)/DOMEventListener.h \
    $(PUBLIC_HEADERS_DIR)/DOMEventTarget.h \
    $(PUBLIC_HEADERS_DIR)/DOMEvents.h \
    $(PUBLIC_HEADERS_DIR)/DOMException.h \
    $(PUBLIC_HEADERS_DIR)/DOMExtensions.h \
    $(PUBLIC_HEADERS_DIR)/DOMHTML.h \
    $(PUBLIC_HEADERS_DIR)/DOMHTMLAnchorElement.h \
    $(PRIVATE_HEADERS_DIR)/DOMHTMLAnchorElementPrivate.h \
    $(PUBLIC_HEADERS_DIR)/DOMHTMLAppletElement.h \
    $(PUBLIC_HEADERS_DIR)/DOMHTMLAreaElement.h \
    $(PRIVATE_HEADERS_DIR)/DOMHTMLAreaElementPrivate.h \
    $(PUBLIC_HEADERS_DIR)/DOMHTMLBRElement.h \
    $(PUBLIC_HEADERS_DIR)/DOMHTMLBaseElement.h \
    $(PUBLIC_HEADERS_DIR)/DOMHTMLBaseFontElement.h \
    $(PUBLIC_HEADERS_DIR)/DOMHTMLBodyElement.h \
    $(PRIVATE_HEADERS_DIR)/DOMHTMLBodyElementPrivate.h \
    $(PUBLIC_HEADERS_DIR)/DOMHTMLButtonElement.h \
    $(PRIVATE_HEADERS_DIR)/DOMHTMLButtonElementPrivate.h \
    $(PUBLIC_HEADERS_DIR)/DOMHTMLCollection.h \
    $(PUBLIC_HEADERS_DIR)/DOMHTMLDListElement.h \
    $(PUBLIC_HEADERS_DIR)/DOMHTMLDirectoryElement.h \
    $(PUBLIC_HEADERS_DIR)/DOMHTMLDivElement.h \
    $(PUBLIC_HEADERS_DIR)/DOMHTMLDocument.h \
    $(PRIVATE_HEADERS_DIR)/DOMHTMLDocumentPrivate.h \
    $(PUBLIC_HEADERS_DIR)/DOMHTMLElement.h \
    $(INTERNAL_HEADERS_DIR)/DOMHTMLElementInternal.h \
    $(PUBLIC_HEADERS_DIR)/DOMHTMLEmbedElement.h \
    $(PUBLIC_HEADERS_DIR)/DOMHTMLFieldSetElement.h \
    $(PUBLIC_HEADERS_DIR)/DOMHTMLFontElement.h \
    $(PUBLIC_HEADERS_DIR)/DOMHTMLFormElement.h \
    $(PRIVATE_HEADERS_DIR)/DOMHTMLFormElementPrivate.h \
    $(PUBLIC_HEADERS_DIR)/DOMHTMLFrameElement.h \
    $(PRIVATE_HEADERS_DIR)/DOMHTMLFrameElementPrivate.h \
    $(PUBLIC_HEADERS_DIR)/DOMHTMLFrameSetElement.h \
    $(PUBLIC_HEADERS_DIR)/DOMHTMLHRElement.h \
    $(PUBLIC_HEADERS_DIR)/DOMHTMLHeadElement.h \
    $(PUBLIC_HEADERS_DIR)/DOMHTMLHeadingElement.h \
    $(PUBLIC_HEADERS_DIR)/DOMHTMLHtmlElement.h \
    $(PUBLIC_HEADERS_DIR)/DOMHTMLIFrameElement.h \
    $(PUBLIC_HEADERS_DIR)/DOMHTMLImageElement.h \
    $(PRIVATE_HEADERS_DIR)/DOMHTMLImageElementPrivate.h \
    $(PUBLIC_HEADERS_DIR)/DOMHTMLInputElement.h \
    $(PRIVATE_HEADERS_DIR)/DOMHTMLInputElementPrivate.h \
    $(INTERNAL_HEADERS_DIR)/DOMHTMLInputElementInternal.h \
    $(PUBLIC_HEADERS_DIR)/DOMHTMLIsIndexElement.h \
    $(PUBLIC_HEADERS_DIR)/DOMHTMLLIElement.h \
    $(PUBLIC_HEADERS_DIR)/DOMHTMLLabelElement.h \
    $(PRIVATE_HEADERS_DIR)/DOMHTMLLabelElementPrivate.h \
    $(PUBLIC_HEADERS_DIR)/DOMHTMLLegendElement.h \
    $(PRIVATE_HEADERS_DIR)/DOMHTMLLegendElementPrivate.h \
    $(PUBLIC_HEADERS_DIR)/DOMHTMLLinkElement.h \
    $(PRIVATE_HEADERS_DIR)/DOMHTMLLinkElementPrivate.h \
    $(PUBLIC_HEADERS_DIR)/DOMHTMLMapElement.h \
    $(PUBLIC_HEADERS_DIR)/DOMHTMLMarqueeElement.h \
    $(PUBLIC_HEADERS_DIR)/DOMHTMLMenuElement.h \
    $(PUBLIC_HEADERS_DIR)/DOMHTMLMetaElement.h \
    $(PUBLIC_HEADERS_DIR)/DOMHTMLModElement.h \
    $(PUBLIC_HEADERS_DIR)/DOMHTMLOListElement.h \
    $(PUBLIC_HEADERS_DIR)/DOMHTMLObjectElement.h \
    $(PUBLIC_HEADERS_DIR)/DOMHTMLOptGroupElement.h \
    $(PUBLIC_HEADERS_DIR)/DOMHTMLOptionElement.h \
    $(PUBLIC_HEADERS_DIR)/DOMHTMLOptionsCollection.h \
    $(PRIVATE_HEADERS_DIR)/DOMHTMLOptionsCollectionPrivate.h \
    $(PUBLIC_HEADERS_DIR)/DOMHTMLParagraphElement.h \
    $(PUBLIC_HEADERS_DIR)/DOMHTMLParamElement.h \
    $(PUBLIC_HEADERS_DIR)/DOMHTMLPreElement.h \
    $(PRIVATE_HEADERS_DIR)/DOMHTMLPreElementPrivate.h \
    $(PUBLIC_HEADERS_DIR)/DOMHTMLQuoteElement.h \
    $(PUBLIC_HEADERS_DIR)/DOMHTMLScriptElement.h \
    $(PUBLIC_HEADERS_DIR)/DOMHTMLSelectElement.h \
    $(PUBLIC_HEADERS_DIR)/DOMHTMLStyleElement.h \
    $(PRIVATE_HEADERS_DIR)/DOMHTMLStyleElementPrivate.h \
    $(PUBLIC_HEADERS_DIR)/DOMHTMLTableCaptionElement.h \
    $(PUBLIC_HEADERS_DIR)/DOMHTMLTableCellElement.h \
    $(PUBLIC_HEADERS_DIR)/DOMHTMLTableColElement.h \
    $(PUBLIC_HEADERS_DIR)/DOMHTMLTableElement.h \
    $(PUBLIC_HEADERS_DIR)/DOMHTMLTableRowElement.h \
    $(PUBLIC_HEADERS_DIR)/DOMHTMLTableSectionElement.h \
    $(PUBLIC_HEADERS_DIR)/DOMHTMLTextAreaElement.h \
    $(PRIVATE_HEADERS_DIR)/DOMHTMLTextAreaElementPrivate.h \
    $(INTERNAL_HEADERS_DIR)/DOMHTMLTextAreaElementInternal.h \
    $(PUBLIC_HEADERS_DIR)/DOMHTMLTitleElement.h \
    $(PUBLIC_HEADERS_DIR)/DOMHTMLUListElement.h \
    $(PUBLIC_HEADERS_DIR)/DOMKeyboardEvent.h \
    $(PRIVATE_HEADERS_DIR)/DOMKeyboardEventPrivate.h \
    $(PUBLIC_HEADERS_DIR)/DOMMediaList.h \
    $(PUBLIC_HEADERS_DIR)/DOMMouseEvent.h \
    $(PRIVATE_HEADERS_DIR)/DOMMouseEventPrivate.h \
    $(PUBLIC_HEADERS_DIR)/DOMMutationEvent.h \
    $(PUBLIC_HEADERS_DIR)/DOMNamedNodeMap.h \
    $(PUBLIC_HEADERS_DIR)/DOMNode.h \
    $(INTERNAL_HEADERS_DIR)/DOMNodeInternal.h \
    $(PRIVATE_HEADERS_DIR)/DOMNodePrivate.h \
    $(PUBLIC_HEADERS_DIR)/DOMNodeFilter.h \
    $(PUBLIC_HEADERS_DIR)/DOMNodeIterator.h \
    $(PRIVATE_HEADERS_DIR)/DOMNodeIteratorPrivate.h \
    $(PUBLIC_HEADERS_DIR)/DOMNodeList.h \
    $(PUBLIC_HEADERS_DIR)/DOMNotation.h \
    $(PUBLIC_HEADERS_DIR)/DOMObject.h \
    $(PUBLIC_HEADERS_DIR)/DOMOverflowEvent.h \
    $(PRIVATE_HEADERS_DIR)/DOMPrivate.h \
    $(PUBLIC_HEADERS_DIR)/DOMProcessingInstruction.h \
    $(PRIVATE_HEADERS_DIR)/DOMProcessingInstructionPrivate.h \
    $(PUBLIC_HEADERS_DIR)/DOMRGBColor.h \
    $(PUBLIC_HEADERS_DIR)/DOMRange.h \
    $(INTERNAL_HEADERS_DIR)/DOMRangeInternal.h \
    $(PRIVATE_HEADERS_DIR)/DOMRangePrivate.h \
    $(PUBLIC_HEADERS_DIR)/DOMRangeException.h \
    $(PUBLIC_HEADERS_DIR)/DOMRanges.h \
    $(PUBLIC_HEADERS_DIR)/DOMRect.h \
    $(PUBLIC_HEADERS_DIR)/DOMStyleSheet.h \
    $(PUBLIC_HEADERS_DIR)/DOMStyleSheetList.h \
    $(PUBLIC_HEADERS_DIR)/DOMStylesheets.h \
    $(PUBLIC_HEADERS_DIR)/DOMText.h \
    $(PUBLIC_HEADERS_DIR)/DOMTraversal.h \
    $(PUBLIC_HEADERS_DIR)/DOMTreeWalker.h \
    $(PUBLIC_HEADERS_DIR)/DOMUIEvent.h \
    $(PRIVATE_HEADERS_DIR)/DOMUIEventPrivate.h \
    $(PUBLIC_HEADERS_DIR)/DOMViews.h \
    $(PUBLIC_HEADERS_DIR)/DOMWheelEvent.h \
    $(PRIVATE_HEADERS_DIR)/DOMWheelEventPrivate.h \
    $(PUBLIC_HEADERS_DIR)/DOMXPath.h \
    $(PUBLIC_HEADERS_DIR)/DOMXPathException.h \
    $(PUBLIC_HEADERS_DIR)/DOMXPathExpression.h \
    $(PUBLIC_HEADERS_DIR)/DOMXPathNSResolver.h \
    $(PUBLIC_HEADERS_DIR)/DOMXPathResult.h \
    $(PRIVATE_HEADERS_DIR)/WebDashboardRegion.h \
    $(PUBLIC_HEADERS_DIR)/WebScriptObject.h \
    $(PUBLIC_HEADERS_DIR)/npapi.h \
    $(PUBLIC_HEADERS_DIR)/npruntime.h \
#

REPLACE_RULES = -e s/\<WebCore/\<WebKit/ -e s/\<JavaScriptCore/\<WebKit/ -e s/DOMDOMImplementation/DOMImplementation/ -e 's/\<WebKit\/JSBase.h/\<JavaScriptCore\/JSBase.h/'
HEADER_MIGRATE_CMD = sed $(REPLACE_RULES) $< $(PROCESS_HEADER_FOR_MACOSX_TARGET_CMD) > $@

ifeq ($(MACOSX_DEPLOYMENT_TARGET),10.4)
PROCESS_HEADER_FOR_MACOSX_TARGET_CMD = | ( unifdef -DBUILDING_ON_TIGER || exit 0 )
else
PROCESS_HEADER_FOR_MACOSX_TARGET_CMD = | ( unifdef -UBUILDING_ON_TIGER || exit 0 )
endif

$(PUBLIC_HEADERS_DIR)/DOM% : DOMDOM% MigrateHeaders.make
	$(HEADER_MIGRATE_CMD)

$(PRIVATE_HEADERS_DIR)/DOM% : DOMDOM% MigrateHeaders.make
	$(HEADER_MIGRATE_CMD)

$(PUBLIC_HEADERS_DIR)/% : % MigrateHeaders.make
	$(HEADER_MIGRATE_CMD)

$(PRIVATE_HEADERS_DIR)/% : % MigrateHeaders.make
	$(HEADER_MIGRATE_CMD)

$(INTERNAL_HEADERS_DIR)/% : % MigrateHeaders.make
	$(HEADER_MIGRATE_CMD)
