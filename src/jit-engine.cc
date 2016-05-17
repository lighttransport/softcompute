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

//extern "C" {
//extern float randomreal(); // main.cpp
//}

typedef void (*ShaderFunctionP)(softcompute::ShaderEnv *env);

typedef struct {
  ShaderFunctionP shaderFunction;
  //ShaderISPCFunctionP shaderISPCFunction;
} ShaderJIT;

static std::string GetFileExtension(const std::string &FileName) {
  if (FileName.find_last_of(".") != std::string::npos)
    return FileName.substr(FileName.find_last_of(".") + 1);
  return "";
}

#if 0
static bool EqSymbol(const std::string &name, const std::string &sym) {
  if (name == sym) {
    return true;
  }

  std::string s = "_" + sym;
  if (name == s) {
    return true;
  }

  return false;
}

static int __lte__isinff(float x) { return std::isinf(x); }

static int __lte__isfinitef(float x) { return std::isfinite(x); }

static void *MathSymbolResolver(const std::string &name) {
  if (EqSymbol(name, "fmaf")) {
    return reinterpret_cast<void *>(fmaf);
  } else if (EqSymbol(name, "fma")) {
    return reinterpret_cast<void *>(fmad);
  } else if (EqSymbol(name, "fmad")) {
    return reinterpret_cast<void *>(fmad);
  } else if (EqSymbol(name, "fmodf")) {
    return reinterpret_cast<void *>(fmodf);
  } else if (EqSymbol(name, "fmodd")) {
    return reinterpret_cast<void *>(fmodd);
  } else if (EqSymbol(name, "cosf")) {
    return reinterpret_cast<void *>(cosf);
  } else if (EqSymbol(name, "cosd")) {
    return reinterpret_cast<void *>(cosd);
  } else if (EqSymbol(name, "sinf")) {
    return reinterpret_cast<void *>(sinf);
  } else if (EqSymbol(name, "sind")) {
    return reinterpret_cast<void *>(sind);
  } else if (EqSymbol(name, "tanf")) {
    return reinterpret_cast<void *>(tanf);
  } else if (EqSymbol(name, "tand")) {
    return reinterpret_cast<void *>(tand);
  } else if (EqSymbol(name, "sqrtf")) {
    return reinterpret_cast<void *>(sqrtf);
  } else if (EqSymbol(name, "sqrtd")) {
    return reinterpret_cast<void *>(sqrtd);
  } else if (EqSymbol(name, "fabsf")) {
    return reinterpret_cast<void *>(fabsf);
  } else if (EqSymbol(name, "fabsd")) {
    return reinterpret_cast<void *>(fabsd);
  } else if (EqSymbol(name, "acosf")) {
    return reinterpret_cast<void *>(acosf);
  } else if (EqSymbol(name, "acosd")) {
    return reinterpret_cast<void *>(acosd);
  } else if (EqSymbol(name, "acoshf")) {
    return reinterpret_cast<void *>(acoshf);
  } else if (EqSymbol(name, "acoshd")) {
    return reinterpret_cast<void *>(acoshd);
  } else if (EqSymbol(name, "asinf")) {
    return reinterpret_cast<void *>(asinf);
  } else if (EqSymbol(name, "asind")) {
    return reinterpret_cast<void *>(asind);
  } else if (EqSymbol(name, "asinhf")) {
    return reinterpret_cast<void *>(asinhf);
  } else if (EqSymbol(name, "asinhd")) {
    return reinterpret_cast<void *>(asinhd);
  } else if (EqSymbol(name, "atanf")) {
    return reinterpret_cast<void *>(atanf);
  } else if (EqSymbol(name, "atand")) {
    return reinterpret_cast<void *>(atand);
  } else if (EqSymbol(name, "atanhf")) {
    return reinterpret_cast<void *>(atanhf);
  } else if (EqSymbol(name, "atanhd")) {
    return reinterpret_cast<void *>(atanhd);
  } else if (EqSymbol(name, "atan2f")) {
    return reinterpret_cast<void *>(atan2f);
  } else if (EqSymbol(name, "atan2d")) {
    return reinterpret_cast<void *>(atan2d);
  } else if (EqSymbol(name, "expf")) {
    return reinterpret_cast<void *>(expf);
  } else if (EqSymbol(name, "expd")) {
    return reinterpret_cast<void *>(expd);
  } else if (EqSymbol(name, "exp2f")) {
    return reinterpret_cast<void *>(exp2f);
  } else if (EqSymbol(name, "exp2d")) {
    return reinterpret_cast<void *>(exp2d);
  } else if (EqSymbol(name, "logf")) {
    return reinterpret_cast<void *>(logf);
  } else if (EqSymbol(name, "logd")) {
    return reinterpret_cast<void *>(logd);
  } else if (EqSymbol(name, "log2f")) {
    return reinterpret_cast<void *>(log2f);
  } else if (EqSymbol(name, "log2d")) {
    return reinterpret_cast<void *>(log2d);
  } else if (EqSymbol(name, "floorf")) {
    return reinterpret_cast<void *>(floorf);
  } else if (EqSymbol(name, "floord")) {
    return reinterpret_cast<void *>(floord);
  } else if (EqSymbol(name, "ceilf")) {
    return reinterpret_cast<void *>(ceilf);
  } else if (EqSymbol(name, "ceild")) {
    return reinterpret_cast<void *>(ceild);
  } else if (EqSymbol(name, "powf")) {
    return reinterpret_cast<void *>(powf);
  } else if (EqSymbol(name, "powd")) {
    return reinterpret_cast<void *>(powd);
  } else if (EqSymbol(name, "modff")) {
    return reinterpret_cast<void *>(modff);
  } else if (EqSymbol(name, "modfd")) {
    return reinterpret_cast<void *>(modfd);
  } else if (EqSymbol(name, "isinff")) {
    return reinterpret_cast<void *>(__lte__isinff);
  } else if (EqSymbol(name, "isinfd")) {
    return reinterpret_cast<void *>(isinfd);
  } else if (EqSymbol(name, "isfinitef")) {
    return reinterpret_cast<void *>(__lte__isfinitef);
  } else if (EqSymbol(name, "isfinited")) {
    return reinterpret_cast<void *>(isfinited);
  }

  return NULL;
}
#endif

