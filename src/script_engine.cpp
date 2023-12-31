#include "script_engine.h"
#include "script_glue.h"
#include "module.h"

#include <wizard/module.h>
#include <wizard/plugin.h>
#include <wizard/plugin_descriptor.h>

//#include <iostream>
#include <cassert>
#include <fstream>
#include <cstring>

using namespace csharplm;

namespace csharplm::utils {
    template<typename T>
    inline bool ReadBytes(const fs::path& file, const std::function<void(std::span<T>)>& callback) {
        std::ifstream istream{file, std::ios::binary};
        if (!istream)
            return false;
        std::vector<T> buffer{ std::istreambuf_iterator<char>(istream), std::istreambuf_iterator<char>() };
        callback({ buffer.data(), buffer.size() });
        return true;
    }

    MonoAssembly* LoadMonoAssembly(const fs::path& assemblyPath, bool loadPDB, MonoImageOpenStatus& status) {
        MonoImage* image = nullptr;

        ReadBytes<char>(assemblyPath, [&image, &status](std::span<char> buffer) {
            image = mono_image_open_from_data_full(buffer.data(), static_cast<uint32_t>(buffer.size()), 1, &status, 0);
        });

        if (status != MONO_IMAGE_OK) {
            return nullptr;
        }

        if (loadPDB) {
            fs::path pdbPath{ assemblyPath };
            pdbPath.replace_extension(".pdb");

            ReadBytes<mono_byte>(pdbPath, [&image](std::span<mono_byte> buffer) {
                mono_debug_open_image_from_memory(image, buffer.data(), static_cast<int>(buffer.size()));
            });

            // If pdf not load ?
        }

        MonoAssembly* assembly = mono_assembly_load_from_full(image, assemblyPath.string().c_str(), &status, 0);
        mono_image_close(image);
        return assembly;
    }

    [[maybe_unused]] void PrintAssemblyTypes(MonoAssembly* assembly, const std::function<void(std::string)>& out) {
        MonoImage* image = mono_assembly_get_image(assembly);
        const MonoTableInfo* typeDefinitionsTable = mono_image_get_table_info(image, MONO_TABLE_TYPEDEF);
        int numTypes = mono_table_info_get_rows(typeDefinitionsTable);

        for (int i = 0; i < numTypes; ++i) {
            uint32_t cols[MONO_TYPEDEF_SIZE];
            mono_metadata_decode_row(typeDefinitionsTable, i, cols, MONO_TYPEDEF_SIZE);

            const char* nameSpace = mono_metadata_string_heap(image, cols[MONO_TYPEDEF_NAMESPACE]);
            const char* name = mono_metadata_string_heap(image, cols[MONO_TYPEDEF_NAME]);

            out(std::format("{}.{}", nameSpace, ".", + name));
        }
    }

    std::string MonoStringToString(MonoString* string) {
        char* cStr = mono_string_to_utf8(string);
        std::string str{cStr};
        mono_free(cStr);
        return str;
    }

    wizard::ValueType MonoTypeToValueType(const char* typeName) {
        using enum wizard::ValueType;
        static std::unordered_map<std::string_view, wizard::ValueType> valueTypeMap = {
            { "System.Void", Void },
            { "System.Boolean", Bool },
            { "System.Char", Char8 },
            { "System.SByte", Int8 },
            { "System.Int16", Int16 },
            { "System.Int32", Int32 },
            { "System.Int64", Int64 },
            { "System.Byte", Uint8 },
            { "System.UInt16", Uint16 },
            { "System.UInt32", Uint32 },
            { "System.UInt64", Uint64 },
            { "System.IntPtr", Ptr64 },
            { "System.UIntPtr", Ptr64 },
            { "System.Single", Float },
            { "System.Double", Double },
            { "System.String", String },
        };
        auto it = valueTypeMap.find(typeName);
        if (it != valueTypeMap.end())
            return it->second;
        return Invalid;
    }

    std::vector<std::string_view> Split(std::string_view strv, std::string_view delims = " ") {
        std::vector<std::string_view> output;
        size_t first = 0;

        while (first < strv.size()) {
            const size_t second = strv.find_first_of(delims, first);

            if (first != second)
                output.emplace_back(strv.substr(first, second-first));

            if (second == std::string_view::npos)
                break;

            first = second + 1;
        }

        return output;
    }
}

