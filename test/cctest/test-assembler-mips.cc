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

#include "disassembler.h"
#include "factory.h"
#include "macro-assembler.h"
#include "mips/macro-assembler-mips.h"
#include "mips/simulator-mips.h"

#include "cctest.h"

using namespace v8::internal;


// Define these function prototypes to match JSEntryFunction in execution.cc.
typedef Object* (*F1)(int x, int p1, int p2, int p3, int p4);
typedef Object* (*F2)(int x, int y, int p2, int p3, int p4);
typedef Object* (*F3)(void* p, int p1, int p2, int p3, int p4);


static v8::Persistent<v8::Context> env;


// The test framework does not accept flags on the command line, so we set them.
static void InitializeVM() {
  // Disable compilation of natives.
  FLAG_disable_native_files = true;

  // Enable generation of comments.
  FLAG_debug_code = true;

  if (env.IsEmpty()) {
    env = v8::Context::New();
  }
}


#define __ assm.

TEST(MIPS0) {
  InitializeVM();
  v8::HandleScope scope;

  MacroAssembler assm(NULL, 0);

  // Addition.
  __ addu(v0, a0, a1);
  __ jr(ra);
  __ nop();

  CodeDesc desc;
  assm.GetCode(&desc);
  Object* code = Heap::CreateCode(desc,
                                  NULL,
                                  Code::ComputeFlags(Code::STUB),
                                  Handle<Object>(Heap::undefined_value()));
  CHECK(code->IsCode());
#ifdef DEBUG
  Code::cast(code)->Print();
#endif
  F2 f = FUNCTION_CAST<F2>(Code::cast(code)->entry());
  int res = reinterpret_cast<int>(CALL_GENERATED_CODE(f, 0xab0, 0xc, 0, 0, 0));
  ::printf("f() = %d\n", res);
  CHECK_EQ(0xabc, res);
}


TEST(MIPS1) {
  InitializeVM();
  v8::HandleScope scope;

  MacroAssembler assm(NULL, 0);
  Label L, C;

  __ mov(a1, a0);
  __ li(v0, 0);
  __ b(&C);
  __ nop();

  __ bind(&L);
  __ addu(v0, v0, a1);
  __ addiu(a1, a1, -1);

  __ bind(&C);
  __ xori(v1, a1, 0);
  __ Branch(&L, ne, v1, Operand(0));
  __ nop();

  __ jr(ra);
  __ nop();

  CodeDesc desc;
  assm.GetCode(&desc);
  Object* code = Heap::CreateCode(desc,
                                  NULL,
                                  Code::ComputeFlags(Code::STUB),
                                  Handle<Object>(Heap::undefined_value()));
  CHECK(code->IsCode());
#ifdef DEBUG
  Code::cast(code)->Print();
#endif
  F1 f = FUNCTION_CAST<F1>(Code::cast(code)->entry());
  int res = reinterpret_cast<int>(CALL_GENERATED_CODE(f, 50, 0, 0, 0, 0));
  ::printf("f() = %d\n", res);
  CHECK_EQ(1275, res);
}


