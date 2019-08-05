#include "analyse.hpp"

#include "../include/dr_ir_opcodes.h"
#include "../include/dr_ir_opnd.h"
#include "drmgr.h"
#include "drsyms.h"
#include <string.h>

#include <algorithm>
#include <fstream>
#include <iostream>

static std::vector<app_pc> app_pc_vect;

static std::vector<reg_id_t> gpr_reg;
static std::vector<reg_id_t> float_reg;
static std::vector<reg_id_t> fused_gpr_reg;
static std::vector<reg_id_t> fused_float_reg;

std::vector<reg_id_t> get_gpr_reg(){
    return gpr_reg;
}

std::vector<reg_id_t> get_float_reg(){
    return float_reg;
}

std::vector<reg_id_t> get_fused_gpr_reg(){
    return fused_gpr_reg;
}

std::vector<reg_id_t> get_fused_float_reg(){
    return fused_float_reg;
}

static void print_tabs(int tabs){
    for(int i = 0; i < tabs; ++i)
        dr_printf("\t");
}

static void print_vect(std::vector<reg_id_t> vect){
    for(auto i = vect.begin(); i != vect.end(); ++i){
        dr_printf("%s", get_register_name(*i));
        if(i+1 != vect.end()){
            dr_printf(", ");
        }
    }
}

void print_register_vectors(){
    dr_printf("List of gpr registers : \n\t");
    print_vect(gpr_reg);
    dr_printf("\nList of float registers : \n\t");
    print_vect(float_reg);
    dr_printf("\nList of fused gpr registers : \n\t");
    print_vect(fused_gpr_reg);
    dr_printf("\nList of fused float registers : \n\t");
    print_vect(fused_float_reg);
    dr_printf("\n");
}

static void add_to_vect(reg_id_t reg, bool fused){
    bool overlaps = false;

    if(reg_is_gpr(reg)){
        for(auto temp: (fused ? fused_gpr_reg : gpr_reg)){
            if(reg == temp)
                overlaps = true;
            if(reg_overlap(reg, temp))
                overlaps = true;
            if(overlaps)
                break;
        }

        if(!overlaps){
            (fused ? fused_gpr_reg : gpr_reg).push_back(reg);
        }

    }else if(reg_is_strictly_xmm(reg) || reg_is_strictly_ymm(reg)
        || reg_is_strictly_zmm(reg)){
        for(auto temp: (fused ? fused_float_reg : float_reg)){
            if(reg == temp)
                overlaps = true;
            if(reg_overlap(reg, temp))
                overlaps = true;
            if(overlaps)
                break;
        }

        if(!overlaps){
            (fused ? fused_float_reg : float_reg).push_back(reg);
        }
    }
}

static void fill_reg_vect(instr_t *instr, bool fused){
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
        add_to_vect(reg1, fused);
        add_to_vect(reg2, fused);
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
        add_to_vect(reg1, fused);
        add_to_vect(reg2, fused);
    }
}

