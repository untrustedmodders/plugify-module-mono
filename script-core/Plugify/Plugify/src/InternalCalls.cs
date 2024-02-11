﻿using System.Numerics;
using System.Runtime.CompilerServices;

#nullable enable

namespace Plugify
{
	public static class InternalCalls
	{
		#region Plugin
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal static extern object? Plugin_FindPluginByName(string name);
		#endregion
	}
}

#nullable disable