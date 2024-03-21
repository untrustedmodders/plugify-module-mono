#include "glue.h"
#include "module.h"

#include <plugify/plugify_provider.h>

#include <mono/metadata/object.h>

using namespace csharplm;

#define PLUG_ADD_INTERNAL_CALL(name) mono_add_internal_call("Plugify.InternalCalls::" #name, (const void*) &(name))

static MonoString* Core_GetBaseDirectory() {
	const fs::path& baseDir = g_csharplm.GetProvider()->GetBaseDir();
	return g_csharplm.CreateString(baseDir.string());
}

static bool Core_IsPluginLoaded(MonoString* name) {
	return g_csharplm.GetProvider()->IsPluginLoaded(utils::MonoStringToUTF8(name));
}

static MonoObject* Plugin_FindPluginByName(MonoString* name) {
	ScriptOpt script = g_csharplm.FindScript(utils::MonoStringToUTF8(name));
	return script.has_value() ? script->get().GetManagedObject() : nullptr;
}

void Glue::RegisterFunctions() {
	PLUG_ADD_INTERNAL_CALL(Core_GetBaseDirectory);
	PLUG_ADD_INTERNAL_CALL(Core_IsPluginLoaded);
	PLUG_ADD_INTERNAL_CALL(Plugin_FindPluginByName);
}