#include <plugify/cpp_plugin.h>
#include <plugin_export.h>
#include <test/test.h>
#include <pps/CSharpTest.h>

class CppTestPlugin : public plugify::IPluginEntry {
public:
    void OnPluginStart() override {
        // No Params, Only Return
        {
            std::cout << "==========================================================================" << std::endl;

            NoParamReturnVoid();

            PrintReturnValue("NoParamReturnBool", CSharpTest::NoParamReturnBool());

            PrintReturnValue("NoParamReturnChar16", CSharpTest::NoParamReturnChar16());

            PrintReturnValue("NoParamReturnInt8", CSharpTest::NoParamReturnInt8());

            PrintReturnValue("NoParamReturnInt16", CSharpTest::NoParamReturnInt16());

            PrintReturnValue("NoParamReturnInt32", CSharpTest::NoParamReturnInt32());

            PrintReturnValue("NoParamReturnInt64", CSharpTest::NoParamReturnInt64());

            PrintReturnValue("NoParamReturnUInt8", CSharpTest::NoParamReturnUInt8());

            PrintReturnValue("NoParamReturnUInt16", CSharpTest::NoParamReturnUInt16());

            PrintReturnValue("NoParamReturnUInt32", CSharpTest::NoParamReturnUInt32());

            PrintReturnValue("NoParamReturnUInt64", CSharpTest::NoParamReturnUInt64());

            PrintReturnValue("NoParamReturnPtr64", CSharpTest::NoParamReturnPtr64());

            PrintReturnValue("NoParamReturnFloat", CSharpTest::NoParamReturnFloat());

            PrintReturnValue("NoParamReturnDouble", CSharpTest::NoParamReturnDouble());

            PrintReturnValue("NoParamReturnFunction", CSharpTest::NoParamReturnFunction());

            // std::string
            PrintReturnValue("NoParamReturnString", CSharpTest::NoParamReturnString());

            // std::vector
            PrintReturnArray("NoParamReturnArrayBool", CSharpTest::NoParamReturnArrayBool());

            PrintReturnArray("NoParamReturnArrayChar16", CSharpTest::NoParamReturnArrayChar16());

            PrintReturnArray("NoParamReturnArrayInt8", CSharpTest::NoParamReturnArrayInt8());

            PrintReturnArray("NoParamReturnArrayInt16", CSharpTest::NoParamReturnArrayInt16());

            PrintReturnArray("NoParamReturnArrayInt32", CSharpTest::NoParamReturnArrayInt32());

            PrintReturnArray("NoParamReturnArrayInt64", CSharpTest::NoParamReturnArrayInt64());

            PrintReturnArray("NoParamReturnArrayUInt8", CSharpTest::NoParamReturnArrayUInt8());

            PrintReturnArray("NoParamReturnArrayUInt16", CSharpTest::NoParamReturnArrayUInt16());

            PrintReturnArray("NoParamReturnArrayUInt32", CSharpTest::NoParamReturnArrayUInt32());

            PrintReturnArray("NoParamReturnArrayUInt64", CSharpTest::NoParamReturnArrayUInt64());

            PrintReturnArray("NoParamReturnArrayPtr64", CSharpTest::NoParamReturnArrayPtr64());

            PrintReturnArray("NoParamReturnArrayFloat", CSharpTest::NoParamReturnArrayFloat());

            PrintReturnArray("NoParamReturnArrayDouble", CSharpTest::NoParamReturnArrayDouble());

            PrintReturnArray("NoParamReturnArrayString", CSharpTest::NoParamReturnArrayString());

            // glm:vec
            //PrintReturnValue("NoParamReturnVector2", CSharpTest::NoParamReturnVector2());

            //PrintReturnValue("NoParamReturnVector3", CSharpTest::NoParamReturnVector3());

            //PrintReturnValue("NoParamReturnVector4", CSharpTest::NoParamReturnVector4());

            //glm::mat
           // PrintReturnValue("NoParamReturnMatrix4x4", CSharpTest::NoParamReturnMatrix4x4());
        }

        int32_t intValue = 42;
        float floatValue = 3.14f;
        double doubleValue = 6.28;
        plugify::Vector4 vector4Value = plugify::Vector4(1.0f, 2.0f, 3.0f, 4.0f);
        std::vector<int64_t> longListValue = { 100, 200, 300 };
        char16_t charValue = 'A';
        std::string stringValue = "Hello";
        float floatValue2 = 2.71f;
        int16_t shortValue = 10;
        void* ptrValue = 0; // Provide appropriate value for pointer

        // Params (no refs)
        {
            CSharpTest::Param1(intValue);
            CSharpTest::Param2(intValue, floatValue);
            CSharpTest::Param3(intValue, floatValue, doubleValue);
            CSharpTest::Param4(intValue, floatValue, doubleValue, vector4Value);
            CSharpTest::Param5(intValue, floatValue, doubleValue, vector4Value, longListValue);
            CSharpTest::Param6(intValue, floatValue, doubleValue, vector4Value, longListValue, charValue);
            CSharpTest::Param7(intValue, floatValue, doubleValue, vector4Value, longListValue, charValue, stringValue);
            CSharpTest::Param8(intValue, floatValue, doubleValue, vector4Value, longListValue, charValue, stringValue, floatValue2);
            CSharpTest::Param9(intValue, floatValue, doubleValue, vector4Value, longListValue, charValue, stringValue, floatValue2, shortValue);
            CSharpTest::Param10(intValue, floatValue, doubleValue, vector4Value, longListValue, charValue, stringValue, floatValue2, shortValue, ptrValue);
        }

        // Params (with refs)
        {
            CSharpTest::ParamRef1(intValue);
            std::cout << "Value after calling ParamRef1: " << intValue << std::endl;

            CSharpTest::ParamRef2(intValue, floatValue);
            std::cout << "Values after calling ParamRef2: " << intValue << ", " << floatValue << std::endl;

            CSharpTest::ParamRef3(intValue, floatValue, doubleValue);
            std::cout << "Values after calling ParamRef3: " << intValue << ", " << floatValue << ", " << doubleValue << std::endl;

            CSharpTest::ParamRef4(intValue, floatValue, doubleValue, vector4Value);
            std::cout << "Values after calling ParamRef4: " << intValue << ", " << floatValue << ", " << doubleValue << ", " << "[" << vector4Value.x << " " << vector4Value.y << " " << vector4Value.z << " " << vector4Value.w << "]" << std::endl;

            CSharpTest::ParamRef5(intValue, floatValue, doubleValue, vector4Value, longListValue);
            std::cout << "Values after calling ParamRef5: " << intValue << ", " << floatValue << ", " << doubleValue << ", " << "[" << vector4Value.x << " " << vector4Value.y << " " << vector4Value.z << " " << vector4Value.w << "]" << ", " << longListValue[0] << std::endl;

            CSharpTest::ParamRef6(intValue, floatValue, doubleValue, vector4Value, longListValue, charValue);
            std::cout << "Values after calling ParamRef6: " << intValue << ", " << floatValue << ", " << doubleValue << ", " << "[" << vector4Value.x << " " << vector4Value.y << " " << vector4Value.z << " " << vector4Value.w << "]" << ", " << longListValue[0] << ", " << static_cast<char>(charValue) << std::endl;

            CSharpTest::ParamRef7(intValue, floatValue, doubleValue, vector4Value, longListValue, charValue, stringValue);
            std::cout << "Values after calling ParamRef7: " << intValue << ", " << floatValue << ", " << doubleValue << ", " << "[" << vector4Value.x << " " << vector4Value.y << " " << vector4Value.z << " " << vector4Value.w << "]" << ", " << longListValue[0] << ", " << static_cast<char>(charValue) << ", " << stringValue << std::endl;

            CSharpTest::ParamRef8(intValue, floatValue, doubleValue, vector4Value, longListValue, charValue, stringValue, floatValue2);
            std::cout << "Values after calling ParamRef8: " << intValue << ", " << floatValue << ", " << doubleValue << ", " << "[" << vector4Value.x << " " << vector4Value.y << " " << vector4Value.z << " " << vector4Value.w << "]" << ", " << longListValue[0] << ", " << static_cast<char>(charValue) << ", " << stringValue << ", " << floatValue2 << std::endl;

            CSharpTest::ParamRef9(intValue, floatValue, doubleValue, vector4Value, longListValue, charValue, stringValue, floatValue2, shortValue);
            std::cout << "Values after calling ParamRef9: " << intValue << ", " << floatValue << ", " << doubleValue << ", " << "[" << vector4Value.x << " " << vector4Value.y << " " << vector4Value.z << " " << vector4Value.w << "]" << ", " << longListValue[0] << ", " << static_cast<char>(charValue) << ", " << stringValue << ", " << floatValue2 << ", " << shortValue << std::endl;

            CSharpTest::ParamRef10(intValue, floatValue, doubleValue, vector4Value, longListValue, charValue, stringValue, floatValue2, shortValue, ptrValue);
            std::cout << "Values after calling ParamRef10: " << intValue << ", " << floatValue << ", " << doubleValue << ", " << "[" << vector4Value.x << " " << vector4Value.y << " " << vector4Value.z << " " << vector4Value.w << "]" << ", " << longListValue[0] << ", " << static_cast<char>(charValue) << ", " << stringValue << ", " << floatValue2 << ", " << shortValue << ", " << ptrValue << std::endl;
        }

        // Initialize arrays
        std::vector<bool> boolArray = { true, false, true };
        std::vector<char16_t> char16Array = { 'A', 'B', 'C' };
        std::vector<int8_t> sbyteArray = { -1, -2, -3 };
        std::vector<int16_t> shortArray = { 10, 20, 30 };
        std::vector<int32_t> intArray = { 100, 200, 300 };
        std::vector<int64_t> longArray = { 1000, 2000, 3000 };
        std::vector<uint8_t> byteArray = { 1, 2, 3 };
        std::vector<uint16_t> ushortArray = { 1000, 2000, 3000 };
        std::vector<uint32_t> uintArray = { 10000, 20000, 30000 };
        std::vector<uint64_t> ulongArray = { 100000, 200000, 300000 };
        std::vector<void*> intPtrArray = { nullptr, nullptr, nullptr };
        std::vector<float> floatArray = { 1.1f, 2.2f, 3.3f };
        std::vector<double> doubleArray = { 1.1, 2.2, 3.3 };
        std::vector<std::string> stringArray = { "Hello", "World", "!" };

        {
            // Call function ParamRefVectors and print how values change
            CSharpTest::ParamRefVectors(boolArray, char16Array, sbyteArray, shortArray, intArray, longArray,
                            byteArray, ushortArray, uintArray, ulongArray, intPtrArray, floatArray, doubleArray, stringArray);

            std::cout << "Values after calling ParamRefVectors:" << std::endl;
            PrintReturnArray("boolArray", boolArray);
            PrintReturnArray("char16Array", char16Array);
            PrintReturnArray("sbyteArray", sbyteArray);
            PrintReturnArray("shortArray", shortArray);
            PrintReturnArray("intArray", intArray);
            PrintReturnArray("longArray", longArray);
            PrintReturnArray("byteArray", byteArray);
            PrintReturnArray("ushortArray", ushortArray);
            PrintReturnArray("uintArray", uintArray);
            PrintReturnArray("ulongArray", ulongArray);
            PrintReturnArray("intPtrArray", intPtrArray);
            PrintReturnArray("floatArray", floatArray);
            PrintReturnArray("doubleArray", doubleArray);
            PrintReturnArray("stringArray", stringArray);

            // Call function ParamAllPrimitives and print the returned value
            int64_t returnValue = CSharpTest::ParamAllPrimitives(boolArray[0], char16Array[0], sbyteArray[0], shortArray[0], intArray[0], longArray[0],
                                                     byteArray[0], ushortArray[0], uintArray[0], ulongArray[0], intPtrArray[0], floatArray[0], doubleArray[0]);

            std::cout << "Return value from ParamAllPrimitives: " << returnValue << std::endl;
        }
    }

