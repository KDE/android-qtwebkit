/*
 * Copyright (C) 2004 Apple Computer, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef __htmlediting_h__
#define __htmlediting_h__

#include "dom_position.h"
#include "dom_string.h"
#include "khtml_selection.h"
#include "shared.h"

class KHTMLSelection;

namespace DOM {
    class DocumentImpl;
    class DOMPosition;
    class DOMString;
    class NodeImpl;
    class TextImpl;
};

namespace khtml {

class AppendNodeCommandImpl;
class CompositeEditCommandImpl;
class DeleteKeyCommandImpl;
class DeleteSelectionCommandImpl;
class DeleteTextCommandImpl;
class EditCommandImpl;
class InputNewlineCommandImpl;
class InputTextCommandImpl;
class InsertNodeBeforeCommandImpl;
class InsertTextCommandImpl;
class JoinTextNodesCommandImpl;
class ModifyTextNodeCommand;
class ModifyTextNodeCommandImpl;
class PasteHTMLCommandImpl;
class PasteImageCommandImpl;
class RemoveNodeCommandImpl;
class SplitTextNodeCommandImpl;
class TypingCommandImpl;

//------------------------------------------------------------------------------------------
// Constants

enum ECommandID { 
    EditCommandID, // leave the base class first, others in alpha order
    AppendNodeCommandID,
    CompositeEditCommandID,
    DeleteKeyCommandID,
    DeleteSelectionCommandID,
    DeleteTextCommandID,
    InputNewlineCommandID,
    InputTextCommandID,
    InsertNodeBeforeCommandID,
    InsertTextCommandID,
    JoinTextNodesCommandID,
    ModifyTextNodeCommandID,
    PasteHTMLCommandID,
    PasteImageCommandID,
    RemoveNodeCommandID,
    SplitTextNodeCommandID,
    TypingCommandID,
};

//------------------------------------------------------------------------------------------
// SharedCommandImpl

class SharedCommandImpl : public Shared<SharedCommandImpl>
{
public:
	SharedCommandImpl() {}
	virtual ~SharedCommandImpl() {}

    virtual int commandID() const = 0;
    virtual bool isCompositeStep() const = 0;
    virtual void setIsCompositeStep(bool flag=true) = 0;

	virtual void apply() = 0;	
	virtual void unapply() = 0;
	virtual void reapply() = 0;

    virtual DOM::DocumentImpl * const document() const = 0;

    virtual KHTMLSelection startingSelection() const = 0;
    virtual KHTMLSelection endingSelection() const = 0;
    virtual KHTMLSelection currentSelection() const = 0;

    virtual void setStartingSelection(const KHTMLSelection &s) = 0;
    virtual void setEndingSelection(const KHTMLSelection &s) = 0;

    virtual void moveToStartingSelection() = 0;
    virtual void moveToEndingSelection() = 0;
};

//------------------------------------------------------------------------------------------
// EditCommand

class EditCommand : public SharedPtr<SharedCommandImpl>
{
public:
	EditCommand();
	EditCommand(EditCommandImpl *);
	EditCommand(const EditCommand &);
	virtual ~EditCommand();

    int commandID() const;
    bool isCompositeStep() const;
    void setIsCompositeStep(bool flag=true);

	void apply();	
	void unapply();
	void reapply();

    DOM::DocumentImpl * const document() const;

    KHTMLSelection startingSelection() const;
    KHTMLSelection endingSelection() const;
    KHTMLSelection currentSelection() const;

    void setStartingSelection(const KHTMLSelection &s);
    void setEndingSelection(const KHTMLSelection &s);

    void moveToStartingSelection();
    void moveToEndingSelection();

    EditCommandImpl *handle() const;
};

//------------------------------------------------------------------------------------------
// CompositeEditCommand

class CompositeEditCommand : public EditCommand
{
public:
	CompositeEditCommand();
	CompositeEditCommand(CompositeEditCommandImpl *);
	CompositeEditCommand(const CompositeEditCommand &);
	virtual ~CompositeEditCommand();

private:
    inline CompositeEditCommandImpl *impl() const;
};

//------------------------------------------------------------------------------------------
// ModifyTextNodeCommand

class ModifyTextNodeCommand : public EditCommand
{
public:
    ModifyTextNodeCommand(ModifyTextNodeCommandImpl *);
	virtual ~ModifyTextNodeCommand();
};

//==========================================================================================
// Concrete commands
//------------------------------------------------------------------------------------------
// AppendNodeCommand

class AppendNodeCommand : public EditCommand
{
public:
    AppendNodeCommand(DOM::DocumentImpl *, DOM::NodeImpl *parent, DOM::NodeImpl *appendChild);
	virtual ~AppendNodeCommand();

    DOM::NodeImpl *parent() const;
    DOM::NodeImpl *appendChild() const;
    
private:
    inline AppendNodeCommandImpl *impl() const;
};

//------------------------------------------------------------------------------------------
// DeleteKeyCommand

class DeleteKeyCommand : public CompositeEditCommand
{
public:
    DeleteKeyCommand(DOM::DocumentImpl *document);
    virtual ~DeleteKeyCommand();

private:
    inline DeleteKeyCommandImpl *impl() const;
};

//------------------------------------------------------------------------------------------
// DeleteSelectionCommand

class DeleteSelectionCommand : public CompositeEditCommand
{
public:
	DeleteSelectionCommand(DOM::DocumentImpl *document);
	virtual ~DeleteSelectionCommand();

private:
    inline DeleteSelectionCommandImpl *impl() const;
};

//------------------------------------------------------------------------------------------
// DeleteTextCommand

class DeleteTextCommand : public EditCommand
{
public:
	DeleteTextCommand(DOM::DocumentImpl *document, DOM::TextImpl *, long offset, long count);
	virtual ~DeleteTextCommand();

    DOM::TextImpl *node() const;
    long offset() const;
    long count() const;

private:
    inline DeleteTextCommandImpl *impl() const;
};

//------------------------------------------------------------------------------------------
// InputNewlineCommand

class InputNewlineCommand : public CompositeEditCommand
{
public:
    InputNewlineCommand(DOM::DocumentImpl *document);
    virtual ~InputNewlineCommand();

private:
    inline InputNewlineCommandImpl *impl() const;
};

//------------------------------------------------------------------------------------------
// InputTextCommand

class InputTextCommand : public CompositeEditCommand
{
public:
    InputTextCommand(DOM::DocumentImpl *document, const DOM::DOMString &text);
    virtual ~InputTextCommand();

    DOM::DOMString text() const;

    void deleteCharacter();
    void coalesce(const DOM::DOMString &text);

private:
    inline InputTextCommandImpl *impl() const;
};

//------------------------------------------------------------------------------------------
// InsertNodeBeforeCommand

class InsertNodeBeforeCommand : public EditCommand
{
public:
    InsertNodeBeforeCommand();
    InsertNodeBeforeCommand(DOM::DocumentImpl *, DOM::NodeImpl *insertChild, DOM::NodeImpl *refChild);
    InsertNodeBeforeCommand(const InsertNodeBeforeCommand &);
    virtual ~InsertNodeBeforeCommand();

    DOM::NodeImpl *insertChild() const;
    DOM::NodeImpl *refChild() const;
    
private:
    inline InsertNodeBeforeCommandImpl *impl() const;
};

//------------------------------------------------------------------------------------------
// InsertTextCommand

class InsertTextCommand : public EditCommand
{
public:
	InsertTextCommand(DOM::DocumentImpl *document, DOM::TextImpl *, long, const DOM::DOMString &);
	virtual ~InsertTextCommand();

    DOM::TextImpl *node() const;
    long offset() const;
    DOM::DOMString text() const;

private:
    inline InsertTextCommandImpl *impl() const;
};

//------------------------------------------------------------------------------------------
// JoinTextNodesCommand

class JoinTextNodesCommand : public ModifyTextNodeCommand
{
public:
	JoinTextNodesCommand(DOM::DocumentImpl *, DOM::TextImpl *, DOM::TextImpl *);
	virtual ~JoinTextNodesCommand();

    DOM::TextImpl *firstNode() const;
    DOM::TextImpl *secondNode() const;
    
private:
    inline JoinTextNodesCommandImpl *impl() const;
};

//------------------------------------------------------------------------------------------
// PasteHTMLCommand

class PasteHTMLCommand : public CompositeEditCommand
{
public:
    PasteHTMLCommand(DOM::DocumentImpl *document, const DOM::DOMString &HTMLString);
    virtual ~PasteHTMLCommand();

    DOM::DOMString HTMLString() const;

private:
    inline PasteHTMLCommandImpl *impl() const;
};

//------------------------------------------------------------------------------------------
// PasteImageCommand

class PasteImageCommand : public CompositeEditCommand
{
public:
    PasteImageCommand(DOM::DocumentImpl *document, const DOM::DOMString &src);
    virtual ~PasteImageCommand();
        
private:
    inline PasteImageCommandImpl *impl() const;
};

//------------------------------------------------------------------------------------------
// RemoveNodeCommand

class RemoveNodeCommand : public EditCommand
{
public:
	RemoveNodeCommand(DOM::DocumentImpl *, DOM::NodeImpl *node);
	virtual ~RemoveNodeCommand();

    DOM::NodeImpl *node() const;
    
private:
    inline RemoveNodeCommandImpl *impl() const;
};

//------------------------------------------------------------------------------------------
// SplitTextNodeCommand

class SplitTextNodeCommand : public ModifyTextNodeCommand
{
public:
	SplitTextNodeCommand(DOM::DocumentImpl *, DOM::TextImpl *, long);
	virtual ~SplitTextNodeCommand();

    DOM::TextImpl *node() const;
    long offset() const;
    
private:
    inline SplitTextNodeCommandImpl *impl() const;
};

//------------------------------------------------------------------------------------------
// TypingCommand

class TypingCommand : public CompositeEditCommand
{
public:
    static void deleteKeyPressed(DOM::DocumentImpl *document);
    static void insertText(DOM::DocumentImpl *document, const DOM::DOMString &text);
    static void insertNewline(DOM::DocumentImpl *document);
    static bool isOpenForMoreTypingCommand(const EditCommand &);
    static void closeTyping(EditCommand);

    bool openForMoreTyping() const;
    void closeTyping();

private:
	TypingCommand(DOM::DocumentImpl *document);
	TypingCommand(TypingCommand *);
	TypingCommand(const TypingCommand &);
	virtual ~TypingCommand();

    void deleteKeyPressed();
    void insertText(const DOM::DOMString &text);
    void insertNewline();

    inline TypingCommandImpl *impl() const;
};

//------------------------------------------------------------------------------------------

} // end namespace khtml

#endif
