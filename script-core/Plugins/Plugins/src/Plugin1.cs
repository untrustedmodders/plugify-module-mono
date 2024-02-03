using System;
using System.Runtime.CompilerServices;
using Plugify;

namespace example_plugin
{
	public static class example_plugin 
	{
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal static extern void MakePrint(int i);
	}
}

namespace Plugin1
{
	public class SamplePlugin : Plugin
    {
	    public delegate long MyFunc(int a, float b, double c);
	    
		/**
		 * Called when the plugin is fully initialized and all known external dependencies.
		 * This is only called once in the lifetime of the plugin, and is paired with OnEnd().
		 */
		void OnStart()
		{
			Console.Write($"{Name}: OnStart\n");
			//example_plugin.example_plugin.MakePrint(3);
		}

		/**
		 * Called when the plugin is about to be unloading and all known external dependencies are still exist.
		 */
		void OnEnd()
		{
			Console.Write($"{Name}: OnEnd\n");
		}

		public static int MyExportFunction(int a, float b, double c)
		{
		    Console.Write("I believe that what doesn't kill you makes you... stranger!!! \n");
		    Console.Write($"{a}, {b}, {c}");
		    return 1337;
		}
		
		public static long MyExportDelegate(MyFunc func)
		{
			var ret = func(1, 2, 3);
			return ret;
		}
    }
}