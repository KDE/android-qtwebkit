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

#ifndef X86Assembler_h
#define X86Assembler_h

#if ENABLE(MASM) && PLATFORM(X86)

#include <wtf/Assertions.h>
#include <wtf/AlwaysInline.h>
#if HAVE(MMAN)
#include <sys/mman.h>
#endif

#include <string.h>

namespace JSC {

class JITCodeBuffer {
public:
    JITCodeBuffer(int size)
        : m_buffer(static_cast<char*>(fastMalloc(size)))
        , m_size(size)
        , m_index(0)
    {
    }

    ~JITCodeBuffer()
    {
        fastFree(m_buffer);
    }

    void ensureSpace(int space)
    {
        if (m_index > m_size - space)
            growBuffer();
    }

    void putByteUnchecked(int value)
    {
        m_buffer[m_index] = value;
        m_index++;
    }

    void putByte(int value)
    {
        if (m_index > m_size - 4)
            growBuffer();
        putByteUnchecked(value);
    }
    
    void putShortUnchecked(int value)
    {
        *(short*)(&m_buffer[m_index]) = value;
        m_index += 2;
    }

    void putShort(int value)
    {
        if (m_index > m_size - 4)
            growBuffer();
        putShortUnchecked(value);
    }
    
    void putIntUnchecked(int value)
    {
        *(int*)(&m_buffer[m_index]) = value;
        m_index += 4;
    }

    void putInt(int value)
    {
        if (m_index > m_size - 4)
            growBuffer();
        putIntUnchecked(value);
    }

    void* getEIP()
    {
        return m_buffer + m_index;
    }
    
    void* start()
    {
        return m_buffer;
    }
    
    int getOffset()
    {
        return m_index;
    }
    
    JITCodeBuffer* reset()
    {
        m_index = 0;
        return this;
    }
    
    void* copy()
    {
        if (!m_index)
            return 0;

        void* result = fastMalloc(m_index);

        if (!result)
            return 0;

        return memcpy(result, m_buffer, m_index);
    }

private:
    void growBuffer()
    {
        m_size += m_size / 2;
        m_buffer = static_cast<char*>(fastRealloc(m_buffer, m_size));
    }

    char* m_buffer;
    int m_size;
    int m_index;
};

#define MODRM(type, reg, rm) ((type << 6) | (reg << 3) | (rm))
#define SIB(type, reg, rm) MODRM(type, reg, rm)
#define CAN_SIGN_EXTEND_8_32(value) (value == ((int)(signed char)value))

class X86Assembler {
public:
    typedef enum {
        eax,
        ecx,
        edx,
        ebx,
        esp,
        ebp,
        esi,
        edi,

        NO_BASE  = ebp,
        HAS_SIB  = esp,
        NO_SCALE = esp,
    } RegisterID;

