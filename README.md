# Simple Hotkeys
This plugin made with Example plugin template for use with CommonLibSSE by [Ryan-rsm-McKenzie.](https://github.com/Ryan-rsm-McKenzie/ExamplePlugin-CommonLibSSE)

SKSE64 Plugin made for automatically using Soulgems with enchanted weapons.

## Requirements
* [CMake](https://cmake.org/)
	* Add this to your `PATH`
* [PowerShell](https://github.com/PowerShell/PowerShell/releases/latest)
* [Vcpkg](https://github.com/microsoft/vcpkg)
	* Add the environment variable `VCPKG_ROOT` with the value as the path to the folder containing vcpkg
* [Visual Studio Community 2019](https://visualstudio.microsoft.com/)
	* Desktop development with C++

## Register Visual Studio as a Generator
* Open `x64 Native Tools Command Prompt`
* Run `cmake`
* Close the cmd window

## Building
```
git clone https://github.com/neogulcity/AutoUseSoulgemsSSE
cd AutoUseSoulgemsSSE
cmake -B build -S .
```
