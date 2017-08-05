-- Enable JIT
newoption {
   trigger     = "enable-jit",
   description = "Enable clang/LLVM JIT."
}


-- LLVM/Clang path
newoption {
   trigger     = "llvm-config",
   value       = "PATH",
   description = "Path to llvm-config."
}

-- clang
newoption {
   trigger     = "clang",
   description = "Use clang."
}

-- Address-sanitizerr
newoption {
   trigger     = "with-asan",
   description = "Use Address sanitizer(recent clang or gcc only)"
}

if _OPTIONS["clang"] then
   toolset "clang"
end

workspace "SoftCompute"
   configurations { "Release", "Debug" }
   if os.is("windows") then
      platforms { "x64", "x32" }
   else
      platforms { "native", "x64", "x32" }
   end


include "src"

include "test"
