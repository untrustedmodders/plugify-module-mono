#include <wizard/plugin.h>
#include <wizard/module.h>
#include <wizard/language_module.h>
#include "module_export.h"
#include "script_engine.h"

using namespace wizard;

namespace csharplm {
	class CSharpLanguageModule final : public ILanguageModule {
	public:
        CSharpLanguageModule() = default;

		void* GetNativeMethod(std::string_view method_name) {
			// TODO: find in _nativesMap
			return nullptr;
		}

		// ILanguageModule
		bool Initialize(const IModule& module) override {
			return _scriptEngine.Initialize(module);
		}

		void Shutdown() override {
            _scriptEngine.Shutdown();
		}

		void OnNativeAdded(/* data */) override {
			// TODO: Add to natives map
		}

		LoadResult OnPluginLoad(const IPlugin& plugin) override {
            _scriptEngine.LoadScript(plugin);
			return {};
		}

		void OnPluginStart(const IPlugin& plugin) override {
            _scriptEngine.StartScript(plugin);
		}

		void OnPluginEnd(const IPlugin& plugin) override {
            _scriptEngine.EndScript(plugin);
		}

	private:
		//TODO: _nativesMap
        ScriptEngine _scriptEngine;
	};

    CSharpLanguageModule g_charplm;

    CSHARPLM_EXPORT void* GetNativeMethod(std::string_view method_name) {
		return g_charplm.GetNativeMethod(method_name);
	}
}

extern "C" CSHARPLM_EXPORT ILanguageModule* GetLanguageModule() {
	return &csharplm::g_charplm;
}
