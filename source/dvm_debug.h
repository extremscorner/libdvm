#pragma once

//#define LIBDVM_DEBUG

#if defined(LIBDVM_DEBUG) && defined(LIBDVM_CALICO)
#include <calico/system/dietprint.h>
#define dvmDebug(...) dietPrint(__VA_ARGS__)
#else
#define dvmDebug(...) ((void)0)
#endif
