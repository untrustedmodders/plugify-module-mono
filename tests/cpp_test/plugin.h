#pragma once

#include <plugify/cpp_plugin.h>
#include <plugin_export.h>

#include <test/test.h>
#include <pps/CSharpTest.h>

#include <cassert>

class CppTestPlugin : public plugify::IPluginEntry {
public:
    void OnPluginStart() override;
};

extern CppTestPlugin g_testPlugin;
EXPOSE_PLUGIN(PLUGIN_API, &g_testPlugin)