    template<typename T>
    void PrintReturnValue(const std::string& functionName, T returnValue) {
        if constexpr (std::is_same_v<T, plugify::Matrix4x4>) {
            std::cout << "Function: " << functionName << ", Return Value:" << std::endl;
            std::cout << "[" << returnValue.a.x << ", " << returnValue.a.y << ", " << returnValue.a.z << ", " << returnValue.a.w << "]" << std::endl;
            std::cout << "[" << returnValue.b.x << ", " << returnValue.b.y << ", " << returnValue.b.z << ", " << returnValue.b.w << "]" << std::endl;
            std::cout << "[" << returnValue.c.x << ", " << returnValue.c.y << ", " << returnValue.c.z << ", " << returnValue.c.w << "]" << std::endl;
            std::cout << "[" << returnValue.d.x << ", " << returnValue.d.y << ", " << returnValue.d.z << ", " << returnValue.d.w << "]" << std::endl;
        } else if constexpr (std::is_same_v<T, char16_t>) {
            std::cout << "Function: " << functionName << ", Return Value: " << static_cast<char>(returnValue) << std::endl;
        } else if constexpr (std::is_same_v<T, plugify::Vector2>) {
                std::cout << "Function: " << functionName << ", Return Value: [" << returnValue.x << ", " << returnValue.y << "]" << std::endl;
        } else if constexpr (std::is_same_v<T, plugify::Vector3>) {
                std::cout << "Function: " << functionName << ", Return Value: [" << returnValue.x << ", " << returnValue.y << ", " << returnValue.z << "]" << std::endl;
        } else if constexpr (std::is_same_v<T, plugify::Vector4>) {
                std::cout << "Function: " << functionName << ", Return Value: [" << returnValue.x << ", " << returnValue.y << ", " << returnValue.z << ", " << returnValue.w << "]" << std::endl;
        } else {
            std::cout << "Function: " << functionName << ", Return Value: " << returnValue << std::endl;
        }
    }

