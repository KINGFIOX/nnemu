{
  description = "RISC-V Spike ISA Simulator (static libs for linking)";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, flake-utils }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = nixpkgs.legacyPackages.${system};
        version = self.shortRev or self.dirtyShortRev or "dirty";
      in
      {
        packages = rec {
          default = spike;

          spike = pkgs.stdenv.mkDerivation {
            pname = "spike";
            inherit version;
            src = pkgs.lib.cleanSource self;

            nativeBuildInputs = with pkgs; [ cmake ninja pkg-config dtc python3 ];

            cmakeBuildType = "Release";
            cmakeFlags = [
              "-DBUILD_SHARED_LIBS=OFF"
              "-Dsoftfloat_pa=ON"
              "-Ddefault_isa=RV64IMAFDC_zicntr_zihpm"
              "-Ddefault_priv=MSU"
              "-Ddefault_varch=vlen:128,elen:64"
            ];

            postInstall = ''
              mkdir -p $out/nix-support
              echo 'export SPIKE_HOME='"$out" > $out/nix-support/setup-hook
            '';

            meta = with pkgs.lib; {
              description = "RISC-V Spike ISA Simulator";
              homepage = "https://github.com/KINGFIOX/riscv-isa-sim";
              license = licenses.bsd3;
              platforms = platforms.unix;
            };
          };

          static = pkgs.runCommand "spike-static" { } ''
            mkdir -p $out/{lib,include,nix-support}
            cp ${spike}/lib/*.a $out/lib/
            cp -r ${spike}/include/* $out/include/
            echo 'export SPIKE_HOME='"$out" > $out/nix-support/setup-hook
          '';
        };

        devShells.default = pkgs.mkShell {
          inputsFrom = [ self.packages.${system}.default ];
          shellHook = ''
            export SPIKE_HOME="$(pwd)"
          '';
        };
      }
    ) // {
      overlays.default = final: prev: {
        spike = self.packages.${final.system}.default;
        spike-static = self.packages.${final.system}.static;
      };
    };
}
