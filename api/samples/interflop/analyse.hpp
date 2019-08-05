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

void print_register_vectors();

bool analyse_config_from_args(int argc, const char* argv[]);

#endif