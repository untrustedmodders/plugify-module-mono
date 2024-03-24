#include "module.h"
#include "glue.h"

#include <mono/jit/jit.h>
#include <mono/utils/mono-logger.h>
#include <mono/metadata/assembly.h>
#include <mono/metadata/object.h>
#include <mono/metadata/class.h>
#include <mono/metadata/attrdefs.h>
#include <mono/metadata/mono-debug.h>
#include <mono/metadata/mono-config.h>
#include <mono/metadata/threads.h>
#include <mono/metadata/exception.h>

#include <plugify/module.h>
#include <plugify/plugin.h>
#include <plugify/log.h>
#include <plugify/math.h>
#include <plugify/plugin_descriptor.h>
#include <plugify/plugify_provider.h>

#include <glaze/glaze.hpp>

#if CSHARPLM_PLATFORM_WINDOWS
#include <windows.h>
#endif

MONO_API MonoDelegate* mono_ftnptr_to_delegate(MonoClass* klass, void* ftn);
MONO_API void* mono_delegate_to_ftnptr(MonoDelegate* delegate);
MONO_API const void* mono_lookup_internal_call_full(MonoMethod* method, int warn_on_missing, mono_bool* uses_handles, mono_bool* foreign);

struct _MonoDelegate {
	MonoObject object;
	void* method_ptr;
	void* invoke_impl;
	MonoObject* target;
	MonoMethod* method;
	//....
};

#define LOG_PREFIX "[CSHARPLM] "

#if CSHARPLM_PLATFORM_WINDOWS
#define PATH_SEPARATOR ";"
#else
#define PATH_SEPARATOR ":"
#endif

using namespace csharplm;
using namespace plugify;
using namespace std::string_literals;

namespace csharplm::utils {
	std::string ReadText(const fs::path& filepath) {
		std::ifstream istream(filepath, std::ios::binary);
		if (!istream.is_open())
			return {};
		istream.unsetf(std::ios::skipws);
		return { std::istreambuf_iterator<char>(istream), std::istreambuf_iterator<char>() };
	}

	template<typename T>
	std::vector<T> ReadBytes(const fs::path& filepath) {
		std::ifstream istream(filepath, std::ios::binary);
		if (!istream.is_open())
			return {};
		return { std::istreambuf_iterator<char>(istream), std::istreambuf_iterator<char>() };
	}

	MonoAssembly* LoadMonoAssembly(const fs::path& assemblyPath, bool loadPDB, MonoImageOpenStatus& status) {
		auto buffer = ReadBytes<char>(assemblyPath);
		MonoImage* image = mono_image_open_from_data_full(buffer.data(), static_cast<uint32_t>(buffer.size()), 1, &status, 0);

		if (status != MONO_IMAGE_OK)
			return nullptr;

		/*for (int i = 0; i < mono_image_get_table_rows(image, MONO_TABLE_ASSEMBLYREF); ++i) {
			mono_assembly_load_reference(image, i);

			MonoAssemblyName* aname = g_csharplm.GetAssemblyName();
			mono_assembly_get_assemblyref(image, i, aname);

			MonoAssembly* assembly = mono_assembly_loaded(aname);
			if (!assembly) {
				std::error_code error;
				std::string_view name = mono_assembly_name_get_name(aname);
				bool hasExtension = name.ends_with(".dll") || name.ends_with(".exe");
				fs::path refPath(assemblyPath.parent_path() / name);
				if (hasExtension) {
					if (fs::exists(refPath, error)) {
						LoadMonoAssembly(refPath, loadPDB, status);
						if (status != MONO_IMAGE_OK)
							break;
					}
				} else {
					refPath += ".dll";
					if (fs::exists(refPath, error)) {
						LoadMonoAssembly(refPath, loadPDB, status);
						if (status == MONO_IMAGE_OK)
							continue;
					}

					refPath.replace_extension(".exe");
					if (fs::exists(refPath, error)) {
						LoadMonoAssembly(refPath, loadPDB, status);
						if (status != MONO_IMAGE_OK)
							break;
					}
					// If ref not load ?
				}
			}
		}*/

		if (loadPDB) {
			fs::path pdbPath(assemblyPath);
			pdbPath.replace_extension(".pdb");

			auto bytes = ReadBytes<mono_byte>(pdbPath);
			mono_debug_open_image_from_memory(image, bytes.data(), static_cast<int>(bytes.size()));

			// If pdf not load ?
		}

		MonoAssembly* assembly = mono_assembly_load_from_full(image, assemblyPath.string().c_str(), &status, 0);
		mono_image_close(image);
		return assembly;
	}

	std::string MonoStringToUTF8(MonoString* string) {
		if (string == nullptr || mono_string_length(string) == 0)
			return {};
		MonoError error;
		char* utf8 = mono_string_to_utf8_checked(string, &error);
		if (!mono_error_ok(&error)) {
			g_csharplm.GetProvider()->Log(std::format(LOG_PREFIX "Failed to convert MonoString* to UTF-8: [{}] '{}'.", mono_error_get_error_code(&error), mono_error_get_message(&error)), Severity::Debug);
			mono_error_cleanup(&error);
			return {};
		}
		std::string result(utf8);
		mono_free(utf8);
		return result;
	}

	template<typename T>
	void MonoArrayToVector(MonoArray* array, std::vector<T>& dest) {
		auto length = mono_array_length(array);
		dest.resize(length);
		for (size_t i = 0; i < length; ++i) {
			if constexpr (std::is_same_v<T, std::string>) {
				MonoObject* element = mono_array_get(array, MonoObject*, i);
				if (element != nullptr)
					dest[i] = MonoStringToUTF8(reinterpret_cast<MonoString*>(element));
				else
					dest[i] = T{};
			} else {
				dest[i] =  mono_array_get(array, T, i);
			}
		}
	}

	ValueType MonoTypeToValueType(const char* typeName) {
		static std::unordered_map<std::string, ValueType> valueTypeMap = {
			{ "System.Void", ValueType::Void },
			{ "System.Boolean", ValueType::Bool },
			{ "System.Char", ValueType::Char8 },
			{ "System.SByte", ValueType::Int8 },
			{ "System.Int16", ValueType::Int16 },
			{ "System.Int32", ValueType::Int32 },
			{ "System.Int64", ValueType::Int64 },
			{ "System.Byte", ValueType::UInt8 },
			{ "System.UInt16", ValueType::UInt16 },
			{ "System.UInt32", ValueType::UInt32 },
			{ "System.UInt64", ValueType::UInt64 },
			{ "System.IntPtr", ValueType::Ptr64 },
			{ "System.UIntPtr", ValueType::Ptr64 },
			{ "System.Single", ValueType::Float },
			{ "System.Double", ValueType::Double },
			{ "System.String", ValueType::String },
			{ "System.Boolean[]", ValueType::ArrayBool },
			{ "System.Char[]", ValueType::ArrayChar8 },
			{ "System.SByte[]", ValueType::ArrayInt8 },
			{ "System.Int16[]", ValueType::ArrayInt16 },
			{ "System.Int32[]", ValueType::ArrayInt32 },
			{ "System.Int64[]", ValueType::ArrayInt64 },
			{ "System.Byte[]", ValueType::ArrayUInt8 },
			{ "System.UInt16[]", ValueType::ArrayUInt16 },
			{ "System.UInt32[]", ValueType::ArrayUInt32 },
			{ "System.UInt64[]", ValueType::ArrayUInt64 },
			{ "System.IntPtr[]", ValueType::ArrayPtr64 },
			{ "System.UIntPtr[]", ValueType::ArrayPtr64 },
			{ "System.Single[]", ValueType::ArrayFloat },
			{ "System.Double[]", ValueType::ArrayDouble },
			{ "System.String[]", ValueType::ArrayString },

			{ "System.Delegate", ValueType::Function },
			{ "System.Func`1", ValueType::Function },
			{ "System.Func`2", ValueType::Function },
			{ "System.Func`3", ValueType::Function },
			{ "System.Func`4", ValueType::Function },
			{ "System.Func`5", ValueType::Function },
			{ "System.Func`6", ValueType::Function },
			{ "System.Func`7", ValueType::Function },
			{ "System.Func`8", ValueType::Function },
			{ "System.Func`9", ValueType::Function },
			{ "System.Func`10", ValueType::Function },
			{ "System.Func`11", ValueType::Function },
			{ "System.Func`12", ValueType::Function },
			{ "System.Func`13", ValueType::Function },
			{ "System.Func`14", ValueType::Function },
			{ "System.Func`15", ValueType::Function },
			{ "System.Func`16", ValueType::Function },
			{ "System.Func`17", ValueType::Function },
			{ "System.Action", ValueType::Function },
			{ "System.Action`1", ValueType::Function },
			{ "System.Action`2", ValueType::Function },
			{ "System.Action`3", ValueType::Function },
			{ "System.Action`4", ValueType::Function },
			{ "System.Action`5", ValueType::Function },
			{ "System.Action`6", ValueType::Function },
			{ "System.Action`7", ValueType::Function },
			{ "System.Action`8", ValueType::Function },
			{ "System.Action`9", ValueType::Function },
			{ "System.Action`10", ValueType::Function },
			{ "System.Action`11", ValueType::Function },
			{ "System.Action`12", ValueType::Function },
			{ "System.Action`13", ValueType::Function },
			{ "System.Action`14", ValueType::Function },
			{ "System.Action`15", ValueType::Function },
			{ "System.Action`16", ValueType::Function },
			
			{ "Plugify.Vector2", ValueType::Vector2 },
			{ "Plugify.Vector3", ValueType::Vector3 },
			{ "Plugify.Vector4", ValueType::Vector4 },
			{ "Plugify.Matrix4x4", ValueType::Matrix4x4 },
		};
		auto it = valueTypeMap.find(typeName);
		if (it != valueTypeMap.end())
			return std::get<ValueType>(*it);
		return ValueType::Invalid;
	}

	std::string GetStringProperty(const char* propertyName, MonoClass* classType, MonoObject* classObject) {
		MonoProperty* messageProperty = mono_class_get_property_from_name(classType, propertyName);
		MonoMethod* messageGetter = mono_property_get_get_method(messageProperty);
		MonoString* messageString = reinterpret_cast<MonoString*>(mono_runtime_invoke(messageGetter, classObject, nullptr, nullptr));
		if (messageString != nullptr)
			return MonoStringToUTF8(messageString);
		return {};
	}

	bool IsMethodPrimitive(const plugify::Method& method) {
		if (method.retType.type >= ValueType::LastPrimitive)
			return false;

		for (const auto& param : method.paramTypes) {
			if (param.type >= ValueType::LastPrimitive && param.type < ValueType::FirstPOD)
				return false;
		}

		return true;
	}

	void LoadSystemClass(std::vector<MonoClass*>& storage, const char* name) {
		MonoClass* klass = mono_class_from_name(mono_get_corlib(), "System", name);
		if (klass != nullptr) storage.push_back(klass);
	}

