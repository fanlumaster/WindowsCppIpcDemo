# WindowsCppIpcDemo

![](https://s2.loli.net/2025/04/15/Mgs2TyOeB4UFGqN.png)

## Build and Run

Prerequisites:

- Visual Studio 2022
- CMake
- vcpkg
- Ninja
- NuGet

### Build and Run FirstProcessWindow

```powershell
cd FirstProcessWindow
.\scripts\llaunch.ps1
```

### Build and Run SecondProcessWindow

```powershell
cd SecondProcessWindow
.\scripts\llaunch.ps1
```

If you want to use tentent translate api to translate the text, please create a folder named keys in the SecondProcessWindow folder, and create id.txt and key.txt and put the content of id.txt and key.txt in the file.

For the second process window, you need to change the path of vcpkg int the CMakePreset.json file, you can change it to the path of the vcpkg in your computer.

Also, you need to install Webview2 using nuget.
