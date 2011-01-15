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
#include "compiler.h"
#include "debug.h"
#include "full-codegen.h"
#include "parser.h"

namespace v8 {
namespace internal {

#define __ ACCESS_MASM(masm_)

void FullCodeGenerator::Generate(CompilationInfo* info) {
  UNIMPLEMENTED_MIPS();
}


void FullCodeGenerator::EmitReturnSequence() {
  UNIMPLEMENTED_MIPS();
}


void FullCodeGenerator::Apply(Expression::Context context, Register reg) {
  UNIMPLEMENTED_MIPS();
}


void FullCodeGenerator::Apply(Expression::Context context, Slot* slot) {
  UNIMPLEMENTED_MIPS();
}

void FullCodeGenerator::Apply(Expression::Context context, Literal* lit) {
  UNIMPLEMENTED_MIPS();
}


void FullCodeGenerator::ApplyTOS(Expression::Context context) {
  UNIMPLEMENTED_MIPS();
}


void FullCodeGenerator::DropAndApply(int count,
                                     Expression::Context context,
                                     Register reg) {
  UNIMPLEMENTED_MIPS();
}


void FullCodeGenerator::Apply(Expression::Context context,
                              Label* materialize_true,
                              Label* materialize_false) {
  UNIMPLEMENTED_MIPS();
}


void FullCodeGenerator::DoTest(Label* if_true,
                               Label* if_false,
                               Label* fall_through) {
  UNIMPLEMENTED_MIPS();
}


MemOperand FullCodeGenerator::EmitSlotSearch(Slot* slot, Register scratch) {
  UNIMPLEMENTED_MIPS();
  return MemOperand(zero_reg, 0);   // UNIMPLEMENTED RETURN
}


void FullCodeGenerator::Move(Register destination, Slot* source) {
  UNIMPLEMENTED_MIPS();
}


void FullCodeGenerator::Move(Slot* dst,
                             Register src,
                             Register scratch1,
                             Register scratch2) {
  UNIMPLEMENTED_MIPS();
}


void FullCodeGenerator::VisitDeclaration(Declaration* decl) {
  UNIMPLEMENTED_MIPS();
}


void FullCodeGenerator::DeclareGlobals(Handle<FixedArray> pairs) {
  UNIMPLEMENTED_MIPS();
}


void FullCodeGenerator::VisitSwitchStatement(SwitchStatement* stmt) {
  UNIMPLEMENTED_MIPS();
}


void FullCodeGenerator::VisitForInStatement(ForInStatement* stmt) {
  UNIMPLEMENTED_MIPS();
}


void FullCodeGenerator::EmitNewClosure(Handle<SharedFunctionInfo> info) {
  UNIMPLEMENTED_MIPS();
}


void FullCodeGenerator::VisitVariableProxy(VariableProxy* expr) {
  UNIMPLEMENTED_MIPS();
}


void FullCodeGenerator::EmitVariableLoad(Variable* var,
                                         Expression::Context context) {
  UNIMPLEMENTED_MIPS();
}


void FullCodeGenerator::VisitRegExpLiteral(RegExpLiteral* expr) {
  UNIMPLEMENTED_MIPS();
}


void FullCodeGenerator::VisitObjectLiteral(ObjectLiteral* expr) {
  UNIMPLEMENTED_MIPS();
}


void FullCodeGenerator::VisitArrayLiteral(ArrayLiteral* expr) {
  UNIMPLEMENTED_MIPS();
}


void FullCodeGenerator::VisitAssignment(Assignment* expr) {
  UNIMPLEMENTED_MIPS();
}


void FullCodeGenerator::EmitNamedPropertyLoad(Property* prop) {
  UNIMPLEMENTED_MIPS();
}


void FullCodeGenerator::EmitKeyedPropertyLoad(Property* prop) {
  UNIMPLEMENTED_MIPS();
}


void FullCodeGenerator::EmitBinaryOp(Token::Value op,
                                     Expression::Context context) {
  UNIMPLEMENTED_MIPS();
}


void FullCodeGenerator::EmitVariableAssignment(Variable* var,
                                               Token::Value op,
                                               Expression::Context context) {
  UNIMPLEMENTED_MIPS();
}


void FullCodeGenerator::EmitNamedPropertyAssignment(Assignment* expr) {
  UNIMPLEMENTED_MIPS();
}


void FullCodeGenerator::EmitKeyedPropertyAssignment(Assignment* expr) {
  UNIMPLEMENTED_MIPS();
}


void FullCodeGenerator::VisitProperty(Property* expr) {
  UNIMPLEMENTED_MIPS();
}

void FullCodeGenerator::EmitCallWithIC(Call* expr,
                                       Handle<Object> ignored,
                                       RelocInfo::Mode mode) {
  UNIMPLEMENTED_MIPS();
}


void FullCodeGenerator::EmitCallWithStub(Call* expr) {
  UNIMPLEMENTED_MIPS();
}


void FullCodeGenerator::VisitCall(Call* expr) {
  UNIMPLEMENTED_MIPS();
}


void FullCodeGenerator::VisitCallNew(CallNew* expr) {
  UNIMPLEMENTED_MIPS();
}


void FullCodeGenerator::VisitCallRuntime(CallRuntime* expr) {
  UNIMPLEMENTED_MIPS();
}


void FullCodeGenerator::VisitUnaryOperation(UnaryOperation* expr) {
  UNIMPLEMENTED_MIPS();
}


void FullCodeGenerator::VisitCountOperation(CountOperation* expr) {
  UNIMPLEMENTED_MIPS();
}


void FullCodeGenerator::VisitCompareOperation(CompareOperation* expr) {
  UNIMPLEMENTED_MIPS();
}

void FullCodeGenerator::VisitCompareToNull(CompareToNull* expr) {
  UNIMPLEMENTED_MIPS();
}

void FullCodeGenerator::VisitThisFunction(ThisFunction* expr) {
  UNIMPLEMENTED_MIPS();
}


Register FullCodeGenerator::result_register() { return v0; }


Register FullCodeGenerator::context_register() { return cp; }


void FullCodeGenerator::StoreToFrameField(int frame_offset, Register value) {
  UNIMPLEMENTED_MIPS();
}


void FullCodeGenerator::LoadContextField(Register dst, int context_index) {
  UNIMPLEMENTED_MIPS();
}


// ----------------------------------------------------------------------------
// Non-local control flow support.

void FullCodeGenerator::EnterFinallyBlock() {
  UNIMPLEMENTED_MIPS();
}


void FullCodeGenerator::ExitFinallyBlock() {
  UNIMPLEMENTED_MIPS();
}

// ----------------------------------------------------------------------------
// This is a quick way to define some functions that are
// currently unimplemented.
#define MIPS_UNIMPLEMENTED_FULL_CODEGEN_FUNC(Name) \
  void FullCodeGenerator::Name(ZoneList<v8::internal::Expression*>*) \
  { UNIMPLEMENTED_MIPS(); }

MIPS_UNIMPLEMENTED_FULL_CODEGEN_FUNC(EmitIsSmi)
MIPS_UNIMPLEMENTED_FULL_CODEGEN_FUNC(EmitIsNonNegativeSmi)
MIPS_UNIMPLEMENTED_FULL_CODEGEN_FUNC(EmitIsObject)
MIPS_UNIMPLEMENTED_FULL_CODEGEN_FUNC(EmitIsSpecObject)
MIPS_UNIMPLEMENTED_FULL_CODEGEN_FUNC(EmitIsUndetectableObject)
MIPS_UNIMPLEMENTED_FULL_CODEGEN_FUNC(EmitIsFunction)
MIPS_UNIMPLEMENTED_FULL_CODEGEN_FUNC(EmitIsArray)
MIPS_UNIMPLEMENTED_FULL_CODEGEN_FUNC(EmitIsRegExp)
MIPS_UNIMPLEMENTED_FULL_CODEGEN_FUNC(EmitIsConstructCall)
MIPS_UNIMPLEMENTED_FULL_CODEGEN_FUNC(EmitObjectEquals)
MIPS_UNIMPLEMENTED_FULL_CODEGEN_FUNC(EmitArguments)
MIPS_UNIMPLEMENTED_FULL_CODEGEN_FUNC(EmitArgumentsLength)
MIPS_UNIMPLEMENTED_FULL_CODEGEN_FUNC(EmitClassOf)
MIPS_UNIMPLEMENTED_FULL_CODEGEN_FUNC(EmitLog)
MIPS_UNIMPLEMENTED_FULL_CODEGEN_FUNC(EmitRandomHeapNumber)
MIPS_UNIMPLEMENTED_FULL_CODEGEN_FUNC(EmitSubString)
MIPS_UNIMPLEMENTED_FULL_CODEGEN_FUNC(EmitRegExpExec)
MIPS_UNIMPLEMENTED_FULL_CODEGEN_FUNC(EmitValueOf)
MIPS_UNIMPLEMENTED_FULL_CODEGEN_FUNC(EmitSetValueOf)
MIPS_UNIMPLEMENTED_FULL_CODEGEN_FUNC(EmitNumberToString)
MIPS_UNIMPLEMENTED_FULL_CODEGEN_FUNC(EmitStringCharFromCode)
MIPS_UNIMPLEMENTED_FULL_CODEGEN_FUNC(EmitStringCharCodeAt)
MIPS_UNIMPLEMENTED_FULL_CODEGEN_FUNC(EmitStringCharAt)
MIPS_UNIMPLEMENTED_FULL_CODEGEN_FUNC(EmitStringAdd)
MIPS_UNIMPLEMENTED_FULL_CODEGEN_FUNC(EmitStringCompare)
MIPS_UNIMPLEMENTED_FULL_CODEGEN_FUNC(EmitMathPow)
MIPS_UNIMPLEMENTED_FULL_CODEGEN_FUNC(EmitMathSin)
MIPS_UNIMPLEMENTED_FULL_CODEGEN_FUNC(EmitMathCos)
MIPS_UNIMPLEMENTED_FULL_CODEGEN_FUNC(EmitMathSqrt)
MIPS_UNIMPLEMENTED_FULL_CODEGEN_FUNC(EmitCallFunction)
MIPS_UNIMPLEMENTED_FULL_CODEGEN_FUNC(EmitRegExpConstructResult)
MIPS_UNIMPLEMENTED_FULL_CODEGEN_FUNC(EmitSwapElements)
MIPS_UNIMPLEMENTED_FULL_CODEGEN_FUNC(EmitGetFromCache)
MIPS_UNIMPLEMENTED_FULL_CODEGEN_FUNC(EmitIsRegExpEquivalent)
MIPS_UNIMPLEMENTED_FULL_CODEGEN_FUNC(EmitIsStringWrapperSafeForDefaultValueOf)

#undef __

} }  // namespace v8::internal

#endif  // V8_TARGET_ARCH_MIPS
