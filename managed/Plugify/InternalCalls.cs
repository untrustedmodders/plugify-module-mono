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
		internal static extern string Plugin_FindResource(long id, string path);
		#endregion
	}
}