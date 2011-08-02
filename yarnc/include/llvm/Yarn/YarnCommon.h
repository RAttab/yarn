//===- YarnCommon.h - Example code from "Writing an LLVM Pass" ------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the FreeBSD License.
// See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// Common utility header for the include folder.
//
//===----------------------------------------------------------------------===//

#ifndef YARN_COMMON_H
#define YARN_COMMON_H

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

  enum {
    YarnRetBitSize = sizeof(yarn_ret)*8
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



} // namespace yarn

#endif // YARN_COMMON_H
