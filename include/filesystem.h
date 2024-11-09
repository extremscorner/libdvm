// SPDX-License-Identifier: ZPL-2.1
// SPDX-FileCopyrightText: Copyright fincs, devkitPro
#pragma once
#include <calico/nds/nitrorom.h>

#ifdef __cplusplus
extern "C" {
#endif

bool nitroFSInit(char** basepath);

bool nitroFSMount(NitroRom* nr);

#ifdef __cplusplus
}
#endif
