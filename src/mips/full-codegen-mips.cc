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

// Note on Mips implementation:
//
// The result_register() for mips is the 'v0' register, which is defined
// by the ABI to contain function return values. However, the first
// parameter to a function is defined to be 'a0'. So there are many
// places where we have to move a previous result in v0 to a0 for the
// next call: mov(a0, v0). This is not needed on the other architectures.

#include "code-stubs.h"
#include "codegen-inl.h"
#include "compiler.h"
#include "debug.h"
#include "full-codegen.h"
#include "parser.h"
#include "scopes.h"
#include "stub-cache.h"

namespace v8 {
namespace internal {

#define __ ACCESS_MASM(masm_)

// Generate code for a JS function.  On entry to the function the receiver
// and arguments have been pushed on the stack left to right.  The actual
// argument count matches the formal parameter count expected by the
// function.
//
// The live registers are:
//   o a1: the JS function object being called (ie, ourselves)
//   o cp: our context
//   o fp: our caller's frame pointer
//   o sp: stack pointer
//   o ra: return address
//
// The function builds a JS frame.  Please see JavaScriptFrameConstants in
// frames-mips.h for its layout.
void FullCodeGenerator::Generate(CompilationInfo* info) {
  ASSERT(info_ == NULL);
  info_ = info;
  SetFunctionPosition(function());
  Comment cmnt(masm_, "[ function compiled by full code generator");

#ifdef DEBUG
  if (strlen(FLAG_stop_at) > 0 &&
      info->function()->name()->IsEqualTo(CStrVector(FLAG_stop_at))) {
    __ stop("stop-at");
  }
#endif

  int locals_count = scope()->num_stack_slots();

  __ Push(ra, fp, cp, a1);
  if (locals_count > 0) {
    // Load undefined value here, so the value is ready for the loop
    // below.
    __ LoadRoot(at, Heap::kUndefinedValueRootIndex);
  }
  // Adjust fp to point to caller's fp.
  __ Addu(fp, sp, Operand(2 * kPointerSize));

  { Comment cmnt(masm_, "[ Allocate locals");
    for (int i = 0; i < locals_count; i++) {
      __ push(at);
    }
  }

  bool function_in_register = true;

  // Possibly allocate a local context.
  int heap_slots = scope()->num_heap_slots() - Context::MIN_CONTEXT_SLOTS;
  if (heap_slots > 0) {
    Comment cmnt(masm_, "[ Allocate local context");
    // Argument to NewContext is the function, which is in a1.
    __ push(a1);
    if (heap_slots <= FastNewContextStub::kMaximumSlots) {
      FastNewContextStub stub(heap_slots);
      __ CallStub(&stub);
    } else {
      __ CallRuntime(Runtime::kNewContext, 1);
    }
    function_in_register = false;
    // Context is returned in both v0 and cp.  It replaces the context
    // passed to us.  It's saved in the stack and kept live in cp.
    __ sw(cp, MemOperand(fp, StandardFrameConstants::kContextOffset));
    // Copy any necessary parameters into the context.
    int num_parameters = scope()->num_parameters();
    for (int i = 0; i < num_parameters; i++) {
      Slot* slot = scope()->parameter(i)->AsSlot();
      if (slot != NULL && slot->type() == Slot::CONTEXT) {
        int parameter_offset = StandardFrameConstants::kCallerSPOffset +
                                 (num_parameters - 1 - i) * kPointerSize;
        // Load parameter from stack.
        __ lw(a0, MemOperand(fp, parameter_offset));
        // Store it in the context.
        __ li(a1, Operand(Context::SlotOffset(slot->index())));
        __ addu(a2, cp, a1);
        __ sw(a0, MemOperand(a2, 0));
        // Update the write barrier. This clobbers all involved
        // registers, so we have to use two more registers to avoid
        // clobbering cp.
        __ mov(a2, cp);
        __ RecordWrite(a2, a1, a3);
      }
    }
  }

  Variable* arguments = scope()->arguments();
  if (arguments != NULL) {
    // Function uses arguments object.
    Comment cmnt(masm_, "[ Allocate arguments object");
    if (!function_in_register) {
      // Load this again, if it's used by the local context below.
      __ lw(a3, MemOperand(fp, JavaScriptFrameConstants::kFunctionOffset));
    } else {
      __ mov(a3, a1);
    }
    // Receiver is just before the parameters on the caller's stack.
    int offset = scope()->num_parameters() * kPointerSize;
    __ Addu(a2, fp,
           Operand(StandardFrameConstants::kCallerSPOffset + offset));
    __ li(a1, Operand(Smi::FromInt(scope()->num_parameters())));
    __ Push(a3, a2, a1);

    // Arguments to ArgumentsAccessStub:
    //   function, receiver address, parameter count.
    // The stub will rewrite receiever and parameter count if the previous
    // stack frame was an arguments adapter frame.
    ArgumentsAccessStub stub(ArgumentsAccessStub::NEW_OBJECT);
    __ CallStub(&stub);
    // Duplicate the value; move-to-slot operation might clobber registers.
    __ mov(a3, v0);
    Move(arguments->AsSlot(), v0, a1, a2);
    Slot* dot_arguments_slot = scope()->arguments_shadow()->AsSlot();
    Move(dot_arguments_slot, a3, a1, a2);
  }


  { Comment cmnt(masm_, "[ Declarations");
    // For named function expressions, declare the function name as a
    // constant.
    if (scope()->is_function_scope() && scope()->function() != NULL) {
      EmitDeclaration(scope()->function(), Variable::CONST, NULL);
    }
    // Visit all the explicit declarations unless there is an illegal
    // redeclaration.
    if (scope()->HasIllegalRedeclaration()) {
      scope()->VisitIllegalRedeclaration(this);
    } else {
      VisitDeclarations(scope()->declarations());
    }
  }

  // Check the stack for overflow or break request.
  { Comment cmnt(masm_, "[ Stack check");
    __ LoadRoot(t0, Heap::kStackLimitRootIndex);
    StackCheckStub stub;
    // Call the stub if lower.
    __ Push(ra);
    __ Call(Operand(reinterpret_cast<intptr_t>(stub.GetCode().location()),
            RelocInfo::CODE_TARGET),
            Uless, sp, Operand(t0));
    __ Pop(ra);
  }

  if (FLAG_trace) {
    __ CallRuntime(Runtime::kTraceEnter, 0);
  }

  { Comment cmnt(masm_, "[ Body");
    ASSERT(loop_depth() == 0);
    VisitStatements(function()->body());
    ASSERT(loop_depth() == 0);
  }

  { Comment cmnt(masm_, "[ return <undefined>;");
    // Emit a 'return undefined' in case control fell off the end of the
    // body.
    __ LoadRoot(v0, Heap::kUndefinedValueRootIndex);
  }
  EmitReturnSequence();
}


void FullCodeGenerator::EmitReturnSequence() {
  Comment cmnt(masm_, "[ Return sequence");
  if (return_label_.is_bound()) {
    __ Branch(&return_label_);
  } else {
    __ bind(&return_label_);
    if (FLAG_trace) {
      // Push the return value on the stack as the parameter.
      // Runtime::TraceExit returns its parameter in v0.
      __ push(v0);
      __ CallRuntime(Runtime::kTraceExit, 1);
    }

#ifdef DEBUG
    // Add a label for checking the size of the code used for returning.
    Label check_exit_codesize;
    masm_->bind(&check_exit_codesize);
#endif
    // Make sure that the constant pool is not emitted inside of the return
    // sequence.
    { Assembler::BlockTrampolinePoolScope block_trampoline_pool(masm_);
      // Here we use masm_-> instead of the __ macro to avoid the code coverage
      // tool from instrumenting as we rely on the code size here.
      int32_t sp_delta = (scope()->num_parameters() + 1) * kPointerSize;
      CodeGenerator::RecordPositions(masm_, function()->end_position() - 1);
      __ RecordJSReturn();
      masm_->mov(sp, fp);
      masm_->MultiPop(static_cast<RegList>(fp.bit() | ra.bit()));
      masm_->Addu(sp, sp, Operand(sp_delta));
      masm_->Jump(ra);
    }

#ifdef DEBUG
    // Check that the size of the code used for returning matches what is
    // expected by the debugger. If the sp_delts above cannot be encoded in the
    // add instruction the add will generate two instructions.
    int return_sequence_length =
        masm_->InstructionsGeneratedSince(&check_exit_codesize);
    CHECK(return_sequence_length ==
          Assembler::kJSReturnSequenceInstructions ||
          return_sequence_length ==
          Assembler::kJSReturnSequenceInstructions + 1);
#endif
  }
}


FullCodeGenerator::ConstantOperand FullCodeGenerator::GetConstantOperand(
    Token::Value op, Expression* left, Expression* right) {
  ASSERT(ShouldInlineSmiCase(op));
  return kNoConstants;
}


void FullCodeGenerator::EffectContext::Plug(Slot* slot) const {
}


void FullCodeGenerator::AccumulatorValueContext::Plug(Slot* slot) const {
  codegen()->Move(result_register(), slot);
}


void FullCodeGenerator::StackValueContext::Plug(Slot* slot) const {
  codegen()->Move(result_register(), slot);
  __ push(result_register());
}


void FullCodeGenerator::TestContext::Plug(Slot* slot) const {
  // For simplicity we always test the accumulator register.
  codegen()->Move(result_register(), slot);
  codegen()->DoTest(true_label_, false_label_, fall_through_);
}


void FullCodeGenerator::EffectContext::Plug(Heap::RootListIndex index) const {
}


void FullCodeGenerator::AccumulatorValueContext::Plug(
    Heap::RootListIndex index) const {
  __ LoadRoot(result_register(), index);
}


void FullCodeGenerator::StackValueContext::Plug(
    Heap::RootListIndex index) const {
  __ LoadRoot(result_register(), index);
  __ push(result_register());
}


void FullCodeGenerator::TestContext::Plug(Heap::RootListIndex index) const {
  if (index == Heap::kUndefinedValueRootIndex ||
      index == Heap::kNullValueRootIndex ||
      index == Heap::kFalseValueRootIndex) {
    __ Branch(false_label_);
  } else if (index == Heap::kTrueValueRootIndex) {
    __ Branch(true_label_);
  } else {
    __ LoadRoot(result_register(), index);
    codegen()->DoTest(true_label_, false_label_, fall_through_);
  }
}


void FullCodeGenerator::EffectContext::Plug(Handle<Object> lit) const {
}


void FullCodeGenerator::AccumulatorValueContext::Plug(
    Handle<Object> lit) const {
  __ li(result_register(), Operand(lit));
}


void FullCodeGenerator::StackValueContext::Plug(Handle<Object> lit) const {
  // Immediates can be pushed directly.
  __ li(result_register(), Operand(lit));
  __ push(result_register());
}


void FullCodeGenerator::TestContext::Plug(Handle<Object> lit) const {
  ASSERT(!lit->IsUndetectableObject());  // There are no undetectable literals.
  if (lit->IsUndefined() || lit->IsNull() || lit->IsFalse()) {
    __ Branch(false_label_);
  } else if (lit->IsTrue() || lit->IsJSObject()) {
    __ Branch(true_label_);
  } else if (lit->IsString()) {
    if (String::cast(*lit)->length() == 0) {
      __ Branch(false_label_);
    } else {
      __ Branch(true_label_);
    }
  } else if (lit->IsSmi()) {
    if (Smi::cast(*lit)->value() == 0) {
      __ Branch(false_label_);
    } else {
      __ Branch(true_label_);
    }
  } else {
    // For simplicity we always test the accumulator register.
    __ li(result_register(), Operand(lit));
    codegen()->DoTest(true_label_, false_label_, fall_through_);
  }
}


void FullCodeGenerator::EffectContext::DropAndPlug(int count,
                                                   Register reg) const {
  ASSERT(count > 0);
  __ Drop(count);
}


void FullCodeGenerator::AccumulatorValueContext::DropAndPlug(
    int count,
    Register reg) const {
  ASSERT(count > 0);
  __ Drop(count);
  __ Move(result_register(), reg);
}


void FullCodeGenerator::StackValueContext::DropAndPlug(int count,
                                                       Register reg) const {
  ASSERT(count > 0);
  if (count > 1) __ Drop(count - 1);
  __ sw(reg, MemOperand(sp, 0));
}


void FullCodeGenerator::TestContext::DropAndPlug(int count,
                                                 Register reg) const {
  ASSERT(count > 0);
  // For simplicity we always test the accumulator register.
  __ Drop(count);
  __ Move(result_register(), reg);
  codegen()->DoTest(true_label_, false_label_, fall_through_);
}


void FullCodeGenerator::EffectContext::Plug(Label* materialize_true,
                                            Label* materialize_false) const {
  ASSERT_EQ(materialize_true, materialize_false);
  __ bind(materialize_true);
}


void FullCodeGenerator::AccumulatorValueContext::Plug(
    Label* materialize_true,
    Label* materialize_false) const {
  Label done;
  __ bind(materialize_true);
  __ LoadRoot(result_register(), Heap::kTrueValueRootIndex);
  __ Branch(&done);
  __ bind(materialize_false);
  __ LoadRoot(result_register(), Heap::kFalseValueRootIndex);
  __ bind(&done);
}


void FullCodeGenerator::StackValueContext::Plug(
    Label* materialize_true,
    Label* materialize_false) const {
  Label done;
  __ bind(materialize_true);
  __ LoadRoot(at, Heap::kTrueValueRootIndex);
  __ push(at);
  __ Branch(&done);
  __ bind(materialize_false);
  __ LoadRoot(at, Heap::kFalseValueRootIndex);
  __ push(at);
  __ bind(&done);
}


void FullCodeGenerator::TestContext::Plug(Label* materialize_true,
                                          Label* materialize_false) const {
  ASSERT(materialize_false == false_label_);
  ASSERT(materialize_true == true_label_);
}


void FullCodeGenerator::EffectContext::Plug(bool flag) const {
}


void FullCodeGenerator::AccumulatorValueContext::Plug(bool flag) const {
  Heap::RootListIndex value_root_index =
      flag ? Heap::kTrueValueRootIndex : Heap::kFalseValueRootIndex;
  __ LoadRoot(result_register(), value_root_index);
}


void FullCodeGenerator::StackValueContext::Plug(bool flag) const {
  Heap::RootListIndex value_root_index =
      flag ? Heap::kTrueValueRootIndex : Heap::kFalseValueRootIndex;
  __ LoadRoot(at, value_root_index);
  __ push(at);
}


void FullCodeGenerator::TestContext::Plug(bool flag) const {
  if (flag) {
    if (true_label_ != fall_through_) __ Branch(true_label_);
  } else {
    if (false_label_ != fall_through_) __ Branch(false_label_);
  }
}


void FullCodeGenerator::DoTest(Label* if_true,
                               Label* if_false,
                               Label* fall_through) {
  // Call the runtime to find the boolean value of the source and then
  // translate it into control flow to the pair of labels.
  __ push(result_register());
  __ CallRuntime(Runtime::kToBool, 1);
  __ LoadRoot(at, Heap::kTrueValueRootIndex);
  Split(eq, v0, Operand(at), if_true, if_false, fall_through);
}


void FullCodeGenerator::Split(Condition cc,
                              Register lhs,
                              const Operand&  rhs,
                              Label* if_true,
                              Label* if_false,
                              Label* fall_through) {
  if (if_false == fall_through) {
    __ Branch(if_true, cc, lhs, rhs);
  } else if (if_true == fall_through) {
    __ Branch(if_false, NegateCondition(cc), lhs, rhs);
  } else {
    __ Branch(cc, if_true, cc, lhs, rhs);
    __ Branch(if_false);
  }
}


MemOperand FullCodeGenerator::EmitSlotSearch(Slot* slot, Register scratch) {
  switch (slot->type()) {
    case Slot::PARAMETER:
    case Slot::LOCAL:
      return MemOperand(fp, SlotOffset(slot));
    case Slot::CONTEXT: {
      int context_chain_length =
          scope()->ContextChainLength(slot->var()->scope());
      __ LoadContext(scratch, context_chain_length);
      return ContextOperand(scratch, slot->index());
    }
    case Slot::LOOKUP:
      UNREACHABLE();
  }
  UNREACHABLE();
  return MemOperand(v0, 0);
}


void FullCodeGenerator::Move(Register destination, Slot* source) {
  // Use destination as scratch.
  MemOperand slot_operand = EmitSlotSearch(source, destination);
  __ lw(destination, slot_operand);
}


void FullCodeGenerator::Move(Slot* dst,
                             Register src,
                             Register scratch1,
                             Register scratch2) {
  ASSERT(dst->type() != Slot::LOOKUP);  // Not yet implemented.
  ASSERT(!scratch1.is(src) && !scratch2.is(src));
  MemOperand location = EmitSlotSearch(dst, scratch1);
  __ sw(src, location);
  // Emit the write barrier code if the location is in the heap.
  if (dst->type() == Slot::CONTEXT) {
    __ RecordWrite(scratch1,
                   Operand(Context::SlotOffset(dst->index())),
                   scratch2,
                   src);
  }
}


void FullCodeGenerator::EmitDeclaration(Variable* variable,
                                        Variable::Mode mode,
                                        FunctionLiteral* function) {
  Comment cmnt(masm_, "[ Declaration");
  ASSERT(variable != NULL);  // Must have been resolved.
  Slot* slot = variable->AsSlot();
  Property* prop = variable->AsProperty();

  if (slot != NULL) {
    switch (slot->type()) {
      case Slot::PARAMETER:
      case Slot::LOCAL:
        if (mode == Variable::CONST) {
          __ LoadRoot(t0, Heap::kTheHoleValueRootIndex);
          __ sw(t0, MemOperand(fp, SlotOffset(slot)));
        } else if (function != NULL) {
          VisitForAccumulatorValue(function);
          __ sw(result_register(), MemOperand(fp, SlotOffset(slot)));
        }
        break;

      case Slot::CONTEXT:
        // We bypass the general EmitSlotSearch because we know more about
        // this specific context.

        // The variable in the decl always resides in the current context.
        ASSERT_EQ(0, scope()->ContextChainLength(variable->scope()));
        if (FLAG_debug_code) {
          // Check if we have the correct context pointer.
          __ lw(a1, ContextOperand(cp, Context::FCONTEXT_INDEX));
          __ Check(eq, "Unexpected declaration in current context.",
                   a1, Operand(cp));
        }
        if (mode == Variable::CONST) {
          __ LoadRoot(at, Heap::kTheHoleValueRootIndex);
          __ sw(at, ContextOperand(cp, slot->index()));
          // No write barrier since the_hole_value is in old space.
        } else if (function != NULL) {
          VisitForAccumulatorValue(function);
          __ sw(result_register(), ContextOperand(cp, slot->index()));
          int offset = Context::SlotOffset(slot->index());
          // We know that we have written a function, which is not a smi.
          __ mov(a1, cp);
          __ RecordWrite(a1, Operand(offset), a2, result_register());
        }
        break;

      case Slot::LOOKUP: {
        __ li(a2, Operand(variable->name()));
        // Declaration nodes are always introduced in one of two modes.
        ASSERT(mode == Variable::VAR ||
               mode == Variable::CONST);
        PropertyAttributes attr =
            (mode == Variable::VAR) ? NONE : READ_ONLY;
        __ li(a1, Operand(Smi::FromInt(attr)));
        // Push initial value, if any.
        // Note: For variables we must not push an initial value (such as
        // 'undefined') because we may have a (legal) redeclaration and we
        // must not destroy the current value.
        if (mode == Variable::CONST) {
          __ LoadRoot(a0, Heap::kTheHoleValueRootIndex);
          __ Push(cp, a2, a1, a0);
        } else if (function != NULL) {
          __ Push(cp, a2, a1);
          // Push initial value for function declaration.
          VisitForStackValue(function);
        } else {
          ASSERT(Smi::FromInt(0) == 0);
          // No initial value!
          __ mov(a0, zero_reg);  // Operand(Smi::FromInt(0)));
          __ Push(cp, a2, a1, a0);
        }
        __ CallRuntime(Runtime::kDeclareContextSlot, 4);
        break;
      }
    }

  } else if (prop != NULL) {
    if (function != NULL || mode == Variable::CONST) {
      // We are declaring a function or constant that rewrites to a
      // property.  Use (keyed) IC to set the initial value.
      VisitForStackValue(prop->obj());
      if (function != NULL) {
        VisitForStackValue(prop->key());
        VisitForAccumulatorValue(function);
        __ mov(a0, result_register());
        __ pop(a1);  // Key.
      } else {
        VisitForAccumulatorValue(prop->key());
        __ mov(a1, result_register());  // Key.
        __ LoadRoot(a0, Heap::kTheHoleValueRootIndex);
      }
      __ pop(a2);  // Receiver.

      Handle<Code> ic(Builtins::builtin(Builtins::KeyedStoreIC_Initialize));
      EmitCallIC(ic, RelocInfo::CODE_TARGET);
      // Value in v0 is ignored (declarations are statements).
    }
  }
}


void FullCodeGenerator::VisitDeclaration(Declaration* decl) {
  EmitDeclaration(decl->proxy()->var(), decl->mode(), decl->fun());
}


void FullCodeGenerator::DeclareGlobals(Handle<FixedArray> pairs) {
  // Call the runtime to declare the globals.
  // The context is the first argument.
  __ li(a1, Operand(pairs));
  __ li(a0, Operand(Smi::FromInt(is_eval() ? 1 : 0)));
  __ Push(cp, a1, a0);
  __ CallRuntime(Runtime::kDeclareGlobals, 3);
  // Return value is ignored.
}


void FullCodeGenerator::VisitSwitchStatement(SwitchStatement* stmt) {
  Comment cmnt(masm_, "[ SwitchStatement");
  Breakable nested_statement(this, stmt);
  SetStatementPosition(stmt);
  // Keep the switch value on the stack until a case matches.
  VisitForStackValue(stmt->tag());

  ZoneList<CaseClause*>* clauses = stmt->cases();
  CaseClause* default_clause = NULL;  // Can occur anywhere in the list.

  Label next_test;  // Recycled for each test.
  // Compile all the tests with branches to their bodies.
  for (int i = 0; i < clauses->length(); i++) {
    CaseClause* clause = clauses->at(i);
    // The default is not a test, but remember it as final fall through.
    if (clause->is_default()) {
      default_clause = clause;
      continue;
    }

    Comment cmnt(masm_, "[ Case comparison");
    __ bind(&next_test);
    next_test.Unuse();

    // Compile the label expression.
    VisitForAccumulatorValue(clause->label());
    __ mov(a0, result_register());  // CompareStub requires args in a0, a1.

    // Perform the comparison as if via '==='.
    __ lw(a1, MemOperand(sp, 0));  // Switch value.
    bool inline_smi_code = ShouldInlineSmiCase(Token::EQ_STRICT);
    if (inline_smi_code) {
      Label slow_case;
      __ or_(a2, a1, a0);
      __ And(at, a2, Operand(kSmiTagMask));
      __ Branch(&slow_case, ne, at, Operand(zero_reg));
      __ Branch(&next_test, ne, a1, Operand(a0));
      __ Drop(1);  // Switch value is no longer needed.
      __ Branch(clause->body_target()->entry_label());

      __ bind(&slow_case);
    }

    CompareFlags flags = inline_smi_code
        ? NO_SMI_COMPARE_IN_STUB
        : NO_COMPARE_FLAGS;
    CompareStub stub(eq, true, flags, a1, a0);
    __ CallStub(&stub);
    __ Branch(&next_test, ne, v0, Operand(zero_reg));
    __ Drop(1);  // Switch value is no longer needed.
    __ Branch(clause->body_target()->entry_label());
  }

  // Discard the test value and jump to the default if present, otherwise to
  // the end of the statement.
  __ bind(&next_test);
  __ Drop(1);  // Switch value is no longer needed.
  if (default_clause == NULL) {
    __ Branch(nested_statement.break_target());
  } else {
    __ Branch(default_clause->body_target()->entry_label());
  }

  // Compile all the case bodies.
  for (int i = 0; i < clauses->length(); i++) {
    Comment cmnt(masm_, "[ Case body");
    CaseClause* clause = clauses->at(i);
    __ bind(clause->body_target()->entry_label());
    VisitStatements(clause->statements());
  }

  __ bind(nested_statement.break_target());
}


void FullCodeGenerator::VisitForInStatement(ForInStatement* stmt) {
  Comment cmnt(masm_, "[ ForInStatement");
  SetStatementPosition(stmt);

  Label loop, exit;
  ForIn loop_statement(this, stmt);
  increment_loop_depth();

  // Get the object to enumerate over. Both SpiderMonkey and JSC
  // ignore null and undefined in contrast to the specification; see
  // ECMA-262 section 12.6.4.
  VisitForAccumulatorValue(stmt->enumerable());
  __ mov(a0, result_register());  // Result as param to InvokeBuiltin below.
  __ LoadRoot(at, Heap::kUndefinedValueRootIndex);
  __ Branch(&exit, eq, a0, Operand(at));
  __ LoadRoot(at, Heap::kNullValueRootIndex);
  __ Branch(&exit, eq, a0, Operand(at));

  // Convert the object to a JS object.
  Label convert, done_convert;
  __ BranchOnSmi(a0, &convert);
  __ GetObjectType(a0, a1, a1);
  __ Branch(&done_convert, hs, a1, Operand(FIRST_JS_OBJECT_TYPE));
  __ bind(&convert);
  __ push(a0);
  __ InvokeBuiltin(Builtins::TO_OBJECT, CALL_JS);
  __ mov(a0, v0);
  __ bind(&done_convert);
  __ push(a0);

  // TODO(kasperl): Check cache validity in generated code. This is a
  // fast case for the JSObject::IsSimpleEnum cache validity
  // checks. If we cannot guarantee cache validity, call the runtime
  // system to check cache validity or get the property names in a
  // fixed array.

  // Get the set of properties to enumerate.
  __ push(a0);  // Duplicate the enumerable object on the stack.
  __ CallRuntime(Runtime::kGetPropertyNamesFast, 1);

  // If we got a map from the runtime call, we can do a fast
  // modification check. Otherwise, we got a fixed array, and we have
  // to do a slow check.
  Label fixed_array;
  __ mov(a2, v0);
  __ lw(a1, FieldMemOperand(a2, HeapObject::kMapOffset));
  __ LoadRoot(at, Heap::kMetaMapRootIndex);
  __ Branch(&fixed_array, ne, a1, Operand(at));

  // We got a map in register v0. Get the enumeration cache from it.
  __ lw(a1, FieldMemOperand(v0, Map::kInstanceDescriptorsOffset));
  __ lw(a1, FieldMemOperand(a1, DescriptorArray::kEnumerationIndexOffset));
  __ lw(a2, FieldMemOperand(a1, DescriptorArray::kEnumCacheBridgeCacheOffset));

  // Setup the four remaining stack slots.
  __ push(v0);  // Map.
  __ lw(a1, FieldMemOperand(a2, FixedArray::kLengthOffset));
  __ li(a0, Operand(Smi::FromInt(0)));
  // Push enumeration cache, enumeration cache length (as smi) and zero.
  __ Push(a2, a1, a0);
  __ jmp(&loop);

  // We got a fixed array in register v0. Iterate through that.
  __ bind(&fixed_array);
  __ li(a1, Operand(Smi::FromInt(0)));  // Map (0) - force slow check.
  __ Push(a1, v0);
  __ lw(a1, FieldMemOperand(v0, FixedArray::kLengthOffset));
  __ li(a0, Operand(Smi::FromInt(0)));
  __ Push(a1, a0);  // Fixed array length (as smi) and initial index.

  // Generate code for doing the condition check.
  __ bind(&loop);
  // Load the current count to a0, load the length to a1.
  __ lw(a0, MemOperand(sp, 0 * kPointerSize));
  __ lw(a1, MemOperand(sp, 1 * kPointerSize));
  __ Branch(loop_statement.break_target(), hs, a0, Operand(a1));

  // Get the current entry of the array into register a3.
  __ lw(a2, MemOperand(sp, 2 * kPointerSize));
  __ Addu(a2, a2, Operand(FixedArray::kHeaderSize - kHeapObjectTag));
  __ sll(t0, a0, kPointerSizeLog2 - kSmiTagSize);
  __ addu(t0, a2, t0);  // Array base + scaled (smi) index.
  __ lw(a3, MemOperand(t0));  // Current entry.

  // Get the expected map from the stack or a zero map in the
  // permanent slow case into register a2.
  __ lw(a2, MemOperand(sp, 3 * kPointerSize));

  // Check if the expected map still matches that of the enumerable.
  // If not, we have to filter the key.
  Label update_each;
  __ lw(a1, MemOperand(sp, 4 * kPointerSize));
  __ lw(t0, FieldMemOperand(a1, HeapObject::kMapOffset));
  __ Branch(&update_each, eq, t0, Operand(a2));

  // Convert the entry to a string or (smi) 0 if it isn't a property
  // any more. If the property has been removed while iterating, we
  // just skip it.
  __ push(a1);  // Enumerable.
  __ push(a3);  // Current entry.
  __ InvokeBuiltin(Builtins::FILTER_KEY, CALL_JS);
  __ mov(a3, result_register());
  __ Branch(loop_statement.continue_target(), eq, a3, Operand(zero_reg));

  // Update the 'each' property or variable from the possibly filtered
  // entry in register a3.
  __ bind(&update_each);
  __ mov(result_register(), a3);
  // Perform the assignment as if via '='.
  EmitAssignment(stmt->each());

  // Generate code for the body of the loop.
  Label stack_limit_hit, stack_check_done;
  Visit(stmt->body());

  __ StackLimitCheck(&stack_limit_hit);
  __ bind(&stack_check_done);

  // Generate code for the going to the next element by incrementing
  // the index (smi) stored on top of the stack.
  __ bind(loop_statement.continue_target());
  __ pop(a0);
  __ Addu(a0, a0, Operand(Smi::FromInt(1)));
  __ push(a0);
  __ Branch(&loop);

  // Slow case for the stack limit check.
  StackCheckStub stack_check_stub;
  __ bind(&stack_limit_hit);
  __ CallStub(&stack_check_stub);
  __ Branch(&stack_check_done);

  // Remove the pointers stored on the stack.
  __ bind(loop_statement.break_target());
  __ Drop(5);

  // Exit and decrement the loop depth.
  __ bind(&exit);
  decrement_loop_depth();
}


void FullCodeGenerator::EmitNewClosure(Handle<SharedFunctionInfo> info,
                                       bool pretenure) {
  // Use the fast case closure allocation code that allocates in new
  // space for nested functions that don't need literals cloning.
  if (scope()->is_function_scope() &&
      info->num_literals() == 0 &&
      !pretenure) {
    FastNewClosureStub stub;
    __ li(a0, Operand(info));
    __ push(a0);
    __ CallStub(&stub);
  } else {
    __ li(a0, Operand(info));
    __ LoadRoot(a1, pretenure ? Heap::kTrueValueRootIndex
                              : Heap::kFalseValueRootIndex);
    __ Push(cp, a0, a1);
    __ CallRuntime(Runtime::kNewClosure, 3);
  }
  context()->Plug(v0);
}


void FullCodeGenerator::VisitVariableProxy(VariableProxy* expr) {
  Comment cmnt(masm_, "[ VariableProxy");
  EmitVariableLoad(expr->var());
}


MemOperand FullCodeGenerator::ContextSlotOperandCheckExtensions(
    Slot* slot,
    Label* slow) {
  ASSERT(slot->type() == Slot::CONTEXT);
  Register current = cp;
  Register next = a3;
  Register temp = t0;

  for (Scope* s = scope(); s != slot->var()->scope(); s = s->outer_scope()) {
    if (s->num_heap_slots() > 0) {
      if (s->calls_eval()) {
        // Check that extension is NULL.
        __ lw(temp, ContextOperand(current, Context::EXTENSION_INDEX));
        __ Branch(slow, ne, temp, Operand(zero_reg));
      }
      __ lw(next, ContextOperand(current, Context::CLOSURE_INDEX));
      __ lw(next, FieldMemOperand(next, JSFunction::kContextOffset));
      // Walk the rest of the chain without clobbering cp.
      current = next;
    }
  }
  // Check that last extension is NULL.
  __ lw(temp, ContextOperand(current, Context::EXTENSION_INDEX));
  __ Branch(slow, ne, temp, Operand(zero_reg));
  __ lw(temp, ContextOperand(current, Context::FCONTEXT_INDEX));
  return ContextOperand(temp, slot->index());
}


void FullCodeGenerator::EmitDynamicLoadFromSlotFastCase(
    Slot* slot,
    TypeofState typeof_state,
    Label* slow,
    Label* done) {
  // Generate fast-case code for variables that might be shadowed by
  // eval-introduced variables.  Eval is used a lot without
  // introducing variables.  In those cases, we do not want to
  // perform a runtime call for all variables in the scope
  // containing the eval.
  if (slot->var()->mode() == Variable::DYNAMIC_GLOBAL) {
    EmitLoadGlobalSlotCheckExtensions(slot, typeof_state, slow);
    __ Branch(done);
  } else if (slot->var()->mode() == Variable::DYNAMIC_LOCAL) {
    Slot* potential_slot = slot->var()->local_if_not_shadowed()->AsSlot();
    Expression* rewrite = slot->var()->local_if_not_shadowed()->rewrite();
    if (potential_slot != NULL) {
      // Generate fast case for locals that rewrite to slots.
      __ lw(v0, ContextSlotOperandCheckExtensions(potential_slot, slow));
      if (potential_slot->var()->mode() == Variable::CONST) {
        __ LoadRoot(at, Heap::kTheHoleValueRootIndex);
        __ subu(at, v0, at);  // Sub as compare: at == 0 on eq.
        __ LoadRoot(a0, Heap::kUndefinedValueRootIndex);
        __ movz(v0, a0, at);  // Conditional move.
      }
      __ Branch(done);
    } else if (rewrite != NULL) {
      // Generate fast case for calls of an argument function.
      Property* property = rewrite->AsProperty();
      if (property != NULL) {
        VariableProxy* obj_proxy = property->obj()->AsVariableProxy();
        Literal* key_literal = property->key()->AsLiteral();
        if (obj_proxy != NULL &&
            key_literal != NULL &&
            obj_proxy->IsArguments() &&
            key_literal->handle()->IsSmi()) {
          // Load arguments object if there are no eval-introduced
          // variables. Then load the argument from the arguments
          // object using keyed load.
          __ lw(a1,
                 ContextSlotOperandCheckExtensions(obj_proxy->var()->AsSlot(),
                                                   slow));
          __ li(a0, Operand(key_literal->handle()));
          Handle<Code> ic(Builtins::builtin(Builtins::KeyedLoadIC_Initialize));
          EmitCallIC(ic, RelocInfo::CODE_TARGET);
          __ Branch(done);
        }
      }
    }
  }
}


void FullCodeGenerator::EmitLoadGlobalSlotCheckExtensions(
    Slot* slot,
    TypeofState typeof_state,
    Label* slow) {
  Register current = cp;
  Register next = a1;
  Register temp = a2;

  Scope* s = scope();
  while (s != NULL) {
    if (s->num_heap_slots() > 0) {
      if (s->calls_eval()) {
        // Check that extension is NULL.
        __ lw(temp, ContextOperand(current, Context::EXTENSION_INDEX));
        __ Branch(slow, ne, temp, Operand(zero_reg));
      }
      // Load next context in chain.
      __ lw(next, ContextOperand(current, Context::CLOSURE_INDEX));
      __ lw(next, FieldMemOperand(next, JSFunction::kContextOffset));
      // Walk the rest of the chain without clobbering cp.
      current = next;
    }
    // If no outer scope calls eval, we do not need to check more
    // context extensions.
    if (!s->outer_scope_calls_eval() || s->is_eval_scope()) break;
    s = s->outer_scope();
  }

  if (s->is_eval_scope()) {
    Label loop, fast;
    if (!current.is(next)) {
      __ Move(next, current);
    }
    __ bind(&loop);
    // Terminate at global context.
    __ lw(temp, FieldMemOperand(next, HeapObject::kMapOffset));
    __ LoadRoot(at, Heap::kGlobalContextMapRootIndex);
    __ Branch(&fast, eq, temp, Operand(at));

    // Check that extension is NULL.
    __ lw(temp, ContextOperand(next, Context::EXTENSION_INDEX));
    __ Branch(slow, ne, temp, Operand(zero_reg));

    // Load next context in chain.
    __ lw(next, ContextOperand(next, Context::CLOSURE_INDEX));
    __ lw(next, FieldMemOperand(next, JSFunction::kContextOffset));
    __ Branch(&loop);
    __ bind(&fast);
  }

  __ lw(a0, GlobalObjectOperand());
  __ li(a2, Operand(slot->var()->name()));
  RelocInfo::Mode mode = (typeof_state == INSIDE_TYPEOF)
      ? RelocInfo::CODE_TARGET
      : RelocInfo::CODE_TARGET_CONTEXT;
  Handle<Code> ic(Builtins::builtin(Builtins::LoadIC_Initialize));
  EmitCallIC(ic, mode);
}


void FullCodeGenerator::EmitVariableLoad(Variable* var) {
  // Four cases: non-this global variables, lookup slots, all other
  // types of slots, and parameters that rewrite to explicit property
  // accesses on the arguments object.
  Slot* slot = var->AsSlot();
  Property* property = var->AsProperty();

  if (var->is_global() && !var->is_this()) {
    Comment cmnt(masm_, "Global variable");
    // Use inline caching. Variable name is passed in a2 and the global
    // object (receiver) in a0.
    __ lw(a0, GlobalObjectOperand());
    __ li(a2, Operand(var->name()));
    Handle<Code> ic(Builtins::builtin(Builtins::LoadIC_Initialize));
    EmitCallIC(ic, RelocInfo::CODE_TARGET_CONTEXT);
    context()->Plug(v0);

  } else if (slot != NULL && slot->type() == Slot::LOOKUP) {
    Label done, slow;

    // Generate code for loading from variables potentially shadowed
    // by eval-introduced variables.
    EmitDynamicLoadFromSlotFastCase(slot, NOT_INSIDE_TYPEOF, &slow, &done);

    __ bind(&slow);
    Comment cmnt(masm_, "Lookup slot");
    __ li(a1, Operand(var->name()));
    __ Push(cp, a1);  // Context and name.
    __ CallRuntime(Runtime::kLoadContextSlot, 2);
    __ bind(&done);

    context()->Plug(v0);

  } else if (slot != NULL) {
    Comment cmnt(masm_, (slot->type() == Slot::CONTEXT)
                            ? "Context slot"
                            : "Stack slot");
    if (var->mode() == Variable::CONST) {
       // Constants may be the hole value if they have not been initialized.
       // Unhole them.
       MemOperand slot_operand = EmitSlotSearch(slot, a0);
       __ lw(v0, slot_operand);
       __ LoadRoot(at, Heap::kTheHoleValueRootIndex);
       __ subu(at, v0, at);  // Sub as compare: at == 0 on eq.
       __ LoadRoot(a0, Heap::kUndefinedValueRootIndex);
       __ movz(v0, a0, at);  // Conditional move.
       context()->Plug(v0);
     } else {
       context()->Plug(slot);
     }
  } else {
    Comment cmnt(masm_, "Rewritten parameter");
    ASSERT_NOT_NULL(property);
    // Rewritten parameter accesses are of the form "slot[literal]".
    // Assert that the object is in a slot.
    Variable* object_var = property->obj()->AsVariableProxy()->AsVariable();
    ASSERT_NOT_NULL(object_var);
    Slot* object_slot = object_var->AsSlot();
    ASSERT_NOT_NULL(object_slot);

    // Load the object.
    Move(a1, object_slot);

    // Assert that the key is a smi.
    Literal* key_literal = property->key()->AsLiteral();
    ASSERT_NOT_NULL(key_literal);
    ASSERT(key_literal->handle()->IsSmi());

    // Load the key.
    __ li(a0, Operand(key_literal->handle()));

    // Call keyed load IC. It has arguments key and receiver in a0 and a1.
    Handle<Code> ic(Builtins::builtin(Builtins::KeyedLoadIC_Initialize));
    EmitCallIC(ic, RelocInfo::CODE_TARGET);
    context()->Plug(v0);
  }
}


void FullCodeGenerator::VisitRegExpLiteral(RegExpLiteral* expr) {
  Comment cmnt(masm_, "[ RegExpLiteral");
  Label materialized;
  // Registers will be used as follows:
  // t0 = JS function, literals array
  // a3 = literal index
  // a2 = RegExp pattern
  // a1 = RegExp flags
  // a0 = temp + materialized value (RegExp literal)
  __ lw(a0, MemOperand(fp, JavaScriptFrameConstants::kFunctionOffset));
  __ lw(t0, FieldMemOperand(a0, JSFunction::kLiteralsOffset));
  int literal_offset =
      FixedArray::kHeaderSize + expr->literal_index() * kPointerSize;
  __ lw(v0, FieldMemOperand(t0, literal_offset));
  __ LoadRoot(at, Heap::kUndefinedValueRootIndex);
  __ Branch(&materialized, ne, v0, Operand(at));

  // Create regexp literal using runtime function.
  // Result will be in v0.
  __ li(a3, Operand(Smi::FromInt(expr->literal_index())));
  __ li(a2, Operand(expr->pattern()));
  __ li(a1, Operand(expr->flags()));
  __ Push(t0, a3, a2, a1);
  __ CallRuntime(Runtime::kMaterializeRegExpLiteral, 4);

  __ bind(&materialized);
  int size = JSRegExp::kSize + JSRegExp::kInObjectFieldCount * kPointerSize;
  __ push(v0);
  __ li(a0, Operand(Smi::FromInt(size)));
  __ push(a0);
  __ CallRuntime(Runtime::kAllocateInNewSpace, 1);

  // After this, registers are used as follows:
  // v0: Newly allocated regexp.
  // a1: Materialized regexp.
  // a2: temp.
  __ pop(a1);
  __ CopyFields(v0, a1, a2.bit(), size / kPointerSize);
  context()->Plug(v0);
}


void FullCodeGenerator::VisitObjectLiteral(ObjectLiteral* expr) {
  Comment cmnt(masm_, "[ ObjectLiteral");
  __ lw(a3, MemOperand(fp,  JavaScriptFrameConstants::kFunctionOffset));
  __ lw(a3, FieldMemOperand(a3, JSFunction::kLiteralsOffset));
  __ li(a2, Operand(Smi::FromInt(expr->literal_index())));
  __ li(a1, Operand(expr->constant_properties()));
  __ li(a0, Operand(Smi::FromInt(expr->fast_elements() ? 1 : 0)));
  __ Push(a3, a2, a1, a0);
  if (expr->depth() > 1) {
    __ CallRuntime(Runtime::kCreateObjectLiteral, 4);
  } else {
    __ CallRuntime(Runtime::kCreateObjectLiteralShallow, 4);
  }

  // If result_saved is true the result is on top of the stack.  If
  // result_saved is false the result is in v0.
  bool result_saved = false;

  // Mark all computed expressions that are bound to a key that
  // is shadowed by a later occurrence of the same key. For the
  // marked expressions, no store code is emitted.
  expr->CalculateEmitStore();

  for (int i = 0; i < expr->properties()->length(); i++) {
    ObjectLiteral::Property* property = expr->properties()->at(i);
    if (property->IsCompileTimeValue()) continue;

    Literal* key = property->key();
    Expression* value = property->value();
    if (!result_saved) {
      __ push(v0);  // Save result on stack
      result_saved = true;
    }
    switch (property->kind()) {
      case ObjectLiteral::Property::CONSTANT:
        UNREACHABLE();
      case ObjectLiteral::Property::MATERIALIZED_LITERAL:
        ASSERT(!CompileTimeValue::IsCompileTimeValue(property->value()));
        // Fall through.
      case ObjectLiteral::Property::COMPUTED:
        if (key->handle()->IsSymbol()) {
          VisitForAccumulatorValue(value);
          __ mov(a0, result_register());
          __ li(a2, Operand(key->handle()));
          __ lw(a1, MemOperand(sp));
          if (property->emit_store()) {
            Handle<Code> ic(Builtins::builtin(Builtins::StoreIC_Initialize));
            EmitCallIC(ic, RelocInfo::CODE_TARGET);
          }
          break;
        }
        // Fall through.
      case ObjectLiteral::Property::PROTOTYPE:
        // Duplicate receiver on stack.
        __ lw(a0, MemOperand(sp));
        __ push(a0);
        VisitForStackValue(key);
        VisitForStackValue(value);
        if (property->emit_store()) {
          __ CallRuntime(Runtime::kSetProperty, 3);
        } else {
          __ Drop(3);
        }
        break;
      case ObjectLiteral::Property::GETTER:
      case ObjectLiteral::Property::SETTER:
        // Duplicate receiver on stack.
        __ lw(a0, MemOperand(sp));
        __ push(a0);
        VisitForStackValue(key);
        __ li(a1, Operand(property->kind() == ObjectLiteral::Property::SETTER ?
                           Smi::FromInt(1) :
                           Smi::FromInt(0)));
        __ push(a1);
        VisitForStackValue(value);
        __ CallRuntime(Runtime::kDefineAccessor, 4);
        break;
    }
  }

  if (result_saved) {
    context()->PlugTOS();
  } else {
    context()->Plug(v0);
  }
}


void FullCodeGenerator::VisitArrayLiteral(ArrayLiteral* expr) {
  Comment cmnt(masm_, "[ ArrayLiteral");

  ZoneList<Expression*>* subexprs = expr->values();
  int length = subexprs->length();
  __ mov(a0, result_register());
  __ lw(a3, MemOperand(fp, JavaScriptFrameConstants::kFunctionOffset));
  __ lw(a3, FieldMemOperand(a3, JSFunction::kLiteralsOffset));
  __ li(a2, Operand(Smi::FromInt(expr->literal_index())));
  __ li(a1, Operand(expr->constant_elements()));
  __ Push(a3, a2, a1);
  if (expr->constant_elements()->map() == Heap::fixed_cow_array_map()) {
    FastCloneShallowArrayStub stub(
        FastCloneShallowArrayStub::COPY_ON_WRITE_ELEMENTS, length);
    __ CallStub(&stub);
    __ IncrementCounter(&Counters::cow_arrays_created_stub, 1, a1, a2);
  } else if (expr->depth() > 1) {
    __ CallRuntime(Runtime::kCreateArrayLiteral, 3);
  } else if (length > FastCloneShallowArrayStub::kMaximumClonedLength) {
    __ CallRuntime(Runtime::kCreateArrayLiteralShallow, 3);
  } else {
    FastCloneShallowArrayStub stub(
        FastCloneShallowArrayStub::CLONE_ELEMENTS, length);
    __ CallStub(&stub);
  }

  bool result_saved = false;  // Is the result saved to the stack?

  // Emit code to evaluate all the non-constant subexpressions and to store
  // them into the newly cloned array.
  for (int i = 0; i < length; i++) {
    Expression* subexpr = subexprs->at(i);
    // If the subexpression is a literal or a simple materialized literal it
    // is already set in the cloned array.
    if (subexpr->AsLiteral() != NULL ||
        CompileTimeValue::IsCompileTimeValue(subexpr)) {
      continue;
    }

    if (!result_saved) {
      __ push(v0);
      result_saved = true;
    }
    VisitForAccumulatorValue(subexpr);

    // Store the subexpression value in the array's elements.
    __ lw(a1, MemOperand(sp));  // Copy of array literal.
    __ lw(a1, FieldMemOperand(a1, JSObject::kElementsOffset));
    int offset = FixedArray::kHeaderSize + (i * kPointerSize);
    __ sw(result_register(), FieldMemOperand(a1, offset));

    // Update the write barrier for the array store with v0 as the scratch
    // register.
    __ li(a2, Operand(offset));
    // TODO(PJ): double check this RecordWrite call.
    __ RecordWrite(a1, a2, result_register());
  }

  if (result_saved) {
    context()->PlugTOS();
  } else {
    context()->Plug(v0);
  }
}


void FullCodeGenerator::VisitAssignment(Assignment* expr) {
  Comment cmnt(masm_, "[ Assignment");
  // Invalid left-hand sides are rewritten to have a 'throw ReferenceError'
  // on the left-hand side.
  if (!expr->target()->IsValidLeftHandSide()) {
    VisitForEffect(expr->target());
    return;
  }

  // Left-hand side can only be a property, a global or a (parameter or local)
  // slot. Variables with rewrite to .arguments are treated as KEYED_PROPERTY.
  enum LhsKind { VARIABLE, NAMED_PROPERTY, KEYED_PROPERTY };
  LhsKind assign_type = VARIABLE;
  Property* property = expr->target()->AsProperty();
  if (property != NULL) {
    assign_type = (property->key()->IsPropertyName())
        ? NAMED_PROPERTY
        : KEYED_PROPERTY;
  }

  // Evaluate LHS expression.
  switch (assign_type) {
    case VARIABLE:
      // Nothing to do here.
      break;
    case NAMED_PROPERTY:
      if (expr->is_compound()) {
        // We need the receiver both on the stack and in the accumulator.
        VisitForAccumulatorValue(property->obj());
        __ push(result_register());
      } else {
        VisitForStackValue(property->obj());
      }
      break;
    case KEYED_PROPERTY:
      // We need the key and receiver on both the stack and in v0 and a1.
      if (expr->is_compound()) {
        VisitForStackValue(property->obj());
        VisitForAccumulatorValue(property->key());
        __ lw(a1, MemOperand(sp, 0));
        __ push(v0);
      } else {
        VisitForStackValue(property->obj());
        VisitForStackValue(property->key());
      }
      break;
  }

  if (expr->is_compound()) {
    { AccumulatorValueContext context(this);
      switch (assign_type) {
        case VARIABLE:
          EmitVariableLoad(expr->target()->AsVariableProxy()->var());
          break;
        case NAMED_PROPERTY:
          EmitNamedPropertyLoad(property);
          break;
        case KEYED_PROPERTY:
          EmitKeyedPropertyLoad(property);
          break;
      }
    }
    Token::Value op = expr->binary_op();
    ConstantOperand constant = ShouldInlineSmiCase(op)
        ? GetConstantOperand(op, expr->target(), expr->value())
        : kNoConstants;
    ASSERT(constant == kRightConstant || constant == kNoConstants);
    if (constant == kNoConstants) {
      __ push(v0);  // Left operand goes on the stack.
      VisitForAccumulatorValue(expr->value());
    }

    OverwriteMode mode = expr->value()->ResultOverwriteAllowed()
        ? OVERWRITE_RIGHT
        : NO_OVERWRITE;
    SetSourcePosition(expr->position() + 1);
    // Write posistions prior to subsequent call, to ensure RelocInfo ordering.
    __ positions_recorder()->WriteRecordedPositions();

    if (ShouldInlineSmiCase(op)) {
      EmitInlineSmiBinaryOp(expr,
                            op,
                            mode,
                            expr->target(),
                            expr->value(),
                            constant);
    } else {
      { AccumulatorValueContext context(this);  // plind HACK, will break stuff
        EmitBinaryOp(op, mode);
      }
    }
  } else {
    VisitForAccumulatorValue(expr->value());
  }

  // Record source position before possible IC call.
  SetSourcePosition(expr->position());
  // Write posistions prior to subsequent call, to ensure RelocInfo ordering.
  __ positions_recorder()->WriteRecordedPositions();

  // Store the value.
  switch (assign_type) {
    case VARIABLE:
      EmitVariableAssignment(expr->target()->AsVariableProxy()->var(),
                             expr->op());
      break;
    case NAMED_PROPERTY:
      EmitNamedPropertyAssignment(expr);
      break;
    case KEYED_PROPERTY:
      EmitKeyedPropertyAssignment(expr);
      break;
  }
}


void FullCodeGenerator::EmitNamedPropertyLoad(Property* prop) {
  SetSourcePosition(prop->position());
  // Write posistions prior to subsequent call, to ensure RelocInfo ordering.
  __ positions_recorder()->WriteRecordedPositions();

  Literal* key = prop->key()->AsLiteral();
  __ mov(a0, result_register());
  __ li(a2, Operand(key->handle()));
  // Call load IC. It has arguments receiver and property name a0 and a2.
  Handle<Code> ic(Builtins::builtin(Builtins::LoadIC_Initialize));
  EmitCallIC(ic, RelocInfo::CODE_TARGET);
}


void FullCodeGenerator::EmitKeyedPropertyLoad(Property* prop) {
  SetSourcePosition(prop->position());
  // Write posistions prior to subsequent call, to ensure RelocInfo ordering.
  __ positions_recorder()->WriteRecordedPositions();

  __ mov(a0, result_register());
  // Call keyed load IC. It has arguments key and receiver in a0 and a1.
  Handle<Code> ic(Builtins::builtin(Builtins::KeyedLoadIC_Initialize));
  EmitCallIC(ic, RelocInfo::CODE_TARGET);
}


void FullCodeGenerator::EmitInlineSmiBinaryOp(Expression* expr,
                                              Token::Value op,
                                              OverwriteMode mode,
                                              Expression* left,
                                              Expression* right,
                                              ConstantOperand constant) {
  ASSERT(constant == kNoConstants);  // Only handled case.
  EmitBinaryOp(op, mode);
}


void FullCodeGenerator::EmitBinaryOp(Token::Value op,
                                     OverwriteMode mode) {
  __ mov(a0, result_register());
  __ pop(a1);
  GenericBinaryOpStub stub(op, mode, a1, a0);
  __ CallStub(&stub);
  context()->Plug(v0);
}


void FullCodeGenerator::EmitAssignment(Expression* expr) {
  // Invalid left-hand sides are rewritten to have a 'throw
  // ReferenceError' on the left-hand side.
  if (!expr->IsValidLeftHandSide()) {
    VisitForEffect(expr);
    return;
  }

  // Left-hand side can only be a property, a global or a (parameter or local)
  // slot. Variables with rewrite to .arguments are treated as KEYED_PROPERTY.
  enum LhsKind { VARIABLE, NAMED_PROPERTY, KEYED_PROPERTY };
  LhsKind assign_type = VARIABLE;
  Property* prop = expr->AsProperty();
  if (prop != NULL) {
    assign_type = (prop->key()->IsPropertyName())
        ? NAMED_PROPERTY
        : KEYED_PROPERTY;
  }

  switch (assign_type) {
    case VARIABLE: {
      Variable* var = expr->AsVariableProxy()->var();
      EffectContext context(this);
      EmitVariableAssignment(var, Token::ASSIGN);
      break;
    }
    case NAMED_PROPERTY: {
      __ push(result_register());  // Preserve value.
      VisitForAccumulatorValue(prop->obj());
      __ mov(a1, result_register());
      __ pop(a0);  // Restore value.
      __ li(a2, Operand(prop->key()->AsLiteral()->handle()));
      Handle<Code> ic(Builtins::builtin(Builtins::StoreIC_Initialize));
      EmitCallIC(ic, RelocInfo::CODE_TARGET);
      break;
    }
    case KEYED_PROPERTY: {
      __ push(result_register());  // Preserve value.
      VisitForStackValue(prop->obj());
      VisitForAccumulatorValue(prop->key());
      __ mov(a1, result_register());
      __ pop(a2);
      __ pop(a0);  // Restore value.
      Handle<Code> ic(Builtins::builtin(Builtins::KeyedStoreIC_Initialize));
      EmitCallIC(ic, RelocInfo::CODE_TARGET);
      break;
    }
  }
}


void FullCodeGenerator::EmitVariableAssignment(Variable* var,
                                               Token::Value op) {
  // Left-hand sides that rewrite to explicit property accesses do not reach
  // here.
  ASSERT(var != NULL);
  ASSERT(var->is_global() || var->AsSlot() != NULL);

  if (var->is_global()) {
    ASSERT(!var->is_this());
    // Assignment to a global variable.  Use inline caching for the
    // assignment.  Right-hand-side value is passed in a0, variable name in
    // a2, and the global object in a1.
    __ mov(a0, result_register());
    __ li(a2, Operand(var->name()));
    __ lw(a1, GlobalObjectOperand());
    Handle<Code> ic(Builtins::builtin(Builtins::StoreIC_Initialize));
    EmitCallIC(ic, RelocInfo::CODE_TARGET);

  } else if (var->mode() != Variable::CONST || op == Token::INIT_CONST) {
    // Perform the assignment for non-const variables and for initialization
    // of const variables.  Const assignments are simply skipped.
    Label done;
    Slot* slot = var->AsSlot();
    switch (slot->type()) {
      case Slot::PARAMETER:
      case Slot::LOCAL:
        if (op == Token::INIT_CONST) {
          // Detect const reinitialization by checking for the hole value.
          __ lw(a1, MemOperand(fp, SlotOffset(slot)));
          __ LoadRoot(t0, Heap::kTheHoleValueRootIndex);
          __ Branch(&done, ne, a1, Operand(t0));
        }
        // Perform the assignment.
        __ sw(result_register(), MemOperand(fp, SlotOffset(slot)));
        break;

      case Slot::CONTEXT: {
        MemOperand target = EmitSlotSearch(slot, a1);
        if (op == Token::INIT_CONST) {
          // Detect const reinitialization by checking for the hole value.
          __ lw(a2, target);
          __ LoadRoot(t0, Heap::kTheHoleValueRootIndex);
          __ Branch(&done, ne, a2, Operand(t0));
        }
        // Perform the assignment and issue the write barrier.
        __ sw(result_register(), target);
        // RecordWrite may destroy all its register arguments.
        __ mov(a3, result_register());
         int offset = FixedArray::kHeaderSize + slot->index() * kPointerSize;
        __ RecordWrite(a1, Operand(offset), a2, a3);
        break;
      }

      case Slot::LOOKUP:
        // Call the runtime for the assignment.  The runtime will ignore
        // const reinitialization.
        __ push(v0);  // Value.
        __ li(a0, Operand(slot->var()->name()));
        __ Push(cp, a0);  // Context and name.
        if (op == Token::INIT_CONST) {
          // The runtime will ignore const redeclaration.
          __ CallRuntime(Runtime::kInitializeConstContextSlot, 3);
        } else {
          __ CallRuntime(Runtime::kStoreContextSlot, 3);
        }
        break;
    }
    __ bind(&done);
  }

  context()->Plug(result_register());
}


void FullCodeGenerator::EmitNamedPropertyAssignment(Assignment* expr) {
  // Assignment to a property, using a named store IC.
  Property* prop = expr->target()->AsProperty();
  ASSERT(prop != NULL);
  ASSERT(prop->key()->AsLiteral() != NULL);

  // If the assignment starts a block of assignments to the same object,
  // change to slow case to avoid the quadratic behavior of repeatedly
  // adding fast properties.
  if (expr->starts_initialization_block()) {
    __ push(result_register());
    __ lw(t0, MemOperand(sp, kPointerSize));  // Receiver is now under value.
    __ push(t0);
    __ CallRuntime(Runtime::kToSlowProperties, 1);
    __ pop(result_register());
  }

  // Record source code position before IC call.
  SetSourcePosition(expr->position());
  // Write posistions prior to subsequent call, to ensure RelocInfo ordering.
  __ positions_recorder()->WriteRecordedPositions();

  __ mov(a0, result_register());  // Load the value.
  __ li(a2, Operand(prop->key()->AsLiteral()->handle()));
  // Load receiver to a1. Leave a copy in the stack if needed for turning the
  // receiver into fast case.
  if (expr->ends_initialization_block()) {
    __ lw(a1, MemOperand(sp));
  } else {
    __ pop(a1);
  }

  Handle<Code> ic(Builtins::builtin(Builtins::StoreIC_Initialize));
  EmitCallIC(ic, RelocInfo::CODE_TARGET);

  // If the assignment ends an initialization block, revert to fast case.
  if (expr->ends_initialization_block()) {
    __ push(v0);  // Result of assignment, saved even if not needed.
    // Receiver is under the result value.
    __ lw(t0, MemOperand(sp, kPointerSize));
    __ push(t0);
    __ CallRuntime(Runtime::kToFastProperties, 1);
    __ pop(v0);
    context()->DropAndPlug(1, v0);
  } else {
    context()->Plug(v0);
  }
}


void FullCodeGenerator::EmitKeyedPropertyAssignment(Assignment* expr) {
  // Assignment to a property, using a keyed store IC.

  // If the assignment starts a block of assignments to the same object,
  // change to slow case to avoid the quadratic behavior of repeatedly
  // adding fast properties.
  if (expr->starts_initialization_block()) {
    __ push(result_register());
    // Receiver is now under the key and value.
    __ lw(t0, MemOperand(sp, 2 * kPointerSize));
    __ push(t0);
    __ CallRuntime(Runtime::kToSlowProperties, 1);
    __ pop(result_register());
  }

  // Record source code position before IC call.
  SetSourcePosition(expr->position());
  // Write posistions prior to subsequent call, to ensure RelocInfo ordering.
  __ positions_recorder()->WriteRecordedPositions();

  // Call keyed store IC.
  // The arguments are:
  // - a0 is the value,
  // - a1 is the key,
  // - a2 is the receiver.
  __ mov(a0, result_register());
  __ pop(a1);  // Key.
  // Load receiver to a2. Leave a copy in the stack if needed for turning the
  // receiver into fast case.
  if (expr->ends_initialization_block()) {
    __ lw(a2, MemOperand(sp));
  } else {
    __ pop(a2);
  }

  Handle<Code> ic(Builtins::builtin(Builtins::KeyedStoreIC_Initialize));
  EmitCallIC(ic, RelocInfo::CODE_TARGET);

  // If the assignment ends an initialization block, revert to fast case.
  if (expr->ends_initialization_block()) {
    __ push(v0);  // Result of assignment, saved even if not needed.
    // Receiver is under the result value.
    __ lw(t0, MemOperand(sp, kPointerSize));
    __ push(t0);
    __ CallRuntime(Runtime::kToFastProperties, 1);
    __ pop(v0);
    context()->DropAndPlug(1, v0);
  } else {
    context()->Plug(v0);
  }
}


void FullCodeGenerator::VisitProperty(Property* expr) {
  Comment cmnt(masm_, "[ Property");
  Expression* key = expr->key();

  if (key->IsPropertyName()) {
    VisitForAccumulatorValue(expr->obj());
    EmitNamedPropertyLoad(expr);
  } else {
    VisitForStackValue(expr->obj());
    VisitForAccumulatorValue(expr->key());
    __ pop(a1);
    EmitKeyedPropertyLoad(expr);
  }
  context()->Plug(v0);
}


void FullCodeGenerator::EmitCallWithIC(Call* expr,
                                       Handle<Object> name,
                                       RelocInfo::Mode mode) {
  // Code common for calls using the IC.
  ZoneList<Expression*>* args = expr->arguments();
  int arg_count = args->length();
  { PreservePositionScope scope(masm()->positions_recorder());
    for (int i = 0; i < arg_count; i++) {
      VisitForStackValue(args->at(i));
    }
    __ li(a2, Operand(name));
  }
  // Record source position for debugger.
  SetSourcePosition(expr->position());
  // Write posistions prior to subsequent call, to ensure RelocInfo ordering.
  __ positions_recorder()->WriteRecordedPositions();

  // Call the IC initialization code.
  InLoopFlag in_loop = (loop_depth() > 0) ? IN_LOOP : NOT_IN_LOOP;
  Handle<Code> ic = StubCache::ComputeCallInitialize(arg_count, in_loop);
  EmitCallIC(ic, mode);
  // Restore context register.
  __ lw(cp, MemOperand(fp, StandardFrameConstants::kContextOffset));
  context()->Plug(v0);
}


void FullCodeGenerator::EmitKeyedCallWithIC(Call* expr,
                                            Expression* key,
                                            RelocInfo::Mode mode) {
  // Load the key.
  VisitForAccumulatorValue(key);

  // Swap the name of the function and the receiver on the stack to follow
  // the calling convention for call ICs.
  __ Pop(a1);
  __ Push(v0);
  __ Push(a1);

  // Code common for calls using the IC.
  ZoneList<Expression*>* args = expr->arguments();
  int arg_count = args->length();
  { PreservePositionScope scope(masm()->positions_recorder());
    for (int i = 0; i < arg_count; i++) {
      VisitForStackValue(args->at(i));
    }
  }
  // Record source position for debugger.
  SetSourcePosition(expr->position());
  // Write posistions prior to subsequent call, to ensure RelocInfo ordering.
  __ positions_recorder()->WriteRecordedPositions();

  // Call the IC initialization code.
  InLoopFlag in_loop = (loop_depth() > 0) ? IN_LOOP : NOT_IN_LOOP;
  Handle<Code> ic = StubCache::ComputeKeyedCallInitialize(arg_count, in_loop);
  __ lw(a2, MemOperand(sp, (arg_count + 1) * kPointerSize));  // Key.
  EmitCallIC(ic, mode);
  // Restore context register.
  __ lw(cp, MemOperand(fp, StandardFrameConstants::kContextOffset));
  context()->DropAndPlug(1, v0);  // Drop the key still on the stack.
}


void FullCodeGenerator::EmitCallWithStub(Call* expr) {
  // Code common for calls using the call stub.
  ZoneList<Expression*>* args = expr->arguments();
  int arg_count = args->length();
  { PreservePositionScope scope(masm()->positions_recorder());
    for (int i = 0; i < arg_count; i++) {
      VisitForStackValue(args->at(i));
    }
  }
  // Record source position for debugger.
  SetSourcePosition(expr->position());
  // Write posistions prior to subsequent call, to ensure RelocInfo ordering.
  __ positions_recorder()->WriteRecordedPositions();

  InLoopFlag in_loop = (loop_depth() > 0) ? IN_LOOP : NOT_IN_LOOP;
  CallFunctionStub stub(arg_count, in_loop, RECEIVER_MIGHT_BE_VALUE);
  __ CallStub(&stub);
  // Restore context register.
  __ lw(cp, MemOperand(fp, StandardFrameConstants::kContextOffset));
  context()->DropAndPlug(1, v0);
}


void FullCodeGenerator::VisitCall(Call* expr) {
  Comment cmnt(masm_, "[ Call");
  Expression* fun = expr->expression();
  Variable* var = fun->AsVariableProxy()->AsVariable();

  if (var != NULL && var->is_possibly_eval()) {
    // In a call to eval, we first call %ResolvePossiblyDirectEval to
    // resolve the function we need to call and the receiver of the
    // call.  Then we call the resolved function using the given
    // arguments.
    ZoneList<Expression*>* args = expr->arguments();
    int arg_count = args->length();

    { PreservePositionScope pos_scope(masm()->positions_recorder());
      VisitForStackValue(fun);
      __ LoadRoot(a2, Heap::kUndefinedValueRootIndex);
      __ push(a2);  // Reserved receiver slot.

      // Push the arguments.
      for (int i = 0; i < arg_count; i++) {
        VisitForStackValue(args->at(i));
      }

      // Push copy of the function - found below the arguments.
      __ lw(a1, MemOperand(sp, (arg_count + 1) * kPointerSize));
      __ push(a1);

      // Push copy of the first argument or undefined if it doesn't exist.
      if (arg_count > 0) {
        __ lw(a1, MemOperand(sp, arg_count * kPointerSize));
        __ push(a1);
      } else {
        __ push(a2);
      }

      // Push the receiver of the enclosing function and do runtime call.
      __ lw(a1,
            MemOperand(fp, (2 + scope()->num_parameters()) * kPointerSize));
      __ push(a1);
      __ CallRuntime(Runtime::kResolvePossiblyDirectEval, 3);

      // The runtime call returns a pair of values in v0 (function) and
      // v1 (receiver). Touch up the stack with the right values.
      __ sw(v0, MemOperand(sp, (arg_count + 1) * kPointerSize));
      __ sw(v1, MemOperand(sp, arg_count * kPointerSize));
    }
    // Record source position for debugger.
    SetSourcePosition(expr->position());
    // Write posistions prior to subsequent call, to ensure RelocInfo ordering.
    __ positions_recorder()->WriteRecordedPositions();

    InLoopFlag in_loop = (loop_depth() > 0) ? IN_LOOP : NOT_IN_LOOP;
    CallFunctionStub stub(arg_count, in_loop, RECEIVER_MIGHT_BE_VALUE);
    __ CallStub(&stub);
    // Restore context register.
    __ lw(cp, MemOperand(fp, StandardFrameConstants::kContextOffset));
    context()->DropAndPlug(1, v0);
  } else if (var != NULL && !var->is_this() && var->is_global()) {
    // Push global object as receiver for the call IC.
    __ lw(a0, GlobalObjectOperand());
    __ push(a0);
    EmitCallWithIC(expr, var->name(), RelocInfo::CODE_TARGET_CONTEXT);
  } else if (var != NULL && var->AsSlot() != NULL &&
             var->AsSlot()->type() == Slot::LOOKUP) {
    // Call to a lookup slot (dynamically introduced variable).
    Label slow, done;

    { PreservePositionScope scope(masm()->positions_recorder());
      // Generate code for loading from variables potentially shadowed
      // by eval-introduced variables.
      EmitDynamicLoadFromSlotFastCase(var->AsSlot(),
                                      NOT_INSIDE_TYPEOF,
                                      &slow,
                                      &done);
    }

    __ bind(&slow);
    // Call the runtime to find the function to call (returned in v0)
    // and the object holding it (returned in v1).
    __ push(context_register());
    __ li(a2, Operand(var->name()));
    __ push(a2);
    __ CallRuntime(Runtime::kLoadContextSlot, 2);
    __ Push(v0, v1);  // Function, receiver.

    // If fast case code has been generated, emit code to push the
    // function and receiver and have the slow path jump around this
    // code.
    if (done.is_linked()) {
      Label call;
      __ Branch(&call);
      __ bind(&done);
      // Push function.
      __ push(v0);
      // Push global receiver.
      __ lw(a1, GlobalObjectOperand());
      __ lw(a1, FieldMemOperand(a1, GlobalObject::kGlobalReceiverOffset));
      __ push(a1);
      __ bind(&call);
    }

    EmitCallWithStub(expr);
  } else if (fun->AsProperty() != NULL) {
    // Call to an object property.
    Property* prop = fun->AsProperty();
    Literal* key = prop->key()->AsLiteral();
    if (key != NULL && key->handle()->IsSymbol()) {
      // Call to a named property, use call IC.
      { PreservePositionScope scope(masm()->positions_recorder());
        VisitForStackValue(prop->obj());
      }
      EmitCallWithIC(expr, key->handle(), RelocInfo::CODE_TARGET);
    } else {
      // Call to a keyed property.
      // For a synthetic property use keyed load IC followed by function call,
      // for a regular property use keyed CallIC.
      { PreservePositionScope scope(masm()->positions_recorder());
        VisitForStackValue(prop->obj());
      }
      if (prop->is_synthetic()) {
        { PreservePositionScope scope(masm()->positions_recorder());
          VisitForAccumulatorValue(prop->key());
        }
        __ mov(a0, result_register());
        // Record source code position for IC call.
        SetSourcePosition(prop->position());
        // Write posistions prior to subsequent call, to ensure RelocInfo ordering.
        __ positions_recorder()->WriteRecordedPositions();
        __ pop(a1);  // We do not need to keep the receiver.

        Handle<Code> ic(Builtins::builtin(Builtins::KeyedLoadIC_Initialize));
        EmitCallIC(ic, RelocInfo::CODE_TARGET);
        __ lw(a1, GlobalObjectOperand());
        __ lw(a1, FieldMemOperand(a1, GlobalObject::kGlobalReceiverOffset));
        __ Push(v0, a1);  // Function, receiver.
        EmitCallWithStub(expr);
      } else {
        EmitKeyedCallWithIC(expr, prop->key(), RelocInfo::CODE_TARGET);
      }
    }
  } else {
    // Call to some other expression.  If the expression is an anonymous
    // function literal not called in a loop, mark it as one that should
    // also use the fast code generator.
    FunctionLiteral* lit = fun->AsFunctionLiteral();
    if (lit != NULL &&
        lit->name()->Equals(Heap::empty_string()) &&
        loop_depth() == 0) {
      lit->set_try_full_codegen(true);
    }

    { PreservePositionScope scope(masm()->positions_recorder());
      VisitForStackValue(fun);
    }
    // Load global receiver object.
    __ lw(a1, GlobalObjectOperand());
    __ lw(a1, FieldMemOperand(a1, GlobalObject::kGlobalReceiverOffset));
    __ push(a1);
    // Emit function call.
    EmitCallWithStub(expr);
  }
}


void FullCodeGenerator::VisitCallNew(CallNew* expr) {
  Comment cmnt(masm_, "[ CallNew");
  // According to ECMA-262, section 11.2.2, page 44, the function
  // expression in new calls must be evaluated before the
  // arguments.

  // Push constructor on the stack.  If it's not a function it's used as
  // receiver for CALL_NON_FUNCTION, otherwise the value on the stack is
  // ignored.
  VisitForStackValue(expr->expression());

  // Push the arguments ("left-to-right") on the stack.
  ZoneList<Expression*>* args = expr->arguments();
  int arg_count = args->length();
  for (int i = 0; i < arg_count; i++) {
    VisitForStackValue(args->at(i));
  }

  // Call the construct call builtin that handles allocation and
  // constructor invocation.
  SetSourcePosition(expr->position());
  // Write posistions prior to subsequent call, to ensure RelocInfo ordering.
  __ positions_recorder()->WriteRecordedPositions();

  // Load function and argument count into a1 and a0.
  __ li(a0, Operand(arg_count));
  __ lw(a1, MemOperand(sp, arg_count * kPointerSize));

  Handle<Code> construct_builtin(Builtins::builtin(Builtins::JSConstructCall));
  __ Call(construct_builtin, RelocInfo::CONSTRUCT_CALL);
  context()->Plug(v0);
}


void FullCodeGenerator::EmitIsSmi(ZoneList<Expression*>* args) {
  ASSERT(args->length() == 1);

  VisitForAccumulatorValue(args->at(0));

  Label materialize_true, materialize_false;
  Label* if_true = NULL;
  Label* if_false = NULL;
  Label* fall_through = NULL;
  context()->PrepareTest(&materialize_true, &materialize_false,
                         &if_true, &if_false, &fall_through);

  __ BranchOnSmi(v0, if_true);
  __ Branch(if_false);

  context()->Plug(if_true, if_false);
}


void FullCodeGenerator::EmitIsNonNegativeSmi(ZoneList<Expression*>* args) {
  ASSERT(args->length() == 1);

  VisitForAccumulatorValue(args->at(0));

  Label materialize_true, materialize_false;
  Label* if_true = NULL;
  Label* if_false = NULL;
  Label* fall_through = NULL;
  context()->PrepareTest(&materialize_true, &materialize_false,
                         &if_true, &if_false, &fall_through);

  __ And(at, v0, Operand(kSmiTagMask | 0x80000000));
  Split(eq, at, Operand(zero_reg), if_true, if_false, fall_through);

  context()->Plug(if_true, if_false);
}


void FullCodeGenerator::EmitIsObject(ZoneList<Expression*>* args) {
  ASSERT(args->length() == 1);

  VisitForAccumulatorValue(args->at(0));

  Label materialize_true, materialize_false;
  Label* if_true = NULL;
  Label* if_false = NULL;
  Label* fall_through = NULL;
  context()->PrepareTest(&materialize_true, &materialize_false,
                         &if_true, &if_false, &fall_through);

  __ BranchOnSmi(v0, if_false);
  __ LoadRoot(at, Heap::kNullValueRootIndex);
  __ Branch(if_true, eq, v0, Operand(at));
  __ lw(a2, FieldMemOperand(v0, HeapObject::kMapOffset));
  // Undetectable objects behave like undefined when tested with typeof.
  __ lbu(a1, FieldMemOperand(a2, Map::kBitFieldOffset));
  __ And(at, a1, Operand(1 << Map::kIsUndetectable));
  __ Branch(if_false, ne, at, Operand(zero_reg));
  __ lbu(a1, FieldMemOperand(a2, Map::kInstanceTypeOffset));
  __ Branch(if_false, lt, a1, Operand(FIRST_JS_OBJECT_TYPE));
  Split(le, a1, Operand(LAST_JS_OBJECT_TYPE), if_true, if_false, fall_through);

  context()->Plug(if_true, if_false);
}


void FullCodeGenerator::EmitIsSpecObject(ZoneList<Expression*>* args) {
  ASSERT(args->length() == 1);

  VisitForAccumulatorValue(args->at(0));

  Label materialize_true, materialize_false;
  Label* if_true = NULL;
  Label* if_false = NULL;
  Label* fall_through = NULL;
  context()->PrepareTest(&materialize_true, &materialize_false,
                         &if_true, &if_false, &fall_through);

  __ BranchOnSmi(v0, if_false);
  __ GetObjectType(v0, a1, a1);
  Split(ge, a1, Operand(FIRST_JS_OBJECT_TYPE),
        if_true, if_false, fall_through);

  context()->Plug(if_true, if_false);
}


void FullCodeGenerator::EmitIsUndetectableObject(ZoneList<Expression*>* args) {
  ASSERT(args->length() == 1);

  VisitForAccumulatorValue(args->at(0));

  Label materialize_true, materialize_false;
  Label* if_true = NULL;
  Label* if_false = NULL;
  Label* fall_through = NULL;
  context()->PrepareTest(&materialize_true, &materialize_false,
                         &if_true, &if_false, &fall_through);

  __ BranchOnSmi(v0, if_false);
  __ lw(a1, FieldMemOperand(v0, HeapObject::kMapOffset));
  __ lbu(a1, FieldMemOperand(a1, Map::kBitFieldOffset));
  __ And(at, a1, Operand(1 << Map::kIsUndetectable));
  Split(ne, at, Operand(zero_reg), if_true, if_false, fall_through);

  context()->Plug(if_true, if_false);
}


void FullCodeGenerator::EmitIsStringWrapperSafeForDefaultValueOf(
    ZoneList<Expression*>* args) {

  ASSERT(args->length() == 1);

  VisitForAccumulatorValue(args->at(0));

  Label materialize_true, materialize_false;
  Label* if_true = NULL;
  Label* if_false = NULL;
  Label* fall_through = NULL;
  context()->PrepareTest(&materialize_true, &materialize_false,
                         &if_true, &if_false, &fall_through);

  // Just indicate false, as %_IsStringWrapperSafeForDefaultValueOf() is only
  // used in a few functions in runtime.js which should not normally be hit by
  // this compiler.
  __ jmp(if_false);
  context()->Plug(if_true, if_false);
}


void FullCodeGenerator::EmitIsFunction(ZoneList<Expression*>* args) {
  ASSERT(args->length() == 1);

  VisitForAccumulatorValue(args->at(0));

  Label materialize_true, materialize_false;
  Label* if_true = NULL;
  Label* if_false = NULL;
  Label* fall_through = NULL;
  context()->PrepareTest(&materialize_true, &materialize_false,
                         &if_true, &if_false, &fall_through);

  __ BranchOnSmi(v0, if_false);
  __ GetObjectType(v0, a1, a2);
  __ Branch(if_true, eq, a2, Operand(JS_FUNCTION_TYPE));
  __ Branch(if_false);

  context()->Plug(if_true, if_false);
}


void FullCodeGenerator::EmitIsArray(ZoneList<Expression*>* args) {
  ASSERT(args->length() == 1);

  VisitForAccumulatorValue(args->at(0));

  Label materialize_true, materialize_false;
  Label* if_true = NULL;
  Label* if_false = NULL;
  Label* fall_through = NULL;
  context()->PrepareTest(&materialize_true, &materialize_false,
                         &if_true, &if_false, &fall_through);

  __ BranchOnSmi(v0, if_false);
  __ GetObjectType(v0, a1, a1);
  Split(eq, a1, Operand(JS_ARRAY_TYPE),
        if_true, if_false, fall_through);

  context()->Plug(if_true, if_false);
}


void FullCodeGenerator::EmitIsRegExp(ZoneList<Expression*>* args) {
  ASSERT(args->length() == 1);

  VisitForAccumulatorValue(args->at(0));

  Label materialize_true, materialize_false;
  Label* if_true = NULL;
  Label* if_false = NULL;
  Label* fall_through = NULL;
  context()->PrepareTest(&materialize_true, &materialize_false,
                         &if_true, &if_false, &fall_through);

  __ BranchOnSmi(v0, if_false);
  __ GetObjectType(v0, a1, a1);
  Split(eq, a1, Operand(JS_REGEXP_TYPE), if_true, if_false, fall_through);

  context()->Plug(if_true, if_false);
}


void FullCodeGenerator::EmitIsConstructCall(ZoneList<Expression*>* args) {
  ASSERT(args->length() == 0);

  Label materialize_true, materialize_false;
  Label* if_true = NULL;
  Label* if_false = NULL;
  Label* fall_through = NULL;
  context()->PrepareTest(&materialize_true, &materialize_false,
                         &if_true, &if_false, &fall_through);

  // Get the frame pointer for the calling frame.
  __ lw(a2, MemOperand(fp, StandardFrameConstants::kCallerFPOffset));

  // Skip the arguments adaptor frame if it exists.
  Label check_frame_marker;
  __ lw(a1, MemOperand(a2, StandardFrameConstants::kContextOffset));
  __ Branch(&check_frame_marker, ne,
            a1, Operand(Smi::FromInt(StackFrame::ARGUMENTS_ADAPTOR)));
  __ lw(a2, MemOperand(a2, StandardFrameConstants::kCallerFPOffset));

  // Check the marker in the calling frame.
  __ bind(&check_frame_marker);
  __ lw(a1, MemOperand(a2, StandardFrameConstants::kMarkerOffset));
  Split(eq, a1, Operand(Smi::FromInt(StackFrame::CONSTRUCT)),
        if_true, if_false, fall_through);

  context()->Plug(if_true, if_false);
}


void FullCodeGenerator::EmitObjectEquals(ZoneList<Expression*>* args) {
  ASSERT(args->length() == 2);

  // Load the two objects into registers and perform the comparison.
  VisitForStackValue(args->at(0));
  VisitForAccumulatorValue(args->at(1));

  Label materialize_true, materialize_false;
  Label* if_true = NULL;
  Label* if_false = NULL;
  Label* fall_through = NULL;
  context()->PrepareTest(&materialize_true, &materialize_false,
                         &if_true, &if_false, &fall_through);

  __ pop(a1);
  Split(eq, v0, Operand(a1), if_true, if_false, fall_through);

  context()->Plug(if_true, if_false);
}


void FullCodeGenerator::EmitArguments(ZoneList<Expression*>* args) {
  ASSERT(args->length() == 1);

  // ArgumentsAccessStub expects the key in a1 and the formal
  // parameter count in a0.
  VisitForAccumulatorValue(args->at(0));
  __ mov(a1, v0);
  __ li(a0, Operand(Smi::FromInt(scope()->num_parameters())));
  ArgumentsAccessStub stub(ArgumentsAccessStub::READ_ELEMENT);
  __ CallStub(&stub);
  context()->Plug(v0);
}


void FullCodeGenerator::EmitArgumentsLength(ZoneList<Expression*>* args) {
  ASSERT(args->length() == 0);

  Label exit;
  // Get the number of formal parameters.
  __ li(v0, Operand(Smi::FromInt(scope()->num_parameters())));

  // Check if the calling frame is an arguments adaptor frame.
  __ lw(a2, MemOperand(fp, StandardFrameConstants::kCallerFPOffset));
  __ lw(a3, MemOperand(a2, StandardFrameConstants::kContextOffset));
  __ Branch(&exit, ne, a3,
            Operand(Smi::FromInt(StackFrame::ARGUMENTS_ADAPTOR)));

  // Arguments adaptor case: Read the arguments length from the
  // adaptor frame.
  __ lw(v0, MemOperand(a2, ArgumentsAdaptorFrameConstants::kLengthOffset));

  __ bind(&exit);
  context()->Plug(v0);
}


void FullCodeGenerator::EmitClassOf(ZoneList<Expression*>* args) {
  ASSERT(args->length() == 1);
  Label done, null, function, non_function_constructor;

  VisitForAccumulatorValue(args->at(0));

  // If the object is a smi, we return null.
  __ BranchOnSmi(v0, &null);

  // Check that the object is a JS object but take special care of JS
  // functions to make sure they have 'Function' as their class.
  __ GetObjectType(v0, v0, a1);  // Map is now in v0.
  __ Branch(&null, lt, a1, Operand(FIRST_JS_OBJECT_TYPE));

  // As long as JS_FUNCTION_TYPE is the last instance type and it is
  // right after LAST_JS_OBJECT_TYPE, we can avoid checking for
  // LAST_JS_OBJECT_TYPE.
  ASSERT(LAST_TYPE == JS_FUNCTION_TYPE);
  ASSERT(JS_FUNCTION_TYPE == LAST_JS_OBJECT_TYPE + 1);
  __ Branch(&function, eq, a1, Operand(JS_FUNCTION_TYPE));

  // Check if the constructor in the map is a function.
  __ lw(v0, FieldMemOperand(v0, Map::kConstructorOffset));
  __ GetObjectType(v0, a1, a1);
  __ Branch(&non_function_constructor, ne, a1, Operand(JS_FUNCTION_TYPE));

  // v0 now contains the constructor function. Grab the
  // instance class name from there.
  __ lw(v0, FieldMemOperand(v0, JSFunction::kSharedFunctionInfoOffset));
  __ lw(v0, FieldMemOperand(v0, SharedFunctionInfo::kInstanceClassNameOffset));
  __ Branch(&done);

  // Functions have class 'Function'.
  __ bind(&function);
  __ LoadRoot(v0, Heap::kfunction_class_symbolRootIndex);
  __ jmp(&done);

  // Objects with a non-function constructor have class 'Object'.
  __ bind(&non_function_constructor);
  __ LoadRoot(v0, Heap::kfunction_class_symbolRootIndex);
  __ jmp(&done);

  // Non-JS objects have class null.
  __ bind(&null);
  __ LoadRoot(v0, Heap::kNullValueRootIndex);

  // All done.
  __ bind(&done);

  context()->Plug(v0);
}


void FullCodeGenerator::EmitLog(ZoneList<Expression*>* args) {
  // Conditionally generate a log call.
  // Args:
  //   0 (literal string): The type of logging (corresponds to the flags).
  //     This is used to determine whether or not to generate the log call.
  //   1 (string): Format string.  Access the string at argument index 2
  //     with '%2s' (see Logger::LogRuntime for all the formats).
  //   2 (array): Arguments to the format string.
  ASSERT_EQ(args->length(), 3);
#ifdef ENABLE_LOGGING_AND_PROFILING
  if (CodeGenerator::ShouldGenerateLog(args->at(0))) {
    VisitForStackValue(args->at(1));
    VisitForStackValue(args->at(2));
    __ CallRuntime(Runtime::kLog, 2);
  }
#endif
  // Finally, we're expected to leave a value on the top of the stack.
  __ LoadRoot(v0, Heap::kUndefinedValueRootIndex);
  context()->Plug(v0);
}


void FullCodeGenerator::EmitRandomHeapNumber(ZoneList<Expression*>* args) {
  ASSERT(args->length() == 0);

  Label slow_allocate_heapnumber;
  Label heapnumber_allocated;

  // Save the new heap number in callee-saved register s0, since
  // we call out to external C code below.
  __ LoadRoot(t6, Heap::kHeapNumberMapRootIndex);
  __ AllocateHeapNumber(s0, a1, a2, t6, &slow_allocate_heapnumber);
  __ jmp(&heapnumber_allocated);

  __ bind(&slow_allocate_heapnumber);

  // Allocate a heap number.
  __ CallRuntime(Runtime::kNumberAlloc, 0);
  __ mov(s0, v0);   // Save result in s0, so it is saved thru CFunc call.

  __ bind(&heapnumber_allocated);

  // Convert 32 random bits in v0 to 0.(32 random bits) in a double
  // by computing:
  // ( 1.(20 0s)(32 random bits) x 2^20 ) - (1.0 x 2^20)).
  if (CpuFeatures::IsSupported(FPU)) {
    __ PrepareCallCFunction(0, a1);
    __ CallCFunction(ExternalReference::random_uint32_function(), 0);

    CpuFeatures::Scope scope(FPU);
    // 0x41300000 is the top half of 1.0 x 2^20 as a double.
    __ li(a1, Operand(0x41300000));
    // Move 0x41300000xxxxxxxx (x = random bits in v0) to FPU.
    __ mtc1(a1, f13);
    __ mtc1(v0, f12);
    // Move 0x4130000000000000 to FPU.
    __ mtc1(a1, f15);
    __ mtc1(zero_reg, f14);
    // Subtract and store the result in the heap number.
    __ sub_d(f0, f12, f14);
    __ sdc1(f0, MemOperand(s0, HeapNumber::kValueOffset - kHeapObjectTag));
    __ mov(v0, s0);
  } else {
    __ mov(a0, s0);
    __ PrepareCallCFunction(1, a1);
    __ CallCFunction(
        ExternalReference::fill_heap_number_with_random_function(), 1);
  }

  context()->Plug(v0);
}


void FullCodeGenerator::EmitSubString(ZoneList<Expression*>* args) {
  // Load the arguments on the stack and call the stub.
  SubStringStub stub;
  ASSERT(args->length() == 3);
  VisitForStackValue(args->at(0));
  VisitForStackValue(args->at(1));
  VisitForStackValue(args->at(2));
  __ CallStub(&stub);
  context()->Plug(v0);
}


void FullCodeGenerator::EmitRegExpExec(ZoneList<Expression*>* args) {
  // Load the arguments on the stack and call the stub.
  RegExpExecStub stub;
  ASSERT(args->length() == 4);
  VisitForStackValue(args->at(0));
  VisitForStackValue(args->at(1));
  VisitForStackValue(args->at(2));
  VisitForStackValue(args->at(3));
  __ CallStub(&stub);
  context()->Plug(v0);
}


void FullCodeGenerator::EmitValueOf(ZoneList<Expression*>* args) {
  ASSERT(args->length() == 1);

  VisitForAccumulatorValue(args->at(0));  // Load the object.

  Label done;
  // If the object is a smi return the object.
  __ BranchOnSmi(v0, &done);
  // If the object is not a value type, return the object.
  __ GetObjectType(v0, a1, a1);
  __ Branch(&done, ne, a1, Operand(JS_VALUE_TYPE));

  __ lw(v0, FieldMemOperand(v0, JSValue::kValueOffset));

  __ bind(&done);
  context()->Plug(v0);
}


void FullCodeGenerator::EmitMathPow(ZoneList<Expression*>* args) {
  // Load the arguments on the stack and call the runtime function.
  ASSERT(args->length() == 2);
  VisitForStackValue(args->at(0));
  VisitForStackValue(args->at(1));
  __ CallRuntime(Runtime::kMath_pow, 2);
  context()->Plug(v0);
}


void FullCodeGenerator::EmitSetValueOf(ZoneList<Expression*>* args) {
  ASSERT(args->length() == 2);

  VisitForStackValue(args->at(0));  // Load the object.
  VisitForAccumulatorValue(args->at(1));  // Load the value.
  __ pop(a1);  // v0 = value. a1 = object.

  Label done;
  // If the object is a smi, return the value.
  __ BranchOnSmi(a1, &done);

  // If the object is not a value type, return the value.
  __ GetObjectType(a1, a2, a2);
  __ Branch(&done, ne, a2, Operand(JS_VALUE_TYPE));

  // Store the value.
  __ sw(v0, FieldMemOperand(a1, JSValue::kValueOffset));
  // Update the write barrier.  Save the value as it will be
  // overwritten by the write barrier code and is needed afterward.
  __ RecordWrite(a1, Operand(JSValue::kValueOffset - kHeapObjectTag), a2, a3);

  __ bind(&done);
  context()->Plug(v0);
}


void FullCodeGenerator::EmitNumberToString(ZoneList<Expression*>* args) {
  ASSERT_EQ(args->length(), 1);

  // Load the argument on the stack and call the stub.
  VisitForStackValue(args->at(0));

  NumberToStringStub stub;
  __ CallStub(&stub);
  context()->Plug(v0);
}


void FullCodeGenerator::EmitStringCharFromCode(ZoneList<Expression*>* args) {
  ASSERT(args->length() == 1);

  VisitForAccumulatorValue(args->at(0));

  Label done;
  StringCharFromCodeGenerator generator(v0, a1);
  generator.GenerateFast(masm_);
  __ jmp(&done);

  NopRuntimeCallHelper call_helper;
  generator.GenerateSlow(masm_, call_helper);

  __ bind(&done);
  context()->Plug(a1);
}


void FullCodeGenerator::EmitStringCharCodeAt(ZoneList<Expression*>* args) {
  ASSERT(args->length() == 2);

  VisitForStackValue(args->at(0));
  VisitForAccumulatorValue(args->at(1));
  __ mov(a0, result_register());

  Register object = a1;
  Register index = a0;
  Register scratch = a2;
  Register result = v0;

  __ pop(object);

  Label need_conversion;
  Label index_out_of_range;
  Label done;
  StringCharCodeAtGenerator generator(object,
                                      index,
                                      scratch,
                                      result,
                                      &need_conversion,
                                      &need_conversion,
                                      &index_out_of_range,
                                      STRING_INDEX_IS_NUMBER);
  generator.GenerateFast(masm_);
  __ jmp(&done);

  __ bind(&index_out_of_range);
  // When the index is out of range, the spec requires us to return
  // NaN.
  __ LoadRoot(result, Heap::kNanValueRootIndex);
  __ jmp(&done);

  __ bind(&need_conversion);
  // Load the undefined value into the result register, which will
  // trigger conversion.
  __ LoadRoot(result, Heap::kUndefinedValueRootIndex);
  __ jmp(&done);

  NopRuntimeCallHelper call_helper;
  generator.GenerateSlow(masm_, call_helper);

  __ bind(&done);
  context()->Plug(result);
}


void FullCodeGenerator::EmitStringCharAt(ZoneList<Expression*>* args) {
  ASSERT(args->length() == 2);

  VisitForStackValue(args->at(0));
  VisitForAccumulatorValue(args->at(1));
  __ mov(a0, result_register());

  Register object = a1;
  Register index = a0;
  Register scratch1 = a2;
  Register scratch2 = a3;
  Register result = v0;

  __ pop(object);

  Label need_conversion;
  Label index_out_of_range;
  Label done;
  StringCharAtGenerator generator(object,
                                  index,
                                  scratch1,
                                  scratch2,
                                  result,
                                  &need_conversion,
                                  &need_conversion,
                                  &index_out_of_range,
                                  STRING_INDEX_IS_NUMBER);
  generator.GenerateFast(masm_);
  __ jmp(&done);

  __ bind(&index_out_of_range);
  // When the index is out of range, the spec requires us to return
  // the empty string.
  __ LoadRoot(result, Heap::kEmptyStringRootIndex);
  __ jmp(&done);

  __ bind(&need_conversion);
  // Move smi zero into the result register, which will trigger
  // conversion.
  __ li(result, Operand(Smi::FromInt(0)));
  __ jmp(&done);

  NopRuntimeCallHelper call_helper;
  generator.GenerateSlow(masm_, call_helper);

  __ bind(&done);
  context()->Plug(result);
}


void FullCodeGenerator::EmitStringAdd(ZoneList<Expression*>* args) {
  ASSERT_EQ(2, args->length());

  VisitForStackValue(args->at(0));
  VisitForStackValue(args->at(1));

  StringAddStub stub(NO_STRING_ADD_FLAGS);
  __ CallStub(&stub);
  context()->Plug(v0);
}


void FullCodeGenerator::EmitStringCompare(ZoneList<Expression*>* args) {
  ASSERT_EQ(2, args->length());

  VisitForStackValue(args->at(0));
  VisitForStackValue(args->at(1));

  StringCompareStub stub;
  __ CallStub(&stub);
  context()->Plug(v0);
}


void FullCodeGenerator::EmitMathSin(ZoneList<Expression*>* args) {
  // Load the argument on the stack and call the runtime.
  ASSERT(args->length() == 1);
  VisitForStackValue(args->at(0));
  __ CallRuntime(Runtime::kMath_sin, 1);
  context()->Plug(v0);
}


void FullCodeGenerator::EmitMathCos(ZoneList<Expression*>* args) {
  // Load the argument on the stack and call the runtime.
  ASSERT(args->length() == 1);
  VisitForStackValue(args->at(0));
  __ CallRuntime(Runtime::kMath_cos, 1);
  context()->Plug(v0);
}


void FullCodeGenerator::EmitMathSqrt(ZoneList<Expression*>* args) {
  // Load the argument on the stack and call the runtime function.
  ASSERT(args->length() == 1);
  VisitForStackValue(args->at(0));
  __ CallRuntime(Runtime::kMath_sqrt, 1);
  context()->Plug(v0);
}


void FullCodeGenerator::EmitMathLog(ZoneList<Expression*>* args) {
  // Load the argument on the stack and call the runtime function.
  ASSERT(args->length() == 1);
  VisitForStackValue(args->at(0));
  __ CallRuntime(Runtime::kMath_log, 1);
  context()->Plug(v0);
}


void FullCodeGenerator::EmitCallFunction(ZoneList<Expression*>* args) {
  ASSERT(args->length() >= 2);

  int arg_count = args->length() - 2;  // For receiver and function.
  VisitForStackValue(args->at(0));  // Receiver.
  for (int i = 0; i < arg_count; i++) {
    VisitForStackValue(args->at(i + 1));
  }
  VisitForAccumulatorValue(args->at(arg_count + 1));  // Function.

  // InvokeFunction requires function in a1. Move it in there.
  if (!result_register().is(a1)) __ mov(a1, result_register());
  ParameterCount count(arg_count);
  __ InvokeFunction(a1, count, CALL_FUNCTION);
  __ lw(cp, MemOperand(fp, StandardFrameConstants::kContextOffset));
  context()->Plug(v0);
}


void FullCodeGenerator::EmitRegExpConstructResult(ZoneList<Expression*>* args) {
  ASSERT(args->length() == 3);
  VisitForStackValue(args->at(0));
  VisitForStackValue(args->at(1));
  VisitForStackValue(args->at(2));
  __ CallRuntime(Runtime::kRegExpConstructResult, 3);
  context()->Plug(v0);
}


void FullCodeGenerator::EmitSwapElements(ZoneList<Expression*>* args) {
  ASSERT(args->length() == 3);
  VisitForStackValue(args->at(0));
  VisitForStackValue(args->at(1));
  VisitForStackValue(args->at(2));
  __ CallRuntime(Runtime::kSwapElements, 3);
  context()->Plug(v0);
}


void FullCodeGenerator::EmitGetFromCache(ZoneList<Expression*>* args) {
  ASSERT_EQ(2, args->length());

  ASSERT_NE(NULL, args->at(0)->AsLiteral());
  int cache_id = Smi::cast(*(args->at(0)->AsLiteral()->handle()))->value();

  Handle<FixedArray> jsfunction_result_caches(
      Top::global_context()->jsfunction_result_caches());
  if (jsfunction_result_caches->length() <= cache_id) {
    __ Abort("Attempt to use undefined cache.");
    __ LoadRoot(v0, Heap::kUndefinedValueRootIndex);
    context()->Plug(v0);
    return;
  }

  VisitForAccumulatorValue(args->at(1));

  Register key = v0;
  Register cache = a1;
  __ lw(cache, ContextOperand(cp, Context::GLOBAL_INDEX));
  __ lw(cache, FieldMemOperand(cache, GlobalObject::kGlobalContextOffset));
  __ lw(cache,
         ContextOperand(
             cache, Context::JSFUNCTION_RESULT_CACHES_INDEX));
  __ lw(cache,
         FieldMemOperand(cache, FixedArray::OffsetOfElementAt(cache_id)));


  Label done, not_found;
  ASSERT(kSmiTag == 0 && kSmiTagSize == 1);
  __ lw(a2, FieldMemOperand(cache, JSFunctionResultCache::kFingerOffset));
  // a2 now holds finger offset as a smi.
  __ Addu(a3, cache, Operand(FixedArray::kHeaderSize - kHeapObjectTag));
  // a3 now points to the start of fixed array elements.
  __ sll(at, a2, kPointerSizeLog2 - kSmiTagSize);
  __ addu(a3, a3, at);
  // a3 now points to key of indexed element of cache.
  __ lw(a2, MemOperand(a3));
  __ Branch(&not_found, ne, key, Operand(a2));

  __ lw(v0, MemOperand(a3, kPointerSize));
  __ Branch(&done);

  __ bind(&not_found);
  // Call runtime to perform the lookup.
  __ Push(cache, key);
  __ CallRuntime(Runtime::kGetFromCache, 2);

  __ bind(&done);
  context()->Plug(v0);
}


void FullCodeGenerator::EmitIsRegExpEquivalent(ZoneList<Expression*>* args) {
  ASSERT_EQ(2, args->length());

  Register right = v0;
  Register left = a1;
  Register tmp = a2;
  Register tmp2 = a3;

  VisitForStackValue(args->at(0));
  VisitForAccumulatorValue(args->at(1));  // Result (right) in v0.
  __ pop(left);

  Label done, fail, ok;
  __ Branch(&ok, eq, left, Operand(right));
  // Fail if either is a non-HeapObject.
  __ And(tmp, left, Operand(right));
  __ And(at, tmp, Operand(kSmiTagMask));
  __ Branch(&fail, eq, at, Operand(zero_reg));
  __ lw(tmp, FieldMemOperand(left, HeapObject::kMapOffset));
  __ lbu(tmp2, FieldMemOperand(tmp, Map::kInstanceTypeOffset));
  __ Branch(&fail, ne, tmp2, Operand(JS_REGEXP_TYPE));
  __ lw(tmp2, FieldMemOperand(right, HeapObject::kMapOffset));
  __ Branch(&fail, ne, tmp, Operand(tmp2));
  __ lw(tmp, FieldMemOperand(left, JSRegExp::kDataOffset));
  __ lw(tmp2, FieldMemOperand(right, JSRegExp::kDataOffset));
  __ Branch(&ok, eq, tmp, Operand(tmp2));
  __ bind(&fail);
  __ LoadRoot(v0, Heap::kFalseValueRootIndex);
  __ jmp(&done);
  __ bind(&ok);
  __ LoadRoot(v0, Heap::kTrueValueRootIndex);
  __ bind(&done);

  context()->Plug(v0);
}


void FullCodeGenerator::EmitHasCachedArrayIndex(ZoneList<Expression*>* args) {
  VisitForAccumulatorValue(args->at(0));

  Label materialize_true, materialize_false;
  Label* if_true = NULL;
  Label* if_false = NULL;
  Label* fall_through = NULL;
  context()->PrepareTest(&materialize_true, &materialize_false,
                         &if_true, &if_false, &fall_through);

  __ lw(a0, FieldMemOperand(v0, String::kHashFieldOffset));
  __ And(a0, a0, Operand(String::kContainsCachedArrayIndexMask));

  __ Branch(if_true, eq, a0, Operand(zero_reg));
  __ Branch(if_false);

  context()->Plug(if_true, if_false);
}


void FullCodeGenerator::EmitGetCachedArrayIndex(ZoneList<Expression*>* args) {
  ASSERT(args->length() == 1);
  VisitForAccumulatorValue(args->at(0));
  __ lw(v0, FieldMemOperand(v0, String::kHashFieldOffset));
  __ IndexFromHash(v0, v0);
  context()->Plug(v0);
}

void FullCodeGenerator::EmitFastAsciiArrayJoin(ZoneList<Expression*>* args) {
  __ LoadRoot(v0, Heap::kUndefinedValueRootIndex);
  context()->Plug(v0);
  return;
}


void FullCodeGenerator::VisitCallRuntime(CallRuntime* expr) {
  Handle<String> name = expr->name();
  if (name->length() > 0 && name->Get(0) == '_') {
    Comment cmnt(masm_, "[ InlineRuntimeCall");
    EmitInlineRuntimeCall(expr);
    return;
  }

  Comment cmnt(masm_, "[ CallRuntime");
  ZoneList<Expression*>* args = expr->arguments();

  if (expr->is_jsruntime()) {
    // Prepare for calling JS runtime function.
    __ lw(a0, GlobalObjectOperand());
    __ lw(a0, FieldMemOperand(a0, GlobalObject::kBuiltinsOffset));
    __ push(a0);
  }

  // Push the arguments ("left-to-right").
  int arg_count = args->length();
  for (int i = 0; i < arg_count; i++) {
    VisitForStackValue(args->at(i));
  }

  if (expr->is_jsruntime()) {
    // Call the JS runtime function.
    __ li(a2, Operand(expr->name()));
    Handle<Code> ic = StubCache::ComputeCallInitialize(arg_count, NOT_IN_LOOP);
    EmitCallIC(ic, RelocInfo::CODE_TARGET);
    // Restore context register.
    __ lw(cp, MemOperand(fp, StandardFrameConstants::kContextOffset));
  } else {
    // Call the C runtime function.
    __ CallRuntime(expr->function(), arg_count);
  }
  context()->Plug(v0);
}


void FullCodeGenerator::VisitUnaryOperation(UnaryOperation* expr) {
  switch (expr->op()) {
    case Token::DELETE: {
      Comment cmnt(masm_, "[ UnaryOperation (DELETE)");
      Property* prop = expr->expression()->AsProperty();
      Variable* var = expr->expression()->AsVariableProxy()->AsVariable();
      if (prop == NULL && var == NULL) {
        // Result of deleting non-property, non-variable reference is true.
        // The subexpression may have side effects.
        VisitForEffect(expr->expression());
        context()->Plug(true);
      } else if (var != NULL &&
                 !var->is_global() &&
                 var->AsSlot() != NULL &&
                 var->AsSlot()->type() != Slot::LOOKUP) {
        // Result of deleting non-global, non-dynamic variables is false.
        // The subexpression does not have side effects.
        context()->Plug(false);
      } else {
        // Property or variable reference.  Call the delete builtin with
        // object and property name as arguments.
        if (prop != NULL) {
          VisitForStackValue(prop->obj());
          VisitForStackValue(prop->key());
        } else if (var->is_global()) {
          __ lw(a1, GlobalObjectOperand());
          __ li(a0, Operand(var->name()));
          __ Push(a1, a0);
        } else {
          // Non-global variable.  Call the runtime to look up the context
          // where the variable was introduced.
          __ push(context_register());
          __ li(a2, Operand(var->name()));
          __ push(a2);
          __ CallRuntime(Runtime::kLookupContext, 2);
          __ push(v0);
          __ li(a2, Operand(var->name()));
          __ push(a2);
        }
        __ InvokeBuiltin(Builtins::DELETE, CALL_JS);
        context()->Plug(v0);
      }
      break;
    }

    case Token::VOID: {
      Comment cmnt(masm_, "[ UnaryOperation (VOID)");
      VisitForEffect(expr->expression());
      context()->Plug(Heap::kUndefinedValueRootIndex);
      break;
    }

    case Token::NOT: {
      Comment cmnt(masm_, "[ UnaryOperation (NOT)");
      Label materialize_true, materialize_false;
      Label* if_true = NULL;
      Label* if_false = NULL;
      Label* fall_through = NULL;

      // Notice that the labels are swapped.
      context()->PrepareTest(&materialize_true, &materialize_false,
                             &if_false, &if_true, &fall_through);

      VisitForControl(expr->expression(), if_true, if_false, fall_through);

      context()->Plug(if_false, if_true);  // Labels swapped.
      break;
    }

    case Token::TYPEOF: {
      Comment cmnt(masm_, "[ UnaryOperation (TYPEOF)");
      { StackValueContext context(this);
        VisitForTypeofValue(expr->expression());
      }
      __ CallRuntime(Runtime::kTypeof, 1);
      context()->Plug(v0);
      break;
    }

    case Token::ADD: {
      Comment cmt(masm_, "[ UnaryOperation (ADD)");
      VisitForAccumulatorValue(expr->expression());
      Label no_conversion;
      __ BranchOnSmi(result_register(), &no_conversion);
      __ mov(a0, result_register());
      __ push(a0);
      __ InvokeBuiltin(Builtins::TO_NUMBER, CALL_JS);
      __ bind(&no_conversion);
      context()->Plug(result_register());
      break;
    }

    case Token::SUB: {
      Comment cmt(masm_, "[ UnaryOperation (SUB)");
      bool can_overwrite = expr->expression()->ResultOverwriteAllowed();
      UnaryOverwriteMode overwrite =
          can_overwrite ? UNARY_OVERWRITE : UNARY_NO_OVERWRITE;
      GenericUnaryOpStub stub(Token::SUB,
                              overwrite,
                              NO_UNARY_FLAGS);
      // GenericUnaryOpStub expects the argument to be in the
      // register a0.
      VisitForAccumulatorValue(expr->expression());
      __ mov(a0, result_register());
      __ CallStub(&stub);
      context()->Plug(v0);
      break;
    }

    case Token::BIT_NOT: {
      Comment cmt(masm_, "[ UnaryOperation (BIT_NOT)");
      VisitForAccumulatorValue(expr->expression());
      Label done;
      bool inline_smi_code = ShouldInlineSmiCase(expr->op());
      if (inline_smi_code) {
        Label call_stub;
        __ BranchOnNotSmi(result_register(), &call_stub);
        // Invert all bits except Smi tag, which stays 0.
        __ Xor(result_register(), result_register(), 0xfffffffe);
        __ Branch(&done);
        __ bind(&call_stub);
      }
      bool overwrite = expr->expression()->ResultOverwriteAllowed();
      UnaryOpFlags flags = inline_smi_code
          ? NO_UNARY_SMI_CODE_IN_STUB
          : NO_UNARY_FLAGS;
      UnaryOverwriteMode mode =
          overwrite ? UNARY_OVERWRITE : UNARY_NO_OVERWRITE;
      GenericUnaryOpStub stub(Token::BIT_NOT, mode, flags);
      // GenericUnaryOpStub expects the argument to be in register a0.
      __ mov(a0, result_register());
      __ CallStub(&stub);
      __ bind(&done);
      context()->Plug(result_register());
      break;
    }

    default:
      UNREACHABLE();
  }
}


void FullCodeGenerator::VisitCountOperation(CountOperation* expr) {
  Comment cmnt(masm_, "[ CountOperation");
  SetSourcePosition(expr->position());
  // Write posistions prior to subsequent call, to ensure RelocInfo ordering.
  __ positions_recorder()->WriteRecordedPositions();

  // Invalid left-hand sides are rewritten to have a 'throw ReferenceError'
  // as the left-hand side.
  if (!expr->expression()->IsValidLeftHandSide()) {
    VisitForEffect(expr->expression());
    return;
  }

  // Expression can only be a property, a global or a (parameter or local)
  // slot. Variables with rewrite to .arguments are treated as KEYED_PROPERTY.
  enum LhsKind { VARIABLE, NAMED_PROPERTY, KEYED_PROPERTY };
  LhsKind assign_type = VARIABLE;
  Property* prop = expr->expression()->AsProperty();
  // In case of a property we use the uninitialized expression context
  // of the key to detect a named property.
  if (prop != NULL) {
    assign_type =
        (prop->key()->IsPropertyName()) ? NAMED_PROPERTY : KEYED_PROPERTY;
  }

  // Evaluate expression and get value.
  if (assign_type == VARIABLE) {
    ASSERT(expr->expression()->AsVariableProxy()->var() != NULL);
    AccumulatorValueContext context(this);
    EmitVariableLoad(expr->expression()->AsVariableProxy()->var());
  } else {
    // Reserve space for result of postfix operation.
    if (expr->is_postfix() && !context()->IsEffect()) {
      __ li(at, Operand(Smi::FromInt(0)));
      __ push(at);
    }
    if (assign_type == NAMED_PROPERTY) {
      // Put the object both on the stack and in the accumulator.
      VisitForAccumulatorValue(prop->obj());
      __ push(v0);
      EmitNamedPropertyLoad(prop);
    } else {
      VisitForStackValue(prop->obj());
      VisitForAccumulatorValue(prop->key());
      __ lw(a1, MemOperand(sp, 0));
      __ push(v0);
      EmitKeyedPropertyLoad(prop);
    }
  }
  // Call ToNumber only if operand is not a smi.
  Label no_conversion;
  __ BranchOnSmi(v0, &no_conversion);
  __ push(v0);
  __ InvokeBuiltin(Builtins::TO_NUMBER, CALL_JS);
  __ bind(&no_conversion);

  // Save result for postfix expressions.
  if (expr->is_postfix()) {
    if (!context()->IsEffect()) {
      // Save the result on the stack. If we have a named or keyed property
      // we store the result under the receiver that is currently on top
      // of the stack.
      switch (assign_type) {
        case VARIABLE:
          __ push(v0);
          break;
        case NAMED_PROPERTY:
          __ sw(v0, MemOperand(sp, kPointerSize));
          break;
        case KEYED_PROPERTY:
          __ sw(v0, MemOperand(sp, 2 * kPointerSize));
          break;
      }
    }
  }
  __ mov(a0, result_register());

  // Inline smi case if we are in a loop.
  Label stub_call, done;
  int count_value = expr->op() == Token::INC ? 1 : -1;
  __ li(a1, Operand(Smi::FromInt(count_value)));

  if (ShouldInlineSmiCase(expr->op())) {
    __ Addu(v0, a0, a1);

    // Check for overflow of a0 + smi:count_value (in a1).
    __ Xor(t0, v0, a0);
    __ Xor(t1, v0, a1);
    __ and_(t0, t0, t1);    // Overflow occurred if result is negative.
    __ Branch(&stub_call, lt, t0, Operand(zero_reg));  // Do stub on overflow.

    // We could eliminate this smi check if we split the code at
    // the first smi check before calling ToNumber.
    __ BranchOnSmi(v0, &done);
    __ bind(&stub_call);
  }

  GenericBinaryOpStub stub(Token::ADD, NO_OVERWRITE, a1, a0);
  __ CallStub(&stub);
  __ bind(&done);

  // Store the value returned in v0.
  switch (assign_type) {
    case VARIABLE:
      if (expr->is_postfix()) {
        { EffectContext context(this);
          EmitVariableAssignment(expr->expression()->AsVariableProxy()->var(),
                                 Token::ASSIGN);
        }
        // For all contexts except EffectConstant we have the result on
        // top of the stack.
        if (!context()->IsEffect()) {
          context()->PlugTOS();
        }
      } else {
        EmitVariableAssignment(expr->expression()->AsVariableProxy()->var(),
                               Token::ASSIGN);
      }
      break;
    case NAMED_PROPERTY: {
      __ mov(a0, result_register());  // Value.
      __ li(a2, Operand(prop->key()->AsLiteral()->handle()));  // Name.
      __ pop(a1);  // Receiver.
      Handle<Code> ic(Builtins::builtin(Builtins::StoreIC_Initialize));
      EmitCallIC(ic, RelocInfo::CODE_TARGET);
      if (expr->is_postfix()) {
        if (!context()->IsEffect()) {
          context()->PlugTOS();
        }
      } else {
        context()->Plug(v0);
      }
      break;
    }
    case KEYED_PROPERTY: {
      __ mov(a0, result_register());  // Value.
      __ pop(a1);  // Key.
      __ pop(a2);  // Receiver.
      Handle<Code> ic(Builtins::builtin(Builtins::KeyedStoreIC_Initialize));
      EmitCallIC(ic, RelocInfo::CODE_TARGET);
      if (expr->is_postfix()) {
        if (!context()->IsEffect()) {
          context()->PlugTOS();
        }
      } else {
        context()->Plug(v0);
      }
      break;
    }
  }
}


void FullCodeGenerator::VisitForTypeofValue(Expression* expr) {
  VariableProxy* proxy = expr->AsVariableProxy();
  if (proxy != NULL && !proxy->var()->is_this() && proxy->var()->is_global()) {
    Comment cmnt(masm_, "Global variable");
    __ lw(a0, GlobalObjectOperand());
    __ li(a2, Operand(proxy->name()));
    Handle<Code> ic(Builtins::builtin(Builtins::LoadIC_Initialize));
    // Use a regular load, not a contextual load, to avoid a reference
    // error.
    EmitCallIC(ic, RelocInfo::CODE_TARGET);
    context()->Plug(v0);
  } else if (proxy != NULL &&
             proxy->var()->AsSlot() != NULL &&
             proxy->var()->AsSlot()->type() == Slot::LOOKUP) {
    Label done, slow;

    // Generate code for loading from variables potentially shadowed
    // by eval-introduced variables.
    Slot* slot = proxy->var()->AsSlot();
    EmitDynamicLoadFromSlotFastCase(slot, INSIDE_TYPEOF, &slow, &done);

    __ bind(&slow);
    __ li(a0, Operand(proxy->name()));
    __ Push(cp, a0);
    __ CallRuntime(Runtime::kLoadContextSlotNoReferenceError, 2);
    __ bind(&done);

    context()->Plug(v0);
  } else {
    // This expression cannot throw a reference error at the top level.
    Visit(expr);
  }
}


bool FullCodeGenerator::TryLiteralCompare(Token::Value op,
                                          Expression* left,
                                          Expression* right,
                                          Label* if_true,
                                          Label* if_false,
                                          Label* fall_through) {
  if (op != Token::EQ && op != Token::EQ_STRICT) return false;

  // Check for the pattern: typeof <expression> == <string literal>.
  Literal* right_literal = right->AsLiteral();
  if (right_literal == NULL) return false;
  Handle<Object> right_literal_value = right_literal->handle();
  if (!right_literal_value->IsString()) return false;
  UnaryOperation* left_unary = left->AsUnaryOperation();
  if (left_unary == NULL || left_unary->op() != Token::TYPEOF) return false;
  Handle<String> check = Handle<String>::cast(right_literal_value);

  { AccumulatorValueContext context(this);
    VisitForTypeofValue(left_unary->expression());
  }
  if (check->Equals(Heap::number_symbol())) {
    __ And(at, v0, Operand(kSmiTagMask));
    __ Branch(if_true, eq, at, Operand(zero_reg));
    __ lw(v0, FieldMemOperand(v0, HeapObject::kMapOffset));
    __ LoadRoot(at, Heap::kHeapNumberMapRootIndex);
    Split(eq, v0, Operand(at), if_true, if_false, fall_through);
  } else if (check->Equals(Heap::string_symbol())) {
    __ And(at, v0, Operand(kSmiTagMask));
    __ Branch(if_false, eq, at, Operand(zero_reg));
    // Check for undetectable objects => false.
    __ lw(v0, FieldMemOperand(v0, HeapObject::kMapOffset));
    __ lbu(a1, FieldMemOperand(v0, Map::kBitFieldOffset));
    __ And(a1, a1, Operand(1 << Map::kIsUndetectable));
    __ Branch(if_false, ne, a1, Operand(zero_reg));
    __ lbu(a1, FieldMemOperand(v0, Map::kInstanceTypeOffset));
    Split(lt, a1, Operand(FIRST_NONSTRING_TYPE),
          if_true, if_false, fall_through);
  } else if (check->Equals(Heap::boolean_symbol())) {
    __ LoadRoot(at, Heap::kTrueValueRootIndex);
    __ Branch(if_true, eq, v0, Operand(at));
    __ LoadRoot(at, Heap::kFalseValueRootIndex);
    Split(eq, v0, Operand(at), if_true, if_false, fall_through);
  } else if (check->Equals(Heap::undefined_symbol())) {
    __ LoadRoot(at, Heap::kUndefinedValueRootIndex);
    __ Branch(if_true, eq, v0, Operand(at));
    __ And(at, v0, Operand(kSmiTagMask));
    __ Branch(if_false, eq, at, Operand(zero_reg));
    // Check for undetectable objects => true.
    __ lw(v0, FieldMemOperand(v0, HeapObject::kMapOffset));
    __ lbu(a1, FieldMemOperand(v0, Map::kBitFieldOffset));
    __ And(a1, a1, Operand(1 << Map::kIsUndetectable));
    Split(ne, a1, Operand(zero_reg), if_true, if_false, fall_through);
  } else if (check->Equals(Heap::function_symbol())) {
    __ And(at, v0, Operand(kSmiTagMask));
    __ Branch(if_false, eq, at, Operand(zero_reg));
    __ GetObjectType(v0, a1, v0);  // Leave map in a1.
    __ Branch(if_true, eq, v0, Operand(JS_FUNCTION_TYPE));
    // Regular expressions => 'function' (they are callable).
    __ lbu(v0, FieldMemOperand(a1, Map::kInstanceTypeOffset));
    Split(eq, v0, Operand(JS_REGEXP_TYPE), if_true, if_false, fall_through);
  } else if (check->Equals(Heap::object_symbol())) {
    __ And(at, v0, Operand(kSmiTagMask));
    __ Branch(if_false, eq, at, Operand(zero_reg));
    __ LoadRoot(at, Heap::kNullValueRootIndex);
    __ Branch(if_true, eq, v0, Operand(at));
    // Regular expressions => 'function', not 'object'.
    __ GetObjectType(v0, a1, v0);  // Leave map in a1.
    __ Branch(if_false, eq, v0, Operand(JS_REGEXP_TYPE));
    // Check for undetectable objects => false.
    __ lbu(v0, FieldMemOperand(a1, Map::kBitFieldOffset));
    __ And(v0, v0, Operand(1 << Map::kIsUndetectable));
    __ Branch(if_false, ne, v0, Operand(zero_reg));
    // Check for JS objects => true.
    __ lbu(v0, FieldMemOperand(a1, Map::kInstanceTypeOffset));
    __ Branch(if_false, lt, v0, Operand(FIRST_JS_OBJECT_TYPE));
    Split(le, v0, Operand(LAST_JS_OBJECT_TYPE),
          if_true, if_false, fall_through);
  } else {
    if (if_false != fall_through) __ jmp(if_false);
  }

  return true;
}


void FullCodeGenerator::VisitCompareOperation(CompareOperation* expr) {
  Comment cmnt(masm_, "[ CompareOperation");
  SetSourcePosition(expr->position());
  // Write posistions prior to subsequent call, to ensure RelocInfo ordering.
  __ positions_recorder()->WriteRecordedPositions();

  // Always perform the comparison for its control flow.  Pack the result
  // into the expression's context after the comparison is performed.

  Label materialize_true, materialize_false;
  Label* if_true = NULL;
  Label* if_false = NULL;
  Label* fall_through = NULL;
  context()->PrepareTest(&materialize_true, &materialize_false,
                         &if_true, &if_false, &fall_through);

  // First we try a fast inlined version of the compare when one of
  // the operands is a literal.
  Token::Value op = expr->op();
  Expression* left = expr->left();
  Expression* right = expr->right();
  if (TryLiteralCompare(op, left, right, if_true, if_false, fall_through)) {
    context()->Plug(if_true, if_false);
    return;
  }

  VisitForStackValue(expr->left());
  switch (op) {
    case Token::IN:
      VisitForStackValue(expr->right());
      __ InvokeBuiltin(Builtins::IN, CALL_JS);
      __ LoadRoot(t0, Heap::kTrueValueRootIndex);
      Split(eq, v0, Operand(t0), if_true, if_false, fall_through);
      break;

    case Token::INSTANCEOF: {
      VisitForStackValue(expr->right());
      InstanceofStub stub;
      __ CallStub(&stub);
      // The stub returns 0 for true.
      Split(eq, v0, Operand(zero_reg), if_true, if_false, fall_through);
      break;
    }

    default: {
      VisitForAccumulatorValue(expr->right());
      Condition cc = eq;
      bool strict = false;
      switch (op) {
        case Token::EQ_STRICT:
          strict = true;
          // Fall through
        case Token::EQ:
          cc = eq;
          __ mov(a0, result_register());
          __ pop(a1);
          break;
        case Token::LT:
          cc = lt;
          __ mov(a0, result_register());
          __ pop(a1);
          break;
        case Token::GT:
          // Reverse left and right sides to obtain ECMA-262 conversion order.
          cc = lt;
          __ mov(a1, result_register());
          __ pop(a0);
         break;
        case Token::LTE:
          // Reverse left and right sides to obtain ECMA-262 conversion order.
          cc = ge;
          __ mov(a1, result_register());
          __ pop(a0);
          break;
        case Token::GTE:
          cc = ge;
          __ mov(a0, result_register());
          __ pop(a1);
          break;
        case Token::IN:
        case Token::INSTANCEOF:
        default:
          UNREACHABLE();
      }

      bool inline_smi_code = ShouldInlineSmiCase(op);
      if (inline_smi_code) {
        Label slow_case;
        __ Or(a2, a0, Operand(a1));
        __ BranchOnNotSmi(a2, &slow_case);
        Split(cc, a1, Operand(a0), if_true, if_false, NULL);
        __ bind(&slow_case);
      }
      CompareFlags flags = inline_smi_code
          ? NO_SMI_COMPARE_IN_STUB
          : NO_COMPARE_FLAGS;
      CompareStub stub(cc, strict, flags, a1, a0);
      __ CallStub(&stub);
      Split(cc, v0, Operand(zero_reg), if_true, if_false, fall_through);
    }
  }

  // Convert the result of the comparison into one expected for this
  // expression's context.
  context()->Plug(if_true, if_false);
}


void FullCodeGenerator::VisitCompareToNull(CompareToNull* expr) {
  Comment cmnt(masm_, "[ CompareToNull");
  Label materialize_true, materialize_false;
  Label* if_true = NULL;
  Label* if_false = NULL;
  Label* fall_through = NULL;
  context()->PrepareTest(&materialize_true, &materialize_false,
                         &if_true, &if_false, &fall_through);

  VisitForAccumulatorValue(expr->expression());
  __ mov(a0, result_register());
  __ LoadRoot(a1, Heap::kNullValueRootIndex);
  if (expr->is_strict()) {
    Split(eq, a0, Operand(a1), if_true, if_false, fall_through);
  } else {
    __ Branch(if_true, eq, a0, Operand(a1));
    __ LoadRoot(a1, Heap::kUndefinedValueRootIndex);
    __ Branch(if_true, eq, a0, Operand(a1));
    __ And(at, a0, Operand(kSmiTagMask));
    __ Branch(if_false, eq, at, Operand(zero_reg));
    // It can be an undetectable object.
    __ lw(a1, FieldMemOperand(a0, HeapObject::kMapOffset));
    __ lbu(a1, FieldMemOperand(a1, Map::kBitFieldOffset));
    __ And(a1, a1, Operand(1 << Map::kIsUndetectable));
    Split(eq, a1, Operand(1 << Map::kIsUndetectable),  // plind badly optimized
          if_true, if_false, fall_through);
  }
  context()->Plug(if_true, if_false);
}


void FullCodeGenerator::VisitThisFunction(ThisFunction* expr) {
  __ lw(v0, MemOperand(fp, JavaScriptFrameConstants::kFunctionOffset));
  context()->Plug(v0);
}


Register FullCodeGenerator::result_register() {
  return v0;
}


Register FullCodeGenerator::context_register() {
  return cp;
}


void FullCodeGenerator::EmitCallIC(Handle<Code> ic, RelocInfo::Mode mode) {
  ASSERT(mode == RelocInfo::CODE_TARGET ||
         mode == RelocInfo::CODE_TARGET_CONTEXT);
  __ Call(ic, mode);
}


void FullCodeGenerator::StoreToFrameField(int frame_offset, Register value) {
  ASSERT_EQ(POINTER_SIZE_ALIGN(frame_offset), frame_offset);
  __ sw(value, MemOperand(fp, frame_offset));
}


void FullCodeGenerator::LoadContextField(Register dst, int context_index) {
  __ lw(dst, ContextOperand(cp, context_index));
}


// ----------------------------------------------------------------------------
// Non-local control flow support.

void FullCodeGenerator::EnterFinallyBlock() {
  ASSERT(!result_register().is(a1));
  // Store result register while executing finally block.
  __ push(result_register());
  // Cook return address in link register to stack (smi encoded Code* delta)
  __ Subu(a1, ra, Operand(masm_->CodeObject()));
  ASSERT_EQ(1, kSmiTagSize + kSmiShiftSize);
  ASSERT_EQ(0, kSmiTag);
  __ Addu(a1, a1, Operand(a1));  // Convert to smi.
  __ push(a1);
}


void FullCodeGenerator::ExitFinallyBlock() {
  ASSERT(!result_register().is(a1));
  // Restore result register from stack.
  __ pop(a1);
  // Uncook return address and return.
  __ pop(result_register());
  ASSERT_EQ(1, kSmiTagSize + kSmiShiftSize);
  __ sra(a1, a1, 1);  // Un-smi-tag value.
  __ Addu(at, a1, Operand(masm_->CodeObject()));
  __ Jump(at);
}


#undef __

} }  // namespace v8::internal

#endif  // V8_TARGET_ARCH_MIPS
