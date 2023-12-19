#pragma once

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

namespace wand {
    using PluginId = uint64_t;

    namespace utils {
        MonoAssembly* LoadMonoAssembly(const fs::path& assemblyPath, bool loadPDB = false);
        void PrintAssemblyTypes(MonoAssembly* assembly);
        std::string MonoStringToString(MonoString* string);
    }

    struct PluginInfo {
        MonoAssembly* assembly{ nullptr };
        MonoImage* image{ nullptr };
        MonoClass* klass{ nullptr };

        std::string name;
        std::string description;
        std::string author;
        std::string version;
        std::string url;
        std::vector<std::string> dependencies;
        fs::path path;

        PluginInfo() = default;
        PluginInfo(MonoAssembly* assembly, MonoImage* image, MonoClass* klass);
    };

    class PluginInstance {
    public:
        PluginInstance() = default;
        PluginInstance(PluginInfo&& pluginInfo, PluginId pluginId);
        PluginInstance(PluginInstance&& other) noexcept = default;
        ~PluginInstance() = default;

        void invokeOnCreate() const;
        void invokeOnDestroy() const;

        uint64_t getId() const { return id; }
        const std::string& getName() const { return info.name; }
        const std::string& getDescription() const { return info.description; }
        const std::string& getUrl() const { return info.url; }
        const std::string& getAuthor() const { return info.author; }
        const std::string& getVersion() const { return info.version; }
        const fs::path& getPath() const { return info.path; }

        operator bool() const { return instance != nullptr; }
        operator MonoObject*() const { return instance; }
        MonoObject* getManagedObject() const { return instance; }

    private:
		MonoMethod* cacheMethod(const char* name, int params);
		MonoMethod* cacheVirtualMethod(const char* name);
	
	private:
        PluginInfo info{};
        uint64_t id{ UINT64_MAX };

        MonoObject* instance{ nullptr };
		std::unordered_map<std::string, MonoMethod*> methods;

        friend class PluginManager;
    };

    using PluginList = std::vector<PluginInstance>;

    class PluginManager {
    private:
        PluginManager();
        ~PluginManager();

    public:
        static PluginManager& Get();

        void loadAll();
        //void unloadAll();

        const PluginList& getPlugins() const { return plugins; }
        size_t getPluginCount() { return plugins.size(); }

        PluginInstance* findPlugin(PluginId id);
        PluginInstance* findPlugin(std::string_view name);

    private:
        void initMono();
        void shutdownMono();
		
		bool loadPlugin(const fs::path& path);
		
        MonoString* createString(const char* string) const;
        MonoObject* instantiateClass(MonoClass* klass) const;

		MonoClass* cacheCoreClass(const char* name);
		MonoMethod* cacheCoreMethod(MonoClass* klass, const char* name, int params);
		
        MonoClass* findCoreClass(const std::string& name);
        MonoMethod* findCoreMethod(const std::string& name);

    private:
        MonoDomain* rootDomain{ nullptr };
        MonoDomain* appDomain{ nullptr };

        MonoAssembly* coreAssembly{ nullptr };
        MonoImage* coreImage{ nullptr };

        std::unordered_map<std::string, MonoClass*> coreClasses;
        std::unordered_map<std::string, MonoMethod*> coreMethods;

        PluginList plugins;

        fs::path corePath{ "../bin/Wand.dll" };
        fs::path pluginsPath{ "../bin/" };

        friend class PluginInstance;

#ifdef _DEBUG
        bool enableDebugging{ true };
#else
        bool enableDebugging{ false };
#endif
    };
}