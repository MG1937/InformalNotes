# Author: MG193.7
# Email: mgaldys4@gmail.com
# Date: 20240505 8:01 AM
import re
import hashlib
import string
import random
from copy import deepcopy

DEBUG = False

# register type, for register optimize period
TYPE_DATA = 0
TYPE_CALL = 1
TYPE_FUNC = 2
TYPE_REG = 3

# MAGIC Number
MAGIC_CALL = "000" + "".join(random.choices(string.digits, k = 5)) + "000"

def MAGIC_CALL_REG(R_D):
    return R_D + MAGIC_CALL

def UN_MAGIC_CALL_REG(R_D):
    return R_D.replace(MAGIC_CALL, "")

# What we care only if needs to open stack
KEYWORD = ["if", "else", "elseif", "for", "function", "end"]
DATA_PLACEHOLDER = ["nil", "true", "false"]

# for restore line-unuse_reg data for stack code
# specific key: stack code line data
# experiment 20240514 11:40 AM
GLOBAL_STACK_CODE = {}

def LOG(strs):
    if DEBUG:
        print(strs)

def rand_str():
    return "".join(random.choices(string.ascii_uppercase + string.digits, k = 32))

def gen_stack_key():
    return "STACK_CODE_" + rand_str()

def save_stack_code(line):
    global GLOBAL_STACK_CODE
    key = gen_stack_key()
    GLOBAL_STACK_CODE[key] = line
    return key

def update_stack_code(key, line):
    global GLOBAL_STACK_CODE
    GLOBAL_STACK_CODE[key] = line

def get_stack_code(key):
    global GLOBAL_STACK_CODE
    key = key.strip()
    return GLOBAL_STACK_CODE[key]

def is_stack_code(code):
    return code.startswith("STACK_CODE_") if code != None else False

def count_tab(line):
    if line == None:
        return 0
    match = re.match(r"^\s*", line)
    return len(match.group(0)) if match else 0

def gen_data(data, dtype):
    return [data, dtype]

# filter flow control operate
def is_flow_control(tokens):
    if tokens == None or len(tokens) == 0:
        return False
    for k in KEYWORD:
        # flow control keyword usually in tokens' first word
        if tokens[0] == k:
            return True
    return False

# filter data placeholder
def is_data_placeholder(tokens):
    for t in tokens:
        if DATA_PLACEHOLDER.count(t) == 1:
            return True
    return False

# flow spread, usually happen in func call or register assign
# TODO, Maybe still buggy in token replace 20240505 08:55 AM
def flow_spread(R_token, register):
    # Flow spread should avoid taint params
    regs = []
    if "for " in R_token and " in " in R_token and "do" in R_token:
        regs = find_for_registers(R_token)
    else:
        regs = find_registers(R_token)
    for r in regs:
        if r in register and (register[r] != None and register[r][1] != TYPE_CALL):
            # Buggy!!!! may over taint!!!
            R_token = R_token.replace(r, str(register[r][0]))
    return R_token

# line_code e.g: local L1, L2...
def init_registers(line_code):
    # Func head will declare local keyword
    # we take it as declare registers!
    regs = {}
    if "local " not in line_code:
        return regs
    line_code = line_code.replace("local","").strip()
    for r in line_code.split(","):
        # init registers now!
        regs[r.strip()] = None
    return regs

def find_registers(line_code):
    return re.findall(r"(L\d+_\d+|A\d+_\d+)", line_code)

def find_for_registers(line_code):
    finds = []
    param_find = re.findall(r"for.+in(.+)do", line_code)[0]
    return find_registers(param_find)

def find_call_registers(line_code):
    finds = []
    param_find = re.findall(r"\w+\((.*)\)", line_code)[0]
    for p in param_find.split(","):
        if "(" in p:
            finds += find_call_registers(p)
        else:
            finds += find_registers(p)
    return finds

def find_call_name_registers(line_code):
    finds = []
    param_find = re.findall(r"(\w+)\(", line_code)[0]
    return find_registers(param_find)

