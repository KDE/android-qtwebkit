/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "WREC.h"

#if ENABLE(WREC)

#include "CharacterClassConstructor.h"
#include "ExecState.h"
#include "Machine.h"
#include "pcre_internal.h"

using namespace WTF;

namespace JSC {

class GenerateAtomFunctor {
public:
    virtual ~GenerateAtomFunctor() {}

    virtual void generateAtom(WRECGenerator*, JmpSrcVector&) = 0;
    virtual void backtrack(WRECGenerator*) = 0;
};

class GeneratePatternCharacterFunctor : public GenerateAtomFunctor {
public:
    GeneratePatternCharacterFunctor(const UChar ch)
        : m_ch(ch)
    {
    }

    virtual void generateAtom(WRECGenerator*, JmpSrcVector&);
    virtual void backtrack(WRECGenerator*);

private:
    const UChar m_ch;
};

class GenerateCharacterClassFunctor : public GenerateAtomFunctor {
public:
    GenerateCharacterClassFunctor(CharacterClass* charClass, bool invert)
        : m_charClass(charClass)
        , m_invert(invert)
    {
    }

    virtual void generateAtom(WRECGenerator*, JmpSrcVector&);
    virtual void backtrack(WRECGenerator*);

private:
    CharacterClass* m_charClass;
    bool m_invert;
};

class GenerateBackreferenceFunctor : public GenerateAtomFunctor {
public:
    GenerateBackreferenceFunctor(unsigned subpatternId)
        : m_subpatternId(subpatternId)
    {
    }

    virtual void generateAtom(WRECGenerator*, JmpSrcVector&);
    virtual void backtrack(WRECGenerator*);

private:
    unsigned m_subpatternId;
};

class GenerateParenthesesNonGreedyFunctor : public GenerateAtomFunctor {
public:
    GenerateParenthesesNonGreedyFunctor(WRECGenerator::JmpDst start, WRECGenerator::JmpSrc success, WRECGenerator::JmpSrc fail)
        : m_start(start)
        , m_success(success)
        , m_fail(fail)
    {
    }

