{
  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixpkgs-unstable";
    flake-utils.url = "github:numtide/flake-utils";
    arduino-nix.url = "github:bouk/arduino-nix";
    arduino-index = {
      url = "github:bouk/arduino-indexes";
      flake = false;
    };
  };

  outputs = {
    self,
    nixpkgs,
    flake-utils,
    arduino-nix,
    arduino-index,
    ...
  }@attrs:
  let
    overlays = [
      (arduino-nix.overlay)
      (arduino-nix.mkArduinoPackageOverlay (arduino-index + "/index/package_index.json"))
      (arduino-nix.mkArduinoPackageOverlay (arduino-index + "/index/package_rp2040_index.json"))
      (arduino-nix.mkArduinoLibraryOverlay (arduino-index + "/index/library_index.json"))
    ];
  in
  (flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = import nixpkgs {inherit system overlays; };
      in rec {
        devShells.default =
          pkgs.mkShell {
            packages = [
              pkgs.arduino-cli
              pkgs.dfu-util
              pkgs.screen
            ];
          };

        packages = rec {
          # Expose MooreArduino library for other projects
          moore-arduino = pkgs.stdenv.mkDerivation {
            pname = "moore-arduino";
            version = "1.0.0";
            src = ./MooreArduino;
            
            installPhase = ''
              mkdir -p $out
              cp -r * $out/
            '';
            
            meta = {
              description = "Moore finite state machine library for Arduino";
              homepage = "https://github.com/yourusername/moore-arduino";
              license = pkgs.lib.licenses.mit;
            };
          };
          build = pkgs.writeShellScriptBin "build" ''
            EXAMPLE="''${1:-WiFiManager}"
            echo "Building example: $EXAMPLE"
            ${arduino-cli}/bin/arduino-cli compile --warnings all --fqbn arduino:mbed_giga:giga --libraries . examples/$EXAMPLE
          '';

          load = pkgs.writeShellScriptBin "load" ''
            EXAMPLE="''${1:-WiFiManager}"
            echo "Building and uploading example: $EXAMPLE"
            ${arduino-cli}/bin/arduino-cli compile --warnings all --fqbn arduino:mbed_giga:giga --libraries . examples/$EXAMPLE
            ${arduino-cli}/bin/arduino-cli upload --port /dev/ttyACM0 --fqbn arduino:mbed_giga:giga examples/$EXAMPLE
          '';

          monitor = pkgs.writeShellScriptBin "monitor" ''
            ${arduino-cli}/bin/arduino-cli monitor -p /dev/ttyACM0 -- fqbn arduino:mbed_giga:giga
          '';

          arduino-cli = pkgs.wrapArduinoCLI {
            libraries = with pkgs.arduinoLibraries; [
              (arduino-nix.latestVersion ADS1X15)
             # (arduino-nix.latestVersion pkgs.arduinoLibraries."WiFiS3")
            ];

            packages = with pkgs.arduinoPackages; [
              platforms.arduino.mbed_giga."4.2.4"
            ];
          };
          
          # Arduino CLI with MooreArduino library included
          arduino-cli-with-moore = pkgs.wrapArduinoCLI {
            libraries = with pkgs.arduinoLibraries; [
              (arduino-nix.latestVersion ADS1X15)
            ] ++ [ moore-arduino ];

            packages = with pkgs.arduinoPackages; [
              platforms.arduino.mbed_giga."4.2.4"
            ];
          };
        };

        apps = {
          build = flake-utils.lib.mkApp { drv = self.packages.${system}.build; };
          load = flake-utils.lib.mkApp { drv = self.packages.${system}.load; };
          monitor = flake-utils.lib.mkApp { drv = self.packages.${system}.monitor; };
        };
      }
    ));
}
