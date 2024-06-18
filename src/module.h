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

	std::string MonoStringToUTF8(MonoString* string);
	template<typename T>
	void MonoArrayToVector(MonoArray* array, std::vector<T>& dest);

	using ScriptMap = std::unordered_map<std::string, ScriptInstance>;
	using ScriptOpt = std::optional<std::reference_wrapper<ScriptInstance>>;
	using PluginRef = std::reference_wrapper<const plugify::IPlugin>;
	using MethodRef = std::reference_wrapper<const plugify::Method>;
	//using AttributeMap = std::vector<std::pair<const char*, MonoObject*>>;
	//using AssemblyMap = std::unordered_map<std::string, MonoAssembly*>;
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

	struct VMDeleter {
		void operator()(DCCallVM* vm) const {
			dcFree(vm);
		}
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
		ScriptOpt FindScript(const std::string& name);

		const std::shared_ptr<plugify::IPlugifyProvider>& GetProvider() { return _provider; }
		MonoAssemblyName* GetAssemblyName() { return _assemblyName; }

		template<typename T>
		MonoArray* CreateArrayT(const std::vector<T>& source, MonoClass* klass);
		template<typename T>
		MonoObject* CreateObject(T& source, const ClassInfo& info);
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

		ScriptOpt CreateScriptInstance(const plugify::IPlugin& plugin, MonoImage* image);

	private:
		static void HandleException(MonoObject* exc, void* userData);
		static void OnLogCallback(const char* logDomain, const char* logLevel, const char* message, mono_bool fatal, void* userData);
		static void OnPrintCallback(const char* message, mono_bool isStdout);
		static void OnPrintErrorCallback(const char* message, mono_bool isStdout);

		static void ExternalCall(const plugify::Method* method, void* addr, const plugify::Parameters* params, uint8_t count, const plugify::ReturnValue* ret);
		static void InternalCall(const plugify::Method* method, void* data, const plugify::Parameters* params, uint8_t count, const plugify::ReturnValue* ret);
		static void DelegateCall(const plugify::Method* method, void* data, const plugify::Parameters* params, uint8_t count, const plugify::ReturnValue* ret);

		static void DeleteParam(const std::vector<void*>& args, uint8_t& i, plugify::ValueType type);
		static void DeleteReturn(const std::vector<void*>& args, uint8_t& i, plugify::ValueType type);
		static void SetReturn(const plugify::Method* method, const plugify::Parameters* p, const plugify::ReturnValue* ret, MonoObject* result);
		static void SetParams(const plugify::Method* method, const plugify::Parameters* p, uint8_t count, bool hasRet, bool& hasRefs, std::vector<void*>& args);
		static void SetReferences(const plugify::Method* method, const plugify::Parameters* p, uint8_t count, bool hasRet, bool hasRefs, const std::vector<void*>& args);
		static void PullReferences(const plugify::Method* method, const plugify::Parameters* p, uint8_t count, bool hasRet, bool hasRefs, const std::vector<void*>& args);

		template<typename T>
		static void* MonoStructToArg(std::vector<void*>& args);
		template<typename T>
		static void* MonoArrayToArg(MonoArray* source, std::vector<void*>& args);
		static void* MonoStringToArg(MonoString* source, std::vector<void*>& args);
		void* MonoDelegateToArg(MonoDelegate* source, const plugify::Method& method);

		void CleanupDelegateCache();

	private:
		MonoDomain* _rootDomain{ nullptr };
		MonoDomain* _appDomain{ nullptr };
		MonoReferenceQueue* _functionReferenceQueue{ nullptr };
		MonoAssemblyName* _assemblyName{ nullptr };

		AssemblyInfo _core;

		ClassInfo _plugin;
		ClassInfo _vector2;
		ClassInfo _vector3;
		ClassInfo _vector4;
		ClassInfo _matrix4x4;

		std::shared_ptr<asmjit::JitRuntime> _rt;
		std::shared_ptr<plugify::IPlugifyProvider> _provider;
		
		std::set<std::string/*, ImportMethod*/> _importMethods;
		std::vector<std::unique_ptr<ExportMethod>> _exportMethods;
		
		std::vector<std::unique_ptr<plugify::Method>> _methods;
		std::unordered_map<void*, plugify::Function> _functions;

		std::unique_ptr<DCCallVM, VMDeleter> _callVirtMachine;
		std::map<uint32_t, void*> _cachedDelegates;
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