using System;

namespace Wand
{
    [AttributeUsage(AttributeTargets.Method)]
    public class SubscribeAttribute : Attribute
    {
        /** @brief A Internal method name to subscribe */
        private string _method;

        public SubscribeAttribute(string method)
        {
            _method = method;
        }
    }
}