MINGW_DIST=$(HOME)/local/llvm-mingw-20191230-ubuntu-16.04/

LLVM_MINGW_DIST=$(HOME)/work/llvm-project/dist-win

INC_FLAGS = -I$(LLVM_MINGW_DIST)/include

LIB_FLAGS = -L$(LLVM_MINGW_DIST)/lib -lLLVM.dll -lclangFrontend -lclangSerialization -lclangDriver -lclangCodeGen -lclangParse -lclangSema -lclangAnalysis -lclangEdit -lclangAST -lclangLex -lclangBasic -lversion -lunwind.dll

CXX=$(MINGW_DIST)/bin/x86_64-w64-mingw32-g++

all:
	$(CXX) -std=c++14 -shared -o test $(INC_FLAGS) ../../src/jit-engine.cc $(LIB_FLAGS)

