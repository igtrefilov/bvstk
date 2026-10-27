#ifndef BVSTK_NEUTRINO_PROFILE_STORE_H
#define BVSTK_NEUTRINO_PROFILE_STORE_H
#include <stddef.h>
/* Callback validates and appends one profile. Invalid input stops this bus load;
 * SIZE_MAX distinguishes an invalid/unreadable set from an absent/empty one. */
typedef int (*bvstk_neutrino_profile_load_fn)(void *, const char *, const char *);
size_t bvstk_neutrino_profile_load(const char *, bvstk_neutrino_profile_load_fn, void *);
int bvstk_neutrino_profile_mkdir(const char *);
int bvstk_neutrino_profile_save(const char *, const char *, const char *, size_t);
#endif
