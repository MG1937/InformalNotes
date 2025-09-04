// V8 Bytecode Debugger
// 20240307
// Author: MG193.7 mgaldys4@gmail.com

#include "src/v8-bytecode-debugger/v8-bytecode-debugger.h"

// Static Code Block
static auto static_code([]{
 // Redirect stdout&&stdin
 AllocConsole();
 freopen("CONOUT$", "w", stdout);
 freopen("CONIN$", "r", stdin);
 cout << "V8-DBG Author: MG193.7\n";
 cout << "V8BytecodeDebugger Init...\n";

 // For set flags
 cout << "\rinit-flag-set> ";
 string set_flags;
 getline(cin, set_flags);
 v8::V8::SetFlagsFromString(set_flags.c_str(), set_flags.length());
 cout << "\rFlag setted!\n";
 SetConsoleCtrlHandler(v8_debugger::CtrlHandler, TRUE);
 return 0;
}());

static BOOL WINAPI v8_debugger::CtrlHandler(DWORD ctype) {
 if (!v8_debugger::V8BytecodeDebugger::IS_SUSPEND() && ctype == CTRL_C_EVENT) {
  cout << "[!] Try Suspend...\n";
  v8_debugger::V8BytecodeDebugger::SUSPEND();
  return TRUE;
 }
 return FALSE;
}

namespace v8_debugger {
 void V8BytecodeDebugger::HookInterpreterAssembler(InterpreterAssembler* masm, Bytecode bytecode) { // static
  // Hooked to Bytecode Debugger!
  masm->CallRuntime(v8::internal::Runtime::kV8BytecodeDebugger
   , masm->GetContext() // JS Context
   , masm->SmiTag(masm->BytecodeOffset()) // Current Bytecode Offset
   , masm->LoadRegister(v8::internal::interpreter::Register::bytecode_array()) // BytecodeArray Register
   );
 }

 void V8BytecodeDebugger::HookForJSFunction(MacroAssembler* masm) { // static
  void(*hooker)(Address) = V8BytecodeDebugger::HandleJSFunctionExternal;
  // Push edi(JSFunction) to arg0
  masm->push(edi);
  masm->mov(eax, Immediate(Address(hooker)));
  masm->call(eax);
  // Pop out edi for stack balance
  masm->pop(edi);
 }

 void V8BytecodeDebugger::BytecodeInterrupt(int bytecode_offset) { // static
  BytecodeArray* bytecode_array = GetInstance().GetBytecodeArray();
  int bcode = *(bytecode_array->GetFirstBytecodeAddress() + bytecode_offset);
  // Get current bytecode for interrupt check.
  Bytecode current_bytecode = Bytecodes::FromByte(bcode);

  // If execute to Return, update bytecode array next time, avoid memory read violate
  if (current_bytecode == Bytecode::kReturn || current_bytecode == Bytecode::kReThrow) {
   SetUpdateBytecodeArray(true);
  }

  if (GetInstance().show_current_bytecode) PrintBytecodeByOffset(bytecode_offset);
  // cout<<"Current Bytecode -> "<<Bytecodes::ToString(current_bytecode)<<"\n";
  
  // If previous status is suspend, go debugger!
  if(IS_SUSPEND()) { goto debugger; }
  // Interrupt anyway if trigger bytecode break!
  else if (IsTriggerBytecodeBreak(current_bytecode)) { goto debugger; }
  else if (bytecode_offset == 0 && IsTriggerFunctionBreak()) { goto debugger; }

  // Don't suspend
  SKIP();
  return;

  debugger:
  SUSPEND();
  BytecodeDBG();
 }

 void V8BytecodeDebugger::BytecodeDBG() { // static
  string cmd;
  bool interrupt = true;
  while (interrupt) {
   cmd = GetDBGCommand();
   vector<string> cmd_split = str_split(cmd, " ");
   // split cmd with space, get first part as cmd key
   switch (hash(cmd_split.front().c_str())) {
    case hash("c"): // continue alias
    case hash("continue"): //continue execution, no suspend
     SKIP();
     return;
    case hash("s"): // single step alias
    case hash("step"): 
     SUSPEND(); // reset Status, next time execute will suspend
     return;
    case hash("ba"): // break add alias
    case hash("breakadd"):
    case hash("br"): // break remove alias
    case hash("breakrm"): {
     // usage: ba/breakadd/br/breakrm b/bytecode/f/function DATA[,Other DATA]
     if (cmd_split.size() != 3) break;

     BreakType type = BreakTypeChooser(cmd_split.at(1));
     std::function<bool(BreakType,string)> operater;
     if (cmd_split.at(0) == "ba"||cmd_split.at(0) == "breakadd") {
      operater = std::bind(&V8BytecodeDebugger::AddBreak, &GetInstance(), std::placeholders::_1, std::placeholders::_2);
     } else {
      operater = std::bind(&V8BytecodeDebugger::RemoveBreak, &GetInstance(), std::placeholders::_1, std::placeholders::_2);
     }
     
     vector<string> args = str_split(cmd_split.at(2), ",");
     for (string d: args) operater(type, d);
     break;
    }
    case hash("bbl"): // break bytecode list
     cout << "\rBreak Bytecode List:\n";
     for (string b: GetInstance().break_bytecode) {
      cout << b << "\n";
     }
     break;
    case hash("bfl"): // break function list
     cout << "\rBreak Function List:\n";
     for (string b: GetInstance().break_function) {
      cout << b << "\n";
     }
     break;
    case hash("sb"): // show current bytecode (switch)
    case hash("showbytecode"):
     GetInstance().show_current_bytecode = !GetInstance().show_current_bytecode;
     break;
    case hash("sf"): // show current function (switch)
    case hash("showfunction"):
     GetInstance().show_current_function = !GetInstance().show_current_function;
     break;
    case hash("sc"): // show constant pool
    case hash("showconstant"):
     PrintBytecodeConstantPool();
     break;
    case hash("int3"): // int3 for system no alias
     DebugBreak();
     break;
    case hash("e!"): // exit!
    case hash("exit"):
     exit(0);
     return;
   }
  }
 }