static void *CustomSymbolResolver(const std::string &name) {
  // @todo
  printf("[Shader] Resolving %s\n", name.c_str());

#if 0
  void *mathFun = MathSymbolResolver(name);
  if (mathFun) {
    return mathFun;
  }

  if (EqSymbol(name, "printf")) {
    return reinterpret_cast<void *>(printf);
  } else if (EqSymbol(name, "trace")) {
    return reinterpret_cast<void *>(trace);
  } else if (EqSymbol(name, "sunsky")) {
    return reinterpret_cast<void *>(sunsky);
  } else if (EqSymbol(name, "texture2D")) {
    return reinterpret_cast<void *>(texture2D);
  } else if (EqSymbol(name, "textureGrad2D")) {
    return reinterpret_cast<void *>(textureGrad2D);
  } else if (EqSymbol(name, "envmap3D")) {
    return reinterpret_cast<void *>(envmap3D);
  } else if (EqSymbol(name, "getNumLights")) {
    return reinterpret_cast<void *>(getNumLights);
  } else if (EqSymbol(name, "getLight")) {
    return reinterpret_cast<void *>(getLight);
  } else if (EqSymbol(name, "sampleLight")) {
    return reinterpret_cast<void *>(sampleLight);
  } else if (EqSymbol(name, "getNumVPLs")) {
    return reinterpret_cast<void *>(getNumVPLs);
  } else if (EqSymbol(name, "getVPL")) {
    return reinterpret_cast<void *>(getVPL);
  } else if (EqSymbol(name, "computeFresnel")) {
    return reinterpret_cast<void *>(computeFresnel);
  } else if (EqSymbol(name, "randomreal")) {
    return reinterpret_cast<void *>(randomreal);
  } else if (EqSymbol(name, "fetchDiffuse")) {
    return reinterpret_cast<void *>(fetchDiffuse);
  } else if (EqSymbol(name, "fetchReflection")) {
    return reinterpret_cast<void *>(fetchReflection);
  } else if (EqSymbol(name, "fetchRefraction")) {
    return reinterpret_cast<void *>(fetchRefraction);
  //} else if (EqSymbol(name, "getParamFloat")) {
  //  return reinterpret_cast<void *>(getParamFloat);
  //} else if (EqSymbol(name, "getParamFloat3")) {
  //  return reinterpret_cast<void *>(getParamFloat3);
  } else if (EqSymbol(name, "getNumThreads")) {
    return reinterpret_cast<void *>(getNumThreads);
  //} else if (EqSymbol(name, "photondiffuse")) {
  //  return reinterpret_cast<void *>(photondiffuse);
  }
#endif

  std::cout << "[Shader] Failed to resolve symbol : " << name << "\n";
  return NULL; // fail
}

