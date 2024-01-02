#include "module.h"

namespace csharplm {
	CSharpLanguageModule g_csharplm;
}

wizard::ILanguageModule* GetLanguageModule() {
	return &csharplm::g_csharplm;
}
