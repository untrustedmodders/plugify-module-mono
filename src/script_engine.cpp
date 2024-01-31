#include "script_engine.h"
#include "script_glue.h"
#include "module.h"

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
#include <asmjit/asmjit.h>

#include <wizard/module.h>
#include <wizard/plugin.h>
#include <wizard/log.h>
#include <wizard/plugin_descriptor.h>
#include <wizard/wizard_provider.h>

#include <glaze/glaze.hpp>

using namespace csharplm;
using namespace wizard;

namespace csharplm::utils {
	std::string ReadText(const fs::path& filepath) {
		std::ifstream istream(filepath, std::ios::binary);
		if (!istream.is_open())
			return {};
		istream.unsetf(std::ios::skipws);
		return { std::istreambuf_iterator<char>{istream}, std::istreambuf_iterator<char>{} };
	}

	template<typename T>
	bool ReadBytes(const fs::path& file, const std::function<void(std::span<T>)>& callback) {
		std::ifstream istream(file, std::ios::binary);
		if (!istream.is_open())
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

		if (status != MONO_IMAGE_OK)
			return nullptr;

		if (loadPDB) {
			fs::path pdbPath(assemblyPath);
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
		std::string str(cStr);
		mono_free(cStr);
		return str;
	}

	std::string GetStringProperty(const char* propertyName, MonoClass* classType, MonoObject* classObject) {
		MonoProperty* messageProperty = mono_class_get_property_from_name(classType, propertyName);
		MonoMethod* messageGetter = mono_property_get_get_method(messageProperty);
		MonoString* messageString = (MonoString*) mono_runtime_invoke(messageGetter, classObject, nullptr, nullptr);
		if (messageString != nullptr)
			return MonoStringToString(messageString);
		return {};
	}

	ValueType MonoTypeToValueType(const char* typeName) {
		static std::unordered_map<std::string_view, ValueType> valueTypeMap = {
			{ "System.Void", ValueType::Void },
			{ "System.Boolean", ValueType::Bool },
			{ "System.Char", ValueType::Char8 },
			{ "System.SByte", ValueType::Int8 },
			{ "System.Int16", ValueType::Int16 },
			{ "System.Int32", ValueType::Int32 },
			{ "System.Int64", ValueType::Int64 },
			{ "System.Byte", ValueType::Uint8 },
			{ "System.UInt16", ValueType::Uint16 },
			{ "System.UInt32", ValueType::Uint32 },
			{ "System.UInt64", ValueType::Uint64 },
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
			{ "System.Byte[]", ValueType::ArrayUint8 },
			{ "System.UInt16[]", ValueType::ArrayUint16 },
			{ "System.UInt32[]", ValueType::ArrayUint32 },
			{ "System.UInt64[]", ValueType::ArrayUint64 },
			{ "System.IntPtr[]", ValueType::ArrayPtr64 },
			{ "System.UIntPtr[]", ValueType::ArrayPtr64 },
			{ "System.Single[]", ValueType::ArrayFloat },
			{ "System.Double[]", ValueType::ArrayDouble },
			{ "System.String[]", ValueType::ArrayString },
		};
		auto it = valueTypeMap.find(typeName);
		if (it != valueTypeMap.end())
			return std::get<ValueType>(*it);
		return ValueType::Invalid;
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

	auto json = utils::ReadText(m.GetBaseDir() / "config.json");
	auto config = glz::read_json<MonoConfig>(json);
	if (!config.has_value())
		return ErrorData{ std::format("MonoConfig: 'config.json' has JSON parsing error: {}", glz::format_error(config.error(), json)) };
	_config = std::move(*config);

	if (!InitMono(m.GetBaseDir() / "mono/lib"))
		return ErrorData{ "Initialization of mono failed" };

	ScriptGlue::RegisterFunctions();

	_rt = std::make_shared<asmjit::JitRuntime>();

	// Create an app domain
	char appName[] = "WandMonoRuntime";
	_appDomain = mono_domain_create_appdomain(appName, nullptr);
	mono_domain_set(_appDomain, true);

	fs::path coreAssemblyPath{ m.GetBinariesDir() / "Wand.dll" };

	// Load a core assembly
	MonoImageOpenStatus status = MONO_IMAGE_IMAGE_INVALID;
	_coreAssembly = utils::LoadMonoAssembly(coreAssemblyPath, _config.enableDebugging, status);
	if (!_coreAssembly)
		return ErrorData{ std::format("Failed to load '{}' core assembly. Reason: {}", coreAssemblyPath.string(), mono_image_strerror(status)) };

	_coreImage = mono_assembly_get_image(_coreAssembly);
	if (!_coreImage)
		return ErrorData{ std::format("Failed to load '{}' core image.", coreAssemblyPath.string()) };

	// Retrieve and cache core classes/methods

	/// Plugin
	MonoClass* pluginClass = CacheCoreClass("Plugin");
	if (!pluginClass)
		return ErrorData{ std::format("Failed to find 'Plugin' core class! Check '{}' assembly!", coreAssemblyPath.string()) };

	MonoClass* subscribeAttribute = CacheCoreClass("SubscribeAttribute");
	if (!subscribeAttribute)
		return ErrorData{ std::format("Failed to find 'SubscribeAttribute' core class! Check '{}' assembly!", coreAssemblyPath.string()) };

	MonoMethod* pluginCtor = CacheCoreMethod(pluginClass, ".ctor", 8);
	if (!pluginCtor)
		return ErrorData{ std::format("Failed to find 'Plugin' .ctor method! Check '{}' assembly!", coreAssemblyPath.string()) };

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

/*MonoAssembly* ScriptEngine::OnMonoAssemblyPreloadHook(MonoAssemblyName* aname, char** assemblies_path, void* user_data) {
	return OnMonoAssemblyLoad(mono_assembly_name_get_name(aname));
}*/

bool ScriptEngine::InitMono(const fs::path& monoPath) {
	mono_trace_set_print_handler(OnPrintCallback);
	mono_trace_set_printerr_handler(OnPrintErrorCallback);
	mono_trace_set_log_handler(OnLogCallback, nullptr);

	mono_set_assemblies_path(monoPath.string().c_str());

	mono_config_parse(nullptr);

	// Seems we can write custom assembly loader here
	//mono_install_assembly_preload_hook(OnMonoAssemblyPreloadHook, nullptr);

	if (!_config.options.empty()) {
		std::vector<char*> options;
		options.reserve(_config.options.size());
		for (auto& opt : _config.options) {
			if (std::find(options.begin(), options.end(), opt.data()) == options.end()) {
				options.push_back(opt.data());
				if (opt.starts_with("--debugger"))
					_provider->Log(std::format("Mono debugger: {}", opt), Severity::Info);
			}
		}
		mono_jit_parse_options(static_cast<int>(options.size()), options.data());
	}

	if (!_config.level.empty())
		mono_trace_set_level_string(_config.level.c_str());
	if (!_config.mask.empty())
		mono_trace_set_mask_string(_config.mask.c_str());
	if (_config.enableDebugging)
		mono_debug_init(MONO_DEBUG_FORMAT_MONO);

	_rootDomain = mono_jit_init("WandJITRuntime");
	if (!_rootDomain)
		return false;

	if (_config.enableDebugging)
		mono_debug_domain_create(_rootDomain);

	mono_thread_set_main(mono_thread_current());
	
	mono_install_unhandled_exception_hook(HandleException, nullptr);
	//mono_set_crash_chaining(true);
	
	char* buildInfo = mono_get_runtime_build_info();
	_provider->Log(std::format("Mono: Runtime version: {}", buildInfo), Severity::Debug);
	mono_free(buildInfo);

	return true;
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

template<typename T, typename C>
MonoArray* CreateArrayT(std::span<const T> data, C& klass) {
	MonoArray* array = g_csharplm.CreateArray(klass(), data.size());
	for (size_t i = 0; i < data.size(); ++i) {
		mono_array_set(array, T, i, data[i]);
	}
	return array;
}

void ScriptEngine::MethodCall(const Method* method, const Parameters* p, const uint8_t count, const ReturnValue* ret) {
	const auto& exportMethods = g_csharplm._exportMethods;
	auto it = exportMethods.find(method->funcName);
	if (it == exportMethods.end()) {
		if (g_csharplm._provider)
			g_csharplm._provider->Log(std::format("Method '{}' not found!", method->funcName), Severity::Error);
		return;
	}

	/// We not create param vector, and use Parameters* params directly if passing primitives
	std::vector<void*> args;
	args.reserve(count);

	for (uint8_t i = 0; i < count; ++i) {
		auto& param = method->paramTypes[i];
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
			case ValueType::Uint8:
			case ValueType::Uint16:
			case ValueType::Uint32:
			case ValueType::Uint64:
			case ValueType::Ptr64:
			case ValueType::Float:
			case ValueType::Double:
			case ValueType::Function:
				args.push_back(p->GetArgumentPtr(i));
				break;
			case ValueType::String: {
				auto source = p->GetArgument<const char*>(i);
				args.push_back(source != nullptr ? g_csharplm.CreateString(source) : nullptr);
				break;
			}
			case ValueType::ArrayBool: {
				auto source = p->GetArgument<bool*>(i);
				auto size = p->GetArgument<int>(++i);
				args.push_back(source != nullptr || size <= 0 ? CreateArrayT<bool>(std::span(source, size), mono_get_char_class) : nullptr);
				break;
			}
			case ValueType::ArrayChar8: {
				auto source = p->GetArgument<char*>(i);
				auto size = p->GetArgument<int>(++i);
				args.push_back(source != nullptr || size <= 0 ? CreateArrayT<char>(std::span(source, size), mono_get_char_class) : nullptr);
				break;
			}
			case ValueType::ArrayChar16: {
				auto source = p->GetArgument<int16_t*>(i);
				auto size = p->GetArgument<int>(++i);
				args.push_back(source != nullptr || size <= 0 ? CreateArrayT<int16_t>(std::span(source, size), mono_get_int16_class) : nullptr);
				break;
			}
			case ValueType::ArrayInt8: {
				auto source = p->GetArgument<int8_t*>(i);
				auto size = p->GetArgument<int>(++i);
				args.push_back(source != nullptr || size <= 0 ? CreateArrayT<int8_t>(std::span(source, size), mono_get_sbyte_class) : nullptr);
				break;
			}
			case ValueType::ArrayInt16: {
				auto source = p->GetArgument<int16_t*>(i);
				auto size = p->GetArgument<int>(++i);
				args.push_back(source != nullptr || size <= 0 ? CreateArrayT<int16_t>(std::span(source, size), mono_get_int16_class) : nullptr);
				break;
			}
			case ValueType::ArrayInt32: {
				auto source = p->GetArgument<int32_t*>(i);
				auto size = p->GetArgument<int>(++i);
				args.push_back(source != nullptr || size <= 0 ? CreateArrayT<int32_t>(std::span(source, size), mono_get_int32_class) : nullptr);
				break;
			}
			case ValueType::ArrayInt64: {
				auto source = p->GetArgument<int64_t*>(i);
				auto size = p->GetArgument<int>(++i);
				args.push_back(source != nullptr || size <= 0 ? CreateArrayT<int64_t>(std::span(source, size), mono_get_int64_class) : nullptr);
				break;
			}
			case ValueType::ArrayUint8: {
				auto source = p->GetArgument<uint8_t*>(i);
				auto size = p->GetArgument<int>(++i);
				args.push_back(source != nullptr || size <= 0 ? CreateArrayT<uint8_t>(std::span(source, size), mono_get_byte_class) : nullptr);
				break;
			}
			case ValueType::ArrayUint16: {
				auto source = p->GetArgument<uint16_t*>(i);
				auto size = p->GetArgument<int>(++i);
				args.push_back(source != nullptr || size <= 0 ? CreateArrayT<uint16_t>(std::span(source, size), mono_get_uint16_class) : nullptr);
				break;
			}
			case ValueType::ArrayUint32: {
				auto source = p->GetArgument<uint32_t*>(i);
				auto size = p->GetArgument<int>(++i);
				args.push_back(source != nullptr || size <= 0 ? CreateArrayT<uint32_t>(std::span(source, size), mono_get_uint32_class) : nullptr);
				break;
			}
			case ValueType::ArrayUint64: {
				auto source = p->GetArgument<uint64_t*>(i);
				auto size = p->GetArgument<int>(++i);
				args.push_back(source != nullptr || size <= 0 ? CreateArrayT<uint64_t>(std::span(source, size), mono_get_uint64_class) : nullptr);
				break;
			}
			case ValueType::ArrayPtr64: {
				auto source = p->GetArgument<uintptr_t*>(i);
				auto size = p->GetArgument<int>(++i);
				args.push_back(source != nullptr || size <= 0 ? CreateArrayT<uintptr_t>(std::span(source, size), mono_get_uintptr_class) : nullptr);
				break;
			}
			case ValueType::ArrayFloat: {
				auto source = p->GetArgument<float*>(i);
				auto size = p->GetArgument<int>(++i);
				args.push_back(source != nullptr || size <= 0 ? CreateArrayT<float>(std::span(source, size), mono_get_single_class) : nullptr);
				break;
			}
			case ValueType::ArrayDouble: {
				auto source = p->GetArgument<double*>(i);
				auto size = p->GetArgument<int>(++i);
				args.push_back(source != nullptr || size <= 0 ? CreateArrayT<double>(std::span(source, size), mono_get_double_class) : nullptr);
				break;
			}
			case ValueType::ArrayString: {
				auto source = p->GetArgument<const char**>(i);
				auto size = p->GetArgument<int>(++i);
				args.push_back(source != nullptr || size <= 0 ? g_csharplm.CreateStringArray(std::span(source, size)) : nullptr);
				break;
			}
		}
	}

	MonoObject* exception = nullptr;
	MonoObject* result = mono_runtime_invoke(std::get<MonoMethod*>(*it), nullptr, args.data(), &exception);
	if (exception) {
		HandleException(exception, nullptr);
		return;
	}

	switch (method->retType.type) {
		case ValueType::Invalid:
		case ValueType::Void:
			break;
		case ValueType::Bool: {
			bool val = *(bool*) mono_object_unbox(result);
			ret->SetReturnPtr<bool>(val);
			break;
		}
		case ValueType::Char8: {
			char val = *(char*) mono_object_unbox(result);
			ret->SetReturnPtr<char>(val);
			break;
		}
		case ValueType::Char16: {
			wchar_t val = *(wchar_t*) mono_object_unbox(result);
			ret->SetReturnPtr<wchar_t>(val);
			break;
		}
		case ValueType::Int8: {
			int8_t val = *(int8_t*) mono_object_unbox(result);
			ret->SetReturnPtr<int8_t>(val);
			break;
		}
		case ValueType::Int16: {
			int16_t val = *(int16_t*) mono_object_unbox(result);
			ret->SetReturnPtr<int16_t>(val);
			break;
		}
		case ValueType::Int32: {
			int32_t val = *(int32_t*) mono_object_unbox(result);
			ret->SetReturnPtr<int32_t>(val);
			break;
		}
		case ValueType::Int64: {
			int64_t val = *(int64_t*) mono_object_unbox(result);
			ret->SetReturnPtr<int64_t>(val);
			break;
		}
		case ValueType::Uint8: {
			uint8_t val = *(uint8_t*) mono_object_unbox(result);
			ret->SetReturnPtr<uint8_t>(val);
			break;
		}
		case ValueType::Uint16: {
			uint16_t val = *(uint16_t*) mono_object_unbox(result);
			ret->SetReturnPtr<uint16_t>(val);
			break;
		}
		case ValueType::Uint32: {
			uint32_t val = *(uint32_t*) mono_object_unbox(result);
			ret->SetReturnPtr<uint32_t>(val);
			break;
		}
		case ValueType::Uint64: {
			uint64_t val = *(uint64_t*) mono_object_unbox(result);
			ret->SetReturnPtr<uint64_t>(val);
			break;
		}
		case ValueType::Function:
		case ValueType::Ptr64: {
			uintptr_t val = *(uintptr_t*) mono_object_unbox(result);
			ret->SetReturnPtr<uintptr_t>(val);
			break;
		}
		case ValueType::Float: {
			float val = *(float*) mono_object_unbox(result);
			ret->SetReturnPtr<float>(val);
			break;
		}
		case ValueType::Double: {
			double val = *(double*) mono_object_unbox(result);
			ret->SetReturnPtr<double>(val);
			break;
		}
		default: {
			// TODO: How return string and arrays ?

			break;
		}
	}
}

LoadResult ScriptEngine::OnPluginLoad(const IPlugin& plugin) {
	MonoImageOpenStatus status = MONO_IMAGE_IMAGE_INVALID;
	MonoAssembly* assembly = utils::LoadMonoAssembly(plugin.GetFilePath(), _config.enableDebugging, status);
	if (!assembly)
		return ErrorData{ std::format("Failed to load assembly: '{}'",mono_image_strerror(status)) };

	MonoImage* image = mono_assembly_get_image(assembly);
	if (!image)
		return ErrorData{ std::format("Failed to load assembly image") };

	auto scriptRef = CreateScriptInstance(plugin, image);
	if (!scriptRef.has_value())
		return ErrorData{ std::format("Failed to find 'Plugin' class implementation") };
	auto& script = scriptRef->get();

	std::vector<std::string> methodErrors;

	MonoClass* subscribeClass = FindCoreClass("SubscribeAttribute");

	MonoMethod* monoMethod;
	void* iter = nullptr;
	while ((monoMethod = mono_class_get_methods(script._klass, &iter))) {
		const char* className = mono_class_get_name(script._klass);
		const char* methodName = mono_method_get_name(monoMethod);
		MonoCustomAttrInfo* attributes = mono_custom_attrs_from_method(monoMethod);
		if (attributes) {
			for (int i = 0; i < attributes->num_attrs; ++i) {
				MonoCustomAttrEntry& entry = attributes->attrs[i];
				if (subscribeClass != mono_method_get_class(entry.ctor))
					continue;

				std::string methodToFind;

				MonoObject* instance = mono_custom_attrs_get_attr(attributes, subscribeClass);
				void* iterator = nullptr;
				while (MonoClassField* field = mono_class_get_fields(subscribeClass, &iterator)) {
					const char* fieldName = mono_field_get_name(field);
					MonoObject* fieldValue = mono_field_get_value_object(_appDomain, field, instance);
					if (!strcmp(fieldName, "_method")) {
						methodToFind = utils::MonoStringToString((MonoString*) fieldValue);
						break;
					}
				}

				auto it = _importMethods.find(methodToFind);
				if (it != _importMethods.end()) {
					const auto& [ref, addr] = it->second;
					Method method = ref.get();
					method.name = methodName;
					method.funcName = std::format("{}.{}.{}", plugin.GetName(), className, methodName);

					// VALIDATE METHOD

					Function function(_rt);
					void* methodAddr = function.GetJitFunc(method, MethodCall);
					if (!methodAddr) {
						methodErrors.emplace_back(std::format("Method JIT generation error: ", function.GetError()));
						continue;
					}
					_functions.emplace_back(std::move(function));

					const auto [_, result] = _exportMethods.try_emplace(method.funcName, monoMethod);
					if (!result) {
						methodErrors.emplace_back(std::format("Function name duplicate: ", method.funcName));
						continue;
					}

					using RegisterCallbackFn = void(*)(void*);
					auto func = reinterpret_cast<RegisterCallbackFn>(addr);
					func(methodAddr);
				} else {

				}
				break;
			}
			mono_custom_attrs_free(attributes);
		}
	}

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

		/*MonoMethod* */monoMethod = mono_class_get_method_from_name(monoClass, methodName.c_str(), -1);
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

		if (method.retType.type == ValueType::Function && returnType == ValueType::Ptr64) {
			returnType = ValueType::Function; // special case
		}

		if (returnType != method.retType.type) {
			methodErrors.emplace_back(std::format("Method '{}.{}::{}' has invalid return type '{}' when it should have '{}'", nameSpace, className, methodName, ValueTypeToString(method.retType.type), ValueTypeToString(returnType)));
			continue;
		}

		size_t i = 0;
		/*void* */iter = nullptr;
		while (MonoType* type = mono_signature_get_params(sig, &iter)) {
			char* paramTypeName = mono_type_get_name(type);
			ValueType paramType = utils::MonoTypeToValueType(paramTypeName);
			if (paramType == ValueType::Invalid) {
				methodErrors.emplace_back(std::format("Parameter at index '{}' of method '{}.{}::{}' not supported '{}'", i, nameSpace, className, methodName, paramTypeName));
				continue;
			}

			if (method.paramTypes[i].type == ValueType::Function && paramType == ValueType::Ptr64) {
				paramType = ValueType::Function; // special case
			}

			if (paramType != method.paramTypes[i].type) {
				methodErrors.emplace_back(std::format("Method '{}.{}::{}' has invalid param type '{}' at index {} when it should have '{}'", nameSpace, className, methodName, ValueTypeToString(method.paramTypes[i].type), i, ValueTypeToString(paramType)));
				continue;
			}

			i++;
		}

		const auto [_, result] = _exportMethods.try_emplace(method.funcName, monoMethod);
		if (!result) {
			methodErrors.emplace_back(std::format("Function name duplicate: ", method.funcName));
			continue;
		}

		Function function(_rt);
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
		auto funcName = std::format("{}.{}::{}", plugin.GetName(), plugin.GetName(), name);

		if (_importMethods.contains(funcName)) {
			_provider->Log(std::format("Method name duplicate: {}", funcName), Severity::Error);
			continue;
		}

		mono_add_internal_call(funcName.c_str(), addr);

		/* should be always valid */
		MethodOpt ref;
		for (const auto& method : plugin.GetDescriptor().exportedMethods) {
			if (name == method.name) {
				ref = method;
				break;
			}
		}

		_importMethods.emplace(std::move(funcName), MethodInfo{ref.value(), addr});
	}
}

void ScriptEngine::OnPluginStart(const IPlugin& plugin) {
	ScriptOpt script = FindScript(plugin.GetName());
	if (script.has_value()) {
		script->get().InvokeOnStart();
	}
}

void ScriptEngine::OnPluginEnd(const IPlugin& plugin) {
	ScriptOpt script = FindScript(plugin.GetName());
	if (script.has_value()) {
		script->get().InvokeOnEnd();
	}
}

ScriptOpt ScriptEngine::CreateScriptInstance(const IPlugin& plugin, MonoImage* image) {
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

		const auto [it, result] = _scripts.try_emplace(plugin.GetName(), ScriptInstance(*this, plugin, monoClass));
		if (result)
			return std::get<ScriptInstance>(*it);
	}

	return std::nullopt;
}

ScriptOpt ScriptEngine::FindScript(const std::string& name) {
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

MonoString* ScriptEngine::CreateString(const char* source) const {
	return mono_string_new(_appDomain, source);
}

MonoArray* ScriptEngine::CreateArray(MonoClass* klass, size_t count) const {
	return mono_array_new(_appDomain, klass, count);
}

MonoArray* ScriptEngine::CreateStringArray(std::span<const char*> source) const {
	MonoArray* array = CreateArray(mono_get_string_class(), source.size());
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

void ScriptEngine::HandleException(MonoObject* exc, void* /* userData*/) {
	if (!exc || !g_csharplm._provider)
		return;

	MonoClass* exceptionClass = mono_object_get_class(exc);
	std::string message = utils::GetStringProperty("Message", exceptionClass, exc);
	std::string source = utils::GetStringProperty("Source", exceptionClass, exc);
	std::string stackTrace = utils::GetStringProperty("StackTrace", exceptionClass, exc);
	std::string targetSite = utils::GetStringProperty("TargetSite", exceptionClass, exc);
	g_csharplm._provider->Log(std::format("Message: {} | Source: {} | StackTrace: {} | TargetSite: {}", message, source, stackTrace, targetSite), Severity::Error);
}

void ScriptEngine::OnLogCallback(const char* logDomain, const char* logLevel, const char* message, mono_bool fatal, void* /* userData*/) {
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

	std::string_view domain;
	if (!logDomain || strlen(logDomain) == 0) {
		domain = mono_domain_get_friendly_name(g_csharplm._appDomain);
	} else {
		domain = logDomain;
	}
	g_csharplm._provider->Log(std::format("Message: {} | Domain: {}", message, domain), fatal ? Severity::Fatal : severity);
}

void ScriptEngine::OnPrintCallback(const char* message, mono_bool /*isStdout*/) {
	if (g_csharplm._provider)
		g_csharplm._provider->Log(message, Severity::Warning);
}

void ScriptEngine::OnPrintErrorCallback(const char* message, mono_bool /*isStdout*/) {
	if (g_csharplm._provider)
		g_csharplm._provider->Log(message, Severity::Error);
}

/*_________________________________________________*/

ScriptInstance::ScriptInstance(const ScriptEngine& engine, const IPlugin& plugin, MonoClass* klass) : _klass{klass} {
	_instance = engine.InstantiateClass(_klass);

	// Call Script (base) constructor
	{
		MonoMethod* constructor = engine.FindCoreMethod(".ctor");
		const auto& desc = plugin.GetDescriptor();
		auto id = plugin.GetId();
		std::vector<const char*> deps;
		deps.reserve(desc.dependencies.size());
		for (const auto& dependency : desc.dependencies) {
			deps.emplace_back(dependency.name.c_str());
		}
		std::array<void*, 8> args {
			&id,
			engine.CreateString(plugin.GetName().c_str()),
			engine.CreateString(plugin.GetFriendlyName().c_str()),
			engine.CreateString(desc.friendlyName.c_str()),
			engine.CreateString(desc.versionName.c_str()),
			engine.CreateString(desc.createdBy.c_str()),
			engine.CreateString(desc.createdByURL.c_str()),
			engine.CreateStringArray(deps),
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
			ScriptEngine::HandleException(exception, nullptr);
		}
	}
}

void ScriptInstance::InvokeOnEnd() const {
	if (_onEndMethod) {
		MonoObject* exception = nullptr;
		mono_runtime_invoke(_onEndMethod, _instance, nullptr, &exception);
		if (exception) {
			ScriptEngine::HandleException(exception, nullptr);
		}
	}
}