/*!
\author Rémi Attab
\license FreeBSD (see LICENSE file)

\brief Lock-free dictionnary data structure.

Note that this map only supports a single operand. We do this because it's much easier
to synchronize if we don't have to worry about removing elements. This means that if you
want to clear the table, just free it and re-initialize it.

 */

#ifndef YARN_MAP_H_
#define YARN_MAP_H_


#include "yarn/types.h"

struct yarn_map;
typedef void (*yarn_map_destructor) (void*);


//! Initializes a new map object.
struct yarn_map* yarn_map_init (size_t capacity);

//! Deletes every element in the table and re-initializes it with the provided capapcity.
bool yarn_map_reset(struct yarn_map* m, yarn_map_destructor des, size_t capacity);

//! Frees a map object.
void yarn_map_destroy (struct yarn_map* m, yarn_map_destructor des); 


/*!
Looks for the given address and returns the value if present. Otherwise it adds the 
provided value.
*/
void* yarn_map_probe (struct yarn_map* m, uintptr_t addr, void* value);

//! Returns the number of items in the map.
size_t yarn_map_size (struct yarn_map* m);

/*!
Dumps the current state of the hash table (DEBUG ONLY).
\warning This is not thread safe. Use only as a debugging helper.
*/
void yarn_map_dbg_dump (struct yarn_map* m);



#endif // YARN_MAP_H_