TEST(MIPS2) {
  InitializeVM();
  v8::HandleScope scope;

  MacroAssembler assm(NULL, 0);

  Label exit, error;

  // ----- Test all instructions.

  // Test lui, ori, and addiu, used in the li pseudo-instruction.
  // This way we can then safely load registers with chosen values.

  __ ori(t0, zero_reg, 0);
  __ lui(t0, 0x1234);
  __ ori(t0, t0, 0);
  __ ori(t0, t0, 0x0f0f);
  __ ori(t0, t0, 0xf0f0);
  __ addiu(t1, t0, 1);
  __ addiu(t2, t1, -0x10);

  // Load values in temporary registers.
  __ li(t0, 0x00000004);
  __ li(t1, 0x00001234);
  __ li(t2, 0x12345678);
  __ li(t3, 0x7fffffff);
  __ li(t4, 0xfffffffc);
  __ li(t5, 0xffffedcc);
  __ li(t6, 0xedcba988);
  __ li(t7, 0x80000000);

  // SPECIAL class.
  __ srl(v0, t2, 8);    // 0x00123456
  __ sll(v0, v0, 11);   // 0x91a2b000
  __ sra(v0, v0, 3);    // 0xf2345600
  __ srav(v0, v0, t0);  // 0xff234560
  __ sllv(v0, v0, t0);  // 0xf2345600
  __ srlv(v0, v0, t0);  // 0x0f234560
  __ Branch(&error, ne, v0, Operand(0x0f234560));
  __ nop();

  __ addu(v0, t0, t1);   // 0x00001238
  __ subu(v0, v0, t0);  // 0x00001234
  __ Branch(&error, ne, v0, Operand(0x00001234));
  __ nop();
  __ addu(v1, t3, t0);
  __ Branch(&error, ne, v1, Operand(0x80000003));
  __ nop();
  __ subu(v1, t7, t0);  // 0x7ffffffc
  __ Branch(&error, ne, v1, Operand(0x7ffffffc));
  __ nop();

  __ and_(v0, t1, t2);  // 0x00001230
  __ or_(v0, v0, t1);   // 0x00001234
  __ xor_(v0, v0, t2);  // 0x1234444c
  __ nor(v0, v0, t2);   // 0xedcba987
  __ Branch(&error, ne, v0, Operand(0xedcba983));
  __ nop();

  __ slt(v0, t7, t3);
  __ Branch(&error, ne, v0, Operand(0x1));
  __ nop();
  __ sltu(v0, t7, t3);
  __ Branch(&error, ne, v0, Operand(0x0));
  __ nop();
  // End of SPECIAL class.

  __ addiu(v0, zero_reg, 0x7421);  // 0x00007421
  __ addiu(v0, v0, -0x1);  // 0x00007420
  __ addiu(v0, v0, -0x20);  // 0x00007400
  __ Branch(&error, ne, v0, Operand(0x00007400));
  __ nop();
  __ addiu(v1, t3, 0x1);  // 0x80000000
  __ Branch(&error, ne, v1, Operand(0x80000000));
  __ nop();

  __ slti(v0, t1, 0x00002000);  // 0x1
  __ slti(v0, v0, 0xffff8000);  // 0x0
  __ Branch(&error, ne, v0, Operand(0x0));
  __ nop();
  __ sltiu(v0, t1, 0x00002000);  // 0x1
  __ sltiu(v0, v0, 0x00008000);  // 0x1
  __ Branch(&error, ne, v0, Operand(0x1));
  __ nop();

  __ andi(v0, t1, 0xf0f0);  // 0x00001030
  __ ori(v0, v0, 0x8a00);  // 0x00009a30
  __ xori(v0, v0, 0x83cc);  // 0x000019fc
  __ Branch(&error, ne, v0, Operand(0x000019fc));
  __ nop();
  __ lui(v1, 0x8123);  // 0x81230000
  __ Branch(&error, ne, v1, Operand(0x81230000));
  __ nop();

  // Bit twiddling instructions & conditional moves.
  // Uses t0-t7 as set above.
  __ clz(v0, t0);       // 29
  __ clz(v1, t1);       // 19
  __ addu(v0, v0, v1);  // 48
  __ clz(v1, t2);       // 3
  __ addu(v0, v0, v1);  // 51
  __ clz(v1, t7);       // 0
  __ addu(v0, v0, v1);  // 51
  __ Branch(&error, ne, v0, Operand(51));
  __ movn(a0, t3, t0);  // move a0<-t3 (t0 is NOT 0)
  __ ins(a0, t1, 12, 8);  // 0x7ff34fff
  __ Branch(&error, ne, a0, Operand(0x7ff34fff));
  __ movz(a0, t6, t7);    // a0 not updated (t7 is NOT 0)
  __ ext(a1, a0, 8, 12);  // 0x34f
  __ Branch(&error, ne, a1, Operand(0x34f));
  __ movz(a0, t6, v1);    // a0<-t6, v0 is 0, from 8 instr back
  __ Branch(&error, ne, a0, Operand(t6));

  // Everything was correctly executed. Load the expected result.
  __ li(v0, 0x31415926);
  __ b(&exit);
  __ nop();

  __ bind(&error);
  // Got an error. Return a wrong result.
  __ li(v0, 666);

  __ bind(&exit);
  __ jr(ra);
  __ nop();

  CodeDesc desc;
  assm.GetCode(&desc);
  Object* code = Heap::CreateCode(desc,
                                  NULL,
                                  Code::ComputeFlags(Code::STUB),
                                  Handle<Object>(Heap::undefined_value()));
  CHECK(code->IsCode());
#ifdef DEBUG
  Code::cast(code)->Print();
#endif
  F2 f = FUNCTION_CAST<F2>(Code::cast(code)->entry());
  int res = reinterpret_cast<int>(CALL_GENERATED_CODE(f, 0xab0, 0xc, 0, 0, 0));
  ::printf("f() = %d\n", res);
  CHECK_EQ(0x31415926, res);
}


