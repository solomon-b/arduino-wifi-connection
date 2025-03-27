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
          build = pkgs.writeShellScriptBin "build" ''
            ${arduino-cli}/bin/arduino-cli compile --fqbn arduino:mbed_giga:giga .
          '';

          load = pkgs.writeShellScriptBin "load" ''
            ${arduino-cli}/bin/arduino-cli compile --fqbn arduino:mbed_giga:giga .
            ${arduino-cli}/bin/arduino-cli upload --port /dev/ttyACM0 --fqbn arduino:mbed_giga:giga .
          '';

          monitor = pkgs.writeShellScriptBin "monitor" ''
            # Set your serial device and baud rate
            DEVICE="/dev/ttyACM0"
            BAUD="9600"

            # Use stty to configure the serial port
            stty -F "$DEVICE" cs8 "$BAUD" igncr -ixon -icanon -echo

            # Trap Ctrl-C (SIGINT) to clean up
            cleanup() {
              echo -e "\nExiting..."
              stty -F "$DEVICE" sane
              exit 0
            }
            trap cleanup SIGINT

            # Read from the serial port
            cat "$DEVICE"
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
        };

        apps = {
          build = flake-utils.lib.mkApp { drv = self.packages.${system}.build; };
          load = flake-utils.lib.mkApp { drv = self.packages.${system}.load; };
          monitor = flake-utils.lib.mkApp { drv = self.packages.${system}.monitor; };
        };
      }
    ));
}
