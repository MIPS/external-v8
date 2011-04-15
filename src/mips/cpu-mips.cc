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

// CPU specific code for arm independent of OS goes here.

#include <sys/syscall.h>
#include <unistd.h>

#ifdef __mips
#include <asm/cachectl.h>
#endif  // #ifdef __mips

#include "v8.h"

#if defined(V8_TARGET_ARCH_MIPS)

#include "cpu.h"
#include "macro-assembler.h"

#include "simulator.h"  // for cache flushing.

namespace v8 {
namespace internal {


void CPU::Setup() {
  CpuFeatures::Probe();
}


void CPU::FlushICache(void* start, size_t size) {
#ifdef __mips

#if defined(ANDROID)
  char *end = reinterpret_cast<char *>(start) + size;
  cacheflush(reinterpret_cast<intptr_t>(start), reinterpret_cast<intptr_t>(end), 0);
#else
  int res;
  // See http://www.linux-mips.org/wiki/Cacheflush_Syscall
  res = syscall(__NR_cacheflush, start, size, ICACHE);

  if (res) {
    V8_Fatal(__FILE__, __LINE__, "Failed to flush the instruction cache");
  }
#endif  //ANDROID

#else  // simulator mode
  // Not generating mips instructions for C-code. This means that we are
  // building a mips emulator based target.  We should notify the simulator
  // that the Icache was flushed.
  // None of this code ends up in the snapshot so there are no issues
  // around whether or not to generate the code when building snapshots.
  assembler::mips::Simulator::FlushICache(start, size);
#endif    // #ifdef __mips
}


void CPU::DebugBreak() {
#ifdef __mips
  asm volatile("break");
#endif  // #ifdef __mips
}


} }  // namespace v8::internal

#endif  // V8_TARGET_ARCH_MIPS