	ClassInfo LoadCoreClass(std::vector<std::string>& classErrors, MonoImage* image, const char* name, int paramCount) {
		MonoClass* klass = mono_class_from_name(image, "Plugify", name);
		if (!klass) {
			classErrors.emplace_back(name);
			return {};
		}
		MonoMethod* ctor = mono_class_get_method_from_name(klass, ".ctor", paramCount);
		if (!ctor) {
			classErrors.emplace_back(name + "::ctor"s);
			return {};
		}
		return { klass, ctor };
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

#if CSHARPLM_PLATFORM_WINDOWS
	std::string GetEnvVariable(const char* varName) {
		DWORD size = GetEnvironmentVariableA(varName, NULL, 0);
		std::string buffer(size, 0);
		GetEnvironmentVariableA(varName, buffer.data(), size);
		return buffer;
	}

	bool SetEnvVariable(const char* varName, const char* value) {
		return SetEnvironmentVariableA(varName, value) != 0;
	}
#else
	std::string GetEnvVariable(const char* varName) {
		char* val = getenv(varName);
		if (!val)
			return {};
		return val;
	}

	bool SetEnvVariable(const char* varName, const char* value) {
		return setenv(varName, value, 1) == 0;
	}
#endif
}

void FunctionRefQueueCallback(void* function) {
	delete reinterpret_cast<Function*>(function);
}

InitResult CSharpLanguageModule::Initialize(std::weak_ptr<IPlugifyProvider> provider, const IModule& module) {
	if (!(_provider = provider.lock()))
		return ErrorData{ "Provider not exposed" };

	auto json = utils::ReadText(module.GetBaseDir() / "settings.json");
	auto settings = glz::read_json<MonoSettings>(json);
	if (!settings.has_value())
		return ErrorData{ std::format("MonoSettings: 'settings.json' has JSON parsing error: {}", glz::format_error(settings.error(), json)) };
	_settings = std::move(*settings);

	fs::path monoPath(module.GetBaseDir() / "mono/" CSHARPLM_PLATFORM "/mono/");
	
	std::error_code error;
	if (!fs::exists(monoPath, error))
		return ErrorData{ std::format("Path to mono assemblies not exist '{}'", monoPath.string()) };

	fs::path configPath(module.GetBaseDir() / "config");

	if (!InitMono(monoPath, configPath))
		return ErrorData{ "Initialization of mono failed" };

	Glue::RegisterFunctions();

	_rt = std::make_shared<asmjit::JitRuntime>();

	// Create an app domain
	char appName[] = "PlugifyMonoRuntime";
	_appDomain = mono_domain_create_appdomain(appName, nullptr);
	mono_domain_set(_appDomain, true);

	fs::path coreAssemblyPath(module.GetBaseDir() / "bin/Plugify.dll");

	MonoImageOpenStatus status = MONO_IMAGE_IMAGE_INVALID;

	// Load a core assembly
	_core.assembly = utils::LoadMonoAssembly(coreAssemblyPath, _settings.enableDebugging, status);
	if (!_core.assembly)
		return ErrorData{std::format("Failed to load '{}' assembly. Reason: {}", coreAssemblyPath.string(), mono_image_strerror(status))};

	_core.image = mono_assembly_get_image(_core.assembly);
	if (!_core.image)
		return ErrorData{std::format("Failed to load '{}' image.", coreAssemblyPath.string())};

	// Retrieve and cache core classes/methods

	std::vector<std::string> classErrors;

	_plugin = utils::LoadCoreClass(classErrors, _core.image, "Plugin", 8);
	_vector2 = utils::LoadCoreClass(classErrors, _core.image, "Vector2", 2);
	_vector3 = utils::LoadCoreClass(classErrors, _core.image, "Vector3", 3);
	_vector4 = utils::LoadCoreClass(classErrors, _core.image, "Vector4", 4);
	_matrix4x4 = utils::LoadCoreClass(classErrors, _core.image, "Matrix4x4", 16);

	if (!classErrors.empty()) {
		std::string funcs("Not found: " + classErrors[0]);
		for (auto it = std::next(classErrors.begin()); it != classErrors.end(); ++it) {
			std::format_to(std::back_inserter(funcs), ", {}", *it);
		}
		return ErrorData{ std::move(funcs) };
	}

	/// Delegates
	utils::LoadSystemClass(_funcClasses, "Func`1");
	utils::LoadSystemClass(_funcClasses, "Func`2");
	utils::LoadSystemClass(_funcClasses, "Func`3");
	utils::LoadSystemClass(_funcClasses, "Func`4");
	utils::LoadSystemClass(_funcClasses, "Func`5");
	utils::LoadSystemClass(_funcClasses, "Func`6");
	utils::LoadSystemClass(_funcClasses, "Func`7");
	utils::LoadSystemClass(_funcClasses, "Func`8");
	utils::LoadSystemClass(_funcClasses, "Func`9");
	utils::LoadSystemClass(_funcClasses, "Func`10");
	utils::LoadSystemClass(_funcClasses, "Func`11");
	utils::LoadSystemClass(_funcClasses, "Func`12");
	utils::LoadSystemClass(_funcClasses, "Func`13");
	utils::LoadSystemClass(_funcClasses, "Func`14");
	utils::LoadSystemClass(_funcClasses, "Func`15");
	utils::LoadSystemClass(_funcClasses, "Func`16");
	utils::LoadSystemClass(_funcClasses, "Func`17");

	utils::LoadSystemClass(_actionClasses, "Action");
	utils::LoadSystemClass(_actionClasses, "Action`1");
	utils::LoadSystemClass(_actionClasses, "Action`2");
	utils::LoadSystemClass(_actionClasses, "Action`3");
	utils::LoadSystemClass(_actionClasses, "Action`4");
	utils::LoadSystemClass(_actionClasses, "Action`5");
	utils::LoadSystemClass(_actionClasses, "Action`6");
	utils::LoadSystemClass(_actionClasses, "Action`7");
	utils::LoadSystemClass(_actionClasses, "Action`8");
	utils::LoadSystemClass(_actionClasses, "Action`9");
	utils::LoadSystemClass(_actionClasses, "Action`10");
	utils::LoadSystemClass(_actionClasses, "Action`11");
	utils::LoadSystemClass(_actionClasses, "Action`12");
	utils::LoadSystemClass(_actionClasses, "Action`13");
	utils::LoadSystemClass(_actionClasses, "Action`14");
	utils::LoadSystemClass(_actionClasses, "Action`15");
	utils::LoadSystemClass(_actionClasses, "Action`16");

	_functionReferenceQueue = mono_gc_reference_queue_new(FunctionRefQueueCallback);

	// MonoAssemblyName is an incomplete type (internal to mono), so we can't allocate it ourselves.
	// There isn't any api to allocate an empty one either, so we need to do it this way.
	_assemblyName = mono_assembly_name_new("blank");
	mono_assembly_name_free(_assemblyName); // "it does not frees the object itself, only the name members" (typo included)
	
	DCCallVM* vm = dcNewCallVM(4096);
	dcMode(vm, DC_CALL_C_DEFAULT);
	_callVirtMachine = std::unique_ptr<DCCallVM, VMDeleter>(vm);

	_provider->Log(LOG_PREFIX "Inited!", Severity::Debug);

	return InitResultData{};
}

void CSharpLanguageModule::Shutdown() {
	if (_functionReferenceQueue) {
		mono_gc_reference_queue_free(_functionReferenceQueue);
		_functionReferenceQueue = nullptr;
	}
	
	if (_assemblyName) {
		mono_free(_assemblyName);
		_assemblyName = nullptr;
	}
	
	_callVirtMachine.reset();
	_cachedDelegates.clear();
	_funcClasses.clear();
	_actionClasses.clear();
	_importMethods.clear();
	_exportMethods.clear();
	_functions.clear();
	_methods.clear();
	_scripts.clear();
	_provider.reset();
	_rt.reset();

	ShutdownMono();
}

/*MonoAssembly* CSharpLanguageModule::OnMonoAssemblyPreloadHook(MonoAssemblyName* aname, char** assemblies_path, void* user_data) {
	return OnMonoAssemblyLoad(mono_assembly_name_get_name(aname));
}*/

bool CSharpLanguageModule::InitMono(const fs::path& monoPath, const fs::path& configPath) {
	mono_trace_set_print_handler(OnPrintCallback);
	mono_trace_set_printerr_handler(OnPrintErrorCallback);
	mono_trace_set_log_handler(OnLogCallback, nullptr);

	std::error_code error;
	std::string monoEnvPath(utils::GetEnvVariable("MONO_PATH"));
	for (const auto& entry : fs::directory_iterator(monoPath, error)) {
		if (entry.is_directory(error)) {
			fs::path path(entry.path());
			path.make_preferred();
			std::format_to(std::back_inserter(monoEnvPath), PATH_SEPARATOR "{}", path.string());
		}
	}
	//utils::SetEnvVariable("MONO_PATH", monoEnvPath.c_str());
	mono_set_assemblies_path(monoEnvPath.c_str());

	// Seems we can write custom assembly loader here
	//mono_install_assembly_preload_hook(OnMonoAssemblyPreloadHook, nullptr);

	if (_settings.enableDebugging) {
		if (!_settings.options.empty()) {
			std::vector<char*> options;
			options.reserve(_settings.options.size());
			for (auto& opt: _settings.options) {
				if (std::find(options.begin(), options.end(), opt.data()) == options.end()) {
					if (opt.starts_with("--debugger")) {
						_provider->Log(std::format(LOG_PREFIX "Mono debugger: {}", opt), Severity::Info);
					}
					options.push_back(opt.data());
				}
			}
			mono_jit_parse_options(static_cast<int>(options.size()), options.data());
		}
		mono_debug_init(MONO_DEBUG_FORMAT_MONO);
	}

	if (!_settings.level.empty())
		mono_trace_set_level_string(_settings.level.c_str());
	if (!_settings.mask.empty())
		mono_trace_set_mask_string(_settings.mask.c_str());

	mono_config_parse(fs::exists(configPath, error) ? configPath.string().c_str() : nullptr);

	_rootDomain = mono_jit_init("PlugifyJITRuntime");
	if (!_rootDomain)
		return false;

	if (_settings.enableDebugging) {
		mono_debug_domain_create(_rootDomain);
	}

	mono_thread_set_main(mono_thread_current());

	mono_install_unhandled_exception_hook(HandleException, nullptr);
	//mono_set_crash_chaining(true);

	char* buildInfo = mono_get_runtime_build_info();
	_provider->Log(std::format(LOG_PREFIX "Mono: Runtime version: {}", buildInfo), Severity::Debug);
	mono_free(buildInfo);

	return true;
}

void CSharpLanguageModule::ShutdownMono() {
	mono_domain_set(mono_get_root_domain(), false);

	if (_appDomain) {
		mono_domain_unload(_appDomain);
		_appDomain = nullptr;
	}

	if (_rootDomain) {
		mono_jit_cleanup(_rootDomain);
		_rootDomain = nullptr;
	}

	_core.assembly = nullptr;
	_core.image = nullptr;
}

template<typename T>
CSHARPLM_INLINE void* /*CSharpLanguageModule::*/MonoStructToArg(std::vector<void*>& args) {
	auto* dest = new T();
	args.push_back(dest);
	return dest;
}

template<typename T>
CSHARPLM_INLINE void* /*CSharpLanguageModule::*/MonoArrayToArg(MonoArray* source, std::vector<void*>& args) {
	auto* dest = new std::vector<T>();
	if (source != nullptr) {
		utils::MonoArrayToVector(source, *dest);
	}
	args.push_back(dest);
	return dest;
}

CSHARPLM_INLINE void* /*CSharpLanguageModule::*/MonoStringToArg(MonoString* source, std::vector<void*>& args) {
	std::string* dest;
	if (source != nullptr) {
		MonoError error;
		char* cStr = mono_string_to_utf8_checked(source, &error);
		if (!mono_error_ok(&error)) {
			g_csharplm.GetProvider()->Log(std::format(LOG_PREFIX "Failed to convert MonoString* to UTF-8: [{}] '{}'.", mono_error_get_error_code(&error), mono_error_get_message(&error)), Severity::Debug);
			mono_error_cleanup(&error);
			return {};
		}
		dest = new std::string(cStr);
		mono_free(cStr);
	} else {
		dest = new std::string();
	}
	args.push_back(dest);
	return dest;
}

void* CSharpLanguageModule::MonoDelegateToArg(MonoDelegate* source, const plugify::Method& method) {
	if (source == nullptr) {
		_provider->Log(LOG_PREFIX "Delegate is null", Severity::Warning);
		return nullptr;
	}

	if (source->method != nullptr) {
		const void* raw = mono_lookup_internal_call_full(source->method, 0, nullptr, nullptr);
		if (raw != nullptr) {
			void* addr = const_cast<void*>(raw);
			auto it = _functions.find(addr);
			if (it != _functions.end()) {
				return std::get<Function>(*it).GetUserData();
			} else {
				return addr;
			}
		}
	}

	uint32_t ref = mono_gchandle_new_weakref(reinterpret_cast<MonoObject*>(source), 0);

	auto it = _cachedDelegates.find(ref);
	if (it != _cachedDelegates.end()) {
		return std::get<void*>(*it);
	}
	
	CleanupDelegateCache();
	
	void* methodAddr;
		
	if (utils::IsMethodPrimitive(method)) {
		methodAddr = mono_delegate_to_ftnptr(source);
	} else {
		auto function = new plugify::Function(_rt);
		methodAddr = function->GetJitFunc(method, &DelegateCall, source);
		mono_gc_reference_queue_add(_functionReferenceQueue, reinterpret_cast<MonoObject*>(source), reinterpret_cast<void*>(function));
	}
	
	_cachedDelegates.emplace(ref, methodAddr);

	return methodAddr;
}

void CSharpLanguageModule::CleanupDelegateCache() {
	for (auto it = _cachedDelegates.begin(); it != _cachedDelegates.end();) {
		if (mono_gchandle_get_target(it->first) == nullptr) {
			it = _cachedDelegates.erase(it);
		} else {
			++it;
		}
	}
}

void DeleteParam(std::vector<void*>& args, uint8_t& i, ValueType type) {
	switch (type) {
		case ValueType::Vector2: {
			delete reinterpret_cast<plugify::Vector2*>(args[i++]);
			break;
		}
		case ValueType::Vector3: {
			delete reinterpret_cast<plugify::Vector3*>(args[i++]);
			break;
		}
		case ValueType::Vector4: {
			delete reinterpret_cast<plugify::Vector4*>(args[i++]);
			break;
		}
		case ValueType::Matrix4x4: {
			delete reinterpret_cast<plugify::Matrix4x4*>(args[i++]);
			break;
		}
		case ValueType::String: {
			delete reinterpret_cast<std::string*>(args[i++]);
			break;
		}
		case ValueType::ArrayBool: {
			delete reinterpret_cast<std::vector<bool>*>(args[i++]);
			break;
		}
		case ValueType::ArrayChar8: {
			delete reinterpret_cast<std::vector<char>*>(args[i++]);
			break;
		}
		case ValueType::ArrayChar16: {
			delete reinterpret_cast<std::vector<char16_t>*>(args[i++]);
			break;
		}
		case ValueType::ArrayInt8: {
			delete reinterpret_cast<std::vector<int16_t>*>(args[i++]);
			break;
		}
		case ValueType::ArrayInt16: {
			delete reinterpret_cast<std::vector<int16_t>*>(args[i++]);
			break;
		}
		case ValueType::ArrayInt32: {
			delete reinterpret_cast<std::vector<int32_t>*>(args[i++]);
			break;
		}
		case ValueType::ArrayInt64: {
			delete reinterpret_cast<std::vector<int64_t>*>(args[i++]);
			break;
		}
		case ValueType::ArrayUInt8: {
			delete reinterpret_cast<std::vector<uint8_t>*>(args[i++]);
			break;
		}
		case ValueType::ArrayUInt16: {
			delete reinterpret_cast<std::vector<uint16_t>*>(args[i++]);
			break;
		}
		case ValueType::ArrayUInt32: {
			delete reinterpret_cast<std::vector<uint32_t>*>(args[i++]);
			break;
		}
		case ValueType::ArrayUInt64: {
			delete reinterpret_cast<std::vector<uint64_t>*>(args[i++]);
			break;
		}
		case ValueType::ArrayPtr64: {
			delete reinterpret_cast<std::vector<uintptr_t>*>(args[i++]);
			break;
		}
		case ValueType::ArrayFloat: {
			delete reinterpret_cast<std::vector<float>*>(args[i++]);
			break;
		}
		case ValueType::ArrayDouble: {
			delete reinterpret_cast<std::vector<double>*>(args[i++]);
			break;
		}
		case ValueType::ArrayString: {
			delete reinterpret_cast<std::vector<std::string>*>(args[i++]);
			break;
		}
		default: {
			break;
		}
	}
}

// Call from C# to C++
void CSharpLanguageModule::ExternalCall(const Method* method, void* addr, const Parameters* p, uint8_t count, const ReturnValue* ret) {
	// TODO: Does mutex here good choose ?
	std::scoped_lock<std::mutex> lock(g_csharplm._mutex);
	std::vector<void*> args;

	DCCallVM* vm = g_csharplm._callVirtMachine.get();
	dcReset(vm);

	bool hasRet = method->retType.type > ValueType::LastPrimitive;
	bool hasRefs = false;

	// Store parameters

	if (hasRet) {
		switch (method->retType.type) {
			case ValueType::Vector2:
				dcArgPointer(vm, MonoStructToArg<plugify::Vector2>(args));
				break;
			case ValueType::Vector3:
				dcArgPointer(vm, MonoStructToArg<plugify::Vector3>(args));
				break;
			case ValueType::Vector4:
				dcArgPointer(vm, MonoStructToArg<plugify::Vector4>(args));
				break;
			case ValueType::Matrix4x4:
				dcArgPointer(vm, MonoStructToArg<plugify::Matrix4x4>(args));
				break;
			// MonoString*
			case ValueType::String:
				dcArgPointer(vm, MonoStringToArg(nullptr, args));
				break;
			// MonoArray*
			case ValueType::ArrayBool:
				dcArgPointer(vm, MonoArrayToArg<bool>(nullptr, args));
				break;
			case ValueType::ArrayChar8:
				dcArgPointer(vm, MonoArrayToArg<char>(nullptr, args));
				break;
			case ValueType::ArrayChar16:
				dcArgPointer(vm, MonoArrayToArg<char16_t>(nullptr, args));
				break;
			case ValueType::ArrayInt8:
				dcArgPointer(vm, MonoArrayToArg<int8_t>(nullptr, args));
				break;
			case ValueType::ArrayInt16:
				dcArgPointer(vm, MonoArrayToArg<int16_t>(nullptr, args));
				break;
			case ValueType::ArrayInt32:
				dcArgPointer(vm, MonoArrayToArg<int32_t>(nullptr, args));
				break;
			case ValueType::ArrayInt64:
				dcArgPointer(vm, MonoArrayToArg<int64_t>(nullptr, args));
				break;
			case ValueType::ArrayUInt8:
				dcArgPointer(vm, MonoArrayToArg<uint8_t>(nullptr, args));
				break;
			case ValueType::ArrayUInt16:
				dcArgPointer(vm, MonoArrayToArg<uint16_t>(nullptr, args));
				break;
			case ValueType::ArrayUInt32:
				dcArgPointer(vm, MonoArrayToArg<uint32_t>(nullptr, args));
				break;
			case ValueType::ArrayUInt64:
				dcArgPointer(vm, MonoArrayToArg<uint64_t>(nullptr, args));
				break;
			case ValueType::ArrayPtr64:
				dcArgPointer(vm, MonoArrayToArg<uintptr_t>(nullptr, args));
				break;
			case ValueType::ArrayFloat:
				dcArgPointer(vm, MonoArrayToArg<float>(nullptr, args));
				break;
			case ValueType::ArrayDouble:
				dcArgPointer(vm, MonoArrayToArg<double>(nullptr, args));
				break;
			case ValueType::ArrayString:
				dcArgPointer(vm, MonoArrayToArg<std::string>(nullptr, args));
				break;
			default:
				// Should not require storage
				break;
		}
	}

	for (uint8_t i = 0; i < count; ++i) {
		auto& param = method->paramTypes[i];
		if (param.ref) {
			switch (param.type) {
				case ValueType::Invalid:
				case ValueType::Void:
					break;
				case ValueType::Bool:
				case ValueType::Char8:
				case ValueType::Char16:
				case ValueType::Int8:
				case ValueType::Int16:
				case ValueType::Int32:
				case ValueType::Int64:
				case ValueType::UInt8:
				case ValueType::UInt16:
				case ValueType::UInt32:
				case ValueType::UInt64:
				case ValueType::Ptr64:
				case ValueType::Float:
				case ValueType::Double:
				case ValueType::Vector2:
				case ValueType::Vector3:
				case ValueType::Vector4:
				case ValueType::Matrix4x4:
					dcArgPointer(vm, p->GetArgumentPtr(i));
					break;
				// MonoDelegate*
				case ValueType::Function:
					dcArgPointer(vm, g_csharplm.MonoDelegateToArg(p->GetArgument<MonoDelegate*>(i), *param.prototype));
					break;
				// MonoString*
				case ValueType::String:
					dcArgPointer(vm, MonoStringToArg(p->GetArgument<MonoString*>(i), args));
					break;
				// MonoArray*
				case ValueType::ArrayBool:
					dcArgPointer(vm, MonoArrayToArg<bool>(p->GetArgument<MonoArray*>(i), args));
					break;
				case ValueType::ArrayChar8:
					dcArgPointer(vm, MonoArrayToArg<char>(p->GetArgument<MonoArray*>(i), args));
					break;
				case ValueType::ArrayChar16:
					dcArgPointer(vm, MonoArrayToArg<char16_t>(p->GetArgument<MonoArray*>(i), args));
					break;
				case ValueType::ArrayInt8:
					dcArgPointer(vm, MonoArrayToArg<int8_t>(p->GetArgument<MonoArray*>(i), args));
					break;
				case ValueType::ArrayInt16:
					dcArgPointer(vm, MonoArrayToArg<int16_t>(p->GetArgument<MonoArray*>(i), args));
					break;
				case ValueType::ArrayInt32:
					dcArgPointer(vm, MonoArrayToArg<int32_t>(p->GetArgument<MonoArray*>(i), args));
					break;
				case ValueType::ArrayInt64:
					dcArgPointer(vm, MonoArrayToArg<int64_t>(p->GetArgument<MonoArray*>(i), args));
					break;
				case ValueType::ArrayUInt8:
					dcArgPointer(vm, MonoArrayToArg<uint8_t>(p->GetArgument<MonoArray*>(i), args));
					break;
				case ValueType::ArrayUInt16:
					dcArgPointer(vm, MonoArrayToArg<uint16_t>(p->GetArgument<MonoArray*>(i), args));
					break;
				case ValueType::ArrayUInt32:
					dcArgPointer(vm, MonoArrayToArg<uint32_t>(p->GetArgument<MonoArray*>(i), args));
					break;
				case ValueType::ArrayUInt64:
					dcArgPointer(vm, MonoArrayToArg<uint64_t>(p->GetArgument<MonoArray*>(i), args));
					break;
				case ValueType::ArrayPtr64:
					dcArgPointer(vm, MonoArrayToArg<uintptr_t>(p->GetArgument<MonoArray*>(i), args));
					break;
				case ValueType::ArrayFloat:
					dcArgPointer(vm, MonoArrayToArg<float>(p->GetArgument<MonoArray*>(i), args));
					break;
				case ValueType::ArrayDouble:
					dcArgPointer(vm, MonoArrayToArg<double>(p->GetArgument<MonoArray*>(i), args));
					break;
				case ValueType::ArrayString:
					dcArgPointer(vm, MonoArrayToArg<std::string>(p->GetArgument<MonoArray*>(i), args));
					break;
				default:
					puts("Unsupported types!\n");
					break;
			}
		} else {
			switch (param.type) {
				case ValueType::Invalid:
				case ValueType::Void:
					break;
				case ValueType::Bool:
					dcArgBool(vm, p->GetArgument<bool>(i));
					break;
				case ValueType::Char8:
					dcArgChar(vm, p->GetArgument<char>(i));
					break;
				case ValueType::Char16:
					dcArgShort(vm, static_cast<short>(p->GetArgument<char16_t>(i)));
					break;
				case ValueType::Int8:
				case ValueType::UInt8:
					dcArgChar(vm, p->GetArgument<int8_t>(i));
					break;
				case ValueType::Int16:
				case ValueType::UInt16:
					dcArgShort(vm, p->GetArgument<int16_t>(i));
					break;
				case ValueType::Int32:
				case ValueType::UInt32:
					dcArgInt(vm, p->GetArgument<int32_t>(i));
					break;
				case ValueType::Int64:
				case ValueType::UInt64:
					dcArgLongLong(vm, p->GetArgument<int64_t>(i));
					break;
				case ValueType::Ptr64:
					dcArgPointer(vm, p->GetArgument<void*>(i));
					break;
				case ValueType::Float:
					dcArgFloat(vm, p->GetArgument<float>(i));
					break;
				case ValueType::Double:
					dcArgDouble(vm, p->GetArgument<double>(i));
					break;
				case ValueType::Vector2:
				case ValueType::Vector3:
				case ValueType::Vector4:
				case ValueType::Matrix4x4:
					dcArgPointer(vm, p->GetArgumentPtr(i));
					break;	
				// MonoDelegate*
				case ValueType::Function:
					dcArgPointer(vm, g_csharplm.MonoDelegateToArg(p->GetArgument<MonoDelegate*>(i), *param.prototype));
					break;
				// MonoString*
				case ValueType::String:
					dcArgPointer(vm, MonoStringToArg(p->GetArgument<MonoString*>(i), args));
					break;
				// MonoArray*
				case ValueType::ArrayBool:
					dcArgPointer(vm, MonoArrayToArg<bool>(p->GetArgument<MonoArray*>(i), args));
					break;
				case ValueType::ArrayChar8:
					dcArgPointer(vm, MonoArrayToArg<char>(p->GetArgument<MonoArray*>(i), args));
					break;
				case ValueType::ArrayChar16:
					dcArgPointer(vm, MonoArrayToArg<char16_t>(p->GetArgument<MonoArray*>(i), args));
					break;
				case ValueType::ArrayInt8:
					dcArgPointer(vm, MonoArrayToArg<int8_t>(p->GetArgument<MonoArray*>(i), args));
					break;
				case ValueType::ArrayInt16:
					dcArgPointer(vm, MonoArrayToArg<int16_t>(p->GetArgument<MonoArray*>(i), args));
					break;
				case ValueType::ArrayInt32:
					dcArgPointer(vm, MonoArrayToArg<int32_t>(p->GetArgument<MonoArray*>(i), args));
					break;
				case ValueType::ArrayInt64:
					dcArgPointer(vm, MonoArrayToArg<int64_t>(p->GetArgument<MonoArray*>(i), args));
					break;
				case ValueType::ArrayUInt8:
					dcArgPointer(vm, MonoArrayToArg<uint8_t>(p->GetArgument<MonoArray*>(i), args));
					break;
				case ValueType::ArrayUInt16:
					dcArgPointer(vm, MonoArrayToArg<uint16_t>(p->GetArgument<MonoArray*>(i), args));
					break;
				case ValueType::ArrayUInt32:
					dcArgPointer(vm, MonoArrayToArg<uint32_t>(p->GetArgument<MonoArray*>(i), args));
					break;
				case ValueType::ArrayUInt64:
					dcArgPointer(vm, MonoArrayToArg<uint64_t>(p->GetArgument<MonoArray*>(i), args));
					break;
				case ValueType::ArrayPtr64:
					dcArgPointer(vm, MonoArrayToArg<uintptr_t>(p->GetArgument<MonoArray*>(i), args));
					break;
				case ValueType::ArrayFloat:
					dcArgPointer(vm, MonoArrayToArg<float>(p->GetArgument<MonoArray*>(i), args));
					break;
				case ValueType::ArrayDouble:
					dcArgPointer(vm, MonoArrayToArg<double>(p->GetArgument<MonoArray*>(i), args));
					break;
				case ValueType::ArrayString:
					dcArgPointer(vm, MonoArrayToArg<std::string>(p->GetArgument<MonoArray*>(i), args));
					break;
				default:
					puts("Unsupported types!\n");
					break;
			}
		}
		hasRefs |= param.ref;
	}

	// Call function and store return

	switch (method->retType.type) {
		case ValueType::Invalid:
			break;

		case ValueType::Void: {
			dcCallVoid(vm, addr);
			break;
		}
		case ValueType::Bool: {
			bool val = dcCallBool(vm, addr);
			ret->SetReturnPtr(val);
			break;
		}
		case ValueType::Char8: {
			char val = dcCallChar(vm, addr);
			ret->SetReturnPtr(val);
			break;
		}
		case ValueType::Char16: {
			char16_t val = static_cast<char16_t>(dcCallShort(vm, addr));
			ret->SetReturnPtr(val);
			break;
		}
		case ValueType::Int8: {
			int8_t val = dcCallChar(vm, addr);
			ret->SetReturnPtr(val);
			break;
		}
		case ValueType::Int16: {
			int16_t val = dcCallShort(vm, addr);
			ret->SetReturnPtr(val);
			break;
		}
		case ValueType::Int32: {
			int32_t val = dcCallInt(vm, addr);
			ret->SetReturnPtr(val);
			break;
		}
		case ValueType::Int64: {
			int64_t val = dcCallLongLong(vm, addr);
			ret->SetReturnPtr(val);
			break;
		}
		case ValueType::UInt8: {
			uint8_t val = static_cast<uint8_t>(dcCallChar(vm, addr));
			ret->SetReturnPtr(val);
			break;
		}
		case ValueType::UInt16: {
			uint16_t val = static_cast<uint16_t>(dcCallShort(vm, addr));
			ret->SetReturnPtr(val);
			break;
		}
		case ValueType::UInt32: {
			uint32_t val = static_cast<uint32_t>(dcCallInt(vm, addr));
			ret->SetReturnPtr(val);
			break;
		}
		case ValueType::UInt64: {
			uint64_t val = static_cast<uint64_t>(dcCallLongLong(vm, addr));
			ret->SetReturnPtr(val);
			break;
		}
		case ValueType::Ptr64: {
			void* val = dcCallPointer(vm, addr);
			ret->SetReturnPtr(val);
			break;
		}
		case ValueType::Float: {
			float val = dcCallFloat(vm, addr);
			ret->SetReturnPtr(val);
			break;
		}
		case ValueType::Double: {
			double val = dcCallDouble(vm, addr);
			ret->SetReturnPtr(val);
			break;
		}
		case ValueType::Function: {
			void* val = dcCallPointer(vm, addr);
			ret->SetReturnPtr(g_csharplm.CreateDelegate(val, *method->retType.prototype));
			break;
		}
		case ValueType::Vector2: {
			dcCallVoid(vm, addr);
			ret->SetReturnPtr(g_csharplm.CreateObject(*reinterpret_cast<plugify::Vector2*>(args[0]), g_csharplm._vector2));
			break;
		}
		case ValueType::Vector3: {
			dcCallVoid(vm, addr);
			ret->SetReturnPtr(g_csharplm.CreateObject(*reinterpret_cast<plugify::Vector3*>(args[0]), g_csharplm._vector3));
			break;
		}
		case ValueType::Vector4: {
			dcCallVoid(vm, addr);
			ret->SetReturnPtr(g_csharplm.CreateObject(*reinterpret_cast<plugify::Vector4*>(args[0]), g_csharplm._vector4));
			break;
		}
		case ValueType::Matrix4x4: {
			dcCallVoid(vm, addr);
			ret->SetReturnPtr(g_csharplm.CreateObject(*reinterpret_cast<plugify::Matrix4x4*>(args[0]), g_csharplm._matrix4x4));
			break;
		}
		case ValueType::String: {
			dcCallVoid(vm, addr);
			ret->SetReturnPtr(g_csharplm.CreateString(*reinterpret_cast<std::string*>(args[0])));
			break;
		}
		case ValueType::ArrayBool: {
			dcCallVoid(vm, addr);
			ret->SetReturnPtr(g_csharplm.CreateArrayT<bool>(*reinterpret_cast<std::vector<bool>*>(args[0]), mono_get_char_class()));
			break;
		}
		case ValueType::ArrayChar8: {
			dcCallVoid(vm, addr);
			ret->SetReturnPtr(g_csharplm.CreateArrayT<char>(*reinterpret_cast<std::vector<char>*>(args[0]), mono_get_char_class()));
			break;
		}
		case ValueType::ArrayChar16: {
			dcCallVoid(vm, addr);
			ret->SetReturnPtr(g_csharplm.CreateArrayT<char16_t>(*reinterpret_cast<std::vector<char16_t>*>(args[0]), mono_get_int16_class()));
			break;
		}
		case ValueType::ArrayInt8: {
			dcCallVoid(vm, addr);
			ret->SetReturnPtr(g_csharplm.CreateArrayT<int8_t>(*reinterpret_cast<std::vector<int8_t>*>(args[0]), mono_get_sbyte_class()));
			break;
		}
		case ValueType::ArrayInt16: {
			dcCallVoid(vm, addr);
			ret->SetReturnPtr(g_csharplm.CreateArrayT<int16_t>(*reinterpret_cast<std::vector<int16_t>*>(args[0]), mono_get_int16_class()));
			break;
		}
		case ValueType::ArrayInt32: {
			dcCallVoid(vm, addr);
			ret->SetReturnPtr(g_csharplm.CreateArrayT<int32_t>(*reinterpret_cast<std::vector<int32_t>*>(args[0]), mono_get_int32_class()));
			break;
		}
		case ValueType::ArrayInt64: {
			dcCallVoid(vm, addr);
			ret->SetReturnPtr(g_csharplm.CreateArrayT<int64_t>(*reinterpret_cast<std::vector<int64_t>*>(args[0]), mono_get_int64_class()));
			break;
		}
		case ValueType::ArrayUInt8: {
			dcCallVoid(vm, addr);
			ret->SetReturnPtr(g_csharplm.CreateArrayT<uint8_t>(*reinterpret_cast<std::vector<uint8_t>*>(args[0]), mono_get_byte_class()));
			break;
		}
		case ValueType::ArrayUInt16: {
			dcCallVoid(vm, addr);
			ret->SetReturnPtr(g_csharplm.CreateArrayT<uint16_t>(*reinterpret_cast<std::vector<uint16_t>*>(args[0]), mono_get_uint16_class()));
			break;
		}
		case ValueType::ArrayUInt32: {
			dcCallVoid(vm, addr);
			ret->SetReturnPtr(g_csharplm.CreateArrayT<uint32_t>(*reinterpret_cast<std::vector<uint32_t>*>(args[0]), mono_get_uint32_class()));
			break;
		}
		case ValueType::ArrayUInt64: {
			dcCallVoid(vm, addr);
			ret->SetReturnPtr(g_csharplm.CreateArrayT<uint64_t>(*reinterpret_cast<std::vector<uint64_t>*>(args[0]), mono_get_uint64_class()));
			break;
		}
		case ValueType::ArrayPtr64: {
			dcCallVoid(vm, addr);
			ret->SetReturnPtr(g_csharplm.CreateArrayT<uintptr_t>(*reinterpret_cast<std::vector<uintptr_t>*>(args[0]), mono_get_intptr_class()));
			break;
		}
		case ValueType::ArrayFloat: {
			dcCallVoid(vm, addr);
			ret->SetReturnPtr(g_csharplm.CreateArrayT<float>(*reinterpret_cast<std::vector<float>*>(args[0]), mono_get_single_class()));
			break;
		}
		case ValueType::ArrayDouble: {
			dcCallVoid(vm, addr);
			ret->SetReturnPtr(g_csharplm.CreateArrayT<double>(*reinterpret_cast<std::vector<double>*>(args[0]), mono_get_double_class()));
			break;
		}
		case ValueType::ArrayString: {
			dcCallVoid(vm, addr);
			ret->SetReturnPtr(g_csharplm.CreateStringArray(*reinterpret_cast<std::vector<std::string>*>(args[0])));
			break;
		}
		default:
			puts("Unsupported types!\n");
			break;
	}

	// Pull back references into provided arguments

	if (hasRefs) {
		uint8_t j = hasRet; // skip first param if has return

		if (j < args.size()) {
			for (uint8_t i = 0; i < count; ++i) {
				auto& param = method->paramTypes[i];
				if (param.ref) {
					switch (param.type) {
						case ValueType::String: {
							p->SetArgument(i, g_csharplm.CreateString(*reinterpret_cast<std::string*>(args[j++])));
							break;
						}
						case ValueType::ArrayBool: {
							p->SetArgument(i, g_csharplm.CreateArrayT<bool>(*reinterpret_cast<std::vector<bool>*>(args[j++]), mono_get_char_class()));
							break;
						}
						case ValueType::ArrayChar8: {
							p->SetArgument(i, g_csharplm.CreateArrayT<char>(*reinterpret_cast<std::vector<char>*>(args[j++]), mono_get_char_class()));
							break;
						}
						case ValueType::ArrayChar16: {
							p->SetArgument(i, g_csharplm.CreateArrayT<char16_t>(*reinterpret_cast<std::vector<char16_t>*>(args[j++]), mono_get_int16_class()));
							break;
						}
						case ValueType::ArrayInt8: {
							p->SetArgument(i, g_csharplm.CreateArrayT<int8_t>(*reinterpret_cast<std::vector<int8_t>*>(args[j++]), mono_get_sbyte_class()));
							break;
						}
						case ValueType::ArrayInt16: {
							p->SetArgument(i, g_csharplm.CreateArrayT<int16_t>(*reinterpret_cast<std::vector<int16_t>*>(args[j++]), mono_get_int16_class()));
							break;
						}
						case ValueType::ArrayInt32: {
							p->SetArgument(i, g_csharplm.CreateArrayT<int32_t>(*reinterpret_cast<std::vector<int32_t>*>(args[j++]), mono_get_int32_class()));
							break;
						}
						case ValueType::ArrayInt64: {
							p->SetArgument(i, g_csharplm.CreateArrayT<int64_t>(*reinterpret_cast<std::vector<int64_t>*>(args[j++]), mono_get_int64_class()));
							break;
						}
						case ValueType::ArrayUInt8: {
							p->SetArgument(i, g_csharplm.CreateArrayT<uint8_t>(*reinterpret_cast<std::vector<uint8_t>*>(args[j++]), mono_get_byte_class()));
							break;
						}
						case ValueType::ArrayUInt16: {
							p->SetArgument(i, g_csharplm.CreateArrayT<uint16_t>(*reinterpret_cast<std::vector<uint16_t>*>(args[j++]), mono_get_uint16_class()));
							break;
						}
						case ValueType::ArrayUInt32: {
							p->SetArgument(i, g_csharplm.CreateArrayT<uint32_t>(*reinterpret_cast<std::vector<uint32_t>*>(args[j++]), mono_get_uint32_class()));
							break;
						}
						case ValueType::ArrayUInt64: {
							p->SetArgument(i, g_csharplm.CreateArrayT<uint64_t>(*reinterpret_cast<std::vector<uint64_t>*>(args[j++]), mono_get_uint64_class()));
							break;
						}
						case ValueType::ArrayPtr64: {
							p->SetArgument(i, g_csharplm.CreateArrayT<uintptr_t>(*reinterpret_cast<std::vector<uintptr_t>*>(args[j++]), mono_get_intptr_class()));
							break;
						}
						case ValueType::ArrayFloat: {
							p->SetArgument(i, g_csharplm.CreateArrayT<float>(*reinterpret_cast<std::vector<float>*>(args[j++]), mono_get_single_class()));
							break;
						}
						case ValueType::ArrayDouble: {
							p->SetArgument(i, g_csharplm.CreateArrayT<double>(*reinterpret_cast<std::vector<double>*>(args[j++]), mono_get_double_class()));
							break;
						}
						case ValueType::ArrayString: {
							p->SetArgument(i, g_csharplm.CreateStringArray(*reinterpret_cast<std::vector<std::string>*>(args[j++])));
							break;
						}
						default:
							break;
					}
				}
				if (j == args.size())
					break;
			}
		}
	}

	if (!args.empty()) {
		uint8_t j = 0;

		if (hasRet) {
			DeleteParam(args, j, method->retType.type);
		}

		if (j < args.size()) {
			for (uint8_t i = 0; i < count; ++i) {
				auto& param = method->paramTypes[i];
				DeleteParam(args, j, param.type);
				if (j == args.size())
					break;
			}
		}
	}
}

// Call from C++ to C#
void CSharpLanguageModule::InternalCall(const Method* method, void* data, const Parameters* p, uint8_t count, const ReturnValue* ret) {
	const auto& [monoMethod, monoObject] = *reinterpret_cast<ExportMethod*>(data);

	/// We not create param vector, and use Parameters* params directly if passing primitives
	bool hasRefs = false;
	bool hasRet = method->retType.type > ValueType::LastPrimitive;

	std::vector<void*> args(hasRet ? count - 1 : count);

	for (uint8_t i = hasRet, j = 0; i < count; ++i) {
		auto& param = method->paramTypes[j];
		hasRefs |= param.ref;
		switch (param.type) {
			case ValueType::Invalid:
			case ValueType::Void:
				// Should not trigger!
				break;
			case ValueType::Bool:
			case ValueType::Char8:
			case ValueType::Char16:
			case ValueType::Int8:
			case ValueType::Int16:
			case ValueType::Int32:
			case ValueType::Int64:
			case ValueType::UInt8:
			case ValueType::UInt16:
			case ValueType::UInt32:
			case ValueType::UInt64:
			case ValueType::Ptr64:
			case ValueType::Float:
			case ValueType::Double:
			case ValueType::Vector2:
			case ValueType::Vector3:
			case ValueType::Vector4:
			case ValueType::Matrix4x4:
				args[j] = p->GetArgumentPtr(i);
				break;
			case ValueType::Function: {
				auto source = p->GetArgument<void*>(i);
				args[j] = source != nullptr ? g_csharplm.CreateDelegate(source, *param.prototype) : nullptr;
				break;
			}
			case ValueType::String: {
				auto source = p->GetArgument<std::string*>(i);
				args[j] = source != nullptr ? g_csharplm.CreateString(*source) : nullptr;
				break;
			}
			case ValueType::ArrayBool: {
				auto source = p->GetArgument<std::vector<bool>*>(i);
				args[j] = source != nullptr ? g_csharplm.CreateArrayT<bool>(*source, mono_get_char_class()) : nullptr;
				break;
			}
			case ValueType::ArrayChar8: {
				auto source = p->GetArgument<std::vector<char>*>(i);
				args[j] = source != nullptr ? g_csharplm.CreateArrayT<char>(*source, mono_get_char_class()) : nullptr;
				break;
			}
			case ValueType::ArrayChar16: {
				auto source = p->GetArgument<std::vector<char16_t>*>(i);
				args[j] = source != nullptr ? g_csharplm.CreateArrayT<char16_t>(*source, mono_get_int16_class()) : nullptr;
				break;
			}
			case ValueType::ArrayInt8: {
				auto source = p->GetArgument<std::vector<int8_t>*>(i);
				args[j] = source != nullptr ? g_csharplm.CreateArrayT<int8_t>(*source, mono_get_sbyte_class()) : nullptr;
				break;
			}
			case ValueType::ArrayInt16: {
				auto source = p->GetArgument<std::vector<int16_t>*>(i);
				args[j] = source != nullptr ? g_csharplm.CreateArrayT<int16_t>(*source, mono_get_int16_class()) : nullptr;
				break;
			}
			case ValueType::ArrayInt32: {
				auto source = p->GetArgument<std::vector<int32_t>*>(i);
				args[j] = source != nullptr ? g_csharplm.CreateArrayT<int32_t>(*source, mono_get_int32_class()) : nullptr;
				break;
			}
			case ValueType::ArrayInt64: {
				auto source = p->GetArgument<std::vector<int64_t>*>(i);
				args[j] = source != nullptr ? g_csharplm.CreateArrayT<int64_t>(*source, mono_get_int64_class()) : nullptr;
				break;
			}
			case ValueType::ArrayUInt8: {
				auto source = p->GetArgument<std::vector<uint8_t>*>(i);
				args[j] = source != nullptr ? g_csharplm.CreateArrayT<uint8_t>(*source, mono_get_byte_class()) : nullptr;
				break;
			}
			case ValueType::ArrayUInt16: {
				auto source = p->GetArgument<std::vector<uint16_t>*>(i);
				args[j] = source != nullptr ? g_csharplm.CreateArrayT<uint16_t>(*source, mono_get_uint16_class()) : nullptr;
				break;
			}
			case ValueType::ArrayUInt32: {
				auto source = p->GetArgument<std::vector<uint32_t>*>(i);
				args[j] = source != nullptr ? g_csharplm.CreateArrayT<uint32_t>(*source, mono_get_uint32_class()) : nullptr;
				break;
			}
			case ValueType::ArrayUInt64: {
				auto source = p->GetArgument<std::vector<uint64_t>*>(i);
				args[j] = source != nullptr ? g_csharplm.CreateArrayT<uint64_t>(*source, mono_get_uint64_class()) : nullptr;
				break;
			}
			case ValueType::ArrayPtr64: {
				auto source = p->GetArgument<std::vector<uintptr_t>*>(i);
				args[j] = source != nullptr ? g_csharplm.CreateArrayT<uintptr_t>(*source, mono_get_intptr_class()) : nullptr;
				break;
			}
			case ValueType::ArrayFloat: {
				auto source = p->GetArgument<std::vector<float>*>(i);
				args[j] = source != nullptr ? g_csharplm.CreateArrayT<float>(*source, mono_get_single_class()) : nullptr;
				break;
			}
			case ValueType::ArrayDouble: {
				auto source = p->GetArgument<std::vector<double>*>(i);
				args[j] = source != nullptr ? g_csharplm.CreateArrayT<double>(*source, mono_get_double_class()) : nullptr;
				break;
			}
			case ValueType::ArrayString: {
				auto source = p->GetArgument<std::vector<std::string>*>(i);
				args[j] = source != nullptr ? g_csharplm.CreateStringArray(*source) : nullptr;
				break;
			}
		}
		++j;
	}

	MonoObject* exception = nullptr;
	MonoObject* result = mono_runtime_invoke(monoMethod, monoObject, args.data(), &exception);
	if (exception) {
		HandleException(exception, nullptr);
		ret->SetReturnPtr<uintptr_t>({});
		return;
	}

	if (hasRefs) {
		for (uint8_t i = hasRet, j = 0; i < count; ++i) {
			auto& param = method->paramTypes[j];
			if (param.ref) {
				switch (param.type) {
					case ValueType::String: {
						auto source = reinterpret_cast<MonoString*>(args[j]);
						if (source != nullptr)  {
							auto dest = p->GetArgument<std::string*>(i);
							*dest = utils::MonoStringToUTF8(source);
						}
						break;
					}
					case ValueType::ArrayBool: {
						auto source = reinterpret_cast<MonoArray*>(args[j]);
						if (source != nullptr) {
							auto dest = p->GetArgument<std::vector<bool>*>(i);
							utils::MonoArrayToVector(source, *dest);
						}
						break;
					}
					case ValueType::ArrayChar8: {
						auto source = reinterpret_cast<MonoArray*>(args[j]);
						if (source != nullptr) {
							auto dest = p->GetArgument<std::vector<char>*>(i);
							utils::MonoArrayToVector(source, *dest);
						}
						break;
					}
					case ValueType::ArrayChar16: {
						auto source = reinterpret_cast<MonoArray*>(args[j]);
						if (source != nullptr) {
							auto dest = p->GetArgument<std::vector<char16_t>*>(i);
							utils::MonoArrayToVector(source, *dest);
						}
						break;
					}
					case ValueType::ArrayInt8: {
						auto source = reinterpret_cast<MonoArray*>(args[j]);
						if (source != nullptr) {
							auto dest = p->GetArgument<std::vector<int8_t>*>(i);
							utils::MonoArrayToVector(source, *dest);
						}
						break;
					}
					case ValueType::ArrayInt16: {
						auto source = reinterpret_cast<MonoArray*>(args[j]);
						if (source != nullptr) {
							auto dest = p->GetArgument<std::vector<int16_t>*>(i);
							utils::MonoArrayToVector(source, *dest);
						}
						break;
					}
					case ValueType::ArrayInt32: {
						auto source = reinterpret_cast<MonoArray*>(args[j]);
						if (source != nullptr) {
							auto dest = p->GetArgument<std::vector<int32_t>*>(i);
							utils::MonoArrayToVector(source, *dest);
						}
						break;
					}
					case ValueType::ArrayInt64: {
						auto source = reinterpret_cast<MonoArray*>(args[j]);
						if (source != nullptr) {
							auto dest = p->GetArgument<std::vector<int64_t>*>(i);
							utils::MonoArrayToVector(source, *dest);
						}
						break;
					}
					case ValueType::ArrayUInt8: {
						auto source = reinterpret_cast<MonoArray*>(args[j]);
						if (source != nullptr) {
							auto dest = p->GetArgument<std::vector<uint8_t>*>(i);
							utils::MonoArrayToVector(source, *dest);
						}
						break;
					}
					case ValueType::ArrayUInt16: {
						auto source = reinterpret_cast<MonoArray*>(args[j]);
						if (source != nullptr) {
							auto dest = p->GetArgument<std::vector<uint16_t>*>(i);
							utils::MonoArrayToVector(source, *dest);
						}
						break;
					}
					case ValueType::ArrayUInt32: {
						auto source = reinterpret_cast<MonoArray*>(args[j]);
						if (source != nullptr) {
							auto dest = p->GetArgument<std::vector<uint32_t>*>(i);
							utils::MonoArrayToVector(source, *dest);
						}
						break;
					}
					case ValueType::ArrayUInt64: {
						auto source = reinterpret_cast<MonoArray*>(args[j]);
						if (source != nullptr) {
							auto dest = p->GetArgument<std::vector<uint64_t>*>(i);
							utils::MonoArrayToVector(source, *dest);
						}
						break;
					}
					case ValueType::ArrayPtr64: {
						auto source = reinterpret_cast<MonoArray*>(args[j]);
						if (source != nullptr) {
							auto dest = p->GetArgument<std::vector<uintptr_t>*>(i);
							utils::MonoArrayToVector(source, *dest);
						}
						break;
					}
					case ValueType::ArrayFloat: {
						auto source = reinterpret_cast<MonoArray*>(args[j]);
						if (source != nullptr) {
							auto dest = p->GetArgument<std::vector<float>*>(i);
							utils::MonoArrayToVector(source, *dest);
						}
						break;
					}
					case ValueType::ArrayDouble: {
						auto source = reinterpret_cast<MonoArray*>(args[j]);
						if (source != nullptr) {
							auto dest = p->GetArgument<std::vector<double>*>(i);
							utils::MonoArrayToVector(source, *dest);
						}
						break;
					}
					case ValueType::ArrayString: {
						auto source = reinterpret_cast<MonoArray*>(args[j]);
						if (source != nullptr) {
							auto dest = p->GetArgument<std::vector<std::string>*>(i);
							utils::MonoArrayToVector(source, *dest);
						}
						break;
					}
					default:
						break;
				}
			}
			++j;
		}
	}

	if (result) {
		switch (method->retType.type) {
			case ValueType::Invalid:
			case ValueType::Void:
				break;
			case ValueType::Bool: {
				bool val = *reinterpret_cast<bool*>(mono_object_unbox(result));
				ret->SetReturnPtr<bool>(val);
				break;
			}
			case ValueType::Char8: {
				char val = *reinterpret_cast<char*>(mono_object_unbox(result));
				ret->SetReturnPtr<char>(val);
				break;
			}
			case ValueType::Char16: {
				char16_t val = *reinterpret_cast<char16_t*>(mono_object_unbox(result));
				ret->SetReturnPtr<char16_t>(val);
				break;
			}
			case ValueType::Int8: {
				int8_t val = *reinterpret_cast<int8_t*>(mono_object_unbox(result));
				ret->SetReturnPtr<int8_t>(val);
				break;
			}
			case ValueType::Int16: {
				int16_t val = *reinterpret_cast<int16_t*>(mono_object_unbox(result));
				ret->SetReturnPtr<int16_t>(val);
				break;
			}
			case ValueType::Int32: {
				int32_t val = *reinterpret_cast<int32_t*>(mono_object_unbox(result));
				ret->SetReturnPtr<int32_t>(val);
				break;
			}
			case ValueType::Int64: {
				int64_t val = *reinterpret_cast<int64_t*>(mono_object_unbox(result));
				ret->SetReturnPtr<int64_t>(val);
				break;
			}
			case ValueType::UInt8: {
				uint8_t val = *reinterpret_cast<uint8_t*>(mono_object_unbox(result));
				ret->SetReturnPtr<uint8_t>(val);
				break;
			}
			case ValueType::UInt16: {
				uint16_t val = *reinterpret_cast<uint16_t*>(mono_object_unbox(result));
				ret->SetReturnPtr<uint16_t>(val);
				break;
			}
			case ValueType::UInt32: {
				uint32_t val = *reinterpret_cast<uint32_t*>(mono_object_unbox(result));
				ret->SetReturnPtr<uint32_t>(val);
				break;
			}
			case ValueType::UInt64: {
				uint64_t val = *reinterpret_cast<uint64_t*>(mono_object_unbox(result));
				ret->SetReturnPtr<uint64_t>(val);
				break;
			}
			case ValueType::Ptr64: {
				uintptr_t val = *reinterpret_cast<uintptr_t*>(mono_object_unbox(result));
				ret->SetReturnPtr<uintptr_t>(val);
				break;
			}
			case ValueType::Float: {
				float val = *reinterpret_cast<float*>(mono_object_unbox(result));
				ret->SetReturnPtr<float>(val);
				break;
			}
			case ValueType::Double: {
				double val = *reinterpret_cast<double*>(mono_object_unbox(result));
				ret->SetReturnPtr<double>(val);
				break;
			}
			case ValueType::Vector2: {
				auto source = reinterpret_cast<plugify::Vector2*>(mono_object_unbox(result));
				auto dest = p->GetArgument<plugify::Vector2*>(0);
				*dest = *source;
				break;
			}
			case ValueType::Vector3: {
				auto source = reinterpret_cast<plugify::Vector3*>(mono_object_unbox(result));
				auto dest = p->GetArgument<plugify::Vector3*>(0);
				*dest = *source;
				break;
			}
			case ValueType::Vector4: {
				auto source = reinterpret_cast<plugify::Vector4*>(mono_object_unbox(result));
				auto dest = p->GetArgument<plugify::Vector4*>(0);
				*dest = *source;
				break;
			}
			case ValueType::Matrix4x4: {
				auto source = reinterpret_cast<plugify::Matrix4x4*>(mono_object_unbox(result));
				auto dest = p->GetArgument<plugify::Matrix4x4*>(0);
				*dest = *source;
				break;
			}
			case ValueType::Function: {
				auto source = reinterpret_cast<MonoDelegate*>(result);
				ret->SetReturnPtr<void*>(g_csharplm.MonoDelegateToArg(source, *(method->retType.prototype)));
				break;
			}
			case ValueType::String: {
				auto source = reinterpret_cast<MonoString*>(result);
				auto dest = p->GetArgument<std::string*>(0);
				*dest = utils::MonoStringToUTF8(source);
				break;
			}
			case ValueType::ArrayBool: {
				auto source = reinterpret_cast<MonoArray*>(result);
				auto dest = p->GetArgument<std::vector<bool>*>(0);
				utils::MonoArrayToVector(source, *dest);
				break;
			}
			case ValueType::ArrayChar8: {
				auto source = reinterpret_cast<MonoArray*>(result);
				auto dest = p->GetArgument<std::vector<char>*>(0);
				utils::MonoArrayToVector(source, *dest);
				break;
			}
			case ValueType::ArrayChar16: {
				auto source = reinterpret_cast<MonoArray*>(result);
				auto dest = p->GetArgument<std::vector<char16_t>*>(0);
				utils::MonoArrayToVector(source, *dest);
				break;
			}
			case ValueType::ArrayInt8: {
				auto source = reinterpret_cast<MonoArray*>(result);
				auto dest = p->GetArgument<std::vector<int8_t>*>(0);
				utils::MonoArrayToVector(source, *dest);
				break;
			}
			case ValueType::ArrayInt16: {
				auto source = reinterpret_cast<MonoArray*>(result);
				auto dest = p->GetArgument<std::vector<int16_t>*>(0);
				utils::MonoArrayToVector(source, *dest);
				break;
			}
			case ValueType::ArrayInt32: {
				auto source = reinterpret_cast<MonoArray*>(result);
				auto dest = p->GetArgument<std::vector<int32_t>*>(0);
				utils::MonoArrayToVector(source, *dest);
				break;
			}
			case ValueType::ArrayInt64: {
				auto source = reinterpret_cast<MonoArray*>(result);
				auto dest = p->GetArgument<std::vector<int64_t>*>(0);
				utils::MonoArrayToVector(source, *dest);
				break;
			}
			case ValueType::ArrayUInt8: {
				auto source = reinterpret_cast<MonoArray*>(result);
				auto dest = p->GetArgument<std::vector<uint8_t>*>(0);
				utils::MonoArrayToVector(source, *dest);
				break;
			}
			case ValueType::ArrayUInt16: {
				auto source = reinterpret_cast<MonoArray*>(result);
				auto dest = p->GetArgument<std::vector<uint16_t>*>(0);
				utils::MonoArrayToVector(source, *dest);
				break;
			}
			case ValueType::ArrayUInt32: {
				auto source = reinterpret_cast<MonoArray*>(result);
				auto dest = p->GetArgument<std::vector<uint32_t>*>(0);
				utils::MonoArrayToVector(source, *dest);
				break;
			}
			case ValueType::ArrayUInt64: {
				auto source = reinterpret_cast<MonoArray*>(result);
				auto dest = p->GetArgument<std::vector<uint64_t>*>(0);
				utils::MonoArrayToVector(source, *dest);
				break;
			}
			case ValueType::ArrayPtr64: {
				auto source = reinterpret_cast<MonoArray*>(result);
				auto dest = p->GetArgument<std::vector<uintptr_t>*>(0);
				utils::MonoArrayToVector(source, *dest);
				break;
			}
			case ValueType::ArrayFloat: {
				auto source = reinterpret_cast<MonoArray*>(result);
				auto dest = p->GetArgument<std::vector<float>*>(0);
				utils::MonoArrayToVector(source, *dest);
				break;
			}
			case ValueType::ArrayDouble: {
				auto source = reinterpret_cast<MonoArray*>(result);
				auto dest = p->GetArgument<std::vector<double>*>(0);
				utils::MonoArrayToVector(source, *dest);
				break;
			}
			case ValueType::ArrayString: {
				auto source = reinterpret_cast<MonoArray*>(result);
				auto dest = p->GetArgument<std::vector<std::string>*>(0);
				utils::MonoArrayToVector(source, *dest);
				break;
			}
		}
	} else {
		switch (method->retType.type) {
			case ValueType::Invalid:
			case ValueType::Void:
				break;
			default:
				ret->SetReturnPtr<uintptr_t>({});
				break;
		}
	}
}

// Call from C++ to C#
void CSharpLanguageModule::DelegateCall(const Method* method, void* data, const Parameters* p, uint8_t count, const ReturnValue* ret) {
	auto monoDelegate = reinterpret_cast<MonoObject*>(data);

	/// We not create param vector, and use Parameters* params directly if passing primitives
	bool hasRefs = false;
	bool hasRet = method->retType.type > ValueType::LastPrimitive;

	std::vector<void*> args(hasRet ? count - 1 : count);

	for (uint8_t i = hasRet, j = 0; i < count; ++i) {
		auto& param = method->paramTypes[j];
		hasRefs |= param.ref;
		switch (param.type) {
			case ValueType::Invalid:
			case ValueType::Void:
				// Should not trigger!
				break;
			case ValueType::Bool:
			case ValueType::Char8:
			case ValueType::Char16:
			case ValueType::Int8:
			case ValueType::Int16:
			case ValueType::Int32:
			case ValueType::Int64:
			case ValueType::UInt8:
			case ValueType::UInt16:
			case ValueType::UInt32:
			case ValueType::UInt64:
			case ValueType::Ptr64:
			case ValueType::Float:
			case ValueType::Double:
			case ValueType::Vector2:
			case ValueType::Vector3:
			case ValueType::Vector4:
			case ValueType::Matrix4x4:
				args[j] = p->GetArgumentPtr(i);
				break;
			case ValueType::Function: {
				auto source = p->GetArgument<void*>(i);
				args[j] = source != nullptr ? g_csharplm.CreateDelegate(source, *param.prototype) : nullptr;
				break;
			}
			case ValueType::String: {
				auto source = p->GetArgument<std::string*>(i);
				args[j] = source != nullptr ? g_csharplm.CreateString(*source) : nullptr;
				break;
			}
			case ValueType::ArrayBool: {
				auto source = p->GetArgument<std::vector<bool>*>(i);
				args[j] = source != nullptr ? g_csharplm.CreateArrayT<bool>(*source, mono_get_char_class()) : nullptr;
				break;
			}
			case ValueType::ArrayChar8: {
				auto source = p->GetArgument<std::vector<char>*>(i);
				args[j] = source != nullptr ? g_csharplm.CreateArrayT<char>(*source, mono_get_char_class()) : nullptr;
				break;
			}
			case ValueType::ArrayChar16: {
				auto source = p->GetArgument<std::vector<char16_t>*>(i);
				args[j] = source != nullptr ? g_csharplm.CreateArrayT<char16_t>(*source, mono_get_int16_class()) : nullptr;
				break;
			}
			case ValueType::ArrayInt8: {
				auto source = p->GetArgument<std::vector<int8_t>*>(i);
				args[j] = source != nullptr ? g_csharplm.CreateArrayT<int8_t>(*source, mono_get_sbyte_class()) : nullptr;
				break;
			}
			case ValueType::ArrayInt16: {
				auto source = p->GetArgument<std::vector<int16_t>*>(i);
				args[j] = source != nullptr ? g_csharplm.CreateArrayT<int16_t>(*source, mono_get_int16_class()) : nullptr;
				break;
			}
			case ValueType::ArrayInt32: {
				auto source = p->GetArgument<std::vector<int32_t>*>(i);
				args[j] = source != nullptr ? g_csharplm.CreateArrayT<int32_t>(*source, mono_get_int32_class()) : nullptr;
				break;
			}
			case ValueType::ArrayInt64: {
				auto source = p->GetArgument<std::vector<int64_t>*>(i);
				args[j] = source != nullptr ? g_csharplm.CreateArrayT<int64_t>(*source, mono_get_int64_class()) : nullptr;
				break;
			}
			case ValueType::ArrayUInt8: {
				auto source = p->GetArgument<std::vector<uint8_t>*>(i);
				args[j] = source != nullptr ? g_csharplm.CreateArrayT<uint8_t>(*source, mono_get_byte_class()) : nullptr;
				break;
			}
			case ValueType::ArrayUInt16: {
				auto source = p->GetArgument<std::vector<uint16_t>*>(i);
				args[j] = source != nullptr ? g_csharplm.CreateArrayT<uint16_t>(*source, mono_get_uint16_class()) : nullptr;
				break;
			}
			case ValueType::ArrayUInt32: {
				auto source = p->GetArgument<std::vector<uint32_t>*>(i);
				args[j] = source != nullptr ? g_csharplm.CreateArrayT<uint32_t>(*source, mono_get_uint32_class()) : nullptr;
				break;
			}
			case ValueType::ArrayUInt64: {
				auto source = p->GetArgument<std::vector<uint64_t>*>(i);
				args[j] = source != nullptr ? g_csharplm.CreateArrayT<uint64_t>(*source, mono_get_uint64_class()) : nullptr;
				break;
			}
			case ValueType::ArrayPtr64: {
				auto source = p->GetArgument<std::vector<uintptr_t>*>(i);
				args[j] = source != nullptr ? g_csharplm.CreateArrayT<uintptr_t>(*source, mono_get_intptr_class()) : nullptr;
				break;
			}
			case ValueType::ArrayFloat: {
				auto source = p->GetArgument<std::vector<float>*>(i);
				args[j] = source != nullptr ? g_csharplm.CreateArrayT<float>(*source, mono_get_single_class()) : nullptr;
				break;
			}
			case ValueType::ArrayDouble: {
				auto source = p->GetArgument<std::vector<double>*>(i);
				args[j] = source != nullptr ? g_csharplm.CreateArrayT<double>(*source, mono_get_double_class()) : nullptr;
				break;
			}
			case ValueType::ArrayString: {
				auto source = p->GetArgument<std::vector<std::string>*>(i);
				args[j] = source != nullptr ? g_csharplm.CreateStringArray(*source) : nullptr;
				break;
			}
		}
		++j;
	}

	MonoObject* exception = nullptr;
	MonoObject* result = mono_runtime_delegate_invoke(monoDelegate, args.data(), &exception);
	if (exception) {
		HandleException(exception, nullptr);
		ret->SetReturnPtr<uintptr_t>({});
		return;
	}

	if (hasRefs) {
		for (uint8_t i = hasRet, j = 0; i < count; ++i) {
			auto& param = method->paramTypes[j];
			if (param.ref) {
				switch (param.type) {
					case ValueType::String: {
						auto source = reinterpret_cast<MonoString*>(args[j]);
						if (source != nullptr)  {
							auto dest = p->GetArgument<std::string*>(i);
							*dest = utils::MonoStringToUTF8(source);
						}
						break;
					}
					case ValueType::ArrayBool: {
						auto source = reinterpret_cast<MonoArray*>(args[j]);
						if (source != nullptr) {
							auto dest = p->GetArgument<std::vector<bool>*>(i);
							utils::MonoArrayToVector(source, *dest);
						}
						break;
					}
					case ValueType::ArrayChar8: {
						auto source = reinterpret_cast<MonoArray*>(args[j]);
						if (source != nullptr) {
							auto dest = p->GetArgument<std::vector<char>*>(i);
							utils::MonoArrayToVector(source, *dest);
						}
						break;
					}
					case ValueType::ArrayChar16: {
						auto source = reinterpret_cast<MonoArray*>(args[j]);
						if (source != nullptr) {
							auto dest = p->GetArgument<std::vector<char16_t>*>(i);
							utils::MonoArrayToVector(source, *dest);
						}
						break;
					}
					case ValueType::ArrayInt8: {
						auto source = reinterpret_cast<MonoArray*>(args[j]);
						if (source != nullptr) {
							auto dest = p->GetArgument<std::vector<int8_t>*>(i);
							utils::MonoArrayToVector(source, *dest);
						}
						break;
					}
					case ValueType::ArrayInt16: {
						auto source = reinterpret_cast<MonoArray*>(args[j]);
						if (source != nullptr) {
							auto dest = p->GetArgument<std::vector<int16_t>*>(i);
							utils::MonoArrayToVector(source, *dest);
						}
						break;
					}
					case ValueType::ArrayInt32: {
						auto source = reinterpret_cast<MonoArray*>(args[j]);
						if (source != nullptr) {
							auto dest = p->GetArgument<std::vector<int32_t>*>(i);
							utils::MonoArrayToVector(source, *dest);
						}
						break;
					}
					case ValueType::ArrayInt64: {
						auto source = reinterpret_cast<MonoArray*>(args[j]);
						if (source != nullptr) {
							auto dest = p->GetArgument<std::vector<int64_t>*>(i);
							utils::MonoArrayToVector(source, *dest);
						}
						break;
					}
					case ValueType::ArrayUInt8: {
						auto source = reinterpret_cast<MonoArray*>(args[j]);
						if (source != nullptr) {
							auto dest = p->GetArgument<std::vector<uint8_t>*>(i);
							utils::MonoArrayToVector(source, *dest);
						}
						break;
					}
					case ValueType::ArrayUInt16: {
						auto source = reinterpret_cast<MonoArray*>(args[j]);
						if (source != nullptr) {
							auto dest = p->GetArgument<std::vector<uint16_t>*>(i);
							utils::MonoArrayToVector(source, *dest);
						}
						break;
					}
					case ValueType::ArrayUInt32: {
						auto source = reinterpret_cast<MonoArray*>(args[j]);
						if (source != nullptr) {
							auto dest = p->GetArgument<std::vector<uint32_t>*>(i);
							utils::MonoArrayToVector(source, *dest);
						}
						break;
					}
					case ValueType::ArrayUInt64: {
						auto source = reinterpret_cast<MonoArray*>(args[j]);
						if (source != nullptr) {
							auto dest = p->GetArgument<std::vector<uint64_t>*>(i);
							utils::MonoArrayToVector(source, *dest);
						}
						break;
					}
					case ValueType::ArrayPtr64: {
						auto source = reinterpret_cast<MonoArray*>(args[j]);
						if (source != nullptr) {
							auto dest = p->GetArgument<std::vector<uintptr_t>*>(i);
							utils::MonoArrayToVector(source, *dest);
						}
						break;
					}
					case ValueType::ArrayFloat: {
						auto source = reinterpret_cast<MonoArray*>(args[j]);
						if (source != nullptr) {
							auto dest = p->GetArgument<std::vector<float>*>(i);
							utils::MonoArrayToVector(source, *dest);
						}
						break;
					}
					case ValueType::ArrayDouble: {
						auto source = reinterpret_cast<MonoArray*>(args[j]);
						if (source != nullptr) {
							auto dest = p->GetArgument<std::vector<double>*>(i);
							utils::MonoArrayToVector(source, *dest);
						}
						break;
					}
					case ValueType::ArrayString: {
						auto source = reinterpret_cast<MonoArray*>(args[j]);
						if (source != nullptr) {
							auto dest = p->GetArgument<std::vector<std::string>*>(i);
							utils::MonoArrayToVector(source, *dest);
						}
						break;
					}
					default:
						break;
				}
			}
			++j;
		}
	}

	if (result) {
		switch (method->retType.type) {
			case ValueType::Invalid:
			case ValueType::Void:
				break;
			case ValueType::Bool: {
				bool val = *reinterpret_cast<bool*>(mono_object_unbox(result));
				ret->SetReturnPtr<bool>(val);
				break;
			}
			case ValueType::Char8: {
				char val = *reinterpret_cast<char*>(mono_object_unbox(result));
				ret->SetReturnPtr<char>(val);
				break;
			}
			case ValueType::Char16: {
				char16_t val = *reinterpret_cast<char16_t*>(mono_object_unbox(result));
				ret->SetReturnPtr<char16_t>(val);
				break;
			}
			case ValueType::Int8: {
				int8_t val = *reinterpret_cast<int8_t*>(mono_object_unbox(result));
				ret->SetReturnPtr<int8_t>(val);
				break;
			}
			case ValueType::Int16: {
				int16_t val = *reinterpret_cast<int16_t*>(mono_object_unbox(result));
				ret->SetReturnPtr<int16_t>(val);
				break;
			}
			case ValueType::Int32: {
				int32_t val = *reinterpret_cast<int32_t*>(mono_object_unbox(result));
				ret->SetReturnPtr<int32_t>(val);
				break;
			}
			case ValueType::Int64: {
				int64_t val = *reinterpret_cast<int64_t*>(mono_object_unbox(result));
				ret->SetReturnPtr<int64_t>(val);
				break;
			}
			case ValueType::UInt8: {
				uint8_t val = *reinterpret_cast<uint8_t*>(mono_object_unbox(result));
				ret->SetReturnPtr<uint8_t>(val);
				break;
			}
			case ValueType::UInt16: {
				uint16_t val = *reinterpret_cast<uint16_t*>(mono_object_unbox(result));
				ret->SetReturnPtr<uint16_t>(val);
				break;
			}
			case ValueType::UInt32: {
				uint32_t val = *reinterpret_cast<uint32_t*>(mono_object_unbox(result));
				ret->SetReturnPtr<uint32_t>(val);
				break;
			}
			case ValueType::UInt64: {
				uint64_t val = *reinterpret_cast<uint64_t*>(mono_object_unbox(result));
				ret->SetReturnPtr<uint64_t>(val);
				break;
			}
			case ValueType::Ptr64: {
				uintptr_t val = *reinterpret_cast<uintptr_t*>(mono_object_unbox(result));
				ret->SetReturnPtr<uintptr_t>(val);
				break;
			}
			case ValueType::Float: {
				float val = *reinterpret_cast<float*>(mono_object_unbox(result));
				ret->SetReturnPtr<float>(val);
				break;
			}
			case ValueType::Double: {
				double val = *reinterpret_cast<double*>(mono_object_unbox(result));
				ret->SetReturnPtr<double>(val);
				break;
			}
			case ValueType::Vector2: {
				auto source = reinterpret_cast<plugify::Vector2*>(mono_object_unbox(result));
				auto dest = p->GetArgument<plugify::Vector2*>(0);
				*dest = *source;
				break;
			}
			case ValueType::Vector3: {
				auto source = reinterpret_cast<plugify::Vector3*>(mono_object_unbox(result));
				auto dest = p->GetArgument<plugify::Vector3*>(0);
				*dest = *source;
				break;
			}
			case ValueType::Vector4: {
				auto source = reinterpret_cast<plugify::Vector4*>(mono_object_unbox(result));
				auto dest = p->GetArgument<plugify::Vector4*>(0);
				*dest = *source;
				break;
			}
			case ValueType::Matrix4x4: {
				auto source = reinterpret_cast<plugify::Matrix4x4*>(mono_object_unbox(result));
				auto dest = p->GetArgument<plugify::Matrix4x4*>(0);
				*dest = *source;
				break;
			}
			case ValueType::Function: {
				auto source = reinterpret_cast<MonoDelegate*>(result);
				ret->SetReturnPtr<void*>(g_csharplm.MonoDelegateToArg(source, *(method->retType.prototype)));
				break;
			}
			case ValueType::String: {
				auto source = reinterpret_cast<MonoString*>(result);
				auto dest = p->GetArgument<std::string*>(0);
				*dest = utils::MonoStringToUTF8(source);
				break;
			}
			case ValueType::ArrayBool: {
				auto source = reinterpret_cast<MonoArray*>(result);
				auto dest = p->GetArgument<std::vector<bool>*>(0);
				utils::MonoArrayToVector(source, *dest);
				break;
			}
			case ValueType::ArrayChar8: {
				auto source = reinterpret_cast<MonoArray*>(result);
				auto dest = p->GetArgument<std::vector<char>*>(0);
				utils::MonoArrayToVector(source, *dest);
				break;
			}
			case ValueType::ArrayChar16: {
				auto source = reinterpret_cast<MonoArray*>(result);
				auto dest = p->GetArgument<std::vector<char16_t>*>(0);
				utils::MonoArrayToVector(source, *dest);
				break;
			}
			case ValueType::ArrayInt8: {
				auto source = reinterpret_cast<MonoArray*>(result);
				auto dest = p->GetArgument<std::vector<int8_t>*>(0);
				utils::MonoArrayToVector(source, *dest);
				break;
			}
			case ValueType::ArrayInt16: {
				auto source = reinterpret_cast<MonoArray*>(result);
				auto dest = p->GetArgument<std::vector<int16_t>*>(0);
				utils::MonoArrayToVector(source, *dest);
				break;
			}
			case ValueType::ArrayInt32: {
				auto source = reinterpret_cast<MonoArray*>(result);
				auto dest = p->GetArgument<std::vector<int32_t>*>(0);
				utils::MonoArrayToVector(source, *dest);
				break;
			}
			case ValueType::ArrayInt64: {
				auto source = reinterpret_cast<MonoArray*>(result);
				auto dest = p->GetArgument<std::vector<int64_t>*>(0);
				utils::MonoArrayToVector(source, *dest);
				break;
			}
			case ValueType::ArrayUInt8: {
				auto source = reinterpret_cast<MonoArray*>(result);
				auto dest = p->GetArgument<std::vector<uint8_t>*>(0);
				utils::MonoArrayToVector(source, *dest);
				break;
			}
			case ValueType::ArrayUInt16: {
				auto source = reinterpret_cast<MonoArray*>(result);
				auto dest = p->GetArgument<std::vector<uint16_t>*>(0);
				utils::MonoArrayToVector(source, *dest);
				break;
			}
			case ValueType::ArrayUInt32: {
				auto source = reinterpret_cast<MonoArray*>(result);
				auto dest = p->GetArgument<std::vector<uint32_t>*>(0);
				utils::MonoArrayToVector(source, *dest);
				break;
			}
			case ValueType::ArrayUInt64: {
				auto source = reinterpret_cast<MonoArray*>(result);
				auto dest = p->GetArgument<std::vector<uint64_t>*>(0);
				utils::MonoArrayToVector(source, *dest);
				break;
			}
			case ValueType::ArrayPtr64: {
				auto source = reinterpret_cast<MonoArray*>(result);
				auto dest = p->GetArgument<std::vector<uintptr_t>*>(0);
				utils::MonoArrayToVector(source, *dest);
				break;
			}
			case ValueType::ArrayFloat: {
				auto source = reinterpret_cast<MonoArray*>(result);
				auto dest = p->GetArgument<std::vector<float>*>(0);
				utils::MonoArrayToVector(source, *dest);
				break;
			}
			case ValueType::ArrayDouble: {
				auto source = reinterpret_cast<MonoArray*>(result);
				auto dest = p->GetArgument<std::vector<double>*>(0);
				utils::MonoArrayToVector(source, *dest);
				break;
			}
			case ValueType::ArrayString: {
				auto source = reinterpret_cast<MonoArray*>(result);
				auto dest = p->GetArgument<std::vector<std::string>*>(0);
				utils::MonoArrayToVector(source, *dest);
				break;
			}
		}
	} else {
		switch (method->retType.type) {
			case ValueType::Invalid:
			case ValueType::Void:
				break;
			default:
				ret->SetReturnPtr<uintptr_t>({});
				break;
		}
	}
}

LoadResult CSharpLanguageModule::OnPluginLoad(const IPlugin& plugin) {
	MonoImageOpenStatus status = MONO_IMAGE_IMAGE_INVALID;
	
	fs::path assemblyPath(plugin.GetBaseDir() / plugin.GetDescriptor().entryPoint);
	
	MonoAssembly* assembly = utils::LoadMonoAssembly(assemblyPath, _settings.enableDebugging, status);
	if (!assembly)
		return ErrorData{ std::format("Failed to load assembly: '{}'", mono_image_strerror(status)) };

	MonoImage* image = mono_assembly_get_image(assembly);
	if (!image)
		return ErrorData{ "Failed to load assembly image" };

	auto scriptRef = CreateScriptInstance(plugin, image);
	if (!scriptRef.has_value())
		return ErrorData{ "Failed to find 'Plugin' class implementation" };
	auto& script = scriptRef->get();

	std::vector<std::string> methodErrors;

	const auto& exportedMethods = plugin.GetDescriptor().exportedMethods;
	std::vector<MethodData> methods;
	methods.reserve(exportedMethods.size());

	for (const auto& method : exportedMethods) {
		auto seperated = utils::Split(method.funcName, ".");
		if (seperated.size() != 4) {
			methodErrors.emplace_back(std::format("Invalid function name: '{}'. Please provide name in that format: 'Plugin.Namespace.Class.Method'", method.funcName));
			continue;
		}

		std::string nameSpace(seperated[1]);
		std::string className(seperated[2]);
		std::string methodName(seperated[3]);

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

		MonoObject* monoInstance = monoClass == script._klass ? script._instance : nullptr;

		uint32_t methodFlags = mono_method_get_flags(monoMethod, nullptr);
		if (!(methodFlags & MONO_METHOD_ATTR_STATIC) && !monoInstance) {
			methodErrors.emplace_back(std::format("Method '{}.{}::{}' is not static", nameSpace, className, methodName));
			continue;
		}

		MonoMethodSignature* sig = mono_method_signature(monoMethod);

		uint32_t paramCount = mono_signature_get_param_count(sig);
		if (paramCount != method.paramTypes.size()) {
			methodErrors.emplace_back(std::format("Invalid parameter count {} when it should have {}", method.paramTypes.size(), paramCount));
			continue;
		}

		MonoType* returnType = mono_signature_get_return_type(sig);
		char* returnTypeName = mono_type_get_name(returnType);
		ValueType retType = utils::MonoTypeToValueType(returnTypeName);
		
		if (retType == ValueType::Invalid) {
			MonoClass* returnClass = mono_class_from_mono_type(returnType);
			if (mono_class_is_delegate(returnClass)) {
				retType = ValueType::Function;
			}
		}
		
		if (retType == ValueType::Invalid) {
			methodErrors.emplace_back(std::format("Return of method '{}.{}::{}' not supported '{}'", nameSpace, className, methodName, returnTypeName));
			continue;
		}

		if (retType != method.retType.type) {
			methodErrors.emplace_back(std::format("Method '{}.{}::{}' has invalid return type '{}' when it should have '{}'", nameSpace, className, methodName, ValueTypeToString(method.retType.type), ValueTypeToString(retType)));
			continue;
		}

		bool methodFail = false;

		size_t i = 0;
		void* iter = nullptr;
		while (MonoType* type = mono_signature_get_params(sig, &iter)) {
			char* paramTypeName = mono_type_get_name(type);
			ValueType paramType = utils::MonoTypeToValueType(paramTypeName);
			
			if (paramType == ValueType::Invalid) {
				MonoClass* paramClass = mono_class_from_mono_type(type);
				if (mono_class_is_delegate(paramClass)) {
					paramType = ValueType::Function;
				}
			}
			
			if (paramType == ValueType::Invalid) {
				methodFail = true;
				methodErrors.emplace_back(std::format("Parameter at index '{}' of method '{}.{}::{}' not supported '{}'", i, nameSpace, className, methodName, paramTypeName));
				continue;
			}

			if (paramType != method.paramTypes[i].type) {
				methodFail = true;
				methodErrors.emplace_back(std::format("Method '{}.{}::{}' has invalid param type '{}' at index {} when it should have '{}'", nameSpace, className, methodName, ValueTypeToString(method.paramTypes[i].type), i, ValueTypeToString(paramType)));
				continue;
			}

			i++;
		}

		if (methodFail)
			continue;

		auto exportMethod = std::make_unique<ExportMethod>(monoMethod, monoInstance);

		Function function(_rt);
		void* methodAddr = function.GetJitFunc(method, &InternalCall, exportMethod.get());
		if (!methodAddr) {
			methodErrors.emplace_back(std::format("Method JIT generation error: ", function.GetError()));
			continue;
		}
		_functions.emplace(exportMethod.get(), std::move(function));
		_exportMethods.emplace_back(std::move(exportMethod));

		methods.emplace_back(method.name, methodAddr);
	}

	if (!methodErrors.empty()) {
		std::string funcs(methodErrors[0]);
		for (auto it = std::next(methodErrors.begin()); it != methodErrors.end(); ++it) {
			std::format_to(std::back_inserter(funcs), ", {}", *it);
		}
		return ErrorData{ std::move(funcs) };
	}

	return LoadResultData{ std::move(methods) };
}

void CSharpLanguageModule::OnMethodExport(const IPlugin& plugin) {
	for (const auto& [name, addr] : plugin.GetMethods()) {
		auto funcName = std::format("{}.{}::{}", plugin.GetName(), plugin.GetName(), name);

		if (_importMethods.contains(funcName)) {
			_provider->Log(std::format(LOG_PREFIX "Method name duplicate: {}", funcName), Severity::Error);
			continue;
		}

		for (const auto& method : plugin.GetDescriptor().exportedMethods) {
			if (name == method.name) {
				if (utils::IsMethodPrimitive(method)) {
					mono_add_internal_call(funcName.c_str(), addr);
				} else {
					Function function(_rt);
					void* methodAddr = function.GetJitFunc(method, &ExternalCall, addr);
					if (!methodAddr) {
						_provider->Log(std::format(LOG_PREFIX "{}: {}", method.funcName, function.GetError()), Severity::Error);
						continue;
					}
					_functions.emplace(methodAddr, std::move(function));

					mono_add_internal_call(funcName.c_str(), methodAddr);
				}

				_importMethods.emplace(std::move(funcName)/*, ImportMethod{method, addr}*/);
				break;
			}
		}
	}
}

void CSharpLanguageModule::OnPluginStart(const IPlugin& plugin) {
	ScriptOpt scriptRef = FindScript(plugin.GetName());
	if (scriptRef.has_value()) {
		auto& script = scriptRef->get();

		script.InvokeOnStart();
	}
}

void CSharpLanguageModule::OnPluginEnd(const IPlugin& plugin) {
	ScriptOpt scriptRef = FindScript(plugin.GetName());
	if (scriptRef.has_value()) {
		auto& script = scriptRef->get();

		script.InvokeOnEnd();
	}
}

ScriptOpt CSharpLanguageModule::CreateScriptInstance(const IPlugin& plugin, MonoImage* image) {
	const MonoTableInfo* typeDefinitionsTable = mono_image_get_table_info(image, MONO_TABLE_TYPEDEF);
	int numTypes = mono_table_info_get_rows(typeDefinitionsTable);

	for (int i = 0; i < numTypes; ++i) {
		uint32_t cols[MONO_TYPEDEF_SIZE];
		mono_metadata_decode_row(typeDefinitionsTable, i, cols, MONO_TYPEDEF_SIZE);

		const char* nameSpace = mono_metadata_string_heap(image, cols[MONO_TYPEDEF_NAMESPACE]);
		const char* className = mono_metadata_string_heap(image, cols[MONO_TYPEDEF_NAME]);

		MonoClass* monoClass = mono_class_from_name(image, nameSpace, className);
		if (monoClass == _plugin.klass)
			continue;

		bool isPlugin = mono_class_is_subclass_of(monoClass, _plugin.klass, false);
		if (!isPlugin)
			continue;

		const auto [it, result] = _scripts.try_emplace(plugin.GetName(), ScriptInstance(plugin, image, monoClass));
		if (result)
			return std::get<ScriptInstance>(*it);
	}

	return std::nullopt;
}

ScriptOpt CSharpLanguageModule::FindScript(const std::string& name) {
	auto it = _scripts.find(name);
	if (it != _scripts.end())
		return std::get<ScriptInstance>(*it);
	return std::nullopt;
}

MonoDelegate* CSharpLanguageModule::CreateDelegate(void* func, const plugify::Method& method) {
	auto& delegateClasses = method.retType.type != ValueType::Void ? _funcClasses : _actionClasses;

	size_t paramCount = method.paramTypes.size();
	if (paramCount >= delegateClasses.size()) {
		_provider->Log(std::format(LOG_PREFIX "Function '{}' has too much arguments to create delegate", method.name), Severity::Error);
		return nullptr;
	}

	MonoClass* delegateClass = method.retType.type != ValueType::Void ? _funcClasses[paramCount] : _actionClasses[paramCount];

	if (utils::IsMethodPrimitive(method)) {
		return mono_ftnptr_to_delegate(delegateClass, func);
	} else {
		auto function = new plugify::Function(_rt);
		void* methodAddr = function->GetJitFunc(method, &ExternalCall, func);
		MonoDelegate* delegate = mono_ftnptr_to_delegate(delegateClass, methodAddr);
		mono_gc_reference_queue_add(_functionReferenceQueue, reinterpret_cast<MonoObject*>(delegate), reinterpret_cast<void*>(function));
		return delegate;
	}
}

MonoString* CSharpLanguageModule::CreateString(const std::string& source) const {
	return source.empty() ? mono_string_empty(_appDomain) : mono_string_new(_appDomain, source.c_str());
}

template<typename T>
MonoArray* CSharpLanguageModule::CreateArrayT(const std::vector<T>& source, MonoClass* klass) {
	MonoArray* array = CreateArray(klass, source.size());
	for (size_t i = 0; i < source.size(); ++i) {
		mono_array_set(array, T, i, source[i]);
	}
	return array;
}

template<typename T>
MonoObject* CSharpLanguageModule::CreateObject(T& source, const ClassInfo& info) {
	MonoObject* instance = InstantiateClass(info.klass);
	std::vector<void*> args;
	args.resize(source.data.size());
	for (auto& e : source.data) {
		args.push_back(&e);
	}
	mono_runtime_invoke(info.ctor, instance, args.data(), nullptr);
	return instance;
}

MonoArray* CSharpLanguageModule::CreateArray(MonoClass* klass, size_t count) const {
	return mono_array_new(_appDomain, klass, count);
}

MonoArray* CSharpLanguageModule::CreateStringArray(const std::vector<std::string>& source) const {
	MonoArray* array = CreateArray(mono_get_string_class(), source.size());
	for (size_t i = 0; i < source.size(); ++i) {
		mono_array_set(array, MonoString*, i, CreateString(source[i]));
	}
	return array;
}

MonoObject* CSharpLanguageModule::InstantiateClass(MonoClass* klass) const {
	MonoObject* instance = mono_object_new(_appDomain, klass);
	mono_runtime_object_init(instance);
	return instance;
}

void CSharpLanguageModule::HandleException(MonoObject* exc, void* /* userData*/) {
	if (!exc || !g_csharplm._provider)
		return;

	MonoClass* exceptionClass = mono_object_get_class(exc);

	std::string result(LOG_PREFIX "[Exception] ");

	std::string message = utils::GetStringProperty("Message", exceptionClass, exc);
	if (!message.empty()) {
		std::format_to(std::back_inserter(result), " | Message: {}", message);
	}

	std::string source = utils::GetStringProperty("Source", exceptionClass, exc);
	if (!source.empty()) {
		std::format_to(std::back_inserter(result), " | Source: {}", source);
	}

	std::string stackTrace = utils::GetStringProperty("StackTrace", exceptionClass, exc);
	if (!stackTrace.empty()) {
		std::format_to(std::back_inserter(result), " | StackTrace: {}", stackTrace);
	}

	std::string targetSite = utils::GetStringProperty("TargetSite", exceptionClass, exc);
	if (!targetSite.empty()) {
		std::format_to(std::back_inserter(result), " | TargetSite: {}", targetSite);
	}

	g_csharplm._provider->Log(result, Severity::Error);
}

void CSharpLanguageModule::OnLogCallback(const char* logDomain, const char* logLevel, const char* message, mono_bool fatal, void* /* userData*/) {
	if (!g_csharplm._provider)
		return;

	Severity severity = Severity::None;
	if (logLevel != nullptr) {
		switch (std::tolower(logLevel[0], std::locale{})) {
			case 'e': // "error"
				severity = Severity::Error;
				break;
			case 'c': // "critical"
				severity = Severity::Fatal;
				break;
			case 'w': // "warning"
				severity = Severity::Warning;
				break;
			case 'm': // "message"
				severity = Severity::Verbose;
				break;
			case 'i': // "info"
				severity = Severity::Info;
				break;
			case 'd': // "debug"
				severity = Severity::Debug;
				break;
			default:
				break;
		}
	}

	if (!logDomain || strlen(logDomain) == 0) {
		g_csharplm._provider->Log(std::format(LOG_PREFIX "{}", message), fatal ? Severity::Fatal : severity);
	} else {
		g_csharplm._provider->Log(std::format(LOG_PREFIX "[{}] {}", logDomain, message), fatal ? Severity::Fatal : severity);
	}
}

void CSharpLanguageModule::OnPrintCallback(const char* message, mono_bool /*isStdout*/) {
	if (g_csharplm._provider)
		g_csharplm._provider->Log(std::format(LOG_PREFIX "{}", message), Severity::Warning);
}

void CSharpLanguageModule::OnPrintErrorCallback(const char* message, mono_bool /*isStdout*/) {
	if (g_csharplm._provider)
		g_csharplm._provider->Log(std::format(LOG_PREFIX "{}", message), Severity::Error);
}

/*_________________________________________________*/

ScriptInstance::ScriptInstance(const IPlugin& plugin, MonoImage* image, MonoClass* klass) : _image{image}, _klass{klass} {
	_instance = g_csharplm.InstantiateClass(_klass);

	// Call Script (base) constructor
	{
		const auto& desc = plugin.GetDescriptor();
		auto id = plugin.GetId();
		std::vector<std::string> deps;
		deps.reserve(desc.dependencies.size());
		for (const auto& dependency : desc.dependencies) {
			deps.emplace_back(dependency.name);
		}
		std::array<void*, 8> args {
			&id,
			g_csharplm.CreateString(plugin.GetName()),
			g_csharplm.CreateString(plugin.GetFriendlyName()),
			g_csharplm.CreateString(desc.friendlyName),
			g_csharplm.CreateString(desc.versionName),
			g_csharplm.CreateString(desc.createdBy),
			g_csharplm.CreateString(desc.createdByURL),
			g_csharplm.CreateStringArray(deps),
		};
		mono_runtime_invoke(g_csharplm._plugin.ctor, _instance, args.data(), nullptr);
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
			CSharpLanguageModule::HandleException(exception, nullptr);
		}
	}
}

void ScriptInstance::InvokeOnEnd() const {
	if (_onEndMethod) {
		MonoObject* exception = nullptr;
		mono_runtime_invoke(_onEndMethod, _instance, nullptr, &exception);
		if (exception) {
			CSharpLanguageModule::HandleException(exception, nullptr);
		}
	}
}

namespace csharplm {
	CSharpLanguageModule g_csharplm;
}

plugify::ILanguageModule* GetLanguageModule() {
	return &csharplm::g_csharplm;
}