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

#include "codegen-inl.h"
#include "register-allocator-inl.h"
#include "scopes.h"
#include "virtual-frame-inl.h"

namespace v8 {
namespace internal {

#define __ ACCESS_MASM(masm())

void VirtualFrame::PopToA1A0() {
  // Shuffle things around so the top of stack is in a0 and a1.
  MergeTOSTo(A0_A1_TOS);
  // Pop the two registers off the stack so they are detached from the frame.
  LowerHeight(2);
  top_of_stack_state_ = NO_TOS_REGISTERS;
}


void VirtualFrame::PopToA1() {
  // Shuffle things around so the top of stack is only in a1.
  MergeTOSTo(A1_TOS);
  // Pop the register off the stack so it is detached from the frame.
  LowerHeight(1);
  top_of_stack_state_ = NO_TOS_REGISTERS;
}


void VirtualFrame::PopToA0() {
  // Shuffle things around so the top of stack only in a0.
  MergeTOSTo(A0_TOS);
  // Pop the register off the stack so it is detached from the frame.
  LowerHeight(1);
  top_of_stack_state_ = NO_TOS_REGISTERS;
}

void VirtualFrame::MergeTo(const VirtualFrame* expected,
                           Condition cond,
                           Register r1,
                           const Operand& r2) {
  if (Equals(expected)) return;
  ASSERT(expected->IsCompatibleWith(this));
  MergeTOSTo(expected->top_of_stack_state_, cond, r1, r2);
  ASSERT(register_allocation_map_ == expected->register_allocation_map_);
}

void VirtualFrame::MergeTo(VirtualFrame* expected,
                           Condition cond,
                           Register r1,
                           const Operand& r2) {
  if (Equals(expected)) return;
  expected->tos_known_smi_map_ &= tos_known_smi_map_;
  MergeTOSTo(expected->top_of_stack_state_, cond, r1, r2);
  ASSERT(register_allocation_map_ == expected->register_allocation_map_);
}
void VirtualFrame::MergeTOSTo(
    VirtualFrame::TopOfStack expected_top_of_stack_state,
    Condition cond,
    Register r1,
    const Operand& r2) {
  Label merge_tos_to_end;
  if (cond != al) {
    __ Branch(&merge_tos_to_end, NegateCondition(cond), r1, r2);
  }

#define CASE_NUMBER(a, b) ((a) * TOS_STATES + (b))
  switch (CASE_NUMBER(top_of_stack_state_, expected_top_of_stack_state)) {
    case CASE_NUMBER(NO_TOS_REGISTERS, NO_TOS_REGISTERS):
      break;
    case CASE_NUMBER(NO_TOS_REGISTERS, A0_TOS):
      __ Pop(a0);
      break;
    case CASE_NUMBER(NO_TOS_REGISTERS, A1_TOS):
      __ Pop(a1);
      break;
    case CASE_NUMBER(NO_TOS_REGISTERS, A0_A1_TOS):
      __ Pop(a0);
      __ Pop(a1);
      break;
    case CASE_NUMBER(NO_TOS_REGISTERS, A1_A0_TOS):
      __ Pop(a1);
      __ Pop(a0);
      break;
    case CASE_NUMBER(A0_TOS, NO_TOS_REGISTERS):
      __ Push(a0);
      break;
    case CASE_NUMBER(A0_TOS, A0_TOS):
      break;
    case CASE_NUMBER(A0_TOS, A1_TOS):
      __ mov(a1, a0);
      break;
    case CASE_NUMBER(A0_TOS, A0_A1_TOS):
      __ Pop(a1);
      break;
    case CASE_NUMBER(A0_TOS, A1_A0_TOS):
      __ mov(a1, a0);
      __ Pop(a0);
      break;
    case CASE_NUMBER(A1_TOS, NO_TOS_REGISTERS):
      __ Push(a1);
      break;
    case CASE_NUMBER(A1_TOS, A0_TOS):
      __ mov(a0, a1);
      break;
    case CASE_NUMBER(A1_TOS, A1_TOS):
      break;
    case CASE_NUMBER(A1_TOS, A0_A1_TOS):
      __ mov(a0, a1);
      __ Pop(a1);
      break;
    case CASE_NUMBER(A1_TOS, A1_A0_TOS):
      __ Pop(a0);
      break;
    case CASE_NUMBER(A0_A1_TOS, NO_TOS_REGISTERS):
      __ Push(a1);
      __ Push(a0);
      break;
    case CASE_NUMBER(A0_A1_TOS, A0_TOS):
      __ Push(a1);
      break;
    case CASE_NUMBER(A0_A1_TOS, A1_TOS):
      __ Push(a1);
      __ mov(a1, a0);
      break;
    case CASE_NUMBER(A0_A1_TOS, A0_A1_TOS):
      break;
    case CASE_NUMBER(A0_A1_TOS, A1_A0_TOS):
      __ Swap(a0, a1, at);
      break;
    case CASE_NUMBER(A1_A0_TOS, NO_TOS_REGISTERS):
      __ Push(a0);
      __ Push(a1);
      break;
    case CASE_NUMBER(A1_A0_TOS, A0_TOS):
      __ Push(a0);
      __ mov(a0, a1);
      break;
    case CASE_NUMBER(A1_A0_TOS, A1_TOS):
      __ Push(a0);
      break;
    case CASE_NUMBER(A1_A0_TOS, A0_A1_TOS):
      __ Swap(a0, a1, at);
      break;
    case CASE_NUMBER(A1_A0_TOS, A1_A0_TOS):
      break;
    default:
      UNREACHABLE();
#undef CASE_NUMBER
  }

  __ bind(&merge_tos_to_end);
  if (cond == al) {
    // A conditional merge will be followed by a conditional branch and the
    // fall-through code will have an unchanged virtual frame state.  If the
    // merge is unconditional ('al'ways) then it might be followed by a fall
    // through.  We need to update the virtual frame state to match the code we
    // are falling into.  The final case is an unconditional merge followed by
    // an unconditional branch, in which case it doesn't matter what we do to
    // the virtual frame state, because the virtual frame will be invalidated.
    top_of_stack_state_ = expected_top_of_stack_state;
  }
}


void VirtualFrame::Enter() {
  Comment cmnt(masm(), "[ Enter JS frame");

#ifdef DEBUG
  // Verify that a1 contains a JS function.  The following code relies
  // on a2 being available for use.
  if (FLAG_debug_code) {
    Label map_check, done;
    __ BranchOnNotSmi(a1, &map_check, t1);
    __ stop("VirtualFrame::Enter - a1 is not a function (smi check).");
    __ bind(&map_check);
    __ GetObjectType(a1, a2, a2);
    __ Branch(&done, eq, a2, Operand(JS_FUNCTION_TYPE));
    __ stop("VirtualFrame::Enter - a1 is not a function (map check).");
    __ bind(&done);
  }
#endif  // DEBUG

  // We are about to push four values to the frame.
  Adjust(4);
  __ Push(ra, fp, cp, a1);
  // Adjust FP to point to saved FP.
  __ addiu(fp, sp, 2 * kPointerSize);
}


void VirtualFrame::Exit() {
  Comment cmnt(masm(), "[ Exit JS frame");
  // Record the location of the JS exit code for patching when setting
  // break point.
  __ RecordJSReturn();

  // Drop the execution stack down to the frame pointer and restore the caller
  // frame pointer and return address.
  __ mov(sp, fp);
  __ lw(fp, MemOperand(sp, 0));
  __ lw(ra, MemOperand(sp, 4));
  __ addiu(sp, sp, 8);
}


void VirtualFrame::AllocateStackSlots() {
  int count = local_count();
  __ LoadRoot(t2, Heap::kStackLimitRootIndex);
  if (count > 0) {
    Comment cmnt(masm(), "[ Allocate space for locals");
    Adjust(count);
    // Initialize stack slots with 'undefined' value.
    __ LoadRoot(t0, Heap::kUndefinedValueRootIndex);
    __ addiu(sp, sp, -count * kPointerSize);
    if (count < kLocalVarBound) {
      // For less locals the unrolled loop is more compact.
      for (int i = 0; i < count; i++) {
        __ sw(t0, MemOperand(sp, (count-i-1)*kPointerSize));
      }
    } else {
      // For more locals a loop in generated code is more compact.
      Label alloc_locals_loop;
      __ li(a1, Operand(count));
      __ mov(a2, sp);
      __ bind(&alloc_locals_loop);
      __ sw(t0, MemOperand(a2, 0));
      __ Subu(a1, a1, Operand(1));
      __ Branch(false, &alloc_locals_loop, gt, a1, Operand(zero_reg));
      __ Addu(a2, a2, Operand(kPointerSize));  // Use branch-delay slot.
    }
  }
  // Call the stub if lower.
  Label stack_ok;
  __ Branch(&stack_ok, Uless, t2, Operand(sp));
  StackCheckStub stub;
  __ Push(ra);
  __ Call(Operand(reinterpret_cast<intptr_t>(stub.GetCode().location()),
          RelocInfo::CODE_TARGET));
  __ Pop(ra);
  __ bind(&stack_ok);
}



void VirtualFrame::PushReceiverSlotAddress() {
  UNIMPLEMENTED_MIPS();
}


void VirtualFrame::PushTryHandler(HandlerType type) {
  // Grow the expression stack by handler size less one (the return
  // address in lr is already counted by a call instruction).
  Adjust(kHandlerSize - 1);
  __ PushTryHandler(IN_JAVASCRIPT, type);
}


void VirtualFrame::CallJSFunction(int arg_count) {
  // InvokeFunction requires function in a1.
  PopToA1();
  SpillAll();

  // +1 for receiver.
  Forget(arg_count + 1);
  ASSERT(cgen()->HasValidEntryRegisters());
  ParameterCount count(arg_count);
  __ InvokeFunction(a1, count, CALL_FUNCTION);
  // Restore the context.
  __ lw(cp, Context());
}


void VirtualFrame::CallRuntime(Runtime::Function* f, int arg_count) {
  SpillAll();
  Forget(arg_count);
  ASSERT(cgen()->HasValidEntryRegisters());
  __ CallRuntime(f, arg_count);
}


void VirtualFrame::CallRuntime(Runtime::FunctionId id, int arg_count) {
  SpillAll();
  Forget(arg_count);
  ASSERT(cgen()->HasValidEntryRegisters());
  __ CallRuntime(id, arg_count);
}


#ifdef ENABLE_DEBUGGER_SUPPORT
void VirtualFrame::DebugBreak() {
  ASSERT(cgen()->HasValidEntryRegisters());
  __ DebugBreak();
}
#endif


void VirtualFrame::InvokeBuiltin(Builtins::JavaScript id,
                                 InvokeJSFlags flags,
                                 int arg_count) {
  Forget(arg_count);
  __ InvokeBuiltin(id, flags);
}


void VirtualFrame::CallLoadIC(Handle<String> name, RelocInfo::Mode mode) {
  Handle<Code> ic(Builtins::builtin(Builtins::LoadIC_Initialize));
  PopToA0();
  SpillAll();
  __ li(a2, Operand(name));
  CallCodeObject(ic, mode, 0);
}


void VirtualFrame::CallStoreIC(Handle<String> name, bool is_contextual) {
  Handle<Code> ic(Builtins::builtin(Builtins::StoreIC_Initialize));
  PopToA0();
  if (is_contextual) {
    SpillAll();
    __ lw(a1, MemOperand(cp, Context::SlotOffset(Context::GLOBAL_INDEX)));
  } else {
    EmitPop(a1);
    SpillAll();
  }
  __ li(a2, Operand(name));
  CallCodeObject(ic, RelocInfo::CODE_TARGET, 0);
}


void VirtualFrame::CallKeyedLoadIC() {
  Handle<Code> ic(Builtins::builtin(Builtins::KeyedLoadIC_Initialize));
  PopToA1A0();
  SpillAll();
  CallCodeObject(ic, RelocInfo::CODE_TARGET, 0);
}


void VirtualFrame::CallKeyedStoreIC() {
  Handle<Code> ic(Builtins::builtin(Builtins::KeyedStoreIC_Initialize));
  PopToA1A0();
  SpillAll();
  EmitPop(a2);
  CallCodeObject(ic, RelocInfo::CODE_TARGET, 0);
}


void VirtualFrame::CallCodeObject(Handle<Code> code,
                                  RelocInfo::Mode rmode,
                                  int dropped_args) {
  switch (code->kind()) {
    case Code::CALL_IC:
    case Code::KEYED_CALL_IC:
      break;

    case Code::FUNCTION:
      UNIMPLEMENTED_MIPS();
      __ break_(__LINE__);
      break;

    case Code::KEYED_LOAD_IC:
    case Code::LOAD_IC:
    case Code::KEYED_STORE_IC:
    case Code::STORE_IC:
      ASSERT(dropped_args == 0);
      break;

    case Code::BUILTIN:
      ASSERT(*code == Builtins::builtin(Builtins::JSConstructCall));
      break;

    default:
      UNREACHABLE();
      break;
  }
  Forget(dropped_args);
  ASSERT(cgen()->HasValidEntryRegisters());
  __ Call(code, rmode);
}


//    NO_TOS_REGISTERS, A0_TOS, A1_TOS, A1_A0_TOS, A0_A1_TOS.
const bool VirtualFrame::kA0InUse[TOS_STATES] =
    { false,            true,   false,  true,      true };
const bool VirtualFrame::kA1InUse[TOS_STATES] =
    { false,            false,  true,   true,      true };
const int VirtualFrame::kVirtualElements[TOS_STATES] =
    { 0,                1,      1,      2,         2 };
const Register VirtualFrame::kTopRegister[TOS_STATES] =
    { a0,               a0,     a1,     a1,        a0 };
const Register VirtualFrame::kBottomRegister[TOS_STATES] =
    { a0,               a0,     a1,     a0,        a1 };
const Register VirtualFrame::kAllocatedRegisters[
    VirtualFrame::kNumberOfAllocatedRegisters] = { a2, a3, t0, t1, t2 };
// Popping is done by the transition implied by kStateAfterPop.  Of course if
// there were no stack slots allocated to registers then the physical SP must
// be adjusted.
const VirtualFrame::TopOfStack VirtualFrame::kStateAfterPop[TOS_STATES] =
    { NO_TOS_REGISTERS, NO_TOS_REGISTERS, NO_TOS_REGISTERS, A0_TOS, A1_TOS };
// Pushing is done by the transition implied by kStateAfterPush.  Of course if
// the maximum number of registers was already allocated to the top of stack
// slots then one register must be physically pushed onto the stack.
const VirtualFrame::TopOfStack VirtualFrame::kStateAfterPush[TOS_STATES] =
    { A0_TOS, A1_A0_TOS, A0_A1_TOS, A0_A1_TOS, A1_A0_TOS };


bool VirtualFrame::SpilledScope::is_spilled_ = false;


void VirtualFrame::Drop(int count) {
  ASSERT(count >= 0);
  ASSERT(height() >= count);
  // Discard elements from the virtual frame and free any registers.
  int num_virtual_elements = kVirtualElements[top_of_stack_state_];
  while (num_virtual_elements > 0) {
    Pop();
    num_virtual_elements--;
    count--;
    if (count == 0) return;
  }
  if (count == 0) return;
  __ Addu(sp, sp, Operand(count * kPointerSize));
  LowerHeight(count);
}


void VirtualFrame::Pop() {
  if (top_of_stack_state_ == NO_TOS_REGISTERS) {
    __ Addu(sp, sp, Operand(kPointerSize));
  } else {
    top_of_stack_state_ = kStateAfterPop[top_of_stack_state_];
  }
  LowerHeight(1);
}


void VirtualFrame::EmitPop(Register reg) {
  ASSERT(!is_used(RegisterAllocator::ToNumber(reg)));
  if (top_of_stack_state_ == NO_TOS_REGISTERS) {
    __ Pop(reg);
  } else {
    __ mov(reg, kTopRegister[top_of_stack_state_]);
    top_of_stack_state_ = kStateAfterPop[top_of_stack_state_];
  }
  LowerHeight(1);
}


void VirtualFrame::SpillAllButCopyTOSToA0() {
  switch (top_of_stack_state_) {
    case NO_TOS_REGISTERS:
      __ lw(a0, MemOperand(sp, 0));
      break;
    case A0_TOS:
      __ Push(a0);
      break;
    case A1_TOS:
      __ Push(a1);
      __ mov(a0, a1);
      break;
    case A0_A1_TOS:
      __ Push(a1, a0);
      break;
    case A1_A0_TOS:
      __ Push(a0, a1);
      __ mov(a0, a1);
      break;
    default:
      UNREACHABLE();
  }
  top_of_stack_state_ = NO_TOS_REGISTERS;
}

void VirtualFrame::SpillAllButCopyTOSToA1() {
  switch (top_of_stack_state_) {
    case NO_TOS_REGISTERS:
      __ lw(a1, MemOperand(sp, 0));
      break;
    case A0_TOS:
      __ Push(a0);
      __ mov(a1, a0);
      break;
    case A1_TOS:
      __ Push(a1);
      break;
    case A0_A1_TOS:
      __ Push(a1, a0);
      __ mov(a1, a0);
      break;
    case A1_A0_TOS:
      __ Push(a0, a1);
      break;
    default:
      UNREACHABLE();
  }
  top_of_stack_state_ = NO_TOS_REGISTERS;
}


void VirtualFrame::SpillAllButCopyTOSToA1A0() {
  switch (top_of_stack_state_) {
    case NO_TOS_REGISTERS:
      __ lw(a1, MemOperand(sp, 0));
      __ lw(a0, MemOperand(sp, kPointerSize));
      break;
    case A0_TOS:
      __ Push(a0);
      __ mov(a1, a0);
      __ lw(a0, MemOperand(sp, kPointerSize));
      break;
    case A1_TOS:
      __ Push(a1);
      __ lw(a0, MemOperand(sp, kPointerSize));
      break;
    case A0_A1_TOS:
      __ Push(a1, a0);
      __ Swap(a0, a1, at);
      break;
    case A1_A0_TOS:
      __ Push(a0, a1);
      break;
    default:
      UNREACHABLE();
  }
  top_of_stack_state_ = NO_TOS_REGISTERS;
}


Register VirtualFrame::Peek() {
  AssertIsNotSpilled();
  if (top_of_stack_state_ == NO_TOS_REGISTERS) {
    top_of_stack_state_ = kStateAfterPush[top_of_stack_state_];
    Register answer = kTopRegister[top_of_stack_state_];
    __ Pop(answer);
    return answer;
  } else {
    return kTopRegister[top_of_stack_state_];
  }
}

Register VirtualFrame::Peek2() {
  AssertIsNotSpilled();
  switch (top_of_stack_state_) {
    case NO_TOS_REGISTERS:
    case A0_TOS:
    case A0_A1_TOS:
      MergeTOSTo(A0_A1_TOS);
      return a1;
    case A1_TOS:
    case A1_A0_TOS:
      MergeTOSTo(A1_A0_TOS);
      return a0;
    default:
      UNREACHABLE();
      return no_reg;
  }
}


void VirtualFrame::Dup() {
  if (SpilledScope::is_spilled()) {
    __ lw(at, MemOperand(sp, 0));
    __ Push(at);
  } else {
    switch (top_of_stack_state_) {
      case NO_TOS_REGISTERS:
        __ lw(a0, MemOperand(sp, 0));
        top_of_stack_state_ = A0_TOS;
        break;
      case A0_TOS:
        __ mov(a1, a0);
        // a0 and a1 contains the same value.
        // Prefer a state with a0 holding TOS.
        top_of_stack_state_ = A0_A1_TOS;
        break;
      case A1_TOS:
        __ mov(a0, a1);
        // a0 and a1 contains the same value.
        // Prefer a state with a0 holding TOS.
        top_of_stack_state_ = A0_A1_TOS;
        break;
      case A0_A1_TOS:
        __ Push(a1);
        __ mov(a1, a0);
        // a0 and a1 contains the same value.
        // Prefer a state with a0 holding TOS.
        top_of_stack_state_ = A0_A1_TOS;
        break;
      case A1_A0_TOS:
        __ Push(a0);
        __ mov(a0, a1);
        // a0 and a1 contains the same value.
        // Prefer a state with a0 holding TOS.
        top_of_stack_state_ = A0_A1_TOS;
        break;
      default:
        UNREACHABLE();
    }
  }
  RaiseHeight(1, tos_known_smi_map_ & 1);
}


void VirtualFrame::Dup2() {
  if (SpilledScope::is_spilled()) {
    __ lw(t0, MemOperand(sp, kPointerSize));
    __ Push(t0);
    __ lw(t0, MemOperand(sp, kPointerSize));
    __ Push(t0);
  } else {
    switch (top_of_stack_state_) {
      case NO_TOS_REGISTERS:
        __ lw(a0, MemOperand(sp, 0));
        __ lw(a1, MemOperand(sp, kPointerSize));
        top_of_stack_state_ = A0_A1_TOS;
        break;
      case A0_TOS:
        __ Push(a0);
        __ lw(a1, MemOperand(sp, kPointerSize));
        top_of_stack_state_ = A0_A1_TOS;
        break;
      case A1_TOS:
        __ Push(a1);
        __ lw(a0, MemOperand(sp, kPointerSize));
        top_of_stack_state_ = A1_A0_TOS;
        break;
      case A0_A1_TOS:
        __ Push(a1, a0);
        top_of_stack_state_ = A0_A1_TOS;
        break;
      case A1_A0_TOS:
        __ Push(a0, a1);
        top_of_stack_state_ = A1_A0_TOS;
        break;
      default:
        UNREACHABLE();
    }
  }
  RaiseHeight(2, tos_known_smi_map_ & 3);
}

Register VirtualFrame::PopToRegister(Register but_not_to_this_one) {
  ASSERT(but_not_to_this_one.is(a0) ||
         but_not_to_this_one.is(a1) ||
         but_not_to_this_one.is(no_reg));
  LowerHeight(1);
  if (top_of_stack_state_ == NO_TOS_REGISTERS) {
    if (but_not_to_this_one.is(a0)) {
      __ Pop(a1);
      return a1;
    } else {
      __ Pop(a0);
      return a0;
    }
  } else {
    Register answer = kTopRegister[top_of_stack_state_];
    ASSERT(!answer.is(but_not_to_this_one));
    top_of_stack_state_ = kStateAfterPop[top_of_stack_state_];
    return answer;
  }
}


void VirtualFrame::EnsureOneFreeTOSRegister() {
  if (kVirtualElements[top_of_stack_state_] == kMaxTOSRegisters) {
    __ push(kBottomRegister[top_of_stack_state_]);
    top_of_stack_state_ = kStateAfterPush[top_of_stack_state_];
    top_of_stack_state_ = kStateAfterPop[top_of_stack_state_];
  }
  ASSERT(kVirtualElements[top_of_stack_state_] != kMaxTOSRegisters);
}

void VirtualFrame::EmitMultiPop(RegList regs) {
  for (int16_t i = 0; i < kNumRegisters; i++) {
    if ((regs & (1 << i)) != 0) {
      element_count_--;
    }
  }
  __ MultiPop(regs);
}


void VirtualFrame::EmitPush(Register reg, TypeInfo info) {
  RaiseHeight(1, info.IsSmi() ? 1 : 0);
  if (reg.is(cp)) {
    // If we are pushing cp then we are about to make a call and things have to
    // be pushed to the physical stack.  There's nothing to be gained my moving
    // to a TOS register and then pushing that, we might as well push to the
    // physical stack immediately.
    MergeTOSTo(NO_TOS_REGISTERS);
    __ push(reg);
    return;
  }
  if (SpilledScope::is_spilled()) {
    ASSERT(top_of_stack_state_ == NO_TOS_REGISTERS);
    __ Push(reg);
    return;
  }
  if (top_of_stack_state_ == NO_TOS_REGISTERS) {
    if (reg.is(a0)) {
      top_of_stack_state_ = A0_TOS;
      return;
    }
    if (reg.is(a1)) {
      top_of_stack_state_ = A1_TOS;
      return;
    }
  }
  EnsureOneFreeTOSRegister();
  top_of_stack_state_ = kStateAfterPush[top_of_stack_state_];
  Register dest = kTopRegister[top_of_stack_state_];
  __ Move(dest, reg);
}

void VirtualFrame::SetElementAt(Register reg, int this_far_down) {
  if (this_far_down < kTOSKnownSmiMapSize) {
    tos_known_smi_map_ &= ~(1 << this_far_down);
  }
  if (this_far_down == 0) {
    Pop();
    Register dest = GetTOSRegister();
    if (dest.is(reg)) {
      // We already popped one item off the top of the stack.  If the only
      // free register is the one we were asked to push then we have been
      // asked to push a register that was already in use, which cannot
      // happen.  It therefore folows that there are two free TOS registers:
      ASSERT(top_of_stack_state_ == NO_TOS_REGISTERS);
      dest = dest.is(a0) ? a1 : a0;
    }
    __ mov(dest, reg);
    EmitPush(dest);
  } else if (this_far_down == 1) {
    int virtual_elements = kVirtualElements[top_of_stack_state_];
    if (virtual_elements < 2) {
      __ sw(reg, ElementAt(this_far_down));
    } else {
      ASSERT(virtual_elements == 2);
      ASSERT(!reg.is(a0));
      ASSERT(!reg.is(a1));
      Register dest = kBottomRegister[top_of_stack_state_];
      __ mov(dest, reg);
    }
  } else {
    ASSERT(this_far_down >= 2);
    ASSERT(kVirtualElements[top_of_stack_state_] <= 2);
    __ sw(reg, ElementAt(this_far_down));
  }
}

Register VirtualFrame::GetTOSRegister() {
  if (SpilledScope::is_spilled()) return a0;

  EnsureOneFreeTOSRegister();
  return kTopRegister[kStateAfterPush[top_of_stack_state_]];
}


void VirtualFrame::EmitPush(Operand operand, TypeInfo info) {
  RaiseHeight(1, info.IsSmi() ? 1 : 0);
  if (SpilledScope::is_spilled()) {
    __ li(a0, operand);
    __ Push(a0);
    return;
  }
  EnsureOneFreeTOSRegister();
  top_of_stack_state_ = kStateAfterPush[top_of_stack_state_];
  __ li(kTopRegister[top_of_stack_state_], operand);
}


void VirtualFrame::EmitPush(MemOperand operand, TypeInfo info) {
  RaiseHeight(1, info.IsSmi() ? 1 : 0);
  if (SpilledScope::is_spilled()) {
    __ lw(a0, operand);
    __ Push(a0);
    return;
  }
  EnsureOneFreeTOSRegister();
  top_of_stack_state_ = kStateAfterPush[top_of_stack_state_];
  __ lw(kTopRegister[top_of_stack_state_], operand);
}


void VirtualFrame::EmitPushRoot(Heap::RootListIndex index) {
  RaiseHeight(1, 0);
  if (SpilledScope::is_spilled()) {
    __ LoadRoot(a0, index);
    __ Push(a0);
    return;
  }
  EnsureOneFreeTOSRegister();
  top_of_stack_state_ = kStateAfterPush[top_of_stack_state_];
  __ LoadRoot(kTopRegister[top_of_stack_state_], index);
}


void VirtualFrame::EmitMultiPush(RegList regs) {
  for (int16_t i = kNumRegisters; i > 0; i--) {
    if ((regs & (1 << i)) != 0) {
      element_count_++;
    }
  }
  __ MultiPush(regs);
}


void VirtualFrame::EmitMultiPushReversed(RegList regs) {
  for (int16_t i = 0; i< kNumRegisters; i++) {
    if ((regs & (1<<i)) != 0) {
      element_count_++;
    }
  }
  __ MultiPushReversed(regs);
}


void VirtualFrame::SpillAll() {
  switch (top_of_stack_state_) {
    case A1_A0_TOS:
      masm()->push(a0);
      // Fall through.
    case A1_TOS:
      masm()->push(a1);
      top_of_stack_state_ = NO_TOS_REGISTERS;
      break;
    case A0_A1_TOS:
      masm()->push(a1);
      // Fall through.
    case A0_TOS:
      masm()->push(a0);
      top_of_stack_state_ = NO_TOS_REGISTERS;
      // Fall through.
    case NO_TOS_REGISTERS:
      break;
    default:
      UNREACHABLE();
      break;
  }
  ASSERT(register_allocation_map_ == 0);  // Not yet implemented.
}

#undef __

} }  // namespace v8::internal

#endif  // V8_TARGET_ARCH_MIPS
