/************************************************************************/
/* Copyright (c) 2018 CBrain A/S. Version modified from original version by Cristian Adam
 * Copyright (c) 2008 Cristian Adam.

This software is provided 'as-is', without any express or implied
warranty. In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

    1. The origin of this software must not be misrepresented; you must not
    claim that you wrote the original software. If you use this software
    in a product, an acknowledgment in the product documentation would be
    appreciated but is not required.

    2. Altered source versions must be plainly marked as such, and must not be
    misrepresented as being the original software.

    3. This notice may not be removed or altered from any source
    distribution.

/************************************************************************/


#pragma once

#include <stdio.h>
#include <tchar.h>

#include "CmnHdr.h"

#include <windows.h>
#include <shlwapi.h>
#include <bcrypt.h>
#include <wincrypt.h>
#include <winternl.h>

#include <memory>
#include <map>
#include <iostream>
#include <sstream>
#include <vector>

#include <comdef.h>
#include <fstream>
#include <algorithm>
#include <cwctype>
#include <unordered_map>