TEST(MIPS3) {
  // Test floating point instructions.
  InitializeVM();
  v8::HandleScope scope;

  typedef struct {
    double a;
    double b;
    double c;
    double d;
    double e;
    double f;
  } T;
  T t;

  // Create a function that accepts &t, and loads, manipulates, and stores
  // the doubles t.a ... t.f.
  MacroAssembler assm(NULL, 0);
  Label L, C;

  __ ldc1(f4, MemOperand(a0, OFFSET_OF(T, a)) );
  __ ldc1(f6, MemOperand(a0, OFFSET_OF(T, b)) );
  __ add_d(f8, f4, f6);
  __ sdc1(f8, MemOperand(a0, OFFSET_OF(T, c)) );   // c = a + b

  __ mov_d(f10, f8);  // c
  __ neg_d(f12, f6);  // -b
  __ sub_d(f10, f10, f12);
  __ sdc1(f10, MemOperand(a0, OFFSET_OF(T, d)) );   // d = c - (-b)

  __ sdc1(f4, MemOperand(a0, OFFSET_OF(T, b)) );  // b = a

  __ li(t0, 120);
  __ mtc1(t0, f14);
  __ cvt_d_w(f14, f14);   // f14 = 120.0
  __ mul_d(f10, f10, f14);
  __ sdc1(f10, MemOperand(a0, OFFSET_OF(T, e)) );   // e = d * 120 = 1.8066e16

  __ div_d(f12, f10, f4);
  __ sdc1(f12, MemOperand(a0, OFFSET_OF(T, f)) );   // f = e / a = 120.44

  __ jr(ra);
  __ nop();

  CodeDesc desc;
  assm.GetCode(&desc);
  Object* code = Heap::CreateCode(desc,
                                  NULL,
                                  Code::ComputeFlags(Code::STUB),
                                  Handle<Object>(Heap::undefined_value()));
  CHECK(code->IsCode());
#ifdef DEBUG
  Code::cast(code)->Print();
#endif
  F3 f = FUNCTION_CAST<F3>(Code::cast(code)->entry());
  t.a = 1.5e14;
  t.b = 2.75e11;
  t.c = 0.0;
  t.d = 0.0;
  t.e = 0.0;
  t.f = 0.0;
  Object* dummy = CALL_GENERATED_CODE(f, &t, 0, 0, 0, 0);
  USE(dummy);
  CHECK_EQ(1.5e14, t.a);
  CHECK_EQ(1.5e14, t.b);
  CHECK_EQ(1.50275e14, t.c);
  CHECK_EQ(1.50550e14, t.d);
  CHECK_EQ(1.8066e16, t.e);
  CHECK_EQ(120.44, t.f);
}


TEST(MIPS4) {
  // Test moves between floating point and integer registers.
  InitializeVM();
  v8::HandleScope scope;

  typedef struct {
    double a;
    double b;
    double c;
  } T;
  T t;

  Assembler assm(NULL, 0);
  Label L, C;

  __ ldc1(f4, MemOperand(a0, OFFSET_OF(T, a)) );
  __ ldc1(f6, MemOperand(a0, OFFSET_OF(T, b)) );

  // swap f4 and f6, by using four integer registers, t0-t3
  __ mfc1(t0, f4);
  __ mfc1(t1, f5);
  __ mfc1(t2, f6);
  __ mfc1(t3, f7);

  __ mtc1(t0, f6);
  __ mtc1(t1, f7);
  __ mtc1(t2, f4);
  __ mtc1(t3, f5);

  // store the swapped f4 and f5 back to memory
  __ sdc1(f4, MemOperand(a0, OFFSET_OF(T, a)) );
  __ sdc1(f6, MemOperand(a0, OFFSET_OF(T, c)) );

  __ jr(ra);
  __ nop();

  CodeDesc desc;
  assm.GetCode(&desc);
  Object* code = Heap::CreateCode(desc,
                                  NULL,
                                  Code::ComputeFlags(Code::STUB),
                                  Handle<Object>(Heap::undefined_value()));
  CHECK(code->IsCode());
#ifdef DEBUG
  Code::cast(code)->Print();
#endif
  F3 f = FUNCTION_CAST<F3>(Code::cast(code)->entry());
  t.a = 1.5e22;
  t.b = 2.75e11;
  t.c = 17.17;
  Object* dummy = CALL_GENERATED_CODE(f, &t, 0, 0, 0, 0);
  USE(dummy);

  CHECK_EQ(2.75e11, t.a);
  CHECK_EQ(2.75e11, t.b);
  CHECK_EQ(1.5e22, t.c);
}


