import("utils.archive")
import("download")

if (os.host() =="windows") then 
    local windows_llvm_version = "17.0.6"
    local release_url = "SakuraEngine/llvm-build/releases/download/llvm-windows-"..windows_llvm_version.."/llvm-windows-"..windows_llvm_version.."-msvc-x64-md-release.7z"
    local zipname = "llvm-windows-"..windows_llvm_version.."-msvc-x64-md-release.7z"
    download.file_from_github(release_url, zipname)
    archive.extract("SDKs/"..zipname, os.projectdir())
end

-- macos llvm & clang
if (os.host() =="macosx") then
    if (os.arch() == "x86_64") then
        arch_string = "x86_64"
    else
        arch_string = "arm64"
    end 
    local macosx_llvm_version = "17.0.6"
    local release_url = "SakuraEngine/llvm-build/releases/download/llvm-darwin-"..macosx_llvm_version.."/llvm-darwin-"..macosx_llvm_version.."-clang-"..arch_string.."-release.7z"
    local zipname = "llvm-darwin-"..macosx_llvm_version.."-clang-"..arch_string.."-release.7z"
    download.file_from_github(release_url, zipname)
    archive.extract("SDKs/"..zipname, os.projectdir())
end
