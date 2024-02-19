#pragma once

#include <module_export.h>
#include <plugify/language_module.h>
#include <plugify/function.h>
#include <asmjit/asmjit.h>

extern "C" {
	typedef struct _MonoClass MonoClass;
	typedef struct _MonoObject MonoObject;
	typedef struct _MonoMethod MonoMethod;
	typedef struct _MonoAssembly MonoAssembly;
	typedef struct _MonoImage MonoImage;
	typedef struct _MonoReferenceQueue MonoReferenceQueue;
	typedef struct _MonoClassField MonoClassField;
	typedef struct _MonoArray MonoArray;
	typedef struct _MonoDelegate MonoDelegate;
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
	class ScriptInstance;

	using ScriptMap = std::unordered_map<std::string, ScriptInstance>;
	using ScriptOpt = std::optional<std::reference_wrapper<ScriptInstance>>;
	using PluginRef = std::reference_wrapper<const plugify::IPlugin>;
	using MethodRef = std::reference_wrapper<const plugify::Method>;
	//using AttributeMap = std::vector<std::pair<const char*, MonoObject*>>;

	class CSharpLanguageModule final : public plugify::ILanguageModule {
	public:
		CSharpLanguageModule() = default;
		~CSharpLanguageModule() = default;

		// ILanguageModule
		plugify::InitResult Initialize(std::weak_ptr<plugify::IPlugifyProvider> provider, const plugify::IModule& module) override;
		void Shutdown() override;
		plugify::LoadResult OnPluginLoad(const plugify::IPlugin& plugin) override;
		void OnPluginStart(const plugify::IPlugin& plugin) override;
		void OnPluginEnd(const plugify::IPlugin& plugin) override;
		void OnMethodExport(const plugify::IPlugin& plugin) override;

		const ScriptMap& GetScripts() const { return _scripts; }
		ScriptOpt FindScript(const std::string& name);

		template<typename T, typename C>
		static MonoArray* CreateArrayT(const std::vector<T>& source, C& klass);
		MonoDelegate* CreateDelegate(void* func, const plugify::Method& method);
		MonoString* CreateString(const std::string& source) const;
		MonoArray* CreateArray(MonoClass* klass, size_t count) const;
		MonoArray* CreateStringArray(const std::vector<std::string>& source) const;
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

		static void ExternalCall(const plugify::Method* method, void* addr, const plugify::Parameters* params, uint8_t count, const plugify::ReturnValue* ret);
		static void InternalCall(const plugify::Method* method, void* data, const plugify::Parameters* params, uint8_t count, const plugify::ReturnValue* ret);
		static void DelegateCall(const plugify::Method* method, void* data, const plugify::Parameters* params, uint8_t count, const plugify::ReturnValue* ret);

		template<typename T>
		static void* MonoArrayToArg(MonoArray* source, std::vector<void*>& args);
		static void* MonoStringToArg(MonoString* source, std::vector<void*>& args);
		void* MonoDelegateToArg(MonoDelegate* source, const plugify::Method& method) const;

	private:
		MonoDomain* _rootDomain{ nullptr };
		MonoDomain* _appDomain{ nullptr };

		MonoAssembly* _coreAssembly{ nullptr };
		MonoImage* _coreImage{ nullptr };

		MonoReferenceQueue* _functionReferenceQueue{ nullptr };

		struct ImportMethod {
			MethodRef method;
			void* addr{ nullptr };
		};

		struct ExportMethod {
			MonoMethod* method{ nullptr };
			MonoObject* instance{ nullptr };
		};

		std::shared_ptr<asmjit::JitRuntime> _rt;
		std::shared_ptr<plugify::IPlugifyProvider> _provider;
		std::unordered_map<std::string, ImportMethod> _importMethods;
		std::vector<std::unique_ptr<ExportMethod>> _exportMethods;
		std::vector<std::unique_ptr<plugify::Method>> _methods;
		std::unordered_map<void*, plugify::Function> _functions;

		std::vector<MonoClass*> _funcClasses;
		std::vector<MonoClass*> _actionClasses;

		const size_t kDelegateCount = 17;

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

		friend class CSharpLanguageModule;
	};

	extern CSharpLanguageModule g_csharplm;
}

extern "C" CSHARPLM_EXPORT plugify::ILanguageModule* GetLanguageModule();