wizard::InitResult ScriptEngine::Initialize(const wizard::IModule& mod) {
    InitMono(mod.GetBaseDir() / "mono/lib");

    ScriptGlue::RegisterFunctions();

    _rt = std::make_shared<asmjit::JitRuntime>();

    // Create an app domain
    char appName[] = "WandMonoRuntime";
    _appDomain = mono_domain_create_appdomain(appName, nullptr);
    mono_domain_set(_appDomain, true);

    fs::path coreAssemblyPath{ mod.GetBinariesDir() / "Wand.dll" };

    // Load a core assembly
    MonoImageOpenStatus status = MONO_IMAGE_IMAGE_INVALID;
    _coreAssembly = utils::LoadMonoAssembly(coreAssemblyPath, _enableDebugging, status);
    if (!_coreAssembly)
        return wizard::ErrorData{std::format("Could not load '{}' core assembly. Reason: {}", coreAssemblyPath.string(), mono_image_strerror(status))};

    _coreImage = mono_assembly_get_image(_coreAssembly);
    if (!_coreImage)
        return wizard::ErrorData{std::format("Could not load '{}' core image.", coreAssemblyPath.string())};

    // Retrieve and cache core classes/methods

    /// Plugin
    MonoClass* pluginClass = CacheCoreClass("Plugin");
    if (!pluginClass)
        return wizard::ErrorData{std::format("Could not find 'Plugin' core class! Check '{}' assembly!", coreAssemblyPath.string())};

    MonoMethod* pluginCtor = CacheCoreMethod(pluginClass, ".ctor", 8);
    if (!pluginCtor)
        return wizard::ErrorData{std::format("Could not find 'Plugin' .ctor method! Check '{}' assembly!", coreAssemblyPath.string())};

    return {};
}

void ScriptEngine::Shutdown() {
    _scripts.clear();

    ShutdownMono();
}

void ScriptEngine::InitMono(const fs::path& monoPath) {
    mono_set_assemblies_path(monoPath.string().c_str());

    if (_enableDebugging) {
        const char* argv[2] = {
            "--debugger-agent=transport=dt_socket,address=127.0.0.1:2550,server=y,suspend=n,loglevel=3,logfile=MonoDebugger.log",
            "--soft-breakpoints"
        };

        mono_jit_parse_options(2, (char**)argv);
        mono_debug_init(MONO_DEBUG_FORMAT_MONO);
    }

    _rootDomain = mono_jit_init("WandJITRuntime");
    assert(_rootDomain);

    if (_enableDebugging)
        mono_debug_domain_create(_rootDomain);

    mono_thread_set_main(mono_thread_current());
}

void ScriptEngine::ShutdownMono() {
    mono_domain_set(mono_get_root_domain(), false);

    if (_appDomain) {
        mono_domain_unload(_appDomain);
        _appDomain = nullptr;
    }

    if (_rootDomain) {
        mono_jit_cleanup(_rootDomain);
        _rootDomain = nullptr;
    }
}

ScriptRef ScriptEngine::FindScript(const std::string& name) {
    auto it = _scripts.find(name);
    if (it != _scripts.end())
        return it->second;
    return std::nullopt;
}

MonoClass* ScriptEngine::CacheCoreClass(std::string name) {
    assert(!_coreMethods.contains(name));
    MonoClass* klass = mono_class_from_name(_coreImage, "Wand", name.c_str());
    assert(klass);
    _coreClasses.emplace(std::move(name), klass);
    return klass;
}

MonoMethod* ScriptEngine::CacheCoreMethod(MonoClass* klass, std::string name, int params) {
    assert(!_coreMethods.contains(name));
    MonoMethod* method = mono_class_get_method_from_name(klass, name.c_str(), params);
    assert(method);
    _coreMethods.emplace(std::move(name), method);
    return method;
}

MonoClass* ScriptEngine::FindCoreClass(const std::string& name) const{
    auto it = _coreClasses.find(name);
    return it != _coreClasses.end() ? it->second : nullptr;
}

