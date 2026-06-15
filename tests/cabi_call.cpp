#include <dlfcn.h>
#include <cstdio>
#include <cstdlib>
#include <string>

using dispatch_fn = char* (*)(const char*, const char*);
using free_fn = void (*)(char*);

int main(int argc, char** argv)
{
    if (argc != 4) {
        std::fprintf(stderr, "usage: %s <plugin.dylib> <method> <args-json-array>\n", argv[0]);
        return 2;
    }

    const char* plugin_path = argv[1];
    const char* method = argv[2];
    const char* args_json = argv[3];

    void* handle = dlopen(plugin_path, RTLD_NOW);
    if (!handle) {
        std::fprintf(stderr, "dlopen failed: %s\n", dlerror());
        return 1;
    }

    auto dispatch = reinterpret_cast<dispatch_fn>(dlsym(handle, "logos_module_dispatch"));
    auto free_string = reinterpret_cast<free_fn>(dlsym(handle, "logos_module_string_free"));
    if (!dispatch || !free_string) {
        std::fprintf(stderr, "required C ABI symbols missing: %s\n", dlerror());
        return 1;
    }

    char* result = dispatch(method, args_json);
    if (!result) {
        std::fprintf(stderr, "method returned NULL\n");
        return 1;
    }

    std::printf("%s\n", result);
    free_string(result);
    return 0;
}
