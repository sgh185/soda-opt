#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include <fcntl.h>

#define MAGIC_THRESHOLD 512

using namespace llvm;

namespace {

int indent;
struct IdentRAII {
  int &indent;
  IdentRAII(int &indent) : indent(indent) {}
  ~IdentRAII() { --indent; }
};

void resetIndent() { indent = 0; }

IdentRAII pushIndent() { return IdentRAII(++indent); }

raw_ostream *outputStream;

raw_ostream &xmlOut() { return *outputStream; }

llvm::raw_ostream &printIndent() {
  for (int i = 0; i < indent; ++i)
    xmlOut() << " ";
  return xmlOut();
}

void printPreamble() {
  printIndent() << "<?xml version=\"1.0\"?>\n"
                << "<memory>\n";
  auto indent = pushIndent();
  printIndent() << "<memory_allocation>\n";
}

void printClosure() { 
  printIndent() << "</memory_allocation>\n"; 
  resetIndent();
  printIndent() << "</memory>\n";
}

struct XMLWriter : public ModulePass {
  static char ID;
  XMLWriter() : ModulePass(ID) {}

  bool runOnModule(Module &M) override {
    std::error_code errorMessage;
    StringRef fileName = "memory_allocation.xml";
    raw_fd_ostream outFile(fileName, errorMessage);
    outputStream = &outFile;

    resetIndent();
    printPreamble();
    auto indent = pushIndent();  
    {
      for(auto f = M.begin(), fE = M.end(); f != fE; ++f) {
      if (!(*f).isDeclaration()) {
        for(auto I = inst_begin(*f), E = inst_end(*f); I != E; ++I) {
          if(isa<AllocaInst>(*I)){
            if (MDNode* N = (*I).getMetadata("annotation")) {
              auto indent = pushIndent();
              printIndent() << "<object scope=\"" << (*f).getName() << "\" ";
              xmlOut() << "name=\"" << cast<MDString>(N->getOperand(0))->getString() << "\" ";
              xmlOut() << "is_internal=\"";              
              uint64_t total_size = 0;
              uint64_t array_size = 0;
              DataLayout* d = new DataLayout(&M);
              if(cast<AllocaInst>(*I).getAllocationSizeInBits(*d) == None)
              {
                Value* v = cast<AllocaInst>(*I).getArraySize();
                Value* first_op = dyn_cast<ConstantExpr>(v)->getOperand(0);
                Value* first_op_second_op = dyn_cast<ConstantExpr>(first_op)->getOperand(1);
                array_size = dyn_cast<ConstantInt>(first_op_second_op)->getZExtValue();
                Type* t = cast<AllocaInst>(*I).getAllocatedType();
                total_size = uint64_t(d->getTypeAllocSizeInBits(t) * array_size);
              }
              if(total_size > MAGIC_THRESHOLD)
              {
                xmlOut() << "F\"";
              }
              else
              {
                xmlOut() << "T\"";
              }
              xmlOut() << "/>\n";
            }
          }
        }
      }
      }
    }
    
    printClosure();
    return false;
  }
}; // end of struct Namer
} // end of anonymous namespace

char XMLWriter::ID = 0;
static RegisterPass<XMLWriter>
    X("XMLwriter",
      "Generates an XML file for bambu that allocates memory instructions to internal and external storage.",
      true /* Only looks at CFG */, true /* Analysis Pass */);

static RegisterStandardPasses Y(PassManagerBuilder::EP_EarlyAsPossible,
                                [](const PassManagerBuilder &Builder,
                                   legacy::PassManagerBase &PM) {
                                  PM.add(new XMLWriter());
                                });





