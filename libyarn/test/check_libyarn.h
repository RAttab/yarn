/*!
\author Rémi Attab
\license FreeBSD (see license file).

Suite declariation for the libyarn tests.
*/


#ifndef YARN_CHECK_LIBYARN_H_
#define YARN_CHECK_LIBYARN_H_

#include <check.h>


Suite* yarn_bits_suite();

Suite* yarn_tpool_suite();
Suite* yarn_pstore_suite();
Suite* yarn_pmem_suite();

Suite* yarn_epoch_suite();

Suite* yarn_map_suite();


#endif
