using System;

namespace Plugify
{
    /// <summary>
    /// Class which every c# plugin must implement.
    /// </summary>
    public class Plugin : IEquatable<Plugin>, IComparable<Plugin>
    {
        public ulong Id { get; }
        public string Name { get; }
        public string FullName { get; }
        public string Description { get; }
        public string Version { get; }
        public string Author { get; }
        public string Website { get; }
        public string BaseDir { get; }
        public string[] Dependencies { get; }

        protected Plugin()
        {
            Id = ulong.MaxValue;
            Name = string.Empty;
            FullName = string.Empty;
            Description = string.Empty;
            Version = string.Empty;
            Author = string.Empty;
            Website = string.Empty;
            BaseDir = string.Empty;
            Dependencies = new string[0];
        }

        internal Plugin(ulong id, string name, string fullName, string description, string version, string author, string website, string baseDir, string[] dependencies)
        {
            Id = id;
            Name = name;
            FullName = fullName;
            Description = description;
            Version = version;
            Author = author;
            Website = website;
            BaseDir = baseDir;
            Dependencies = dependencies;
        }

        public Plugin FindPluginByName(string name)
        {
            return (Plugin) InternalCalls.Plugin_FindPluginByName(name);
        }

        public static bool operator ==(Plugin lhs, Plugin rhs)
        {
            return lhs.Id == rhs.Id;
        }

        public static bool operator !=(Plugin lhs, Plugin rhs)
        {
            return lhs.Id != rhs.Id;
        }
        
        public int CompareTo(Plugin other)
        {
            if (ReferenceEquals(this, other)) return 0;
            if (ReferenceEquals(null, other)) return 1;
            return Id.CompareTo(other.Id);
        }
        
        public bool Equals(Plugin other)
        {
            return !ReferenceEquals(other, null) && Id == other.Id;
        }

        public override bool Equals(object obj)
        {
            if (ReferenceEquals(null, obj)) return false;
            if (ReferenceEquals(this, obj)) return true;
            return obj.GetType() == GetType() && Id == ((Plugin)obj).Id;
        }
        
        public bool IsNull()
        {
            return Id == ulong.MaxValue;
        }

        public override int GetHashCode()
        {
            return Id.GetHashCode();
        }

		public override string ToString()
        {
            return IsNull() ? "Plugin.Null" : $"Plugin({Id})";
        }
    }
}