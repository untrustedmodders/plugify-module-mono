using System;
using Plugify;

namespace cross_call_worker
{
    public class CrossCallWorker : Plugin
    {
        public void OnStart()
        {
            Console.WriteLine(".Mono: OnStart");
        }

        public void OnEnd()
        {
            Console.WriteLine(".Mono: OnEnd");
        }
    }
}