MonoMethod* ScriptEngine::FindCoreMethod(const std::string& name) const {
    auto it = _coreMethods.find(name);
    return it != _coreMethods.end() ? it->second : nullptr;
}

MonoString* ScriptEngine::CreateString(std::string_view string) const {
    return mono_string_new(_appDomain, string.data());
}

MonoArray* ScriptEngine::CreateArray(MonoClass* klass, size_t count) const {
    return mono_array_new(_appDomain, klass, count);
}

MonoArray* ScriptEngine::CreateStringArray(MonoClass *klass, std::span<std::string_view> source) const {
    MonoArray* array = CreateArray(klass, source.size());
    for (size_t i = 0; i < source.size(); ++i) {
        mono_array_set(array, MonoString*, i, CreateString(source[i]));
    }
    return array;
}

MonoObject* ScriptEngine::InstantiateClass(MonoClass* klass) const {
    MonoObject* instance = mono_object_new(_appDomain, klass);
    mono_runtime_object_init(instance);
    return instance;
}

void ScriptEngine::MethodCall(const wizard::Method* method, const wizard::Parameters* p, const uint8_t count, const wizard::ReturnValue* ret) {
    const auto& exportMethods = g_charplm.GetScriptEngine()._exportMethods;
    auto it = exportMethods.find(method->funcName);
    if (it == exportMethods.end()) {
        printf("Method not found!\n");
        return;
    }

    /// We not create param vector, and use Parameters* params directly if passing primitives
    std::vector<void*> args;
    args.reserve(count);

    for (int i = 0; i < count; i++) {
        using enum wizard::ValueType;
        switch (method->paramTypes[i]) {
            case Invalid:
            case Void:
                // Should not trigger!
                break;
            case Bool:
            case Char8:
            case Char16:
            case Int8:
            case Int16:
            case Int32:
            case Int64:
            case Uint8:
            case Uint16:
            case Uint32:
            case Uint64:
            case Ptr64:
            case Float:
            case Double:
                args.push_back(p->GetArgumentPtr(i));
                break;
            case String: {
                auto str = p->GetArgument<std::string*>(i);
                args.push_back(str != nullptr ? g_charplm.GetScriptEngine().CreateString(*str) : nullptr);
                break;
            }
        }
    }

    MonoObject* exception = nullptr;
    MonoObject* result = mono_runtime_invoke(it->second, nullptr, args.data(), &exception);
    if (exception) {
        mono_print_unhandled_exception(exception);
        return;
    }

    using enum wizard::ValueType;
    switch (method->retType) {
        case Invalid:
        case Void:
            break;
        case Bool: {
            bool val = *(bool*) mono_object_unbox(result);
            ret->SetReturnPtr<bool>(val);
            break;
        }
        case Char8: {
            char val = *(char*) mono_object_unbox(result);
            ret->SetReturnPtr<char>(val);
            break;
        }
        case Char16: {
            wchar_t val = *(wchar_t*) mono_object_unbox(result);
            ret->SetReturnPtr<wchar_t>(val);
            break;
        }
        case Int8: {
            int8_t val = *(int8_t*) mono_object_unbox(result);
            ret->SetReturnPtr<int8_t>(val);
            break;
        }
        case Int16: {
            int16_t val = *(int16_t*) mono_object_unbox(result);
            ret->SetReturnPtr<int16_t>(val);
            break;
        }
        case Int32: {
            int32_t val = *(int32_t*) mono_object_unbox(result);
            ret->SetReturnPtr<int32_t>(val);
            break;
        }
        case Int64: {
            int64_t val = *(int64_t*) mono_object_unbox(result);
            ret->SetReturnPtr<int64_t>(val);
            break;
        }
        case Uint8: {
            uint8_t val = *(uint8_t*) mono_object_unbox(result);
            ret->SetReturnPtr<uint8_t>(val);
            break;
        }
        case Uint16: {
            uint16_t val = *(uint16_t*) mono_object_unbox(result);
            ret->SetReturnPtr<uint16_t>(val);
            break;
        }
        case Uint32: {
            uint32_t val = *(uint32_t*) mono_object_unbox(result);
            ret->SetReturnPtr<uint32_t>(val);
            break;
        }
        case Uint64: {
            uint64_t val = *(uint64_t*) mono_object_unbox(result);
            ret->SetReturnPtr<uint64_t>(val);
            break;
        }
        case Ptr64: {
            uintptr_t val = *(uintptr_t*) mono_object_unbox(result);
            ret->SetReturnPtr<uintptr_t>(val);
            break;
        }
        case Float: {
            float val = *(float*) mono_object_unbox(result);
            ret->SetReturnPtr<float>(val);
            break;
        }
        case Double: {
            double val = *(double*) mono_object_unbox(result);
            ret->SetReturnPtr<double>(val);
            break;
        }
        case String: {
            // TODO: How return string ?
            break;
        }
    }
}

