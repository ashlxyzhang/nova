# NOVA PHASE 2
Neuromorphic Optics and Visualization Application.
Welcome to phase 2 of NOVA. This phase includes a rewrite of NOVA that follows a more modular architectural paradigm with streaming support.

# Images
![Image of NOVA with test file](images/NOVAbasic.PNG)
![Image of NOVA with frame data](images/NOVAframes.PNG)
![Image of NOVA with streamed camera data](images/NOVAcamera.PNG)

# Installing For Users
To install NOVA for users, go to the latest release and download the zip file of the release. Unzip the downloaded zip file and launch NOVA.exe inside the unzipped file.

# Installing For Developers

## Windows
### CMake
NOVA uses CMake as the build system. [CMake install instructions](https://cmake.org/download/)
### vcpkg
NOVA on Windows requires the vcpkg package manager due to the dv-processing dependency. [vcpkg install instructions](https://learn.microsoft.com/en-us/vcpkg/get_started/get-started?pivots=shell-powershell)
### clang
NOVA uses LLVM's Clang/Clang++ as the compiler. Please install it with the following.
```
winget install LLVM -i
```
Make sure to select the add to path option.

## Linux
### CMake
NOVA uses CMake as the build system. You can probably install it using your distro's package manager. Otherwise: [CMake install instructions](https://cmake.org/download/)
### Package manager
Most of the packages NOVA needs will be pulled automatically through CMake's FetchContent.
However depending on the system, some internal libraries used by our dependencies may need
to be manually installed. For example SDL3 will require some library to be able to create
the native window, so wayland devel may be needed.
### clang
The clang compiler will need to be installed, the easiest way is to use the one provided
with your distro's package manager.

## CMake Commands

### List all presets available for a system
```
cmake --list-presets
```

### Generate build files
```
cmake --preset preset-name
```
Where preset-name is one of the presets given in the previous step.

### Invoke the build
```
cmake --build ./build --parallel
```