TEST(MIPS5) {
  // Test conversions between doubles and integers
  InitializeVM();
  v8::HandleScope scope;

  typedef struct {
    double a;
    double b;
    int i;
    int j;
  } T;
  T t;

  Assembler assm(NULL, 0);
  Label L, C;

  // load all structure elements to registers
  __ ldc1(f4, MemOperand(a0, OFFSET_OF(T, a)) );
  __ ldc1(f6, MemOperand(a0, OFFSET_OF(T, b)) );
  __ lw(t0, MemOperand(a0, OFFSET_OF(T, i)) );
  __ lw(t1, MemOperand(a0, OFFSET_OF(T, j)) );

  // convert double in f4 to int in element i
  __ cvt_w_d(f8, f4);
  __ mfc1(t2, f8);
  __ sw(t2, MemOperand(a0, OFFSET_OF(T, i)) );

  // convert double in f6 to int in element j
  __ cvt_w_d(f10, f6);
  __ mfc1(t3, f10);
  __ sw(t3, MemOperand(a0, OFFSET_OF(T, j)) );

  // convert int in original i (t0) to double in a
  __ mtc1(t0, f12);
  __ cvt_d_w(f0, f12);
  __ sdc1(f0, MemOperand(a0, OFFSET_OF(T, a)) );

  // convert int in original j (t1) to double in b
  __ mtc1(t1, f14);
  __ cvt_d_w(f2, f14);
  __ sdc1(f2, MemOperand(a0, OFFSET_OF(T, b)) );

  __ jr(ra);
  __ nop();

  CodeDesc desc;
  assm.GetCode(&desc);
  Object* code = Heap::CreateCode(desc,
                                  NULL,
                                  Code::ComputeFlags(Code::STUB),
                                  Handle<Object>(Heap::undefined_value()));
  CHECK(code->IsCode());
#ifdef DEBUG
  Code::cast(code)->Print();
#endif
  F3 f = FUNCTION_CAST<F3>(Code::cast(code)->entry());
  t.a = 1.5e4;
  t.b = 2.75e8;
  t.i = 12345678;
  t.j = -100000;
  Object* dummy = CALL_GENERATED_CODE(f, &t, 0, 0, 0, 0);
  USE(dummy);

  CHECK_EQ(12345678.0, t.a);
  CHECK_EQ(-100000.0, t.b);
  CHECK_EQ(15000, t.i);
  CHECK_EQ(275000000, t.j);
}


TEST(MIPS6) {
  // Test simple memory loads and stores
  InitializeVM();
  v8::HandleScope scope;

  typedef struct {
    uint32_t ui;
    int32_t si;
    int32_t r1;
    int32_t r2;
    int32_t r3;
    int32_t r4;
    int32_t r5;
    int32_t r6;
  } T;
  T t;

  Assembler assm(NULL, 0);
  Label L, C;

  // basic word load/store
  __ lw(t0, MemOperand(a0, OFFSET_OF(T, ui)) );
  __ sw(t0, MemOperand(a0, OFFSET_OF(T, r1)) );

  // lh with positive data
  __ lh(t1, MemOperand(a0, OFFSET_OF(T, ui)) );
  __ sw(t1, MemOperand(a0, OFFSET_OF(T, r2)) );

  // lh with negative data
  __ lh(t2, MemOperand(a0, OFFSET_OF(T, si)) );
  __ sw(t2, MemOperand(a0, OFFSET_OF(T, r3)) );

  // lhu with negative data
  __ lhu(t3, MemOperand(a0, OFFSET_OF(T, si)) );
  __ sw(t3, MemOperand(a0, OFFSET_OF(T, r4)) );

  // lb with negative data
  __ lb(t4, MemOperand(a0, OFFSET_OF(T, si)) );
  __ sw(t4, MemOperand(a0, OFFSET_OF(T, r5)) );

  // sh writes only 1/2 of word
  __ lui(t5, 0x3333);
  __ ori(t5, t5, 0x3333);
  __ sw(t5, MemOperand(a0, OFFSET_OF(T, r6)) );
  __ lhu(t5, MemOperand(a0, OFFSET_OF(T, si)) );
  __ sh(t5, MemOperand(a0, OFFSET_OF(T, r6)) );


  __ jr(ra);
  __ nop();

  CodeDesc desc;
  assm.GetCode(&desc);
  Object* code = Heap::CreateCode(desc,
                                  NULL,
                                  Code::ComputeFlags(Code::STUB),
                                  Handle<Object>(Heap::undefined_value()));
  CHECK(code->IsCode());
#ifdef DEBUG
  Code::cast(code)->Print();
#endif
  F3 f = FUNCTION_CAST<F3>(Code::cast(code)->entry());
  t.ui = 0x11223344;
  t.si = 0x99aabbcc;
  Object* dummy = CALL_GENERATED_CODE(f, &t, 0, 0, 0, 0);
  USE(dummy);

  CHECK_EQ(0x11223344, t.r1);
  CHECK_EQ(0x3344, t.r2);
  CHECK_EQ(0xffffbbcc, t.r3);
  CHECK_EQ(0x0000bbcc, t.r4);
  CHECK_EQ(0xffffffcc, t.r5);
  CHECK_EQ(0x3333bbcc, t.r6);
}


