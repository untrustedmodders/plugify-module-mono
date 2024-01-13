#pragma once

#include <wizard/language_module.h>
#include <wizard/function.h>

#include "fwd.h"

namespace csharplm {
	class ScriptEngine;

	class ScriptInstance {
	public:
		ScriptInstance(const ScriptEngine& engine, const wizard::IPlugin& plugin, MonoClass* klass);
		~ScriptInstance();

		operator bool() const { return _instance != nullptr; }
		operator MonoObject*() const { return _instance; }
		MonoObject* GetManagedObject() const { return _instance; }

	private:
		void InvokeOnStart() const;
		void InvokeOnEnd() const;

	private:
		MonoClass* _klass{ nullptr };
		MonoObject* _instance{ nullptr };
		MonoMethod* _onStartMethod{ nullptr };
		MonoMethod* _onEndMethod{ nullptr };

		friend class ScriptEngine;
	};

	using ScriptMap = std::unordered_map<std::string, ScriptInstance>;
	using ScriptOpt = std::optional<std::reference_wrapper<ScriptInstance>>;

	class ScriptEngine :  public wizard::ILanguageModule {
	public:
		ScriptEngine() = default;
		~ScriptEngine() = default;

		// ILanguageModule
		wizard::InitResult Initialize(std::weak_ptr<wizard::IWizardProvider> provider, const wizard::IModule& module) override;
		void Shutdown() override;
		wizard::LoadResult OnPluginLoad(const wizard::IPlugin& plugin) override;
		void OnPluginStart(const wizard::IPlugin& plugin) override;
		void OnPluginEnd(const wizard::IPlugin& plugin) override;
		void OnMethodExport(const wizard::IPlugin& plugin) override;

		const ScriptMap& GetScripts() const { return _scripts; }
		ScriptOpt FindScript(const std::string& name);

		MonoString* CreateString(std::string_view string) const;
		MonoArray* CreateArray(MonoClass* klass, size_t count) const;
		MonoArray* CreateStringArray(MonoClass* klass, std::span<std::string_view> source) const;
		MonoObject* InstantiateClass(MonoClass* klass) const;

	private:
		bool InitMono(const fs::path& monoPath);
		void ShutdownMono();

		ScriptOpt CreateScriptInstance(const wizard::IPlugin& plugin, MonoImage* image);

		MonoClass* CacheCoreClass(std::string name);
		MonoMethod* CacheCoreMethod(MonoClass* klass, std::string name, int params);

		MonoClass* FindCoreClass(const std::string& name) const;
		MonoMethod* FindCoreMethod(const std::string& name) const;

	private:
		static void HandleException(MonoObject* exc, void* userData);
		static void OnLogCallback(const char* logDomain, const char* logLevel, const char* message, mono_bool fatal, void* userData);
		static void OnPrintCallback(const char* message, mono_bool isStdout);
		static void OnPrintErrorCallback(const char* message, mono_bool isStdout);

		static void MethodCall(const wizard::Method* method, const wizard::Parameters* params, const uint8_t count, const wizard::ReturnValue* retVal);

	private:
		MonoDomain* _rootDomain{ nullptr };
		MonoDomain* _appDomain{ nullptr };

		MonoAssembly* _coreAssembly{ nullptr };
		MonoImage* _coreImage{ nullptr };

		std::shared_ptr<asmjit::JitRuntime> _rt;
		std::shared_ptr<wizard::IWizardProvider> _provider;
		std::unordered_map<std::string, MonoClass*> _coreClasses;
		std::unordered_map<std::string, MonoMethod*> _coreMethods;
		std::unordered_map<std::string, MonoMethod*> _exportMethods;
		std::unordered_set<std::string> _importMethods;
		std::vector<wizard::Function> _functions;

		ScriptMap _scripts;

		struct MonoConfig {
			bool enableDebugging{};
			std::string level;
			std::string mask;
			std::vector<std::string> options;
		} _config;

		friend class ScriptInstance;
	};
}