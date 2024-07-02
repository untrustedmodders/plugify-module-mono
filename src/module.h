#pragma once

#include <asmjit/asmjit.h>
#include <dyncall/dyncall.h>
#include <module_export.h>
#include <plugify/function.h>
#include <plugify/language_module.h>

extern "C" {
	typedef struct _MonoClass MonoClass;
	typedef struct _MonoObject MonoObject;
	typedef struct _MonoMethod MonoMethod;
	typedef struct _MonoAssembly MonoAssembly;
	typedef struct _MonoAssemblyName MonoAssemblyName;
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

namespace monolm {
	class ScriptInstance {
	public:
		ScriptInstance(const plugify::IPlugin& plugin, MonoImage* image, MonoClass* klass);
		ScriptInstance(ScriptInstance&& other) = default;
		~ScriptInstance() = default;

		const plugify::IPlugin& GetPlugin() const { return _plugin; }
		MonoObject* GetManagedObject() const { return _instance; }

	private:
		void InvokeOnStart() const;
		void InvokeOnEnd() const;

	private:
		const plugify::IPlugin& _plugin;
		MonoImage* _image{ nullptr };
		MonoClass* _klass{ nullptr };
		MonoObject* _instance{ nullptr };

		friend class CSharpLanguageModule;
	};

	std::string MonoStringToUTF8(MonoString* string);
	template<typename T>
	void MonoArrayToVector(MonoArray* array, std::vector<T>& dest);

	using ScriptMap = std::unordered_map<std::string, ScriptInstance>;
	using ArgumentList = std::vector<void*>;

	/*struct ImportMethod {
		MethodRef method;
		void* addr{ nullptr };
	};*/

	struct ExportMethod {
		MonoMethod* method{ nullptr };
		MonoObject* instance{ nullptr };
	};

	struct AssemblyInfo {
		MonoAssembly* assembly{ nullptr };
		MonoImage* image{ nullptr };
	};

	struct ClassInfo {
		MonoClass* klass{ nullptr };
		MonoMethod* ctor{ nullptr };
	};

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
		ScriptInstance* FindScript(const std::string& name);

		const std::shared_ptr<plugify::IPlugifyProvider>& GetProvider() { return _provider; }

		template<typename T>
		MonoArray* CreateArrayT(const std::vector<T>& source, MonoClass* klass);
		MonoDelegate* CreateDelegate(void* func, const plugify::Method& method);
		template<typename T>
		MonoString* CreateString(const T& source) const;
		MonoArray* CreateArray(MonoClass* klass, size_t count) const;
		template<typename T>
		MonoArray* CreateStringArray(const std::vector<T>& source) const;
		MonoObject* InstantiateClass(MonoClass* klass) const;

	private:
		bool InitMono(const fs::path& monoPath, const std::optional<fs::path>& configPath);
		void ShutdownMono();

		ScriptInstance* CreateScriptInstance(const plugify::IPlugin& plugin, MonoImage* image);

	private:
		static void HandleException(MonoObject* exc, void* userData);
		static void OnLogCallback(const char* logDomain, const char* logLevel, const char* message, mono_bool fatal, void* userData);
		static void OnPrintCallback(const char* message, mono_bool isStdout);
		static void OnPrintErrorCallback(const char* message, mono_bool isStdout);

		static void ExternalCall(const plugify::Method* method, void* addr, const plugify::Parameters* params, uint8_t count, const plugify::ReturnValue* ret);
		static void InternalCall(const plugify::Method* method, void* data, const plugify::Parameters* params, uint8_t count, const plugify::ReturnValue* ret);
		static void DelegateCall(const plugify::Method* method, void* data, const plugify::Parameters* params, uint8_t count, const plugify::ReturnValue* ret);

		static void DeleteParam(const ArgumentList& args, size_t& i, plugify::ValueType type);
		static void DeleteReturn(const ArgumentList& args, size_t& i, plugify::ValueType type);
		static void SetReturn(const plugify::Method* method, const plugify::Parameters* p, const plugify::ReturnValue* ret, MonoObject* result);
		static void SetParams(const plugify::Method* method, const plugify::Parameters* p, uint8_t count, bool hasRet, bool& hasRefs, ArgumentList& args);
		static void SetReferences(const plugify::Method* method, const plugify::Parameters* p, uint8_t count, bool hasRet, bool hasRefs, const ArgumentList& args);

		template<typename T>
		static void* MonoStructToArg(ArgumentList& args);
		template<typename T>
		static void* MonoArrayToArg(MonoArray* source, ArgumentList& args);
		static void* MonoStringToArg(MonoString* source, ArgumentList& args);
		void* MonoDelegateToArg(MonoDelegate* source, const plugify::Method& method);

		void CleanupFunctionCache();

	private:
		std::deleted_unique_ptr<MonoDomain> _rootDomain;
		std::deleted_unique_ptr<MonoDomain> _appDomain;
		std::deleted_unique_ptr<MonoReferenceQueue> _functionReferenceQueue;
		std::deleted_unique_ptr<MonoAssemblyName> _assemblyName;

		AssemblyInfo _core;
		ClassInfo _plugin;
		//ClassInfo _vector2;
		//ClassInfo _vector3;
		//ClassInfo _vector4;
		//ClassInfo _matrix4x4;

		std::shared_ptr<asmjit::JitRuntime> _rt;
		std::shared_ptr<plugify::IPlugifyProvider> _provider;
		
		std::set<std::string/*, ImportMethod*/> _importMethods;
		std::vector<std::unique_ptr<ExportMethod>> _exportMethods;
		
		std::vector<std::unique_ptr<plugify::Method>> _methods;
		std::unordered_map<void*, plugify::Function> _functions;

		std::map<uint32_t, void*> _cachedFunctions;
		std::map<void*, uint32_t> _cachedDelegates;
		
		std::deleted_unique_ptr<DCCallVM> _callVirtMachine;
		std::mutex _mutex;

		std::vector<MonoClass*> _funcClasses;
		std::vector<MonoClass*> _actionClasses;

		ScriptMap _scripts;

		struct MonoSettings {
			bool enableDebugging{ false };
			std::string level;
			std::string mask;
			std::vector<std::string> options;
		} _settings;

		friend class ScriptInstance;
	};

	extern CSharpLanguageModule g_monolm;
}

extern "C" MONOLM_EXPORT plugify::ILanguageModule* GetLanguageModule();