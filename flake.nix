{
  description = "Manifesto app (unofficial): read and sign the Logos manifesto in Basecamp, with a signature anyone can check";

  inputs = {
    logos-module-builder.url = "github:logos-co/logos-module-builder";
    # Optional at run time (metadata.json optional_dependencies); needed at
    # build time to generate the typed client.
    signer_manager_module.url = "github:logos-co/logos-signer-manager-module";
  };

  outputs = inputs@{ logos-module-builder, ... }:
    logos-module-builder.lib.mkLogosQmlModule {
      src = ./.;
      configFile = ./metadata.json;
      flakeInputs = inputs;
    };
}