#if 0
struct ShaderArg
{
  int               index;  // array index
  std::string       name;
  ShaderParamType   type;
};

ShaderParamType
GetShaderParamType(
  llvm::Type* type)
{
  if (type->getTypeID() == llvm::Type::FloatTyID) {
      return SHADER_PARAM_TYPE_FLOAT;
  } else if (type->getTypeID() == llvm::Type::VectorTyID) {
    llvm::VectorType* VTy = llvm::dyn_cast<llvm::VectorType>(type);
    if (VTy->getNumElements() == 4 &&
        VTy->getElementType()->getTypeID() == llvm::Type::FloatTyID) {
      return SHADER_PARAM_TYPE_VEC4;
    }
  }

  // Unsupported type.
  return SHADER_PARAM_TYPE_INVALID;
}

std::vector<ShaderArg>
ExtractShaderArgument(
  const llvm::Function* F)
{
  assert(F->arg_size() >= 1);

  std::vector<ShaderArg> shaderArgs;

  llvm::Function::const_arg_iterator it = F->arg_begin();
  
  it++; // Skip first argument(ShaderEnv* env)

  int index = 1;
  for (; it != F->arg_end(); it++) {
    llvm::Type* ty = it->getType();
    
    std::string name = it->getName();

    assert(!name.empty() && "argument should have name");

    ShaderArg arg;
    arg.index = index;
    arg.name  = name;
    arg.type  = GetShaderParamType(ty);
    shaderArgs.push_back(arg);

    dbgprintf("arg[%d] = name: %s, ty: %d\n",
        index, name.c_str(), arg.type);
  }

  return shaderArgs;
}


llvm::Constant*
BuildFloatConstant(
  LLVMContext& Ctx,
  float f)
{
  llvm::Constant* c = llvm::ConstantFP::get(Ctx, llvm::APFloat(f));

  return c;
}

llvm::Constant*
BuildVec4Constant(
  LLVMContext& Ctx,
  float f0, float f1, float f2, float f3)
{
  llvm::Type *FloatTy = llvm::Type::getFloatTy(Ctx);

  llvm::VectorType* VTy = llvm::VectorType::get(FloatTy, 4); // vec4

  llvm::Constant* c0 = llvm::ConstantFP::get(Ctx, llvm::APFloat(f0));
  llvm::Constant* c1 = llvm::ConstantFP::get(Ctx, llvm::APFloat(f1));
  llvm::Constant* c2 = llvm::ConstantFP::get(Ctx, llvm::APFloat(f2));
  llvm::Constant* c3 = llvm::ConstantFP::get(Ctx, llvm::APFloat(f3));
  std::vector<llvm::Constant*> fs;
  fs.push_back(c0);
  fs.push_back(c1);
  fs.push_back(c2);
  fs.push_back(c3);

  return llvm::ConstantVector::get(fs);

} 

llvm::FunctionType*
BuildFunctionType(
  llvm::Type* retTy,
  const std::vector<llvm::Type*>& argTys)
{
  bool vaArgs = false;
  return llvm::FunctionType::get(retTy, argTys, vaArgs);
}