    typedef enum {
        OP_ADD_EvGv                     = 0x01,
        OP_ADD_GvEv                     = 0x03,
        OP_OR_EvGv                      = 0x09,
        OP_2BYTE_ESCAPE                 = 0x0F,
        OP_AND_EvGv                     = 0x21,
        OP_SUB_EvGv                     = 0x29,
        OP_SUB_GvEv                     = 0x2B,
        PRE_PREDICT_BRANCH_NOT_TAKEN    = 0x2E,
        OP_XOR_EvGv                     = 0x31,
        OP_CMP_EvGv                     = 0x39,
        OP_PUSH_EAX                     = 0x50,
        OP_POP_EAX                      = 0x58,
        PRE_OPERAND_SIZE                = 0x66,
        OP_GROUP1_EvIz                  = 0x81,
        OP_GROUP1_EvIb                  = 0x83,
        OP_TEST_EvGv                    = 0x85,
        OP_MOV_EvGv                     = 0x89,
        OP_MOV_GvEv                     = 0x8B,
        OP_LEA                          = 0x8D,
        OP_GROUP1A_Ev                   = 0x8F,
        OP_CDQ                          = 0x99,
        OP_GROUP2_EvIb                  = 0xC1,
        OP_RET                          = 0xC3,
        OP_GROUP11_EvIz                 = 0xC7,
        OP_INT3                         = 0xCC,
        OP_GROUP2_Ev1                   = 0xD1,
        OP_GROUP2_EvCL                  = 0xD3,
        OP_CALL_rel32                   = 0xE8,
        OP_JMP_rel32                    = 0xE9,
        OP_GROUP3_Ev                    = 0xF7,
        OP_GROUP3_EvIz                  = 0xF7, // OP_GROUP3_Ev has an immediate, when instruction is a test. 
        OP_GROUP5_Ev                    = 0xFF,

        OP2_JO_rel32    = 0x80,
        OP2_JB_rel32    = 0x82,
        OP2_JAE_rel32   = 0x83,
        OP2_JE_rel32    = 0x84,
        OP2_JNE_rel32   = 0x85,
        OP2_JBE_rel32   = 0x86,
        OP2_JL_rel32    = 0x8C,
        OP2_JGE_rel32   = 0x8D,
        OP2_JLE_rel32   = 0x8E,
        OP2_MUL_GvEv    = 0xAF,
        OP2_MOVZX_GvEw  = 0xB7,

        GROUP1_OP_ADD = 0,
        GROUP1_OP_OR  = 1,
        GROUP1_OP_AND = 4,
        GROUP1_OP_SUB = 5,
        GROUP1_OP_XOR = 6,
        GROUP1_OP_CMP = 7,

        GROUP1A_OP_POP = 0,

        GROUP2_OP_SHL = 4,
        GROUP2_OP_SAR = 7,

        GROUP3_OP_TEST = 0,
        GROUP3_OP_IDIV = 7,

        GROUP5_OP_CALLN = 2,
        GROUP5_OP_JMPN  = 4,
        GROUP5_OP_PUSH  = 6,

        GROUP11_MOV = 0,
    } OpcodeID;
    
    static const int MAX_INSTRUCTION_SIZE = 16;

    X86Assembler(JITCodeBuffer* m_buffer)
        : m_buffer(m_buffer)
    {
        m_buffer->reset();
    }

    void emitInt3()
    {
        m_buffer->putByte(OP_INT3);
    }
    
    void emitPushl_r(RegisterID reg)
    {
        m_buffer->putByte(OP_PUSH_EAX + reg);
    }
    
    void emitPushl_m(int offset, RegisterID base)
    {
        m_buffer->putByte(OP_GROUP5_Ev);
        emitModRm_opm(GROUP5_OP_PUSH, base, offset);
    }
    
    void emitPopl_r(RegisterID reg)
    {
        m_buffer->putByte(OP_POP_EAX + reg);
    }

    void emitPopl_m(int offset, RegisterID base)
    {
        m_buffer->putByte(OP_GROUP1A_Ev);
        emitModRm_opm(GROUP1A_OP_POP, base, offset);
    }
    
    void emitMovl_rr(RegisterID src, RegisterID dst)
    {
        m_buffer->putByte(OP_MOV_EvGv);
        emitModRm_rr(src, dst);
    }
    
    void emitAddl_rr(RegisterID src, RegisterID dst)
    {
        m_buffer->putByte(OP_ADD_EvGv);
        emitModRm_rr(src, dst);
    }

    void emitAddl_i8r(int imm, RegisterID dst)
    {
        m_buffer->putByte(OP_GROUP1_EvIb);
        emitModRm_opr(GROUP1_OP_ADD, dst);
        m_buffer->putByte(imm);
    }

    void emitAddl_i32r(int imm, RegisterID dst)
    {
        m_buffer->putByte(OP_GROUP1_EvIz);
        emitModRm_opr(GROUP1_OP_ADD, dst);
        m_buffer->putInt(imm);
    }

    void emitAddl_mr(int offset, RegisterID base, RegisterID dst)
    {
        m_buffer->putByte(OP_ADD_GvEv);
        emitModRm_rm(dst, base, offset);
    }

    void emitAndl_rr(RegisterID src, RegisterID dst)
    {
        m_buffer->putByte(OP_AND_EvGv);
        emitModRm_rr(src, dst);
    }

