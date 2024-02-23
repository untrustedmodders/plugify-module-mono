#include "script_glue.h"
#include "module.h"

#include <mono/metadata/object.h>
#include <mono/metadata/reflection.h>

using namespace csharplm;

#define PLUG_ADD_INTERNAL_CALL(name) mono_add_internal_call("Plugify.InternalCalls::" #name, (const void*) &(name))

static MonoObject* Plugin_FindPluginByName(MonoString* name) {
	csharplm::ScriptOpt script = g_csharplm.FindScript(name);
	return script.has_value() ? script->get().GetManagedObject() : nullptr;
}

void Glue::RegisterFunctions() {
	PLUG_ADD_INTERNAL_CALL(Plugin_FindPluginByName);
}