#include "glue.h"
#include "module.h"

#include <plugify/plugify_provider.h>

#include <mono/metadata/object.h>

using namespace monolm;

#define PLUG_ADD_INTERNAL_CALL(name) mono_add_internal_call("Plugify.InternalCalls::" #name, (const void*) &(name))

static MonoString* Core_GetBaseDirectory() {
	const fs::path& baseDir = g_monolm.GetProvider()->GetBaseDir();
	return g_monolm.CreateString(baseDir.string());
}

static bool Core_IsModuleLoaded(MonoString* name, int version, bool minimum) {
	auto requiredVersion = (version >= 0 && version != INT_MAX) ? std::make_optional(version) : std::nullopt;
	return g_monolm.GetProvider()->IsModuleLoaded(MonoStringToUTF8(name), requiredVersion, minimum);
}

static bool Core_IsPluginLoaded(MonoString* name, int version, bool minimum) {
	auto requiredVersion = (version >= 0 && version != INT_MAX) ? std::make_optional(version) : std::nullopt;
	return g_monolm.GetProvider()->IsPluginLoaded(MonoStringToUTF8(name), requiredVersion, minimum);
}

static MonoObject* Plugin_FindPluginByName(MonoString* name) {
	ScriptOpt script = g_monolm.FindScript(MonoStringToUTF8(name));
	return script.has_value() ? script->get().GetManagedObject() : nullptr;
}

void Glue::RegisterFunctions() {
	PLUG_ADD_INTERNAL_CALL(Core_GetBaseDirectory);
	PLUG_ADD_INTERNAL_CALL(Core_IsModuleLoaded);
	PLUG_ADD_INTERNAL_CALL(Core_IsPluginLoaded);
	PLUG_ADD_INTERNAL_CALL(Plugin_FindPluginByName);
}