    void emitAndl_i32r(int imm, RegisterID dst)
    {
        m_buffer->putByte(OP_GROUP1_EvIz);
        emitModRm_opr(GROUP1_OP_AND, dst);
        m_buffer->putInt(imm);
    }

    void emitCmpl_i8r(int imm, RegisterID dst)
    {
        m_buffer->putByte(OP_GROUP1_EvIb);
        emitModRm_opr(GROUP1_OP_CMP, dst);
        m_buffer->putByte(imm);
    }

    void emitCmpl_rr(RegisterID src, RegisterID dst)
    {
        m_buffer->putByte(OP_CMP_EvGv);
        emitModRm_rr(src, dst);
    }

    void emitCmpl_rm(RegisterID src, int offset, RegisterID base)
    {
        m_buffer->putByte(OP_CMP_EvGv);
        emitModRm_rm(src, base, offset);
    }

    void emitCmpl_i32r(int imm, RegisterID dst)
    {
        m_buffer->putByte(OP_GROUP1_EvIz);
        emitModRm_opr(GROUP1_OP_CMP, dst);
        m_buffer->putInt(imm);
    }

    void emitCmpl_i32m(int imm, RegisterID dst)
    {
        m_buffer->putByte(OP_GROUP1_EvIz);
        emitModRm_opm(GROUP1_OP_CMP, dst);
        m_buffer->putInt(imm);
    }

    void emitCmpl_i32m(int imm, int offset, RegisterID dst)
    {
        m_buffer->putByte(OP_GROUP1_EvIz);
        emitModRm_opm(GROUP1_OP_CMP, dst, offset);
        m_buffer->putInt(imm);
    }

    void emitCmpl_i32m(int imm, void* addr)
    {
        m_buffer->putByte(OP_GROUP1_EvIz);
        emitModRm_opm(GROUP1_OP_CMP, addr);
        m_buffer->putInt(imm);
    }

    void emitCmpw_rm(RegisterID src, RegisterID base, RegisterID index, int scale)
    {
        m_buffer->putByte(PRE_OPERAND_SIZE);
        m_buffer->putByte(OP_CMP_EvGv);
        emitModRm_rmsib(src, base, index, scale);
    }

    void emitOrl_rr(RegisterID src, RegisterID dst)
    {
        m_buffer->putByte(OP_OR_EvGv);
        emitModRm_rr(src, dst);
    }

    void emitOrl_i8r(int imm, RegisterID dst)
    {
        m_buffer->putByte(OP_GROUP1_EvIb);
        emitModRm_opr(GROUP1_OP_OR, dst);
        m_buffer->putByte(imm);
    }

    void emitSubl_rr(RegisterID src, RegisterID dst)
    {
        m_buffer->putByte(OP_SUB_EvGv);
        emitModRm_rr(src, dst);
    }

    void emitSubl_i8r(int imm, RegisterID dst)
    {
        m_buffer->putByte(OP_GROUP1_EvIb);
        emitModRm_opr(GROUP1_OP_SUB, dst);
        m_buffer->putByte(imm);
    }

    void emitSubl_i32r(int imm, RegisterID dst)
    {
        m_buffer->putByte(OP_GROUP1_EvIz);
        emitModRm_opr(GROUP1_OP_SUB, dst);
        m_buffer->putInt(imm);
    }

    void emitSubl_mr(int offset, RegisterID base, RegisterID dst)
    {
        m_buffer->putByte(OP_SUB_GvEv);
        emitModRm_rm(dst, base, offset);
    }

    void emitTestl_i32r(int imm, RegisterID dst)
    {
        m_buffer->ensureSpace(MAX_INSTRUCTION_SIZE);
        m_buffer->putByteUnchecked(OP_GROUP3_EvIz);
        emitModRm_opr_Unchecked(GROUP3_OP_TEST, dst);
        m_buffer->putIntUnchecked(imm);
    }

    void emitTestl_rr(RegisterID src, RegisterID dst)
    {
        m_buffer->putByte(OP_TEST_EvGv);
        emitModRm_rr(src, dst);
    }
    
