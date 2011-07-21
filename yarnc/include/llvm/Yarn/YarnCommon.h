//===- YarnLoop.h - Example code from "Writing an LLVM Pass" --------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the FreeBSD License.
// See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// Yarnc loop analysis pass.
//
//===----------------------------------------------------------------------===//

#ifndef YARN_COMMON_H
#define YARN_COMMON_H

#include <cstdint>

namespapce yarn {

  
//===----------------------------------------------------------------------===//
/// These type are used to declare the libyarn interface.
///
  typedef uint_fast32_t YarnWord;
  enum {
    YarnWordBitSize = (sizeof(YarnWord)*8);
    YarnWord64 = YarnWordBitSize == 64;
  };



//===----------------------------------------------------------------------===//
/// Derive from this class to make sure that it can't be copyed.
/// There might be a reason why llvm doesn't use one of these...
///
  class Noncopyable {
    Noncopyable (const Noncopyable&);
    Noncopyable& operator= (const Noncopyable&);
  };


//===----------------------------------------------------------------------===//
/// Utility class for std::vector
///
  struct VectorUtil<typename T> {
    static void free (std::vector<T*>& v) {
      while(!v.empty()) {
	T* = v.pop_back();
	delete T;
      }
    }
  }


}; // namespace yarn

#endif // YARN_COMMON_H
