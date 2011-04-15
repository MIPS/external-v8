LOCAL_CPP_EXTENSION := .cc

V8_LOCAL_SRC_FILES := \
	src/accessors.cc \
	src/allocation.cc \
	src/api.cc \
	src/assembler.cc \
	src/ast.cc \
	src/bignum.cc \
	src/bignum-dtoa.cc \
	src/bootstrapper.cc \
	src/builtins.cc \
	src/cached-powers.cc \
	src/checks.cc \
	src/circular-queue.cc \
	src/code-stubs.cc \
	src/codegen.cc \
	src/compilation-cache.cc \
	src/compiler.cc \
	src/contexts.cc \
	src/conversions.cc \
	src/counters.cc \
	src/cpu-profiler.cc \
	src/data-flow.cc \
	src/dateparser.cc \
	src/debug-agent.cc \
	src/debug.cc \
	src/disassembler.cc \
	src/diy-fp.cc \
	src/dtoa.cc \
	src/execution.cc \
	src/factory.cc \
	src/flags.cc \
	src/frame-element.cc \
	src/frames.cc \
	src/full-codegen.cc \
	src/func-name-inferrer.cc \
	src/global-handles.cc \
	src/fast-dtoa.cc \
	src/fixed-dtoa.cc \
	src/handles.cc \
	src/hashmap.cc \
	src/heap-profiler.cc \
	src/heap.cc \
	src/ic.cc \
	src/interpreter-irregexp.cc \
	src/jsregexp.cc \
	src/jump-target.cc \
	src/liveedit.cc \
	src/log-utils.cc \
	src/log.cc \
	src/mark-compact.cc \
	src/messages.cc \
	src/objects.cc \
	src/objects-visiting.cc \
	src/oprofile-agent.cc \
	src/parser.cc \
	src/platform-linux.cc \
	src/platform-posix.cc \
	src/preparser.cc \
	src/preparse-data.cc \
	src/profile-generator.cc \
	src/property.cc \
	src/regexp-macro-assembler-irregexp.cc \
	src/regexp-macro-assembler.cc \
	src/regexp-stack.cc \
	src/register-allocator.cc \
	src/rewriter.cc \
	src/runtime.cc \
	src/scanner-base.cc \
	src/scanner.cc \
	src/scopeinfo.cc \
	src/scopes.cc \
	src/serialize.cc \
	src/snapshot-common.cc \
	src/spaces.cc \
	src/string-search.cc \
	src/string-stream.cc \
	src/strtod.cc \
	src/stub-cache.cc \
	src/token.cc \
	src/top.cc \
	src/type-info.cc \
	src/unicode.cc \
	src/utils.cc \
	src/v8-counters.cc \
	src/v8.cc \
	src/v8threads.cc \
	src/variables.cc \
	src/version.cc \
	src/virtual-frame.cc \
	src/zone.cc \
	src/extensions/gc-extension.cc \
	src/extensions/externalize-string-extension.cc

ifeq ($(TARGET_ARCH),arm)
  V8_LOCAL_SRC_FILES += \
	src/jump-target-light.cc \
	src/virtual-frame-light.cc \
	src/arm/builtins-arm.cc \
	src/arm/code-stubs-arm.cc \
	src/arm/codegen-arm.cc \
	src/arm/constants-arm.cc \
	src/arm/cpu-arm.cc \
	src/arm/debug-arm.cc \
	src/arm/disasm-arm.cc \
	src/arm/frames-arm.cc \
	src/arm/full-codegen-arm.cc \
	src/arm/ic-arm.cc \
	src/arm/jump-target-arm.cc \
	src/arm/macro-assembler-arm.cc \
	src/arm/regexp-macro-assembler-arm.cc \
	src/arm/register-allocator-arm.cc \
	src/arm/stub-cache-arm.cc \
	src/arm/virtual-frame-arm.cc \
	src/arm/assembler-arm.cc
endif

ifeq ($(TARGET_ARCH),mips)
  V8_LOCAL_SRC_FILES += \
	src/jump-target-light.cc \
	src/virtual-frame-light.cc \
	src/mips/assembler-mips.cc \
	src/mips/builtins-mips.cc \
	src/mips/code-stubs-mips.cc \
	src/mips/codegen-mips.cc \
	src/mips/constants-mips.cc \
	src/mips/cpu-mips.cc \
	src/mips/debug-mips.cc \
	src/mips/disasm-mips.cc \
	src/mips/full-codegen-mips.cc \
	src/mips/frames-mips.cc \
	src/mips/ic-mips.cc \
	src/mips/jump-target-mips.cc \
	src/mips/macro-assembler-mips.cc \
	src/mips/regexp-macro-assembler-mips.cc \
	src/mips/register-allocator-mips.cc \
	src/mips/stub-cache-mips.cc \
	src/mips/virtual-frame-mips.cc
endif

ifeq ($(TARGET_ARCH),x86)
  V8_LOCAL_SRC_FILES += \
	src/jump-target-heavy.cc \
	src/virtual-frame-heavy.cc \
	src/ia32/assembler-ia32.cc \
	src/ia32/builtins-ia32.cc \
	src/ia32/code-stubs-ia32.cc \
	src/ia32/codegen-ia32.cc \
	src/ia32/cpu-ia32.cc \
	src/ia32/debug-ia32.cc \
	src/ia32/disasm-ia32.cc \
	src/ia32/frames-ia32.cc \
	src/ia32/full-codegen-ia32.cc \
	src/ia32/ic-ia32.cc \
	src/ia32/jump-target-ia32.cc \
	src/ia32/macro-assembler-ia32.cc \
	src/ia32/regexp-macro-assembler-ia32.cc \
	src/ia32/register-allocator-ia32.cc \
	src/ia32/stub-cache-ia32.cc \
	src/ia32/virtual-frame-ia32.cc
endif

# Enable DEBUG option.
ifeq ($(DEBUG_V8),true)
  V8_LOCAL_SRC_FILES += \
		src/objects-debug.cc \
		src/prettyprinter.cc \
		src/regexp-macro-assembler-tracer.cc
endif

# V8_LOCAL_SRC_FILES += \
# 	src/dtoa-config.c

V8_LOCAL_JS_LIBRARY_FILES := \
	src/runtime.js \
	src/v8natives.js \
	src/array.js \
	src/string.js \
	src/uri.js \
	src/math.js \
	src/messages.js \
	src/apinatives.js \
	src/date.js \
	src/regexp.js \
	src/json.js \
	src/liveedit-debugger.js \
	src/mirror-debugger.js \
	src/debug-debugger.js \
	src/macros.py

