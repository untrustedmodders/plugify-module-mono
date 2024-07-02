#include "utils.h"

#if MONOLM_PLATFORM_WINDOWS
#include <windows.h>
#endif

using namespace monolm;

#if MONOLM_PLATFORM_WINDOWS
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
