/*
 * Library Manipulation API Sample, part of the Interflop project.
 * analyse.cpp
 *
 * This file analyses the current backend used in order to determine the
 * different registers used by it. The goal is to know what needs to be saved
 * by the frontend, so that the backend functions don't modify any register
 * used by the application in an unpredictable way. The registers checked for
 * GPRs and Floating points/SIMD registers.
 */

#include <string.h>
#include <algorithm>
#include <fstream>
#include <iostream>

#include "drsyms.h"

#include "analyse.hpp"
#include "utils.hpp"

/**
 * This bool is used to know if DynamoRIO still puts implicit operands
 * at the end of the sources, rather than at their normal place.
 */
static bool NEED_SSE_INVERSE = false;

/**
 * This vector contains addresses and is used so that we don't follow each 
 * and every call when we already checked it once.
 */
static std::vector<app_pc> app_pc_vect;

/**
 * These vectors contain the different registers used by the backend.
 * gpr_reg contains all the GPR used, while float_reg contains all the
 * XMM/YMM/ZMM used on X86, or the Q/D/S/B/H on AArch64.
 */
static std::vector<reg_id_t> gpr_reg;
static std::vector<reg_id_t> float_reg;

/**
 * @brief Getter for the NEED_SSE_INVERSE boolean
 * @return NEED_SSE_INVERSE
 */
bool get_need_sse_inverse(){
    return NEED_SSE_INVERSE;
}

/**
 * @brief Setter for the NEED_SSE_INVERSE boolean
 * 
 * @param new_value The new value for NEED_SSE_INVERSE
 */
void set_need_sse_inverse(bool new_value){
    NEED_SSE_INVERSE = new_value;
}

/**
 * @brief Getter for the gpr_reg vector
 * @return gpr_reg
 */
std::vector<reg_id_t> get_gpr_reg(){
    return gpr_reg;
}

/**
 * @brief Getter for the float_reg vector
 * @return float_reg
 */
std::vector<reg_id_t> get_float_reg(){
    return float_reg;
}

/**
 * @brief Gather the gpr_reg and float_reg vectors into a single one,
 * containing all registers used by the backend
 * @return The combination of both vectors
 */
std::vector<reg_id_t> get_all_registers(){
    std::vector<reg_id_t> ret;

    for(auto reg: gpr_reg){
        if(std::find(ret.begin(), ret.end(), reg) 
                == ret.end()){
            ret.push_back(reg);
        }
    }

    for(auto reg: float_reg){
        if(std::find(ret.begin(), ret.end(), reg) 
                == ret.end()){
            ret.push_back(reg);
        }
    }

    return ret;
}

/**
 * @brief Helper function to print a certain number of \t, used as way to
 * know when we follow a call through
 * 
 * @param tabs The number of tabs
 */
static void print_tabs(int tabs){
    for(int i = 0; i < tabs; ++i)
        dr_printf("\t");
}

/**
 * @brief Prints the content of a vector of reg_id_t
 * @details Iterate over a vector of reg_id_t and prints the names
 * of each register, according to their value in the reg_id_t enum.
 * 
 * @param vect The vector to print
 */
static void print_vect(std::vector<reg_id_t> vect){
    for(auto i = vect.begin(); i != vect.end(); ++i){
        /* We use get_register_name to get the name of a register
         * by giving it the function it's number
         */
        dr_printf("%s", get_register_name(*i));
        if(i+1 != vect.end()){
            dr_printf(", ");
        }
    }
}

/**
 * @brief Prints the content of the gpr_reg and float_reg
 * register, using print_vect for each.
 */
void print_register_vectors(){
    dr_printf("List of gpr registers : \n\t");
    print_vect(gpr_reg);
    dr_printf("\nList of float registers : \n\t");
    print_vect(float_reg);
    dr_printf("\n");
}

/**
 * @brief Adds a reg_id_t to gpr_reg or float_reg according to it's status
 * @details Checks whether a given register is a GPR or a FP register.
 * In both cases, iterate through the 
 * 
 * @param reg [description]
 */
