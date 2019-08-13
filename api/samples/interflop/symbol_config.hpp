/*
* Created on 2019-07-11
*/

#ifndef SYMBOL_CONFIG_HEADER
#define SYMBOL_CONFIG_HEADER

#include <string>
#include <vector>

/** Specifies the type of lookup we are requesting */
typedef enum {
	IFP_LOOKUP_MODULE = true, /** Lookup the module */
	IFP_LOOKUP_SYMBOL = false /** Lookup the symbol */
} ifp_lookup_type_t;

/** Structure holding informations on a module, used for the whitelist and blacklist before further processing*/
typedef struct _module_entry
{
	inline _module_entry (const std::string & mod_name, const bool all) : module_name(mod_name), all_symbols(all){}
	inline bool operator == (struct _module_entry o){return o.module_name == module_name;};

	std::string module_name; /** Prefered name of the module */
	bool all_symbols;       /** True if we are considering the whole module */
	std::vector<std::string> symbols; /** List of the symbols of interest in this module */
} module_entry;

/** Structure of a range of app_pc*/
typedef struct _addr_range_t
{
	inline _addr_range_t():start(0), end(0){}
	inline _addr_range_t(app_pc _start, app_pc _end): start(_start), end(_end){}
	inline bool contains(app_pc pc)const{return pc >= start && pc<end;} /** Returns true if the given app_pc is in the range*/
	app_pc start; /** Start of the range */
	app_pc end; /**End of the range */
} addr_range_t;

/** Structure holding a symbol */
typedef struct _symbol_entry_t
{
	inline bool contains(app_pc pc)const{return range.contains(pc);}
	std::string name; /** Name of the symbol */
	addr_range_t range; /** Range of app_pc of this symbol*/
} symbol_entry_t;

/** Structure holding a module and its symbols for lookup purposes */
typedef struct _lookup_entry_t
{
	/**
	 * \brief Returns true if the module contains the given address
	 * 
	 * \param pc Address of the instruction
	 * \param lookup Lookup type, if we are looking for the adress in the whole module, or in a symbol
	 * \return true If the adress is contained within the module
	 */
	inline bool contains(app_pc pc, ifp_lookup_type_t lookup)const
	{
		if(lookup == IFP_LOOKUP_MODULE || (total && symbols.empty()))
		{
#ifdef WINDOWS
			return range.contains(pc);
#else
			//Either we are looking for the whole module, or for a symbol in a total module without symbols as exceptions
			if(contiguous)
				return range.contains(pc);
			else
			{
				//The module isn't contiguous, we need to check each segment
				size_t segsize =  segments.size();
				for(size_t i = 0; i<segsize; i++)
					if(segments[i].contains(pc))
						return true;

				return false;
			}
#endif
		}else
		{
			//We are looking at a symbol
			if(!range.contains(pc))
				//If the address isn't even in the full range of the module, it won't be found
				return false;
			else
			{
				//Note : checking for each range of segments would be possible, but performance-wise wouldn't be much better than looking for symbols
				size_t symsize =  symbols.size();
				for(size_t i = 0; i<symsize; i++)
					if(symbols[i].contains(pc))
						return !total; //If it was "total", then the symbols would be exceptions

				return total; //In total state with symbols, not finding the right symbol means it's not an exception
			}
			
		}
		
	}
	std::string name; /** Name of the module */
	addr_range_t range; /** Full range of the module \note If the module isn't contiguous, an app_pc in this range may not be in the module*/
	bool total; /** True if the module should be considered in its entirety*/
	std::vector<symbol_entry_t> symbols; /** Symbols of interest of the module */
#ifndef WINDOWS
	bool contiguous; /** True if the module is contiguous (always true on Windows, most of the time on Linux, sometimes in MacOS)*/
	std::vector<addr_range_t> segments; /** Ranges of app_pc for each segment of the module*/
#endif //WINDOWS
} lookup_entry_t;

/**
 * \brief Sets the client mode and initialises the symbol lookups
 */
void symbol_lookup_config_from_args(int argc, const char* argv[]);

/**
 * \brief Returns true if \p ilist has to be instrumented
 */
bool needs_to_instrument(instrlist_t* ilist);

/**
 * \brief Logs the symbol associated with \p ilist to the modules vector
 */
void log_symbol(instrlist_t* ilist);

/**
 * \brief Writes the symbols held by the modules vector to the output file (defined by the command line argument)
 * 
 */
void write_symbols_to_file();

/**
 * \brief Returns true if the module passed by \p module should be instrumented
 */
bool should_instrument_module(const module_data_t* module);

/**
 * \brief DEBUG_ONLY
 */
void print_lookup();

/**
 * \brief Parses \param arg as a argument for symbol handling
 */
bool symbol_argument_parser(std::string arg, int* i, int argc, const char* argv[]);

/**
 * \brief Handles the symbols loading from files if they were set
 */
void symbol_client_mode_manager();

#endif //SYMBOL_CONFIG_HEADER