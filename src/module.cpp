#include "module.h"

namespace csharplm {
	CSharpLanguageModule g_charplm;
}

wizard::ILanguageModule* GetLanguageModule() {
	return &csharplm::g_charplm;
}