static void add_to_vect(reg_id_t reg){
    bool add_reg = true;
    reg_id_t to_remove = DR_REG_NULL;

    if(reg_is_gpr(reg)){
        for(auto temp: gpr_reg){
            if(reg == temp || reg_overlap(reg, temp)){
                add_reg = false;
                break;
            }else if(reg_overlap(temp, reg)){
                to_remove = temp;
                break;
            }
        }
        if(to_remove != DR_REG_NULL){
            gpr_reg.erase(
                std::remove(gpr_reg.begin(), gpr_reg.end(), to_remove), 
                gpr_reg.end());
        }
        if(add_reg){
            gpr_reg.push_back(reg);
        }

    }else if(reg_is_strictly_xmm(reg) || reg_is_strictly_ymm(reg)
            || reg_is_strictly_zmm(reg)){
        for(auto temp: float_reg){
            if(reg == temp || reg_overlap(reg, temp)){
                add_reg = false;
                break;
            }else if(reg_overlap(temp, reg)){
                to_remove = temp;
                break;
            }
        }
        if(to_remove != DR_REG_NULL){
            float_reg.erase(
                std::remove(float_reg.begin(), float_reg.end(), to_remove), 
                float_reg.end());
        }
        if(add_reg){
            float_reg.push_back(reg);
        }
    }
}

static void fill_reg_vect(instr_t *instr){
    opnd_t operand;
    reg_id_t reg1, reg2;
    int limit;

    limit = instr_num_srcs(instr);
    for(int i = 0; i < limit; ++i){
        reg1 = DR_REG_NULL;
        reg2 = DR_REG_NULL;
        operand = instr_get_src(instr, i);
        if(opnd_is_reg(operand)){
            reg1 = opnd_get_reg(operand);
        }else if(opnd_is_base_disp(operand)){
            reg1 = opnd_get_base(operand);
            reg2 = opnd_get_index(operand);
        }
        add_to_vect(reg1);
        add_to_vect(reg2);
    }

    limit = instr_num_dsts(instr);
    for(int i = 0; i < limit; ++i){
        reg1 = DR_REG_NULL;
        reg2 = DR_REG_NULL;
        operand = instr_get_dst(instr, i);
        if(opnd_is_reg(operand)){
            reg1 = opnd_get_reg(operand);
        }else if(opnd_is_base_disp(operand)){
            reg1 = opnd_get_base(operand);
            reg2 = opnd_get_index(operand);
        }
        add_to_vect(reg1);
        add_to_vect(reg2);
    }
}

static void show_instr_of_symbols(void *drcontext, module_data_t* lib_data, 
    size_t offset, int tabs){
    instrlist_t* list_bb = decode_as_bb(drcontext, lib_data->start + offset);
    instr_t *instr = nullptr, *next_instr = nullptr;
    app_pc apc = 0;

    for(instr = instrlist_first_app(list_bb); instr != NULL; instr = next_instr){
        next_instr = instr_get_next_app(instr);
        if(get_log_level() >= 3){
            print_tabs(tabs);
            dr_print_instr(drcontext, STDOUT, instr , "ENUM_SYMBOLS : ");
        }

        fill_reg_vect(instr);

        if(instr_is_ubr(instr) || instr_is_cbr(instr) ||
            instr_get_opcode(instr) == OP_jmp_ind ||
            instr_get_opcode(instr) == OP_jmp_far_ind){
            show_instr_of_symbols(drcontext, lib_data, 
                instr_get_app_pc(instr) 
                    - lib_data->start 
                    + instr_length(drcontext, instr), tabs);
        }

        if(instr_is_call(instr)) {
            apc = opnd_get_pc(instr_get_src(instr, 0));
            if(std::find(app_pc_vect.begin(), app_pc_vect.end(), apc) 
                == app_pc_vect.end()){
                app_pc_vect.push_back(apc);
                if(get_log_level() >= 3){
                    print_tabs(tabs);
                    dr_printf("on ajoute l'app_pc = %p au vecteur\n\n\n", apc);
                }
                
                show_instr_of_symbols(drcontext, lib_data, 
                    apc - lib_data->start, tabs + 1);
            }else{
                if(get_log_level() >= 3){
                    print_tabs(tabs);
                    dr_printf("l'app_pc = %p a déjà été vu\n\n\n", apc);
                }
            }

            show_instr_of_symbols(drcontext, lib_data, 
                instr_get_app_pc(instr) 
                    - lib_data->start 
                    + instr_length(drcontext, instr), tabs);
        }
    }

    instrlist_clear_and_destroy(drcontext, list_bb);
}

