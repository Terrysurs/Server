/*
 * Copyright (C) 2021 Canonical Ltd
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

namespace DistributionInfo {
	// Implementation details.
	namespace {
		// WindowsUserInfo holds together the user information retrieved from Win32 API's.
		struct WindowsUserInfo {
			std::wstring userName;
			std::wstring realName;
			std::wstring localeName;

			std::string toYamlUtf8() const;
		};

		// PrintLastError converts the last error code from Win32 API's into
		// an error message.
		inline void PrintLastError() {
			HRESULT error = HRESULT_FROM_WIN32(GetLastError());
			Helpers::PrintErrorMessage(error);
		}

		// toYaml hand-codes the YAML generation to avoid adding a lib just
		// because of such small feature, which would be an overkill. Shall the need
		// for more YAML manipulation in the DistroLauncher arise, thus function should
		// be changed to use a proper YAML manipulation library, such as yaml-cpp.
		std::string WindowsUserInfo::toYamlUtf8() const {
			// yaml-cpp doesn't support wide chars.
			std::unordered_map<std::string,
				std::unordered_map<std::string, std::string>> sections;
			if (!localeName.empty()) {
				auto conv = Win32Utils::wide_string_to_utf8(localeName);
				if(conv){
					sections["Welcome"]["lang"]=conv.value();
				} else {
					Helpers::PrintMessage(MSG_ERROR_CODE, HRESULT_FROM_WIN32(GetLastError()),conv.error().c_str());
				}
			}

			if (!realName.empty()) {
				auto conv = Win32Utils::wide_string_to_utf8(realName);
				if(conv){
					sections["WSLIdentity"]["realname"]= conv.value();
				} else {
					Helpers::PrintMessage(MSG_ERROR_CODE, HRESULT_FROM_WIN32(GetLastError()), conv.error().c_str());
				}
			}

			if (!userName.empty()) {
				auto conv = Win32Utils::wide_string_to_utf8(userName);
				if(conv){
					sections["WSLIdentity"]["username"]= conv.value();
				} else {
					Helpers::PrintMessage(MSG_ERROR_CODE, HRESULT_FROM_WIN32(GetLastError()), conv.error().c_str());
				}
			}

			YAML::Emitter out;
			out << sections << YAML::Newline;
			if (!out.good()) {
				return "";
			}

			// Ensure UTF-8 BOM is present just as a precaution.
			std::string retVal{ "\xEF\xBB\xBF" };
			retVal.append(out.c_str());
			return retVal;
		} // std::wstring WindowsUserInfo::toYaml()


		// QueryWindowsUserInfo queries Win32 API's to provide launcher with locale, user real and login names.
		// Those pieces of information will be used in the Ubuntu OOBE to enhance the UX.
		WindowsUserInfo QueryWindowsUserInfo()
		{
			DistributionInfo::WindowsUserInfo userInfo;
			const int size = LOCALE_NAME_MAX_LENGTH;
			WCHAR loc[size];

			std::size_t result = GetUserDefaultLocaleName(loc, LOCALE_NAME_MAX_LENGTH);
			// `Prefill` should handle partial data, thus a missing piece of information is not a function failure.
			if (result == 0) {
				PrintLastError();
			} else {
				std::wstring_view view{ loc,result };
				std::size_t dashPos = view.find_first_of('-');
				if (dashPos > 0 && dashPos < result) {
					loc[dashPos] = '_';
				}
				// (result-1) because GetUserDefaultLocaleName includes the null-terminating char, not needed here.
				userInfo.localeName = std::wstring{ loc, result - 1 };
			}

			WCHAR userRealName[size];
			ULONG nSize = size;
			BOOLEAN ret = GetUserNameExW(EXTENDED_NAME_FORMAT::NameDisplay, userRealName, &nSize);
			if (ret == 0) {
				PrintLastError();
			} else {
				userInfo.realName = std::wstring{ userRealName,nSize };
			}
			
			nSize = size;
			ret = GetUserNameExW(EXTENDED_NAME_FORMAT::NameSamCompatible, userRealName, &nSize);
			if (ret == 0) {
				PrintLastError();
			} else {
				std::wstring_view samView{ userRealName,nSize };
				std::wstring_view justTheUserName = samView.substr(samView.find_first_of('\\') + 1, samView.length());
				userInfo.userName = std::wstring{ justTheUserName };
			}

			return userInfo;
		} // WindowsUserInfo QueryWindowsUserInfo().

	} // namespace.

	// GetPrefillInfoInYaml exports Windows User Information as an YAML UTF-8
	// encoded string.
	// This is the only symbol visible outside of this translation unit.
	std::string GetPrefillInfoInYaml() {
		WindowsUserInfo userInfo = DistributionInfo::QueryWindowsUserInfo();
		return userInfo.toYamlUtf8();
	} // std::wstring GetPrefillInfoInYaml().

} // namespace DistributionInfo.