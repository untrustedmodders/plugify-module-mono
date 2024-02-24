using System;
using System.IO;
using Plugify;

namespace example_plugin
{
    public static class example_plugin 
    {
        public delegate void MyFunc(int a, string c);
		
       // [MethodImplAttribute(MethodImplOptions.InternalCall)]
        ////internal static extern void MakePrint(int i, string c);
		
       // [MethodImplAttribute(MethodImplOptions.InternalCall)]
        //internal static extern void ReceiveFuncDelegate(MyFunc f);
    }
}

namespace ExamplePlugin
{
    public class SamplePlugin : Plugin
    {
        /**
		 * Called when the plugin is fully initialized and all known external dependencies.
		 * This is only called once in the lifetime of the plugin, and is paired with OnEnd().
		 */
        void OnStart()
        {
            int i = 0;
            int b = 2;
            int c = i + b;

            string S = "name";

            S += c;

            string ddc = S;

            Console.Write($"{Name}: OnStart\n");
            Console.Write(S + "\n");
            
            
            string[] lines = {"some text1", "some text2", "some text3"};
            File.WriteAllLines(@"someText.txt", lines);
            
            //example_plugin.example_plugin.MakePrint(3);
            //example_plugin.example_plugin.ReceiveFuncDelegate(MyExportFunction);
            //example_plugin.example_plugin.ReceiveFuncDelegate(MakePrint);
        }

        /**
		 * Called when the plugin is about to be unloading and all known external dependencies are still exist.
		 */
        void OnEnd()
        {
            //Console.Write($"{Name}: OnEnd\n");
        }

        void MyExportFunction(int a, string c)
        {
            //Console.Write($"{Name}: OnEnd\n");
            //Console.Write("I believe that what doesn't kill you makes you... stranger!!! \n");
           // Console.Write($"{a}, {c}");
        }
    }
}