void
BindShaderParameter(
  std::vector<llvm::Value*>& args, // [out]
  LLVMContext& Ctx, // [in]
  const ShaderParamMap& params, // [in]
  const std::vector<ShaderArg>& shaderArgs)
{
  std::vector<ShaderArg>::const_iterator it(shaderArgs.begin());
  std::vector<ShaderArg>::const_iterator itEnd(shaderArgs.end());

  args.clear();
  
  for (; it != itEnd; ++it) {
    const std::string& name = it->name;

    llvm::Constant* C = NULL;

    if (params.count(name) > 0) {

      ShaderParamMap::const_iterator paramIt = params.find(name);

      if (paramIt->second.type == it->type) {

        if (it->type == SHADER_PARAM_TYPE_FLOAT) {
          C = BuildFloatConstant(Ctx, paramIt->second.fValue);
        } else if (it->type == SHADER_PARAM_TYPE_VEC4) {
          std::vector<float> vfVal = paramIt->second.vfValue;
          assert(vfVal.size() >= 4);

          C = BuildVec4Constant(Ctx, vfVal[0], vfVal[1], vfVal[2], vfVal[3]);
        } else {
          assert(0 && "Unsupported parameter type.");
        }

      }
    }

    if (C == NULL) {

     // No sahder parameter found. Fill with default value.

      if (it->type == SHADER_PARAM_TYPE_FLOAT) {
        C = BuildFloatConstant(Ctx, 0.0f);
      } else if (it->type == SHADER_PARAM_TYPE_VEC4) {
        C = BuildVec4Constant(Ctx, 0.0f, 0.0f, 0.0f, 0.0f);
      } else {
        assert(0 && "Unsupported parameter type.");
      }

    }

    assert(C);

    args.push_back(C);

  }

}

//
// --
//

void
BuildShaderCallStub(
  llvm::Module* M, // [inout]
  const std::string& entryFunctionName,
  const ShaderParamMap& shaderParams) // [in]
{
  //
  // void shder_stub(ShaderEnv* env) {
  //   entry:
  //   shader(env, arg0, arg1, arg2, ...);
  // }
  //
  LLVMContext& Ctx = M->getContext();
  llvm::Type *VoidTy = llvm::Type::getVoidTy(Ctx);
  llvm::StructType* ShaderEnvStructTy = M->getTypeByName("struct.ShaderEnv");
  assert(ShaderEnvStructTy);
  llvm::Type *ShaderEnvStructPtrTy = llvm::PointerType::get(ShaderEnvStructTy, 0); // ShaderEnv*

  std::string name = entryFunctionName + "_stub"; // generally this will be `shader_stub'

  std::vector<llvm::Type*> stubArgTys;
  stubArgTys.push_back(ShaderEnvStructPtrTy); // ShaderEnv*
  llvm::FunctionType* FTy = BuildFunctionType(VoidTy, stubArgTys);

  llvm::Function* F = llvm::cast<llvm::Function>(M->getOrInsertFunction(name, FTy));

  llvm::BasicBlock *EntryBB = llvm::BasicBlock::Create(M->getContext(), "entry", F);

  llvm::IRBuilder<> builder(EntryBB);
 
  // Get pointer to the first argument
  //printf("F->arg_size() = %d\n", F->arg_size());
  //assert(F->arg_size() == 2); // (void, ShaderEnv*)
  llvm::Function::arg_iterator it = F->arg_begin();

  llvm::Argument *envArg = it; // Get the arg.
  envArg->setName("env");

  //llvm::Function* ShaderF = llvm::cast<llvm::Function>(M->getFunction(entryFunctionName, FTy));
  llvm::Function* ShaderF = llvm::cast<llvm::Function>(M->getFunction(entryFunctionName));

  const std::vector<ShaderArg> shaderArgs = ExtractShaderArgument(ShaderF);

  std::vector<llvm::Value*> shaderParamInputs;
  BindShaderParameter(shaderParamInputs, Ctx, shaderParams, shaderArgs);

  // args = (env, arg0, arg1, ....)
  std::vector<llvm::Value*> args;
  args.push_back(envArg);
  args.insert(args.end(), shaderParamInputs.begin(), shaderParamInputs.end());

  llvm::Value* retVal = builder.CreateCall(ShaderF, args);

  // retVal never used;

  builder.CreateRetVoid(); // BB terminator.


  return;
}
#endif

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

  bool Link(const std::string& filename);
  bool Compile(const std::string& type, const std::vector<std::string>& paths, const std::string& filename, const ShaderParamMap& paramMap);
  bool Eval(ShaderEnv *env);

private:
  llvm::Function *EntryFn;       // Shader function
  std::unique_ptr<llvm::Module> Module;
  llvm::ExecutionEngine *EE;

  // args
  //std::vector<llvm::GenericValue> args;

  ShaderJIT jit;
};

ShaderInstance::Impl::Impl()
: EntryFn(NULL),
  //Module(NULL),   
  EE(NULL)
{

}

ShaderInstance::Impl::~Impl()
{ 
  // @fixme { Deletion cause seg fault on LLVM 3.4. }
  EntryFn = NULL;
  //Module = NULL;
  EE = NULL;
  // @fixme { leak }

}