    virtual void generateAtom(WRECGenerator*, JmpSrcVector&);
    virtual void backtrack(WRECGenerator*);

private:
    WRECGenerator::JmpDst m_start;
    WRECGenerator::JmpSrc m_success;
    WRECGenerator::JmpSrc m_fail;
};

void GeneratePatternCharacterFunctor::generateAtom(WRECGenerator* wrec, JmpSrcVector& failures)
{
    wrec->generatePatternCharacter(failures, m_ch);
}
void GeneratePatternCharacterFunctor::backtrack(WRECGenerator* wrec)
{
    wrec->generateBacktrack1();
}

void GenerateCharacterClassFunctor::generateAtom(WRECGenerator* wrec, JmpSrcVector& failures)
{
    wrec->generateCharacterClass(failures, *m_charClass, m_invert);
}
void GenerateCharacterClassFunctor::backtrack(WRECGenerator* wrec)
{
    wrec->generateBacktrack1();
}

void GenerateBackreferenceFunctor::generateAtom(WRECGenerator* wrec, JmpSrcVector& failures)
{
    wrec->generateBackreference(failures, m_subpatternId);
}
void GenerateBackreferenceFunctor::backtrack(WRECGenerator* wrec)
{
    wrec->generateBacktrackBackreference(m_subpatternId);
}

void GenerateParenthesesNonGreedyFunctor::generateAtom(WRECGenerator* wrec, JmpSrcVector& failures)
{
    wrec->generateParenthesesNonGreedy(failures, m_start, m_success, m_fail);
}

void GenerateParenthesesNonGreedyFunctor::backtrack(WRECGenerator*)
{
    // FIXME: do something about this.
    CRASH();
}

void WRECGenerator::generateBacktrack1()
{
    m_jit.subl_i8r(1, currentPositionRegister);
}

void WRECGenerator::generateBacktrackBackreference(unsigned subpatternId)
{
    m_jit.subl_mr((2 * subpatternId + 1) * sizeof(int), outputRegister, currentPositionRegister);
    m_jit.addl_mr((2 * subpatternId) * sizeof(int), outputRegister, currentPositionRegister);
}

void WRECGenerator::generateBackreferenceQuantifier(JmpSrcVector& failures, Quantifier::Type quantifierType, unsigned subpatternId, unsigned min, unsigned max)
{
    GenerateBackreferenceFunctor functor(subpatternId);

    m_jit.movl_mr((2 * subpatternId) * sizeof(int), outputRegister, currentValueRegister);
    m_jit.cmpl_rm(currentValueRegister, ((2 * subpatternId) + 1) * sizeof(int), outputRegister);
    JmpSrc skipIfEmpty = m_jit.emitUnlinkedJe();

    ASSERT(quantifierType == Quantifier::Greedy || quantifierType == Quantifier::NonGreedy);
    if (quantifierType == Quantifier::Greedy)
        generateGreedyQuantifier(failures, functor, min, max);
    else
        generateNonGreedyQuantifier(failures, functor, min, max);

    m_jit.link(skipIfEmpty, m_jit.label());
}

void WRECGenerator::generateNonGreedyQuantifier(JmpSrcVector& failures, GenerateAtomFunctor& functor, unsigned min, unsigned max)
{
    // comment me better!
    JmpSrcVector newFailures;

    // (0) Setup:
    //     init quantifierCountRegister
    m_jit.pushl_r(quantifierCountRegister);
    m_jit.xorl_rr(quantifierCountRegister, quantifierCountRegister);
    JmpSrc gotoStart = m_jit.emitUnlinkedJmp();

    // (4) Failure case

    JmpDst quantifierFailed = m_jit.label();
    // (4.1) Restore original value of quantifierCountRegister from the stack
    m_jit.popl_r(quantifierCountRegister);
    failures.append(m_jit.emitUnlinkedJmp()); 

    // (3) We just tried an alternative, and it failed - check we can try more.
    
    JmpDst alternativeFailed = m_jit.label();
    // (3.1) remove the value pushed prior to testing the alternative
    m_jit.popl_r(currentPositionRegister);
    // (3.2) if there is a limit, and we have reached it, game over. 
    if (max != Quantifier::noMaxSpecified) {
        m_jit.cmpl_i32r(max, quantifierCountRegister);
        m_jit.link(m_jit.emitUnlinkedJe(), quantifierFailed);
    }

    // (1) Do a check for the atom

    // (1.0) This is where we start, if there is a minimum (then we must read at least one of the atom).
    JmpDst testQuantifiedAtom = m_jit.label();
    if (min)
        m_jit.link(gotoStart, testQuantifiedAtom);
    // (1.1) Do a check for the atom check.
    functor.generateAtom(this, newFailures);
    // (1.2) If we get here, successful match!
    m_jit.addl_i8r(1, quantifierCountRegister);
    // (1.3) We needed to read the atom, and we failed - that's terminally  bad news.
    for (unsigned i = 0; i < newFailures.size(); ++i)
        m_jit.link(newFailures[i], quantifierFailed);
    newFailures.clear();
    // (1.4) If there is a minimum, check we have read enough ...
    if (min) {
        // ... except if min was 1 no need to keep checking!
        if (min != 1) {
            m_jit.cmpl_i32r(min, quantifierCountRegister);
            m_jit.link(m_jit.emitUnlinkedJl(), testQuantifiedAtom);
        }
    } else
        m_jit.link(gotoStart, m_jit.label());
    // if there was no minimum, this is where we start.

    // (2) Plant an alternative check for the remainder of the expression
    
    // (2.1) recursively call to parseAlternative, if it falls through, success!
    m_jit.pushl_r(currentPositionRegister);
    m_parser.parseAlternative(newFailures);
    m_jit.addl_i8r(4, X86::esp);
    m_jit.popl_r(quantifierCountRegister);
    // (2.2) link failure cases to jump back up to alternativeFailed.
    for (unsigned i = 0; i < newFailures.size(); ++i)
        m_jit.link(newFailures[i], alternativeFailed);
    newFailures.clear();
}

void WRECGenerator::generateGreedyQuantifier(JmpSrcVector& failures, GenerateAtomFunctor& functor, unsigned min, unsigned max)
{
    if (!max)
        return;

    // comment me better!
    JmpSrcVector newFailures;

    // (0) Setup:
    //     init quantifierCountRegister
    m_jit.pushl_r(quantifierCountRegister);
    m_jit.xorl_rr(quantifierCountRegister, quantifierCountRegister);

    // (1) Greedily read as many of the atom as possible

    JmpDst readMore = m_jit.label();

    // (1.1) Do a character class check.
    functor.generateAtom(this, newFailures);
    // (1.2) If we get here, successful match!
    m_jit.addl_i8r(1, quantifierCountRegister);
    // (1.3) loop, unless we've read max limit.
    if (max != Quantifier::noMaxSpecified) {
        if (max != 1) {
            // if there is a limit, only loop while less than limit, otherwise fall throught to...
            m_jit.cmpl_i32r(max, quantifierCountRegister);
            m_jit.link(m_jit.emitUnlinkedJne(), readMore);
        }
        // ...if there is no min we need jump to the alternative test, if there is we can just fall through to it.
        if (!min)
            newFailures.append(m_jit.emitUnlinkedJmp());
    } else
        m_jit.link(m_jit.emitUnlinkedJmp(), readMore);
    // (1.4) check enough matches to bother trying an alternative...
    if (min) {
        // We will fall through to here if (min && max), after the max check.
        // First, also link a
        JmpDst minimumTest = m_jit.label();
        for (unsigned i = 0; i < newFailures.size(); ++i)
            m_jit.link(newFailures[i], minimumTest);
        newFailures.clear();
        // 
        m_jit.cmpl_i32r(min, quantifierCountRegister);
        newFailures.append(m_jit.emitUnlinkedJae());
    }

    // (4) Failure case

    JmpDst quantifierFailed = m_jit.label();
    // (4.1) Restore original value of quantifierCountRegister from the stack
    m_jit.popl_r(quantifierCountRegister);
    failures.append(m_jit.emitUnlinkedJmp()); 

    // (3) Backtrack

    JmpDst backtrack = m_jit.label();
    // (3.1) this was preserved prior to executing the alternative
    m_jit.popl_r(currentPositionRegister);
    // (3.2) check we can retry with fewer matches - backtracking fails if already at the minimum
    m_jit.cmpl_i32r(min, quantifierCountRegister);
    m_jit.link(m_jit.emitUnlinkedJe(), quantifierFailed);
    // (3.3) roll off one match, and retry.
    functor.backtrack(this);
    m_jit.subl_i8r(1, quantifierCountRegister);

    // (2) Try an alternative.

    // (2.1) point to retry
    JmpDst tryAlternative = m_jit.label();
    for (unsigned i = 0; i < newFailures.size(); ++i)
        m_jit.link(newFailures[i], tryAlternative);
    newFailures.clear();
    // (2.2) recursively call to parseAlternative, if it falls through, success!
    m_jit.pushl_r(currentPositionRegister);
    m_parser.parseAlternative(newFailures);
    m_jit.addl_i8r(4, X86::esp);
    m_jit.popl_r(quantifierCountRegister);
    // (2.3) link failure cases to here.
    for (unsigned i = 0; i < newFailures.size(); ++i)
        m_jit.link(newFailures[i], backtrack);
    newFailures.clear();
}

void WRECGenerator::generatePatternCharacter(JmpSrcVector& failures, int ch)
{
    // check there is more input, read a char.
    m_jit.cmpl_rr(lengthRegister, currentPositionRegister);
    failures.append(m_jit.emitUnlinkedJe());
    m_jit.movzwl_mr(inputRegister, currentPositionRegister, 2, currentValueRegister);

    // used for unicode case insensitive
    bool hasUpper = false;
    JmpSrc isUpper;
    
    // if case insensitive match
    if (m_parser.m_ignoreCase) {
        UChar lower, upper;
        
        // check for ascii case sensitive characters
        if (isASCIIAlpha(ch)) {
            m_jit.orl_i32r(32, currentValueRegister);
            ch |= 32;
        } else if ((ch > 0x7f) && ((lower = Unicode::toLower(ch)) != (upper = Unicode::toUpper(ch)))) {
            // handle unicode case sentitive characters - branch to success on upper
            m_jit.cmpl_i32r(upper, currentValueRegister);
            isUpper = m_jit.emitUnlinkedJe();
            hasUpper = true;
            ch = lower;
        }
    }
    
    // checks for ch, or lower case version of ch, if insensitive
    m_jit.cmpl_i32r((unsigned short)ch, currentValueRegister);
    failures.append(m_jit.emitUnlinkedJne());

    if (m_parser.m_ignoreCase && hasUpper) {
        // for unicode case insensitive matches, branch here if upper matches.
        m_jit.link(isUpper, m_jit.label());
    }
    
    // on success consume the char
    m_jit.addl_i8r(1, currentPositionRegister);
}

void WRECGenerator::generateCharacterClassInvertedRange(JmpSrcVector& failures, JmpSrcVector& matchDest, const CharacterClassRange* ranges, unsigned count, unsigned* matchIndex, const UChar* matches, unsigned matchCount)
{
    do {
        // pick which range we're going to generate
        int which = count >> 1;
        char lo = ranges[which].begin;
        char hi = ranges[which].end;
        
        m_jit.cmpl_i32r((unsigned short)lo, currentValueRegister);

        // check if there are any ranges or matches below lo.  If not, just jl to failure -
        // if there is anything else to check, check that first, if it falls through jmp to failure.
        if ((*matchIndex < matchCount) && (matches[*matchIndex] < lo)) {
            JmpSrc loOrAbove = m_jit.emitUnlinkedJge();
            
            // generate code for all ranges before this one
            if (which)
                generateCharacterClassInvertedRange(failures, matchDest, ranges, which, matchIndex, matches, matchCount);
            
            do {
                m_jit.cmpl_i32r((unsigned short)matches[*matchIndex], currentValueRegister);
                matchDest.append(m_jit.emitUnlinkedJe());
                ++*matchIndex;
            } while ((*matchIndex < matchCount) && (matches[*matchIndex] < lo));
            failures.append(m_jit.emitUnlinkedJmp());

            m_jit.link(loOrAbove, m_jit.label());
        } else if (which) {
            JmpSrc loOrAbove = m_jit.emitUnlinkedJge();

            generateCharacterClassInvertedRange(failures, matchDest, ranges, which, matchIndex, matches, matchCount);
            failures.append(m_jit.emitUnlinkedJmp());

            m_jit.link(loOrAbove, m_jit.label());
        } else
            failures.append(m_jit.emitUnlinkedJl());

        while ((*matchIndex < matchCount) && (matches[*matchIndex] <= hi))
            ++*matchIndex;

        m_jit.cmpl_i32r((unsigned short)hi, currentValueRegister);
        matchDest.append(m_jit.emitUnlinkedJle());
        // fall through to here, the value is above hi.

        // shuffle along & loop around if there are any more matches to handle.
        unsigned next = which + 1;
        ranges += next;
        count -= next;
    } while (count);
}

void WRECGenerator::generateCharacterClassInverted(JmpSrcVector& matchDest, CharacterClass& charClass)
{
    JmpSrc unicodeFail;
    if (charClass.numMatchesUnicode || charClass.numRangesUnicode) {
        m_jit.cmpl_i8r(0x7f, currentValueRegister);
        JmpSrc isAscii = m_jit.emitUnlinkedJle();
    
        if (charClass.numMatchesUnicode) {
            for (unsigned i = 0; i < charClass.numMatchesUnicode; ++i) {
                UChar ch = charClass.matchesUnicode[i];
                m_jit.cmpl_i32r((unsigned short)ch, currentValueRegister);
                matchDest.append(m_jit.emitUnlinkedJe());
            }
        }
        
        if (charClass.numRangesUnicode) {
            for (unsigned i = 0; i < charClass.numRangesUnicode; ++i) {
                UChar lo = charClass.rangesUnicode[i].begin;
                UChar hi = charClass.rangesUnicode[i].end;
                
                m_jit.cmpl_i32r((unsigned short)lo, currentValueRegister);
                JmpSrc below = m_jit.emitUnlinkedJl();
                m_jit.cmpl_i32r((unsigned short)hi, currentValueRegister);
                matchDest.append(m_jit.emitUnlinkedJle());
                m_jit.link(below, m_jit.label());
            }
        }

        unicodeFail = m_jit.emitUnlinkedJmp();
        m_jit.link(isAscii, m_jit.label());
    }

    if (charClass.numRanges) {
        unsigned matchIndex = 0;
        JmpSrcVector failures; 
        generateCharacterClassInvertedRange(failures, matchDest, charClass.ranges, charClass.numRanges, &matchIndex, charClass.matches, charClass.numMatches);
        while (matchIndex < charClass.numMatches) {
            m_jit.cmpl_i32r((unsigned short)charClass.matches[matchIndex], currentValueRegister);
            matchDest.append(m_jit.emitUnlinkedJe());
            ++matchIndex;
        }
        JmpDst noMatch = m_jit.label();
        for (unsigned i = 0; i < failures.size(); ++i)
            m_jit.link(failures[i], noMatch);
        failures.clear();
    } else if (charClass.numMatches) {
        // optimization: gather 'a','A' etc back together, can mask & test once.
        Vector<char> matchesAZaz;

        for (unsigned i = 0; i < charClass.numMatches; ++i) {
            char ch = charClass.matches[i];
            if (m_parser.m_ignoreCase) {
                if (isASCIILower(ch)) {
                    matchesAZaz.append(ch);
                    continue;
                }
                if (isASCIIUpper(ch))
                    continue;
            }
            m_jit.cmpl_i32r((unsigned short)ch, currentValueRegister);
            matchDest.append(m_jit.emitUnlinkedJe());
        }

        if (unsigned countAZaz = matchesAZaz.size()) {
            m_jit.orl_i32r(32, currentValueRegister);

            for (unsigned i = 0; i < countAZaz; ++i) {
                char ch = matchesAZaz[i];
                m_jit.cmpl_i32r((unsigned short)ch, currentValueRegister);
                matchDest.append(m_jit.emitUnlinkedJe());
            }
        }
    }

    if (charClass.numMatchesUnicode || charClass.numRangesUnicode)
        m_jit.link(unicodeFail, m_jit.label());
}

void WRECGenerator::generateCharacterClass(JmpSrcVector& failures, CharacterClass& charClass, bool invert)
{
    m_jit.cmpl_rr(lengthRegister, currentPositionRegister);
    failures.append(m_jit.emitUnlinkedJe());
    m_jit.movzwl_mr(inputRegister, currentPositionRegister, 2, currentValueRegister);

    if (invert)
        generateCharacterClassInverted(failures, charClass);
    else {
        JmpSrcVector successes;
        generateCharacterClassInverted(successes, charClass);
        failures.append(m_jit.emitUnlinkedJmp());
        JmpDst here = m_jit.label();
        for (unsigned i = 0; i < successes.size(); ++i)
            m_jit.link(successes[i], here);
    }
    
    m_jit.addl_i8r(1, currentPositionRegister);
}

WRECGenerator::JmpSrc WRECGenerator::generateParentheses(ParenthesesType type)
{
    unsigned subpatternId = (type == capturing) ? ++m_parser.m_numSubpatterns : m_parser.m_numSubpatterns;

    // push pos, both to preserve for fail + reloaded in parseDisjunction
    m_jit.pushl_r(currentPositionRegister);

    // Plant a disjunction, wrapped to invert behaviour - 
    JmpSrcVector newFailures;
    m_parser.parseDisjunction(newFailures);
    
    if (type == capturing) {
        m_jit.popl_r(currentValueRegister);
        m_jit.movl_rm(currentValueRegister, (2 * subpatternId) * sizeof(int), outputRegister);
        m_jit.movl_rm(currentPositionRegister, (2 * subpatternId + 1) * sizeof(int), outputRegister);
    } else if (type == non_capturing)
        m_jit.addl_i8r(4, X86::esp);
    else
        m_jit.popl_r(currentPositionRegister);

    // This is a little lame - jump to jump if there is a nested disjunction.
    // (suggestion to fix: make parseDisjunction populate a JmpSrcVector of
    // disjunct successes... this is probably not worth the compile cost in
    // the common case to fix).
    JmpSrc successfulMatch = m_jit.emitUnlinkedJmp();

    JmpDst originalFailure = m_jit.label();
    for (unsigned i = 0; i < newFailures.size(); ++i)
        m_jit.link(newFailures[i], originalFailure);
    newFailures.clear();
    // fail: restore currentPositionRegister
    m_jit.popl_r(currentPositionRegister);

    JmpSrc jumpToFail;
    // If this was an inverted assert, fail = success! - just let the failure case drop through,
    // success case goes to failures.  Both paths restore curr pos.
    if (type == inverted_assertion)
        jumpToFail = successfulMatch;
    else {
        // plant a jump so any fail will link off to 'failures',
        jumpToFail = m_jit.emitUnlinkedJmp();
        // link successes to jump here
        m_jit.link(successfulMatch, m_jit.label());
    }
    return jumpToFail;
}

void WRECGenerator::generateParenthesesNonGreedy(JmpSrcVector& failures, JmpDst start, JmpSrc success, JmpSrc fail)
{
    m_jit.link(m_jit.emitUnlinkedJmp(), start);
    m_jit.link(success, m_jit.label());

    failures.append(fail);
}

WRECGenerator::JmpSrc WRECGenerator::gererateParenthesesResetTrampoline(JmpSrcVector& newFailures, unsigned subpatternIdBefore, unsigned subpatternIdAfter)
{
    JmpSrc skip = m_jit.emitUnlinkedJmp();

    JmpDst subPatternResetTrampoline = m_jit.label();
    for (unsigned i = 0; i < newFailures.size(); ++i)
        m_jit.link(newFailures[i], subPatternResetTrampoline);
    newFailures.clear();
    for (unsigned i = subpatternIdBefore + 1; i <= subpatternIdAfter; ++i) {
        m_jit.movl_i32m(-1, (2 * i) * sizeof(int), outputRegister);
        m_jit.movl_i32m(-1, (2 * i + 1) * sizeof(int), outputRegister);
    }
    
    JmpSrc newFailJump = m_jit.emitUnlinkedJmp();
    m_jit.link(skip, m_jit.label());
    
    return newFailJump;
}

void WRECGenerator::generateAssertionBOL(JmpSrcVector& failures)
{
    if (m_parser.m_multiline) {
        JmpSrcVector previousIsNewline;

        // begin of input == success
        m_jit.cmpl_i8r(0, currentPositionRegister);
        previousIsNewline.append(m_jit.emitUnlinkedJe());

        // now check prev char against newline characters.
        m_jit.movzwl_mr(-2, inputRegister, currentPositionRegister, 2, currentValueRegister);
        generateCharacterClassInverted(previousIsNewline, getCharacterClassNewline());

        failures.append(m_jit.emitUnlinkedJmp());

        JmpDst success = m_jit.label();
        for (unsigned i = 0; i < previousIsNewline.size(); ++i)
            m_jit.link(previousIsNewline[i], success);
        previousIsNewline.clear();
    } else {
        m_jit.cmpl_i8r(0, currentPositionRegister);
        failures.append(m_jit.emitUnlinkedJne());
    }
}

void WRECGenerator::generateAssertionEOL(JmpSrcVector& failures)
{
    if (m_parser.m_multiline) {
        JmpSrcVector nextIsNewline;

        // end of input == success
        m_jit.cmpl_rr(lengthRegister, currentPositionRegister);
        nextIsNewline.append(m_jit.emitUnlinkedJe());

        // now check next char against newline characters.
        m_jit.movzwl_mr(inputRegister, currentPositionRegister, 2, currentValueRegister);
        generateCharacterClassInverted(nextIsNewline, getCharacterClassNewline());

        failures.append(m_jit.emitUnlinkedJmp());

        JmpDst success = m_jit.label();
        for (unsigned i = 0; i < nextIsNewline.size(); ++i)
            m_jit.link(nextIsNewline[i], success);
        nextIsNewline.clear();
    } else {
        m_jit.cmpl_rr(lengthRegister, currentPositionRegister);
        failures.append(m_jit.emitUnlinkedJne());
    }
}

void WRECGenerator::generateAssertionWordBoundary(JmpSrcVector& failures, bool invert)
{
    JmpSrcVector wordBoundary;
    JmpSrcVector notWordBoundary;

    // (1) Check if the previous value was a word char

    // (1.1) check for begin of input
    m_jit.cmpl_i8r(0, currentPositionRegister);
    JmpSrc atBegin = m_jit.emitUnlinkedJe();
    // (1.2) load the last char, and chck if is word character
    m_jit.movzwl_mr(-2, inputRegister, currentPositionRegister, 2, currentValueRegister);
    JmpSrcVector previousIsWord;
    generateCharacterClassInverted(previousIsWord, getCharacterClassWordchar());
    // (1.3) if we get here, previous is not a word char
    m_jit.link(atBegin, m_jit.label());

    // (2) Handle situation where previous was NOT a \w

    // (2.1) check for end of input
    m_jit.cmpl_rr(lengthRegister, currentPositionRegister);
    notWordBoundary.append(m_jit.emitUnlinkedJe());
    // (2.2) load the next char, and chck if is word character
    m_jit.movzwl_mr(inputRegister, currentPositionRegister, 2, currentValueRegister);
    generateCharacterClassInverted(wordBoundary, getCharacterClassWordchar());
    // (2.3) If we get here, neither chars are word chars
    notWordBoundary.append(m_jit.emitUnlinkedJmp());

    // (3) Handle situation where previous was a \w

    // (3.0) link success in first match to here
    JmpDst section3 = m_jit.label();
    for (unsigned i = 0; i < previousIsWord.size(); ++i)
        m_jit.link(previousIsWord[i], section3);
    previousIsWord.clear();
    // (3.1) check for end of input
    m_jit.cmpl_rr(lengthRegister, currentPositionRegister);
    wordBoundary.append(m_jit.emitUnlinkedJe());
    // (3.2) load the next char, and chck if is word character
    m_jit.movzwl_mr(inputRegister, currentPositionRegister, 2, currentValueRegister);
    generateCharacterClassInverted(notWordBoundary, getCharacterClassWordchar());
    // (3.3) If we get here, this is an end of a word, within the input.
    
    // (4) Link everything up
    
    if (invert) {
        // handle the fall through case
        wordBoundary.append(m_jit.emitUnlinkedJmp());
    
        // looking for non word boundaries, so link boundary fails to here.
        JmpDst success = m_jit.label();
        for (unsigned i = 0; i < notWordBoundary.size(); ++i)
            m_jit.link(notWordBoundary[i], success);
        notWordBoundary.clear();
        
        failures.append(wordBoundary.begin(), wordBoundary.size());
    } else {
        // looking for word boundaries, so link successes here.
        JmpDst success = m_jit.label();
        for (unsigned i = 0; i < wordBoundary.size(); ++i)
            m_jit.link(wordBoundary[i], success);
        wordBoundary.clear();
        
        failures.append(notWordBoundary.begin(), notWordBoundary.size());
    }
}

void WRECGenerator::generateBackreference(JmpSrcVector& failures, unsigned subpatternId)
{
    m_jit.pushl_r(currentPositionRegister);
    m_jit.pushl_r(quantifierCountRegister);

    // get the start pos of the backref into quantifierCountRegister (multipurpose!)
    m_jit.movl_mr((2 * subpatternId) * sizeof(int), outputRegister, quantifierCountRegister);

    JmpSrc skipIncrement = m_jit.emitUnlinkedJmp();
    JmpDst topOfLoop = m_jit.label();
    m_jit.addl_i8r(1, currentPositionRegister);
    m_jit.addl_i8r(1, quantifierCountRegister);
    m_jit.link(skipIncrement, m_jit.label());

    // check if we're at the end of backref (if we are, success!)
    m_jit.cmpl_rm(quantifierCountRegister, ((2 * subpatternId) + 1) * sizeof(int), outputRegister);
    JmpSrc endOfBackRef = m_jit.emitUnlinkedJe();
    
    m_jit.movzwl_mr(inputRegister, quantifierCountRegister, 2, currentValueRegister);
    
    // check if we've run out of input (this would be a can o'fail)
    m_jit.cmpl_rr(lengthRegister, currentPositionRegister);
    JmpSrc endOfInput = m_jit.emitUnlinkedJe();
    
    m_jit.cmpw_rm(currentValueRegister, inputRegister, currentPositionRegister, 2);
    m_jit.link(m_jit.emitUnlinkedJe(), topOfLoop);
    
    m_jit.link(endOfInput, m_jit.label());
    // Failure
    m_jit.popl_r(quantifierCountRegister);
    m_jit.popl_r(currentPositionRegister);
    failures.append(m_jit.emitUnlinkedJmp());
    
    // Success
    m_jit.link(endOfBackRef, m_jit.label());
    m_jit.popl_r(quantifierCountRegister);
    m_jit.addl_i8r(4, X86::esp);
}

void WRECGenerator::gernerateDisjunction(JmpSrcVector& successes, JmpSrcVector& failures)
{
    successes.append(m_jit.emitUnlinkedJmp());
    
    JmpDst here = m_jit.label();
    
    for (unsigned i = 0; i < failures.size(); ++i)
        m_jit.link(failures[i], here);
    failures.clear();
    
    m_jit.movl_mr(X86::esp, currentPositionRegister);
}

void WRECGenerator::terminateDisjunction(JmpSrcVector& successes)
{
    JmpDst here = m_jit.label();
    for (unsigned i = 0; i < successes.size(); ++i)
        m_jit.link(successes[i], here);
    successes.clear();
}

ALWAYS_INLINE Quantifier WRECParser::parseGreedyQuantifier()
{
    switch (peek()) {
        case '?':
            consume();
            return Quantifier(Quantifier::Greedy, 0, 1);

        case '*':
            consume();
            return Quantifier(Quantifier::Greedy, 0);

        case '+':
            consume();
            return Quantifier(Quantifier::Greedy, 1);

        case '{': {
            consume();
            // a numeric quantifier should always have a lower bound
            if (!peekIsDigit()) {
                m_err = Error_malformedQuantifier;
                return Quantifier(Quantifier::Error);
            }
            int min = consumeNumber();
            
            // this should either be a , or a }
            switch (peek()) {
            case '}':
                // {n} - exactly n times. (technically I think a '?' is valid in the bnf - bit meaningless).
                consume();
                return Quantifier(Quantifier::Greedy, min, min);

            case ',':
                consume();
                switch (peek()) {
                case '}':
                    // {n,} - n to inf times.
                    consume();
                    return Quantifier(Quantifier::Greedy, min);

                case '0':
                case '1':
                case '2':
                case '3':
                case '4':
                case '5':
                case '6':
                case '7':
                case '8':
                case '9': {
                    // {n,m} - n to m times.
                    int max = consumeNumber();
                    
                    if (peek() != '}') {
                        m_err = Error_malformedQuantifier;
                        return Quantifier(Quantifier::Error);
                    }
                    consume();

                    return Quantifier(Quantifier::Greedy, min, max);
                }

                default:
                    m_err = Error_malformedQuantifier;
                    return Quantifier(Quantifier::Error);
                }

            default:
                m_err = Error_malformedQuantifier;
                return Quantifier(Quantifier::Error);
            }
        }
        // None of the above - no quantifier
        default:
            return Quantifier();
    }
}

Quantifier WRECParser::parseQuantifier()
{
    Quantifier q = parseGreedyQuantifier();
    
    if ((q.type == Quantifier::Greedy) && (peek() == '?')) {
        consume();
        q.type = Quantifier::NonGreedy;
    }
    
    return q;
}

bool WRECParser::parsePatternCharacterQualifier(JmpSrcVector& failures, int ch)
{
    Quantifier q = parseQuantifier();

    switch (q.type) {
    case Quantifier::None: {
        m_generator.generatePatternCharacter(failures, ch);
        break;
    }

    case Quantifier::Greedy: {
        GeneratePatternCharacterFunctor functor(ch);
        m_generator.generateGreedyQuantifier(failures, functor, q.min, q.max);
        break;
    }

    case Quantifier::NonGreedy: {
        GeneratePatternCharacterFunctor functor(ch);
        m_generator.generateNonGreedyQuantifier(failures, functor, q.min, q.max);
        break;
    }

    case Quantifier::Error:
        return false;
    }
    
    return true;
}

bool WRECParser::parseCharacterClassQuantifier(JmpSrcVector& failures, CharacterClass& charClass, bool invert)
{
    Quantifier q = parseQuantifier();

    switch (q.type) {
    case Quantifier::None: {
        m_generator.generateCharacterClass(failures, charClass, invert);
        break;
    }

    case Quantifier::Greedy: {
        GenerateCharacterClassFunctor functor(&charClass, invert);
        m_generator.generateGreedyQuantifier(failures, functor, q.min, q.max);
        break;
    }

    case Quantifier::NonGreedy: {
        GenerateCharacterClassFunctor functor(&charClass, invert);
        m_generator.generateNonGreedyQuantifier(failures, functor, q.min, q.max);
        break;
    }

    case Quantifier::Error:
        return false;
    }
    
    return true;
}

bool WRECParser::parseBackreferenceQuantifier(JmpSrcVector& failures, unsigned subpatternId)
{
    Quantifier q = parseQuantifier();

    switch (q.type) {
    case Quantifier::None: {
        m_generator.generateBackreference(failures, subpatternId);
        break;
    }

    case Quantifier::Greedy:
    case Quantifier::NonGreedy:
        m_generator.generateBackreferenceQuantifier(failures, q.type, subpatternId, q.min, q.max);
        return true;

    case Quantifier::Error:
        return false;
    }
    
    return true;
}

bool WRECParser::parseParentheses(JmpSrcVector&)
{
    // FIXME: We don't currently backtrack correctly within parentheses in cases such as
    // "c".match(/(.*)c/) so we fall back to PCRE for any regexp containing parentheses.

    m_err = TempError_unsupportedParentheses;
    return false;
}

bool WRECParser::parseCharacterClass(JmpSrcVector& failures)
{
    bool invert = false;
    if (peek() == '^') {
        consume();
        invert = true;
    }

    CharacterClassConstructor charClassConstructor(m_ignoreCase);

    UChar ch;
    while ((ch = peek()) != ']') {
        switch (ch) {
        case EndOfPattern:
            m_err = Error_malformedCharacterClass;
            return false;
            
        case '\\':
            consume();
            switch (ch = peek()) {
            case EndOfPattern:
                m_err = Error_malformedEscape;
                return false;
            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
                charClassConstructor.put(consumeOctal());
                break;

            // ControlEscape
            case 'b':
                consume();
                charClassConstructor.put('\b');
                break;
            case 'f':
                consume();
                charClassConstructor.put('\f');
                break;
            case 'n':
                consume();
                charClassConstructor.put('\n');
                break;
            case 'r':
                consume();
                charClassConstructor.put('\r');
                break;
            case 't':
                consume();
                charClassConstructor.put('\t');
                break;
            case 'v':
                consume();
                charClassConstructor.put('\v');
                break;

            // ControlLetter
            case 'c': {
                consume();
                int control = consume();
                if (!isASCIIAlpha(control)) {
                    m_err = Error_malformedEscape;
                    return false;
                }
                charClassConstructor.put(control&31);
                break;
            }

            // HexEscape
            case 'x': {
                consume();
                int x = consumeHex(2);
                if (x == -1) {
                    m_err = Error_malformedEscape;
                    return false;
                }
                charClassConstructor.put(x);
                break;
            }

            // UnicodeEscape
            case 'u': {
                consume();
                int x = consumeHex(4);
                if (x == -1) {
                    m_err = Error_malformedEscape;
                    return false;
                }
                charClassConstructor.put(x);
                break;
            }
            
            // CharacterClassEscape
            case 'd':
                consume();
                charClassConstructor.append(getCharacterClassDigits());
                break;
            case 's':
                consume();
                charClassConstructor.append(getCharacterClassSpaces());
                break;
            case 'w':
                consume();
                charClassConstructor.append(getCharacterClassWordchar());
                break;
            case 'D':
                consume();
                charClassConstructor.append(getCharacterClassNondigits());
                break;
            case 'S':
                consume();
                charClassConstructor.append(getCharacterClassNonspaces());
                break;
            case 'W':
                consume();
                charClassConstructor.append(getCharacterClassNonwordchar());
                break;

            case '-':
                consume();
                charClassConstructor.put(ch);
                charClassConstructor.flush();
                break;

            // IdentityEscape
            default: {
                // TODO: check this test for IdentifierPart.
                int ch = consume();
                if (isASCIIAlphanumeric(ch) || (ch == '_')) {
                    m_err = Error_malformedEscape;
                    return false;
                }
                charClassConstructor.put(ch);
                break;
            }
            }
            break;
            
        default:
            consume();
            charClassConstructor.put(ch);
        }
    }
    consume();

    // lazily catch reversed ranges ([z-a])in character classes
    if (charClassConstructor.isUpsideDown()) {
        m_err = Error_malformedCharacterClass;
        return false;
    }

    charClassConstructor.flush();
    CharacterClass charClass = charClassConstructor.charClass();
    return parseCharacterClassQuantifier(failures, charClass, invert);
}

bool WRECParser::parseOctalEscape(JmpSrcVector& failures)
{
    return parsePatternCharacterQualifier(failures, consumeOctal());
}

bool WRECParser::parseEscape(JmpSrcVector& failures)
{
    switch (peek()) {
    case EndOfPattern:
        m_err = Error_malformedEscape;
        return false;

    // Assertions
    case 'b':
        consume();
        m_generator.generateAssertionWordBoundary(failures, false);
        return true;

    case 'B':
        consume();
        m_generator.generateAssertionWordBoundary(failures, true);
        return true;

    // Octal escape
    case '0':
        consume();
        return parseOctalEscape(failures);

    // DecimalEscape
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7': {
        // FIXME: This does not support forward references. It's not clear whether they
        // should be valid.
        unsigned value = peekDigit();
        if (value > m_numSubpatterns)
            return parseOctalEscape(failures);
    }
    case '8':
    case '9': {
        unsigned value = peekDigit();
        if (value > m_numSubpatterns) {
            consume();
            m_err = Error_malformedEscape;
            return false;
        }
        consume();

        while (peekIsDigit()) {
            unsigned newValue = value * 10 + peekDigit();
            if (newValue > m_numSubpatterns)
                break;
            value = newValue;
            consume();
        }

        parseBackreferenceQuantifier(failures, value);
        return true;
    }
    
    // ControlEscape
    case 'f':
        consume();
        return parsePatternCharacterQualifier(failures, '\f');
    case 'n':
        consume();
        return parsePatternCharacterQualifier(failures, '\n');
    case 'r':
        consume();
        return parsePatternCharacterQualifier(failures, '\r');
    case 't':
        consume();
        return parsePatternCharacterQualifier(failures, '\t');
    case 'v':
        consume();
        return parsePatternCharacterQualifier(failures, '\v');

    // ControlLetter
    case 'c': {
        consume();
        int control = consume();
        if (!isASCIIAlpha(control)) {
            m_err = Error_malformedEscape;
            return false;
        }
        return parsePatternCharacterQualifier(failures, control&31);
    }

    // HexEscape
    case 'x': {
        consume();
        int x = consumeHex(2);
        if (x == -1) {
            m_err = Error_malformedEscape;
            return false;
        }
        return parsePatternCharacterQualifier(failures, x);
    }

    // UnicodeEscape
    case 'u': {
        consume();
        int x = consumeHex(4);
        if (x == -1) {
            m_err = Error_malformedEscape;
            return false;
        }
        return parsePatternCharacterQualifier(failures, x);
    }

    // CharacterClassEscape
    case 'd':
        consume();
        return parseCharacterClassQuantifier(failures, getCharacterClassDigits(), false);
    case 's':
        consume();
        return parseCharacterClassQuantifier(failures, getCharacterClassSpaces(), false);
    case 'w':
        consume();
        return parseCharacterClassQuantifier(failures, getCharacterClassWordchar(), false);
    case 'D':
        consume();
        return parseCharacterClassQuantifier(failures, getCharacterClassDigits(), true);
    case 'S':
        consume();
        return parseCharacterClassQuantifier(failures, getCharacterClassSpaces(), true);
    case 'W':
        consume();
        return parseCharacterClassQuantifier(failures, getCharacterClassWordchar(), true);

    // IdentityEscape
    default: {
        // TODO: check this test for IdentifierPart.
        int ch = consume();
        if (isASCIIAlphanumeric(ch) || (ch == '_')) {
            m_err = Error_malformedEscape;
            return false;
        }
        return parsePatternCharacterQualifier(failures, ch);
    }
    }
}