    template<Matrix4x4>
    void PrintReturnValue(const std::string& functionName, const Matrix4x4& returnValue) {
        std::cout << "Function: " << functionName << ", Return Value:" << std::endl;
        std::cout << "[" << returnValue.a.x << ", " << returnValue.a.y << ", " << returnValue.a.z << ", " << returnValue.a.w << "]" << std::endl;
        std::cout << "[" << returnValue.b.x << ", " << returnValue.b.y << ", " << returnValue.b.z << ", " << returnValue.b.w << "]" << std::endl;
        std::cout << "[" << returnValue.c.x << ", " << returnValue.c.y << ", " << returnValue.c.z << ", " << returnValue.c.w << "]" << std::endl;
        std::cout << "[" << returnValue.d.x << ", " << returnValue.d.y << ", " << returnValue.d.z << ", " << returnValue.d.w << "]" << std::endl;
    }

    template<typename T>
    void PrintReturnArray(const std::string& functionName, const std::vector<T>& returnValue) {
        std::cout << "Function: " << functionName << ", Return Value: [";
        for (const auto& val : returnValue) {
            if constexpr (std::is_same_v<T, char16_t>)
                std::cout << static_cast<char>(val) << ", ";
            else    
                std::cout << val << ", ";
        }
        std::cout << "]" << std::endl;
    }
};

CppTestPlugin g_testPlugin;
EXPOSE_PLUGIN(PLUGIN_API, &g_testPlugin)