bool ShaderInstance::Impl::Compile(
  const std::string& type,
  const std::vector<std::string>& paths,
  const std::string &filename,
  const ShaderParamMap& paramMap)
{
  std::string ext = GetFileExtension(filename);
  if ((ext.compare("bc") == 0) || (ext.compare("o") == 0)) {
    return Link(filename);
  }

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

  if (!Module->getFunction("shader")) {
    llvm::errs() << "'shader' function not found in module.\n";
    return false;
  }

  {
    // Create stub function
    //BuildShaderCallStub(Module.get(), "shader", paramMap);
  }

  EntryFn = Module->getFunction("shader_stub");
  if (!EntryFn) {
    llvm::errs() << "'shader' function not found in module.\n";
    return false;
  }

  std::string Error;
#if (LLVM_VERSION_MINOR >= 4)
  EE = llvm::EngineBuilder(std::move(Module))
      .setMCJITMemoryManager(llvm::make_unique<ShaderJITMemoryManager>()).create();
#else
  EE = llvm::EngineBuilder(Module).setUseMCJIT(true)
      .setJITMemoryManager(new ShaderJITMemoryManager()).create();
#endif
  if (!EE) {
    llvm::errs() << "unable to make execution engine: " << Error << "\n";
    return false;
  }

  // Disable symbol search using dlsym for security(e.g. disable sysmte() call
  // from the shader)
  EE->DisableSymbolSearching();

  EE->DisableLazyCompilation(true);

  // Install unknown symbol resolver
  EE->InstallLazyFunctionCreator(CustomSymbolResolver);

  void *ptr = EE->getPointerToFunction(EntryFn);
  assert(ptr);

  jit.shaderFunction = reinterpret_cast<ShaderFunctionP>(ptr);

  // @todo { tracer, imager }

  // Need to call finalizeObject to ensure module is usable.
  EE->finalizeObject();

#if 0
  ShaderEnv env;
  jit.shaderFunction(&env);
//ShaderEnv env;
//std::vector<GenericValue> args;
//args.push_back(GenericValue((void *)&env));
//EE->runFunction(EntryFn, args);
#endif

  printf("[Shader] Shader [ %s ] compile OK.\n", filename.c_str());

  return true;
}

bool
ShaderInstance::Impl::Eval(ShaderEnv* env)
{
  assert(jit.shaderFunction);
  jit.shaderFunction(env);
  return true;
}

ShaderInstance::ShaderInstance()
    : impl(new Impl()) {}

ShaderInstance::~ShaderInstance() { delete impl; }

bool ShaderInstance::Compile(const std::string& type, const std::vector<std::string>& paths, const std::string& filename, const ShaderParamMap& paramMap)
{
  assert(impl);
  if (type != "comp") {
    std::cerr << "Unknown type: " << type << std::endl;
    return false;
  }

  return impl->Compile(type, paths, filename, paramMap);
}


bool ShaderInstance::Eval(ShaderEnv *env)
{
  assert(impl);
  return impl->Eval(env);
}

bool ShaderInstance::EvalImager(ShaderEnv *env)
{
  assert(impl);
  return false;// @todo impl->EvalImager(env);
}

//
// --
//

class ShaderEngine::Impl {
public:
  Impl(bool abortOnFailure);
  ~Impl();

  ShaderInstance* Link(const std::string &filename);
  ShaderInstance* Compile(const std::string& type, unsigned int shaderID, const std::vector<std::string>& paths, const std::string &filename, const ShaderParamMap& paramMap);

  void Eval(ShaderEnv *env);

private:
  //llvm::Function *EntryFn;       // Shader function
  //llvm::Module *Module;
  //llvm::ExecutionEngine *EE;
  //CodeGenAction *Act;

  //ShaderJIT jit;

  bool abortOnFailure_;
};

//void ShaderEngine::Impl::Eval(ShaderEnv *env) {
//  assert(0);
//  //assert(jit.shaderFunction);
//  //jit.shaderFunction(env);
//}

//void ShaderEngine::Impl::evalISPC(ShaderEnv *env) {
//  assert(0);
//  //assert(jit.shaderFunction);
//  //jit.shaderFunction(env);
//}