# Optimize and cut the unused registers, usually call after register optimizer
# 20240505 2:55 PM
# param code: for input code needs to be optimized
# param in_stack: for mark whether current domain is in stack
def body_optimizer(code, in_stack = False):
    codes = code.split("\n")
    
    # line: [L1, L2...]
    line = []
    unused_regs = []

    # record current domain stack tab, for check whether line in stack code
    current_stack_tab = count_tab(codes[0])

    # when meet flow control token, this count +1
    open_stack_count = 0

    # We want cut frame local registers in the end
    # if frame local reg not None, maybe we are in new domain!!
    frame_local_regs = list(init_registers(codes[0]).keys())

    def put_pc(line_code, unuse = None):
        nonlocal line
        LOG("put_pc >> " + line_code + " >>> " + str(unuse))
        line += [[line_code + "\n", unuse]]

    def replace_pc(num, code, unuse):
        line[num] = [code + "\n", unuse]

    def del_pc(num, line_ = None):
        nonlocal line
        if line_ == None:
            line_ = line
        LOG("really del pc >>> " + str(line_[num]))
        line_[num] = [None, None]

    def get_pc_unuse(num, line_ = None):
        nonlocal line
        if line_ == None:
            line_ = line
        return line_[num][1]

    def add_unused_table(unuse):
        nonlocal unused_regs
        unused_regs += unuse if unuse != None else []
        unused_regs = list(set(unused_regs))
    
    def del_unused_table(unuse):
        nonlocal unused_regs
        for u in unuse:
            if u  in unused_regs:
                unused_regs.remove(u)

    def join_all(stack_code_key = None):
        tmp_code = ""
        if stack_code_key == None:
            line_ = line
        else:
            LOG("join_all, find stack code key >>> " + stack_code_key)            
            line_ = get_stack_code(stack_code_key)

        for l in line_:
            tmp = l[0]
            if is_stack_code(tmp):
                LOG("Try to join stack code >>> " + tmp)
                tmp = join_all(tmp)
            tmp_code += (tmp if tmp != None else "") + ("\n" if tmp != None and not tmp.endswith("\n") else "")
            LOG("join >> " + str(l[0]).replace("\n","") + " >> " + str(l[1]))
        return tmp_code.rstrip("\n")

    def cut_unuse_pc(unuse, stack_code_key = None):
        LOG("try cut unuse pc >> " + str(unuse))
        if stack_code_key == None:
            line_ = line
        else:
            LOG("cut unuse pc, find stack code key >>> " + stack_code_key)
            line_ = get_stack_code(stack_code_key)

        for pc in range(len(line_)):
            allow_cut = False
            LOG("try cut pc... " + str(pc) + " >> " + str(line_[pc][0]))
            # e.g: L1, L2...
            mark = get_pc_unuse(pc, line_)
            if mark == None or len(mark) == 0:
                LOG("mark is null >> " + str(line_[pc][0]) + " >> " + str(mark))
                continue

            # Mark is current pc the stack code
            is_in_stack_code = is_stack_code(line_[pc][0])
            if not is_in_stack_code:
                if len(unuse) < len(mark):
                    LOG("unuse < mark(not stack) >> " + str(line_[pc][0]) + " >>>mark>>> " + str(line_[pc][1]))
                    continue
                # Cut pc requires Full match!
                # But full match rule only fit single line_ cut
                # not fit with stack code block cut 20240506 10:10 PM
                for m in mark:
                    if m not in unuse:
                        LOG("m not in unuse(not stack) >> " + str(line_[pc][0]) + " >>>mark>>> " + str(line_[pc][1]))
                        break
                    allow_cut = True
            else:
                for u in unuse:
                    if u in mark:
                        allow_cut = True
                        break
                if DEBUG and not allow_cut:
                    LOG("u not in mark (stack) >> " + str(line_[pc][0]) + " >>>mark>>> " + str(line_[pc][1]))

            if allow_cut:
                # Cut pc for cut unused register!
                LOG("del " + line_[pc][0].replace("\n",""))
                LOG("unuse " + str(mark))
                LOG("passed in unuse" + str(unuse))
                if is_in_stack_code:
                    # only cut the unuse reg exist both in mark and passed-in unuse reg
                    tmp_only_cut = []
                    for u in unuse:
                        if u in mark:
                            tmp_only_cut += [u]
                    LOG("try cut in stack code >>> " + str(line_[pc][0]) + " >>> " + str(tmp_only_cut))
                    cut_unuse_pc(tmp_only_cut, line_[pc][0])
                    continue
                del_pc(pc, line_)
        # del unuse table any way
        del_unused_table(unuse)
        return

    def remove_previous_unuse_tag(unuse, stack_code_key = None):
        if stack_code_key == None:
            line_ = line
        else:
            LOG("remove_previous_unuse_tag, find stack code key >>> " + stack_code_key) 
            line_ = get_stack_code(stack_code_key)

        if unuse == None or len(unuse) == 0:
            return

        for pc in range(len(line_)):
            mark = get_pc_unuse(pc, line_)
            if mark == None or len(mark) == 0:
                continue
            for u in unuse:
                if u in mark:
                    mark.remove(u)
            if is_stack_code(line_[pc][0]):
                remove_previous_unuse_tag(unuse, line_[pc][0])
            LOG("remove unuse tag >> " + str(line_[pc][0]).replace("\n","") + " >>> " + str(line_[pc][1])  + " >>>unuse " + str(unuse))
        del_unused_table(unuse)
        return

    # Stack config
    stack_tab = -1
    stack_code = ""

    for pc in codes:
        LOG("current pc " + pc)
        tokens = pc.strip().split(" ")
        unuse = None
        if stack_tab != -1 or is_flow_control(tokens):
            tab_num = count_tab(pc)
            # Close Stack
            if stack_tab == tab_num or tab_num < stack_tab:                
                tmp_code, tmp_unuse = body_optimizer(stack_code, True)

                put_pc(tmp_code, tmp_unuse)
                LOG("close stack, code block >>> " + stack_code + " >>>block unuse>>> " + str(tmp_unuse))
                if is_stack_code(tmp_code):
                    LOG("package into stack code specific key>>> " + tmp_code)
                put_pc(pc)
                stack_tab = -1
                stack_code = ""
                add_unused_table(tmp_unuse)
                # elseif or else keyword, need to open stack again
                if "else" in tokens[0]:
                    stack_tab = tab_num
                    use_regs = find_registers(pc)
                    LOG("elseif open stack again >> " + str(use_regs) + " >>> " + pc)
                    remove_previous_unuse_tag(use_regs)
                continue
            # Open Stack
            elif stack_tab == -1:
                stack_tab = tab_num
                use_regs = find_registers(pc) if "function" != tokens[0] else []
                LOG("open stack remove unuse >> " + str(use_regs) + " >>> " + pc)

                if "for" == tokens[0]:
                    for_finder = list(re.findall(r"for\s(.+)in\s(.+)\sdo", pc)[0])
                    for_in_use_regs = find_registers(for_finder[1])
                    remove_previous_unuse_tag(for_in_use_regs)
                    for_use_regs = find_registers(for_finder[0])
                    cut_unuse_pc(for_use_regs)
                elif "function" == tokens[0]:
                    # Function declare should be same as register assign
                    function_use_regs = find_call_name_registers(pc)
                    cut_unuse_pc(function_use_regs)
                else:
                    remove_previous_unuse_tag(use_regs)

                put_pc(pc)
                continue
            else:
                stack_code += pc + "\n"
                if "goto " in pc:
                    remove_previous_unuse_tag(unused_regs)
                continue

        # Register assign
        elif tokens.count("=") == 1:
            tmp_exp = pc.split("=", 1)
            R_D = tmp_exp[0].replace(MAGIC_CALL, "").strip()
            R_S = tmp_exp[1].strip()
            # Mark whether to optimize current pc
            no_optimize = False
            unuse = find_registers(R_D)

            LOG("find unuse " + str(unuse) + " >>> " + str(pc))
            use_reg = find_registers(R_S)
            if (not R_S.startswith("\"") and ("(" in R_S or "{}" in R_S)) or "[" in R_D or "." in R_D:
                LOG("current pc no optimize!!!")
                no_optimize = True

            # Avoid over taint external register!
            # Although optimize external register may not cause semantic distortion
            # but for code audit, researcher still needs to know what happen to external register!
            #for u in unuse:
            #    if u not in frame_local_regs:
            #        no_optimize = True

            if len(use_reg) != 0:
                for u in use_reg:
                    if u in unuse:
                        unuse.remove(u)
                remove_previous_unuse_tag(use_reg)
                LOG("use_reg " + str(use_reg) + " >>> " + str(pc))
            
            # Anyway, optmize previous pc first
            cut_unuse_pc(unuse)
            
            # even in only cut mode, we still add unuse register
            # because only cut may still leave some useless register
            # we need to record it, wait for future adjust 20240507 3:06 PM
            add_unused_table(unuse)
            put_pc(pc, unuse if not no_optimize else None)
        
        # Call or Return operate
        elif ("(" in tokens[0] or "return" == tokens[0]):
            use_reg = find_registers(pc)
            LOG("call operate remove unuse tag >> " + str(use_reg) + " >>> " + pc)
            remove_previous_unuse_tag(use_reg)
            put_pc(pc, None)
        elif "goto" == tokens[0]:
            # when goto happens, avoid optimize code block/current domain, make sure the follow code block not get affected
            remove_previous_unuse_tag(unused_regs)
            put_pc(pc, None)
        else:
            put_pc(pc, None)

    if in_stack:
        # once leave the domain, local registers no need to exists.
        LOG("Clear frame local regs >>> " + str(frame_local_regs))
        if len(frame_local_regs) != 0:
            cut_unuse_pc(frame_local_regs)
        key = save_stack_code(line)
        return key, unused_regs
    else:
        LOG("Really goes to end, cut all unused_regs again!")
        cut_unuse_pc(deepcopy(unused_regs))

    LOG("current unused_regs >>> " + str(unused_regs))
    opt_code = join_all()
    return opt_code if not in_stack else (opt_code, unused_regs)

