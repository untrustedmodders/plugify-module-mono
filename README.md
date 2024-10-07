# C# (Mono) Language Module for Plugify

The C# (Mono) Language Module for Plugify facilitates the development of plugins in C# for the Plugify framework. With this module, you can seamlessly integrate C# plugins, allowing dynamic loading and management by the Plugify core.

## Features

- **C# (Mono) Plugin Support**: Write your plugins in C# (Mono) and seamlessly integrate them with the Plugify framework.
- **Automatic Exporting**: Effortlessly export and import methods between plugins and the language module.
- **Initialization and Cleanup**: Handle plugin initialization, startup, and cleanup with dedicated module events.
- **Interoperability**: Communicate with plugins written in other languages through auto-generated interfaces.

**Note**: All C# (Mono) plugins are hosted within the single domain. This allows for seamless collaboration and interaction between C# plugins without the Plugify framework.

## Getting Started

### Prerequisites

- Mono Runtime [(.NET 4.7.2)](https://www.mono-project.com/docs/about-mono/compatibility/)
- Plugify Framework Installed

### Installation

#### Option 1: Install via Plugify Plugin Manager

You can install the C++ Language Module using the Plugify plugin manager by running the following command:

```bash
plugify install plugify-module-mono
```

#### Option 2: Manual Installation

1. Clone this repository:

    ```bash
    git clone https://github.com/untrustedmodders/mono-lang-module.git
    cd mono-lang-module
    git submodule update --init --recursive
    ```

2. Build the C# (Mono) language module:

    ```bash
    mkdir build && cd build
    cmake ..
    cmake --build .
    ```

### Usage

1. **Integration with Plugify**

   Ensure that your C# (Mono) language module is available in the same directory as your Plugify setup.

2. **Write C# Plugins**

   Develop your plugins in C# using the Plugify C# API. Refer to the [Plugify C# Plugin Guide](https://untrustedmodders.github.io/en/developing/plugins/first-plugin) for detailed instructions.

3. **Build and Install Plugins**

   Compile your C# plugins and place the assemblies in a directory accessible to the Plugify core.

4. **Run Plugify**

   Start the Plugify framework, and it will dynamically load your C# plugins.

## Example

```c#
using System;
using System.IO;
using Plugify;

namespace ExamplePlugin
{
    public class SamplePlugin : Plugin
    {
        void OnStart()
        {
            Console.Write($"{Name}: OnStart\n");
        }

        void OnEnd()
        {
            Console.Write($"{Name}: OnEnd\n");
        }
    }
}
```

## Documentation

For comprehensive documentation on writing plugins in C# (Mono) using the Plugify framework, refer to the [Plugify Documentation](https://untrustedmodders.github.io).

## Contributing

Feel free to contribute by opening issues or submitting pull requests. We welcome your feedback and ideas!

## License

This C# (Mono) Language Module for Plugify is licensed under the [MIT License](LICENSE).
