#include "script_engine.h"

#include <wizard/module.h>
#include <wizard/plugin.h>
#include <wizard/plugin_descriptor.h>

#include <iostream>
#include <cassert>
#include <fstream>

using namespace std::string_literals;
using namespace csharplm;

namespace csharplm::utils {
    template<typename T>
    static inline bool ReadBytes(const fs::path& file, const std::function<void(std::span<T>)>& callback) {
        std::ifstream istream{file, std::ios::binary};
        if (!istream)
            return false;
        std::vector<T> buffer{ std::istreambuf_iterator<char>(istream), std::istreambuf_iterator<char>() };
        callback({ buffer.data(), buffer.size() });
        return true;
    }

    MonoAssembly* LoadMonoAssembly(const fs::path& assemblyPath, bool loadPDB) {
        MonoImageOpenStatus status = MONO_IMAGE_IMAGE_INVALID;
        MonoImage* image = nullptr;

        ReadBytes<char>(assemblyPath, [&image, &status](std::span<char> buffer) {
            image = mono_image_open_from_data_full(buffer.data(), static_cast<uint32_t>(buffer.size()), 1, &status, 0);
        });

        if (status != MONO_IMAGE_OK) {
            std::cout << "Failed to load assembly file: " << mono_image_strerror(status) << std::endl;
            return nullptr;
        }

        if (loadPDB) {
            fs::path pdbPath{ assemblyPath };
            pdbPath.replace_extension(".pdb");

            ReadBytes<mono_byte>(pdbPath, [&image, &pdbPath](std::span<mono_byte> buffer) {
                mono_debug_open_image_from_memory(image, buffer.data(), static_cast<int>(buffer.size()));
                std::cout << "Loaded PDB: " << pdbPath.string() << std::endl;
            });
        }

        MonoAssembly* assembly = mono_assembly_load_from_full(image, assemblyPath.string().c_str(), &status, 0);
        mono_image_close(image);
        return assembly;
    }

    void PrintAssemblyTypes(MonoAssembly* assembly) {
        MonoImage* image = mono_assembly_get_image(assembly);
        const MonoTableInfo* typeDefinitionsTable = mono_image_get_table_info(image, MONO_TABLE_TYPEDEF);
        int numTypes = mono_table_info_get_rows(typeDefinitionsTable);

        for (int i = 0; i < numTypes; ++i) {
            uint32_t cols[MONO_TYPEDEF_SIZE];
            mono_metadata_decode_row(typeDefinitionsTable, i, cols, MONO_TYPEDEF_SIZE);

            const char* nameSpace = mono_metadata_string_heap(image, cols[MONO_TYPEDEF_NAMESPACE]);
            const char* name = mono_metadata_string_heap(image, cols[MONO_TYPEDEF_NAME]);

            std::cout << nameSpace << "." << name << std::endl;
        }
    }

    std::string MonoStringToString(MonoString* string) {
        char* cStr = mono_string_to_utf8(string);
        std::string str{cStr};
        mono_free(cStr);
        return str;
    }
}

bool ScriptEngine::Initialize(const wizard::IModule& m) {
    InitMono(m.GetBaseDir() / "mono/lib");

    fs::path coreAssemblyPath{ m.GetBinariesDir() / "Wand.dll" };

    // Create an app domain
    char appName[] = "WandMonoRuntime";
    _appDomain = mono_domain_create_appdomain(appName, nullptr);
    mono_domain_set(_appDomain, true);

    // Load a core assembly
    _coreAssembly = utils::LoadMonoAssembly(coreAssemblyPath, _enableDebugging);
    if (!_coreAssembly) {
        std::cout << "Could not load '" << coreAssemblyPath.string() << "' core assembly." << std::endl;
        return false;
    }

    _coreImage = mono_assembly_get_image(_coreAssembly);
    if (!_coreImage) {
        std::cout << "Could not load '" << coreAssemblyPath.string() << "' core image." << std::endl;
        return false;
    }

    // Retrieve and cache core classes/methods

    /// Plugin
    MonoClass* pluginClass = CacheCoreClass("Plugin");
    CacheCoreMethod(pluginClass, ".ctor", 8);

    return true;
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

ScriptRef ScriptEngine::FindScript(ScriptId id) {
    auto it = _scripts.find(id);
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

MonoArray* ScriptEngine::CreateArray(MonoClass *klass, size_t count) const {
    return mono_array_new(_appDomain, klass, count);
}

MonoObject* ScriptEngine::InstantiateClass(MonoClass* klass) const {
    MonoObject* instance = mono_object_new(_appDomain, klass);
    mono_runtime_object_init(instance);
    return instance;
}

bool ScriptEngine::LoadScript(const wizard::IPlugin& plugin) {
    MonoAssembly* assembly = utils::LoadMonoAssembly(plugin.GetFilePath(), _enableDebugging);
    if (!assembly)
        return false;

    MonoImage* image = mono_assembly_get_image(assembly);
    if (!image)
        return false;

    const MonoTableInfo* typeDefinitionsTable = mono_image_get_table_info(image, MONO_TABLE_TYPEDEF);
    int numTypes = mono_table_info_get_rows(typeDefinitionsTable);

    MonoClass* pluginClass = FindCoreClass("Plugin");

    bool success = false;

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

        _scripts.emplace(plugin.GetId(), ScriptInstance{ *this, plugin, assembly, image, monoClass });

        success = true;
    }

    return success;
}

void ScriptEngine::StartScript(const wizard::IPlugin& plugin) {
    auto script = FindScript(plugin.GetId());
    if (script.has_value()) {
        script->get().Invoke("OnStart");
    }
}

void ScriptEngine::EndScript(const wizard::IPlugin& plugin) {
    auto script = FindScript(plugin.GetId());
    if (script.has_value()) {
        script->get().Invoke("OnEnd");
    }
}

/*_________________________________________________*/

ScriptInstance::ScriptInstance(const ScriptEngine& engine, const wizard::IPlugin& plugin, MonoAssembly* assembly, MonoImage* image, MonoClass* klass)
    : _engine{engine}, _plugin{plugin}, _assembly{assembly}, _image{image}, _klass{klass} {

    _instance = _engine.InstantiateClass(_klass);

    // Call Script (base) constructor
    {
        MonoMethod* constructor = _engine.FindCoreMethod(".ctor");
        assert(constructor);
        const auto& desc = _plugin.GetDescriptor();
        ScriptId id = _plugin.GetId();
        MonoArray* deps = _engine.CreateArray(mono_get_string_class(), desc.dependencies.size());
        for (size_t i = 0; i < desc.dependencies.size(); ++i) {
            mono_array_set(deps, MonoString*, i,  _engine.CreateString(desc.dependencies[i].name));
        }
        std::array<void*, 8> args{
            &id,
            _engine.CreateString(_plugin.GetName()),
            _engine.CreateString(_plugin.GetFriendlyName()),
            _engine.CreateString(desc.friendlyName),
            _engine.CreateString(desc.versionName),
            _engine.CreateString(desc.createdBy),
            _engine.CreateString(desc.createdByURL),
            deps
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

MonoMethod* ScriptInstance::CacheVirtualMethod(std::string name) {
    assert(!_methods.contains(name));
    MonoMethod* method = _engine.FindCoreMethod(name);
    assert(method);
    MonoMethod* vmethod = mono_object_get_virtual_method(_instance, method);
    assert(vmethod);
    _methods.emplace(std::move(name), vmethod);
    return vmethod;
}