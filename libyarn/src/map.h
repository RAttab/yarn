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


#include <stdint.h>
#include <stddef.h>


struct yarn_map;


//! Initializes a new map object.
struct yarn_map* yarn_map_init (size_t capacity);

//! Frees a map object.
void yarn_map_free (struct yarn_map* m); 

/*!
Looks for the given address and returns the value if present. Otherwise it adds the 
provided value.
*/
void* yarn_map_probe (struct yarn_map* m, uintptr_t addr, void* value);



#endif // YARN_MAP_H_
