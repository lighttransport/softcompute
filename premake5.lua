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

-- SPIRV-Cross path
newoption {
   trigger     = "spirv-cross",
   value       = "PATH",
   description = "Path to SPIRV-Cross."
}

-- Address-sanitizerr
newoption {
   trigger     = "with-asan",
   description = "Use Address sanitizer(recent clang or gcc only)"
}

sources = {
   "src/main.cc"
 , "src/OptionParser.cpp"
}



-- premake4.lua
workspace "SoftCompute"
   configurations { "Release", "Debug" }


-- A project defines one build target
project "SoftCompute"
   kind "ConsoleApp"
   language "C++"
   files { sources }

   includedirs {
      "./",
      "./include"
   }

   if _OPTIONS['enable-jit'] then
      defines { 'ENABLE_JIT' }
      files { 'src/jit-engine.cc' }
   else
      files { 'src/dll-engine.cc' }
   end

   llvm_config = "llvm-config"
   if _OPTIONS['llvm-config'] then
      llvm_config = _OPTIONS['llvm-config']
   end

   spirv_cross_path = "../SPIRV-Cross/"
   if _OPTIONS['spirv_cross'] then
      spirv_cross_path = _OPTIONS['spirv_cross']
   end

   includedirs { spirv_cross_path }
   includedirs { spirv_cross_path .. '/include' }

   -- MacOSX. Guess we use gcc.
   configuration { "macosx" }

      buildoptions { '-std=c++11' }

      -- glm
      includedirs { '/usr/local/include' }

      if _OPTIONS['enable-jit'] then
         -- includedirs { "`" .. llvm_config .. " --includedir`" }
         buildoptions { "`" .. llvm_config .. " --cxxflags`" }
         -- For XCode7 + El Capitan
         buildoptions { '-isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.11.sdk' }

         linkoptions { "`" .. llvm_config .. " --ldflags --libs --system-libs`" }
         links { "clangFrontend", "clangSerialization", "clangDriver", "clangCodeGen"
               , "clangParse", "clangSema", "clangStaticAnalyzerFrontend"
               , "clangStaticAnalyzerCheckers", "clangStaticAnalyzerCore"
               , "clangAnalysis", "clangRewrite", "clangRewriteFrontend"
               , "clangEdit", "clangAST", "clangLex", "clangBasic"
               }
      end

      linkoptions { "-lpthread" }

   -- Windows specific
   --configuration { "windows" }

   --   flags { "EnableSSE2" }

   --   defines { 'NOMINMAX', '_LARGEFILE_SOURCE', '_FILE_OFFSET_BITS=64' }
   --   defines { '__STDC_CONSTANT_MACROS', '__STDC_LIMIT_MACROS' } -- c99

   --   includedirs { _OPTIONS['llvm-path'] .. "/include" }
   --   libdirs { _OPTIONS['llvm-path'] .. "/lib" }

   --   links { "clangFrontend", "clangSerialization", "clangDriver", "clangCodeGen"
   --         , "clangParse", "clangSema", "clangStaticAnalyzerFrontend"
   --         , "clangStaticAnalyzerCheckers", "clangStaticAnalyzerCore"
   --         , "clangAnalysis", "clangRewriteFrontend", "clangRewrite"
   --         , "clangEdit", "clangAST", "clangLex", "clangBasic"
   --         }

   --   links { "LLVMAsmParser"
   --         , "LLVMInstrumentation"
   --         , "LLVMProfileData"
   --         , "LLVMLinker"
   --         , "LLVMLTO"
   --         , "LLVMBitReader"
   --         , "LLVMDebugInfo"
   --         , "LLVMMC"
   --         , "LLVMMCDisassembler"
   --         , "LLVMMCJIT"
   --         , "LLVMObject"
   --         , "LLVMipo", "LLVMVectorize", "LLVMBitWriter"
   --         , "LLVMX86AsmParser", "LLVMX86Disassembler", "LLVMX86CodeGen"
   --         , "LLVMSelectionDAG", "LLVMAsmPrinter"
   --         , "LLVMIRReader"
   --         , "LLVMX86Desc", "LLVMX86Info", "LLVMX86AsmPrinter", "LLVMX86Utils"
   --         , "LLVMMCParser", "LLVMCodeGen", "LLVMObjCARCOpts", "LLVMScalarOpts", "LLVMInstCombine"
   --         , "LLVMTransformUtils", "LLVMipa", "LLVMAnalysis"
   --         , "LLVMRuntimeDyld", "LLVMExecutionEngine"
   --         , "LLVMTarget", "LLVMMC", "LLVMObject", "LLVMCore", "LLVMSupport", "LLVMOption",
   --         }

   --   links { "psapi", "imagehlp" }

   -- Linux specific
   configuration {"linux"}
      defines { '__STDC_CONSTANT_MACROS', '__STDC_LIMIT_MACROS' } -- c99

      buildoptions { '-std=c++11' }
      buildoptions { "-pthread" }
      --buildoptions { '-stdlib=libc++' }

      if _OPTIONS['with-asan'] then
         buildoptions { "-fsanitizer=address -fno-omit-frame-pointer" }
         linkoptions { "-fsanitizer=address" }
      end

      if _OPTIONS['enable-jit'] then
         -- Strip "-O3 -NDEBUG"
         buildoptions { "`" .. llvm_config .. " --cxxflags | sed -e 's/-O3 -NDEBUG//g'`" }
         linkoptions { "-lclangFrontend", "-lclangSerialization", "-lclangDriver", "-lclangCodeGen"
               , "-lclangParse", "-lclangSema", "-lclangStaticAnalyzerFrontend"
               , "-lclangStaticAnalyzerCheckers", "-lclangStaticAnalyzerCore"
               , "-lclangAnalysis", "-lclangRewriteFrontend", "-lclangRewrite"
               , "-lclangEdit", "-lclangAST", "-lclangLex", "-lclangBasic"
               }
         linkoptions { "`" .. llvm_config .. " --ldflags --libs --system-libs`" }
      else
         links { 'pthread', 'dl' }
      end


   configuration "Debug"
      defines { "DEBUG" } -- -DDEBUG
      flags { "Symbols" }
      targetdir "bin"
      targetname "softcompute_debug"

   configuration "Release"
      -- defines { "NDEBUG" } -- -NDEBUG
      flags { "Symbols", "Optimize" }
      targetdir "bin"

      targetname "softcompute"

   
