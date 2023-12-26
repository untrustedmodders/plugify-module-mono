using System;
using Wand;

namespace Plugin1
{
    public class SamplePlugin : Plugin
    {
        /**
         * Called when the plugin is created.
         */
        void OnCreate()
        {
            Console.Write("Plugin1: OnCreate\n");
        }

		/**
		 * Called when the plugin is fully initialized and all known external dependencies.
		 * This is only called once in the lifetime of the plugin, and is paired with OnEnd().
		 */
		void OnStart()
		{
			Console.Write("Plugin1: OnStart\n");
		}

		/**
		 * Called when the plugin is about to be unloading and all known external dependencies are still exist.
		 */
		void OnEnd()
		{
			Console.Write("Plugin1: OnEnd\n");
		}

		/**
		 * Called when the plugin is about to be destroy.
		 */
		void OnDestroy()
		{
			Console.Write("Plugin1: OnDestroy\n");
		}
    }
}