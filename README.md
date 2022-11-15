# Build
## XMake (Recommended)

- macos 上使用 LLVM 发布的预编译二进制包进行链接和构建
- windows 上使用组织仓库的 LLVM 预编译副本进行构建

``` bash
xmake setup.lua
xmake build
```

提供了 Github Actions，产出物会被上传到 Action 的 Artifacts 中。

## CMake & LLVM InSource
1. 下载 llvm
2. 把代码放到 clang-tools-extra/clang-reflector 中
3. 修改 clang-tools-extra/CMakeLists.txt 添加 add_subdirectory(clang-reflector)
4. cmake 配置，配置参数带上 -DLLVM_ENABLE_PROJECTS='clang;clang-tools-extra'
5. 编译 clang-reflector 目标
6. 在 build/bin 中可以找到结果