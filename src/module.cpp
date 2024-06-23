#include "module.h"
#include "glue.h"
#include "utils.h"

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

#include <cpptrace/cpptrace.hpp>
#include <glaze/glaze.hpp>

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

#define LOG_PREFIX "[MONOLM] "

#if MONOLM_PLATFORM_WINDOWS
#define PATH_SEPARATOR ";"
#else
#define PATH_SEPARATOR ":"
#endif

using namespace monolm;
using namespace plugify;

bool IsMethodPrimitive(const plugify::Method& method) {
	// char8 is exception among primitive types

	if (method.retType.type == ValueType::Char8 || (method.retType.type >= ValueType::LastPrimitive && method.retType.type < ValueType::FirstPOD))
		return false;

	for (const auto& param : method.paramTypes) {
		if (param.type == ValueType::Char8 || (param.type >= ValueType::LastPrimitive && method.retType.type < ValueType::FirstPOD))
			return false;
	}

	return true;
}

std::string monolm::MonoStringToUTF8(MonoString* string) {
	if (string == nullptr || mono_string_length(string) == 0)
		return {};
	MonoError error;
	char* utf8 = mono_string_to_utf8_checked(string, &error);
	if (!mono_error_ok(&error)) {
		g_monolm.GetProvider()->Log(std::format(LOG_PREFIX "Failed to convert MonoString* to UTF-8: ({}) {}.", mono_error_get_error_code(&error), mono_error_get_message(&error)), Severity::Debug);
		mono_error_cleanup(&error);
		return {};
	}
	std::string result(utf8);
	mono_free(utf8);
	return result;
}

template<typename T>
void monolm::MonoArrayToVector(MonoArray* array, std::vector<T>& dest) {
	auto length = mono_array_length(array);
	dest.resize(length);
	for (size_t i = 0; i < length; ++i) {
		if constexpr (std::is_same_v<T, std::string>) {
			MonoObject* element = mono_array_get(array, MonoObject*, i);
			if (element != nullptr)
				dest[i] = MonoStringToUTF8(reinterpret_cast<MonoString*>(element));
			else
				dest[i] = T{};
		} else if constexpr (std::is_same_v<T, char>) {
			dest[i] = static_cast<char>(mono_array_get(array, char16_t, i));
		} else {
			dest[i] = mono_array_get(array, T, i);
		}
	}
}

