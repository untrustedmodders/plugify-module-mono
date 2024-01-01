#include "script_engine.h"
#include "script_glue.h"
#include "module.h"

#include <mono/jit/jit.h>
#include <mono/metadata/assembly.h>
#include <mono/metadata/object.h>
#include <mono/metadata/class.h>
#include <mono/metadata/attrdefs.h>
#include <mono/metadata/mono-debug.h>
#include <mono/metadata/threads.h>

#include <wizard/module.h>
#include <wizard/plugin.h>
#include <wizard/plugin_descriptor.h>
#include <wizard/wizard_provider.h>

#include <fstream>

using namespace csharplm;
using namespace wizard;

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

    ValueType MonoTypeToValueType(const char* typeName) {
        using enum ValueType;
        static std::unordered_map<std::string_view, ValueType> valueTypeMap = {
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
            return std::get<ValueType>(*it);
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

InitResult ScriptEngine::Initialize(std::weak_ptr<IWizardProvider> provider, const IModule& m) {
    if (!(_provider = provider.lock()))
        return ErrorData{ "Provider not exposed" };

    InitMono(m.GetBaseDir() / "mono/lib");

    ScriptGlue::RegisterFunctions();

    _rt = std::make_shared<asmjit::JitRuntime>();

    // Create an app domain
    char appName[] = "WandMonoRuntime";
    _appDomain = mono_domain_create_appdomain(appName, nullptr);
    mono_domain_set(_appDomain, true);

    fs::path coreAssemblyPath{ m.GetBinariesDir() / "Wand.dll" };

    // Load a core assembly
    MonoImageOpenStatus status = MONO_IMAGE_IMAGE_INVALID;
    _coreAssembly = utils::LoadMonoAssembly(coreAssemblyPath, _enableDebugging, status);
    if (!_coreAssembly)
        return ErrorData{std::format("Failed to load '{}' core assembly. Reason: {}", coreAssemblyPath.string(), mono_image_strerror(status))};

    _coreImage = mono_assembly_get_image(_coreAssembly);
    if (!_coreImage)
        return ErrorData{std::format("Failed to load '{}' core image.", coreAssemblyPath.string())};

    // Retrieve and cache core classes/methods

    /// Plugin
    MonoClass* pluginClass = CacheCoreClass("Plugin");
    if (!pluginClass)
        return ErrorData{std::format("Failed to find 'Plugin' core class! Check '{}' assembly!", coreAssemblyPath.string())};

    MonoMethod* pluginCtor = CacheCoreMethod(pluginClass, ".ctor", 8);
    if (!pluginCtor)
        return ErrorData{std::format("Failed to find 'Plugin' .ctor method! Check '{}' assembly!", coreAssemblyPath.string())};

    /// IPluginListener
    /*{
        MonoClass* serverListener = CacheCoreClass("IPluginListener");
        CacheCoreMethod(serverListener, "OnPluginCreate", 0);
        CacheCoreMethod(serverListener, "OnPluginStart", 0);
        CacheCoreMethod(serverListener, "OnPluginEnd", 0);
        CacheCoreMethod(serverListener, "OnPluginDestroy", 0);
    }*/

    _provider->Log("[CSHARPLM] Inited!", Severity::Debug);

    return InitResultData{};
}

void ScriptEngine::Shutdown() {
    _coreClasses.clear();
    _coreMethods.clear();
    _exportMethods.clear();
    _importMethods.clear();
    _functions.clear();
    _scripts.clear();
    _provider.reset();
    _rt.reset();

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
    if (!_rootDomain) {
        _provider->Log("Initialization of mono failed", Severity::Error);
        return;
    }

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

    _coreAssembly = nullptr;
    _coreImage = nullptr;
}

void ScriptEngine::MethodCall(const Method* method, const Parameters* p, const uint8_t count, const ReturnValue* ret) {
    const auto& exportMethods = g_charplm._exportMethods;
    auto it = exportMethods.find(method->funcName);
    if (it == exportMethods.end()) {
        printf("Method not found!\n");
        return;
    }

    /// We not create param vector, and use Parameters* params directly if passing primitives
    std::vector<void*> args;
    args.reserve(count);

    for (uint8_t i = 0; i < count; ++i) {
        using enum ValueType;
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
                args.push_back(str != nullptr ? g_charplm.CreateString(*str) : nullptr);
                break;
            }
        }
    }

    MonoObject* exception = nullptr;
    MonoObject* result = mono_runtime_invoke(std::get<MonoMethod*>(*it), nullptr, args.data(), &exception);
    if (exception) {
        mono_print_unhandled_exception(exception);
        return;
    }

    using enum ValueType;
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

LoadResult ScriptEngine::OnPluginLoad(const IPlugin& plugin) {
    MonoImageOpenStatus status = MONO_IMAGE_IMAGE_INVALID;
    MonoAssembly* assembly = utils::LoadMonoAssembly(plugin.GetFilePath(), _enableDebugging, status);
    if (!assembly)
        return ErrorData{std::format("Failed to load assembly: '{}'",mono_image_strerror(status))};

    MonoImage* image = mono_assembly_get_image(assembly);
    if (!image)
        return ErrorData{std::format("Failed to load assembly image")};

    auto script = CreateScriptInstance(plugin, image);
    if (!script.has_value())
        return ErrorData{std::format("Failed to find 'Plugin' class implementation")};

    const auto& exportedMethods = plugin.GetDescriptor().exportedMethods;
    std::vector<MethodData> methods;
    methods.reserve(exportedMethods.size());

    std::vector<std::string> methodErrors;

    for (const auto& method : exportedMethods) {
        auto seperated = utils::Split(method.funcName, ".");
        if (seperated.size() != 4) {
            methodErrors.emplace_back(std::format("Invalid function name: '{}'. Please provide name in that format: 'Plugin.Namespace.Class.Method'", method.funcName));
            continue;
        }

        std::string nameSpace{ seperated[1] };
        std::string className{ seperated[2] };
        std::string methodName{ seperated[3] };

        MonoClass* monoClass = mono_class_from_name(image, nameSpace.c_str(), className.c_str());
        if (!monoClass) {
            methodErrors.emplace_back(std::format("Failed to find class '{}.{}'", nameSpace, className));
            continue;
        }

        MonoMethod* monoMethod = mono_class_get_method_from_name(monoClass, methodName.c_str(), -1);
        if (!monoMethod) {
            methodErrors.emplace_back(std::format("Failed to find method '{}.{}::{}'", nameSpace, className, methodName));
            continue;
        }

        uint32_t methodFlags = mono_method_get_flags(monoMethod, nullptr);
        if (!(methodFlags & MONO_METHOD_ATTR_STATIC)) {
            methodErrors.emplace_back(std::format("Method '{}.{}::{}' is not static", nameSpace, className, methodName));
            continue;
        }

        MonoMethodSignature* sig = mono_method_signature(monoMethod);

        uint32_t paramCount = mono_signature_get_param_count(sig);
        if (paramCount != method.paramTypes.size()) {
            methodErrors.emplace_back(std::format("Invalid parameter count {} when it should have {}", method.paramTypes.size(), paramCount));
            continue;
        }

        char* returnTypeName = mono_type_get_name(mono_signature_get_return_type(sig));
        ValueType returnType = utils::MonoTypeToValueType(returnTypeName);
        if (returnType == ValueType::Invalid) {
            methodErrors.emplace_back(std::format("Return of method '{}.{}::{}' not supported '{}'", nameSpace, className, methodName, returnTypeName));
            continue;
        }

        if (returnType != method.retType) {
            methodErrors.emplace_back(std::format("Method '{}.{}::{}' has invalid return type '{}' when it should have '{}'", nameSpace, className, methodName, ValueTypeToString(method.retType), ValueTypeToString(returnType)));
            continue;
        }

        size_t i = 0;
        void* iter = nullptr;
        while (MonoType* type = mono_signature_get_params(sig, &iter)) {
            char* paramTypeName = mono_type_get_name(type);
            ValueType paramType = utils::MonoTypeToValueType(paramTypeName);
            if (paramType == ValueType::Invalid) {
                methodErrors.emplace_back(std::format("Parameter at index '{}' of method '{}.{}::{}' not supported '{}'", i, nameSpace, className, methodName, paramTypeName));
                continue;
            }

            if (paramType != method.paramTypes[i]) {
                methodErrors.emplace_back(std::format("Method '{}.{}::{}' has invalid param type '{}' at index {} when it should have '{}'", nameSpace, className, methodName, ValueTypeToString(method.paramTypes[i]), i, ValueTypeToString(paramType)));
                continue;
            }

            i++;
        }

        const auto [_, result] = _exportMethods.try_emplace(method.funcName, monoMethod);
        if (!result) {
            methodErrors.emplace_back(std::format("Function name duplicate: ", method.funcName));
            continue;
        }

        Function function{_rt};
        void* methodAddr = function.GetJitFunc(method, MethodCall);
        if (!methodAddr) {
            methodErrors.emplace_back(std::format("Method JIT generation error: ", function.GetError()));
            continue;
        }
        _functions.emplace_back(std::move(function));

        methods.emplace_back(method.name, methodAddr);
    }

    if (!methodErrors.empty()) {
        std::ostringstream funcs;
        funcs << methodErrors[0];
        for (auto it = std::next(methodErrors.begin()); it != methodErrors.end(); ++it) {
            funcs << ", " << *it;
        }
        return ErrorData{ funcs.str() };
    }

    return LoadResultData{ std::move(methods) };
}

void ScriptEngine::OnMethodExport(const IPlugin& plugin) {
    for (const auto& [name, addr] : plugin.GetMethods()) {
        std::string funcName{ std::format("{}.{}::{}", plugin.GetName(), plugin.GetName(), name) };

        if (_importMethods.contains(funcName)) {
            _provider->Log(std::format("Method name duplicate: {}", funcName), Severity::Error);
            continue;
        }

        mono_add_internal_call(funcName.c_str(), addr);
        _importMethods.emplace(std::move(funcName));
    }
}

void ScriptEngine::OnPluginStart(const IPlugin& plugin) {
    ScriptRef script = FindScript(plugin.GetName());
    if (script.has_value()) {
        script->get().InvokeOnStart();
    }
}

void ScriptEngine::OnPluginEnd(const IPlugin& plugin) {
    ScriptRef script = FindScript(plugin.GetName());
    if (script.has_value()) {
        script->get().InvokeOnEnd();
    }
}

ScriptRef ScriptEngine::CreateScriptInstance(const IPlugin& plugin, MonoImage* image) {
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

        const auto [it, result] = _scripts.try_emplace(plugin.GetName(), ScriptInstance{ *this, plugin, monoClass });
        if (result)
            return std::get<ScriptInstance>(*it);
    }

    return std::nullopt;
}

