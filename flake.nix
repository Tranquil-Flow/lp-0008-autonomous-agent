{
  description = "Autonomous AI Agent Module for Logos Core (LP-0008)";

  inputs = {
    logos-module-builder.url = "github:logos-co/logos-module-builder";
    storage_module.url = "github:logos-co/logos-storage-module";
    delivery_module.url = "github:logos-co/logos-delivery-module";
  };

  outputs = inputs@{ logos-module-builder, ... }:
    logos-module-builder.lib.mkLogosModule {
      src = ./.;
      configFile = ./metadata.json;
      flakeInputs = inputs;

      # Work around current logos-module-builder generated dependency SDK bug:
      # LIDL dependency wrappers expose constructors taking LogosAPI*, but
      # generated logos_sdk.h initializes them with the consuming module name.
      # The generated C ABI context hook also constructs LogosModules without
      # access to the host LogosAPI*, which segfaults in real Logos Core.
      # This module uses logos::LpClient directly for optional live calls, so
      # keep the generated modules() pointer null until upstream generator can
      # pass the host API pointer through this path correctly.
      preConfigure = ''
        if [ -f generated_code/include/logos_sdk.h ]; then
          substituteInPlace generated_code/include/logos_sdk.h \
            --replace 'LogosModules() : storage_module("agent_module"),' \
                      'explicit LogosModules(LogosAPI* api = nullptr) : storage_module(api),' \
            --replace '        delivery_module("agent_module") {}' \
                      '        delivery_module(api) {}'
        fi
        if [ -f generated_code/agent_module_module_impl.cpp ]; then
          substituteInPlace generated_code/agent_module_module_impl.cpp \
            --replace '_logos_codegen_::maybeSetLogosModules(lidlImpl(), new LogosModules());' \
                      '_logos_codegen_::maybeSetLogosModules(lidlImpl(), nullptr);'
        fi
      '';
    };
}
