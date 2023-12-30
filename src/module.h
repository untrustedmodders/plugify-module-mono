#include <wizard/plugin.h>
#include <wizard/module.h>
#include <wizard/language_module.h>
#include <module_export.h>
#include "script_engine.h"

namespace csharplm {
    class CSharpLanguageModule final : public wizard::ILanguageModule {
    public:
        CSharpLanguageModule() = default;

        void* GetNativeMethod(std::string_view method_name) {
            // TODO: find in _nativesMap
            return nullptr;
        }

        // ILanguageModule
        wizard::InitResult Initialize(std::weak_ptr<wizard::IWizardProvider> provider, const wizard::IModule& module) override {
            return  _scriptEngine.Initialize(module);
        }

        void Shutdown() override {
            _scriptEngine.Shutdown();
        }

        void OnNativeAdded(/* data */) override {
            // TODO: Add to natives map
        }

        wizard::LoadResult OnPluginLoad(const wizard::IPlugin& plugin) override {
            return _scriptEngine.LoadScript(plugin);
        }

        void OnPluginStart(const wizard::IPlugin& plugin) override {
            _scriptEngine.StartScript(plugin);
        }

        void OnPluginEnd(const wizard::IPlugin& plugin) override {
            _scriptEngine.EndScript(plugin);
        }

        ScriptEngine& GetScriptEngine() {
            return _scriptEngine;
        }

    private:
        //TODO: _nativesMap
        ScriptEngine _scriptEngine;
    };

    extern CSharpLanguageModule g_charplm;
}

extern "C" CSHARPLM_EXPORT wizard::ILanguageModule* GetLanguageModule();