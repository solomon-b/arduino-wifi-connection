# Using MooreArduino in Other Projects

## Method 1: Flake Input (Recommended)

In your project's `flake.nix`:

```nix
{
  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixpkgs-unstable";
    flake-utils.url = "github:numtide/flake-utils";
    arduino-nix.url = "github:bouk/arduino-nix";
    arduino-index = {
      url = "github:bouk/arduino-indexes";
      flake = false;
    };
    # Add MooreArduino as input
    moore-arduino.url = "github:yourusername/moore-arduino";
  };

  outputs = { self, nixpkgs, flake-utils, arduino-nix, arduino-index, moore-arduino, ... }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = import nixpkgs {
          inherit system;
          overlays = [
            (arduino-nix.overlay)
            (arduino-nix.mkArduinoPackageOverlay (arduino-index + "/index/package_index.json"))
            (arduino-nix.mkArduinoLibraryOverlay (arduino-index + "/index/library_index.json"))
          ];
        };
      in {
        packages = {
          build = pkgs.writeShellScriptBin "build" ''
            SKETCH="''${1:-MySketch}"
            ${arduino-cli}/bin/arduino-cli compile --warnings all --fqbn arduino:mbed_giga:giga --libraries ${moore-arduino.packages.${system}.moore-arduino} $SKETCH
          '';

          arduino-cli = pkgs.wrapArduinoCLI {
            libraries = [
              # Include MooreArduino library
              moore-arduino.packages.${system}.moore-arduino
              # Add other libraries as needed
            ];
            packages = with pkgs.arduinoPackages; [
              platforms.arduino.mbed_giga."4.2.4"
            ];
          };
        };

        apps.build = flake-utils.lib.mkApp { drv = self.packages.${system}.build; };
      });
}
```

## Method 2: Direct Library Reference

You can also reference the library directly:

```nix
# In your flake.nix
arduino-cli = pkgs.wrapArduinoCLI {
  libraries = [
    moore-arduino.packages.${system}.moore-arduino
  ];
};
```

## Usage in Your Arduino Sketch

```cpp
#include <MooreArduino.h>
using namespace MooreArduino;

// Define your state machine
enum MyState { STATE_A, STATE_B };
enum MyInput { INPUT_NONE, INPUT_TICK };

MyState transition(const MyState& state, const MyInput& input) {
  // Your pure transition logic
  return state;
}

MooreMachine<MyState, MyInput> machine(transition, STATE_A);

void setup() {
  // Setup code
}

void loop() {
  // Your main loop
}
```

## Building Your Project

```bash
# Build your sketch
nix run '.#build' MySketch

# If you have upload capability
nix run '.#upload' MySketch
```