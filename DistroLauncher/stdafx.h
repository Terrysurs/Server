//
//    Copyright (C) Microsoft.  All rights reserved.
// Licensed under the terms described in the LICENSE file in the root of this project.
//
// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#include "targetver.h"

#include <stdio.h>
#include <tchar.h>
#include <Windows.h>
#include <stdio.h>
#include <conio.h>
#include <io.h>
#include <string>
#include <memory>
#include <assert.h>
#include <locale>
#include <codecvt>
#include <string_view>
#include <vector>
#include <wslapi.h>
#include <fstream>
#include <array>
#define SECURITY_WIN32
#include <sspi.h>
#include <secext.h>
#include <unordered_map>
#include <yaml-cpp/yaml.h>
#include <nonstd/expected.hpp>
#include "WslApiLoader.h"
#include "Helpers.h"
#include "DistributionInfo.h"
#include "OOBE.h"
#include "ProcessRunner.h"
#include "WSLInfo.h"
#include "Win32Utils.h"
#include "YamlExtensions.h"

// Message strings compiled from .MC file.
#include "messages.h"
