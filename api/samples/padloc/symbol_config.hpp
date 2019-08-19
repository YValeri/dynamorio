#ifndef SYMBOL_CONFIG_HEADER
#define SYMBOL_CONFIG_HEADER

/**
 * \file symbol_config.hpp
 * \brief Symbol instrumentation header. Part of the PADLOC project.
 * 
 * \author Brasseur Dylan, Teaudors Mickaël, Valeri Yoann
 * \date 2019
 * \copyright Interflop 
 */
#include <string>
#include <vector>

/**
 * \enum plc_lookup_type_t
 * \brief Specifies the type of lookup we are requesting
 */
typedef enum{
    /** Lookup the module */
    PLC_LOOKUP_MODULE = true,
    /** Lookup the symbol */
    PLC_LOOKUP_SYMBOL = false
} plc_lookup_type_t;

/**
 * \struct module_entry
 * \brief Structure holding informations on a module, 
 * used for the whitelist and blacklist before further processing
 * 
 * TODO virer _module_entry
 */
typedef struct _module_entry{
    inline _module_entry(const std::string &mod_name, const bool all) : module_name(mod_name), all_symbols(all){
    }

    inline bool operator==(struct _module_entry o){
        return o.module_name == module_name;
    };

    /** Prefered name of the module */
    std::string module_name;
    /** True if we are considering the whole module */
    bool all_symbols;
    /** List of the symbols of interest in this module */
    std::vector<std::string> symbols;
} module_entry;

/**
 * \struct addr_range_t
 * \brief Structure of a range of app_pc
 * 
 * TODO virer _addr_range_t
 */
typedef struct _addr_range_t{
    inline _addr_range_t() : start(0), end(0){
    }

    inline _addr_range_t(app_pc _start, app_pc _end) : start(_start), end(_end){
    }

    /**
     * @brief [brief description]
     * @details [long description]
     * 
     * @param pc [description]
     * @return Returns true if the given app_pc is in the range
     */
    inline bool contains(app_pc pc) const{
        return pc >= start && pc < end;
    }

    /** Start of the range */
    app_pc start;
    /**End of the range */
    app_pc end;
} addr_range_t;

/**
 * \struct symbol_entry_t
 * \brief Structure holding a symbol
 * 
 * TODO virer _symbol_entry_t
 */
typedef struct _symbol_entry_t{
    inline bool contains(app_pc pc) const{
        return range.contains(pc);
    }

    /** Name of the symbol */
    std::string name;
    /** Range of app_pc of this symbol*/
    addr_range_t range;
} symbol_entry_t;

/**
 * \struct lookup_entry_t
 * \brief Structure holding a module and its symbols for lookup purposes
 * 
 * TODO virer _lookup_entry_t
 */
typedef struct _lookup_entry_t{
    /**
     * \brief Returns true if the module contains the given address
     *
     * \param pc Address of the instruction
     * \param lookup Lookup type, if we are looking for the adress in the whole module, or in a symbol
     * \return true If the adress is contained within the module
     */
    inline bool contains(app_pc pc, plc_lookup_type_t lookup) const{
        if(lookup == PLC_LOOKUP_MODULE || (total && symbols.empty())){
#ifdef WINDOWS
            return range.contains(pc);
#else
            //Either we are looking for the whole module, or for a symbol in a total module without symbols as exceptions
            if(contiguous){
                return range.contains(pc);
            }else{
                //The module isn't contiguous, we need to check each segment
                size_t segsize = segments.size();
                for(size_t i = 0; i < segsize; i++){
                    if(segments[i].contains(pc)){
                        return true;
                    }
                }

                return false;
            }
#endif
        }else{
            //We are looking at a symbol
            if(!range.contains(pc)){
                //If the address isn't even in the full range of the module, it won't be found
                return false;
            }else{
                //Note : checking for each range of segments would be possible, but performance-wise wouldn't be much better than looking for symbols
                size_t symsize = symbols.size();
                for(size_t i = 0; i < symsize; i++){
                    if(symbols[i].contains(pc)){
                        return !total;
                    }
                } //If it was "total", then the symbols would be exceptions

                return total; //In total state with symbols, not finding the right symbol means it's not an exception
            }
        }
    }

    /** Name of the module */
    std::string name;
    /** Full range of the module 
     * \note If the module isn't contiguous, an app_pc in this range may not be in the module
     */
    addr_range_t range;
    /** True if the module should be considered in its entirety */
    bool total;
    /** Symbols of interest of the module */
    std::vector<symbol_entry_t> symbols;
#ifndef WINDOWS
    /** True if the module is contiguous (always true on Windows, most of the time on Linux, sometimes in MacOS) */
    bool contiguous;
    /** Ranges of app_pc for each segment of the module */
    std::vector<addr_range_t> segments; 
#endif //WINDOWS
} lookup_entry_t;

/**
 * \brief TODO
 * \details [long description]
 */
void print_lookup();

/**
 * \brief Writes the symbols held by the modules vector to the output file
 * (defined by the command line argument)
 * \details TODO
 */
void write_symbols_to_file();

/**
 * \brief Logs the symbol associated with ilist to the modules vector
 * \details TODO
 * 
 * \param ilist TODO
 */
void log_symbol(instrlist_t *ilist);

/**
 * \brief Returns true if the module passed by \p module should be instrumented
 */
/**
 * \brief Verify that we want to instrument a particular module
 * \details This function verifies, based on the current state of the client
 * (changed by the command line option), if we want to instrument a module
 * or not.
 * 
 * \param module The module we may want to instrument
 * \return True if the module should be instrumented, else false
 */
bool should_instrument_module(const module_data_t *module);

/**
 * \brief Verify if we want to instrument the list of instructions ilist
 * \details TODO
 * 
 * \param ilist The list of instructions we may want to instrument
 * \return True if ilist has to be instrumented, else false
 */
bool needs_to_instrument(instrlist_t *ilist);

/**
 * \brief Symbol analyser plugin's parser
 * \details Parser for the symbol analyser functionnalities. The possible 
 * options are currently :
 *      - no lookup, with "--no-lookup" or "-n", which make it so that
 *      no symbol are instrumented;
 *      - blacklist, with "--blacklist" or "-b", which make it so that
 *      the symbols specified by the following file are not instrumented;
 *      - whitelist, with "--whitelist" or "-w", which make it so that
 *      the symbols specified by the following file are instrumented;
 *      - generate, with "--generate" or "-g", which instruments all symbols
 *      and put all the names and modules in the given file.
 * 
 * \param arg The current argument as string
 * \param i The index of the current argument, given as pointer to be modified
 * if necessary when checking for an option with special parameters
 * \param argc The length of the command line
 * \param argv The list of arguments in the command line
 * \return True if the execution of the program must be stopped, else false
 */
bool symbol_argument_parser(std::string arg, int *i, int argc, const char *argv[]);

/**
 * \brief The manager for the symbol plugin
 * \details After parsing each argument, the program is in a particular
 * symbol mode, thus different actions must be done for each mode (i.e, 
 * open the files if necessary, ...).
 */
void symbol_client_mode_manager();

#endif //SYMBOL_CONFIG_HEADER