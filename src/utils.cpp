#include "utils.h"

using namespace monolm;

#if MONOLM_PLATFORM_WINDOWS
#include <Windows.h>

std::wstring Utils::ConvertUtf8ToWide(std::string_view str){
	std::wstring ret;
	if (!ConvertUtf8ToWide(ret, str))
		return {};
	return ret;
}

bool Utils::ConvertUtf8ToWide(std::wstring& dest, std::string_view str) {
	int wlen = MultiByteToWideChar(CP_UTF8, 0, str.data(), static_cast<int>(str.length()), nullptr, 0);
	if (wlen < 0)
		return false;

	dest.resize(static_cast<size_t>(wlen));
	if (wlen > 0 && MultiByteToWideChar(CP_UTF8, 0, str.data(), static_cast<int>(str.length()), dest.data(), wlen) < 0)
		return false;

	return true;
}

std::string Utils::ConvertWideToUtf8(std::wstring_view str) {
	std::string ret;
	if (!ConvertWideToUtf8(ret, str))
		return {};
	return ret;
}

bool Utils::ConvertWideToUtf8(std::string& dest, std::wstring_view str) {
	int mblen = WideCharToMultiByte(CP_UTF8, 0, str.data(), static_cast<int>(str.length()), nullptr, 0, nullptr, nullptr);
	if (mblen < 0)
		return false;

	dest.resize(static_cast<size_t>(mblen));
	if (mblen > 0 && WideCharToMultiByte(CP_UTF8, 0, str.data(), static_cast<int>(str.length()), dest.data(), mblen, nullptr, nullptr) < 0)
		return false;

	return true;
}

std::string Utils::GetEnvVariable(const char* varName) {
	DWORD size = GetEnvironmentVariableA(varName, NULL, 0);
	std::string buffer(size, 0);
	GetEnvironmentVariableA(varName, buffer.data(), size);
	return buffer;
}

bool Utils::SetEnvVariable(const char* varName, const char* value) {
	return SetEnvironmentVariableA(varName, value) != 0;
}
#else
std::string Utils::GetEnvVariable(const char* varName) {
	char* val = getenv(varName);
	if (!val)
		return {};
	return val;
}

bool Utils::SetEnvVariable(const char* varName, const char* value) {
	return setenv(varName, value, 1) == 0;
}
#endif