wizard::LoadResult ScriptEngine::LoadScript(const wizard::IPlugin& plugin) {
    MonoImageOpenStatus status = MONO_IMAGE_IMAGE_INVALID;
    MonoAssembly* assembly = utils::LoadMonoAssembly(plugin.GetFilePath(), _enableDebugging, status);
    if (!assembly)
        return wizard::ErrorData{std::format("Could not load '{}' plugin assembly. Reason: {}", plugin.GetFilePath().string(), mono_image_strerror(status))};

    MonoImage* image = mono_assembly_get_image(assembly);
    if (!image)
        return wizard::ErrorData{std::format("Could not load '{}' plugin image.", plugin.GetFilePath().string())};

    auto script = CreateScriptInstance(plugin, assembly, image);
    if (!script.has_value())
        return wizard::ErrorData{std::format("Could not find 'Plugin' class implementation! Check '{}' assembly!", plugin.GetFilePath().string())};

    auto& exportedMethods = plugin.GetDescriptor().exportedMethods;
    wizard::LoadResultData loadResult;
    loadResult.methods.reserve(exportedMethods.size());

    asmjit::JitRuntime rt;

    for (const auto& method : exportedMethods) {
        auto seperated = utils::Split(method.funcName, ".");
        if (seperated.size() != 4)
            return wizard::ErrorData{"Error TODO: !"};

        // Name of function in format "Plugin.Namespace.Class.Method"
        std::string nameSpace{ seperated[1] };
        std::string className{ seperated[2] };
        std::string methodName{ seperated[3] };

        MonoClass* monoClass = mono_class_from_name(image, nameSpace.c_str(), className.c_str());
        if (!monoClass)
            return wizard::ErrorData{"Error TODO: !"};

        int paramCount = static_cast<int>(method.paramTypes.size());
        MonoMethod* monoMethod = mono_class_get_method_from_name(monoClass, methodName.c_str(), paramCount);
        if (!monoMethod)
            return wizard::ErrorData{"Error TODO: !"};

        MonoMethodSignature* sig = mono_method_signature(monoMethod);

        char* returnType = mono_type_get_name(mono_signature_get_return_type(sig));
        if (utils::MonoTypeToValueType(returnType) != method.retType) {
            continue;
        }

        size_t i = 0;
        void* iter = nullptr;
        while (MonoType* type = mono_signature_get_params(sig, &iter)) {
            char* paramType = mono_type_get_name(type);
            if (utils::MonoTypeToValueType(paramType) != method.paramTypes[i++]) {
                continue;
            }
        }

        _exportMethods.emplace(method.funcName, monoMethod);

        auto jit = _functions.emplace_back(_rt).GetJitFunc(method, MethodCall);

        loadResult.methods.emplace_back(method.name, jit);
    }

    return loadResult;
}

void ScriptEngine::MethodExport(const wizard::IPlugin& plugin) {
    for (const auto& [name, addr] : plugin.GetMethods()) {
        std::string fullName{ std::format("{}.{}::{}", plugin.GetName(), plugin.GetName(), name) };

        if (_importMethods.contains(fullName)) {
            // LOG error!
            continue;
        }

        mono_add_internal_call(fullName.c_str(), addr);

        _importMethods.emplace(std::move(fullName));
    }
}

void ScriptEngine::StartScript(const wizard::IPlugin& plugin) {
    ScriptRef script = FindScript(plugin.GetName());
    if (script.has_value()) {
        script->get().Invoke("OnStart");
    }
}