static void show_instr_of_symbols(void *drcontext, module_data_t* lib_data, 
    size_t offset, int tabs, bool fused){
    instrlist_t* list_bb = decode_as_bb(drcontext, lib_data->start + offset);
    instr_t *instr = nullptr, *next_instr = nullptr;
    app_pc apc = 0;

    for(instr = instrlist_first_app(list_bb); instr != NULL; instr = next_instr){
        next_instr = instr_get_next_app(instr);
        //if(isDebug()){
            print_tabs(tabs);
            dr_print_instr(drcontext, STDOUT, instr , "ENUM_SYMBOLS : ");
        //}

        fill_reg_vect(instr, fused);

        if(instr_is_ubr(instr) || instr_is_cbr(instr) ||
            instr_get_opcode(instr) == OP_jmp_ind ||
            instr_get_opcode(instr) == OP_jmp_far_ind){
            show_instr_of_symbols(drcontext, lib_data, 
                instr_get_app_pc(instr) 
                    - lib_data->start 
                    + instr_length(drcontext, instr), tabs, fused);
        }

        if(instr_is_call(instr)) {
            apc = opnd_get_pc(instr_get_src(instr, 0));
            if(std::find(app_pc_vect.begin(), app_pc_vect.end(), apc) 
                == app_pc_vect.end()){
                app_pc_vect.push_back(apc);
                //if(isDebug()){
                    print_tabs(tabs);
                    dr_printf("on ajoute l'app_pc = %p au vecteur\n\n\n", apc);
                //}
                
                show_instr_of_symbols(drcontext, lib_data, 
                    apc - lib_data->start, tabs + 1, fused);
            }else{
                //if(isDebug()){
                    print_tabs(tabs);
                    dr_printf("l'app_pc = %p a déjà été vu\n\n\n", apc);
                //}
            }

            show_instr_of_symbols(drcontext, lib_data, 
                instr_get_app_pc(instr) 
                    - lib_data->start 
                    + instr_length(drcontext, instr), tabs, fused);
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

static bool write_reg_to_file(const char* path){
    std::ofstream analyse_file;
    analyse_file.open(path);
    if(analyse_file.fail()){
        dr_fprintf(STDERR, "FAILED TO OPEN THE GIVEN FILE FOR WRITING : \"%s\"\n", 
            path);
        return true;
    }
    write_vect(analyse_file, gpr_reg, "gpr_reg");
    write_vect(analyse_file, float_reg, "float_reg");
    write_vect(analyse_file, fused_gpr_reg, "fused_gpr_reg");
    write_vect(analyse_file, fused_float_reg, "fused_float_reg");
    analyse_file.flush();
    analyse_file.close();
    return true;;
}

static bool is_number(const std::string& s){
    return !s.empty() && std::find_if(s.begin(), 
        s.end(), [](char c) { return !std::isdigit(c); }) == s.end();
}

static bool read_reg_from_file(const char* path){
    std::ifstream analyse_file;
    std::string buffer;
    int line_number = 0, vector_number = 0;

    analyse_file.open(path);
    if(analyse_file.fail()){
        dr_fprintf(STDERR, "FAILED TO OPEN THE GIVEN FILE FOR READING : \"%s\"\n", 
            path);
        return true;
    }
    while(std::getline(analyse_file, buffer)){
        std::cout << buffer << '\n';
        if(buffer == "gpr_reg"){
            vector_number = 1;
        }else if(buffer == "float_reg"){
            vector_number = 2;
        }else if(buffer == "fused_gpr_reg"){
            vector_number = 3;
        }else if(buffer == "fused_float_reg"){
            vector_number = 4;
        }else if(is_number(buffer)){
            switch(vector_number){
                case 1:
                    gpr_reg.push_back((reg_id_t)std::stoi(buffer));
                    break;
                case 2:
                    float_reg.push_back((reg_id_t)std::stoi(buffer));
                    break;
                case 3:
                    fused_gpr_reg.push_back((reg_id_t)std::stoi(buffer));
                    break;
                case 4:
                    fused_float_reg.push_back((reg_id_t)std::stoi(buffer));
                    break;
                default:
                    return true;
            }
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

static bool enum_symbols(const char *name, size_t modoffs, void *data){
    void *drcontext = nullptr;
    module_data_t* lib_data = nullptr;

    std::string str(name);
    if(str.find("backend<>::apply") != std::string::npos) {
        drcontext = dr_get_current_drcontext();
        lib_data = dr_lookup_module_by_name("libinterflop.so");
        show_instr_of_symbols(drcontext, lib_data, modoffs, 0, false);
        dr_free_module_data(lib_data);
    }

    if(str.find("backend_fused<>::apply") != std::string::npos) {
        drcontext = dr_get_current_drcontext();
        lib_data = dr_lookup_module_by_name("libinterflop.so");
        show_instr_of_symbols(drcontext, lib_data, modoffs, 0, true);
        dr_free_module_data(lib_data);
    }

    return true;
}

static void path_to_library(char* temp, size_t length){
    dr_get_current_directory(temp, length);
    strcat(temp, "/api/bin/libinterflop.so");
}

bool analyse_config_from_args(int argc, const char* argv[])
{
    char temp[256];
    path_to_library(temp, 256);
    for(int i=1; i<argc; i++){
        std::string arg(argv[i]);
        if(arg == "--analyse_abort" || arg == "-aa"){
            if(drsym_enumerate_symbols(temp, enum_symbols, 
                NULL, DRSYM_DEFAULT_FLAGS) != DRSYM_SUCCESS){
                return true;
            }
            return write_reg_to_file(argv[++i]);
        }else if(arg == "--analyse_from_file" || arg == "-af"){
            return read_reg_from_file(argv[++i]);
        }
    }
    if(drsym_enumerate_symbols(temp, enum_symbols, NULL, DRSYM_DEFAULT_FLAGS)
        != DRSYM_SUCCESS){
        return true;
    }
    return false;
}