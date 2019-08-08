#ifndef ANALYSE_BARRIER_HEADER
#define ANALYSE_BARRIER_HEADER

#include "dr_api.h"
#include "drreg.h"

#include <vector>
#include <string>

std::vector<reg_id_t> get_gpr_reg();
std::vector<reg_id_t> get_float_reg();
std::vector<reg_id_t> get_fused_gpr_reg();
std::vector<reg_id_t> get_fused_float_reg();

std::vector<reg_id_t> get_all_registers();


bool get_need_sse_inverse();
void set_need_sse_inverse(bool new_value);
void print_register_vectors();

bool analyse_argument_parser(std::string, int*, int, const char**);
void analyse_mode_manager();


#endif