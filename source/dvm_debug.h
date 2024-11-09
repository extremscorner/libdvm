// SPDX-License-Identifier: ZPL-2.1
// SPDX-FileCopyrightText: Copyright fincs, devkitPro
#pragma once

//#define LIBDVM_DEBUG

#if defined(LIBDVM_DEBUG) && defined(__NDS__)
#include <calico/system/dietprint.h>
#define dvmDebug(...) dietPrint(__VA_ARGS__)
#else
#define dvmDebug(...) ((void)0)
#endif