bool ShaderInstance::Impl::Link(const std::string &filename) {

  // Init
  EntryFn = NULL;
  EE = NULL;

  SMDiagnostic Err;
  LLVMContext Context;
  Module = llvm::parseIRFile(filename, Err, Context);
  if (!Module) {
    Err.print("[Shader]", llvm::errs());
    return false;
  }

  EntryFn = Module->getFunction("shader");
  if (!EntryFn) {
    llvm::errs() << "'shader' function not found in module.\n";
    return false;
  }


  std::string Error;
#if (LLVM_VERSION_MINOR >= 4)
  EE = llvm::EngineBuilder(std::move(Module))
      .setMCJITMemoryManager(llvm::make_unique<ShaderJITMemoryManager>()).create();
#else
  EE = llvm::EngineBuilder(Module).setUseMCJIT(true)
      .setJITMemoryManager(new ShaderJITMemoryManager()).create();
#endif
  if (!EE) {
    llvm::errs() << "unable to make execution engine: " << Error << "\n";
    return false;
  }

  // Disable symbol search using dlsym for security(e.g. disable sysmte() call
  // from the shader)
  EE->DisableSymbolSearching();

  EE->DisableLazyCompilation(true);

  // Install unknown symbol resolver
  EE->InstallLazyFunctionCreator(CustomSymbolResolver);

  //
  //
#if (LLVM_VERSION_MINOR >= 6)
  void *ptr = EE->getPointerToFunction(EntryFn);
#else
  void *ptr = EE->recompileAndRelinkFunction(EntryFn);
#endif
  assert(ptr);

  jit.shaderFunction = reinterpret_cast<ShaderFunctionP>(ptr);

  printf("[Shader] Shader [ %s ] compile OK.\n", filename.c_str());
  
  //assert(0);
  return true;
}

ShaderInstance* ShaderEngine::Impl::Compile(
  const std::string& type,
  unsigned int shaderID,
  const std::vector<std::string>& paths,
  const std::string &filename,
  const ShaderParamMap& paramMap)
{
  //std::string ext = GetFileExtension(filename);
  //if ((ext == "bc") || (ext == "o")) {
  //  return Link(filename);
  //}

  static bool initialized = false;
  if (!initialized) {
    llvm::InitializeNativeTarget();
#if USE_MCJIT
    llvm::InitializeNativeTargetAsmPrinter();
    llvm::InitializeNativeTargetAsmParser();
#endif
    initialized = true;
  }

  ShaderInstance* shaderInstance = new ShaderInstance();
  bool ret = shaderInstance->Compile(type, paths, filename, paramMap);
  if (!ret) {
    fprintf(stderr, "[Shader] Failed to compile shader: %s\n", filename.c_str());
    delete shaderInstance;
    return NULL;
  }

  return shaderInstance;
}

ShaderEngine::Impl::Impl(bool abortOnFailure)
    : abortOnFailure_(abortOnFailure) {
  //EE = NULL;
  //Act = NULL;
  //EntryFn = NULL;
}

ShaderEngine::Impl::~Impl() {
  // Shutdown.

  // Calling llvm_shutdown() here will invoke
  //   pure virtual method called
  //   terminate called without an active exception
  // error, thus don't call that function here.
  //
  // @todo { Call llvm_shutdown() somewhere. }
  //llvm::llvm_shutdown();
}

ShaderEngine::ShaderEngine(bool abortOnFailure)
    : impl(new Impl(abortOnFailure)) {}

ShaderEngine::~ShaderEngine() { delete impl; }

ShaderInstance* ShaderEngine::Compile(
  const std::string& type,
  unsigned int shaderID,
  const std::vector<std::string>& paths, const std::string &filename,
  const ShaderParamMap& paramMap) {

  assert(impl);
  assert(shaderID != (unsigned int)(-1));

  if (shaderInstanceMap_.find(shaderID) != shaderInstanceMap_.end()) {
    printf("[ShaderEngine] Err: Duplicate shader ID.\n");
    ShaderInstance* instance = shaderInstanceMap_[shaderID];
    if (instance) {
      delete instance;
    }
  }

  ShaderInstance* shaderInstance = impl->Compile(type, shaderID, paths, filename, paramMap);

  shaderInstanceMap_[shaderID] = shaderInstance;

  //printf("shaderID[%d] = %p\n", shaderID, shaderInstance);

  return shaderInstance;
}

}  // namespace softcompute
