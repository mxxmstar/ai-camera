# GB28181 模块依赖安装说明

## 概述

GB28181 模块需要 eXosip2 + osip2 作为 SIP 信令栈。以下是 Windows 环境下的安装步骤。

## 方法一：通过 vcpkg 安装（推荐）

### 1. 安装 vcpkg

如果尚未安装 vcpkg，请按以下步骤安装：

```powershell
# 克隆 vcpkg 仓库
git clone https://github.com/microsoft/vcpkg.git C:\vcpkg

# 运行引导脚本
C:\vcpkg\bootstrap-vcpkg.bat
```

### 2. 安装 eXosip2 和 osip2

```powershell
# 安装 32 位库（如果项目是 32 位）
C:\vcpkg\vcpkg install libosip2:x86-windows libexosip2:x86-windows

# 安装 64 位库（如果项目是 64 位）
C:\vcpkg\vcpkg install libosip2:x64-windows libexosip2:x64-windows
```

### 3. 配置 CMake 工具链

在构建项目时，指定 vcpkg 工具链文件：

```powershell
# 使用 CMake GUI 或命令行
cmake -B build -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake

# 或者使用环境变量
$env:CMAKE_TOOLCHAIN_FILE = "C:/vcpkg/scripts/buildsystems/vcpkg.cmake"
cmake -B build
```

## 方法二：通过源码编译

如果 vcpkg 不可用，可以从源码编译 eXosip2 和 osip2：

### 1. 下载源码

- osip2: https://ftp.gnu.org/gnu/osip/libosip2-5.3.1.tar.gz
- eXosip2: https://ftp.gnu.org/gnu/exosip/libeXosip2-5.3.1.tar.gz

### 2. 编译 osip2

```powershell
# 解压后，使用 CMake 或 autotools 编译
cd libosip2-5.3.1
mkdir build && cd build
cmake ..
cmake --build . --config Release
```

### 3. 编译 eXosip2

```powershell
# eXosip2 依赖 osip2，需要先编译 osip2
cd libeXosip2-5.3.1
mkdir build && cd build
cmake .. -DOSIP2_INCLUDE_DIR=path/to/osip2/include -DOSIP2_LIB_DIR=path/to/osip2/lib
cmake --build . --config Release
```

### 4. 配置 CMake 查找路径

在 CMakeLists.txt 中，可以通过 `-DCMAKE_PREFIX_PATH` 指定安装路径：

```powershell
cmake -B build -DCMAKE_PREFIX_PATH="path/to/osip2;path/to/exosip2"
```

## 验证安装

安装完成后，重新运行 CMake 配置，应该能看到：

```
-- Found osip2:  C:/vcpkg/installed/x64-windows/include
-- Found eXosip2: C:/vcpkg/installed/x64-windows/include
```

如果看到警告信息 "eXosip2/osip2 not found"，请检查安装路径和 CMake 配置。

## 注意事项

1. Windows 下 eXosip2 需要链接 `ws2_32` 库（项目已配置）
2. eXosip2 是 C 库，头文件需要用 `extern "C"` 包裹（后续实现时注意）
3. 如果使用的是 MinGW 编译器，可能需要从源码编译 eXosip2
