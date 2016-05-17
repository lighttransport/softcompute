#include <cstdio>
#include <map>
#include <vector>
#include <iostream>

#include "clang/CodeGen/CodeGenAction.h"
#include "clang/Basic/DiagnosticOptions.h"
#include "clang/Driver/Compilation.h"
#include "clang/Driver/Driver.h"
#include "clang/Driver/Tool.h"
#include "clang/Frontend/CompilerInvocation.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendDiagnostic.h"
#include "clang/Frontend/TextDiagnosticPrinter.h"

#include "llvm/ADT/SmallString.h"

#include "llvm/ExecutionEngine/ExecutionEngine.h"
#include "llvm/ExecutionEngine/ObjectCache.h"
#include "llvm/ExecutionEngine/MCJIT.h"
#include "llvm/ExecutionEngine/SectionMemoryManager.h"

#include "llvm/IR/Module.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/IRBuilder.h"

#include "llvm/IRReader/IRReader.h"

#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Host.h"
#include "llvm/Support/ManagedStatic.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/SourceMgr.h" // SMDiagnostic
#include "llvm/Support/raw_ostream.h"

using namespace clang;
using namespace clang::driver;
using namespace llvm;

#include "jit-engine.h"
#include "spirv_cross/external_interface.h"

static std::string GetFileExtension(const std::string &FileName) {
  if (FileName.find_last_of(".") != std::string::npos)
    return FileName.substr(FileName.find_last_of(".") + 1);
  return "";
}

static void *CustomSymbolResolver(const std::string &name) {
  // @todo
  printf("[Shader] Resolving %s\n", name.c_str());

  std::cout << "Failed to resolve symbol : " << name << "\n";
  return NULL; // fail
}

class ShaderJITMemoryManager : public SectionMemoryManager {
  ShaderJITMemoryManager(const ShaderJITMemoryManager &) ;
  void operator=(const ShaderJITMemoryManager &);

public:
  ShaderJITMemoryManager() {}
  virtual ~ShaderJITMemoryManager() {}

  /// Implements shader function symbol resolver.
  virtual void *getPointerToNamedFunction(const std::string &Name,
                                          bool AbortOnFailure = true);

private:

};

void *ShaderJITMemoryManager::getPointerToNamedFunction(const std::string &Name,
                                                        bool AbortOnFailure) {
  void *pfn = CustomSymbolResolver(Name);
  if (!pfn && AbortOnFailure) {
    report_fatal_error("Program used external function '" + Name +
                       "' which could not be resolved!");
    return NULL;
  }
  return pfn;
}

std::string GetExecutablePath(const char *Argv0) {
  // This just needs to be some symbol in the binary; C++ doesn't
  // allow taking the address of ::main however.
  void *MainAddr = (void *)(intptr_t) GetExecutablePath;
  return llvm::sys::fs::getMainExecutable(Argv0, MainAddr);
}

namespace softcompute {

class ShaderInstance::Impl {
public:
  Impl();
  ~Impl();

  bool Compile(const std::string& type, const std::vector<std::string>& paths, const std::string& filename);
  void *GetInterface();

private:
  llvm::Function *EntryFn;
  std::unique_ptr<llvm::Module> Module;
  llvm::ExecutionEngine *EE;

