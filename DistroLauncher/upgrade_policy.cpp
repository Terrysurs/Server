/*
 * Copyright (C) 2022 Canonical Ltd
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "stdafx.h"

namespace internal
{

    bool starts_with(const std::wstring_view tested, const std::wstring_view start)
    {
        if (tested.size() < start.size()) {
            return false;
        }
        auto mismatch = std::mismatch(start.cbegin(), start.cend(), tested.cbegin());
        return mismatch.first == start.cend();
    }

    bool ends_with(const std::wstring_view tested, const std::wstring_view end)
    {
        if (tested.size() < end.size()) {
            return false;
        }
        auto mismatch = std::mismatch(end.crbegin(), end.crend(), tested.crbegin());
        return mismatch.first == end.crend();
    }

    std::wstring GetUpgradePolicy()
    {
        std::wstring_view name = DistributionInfo::Name;
        if (name == L"Ubuntu") {
            return L"lts";
        }
        if (starts_with(name, L"Ubuntu") && ends_with(name, L"LTS")) {
            return L"never";
        }
        return L"normal";
    }

    bool WslExists(std::filesystem::path linux_path)
    {
        std::error_code err;
        const bool found = std::filesystem::exists(Oobe::WindowsPath(linux_path), err);
        if (!err) {
            return found;
        }

        // Fallback
        const auto cmd = concat(L"test -f ", linux_path, L" > /dev/null 2>&1");
        DWORD exitCode;
        const HRESULT hr = g_wslApi.WslLaunchInteractive(cmd.c_str(), FALSE, &exitCode);
        if (SUCCEEDED(hr)) {
            return exitCode == 0;
        }
        return false;
    }

    void SetDefaultUpgradePolicyImpl()
    {
        namespace fs = std::filesystem;

        const fs::path log{L"/var/log/upgrade-policy-changed.log"};
        const fs::path policyfile{L"/etc/update-manager/release-upgrades"};

        if (WslExists(log)) {
            return;
        }

        std::wstring regex = concat(L"s/Prompt=lts/Prompt=", GetUpgradePolicy(), L'/');
        std::wstring sed = concat(L"sed -i ", std::quoted(regex), L' ', policyfile);
        std::wstring date = concat(L"date --iso-8601=seconds > ", log);

        std::wstring command = concat(L"bash -ec ", std::quoted(concat(sed, L" && ", date)));

        DWORD errCode;
        Sudo::WslLaunchInteractive(command.c_str(), FALSE, &errCode);
    }

}

void SetDefaultUpgradePolicy()
{
    auto mutex = NamedMutex(L"upgrade-policy");
    mutex.lock().and_then(internal::SetDefaultUpgradePolicyImpl);
}