 void V8BytecodeDebugger::PrintBytecodeConstantPool() { // static
  FixedArray* constant_pool = GetInstance().GetBytecodeArray()->constant_pool();
  int len = constant_pool->length();
  cout << "Constant POOL len = " << len << "\n";
  int index = 0;
  while (index < len) {
   cout << "[" << index << "] ";
   constant_pool->get(index)->Print(cout);
   cout << "\n";
   index += 1;
  }
  cout << ">>>END OF Constant POOL<<<\n";
 }

 void V8BytecodeDebugger::PrintBytecodeByOffset(int offset) { // static
  const uint8_t* base_addr = GetInstance().GetBytecodeArray()->GetFirstBytecodeAddress();
  const uint8_t* bytecode_addr = base_addr + offset;
  cout << "> " << static_cast<const void*>(bytecode_addr) << " @ " << offset << " : ";
  v8::internal::interpreter::BytecodeDecoder::Decode(cout, bytecode_addr, GetInstance().GetBytecodeArray()->parameter_count());
  cout << "\n";
 }

 bool V8BytecodeDebugger::IsTriggerBytecodeBreak(Bytecode bcode) { // static
  if (GetInstance().break_bytecode.size() == 0) return false; // empty 
  const char* bytecode = Bytecodes::ToString(bcode);
  for (string b: GetInstance().break_bytecode) {
   if(strcmp(b.c_str(), bytecode) == 0) return true;
  }
  return false;
 }

 bool V8BytecodeDebugger::IsTriggerFunctionBreak() { // static
 if (GetInstance().break_function.size() == 0) return false; // empty 
 for (string f: GetInstance().break_function) {
   if(strcmp(f.c_str(), GetInstance().GetCurrentFunctionName().get()) == 0) return true;
  }
  return false;
 }

 void V8BytecodeDebugger::HandleJSFunctionExternal(Address address) { // static
  // Due to Hook InterpreterEntryTrampoline, so every time hook function called
  // needs to reset previous function name
  GetInstance().ResetCurrentFunctionName();
  
  JSFunction* jsfunction = reinterpret_cast<JSFunction*>(address);
  GetInstance().SetJSFunction(jsfunction);
  GetInstance().SetBytecodeArray((BytecodeArray*)jsfunction->shared()->function_data());

  // Debug... 20240315
  if (GetInstance().show_current_function)
  cout<<"[-]Current Function -> "<<GetInstance().GetCurrentFunctionName().get()<<"\n";
 }

 unique_ptr<char[]>& V8BytecodeDebugger::GetCurrentFunctionName() {
  if (current_function_name.get() != nullptr) return current_function_name;
  if (GetJSFunction() == nullptr) return unique_ptr<char[]>(nullptr);
  Handle<JSFunction> jsfunction = Handle<JSFunction>(GetJSFunction());
  current_function_name = GetJSFunction()->GetName(jsfunction)->ToCString();
  return current_function_name;
 }

 bool V8BytecodeDebugger::AddBreak(BreakType type_b, string data_b) {
  if (type_b == BreakType::kUnknown) return false;
  vector<string>* datas = nullptr;
  switch (type_b) {
   case BreakType::kBytecode:
    datas = &break_bytecode;
    break;
   case BreakType::kFunction:
    datas = &break_function;
    break;
  }
  datas->push_back(data_b);
  return true;
 }

 bool V8BytecodeDebugger::RemoveBreak(BreakType type_b, string data_b) {
  if (type_b == BreakType::kUnknown) return false;
  vector<string>* datas = nullptr;
  vector<string>::const_iterator cit;
  switch (type_b) {
   case BreakType::kBytecode:
    datas = &break_bytecode;
    break;
   case BreakType::kFunction:
    datas = &break_function;
    break;
  }
  if (datas == nullptr) return false;
  cit = datas->begin();
  for (string cs: *datas) {
   if (cs.compare(data_b) == 0) {
    datas->erase(cit);
    return true;
   }
   cit++;
  }
  return false;
 }
} // v8_debugger

namespace v8 {
namespace internal {
  // arg0 Current Bytecode Offset
  RUNTIME_FUNCTION(Runtime_V8BytecodeDebugger) {
   SealHandleScope shs(isolate);
   int bytecode_offset = args.smi_at(0);
   bytecode_offset -= 0x21; // offset to Current Offset

   if (v8_debugger::V8BytecodeDebugger::IsNeedUpdateBytecodeArray()) {
    cout << "update bytecode array...\n";
    BytecodeArray* b_array = *args.at<BytecodeArray>(1).location();
    v8_debugger::V8BytecodeDebugger::GetInstance().SetBytecodeArray(b_array);
    v8_debugger::V8BytecodeDebugger::SetUpdateBytecodeArray(false);
   }

   // Every time bytecode execute, needs to check interrupt
   v8_debugger::V8BytecodeDebugger::BytecodeInterrupt(bytecode_offset);
   return isolate->heap()->undefined_value();
  }
} // internal
} // v8
