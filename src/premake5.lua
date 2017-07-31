sources = {
   "main.cc"
 , "softgl.cc"
 , "OptionParser.cpp"
 , "loguru-impl.cc"
 -- SPIRV-Cross
 , "../third_party/SPIRV-Cross/spirv_cross.cpp"
 , "../third_party/SPIRV-Cross/spirv_cfg.cpp"
 , "../third_party/SPIRV-Cross/spirv_glsl.cpp"
}


project "SoftCompute"
   kind "StaticLib"
   language "C++"
   files { sources }

   includedirs {
      "../",
      "../include",
      "../third_party/glm"
   }

   if _OPTIONS['enable-jit'] then
      defines { 'SOFTCOMPUTE_ENABLE_JIT' }
      files { 'jit-engine.cc' }
   else
      files { 'dll-engine.cc' }
   end

   llvm_config = "llvm-config"
   if _OPTIONS['llvm-config'] then
      llvm_config = _OPTIONS['llvm-config']
   end

   -- SPIRV-Cross
   spirv_cross_path = "../third_party/SPIRV-Cross/" -- path to SPIRV-Cross(submodule)

   includedirs { spirv_cross_path }
   includedirs { spirv_cross_path .. '/include' }

   -- Loguru
   includedirs { "../third_party/" }

   flags { "c++11" }

   -- Disable exception(for SPIRV-Cross code)
   defines { "SPIRV_CROSS_EXCEPTIONS_TO_ASSERTIONS=1" }

   -- MacOSX. Guess we use gcc.
   configuration { "macosx" }

      -- Assume clang
      buildoptions { "-Weverything -Werror -Wno-padded -Wno-c++98-compat -Wno-c++98-compat-pedantic" } 


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

   -- MinGW
   configuration { "windows", "gmake" }

      defines { '__STDC_CONSTANT_MACROS', '__STDC_LIMIT_MACROS' } -- c99
      buildoptions { '-std=c++11' }
