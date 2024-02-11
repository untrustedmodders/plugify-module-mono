#pragma once

#include <plugify/language_module.h>
#include <plugify/function.h>
#include <asmjit/asmjit.h>

extern "C" {
	typedef struct _MonoClass MonoClass;
	typedef struct _MonoObject MonoObject;
	typedef struct _MonoMethod MonoMethod;
	typedef struct _MonoAssembly MonoAssembly;
	typedef struct _MonoImage MonoImage;
	typedef struct _MonoClassField MonoClassField;
	typedef struct _MonoArray MonoArray;
	typedef struct _MonoString MonoString;
	typedef struct _MonoDomain MonoDomain;
	typedef int32_t mono_bool;
}

namespace plugify {
	class IModule;
	class IPlugin;
	struct Method;
	struct Parameters;
	struct ReturnValue;
}

namespace csharplm {
	class ScriptEngine;

	class ScriptInstance {
	public:
		ScriptInstance(const plugify::IPlugin& plugin, MonoImage* image, MonoClass* klass);
		~ScriptInstance();

		operator bool() const { return _instance != nullptr; }
		operator MonoObject*() const { return _instance; }
		MonoObject* GetManagedObject() const { return _instance; }

	private:
		void InvokeOnStart() const;
		void InvokeOnEnd() const;

	private:
		MonoImage* _image{ nullptr };
		MonoClass* _klass{ nullptr };
		MonoObject* _instance{ nullptr };
		MonoMethod* _onStartMethod{ nullptr };
		MonoMethod* _onEndMethod{ nullptr };

		friend class ScriptEngine;
	};

	using ScriptMap = std::unordered_map<std::string, ScriptInstance>;
	using ScriptOpt = std::optional<std::reference_wrapper<ScriptInstance>>;
	using PluginRef = std::reference_wrapper<const plugify::IPlugin>;
	using MethodRef = std::reference_wrapper<const plugify::Method>;
	using MethodOpt = std::optional<MethodRef>;
	//using AttributeMap = std::vector<std::pair<const char*, MonoObject*>>;

	struct ImportMethod {
		MethodRef method;
		void* addr{ nullptr };
	};
	
	struct ExportMethod {
		MonoMethod* method{ nullptr };
		MonoObject* instance{ nullptr };
	};

	class ScriptEngine :  public plugify::ILanguageModule {
	public:
		ScriptEngine() = default;
		~ScriptEngine() = default;

		// ILanguageModule
		plugify::InitResult Initialize(std::weak_ptr<plugify::IPlugifyProvider> provider, const plugify::IModule& module) override;
		void Shutdown() override;
		plugify::LoadResult OnPluginLoad(const plugify::IPlugin& plugin) override;
		void OnPluginStart(const plugify::IPlugin& plugin) override;
		void OnPluginEnd(const plugify::IPlugin& plugin) override;
		void OnMethodExport(const plugify::IPlugin& plugin) override;

		const ScriptMap& GetScripts() const { return _scripts; }
		ScriptOpt FindScript(const std::string& name);

		//template<typename T, typename C>
		//MonoArray* CreateArrayT(T* data, size_t size, C& klass) const;
		MonoString* CreateString(const char* source) const;
		MonoArray* CreateArray(MonoClass* klass, size_t count) const;
		MonoArray* CreateStringArray(const char** source, size_t size) const;
		MonoObject* InstantiateClass(MonoClass* klass) const;

	private:
		bool InitMono(const fs::path& monoPath);
		void ShutdownMono();

		ScriptOpt CreateScriptInstance(const plugify::IPlugin& plugin, MonoImage* image);
		void* ValidateMethod(const plugify::Method& method, std::vector<std::string>& methodErrors, MonoObject* monoInstance, MonoMethod* monoMethod, const char* nameSpace, const char* className, const char* methodName);
		
	private:
		static void HandleException(MonoObject* exc, void* userData);
		static void OnLogCallback(const char* logDomain, const char* logLevel, const char* message, mono_bool fatal, void* userData);
		static void OnPrintCallback(const char* message, mono_bool isStdout);
		static void OnPrintErrorCallback(const char* message, mono_bool isStdout);

		static void InternalCall(const plugify::Method* method, void* data, const plugify::Parameters* params, uint8_t count, const plugify::ReturnValue* ret);

	private:
		MonoDomain* _rootDomain{ nullptr };
		MonoDomain* _appDomain{ nullptr };

		MonoAssembly* _coreAssembly{ nullptr };
		MonoImage* _coreImage{ nullptr };

		std::shared_ptr<asmjit::JitRuntime> _rt;
		std::shared_ptr<plugify::IPlugifyProvider> _provider;
		std::unordered_map<std::string, ImportMethod> _importMethods;
		std::vector<std::unique_ptr<ExportMethod>> _exportMethods;
		std::vector<std::unique_ptr<plugify::Method>> _methods;
		std::vector<plugify::Function> _functions;

		ScriptMap _scripts;

		struct MonoConfig {
			bool enableDebugging{ false };
			bool subscribeFeature{ true };
			std::string level;
			std::string mask;
			std::vector<std::string> options;
		} _config;

		friend class ScriptInstance;
	};
}