/*!
\author Rémi Attab
\license FreeBSD (see the LICENSE file)

Keeper of the one and only speculative timeline.

\warning yarn_epoch is not guaranteed to work correctly if 
\code yarn_tpool_size() <= sizeof(yarn_word_t)-1 \endcode. If we ever enter an
era of 30+ core CPUs then this class will need to be updated so that it works better.
 */

#ifndef YARN_EPOCH_H_
#define YARN_EPOCH_H_


#include <types.h>


enum yarn_epoch_status {
  yarn_epoch_waiting, 
  yarn_epoch_executing,
  yarn_epoch_done,
  yarn_epoch_rollback,
  yarn_epoch_commit
};



bool yarn_epoch_init(void);
void yarn_epoch_destroy(void);

yarn_word_t yarn_epoch_first(void);
yarn_word_t yarn_epoch_last(void);
yarn_word_t yarn_epoch_next(void);

void yarn_epoch_do_rollback(yarn_word_t start);
yarn_word_t yarn_epoch_do_commit(void** task, void** data);

enum yarn_epoch_status yarn_epoch_get_status (yarn_word_t epoch);
void yarn_epoch_set_status(yarn_word_t epoch, enum yarn_epoch_status status);

// Stores the generated task with the epoch.
void* yarn_epoch_get_task (yarn_word_t epoch);
void yarn_epoch_set_task (yarn_word_t epoch, void* task);

// This is used in yarn_dep to store a linked list of all the dep associated with an epoch.
void* yarn_epoch_get_data(yarn_word_t epoch);
void yarn_epoch_set_data(yarn_word_t epoch, void* data);


#endif // YARN_EPOCH_H_