    void emitXorl_i8r(int imm, RegisterID dst)
    {
        m_buffer->putByte(OP_GROUP1_EvIb);
        emitModRm_opr(GROUP1_OP_XOR, dst);
        m_buffer->putByte(imm);
    }

    void emitXorl_rr(RegisterID src, RegisterID dst)
    {
        m_buffer->putByte(OP_XOR_EvGv);
        emitModRm_rr(src, dst);
    }

    void emitSarl_i8r(int imm, RegisterID dst)
    {
        if (imm == 1) {
            m_buffer->putByte(OP_GROUP2_Ev1);
            emitModRm_opr(GROUP2_OP_SAR, dst);
        } else {
            m_buffer->putByte(OP_GROUP2_EvIb);
            emitModRm_opr(GROUP2_OP_SAR, dst);
            m_buffer->putByte(imm);
        }
    }

    void emitSarl_CLr(RegisterID dst)
    {
        m_buffer->putByte(OP_GROUP2_EvCL);
        emitModRm_opr(GROUP2_OP_SAR, dst);
    }

    void emitShl_i8r(int imm, RegisterID dst)
    {
        if (imm == 1) {
            m_buffer->putByte(OP_GROUP2_Ev1);
            emitModRm_opr(GROUP2_OP_SHL, dst);
        } else {
            m_buffer->putByte(OP_GROUP2_EvIb);
            emitModRm_opr(GROUP2_OP_SHL, dst);
            m_buffer->putByte(imm);
        }
    }

    void emitShll_CLr(RegisterID dst)
    {
        m_buffer->putByte(OP_GROUP2_EvCL);
        emitModRm_opr(GROUP2_OP_SHL, dst);
    }

    void emitMull_rr(RegisterID src, RegisterID dst)
    {
        m_buffer->putByte(OP_2BYTE_ESCAPE);
        m_buffer->putByte(OP2_MUL_GvEv);
        emitModRm_rr(dst, src);
    }

    void emitIdivl_r(RegisterID dst)
    {
        m_buffer->putByte(OP_GROUP3_Ev);
        emitModRm_opr(GROUP3_OP_IDIV, dst);
    }

    void emitCdq()
    {
        m_buffer->putByte(OP_CDQ);
    }

    void emitMovl_mr(RegisterID base, RegisterID dst)
    {
        m_buffer->putByte(OP_MOV_GvEv);
        emitModRm_rm(dst, base);
    }

    void emitMovl_mr(int offset, RegisterID base, RegisterID dst)
    {
        m_buffer->ensureSpace(MAX_INSTRUCTION_SIZE);
        m_buffer->putByteUnchecked(OP_MOV_GvEv);
        emitModRm_rm_Unchecked(dst, base, offset);
    }

    void emitMovl_mr(void* addr, RegisterID dst)
    {
        m_buffer->putByte(OP_MOV_GvEv);
        emitModRm_rm(dst, addr);
    }

    void emitMovl_mr(int offset, RegisterID base, RegisterID index, int scale, RegisterID dst)
    {
        m_buffer->putByte(OP_MOV_GvEv);
        emitModRm_rmsib(dst, base, index, scale, offset);
    }

    void emitMovzwl_mr(int offset, RegisterID base, RegisterID dst)
    {
        m_buffer->putByte(OP_2BYTE_ESCAPE);
        m_buffer->putByte(OP2_MOVZX_GvEw);
        emitModRm_rm(dst, base, offset);
    }

    void emitMovzwl_mr(RegisterID base, RegisterID index, int scale, RegisterID dst)
    {
        m_buffer->putByte(OP_2BYTE_ESCAPE);
        m_buffer->putByte(OP2_MOVZX_GvEw);
        emitModRm_rmsib(dst, base, index, scale);
    }

    void emitMovzwl_mr(int offset, RegisterID base, RegisterID index, int scale, RegisterID dst)
    {
        m_buffer->putByte(OP_2BYTE_ESCAPE);
        m_buffer->putByte(OP2_MOVZX_GvEw);
        emitModRm_rmsib(dst, base, index, scale, offset);
    }

