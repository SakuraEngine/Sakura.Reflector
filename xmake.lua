add_rules("mode.debug", "mode.release")

set_languages("c11", "cxx17")

if (is_os("windows")) then 

target("meta")
    set_runtimes("MD")
    set_kind("binary")
    add_files("src/**.cpp")
    add_cxflags("clang::-Wno-c++11-narrowing", "-fno-rtti", {force = true})
    add_links("lib/**")
    add_links("Version", "advapi32", "Shcore", "user32", "shell32", "Ole32", {public = true})
    add_includedirs("include")

else

add_requires("zstd")
target("meta")
    set_kind("binary")
    add_files("**.cpp")
    add_cxflags("-Wno-c++11-narrowing")
    add_cxflags("-fno-rtti")
    add_syslinks("pthread", "curses")
    add_linkdirs("lib")
    add_includedirs("include")
    add_packages("zstd")
    on_load(function (target, opt)
        local libs = {}
        local p = "lib/lib*.a"
        for __, filepath in ipairs(os.files(p)) do
            local basename = path.basename(filepath)
            local matchname = string.match(basename, "lib(.*)$")
            table.insert(libs, matchname or basename)
        end
        target:add("links", libs)
    end)
    
end