TEST(MIPS7) {
  // Test floating point compare and branch instructions.
  InitializeVM();
  v8::HandleScope scope;

  typedef struct {
    double a;
    double b;
    double c;
    double d;
    double e;
    double f;
    int32_t result;
  } T;
  T t;

  // Create a function that accepts &t, and loads, manipulates, and stores
  // the doubles t.a ... t.f.
  MacroAssembler assm(NULL, 0);
  Label neither_is_nan, less_than, outa_here;

  __ ldc1(f4, MemOperand(a0, OFFSET_OF(T, a)) );
  __ ldc1(f6, MemOperand(a0, OFFSET_OF(T, b)) );
  __ c(UN, D, f4, f6);
  __ bc1f(&neither_is_nan);
  __ nop();
  __ sw(zero_reg, MemOperand(a0, OFFSET_OF(T, result)) );
  __ Branch(&outa_here, al);

  __ bind(&neither_is_nan);

  __ c(OLT, D, f6, f4, 2);
  __ bc1t(&less_than, 2);
  __ nop();
  __ sw(zero_reg, MemOperand(a0, OFFSET_OF(T, result)) );
  __ Branch(&outa_here, al);

  __ bind(&less_than);
  __ Addu(t0, zero_reg, Operand(1));
  __ sw(t0, MemOperand(a0, OFFSET_OF(T, result)) );  // Set true.


  // This test-case needs additional tests, this is not quite sufficient.

  __ bind(&outa_here);

  __ jr(ra);
  __ nop();

  CodeDesc desc;
  assm.GetCode(&desc);
  Object* code = Heap::CreateCode(desc,
                                  NULL,
                                  Code::ComputeFlags(Code::STUB),
                                  Handle<Object>(Heap::undefined_value()));
  CHECK(code->IsCode());
#ifdef DEBUG
  Code::cast(code)->Print();
#endif
  F3 f = FUNCTION_CAST<F3>(Code::cast(code)->entry());
  t.a = 1.5e14;
  t.b = 2.75e11;
  t.c = 2.0;
  t.d = -4.0;
  t.e = 0.0;
  t.f = 0.0;
  t.result = 0;
  Object* dummy = CALL_GENERATED_CODE(f, &t, 0, 0, 0, 0);
  USE(dummy);
  CHECK_EQ(1.5e14, t.a);
  CHECK_EQ(2.75e11, t.b);
  CHECK_EQ(1, t.result);
}

TEST(MIPS8) {
  // Test ROTR and ROTRV instructions.
  InitializeVM();
  v8::HandleScope scope;

  typedef struct {
    int32_t input;
    int32_t result_rotr_4;
    int32_t result_rotr_8;
    int32_t result_rotr_12;
    int32_t result_rotr_16;
    int32_t result_rotr_20;
    int32_t result_rotr_24;
    int32_t result_rotr_28;
    int32_t result_rotrv_4;
    int32_t result_rotrv_8;
    int32_t result_rotrv_12;
    int32_t result_rotrv_16;
    int32_t result_rotrv_20;
    int32_t result_rotrv_24;
    int32_t result_rotrv_28;
  } T;
  T t;

  MacroAssembler assm(NULL, 0);

  // basic word load
  __ lw(t0, MemOperand(a0, OFFSET_OF(T, input)) );

  // ROTR instruction
  __ rotr(t1, t0, 0x0004);
  __ rotr(t2, t0, 0x0008);
  __ rotr(t3, t0, 0x000c);
  __ rotr(t4, t0, 0x0010);
  __ rotr(t5, t0, 0x0014);
  __ rotr(t6, t0, 0x0018);
  __ rotr(t7, t0, 0x001c);

  // basic word store
  __ sw(t1, MemOperand(a0, OFFSET_OF(T, result_rotr_4)) );
  __ sw(t2, MemOperand(a0, OFFSET_OF(T, result_rotr_8)) );
  __ sw(t3, MemOperand(a0, OFFSET_OF(T, result_rotr_12)) );
  __ sw(t4, MemOperand(a0, OFFSET_OF(T, result_rotr_16)) );
  __ sw(t5, MemOperand(a0, OFFSET_OF(T, result_rotr_20)) );
  __ sw(t6, MemOperand(a0, OFFSET_OF(T, result_rotr_24)) );
  __ sw(t7, MemOperand(a0, OFFSET_OF(T, result_rotr_28)) );

  // ROTRV instruction
  __ li(t7, 0x0004);
  __ rotrv(t1, t0, t7);
  __ li(t7, 0x0008);
  __ rotrv(t2, t0, t7);
  __ li(t7, 0x000C);
  __ rotrv(t3, t0, t7);
  __ li(t7, 0x0010);
  __ rotrv(t4, t0, t7);
  __ li(t7, 0x0014);
  __ rotrv(t5, t0, t7);
  __ li(t7, 0x0018);
  __ rotrv(t6, t0, t7);
  __ li(t7, 0x001C);
  __ rotrv(t7, t0, t7);

  // basic word store
  __ sw(t1, MemOperand(a0, OFFSET_OF(T, result_rotrv_4)) );
  __ sw(t2, MemOperand(a0, OFFSET_OF(T, result_rotrv_8)) );
  __ sw(t3, MemOperand(a0, OFFSET_OF(T, result_rotrv_12)) );
  __ sw(t4, MemOperand(a0, OFFSET_OF(T, result_rotrv_16)) );
  __ sw(t5, MemOperand(a0, OFFSET_OF(T, result_rotrv_20)) );
  __ sw(t6, MemOperand(a0, OFFSET_OF(T, result_rotrv_24)) );
  __ sw(t7, MemOperand(a0, OFFSET_OF(T, result_rotrv_28)) );

  __ jr(ra);
  __ nop();

  CodeDesc desc;
  assm.GetCode(&desc);
  Object* code = Heap::CreateCode(desc,
                                  NULL,
                                  Code::ComputeFlags(Code::STUB),
                                  Handle<Object>(Heap::undefined_value()));
  CHECK(code->IsCode());
#ifdef DEBUG
  Code::cast(code)->Print();
#endif
  F3 f = FUNCTION_CAST<F3>(Code::cast(code)->entry());
  t.input = 0x12345678;
  Object* dummy = CALL_GENERATED_CODE(f, &t, 0x0, 0, 0, 0);
  USE(dummy);
  CHECK_EQ(0x81234567, t.result_rotr_4);
  CHECK_EQ(0x78123456, t.result_rotr_8);
  CHECK_EQ(0x67812345, t.result_rotr_12);
  CHECK_EQ(0x56781234, t.result_rotr_16);
  CHECK_EQ(0x45678123, t.result_rotr_20);
  CHECK_EQ(0x34567812, t.result_rotr_24);
  CHECK_EQ(0x23456781, t.result_rotr_28);

  CHECK_EQ(0x81234567, t.result_rotrv_4);
  CHECK_EQ(0x78123456, t.result_rotrv_8);
  CHECK_EQ(0x67812345, t.result_rotrv_12);
  CHECK_EQ(0x56781234, t.result_rotrv_16);
  CHECK_EQ(0x45678123, t.result_rotrv_20);
  CHECK_EQ(0x34567812, t.result_rotrv_24);
  CHECK_EQ(0x23456781, t.result_rotrv_28);
}

