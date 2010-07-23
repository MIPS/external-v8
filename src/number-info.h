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

#ifndef V8_NUMBER_INFO_H_
#define V8_NUMBER_INFO_H_

#include "globals.h"

namespace v8 {
namespace internal {

//        Unknown
//           |
//      PrimitiveType
//           |   \--------|
//         Number      String
//         /    |         |
//    Double  Integer32   |
//        |      |       /
//        |     Smi     /
//        |     /      /
//        Uninitialized.

class NumberInfo {
 public:
  NumberInfo() { }

  static inline NumberInfo Unknown();
  // We know it's a primitive type.
  static inline NumberInfo Primitive();
  // We know it's a number of some sort.
  static inline NumberInfo Number();
  // We know it's signed or unsigned 32 bit integer.
  static inline NumberInfo Integer32();
  // We know it's a Smi.
  static inline NumberInfo Smi();
  // We know it's a heap number.
  static inline NumberInfo Double();
  // We know it's a string.
  static inline NumberInfo String();
  // We haven't started collecting info yet.
  static inline NumberInfo Uninitialized();

  // Return compact representation.  Very sensitive to enum values below!
  // Compacting drops information about primtive types and strings types.
  // We use the compact representation when we only care about number types.
  int ThreeBitRepresentation() {
    ASSERT(type_ != kUninitializedType);
    int answer = type_ & 0xf;
    answer = answer > 6 ? answer - 2 : answer;
    ASSERT(answer >= 0);
    ASSERT(answer <= 7);
    return answer;
  }

  // Decode compact representation.  Very sensitive to enum values below!
  static NumberInfo ExpandedRepresentation(int three_bit_representation) {
    Type t = static_cast<Type>(three_bit_representation >= 6 ?
                               three_bit_representation + 2 :
                               three_bit_representation);
    t = (t == kUnknownType) ? t : static_cast<Type>(t | kPrimitiveType);
    ASSERT(t == kUnknownType ||
           t == kNumberType ||
           t == kInteger32Type ||
           t == kSmiType ||
           t == kDoubleType);
    return NumberInfo(t);
  }

  int ToInt() {
    return type_;
  }

  static NumberInfo FromInt(int bit_representation) {
    Type t = static_cast<Type>(bit_representation);
    ASSERT(t == kUnknownType ||
           t == kPrimitiveType ||
           t == kNumberType ||
           t == kInteger32Type ||
           t == kSmiType ||
           t == kDoubleType ||
           t == kStringType);
    return NumberInfo(t);
  }

  // Return the weakest (least precise) common type.
  static NumberInfo Combine(NumberInfo a, NumberInfo b) {
    return NumberInfo(static_cast<Type>(a.type_ & b.type_));
  }


  // Integer32 is an integer that can be represented as either a signed
  // 32-bit integer or as an unsigned 32-bit integer. It has to be
  // in the range [-2^31, 2^32 - 1]. We also have to check for negative 0
  // as it is not an Integer32.
  static inline bool IsInt32Double(double value) {
    const DoubleRepresentation minus_zero(-0.0);
    DoubleRepresentation rep(value);
    if (rep.bits == minus_zero.bits) return false;
    if (value >= kMinInt && value <= kMaxUInt32) {
      if (value <= kMaxInt && value == static_cast<int32_t>(value)) {
        return true;
      }
      if (value == static_cast<uint32_t>(value)) return true;
    }
    return false;
  }

  static inline NumberInfo TypeFromValue(Handle<Object> value);

  inline bool IsUnknown() {
    return type_ == kUnknownType;
  }

  inline bool IsNumber() {
    ASSERT(type_ != kUninitializedType);
    return ((type_ & kNumberType) == kNumberType);
  }

  inline bool IsSmi() {
    ASSERT(type_ != kUninitializedType);
    return ((type_ & kSmiType) == kSmiType);
  }

  inline bool IsInteger32() {
    ASSERT(type_ != kUninitializedType);
    return ((type_ & kInteger32Type) == kInteger32Type);
  }

  inline bool IsDouble() {
    ASSERT(type_ != kUninitializedType);
    return ((type_ & kDoubleType) == kDoubleType);
  }

  inline bool IsUninitialized() {
    return type_ == kUninitializedType;
  }

  const char* ToString() {
    switch (type_) {
      case kUnknownType: return "UnknownType";
      case kPrimitiveType: return "PrimitiveType";
      case kNumberType: return "NumberType";
      case kInteger32Type: return "Integer32Type";
      case kSmiType: return "SmiType";
      case kDoubleType: return "DoubleType";
      case kStringType: return "StringType";
      case kUninitializedType:
        UNREACHABLE();
        return "UninitializedType";
    }
    UNREACHABLE();
    return "Unreachable code";
  }

 private:
  // We use 6 bits to represent the types.
  enum Type {
    kUnknownType = 0,          // 000000
    kPrimitiveType = 0x10,     // 010000
    kNumberType = 0x11,        // 010001
    kInteger32Type = 0x13,     // 010011
    kSmiType = 0x17,           // 010111
    kDoubleType = 0x19,        // 011001
    kStringType = 0x30,        // 110000
    kUninitializedType = 0x3f  // 111111
  };
  explicit inline NumberInfo(Type t) : type_(t) { }

  Type type_;
};


NumberInfo NumberInfo::Unknown() {
  return NumberInfo(kUnknownType);
}


NumberInfo NumberInfo::Primitive() {
  return NumberInfo(kPrimitiveType);
}


NumberInfo NumberInfo::Number() {
  return NumberInfo(kNumberType);
}


NumberInfo NumberInfo::Integer32() {
  return NumberInfo(kInteger32Type);
}


NumberInfo NumberInfo::Smi() {
  return NumberInfo(kSmiType);
}


NumberInfo NumberInfo::Double() {
  return NumberInfo(kDoubleType);
}


NumberInfo NumberInfo::String() {
  return NumberInfo(kStringType);
}


NumberInfo NumberInfo::Uninitialized() {
  return NumberInfo(kUninitializedType);
}

} }  // namespace v8::internal

#endif  // V8_NUMBER_INFO_H_
