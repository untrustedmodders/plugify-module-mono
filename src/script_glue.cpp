#include "script_glue.h"
#include "module.h"

#include <mono/metadata/object.h>
#include <mono/metadata/reflection.h>

using namespace csharplm;

#define WAND_ADD_INTERNAL_CALL(name) mono_add_internal_call("Wand.InternalCalls::" #name, (const void*)&(name))

static MonoObject* Plugin_FindPluginByName(MonoString* name) {
	char* nameCStr = mono_string_to_utf8(name);
	csharplm::ScriptRef script = g_csharplm.FindScript(nameCStr);
	mono_free(nameCStr);
	return script.has_value() ? script->get().GetManagedObject() : nullptr;
}

void ScriptGlue::RegisterFunctions() {
	WAND_ADD_INTERNAL_CALL(Plugin_FindPluginByName);
}