#pragma once

#include <wizard/language_module.h>
#include <wizard/function.h>
#include <mono/jit/jit.h>
#include <mono/metadata/assembly.h>
#include <mono/metadata/object.h>
#include <mono/metadata/class.h>
#include <mono/metadata/attrdefs.h>
#include <mono/metadata/mono-debug.h>
#include <mono/metadata/threads.h>

extern "C" {
    typedef struct _MonoClass MonoClass;
    typedef struct _MonoObject MonoObject;
    typedef struct _MonoMethod MonoMethod;
    typedef struct _MonoAssembly MonoAssembly;
    typedef struct _MonoImage MonoImage;
    typedef struct _MonoClassField MonoClassField;
    typedef struct _MonoString MonoString;
    typedef struct _MonoDomain MonoDomain;
}

namespace wizard {
    class IModule;
    class IPlugin;
    struct Method;
    struct Parameters;
    struct ReturnValue;
}

namespace asmjit { inline namespace _abi_1_11 {
        class JitRuntime;
    }
}

namespace csharplm {

    class ScriptEngine;

    class ScriptInstance {
    public:
        ScriptInstance() = delete;
        ScriptInstance(const ScriptEngine& engine, const wizard::IPlugin& plugin, MonoAssembly* assembly, MonoImage* image, MonoClass* klass);
        ScriptInstance(ScriptInstance&& other) noexcept = default;
        ~ScriptInstance();

        bool Invoke(const std::string& name) const;

        operator bool() const { return _instance != nullptr; }
        operator MonoObject*() const { return _instance; }
        MonoObject* GetManagedObject() const { return _instance; }

    private:
        MonoMethod* CacheMethod(std::string name, int params);
        //MonoMethod* CacheVirtualMethod(std::string name);

    private:
        MonoAssembly* _assembly{ nullptr };
        MonoImage* _image{ nullptr };
        MonoClass* _klass{ nullptr };
        MonoObject* _instance{ nullptr };
        std::unordered_map<std::string, MonoMethod*> _methods;

        friend class ScriptEngine;
    };

    using ScriptMap = std::unordered_map<std::string, ScriptInstance>;
    using ScriptRef = std::optional<std::reference_wrapper<ScriptInstance>>;

    class ScriptEngine {
    public:
        ScriptEngine() = default;
        ~ScriptEngine() = default;

        wizard::InitResult Initialize(const wizard::IModule& module);
        void Shutdown();

        const ScriptMap& GetScriptMap() const { return _scripts; }
        ScriptRef FindScript(const std::string& name);

        wizard::LoadResult LoadScript(const wizard::IPlugin& plugin);
        void StartScript(const wizard::IPlugin& plugin);
        void EndScript(const wizard::IPlugin& plugin);

        MonoString* CreateString(std::string_view string) const;
        MonoArray* CreateArray(MonoClass* klass, size_t count) const;
        MonoArray* CreateStringArray(MonoClass* klass, std::span<std::string_view> source) const;
        MonoObject* InstantiateClass(MonoClass* klass) const;

    private:
        void InitMono(const fs::path& monoPath);
        void ShutdownMono();

        ScriptRef CreateScriptInstance(const wizard::IPlugin& plugin, MonoAssembly* assembly, MonoImage* image);

        MonoClass* CacheCoreClass(std::string name);
        MonoMethod* CacheCoreMethod(MonoClass* klass, std::string name, int params);

        MonoClass* FindCoreClass(const std::string& name) const;
        MonoMethod* FindCoreMethod(const std::string& name) const;

        static void MethodCall(const wizard::Method* method, const wizard::Parameters* params, const uint8_t count, const wizard::ReturnValue* retVal);

    private:
        MonoDomain* _rootDomain{ nullptr };
        MonoDomain* _appDomain{ nullptr };

        MonoAssembly* _coreAssembly{ nullptr };
        MonoImage* _coreImage{ nullptr };

        std::shared_ptr<asmjit::JitRuntime> _rt;
        std::unordered_map<std::string, MonoClass*> _coreClasses;
        std::unordered_map<std::string, MonoMethod*> _coreMethods;
        std::unordered_map<std::string, MonoMethod*> _exportMethods;
        std::vector<wizard::Function> _functions;

        ScriptMap _scripts;

#ifdef _DEBUG
        bool _enableDebugging{ true };
#else
        bool _enableDebugging{ false };
#endif

        friend class ScriptInstance;
    };
}