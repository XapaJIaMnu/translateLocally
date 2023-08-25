{
  inputs = {
    # GCC 12.2 in nixos 23.05 has some regression preventing building.
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
    flaky-utils.url = "git+https://cgit.pacien.net/libs/flaky-utils";
  };

  outputs = { self, nixpkgs, flake-utils, flaky-utils }:
  flake-utils.lib.eachDefaultSystem (system: let
    pkgs = import nixpkgs { inherit system; };
  in pkgs.lib.fold pkgs.lib.recursiveUpdate { } [

    {
      devShell = flaky-utils.lib.mkDevShell {
        inherit pkgs;

        tools = with pkgs; [
          # nativeBuildInputs
          cmake
          protobuf
          qt6.wrapQtAppsHook

          # buildInputs
          qt6.qttools
          qt6.qtbase
          qt6.qtsvg
          libarchive
          pcre2
          protobuf
          gperftools  # provides tcmalloc
          blas
        ];

        prePrompt = ''
          cat << EOP
          <C-d> to exit this development shell.

          Compiling from sources:
            mkdir build
            cd build
            cmake -DBLAS_LIBRARIES=-lblas -DCBLAS_LIBRARIES=-lcblas ..
            make -j4
            ./translateLocally --help
          EOP
        '';

        shell = null;
      };
    }

  ]);
}