TEST(MIPS9) {
  // Test BRANCH improvements.
  InitializeVM();
  v8::HandleScope scope;

  MacroAssembler assm(NULL, 0);
  Label exit, exit2, exit3;

  __ Branch(&exit, ge, a0, Operand(0x00000000));
  __ Branch(&exit2, ge, a0, Operand(0x00001FFF));
  __ Branch(&exit3, ge, a0, Operand(0x0001FFFF));

  __ bind(&exit);
  __ bind(&exit2);
  __ bind(&exit3);
  __ jr(ra);
  __ nop();

  CodeDesc desc;
  assm.GetCode(&desc);
  Object* code = Heap::CreateCode(desc,
                                  NULL,
                                  Code::ComputeFlags(Code::STUB),
                                  Handle<Object>(Heap::undefined_value()));
  CHECK(code->IsCode());
#ifdef DEBUG
  Code::cast(code)->Print();
#endif
}

TEST(MIPS10) {
  // Test conversions between doubles and long integers.
  // Test hos the long ints map to FP regs pairs.
  InitializeVM();
  v8::HandleScope scope;

  typedef struct {
    double a;
    double b;
    int32_t dbl_mant;
    int32_t dbl_exp;
    int32_t long_hi;
    int32_t long_lo;
    int32_t b_long_hi;
    int32_t b_long_lo;
  } T;
  T t;

  Assembler assm(NULL, 0);
  Label L, C;

  // Load all structure elements to registers
  __ ldc1(f0, MemOperand(a0, OFFSET_OF(T, a)));

  // Save the raw bits of the double.
  __ mfc1(t0, f0);
  __ mfc1(t1, f1);
  __ sw(t0, MemOperand(a0, OFFSET_OF(T, dbl_mant)));
  __ sw(t1, MemOperand(a0, OFFSET_OF(T, dbl_exp)));

  // Convert double in f0 to long, save hi/lo parts.
  __ cvt_l_d(f0, f0);
  __ mfc1(t0, f0);  // f0 has LS 32 bits of long.
  __ mfc1(t1, f1);  // f1 has MS 32 bits of long.
  __ sw(t0, MemOperand(a0, OFFSET_OF(T, long_lo)));
  __ sw(t1, MemOperand(a0, OFFSET_OF(T, long_hi)));

  // Convert the b long integers to double b.
  __ lw(t0, MemOperand(a0, OFFSET_OF(T, b_long_lo)));
  __ lw(t1, MemOperand(a0, OFFSET_OF(T, b_long_hi)));
  __ mtc1(t0, f8);  // f8 has LS 32-bits.
  __ mtc1(t1, f9);  // f9 has MS 32-bits.
  __ cvt_d_l(f10, f8);
  __ sdc1(f10, MemOperand(a0, OFFSET_OF(T, b)));

  __ jr(ra);
  __ nop();

  CodeDesc desc;
  assm.GetCode(&desc);
  Object* code = Heap::CreateCode(desc,
                                  NULL,
                                  Code::ComputeFlags(Code::STUB),
                                  Handle<Object>(Heap::undefined_value()));
  CHECK(code->IsCode());
#ifdef DEBUG
  Code::cast(code)->Print();
#endif
  F3 f = FUNCTION_CAST<F3>(Code::cast(code)->entry());
  t.a = 2.147483647e9;       // 0x7fffffff -> 0x41DFFFFFFFC00000 as double.
  t.b_long_hi = 0x000000ff;  // 0xFF00FF00FF -> 0x426FE01FE01FE000 as double.
  t.b_long_lo = 0x00ff00ff;
  Object* dummy = CALL_GENERATED_CODE(f, &t, 0, 0, 0, 0);
  USE(dummy);

  CHECK_EQ(0x41DFFFFF, t.dbl_exp);
  CHECK_EQ(0xFFC00000, t.dbl_mant);
  CHECK_EQ(0, t.long_hi);
  CHECK_EQ(0x7fffffff, t.long_lo);
  CHECK_EQ(1.095233372415e12, t.b); // 0xFF00FF00FF -> 1.095233372415e12.
}

