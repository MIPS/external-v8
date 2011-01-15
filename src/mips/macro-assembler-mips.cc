// Copyright 2010 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.



#include "v8.h"

#if defined(V8_TARGET_ARCH_MIPS)

#include "bootstrapper.h"
#include "codegen-inl.h"
#include "debug.h"
#include "runtime.h"

#ifdef _MIPS_ARCH_MIPS32R2
  #define mips32r2 1
#else
  #define mips32r2 0
#endif

namespace v8 {
namespace internal {

MacroAssembler::MacroAssembler(void* buffer, int size)
    : Assembler(buffer, size),
      generating_stub_(false),
      allow_stub_calls_(true),
      code_object_(Heap::undefined_value()) {
}

// Arguments macros
#define COND_TYPED_ARGS Condition cond, Register r1, const Operand& r2
#define COND_ARGS cond, r1, r2

#define REGISTER_TARGET_BODY(Name) \
void MacroAssembler::Name(Register target, \
                          bool ProtectBranchDelaySlot) { \
  Name(Operand(target), ProtectBranchDelaySlot); \
} \
void MacroAssembler::Name(Register target, COND_TYPED_ARGS, \
                          bool ProtectBranchDelaySlot) { \
  Name(Operand(target), COND_ARGS, ProtectBranchDelaySlot); \
}

#define INT_PTR_TARGET_BODY(Name) \
void MacroAssembler::Name(intptr_t target, RelocInfo::Mode rmode, \
                          bool ProtectBranchDelaySlot) { \
  Name(Operand(target, rmode), ProtectBranchDelaySlot); \
} \
void MacroAssembler::Name(intptr_t target, \
                          RelocInfo::Mode rmode, \
                          COND_TYPED_ARGS, \
                          bool ProtectBranchDelaySlot) { \
  Name(Operand(target, rmode), COND_ARGS, ProtectBranchDelaySlot); \
}

#define BYTE_PTR_TARGET_BODY(Name) \
void MacroAssembler::Name(byte* target, RelocInfo::Mode rmode, \
                          bool ProtectBranchDelaySlot) { \
  Name(reinterpret_cast<intptr_t>(target), rmode, ProtectBranchDelaySlot); \
} \
void MacroAssembler::Name(byte* target, \
                          RelocInfo::Mode rmode, \
                          COND_TYPED_ARGS, \
                          bool ProtectBranchDelaySlot) { \
  Name(reinterpret_cast<intptr_t>(target), \
       rmode, \
       COND_ARGS, \
       ProtectBranchDelaySlot); \
}

#define CODE_TARGET_BODY(Name) \
void MacroAssembler::Name(Handle<Code> target, RelocInfo::Mode rmode, \
                          bool ProtectBranchDelaySlot) { \
  Name(reinterpret_cast<intptr_t>(target.location()), \
       rmode, ProtectBranchDelaySlot); \
} \
void MacroAssembler::Name(Handle<Code> target, \
                          RelocInfo::Mode rmode, \
                          COND_TYPED_ARGS, \
                          bool ProtectBranchDelaySlot) { \
  Name(reinterpret_cast<intptr_t>(target.location()), \
       rmode, \
       COND_ARGS, \
       ProtectBranchDelaySlot); \
}

REGISTER_TARGET_BODY(Jump)
REGISTER_TARGET_BODY(Call)
INT_PTR_TARGET_BODY(Jump)
INT_PTR_TARGET_BODY(Call)
BYTE_PTR_TARGET_BODY(Jump)
BYTE_PTR_TARGET_BODY(Call)
CODE_TARGET_BODY(Jump)
CODE_TARGET_BODY(Call)

#undef COND_TYPED_ARGS
#undef COND_ARGS
#undef REGISTER_TARGET_BODY
#undef BYTE_PTR_TARGET_BODY
#undef CODE_TARGET_BODY


void MacroAssembler::Ret(bool ProtectBranchDelaySlot) {
  Jump(Operand(ra), ProtectBranchDelaySlot);
}


void MacroAssembler::Ret(Condition cond, Register r1, const Operand& r2,
    bool ProtectBranchDelaySlot) {
  Jump(Operand(ra), cond, r1, r2, ProtectBranchDelaySlot);
}


void MacroAssembler::LoadRoot(Register destination,
                              Heap::RootListIndex index) {
  lw(destination, MemOperand(s6, index << kPointerSizeLog2));
}

void MacroAssembler::LoadRoot(Register destination,
                              Heap::RootListIndex index,
                              Condition cond,
                              Register src1, const Operand& src2) {
  Branch(2, NegateCondition(cond), src1, src2);
  lw(destination, MemOperand(s6, index << kPointerSizeLog2));
}


void MacroAssembler::StoreRoot(Register source,
                               Heap::RootListIndex index) {
  sw(source, MemOperand(s6, index << kPointerSizeLog2));
}

void MacroAssembler::StoreRoot(Register source,
                               Heap::RootListIndex index,
                               Condition cond,
                               Register src1, const Operand& src2) {
  Branch(2, NegateCondition(cond), src1, src2);
  sw(source, MemOperand(s6, index << kPointerSizeLog2));
}


void MacroAssembler::RecordWriteHelper(Register object,
                                       Register address,
                                       Register scratch) {
  if (FLAG_debug_code) {
    // Check that the object is not in new space.
    Label not_in_new_space;
    InNewSpace(object, scratch, ne, &not_in_new_space);
    Abort("new-space object passed to RecordWriteHelper");
    bind(&not_in_new_space);
  }

  // Calculate page address: Clear bits from 0 to kPageSizeBits.
  if (mips32r2) {
    Ins(object, zero_reg, 0, kPageSizeBits);
  } else {
    // The Ins macro is slow on r1, so use shifts instead.
    srl(object, object, kPageSizeBits);
    sll(object, object, kPageSizeBits);
  }

  // Calculate region number.
  Ext(address, address, Page::kRegionSizeLog2,
      kPageSizeBits - Page::kRegionSizeLog2);

  // Mark region dirty.
  lw(scratch, MemOperand(object, Page::kDirtyFlagOffset));
  li(at, Operand(1));
  sllv(at, at, address);
  or_(scratch, scratch, at);
  sw(scratch, MemOperand(object, Page::kDirtyFlagOffset));
}


void MacroAssembler::InNewSpace(Register object,
                                Register scratch,
                                Condition cc,
                                Label* branch) {
  ASSERT(cc == eq || cc == ne);
  And(scratch, object, Operand(ExternalReference::new_space_mask()));
  Branch(branch, cc, scratch, Operand(ExternalReference::new_space_start()));
}


// Will clobber 4 registers: object, scratch0, scratch1, at. The
// register 'object' contains a heap object pointer.  The heap object
// tag is shifted away.
void MacroAssembler::RecordWrite(Register object,
                                 Operand offset,
                                 Register scratch0,
                                 Register scratch1) {
  // The compiled code assumes that record write doesn't change the
  // context register, so we check that none of the clobbered
  // registers are cp.
  ASSERT(!object.is(cp) && !scratch0.is(cp) && !scratch1.is(cp));

  Label done;

  // First, test that the object is not in the new space.  We cannot set
  // region marks for new space pages.
  InNewSpace(object, scratch0, eq, &done);

  // Add offset into the object.
  Addu(scratch0, object, offset);

  // Record the actual write.
  RecordWriteHelper(object, scratch0, scratch1);

  bind(&done);

  // Clobber all input registers when running with the debug-code flag
  // turned on to provoke errors.
  if (FLAG_debug_code) {
    li(object, Operand(BitCast<int32_t>(kZapValue)));
    li(scratch0, Operand(BitCast<int32_t>(kZapValue)));
    li(scratch1, Operand(BitCast<int32_t>(kZapValue)));
  }
}


// Will clobber 4 registers: object, address, scratch, ip.  The
// register 'object' contains a heap object pointer.  The heap object
// tag is shifted away.
void MacroAssembler::RecordWrite(Register object,
                                 Register address,
                                 Register scratch) {
  // The compiled code assumes that record write doesn't change the
  // context register, so we check that none of the clobbered
  // registers are cp.
  ASSERT(!object.is(cp) && !address.is(cp) && !scratch.is(cp));

  Label done;

  // First, test that the object is not in the new space.  We cannot set
  // region marks for new space pages.
  InNewSpace(object, scratch, eq, &done);

  // Record the actual write.
  RecordWriteHelper(object, address, scratch);

  bind(&done);

  // Clobber all input registers when running with the debug-code flag
  // turned on to provoke errors.
  if (FLAG_debug_code) {
    li(object, Operand(BitCast<int32_t>(kZapValue)));
    li(address, Operand(BitCast<int32_t>(kZapValue)));
    li(scratch, Operand(BitCast<int32_t>(kZapValue)));
  }
}


// -----------------------------------------------------------------------------
// Allocation support


void MacroAssembler::CheckAccessGlobalProxy(Register holder_reg,
                                            Register scratch,
                                            Label* miss) {
  Label same_contexts;

  ASSERT(!holder_reg.is(scratch));
  ASSERT(!holder_reg.is(at));
  ASSERT(!scratch.is(at));

  // Load current lexical context from the stack frame.
  lw(scratch, MemOperand(fp, StandardFrameConstants::kContextOffset));
  // In debug mode, make sure the lexical context is set.
#ifdef DEBUG
  Check(ne, "we should not have an empty lexical context",
      scratch, Operand(zero_reg));
#endif

  // Load the global context of the current context.
  int offset = Context::kHeaderSize + Context::GLOBAL_INDEX * kPointerSize;
  lw(scratch, FieldMemOperand(scratch, offset));
  lw(scratch, FieldMemOperand(scratch, GlobalObject::kGlobalContextOffset));

  // Check the context is a global context.
  if (FLAG_debug_code) {
    // TODO(119): Avoid push(holder_reg)/pop(holder_reg).
    Push(holder_reg);  // Temporarily save holder on the stack.
    // Read the first word and compare to the global_context_map.
    lw(holder_reg, FieldMemOperand(scratch, HeapObject::kMapOffset));
    LoadRoot(at, Heap::kGlobalContextMapRootIndex);
    Check(eq, "JSGlobalObject::global_context should be a global context.",
          holder_reg, Operand(at));
    Pop(holder_reg);  // Restore holder.
  }

  // Check if both contexts are the same.
  lw(at, FieldMemOperand(holder_reg, JSGlobalProxy::kContextOffset));
  Branch(&same_contexts, eq, scratch, Operand(at));

  // Check the context is a global context.
  if (FLAG_debug_code) {
    // TODO(119): Avoid push(holder_reg)/pop(holder_reg).
    Push(holder_reg);  // Temporarily save holder on the stack.
    mov(holder_reg, at);  // Move at to its holding place.
    LoadRoot(at, Heap::kNullValueRootIndex);
    Check(ne, "JSGlobalProxy::context() should not be null.",
          holder_reg, Operand(at));

    lw(holder_reg, FieldMemOperand(holder_reg, HeapObject::kMapOffset));
    LoadRoot(at, Heap::kGlobalContextMapRootIndex);
    Check(eq, "JSGlobalObject::global_context should be a global context.",
          holder_reg, Operand(at));
    // Restore at is not needed. at is reloaded below.
    Pop(holder_reg);  // Restore holder.
    // Restore at to holder's context.
    lw(at, FieldMemOperand(holder_reg, JSGlobalProxy::kContextOffset));
  }

  // Check that the security token in the calling global object is
  // compatible with the security token in the receiving global
  // object.
  int token_offset = Context::kHeaderSize +
                     Context::SECURITY_TOKEN_INDEX * kPointerSize;

  lw(scratch, FieldMemOperand(scratch, token_offset));
  lw(at, FieldMemOperand(at, token_offset));
  Branch(miss, ne, scratch, Operand(at));

  bind(&same_contexts);
}


// ---------------------------------------------------------------------------
// Instruction macros

void MacroAssembler::Addu(Register rd, Register rs, const Operand& rt) {
  if (rt.is_reg()) {
    addu(rd, rs, rt.rm());
  } else {
    if (is_int16(rt.imm32_) && !MustUseReg(rt.rmode_)) {
      addiu(rd, rs, rt.imm32_);
    } else {
      // li handles the relocation.
      ASSERT(!rs.is(at));
      li(at, rt);
      addu(rd, rs, at);
    }
  }
}


void MacroAssembler::Subu(Register rd, Register rs, const Operand& rt) {
  if (rt.is_reg()) {
    subu(rd, rs, rt.rm());
  } else {
    if (is_int16(rt.imm32_) && !MustUseReg(rt.rmode_)) {
      addiu(rd, rs, -rt.imm32_);  // No subiu instr, use addiu(x, y, -imm).
    } else {
      // li handles the relocation.
      ASSERT(!rs.is(at));
      li(at, rt);
      subu(rd, rs, at);
    }
  }
}


void MacroAssembler::Mul(Register rd, Register rs, const Operand& rt) {
  if (rt.is_reg()) {
    mul(rd, rs, rt.rm());
  } else {
    // li handles the relocation.
    ASSERT(!rs.is(at));
    li(at, rt);
    mul(rd, rs, at);
  }
}


void MacroAssembler::Mult(Register rs, const Operand& rt) {
  if (rt.is_reg()) {
    mult(rs, rt.rm());
  } else {
    // li handles the relocation.
    ASSERT(!rs.is(at));
    li(at, rt);
    mult(rs, at);
  }
}


void MacroAssembler::Multu(Register rs, const Operand& rt) {
  if (rt.is_reg()) {
    multu(rs, rt.rm());
  } else {
    // li handles the relocation.
    ASSERT(!rs.is(at));
    li(at, rt);
    multu(rs, at);
  }
}


void MacroAssembler::Div(Register rs, const Operand& rt) {
  if (rt.is_reg()) {
    div(rs, rt.rm());
  } else {
    // li handles the relocation.
    ASSERT(!rs.is(at));
    li(at, rt);
    div(rs, at);
  }
}


void MacroAssembler::Divu(Register rs, const Operand& rt) {
  if (rt.is_reg()) {
    divu(rs, rt.rm());
  } else {
    // li handles the relocation.
    ASSERT(!rs.is(at));
    li(at, rt);
    divu(rs, at);
  }
}


void MacroAssembler::And(Register rd, Register rs, const Operand& rt) {
  if (rt.is_reg()) {
    and_(rd, rs, rt.rm());
  } else {
    if (is_uint16(rt.imm32_) && !MustUseReg(rt.rmode_)) {
      andi(rd, rs, rt.imm32_);
    } else {
      // li handles the relocation.
      ASSERT(!rs.is(at));
      li(at, rt);
      and_(rd, rs, at);
    }
  }
}


void MacroAssembler::Or(Register rd, Register rs, const Operand& rt) {
  if (rt.is_reg()) {
    or_(rd, rs, rt.rm());
  } else {
    if (is_uint16(rt.imm32_) && !MustUseReg(rt.rmode_)) {
      ori(rd, rs, rt.imm32_);
    } else {
      // li handles the relocation.
      ASSERT(!rs.is(at));
      li(at, rt);
      or_(rd, rs, at);
    }
  }
}


void MacroAssembler::Xor(Register rd, Register rs, const Operand& rt) {
  if (rt.is_reg()) {
    xor_(rd, rs, rt.rm());
  } else {
    if (is_uint16(rt.imm32_) && !MustUseReg(rt.rmode_)) {
      xori(rd, rs, rt.imm32_);
    } else {
      // li handles the relocation.
      ASSERT(!rs.is(at));
      li(at, rt);
      xor_(rd, rs, at);
    }
  }
}


void MacroAssembler::Nor(Register rd, Register rs, const Operand& rt) {
  if (rt.is_reg()) {
    nor(rd, rs, rt.rm());
  } else {
    // li handles the relocation.
    ASSERT(!rs.is(at));
    li(at, rt);
    nor(rd, rs, at);
  }
}


void MacroAssembler::Slt(Register rd, Register rs, const Operand& rt) {
  if (rt.is_reg()) {
    slt(rd, rs, rt.rm());
  } else {
    if (is_int16(rt.imm32_) && !MustUseReg(rt.rmode_)) {
      slti(rd, rs, rt.imm32_);
    } else {
      // li handles the relocation.
      ASSERT(!rs.is(at));
      li(at, rt);
      slt(rd, rs, at);
    }
  }
}


void MacroAssembler::Sltu(Register rd, Register rs, const Operand& rt) {
  if (rt.is_reg()) {
    sltu(rd, rs, rt.rm());
  } else {
    if (is_uint16(rt.imm32_) && !MustUseReg(rt.rmode_)) {
      sltiu(rd, rs, rt.imm32_);
    } else {
      // li handles the relocation.
      ASSERT(!rs.is(at));
      li(at, rt);
      sltu(rd, rs, at);
    }
  }
}


void MacroAssembler::Ror(Register rd, Register rs, const Operand& rt) {
  if (mips32r2) {
    if (rt.is_reg()) {
      rotrv(rd, rs, rt.rm());
    } else {
      rotr(rd, rs, rt.imm32_);
    }
  } else {
    if (rt.is_reg()) {
      subu(at, zero_reg, rt.rm());
      sllv(at, rs, at);
      srlv(rd, rs, rt.rm());
      or_(rd, rd, at);
    } else {
      if (rt.imm32_ == 0) {
        srl(rd, rs, 0);
      } else {
        srl(at, rs, rt.imm32_);
        sll(rd, rs, (0x20 - rt.imm32_) & 0x1f);
        or_(rd, rd, at);
      }
    }
  }
}


//------------Pseudo-instructions-------------

void MacroAssembler::li(Register rd, Operand j, bool gen2instr) {
  ASSERT(!j.is_reg());
  BlockTrampolinePoolScope block_trampoline_pool(this);
  if (!MustUseReg(j.rmode_) && !gen2instr) {
    // Normal load of an immediate value which does not need Relocation Info.
    if (is_int16(j.imm32_)) {
      addiu(rd, zero_reg, j.imm32_);
    } else if (!(j.imm32_ & HIMask)) {
      ori(rd, zero_reg, j.imm32_);
    } else if (!(j.imm32_ & LOMask)) {
      lui(rd, (HIMask & j.imm32_) >> 16);
    } else {
      lui(rd, (HIMask & j.imm32_) >> 16);
      ori(rd, rd, (LOMask & j.imm32_));
    }
  } else if (MustUseReg(j.rmode_) || gen2instr) {
    if (MustUseReg(j.rmode_)) {
      RecordRelocInfo(j.rmode_, j.imm32_);
    }
    // We need always the same number of instructions as we may need to patch
    // this code to load another value which may need 2 instructions to load.
    if (is_int16(j.imm32_)) {
      nop();
      addiu(rd, zero_reg, j.imm32_);
    } else if (!(j.imm32_ & HIMask)) {
      nop();
      ori(rd, zero_reg, j.imm32_);
    } else if (!(j.imm32_ & LOMask)) {
      nop();
      lui(rd, (HIMask & j.imm32_) >> 16);
    } else {
      lui(rd, (HIMask & j.imm32_) >> 16);
      ori(rd, rd, (LOMask & j.imm32_));
    }
  }
}


// Exception-generating instructions and debugging support
void MacroAssembler::stop(const char* msg) {
  // TO_UPGRADE: Just a break for now. Maybe we could upgrade it.
  // We use the 0x54321 value to be able to find it easily when reading memory.
  break_(0x54321);
}


void MacroAssembler::MultiPush(RegList regs) {
  int16_t NumSaved = 0;
  int16_t NumToPush = NumberOfBitsSet(regs);

  addiu(sp, sp, -4 * NumToPush);
  for (int16_t i = kNumRegisters; i > 0; i--) {
    if ((regs & (1 << i)) != 0) {
      sw(ToRegister(i), MemOperand(sp, 4 * (NumToPush - ++NumSaved)));
    }
  }
}


void MacroAssembler::MultiPushReversed(RegList regs) {
  int16_t NumSaved = 0;
  int16_t NumToPush = NumberOfBitsSet(regs);

  addiu(sp, sp, -4 * NumToPush);
  for (int16_t i = 0; i < kNumRegisters; i++) {
    if ((regs & (1 << i)) != 0) {
      sw(ToRegister(i), MemOperand(sp, 4 * (NumToPush - ++NumSaved)));
    }
  }
}


void MacroAssembler::MultiPop(RegList regs) {
  int16_t NumSaved = 0;

  for (int16_t i = 0; i < kNumRegisters; i++) {
    if ((regs & (1 << i)) != 0) {
      lw(ToRegister(i), MemOperand(sp, 4 * (NumSaved++)));
    }
  }
  addiu(sp, sp, 4 * NumSaved);
}


void MacroAssembler::MultiPopReversed(RegList regs) {
  int16_t NumSaved = 0;

  for (int16_t i = kNumRegisters; i > 0; i--) {
    if ((regs & (1 << i)) != 0) {
      lw(ToRegister(i), MemOperand(sp, 4 * (NumSaved++)));
    }
  }
  addiu(sp, sp, 4 * NumSaved);
}


void MacroAssembler::Ext(Register rt,
                         Register rs,
                         uint16_t pos,
                         uint16_t size) {
  if (mips32r2) {
    ext_(rt, rs, pos, size);
  } else {
    // Move rs to rt and shift it left then right to get the
    // desired bitfield on the right side and zeroes on the left.
    sll(rt, rs, 32 - (pos + size));
    srl(rt, rt, 32 - size);
  }
}


void MacroAssembler::Ins(Register rt,
                         Register rs,
                         uint16_t pos,
                         uint16_t size) {
  if (mips32r2) {
    ins_(rt, rs, pos, size);
  } else {
    ASSERT(!rt.is(t8) && !rs.is(t8));

    srl(t8, rt, pos + size);
    // The left chunk from rt that needs to
    // be saved is on the right side of t8.
    sll(at, t8, pos + size);
    // The 'at' register now contains the left chunk on
    // the left (proper position) and zeroes.
    sll(t8, rt, 32 - pos);
    // t8 now contains the right chunk on the left and zeroes.
    srl(t8, t8, 32 - pos);
    // t8 now contains the right chunk on
    // the right (proper position) and zeroes.
    or_(rt, at, t8);
    // rt now contains the left and right chunks from the original rt
    // in their proper position and zeroes in the middle.
    sll(t8, rs, 32 - size);
    // t8 now contains the chunk from rs on the left and zeroes.
    srl(t8, t8, 32 - size - pos);
    // t8 now contains the original chunk from rs in
    // the middle (proper position).
    or_(rt, rt, t8);
    // rt now contains the result of the ins instruction in R2 mode.
  }
}


void MacroAssembler::Cvt_d_uw(FPURegister fd, FPURegister fs) {
  // Move the data from fs to t4.
  mfc1(t4, fs);
  return Cvt_d_uw(fd, t4);
}


void MacroAssembler::Cvt_d_uw(FPURegister fd, Register rs) {
  // Convert rs to a FP value in fd (and fd + 1).
  // We do this by converting rs minus the MSB to avoid sign conversion,
  // then adding 2^31-1 and 1 to the result.

  ASSERT(!fd.is(f20));
  ASSERT(!rs.is(t9));
  ASSERT(!rs.is(t8));

  // Save rs's MSB to t8
  And(t8, rs, 0x80000000);
  // Remove rs's MSB.
  And(t9, rs, 0x7FFFFFFF);
  // Move t9 to fd
  mtc1(t9, fd);

  // Convert fd to a real FP value.
  cvt_d_w(fd, fd);

  Label conversion_done;

  // If rs's MSB was 0, it's done.
  // Otherwise we need to add that to the FP register.
  Branch(&conversion_done, eq, t8, Operand(zero_reg));

  // First load 2^31 - 1 into f20.
  Or(t9, zero_reg, 0x7FFFFFFF);
  mtc1(t9, f20);

  // Convert it to FP and add it to fd.
  cvt_d_w(f20, f20);
  add_d(fd, fd, f20);
  // Now add 1.
  Or(t9, zero_reg, 1);
  mtc1(t9, f20);

  cvt_d_w(f20, f20);
  add_d(fd, fd, f20);
  bind(&conversion_done);
}


void MacroAssembler::Trunc_uw_d(FPURegister fd, FPURegister fs) {
  Trunc_uw_d(fs, t4);
  mtc1(t4, fd);
}


void MacroAssembler::Trunc_uw_d(FPURegister fd, Register rs) {
  ASSERT(!fd.is(f22));
  ASSERT(!rs.is(t6));

  // Load 2^31 into f22.
  Or(t6, zero_reg, 0x80000000);
  Cvt_d_uw(f22, t6);

  // Test if f22 > fd.
  c(OLT, D, fd, f22);

  Label simple_convert;
  // If fd < 2^31 we can convert it normally.
  bc1t(&simple_convert);

  // First we subtract 2^31 from fd, then trunc it to rs
  // and add 2^31 to rs.

  sub_d(f22, fd, f22);
  trunc_w_d(f22, f22);
  mfc1(rs, f22);
  or_(rs, rs, t6);

  Label done;
  Branch(&done);
  // Simple conversion.
  bind(&simple_convert);
  trunc_w_d(f22, fd);
  mfc1(rs, f22);

  bind(&done);
}


// Emulated condtional branches do not emit a nop in the branch delay slot.
//
// BRANCH_ARGS_CHECK checks that conditional jump arguments are correct.
#define BRANCH_ARGS_CHECK(cond, rs, rt) ASSERT(                                \
    (cond == cc_always && rs.is(zero_reg) && rt.rm().is(zero_reg)) ||          \
    (cond != cc_always && (!rs.is(zero_reg) || !rt.rm().is(zero_reg))))

void MacroAssembler::Branch(int16_t offset,
                            bool ProtectBranchDelaySlot) {
  b(offset);

  // Emit a nop in the branch delay slot if required.
  if (ProtectBranchDelaySlot)
    nop();
}

void MacroAssembler::Branch(int16_t offset, Condition cond, Register rs,
                            const Operand& rt,
                            bool ProtectBranchDelaySlot) {
  BRANCH_ARGS_CHECK(cond, rs, rt);
  ASSERT(!rs.is(zero_reg));
  Register r2 = no_reg;
  Register scratch = at;

  if (rt.is_reg()) {
    // We don't want any other register but scratch clobbered.
    ASSERT(!scratch.is(rs) && !scratch.is(rt.rm_));
    r2 = rt.rm_;
    switch (cond) {
      case cc_always:
        b(offset);
        break;
      case eq:
        beq(rs, r2, offset);
        break;
      case ne:
        bne(rs, r2, offset);
        break;
      // Signed comparison
      case greater:
        if (r2.is(zero_reg)) {
          bgtz(rs, offset);
        } else {
          slt(scratch, r2, rs);
          bne(scratch, zero_reg, offset);
        }
        break;
      case greater_equal:
        if (r2.is(zero_reg)) {
          bgez(rs, offset);
        } else {
          slt(scratch, rs, r2);
          beq(scratch, zero_reg, offset);
        }
        break;
      case less:
        if (r2.is(zero_reg)) {
          bltz(rs, offset);
        } else {
          slt(scratch, rs, r2);
          bne(scratch, zero_reg, offset);
        }
        break;
      case less_equal:
        if (r2.is(zero_reg)) {
          blez(rs, offset);
        } else {
          slt(scratch, r2, rs);
          beq(scratch, zero_reg, offset);
        }
        break;
      // Unsigned comparison.
      case Ugreater:
        if (r2.is(zero_reg)) {
          bgtz(rs, offset);
        } else {
          sltu(scratch, r2, rs);
          bne(scratch, zero_reg, offset);
        }
        break;
      case Ugreater_equal:
        if (r2.is(zero_reg)) {
          bgez(rs, offset);
        } else {
          sltu(scratch, rs, r2);
          beq(scratch, zero_reg, offset);
        }
        break;
      case Uless:
        if (r2.is(zero_reg)) {
          b(offset);
        } else {
          sltu(scratch, rs, r2);
          bne(scratch, zero_reg, offset);
        }
        break;
      case Uless_equal:
        if (r2.is(zero_reg)) {
          b(offset);
        } else {
          sltu(scratch, r2, rs);
          beq(scratch, zero_reg, offset);
        }
        break;
      default:
        UNREACHABLE();
    }
  } else {
    // Be careful to always use shifted_branch_offset only just before the
    // branch instruction, as the location will be remember for patching the
    // target.
    switch (cond) {
      case cc_always:
        b(offset);
        break;
      case eq:
        // We don't want any other register but scratch clobbered.
        ASSERT(!scratch.is(rs));
        r2 = scratch;
        li(r2, rt);
        beq(rs, r2, offset);
        break;
      case ne:
        // We don't want any other register but scratch clobbered.
        ASSERT(!scratch.is(rs));
        r2 = scratch;
        li(r2, rt);
        bne(rs, r2, offset);
        break;
      // Signed comparison
      case greater:
        if (rt.imm32_ == 0) {
          bgtz(rs, offset);
        } else {
          r2 = scratch;
          li(r2, rt);
          slt(scratch, r2, rs);
          bne(scratch, zero_reg, offset);
        }
        break;
      case greater_equal:
        if (rt.imm32_ == 0) {
          bgez(rs, offset);
        } else if (is_int16(rt.imm32_)) {
          slti(scratch, rs, rt.imm32_);
          beq(scratch, zero_reg, offset);
        } else {
          r2 = scratch;
          li(r2, rt);
          sltu(scratch, rs, r2);
          beq(scratch, zero_reg, offset);
        }
        break;
      case less:
        if (rt.imm32_ == 0) {
          bltz(rs, offset);
        } else if (is_int16(rt.imm32_)) {
          slti(scratch, rs, rt.imm32_);
          bne(scratch, zero_reg, offset);
        } else {
          r2 = scratch;
          li(r2, rt);
          slt(scratch, rs, r2);
          bne(scratch, zero_reg, offset);
        }
        break;
      case less_equal:
        if (rt.imm32_ == 0) {
          blez(rs, offset);
        } else {
          r2 = scratch;
          li(r2, rt);
          slt(scratch, r2, rs);
          beq(scratch, zero_reg, offset);
       }
       break;
      // Unsigned comparison.
      case Ugreater:
        if (rt.imm32_ == 0) {
          bgtz(rs, offset);
        } else {
          r2 = scratch;
          li(r2, rt);
          sltu(scratch, r2, rs);
          bne(scratch, zero_reg, offset);
        }
        break;
      case Ugreater_equal:
        if (rt.imm32_ == 0) {
          bgez(rs, offset);
        } else if (is_int16(rt.imm32_)) {
          sltiu(scratch, rs, rt.imm32_);
          beq(scratch, zero_reg, offset);
        } else {
          r2 = scratch;
          li(r2, rt);
          sltu(scratch, rs, r2);
          beq(scratch, zero_reg, offset);
        }
        break;
      case Uless:
        if (rt.imm32_ == 0) {
          b(offset);
        } else if (is_int16(rt.imm32_)) {
          sltiu(scratch, rs, rt.imm32_);
          bne(scratch, zero_reg, offset);
        } else {
          r2 = scratch;
          li(r2, rt);
          sltu(scratch, rs, r2);
          bne(scratch, zero_reg, offset);
        }
        break;
      case Uless_equal:
        if (rt.imm32_ == 0) {
          b(offset);
        } else {
          r2 = scratch;
          li(r2, rt);
          sltu(scratch, r2, rs);
          beq(scratch, zero_reg, offset);
        }
        break;
      default:
        UNREACHABLE();
    }
  }
  // Emit a nop in the branch delay slot if required.
  if (ProtectBranchDelaySlot)
    nop();
}


void MacroAssembler::Branch(Label* L,
                            bool ProtectBranchDelaySlot) {
  // We use branch_offset as an argument for the branch instructions to be sure
  // it is called just before generating the branch instruction, as needed.

  b(shifted_branch_offset(L, false));

  // Emit a nop in the branch delay slot if required.
  if (ProtectBranchDelaySlot)
    nop();
}


void MacroAssembler::Branch(Label* L, Condition cond, Register rs,
                            const Operand& rt,
                            bool ProtectBranchDelaySlot) {
  BRANCH_ARGS_CHECK(cond, rs, rt);

  int32_t offset;
  Register r2 = no_reg;
  Register scratch = at;
  if (rt.is_reg()) {
    r2 = rt.rm_;
    // Be careful to always use shifted_branch_offset only just before the
    // branch instruction, as the location will be remember for patching the
    // target.
    switch (cond) {
      case cc_always:
        offset = shifted_branch_offset(L, false);
        b(offset);
        break;
      case eq:
        offset = shifted_branch_offset(L, false);
        beq(rs, r2, offset);
        break;
      case ne:
        offset = shifted_branch_offset(L, false);
        bne(rs, r2, offset);
        break;
      // Signed comparison
      case greater:
        if (r2.is(zero_reg)) {
          offset = shifted_branch_offset(L, false);
          bgtz(rs, offset);
        } else {
          slt(scratch, r2, rs);
          offset = shifted_branch_offset(L, false);
          bne(scratch, zero_reg, offset);
        }
        break;
      case greater_equal:
        if (r2.is(zero_reg)) {
          offset = shifted_branch_offset(L, false);
          bgez(rs, offset);
        } else {
          slt(scratch, rs, r2);
          offset = shifted_branch_offset(L, false);
          beq(scratch, zero_reg, offset);
        }
        break;
      case less:
        if (r2.is(zero_reg)) {
          offset = shifted_branch_offset(L, false);
          bltz(rs, offset);
        } else {
          slt(scratch, rs, r2);
          offset = shifted_branch_offset(L, false);
          bne(scratch, zero_reg, offset);
        }
        break;
      case less_equal:
        if (r2.is(zero_reg)) {
          offset = shifted_branch_offset(L, false);
          blez(rs, offset);
        } else {
          slt(scratch, r2, rs);
          offset = shifted_branch_offset(L, false);
          beq(scratch, zero_reg, offset);
        }
        break;
      // Unsigned comparison.
      case Ugreater:
        if (r2.is(zero_reg)) {
          offset = shifted_branch_offset(L, false);
           bgtz(rs, offset);
        } else {
          sltu(scratch, r2, rs);
          offset = shifted_branch_offset(L, false);
          bne(scratch, zero_reg, offset);
        }
        break;
      case Ugreater_equal:
        if (r2.is(zero_reg)) {
          offset = shifted_branch_offset(L, false);
          bgez(rs, offset);
        } else {
          sltu(scratch, rs, r2);
          offset = shifted_branch_offset(L, false);
          beq(scratch, zero_reg, offset);
        }
        break;
      case Uless:
        if (r2.is(zero_reg)) {
          offset = shifted_branch_offset(L, false);
          b(offset);
        } else {
          sltu(scratch, rs, r2);
          offset = shifted_branch_offset(L, false);
          bne(scratch, zero_reg, offset);
        }
        break;
      case Uless_equal:
        if (r2.is(zero_reg)) {
          offset = shifted_branch_offset(L, false);
          b(offset);
        } else {
          sltu(scratch, r2, rs);
          offset = shifted_branch_offset(L, false);
          beq(scratch, zero_reg, offset);
        }
        break;
      default:
        UNREACHABLE();
    }
  } else {
    // Be careful to always use shifted_branch_offset only just before the
    // branch instruction, as the location will be remember for patching the
    // target.
    switch (cond) {
      case cc_always:
        offset = shifted_branch_offset(L, false);
        b(offset);
        break;
      case eq:
        r2 = scratch;
        li(r2, rt);
        offset = shifted_branch_offset(L, false);
        beq(rs, r2, offset);
        break;
      case ne:
        r2 = scratch;
        li(r2, rt);
        offset = shifted_branch_offset(L, false);
        bne(rs, r2, offset);
        break;
      // Signed comparison
      case greater:
        if (rt.imm32_ == 0) {
          offset = shifted_branch_offset(L, false);
          bgtz(rs, offset);
        } else {
          r2 = scratch;
          li(r2, rt);
          slt(scratch, r2, rs);
          offset = shifted_branch_offset(L, false);
          bne(scratch, zero_reg, offset);
        }
        break;
      case greater_equal:
        if (rt.imm32_ == 0) {
          offset = shifted_branch_offset(L, false);
          bgez(rs, offset);
        } else if (is_int16(rt.imm32_)) {
          slti(scratch, rs, rt.imm32_);
          offset = shifted_branch_offset(L, false);
          beq(scratch, zero_reg, offset);
        } else {
          r2 = scratch;
          li(r2, rt);
          sltu(scratch, rs, r2);
          offset = shifted_branch_offset(L, false);
          beq(scratch, zero_reg, offset);
        }
        break;
      case less:
        if (rt.imm32_ == 0) {
          offset = shifted_branch_offset(L, false);
          bltz(rs, offset);
        } else if (is_int16(rt.imm32_)) {
          slti(scratch, rs, rt.imm32_);
          offset = shifted_branch_offset(L, false);
          bne(scratch, zero_reg, offset);
        } else {
          r2 = scratch;
          li(r2, rt);
          slt(scratch, rs, r2);
          offset = shifted_branch_offset(L, false);
          bne(scratch, zero_reg, offset);
        }
        break;
      case less_equal:
        if (rt.imm32_ == 0) {
          offset = shifted_branch_offset(L, false);
          blez(rs, offset);
        } else {
          r2 = scratch;
          li(r2, rt);
          slt(scratch, r2, rs);
          offset = shifted_branch_offset(L, false);
          beq(scratch, zero_reg, offset);
        }
        break;
      // Unsigned comparison.
      case Ugreater:
        if (rt.imm32_ == 0) {
          offset = shifted_branch_offset(L, false);
          bgtz(rs, offset);
        } else {
          r2 = scratch;
          li(r2, rt);
          sltu(scratch, r2, rs);
          offset = shifted_branch_offset(L, false);
          bne(scratch, zero_reg, offset);
        }
        break;
      case Ugreater_equal:
        if (rt.imm32_ == 0) {
          offset = shifted_branch_offset(L, false);
          bgez(rs, offset);
        } else if (is_int16(rt.imm32_)) {
          sltiu(scratch, rs, rt.imm32_);
          offset = shifted_branch_offset(L, false);
          beq(scratch, zero_reg, offset);
        } else {
          r2 = scratch;
          li(r2, rt);
          sltu(scratch, rs, r2);
          offset = shifted_branch_offset(L, false);
          beq(scratch, zero_reg, offset);
        }
        break;
     case Uless:
        if (rt.imm32_ == 0) {
          offset = shifted_branch_offset(L, false);
          b(offset);
        } else if (is_int16(rt.imm32_)) {
          sltiu(scratch, rs, rt.imm32_);
          offset = shifted_branch_offset(L, false);
          bne(scratch, zero_reg, offset);
        } else {
          r2 = scratch;
          li(r2, rt);
          sltu(scratch, rs, r2);
          offset = shifted_branch_offset(L, false);
          bne(scratch, zero_reg, offset);
        }
        break;
      case Uless_equal:
        if (rt.imm32_ == 0) {
          offset = shifted_branch_offset(L, false);
          b(offset);
        } else {
          r2 = scratch;
          li(r2, rt);
          sltu(scratch, r2, rs);
          offset = shifted_branch_offset(L, false);
          beq(scratch, zero_reg, offset);
        }
        break;
      default:
        UNREACHABLE();
    }
  }
  // Check that offset could actually hold on an int16_t.
  ASSERT(is_int16(offset));
  // Emit a nop in the branch delay slot if required.
  if (ProtectBranchDelaySlot)
    nop();
}


// We need to use a bgezal or bltzal, but they can't be used directly with the
// slt instructions. We could use sub or add instead but we would miss overflow
// cases, so we keep slt and add an intermediate third instruction.
void MacroAssembler::BranchAndLink(int16_t offset,
                                   bool ProtectBranchDelaySlot) {
  bal(offset);

  // Emit a nop in the branch delay slot if required.
  if (ProtectBranchDelaySlot)
    nop();
}


void MacroAssembler::BranchAndLink(int16_t offset, Condition cond, Register rs,
                                   const Operand& rt,
                                   bool ProtectBranchDelaySlot) {
  BRANCH_ARGS_CHECK(cond, rs, rt);
  Register r2 = no_reg;
  Register scratch = at;

  if (rt.is_reg()) {
    r2 = rt.rm_;
  } else if (cond != cc_always) {
    r2 = scratch;
    li(r2, rt);
  }

  switch (cond) {
    case cc_always:
      bal(offset);
      break;
    case eq:
      bne(rs, r2, 2);
      nop();
      bal(offset);
      break;
    case ne:
      beq(rs, r2, 2);
      nop();
      bal(offset);
      break;

    // Signed comparison
    case greater:
      slt(scratch, r2, rs);
      addiu(scratch, scratch, -1);
      bgezal(scratch, offset);
      break;
    case greater_equal:
      slt(scratch, rs, r2);
      addiu(scratch, scratch, -1);
      bltzal(scratch, offset);
      break;
    case less:
      slt(scratch, rs, r2);
      addiu(scratch, scratch, -1);
      bgezal(scratch, offset);
      break;
    case less_equal:
      slt(scratch, r2, rs);
      addiu(scratch, scratch, -1);
      bltzal(scratch, offset);
      break;

    // Unsigned comparison.
    case Ugreater:
      sltu(scratch, r2, rs);
      addiu(scratch, scratch, -1);
      bgezal(scratch, offset);
      break;
    case Ugreater_equal:
      sltu(scratch, rs, r2);
      addiu(scratch, scratch, -1);
      bltzal(scratch, offset);
      break;
    case Uless:
      sltu(scratch, rs, r2);
      addiu(scratch, scratch, -1);
      bgezal(scratch, offset);
      break;
    case Uless_equal:
      sltu(scratch, r2, rs);
      addiu(scratch, scratch, -1);
      bltzal(scratch, offset);
      break;

    default:
      UNREACHABLE();
  }
  // Emit a nop in the branch delay slot if required.
  if (ProtectBranchDelaySlot)
    nop();
}


void MacroAssembler::BranchAndLink(Label* L,
                                   bool ProtectBranchDelaySlot) {
  bal(shifted_branch_offset(L, false));

  // Emit a nop in the branch delay slot if required.
  if (ProtectBranchDelaySlot)
    nop();
}


void MacroAssembler::BranchAndLink(Label* L, Condition cond, Register rs,
                                   const Operand& rt,
                                   bool ProtectBranchDelaySlot) {
  BRANCH_ARGS_CHECK(cond, rs, rt);

  int32_t offset;
  Register r2 = no_reg;
  Register scratch = at;
  if (rt.is_reg()) {
    r2 = rt.rm_;
  } else if (cond != cc_always) {
    r2 = scratch;
    li(r2, rt);
  }

  switch (cond) {
    case cc_always:
      offset = shifted_branch_offset(L, false);
      bal(offset);
      break;
    case eq:
      bne(rs, r2, 2);
      nop();
      offset = shifted_branch_offset(L, false);
      bal(offset);
      break;
    case ne:
      beq(rs, r2, 2);
      nop();
      offset = shifted_branch_offset(L, false);
      bal(offset);
      break;

    // Signed comparison
    case greater:
      slt(scratch, r2, rs);
      addiu(scratch, scratch, -1);
      offset = shifted_branch_offset(L, false);
      bgezal(scratch, offset);
      break;
    case greater_equal:
      slt(scratch, rs, r2);
      addiu(scratch, scratch, -1);
      offset = shifted_branch_offset(L, false);
      bltzal(scratch, offset);
      break;
    case less:
      slt(scratch, rs, r2);
      addiu(scratch, scratch, -1);
      offset = shifted_branch_offset(L, false);
      bgezal(scratch, offset);
      break;
    case less_equal:
      slt(scratch, r2, rs);
      addiu(scratch, scratch, -1);
      offset = shifted_branch_offset(L, false);
      bltzal(scratch, offset);
      break;

    // Unsigned comparison.
    case Ugreater:
      sltu(scratch, r2, rs);
      addiu(scratch, scratch, -1);
      offset = shifted_branch_offset(L, false);
      bgezal(scratch, offset);
      break;
    case Ugreater_equal:
      sltu(scratch, rs, r2);
      addiu(scratch, scratch, -1);
      offset = shifted_branch_offset(L, false);
      bltzal(scratch, offset);
      break;
    case Uless:
      sltu(scratch, rs, r2);
      addiu(scratch, scratch, -1);
      offset = shifted_branch_offset(L, false);
      bgezal(scratch, offset);
      break;
    case Uless_equal:
      sltu(scratch, r2, rs);
      addiu(scratch, scratch, -1);
      offset = shifted_branch_offset(L, false);
      bltzal(scratch, offset);
      break;

    default:
      UNREACHABLE();
  }

  // Check that offset could actually hold on an int16_t.
  ASSERT(is_int16(offset));

  // Emit a nop in the branch delay slot if required.
  if (ProtectBranchDelaySlot)
    nop();
}


void MacroAssembler::Jump(const Operand& target,
                          bool ProtectBranchDelaySlot) {
  if (target.is_reg()) {
      jr(target.rm());
  } else {    // !target.is_reg()
    if (!MustUseReg(target.rmode_)) {
        j(target.imm32_);
    } else {  // MustUseReg(target)
      li(t9, target);
      jr(t9);
    }
  }
  // Emit a nop in the branch delay slot if required.
  if (ProtectBranchDelaySlot)
    nop();
}


void MacroAssembler::Jump(const Operand& target,
                          Condition cond, Register rs, const Operand& rt,
                          bool ProtectBranchDelaySlot) {
  BRANCH_ARGS_CHECK(cond, rs, rt);
  if (target.is_reg()) {
    if (cond == cc_always) {
      jr(target.rm());
    } else {
      Branch(2, NegateCondition(cond), rs, rt);
      jr(target.rm());
    }
  } else {    // !target.is_reg()
    if (!MustUseReg(target.rmode_)) {
      if (cond == cc_always) {
        j(target.imm32_);
      } else {
        Branch(2, NegateCondition(cond), rs, rt);
        j(target.imm32_);  // Will generate only one instruction.
      }
    } else {  // MustUseReg(target)
      li(t9, target);
      if (cond == cc_always) {
        jr(t9);
      } else {
        Branch(2, NegateCondition(cond), rs, rt);
        jr(t9);  // Will generate only one instruction.
      }
    }
  }
  // Emit a nop in the branch delay slot if required.
  if (ProtectBranchDelaySlot)
    nop();
}


// Note: To call gcc-compiled C code on mips, you must call thru t9.
void MacroAssembler::Call(const Operand& target,
                          bool ProtectBranchDelaySlot) {
  BlockTrampolinePoolScope block_trampoline_pool(this);
  if (target.is_reg()) {
      jalr(target.rm());
  } else {    // !target.is_reg()
    if (!MustUseReg(target.rmode_)) {
      jal(target.imm32_);
    } else {  // MustUseReg(target)
      li(t9, target);
      jalr(t9);
    }
  }
  // Emit a nop in the branch delay slot if required.
  if (ProtectBranchDelaySlot)
    nop();
}


// Note: To call gcc-compiled C code on mips, you must call thru t9.
void MacroAssembler::Call(const Operand& target,
                          Condition cond, Register rs, const Operand& rt,
                          bool ProtectBranchDelaySlot) {
  BRANCH_ARGS_CHECK(cond, rs, rt);
  BlockTrampolinePoolScope block_trampoline_pool(this);
  if (target.is_reg()) {
    if (cond == cc_always) {
      jalr(target.rm());
    } else {
      Branch(2, NegateCondition(cond), rs, rt);
      jalr(target.rm());
    }
  } else {    // !target.is_reg()
    if (!MustUseReg(target.rmode_)) {
      if (cond == cc_always) {
        jal(target.imm32_);
      } else {
        Branch(2, NegateCondition(cond), rs, rt);
        jal(target.imm32_);  // Will generate only one instruction.
      }
    } else {  // MustUseReg(target)
      li(t9, target);
      if (cond == cc_always) {
        jalr(t9);
      } else {
        Branch(2, NegateCondition(cond), rs, rt);
        jalr(t9);  // Will generate only one instruction.
      }
    }
  }
  // Emit a nop in the branch delay slot if required.
  if (ProtectBranchDelaySlot)
    nop();
}


void MacroAssembler::StackLimitCheck(Label* on_stack_overflow) {
  UNIMPLEMENTED_MIPS();
  break_(__LINE__);
}


void MacroAssembler::Drop(int count, Condition cond) {
  ASSERT(cond == al);
  if (count > 0) {
    addiu(sp, sp, count * kPointerSize);
  }
}


void MacroAssembler::Swap(Register reg1,
                          Register reg2,
                          Register scratch) {
  if (scratch.is(no_reg)) {
    Xor(reg1, reg1, Operand(reg2));
    Xor(reg2, reg2, Operand(reg1));
    Xor(reg1, reg1, Operand(reg2));
  } else {
    mov(scratch, reg1);
    mov(reg1, reg2);
    mov(reg2, scratch);
  }
}


void MacroAssembler::Call(Label* target) {
  BranchAndLink(cc_always, target);
}


void MacroAssembler::Move(Register dst, Register src) {
  if (!dst.is(src)) {
    mov(dst, src);
  }
}


#ifdef ENABLE_DEBUGGER_SUPPORT
// ---------------------------------------------------------------------------
// Debugger Support

void MacroAssembler::SaveRegistersToMemory(RegList regs) {
  ASSERT((regs & ~kJSCallerSaved) == 0);
  // Copy the content of registers to memory location.
  for (int i = 0; i < kNumJSCallerSaved; i++) {
    int r = JSCallerSavedCode(i);
    if ((regs & (1 << r)) != 0) {
      Register reg = { r };
      li(at, Operand(ExternalReference(Debug_Address::Register(i))));
      sw(reg, MemOperand(at));
    }
  }
}


void MacroAssembler::RestoreRegistersFromMemory(RegList regs) {
  ASSERT((regs & ~kJSCallerSaved) == 0);
  // Copy the content of memory location to registers.
  for (int i = kNumJSCallerSaved; --i >= 0;) {
    int r = JSCallerSavedCode(i);
    if ((regs & (1 << r)) != 0) {
      Register reg = { r };
      li(at, Operand(ExternalReference(Debug_Address::Register(i))));
      lw(reg, MemOperand(at));
    }
  }
}


void MacroAssembler::CopyRegistersFromMemoryToStack(Register base,
                                                    RegList regs) {
  ASSERT((regs & ~kJSCallerSaved) == 0);
  // Copy the content of the memory location to the stack and adjust base.
  for (int i = kNumJSCallerSaved; --i >= 0;) {
    int r = JSCallerSavedCode(i);
    if ((regs & (1 << r)) != 0) {
      li(at, Operand(ExternalReference(Debug_Address::Register(i))));
      lw(at, MemOperand(at));
      addiu(base, base, -kPointerSize);
      sw(at, MemOperand(base));
    }
  }
}


void MacroAssembler::CopyRegistersFromStackToMemory(Register base,
                                                    Register scratch,
                                                    RegList regs) {
  ASSERT((regs & ~kJSCallerSaved) == 0);
  // Copy the content of the stack to the memory location and adjust base.
  for (int i = 0; i < kNumJSCallerSaved; i++) {
    int r = JSCallerSavedCode(i);
    if ((regs & (1 << r)) != 0) {
      li(at, Operand(ExternalReference(Debug_Address::Register(i))));
      lw(scratch, MemOperand(base));
      addiu(base, base, kPointerSize);
      sw(scratch, MemOperand(at));
    }
  }
}


void MacroAssembler::DebugBreak() {
  ASSERT(allow_stub_calls());
  mov(a0, zero_reg);
  li(a1, Operand(ExternalReference(Runtime::kDebugBreak)));
  CEntryStub ces(1);
  Call(ces.GetCode(), RelocInfo::DEBUG_BREAK);
}

#endif  // ENABLE_DEBUGGER_SUPPORT


// ---------------------------------------------------------------------------
// Exception handling

void MacroAssembler::PushTryHandler(CodeLocation try_location,
                                    HandlerType type) {
  // Adjust this code if not the case.
  ASSERT(StackHandlerConstants::kSize == 4 * kPointerSize);
  // The return address is passed in register ra.
  if (try_location == IN_JAVASCRIPT) {
    if (type == TRY_CATCH_HANDLER) {
      li(t0, Operand(StackHandler::TRY_CATCH));
    } else {
      li(t0, Operand(StackHandler::TRY_FINALLY));
    }
    ASSERT(StackHandlerConstants::kStateOffset == 1 * kPointerSize
           && StackHandlerConstants::kFPOffset == 2 * kPointerSize
           && StackHandlerConstants::kPCOffset == 3 * kPointerSize
           && StackHandlerConstants::kNextOffset == 0 * kPointerSize);
    // Save the current handler as the next handler.
    LoadExternalReference(t2, ExternalReference(Top::k_handler_address));
    lw(t1, MemOperand(t2));

    addiu(sp, sp, -StackHandlerConstants::kSize);
    sw(ra, MemOperand(sp, 12));
    sw(fp, MemOperand(sp, 8));
    sw(t0, MemOperand(sp, 4));
    sw(t1, MemOperand(sp, 0));

    // Link this handler as the new current one.
    sw(sp, MemOperand(t2));

  } else {
    // Must preserve a0-a3, and s0 (argv).
    ASSERT(try_location == IN_JS_ENTRY);
    ASSERT(StackHandlerConstants::kStateOffset == 1 * kPointerSize
           && StackHandlerConstants::kFPOffset == 2 * kPointerSize
           && StackHandlerConstants::kPCOffset == 3 * kPointerSize
           && StackHandlerConstants::kNextOffset == 0 * kPointerSize);

    // The frame pointer does not point to a JS frame so we save NULL
    // for fp. We expect the code throwing an exception to check fp
    // before dereferencing it to restore the context.
    li(t0, Operand(StackHandler::ENTRY));

    // Save the current handler as the next handler.
    LoadExternalReference(t2, ExternalReference(Top::k_handler_address));
    lw(t1, MemOperand(t2));

    addiu(sp, sp, -StackHandlerConstants::kSize);
    sw(ra, MemOperand(sp, 12));
    sw(zero_reg, MemOperand(sp, 8));
    sw(t0, MemOperand(sp, 4));
    sw(t1, MemOperand(sp, 0));

    // Link this handler as the new current one.
    sw(sp, MemOperand(t2));
  }
}


void MacroAssembler::PopTryHandler() {
  UNIMPLEMENTED_MIPS();
}


void MacroAssembler::AllocateInNewSpace(int object_size,
                                        Register result,
                                        Register scratch1,
                                        Register scratch2,
                                        Label* gc_required,
                                        AllocationFlags flags) {
  ASSERT(!result.is(scratch1));
  ASSERT(!scratch1.is(scratch2));

  // Make object size into bytes.
  if ((flags & SIZE_IN_WORDS) != 0) {
    object_size *= kPointerSize;
  }
  ASSERT_EQ(0, object_size & kObjectAlignmentMask);

  // Load address of new object into result and allocation top address into
  // scratch1.
  ExternalReference new_space_allocation_top =
      ExternalReference::new_space_allocation_top_address();
  li(scratch1, Operand(new_space_allocation_top));
  if ((flags & RESULT_CONTAINS_TOP) == 0) {
    lw(result, MemOperand(scratch1));
  } else {
#ifdef DEBUG
    // Assert that result actually contains top on entry. scratch2 is used
    // immediately below so this use of scratch2 does not cause difference with
    // respect to register content between debug and release mode.
    lw(scratch2, MemOperand(scratch1));
    Check(eq, "Unexpected allocation top", result, Operand(scratch2));
#endif
  }

  // Calculate new top and bail out if new space is exhausted. Use result
  // to calculate the new top.
  ExternalReference new_space_allocation_limit =
      ExternalReference::new_space_allocation_limit_address();
  li(scratch2, Operand(new_space_allocation_limit));
  lw(scratch2, MemOperand(scratch2));
  Addu(result, result, Operand(object_size));
  Branch(gc_required, Ugreater, result, Operand(scratch2));

  // Update allocation top. Result temporarily holds the new top.
  sw(result, MemOperand(scratch1));

  // Tag and adjust back to start of new object.
  if ((flags & TAG_OBJECT) != 0) {
    Addu(result, result, Operand(-object_size + kHeapObjectTag));
  } else {
    Addu(result, result, Operand(-object_size));
  }
}


void MacroAssembler::AllocateInNewSpace(Register object_size,
                                        Register result,
                                        Register scratch1,
                                        Register scratch2,
                                        Label* gc_required,
                                        AllocationFlags flags) {
  ASSERT(!result.is(scratch1));
  ASSERT(!scratch1.is(scratch2));

  // Load address of new object into result and allocation top address into
  // scratch1.
  ExternalReference new_space_allocation_top =
      ExternalReference::new_space_allocation_top_address();
  li(scratch1, Operand(new_space_allocation_top));
  if ((flags & RESULT_CONTAINS_TOP) == 0) {
    lw(result, MemOperand(scratch1));
  } else {
#ifdef DEBUG
    // Assert that result actually contains top on entry. scratch2 is used
    // immediately below so this use of scratch2 does not cause difference with
    // respect to register content between debug and release mode.
    lw(scratch2, MemOperand(scratch1));
    Check(eq, "Unexpected allocation top", result, Operand(scratch2));
#endif
  }

  // Calculate new top and bail out if new space is exhausted. Use result
  // to calculate the new top. Object size is in words so a shift is required to
  // get the number of bytes
  ExternalReference new_space_allocation_limit =
      ExternalReference::new_space_allocation_limit_address();
  li(scratch2, Operand(new_space_allocation_limit));
  lw(scratch2, MemOperand(scratch2));
  if ((flags & SIZE_IN_WORDS) != 0) {
    sll(t8, object_size, kPointerSizeLog2);
    Addu(result, result, Operand(t8));
  } else {
    addu(result, result, object_size);
  }
  Branch(gc_required, Ugreater, result, Operand(scratch2));

  // Update allocation top. result temporarily holds the new top,
  sw(result, MemOperand(scratch1));

  // Adjust back to start of new object.
  if ((flags & SIZE_IN_WORDS) != 0) {
    Subu(result, result, Operand(t8));
  } else {
    subu(result, result, object_size);
  }

  // Tag object if requested.
  if ((flags & TAG_OBJECT) != 0) {
    Addu(result, result, Operand(kHeapObjectTag));
  }
}


void MacroAssembler::UndoAllocationInNewSpace(Register object,
                                              Register scratch) {
  ExternalReference new_space_allocation_top =
      ExternalReference::new_space_allocation_top_address();

  // Make sure the object has no tag before resetting top.
  And(object, object, Operand(~kHeapObjectTagMask));
#ifdef DEBUG
  // Check that the object un-allocated is below the current top.
  li(scratch, Operand(new_space_allocation_top));
  lw(scratch, MemOperand(scratch));
  Check(less, "Undo allocation of non allocated memory",
      object, Operand(scratch));
#endif
  // Write the address of the object to un-allocate as the current top.
  li(scratch, Operand(new_space_allocation_top));
  sw(object, MemOperand(scratch));
}


void MacroAssembler::AllocateTwoByteString(Register result,
                                           Register length,
                                           Register scratch1,
                                           Register scratch2,
                                           Register scratch3,
                                           Label* gc_required) {
  // Calculate the number of bytes needed for the characters in the string while
  // observing object alignment.
  ASSERT((SeqTwoByteString::kHeaderSize & kObjectAlignmentMask) == 0);
  sll(scratch1, length, 1);  // Length in bytes, not chars.
  addiu(scratch1, scratch1,
       kObjectAlignmentMask + SeqTwoByteString::kHeaderSize);
  And(scratch1, scratch1, Operand(~kObjectAlignmentMask));

  // Allocate two-byte string in new space.
  AllocateInNewSpace(scratch1,
                     result,
                     scratch2,
                     scratch3,
                     gc_required,
                     TAG_OBJECT);

  // Set the map, length and hash field.
  InitializeNewString(result,
                     length,
                     Heap::kStringMapRootIndex,
                     scratch1,
                     scratch2);
}


void MacroAssembler::AllocateAsciiString(Register result,
                                         Register length,
                                         Register scratch1,
                                         Register scratch2,
                                         Register scratch3,
                                         Label* gc_required) {
  // Calculate the number of bytes needed for the characters in the string
  // while observing object alignment.
  ASSERT((SeqAsciiString::kHeaderSize & kObjectAlignmentMask) == 0);
  ASSERT(kCharSize == 1);
  addiu(scratch1, length, kObjectAlignmentMask + SeqAsciiString::kHeaderSize);
  And(scratch1, scratch1, Operand(~kObjectAlignmentMask));

  // Allocate ASCII string in new space.
  AllocateInNewSpace(scratch1,
                     result,
                     scratch2,
                     scratch3,
                     gc_required,
                     TAG_OBJECT);

  // Set the map, length and hash field.
  InitializeNewString(result,
                     length,
                     Heap::kAsciiStringMapRootIndex,
                     scratch1,
                     scratch2);
}


void MacroAssembler::AllocateTwoByteConsString(Register result,
                                               Register length,
                                               Register scratch1,
                                               Register scratch2,
                                               Label* gc_required) {
  AllocateInNewSpace(ConsString::kSize,
                     result,
                     scratch1,
                     scratch2,
                     gc_required,
                     TAG_OBJECT);
  InitializeNewString(result,
                     length,
                     Heap::kConsStringMapRootIndex,
                     scratch1,
                     scratch2);
}


void MacroAssembler::AllocateAsciiConsString(Register result,
                                             Register length,
                                             Register scratch1,
                                             Register scratch2,
                                             Label* gc_required) {
  AllocateInNewSpace(ConsString::kSize,
                     result,
                     scratch1,
                     scratch2,
                     gc_required,
                     TAG_OBJECT);
  InitializeNewString(result,
                    length,
                    Heap::kConsAsciiStringMapRootIndex,
                    scratch1,
                    scratch2);
}


// Allocates a heap number or jumps to the label if the young space is full and
// a scavenge is needed.
void MacroAssembler::AllocateHeapNumber(Register result,
                               Register scratch1,
                               Register scratch2,
                               Register heap_number_map,
                               Label* need_gc) {

  // Allocate an object in the heap for the heap number and tag it as a heap
  // object.
  // We ask for four more bytes to align it as we need and align the result.
  // (HeapNumber::kSize is modified to be 4-byte bigger)
  AllocateInNewSpace(HeapNumber::kSize,
                        result,
                        scratch1,
                        scratch2,
                        need_gc,
                        TAG_OBJECT);

  // Align to 8 bytes. [Commented out pending code review.]
  // __ addiu(result, result, 7-1);  // -1 because result is tagged.
  // __ And(result, result, Operand(~7));
  // __ Or(result, result, Operand(1));  // Tag it back.

#ifdef DEBUG
//  // TODO(MIPS.6)
//  // Check that the result is 8-byte aligned.
//  andi(scratch2, result, Operand(7));
//  xori(scratch2, scratch2, Operand(1));  // Fail if the tag is missing.
//  Check(eq,
//          "Error in HeapNumber alloc (not 8-byte aligned or tag missing)",
//          scratch2, Operand(zero_reg));
#endif

  // Store heap number map in the allocated object.
  AssertRegisterIsRoot(heap_number_map, Heap::kHeapNumberMapRootIndex);
  sw(heap_number_map, FieldMemOperand(result, HeapObject::kMapOffset));
}


void MacroAssembler::AllocateHeapNumberWithValue(Register result,
                                                 FPURegister value,
                                                 Register scratch1,
                                                 Register scratch2,
                                                 Label* gc_required) {
  LoadRoot(t6, Heap::kHeapNumberMapRootIndex);
  AllocateHeapNumber(result, scratch1, scratch2, t6, gc_required);
  sdc1(value, FieldMemOperand(result, HeapNumber::kValueOffset));
}

// Copies a fixed number of fields of heap objects from src to dst.
void MacroAssembler::CopyFields(Register dst,
                                Register src,
                                RegList temps,
                                int field_count) {
  ASSERT((temps & dst.bit()) == 0);
  ASSERT((temps & src.bit()) == 0);
  // Primitive implementation using only one temporary register.

  Register tmp = no_reg;
  // Find a temp register in temps list.
  for (int i = 0; i < kNumRegisters; i++) {
    if ((temps & (1 << i)) != 0) {
      tmp.code_ = i;
      break;
    }
  }
  ASSERT(!tmp.is(no_reg));

  for (int i = 0; i < field_count; i++) {
    lw(tmp, FieldMemOperand(src, i * kPointerSize));
    sw(tmp, FieldMemOperand(dst, i * kPointerSize));
  }
}


void MacroAssembler::CheckMap(Register obj,
                              Register scratch,
                              Handle<Map> map,
                              Label* fail,
                              bool is_heap_object) {
  if (!is_heap_object) {
    BranchOnSmi(obj, fail);
  }
  lw(scratch, FieldMemOperand(obj, HeapObject::kMapOffset));
  li(at, Operand(map));
  Branch(fail, ne, scratch, Operand(at));
}


void MacroAssembler::CheckMap(Register obj,
                              Register scratch,
                              Heap::RootListIndex index,
                              Label* fail,
                              bool is_heap_object) {
  if (!is_heap_object) {
    BranchOnSmi(obj, fail);
  }
  lw(scratch, FieldMemOperand(obj, HeapObject::kMapOffset));
  LoadRoot(at, index);
  Branch(fail, ne, scratch, Operand(at));
}


// -----------------------------------------------------------------------------
// JavaScript invokes

void MacroAssembler::InvokePrologue(const ParameterCount& expected,
                                    const ParameterCount& actual,
                                    Handle<Code> code_constant,
                                    Register code_reg,
                                    Label* done,
                                    InvokeFlag flag) {
  bool definitely_matches = false;
  Label regular_invoke;

  // Check whether the expected and actual arguments count match. If not,
  // setup registers according to contract with ArgumentsAdaptorTrampoline:
  //  a0: actual arguments count
  //  a1: function (passed through to callee)
  //  a2: expected arguments count
  //  a3: callee code entry

  // The code below is made a lot easier because the calling code already sets
  // up actual and expected registers according to the contract if values are
  // passed in registers.
  ASSERT(actual.is_immediate() || actual.reg().is(a0));
  ASSERT(expected.is_immediate() || expected.reg().is(a2));
  ASSERT((!code_constant.is_null() && code_reg.is(no_reg)) || code_reg.is(a3));

  if (expected.is_immediate()) {
    ASSERT(actual.is_immediate());
    if (expected.immediate() == actual.immediate()) {
      definitely_matches = true;
    } else {
      li(a0, Operand(actual.immediate()));
      const int sentinel = SharedFunctionInfo::kDontAdaptArgumentsSentinel;
      if (expected.immediate() == sentinel) {
        // Don't worry about adapting arguments for builtins that
        // don't want that done. Skip adaption code by making it look
        // like we have a match between expected and actual number of
        // arguments.
        definitely_matches = true;
      } else {
        li(a2, Operand(expected.immediate()));
      }
    }
  } else if (actual.is_immediate()) {
    Branch(&regular_invoke, eq, expected.reg(), Operand(actual.immediate()));
    li(a0, Operand(actual.immediate()));
  } else {
    Branch(&regular_invoke, eq, expected.reg(), Operand(actual.reg()));
  }

  if (!definitely_matches) {
    if (!code_constant.is_null()) {
      li(a3, Operand(code_constant));
      addiu(a3, a3, Code::kHeaderSize - kHeapObjectTag);
    }

    ExternalReference adaptor(Builtins::ArgumentsAdaptorTrampoline);
    if (flag == CALL_FUNCTION) {
      CallBuiltin(adaptor);
      jmp(done);
    } else {
      JumpToBuiltin(adaptor);
      // break_(__LINE__);
    }
    bind(&regular_invoke);
  }
}


void MacroAssembler::InvokeCode(Register code,
                                const ParameterCount& expected,
                                const ParameterCount& actual,
                                InvokeFlag flag) {
  Label done;

  InvokePrologue(expected, actual, Handle<Code>::null(), code, &done, flag);
  if (flag == CALL_FUNCTION) {
    Call(code);
  } else {
    ASSERT(flag == JUMP_FUNCTION);
    Jump(code);
  }
  // Continue here if InvokePrologue does handle the invocation due to
  // mismatched parameter counts.
  bind(&done);
}


void MacroAssembler::InvokeCode(Handle<Code> code,
                                const ParameterCount& expected,
                                const ParameterCount& actual,
                                RelocInfo::Mode rmode,
                                InvokeFlag flag) {
  Label done;

  InvokePrologue(expected, actual, code, no_reg, &done, flag);
  if (flag == CALL_FUNCTION) {
    Call(code, rmode);
  } else {
    Jump(code, rmode);
  }
  // Continue here if InvokePrologue does handle the invocation due to
  // mismatched parameter counts.
  bind(&done);
}


void MacroAssembler::InvokeFunction(Register function,
                                    const ParameterCount& actual,
                                    InvokeFlag flag) {
  // Contract with called JS functions requires that function is passed in a1.
  ASSERT(function.is(a1));
  Register expected_reg = a2;
  Register code_reg = a3;

  lw(code_reg, FieldMemOperand(a1, JSFunction::kSharedFunctionInfoOffset));
  lw(cp, FieldMemOperand(a1, JSFunction::kContextOffset));
  lw(expected_reg,
      FieldMemOperand(code_reg,
                      SharedFunctionInfo::kFormalParameterCountOffset));
  sra(expected_reg, expected_reg, kSmiTagSize);
  lw(code_reg, FieldMemOperand(a1, JSFunction::kCodeEntryOffset));

  ParameterCount expected(expected_reg);
  InvokeCode(code_reg, expected, actual, flag);
}


void MacroAssembler::InvokeFunction(JSFunction* function,
                                    const ParameterCount& actual,
                                    InvokeFlag flag) {
  ASSERT(function->is_compiled());

  // Get the function and setup the context.
  li(a1, Operand(Handle<JSFunction>(function)));
  lw(cp, FieldMemOperand(a1, JSFunction::kContextOffset));

  // Invoke the cached code.
  Handle<Code> code(function->code());
  ParameterCount expected(function->shared()->formal_parameter_count());
  InvokeCode(code, expected, actual, RelocInfo::CODE_TARGET, flag);
}


// ---------------------------------------------------------------------------
// Support functions.

void MacroAssembler::TryGetFunctionPrototype(Register function,
                                             Register result,
                                             Register scratch,
                                             Label* miss) {
  // Check that the receiver isn't a smi.
  BranchOnSmi(function, miss);

  // Check that the function really is a function.  Load map into result reg.
  GetObjectType(function, result, scratch);
  Branch(miss, ne, scratch, Operand(JS_FUNCTION_TYPE));

  // Make sure that the function has an instance prototype.
  Label non_instance;
  lbu(scratch, FieldMemOperand(result, Map::kBitFieldOffset));
  And(scratch, scratch, Operand(1 << Map::kHasNonInstancePrototype));
  Branch(&non_instance, ne, scratch, Operand(zero_reg));

  // Get the prototype or initial map from the function.
  lw(result,
      FieldMemOperand(function, JSFunction::kPrototypeOrInitialMapOffset));

  // If the prototype or initial map is the hole, don't return it and
  // simply miss the cache instead. This will allow us to allocate a
  // prototype object on-demand in the runtime system.
  LoadRoot(t8, Heap::kTheHoleValueRootIndex);
  Branch(miss, eq, result, Operand(t8));

  // If the function does not have an initial map, we're done.
  Label done;
  GetObjectType(result, scratch, scratch);
  Branch(&done, ne, scratch, Operand(MAP_TYPE));

  // Get the prototype from the initial map.
  lw(result, FieldMemOperand(result, Map::kPrototypeOffset));
  jmp(&done);

  // Non-instance prototype: Fetch prototype from constructor field
  // in initial map.
  bind(&non_instance);
  lw(result, FieldMemOperand(result, Map::kConstructorOffset));

  // All done.
  bind(&done);
}


void MacroAssembler::GetObjectType(Register object,
                                   Register map,
                                   Register type_reg) {
  lw(map, FieldMemOperand(object, HeapObject::kMapOffset));
  lbu(type_reg, FieldMemOperand(map, Map::kInstanceTypeOffset));
}


void MacroAssembler::CallBuiltin(ExternalReference builtin_entry) {
  // Load builtin address.
  LoadExternalReference(t9, builtin_entry);
  lw(t9, MemOperand(t9));  // Deref address.
  addiu(t9, t9, Code::kHeaderSize - kHeapObjectTag);
  // Call and allocate arguments slots.
  jalr(t9);
  // Use the branch delay slot to allocated argument slots.
  addiu(sp, sp, -StandardFrameConstants::kBArgsSlotsSize);
  addiu(sp, sp, StandardFrameConstants::kBArgsSlotsSize);
}


void MacroAssembler::CallBuiltin(Register target) {
  // Target already holds target address.
  // Call and allocate arguments slots.
  jalr(target);
  // Use the branch delay slot to allocated argument slots.
  addiu(sp, sp, -StandardFrameConstants::kBArgsSlotsSize);
  addiu(sp, sp, StandardFrameConstants::kBArgsSlotsSize);
}


void MacroAssembler::CallBuiltin(Handle<Code> code, RelocInfo::Mode rmode) {
  ASSERT(RelocInfo::IsCodeTarget(rmode));
  // Jump but do not protect the branch delay slot.
  Call(false, code, rmode);
  // Use the branch delay slot to allocated argument slots.
  addiu(sp, sp, -StandardFrameConstants::kBArgsSlotsSize);
  addiu(sp, sp, StandardFrameConstants::kBArgsSlotsSize);
}


void MacroAssembler::JumpToBuiltin(ExternalReference builtin_entry) {
  // Load builtin address.
  LoadExternalReference(t9, builtin_entry);
  lw(t9, MemOperand(t9));  // Deref address.
  addiu(t9, t9, Code::kHeaderSize - kHeapObjectTag);
  // Call and allocate arguments slots.
  jr(t9);
  // Use the branch delay slot to allocated argument slots.
  addiu(sp, sp, -StandardFrameConstants::kBArgsSlotsSize);
}


void MacroAssembler::JumpToBuiltin(Register target) {
  // t9 already holds target address.
  // Call and allocate arguments slots.
  jr(t9);
  // Use the branch delay slot to allocated argument slots.
  addiu(sp, sp, -StandardFrameConstants::kBArgsSlotsSize);
}


void MacroAssembler::JumpToBuiltin(Handle<Code> code, RelocInfo::Mode rmode) {
  ASSERT(RelocInfo::IsCodeTarget(rmode));
  // Jump but do not protect the branch delay slot.
  Jump(false, code, rmode);
  // Use the branch delay slot to allocated argument slots.
  addiu(sp, sp, -StandardFrameConstants::kBArgsSlotsSize);
}


// -----------------------------------------------------------------------------
// Runtime calls

void MacroAssembler::CallStub(CodeStub* stub, Condition cond,
                              Register r1, const Operand& r2) {
  ASSERT(allow_stub_calls());  // Stub calls are not allowed in some stubs.
  Call(stub->GetCode(), RelocInfo::CODE_TARGET, cond, r1, r2);
}


void MacroAssembler::TailCallStub(CodeStub* stub) {
  ASSERT(allow_stub_calls());  // stub calls are not allowed in some stubs
  Jump(stub->GetCode(), RelocInfo::CODE_TARGET);
}


void MacroAssembler::StubReturn(int argc) {
  ASSERT(argc >= 1 && generating_stub());
  if (argc > 1)
    addiu(sp, sp, (argc - 1) * kPointerSize);
  Ret();
}


void MacroAssembler::IllegalOperation(int num_arguments) {
  if (num_arguments > 0) {
    addiu(sp, sp, num_arguments * kPointerSize);
  }
  LoadRoot(v0, Heap::kUndefinedValueRootIndex);
}

void MacroAssembler::ObjectToDoubleFPURegister(Register object,
                                               FPURegister result,
                                               Register scratch1,
                                               Register scratch2,
                                               Register heap_number_map,
                                               Label* not_number,
                                               ObjectToDoubleFlags flags) {
  Label done;
  if ((flags & OBJECT_NOT_SMI) == 0) {
    Label not_smi;
    BranchOnNotSmi(object, &not_smi);
    // Remove smi tag and convert to double.
    sra(scratch1, object, kSmiTagSize);
    mtc1(scratch1, result);
    cvt_d_w(result, result);
    Branch(&done);
    bind(&not_smi);
  }
  // Check for heap number and load double value from it.
  lw(scratch1, FieldMemOperand(object, HeapObject::kMapOffset));
  Subu(scratch2, object, Operand(kHeapObjectTag));
  Branch(not_number, ne, scratch1, Operand(heap_number_map));

  if ((flags & AVOID_NANS_AND_INFINITIES) != 0) {
    // If exponent is all ones the number is either a NaN or +/-Infinity.
    lw(scratch1, FieldMemOperand(object, HeapNumber::kExponentOffset));
    {
      // Signed bitfield extraction, based on ARM's Sbfx macro.
      // We cannot use the ext instruction, because it doesn't sign-extend.
      int lsb = HeapNumber::kExponentShift;
      int width = HeapNumber::kExponentBits;
      Register dst = scratch1;
      Register src = scratch1;

      int mask = (1 << (width + lsb)) - 1 - ((1 << lsb) - 1);
      And(dst, src, mask);
      int shift_up = 32 - lsb - width;
      int shift_down = lsb + shift_up;
      if (shift_up != 0) {
        sll(dst, dst, shift_up);
      }
      if (shift_down != 0) {
        sra(dst, dst, shift_down);
      }
    }
    // All-one value sign extend to -1.
    Branch(not_number, eq, scratch1, Operand(-1));
  }
  ldc1(result, MemOperand(scratch2, HeapNumber::kValueOffset));
  bind(&done);
}


void MacroAssembler::SmiToDoubleFPURegister(Register smi,
                                            FPURegister value,
                                            Register scratch1) {
  sra(scratch1, smi, kSmiTagSize);
  mtc1(scratch1, value);
  cvt_d_w(value, value);
}

void MacroAssembler::CallRuntime(Runtime::Function* f, int num_arguments) {
  // All parameters are on the stack. v0 has the return value after call.

  // If the expected number of arguments of the runtime function is
  // constant, we check that the actual number of arguments match the
  // expectation.
  if (f->nargs >= 0 && f->nargs != num_arguments) {
    IllegalOperation(num_arguments);
    return;
  }

  // TODO(1236192): Most runtime routines don't need the number of
  // arguments passed in because it is constant. At some point we
  // should remove this need and make the runtime routine entry code
  // smarter.
  li(a0, num_arguments);
  LoadExternalReference(a1, ExternalReference(f));
  CEntryStub stub(1);
  CallStub(&stub);
}


void MacroAssembler::CallRuntime(Runtime::FunctionId fid, int num_arguments) {
  CallRuntime(Runtime::FunctionForId(fid), num_arguments);
}


void MacroAssembler::CallExternalReference(const ExternalReference& ext,
                                           int num_arguments) {
  li(a0, Operand(num_arguments));
  li(a1, Operand(ext));

  CEntryStub stub(1);
  CallStub(&stub);
}


void MacroAssembler::TailCallExternalReference(const ExternalReference& ext,
                                               int num_arguments,
                                               int result_size) {
  // TODO(1236192): Most runtime routines don't need the number of
  // arguments passed in because it is constant. At some point we
  // should remove this need and make the runtime routine entry code
  // smarter.
  li(a0, Operand(num_arguments));
  JumpToExternalReference(ext);
}


void MacroAssembler::TailCallRuntime(Runtime::FunctionId fid,
                                     int num_arguments,
                                     int result_size) {
  TailCallExternalReference(ExternalReference(fid), num_arguments, result_size);
}


void MacroAssembler::JumpToExternalReference(const ExternalReference& builtin) {
  li(a1, Operand(builtin));
  CEntryStub stub(1);
  Jump(stub.GetCode(), RelocInfo::CODE_TARGET);
}


void MacroAssembler::InvokeBuiltin(Builtins::JavaScript id,
                                   InvokeJSFlags flags) {
  GetBuiltinEntry(t9, id);
  if (flags == CALL_JS) {
    Call(t9);
  } else {
    ASSERT(flags == JUMP_JS);
    Jump(t9);
  }
}


void MacroAssembler::GetBuiltinFunction(Register target,
                                        Builtins::JavaScript id) {
  // Load the builtins object into target register.
  lw(target, MemOperand(cp, Context::SlotOffset(Context::GLOBAL_INDEX)));
  lw(target, FieldMemOperand(target, GlobalObject::kBuiltinsOffset));
  // Load the JavaScript builtin function from the builtins object.
  lw(target, FieldMemOperand(target,
                          JSBuiltinsObject::OffsetOfFunctionWithId(id)));
}


void MacroAssembler::GetBuiltinEntry(Register target, Builtins::JavaScript id) {
  ASSERT(!target.is(a1));
  GetBuiltinFunction(a1, id);
  // Load the code entry point from the builtins object.
  lw(target, FieldMemOperand(a1, JSFunction::kCodeEntryOffset));
}


void MacroAssembler::SetCounter(StatsCounter* counter, int value,
                                Register scratch1, Register scratch2) {
  if (FLAG_native_code_counters && counter->Enabled()) {
    li(scratch1, Operand(value));
    li(scratch2, Operand(ExternalReference(counter)));
    sw(scratch1, MemOperand(scratch2));
  }
}


void MacroAssembler::IncrementCounter(StatsCounter* counter, int value,
                                      Register scratch1, Register scratch2) {
  ASSERT(value > 0);
  if (FLAG_native_code_counters && counter->Enabled()) {
    li(scratch2, Operand(ExternalReference(counter)));
    lw(scratch1, MemOperand(scratch2));
    Addu(scratch1, scratch1, Operand(value));
    sw(scratch1, MemOperand(scratch2));
  }
}


void MacroAssembler::DecrementCounter(StatsCounter* counter, int value,
                                      Register scratch1, Register scratch2) {
  ASSERT(value > 0);
  if (FLAG_native_code_counters && counter->Enabled()) {
    li(scratch2, Operand(ExternalReference(counter)));
    lw(scratch1, MemOperand(scratch2));
    Subu(scratch1, scratch1, Operand(value));
    sw(scratch1, MemOperand(scratch2));
  }
}


// -----------------------------------------------------------------------------
// Debugging

void MacroAssembler::Assert(Condition cc, const char* msg,
                            Register rs, Operand rt) {
  if (FLAG_debug_code)
    Check(cc, msg, rs, rt);
}


void MacroAssembler::AssertRegisterIsRoot(Register reg,
                                          Heap::RootListIndex index) {
  if (FLAG_debug_code) {
    LoadRoot(at, index);
    Check(eq, "Register did not match expected root", reg, Operand(at));
  }
}

void MacroAssembler::AssertFastElements(Register elements) {
  if (FLAG_debug_code) {
    ASSERT(!elements.is(at));
    Label ok;
    Push(elements);
    lw(elements, FieldMemOperand(elements, HeapObject::kMapOffset));
    LoadRoot(at, Heap::kFixedArrayMapRootIndex);
    Branch(&ok, eq, elements, Operand(at));
    LoadRoot(at, Heap::kFixedCOWArrayMapRootIndex);
    Branch(&ok, eq, elements, Operand(at));
    Abort("JSObject with fast elements map has slow elements");
    bind(&ok);
    Pop(elements);
  }
}


void MacroAssembler::Check(Condition cc, const char* msg,
                           Register rs, Operand rt) {
  Label L;
  Branch(&L, cc, rs, rt);
  Abort(msg);
  // will not return here
  bind(&L);
}


void MacroAssembler::Abort(const char* msg) {
  Label abort_start;
  bind(&abort_start);
  // We want to pass the msg string like a smi to avoid GC
  // problems, however msg is not guaranteed to be aligned
  // properly. Instead, we pass an aligned pointer that is
  // a proper v8 smi, but also pass the alignment difference
  // from the real pointer as a smi.
  intptr_t p1 = reinterpret_cast<intptr_t>(msg);
  intptr_t p0 = (p1 & ~kSmiTagMask) + kSmiTag;
  ASSERT(reinterpret_cast<Object*>(p0)->IsSmi());
#ifdef DEBUG
  if (msg != NULL) {
    RecordComment("Abort message: ");
    RecordComment(msg);
  }
#endif
  // Disable stub call restrictions to always allow calls to abort.
  set_allow_stub_calls(true);

  li(a0, Operand(p0));
  Push(a0);
  li(a0, Operand(Smi::FromInt(p1 - p0)));
  Push(a0);
  CallRuntime(Runtime::kAbort, 2);
  // will not return here
  if (is_trampoline_pool_blocked()) {
    // If the calling code cares about the exact number of
    // instructions generated, we insert padding here to keep the size
    // of the Abort macro constant.
    // Currently in debug mode with debug_code enabled the number of
    // generated instructions is 14, so we use this as a maximum value.
    static const int kExpectedAbortInstructions = 14;
    int abort_instructions = InstructionsGeneratedSince(&abort_start);
    ASSERT(abort_instructions <= kExpectedAbortInstructions);
    while (abort_instructions++ < kExpectedAbortInstructions) {
      nop();
    }
  }
}


void MacroAssembler::EnterFrame(StackFrame::Type type) {
  addiu(sp, sp, -5 * kPointerSize);
  li(t8, Operand(Smi::FromInt(type)));
  li(t9, Operand(CodeObject()));
  sw(ra, MemOperand(sp, 4 * kPointerSize));
  sw(fp, MemOperand(sp, 3 * kPointerSize));
  sw(cp, MemOperand(sp, 2 * kPointerSize));
  sw(t8, MemOperand(sp, 1 * kPointerSize));
  sw(t9, MemOperand(sp, 0 * kPointerSize));
  addiu(fp, sp, 3 * kPointerSize);
}


void MacroAssembler::LeaveFrame(StackFrame::Type type) {
  mov(sp, fp);
  lw(fp, MemOperand(sp, 0 * kPointerSize));
  lw(ra, MemOperand(sp, 1 * kPointerSize));
  addiu(sp, sp, 2 * kPointerSize);
}


void MacroAssembler::EnterExitFrame(ExitFrame::Mode mode,
                                    Register hold_argc,
                                    Register hold_argv,
                                    Register hold_function) {
  // Compute the argv pointer and keep it in a callee-saved register.
  // a0 is argc.
  sll(t8, a0, kPointerSizeLog2);
  addu(hold_argv, sp, t8);
  addiu(hold_argv, hold_argv, -kPointerSize);

  // Compute callee's stack pointer before making changes and save it as
  // t9 register so that it is restored as sp register on exit, thereby
  // popping the args.
  // t9 = sp + kPointerSize * #args
  addu(t9, sp, t8);

  // Align the stack at this point.
  AlignStack(0);

  // Save registers.
  addiu(sp, sp, -12);
  sw(t9, MemOperand(sp, 8));
  sw(ra, MemOperand(sp, 4));
  sw(fp, MemOperand(sp, 0));
  mov(fp, sp);  // Setup new frame pointer.

  li(t8, Operand(CodeObject()));
  Push(t8);  // Accessed from ExitFrame::code_slot.

  // Save the frame pointer and the context in top.
  LoadExternalReference(t8, ExternalReference(Top::k_c_entry_fp_address));
  sw(fp, MemOperand(t8));
  LoadExternalReference(t8, ExternalReference(Top::k_context_address));
  sw(cp, MemOperand(t8));

  // Setup argc and the builtin function in callee-saved registers.
  mov(hold_argc, a0);
  mov(hold_function, a1);


  #ifdef ENABLE_DEBUGGER_SUPPORT
    // Save the state of all registers to the stack from the memory
    // location. This is needed to allow nested break points.
    if (mode == ExitFrame::MODE_DEBUG) {
      // Use sp as base to push.
      CopyRegistersFromMemoryToStack(sp, kJSCallerSaved);
    }
  #endif
}


void MacroAssembler::LeaveExitFrame(ExitFrame::Mode mode) {
#ifdef ENABLE_DEBUGGER_SUPPORT
  // Restore the memory copy of the registers by digging them out from
  // the stack. This is needed to allow nested break points.
  if (mode == ExitFrame::MODE_DEBUG) {
    // This code intentionally clobbers a2 and a3.
    const int kCallerSavedSize = kNumJSCallerSaved * kPointerSize;
    const int kOffset = ExitFrameConstants::kCodeOffset - kCallerSavedSize;
    Addu(a3, fp, Operand(kOffset));
    CopyRegistersFromStackToMemory(a3, a2, kJSCallerSaved);
  }
#endif

  // Clear top frame.
  LoadExternalReference(t8, ExternalReference(Top::k_c_entry_fp_address));
  sw(zero_reg, MemOperand(t8));

  // Restore current context from top and clear it in debug mode.
  LoadExternalReference(t8, ExternalReference(Top::k_context_address));
  lw(cp, MemOperand(t8));
#ifdef DEBUG
  sw(a3, MemOperand(t8));
#endif

  // Pop the arguments, restore registers, and return.
  mov(sp, fp);  // Respect ABI stack constraint.
  lw(fp, MemOperand(sp, 0));
  lw(ra, MemOperand(sp, 4));
  lw(sp, MemOperand(sp, 8));
  jr(ra);
  nop();  // Branch delay slot nop.
}


void MacroAssembler::InitializeNewString(Register string,
                                         Register length,
                                         Heap::RootListIndex map_index,
                                         Register scratch1,
                                         Register scratch2) {
  sll(scratch1, length, kSmiTagSize);
  LoadRoot(scratch2, map_index);
  sw(scratch1, FieldMemOperand(string, String::kLengthOffset));
  li(scratch1, Operand(String::kEmptyHashField));
  sw(scratch2, FieldMemOperand(string, HeapObject::kMapOffset));
  sw(scratch1, FieldMemOperand(string, String::kHashFieldOffset));
}


int MacroAssembler::ActivationFrameAlignment() {
#if defined(V8_HOST_ARCH_MIPS)
  // Running on the real platform. Use the alignment as mandated by the local
  // environment.
  // Note: This will break if we ever start generating snapshots on one ARM
  // platform for another ARM platform with a different alignment.
  return OS::ActivationFrameAlignment();
#else  // defined(V8_HOST_ARCH_MIPS)
  // If we are using the simulator then we should always align to the expected
  // alignment. As the simulator is used to generate snapshots we do not know
  // if the target platform will need alignment, so this is controlled from a
  // flag.
  // return FLAG_sim_stack_alignment;  // TODO(REBASE): uncomment
  return 2 * kPointerSize;  // TODO(REBASE): use above flog & remove this line.
#endif  // defined(V8_HOST_ARCH_MIPS)
}


void MacroAssembler::AlignStack(int offset) {
  // On MIPS an offset of 0 aligns to 0 modulo 8 bytes,
  //     and an offset of 1 aligns to 4 modulo 8 bytes.
#if defined(V8_HOST_ARCH_MIPS)
  // Running on the real platform. Use the alignment as mandated by the local
  // environment.
  // Note: This will break if we ever start generating snapshots on one MIPS
  // platform for another MIPS platform with a different alignment.
  int activation_frame_alignment = OS::ActivationFrameAlignment();
#else  // defined(V8_HOST_ARCH_MIPS)
  // If we are using the simulator then we should always align to the expected
  // alignment. As the simulator is used to generate snapshots we do not know
  // if the target platform will need alignment, so we will always align at
  // this point here.
  int activation_frame_alignment = 2 * kPointerSize;
#endif  // defined(V8_HOST_ARCH_MIPS)
  if (activation_frame_alignment != kPointerSize) {
    // This code needs to be made more general if this assert doesn't hold.
    ASSERT(activation_frame_alignment == 2 * kPointerSize);
    if (offset == 0) {
      andi(t8, sp, activation_frame_alignment - 1);
      Push(zero_reg, eq, t8, zero_reg);
    } else {
      andi(t8, sp, activation_frame_alignment - 1);
      addiu(t8, t8, -4);
      Push(zero_reg, eq, t8, zero_reg);
    }
  }
}


void MacroAssembler::JumpIfNotBothSmi(Register reg1,
                                      Register reg2,
                                      Label* on_not_both_smi) {
  ASSERT_EQ(0, kSmiTag);
  ASSERT_EQ(1, kSmiTagMask);
  or_(at, reg1, reg2);
  andi(at, at, kSmiTagMask);
  Branch(on_not_both_smi, ne, at, Operand(zero_reg));
}


void MacroAssembler::JumpIfEitherSmi(Register reg1,
                                     Register reg2,
                                     Label* on_either_smi) {
  ASSERT_EQ(0, kSmiTag);
  ASSERT_EQ(1, kSmiTagMask);
  // Both Smi tags must be 1 (not Smi).
  and_(at, reg1, reg2);
  andi(at, at, kSmiTagMask);
  Branch(on_either_smi, eq, at, Operand(zero_reg));
}

void MacroAssembler::AbortIfSmi(Register object) {
  ASSERT_EQ(0, kSmiTag);
  andi(at, object, kSmiTagMask);
  Assert(ne, "Operand is a smi", at, Operand(zero_reg));
}

void MacroAssembler::JumpIfNonSmisNotBothSequentialAsciiStrings(
    Register first,
    Register second,
    Register scratch1,
    Register scratch2,
    Label* failure) {
  // Test that both first and second are sequential ASCII strings.
  // Assume that they are non-smis.
  lw(scratch1, FieldMemOperand(first, HeapObject::kMapOffset));
  lw(scratch2, FieldMemOperand(second, HeapObject::kMapOffset));
  lbu(scratch1, FieldMemOperand(scratch1, Map::kInstanceTypeOffset));
  lbu(scratch2, FieldMemOperand(scratch2, Map::kInstanceTypeOffset));

  JumpIfBothInstanceTypesAreNotSequentialAscii(scratch1,
                                               scratch2,
                                               scratch1,
                                               scratch2,
                                               failure);
}


void MacroAssembler::JumpIfNotBothSequentialAsciiStrings(Register first,
                                                         Register second,
                                                         Register scratch1,
                                                         Register scratch2,
                                                         Label* failure) {
  // Check that neither is a smi.
  ASSERT_EQ(0, kSmiTag);
  And(scratch1, first, Operand(second));
  And(scratch1, scratch1, Operand(kSmiTagMask));
  Branch(failure, eq, scratch1, Operand(zero_reg));
  JumpIfNonSmisNotBothSequentialAsciiStrings(first,
                                             second,
                                             scratch1,
                                             scratch2,
                                             failure);
}


void MacroAssembler::JumpIfBothInstanceTypesAreNotSequentialAscii(
    Register first,
    Register second,
    Register scratch1,
    Register scratch2,
    Label* failure) {
  int kFlatAsciiStringMask =
      kIsNotStringMask | kStringEncodingMask | kStringRepresentationMask;
  int kFlatAsciiStringTag = ASCII_STRING_TYPE;
  ASSERT(kFlatAsciiStringTag <= 0xffff);  // Ensure this fits 16-bit immed.
  andi(scratch1, first, kFlatAsciiStringMask);
  Branch(failure, ne, scratch1, Operand(kFlatAsciiStringTag));
  andi(scratch2, second, kFlatAsciiStringMask);
  Branch(failure, ne, scratch2, Operand(kFlatAsciiStringTag));
}


void MacroAssembler::JumpIfInstanceTypeIsNotSequentialAscii(Register type,
                                                            Register scratch,
                                                            Label* failure) {
  int kFlatAsciiStringMask =
      kIsNotStringMask | kStringEncodingMask | kStringRepresentationMask;
  int kFlatAsciiStringTag = ASCII_STRING_TYPE;
  And(scratch, type, Operand(kFlatAsciiStringMask));
  Branch(failure, ne, scratch, Operand(kFlatAsciiStringTag));
}


void MacroAssembler::PrepareCallCFunction(int num_arguments, Register scratch) {
  int frame_alignment = ActivationFrameAlignment();
  // Up to four simple arguments are passed in registers a0..a3.
  // Those four arguments must have reserved argument slots on the stack for
  // mips, even though those argument slots are not normally used.
  // Remaining arguments are pushed on the stack, above (higher address than)
  // the argument slots.
  ASSERT(StandardFrameConstants::kCArgsSlotsSize % kPointerSize == 0);
  int stack_passed_arguments = ((num_arguments <= 4) ? 0 : num_arguments - 4) +
                               StandardFrameConstants::kCArgsSlotsSize /
                               kPointerSize;
  if (frame_alignment > kPointerSize) {
    // Make stack end at alignment and make room for num_arguments - 4 words
    // and the original value of sp.
    mov(scratch, sp);
    Subu(sp, sp, Operand((stack_passed_arguments + 1) * kPointerSize));
    ASSERT(IsPowerOf2(frame_alignment));
    And(sp, sp, Operand(-frame_alignment));
    sw(scratch, MemOperand(sp, stack_passed_arguments * kPointerSize));
  } else {
    Subu(sp, sp, Operand(stack_passed_arguments * kPointerSize));
  }
}


void MacroAssembler::CallCFunction(ExternalReference function,
                                   int num_arguments) {
  li(t9, Operand(function));
  CallCFunction(t9, num_arguments);
}


void MacroAssembler::CallCFunction(Register function, int num_arguments) {
  // Make sure that the stack is aligned before calling a C function unless
  // running in the simulator. The simulator has its own alignment check which
  // provides more information.
  // The argument stots are presumed to have been set up by
  // PrepareCallCFunction. The C function must be called via t9, for mips ABI.
#if defined(V8_HOST_ARCH_MIPS)
  if (FLAG_debug_code) {
    int frame_alignment = OS::ActivationFrameAlignment();
    int frame_alignment_mask = frame_alignment - 1;
    if (frame_alignment > kPointerSize) {
      ASSERT(IsPowerOf2(frame_alignment));
      Label alignment_as_expected;
      And(at, sp, Operand(frame_alignment_mask));
      Branch(&alignment_as_expected, eq, at, Operand(zero_reg));
      // Don't use Check here, as it will call Runtime_Abort possibly
      // re-entering here.
      stop("Unexpected alignment in CallCFunction");
      bind(&alignment_as_expected);
    }
  }
#endif  // V8_HOST_ARCH_MIPS

  // Just call directly. The function called cannot cause a GC, or
  // allow preemption, so the return address in the link register
  // stays correct.
  if (!function.is(t9)) {
    mov(t9, function);
  }
  Call(t9);

  ASSERT(StandardFrameConstants::kCArgsSlotsSize % kPointerSize == 0);
  int stack_passed_arguments = ((num_arguments <= 4) ? 0 : num_arguments - 4) +
                               StandardFrameConstants::kCArgsSlotsSize /
                               kPointerSize;

  if (OS::ActivationFrameAlignment() > kPointerSize) {
    lw(sp, MemOperand(sp, stack_passed_arguments * kPointerSize));
  } else {
    Addu(sp, sp, Operand(stack_passed_arguments * sizeof(kPointerSize)));
  }
}


#undef BRANCH_ARGS_CHECK


#ifdef ENABLE_DEBUGGER_SUPPORT
CodePatcher::CodePatcher(byte* address, int instructions)
    : address_(address),
      instructions_(instructions),
      size_(instructions * Assembler::kInstrSize),
      masm_(address, size_ + Assembler::kGap) {
  // Create a new macro assembler pointing to the address of the code to patch.
  // The size is adjusted with kGap on order for the assembler to generate size
  // bytes of instructions without failing with buffer size constraints.
  ASSERT(masm_.reloc_info_writer.pos() == address_ + size_ + Assembler::kGap);
}


CodePatcher::~CodePatcher() {
  // Indicate that code has changed.
  CPU::FlushICache(address_, size_);

  // Check that the code was patched as expected.
  ASSERT(masm_.pc_ == address_ + size_);
  ASSERT(masm_.reloc_info_writer.pos() == address_ + size_ + Assembler::kGap);
}


void CodePatcher::Emit(Instr x) {
  masm()->emit(x);
}


void CodePatcher::Emit(Address addr) {
  masm()->emit(reinterpret_cast<Instr>(addr));
}
#endif  // ENABLE_DEBUGGER_SUPPORT


} }  // namespace v8::internal

#endif  // V8_TARGET_ARCH_MIPS
