/*!
\author Rémi Attab
\license FreeBSD (see LICENSE file)

Some globally usefull check macros.

Note that we use macros to preserve the line number of the callsite which shows up in the
error messages of the fail_xxx macros.
*/

#ifndef YARN_T_UTILS_H_
#define YARN_T_UTILS_H_


#include <helper.h>
#include <bits.h>


// Time related stuff
#define STR_TS "(%zums) "
void set_base_time();
long get_rel_time();



// Testing values that is adjusted based on the machine's word size.
#ifdef YARN_WORD_64
#  define YARN_T_VALUE_1 0xAAAAAAAAAAAAAAAA // 1010
#  define YARN_T_VALUE_2 0x5555555555555555 // 0101
#  define YARN_T_VALUE_3 0x9999999999999999 // 1001
#  define YARN_T_VALUE_4 0x6666666666666666 // 0110
#else
#  define YARN_T_VALUE_1 0xAAAAAAAA
#  define YARN_T_VALUE_2 0x55555555
#  define YARN_T_VALUE_3 0x99999999
#  define YARN_T_VALUE_4 0x66666666
#endif


// yarn_epoch utilities.

#define t_yarn_check_status(status, expected)				\
  do {									\
    fail_if((status) != (expected), "status=%d, expected=%d", (status), (expected)); \
  } while(false)

#define t_yarn_check_epoch_status(epoch, expected)			\
  do {									\
    enum yarn_epoch_status status = yarn_epoch_get_status(epoch);	\
    t_yarn_check_status((status), (expected));				\
  } while(false)


#define t_yarn_set_epoch_data(epoch, value)	\
  do {						\
    yarn_epoch_set_task((epoch), (value));	\
  } while(false)

#define t_yarn_check_data(task, expected)				\
  do {									\
    fail_if((task) != (expected), "task=%p, expected=%p", (task), (expected)); \
  } while(false)

#define t_yarn_check_epoch_data(epoch, expected)	\
  do {							\
    void* task = (void*) yarn_epoch_get_task(epoch);	\
    t_yarn_check_data(task, expected);			\
  } while(false)



// yarn_dep utilities.

#define t_yarn_check_dep_load(pool_id, src, expected)			\
  do {									\
    yarn_word_t dest = 0;						\
    bool ret = yarn_dep_load((pool_id), (void*)(src), (void*)(&dest));	\
    fail_if(!ret);							\
    fail_if (dest != (expected),					\
	     "LOAD -> pid=%zu, src="YARN_SHEX", dest="YARN_SHEX		\
	     ", expected="YARN_SHEX,					\
	     (pool_id), YARN_AHEX(*(src)), YARN_AHEX(dest),		\
	     YARN_AHEX((expected)));					\
  } while(false)

#define t_yarn_check_dep_load_fast(pool_id, index, src, expected)	\
  do {									\
    yarn_word_t dest = 0;						\
    bool ret = yarn_dep_load_fast((pool_id), (index), (void*)(src), (void*)(&dest)); \
    fail_if(!ret);							\
    fail_if (dest != (expected),					\
	     "LOAD_F -> pid=%zu, src="YARN_SHEX", dest="YARN_SHEX	\
	     ", expected="YARN_SHEX,					\
	     (pool_id), YARN_AHEX(*(src)), YARN_AHEX(dest),		\
	     YARN_AHEX(expected));					\
  } while(false)


#define t_yarn_check_dep_store(pool_id, dest, expected)			\
  do {									\
    const yarn_word_t src = expected;					\
    bool ret = yarn_dep_store((pool_id), (void*)(&src), (void*)(dest));	\
    fail_if (!ret);							\
  } while (false)

#define t_yarn_check_dep_store_fast(pool_id, index, dest, expected)	\
  do {									\
    const yarn_word_t src = expected;					\
    bool ret = yarn_dep_store_fast((pool_id), index, (void*)(&src), (void*)(dest)); \
    fail_if(!ret);							\
  } while (false)


#define t_yarn_check_dep_mem(pool_id, mem, expected, prefix)		\
  do {									\
    fail_if ((mem) != (expected),					\
	     "%s -> pid=%zu, mem="YARN_SHEX", expected="YARN_SHEX,	\
	     (prefix), (pool_id), YARN_AHEX(mem), YARN_AHEX(expected));	\
  } while (false)

#endif
