import("utils.archive")
import("download")

-- windows llvm
if (os.host() =="windows") then 
    local release_url = "SakuraEngine/llvm-package-windows/releases/download/llvm-14.0.6/llvm-14.0.6-windows-amd64-msvc17-msvcrt.7z"
    local zipname = "llvm-14.0.6-windows-amd64-msvc17-msvcrt.7z"
    download.file_from_github(release_url, zipname)
    archive.extract("SDKs/"..zipname, os.projectdir())
end
-- windows clang
if (os.host() =="windows") then 
    local release_url = "SakuraEngine/llvm-package-windows/releases/download/llvm-14.0.6/clang-14.0.6-windows-amd64-msvc17-msvcrt.7z"
    local zipname = "clang-14.0.6-windows-amd64-msvc17-msvcrt.7z"
    download.file_from_github(release_url, zipname)
    archive.extract("SDKs/"..zipname, os.projectdir())
end

-- macos llvm & clang
if (os.host() =="macosx") then 
    local release_url = "llvm/llvm-project/releases/download/llvmorg-15.0.1/clang+llvm-15.0.1-x86_64-apple-darwin.tar.xz"
    local zipname = "clang+llvm-15.0.1-x86_64-apple-darwin.tar.xz"
    download.file_from_github(release_url, zipname)
    archive.extract("SDKs/"..zipname, os.projectdir())
end
