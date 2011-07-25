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

#include <vector>
#include <tr1/cstdint>

namespace yarn {

  
//===----------------------------------------------------------------------===//
/// These type are used to declare the libyarn interface.
/// \todo Should be aggregated into a common header or libyarn and yarnc.
///
  typedef uint_fast32_t YarnWord;
  enum {
    YarnWordBitSize = (sizeof(YarnWord)*8),
    YarnWord64 = YarnWordBitSize == 64
  };


//===----------------------------------------------------------------------===//
/// Return values declared in yarn.h for the instrumented function.
/// \todo Should be aggregated into a common header or libyarn and yarnc.
///
  enum yarn_ret {
    yarn_ret_continue = 0,
    yarn_ret_break = 1,
    yarn_ret_error = 2
  };




//===----------------------------------------------------------------------===//
/// Derive from this class to make sure that it can't be copyed.
/// There might be a reason why llvm doesn't use one of these...
///
  class Noncopyable {
    Noncopyable (const Noncopyable&);
    Noncopyable& operator= (const Noncopyable&);
  public:
    Noncopyable () {}
  };


//===----------------------------------------------------------------------===//
/// Utility class for std::vector
///
  template<typename T>
  struct VectorUtil {
    static void free (std::vector<T*>& v) {
      while(!v.empty()) {
	T* ptr = v.back();
	v.pop_back();
	delete ptr;
      }
    }
  };


} // namespace yarn

#endif // YARN_COMMON_H
