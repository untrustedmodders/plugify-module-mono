using System;
using System.Numerics;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;

//generated with https://github.com/untrustedmodders/csharp-lang-module/blob/main/generator/generator.py from cross_call_master 

namespace cross_call_master
{
	public delegate int NoParamReturnFunctionCallbackFunc();

	internal static class cross_call_master
	{
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal static extern void ReverseReturn(string returnString);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal static extern void NoParamReturnVoidCallback();
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal static extern bool NoParamReturnBoolCallback();
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal static extern char NoParamReturnChar8Callback();
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal static extern char NoParamReturnChar16Callback();
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal static extern sbyte NoParamReturnInt8Callback();
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal static extern short NoParamReturnInt16Callback();
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal static extern int NoParamReturnInt32Callback();
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal static extern long NoParamReturnInt64Callback();
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal static extern byte NoParamReturnUInt8Callback();
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal static extern ushort NoParamReturnUInt16Callback();
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal static extern uint NoParamReturnUInt32Callback();
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal static extern ulong NoParamReturnUInt64Callback();
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal static extern IntPtr NoParamReturnPointerCallback();
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal static extern float NoParamReturnFloatCallback();
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal static extern double NoParamReturnDoubleCallback();
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal static extern NoParamReturnFunctionCallbackFunc NoParamReturnFunctionCallback();
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal static extern string NoParamReturnStringCallback();
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal static extern bool[] NoParamReturnArrayBoolCallback();
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal static extern char[] NoParamReturnArrayChar8Callback();
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal static extern char[] NoParamReturnArrayChar16Callback();
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal static extern sbyte[] NoParamReturnArrayInt8Callback();
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal static extern short[] NoParamReturnArrayInt16Callback();
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal static extern int[] NoParamReturnArrayInt32Callback();
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal static extern long[] NoParamReturnArrayInt64Callback();
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal static extern byte[] NoParamReturnArrayUInt8Callback();
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal static extern ushort[] NoParamReturnArrayUInt16Callback();
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal static extern uint[] NoParamReturnArrayUInt32Callback();
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal static extern ulong[] NoParamReturnArrayUInt64Callback();
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal static extern IntPtr[] NoParamReturnArrayPointerCallback();
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal static extern float[] NoParamReturnArrayFloatCallback();
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal static extern double[] NoParamReturnArrayDoubleCallback();
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal static extern string[] NoParamReturnArrayStringCallback();
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal static extern Vector2 NoParamReturnVector2Callback();
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal static extern Vector3 NoParamReturnVector3Callback();
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal static extern Vector4 NoParamReturnVector4Callback();
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal static extern Matrix4x4 NoParamReturnMatrix4x4Callback();
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal static extern void Param1Callback(int a);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal static extern void Param2Callback(int a, float b);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal static extern void Param3Callback(int a, float b, double c);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal static extern void Param4Callback(int a, float b, double c, Vector4 d);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal static extern void Param5Callback(int a, float b, double c, Vector4 d, long[] e);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal static extern void Param6Callback(int a, float b, double c, Vector4 d, long[] e, char f);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal static extern void Param7Callback(int a, float b, double c, Vector4 d, long[] e, char f, string g);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal static extern void Param8Callback(int a, float b, double c, Vector4 d, long[] e, char f, string g, char h);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal static extern void Param9Callback(int a, float b, double c, Vector4 d, long[] e, char f, string g, char h, short k);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal static extern void Param10Callback(int a, float b, double c, Vector4 d, long[] e, char f, string g, char h, short k, IntPtr l);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal static extern void ParamRef1Callback(ref int a);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal static extern void ParamRef2Callback(ref int a, ref float b);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal static extern void ParamRef3Callback(ref int a, ref float b, ref double c);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal static extern void ParamRef4Callback(ref int a, ref float b, ref double c, ref Vector4 d);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal static extern void ParamRef5Callback(ref int a, ref float b, ref double c, ref Vector4 d, ref long[] e);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal static extern void ParamRef6Callback(ref int a, ref float b, ref double c, ref Vector4 d, ref long[] e, ref char f);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal static extern void ParamRef7Callback(ref int a, ref float b, ref double c, ref Vector4 d, ref long[] e, ref char f, ref string g);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal static extern void ParamRef8Callback(ref int a, ref float b, ref double c, ref Vector4 d, ref long[] e, ref char f, ref string g, ref char h);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal static extern void ParamRef9Callback(ref int a, ref float b, ref double c, ref Vector4 d, ref long[] e, ref char f, ref string g, ref char h, ref short k);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal static extern void ParamRef10Callback(ref int a, ref float b, ref double c, ref Vector4 d, ref long[] e, ref char f, ref string g, ref char h, ref short k, ref IntPtr l);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal static extern void ParamRefVectorsCallback(ref bool[] p1, ref char[] p2, ref char[] p3, ref sbyte[] p4, ref short[] p5, ref int[] p6, ref long[] p7, ref byte[] p8, ref ushort[] p9, ref uint[] p10, ref ulong[] p11, ref IntPtr[] p12, ref float[] p13, ref double[] p14, ref string[] p15);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal static extern long ParamAllPrimitivesCallback(bool p1, char p2, char p3, sbyte p4, short p5, int p6, long p7, byte p8, ushort p9, uint p10, ulong p11, IntPtr p12, float p13, double p14);
	}
}
