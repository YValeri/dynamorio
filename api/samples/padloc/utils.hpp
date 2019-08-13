#ifndef UTILS_BARRIER_HEADER
#define UTILS_BARRIER_HEADER

#include <fstream>

/** Specifies the behavior of the client */
typedef enum{
    PLC_CLIENT_DEFAULT = 0, /** Default */
    PLC_CLIENT_NOLOOKUP = 0, /** Don't look at symbols */
    PLC_CLIENT_GENERATE = 1, /** Generate the symbols from an execution */
    PLC_CLIENT_BL_ONLY = 2, /** Don't instrument the symbols in the blacklist */
    PLC_CLIENT_WL_ONLY = 4, /** Instrument only the symbols in the whitelist */
    PLC_CLIENT_BL_WL = 6, /** Instrument the symbols in the whitelist that aren't in the blacklist */
    PLC_CLIENT_HELP = -1 /** Display arguments help */
} padloc_symbols_mode_t;

typedef enum{
    PLC_ANALYSE_NOT_NEEDED = 0, /* Backend analysis not needed */
    PLC_ANALYSE_NEEDED = 1 /* Backend analysis needed */
} padloc_analyse_mode_t;

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
 * \return padloc_symbols_mode_t Client mode
 */
padloc_symbols_mode_t get_client_mode();

/**
 * \brief Sets the client mode
 * 
 * \param mode 
 */
void set_client_mode(padloc_symbols_mode_t);

/**
 * \brief Get the current analyse mode
 * 
 * \return padloc_analyse_mode_t Analyse mode
 */
padloc_analyse_mode_t get_analyse_mode();

/**
 * \brief Sets the analyse mode
 * 
 * \param mode 
 */
void set_analyse_mode(padloc_analyse_mode_t);

void inc_error();

bool is_number(const std::string&);

bool arguments_parser(int, const char**);

#endif