bool WRECParser::parseTerm(JmpSrcVector& failures)
{
    switch (peek()) {
    case EndOfPattern:
    case '*':
    case '+':
    case '?':
    case ')':
    case ']':
    case '{':
    case '}':
    case '|':
        // Not allowed in a Term!
        return false;
        
    case '^':
        consume();
        m_generator.generateAssertionBOL(failures);
        return true;
    
    case '$':
        consume();
        m_generator.generateAssertionEOL(failures);
        return true;

    case '\\':
        // b & B are also assertions...
        consume();
        return parseEscape(failures);

    case '.':
        consume();
        return parseCharacterClassQuantifier(failures, getCharacterClassNewline(), true);

    case '[':
        // CharacterClass
        consume();
        return parseCharacterClass(failures);

    case '(':
        consume();
        return parseParentheses(failures);

    default:
        // Anything else is a PatternCharacter
        return parsePatternCharacterQualifier(failures, consume());
    }
}

/*
  interface req: CURR_POS is on stack (can be reloaded).
*/
void WRECParser::parseDisjunction(JmpSrcVector& failures)
{
    parseAlternative(failures);

    if (peek() == '|') {
        JmpSrcVector successes;

        do {
            consume();

            m_generator.gernerateDisjunction(successes, failures);

            parseAlternative(failures);
        } while (peek() == '|');

        m_generator.terminateDisjunction(successes);
    }
}

} // namespace JSC

#endif // ENABLE(WREC)