    void emitMovl_rm(RegisterID src, RegisterID base)
    {
        m_buffer->putByte(OP_MOV_EvGv);
        emitModRm_rm(src, base);
    }

    void emitMovl_rm(RegisterID src, int offset, RegisterID base)
    {
        m_buffer->ensureSpace(MAX_INSTRUCTION_SIZE);
        m_buffer->putByteUnchecked(OP_MOV_EvGv);
        emitModRm_rm_Unchecked(src, base, offset);
    }
    
    void emitMovl_rm(RegisterID src, int offset, RegisterID base, RegisterID index, int scale)
    {
        m_buffer->putByte(OP_MOV_EvGv);
        emitModRm_rmsib(src, base, index, scale, offset);
    }
    
    void emitMovl_i32r(int imm, RegisterID dst)
    {
        m_buffer->putByte(OP_GROUP11_EvIz);
        emitModRm_opr(GROUP11_MOV, dst);
        m_buffer->putInt(imm);
    }

    void emitMovl_i32m(int imm, int offset, RegisterID base)
    {
        m_buffer->ensureSpace(MAX_INSTRUCTION_SIZE);
        m_buffer->putByteUnchecked(OP_GROUP11_EvIz);
        emitModRm_opm_Unchecked(GROUP11_MOV, base, offset);
        m_buffer->putIntUnchecked(imm);
    }

    void emitMovl_i32m(int imm, void* addr)
    {
        m_buffer->putByte(OP_GROUP11_EvIz);
        emitModRm_opm(GROUP11_MOV, addr);
        m_buffer->putInt(imm);
    }

    void emitLeal_mr(int offset, RegisterID base, RegisterID dst)
    {
        m_buffer->putByte(OP_LEA);
        emitModRm_rm(dst, base, offset);
    }

    void emitRet()
    {
        m_buffer->putByte(OP_RET);
    }
    
    void emitJmpN_r(RegisterID dst)
    {
        m_buffer->putByte(OP_GROUP5_Ev);
        emitModRm_opr(GROUP5_OP_JMPN, dst);
    }
    
    void emitJmpN_m(int offset, RegisterID base)
    {
        m_buffer->putByte(OP_GROUP5_Ev);
        emitModRm_opm(GROUP5_OP_JMPN, base, offset);
    }
    
    void emitCallN_r(RegisterID dst)
    {
        m_buffer->putByte(OP_GROUP5_Ev);
        emitModRm_opr(GROUP5_OP_CALLN, dst);
    }

    // Opaque label types
    
    class JmpSrc {
        friend class X86Assembler;
    public:
        JmpSrc()
            : m_offset(-1)
        {
        }

    private:
        JmpSrc(int offset)
            : m_offset(offset)
        {
        }

        int m_offset;
    };
    
    class JmpDst {
        friend class X86Assembler;
    public:
        JmpDst()
            : m_offset(-1)
        {
        }

    private:
        JmpDst(int offset)
            : m_offset(offset)
        {
        }

        int m_offset;
    };

    // FIXME: make this point to a global label, linked later.
    JmpSrc emitCall()
    {
        m_buffer->putByte(OP_CALL_rel32);
        m_buffer->putInt(0);
        return JmpSrc(m_buffer->getOffset());
    }
    
    JmpDst label()
    {
        return JmpDst(m_buffer->getOffset());
    }
    
    JmpSrc emitUnlinkedJmp()
    {
        m_buffer->putByte(OP_JMP_rel32);
        m_buffer->putInt(0);
        return JmpSrc(m_buffer->getOffset());
    }
    
    JmpSrc emitUnlinkedJne()
    {
        m_buffer->putByte(OP_2BYTE_ESCAPE);
        m_buffer->putByte(OP2_JNE_rel32);
        m_buffer->putInt(0);
        return JmpSrc(m_buffer->getOffset());
    }
    
    JmpSrc emitUnlinkedJe()
    {
        m_buffer->ensureSpace(MAX_INSTRUCTION_SIZE);
        m_buffer->putByteUnchecked(OP_2BYTE_ESCAPE);
        m_buffer->putByteUnchecked(OP2_JE_rel32);
        m_buffer->putIntUnchecked(0);
        return JmpSrc(m_buffer->getOffset());
    }
    
