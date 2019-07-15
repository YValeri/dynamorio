/*
* Created on 2019-07-11
*/

#ifndef SYMBOL_CONFIG_HEADER
#define SYMBOL_CONFIG_HEADER

#ifndef SYMBOL_NAME_MAX_SIZE
#define SYMBOL_NAME_MAX_SIZE 1024
#endif

typedef enum{
    IFP_CLIENT_NOLOOKUP         =0, //Don't look at symbols
    IFP_CLIENT_GENERATE         =1, //Generate the symbols from an execution
    IFP_CLIENT_BL_ONLY          =2, //Don't instrument the symbols in the blacklist
    IFP_CLIENT_WL_ONLY          =4, //Instrument only the symbols in the whitelist
    IFP_CLIENT_BL_WL            =6, //Instrument the symbols in the whitelist that aren't in the blacklist
    IFP_CLIENT_HELP             =-1, //Display arguments help
    IFP_CLIENT_DEFAULT=IFP_CLIENT_NOLOOKUP
}interflop_client_mode_t;

void print_help();

interflop_client_mode_t get_client_mode();
void set_client_mode(interflop_client_mode_t mode);

void symbol_lookup_config_from_args(int argc, const char* argv[]);

bool needsToinstrument(instrlist_t* ilist);

void logSymbol(instrlist_t* ilist);
#endif