  void *EntryPoint;
};

ShaderInstance::Impl::Impl()
: EntryFn(NULL),
  EE(NULL)
{

}

ShaderInstance::Impl::~Impl()
{ 
  EntryFn = NULL;
  EE = NULL;
}

bool ShaderInstance::Impl::Compile(
  const std::string& type,
  const std::vector<std::string>& paths,
  const std::string &filename)
{
  std::string ext = GetFileExtension(filename);

  std::string abspath = filename; // @fixme
  if (abspath.empty()) {
    fprintf(stderr, "[ShaderEngine] File not found in the search path: %s.\n", filename.c_str());
    return false;
  }

  // Init
  EntryFn = NULL;
  EE = NULL;
  CodeGenAction* Act = NULL;

  void *MainAddr = (void *)(intptr_t) GetExecutablePath;
  std::string Path = GetExecutablePath("./");
  //llvm::errs() << Path.str() << "\n";

  std::string stringBuffer;
  llvm::raw_string_ostream ss(stringBuffer);
  IntrusiveRefCntPtr<DiagnosticOptions> DiagOpts = new DiagnosticOptions();
  TextDiagnosticPrinter *DiagClient = new TextDiagnosticPrinter(ss, &*DiagOpts);

  llvm::IntrusiveRefCntPtr<DiagnosticIDs> DiagID(new DiagnosticIDs());
  DiagnosticsEngine Diags(DiagID, &*DiagOpts, DiagClient);

#ifdef _WIN32
  // For clang/LLVM 3.5+, we neeed to add "-elf" suffix to the target triple.
  // http://the-ravi-programming-language.readthedocs.org/en/latest/llvm-notes.html
  std::string triple = llvm::sys::getDefaultTargetTriple() + "-elf";	
#else
  std::string triple = llvm::sys::getDefaultTargetTriple();
#endif

  Driver TheDriver(Path, triple, Diags);
  TheDriver.setTitle("clang interpreter");

  // FIXME: This is a hack to try to force the driver to do something we can
  // recognize. We need to extend the driver library to support this use model
  // (basically, exactly one input, and the operation mode is hard wired).
  llvm::SmallVector<const char *, 16> Args;
  //filename.c_str());
  Args.push_back("<clang>");            // argv[0]
  //Args.push_back("-nostdinc");          // Disable stdinc
  //Args.push_back("-D__LTE_CLSHADER__"); // CL shader
  Args.push_back(abspath.c_str());
  Args.push_back("-x");
  Args.push_back("c++");
  Args.push_back("-fsyntax-only");
  Args.push_back("-fno-stack-protector"); // Avoid unresolved __stack_chk_fail symbol error in musl libc environment.

  Args.push_back("-std=c++11");

  Args.push_back("-fno-exceptions");
  Args.push_back("-fno-rtti");
  Args.push_back("-D_GNU_SOURCE");
  Args.push_back("-D__STDC_CONSTANT_MACROS");
  Args.push_back("-D__STDC_FORMAT_MACROS");
  Args.push_back("-D__STDC_LIMIT_MACROS");

  // Add current dir to path
  Args.push_back("-I.");
  Args.push_back("-Ispirv_cross");
  Args.push_back("-I/home/syoyo/local/clang+llvm-3.8.0-linux-x86_64-centos6/include/c++/v1"); // HACK
  Args.push_back("-I/home/syoyo/local/clang+llvm-3.8.0-linux-x86_64-centos6/lib/clang/3.8.0/include"); // HACK


  // OSX
  Args.push_back("-isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.11.sdk");

  Args.push_back("-I/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/include/c++/v1");
  Args.push_back("-I/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/lib/clang/7.3.0/include");
  Args.push_back("-I/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/include");
  Args.push_back("-I/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.11.sdk/usr/include/");
  //Args.push_back("-I/Users/syoyo/local/clang+llvm-3.8.0-x86_64-apple-darwin/include/c++/v1");
  //Args.push_back("-I/Users/syoyo/local/clang+llvm-3.8.0-x86_64-apple-darwin/lib/clang/3.8.0/include");
  Args.push_back("-I/usr/local/include"); // homebrew

  //Args.push_back("-I./tmp");
  //Args.push_back("-I./shaders");
  //Args.push_back("-I./tmp");

  std::vector<std::string> searchPathArgs;

  // Add search path
  for (size_t i = 0; i < paths.size(); i++) {
    std::string incPath = std::string("-I") + paths[i];
    searchPathArgs.push_back(incPath);
  }

  for (size_t i = 0; i < searchPathArgs.size(); i++) {
    Args.push_back(searchPathArgs[i].c_str()); // NOTE: reference a pointer value.
  }

  for (size_t i = 0; i < Args.size(); i++) {
    printf("arg: %s\n", Args[i]);
  }

  std::unique_ptr<Compilation> C(TheDriver.BuildCompilation(Args));

  if (!C) {
    fprintf(stderr, "[ShaderEngine] Failed to create compilation.\n");
    return false;
  }

  // FIXME: This is copied from ASTUnit.cpp; simplify and eliminate.

  // We expect to get back exactly one command job, if we didn't something
  // failed. Extract that job from the compilation.
  const driver::JobList &Jobs = C->getJobs();
  if (Jobs.size() != 1 || !isa<driver::Command>(*Jobs.begin())) {
    llvm::SmallString<256> Msg;
    llvm::raw_svector_ostream OS(Msg);
    Jobs.Print(OS, "; ", true);
    Diags.Report(diag::err_fe_expected_compiler_job) << OS.str();
    printf("job error\n");
    return false;
  }

  const driver::Command &Cmd = cast<driver::Command>(*Jobs.begin());
  if (llvm::StringRef(Cmd.getCreator().getName()) != "clang") {
    Diags.Report(diag::err_fe_expected_clang_command);
    printf("clang error\n");
    return false;
  }

  // Create a compiler instance to handle the actual work.
  //OwningPtr<CompilerInstance> Clang(new CompilerInstance());
  CompilerInstance *Clang = new CompilerInstance();

  // Initialize a compiler invocation object from the clang (-cc1) arguments.
  const driver::ArgStringList &CCArgInputs = Cmd.getArguments();
  //llvm::OwningPtr<CompilerInvocation> CI(new CompilerInvocation);
  //CompilerInvocation::CreateFromArgs(*CI,
  //                                   const_cast<const char **>(CCArgs.data()),
  //                                   const_cast<const char **>(CCArgs.data())
  // +
  //                                     CCArgs.size(),
  //                                   Diags);

  // Workaround for LLVM 3.3(bug?)
  // filter out '-backend-option -vectorize-loops'
  driver::ArgStringList CCArgs;
  for (size_t i = 0; i < CCArgInputs.size(); i++) {
    if ((strcmp(CCArgInputs[i], "-backend-option") == 0) ||
        (strcmp(CCArgInputs[i], "-vectorize-loops") ==0)) {
      // skip;
    } else {
      CCArgs.push_back(CCArgInputs[i]);
    }
  }

  //for (int i = 0; i < CCArgs.size(); i++) {
  //  printf("ccarg[%d] = %s\n", i, CCArgs[i]);
  //}
  bool Success;
  Success = CompilerInvocation::CreateFromArgs(
      Clang->getInvocation(), const_cast<const char **>(CCArgs.data()),
      const_cast<const char **>(CCArgs.data()) + CCArgs.size(), Diags);

  //// Show the invocation, with -v.
  //if (CI->getHeaderSearchOpts().Verbose) {
  //  llvm::errs() << "clang invocation:\n";
  //  C->PrintJob(llvm::errs(), C->getJobs(), "\n", true);
  //  llvm::errs() << "\n";
  //}

  // FIXME: This is copied from cc1_main.cpp; simplify and eliminate.

  //Clang.setInvocation(CI.take());

  // Infer the builtin include path if unspecified.
  if (Clang->getHeaderSearchOpts().UseBuiltinIncludes &&
      Clang->getHeaderSearchOpts().ResourceDir.empty())
    Clang->getHeaderSearchOpts().ResourceDir =
        CompilerInvocation::GetResourcesPath("./", MainAddr);

  // Create the compilers actual diagnostics engine.
  //Clang.createDiagnostics(int(CCArgs.size()),const_cast<char**>(CCArgs.data()));
  Clang->createDiagnostics();
  if (!Clang->hasDiagnostics()) {
    fprintf(stderr, "[ShaderEngine] hasDiagnostics failed.\n");
    return false;
  }

  // Create and execute the frontend to generate an LLVM bitcode module.
  Act = new EmitLLVMOnlyAction();
  if (!Clang->ExecuteAction(*Act)) {
    fprintf(stderr, "[ShaderEngine] ExecuteAction failed.\n");
    return false;
  }

  // Explicitly free Clang
  delete Clang;
  //Clang.take();
  //Clang.reset();
  //C.take();

  Module = Act->takeModule();
  if (!Module) {
    fprintf(stderr, "[ShaderEngine] Module gen failed.\n");
    return false;
  }

  EntryFn = Module->getFunction("spirv_cross_get_interface");
  if (!EntryFn) {
    llvm::errs() << "'spirv_cross_get_interface' function not found in module.\n";
    return false;
  }

  std::string Error;
  EE = llvm::EngineBuilder(std::move(Module))
      .setMCJITMemoryManager(llvm::make_unique<ShaderJITMemoryManager>()).create();
  if (!EE) {
    llvm::errs() << "unable to make execution engine: " << Error << "\n";
    return false;
  }

  // Disable symbol search using dlsym for security(e.g. disable sysmte() call
  // from the shader)
  //EE->DisableSymbolSearching(); // @todo { Turn on this feature. }

  EE->DisableLazyCompilation(true);

  // Install unknown symbol resolver
  //EE->InstallLazyFunctionCreator(CustomSymbolResolver);

  EntryPoint = EE->getPointerToFunction(EntryFn);
  assert(EntryPoint);

  // Need to call finalizeObject to ensure module is usable.
  EE->finalizeObject();

  printf("[JITEngine] Shader [ %s ] compile OK.\n", filename.c_str());

  return true;
}

void *
ShaderInstance::Impl::GetInterface()
{
  assert(EntryPoint);
  return EntryPoint;
}

ShaderInstance::ShaderInstance()
    : impl(new Impl()) {}

ShaderInstance::~ShaderInstance() { delete impl; }

bool ShaderInstance::Compile(const std::string& type, const std::vector<std::string>& paths, const std::string& filename)
{
  assert(impl);
  if (type != "comp") {
    std::cerr << "Unknown type: " << type << std::endl;
    return false;
  }

  return impl->Compile(type, paths, filename);
}


void *ShaderInstance::GetInterface()
{
  assert(impl);
  return impl->GetInterface();
}

//
// --
//

class ShaderEngine::Impl {
public:
  Impl(bool abortOnFailure);
  ~Impl();

