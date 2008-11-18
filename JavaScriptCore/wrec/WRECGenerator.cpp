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
#include "Interpreter.h"
#include "WRECFunctors.h"
#include "WRECParser.h"
#include "pcre_internal.h"

#define __ m_assembler.

using namespace WTF;

namespace JSC { namespace WREC {

void Generator::generateBacktrack1()
{
    __ subl_i8r(1, currentPositionRegister);
}

void Generator::generateBacktrackBackreference(unsigned subpatternId)
{
    __ subl_mr((2 * subpatternId + 1) * sizeof(int), outputRegister, currentPositionRegister);
    __ addl_mr((2 * subpatternId) * sizeof(int), outputRegister, currentPositionRegister);
}

void Generator::generateBackreferenceQuantifier(JmpSrcVector& failures, Quantifier::Type quantifierType, unsigned subpatternId, unsigned min, unsigned max)
{
    GenerateBackreferenceFunctor functor(subpatternId);

    __ movl_mr((2 * subpatternId) * sizeof(int), outputRegister, currentValueRegister);
    __ cmpl_rm(currentValueRegister, ((2 * subpatternId) + 1) * sizeof(int), outputRegister);
    JmpSrc skipIfEmpty = __ emitUnlinkedJe();

    ASSERT(quantifierType == Quantifier::Greedy || quantifierType == Quantifier::NonGreedy);
    if (quantifierType == Quantifier::Greedy)
        generateGreedyQuantifier(failures, functor, min, max);
    else
        generateNonGreedyQuantifier(failures, functor, min, max);

    __ link(skipIfEmpty, __ label());
}

void Generator::generateNonGreedyQuantifier(JmpSrcVector& failures, GenerateAtomFunctor& functor, unsigned min, unsigned max)
{
    // comment me better!
    JmpSrcVector newFailures;

    // (0) Setup:
    //     init quantifierCountRegister
    __ pushl_r(quantifierCountRegister);
    __ xorl_rr(quantifierCountRegister, quantifierCountRegister);
    JmpSrc gotoStart = __ emitUnlinkedJmp();

    // (4) Failure case

    JmpDst quantifierFailed = __ label();
    // (4.1) Restore original value of quantifierCountRegister from the stack
    __ popl_r(quantifierCountRegister);
    failures.append(__ emitUnlinkedJmp()); 

    // (3) We just tried an alternative, and it failed - check we can try more.
    
    JmpDst alternativeFailed = __ label();
    // (3.1) remove the value pushed prior to testing the alternative
    __ popl_r(currentPositionRegister);
    // (3.2) if there is a limit, and we have reached it, game over. 
    if (max != Quantifier::noMaxSpecified) {
        __ cmpl_i32r(max, quantifierCountRegister);
        __ link(__ emitUnlinkedJe(), quantifierFailed);
    }

    // (1) Do a check for the atom

    // (1.0) This is where we start, if there is a minimum (then we must read at least one of the atom).
    JmpDst testQuantifiedAtom = __ label();
    if (min)
        __ link(gotoStart, testQuantifiedAtom);
    // (1.1) Do a check for the atom check.
    functor.generateAtom(this, newFailures);
    // (1.2) If we get here, successful match!
    __ addl_i8r(1, quantifierCountRegister);
    // (1.3) We needed to read the atom, and we failed - that's terminally  bad news.
    for (unsigned i = 0; i < newFailures.size(); ++i)
        __ link(newFailures[i], quantifierFailed);
    newFailures.clear();
    // (1.4) If there is a minimum, check we have read enough ...
    if (min) {
        // ... except if min was 1 no need to keep checking!
        if (min != 1) {
            __ cmpl_i32r(min, quantifierCountRegister);
            __ link(__ emitUnlinkedJl(), testQuantifiedAtom);
        }
    } else
        __ link(gotoStart, __ label());
    // if there was no minimum, this is where we start.

    // (2) Plant an alternative check for the remainder of the expression
    
    // (2.1) recursively call to parseAlternative, if it falls through, success!
    __ pushl_r(currentPositionRegister);
    m_parser.parseAlternative(newFailures);
    __ addl_i8r(4, X86::esp);
    __ popl_r(quantifierCountRegister);
    // (2.2) link failure cases to jump back up to alternativeFailed.
    for (unsigned i = 0; i < newFailures.size(); ++i)
        __ link(newFailures[i], alternativeFailed);
    newFailures.clear();
}

void Generator::generateGreedyQuantifier(JmpSrcVector& failures, GenerateAtomFunctor& functor, unsigned min, unsigned max)
{
    if (!max)
        return;

    // comment me better!
    JmpSrcVector newFailures;

    // (0) Setup:
    //     init quantifierCountRegister
    __ pushl_r(quantifierCountRegister);
    __ xorl_rr(quantifierCountRegister, quantifierCountRegister);

    // (1) Greedily read as many of the atom as possible

    JmpDst readMore = __ label();

    // (1.1) Do a character class check.
    functor.generateAtom(this, newFailures);
    // (1.2) If we get here, successful match!
    __ addl_i8r(1, quantifierCountRegister);
    // (1.3) loop, unless we've read max limit.
    if (max != Quantifier::noMaxSpecified) {
        if (max != 1) {
            // if there is a limit, only loop while less than limit, otherwise fall throught to...
            __ cmpl_i32r(max, quantifierCountRegister);
            __ link(__ emitUnlinkedJne(), readMore);
        }
        // ...if there is no min we need jump to the alternative test, if there is we can just fall through to it.
        if (!min)
            newFailures.append(__ emitUnlinkedJmp());
    } else
        __ link(__ emitUnlinkedJmp(), readMore);
    // (1.4) check enough matches to bother trying an alternative...
    if (min) {
        // We will fall through to here if (min && max), after the max check.
        // First, also link a
        JmpDst minimumTest = __ label();
        for (unsigned i = 0; i < newFailures.size(); ++i)
            __ link(newFailures[i], minimumTest);
        newFailures.clear();
        // 
        __ cmpl_i32r(min, quantifierCountRegister);
        newFailures.append(__ emitUnlinkedJae());
    }

    // (4) Failure case

    JmpDst quantifierFailed = __ label();
    // (4.1) Restore original value of quantifierCountRegister from the stack
    __ popl_r(quantifierCountRegister);
    failures.append(__ emitUnlinkedJmp()); 

    // (3) Backtrack

    JmpDst backtrack = __ label();
    // (3.1) this was preserved prior to executing the alternative
    __ popl_r(currentPositionRegister);
    // (3.2) check we can retry with fewer matches - backtracking fails if already at the minimum
    __ cmpl_i32r(min, quantifierCountRegister);
    __ link(__ emitUnlinkedJe(), quantifierFailed);
    // (3.3) roll off one match, and retry.
    functor.backtrack(this);
    __ subl_i8r(1, quantifierCountRegister);

    // (2) Try an alternative.

    // (2.1) point to retry
    JmpDst tryAlternative = __ label();
    for (unsigned i = 0; i < newFailures.size(); ++i)
        __ link(newFailures[i], tryAlternative);
    newFailures.clear();
    // (2.2) recursively call to parseAlternative, if it falls through, success!
    __ pushl_r(currentPositionRegister);
    m_parser.parseAlternative(newFailures);
    __ addl_i8r(4, X86::esp);
    __ popl_r(quantifierCountRegister);
    // (2.3) link failure cases to here.
    for (unsigned i = 0; i < newFailures.size(); ++i)
        __ link(newFailures[i], backtrack);
    newFailures.clear();
}

void Generator::generatePatternCharacter(JmpSrcVector& failures, int ch)
{
    // check there is more input, read a char.
    __ cmpl_rr(lengthRegister, currentPositionRegister);
    failures.append(__ emitUnlinkedJe());
    __ movzwl_mr(inputRegister, currentPositionRegister, 2, currentValueRegister);

    // used for unicode case insensitive
    bool hasUpper = false;
    JmpSrc isUpper;
    
    // if case insensitive match
    if (m_parser.ignoreCase()) {
        UChar lower, upper;
        
        // check for ascii case sensitive characters
        if (isASCIIAlpha(ch)) {
            __ orl_i32r(32, currentValueRegister);
            ch |= 32;
        } else if ((ch > 0x7f) && ((lower = Unicode::toLower(ch)) != (upper = Unicode::toUpper(ch)))) {
            // handle unicode case sentitive characters - branch to success on upper
            __ cmpl_i32r(upper, currentValueRegister);
            isUpper = __ emitUnlinkedJe();
            hasUpper = true;
            ch = lower;
        }
    }
    
    // checks for ch, or lower case version of ch, if insensitive
    __ cmpl_i32r((unsigned short)ch, currentValueRegister);
    failures.append(__ emitUnlinkedJne());

    if (m_parser.ignoreCase() && hasUpper) {
        // for unicode case insensitive matches, branch here if upper matches.
        __ link(isUpper, __ label());
    }
    
    // on success consume the char
    __ addl_i8r(1, currentPositionRegister);
}

void Generator::generateCharacterClassInvertedRange(JmpSrcVector& failures, JmpSrcVector& matchDest, const CharacterRange* ranges, unsigned count, unsigned* matchIndex, const UChar* matches, unsigned matchCount)
{
    do {
        // pick which range we're going to generate
        int which = count >> 1;
        char lo = ranges[which].begin;
        char hi = ranges[which].end;
        
        __ cmpl_i32r((unsigned short)lo, currentValueRegister);

        // check if there are any ranges or matches below lo.  If not, just jl to failure -
        // if there is anything else to check, check that first, if it falls through jmp to failure.
        if ((*matchIndex < matchCount) && (matches[*matchIndex] < lo)) {
            JmpSrc loOrAbove = __ emitUnlinkedJge();
            
            // generate code for all ranges before this one
            if (which)
                generateCharacterClassInvertedRange(failures, matchDest, ranges, which, matchIndex, matches, matchCount);
            
            do {
                __ cmpl_i32r((unsigned short)matches[*matchIndex], currentValueRegister);
                matchDest.append(__ emitUnlinkedJe());
                ++*matchIndex;
            } while ((*matchIndex < matchCount) && (matches[*matchIndex] < lo));
            failures.append(__ emitUnlinkedJmp());

            __ link(loOrAbove, __ label());
        } else if (which) {
            JmpSrc loOrAbove = __ emitUnlinkedJge();

            generateCharacterClassInvertedRange(failures, matchDest, ranges, which, matchIndex, matches, matchCount);
            failures.append(__ emitUnlinkedJmp());

            __ link(loOrAbove, __ label());
        } else
            failures.append(__ emitUnlinkedJl());

        while ((*matchIndex < matchCount) && (matches[*matchIndex] <= hi))
            ++*matchIndex;

        __ cmpl_i32r((unsigned short)hi, currentValueRegister);
        matchDest.append(__ emitUnlinkedJle());
        // fall through to here, the value is above hi.

        // shuffle along & loop around if there are any more matches to handle.
        unsigned next = which + 1;
        ranges += next;
        count -= next;
    } while (count);
}

void Generator::generateCharacterClassInverted(JmpSrcVector& matchDest, const CharacterClass& charClass)
{
    JmpSrc unicodeFail;
    if (charClass.numMatchesUnicode || charClass.numRangesUnicode) {
        __ cmpl_i8r(0x7f, currentValueRegister);
        JmpSrc isAscii = __ emitUnlinkedJle();
    
        if (charClass.numMatchesUnicode) {
            for (unsigned i = 0; i < charClass.numMatchesUnicode; ++i) {
                UChar ch = charClass.matchesUnicode[i];
                __ cmpl_i32r((unsigned short)ch, currentValueRegister);
                matchDest.append(__ emitUnlinkedJe());
            }
        }
        
        if (charClass.numRangesUnicode) {
            for (unsigned i = 0; i < charClass.numRangesUnicode; ++i) {
                UChar lo = charClass.rangesUnicode[i].begin;
                UChar hi = charClass.rangesUnicode[i].end;
                
                __ cmpl_i32r((unsigned short)lo, currentValueRegister);
                JmpSrc below = __ emitUnlinkedJl();
                __ cmpl_i32r((unsigned short)hi, currentValueRegister);
                matchDest.append(__ emitUnlinkedJle());
                __ link(below, __ label());
            }
        }

        unicodeFail = __ emitUnlinkedJmp();
        __ link(isAscii, __ label());
    }

    if (charClass.numRanges) {
        unsigned matchIndex = 0;
        JmpSrcVector failures; 
        generateCharacterClassInvertedRange(failures, matchDest, charClass.ranges, charClass.numRanges, &matchIndex, charClass.matches, charClass.numMatches);
        while (matchIndex < charClass.numMatches) {
            __ cmpl_i32r((unsigned short)charClass.matches[matchIndex], currentValueRegister);
            matchDest.append(__ emitUnlinkedJe());
            ++matchIndex;
        }
        JmpDst noMatch = __ label();
        for (unsigned i = 0; i < failures.size(); ++i)
            __ link(failures[i], noMatch);
        failures.clear();
    } else if (charClass.numMatches) {
        // optimization: gather 'a','A' etc back together, can mask & test once.
        Vector<char> matchesAZaz;

        for (unsigned i = 0; i < charClass.numMatches; ++i) {
            char ch = charClass.matches[i];
            if (m_parser.ignoreCase()) {
                if (isASCIILower(ch)) {
                    matchesAZaz.append(ch);
                    continue;
                }
                if (isASCIIUpper(ch))
                    continue;
            }
            __ cmpl_i32r((unsigned short)ch, currentValueRegister);
            matchDest.append(__ emitUnlinkedJe());
        }

        if (unsigned countAZaz = matchesAZaz.size()) {
            __ orl_i32r(32, currentValueRegister);

            for (unsigned i = 0; i < countAZaz; ++i) {
                char ch = matchesAZaz[i];
                __ cmpl_i32r((unsigned short)ch, currentValueRegister);
                matchDest.append(__ emitUnlinkedJe());
            }
        }
    }

    if (charClass.numMatchesUnicode || charClass.numRangesUnicode)
        __ link(unicodeFail, __ label());
}

void Generator::generateCharacterClass(JmpSrcVector& failures, const CharacterClass& charClass, bool invert)
{
    __ cmpl_rr(lengthRegister, currentPositionRegister);
    failures.append(__ emitUnlinkedJe());
    __ movzwl_mr(inputRegister, currentPositionRegister, 2, currentValueRegister);

    if (invert)
        generateCharacterClassInverted(failures, charClass);
    else {
        JmpSrcVector successes;
        generateCharacterClassInverted(successes, charClass);
        failures.append(__ emitUnlinkedJmp());
        JmpDst here = __ label();
        for (unsigned i = 0; i < successes.size(); ++i)
            __ link(successes[i], here);
    }
    
    __ addl_i8r(1, currentPositionRegister);
}

Generator::JmpSrc Generator::generateParentheses(ParenthesesType type)
{
    if (type == capturing)
        m_parser.recordSubpattern();

    unsigned subpatternId = m_parser.numSubpatterns();

    // push pos, both to preserve for fail + reloaded in parseDisjunction
    __ pushl_r(currentPositionRegister);

    // Plant a disjunction, wrapped to invert behaviour - 
    JmpSrcVector newFailures;
    m_parser.parseDisjunction(newFailures);
    
    if (type == capturing) {
        __ popl_r(currentValueRegister);
        __ movl_rm(currentValueRegister, (2 * subpatternId) * sizeof(int), outputRegister);
        __ movl_rm(currentPositionRegister, (2 * subpatternId + 1) * sizeof(int), outputRegister);
    } else if (type == non_capturing)
        __ addl_i8r(4, X86::esp);
    else
        __ popl_r(currentPositionRegister);

    // This is a little lame - jump to jump if there is a nested disjunction.
    // (suggestion to fix: make parseDisjunction populate a JmpSrcVector of
    // disjunct successes... this is probably not worth the compile cost in
    // the common case to fix).
    JmpSrc successfulMatch = __ emitUnlinkedJmp();

    JmpDst originalFailure = __ label();
    for (unsigned i = 0; i < newFailures.size(); ++i)
        __ link(newFailures[i], originalFailure);
    newFailures.clear();
    // fail: restore currentPositionRegister
    __ popl_r(currentPositionRegister);

    JmpSrc jumpToFail;
    // If this was an inverted assert, fail = success! - just let the failure case drop through,
    // success case goes to failures.  Both paths restore curr pos.
    if (type == inverted_assertion)
        jumpToFail = successfulMatch;
    else {
        // plant a jump so any fail will link off to 'failures',
        jumpToFail = __ emitUnlinkedJmp();
        // link successes to jump here
        __ link(successfulMatch, __ label());
    }
    return jumpToFail;
}

void Generator::generateParenthesesNonGreedy(JmpSrcVector& failures, JmpDst start, JmpSrc success, JmpSrc fail)
{
    __ link(__ emitUnlinkedJmp(), start);
    __ link(success, __ label());

    failures.append(fail);
}

Generator::JmpSrc Generator::generateParenthesesResetTrampoline(JmpSrcVector& newFailures, unsigned subpatternIdBefore, unsigned subpatternIdAfter)
{
    JmpSrc skip = __ emitUnlinkedJmp();

    JmpDst subPatternResetTrampoline = __ label();
    for (unsigned i = 0; i < newFailures.size(); ++i)
        __ link(newFailures[i], subPatternResetTrampoline);
    newFailures.clear();
    for (unsigned i = subpatternIdBefore + 1; i <= subpatternIdAfter; ++i) {
        __ movl_i32m(-1, (2 * i) * sizeof(int), outputRegister);
        __ movl_i32m(-1, (2 * i + 1) * sizeof(int), outputRegister);
    }
    
    JmpSrc newFailJump = __ emitUnlinkedJmp();
    __ link(skip, __ label());
    
    return newFailJump;
}

void Generator::generateAssertionBOL(JmpSrcVector& failures)
{
    if (m_parser.multiline()) {
        JmpSrcVector previousIsNewline;

        // begin of input == success
        __ cmpl_i8r(0, currentPositionRegister);
        previousIsNewline.append(__ emitUnlinkedJe());

        // now check prev char against newline characters.
        __ movzwl_mr(-2, inputRegister, currentPositionRegister, 2, currentValueRegister);
        generateCharacterClassInverted(previousIsNewline, CharacterClass::newline());

        failures.append(__ emitUnlinkedJmp());

        JmpDst success = __ label();
        for (unsigned i = 0; i < previousIsNewline.size(); ++i)
            __ link(previousIsNewline[i], success);
        previousIsNewline.clear();
    } else {
        __ cmpl_i8r(0, currentPositionRegister);
        failures.append(__ emitUnlinkedJne());
    }
}

void Generator::generateAssertionEOL(JmpSrcVector& failures)
{
    if (m_parser.multiline()) {
        JmpSrcVector nextIsNewline;

        // end of input == success
        __ cmpl_rr(lengthRegister, currentPositionRegister);
        nextIsNewline.append(__ emitUnlinkedJe());

        // now check next char against newline characters.
        __ movzwl_mr(inputRegister, currentPositionRegister, 2, currentValueRegister);
        generateCharacterClassInverted(nextIsNewline, CharacterClass::newline());

        failures.append(__ emitUnlinkedJmp());

        JmpDst success = __ label();
        for (unsigned i = 0; i < nextIsNewline.size(); ++i)
            __ link(nextIsNewline[i], success);
        nextIsNewline.clear();
    } else {
        __ cmpl_rr(lengthRegister, currentPositionRegister);
        failures.append(__ emitUnlinkedJne());
    }
}

void Generator::generateAssertionWordBoundary(JmpSrcVector& failures, bool invert)
{
    JmpSrcVector wordBoundary;
    JmpSrcVector notWordBoundary;

    // (1) Check if the previous value was a word char

    // (1.1) check for begin of input
    __ cmpl_i8r(0, currentPositionRegister);
    JmpSrc atBegin = __ emitUnlinkedJe();
    // (1.2) load the last char, and chck if is word character
    __ movzwl_mr(-2, inputRegister, currentPositionRegister, 2, currentValueRegister);
    JmpSrcVector previousIsWord;
    generateCharacterClassInverted(previousIsWord, CharacterClass::wordchar());
    // (1.3) if we get here, previous is not a word char
    __ link(atBegin, __ label());

    // (2) Handle situation where previous was NOT a \w

    // (2.1) check for end of input
    __ cmpl_rr(lengthRegister, currentPositionRegister);
    notWordBoundary.append(__ emitUnlinkedJe());
    // (2.2) load the next char, and chck if is word character
    __ movzwl_mr(inputRegister, currentPositionRegister, 2, currentValueRegister);
    generateCharacterClassInverted(wordBoundary, CharacterClass::wordchar());
    // (2.3) If we get here, neither chars are word chars
    notWordBoundary.append(__ emitUnlinkedJmp());

    // (3) Handle situation where previous was a \w

    // (3.0) link success in first match to here
    JmpDst section3 = __ label();
    for (unsigned i = 0; i < previousIsWord.size(); ++i)
        __ link(previousIsWord[i], section3);
    previousIsWord.clear();
    // (3.1) check for end of input
    __ cmpl_rr(lengthRegister, currentPositionRegister);
    wordBoundary.append(__ emitUnlinkedJe());
    // (3.2) load the next char, and chck if is word character
    __ movzwl_mr(inputRegister, currentPositionRegister, 2, currentValueRegister);
    generateCharacterClassInverted(notWordBoundary, CharacterClass::wordchar());
    // (3.3) If we get here, this is an end of a word, within the input.
    
    // (4) Link everything up
    
    if (invert) {
        // handle the fall through case
        wordBoundary.append(__ emitUnlinkedJmp());
    
        // looking for non word boundaries, so link boundary fails to here.
        JmpDst success = __ label();
        for (unsigned i = 0; i < notWordBoundary.size(); ++i)
            __ link(notWordBoundary[i], success);
        notWordBoundary.clear();
        
        failures.append(wordBoundary.begin(), wordBoundary.size());
    } else {
        // looking for word boundaries, so link successes here.
        JmpDst success = __ label();
        for (unsigned i = 0; i < wordBoundary.size(); ++i)
            __ link(wordBoundary[i], success);
        wordBoundary.clear();
        
        failures.append(notWordBoundary.begin(), notWordBoundary.size());
    }
}

void Generator::generateBackreference(JmpSrcVector& failures, unsigned subpatternId)
{
    __ pushl_r(currentPositionRegister);
    __ pushl_r(quantifierCountRegister);

    // get the start pos of the backref into quantifierCountRegister (multipurpose!)
    __ movl_mr((2 * subpatternId) * sizeof(int), outputRegister, quantifierCountRegister);

    JmpSrc skipIncrement = __ emitUnlinkedJmp();
    JmpDst topOfLoop = __ label();
    __ addl_i8r(1, currentPositionRegister);
    __ addl_i8r(1, quantifierCountRegister);
    __ link(skipIncrement, __ label());

    // check if we're at the end of backref (if we are, success!)
    __ cmpl_rm(quantifierCountRegister, ((2 * subpatternId) + 1) * sizeof(int), outputRegister);
    JmpSrc endOfBackRef = __ emitUnlinkedJe();
    
    __ movzwl_mr(inputRegister, quantifierCountRegister, 2, currentValueRegister);
    
    // check if we've run out of input (this would be a can o'fail)
    __ cmpl_rr(lengthRegister, currentPositionRegister);
    JmpSrc endOfInput = __ emitUnlinkedJe();
    
    __ cmpw_rm(currentValueRegister, inputRegister, currentPositionRegister, 2);
    __ link(__ emitUnlinkedJe(), topOfLoop);
    
    __ link(endOfInput, __ label());
    // Failure
    __ popl_r(quantifierCountRegister);
    __ popl_r(currentPositionRegister);
    failures.append(__ emitUnlinkedJmp());
    
    // Success
    __ link(endOfBackRef, __ label());
    __ popl_r(quantifierCountRegister);
    __ addl_i8r(4, X86::esp);
}

void Generator::generateDisjunction(JmpSrcVector& successes, JmpSrcVector& failures)
{
    successes.append(__ emitUnlinkedJmp());
    
    JmpDst here = __ label();
    
    for (unsigned i = 0; i < failures.size(); ++i)
        __ link(failures[i], here);
    failures.clear();
    
    __ movl_mr(X86::esp, currentPositionRegister);
}

void Generator::terminateDisjunction(JmpSrcVector& successes)
{
    JmpDst here = __ label();
    for (unsigned i = 0; i < successes.size(); ++i)
        __ link(successes[i], here);
    successes.clear();
}

} } // namespace JSC::WREC

#endif // ENABLE(WREC)