    JmpSrc emitUnlinkedJl()
    {
        m_buffer->putByte(OP_2BYTE_ESCAPE);
        m_buffer->putByte(OP2_JL_rel32);
        m_buffer->putInt(0);
        return JmpSrc(m_buffer->getOffset());
    }
    
    JmpSrc emitUnlinkedJb()
    {
        m_buffer->putByte(OP_2BYTE_ESCAPE);
        m_buffer->putByte(OP2_JB_rel32);
        m_buffer->putInt(0);
        return JmpSrc(m_buffer->getOffset());
    }
    
    JmpSrc emitUnlinkedJle()
    {
        m_buffer->putByte(OP_2BYTE_ESCAPE);
        m_buffer->putByte(OP2_JLE_rel32);
        m_buffer->putInt(0);
        return JmpSrc(m_buffer->getOffset());
    }
    
    JmpSrc emitUnlinkedJbe()
    {
        m_buffer->putByte(OP_2BYTE_ESCAPE);
        m_buffer->putByte(OP2_JBE_rel32);
        m_buffer->putInt(0);
        return JmpSrc(m_buffer->getOffset());
    }
    
    JmpSrc emitUnlinkedJge()
    {
        m_buffer->putByte(OP_2BYTE_ESCAPE);
        m_buffer->putByte(OP2_JGE_rel32);
        m_buffer->putInt(0);
        return JmpSrc(m_buffer->getOffset());
    }
    
    JmpSrc emitUnlinkedJae()
    {
        m_buffer->putByte(OP_2BYTE_ESCAPE);
        m_buffer->putByte(OP2_JAE_rel32);
        m_buffer->putInt(0);
        return JmpSrc(m_buffer->getOffset());
    }
    
    JmpSrc emitUnlinkedJo()
    {
        m_buffer->putByte(OP_2BYTE_ESCAPE);
        m_buffer->putByte(OP2_JO_rel32);
        m_buffer->putInt(0);
        return JmpSrc(m_buffer->getOffset());
    }
    
    void emitPredictionNotTaken()
    {
        m_buffer->putByte(PRE_PREDICT_BRANCH_NOT_TAKEN);
    }
    
    void link(JmpSrc from, JmpDst to)
    {
        ASSERT(to.m_offset != -1);
        ASSERT(from.m_offset != -1);
        
        ((int*)(((ptrdiff_t)(m_buffer->start())) + from.m_offset))[-1] = to.m_offset - from.m_offset;
    }
    
    static void linkAbsoluteAddress(void* code, JmpDst useOffset, JmpDst address)
    {
        ASSERT(useOffset.m_offset != -1);
        ASSERT(address.m_offset != -1);
        
        ((int*)(((ptrdiff_t)code) + useOffset.m_offset))[-1] = ((ptrdiff_t)code) + address.m_offset;
    }
    
    static void link(void* code, JmpSrc from, void* to)
    {
        ASSERT(from.m_offset != -1);
        
        ((int*)((ptrdiff_t)code + from.m_offset))[-1] = (ptrdiff_t)to - ((ptrdiff_t)code + from.m_offset);
    }
    
    void* getRelocatedAddress(void* code, JmpSrc jump)
    {
        return reinterpret_cast<void*>((ptrdiff_t)code + jump.m_offset);
    }
    
    void* getRelocatedAddress(void* code, JmpDst jump)
    {
        return reinterpret_cast<void*>((ptrdiff_t)code + jump.m_offset);
    }
    
    void* copy() 
    {
        return m_buffer->copy();
    }

private:
    void emitModRm_rr(RegisterID reg, RegisterID rm)
    {
        m_buffer->ensureSpace(MAX_INSTRUCTION_SIZE);
        emitModRm_rr_Unchecked(reg, rm);
    }

    void emitModRm_rr_Unchecked(RegisterID reg, RegisterID rm)
    {
        m_buffer->putByteUnchecked(MODRM(3, reg, rm));
    }