TEST(MIPS11) {
  // Test LWL, LWR, SWL and SWR instructions.
  InitializeVM();
  v8::HandleScope scope;

  typedef struct {
    int32_t reg_init;
    int32_t mem_init;
    int32_t lwl_0;
    int32_t lwl_1;
    int32_t lwl_2;
    int32_t lwl_3;
    int32_t lwr_0;
    int32_t lwr_1;
    int32_t lwr_2;
    int32_t lwr_3;
    int32_t swl_0;
    int32_t swl_1;
    int32_t swl_2;
    int32_t swl_3;
    int32_t swr_0;
    int32_t swr_1;
    int32_t swr_2;
    int32_t swr_3;
  } T;
  T t;

  Assembler assm(NULL, 0);

  // Test all combinations of LWL and vAddr
  __ lw(t0, MemOperand(a0, OFFSET_OF(T, reg_init)) );
  __ lwl(t0, MemOperand(a0, OFFSET_OF(T, mem_init)) );
  __ sw(t0, MemOperand(a0, OFFSET_OF(T, lwl_0)) );

  __ lw(t1, MemOperand(a0, OFFSET_OF(T, reg_init)) );
  __ lwl(t1, MemOperand(a0, OFFSET_OF(T, mem_init) + 1) );
  __ sw(t1, MemOperand(a0, OFFSET_OF(T, lwl_1)) );

  __ lw(t2, MemOperand(a0, OFFSET_OF(T, reg_init)) );
  __ lwl(t2, MemOperand(a0, OFFSET_OF(T, mem_init) + 2) );
  __ sw(t2, MemOperand(a0, OFFSET_OF(T, lwl_2)) );

  __ lw(t3, MemOperand(a0, OFFSET_OF(T, reg_init)) );
  __ lwl(t3, MemOperand(a0, OFFSET_OF(T, mem_init) + 3) );
  __ sw(t3, MemOperand(a0, OFFSET_OF(T, lwl_3)) );

  // Test all combinations of LWR and vAddr
  __ lw(t0, MemOperand(a0, OFFSET_OF(T, reg_init)) );
  __ lwr(t0, MemOperand(a0, OFFSET_OF(T, mem_init)) );
  __ sw(t0, MemOperand(a0, OFFSET_OF(T, lwr_0)) );

  __ lw(t1, MemOperand(a0, OFFSET_OF(T, reg_init)) );
  __ lwr(t1, MemOperand(a0, OFFSET_OF(T, mem_init) + 1) );
  __ sw(t1, MemOperand(a0, OFFSET_OF(T, lwr_1)) );

  __ lw(t2, MemOperand(a0, OFFSET_OF(T, reg_init)) );
  __ lwr(t2, MemOperand(a0, OFFSET_OF(T, mem_init) + 2) );
  __ sw(t2, MemOperand(a0, OFFSET_OF(T, lwr_2)) );

  __ lw(t3, MemOperand(a0, OFFSET_OF(T, reg_init)) );
  __ lwr(t3, MemOperand(a0, OFFSET_OF(T, mem_init) + 3) );
  __ sw(t3, MemOperand(a0, OFFSET_OF(T, lwr_3)) );

  // Test all combinations of SWL and vAddr
  __ lw(t0, MemOperand(a0, OFFSET_OF(T, mem_init)) );
  __ sw(t0, MemOperand(a0, OFFSET_OF(T, swl_0)) );
  __ lw(t0, MemOperand(a0, OFFSET_OF(T, reg_init)) );
  __ swl(t0, MemOperand(a0, OFFSET_OF(T, swl_0)) );

  __ lw(t1, MemOperand(a0, OFFSET_OF(T, mem_init)) );
  __ sw(t1, MemOperand(a0, OFFSET_OF(T, swl_1)) );
  __ lw(t1, MemOperand(a0, OFFSET_OF(T, reg_init)) );
  __ swl(t1, MemOperand(a0, OFFSET_OF(T, swl_1) + 1) );

  __ lw(t2, MemOperand(a0, OFFSET_OF(T, mem_init)) );
  __ sw(t2, MemOperand(a0, OFFSET_OF(T, swl_2)) );
  __ lw(t2, MemOperand(a0, OFFSET_OF(T, reg_init)) );
  __ swl(t2, MemOperand(a0, OFFSET_OF(T, swl_2) + 2) );

  __ lw(t3, MemOperand(a0, OFFSET_OF(T, mem_init)) );
  __ sw(t3, MemOperand(a0, OFFSET_OF(T, swl_3)) );
  __ lw(t3, MemOperand(a0, OFFSET_OF(T, reg_init)) );
  __ swl(t3, MemOperand(a0, OFFSET_OF(T, swl_3) + 3) );

  // Test all combinations of SWR and vAddr
  __ lw(t0, MemOperand(a0, OFFSET_OF(T, mem_init)) );
  __ sw(t0, MemOperand(a0, OFFSET_OF(T, swr_0)) );
  __ lw(t0, MemOperand(a0, OFFSET_OF(T, reg_init)) );
  __ swr(t0, MemOperand(a0, OFFSET_OF(T, swr_0)) );

  __ lw(t1, MemOperand(a0, OFFSET_OF(T, mem_init)) );
  __ sw(t1, MemOperand(a0, OFFSET_OF(T, swr_1)) );
  __ lw(t1, MemOperand(a0, OFFSET_OF(T, reg_init)) );
  __ swr(t1, MemOperand(a0, OFFSET_OF(T, swr_1) + 1) );

  __ lw(t2, MemOperand(a0, OFFSET_OF(T, mem_init)) );
  __ sw(t2, MemOperand(a0, OFFSET_OF(T, swr_2)) );
  __ lw(t2, MemOperand(a0, OFFSET_OF(T, reg_init)) );
  __ swr(t2, MemOperand(a0, OFFSET_OF(T, swr_2) + 2) );

  __ lw(t3, MemOperand(a0, OFFSET_OF(T, mem_init)) );
  __ sw(t3, MemOperand(a0, OFFSET_OF(T, swr_3)) );
  __ lw(t3, MemOperand(a0, OFFSET_OF(T, reg_init)) );
  __ swr(t3, MemOperand(a0, OFFSET_OF(T, swr_3) + 3) );

  __ jr(ra);
  __ nop();

  CodeDesc desc;
  assm.GetCode(&desc);
  Object* code = Heap::CreateCode(desc,
                                  NULL,
                                  Code::ComputeFlags(Code::STUB),
                                  Handle<Object>(Heap::undefined_value()));
  CHECK(code->IsCode());
#ifdef DEBUG
  Code::cast(code)->Print();
#endif
  F3 f = FUNCTION_CAST<F3>(Code::cast(code)->entry());
  t.reg_init = 0xaabbccdd;
  t.mem_init = 0x11223344;

  Object* dummy = CALL_GENERATED_CODE(f, &t, 0, 0, 0, 0);
  USE(dummy);

  CHECK_EQ(0x44bbccdd, t.lwl_0);
  CHECK_EQ(0x3344ccdd, t.lwl_1);
  CHECK_EQ(0x223344dd, t.lwl_2);
  CHECK_EQ(0x11223344, t.lwl_3);

  CHECK_EQ(0x11223344, t.lwr_0);
  CHECK_EQ(0xaa112233, t.lwr_1);
  CHECK_EQ(0xaabb1122, t.lwr_2);
  CHECK_EQ(0xaabbcc11, t.lwr_3);

  CHECK_EQ(0x112233aa, t.swl_0);
  CHECK_EQ(0x1122aabb, t.swl_1);
  CHECK_EQ(0x11aabbcc, t.swl_2);
  CHECK_EQ(0xaabbccdd, t.swl_3);

  CHECK_EQ(0xaabbccdd, t.swr_0);
  CHECK_EQ(0xbbccdd44, t.swr_1);
  CHECK_EQ(0xccdd3344, t.swr_2);
  CHECK_EQ(0xdd223344, t.swr_3);
}
#undef __
