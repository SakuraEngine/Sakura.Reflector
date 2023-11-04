import("utils.archive")
import("download")

if (os.host() =="windows") then 
    local windows_llvm_version = "17.0.1"
    -- windows llvm https://github.com/SakuraEngine/llvm-build/releases/download/llvm-17.0.1/llvm-17.0.1-msvc17-x64-md-release.7z
    local release_url = "SakuraEngine/llvm-build/releases/download/llvm-"..windows_llvm_version.."/llvm-"..windows_llvm_version.."-msvc17-x64-md-release.7z"
    local zipname = "llvm-"..windows_llvm_version.."-msvc17-x64-md-release.7z"
    download.file_from_github(release_url, zipname)
    archive.extract("SDKs/"..zipname, os.projectdir())
end

-- macos llvm & clang
if (os.host() =="macosx") then 
    local macosx_llvm_version = "17.0.1"
    local release_url = "SakuraEngine/llvm-build/releases/download/llvm-darwin-"..macosx_llvm_version.."/llvm-darwin-"..macosx_llvm_version.."-clang-x86_64-release.7z"
    local zipname = "llvm-darwin-"..macosx_llvm_version.."-clang-x86_64-release.7z"
    download.file_from_github(release_url, zipname)
    archive.extract("SDKs/"..zipname, os.projectdir())
end