ScriptRef ScriptEngine::FindScript(const std::string& name) {
    auto it = _scripts.find(name);
    if (it != _scripts.end())
        return std::get<ScriptInstance>(*it);
    return std::nullopt;
}

MonoClass* ScriptEngine::CacheCoreClass(std::string name) {
    MonoClass* klass = mono_class_from_name(_coreImage, "Wand", name.c_str());
    if (klass)
        _coreClasses.emplace(std::move(name), klass);
    return klass;
}

MonoMethod* ScriptEngine::CacheCoreMethod(MonoClass* klass, std::string name, int params) {
    MonoMethod* method = mono_class_get_method_from_name(klass, name.c_str(), params);
    if (method)
        _coreMethods.emplace(std::move(name), method);
    return method;
}

MonoClass* ScriptEngine::FindCoreClass(const std::string& name) const{
    auto it = _coreClasses.find(name);
    return it != _coreClasses.end() ? std::get<MonoClass*>(*it) : nullptr;
}

MonoMethod* ScriptEngine::FindCoreMethod(const std::string& name) const {
    auto it = _coreMethods.find(name);
    return it != _coreMethods.end() ? std::get<MonoMethod*>(*it) : nullptr;
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

/*_________________________________________________*/

ScriptInstance::ScriptInstance(const ScriptEngine& engine, const IPlugin& plugin, MonoClass* klass) : _klass{klass} {
    _instance = engine.InstantiateClass(_klass);

    // Call Script (base) constructor
    {
        MonoMethod* constructor = engine.FindCoreMethod(".ctor");
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

    _onStartMethod = mono_class_get_method_from_name(_klass, "OnStart", 0);
    _onEndMethod  = mono_class_get_method_from_name(_klass, "OnEnd", 0);
}

ScriptInstance::~ScriptInstance() = default;

void ScriptInstance::InvokeOnStart() const {
    if (_onStartMethod) {
        MonoObject* exception = nullptr;
        mono_runtime_invoke(_onStartMethod, _instance, nullptr, &exception);
        if (exception) {
            mono_print_unhandled_exception(exception);
        }
    }
}

void ScriptInstance::InvokeOnEnd() const {
    if (_onEndMethod) {
        MonoObject* exception = nullptr;
        mono_runtime_invoke(_onEndMethod, _instance, nullptr, &exception);
        if (exception) {
            mono_print_unhandled_exception(exception);
        }
    }
}