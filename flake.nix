{
  description = "RISC-V Spike ISA Simulator";

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
        packages.default = pkgs.stdenv.mkDerivation {
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

          meta = with pkgs.lib; {
            description = "RISC-V Spike ISA Simulator";
            homepage = "https://github.com/KINGFIOX/riscv-isa-sim";
            license = licenses.bsd3;
            platforms = platforms.unix;
          };
        };

        devShells.default = pkgs.mkShell {
          inputsFrom = [ self.packages.${system}.default ];
        };
      }
    ) // {
      overlays.default = final: prev: {
        spike = self.packages.${final.system}.default;
      };
    };
}
