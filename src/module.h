#include <module_export.h>
#include "script_engine.h"

namespace csharplm {
	class CSharpLanguageModule final : public ScriptEngine {
	public:
		CSharpLanguageModule() = default;
	};

	extern CSharpLanguageModule g_charplm;
}

extern "C" CSHARPLM_EXPORT wizard::ILanguageModule* GetLanguageModule();