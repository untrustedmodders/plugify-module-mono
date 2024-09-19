#include "glue.h"
#include "module.h"
#include "utils.h"

#include <plugify/plugify_provider.h>
#include <plugify/plugin.h>

#include <mono/metadata/object.h>

using namespace monolm;

#define PLUG_ADD_INTERNAL_CALL(name) mono_add_internal_call("Plugify.InternalCalls::" #name, (const void*) &(name))

MonoString* Core_GetBaseDirectory() {
	fs::path_view baseDir = g_monolm.GetProvider()->GetBaseDir();
	return g_monolm.CreateString(baseDir);
}

bool Core_IsModuleLoaded(MonoString* name, int32_t version, bool minimum) {
	auto requiredVersion = (version >= 0 && version != INT_MAX) ? std::make_optional(version) : std::nullopt;
	return g_monolm.GetProvider()->IsModuleLoaded(MonoStringToUTF8(name), requiredVersion, minimum);
}

bool Core_IsPluginLoaded(MonoString* name, int32_t version, bool minimum) {
	auto requiredVersion = (version >= 0 && version != INT_MAX) ? std::make_optional(version) : std::nullopt;
	return g_monolm.GetProvider()->IsPluginLoaded(MonoStringToUTF8(name), requiredVersion, minimum);
}

MonoString* Plugin_FindResource(int64_t id, MonoString* path) {
	ScriptInstance* script = g_monolm.FindScript(id);
	if (script) {
#if MONOLM_PLATFORM_WINDOWS
		auto str = MonoStringToUTF16(path);
#elif
		auto str = MonoStringToUTF8(path);
#endif
		auto resource = script->GetPlugin().FindResource(str);
		if (resource.has_value()) {
			return g_monolm.CreateString(*resource);
		}
	}
	return nullptr;
}

void Glue::RegisterFunctions() {
	PLUG_ADD_INTERNAL_CALL(Core_GetBaseDirectory);
	PLUG_ADD_INTERNAL_CALL(Core_IsModuleLoaded);
	PLUG_ADD_INTERNAL_CALL(Core_IsPluginLoaded);
	PLUG_ADD_INTERNAL_CALL(Plugin_FindResource);
}
