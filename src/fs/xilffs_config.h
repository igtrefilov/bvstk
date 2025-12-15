#ifndef XILFFS_CONFIG_H
#define XILFFS_CONFIG_H

#include "xparameters.h"

#ifdef FILE_SYSTEM_USE_LFN
#undef FILE_SYSTEM_USE_LFN
#endif
#define FILE_SYSTEM_USE_LFN 3

#endif /* XILFFS_CONFIG_H */
