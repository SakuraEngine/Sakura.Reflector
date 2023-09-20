add_rules("mode.debug", "mode.release")

set_languages("c11", "cxx17")

if (is_os("windows")) then 

local windows_llvm_version = "17.0.1"
target("meta")
    set_runtimes("MD")
    set_kind("binary")
    add_files("src/**.cpp")
    add_cxflags("clang::-Wno-c++11-narrowing", "-fno-rtti", {force = true})
    add_links("lib/**")
    add_links("Version", "advapi32", "Shcore", "user32", "shell32", "Ole32", {public = true})
    -- llvm & clang
    on_load(function (target, opt)
        local libs = {}
        local p = "llvm-"..windows_llvm_version.."-windows-amd64-msvc17-msvcrt/lib/*.lib"
        for __, filepath in ipairs(os.files(p)) do
            local basename = path.basename(filepath)
            table.insert(libs, basename)
        end
        local p2 = "clang-"..windows_llvm_version.."-windows-amd64-msvc17-msvcrt/lib/*.lib"
        for __, filepath in ipairs(os.files(p2)) do
            local basename = path.basename(filepath)
            table.insert(libs, basename)
        end
        target:add("links", libs)
    end)
    add_includedirs("include")
else

target("meta")
    set_kind("binary")
    add_files("**.cpp")
    add_cxflags("-Wno-c++11-narrowing")
    add_cxflags("-fno-rtti")
    add_syslinks("pthread", "curses")
    add_linkdirs("clang+llvm-17.0.1-x86_64-apple-darwin/lib")
    on_load(function (target, opt)
        local libs = {}
        local p = "clang+llvm-17.0.1-x86_64-apple-darwin/lib/libLLVM*.a"
        for __, filepath in ipairs(os.files(p)) do
            local basename = path.basename(filepath)
            local matchname = string.match(basename, "lib(.*)$")
            table.insert(libs, matchname or basename)
        end
        local p2 = "clang+llvm-17.0.1-x86_64-apple-darwin/lib/libclang*.a"
        for __, filepath in ipairs(os.files(p2)) do
            local basename = path.basename(filepath)
            local matchname = string.match(basename, "lib(.*)$")
            table.insert(libs, matchname or basename)
        end
        target:add("links", libs)
    end)
    add_includedirs("clang+llvm-17.0.1-x86_64-apple-darwin/include")
    
end
