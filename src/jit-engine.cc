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
