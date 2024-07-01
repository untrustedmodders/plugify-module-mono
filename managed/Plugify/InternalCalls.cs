using System.Runtime.CompilerServices;

namespace Plugify
{
	public static class InternalCalls
	{
		#region Core
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal static extern string Core_GetBaseDirectory();
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal static extern bool Core_IsModuleLoaded(string name, int version, bool minimum);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal static extern bool Core_IsPluginLoaded(string name, int version, bool minimum);
		#endregion

		#region Plugin
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal static extern object Plugin_FindPluginByName(string name);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal static extern string Plugin_FindResource(string name, string path);
		#endregion
	}
}