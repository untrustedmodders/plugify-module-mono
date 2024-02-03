#include "module.h"

namespace csharplm {
	CSharpLanguageModule g_csharplm;
}

plugify::ILanguageModule* GetLanguageModule() {
	return &csharplm::g_csharplm;
}
