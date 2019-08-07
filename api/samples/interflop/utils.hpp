#ifndef UTILS_BARRIER_HEADER
#define UTILS_BARRIER_HEADER

#include <fstream>

/** Specifies the behavior of the client */
typedef enum{
	IFP_CLIENT_DEFAULT = 0, /** Default */
	IFP_CLIENT_NOLOOKUP = 0, /** Don't look at symbols */
	IFP_CLIENT_GENERATE = 1, /** Generate the symbols from an execution */
	IFP_CLIENT_BL_ONLY = 2, /** Don't instrument the symbols in the blacklist */
	IFP_CLIENT_WL_ONLY = 4, /** Instrument only the symbols in the whitelist */
	IFP_CLIENT_BL_WL = 6, /** Instrument the symbols in the whitelist that aren't in the blacklist */
	IFP_CLIENT_HELP = -1 /** Display arguments help */
} interflop_client_mode_t;

typedef enum{
	IFP_ANALYSE_NOT_NEEDED = 0, /* Backend analysis not needed */
	IFP_ANALYSE_NEEDED = 1 /* Backend analysis needed */
} interflop_analyse_mode_t;

#define UNKNOWN_ARGUMENT 3

void set_log_level(int);

int get_log_level();

/**
 * \brief Prints the help of the client
 * 
 */
void print_help();

void write_to_file_symbol_file_header(std::ofstream&);

/**
 * \brief Get the current client mode
 * 
 * \return interflop_client_mode_t Client mode
 */
interflop_client_mode_t get_client_mode();

/**
 * \brief Sets the client mode
 * 
 * \param mode 
 */
void set_client_mode(interflop_client_mode_t);

/**
 * \brief Get the current analyse mode
 * 
 * \return interflop_analyse_mode_t Analyse mode
 */
interflop_analyse_mode_t get_analyse_mode();

/**
 * \brief Sets the analyse mode
 * 
 * \param mode 
 */
void set_analyse_mode(interflop_analyse_mode_t);

void inc_error();

bool is_number(const std::string&);

bool arguments_parser(int, const char**);

#endif