def register_optimizer(code, regs = {}):
    # Handle code line by line
    code = code.split("\n")
    tmp_code = ""

    # if current count tab is greater than 2, means in stack
    in_stack = count_tab(code[0]) > 2

    # make sure GLOBAL register do not contain detail value
    # for avoid over taint! because GLOBAL register may change its
    # value by calling other function, it is unstable, may cause semantic distortion
    # so we assume optimizer can not predict GLOBAL register value
    # but local frame register is predictable, we can follow the control flow to know how register change
    # so, wape all GLOBAL register data when optimize function
    register = deepcopy(regs)
    # if count tab equals 2, means optimize start analyse formal function(not ananymous function)
    if count_tab(code[0]) == 2:
        register = {}

    # Local registers belong to current domain
    local_regs = init_registers(code[0])
    register.update(local_regs)

    # record external register not belong to current stack, for avoid over taint
    used_external_register = {}

    # Flow Control config
    stack_tab = -1
    stack_code = "" # code of stack body
    stack_open_line = ""

    pc = -1
    while pc < len(code) - 1:
        pc += 1
        pc_line = code[pc]
        #print("current regpc>> " + str(pc) + " >> " + pc_line)
        # We need a tmp line, because we need to optimize by origin code
        # Also need a token parser 20240505 8:41 AM
        tab_num = count_tab(pc_line)
        tmp_line = pc_line.strip()
        tokens = tmp_line.split(" ")

        # Handle Flow control first! Because we want to record stack!
        if stack_tab != -1  or is_flow_control(tokens):
            #print("current in stack!!")
            # Open stack but make sure avoid over taint
            if stack_tab == -1:
                stack_tab = tab_num
                stack_open_line = tmp_line

                # Avoid taint function handler
                if "function" == tokens[0]:
                    register[tokens[1].split("(")[0]] = None

                tmp_exp = flow_spread(pc_line, register)
                tmp_code += tmp_exp + "\n"

                continue
            # Close stack if stack tab equals current tab, dont care what token is
            elif stack_tab == tab_num or tab_num < stack_tab:
                # stack closed, we need to flow spread the stack open code first

                no_register = False
                if stack_open_line.startswith("for "):
                    # for handler may keep cover the assigned external register,
                    # if spreading current registers may cause over taint
                    # sample, L2 is external register:
                    # for xxx in xxx do
                    #   L1 = L2
                    #   L2 = L1 .. "&"
                    # end
                    # spread L2 may cause semantic distortion
                    no_register = True

                optimize_result = register_optimizer(stack_code, register if not no_register else {})
                used_external_register.update(optimize_result[1])
                register.update(optimize_result[1])

                tmp_code += optimize_result[0] + "\n"
                tmp_code += flow_spread(pc_line, register) + "\n"
                stack_code = ""
                stack_tab = -1
                # When meet elseif or else, we need to open another stack
                if "else" in tokens[0]:
                    stack_tab = tab_num
                continue
            else:
                # We still in stack
                stack_code += pc_line + "\n"
                # if goto happens, avoid taint any registers!
                # for avoid affect following register use
                if "goto " in pc_line:
                    register = {}
                continue
                
        # Register assign operate!
        elif tokens.count("=") == 1:
            tmp_exp = pc_line.split("=", 1)
            R_D = tmp_exp[0].strip()
            R_S = tmp_exp[1].strip()

            # R_D = "R_S"
            # R_D = R_S(X)
            if R_S.startswith("\"") or R_S.isnumeric():
                register[R_D] = gen_data(R_S, TYPE_DATA)
            elif "(" in R_S:
                # if R_S is call operate, do this MAGIC, rename current R_D,
                # redirect R_D in register to renamed R_D, which can avoid over taint
                # problem. here is sample, if no MAGIC, L2_2 = L1_1 will not be optimized,
                # then cause semantic distortion:
                # L1_1 = DO_CALL(xxx)
                # L2_2 = L1_1
                # L1_1 = OTHER_CALL
                # L1_1 = L1_1(L2_2)
                # if no MAGIC, result would be like, we dont want this:
                # L1_1 = DO_CALL(xxx)
                # L1_1 = OTHER_CALL
                # L1_1 = OTHER_CALL(L1_1)
                if "," not in R_D:
                    R_S = flow_spread(R_S, register)
                    magic_R_D = MAGIC_CALL_REG(R_D)
                    register[R_D] = gen_data(magic_R_D, TYPE_REG) 
                    R_D = magic_R_D
                else:
                    R_S = flow_spread(R_S, register)
                    register[R_D] = gen_data(R_D, TYPE_CALL)
            elif "{}" in R_S:
                # Avoid taint array object!!!
                register[R_D] = None
            else:
                R_S = flow_spread(R_S, register)
                register[R_D] = gen_data(R_S, TYPE_FUNC)

            # ============ Model pattern match ============ #

            tmp_R_D = R_D

            if "," in R_D:
                # L1, L2 = X
                # Avoid over taint!!!
                for tmp_r in find_registers(R_D):
                    register[tmp_r] = None

            elif "[" in R_D:
                # L1[L2] = x
                # L2 is unsed, we consider array operate as important as call operate
                tmp_R_D = flow_spread(R_D, register)

            elif MAGIC_CALL in tmp_R_D:
                # L1 = L2(xxx)
                # L3 = L1
                # optimize to
                # L3 = L2(xxx)
                tmp_next_line = code[pc + 1]
                if tmp_next_line.count("=") == 1:
                    tmp_tokens = tmp_next_line.strip().split("=")
                    tmp_next_line_R_D = tmp_tokens[0].strip()
                    tmp_next_line_R_S = tmp_tokens[1].strip()
                    if tmp_next_line_R_S == UN_MAGIC_CALL_REG(R_D):
                        # register redirect again
                        register[UN_MAGIC_CALL_REG(tmp_R_D)] = gen_data(MAGIC_CALL_REG(tmp_next_line_R_D), TYPE_REG)
                        register[tmp_next_line_R_D] = gen_data(MAGIC_CALL_REG(tmp_next_line_R_D), TYPE_REG)
                        tmp_R_D = MAGIC_CALL_REG(tmp_next_line_R_D)

                        # Make sure external register check
                        if tmp_next_line_R_D not in local_regs:
                            used_external_register[tmp_next_line_R_D] = None
                        pc += 1 # skip next line

            # ============ Model pattern match ============ #

            tmp_code += (tab_num * " ") + tmp_R_D + " = " + str(R_S) + "\n"
            
            if in_stack:
                # If in stack, untaint the in-stack-register, avoid over taint!!
                if "," in R_D:
                    for t_r in find_registers(R_D):
                        if t_r not in local_regs:
                            used_external_register[t_r] = None
                if R_D not in local_regs:
                    used_external_register[R_D] = None
                    
        # Call or other operate 20240505 10:46 AM
        elif "(" in tokens[0] or "return" in tokens[0]:
            tmp_code += (tab_num * " ") + flow_spread(tmp_line, register) + "\n"
        else:
            # When meet code block, we consider it as in new domain, wape all register record, avoid over taint!
            if pc_line.count("::") == 2:
                register = {}
            tmp_code += pc_line + "\n"
    return tmp_code.rstrip("\n"), used_external_register

f = open("./test.txt","r")
test_code = f.read()
f.close()

print(body_optimizer(register_optimizer(test_code)[0]).replace(MAGIC_CALL,""))
#print("=======================================")
#print(register_optimizer(test_code)[0])