    void emitModRm_rm(RegisterID reg, void* addr)
    {
        m_buffer->putByte(MODRM(0, reg, NO_BASE));
        m_buffer->putInt((int)addr);
    }

    void emitModRm_rm(RegisterID reg, RegisterID base)
    {
        if (base == esp) {
            m_buffer->putByte(MODRM(0, reg, HAS_SIB));
            m_buffer->putByte(SIB(0, NO_SCALE, esp));
        } else
            m_buffer->putByte(MODRM(0, reg, base));
    }

    void emitModRm_rm_Unchecked(RegisterID reg, RegisterID base, int offset)
    {
        if (base == esp) {
            if (CAN_SIGN_EXTEND_8_32(offset)) {
                m_buffer->putByteUnchecked(MODRM(1, reg, HAS_SIB));
                m_buffer->putByteUnchecked(SIB(0, NO_SCALE, esp));
                m_buffer->putByteUnchecked(offset);
            } else {
                m_buffer->putByteUnchecked(MODRM(2, reg, HAS_SIB));
                m_buffer->putByteUnchecked(SIB(0, NO_SCALE, esp));
                m_buffer->putIntUnchecked(offset);
            }
        } else {
            if (CAN_SIGN_EXTEND_8_32(offset)) {
                m_buffer->putByteUnchecked(MODRM(1, reg, base));
                m_buffer->putByteUnchecked(offset);
            } else {
                m_buffer->putByteUnchecked(MODRM(2, reg, base));
                m_buffer->putIntUnchecked(offset);
            }
        }
    }

    void emitModRm_rm(RegisterID reg, RegisterID base, int offset)
    {
        m_buffer->ensureSpace(MAX_INSTRUCTION_SIZE);
        emitModRm_rm_Unchecked(reg, base, offset);
    }

    void emitModRm_rmsib(RegisterID reg, RegisterID base, RegisterID index, int scale)
    {
        int shift = 0;
        while (scale >>= 1)
            shift++;
    
        m_buffer->putByte(MODRM(0, reg, HAS_SIB));
        m_buffer->putByte(SIB(shift, index, base));
    }

    void emitModRm_rmsib(RegisterID reg, RegisterID base, RegisterID index, int scale, int offset)
    {
        int shift = 0;
        while (scale >>= 1)
            shift++;
    
        if (CAN_SIGN_EXTEND_8_32(offset)) {
            m_buffer->putByte(MODRM(1, reg, HAS_SIB));
            m_buffer->putByte(SIB(shift, index, base));
            m_buffer->putByte(offset);
        } else {
            m_buffer->putByte(MODRM(2, reg, HAS_SIB));
            m_buffer->putByte(SIB(shift, index, base));
            m_buffer->putInt(offset);
        }
    }

    void emitModRm_opr(OpcodeID opcode, RegisterID rm)
    {
        m_buffer->ensureSpace(MAX_INSTRUCTION_SIZE);
        emitModRm_opr_Unchecked(opcode, rm);
    }

    void emitModRm_opr_Unchecked(OpcodeID opcode, RegisterID rm)
    {
        emitModRm_rr_Unchecked(static_cast<RegisterID>(opcode), rm);
    }

    void emitModRm_opm(OpcodeID opcode, RegisterID base)
    {
        emitModRm_rm(static_cast<RegisterID>(opcode), base);
    }

    void emitModRm_opm_Unchecked(OpcodeID opcode, RegisterID base, int offset)
    {
        emitModRm_rm_Unchecked(static_cast<RegisterID>(opcode), base, offset);
    }

    void emitModRm_opm(OpcodeID opcode, RegisterID base, int offset)
    {
        emitModRm_rm(static_cast<RegisterID>(opcode), base, offset);
    }

    void emitModRm_opm(OpcodeID opcode, void* addr)
    {
        emitModRm_rm(static_cast<RegisterID>(opcode), addr);
    }

    JITCodeBuffer* m_buffer;
};

} // namespace JSC

#endif // ENABLE(MASM) && PLATFORM(X86)

#endif // X86Assembler_h