ValueType MonoTypeToValueType(const char* typeName) {
	static std::unordered_map<std::string, ValueType> valueTypeMap = {
			{ "System.Void", ValueType::Void },
			{ "System.Boolean", ValueType::Bool },
			{ "System.Char", ValueType::Char16 },
			{ "System.SByte", ValueType::Int8 },
			{ "System.Int16", ValueType::Int16 },
			{ "System.Int32", ValueType::Int32 },
			{ "System.Int64", ValueType::Int64 },
			{ "System.Byte", ValueType::UInt8 },
			{ "System.UInt16", ValueType::UInt16 },
			{ "System.UInt32", ValueType::UInt32 },
			{ "System.UInt64", ValueType::UInt64 },
			{ "System.IntPtr", ValueType::Pointer },
			{ "System.UIntPtr", ValueType::Pointer },
			{ "System.Single", ValueType::Float },
			{ "System.Double", ValueType::Double },
			{ "System.String", ValueType::String },
			{ "System.Boolean[]", ValueType::ArrayBool },
			{ "System.Char[]", ValueType::ArrayChar16 },
			{ "System.SByte[]", ValueType::ArrayInt8 },
			{ "System.Int16[]", ValueType::ArrayInt16 },
			{ "System.Int32[]", ValueType::ArrayInt32 },
			{ "System.Int64[]", ValueType::ArrayInt64 },
			{ "System.Byte[]", ValueType::ArrayUInt8 },
			{ "System.UInt16[]", ValueType::ArrayUInt16 },
			{ "System.UInt32[]", ValueType::ArrayUInt32 },
			{ "System.UInt64[]", ValueType::ArrayUInt64 },
			{ "System.IntPtr[]", ValueType::ArrayPointer },
			{ "System.UIntPtr[]", ValueType::ArrayPointer },
			{ "System.Single[]", ValueType::ArrayFloat },
			{ "System.Double[]", ValueType::ArrayDouble },
			{ "System.String[]", ValueType::ArrayString },

			{ "System.Boolean&", ValueType::Bool },
			{ "System.Char&", ValueType::Char16 },
			{ "System.SByte&", ValueType::Int8 },
			{ "System.Int16&", ValueType::Int16 },
			{ "System.Int32&", ValueType::Int32 },
			{ "System.Int64&", ValueType::Int64 },
			{ "System.Byte&", ValueType::UInt8 },
			{ "System.UInt16&", ValueType::UInt16 },
			{ "System.UInt32&", ValueType::UInt32 },
			{ "System.UInt64&", ValueType::UInt64 },
			{ "System.IntPtr&", ValueType::Pointer },
			{ "System.UIntPtr&", ValueType::Pointer },
			{ "System.Single&", ValueType::Float },
			{ "System.Double&", ValueType::Double },
			{ "System.String&", ValueType::String },
			{ "System.Boolean[]&", ValueType::ArrayBool },
			{ "System.Char[]&", ValueType::ArrayChar16 },
			{ "System.SByte[]&", ValueType::ArrayInt8 },
			{ "System.Int16[]&", ValueType::ArrayInt16 },
			{ "System.Int32[]&", ValueType::ArrayInt32 },
			{ "System.Int64[]&", ValueType::ArrayInt64 },
			{ "System.Byte[]&", ValueType::ArrayUInt8 },
			{ "System.UInt16[]&", ValueType::ArrayUInt16 },
			{ "System.UInt32[]&", ValueType::ArrayUInt32 },
			{ "System.UInt64[]&", ValueType::ArrayUInt64 },
			{ "System.IntPtr[]&", ValueType::ArrayPointer },
			{ "System.UIntPtr[]&", ValueType::ArrayPointer },
			{ "System.Single[]&", ValueType::ArrayFloat },
			{ "System.Double[]&", ValueType::ArrayDouble },
			{ "System.String[]&", ValueType::ArrayString },

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

			{ "Plugify.Vector2&", ValueType::Vector2 },
			{ "Plugify.Vector3&", ValueType::Vector3 },
			{ "Plugify.Vector4&", ValueType::Vector4 },
			{ "Plugify.Matrix4x4&", ValueType::Matrix4x4 },
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
	return MonoStringToUTF8(messageString);
}

MonoAssembly* LoadMonoAssembly(const fs::path& assemblyPath, bool loadPDB, MonoImageOpenStatus& status) {
	auto buffer = Utils::ReadBytes<char>(assemblyPath);
	MonoImage* image = mono_image_open_from_data_full(buffer.data(), static_cast<uint32_t>(buffer.size()), 1, &status, 0);

	if (status != MONO_IMAGE_OK)
		return nullptr;

	if (loadPDB) {
		fs::path pdbPath(assemblyPath);
		pdbPath.replace_extension(".pdb");

		auto bytes = Utils::ReadBytes<mono_byte>(pdbPath);
		mono_debug_open_image_from_memory(image, bytes.data(), static_cast<int>(bytes.size()));

		// If pdf not load ?
	}
	MonoAssembly* assembly = mono_assembly_load_from_full(image, assemblyPath.string().c_str(), &status, 0);
	mono_image_close(image);
	return assembly;
}

AssemblyInfo LoadCoreAssembly(std::vector<std::string>& errors, const fs::path& assemblyPath, bool loadPDB) {
	std::error_code error;

	if (!fs::exists(assemblyPath, error)) {
		errors.emplace_back(assemblyPath.string());
		return {};
	}

	MonoImageOpenStatus status = MONO_IMAGE_IMAGE_INVALID;

	MonoAssembly* assembly = LoadMonoAssembly(assemblyPath, loadPDB, status);
	if (!assembly) {
		errors.emplace_back(std::format("{} ({})", assemblyPath.filename().string(), mono_image_strerror(status)));
		return {};
	}

	MonoImage* image = mono_assembly_get_image(assembly);
	if (!image) {
		errors.emplace_back(std::format("{}::image", assemblyPath.filename().string()));
		return {};
	}

	return { assembly, image };
}

void LoadSystemClass(std::vector<MonoClass*>& storage, const char* name) {
	MonoClass* klass = mono_class_from_name(mono_get_corlib(), "System", name);
	if (klass != nullptr) storage.push_back(klass);
}

ClassInfo LoadCoreClass(std::vector<std::string>& errors, MonoImage* image, const char* name, int paramCount) {
	MonoClass* klass = mono_class_from_name(image, "Plugify", name);
	if (!klass) {
		errors.emplace_back(name);
		return {};
	}
	MonoMethod* ctor = mono_class_get_method_from_name(klass, ".ctor", paramCount);
	if (!ctor) {
		errors.emplace_back(std::format("{}::ctor", name));
		return {};
	}
	return { klass, ctor };
}

template<typename T>
void* AllocateMemory(ArgumentList& args) {
	void* ptr = malloc(sizeof(T));
	args.push_back(ptr);
	return ptr;
}

template<typename T>
void FreeMemory(void* ptr) {
	reinterpret_cast<T*>(ptr)->~T();
	free(ptr);
}

void FunctionRefQueueCallback(void* function) {
	delete reinterpret_cast<Function*>(function);
}

InitResult CSharpLanguageModule::Initialize(std::weak_ptr<IPlugifyProvider> provider, const IModule& module) {
	if (!(_provider = provider.lock()))
		return ErrorData{ "Provider not exposed" };

	const char* settingsFile = "configs/mono-lang-module.json";

	auto settingsPath = module.FindResource(settingsFile);
	if (!settingsPath.has_value())
		return ErrorData{ std::format("File '{}' has not been found", settingsFile) };

	auto json = Utils::ReadText(*settingsPath);
	auto settings = glz::read_json<MonoSettings>(json);
	if (!settings.has_value())
		return ErrorData{ std::format("File '{}' has JSON parsing error: {}", settingsFile, glz::format_error(settings.error(), json)) };
	_settings = std::move(*settings);

	fs::path monoPath(module.GetBaseDir() / "mono/");
	auto configPath = module.FindResource("configs/mono_config");

	if (!InitMono(monoPath, configPath))
		return ErrorData{ "Initialization of mono failed" };

	Glue::RegisterFunctions();

	_rt = std::make_shared<asmjit::JitRuntime>();

	// Create an app domain
	char appName[] = "PlugifyMonoRuntime";
	MonoDomain* appDomain = mono_domain_create_appdomain(appName, nullptr);
	if (!appDomain)
		return ErrorData{ "Initialization of PlugifyMonoRuntime domain failed" };

	mono_domain_set(appDomain, true);
	_appDomain = std::deleted_unique_ptr<MonoDomain>(appDomain, mono_domain_unload);

	std::vector<std::string> assemblyErrors;

	{
		fs::path assemblyPath(module.GetBaseDir() / "bin/Plugify.dll");

		_core = LoadCoreAssembly(assemblyErrors, assemblyPath, _settings.enableDebugging);

		if (!assemblyErrors.empty()) {
			std::string assemblies("Not found: " + assemblyErrors[0]);
			for (auto it = std::next(assemblyErrors.begin()); it != assemblyErrors.end(); ++it) {
				std::format_to(std::back_inserter(assemblies), ", {}", *it);
			}
			return ErrorData{ std::move(assemblies) };
		}
	}

	{
		_plugin = LoadCoreClass(assemblyErrors, _core.image, "Plugin", 9);
		//_vector2 = LoadCoreClass(assemblyErrors, _core.image, "Vector2", 2);
		//_vector3 = LoadCoreClass(assemblyErrors, _core.image, "Vector3", 3);
		//_vector4 = LoadCoreClass(assemblyErrors, _core.image, "Vector4", 4);
		//_matrix4x4 = LoadCoreClass(assemblyErrors, _core.image, "Matrix4x4", 16);

		if (!assemblyErrors.empty()) {
			std::string classes("Not found: " + assemblyErrors[0]);
			for (auto it = std::next(assemblyErrors.begin()); it != assemblyErrors.end(); ++it) {
				std::format_to(std::back_inserter(classes), ", {}", *it);
			}
			return ErrorData{ std::move(classes) };
		}
	}

	/// Delegates
	LoadSystemClass(_funcClasses, "Func`1");
	LoadSystemClass(_funcClasses, "Func`2");
	LoadSystemClass(_funcClasses, "Func`3");
	LoadSystemClass(_funcClasses, "Func`4");
	LoadSystemClass(_funcClasses, "Func`5");
	LoadSystemClass(_funcClasses, "Func`6");
	LoadSystemClass(_funcClasses, "Func`7");
	LoadSystemClass(_funcClasses, "Func`8");
	LoadSystemClass(_funcClasses, "Func`9");
	LoadSystemClass(_funcClasses, "Func`10");
	LoadSystemClass(_funcClasses, "Func`11");
	LoadSystemClass(_funcClasses, "Func`12");
	LoadSystemClass(_funcClasses, "Func`13");
	LoadSystemClass(_funcClasses, "Func`14");
	LoadSystemClass(_funcClasses, "Func`15");
	LoadSystemClass(_funcClasses, "Func`16");
	LoadSystemClass(_funcClasses, "Func`17");

	LoadSystemClass(_actionClasses, "Action");
	LoadSystemClass(_actionClasses, "Action`1");
	LoadSystemClass(_actionClasses, "Action`2");
	LoadSystemClass(_actionClasses, "Action`3");
	LoadSystemClass(_actionClasses, "Action`4");
	LoadSystemClass(_actionClasses, "Action`5");
	LoadSystemClass(_actionClasses, "Action`6");
	LoadSystemClass(_actionClasses, "Action`7");
	LoadSystemClass(_actionClasses, "Action`8");
	LoadSystemClass(_actionClasses, "Action`9");
	LoadSystemClass(_actionClasses, "Action`10");
	LoadSystemClass(_actionClasses, "Action`11");
	LoadSystemClass(_actionClasses, "Action`12");
	LoadSystemClass(_actionClasses, "Action`13");
	LoadSystemClass(_actionClasses, "Action`14");
	LoadSystemClass(_actionClasses, "Action`15");
	LoadSystemClass(_actionClasses, "Action`16");

	_functionReferenceQueue = std::deleted_unique_ptr<MonoReferenceQueue>(mono_gc_reference_queue_new(FunctionRefQueueCallback), mono_gc_reference_queue_free);

	// MonoAssemblyName is an incomplete type (internal to mono), so we can't allocate it ourselves.
	// There isn't any api to allocate an empty one either, so we need to do it this way.
	_assemblyName = std::deleted_unique_ptr<MonoAssemblyName>(mono_assembly_name_new("blank"), mono_free);
	mono_assembly_name_free(_assemblyName.get()); // "it does not frees the object itself, only the name members" (typo included)

	DCCallVM* vm = dcNewCallVM(4096);
	dcMode(vm, DC_CALL_C_DEFAULT);
	_callVirtMachine = std::deleted_unique_ptr<DCCallVM>(vm, dcFree);

	_provider->Log(LOG_PREFIX "Inited!", Severity::Debug);

	return InitResultData{};
}

void CSharpLanguageModule::Shutdown() {
	_functionReferenceQueue.reset();
	_assemblyName.reset();
	_cachedDelegates.clear();
	_funcClasses.clear();
	_actionClasses.clear();
	_importMethods.clear();
	_exportMethods.clear();
	_functions.clear();
	_methods.clear();
	_scripts.clear();
	_callVirtMachine.reset();
	_rt.reset();
	_provider.reset();

	ShutdownMono();
}

/*MonoAssembly* CSharpLanguageModule::OnMonoAssemblyPreloadHook(MonoAssemblyName* aname, char** assemblies_path, void* user_data) {
	return OnMonoAssemblyLoad(mono_assembly_name_get_name(aname));
}*/

bool CSharpLanguageModule::InitMono(const fs::path& monoPath, const std::optional<fs::path>& configPath) {
	mono_trace_set_print_handler(OnPrintCallback);
	mono_trace_set_printerr_handler(OnPrintErrorCallback);
	mono_trace_set_log_handler(OnLogCallback, nullptr);

	std::error_code error;
	std::string monoEnvPath(Utils::GetEnvVariable("MONO_PATH"));
	if (fs::exists(monoPath, error)) {
		for (const auto& entry : fs::directory_iterator(monoPath, error)) {
			if (entry.is_directory(error)) {
				fs::path path(entry.path());
				path.make_preferred();
				std::format_to(std::back_inserter(monoEnvPath), PATH_SEPARATOR "{}", path.string());
			}
		}
	}
	//SetEnvVariable("MONO_PATH", monoEnvPath.c_str());
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

	mono_config_parse(configPath.has_value() ? configPath->string().c_str() : nullptr);

	MonoDomain* rootDomain = mono_jit_init("PlugifyJITRuntime");
	if (!rootDomain)
		return false;

	_rootDomain = std::deleted_unique_ptr<MonoDomain>(rootDomain, mono_jit_cleanup);

	if (_settings.enableDebugging) {
		mono_debug_domain_create(_rootDomain.get());
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

	_appDomain.reset();
	_rootDomain.reset();

	_core = AssemblyInfo{};
}

template<typename T>
void* CSharpLanguageModule::MonoStructToArg(ArgumentList& args) {
	auto* dest = new T();
	args.push_back(dest);
	return dest;
}

template<typename T>
void* CSharpLanguageModule::MonoArrayToArg(MonoArray* source, ArgumentList& args) {
	auto* dest = new std::vector<T>();
	if (source != nullptr) {
		MonoArrayToVector(source, *dest);
	}
	args.push_back(dest);
	return dest;
}

void* CSharpLanguageModule::MonoStringToArg(MonoString* source, ArgumentList& args) {
	std::string* dest;
	if (source != nullptr) {
		MonoError error;
		char* cStr = mono_string_to_utf8_checked(source, &error);
		if (!mono_error_ok(&error)) {
			g_monolm.GetProvider()->Log(std::format(LOG_PREFIX "Failed to convert MonoString* to UTF-8: ({}) {}.", mono_error_get_error_code(&error), mono_error_get_message(&error)), Severity::Debug);
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

	if (IsMethodPrimitive(method)) {
		methodAddr = mono_delegate_to_ftnptr(source);
	} else {
		auto* function = new plugify::Function(_rt);
		methodAddr = function->GetJitFunc(method, &DelegateCall, source);
		mono_gc_reference_queue_add(_functionReferenceQueue.get(), reinterpret_cast<MonoObject*>(source), reinterpret_cast<void*>(function));
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

// Call from C# to C++
void CSharpLanguageModule::ExternalCall(const Method* method, void* addr, const Parameters* p, uint8_t count, const ReturnValue* ret) {
	// TODO: Does mutex here good choose ?
	std::scoped_lock<std::mutex> lock(g_monolm._mutex);
	ArgumentList args;

	DCCallVM* vm = g_monolm._callVirtMachine.get();
	dcReset(vm);

	bool hasRet = true;
	bool hasRefs = false;

	// Store parameters

	switch (method->retType.type) {
		/*case ValueType::Vector2:
			dcArgPointer(vm, AllocateMemory<plugify::Vector2>(args));
			break;
		case ValueType::Vector3:
			dcArgPointer(vm, AllocateMemory<plugify::Vector3>(args));
			break;
		case ValueType::Vector4:
			dcArgPointer(vm, AllocateMemory<plugify::Vector4>(args));
			break;
		case ValueType::Matrix4x4:
			dcArgPointer(vm, AllocateMemory<plugify::Matrix4x4>(args));
			break;*/
		// MonoString*
		case ValueType::String:
			dcArgPointer(vm, AllocateMemory<std::string>(args));
			break;
		// MonoArray*
		case ValueType::ArrayBool:
			dcArgPointer(vm, AllocateMemory<std::vector<bool>>(args));
			break;
		case ValueType::ArrayChar8:
			dcArgPointer(vm, AllocateMemory<std::vector<char>>(args));
			break;
		case ValueType::ArrayChar16:
			dcArgPointer(vm, AllocateMemory<std::vector<char16_t>>(args));
			break;
		case ValueType::ArrayInt8:
			dcArgPointer(vm, AllocateMemory<std::vector<int8_t>>(args));
			break;
		case ValueType::ArrayInt16:
			dcArgPointer(vm, AllocateMemory<std::vector<int16_t>>(args));
			break;
		case ValueType::ArrayInt32:
			dcArgPointer(vm, AllocateMemory<std::vector<int32_t>>(args));
			break;
		case ValueType::ArrayInt64:
			dcArgPointer(vm, AllocateMemory<std::vector<int64_t>>(args));
			break;
		case ValueType::ArrayUInt8:
			dcArgPointer(vm, AllocateMemory<std::vector<uint8_t>>(args));
			break;
		case ValueType::ArrayUInt16:
			dcArgPointer(vm, AllocateMemory<std::vector<uint16_t>>(args));
			break;
		case ValueType::ArrayUInt32:
			dcArgPointer(vm, AllocateMemory<std::vector<uint32_t>>(args));
			break;
		case ValueType::ArrayUInt64:
			dcArgPointer(vm, AllocateMemory<std::vector<uint64_t>>(args));
			break;
		case ValueType::ArrayPointer:
			dcArgPointer(vm, AllocateMemory<std::vector<uintptr_t>>(args));
			break;
		case ValueType::ArrayFloat:
			dcArgPointer(vm, AllocateMemory<std::vector<float>>(args));
			break;
		case ValueType::ArrayDouble:
			dcArgPointer(vm, AllocateMemory<std::vector<double>>(args));
			break;
		case ValueType::ArrayString:
			dcArgPointer(vm, AllocateMemory<std::vector<std::string>>(args));
			break;
		default:
			// Should not require storage
			hasRet = false;
			break;
	}

	for (uint8_t i = 0; i < count; ++i) {
		const auto& param = method->paramTypes[i];
		if (param.ref) {
			switch (param.type) {
				case ValueType::Invalid:
				case ValueType::Void:
					break;
				case ValueType::Bool:
					dcArgPointer(vm, p->GetArgument<bool*>(i));
					break;
				case ValueType::Char8:
					dcArgPointer(vm, p->GetArgument<char*>(i));
					break;
				case ValueType::Char16:
					dcArgPointer(vm, p->GetArgument<char16_t*>(i));
					break;
				case ValueType::Int8:
					dcArgPointer(vm, p->GetArgument<int8_t*>(i));
					break;
				case ValueType::Int16:
					dcArgPointer(vm, p->GetArgument<int16_t*>(i));
					break;
				case ValueType::Int32:
					dcArgPointer(vm, p->GetArgument<int32_t*>(i));
					break;
				case ValueType::Int64:
					dcArgPointer(vm, p->GetArgument<int64_t*>(i));
					break;
				case ValueType::UInt8:
					dcArgPointer(vm, p->GetArgument<uint8_t*>(i));
					break;
				case ValueType::UInt16:
					dcArgPointer(vm, p->GetArgument<uint16_t*>(i));
					break;
				case ValueType::UInt32:
					dcArgPointer(vm, p->GetArgument<uint32_t*>(i));
					break;
				case ValueType::UInt64:
					dcArgPointer(vm, p->GetArgument<uint64_t*>(i));
					break;
				case ValueType::Pointer:
					dcArgPointer(vm, p->GetArgument<void*>(i));
					break;
				case ValueType::Float:
					dcArgPointer(vm, p->GetArgument<float*>(i));
					break;
				case ValueType::Double:
					dcArgPointer(vm, p->GetArgument<double*>(i));
					break;
				case ValueType::Vector2:
					dcArgPointer(vm, p->GetArgument<Vector2*>(i));
					break;
				case ValueType::Vector3:
					dcArgPointer(vm, p->GetArgument<Vector3*>(i));
					break;
				case ValueType::Vector4:
					dcArgPointer(vm, p->GetArgument<Vector4*>(i));
					break;
				case ValueType::Matrix4x4:
					dcArgPointer(vm, p->GetArgument<Matrix4x4*>(i));
					break;
				// MonoDelegate*
				case ValueType::Function:
					dcArgPointer(vm, g_monolm.MonoDelegateToArg(*p->GetArgument<MonoDelegate**>(i), *param.prototype));
					break;
				// MonoString*
				case ValueType::String:
					dcArgPointer(vm, MonoStringToArg(*p->GetArgument<MonoString**>(i), args));
					break;
				// MonoArray*
				case ValueType::ArrayBool:
					dcArgPointer(vm, MonoArrayToArg<bool>(*p->GetArgument<MonoArray**>(i), args));
					break;
				case ValueType::ArrayChar8:
					dcArgPointer(vm, MonoArrayToArg<char>(*p->GetArgument<MonoArray**>(i), args));
					break;
				case ValueType::ArrayChar16:
					dcArgPointer(vm, MonoArrayToArg<char16_t>(*p->GetArgument<MonoArray**>(i), args));
					break;
				case ValueType::ArrayInt8:
					dcArgPointer(vm, MonoArrayToArg<int8_t>(*p->GetArgument<MonoArray**>(i), args));
					break;
				case ValueType::ArrayInt16:
					dcArgPointer(vm, MonoArrayToArg<int16_t>(*p->GetArgument<MonoArray**>(i), args));
					break;
				case ValueType::ArrayInt32:
					dcArgPointer(vm, MonoArrayToArg<int32_t>(*p->GetArgument<MonoArray**>(i), args));
					break;
				case ValueType::ArrayInt64:
					dcArgPointer(vm, MonoArrayToArg<int64_t>(*p->GetArgument<MonoArray**>(i), args));
					break;
				case ValueType::ArrayUInt8:
					dcArgPointer(vm, MonoArrayToArg<uint8_t>(*p->GetArgument<MonoArray**>(i), args));
					break;
				case ValueType::ArrayUInt16:
					dcArgPointer(vm, MonoArrayToArg<uint16_t>(*p->GetArgument<MonoArray**>(i), args));
					break;
				case ValueType::ArrayUInt32:
					dcArgPointer(vm, MonoArrayToArg<uint32_t>(*p->GetArgument<MonoArray**>(i), args));
					break;
				case ValueType::ArrayUInt64:
					dcArgPointer(vm, MonoArrayToArg<uint64_t>(*p->GetArgument<MonoArray**>(i), args));
					break;
				case ValueType::ArrayPointer:
					dcArgPointer(vm, MonoArrayToArg<uintptr_t>(*p->GetArgument<MonoArray**>(i), args));
					break;
				case ValueType::ArrayFloat:
					dcArgPointer(vm, MonoArrayToArg<float>(*p->GetArgument<MonoArray**>(i), args));
					break;
				case ValueType::ArrayDouble:
					dcArgPointer(vm, MonoArrayToArg<double>(*p->GetArgument<MonoArray**>(i), args));
					break;
				case ValueType::ArrayString:
					dcArgPointer(vm, MonoArrayToArg<std::string>(*p->GetArgument<MonoArray**>(i), args));
					break;
				default:
					std::puts("Unsupported types!\n");
					std::terminate();
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
					dcArgChar(vm, static_cast<char>(p->GetArgument<char16_t>(i)));
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
				case ValueType::Pointer:
					dcArgPointer(vm, p->GetArgument<void*>(i));
					break;
				case ValueType::Float:
					dcArgFloat(vm, p->GetArgument<float>(i));
					break;
				case ValueType::Double:
					dcArgDouble(vm, p->GetArgument<double>(i));
					break;
				case ValueType::Vector2:
					dcArgPointer(vm, p->GetArgument<Vector2*>(i));
					break;
				case ValueType::Vector3:
					dcArgPointer(vm, p->GetArgument<Vector3*>(i));
					break;
				case ValueType::Vector4:
					dcArgPointer(vm, p->GetArgument<Vector4*>(i));
					break;
				case ValueType::Matrix4x4:
					dcArgPointer(vm, p->GetArgument<Matrix4x4*>(i));
					break;
				// MonoDelegate*
				case ValueType::Function:
					dcArgPointer(vm, g_monolm.MonoDelegateToArg(p->GetArgument<MonoDelegate*>(i), *param.prototype));
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
				case ValueType::ArrayPointer:
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
					std::puts("Unsupported types!\n");
					std::terminate();
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
			char16_t val = static_cast<char16_t>(dcCallChar(vm, addr));
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
		case ValueType::Pointer: {
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
			ret->SetReturnPtr(g_monolm.CreateDelegate(val, *method->retType.prototype));
			break;
		}
		case ValueType::Vector2: {
			DCaggr* ag = dcNewAggr(2, sizeof(Vector2));
			for (int i = 0; i < 2; ++i)
				dcAggrField(ag, DC_SIGCHAR_FLOAT, static_cast<int>(sizeof(float) * i), 1);
			dcCloseAggr(ag);
			Vector2 source;
			dcCallAggr(vm, addr, ag, &source);
			ret->SetReturnPtr(source);
			dcFreeAggr(ag);
			break;
		}
#if MONOLM_PLATFORM_WINDOWS
		case ValueType::Vector3: {
			DCaggr* ag = dcNewAggr(3, sizeof(Vector3));
			for (int i = 0; i < 3; ++i)
				dcAggrField(ag, DC_SIGCHAR_FLOAT, static_cast<int>(sizeof(float) * i), 1);
			dcCloseAggr(ag);
			auto* dest = p->GetArgument<Vector3*>(0);
			dcCallAggr(vm, addr, ag, dest);
			ret->SetReturnPtr(dest);
			dcFreeAggr(ag);
			break;
		}
		case ValueType::Vector4: {
			DCaggr* ag = dcNewAggr(4, sizeof(Vector4));
			for (int i = 0; i < 4; ++i)
				dcAggrField(ag, DC_SIGCHAR_FLOAT, static_cast<int>(sizeof(float) * i), 1);
			dcCloseAggr(ag);
			auto* dest = p->GetArgument<Vector4*>(0);
			dcCallAggr(vm, addr, ag, dest);
			ret->SetReturnPtr(dest);
			dcFreeAggr(ag);
			break;
		}
#else
		case ValueType::Vector3: {
			DCaggr* ag = dcNewAggr(3, sizeof(Vector3));
			for (int i = 0; i < 3; ++i)
				dcAggrField(ag, DC_SIGCHAR_FLOAT, static_cast<int>(sizeof(float) * i), 1);
			dcCloseAggr(ag);
			Vector3 source;
			dcCallAggr(vm, addr, ag, &source);
			ret->SetReturnPtr(source);
			dcFreeAggr(ag);
			break;
		}
		case ValueType::Vector4: {
			DCaggr* ag = dcNewAggr(4, sizeof(Vector4));
			for (int i = 0; i < 4; ++i)
				dcAggrField(ag, DC_SIGCHAR_FLOAT, static_cast<int>(sizeof(float) * i), 1);
			dcCloseAggr(ag);
			Vector4 source;
			dcCallAggr(vm, addr, ag, &source);
			ret->SetReturnPtr(source);
			dcFreeAggr(ag);
		}
#endif
		case ValueType::Matrix4x4: {
			DCaggr* ag = dcNewAggr(16, sizeof(Matrix4x4));
			for (int i = 0; i < 16; ++i)
				dcAggrField(ag, DC_SIGCHAR_FLOAT, static_cast<int>(sizeof(float) * i), 1);
			dcCloseAggr(ag);
			auto* dest = p->GetArgument<Matrix4x4*>(0);
			dcCallAggr(vm, addr, ag, dest);
			ret->SetReturnPtr(dest);
			dcFreeAggr(ag);
			break;
		}
		case ValueType::String: {
			dcCallVoid(vm, addr);
			ret->SetReturnPtr(g_monolm.CreateString(*reinterpret_cast<std::string*>(args[0])));
			break;
		}
		case ValueType::ArrayBool: {
			dcCallVoid(vm, addr);
			ret->SetReturnPtr(g_monolm.CreateArrayT<bool>(*reinterpret_cast<std::vector<bool>*>(args[0]), mono_get_byte_class()));
			break;
		}
		case ValueType::ArrayChar8: {
			dcCallVoid(vm, addr);
			ret->SetReturnPtr(g_monolm.CreateArrayT<char>(*reinterpret_cast<std::vector<char>*>(args[0]), mono_get_char_class()));
			break;
		}
		case ValueType::ArrayChar16: {
			dcCallVoid(vm, addr);
			ret->SetReturnPtr(g_monolm.CreateArrayT<char16_t>(*reinterpret_cast<std::vector<char16_t>*>(args[0]), mono_get_char_class()));
			break;
		}
		case ValueType::ArrayInt8: {
			dcCallVoid(vm, addr);
			ret->SetReturnPtr(g_monolm.CreateArrayT<int8_t>(*reinterpret_cast<std::vector<int8_t>*>(args[0]), mono_get_sbyte_class()));
			break;
		}
		case ValueType::ArrayInt16: {
			dcCallVoid(vm, addr);
			ret->SetReturnPtr(g_monolm.CreateArrayT<int16_t>(*reinterpret_cast<std::vector<int16_t>*>(args[0]), mono_get_int16_class()));
			break;
		}
		case ValueType::ArrayInt32: {
			dcCallVoid(vm, addr);
			ret->SetReturnPtr(g_monolm.CreateArrayT<int32_t>(*reinterpret_cast<std::vector<int32_t>*>(args[0]), mono_get_int32_class()));
			break;
		}
		case ValueType::ArrayInt64: {
			dcCallVoid(vm, addr);
			ret->SetReturnPtr(g_monolm.CreateArrayT<int64_t>(*reinterpret_cast<std::vector<int64_t>*>(args[0]), mono_get_int64_class()));
			break;
		}
		case ValueType::ArrayUInt8: {
			dcCallVoid(vm, addr);
			ret->SetReturnPtr(g_monolm.CreateArrayT<uint8_t>(*reinterpret_cast<std::vector<uint8_t>*>(args[0]), mono_get_byte_class()));
			break;
		}
		case ValueType::ArrayUInt16: {
			dcCallVoid(vm, addr);
			ret->SetReturnPtr(g_monolm.CreateArrayT<uint16_t>(*reinterpret_cast<std::vector<uint16_t>*>(args[0]), mono_get_uint16_class()));
			break;
		}
		case ValueType::ArrayUInt32: {
			dcCallVoid(vm, addr);
			ret->SetReturnPtr(g_monolm.CreateArrayT<uint32_t>(*reinterpret_cast<std::vector<uint32_t>*>(args[0]), mono_get_uint32_class()));
			break;
		}
		case ValueType::ArrayUInt64: {
			dcCallVoid(vm, addr);
			ret->SetReturnPtr(g_monolm.CreateArrayT<uint64_t>(*reinterpret_cast<std::vector<uint64_t>*>(args[0]), mono_get_uint64_class()));
			break;
		}
		case ValueType::ArrayPointer: {
			dcCallVoid(vm, addr);
			ret->SetReturnPtr(g_monolm.CreateArrayT<uintptr_t>(*reinterpret_cast<std::vector<uintptr_t>*>(args[0]), mono_get_intptr_class()));
			break;
		}
		case ValueType::ArrayFloat: {
			dcCallVoid(vm, addr);
			ret->SetReturnPtr(g_monolm.CreateArrayT<float>(*reinterpret_cast<std::vector<float>*>(args[0]), mono_get_single_class()));
			break;
		}
		case ValueType::ArrayDouble: {
			dcCallVoid(vm, addr);
			ret->SetReturnPtr(g_monolm.CreateArrayT<double>(*reinterpret_cast<std::vector<double>*>(args[0]), mono_get_double_class()));
			break;
		}
		case ValueType::ArrayString: {
			dcCallVoid(vm, addr);
			ret->SetReturnPtr(g_monolm.CreateStringArray(*reinterpret_cast<std::vector<std::string>*>(args[0])));
			break;
		}
		default:
			std::puts("Unsupported types!\n");
			std::terminate();
			break;
	}

	// Pull back references into provided arguments

	PullReferences(method, p, count, hasRet, hasRefs, args);

	if (!args.empty()) {
		uint8_t j = 0;

		if (hasRet) {
			DeleteReturn(args, j, method->retType.type);
		}

		if (j < args.size()) {
			for (uint8_t i = 0; i < count; ++i) {
				DeleteParam(args, j, method->paramTypes[i].type);
				if (j == args.size())
					break;
			}
		}
	}
}

void CSharpLanguageModule::PullReferences(const Method* method, const Parameters* p, uint8_t count, bool hasRet, bool hasRefs, const ArgumentList& args) {
	if (hasRefs) {
		uint8_t j = hasRet; // skip first param if has return

		if (j < args.size()) {
			for (uint8_t i = 0; i < count; ++i) {
				const auto& param = method->paramTypes[i];
				if (param.ref) {
					switch (param.type) {
						case ValueType::String:
							p->SetArgumentAt(i, g_monolm.CreateString(*reinterpret_cast<std::string*>(args[j++])));
							break;
						case ValueType::ArrayBool:
							p->SetArgumentAt(i, g_monolm.CreateArrayT<bool>(*reinterpret_cast<std::vector<bool>*>(args[j++]), mono_get_byte_class()));
							break;
						case ValueType::ArrayChar8:
							p->SetArgumentAt(i, g_monolm.CreateArrayT<char>(*reinterpret_cast<std::vector<char>*>(args[j++]), mono_get_char_class()));
							break;
						case ValueType::ArrayChar16:
							p->SetArgumentAt(i, g_monolm.CreateArrayT<char16_t>(*reinterpret_cast<std::vector<char16_t>*>(args[j++]), mono_get_char_class()));
							break;
						case ValueType::ArrayInt8:
							p->SetArgumentAt(i, g_monolm.CreateArrayT<int8_t>(*reinterpret_cast<std::vector<int8_t>*>(args[j++]), mono_get_sbyte_class()));
							break;
						case ValueType::ArrayInt16:
							p->SetArgumentAt(i, g_monolm.CreateArrayT<int16_t>(*reinterpret_cast<std::vector<int16_t>*>(args[j++]), mono_get_int16_class()));
							break;
						case ValueType::ArrayInt32:
							p->SetArgumentAt(i, g_monolm.CreateArrayT<int32_t>(*reinterpret_cast<std::vector<int32_t>*>(args[j++]), mono_get_int32_class()));
							break;
						case ValueType::ArrayInt64:
							p->SetArgumentAt(i, g_monolm.CreateArrayT<int64_t>(*reinterpret_cast<std::vector<int64_t>*>(args[j++]), mono_get_int64_class()));
							break;
						case ValueType::ArrayUInt8:
							p->SetArgumentAt(i, g_monolm.CreateArrayT<uint8_t>(*reinterpret_cast<std::vector<uint8_t>*>(args[j++]), mono_get_byte_class()));
							break;
						case ValueType::ArrayUInt16:
							p->SetArgumentAt(i, g_monolm.CreateArrayT<uint16_t>(*reinterpret_cast<std::vector<uint16_t>*>(args[j++]), mono_get_uint16_class()));
							break;
						case ValueType::ArrayUInt32:
							p->SetArgumentAt(i, g_monolm.CreateArrayT<uint32_t>(*reinterpret_cast<std::vector<uint32_t>*>(args[j++]), mono_get_uint32_class()));
							break;
						case ValueType::ArrayUInt64:
							p->SetArgumentAt(i, g_monolm.CreateArrayT<uint64_t>(*reinterpret_cast<std::vector<uint64_t>*>(args[j++]), mono_get_uint64_class()));
							break;
						case ValueType::ArrayPointer:
							p->SetArgumentAt(i, g_monolm.CreateArrayT<uintptr_t>(*reinterpret_cast<std::vector<uintptr_t>*>(args[j++]), mono_get_intptr_class()));
							break;
						case ValueType::ArrayFloat:
							p->SetArgumentAt(i, g_monolm.CreateArrayT<float>(*reinterpret_cast<std::vector<float>*>(args[j++]), mono_get_single_class()));
							break;
						case ValueType::ArrayDouble:
							p->SetArgumentAt(i, g_monolm.CreateArrayT<double>(*reinterpret_cast<std::vector<double>*>(args[j++]), mono_get_double_class()));
							break;
						case ValueType::ArrayString:
							p->SetArgumentAt(i, g_monolm.CreateStringArray(*reinterpret_cast<std::vector<std::string>*>(args[j++])));
							break;
						default:
							break;
					}
				}
				if (j == args.size())
					break;
			}
		}
	}
}

void CSharpLanguageModule::DeleteParam(const ArgumentList& args, uint8_t& i, ValueType type) {
	switch (type) {
		case ValueType::String:
			delete reinterpret_cast<std::string*>(args[i++]);
			break;
		case ValueType::ArrayBool:
			delete reinterpret_cast<std::vector<bool>*>(args[i++]);
			break;
		case ValueType::ArrayChar8:
			delete reinterpret_cast<std::vector<char>*>(args[i++]);
			break;
		case ValueType::ArrayChar16:
			delete reinterpret_cast<std::vector<char16_t>*>(args[i++]);
			break;
		case ValueType::ArrayInt8:
			delete reinterpret_cast<std::vector<int16_t>*>(args[i++]);
			break;
		case ValueType::ArrayInt16:
			delete reinterpret_cast<std::vector<int16_t>*>(args[i++]);
			break;
		case ValueType::ArrayInt32:
			delete reinterpret_cast<std::vector<int32_t>*>(args[i++]);
			break;
		case ValueType::ArrayInt64:
			delete reinterpret_cast<std::vector<int64_t>*>(args[i++]);
			break;
		case ValueType::ArrayUInt8:
			delete reinterpret_cast<std::vector<uint8_t>*>(args[i++]);
			break;
		case ValueType::ArrayUInt16:
			delete reinterpret_cast<std::vector<uint16_t>*>(args[i++]);
			break;
		case ValueType::ArrayUInt32:
			delete reinterpret_cast<std::vector<uint32_t>*>(args[i++]);
			break;
		case ValueType::ArrayUInt64:
			delete reinterpret_cast<std::vector<uint64_t>*>(args[i++]);
			break;
		case ValueType::ArrayPointer:
			delete reinterpret_cast<std::vector<uintptr_t>*>(args[i++]);
			break;
		case ValueType::ArrayFloat:
			delete reinterpret_cast<std::vector<float>*>(args[i++]);
			break;
		case ValueType::ArrayDouble:
			delete reinterpret_cast<std::vector<double>*>(args[i++]);
			break;
		case ValueType::ArrayString:
			delete reinterpret_cast<std::vector<std::string>*>(args[i++]);
			break;
		default:
			break;
	}
}

void CSharpLanguageModule::DeleteReturn(const ArgumentList& args, uint8_t& i, ValueType type) {
	switch (type) {
		/*case ValueType::Vector2:
			FreeMemory<plugify::Vector2>(args[i++]);
			break;
		case ValueType::Vector3:
			FreeMemory<plugify::Vector3>(args[i++]);
			break;
		case ValueType::Vector4:
			FreeMemory<plugify::Vector4>(args[i++]);
			break;
		case ValueType::Matrix4x4:
			FreeMemory<plugify::Matrix4x4>(args[i++]);
			break;*/
		case ValueType::String:
			FreeMemory<std::string>(args[i++]);
			break;
		case ValueType::ArrayBool:
			FreeMemory<std::vector<bool>>(args[i++]);
			break;
		case ValueType::ArrayChar8:
			FreeMemory<std::vector<char>>(args[i++]);
			break;
		case ValueType::ArrayChar16:
			FreeMemory<std::vector<char16_t>>(args[i++]);
			break;
		case ValueType::ArrayInt8:
			FreeMemory<std::vector<int16_t>>(args[i++]);
			break;
		case ValueType::ArrayInt16:
			FreeMemory<std::vector<int16_t>>(args[i++]);
			break;
		case ValueType::ArrayInt32:
			FreeMemory<std::vector<int32_t>>(args[i++]);
			break;
		case ValueType::ArrayInt64:
			FreeMemory<std::vector<int64_t>>(args[i++]);
			break;
		case ValueType::ArrayUInt8:
			FreeMemory<std::vector<uint8_t>>(args[i++]);
			break;
		case ValueType::ArrayUInt16:
			FreeMemory<std::vector<uint16_t>>(args[i++]);
			break;
		case ValueType::ArrayUInt32:
			FreeMemory<std::vector<uint32_t>>(args[i++]);
			break;
		case ValueType::ArrayUInt64:
			FreeMemory<std::vector<uint64_t>>(args[i++]);
			break;
		case ValueType::ArrayPointer:
			FreeMemory<std::vector<uintptr_t>>(args[i++]);
			break;
		case ValueType::ArrayFloat:
			FreeMemory<std::vector<float>>(args[i++]);
			break;
		case ValueType::ArrayDouble:
			FreeMemory<std::vector<double>>(args[i++]);
			break;
		case ValueType::ArrayString:
			FreeMemory<std::vector<std::string>>(args[i++]);
			break;
		default:
			break;
	}
}

// Call from C++ to C#
void CSharpLanguageModule::InternalCall(const Method* method, void* data, const Parameters* p, uint8_t count, const ReturnValue* ret) {
	const auto& [monoMethod, monoObject] = *reinterpret_cast<ExportMethod*>(data);

	/// We not create param vector, and use Parameters* params directly if passing primitives
	bool hasRefs = false;
	bool hasRet = ValueTypeIsHiddenObjectParam(method->retType.type);

	ArgumentList args(hasRet ? count - 1 : count);

	SetParams(method, p, count, hasRet, hasRefs, args);

	MonoObject* exception = nullptr;
	MonoObject* result = mono_runtime_invoke(monoMethod, monoObject, args.data(), &exception);
	if (exception) {
		HandleException(exception, nullptr);
		ret->SetReturnPtr(uintptr_t{});
		return;
	}

	SetReferences(method, p, count, hasRet, hasRefs, args);

	SetReturn(method, p, ret, result);
}

// Call from C++ to C#
void CSharpLanguageModule::DelegateCall(const Method* method, void* data, const Parameters* p, uint8_t count, const ReturnValue* ret) {
	auto* monoDelegate = reinterpret_cast<MonoObject*>(data);

	/// We not create param vector, and use Parameters* params directly if passing primitives
	bool hasRefs = false;
	bool hasRet = ValueTypeIsHiddenObjectParam(method->retType.type);

	ArgumentList args(hasRet ? count - 1 : count);

	SetParams(method, p, count, hasRet, hasRefs, args);

	MonoObject* exception = nullptr;
	MonoObject* result = mono_runtime_delegate_invoke(monoDelegate, args.data(), &exception);
	if (exception) {
		HandleException(exception, nullptr);
		ret->SetReturnPtr(uintptr_t{});
		return;
	}

	SetReferences(method, p, count, hasRet, hasRefs, args);

	SetReturn(method, p, ret, result);
}

void CSharpLanguageModule::SetParams(const Method* method, const Parameters* p, uint8_t count, bool hasRet, bool& hasRefs, ArgumentList& args) {
	for (uint8_t i = hasRet, j = 0; i < count; ++i) {
		const auto& param = method->paramTypes[j];
		if (param.ref) {
			switch (param.type) {
				case ValueType::Invalid:
				case ValueType::Void:
					// Should not trigger as we cannot export function with char8 from c#
					break;
				case ValueType::Bool:
					args[j] = p->GetArgument<bool*>(i);
					break;
				case ValueType::Char8:
					args[j] = new char16_t(static_cast<char16_t>(*p->GetArgument<char*>(i)));
					break;
				case ValueType::Char16:
					args[j] = p->GetArgument<char16_t*>(i);
					break;
				case ValueType::Int8:
					args[j] = p->GetArgument<int8_t*>(i);
					break;
				case ValueType::Int16:
					args[j] = p->GetArgument<int16_t*>(i);
					break;
				case ValueType::Int32:
					args[j] = p->GetArgument<int32_t*>(i);
					break;
				case ValueType::Int64:
					args[j] = p->GetArgument<int64_t*>(i);
					break;
				case ValueType::UInt8:
					args[j] = p->GetArgument<uint8_t*>(i);
					break;
				case ValueType::UInt16:
					args[j] = p->GetArgument<uint16_t*>(i);
					break;
				case ValueType::UInt32:
					args[j] = p->GetArgument<uint32_t*>(i);
					break;
				case ValueType::UInt64:
					args[j] = p->GetArgument<uint64_t*>(i);
					break;
				case ValueType::Pointer:
					args[j] = p->GetArgument<void**>(i);
					break;
				case ValueType::Float:
					args[j] = p->GetArgument<float*>(i);
					break;
				case ValueType::Double:
					args[j] = p->GetArgument<double*>(i);
					break;
				case ValueType::Vector2:
					args[j] = p->GetArgument<Vector2*>(i);
					break;
				case ValueType::Vector3:
					args[j] = p->GetArgument<Vector3*>(i);
					break;
				case ValueType::Vector4:
					args[j] = p->GetArgument<Vector4*>(i);
					break;
				case ValueType::Matrix4x4:
					args[j] = p->GetArgument<Matrix4x4*>(i);
					break;
				case ValueType::Function:
					args[j] = g_monolm.CreateDelegate(p->GetArgument<void*>(i), *param.prototype);
					break;
				case ValueType::String:
					args[j] = new MonoString*[1]{ g_monolm.CreateString(*p->GetArgument<std::string*>(i)) };
					break;
				case ValueType::ArrayBool:
					args[j] = new MonoArray*[1]{ g_monolm.CreateArrayT<bool>(*p->GetArgument<std::vector<bool>*>(i), mono_get_byte_class()) };
					break;
				case ValueType::ArrayChar8:
					args[j] = new MonoArray*[1]{ g_monolm.CreateArrayT<char>(*p->GetArgument<std::vector<char>*>(i), mono_get_char_class()) };
				 	break;
				case ValueType::ArrayChar16:
					args[j] = new MonoArray*[1]{ g_monolm.CreateArrayT<char16_t>(*p->GetArgument<std::vector<char16_t>*>(i), mono_get_char_class()) };
					break;
				case ValueType::ArrayInt8:
					args[j] = new MonoArray*[1]{ g_monolm.CreateArrayT<int8_t>(*p->GetArgument<std::vector<int8_t>*>(i), mono_get_sbyte_class()) };
					break;
				case ValueType::ArrayInt16:
					args[j] = new MonoArray*[1]{ g_monolm.CreateArrayT<int16_t>(*p->GetArgument<std::vector<int16_t>*>(i), mono_get_int16_class()) };
					break;
				case ValueType::ArrayInt32:
					args[j] = new MonoArray*[1]{ g_monolm.CreateArrayT<int32_t>(*p->GetArgument<std::vector<int32_t>*>(i), mono_get_int32_class()) };
					break;
				case ValueType::ArrayInt64:
					args[j] = new MonoArray*[1]{ g_monolm.CreateArrayT<int64_t>(*p->GetArgument<std::vector<int64_t>*>(i), mono_get_int64_class()) };
					break;
				case ValueType::ArrayUInt8:
					args[j] = new MonoArray*[1]{ g_monolm.CreateArrayT<uint8_t>(*p->GetArgument<std::vector<uint8_t>*>(i), mono_get_byte_class()) };
					break;
				case ValueType::ArrayUInt16:
					args[j] = new MonoArray*[1]{ g_monolm.CreateArrayT<uint16_t>(*p->GetArgument<std::vector<uint16_t>*>(i), mono_get_uint16_class()) };
					break;
				case ValueType::ArrayUInt32:
					args[j] = new MonoArray*[1]{ g_monolm.CreateArrayT<uint32_t>(*p->GetArgument<std::vector<uint32_t>*>(i), mono_get_uint32_class()) };
					break;
				case ValueType::ArrayUInt64:
					args[j] = new MonoArray*[1]{ g_monolm.CreateArrayT<uint64_t>(*p->GetArgument<std::vector<uint64_t>*>(i), mono_get_uint64_class()) };
					break;
				case ValueType::ArrayPointer:
					args[j] = new MonoArray*[1]{ g_monolm.CreateArrayT<uintptr_t>(*p->GetArgument<std::vector<uintptr_t>*>(i), mono_get_intptr_class()) };
					break;
				case ValueType::ArrayFloat:
					args[j] = new MonoArray*[1]{ g_monolm.CreateArrayT<float>(*p->GetArgument<std::vector<float>*>(i), mono_get_single_class()) };
					break;
				case ValueType::ArrayDouble:
					args[j] = new MonoArray*[1]{ g_monolm.CreateArrayT<double>(*p->GetArgument<std::vector<double>*>(i), mono_get_double_class()) };
					break;
				case ValueType::ArrayString:
					args[j] = new MonoArray*[1]{ g_monolm.CreateStringArray(*p->GetArgument<std::vector<std::string>*>(i)) };
					break;
			}
			hasRefs |= true;
		} else {
			switch (param.type) {
				case ValueType::Invalid:
				case ValueType::Void:
					// Should not trigger as we cannot export function with char8 from c#
					break;
				case ValueType::Bool:
				case ValueType::Char16:
				case ValueType::Int8:
				case ValueType::Int16:
				case ValueType::Int32:
				case ValueType::Int64:
				case ValueType::UInt8:
				case ValueType::UInt16:
				case ValueType::UInt32:
				case ValueType::UInt64:
				case ValueType::Pointer:
				case ValueType::Float:
				case ValueType::Double:
					args[j] = p->GetArgumentPtr(i);
					break;
				case ValueType::Vector2:
					args[j] = p->GetArgument<Vector2*>(i);
					break;
				case ValueType::Vector3:
					args[j] = p->GetArgument<Vector3*>(i);
					break;
				case ValueType::Vector4:
					args[j] = p->GetArgument<Vector4*>(i);
					break;
				case ValueType::Matrix4x4:
					args[j] = p->GetArgument<Matrix4x4*>(i);
					break;
				case ValueType::Char8:
					args[j] = new char16_t(static_cast<char16_t>(p->GetArgument<char>(i)));
					break;
				case ValueType::Function:
					args[j] = g_monolm.CreateDelegate(p->GetArgument<void*>(i), *param.prototype);
					break;
				case ValueType::String:
					args[j] = g_monolm.CreateString(*p->GetArgument<std::string*>(i));
					break;
				case ValueType::ArrayBool:
					args[j] = g_monolm.CreateArrayT<bool>(*p->GetArgument<std::vector<bool>*>(i), mono_get_byte_class());
					break;
				case ValueType::ArrayChar8:
					args[j] = g_monolm.CreateArrayT<char>(*p->GetArgument<std::vector<char>*>(i), mono_get_char_class());
					break;
				case ValueType::ArrayChar16:
					args[j] = g_monolm.CreateArrayT<char16_t>(*p->GetArgument<std::vector<char16_t>*>(i), mono_get_char_class());
					break;
				case ValueType::ArrayInt8:
					args[j] = g_monolm.CreateArrayT<int8_t>(*p->GetArgument<std::vector<int8_t>*>(i), mono_get_sbyte_class());
					break;
				case ValueType::ArrayInt16:
					args[j] = g_monolm.CreateArrayT<int16_t>(*p->GetArgument<std::vector<int16_t>*>(i), mono_get_int16_class());
					break;
				case ValueType::ArrayInt32:
					args[j] = g_monolm.CreateArrayT<int32_t>(*p->GetArgument<std::vector<int32_t>*>(i), mono_get_int32_class());
					break;
				case ValueType::ArrayInt64:
					args[j] = g_monolm.CreateArrayT<int64_t>(*p->GetArgument<std::vector<int64_t>*>(i), mono_get_int64_class());
					break;
				case ValueType::ArrayUInt8:
					args[j] = g_monolm.CreateArrayT<uint8_t>(*p->GetArgument<std::vector<uint8_t>*>(i), mono_get_byte_class());
					break;
				case ValueType::ArrayUInt16:
					args[j] = g_monolm.CreateArrayT<uint16_t>(*p->GetArgument<std::vector<uint16_t>*>(i), mono_get_uint16_class());
					break;
				case ValueType::ArrayUInt32:
					args[j] = g_monolm.CreateArrayT<uint32_t>(*p->GetArgument<std::vector<uint32_t>*>(i), mono_get_uint32_class());
					break;
				case ValueType::ArrayUInt64:
					args[j] = g_monolm.CreateArrayT<uint64_t>(*p->GetArgument<std::vector<uint64_t>*>(i), mono_get_uint64_class());
					break;
				case ValueType::ArrayPointer:
					args[j] = g_monolm.CreateArrayT<uintptr_t>(*p->GetArgument<std::vector<uintptr_t>*>(i), mono_get_intptr_class());
					break;
				case ValueType::ArrayFloat:
					args[j] = g_monolm.CreateArrayT<float>(*p->GetArgument<std::vector<float>*>(i), mono_get_single_class());
					break;
				case ValueType::ArrayDouble:
					args[j] = g_monolm.CreateArrayT<double>(*p->GetArgument<std::vector<double>*>(i), mono_get_double_class());
					break;
				case ValueType::ArrayString:
					args[j] = g_monolm.CreateStringArray(*p->GetArgument<std::vector<std::string>*>(i));
					break;
			}
			hasRefs |= (param.type == ValueType::Char8);
		}
		++j;
	}
}

void CSharpLanguageModule::SetReferences(const Method* method, const Parameters* p, uint8_t count, bool hasRet, bool hasRefs, const ArgumentList& args) {
	if (hasRefs) {
		for (uint8_t i = hasRet, j = 0; i < count; ++i) {
			const auto& param = method->paramTypes[j];
			if (param.ref) {
				switch (param.type) {
					case ValueType::Char8: {
						auto* source = reinterpret_cast<char16_t*>(args[j]);
						auto* dest = p->GetArgument<char*>(i);
						*dest = static_cast<char>(*source);
						delete source;
						break;
					}
					case ValueType::String: {
						auto source = reinterpret_cast<MonoString**>(args[j]);
						if (source != nullptr)  {
							auto* dest = p->GetArgument<std::string*>(i);
							*dest = MonoStringToUTF8(source[0]);
						}
						delete[] source;
						break;
					}
					case ValueType::ArrayBool: {
						auto source = reinterpret_cast<MonoArray**>(args[j]);
						if (source != nullptr) {
							auto* dest = p->GetArgument<std::vector<bool>*>(i);
							MonoArrayToVector(source[0], *dest);
						}
						delete[] source;
						break;
					}
					case ValueType::ArrayChar8: {
						auto source = reinterpret_cast<MonoArray**>(args[j]);
						if (source != nullptr) {
							auto* dest = p->GetArgument<std::vector<char>*>(i);
							MonoArrayToVector(source[0], *dest);
						}
					 	delete[] source;
						break;
					}
					case ValueType::ArrayChar16: {
						auto source = reinterpret_cast<MonoArray**>(args[j]);
						if (source != nullptr) {
							auto* dest = p->GetArgument<std::vector<char16_t>*>(i);
							MonoArrayToVector(source[0], *dest);
						}
						delete[] source;
						break;
					}
					case ValueType::ArrayInt8: {
						auto source = reinterpret_cast<MonoArray**>(args[j]);
						if (source != nullptr) {
							auto* dest = p->GetArgument<std::vector<int8_t>*>(i);
							MonoArrayToVector(source[0], *dest);
						}
						delete[] source;
						break;
					}
					case ValueType::ArrayInt16: {
						auto source = reinterpret_cast<MonoArray**>(args[j]);
						if (source != nullptr) {
							auto* dest = p->GetArgument<std::vector<int16_t>*>(i);
							MonoArrayToVector(source[0], *dest);
						}
						delete[] source;
						break;
					}
					case ValueType::ArrayInt32: {
						auto source = reinterpret_cast<MonoArray**>(args[j]);
						if (source != nullptr) {
							auto* dest = p->GetArgument<std::vector<int32_t>*>(i);
							MonoArrayToVector(source[0], *dest);
						}
						delete[] source;
						break;
					}
					case ValueType::ArrayInt64: {
						auto source = reinterpret_cast<MonoArray**>(args[j]);
						if (source != nullptr) {
							auto* dest = p->GetArgument<std::vector<int64_t>*>(i);
							MonoArrayToVector(source[0], *dest);
						}
						delete[] source;
						break;
					}
					case ValueType::ArrayUInt8: {
						auto source = reinterpret_cast<MonoArray**>(args[j]);
						if (source != nullptr) {
							auto* dest = p->GetArgument<std::vector<uint8_t>*>(i);
							MonoArrayToVector(source[0], *dest);
						}
						delete[] source;
						break;
					}
					case ValueType::ArrayUInt16: {
						auto source = reinterpret_cast<MonoArray**>(args[j]);
						if (source != nullptr) {
							auto* dest = p->GetArgument<std::vector<uint16_t>*>(i);
							MonoArrayToVector(source[0], *dest);
						}
						delete[] source;
						break;
					}
					case ValueType::ArrayUInt32: {
						auto source = reinterpret_cast<MonoArray**>(args[j]);
						if (source != nullptr) {
							auto* dest = p->GetArgument<std::vector<uint32_t>*>(i);
							MonoArrayToVector(source[0], *dest);
						}
						delete[] source;
						break;
					}
					case ValueType::ArrayUInt64: {
						auto source = reinterpret_cast<MonoArray**>(args[j]);
						if (source != nullptr) {
							auto* dest = p->GetArgument<std::vector<uint64_t>*>(i);
							MonoArrayToVector(source[0], *dest);
						}
						delete[] source;
						break;
					}
					case ValueType::ArrayPointer: {
						auto source = reinterpret_cast<MonoArray**>(args[j]);
						if (source != nullptr) {
							auto* dest = p->GetArgument<std::vector<uintptr_t>*>(i);
							MonoArrayToVector(source[0], *dest);
						}
						delete[] source;
						break;
					}
					case ValueType::ArrayFloat: {
						auto source = reinterpret_cast<MonoArray**>(args[j]);
						if (source != nullptr) {
							auto* dest = p->GetArgument<std::vector<float>*>(i);
							MonoArrayToVector(source[0], *dest);
						}
						delete[] source;
						break;
					}
					case ValueType::ArrayDouble: {
						auto source = reinterpret_cast<MonoArray**>(args[j]);
						if (source != nullptr) {
							auto* dest = p->GetArgument<std::vector<double>*>(i);
							MonoArrayToVector(source[0], *dest);
						}
						delete[] source;
						break;
					}
					case ValueType::ArrayString: {
						auto source = reinterpret_cast<MonoArray**>(args[j]);
						if (source != nullptr) {
							auto* dest = p->GetArgument<std::vector<std::string>*>(i);
							MonoArrayToVector(source[0], *dest);
						}
						delete[] source;
						break;
					}
					default:
						break;
				}
			} else {
				if (param.type == ValueType::Char8) {
					delete reinterpret_cast<char16_t*>(args[j]);
				}
			}
			++j;
		}
	}
}

void CSharpLanguageModule::SetReturn(const Method* method, const Parameters* p, const ReturnValue* ret, MonoObject* result) {
	if (result) {
		switch (method->retType.type) {
			case ValueType::Invalid:
			case ValueType::Void:
				break;
			case ValueType::Bool: {
				bool val = *reinterpret_cast<bool*>(mono_object_unbox(result));
				ret->SetReturnPtr(val);
				break;
			}
			case ValueType::Char8: {
				char16_t val = *reinterpret_cast<char16_t*>(mono_object_unbox(result));
				ret->SetReturnPtr(static_cast<char>(val));
				break;
			}
			case ValueType::Char16: {
				char16_t val = *reinterpret_cast<char16_t*>(mono_object_unbox(result));
				ret->SetReturnPtr(val);
				break;
			}
			case ValueType::Int8: {
				int8_t val = *reinterpret_cast<int8_t*>(mono_object_unbox(result));
				ret->SetReturnPtr(val);
				break;
			}
			case ValueType::Int16: {
				int16_t val = *reinterpret_cast<int16_t*>(mono_object_unbox(result));
				ret->SetReturnPtr(val);
				break;
			}
			case ValueType::Int32: {
				int32_t val = *reinterpret_cast<int32_t*>(mono_object_unbox(result));
				ret->SetReturnPtr(val);
				break;
			}
			case ValueType::Int64: {
				int64_t val = *reinterpret_cast<int64_t*>(mono_object_unbox(result));
				ret->SetReturnPtr(val);
				break;
			}
			case ValueType::UInt8: {
				uint8_t val = *reinterpret_cast<uint8_t*>(mono_object_unbox(result));
				ret->SetReturnPtr(val);
				break;
			}
			case ValueType::UInt16: {
				uint16_t val = *reinterpret_cast<uint16_t*>(mono_object_unbox(result));
				ret->SetReturnPtr(val);
				break;
			}
			case ValueType::UInt32: {
				uint32_t val = *reinterpret_cast<uint32_t*>(mono_object_unbox(result));
				ret->SetReturnPtr(val);
				break;
			}
			case ValueType::UInt64: {
				uint64_t val = *reinterpret_cast<uint64_t*>(mono_object_unbox(result));
				ret->SetReturnPtr(val);
				break;
			}
			case ValueType::Pointer: {
				uintptr_t val = *reinterpret_cast<uintptr_t*>(mono_object_unbox(result));
				ret->SetReturnPtr(val);
				break;
			}
			case ValueType::Float: {
				float val = *reinterpret_cast<float*>(mono_object_unbox(result));
				ret->SetReturnPtr(val);
				break;
			}
			case ValueType::Double: {
				double val = *reinterpret_cast<double*>(mono_object_unbox(result));
				ret->SetReturnPtr(val);
				break;
			}
			case ValueType::Vector2: {
				Vector2 source = *reinterpret_cast<Vector2*>(mono_object_unbox(result));
				ret->SetReturnPtr(source);
				break;
			}
#if MONOLM_PLATFORM_WINDOWS
			case ValueType::Vector3: {
				auto* source = reinterpret_cast<Vector3*>(mono_object_unbox(result));
				auto* dest = p->GetArgument<Vector3*>(0);
				*dest = *source;
				ret->SetReturnPtr(dest);
				break;
			}
			case ValueType::Vector4: {
				auto* source = reinterpret_cast<Vector4*>(mono_object_unbox(result));
				auto* dest = p->GetArgument<Vector4*>(0);
				*dest = *source;
				ret->SetReturnPtr(dest);
				break;
			}
#else
			case ValueType::Vector3: {
				Vector3 source = *reinterpret_cast<Vector3*>(mono_object_unbox(result));
				ret->SetReturnPtr(source);
				break;
			}
			case ValueType::Vector4: {
				Vector4 source = *reinterpret_cast<Vector4*>(mono_object_unbox(result));
				ret->SetReturnPtr(source);
				break;
			}
#endif
			case ValueType::Matrix4x4: {
				auto* source = reinterpret_cast<Matrix4x4*>(mono_object_unbox(result));
				auto* dest = p->GetArgument<Matrix4x4*>(0);
				*dest = *source;
				ret->SetReturnPtr(dest);
				break;
			}
			case ValueType::Function: {
				auto* source = reinterpret_cast<MonoDelegate*>(result);
				ret->SetReturnPtr(g_monolm.MonoDelegateToArg(source, *(method->retType.prototype)));
				break;
			}
			case ValueType::String: {
				auto* source = reinterpret_cast<MonoString*>(result);
				auto* dest = p->GetArgument<std::string*>(0);
				std::construct_at(dest, MonoStringToUTF8(source));
				ret->SetReturnPtr(dest);
				break;
			}
			case ValueType::ArrayBool: {
				auto* source = reinterpret_cast<MonoArray*>(result);
				auto* dest = p->GetArgument<std::vector<bool>*>(0);
				std::vector<bool> storage;
				MonoArrayToVector(source, storage);
				std::construct_at(dest, std::move(storage));
				ret->SetReturnPtr(dest);
				break;
			}
			case ValueType::ArrayChar8: {
				auto* source = reinterpret_cast<MonoArray*>(result);
				auto* dest = p->GetArgument<std::vector<char>*>(0);
				std::vector<char> storage;
				MonoArrayToVector(source, storage);
				std::construct_at(dest, std::move(storage));
				ret->SetReturnPtr(dest);
				break;
			}
			case ValueType::ArrayChar16: {
				auto* source = reinterpret_cast<MonoArray*>(result);
				auto* dest = p->GetArgument<std::vector<char16_t>*>(0);
				std::vector<char16_t> storage;
				MonoArrayToVector(source, storage);
				std::construct_at(dest, std::move(storage));
				ret->SetReturnPtr(dest);
				break;
			}
			case ValueType::ArrayInt8: {
				auto* source = reinterpret_cast<MonoArray*>(result);
				auto* dest = p->GetArgument<std::vector<int8_t>*>(0);
				std::vector<int8_t> storage;
				MonoArrayToVector(source, storage);
				std::construct_at(dest, std::move(storage));
				ret->SetReturnPtr(dest);
				break;
			}
			case ValueType::ArrayInt16: {
				auto* source = reinterpret_cast<MonoArray*>(result);
				auto* dest = p->GetArgument<std::vector<int16_t>*>(0);
				std::vector<int16_t> storage;
				MonoArrayToVector(source, storage);
				std::construct_at(dest, std::move(storage));
				ret->SetReturnPtr(dest);
				break;
			}
			case ValueType::ArrayInt32: {
				auto* source = reinterpret_cast<MonoArray*>(result);
				auto* dest = p->GetArgument<std::vector<int32_t>*>(0);
				std::vector<int32_t> storage;
				MonoArrayToVector(source, storage);
				std::construct_at(dest, std::move(storage));
				ret->SetReturnPtr(dest);
				break;
			}
			case ValueType::ArrayInt64: {
				auto* source = reinterpret_cast<MonoArray*>(result);
				auto* dest = p->GetArgument<std::vector<int64_t>*>(0);
				std::vector<int64_t> storage;
				MonoArrayToVector(source, storage);
				std::construct_at(dest, std::move(storage));
				ret->SetReturnPtr(dest);
				break;
			}
			case ValueType::ArrayUInt8: {
				auto* source = reinterpret_cast<MonoArray*>(result);
				auto* dest = p->GetArgument<std::vector<uint8_t>*>(0);
				std::vector<uint8_t> storage;
				MonoArrayToVector(source, storage);
				std::construct_at(dest, std::move(storage));
				ret->SetReturnPtr(dest);
				break;
			}
			case ValueType::ArrayUInt16: {
				auto* source = reinterpret_cast<MonoArray*>(result);
				auto* dest = p->GetArgument<std::vector<uint16_t>*>(0);
				std::vector<uint16_t> storage;
				MonoArrayToVector(source, storage);
				std::construct_at(dest, std::move(storage));
				ret->SetReturnPtr(dest);
				break;
			}
			case ValueType::ArrayUInt32: {
				auto* source = reinterpret_cast<MonoArray*>(result);
				auto* dest = p->GetArgument<std::vector<uint32_t>*>(0);
				std::vector<uint32_t> storage;
				MonoArrayToVector(source, storage);
				std::construct_at(dest, std::move(storage));
				ret->SetReturnPtr(dest);
				break;
			}
			case ValueType::ArrayUInt64: {
				auto* source = reinterpret_cast<MonoArray*>(result);
				auto* dest = p->GetArgument<std::vector<uint64_t>*>(0);
				std::vector<uint64_t> storage;
				MonoArrayToVector(source, storage);
				std::construct_at(dest, std::move(storage));
				ret->SetReturnPtr(dest);
				break;
			}
			case ValueType::ArrayPointer: {
				auto* source = reinterpret_cast<MonoArray*>(result);
				auto* dest = p->GetArgument<std::vector<uintptr_t>*>(0);
				std::vector<uintptr_t> storage;
				MonoArrayToVector(source, storage);
				std::construct_at(dest, std::move(storage));
				ret->SetReturnPtr(dest);
				break;
			}
			case ValueType::ArrayFloat: {
				auto* source = reinterpret_cast<MonoArray*>(result);
				auto* dest = p->GetArgument<std::vector<float>*>(0);
				std::vector<float> storage;
				MonoArrayToVector(source, storage);
				std::construct_at(dest, std::move(storage));
				ret->SetReturnPtr(dest);
				break;
			}
			case ValueType::ArrayDouble: {
				auto* source = reinterpret_cast<MonoArray*>(result);
				auto* dest = p->GetArgument<std::vector<double>*>(0);
				std::vector<double> storage;
				MonoArrayToVector(source, storage);
				std::construct_at(dest, std::move(storage));
				ret->SetReturnPtr(dest);
				break;
			}
			case ValueType::ArrayString: {
				auto* source = reinterpret_cast<MonoArray*>(result);
				auto* dest = p->GetArgument<std::vector<std::string>*>(0);
				std::vector<std::string> storage;
				MonoArrayToVector(source, storage);
				std::construct_at(dest, std::move(storage));
				ret->SetReturnPtr(dest);
				break;
			}
		}
	} else {
		switch (method->retType.type) {
			case ValueType::Invalid:
			case ValueType::Void:
				break;
			default:
				ret->SetReturnPtr(uintptr_t{});
				break;
		}
	}
}

LoadResult CSharpLanguageModule::OnPluginLoad(const IPlugin& plugin) {
	MonoImageOpenStatus status = MONO_IMAGE_IMAGE_INVALID;

	fs::path assemblyPath(plugin.GetBaseDir() / plugin.GetDescriptor().entryPoint);

	MonoAssembly* assembly = LoadMonoAssembly(assemblyPath, _settings.enableDebugging, status);
	if (!assembly)
		return ErrorData{ std::format("Failed to load assembly: {}", mono_image_strerror(status)) };

	MonoImage* image = mono_assembly_get_image(assembly);
	if (!image)
		return ErrorData{ "Failed to load assembly image" };

	auto scriptRef = CreateScriptInstance(plugin, image);
	if (!scriptRef.has_value())
		return ErrorData{ "Failed to find 'Plugin' class implementation" };
	const auto& script = scriptRef->get();

	std::vector<std::string> methodErrors;

	const auto& exportedMethods = plugin.GetDescriptor().exportedMethods;
	std::vector<MethodData> methods;
	methods.reserve(exportedMethods.size());

	for (const auto& method : exportedMethods) {
		auto separated = Utils::Split(method.funcName, ".");
		if (separated.size() != 3) {
			methodErrors.emplace_back(std::format("Invalid function name: '{}'. Please provide name in that format: 'Namespace.Class.Method'", method.funcName));
			continue;
		}

		std::string nameSpace(separated[0]);
		std::string className(separated[1]);
		std::string methodName(separated[2]);

		MonoClass* monoClass = mono_class_from_name(image, nameSpace.c_str(), className.c_str());
		if (!monoClass) {
			methodErrors.emplace_back(std::format("Failed to find class '{}.{}'", nameSpace, className));
			continue;
		}

		MonoMethod* monoMethod = mono_class_get_method_from_name(monoClass, methodName.c_str(), -1);
		if (!monoMethod) {
			methodErrors.emplace_back(std::format("Failed to find method '{}.{}.{}'", nameSpace, className, methodName));
			continue;
		}

		MonoObject* monoInstance = monoClass == script._klass ? script._instance : nullptr;

		uint32_t methodFlags = mono_method_get_flags(monoMethod, nullptr);
		if (!(methodFlags & MONO_METHOD_ATTR_STATIC) && !monoInstance) {
			methodErrors.emplace_back(std::format("Method '{}.{}.{}' is not static", nameSpace, className, methodName));
			continue;
		}

		MonoMethodSignature* sig = mono_method_signature(monoMethod);

		uint32_t paramCount = mono_signature_get_param_count(sig);
		if (paramCount != method.paramTypes.size()) {
			methodErrors.emplace_back(std::format("Method '{}' has invalid parameter count {} when it should have {}", method.funcName, method.paramTypes.size(), paramCount));
			continue;
		}

		MonoType* returnType = mono_signature_get_return_type(sig);
		char* returnTypeName = mono_type_get_name(returnType);
		ValueType retType = MonoTypeToValueType(returnTypeName);

		if (retType == ValueType::Invalid) {
			MonoClass* returnClass = mono_class_from_mono_type(returnType);
			if (mono_class_is_delegate(returnClass)) {
				retType = ValueType::Function;
			}
		}

		if (retType == ValueType::Invalid) {
			methodErrors.emplace_back(std::format("Return of method '{}' not supported '{}'", method.funcName, returnTypeName));
			continue;
		}

		ValueType methodReturnType = method.retType.type;

		if (methodReturnType == ValueType::Char8 && retType == ValueType::Char16) {
			retType = ValueType::Char8;
		}

		if (retType != methodReturnType) {
			methodErrors.emplace_back(std::format("Method '{}' has invalid return type '{}' when it should have '{}'", method.funcName, ValueTypeToString(methodReturnType), ValueTypeToString(retType)));
			continue;
		}

		bool methodFail = false;

		size_t i = 0;
		void* iter = nullptr;
		while (MonoType* type = mono_signature_get_params(sig, &iter)) {
			char* paramTypeName = mono_type_get_name(type);
			ValueType paramType = MonoTypeToValueType(paramTypeName);

			if (paramType == ValueType::Invalid) {
				MonoClass* paramClass = mono_class_from_mono_type(type);
				if (mono_class_is_delegate(paramClass)) {
					paramType = ValueType::Function;
				}
			}

			if (paramType == ValueType::Invalid) {
				methodFail = true;
				methodErrors.emplace_back(std::format("Parameter at index '{}' of method '{}' not supported '{}'", i, method.funcName, paramTypeName));
				continue;
			}

			ValueType methodParamType = method.paramTypes[i].type;

			if (methodParamType == ValueType::Char8 && paramType == ValueType::Char16) {
				paramType = ValueType::Char8;
			}

			if (paramType != methodParamType) {
				methodFail = true;
				methodErrors.emplace_back(std::format("Method '{}' has invalid param type '{}' at index {} when it should have '{}'", method.funcName, ValueTypeToString(methodParamType), i, ValueTypeToString(paramType)));
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
				if (IsMethodPrimitive(method)) {
					mono_add_internal_call(funcName.c_str(), addr);
				} else {
					Function function(_rt);
					void* methodAddr = function.GetJitFunc(method, &ExternalCall, addr, [](ValueType type) { return type >= ValueType::HiddenParam; });
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
		const auto& script = scriptRef->get();

		script.InvokeOnStart();
	}
}

void CSharpLanguageModule::OnPluginEnd(const IPlugin& plugin) {
	ScriptOpt scriptRef = FindScript(plugin.GetName());
	if (scriptRef.has_value()) {
		const auto& script = scriptRef->get();

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
	const auto& delegateClasses = method.retType.type != ValueType::Void ? _funcClasses : _actionClasses;

	size_t paramCount = method.paramTypes.size();
	if (paramCount >= delegateClasses.size()) {
		_provider->Log(std::format(LOG_PREFIX "Function '{}' has too much arguments to create delegate", method.name), Severity::Error);
		return nullptr;
	}

	MonoClass* delegateClass = delegateClasses[paramCount];

	if (IsMethodPrimitive(method)) {
		return mono_ftnptr_to_delegate(delegateClass, func);
	} else {
		auto* function = new plugify::Function(_rt);
		void* methodAddr = function->GetJitFunc(method, &ExternalCall, func);
		MonoDelegate* delegate = mono_ftnptr_to_delegate(delegateClass, methodAddr);
		mono_gc_reference_queue_add(_functionReferenceQueue.get(), reinterpret_cast<MonoObject*>(delegate), reinterpret_cast<void*>(function));
		return delegate;
	}
}

template<typename T>
MonoString* CSharpLanguageModule::CreateString(const T& source) const {
	return source.empty() ? mono_string_empty(_appDomain.get()) : mono_string_new(_appDomain.get(), source.data());
}

template<typename T>
MonoArray* CSharpLanguageModule::CreateArrayT(const std::vector<T>& source, MonoClass* klass) {
	MonoArray* array = CreateArray(klass, source.size());
	for (size_t i = 0; i < source.size(); ++i) {
		if constexpr (std::is_same_v<T, char>) {
			mono_array_set(array, char16_t, i, static_cast<char16_t>(source[i]));
		} else {
			mono_array_set(array, T, i, source[i]);
		}
	}
	return array;
}

MonoArray* CSharpLanguageModule::CreateArray(MonoClass* klass, size_t count) const {
	return mono_array_new(_appDomain.get(), klass, count);
}

template<typename T>
MonoArray* CSharpLanguageModule::CreateStringArray(const std::vector<T>& source) const {
	MonoArray* array = CreateArray(mono_get_string_class(), source.size());
	for (size_t i = 0; i < source.size(); ++i) {
		mono_array_set(array, MonoString*, i, CreateString(source[i]));
	}
	return array;
}

MonoObject* CSharpLanguageModule::InstantiateClass(MonoClass* klass) const {
	MonoObject* instance = mono_object_new(_appDomain.get(), klass);
	mono_runtime_object_init(instance);
	return instance;
}

void CSharpLanguageModule::HandleException(MonoObject* exc, void* /* userData*/) {
	if (!exc || !g_monolm._provider)
		return;

	MonoClass* exceptionClass = mono_object_get_class(exc);

	std::string result(LOG_PREFIX "[Exception] ");

	std::string message = GetStringProperty("Message", exceptionClass, exc);
	if (!message.empty()) {
		std::format_to(std::back_inserter(result), " | Message: {}", message);
	}

	/*std::string source = GetStringProperty("Source", exceptionClass, exc);
	if (!source.empty()) {
		std::format_to(std::back_inserter(result), " | Source: {}", source);
	}*/

	std::string stackTrace = GetStringProperty("StackTrace", exceptionClass, exc);
	if (!stackTrace.empty()) {
		std::format_to(std::back_inserter(result), " | StackTrace: {}", stackTrace);
	}

	/*std::string targetSite = GetStringProperty("TargetSite", exceptionClass, exc);
	if (!targetSite.empty()) {
		std::format_to(std::back_inserter(result), " | TargetSite: {}", targetSite);
	}*/

	g_monolm._provider->Log(result, Severity::Error);

	std::stringstream stream;
	cpptrace::generate_trace().print(stream);
	g_monolm._provider->Log(stream.str(), Severity::Debug);
}

void CSharpLanguageModule::OnLogCallback(const char* logDomain, const char* logLevel, const char* message, mono_bool fatal, void* /* userData*/) {
	if (!g_monolm._provider)
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
		g_monolm._provider->Log(std::format(LOG_PREFIX "{}", message), fatal ? Severity::Fatal : severity);
	} else {
		g_monolm._provider->Log(std::format(LOG_PREFIX "[{}] {}", logDomain, message), fatal ? Severity::Fatal : severity);
	}

	if (fatal || severity <= Severity::Error) {
		std::stringstream stream;
		cpptrace::generate_trace().print(stream);
		g_monolm._provider->Log(stream.str(), Severity::Debug);
	}
}

void CSharpLanguageModule::OnPrintCallback(const char* message, mono_bool /*isStdout*/) {
	if (g_monolm._provider)
		g_monolm._provider->Log(std::format(LOG_PREFIX "{}", message), Severity::Warning);
}

void CSharpLanguageModule::OnPrintErrorCallback(const char* message, mono_bool /*isStdout*/) {
	if (g_monolm._provider)
		g_monolm._provider->Log(std::format(LOG_PREFIX "{}", message), Severity::Error);
}

/*_________________________________________________*/

ScriptInstance::ScriptInstance(const IPlugin& plugin, MonoImage* image, MonoClass* klass) : _image{image}, _klass{klass} {
	_instance = g_monolm.InstantiateClass(_klass);

	// Call Script (base) constructor
	{
		const auto& desc = plugin.GetDescriptor();
		auto id = plugin.GetId();
		std::vector<std::string_view> deps;
		deps.reserve(desc.dependencies.size());
		for (const auto& dependency : desc.dependencies) {
			deps.emplace_back(dependency.name);
		}
		std::array<void*, 9> args {
				&id,
				g_monolm.CreateString(plugin.GetName()),
				g_monolm.CreateString(plugin.GetFriendlyName()),
				g_monolm.CreateString(desc.description),
				g_monolm.CreateString(desc.versionName),
				g_monolm.CreateString(desc.createdBy),
				g_monolm.CreateString(desc.createdByURL),
				g_monolm.CreateString(plugin.GetBaseDir().string()),
				g_monolm.CreateStringArray(deps),
		};
		mono_runtime_invoke(g_monolm._plugin.ctor, _instance, args.data(), nullptr);
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

namespace monolm {
	CSharpLanguageModule g_monolm;
}

plugify::ILanguageModule* GetLanguageModule() {
	return &monolm::g_monolm;
}