  ShaderInstance* Compile(const std::string& type, unsigned int shaderID, const std::vector<std::string>& paths, const std::string &filename);

  void* GetInterface();

private:
  bool abortOnFailure_;
};

ShaderInstance* ShaderEngine::Impl::Compile(
  const std::string& type,
  unsigned int shaderID,
  const std::vector<std::string>& paths,
  const std::string &filename)
{
  static bool initialized = false;
  if (!initialized) {
    llvm::InitializeNativeTarget();
    // For MCJIT
    llvm::InitializeNativeTargetAsmPrinter();
    llvm::InitializeNativeTargetAsmParser();
    initialized = true;
  }

  ShaderInstance* shaderInstance = new ShaderInstance();
  bool ret = shaderInstance->Compile(type, paths, filename);
  if (!ret) {
    fprintf(stderr, "[Shader] Failed to compile shader: %s\n", filename.c_str());
    delete shaderInstance;
    return NULL;
  }

  return shaderInstance;
}

ShaderEngine::Impl::Impl(bool abortOnFailure)
    : abortOnFailure_(abortOnFailure) {
}

ShaderEngine::Impl::~Impl() {
}

ShaderEngine::ShaderEngine(bool abortOnFailure)
    : impl(new Impl(abortOnFailure)) {}

ShaderEngine::~ShaderEngine() { delete impl; }

ShaderInstance* ShaderEngine::Compile(
  const std::string& type,
  unsigned int shaderID,
  const std::vector<std::string>& paths, const std::string &filename) {

  assert(impl);
  assert(shaderID != (unsigned int)(-1));

  if (shaderInstanceMap_.find(shaderID) != shaderInstanceMap_.end()) {
    printf("[ShaderEngine] Err: Duplicate shader ID.\n");
    ShaderInstance* instance = shaderInstanceMap_[shaderID];
    if (instance) {
      delete instance;
    }
  }

  ShaderInstance* shaderInstance = impl->Compile(type, shaderID, paths, filename);

  shaderInstanceMap_[shaderID] = shaderInstance;

  return shaderInstance;
}

}  // namespace softcompute
