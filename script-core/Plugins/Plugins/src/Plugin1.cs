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
            Console.Write($"{Name}: OnCreate\n");
        }

		/**
		 * Called when the plugin is fully initialized and all known external dependencies.
		 * This is only called once in the lifetime of the plugin, and is paired with OnEnd().
		 */
		void OnStart()
		{
			Console.Write($"{Name}: OnStart\n");
		}

		/**
		 * Called when the plugin is about to be unloading and all known external dependencies are still exist.
		 */
		void OnEnd()
		{
			Console.Write($"{Name}: OnEnd\n");
		}

		/**
		 * Called when the plugin is about to be destroy.
		 */
		void OnDestroy()
		{
			Console.Write($"{Name}: OnDestroy\n");
		}

		public static int MyExportFunction(int a, float b, double c)
		{
		    Console.Write("I believe that what doesn't kill you makes you... stranger!!! \n");
		    return 1337;
		}
    }
}