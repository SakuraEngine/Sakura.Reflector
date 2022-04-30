# Build
1. 下载 llvm
2. 把代码放到 clang-tools-extra/clang-reflector 中
3. 修改 clang-tools-extra/CMakeLists.txt 添加 add_subdirectory(clang-reflector)
4. cmake 配置，配置参数带上 -DLLVM_ENABLE_PROJECTS='clang;clang-tools-extra'
5. 编译 clang-reflector 目标
6. 在 build/bin 中可以找到结果