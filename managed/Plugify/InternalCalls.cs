using System.Runtime.CompilerServices;

namespace Plugify
{
	public static class InternalCalls
	{
		#region Plugin
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal static extern object Plugin_FindPluginByName(string name);
		#endregion
	}
}