static void analyse_symbol_test_sse_src(void *drcontext, module_data_t* lib_data, size_t offset) {

    instrlist_t* list_bb = decode_as_bb(drcontext, lib_data->start + offset);
    instr_t *instr = nullptr, *next_instr = nullptr;
    app_pc apc = 0;

    reg_id_t reg_src0_instr0 = DR_REG_NULL, reg_src0_instr1 = DR_REG_NULL;

    for(instr = instrlist_first_app(list_bb); instr != NULL; instr = next_instr) {
        next_instr = instr_get_next_app(instr);
         
        if(instr_get_opcode(instr) == OP_divpd) { reg_src0_instr0 = opnd_get_reg(instr_get_src(instr,0)); }
        if(instr_get_opcode(instr) == OP_vdivpd) { reg_src0_instr1 = opnd_get_reg(instr_get_src(instr,0)); }
    }
 
    if(reg_src0_instr0 != DR_REG_NULL && reg_src0_instr1 != DR_REG_NULL) {
        if(reg_src0_instr0 != reg_src0_instr1) {
            if(get_log_level() > 1) {
                dr_fprintf(STDERR , "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
                dr_fprintf(STDERR , "!!!!!!!!!!!!! WARNING : SSE sources order is still incorrect in DynamoRIO, so we need to invert them !!!!!!!!!!!!!\n");
                dr_fprintf(STDERR , "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
            }
            set_need_sse_inverse(true);
        }
       
    }

    instrlist_clear_and_destroy(drcontext, list_bb);
}

static void write_vect(std::ofstream& analyse_file, std::vector<reg_id_t> vect,
    const char* vect_type){
    analyse_file << vect_type << '\n';
    for(auto i = vect.begin(); i != vect.end(); ++i){
        analyse_file << ((ushort)*i) << '\n';
    }
}

static void write_reg_to_file(const char* path){
    std::ofstream analyse_file;
    analyse_file.open(path);
    if(analyse_file.fail()){
        dr_fprintf(STDERR, "FAILED TO OPEN THE GIVEN FILE FOR WRITING : \"%s\"\n", 
            path);
    }else{
        write_vect(analyse_file, gpr_reg, "gpr_reg");
        write_vect(analyse_file, float_reg, "float_reg");
        analyse_file.flush();
        analyse_file.close();
    }
}

static bool read_reg_from_file(const char* path){
    std::ifstream analyse_file;
    std::string buffer;
    int line_number = 0;
    bool float_vect = false;

    analyse_file.open(path);
    if(analyse_file.fail()){
        dr_fprintf(STDERR, "FAILED TO OPEN THE GIVEN FILE FOR READING : \"%s\"\n", 
            path);
        return true;
    }
    while(std::getline(analyse_file, buffer)){
        std::cout << buffer << '\n';
        if(buffer == "gpr_reg"){
            float_vect = false;
        }else if(buffer == "float_reg"){
            float_vect = true;
        }else if(is_number(buffer)){
            (float_vect ? float_reg : gpr_reg).push_back((reg_id_t)std::stoi(buffer));
        }else{
            dr_fprintf(STDERR, "FAILED TO CORRECTLY READ THE FILE : Problem on file line %d = \"%s\"\n", 
                line_number, buffer);
            analyse_file.close();
            return true;
        }
        ++line_number;
    }
    analyse_file.close();
    return false;
}

bool enum_symbols_registers(const char *name, size_t modoffs, void *data){
    void *drcontext = nullptr;
    module_data_t* lib_data = nullptr;

    std::string str(name);
    if(str.find("<>::apply") != std::string::npos) {
        drcontext = dr_get_current_drcontext();
        lib_data = dr_lookup_module_by_name("libinterflop.so");
        show_instr_of_symbols(drcontext, lib_data, modoffs, 0);
        dr_free_module_data(lib_data);
    }

    return true;
}

bool enum_symbols_sse(const char *name, size_t modoffs, void *data){
    void *drcontext = nullptr;
    module_data_t* lib_data = nullptr;

    std::string str(name);

    if(str.find("test_sse_src_order") != std::string::npos) {
        drcontext = dr_get_current_drcontext();
        lib_data = dr_lookup_module_by_name("libinterflop.so");
        analyse_symbol_test_sse_src(drcontext , lib_data , modoffs);
        dr_free_module_data(lib_data);
    }

    return true;
}

static void path_to_library(char* path, size_t length){
    dr_get_current_directory(path, length);
    strcat(path, "/api/bin/libinterflop.so");
}

static bool AA_argument_detected(const char* file){
    char path[256];
    path_to_library(path, 256);
    if(drsym_enumerate_symbols(path, enum_symbols_registers, 
        NULL, DRSYM_DEFAULT_FLAGS) == DRSYM_SUCCESS){
        write_reg_to_file(file);
    }else{
        dr_fprintf(STDERR, 
            "ANALYSE FAILURE : Couldn't finish analysing the symbols of the library\n");
    }
    return true;
}

bool analyse_argument_parser(std::string arg, int* i, int argc, const char* argv[]){
    if(arg == "--analyse_abort" || arg == "-aa"){
        *i += 1;
        if(*i < argc){
            return AA_argument_detected(argv[*i]);
        }else{
            dr_fprintf(STDERR, 
                "ANALYSE FAILURE : File not given for \"-aa\"\n");
            return true;
        }
    }/*else if(arg == "--analyse_file" || arg == "-af"){
        set_analyse_mode(IFP_ANALYSE_NOT_NEEDED);
        *i += 1;
        return read_reg_from_file(argv[*i]);
    }else if(arg == "--analyse_run" || arg == "-ar"){
        set_analyse_mode(IFP_ANALYSE_NOT_NEEDED);
        char path[256];
        path_to_library(path, 256);
        if(drsym_enumerate_symbols(path, enum_symbols, NULL, DRSYM_DEFAULT_FLAGS)
            != DRSYM_SUCCESS){
            dr_fprintf(STDERR, 
                "ANALYSE FAILURE : Couldn't finish analysing the backend\n");
            return true;
        }
    }*/else{
        inc_error();
    }
    return false;
}

void analyse_mode_manager(){
    char path[256];
    path_to_library(path, 256);
    if(drsym_enumerate_symbols(path, enum_symbols_sse, 
        NULL, DRSYM_DEFAULT_FLAGS) != DRSYM_SUCCESS){
        dr_fprintf(STDERR, 
            "ANALYSE FAILURE : Couldn't finish analysing the symbols of the library\n");
    }
    /*switch(get_analyse_mode()){
        case IFP_ANALYSE_NEEDED:
            char path[256];
            path_to_library(path, 256);
            if(drsym_enumerate_symbols(path, enum_symbols, NULL, DRSYM_DEFAULT_FLAGS) 
                != DRSYM_SUCCESS){
                DR_ASSERT_MSG(false, 
                    "ANALYSE FAILURE : Couldn't finish analysing the backend\n");
            }
            break;
        default:
            break;
    }*/
}
    
void test_sse_src_order() {
    __asm__ volatile(
            "\t.intel_syntax;\n"
            "\tdivpd %xmm0, %xmm1;\n"
            "\tvdivpd %xmm0, %xmm0, %xmm1;\n"
            "\t.att_syntax;\n"
            );
}