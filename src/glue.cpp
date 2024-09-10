#include "glue.h"
#include "module.h"
#include "utils.h"

#include <plugify/plugify_provider.h>
#include <plugify/plugin.h>

#include <mono/metadata/object.h>

using namespace monolm;

#define PLUG_ADD_INTERNAL_CALL(name) mono_add_internal_call("Plugify.InternalCalls::" #name, (const void*) &(name))

MonoString* Core_GetBaseDirectory() {
	const fs::path& baseDir = g_monolm.GetProvider()->GetBaseDir();
	return g_monolm.CreateString(baseDir.string());
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
		auto str = MonoStringToUTF8(path);
		auto resource = script->GetPlugin().FindResource(static_cast<std::string_view>(str));
		if (resource.has_value()) {
			return g_monolm.CreateString(resource->string());
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
