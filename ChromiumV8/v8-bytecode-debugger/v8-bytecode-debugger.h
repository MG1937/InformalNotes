#ifndef V8_DEBUGGER_BYTECODE_DEBUGGER_H_
#define V8_DEBUGGER_BYTECODE_DEBUGGER_H_
// V8 Bytecode Debugger
// 20240307
// Author: MG193.7 mgaldys4@gmail.com

#include <iostream>
#include <string>
#include <vector>
#include <mutex>
#include <functional>

// For InterpreterAssembler
#include "src/interpreter/interpreter-assembler.h"
#include "src/macro-assembler.h"

// For MASM IA32!!
#include "src/ia32/macro-assembler-ia32.h"
#include "src/ia32/assembler-ia32.h"

#include "src/handles.h"
#include "src/flags.h"
#include "src/interpreter/bytecode-decoder.h"
#include "src/base/win32-headers.h"

using namespace std;
using namespace v8::internal;

// For InterpreterAssembler Hook
typedef interpreter::InterpreterAssembler InterpreterAssembler;
typedef interpreter::Bytecodes Bytecodes;
typedef interpreter::Bytecode Bytecode;

namespace v8_debugger {

static BOOL WINAPI CtrlHandler(DWORD ctype);

// Unit macro

#define STATUS_HANDLE(F) \
  F(SUSPEND) \
  F(INT3) \
  F(SKIP)

#define STATUS_ENUM(STATUS) k##STATUS,
#define STATUS_SETTER(STATUS) \
  static void STATUS() { GetInstance().current_status = Status::k##STATUS; }
#define STATUS_CHECKER(STATUS) \
  static bool IS_##STATUS() { return GetInstance().current_status == Status::k##STATUS; }

// Unit function

// Split source string with delimiter
static vector<string> str_split(string source, char* delimiter) {
 vector<string> splits;
 if (source.empty()) return splits;
 char* split = strtok((char*)source.c_str(), delimiter);
 while (split != NULL) {
  splits.push_back(string(split));
  split = strtok(NULL, delimiter);
 }
 return splits;
}

constexpr static size_t hash(const char *s, int off = 0) {
 // https://stackoverflow.com/a/16388610
 return !s[off] ? 5381 : (hash(s, off+1)*33) ^ s[off];
}

static void hookTester() { cout<<"hooked here!!\n"; }

class V8BytecodeDebugger{
 public:
  // Singleton pattern
  static V8BytecodeDebugger& GetInstance() {
   static V8BytecodeDebugger debugger;
   return debugger;
  }

  enum class Status : uint8_t {
   STATUS_HANDLE(STATUS_ENUM)
  };

  Status current_status = Status::kSKIP;

  STATUS_HANDLE(STATUS_SETTER)
  STATUS_HANDLE(STATUS_CHECKER)

  // Hook MASM for bytecode Debugger
  static void HookInterpreterAssembler(InterpreterAssembler* masm, Bytecode bytecode);
  
  // Adjust MASM Hook for JSFunction
  // Called when InterpreterEntryTrampoline called
  static void HookForJSFunction(MacroAssembler* masm);

  static void HandleJSFunctionExternal(Address address);

  // DBG State Machine
  static void BytecodeDBG();

  static string GetDBGCommand() {
   string cmd;
   cout << "\rv8-dbg> ";
   getline(cin, cmd);
   if (cmd.compare("") == 0) return "s"; // Enter key
   return cmd;
  }

  // Handle Bytecode Interrupt
  // For interrupt Bytecode execution timely
  static void BytecodeInterrupt(int bytecode_offset);
 
  static bool IsTriggerBytecodeBreak(Bytecode bytecode);
 
  static bool IsTriggerFunctionBreak();

  static bool IsNeedUpdateBytecodeArray() { return GetInstance().update_bytecode_array; }

  static void SetUpdateBytecodeArray(bool b) { GetInstance().update_bytecode_array = b; }

  /** APIs for store or access DBG required objects. **/
  /** Start of APIs BLOCK **/
  void SetJSFunction(JSFunction* jsfunction) { current_jsfunction = jsfunction; }
  
  JSFunction* GetJSFunction() { return current_jsfunction; }

  void SetBytecodeArray(BytecodeArray* bytecode_array) { current_bytecode_array = bytecode_array; }

  BytecodeArray* GetBytecodeArray() { return current_bytecode_array; }

  static void PrintBytecodeByOffset(int offset);

  static void PrintBytecodeConstantPool();
  /** End of APIs BLOCK **/

 private:
  JSFunction* current_jsfunction;
  BytecodeArray* current_bytecode_array;
  unique_ptr<char[]> current_function_name;

  // Global DBG Parameter
  vector<string> break_bytecode; // Bytecode needs to break on
  vector<string> break_function; // Functions needs to break on

  bool update_bytecode_array;
  bool show_current_bytecode = true;
  bool show_current_function = true;
  
  enum class BreakType : uint8_t {
   kBytecode, kFunction, kUnknown
  };

  // Prevent external instantiation
  V8BytecodeDebugger() {
   // Init DBG Data
   CollectDBGBreakBytecode();
   CollectDBGBreakFunction();
  }

  unique_ptr<char[]>& GetCurrentFunctionName();

  void ResetCurrentFunctionName() { current_function_name.reset(nullptr); }

  void CollectDBGBreakBytecode() {
   if (FLAG_dbg_bytecode != nullptr) {
    break_bytecode = str_split(string(FLAG_dbg_bytecode), ",");
   }
  }
  
  void CollectDBGBreakFunction() { 
   if(FLAG_dbg_function) {
    break_function = str_split(string(FLAG_dbg_function), ",");
   }
  }

  // type_b: Break type wants to operate
  // data_b: Break data wants to remove
  bool RemoveBreak(BreakType type_b, string data_b);

  // type_b: Break type wants to operate
  // data_b: Break data wants to insert
  bool AddBreak(BreakType type_b, string data_b);

  static BreakType BreakTypeChooser(string type) {
   const char* type_c = type.c_str();
   switch(hash(type_c)) {
    case hash("b"): // bytecode
    case hash("bytecode"):
     return BreakType::kBytecode;
    case hash("f"): // function
    case hash("function"):
     return BreakType::kFunction;
   }
  }
};
}

#endif // V8_DEBUGGER_BYTECODE_DEBUGGER_H_