void ScriptEngine::EndScript(const wizard::IPlugin& plugin) {
    ScriptRef script = FindScript(plugin.GetName());
    if (script.has_value()) {
        script->get().Invoke("OnEnd");
    }
}

ScriptRef ScriptEngine::CreateScriptInstance(const wizard::IPlugin& plugin, MonoAssembly* assembly, MonoImage* image) {
    MonoClass* pluginClass = FindCoreClass("Plugin");

    const MonoTableInfo* typeDefinitionsTable = mono_image_get_table_info(image, MONO_TABLE_TYPEDEF);
    int numTypes = mono_table_info_get_rows(typeDefinitionsTable);

    for (int i = 0; i < numTypes; ++i) {
        uint32_t cols[MONO_TYPEDEF_SIZE];
        mono_metadata_decode_row(typeDefinitionsTable, i, cols, MONO_TYPEDEF_SIZE);

        const char* nameSpace = mono_metadata_string_heap(image, cols[MONO_TYPEDEF_NAMESPACE]);
        const char* className = mono_metadata_string_heap(image, cols[MONO_TYPEDEF_NAME]);

        MonoClass* monoClass = mono_class_from_name(image, nameSpace, className);
        if (monoClass == pluginClass)
            continue;

        bool isPlugin = mono_class_is_subclass_of(monoClass, pluginClass, false);
        if (!isPlugin)
            continue;

        return _scripts.emplace(plugin.GetName(), ScriptInstance{ *this, plugin, assembly, image, monoClass }).first->second;
    }

    return std::nullopt;
}

/*_________________________________________________*/

ScriptInstance::ScriptInstance(const ScriptEngine& engine, const wizard::IPlugin& plugin, MonoAssembly* assembly, MonoImage* image, MonoClass* klass)
    : _assembly{assembly}, _image{image}, _klass{klass} {

    _instance = engine.InstantiateClass(_klass);

    // Call Script (base) constructor
    {
        MonoMethod* constructor = engine.FindCoreMethod(".ctor");
        assert(constructor);
        const auto& desc = plugin.GetDescriptor();
        auto id = plugin.GetId();
        std::vector<std::string_view> deps;
        deps.reserve(desc.dependencies.size());
        for (const auto& dependency : desc.dependencies) {
            deps.emplace_back(dependency.name);
        }
        std::array<void*, 8> args {
            &id,
            engine.CreateString(plugin.GetName()),
            engine.CreateString(plugin.GetFriendlyName()),
            engine.CreateString(desc.friendlyName),
            engine.CreateString(desc.versionName),
            engine.CreateString(desc.createdBy),
            engine.CreateString(desc.createdByURL),
            engine.CreateStringArray(mono_get_string_class(), deps),
        };
        mono_runtime_invoke(constructor, _instance, args.data(), nullptr);
    }

    CacheMethod("OnCreate", 0);
    CacheMethod("OnStart", 0);
    CacheMethod("OnEnd", 0);
    CacheMethod("OnDestroy", 0);

    Invoke("OnCreate");
}

ScriptInstance::~ScriptInstance() {
    Invoke("OnDestroy");
}

bool ScriptInstance::Invoke(const std::string& name) const {
    if (auto it = _methods.find(name); it != _methods.end()) {
        MonoObject* exception = nullptr;
        mono_runtime_invoke(it->second, _instance, nullptr, &exception);
        if (exception) {
            mono_print_unhandled_exception(exception);
            return false;
        }
    }
    return true;
}

MonoMethod* ScriptInstance::CacheMethod(std::string name, int params) {
    assert(!_methods.contains(name));
    MonoMethod* method = mono_class_get_method_from_name(_klass, name.c_str(), params);
    if (method)
        _methods.emplace(std::move(name), method);
    return method;
}

/*MonoMethod* ScriptInstance::CacheVirtualMethod(std::string name) {
    assert(!_methods.contains(name));
    MonoMethod* method = _engine.FindCoreMethod(name);
    assert(method);
    MonoMethod* vmethod = mono_object_get_virtual_method(_instance, method);
    assert(vmethod);
    _methods.emplace(std::move(name), vmethod);
    return vmethod;
}*/