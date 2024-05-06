using System;
using System.Collections.Generic;
using Plugify;
using static cpp_test.cpp_test;

namespace CSharpTest
{
    public class CSharpTestPlugin : Plugin
    {
        public void OnStart() 
        {
	        // No Params, Only Return
	        {
		        NoParamReturnVoid();

                PrintReturnValue(nameof(NoParamReturnBool), NoParamReturnBool());

                PrintReturnValue(nameof(NoParamReturnChar8), NoParamReturnChar8());

                PrintReturnValue(nameof(NoParamReturnChar16), NoParamReturnChar16());

                PrintReturnValue(nameof(NoParamReturnInt8), NoParamReturnInt8());

                PrintReturnValue(nameof(NoParamReturnInt16), NoParamReturnInt16());

                PrintReturnValue(nameof(NoParamReturnInt32), NoParamReturnInt32());

                PrintReturnValue(nameof(NoParamReturnInt64), NoParamReturnInt64());

                PrintReturnValue(nameof(NoParamReturnUInt8), NoParamReturnUInt8());

                PrintReturnValue(nameof(NoParamReturnUInt16), NoParamReturnUInt16());

                PrintReturnValue(nameof(NoParamReturnUInt32), NoParamReturnUInt32());

                PrintReturnValue(nameof(NoParamReturnUInt64), NoParamReturnUInt64());

                PrintReturnValue(nameof(NoParamReturnPtr64), NoParamReturnPtr64());

                PrintReturnValue(nameof(NoParamReturnFloat), NoParamReturnFloat());

                PrintReturnValue(nameof(NoParamReturnDouble), NoParamReturnDouble());

                PrintReturnValue(nameof(NoParamReturnFunction), NoParamReturnFunction());

                // std::string
                PrintReturnValue(nameof(NoParamReturnString), NoParamReturnString());

                // std::vector
                PrintReturnArray(nameof(NoParamReturnArrayBool), NoParamReturnArrayBool());

                PrintReturnArray(nameof(NoParamReturnArrayChar8), NoParamReturnArrayChar8());

                PrintReturnArray(nameof(NoParamReturnArrayChar16), NoParamReturnArrayChar16());

                PrintReturnArray(nameof(NoParamReturnArrayInt8), NoParamReturnArrayInt8());

                PrintReturnArray(nameof(NoParamReturnArrayInt16), NoParamReturnArrayInt16());

                PrintReturnArray(nameof(NoParamReturnArrayInt32), NoParamReturnArrayInt32());

                PrintReturnArray(nameof(NoParamReturnArrayInt64), NoParamReturnArrayInt64());

                PrintReturnArray(nameof(NoParamReturnArrayUInt8), NoParamReturnArrayUInt8());

                PrintReturnArray(nameof(NoParamReturnArrayUInt16), NoParamReturnArrayUInt16());

                PrintReturnArray(nameof(NoParamReturnArrayUInt32), NoParamReturnArrayUInt32());

                PrintReturnArray(nameof(NoParamReturnArrayUInt64), NoParamReturnArrayUInt64());

                PrintReturnArray(nameof(NoParamReturnArrayPtr64), NoParamReturnArrayPtr64());

                PrintReturnArray(nameof(NoParamReturnArrayFloat), NoParamReturnArrayFloat());

                PrintReturnArray(nameof(NoParamReturnArrayDouble), NoParamReturnArrayDouble());

                PrintReturnArray(nameof(NoParamReturnArrayString), NoParamReturnArrayString());

                // glm:vec
                //PrintReturnValue(nameof(NoParamReturnVector2), NoParamReturnVector2());

                //PrintReturnValue(nameof(NoParamReturnVector3), NoParamReturnVector3());

                //PrintReturnValue(nameof(NoParamReturnVector4), NoParamReturnVector4());

                //glm::mat
                //PrintReturnValue(nameof(NoParamReturnMatrix4x4), NoParamReturnMatrix4x4());
            }
	        
	        int intValue = 42;
	        float floatValue = 3.14f;
	        double doubleValue = 6.28;
	        Vector4 vector4Value = new Vector4(1.0f, 2.0f, 3.0f, 4.0f);
            long[] longListValue = new long[] { 100, 200, 300 };
	        char charValue = 'A';
	        string stringValue = "Hello";
	        float floatValue2 = 2.71f;
	        short shortValue = 10;
	        IntPtr ptrValue = IntPtr.Zero; // Provide appropriate value for pointer
	        
	        // Params (no refs)
	        {
		        Param1(intValue);
		        Param2(intValue, floatValue);
		        Param3(intValue, floatValue, doubleValue);
		        Param4(intValue, floatValue, doubleValue, vector4Value);
		        Param5(intValue, floatValue, doubleValue, vector4Value, longListValue);
		        Param6(intValue, floatValue, doubleValue, vector4Value, longListValue, charValue);
		        Param7(intValue, floatValue, doubleValue, vector4Value, longListValue, charValue, stringValue);
		        Param8(intValue, floatValue, doubleValue, vector4Value, longListValue, charValue, stringValue, floatValue2);
		        Param9(intValue, floatValue, doubleValue, vector4Value, longListValue, charValue, stringValue, floatValue2, shortValue);
		        Param10(intValue, floatValue, doubleValue, vector4Value, longListValue, charValue, stringValue, floatValue2, shortValue, ptrValue);
	        }
	        
	        // Params (with refs)
	        {
		        ParamRef1(ref intValue);
		        Console.WriteLine($"Value after calling ParamRef1: {intValue}");

		        ParamRef2(ref intValue, ref floatValue);
		        Console.WriteLine($"Values after calling ParamRef2: {intValue}, {floatValue}");

		        ParamRef3(ref intValue, ref floatValue, ref doubleValue);
		        Console.WriteLine($"Values after calling ParamRef3: {intValue}, {floatValue}, {doubleValue}");

		        ParamRef4(ref intValue, ref floatValue, ref doubleValue, ref vector4Value);
		        Console.WriteLine($"Values after calling ParamRef4: {intValue}, {floatValue}, {doubleValue}, {vector4Value}");

		        ParamRef5(ref intValue, ref floatValue, ref doubleValue, ref vector4Value, ref longListValue);
		        Console.WriteLine($"Values after calling ParamRef5: {intValue}, {floatValue}, {doubleValue}, {vector4Value}, {string.Join(",", longListValue)}");

		        ParamRef6(ref intValue, ref floatValue, ref doubleValue, ref vector4Value, ref longListValue, ref charValue);
		        Console.WriteLine($"Values after calling ParamRef6: {intValue}, {floatValue}, {doubleValue}, {vector4Value}, {string.Join(",", longListValue)}, {charValue}");

		        ParamRef7(ref intValue, ref floatValue, ref doubleValue, ref vector4Value, ref longListValue, ref charValue, ref stringValue);
		        Console.WriteLine($"Values after calling ParamRef7: {intValue}, {floatValue}, {doubleValue}, {vector4Value}, {string.Join(",", longListValue)}, {charValue}, {stringValue}");

		        ParamRef8(ref intValue, ref floatValue, ref doubleValue, ref vector4Value, ref longListValue, ref charValue, ref stringValue, ref floatValue2);
		        Console.WriteLine($"Values after calling ParamRef8: {intValue}, {floatValue}, {doubleValue}, {vector4Value}, {string.Join(",", longListValue)}, {charValue}, {stringValue}, {floatValue2}");

		        ParamRef9(ref intValue, ref floatValue, ref doubleValue, ref vector4Value, ref longListValue, ref charValue, ref stringValue, ref floatValue2, ref shortValue);
		        Console.WriteLine($"Values after calling ParamRef9: {intValue}, {floatValue}, {doubleValue}, {vector4Value}, {string.Join(",", longListValue)}, {charValue}, {stringValue}, {floatValue2}, {shortValue}");

		        ParamRef10(ref intValue, ref floatValue, ref doubleValue, ref vector4Value, ref longListValue, ref charValue, ref stringValue, ref floatValue2, ref shortValue, ref ptrValue);
		        Console.WriteLine($"Values after calling ParamRef10: {intValue}, {floatValue}, {doubleValue}, {vector4Value}, {string.Join(",", longListValue)}, {charValue}, {stringValue}, {floatValue2}, {shortValue}, {ptrValue}");
	        }
	        
	        // Initialize arrays
	        bool[] boolArray = { true, false, true };
	        char[] charArray = { 'a', 'b', 'c' };
	        char[] char16Array = { 'A', 'B', 'C' };
	        sbyte[] sbyteArray = { -1, -2, -3 };
	        short[] shortArray = { 10, 20, 30 };
	        int[] intArray = { 100, 200, 300 };
	        long[] longArray = { 1000, 2000, 3000 };
	        byte[] byteArray = { 1, 2, 3 };
	        ushort[] ushortArray = { 1000, 2000, 3000 };
	        uint[] uintArray = { 10000, 20000, 30000 };
	        ulong[] ulongArray = { 100000, 200000, 300000 };
	        IntPtr[] intPtrArray = { IntPtr.Zero, IntPtr.Zero, IntPtr.Zero };
	        float[] floatArray = { 1.1f, 2.2f, 3.3f };
	        double[] doubleArray = { 1.1, 2.2, 3.3 };
	        string[] stringArray = { "Hello", "World", "!" };

	        {
		        // Call function ParamRefVectors and print how values change
		        ParamRefVectors(ref boolArray, ref charArray, ref char16Array, ref sbyteArray, ref shortArray, ref intArray, ref longArray,
			        ref byteArray, ref ushortArray, ref uintArray, ref ulongArray, ref intPtrArray, ref floatArray, ref doubleArray, ref stringArray);

		        Console.WriteLine("Values after calling ParamRefVectors:");
		        Console.WriteLine($"boolArray: {string.Join(",", boolArray)}");
		        Console.WriteLine($"charArray: {string.Join(",", charArray)}");
		        Console.WriteLine($"char16Array: {string.Join(",", char16Array)}");
		        Console.WriteLine($"sbyteArray: {string.Join(",", sbyteArray)}");
		        Console.WriteLine($"shortArray: {string.Join(",", shortArray)}");
		        Console.WriteLine($"intArray: {string.Join(",", intArray)}");
		        Console.WriteLine($"longArray: {string.Join(",", longArray)}");
		        Console.WriteLine($"byteArray: {string.Join(",", byteArray)}");
		        Console.WriteLine($"ushortArray: {string.Join(",", ushortArray)}");
		        Console.WriteLine($"uintArray: {string.Join(",", uintArray)}");
		        Console.WriteLine($"ulongArray: {string.Join(",", ulongArray)}");
		        Console.WriteLine($"intPtrArray: {string.Join(",", intPtrArray)}");
		        Console.WriteLine($"floatArray: {string.Join(",", floatArray)}");
		        Console.WriteLine($"doubleArray: {string.Join(",", doubleArray)}");
		        Console.WriteLine($"stringArray: {string.Join(",", stringArray)}");

		        // Call function ParamAllPrimitives and print the returned value
		        long returnValue = ParamAllPrimitives(boolArray[0], charArray[0], sbyteArray[0], shortArray[0], intArray[0], longArray[0],
			        byteArray[0], ushortArray[0], uintArray[0], ulongArray[0], intPtrArray[0], floatArray[0], doubleArray[0]);

		        Console.WriteLine($"Return value from ParamAllPrimitives: {returnValue}");
	        }
        }

        public void PrintReturnValue<T>(string functionName, T returnValue)
        {
            Console.WriteLine($"Function: {functionName}, Return Value: {returnValue}");
        }
        
        public void PrintReturnArray<T>(string functionName, IEnumerable<T> returnValue)
        {
            Console.WriteLine($"Function: {functionName}, Return Value: {string.Join(", ", returnValue)}");
        }
    }
}