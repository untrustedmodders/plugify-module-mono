using System;
using Wand;

namespace Plugin1
{
    public class SamplePlugin : Plugin
    {
		/**
		 * Called when the plugin is fully initialized and all known external references are resolved.
		 * This is only called once in the lifetime of the plugin, and is paired with OnDestroy().
		 */
		void OnCreate()
		{
			Console.Write("SamplePlugin1: OnCreate\n");
		}

		/**
		 * Called when the plugin is about to be released.
		 */
		void OnDestroy()
		{
			Console.Write("SamplePlugin1: OnDestroy\n");
		}
    }
}