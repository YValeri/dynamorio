#include "dr_api.h"
#include "dr_defines.h"
#include "drsyms.h"
#include "symbol_config.hpp"
#include "ifp_string_constants.hpp"
#include <fstream>
#include <algorithm>

static void lookup_or_load_module(const module_data_t* module);
static lookup_entry_t* lookup_find(app_pc pc, ifp_lookup_type_t lookup_type);
static std::string getSymbolName(module_data_t* module, app_pc intr_pc);
static void parseLine(std::string line, bool parsing_whitelist);
static void load_lookup_from_modules_vector();

//Current functionning mode of the client, defines the behavior
static interflop_client_mode_t interflop_client_mode;

static std::ofstream symbol_file;

/*Note:
TODO : the modules_vector can contain symbols with a wildcard
whitelist only : modules_vector contains the modules and symbols instrumented, 
    all_symbols=true means whole module, 
    all_symbols=false means only the symbols in this module in the symbols vector
blacklist only : modules_vector contains the modules and symbols we shouldn't instrument,
    all_symbols=true means whole module
    all_symbols=false means only the symbols in this module in the symbols vector
blacklist_whitelist : modules_vector contains the modules and symbols we should instrument
    all_symbols=true means instrument the whole module EXCEPT the symbols in the symbols vector
    all_symbols=false means instrument ONLY the symbols in the symbols vector
*/
static std::vector<module_entry> modules_vector;
static std::vector<lookup_entry_t> lookup_vector;

static bool whitelist_parsed=false;

void print_lookup()
{
    for(size_t i=0; i<lookup_vector.size(); i++)
    {
        dr_printf((lookup_vector[i].name+"\nTotal : %d\n").c_str(), lookup_vector[i].total);
        
        for(size_t j=0; j<lookup_vector[i].symbols.size(); j++)
        {
            dr_printf((lookup_vector[i].symbols[j].name+"\n").c_str());
        }
    }
}



/* ### UTILITIES ### */

inline void print_help()
{
    dr_printf(IFP_HELP_STRING);
}

interflop_client_mode_t get_client_mode()
{
    return interflop_client_mode;
}

void set_client_mode(interflop_client_mode_t mode)
{
    interflop_client_mode = mode;
}

/**
 * \brief Returns the index of \elem, \p vec .size() if it's not found
 */
template<typename T>
static inline size_t vec_idx_of(std::vector<T, std::allocator<T>> vec, T elem)
{
    size_t size = vec.size();
    for(size_t i=0; i<size; ++i)
    {
        if(elem == vec[i])
            return i;
    }
    return size;
}

/**
 * \brief Returns true if we need to clear the entry
 * Clears the symbols if it's only what we need to clear
 */
static bool need_cleanup_module(module_entry & entry)
{
    if(entry.all_symbols && interflop_client_mode != IFP_CLIENT_BL_WL)
    {
        entry.symbols.clear();
        return false;
    }
    if(!entry.all_symbols && entry.symbols.empty())
    {
        return true;
    }
    return false;
}

/* ### SYMBOLS FILE GENERATION ### */

void write_symbols_to_file()
{
    if(interflop_client_mode == IFP_CLIENT_GENERATE)
    {
        if(symbol_file.is_open() && symbol_file.good())
        {
            symbol_file << IFP_SYMBOL_FILE_HEADER;
            size_t num_modules = modules_vector.size();
            size_t totalSymbols=0;
            for(size_t i=0; i<num_modules; i++)
            {
                totalSymbols+=modules_vector[i].symbols.size();
            }
            symbol_file << "# This list contains " << totalSymbols << " symbols of interest, split between " << num_modules << " modules\n";
            for(size_t i=0; i<num_modules; i++)
            {
                symbol_file << modules_vector[i].module_name << "\n";
                size_t symbols_size = modules_vector[i].symbols.size();
                for(size_t j=0; j< symbols_size; j++)
                {
                    symbol_file << '\t' << modules_vector[i].module_name << "!" << modules_vector[i].symbols[j] << "\n";
                }
            }
            symbol_file.flush();
            symbol_file.close();
        }
    }
}

/**
 * \brief Return the name of the symbol in \p module associated with the address \p intr_pc
 */
static std::string getSymbolName(module_data_t* module, app_pc intr_pc)
{
    std::string name;
    if(module)
    {
        //If the module doesn't have symbols, we can't know the name associated the adress
        if(drsym_module_has_symbols(module->full_path) == DRSYM_SUCCESS)
        {
            //We ask a first time to get the length of the name
            drsym_info_t sym;
            sym.file=NULL;
            sym.name=NULL;
            sym.struct_size = sizeof(sym);
            drsym_error_t sym_error = drsym_lookup_address(module->full_path, intr_pc-module->start, &sym, DRSYM_DEFAULT_FLAGS);
            if(sym_error == DRSYM_SUCCESS || sym_error == DRSYM_ERROR_LINE_NOT_AVAILABLE)
            {
                //We found the length of the name, now we retrieve it
                char* name_str = (char*)calloc(sym.name_available_size+1, sizeof(char));
                sym.name = name_str;
                sym.name_size = sym.name_available_size+1;
                sym_error = drsym_lookup_address(module->full_path, intr_pc-module->start, &sym, DRSYM_DEFAULT_FLAGS);
                if(sym_error == DRSYM_SUCCESS || sym_error == DRSYM_ERROR_LINE_NOT_AVAILABLE)
                {
                    name = name_str;
                    free(name_str);
                    return name;
                }
                
                free(name_str);
                name = "";
            }
        }
    }
    return name;
}

void logSymbol(instrlist_t* ilist)
{
    //We save the old module and symbol locations, as we can expect a symbol to appear multiple times in a row
    static size_t oldModule = 0;
    static size_t oldPos = 0;
    instr_t * instr = instrlist_first_app(ilist);
    app_pc pc = 0;
    module_data_t* mod = nullptr;
    module_entry entry("", false);
    if(instr)
    {
        pc = instr_get_app_pc(instr);
        if(pc)
        {
            mod = dr_lookup_module(pc);
            if(mod)
            {
                entry.module_name=std::string(dr_module_preferred_name(mod));
                std::string symbolName = getSymbolName(mod, pc);
                if(!symbolName.empty())
                {
                    if(modules_vector.size() > oldModule)
                    {
                        
                        if(modules_vector[oldModule] == entry) // If the module is the last seen module
                        {
                            //If it's not the old symbol, and it can't be found elsewhere, we need to add it
                            if((modules_vector[oldModule].symbols.size() <= oldPos 
                            || (modules_vector[oldModule].symbols[oldPos] != symbolName)) 
                            && vec_idx_of(modules_vector[oldModule].symbols, symbolName) == modules_vector[oldModule].symbols.size())
                            {
                                modules_vector[oldModule].symbols.push_back(symbolName);
                                oldPos=modules_vector[oldModule].symbols.size()-1;
                            }
                            
                        }else
                        {
                            //It's not the last seen module
                            size_t pos = vec_idx_of(modules_vector, entry);
                            if(pos == modules_vector.size())
                            {
                                modules_vector.push_back(entry);
                                oldModule = pos;
                            }
                            if(vec_idx_of(modules_vector[pos].symbols, symbolName) == modules_vector[pos].symbols.size())
                            {
                                oldPos=modules_vector[oldModule].symbols.size();
                                modules_vector[pos].symbols.push_back(symbolName);
                            }
                            
                        }
                    }else
                    {
                        size_t pos = vec_idx_of(modules_vector, entry);
                        if(pos == modules_vector.size())
                        {
                            modules_vector.push_back(entry);
                            oldModule = pos;
                        }
                        if(vec_idx_of(modules_vector[pos].symbols, symbolName) == modules_vector[pos].symbols.size())
                        {
                            oldPos=modules_vector[oldModule].symbols.size();
                            modules_vector[pos].symbols.push_back(symbolName);
                        }
                        
                    }
                }
            }
        }
    }
    if(mod)
    {
        dr_free_module_data(mod);
    }
}

/* ### INTRUMENTATION ### */

/**
 * \brief Loads the module into the lookup if it doesn't exist in it
 */
static void lookup_or_load_module(const module_data_t* module)
{
    //If we find the module in the lookup, we don't need to load it
    if(lookup_find(module->start, IFP_LOOKUP_MODULE))
    {
        return;
    }

    std::string name=dr_module_preferred_name(module);
    module_entry entry(std::string(name), false);
    size_t pos = vec_idx_of(modules_vector, entry);
    //If it's not in the list of modules we care about, it won't be in the lookup
    if(pos != modules_vector.size())
    {
        module_entry mentry = modules_vector[pos];
        lookup_entry_t lentry;
        lentry.name = name;
        lentry.range.start = module->start;
        lentry.range.end = module->end;
        lentry.total = mentry.all_symbols;
        lentry.contiguous = module->contiguous;
        //When a module isn't continuous (sometimes on Linux, often on MacOS), we need to remember the bounds of each segment
        if(!lentry.contiguous)
        {
            for(size_t i=0; i<module->num_segments; i++)
            {
                lentry.segments.emplace_back(module->segments[i].start, module->segments[i].end);
            }
        }
        //When the module isn't total, or there can be exceptions to the total module, we need to register each symbol
        if(!lentry.total || interflop_client_mode == IFP_CLIENT_BL_WL)
        {
            size_t modoff;
            drsym_info_t info;
            info.struct_size = sizeof(info);
            info.file=nullptr;
            info.name=nullptr;

            for(size_t j=0; j<mentry.symbols.size(); j++)
            {
                drsym_error_t err = drsym_lookup_symbol(module->full_path, (name+"!"+mentry.symbols[j]).c_str(), &modoff, DRSYM_DEFAULT_FLAGS);
                if(err == DRSYM_SUCCESS)
                {
                    err = drsym_lookup_address(module->full_path, modoff, &info, DRSYM_DEFAULT_FLAGS);
                    if(err == DRSYM_SUCCESS || err == DRSYM_ERROR_LINE_NOT_AVAILABLE)
                    {
                        symbol_entry_t sentry;
                        sentry.name = mentry.symbols[j];
                        sentry.range.start = module->start+info.start_offs;
                        sentry.range.end = module->start+info.end_offs;
                        lentry.symbols.push_back(sentry);
                    }
                }
            }
            //We free the ressources of symbol information for this module, we won't need them anymore
            drsym_free_resources(module->full_path);
        }
        lookup_vector.push_back(lentry);
    }
}

/**
 * This function takes the modules vector (assuming it's loaded already), and converts it into the lookup vector
 * If the module can't be found by name, it will be loaded in when DynamoRIO loads it in memory
 */
static void load_lookup_from_modules_vector()
{

    auto end = std::remove_if(modules_vector.begin(), modules_vector.end(),[](module_entry const& mentry) {
                                  std::string module_name = mentry.module_name;
                                  module_data_t* mod = dr_lookup_module_by_name(module_name.c_str());
                                  if(mod) {
                                      lookup_or_load_module(mod);
                                      dr_free_module_data(mod);
                                      return true;
                                  }
                                  return false;
                              });

    modules_vector.erase(end, modules_vector.end());
}

bool shouldInstrumentModule(const module_data_t* module)
{
    lookup_or_load_module(module);
    lookup_entry_t* found_module = lookup_find(module->start, IFP_LOOKUP_MODULE);
    if(found_module != nullptr)
    {
        //We found the module in the list, we need to instrument it if it's not blacklisted totally
        return (interflop_client_mode == IFP_CLIENT_BL_ONLY && !found_module->total) ||
        interflop_client_mode == IFP_CLIENT_WL_ONLY || interflop_client_mode == IFP_CLIENT_BL_WL;
    }else
    {
        //We didn't find the module in the lookup, we instrument it if we're not white listing
        return interflop_client_mode == IFP_CLIENT_BL_ONLY || interflop_client_mode == IFP_CLIENT_NOLOOKUP;
    }
}

bool needsToInstrument(instrlist_t* ilist)
{
    if(ilist != nullptr)
    {
        if(interflop_client_mode == IFP_CLIENT_NOLOOKUP)
        {
            //We always instrument in NOLOOKUP
            return true;
        }
        if(interflop_client_mode == IFP_CLIENT_GENERATE || interflop_client_mode == IFP_CLIENT_HELP)
        {
            //We never instrument in GENERATE or HELP (nor that this check is strictly necessary)
            return false;
        }
        instr_t * instr = instrlist_first_app(ilist);
        if(instr)
        {
            app_pc pc = instr_get_app_pc(instr);
            if(pc)
            {
                //We instrument if we found the symbol in the lookup for whitelists, otherwise always for blacklists
                return lookup_find(pc, IFP_LOOKUP_SYMBOL) != nullptr || interflop_client_mode == IFP_CLIENT_BL_ONLY;
            }
        }
    }else
    {
        //ilist == nullptr
        return false;
    }
    //Something went wrong, so we assume we didn't find the symbol
    return interflop_client_mode == IFP_CLIENT_NOLOOKUP || interflop_client_mode == IFP_CLIENT_BL_ONLY; 
    
}

/* ### FILE PARSING ### */

static void parseLine(std::string line, bool parsing_whitelist)
{
    DR_ASSERT_MSG(parsing_whitelist || (interflop_client_mode==IFP_CLIENT_BL_ONLY || whitelist_parsed), "Wrong parsing order for blacklist/whitelist");
    //Remove comments
    std::size_t pos = line.find('#');
    if(pos != std::string::npos)
    {
        line = line.substr(0, pos);
    }
    //left Trimming
    pos = line.find_first_not_of(" \n\r\t\f\v");
    if(pos != std::string::npos)
    {
        line = line.substr(pos);
    }
    //Right trimming
    pos = line.find_last_not_of(" \n\r\t\f\v");
    if(pos != std::string::npos)
    {
        line = line.substr(0, pos+1);
    }
    if(!line.empty())
    {
        std::string module, symbol;
        //Behold the if statements
        //Checks if it's only the module name or the symbol as well
        pos = line.find('!');
        bool whole=pos == std::string::npos;
        if(whole)
        {
            //Whole module
            module = line;
        }else
        {
            //Symbol
            module = line.substr(0, pos);
            symbol = line.substr(pos+1);
        }
        module_entry entry(module, whole);
        bool exists = (pos = vec_idx_of<module_entry>(modules_vector, entry))!=modules_vector.size();
        if(parsing_whitelist || interflop_client_mode == IFP_CLIENT_BL_ONLY)
        {
            //If we're parsing the whitelist, or the blacklist in blacklist only mode, it's only about adding to the lookup
            if(exists)
            {
                if(whole)
                {
                    //Whole module, we clear the symbols
                    modules_vector[pos].symbols.clear();
                    modules_vector[pos].all_symbols=true;
                }else
                {
                    //Add only if the symbol doesn't exist yet
                    if(!modules_vector[pos].all_symbols && vec_idx_of<std::string>(modules_vector[pos].symbols, symbol) == modules_vector[pos].symbols.size())
                    {
                        modules_vector[pos].symbols.push_back(symbol);
                    }
                }
            }else
            {
                //It doesn't exist yet, we need to push it
                modules_vector.push_back(entry);
                if(!whole)
                {
                    modules_vector[pos].symbols.push_back(symbol);
                }
            }
        }else
        {
            //Parsing blacklist while in "whitelist and blacklist" mode
            if(exists)
            {
                if(whole)
                {
                    modules_vector.erase(modules_vector.begin()+pos);
                }else
                {
                    if(modules_vector[pos].all_symbols)
                    {
                        modules_vector[pos].symbols.push_back(symbol);
                    }else 
                    {
                        size_t sympos= vec_idx_of<std::string>(modules_vector[pos].symbols, symbol);
                        if(sympos != modules_vector[pos].symbols.size())
                        {
                            modules_vector[pos].symbols.erase(modules_vector[pos].symbols.begin() + sympos);
                        }
                    }
                }
                
            }
        }
    }
}

static void generate_blacklist_from_file(std::ifstream& blacklist)
{
    std::string buffer;
    if(blacklist.is_open())
    {
        while(std::getline(blacklist, buffer))
        {
            parseLine(buffer, false);
        }
        DR_ASSERT_MSG(!blacklist.bad(), "Error while reading blacklist\n");
    }else
    {
        DR_ASSERT_MSG(false, "Error while opening blacklist\n");
    }
    auto end = std::remove_if(modules_vector.begin(), modules_vector.end(), [](module_entry & entry){
        return need_cleanup_module(entry);
    });
    modules_vector.erase(end, modules_vector.end());
}

static void generate_whitelist_from_files(std::ifstream& whitelist, std::ifstream& blacklist)
{
    std::string buffer;
    if(whitelist.is_open() && !whitelist.bad() && (!blacklist.is_open() || !blacklist.bad()))
    {
        whitelist.clear();
        whitelist.seekg(std::ios::beg);
        
        while(std::getline(whitelist, buffer))
        {
            parseLine(buffer, true);
        }
        whitelist_parsed=true;
        DR_ASSERT_MSG(!whitelist.bad(), "Error while reading blacklist\n");

        if(blacklist.is_open() && !blacklist.bad())
        {
            blacklist.clear();
            blacklist.seekg(std::ios::beg);
            while(std::getline(blacklist, buffer))
            {
                parseLine(buffer, false);
            }
            DR_ASSERT_MSG(!blacklist.bad(), "Error while reading blacklist\n");
        }
    }
    auto end = std::remove_if(modules_vector.begin(), modules_vector.end(), [](module_entry & entry){
        return need_cleanup_module(entry);
    });
    modules_vector.erase(end, modules_vector.end());
}

static void generate_whitelist_from_file(std::ifstream& whitelist)
{
    std::ifstream bl;
    generate_whitelist_from_files(whitelist, bl);
}

static lookup_entry_t* lookup_find(app_pc pc, ifp_lookup_type_t lookup_type)
{
    for(auto & i : lookup_vector)
    {
        if(i.contains(pc, lookup_type))
        {
            return &i;
        }
    }
    return nullptr;
}

/* ### ARGUMENTS PARSING ### */

void symbol_lookup_config_from_args(int argc, const char* argv[])
{
    interflop_client_mode = IFP_CLIENT_DEFAULT;
    std::string blacklist_filename, whitelist_filename;
    for(int i=1; i<argc; i++)
    {
        std::string arg(argv[i]);
        if(arg == "--no-lookup" || arg == "-n") //No lookup takes precedence over all other arguments
        {
            interflop_client_mode = IFP_CLIENT_NOLOOKUP;
            break;
        }else if(arg == "--whitelist" || arg == "-w") //Whitelist
        {
            if(interflop_client_mode == IFP_CLIENT_BL_ONLY || interflop_client_mode == IFP_CLIENT_BL_WL) //If we have a blacklist
            {
                interflop_client_mode=IFP_CLIENT_BL_WL;
            }else
            {
                interflop_client_mode=IFP_CLIENT_WL_ONLY;
            }
            
            if(++i < argc) //If the filename is precised
            {
                whitelist_filename=argv[i];
            }else
            {
                dr_fprintf(STDERR, "Not enough arguments\n");
                interflop_client_mode = IFP_CLIENT_HELP;
                break;
            }
            
        }else if(arg == "--blacklist" || arg == "-b") //Whitelist
        {
            if(interflop_client_mode == IFP_CLIENT_WL_ONLY || interflop_client_mode == IFP_CLIENT_BL_WL) //If we have a whitelist
            {
                interflop_client_mode=IFP_CLIENT_BL_WL;
            }else
            {
                interflop_client_mode=IFP_CLIENT_BL_ONLY;
            }
            
            if(++i < argc) //If the filename is precised
            {
                blacklist_filename=argv[i];
            }else
            {
                dr_fprintf(STDERR, "Not enough arguments\n");
                interflop_client_mode = IFP_CLIENT_HELP;
                break;
            }
            
        }else if(arg == "--generate" || arg == "-g")
        {
            interflop_client_mode = IFP_CLIENT_GENERATE;
            if(++i < argc) //If the filename is precised
            {
                symbol_file.open(argv[i]);
                if(symbol_file.fail())
                {
                    dr_fprintf(STDERR, "Can't open the generated file\n");
                    interflop_client_mode = IFP_CLIENT_HELP;
                    break;
                }
            }else
            {
                dr_fprintf(STDERR, "Not enough arguments\n");
                interflop_client_mode = IFP_CLIENT_HELP;
                break;
            }
            break;
        }else if(arg == "--help" || arg == "-h")
        {
            interflop_client_mode = IFP_CLIENT_HELP;
            break;
        }else
        {
            DR_ASSERT_MSG(false, "Unknown command line option\n");
        }
        
    }

    std::ifstream  blacklist, whitelist;

    switch(interflop_client_mode)
    {
        case IFP_CLIENT_HELP:
        print_help();
        break;

        case IFP_CLIENT_BL_ONLY:
        blacklist.open(blacklist_filename);
        DR_ASSERT_MSG(!blacklist.fail(), "Can't open blacklist file");

        generate_blacklist_from_file(blacklist);
        load_lookup_from_modules_vector();
        break;

        case IFP_CLIENT_WL_ONLY:
        whitelist.open(whitelist_filename);
        DR_ASSERT_MSG(!whitelist.fail(), "Can't open whitelist file");

        generate_whitelist_from_file(whitelist);
        load_lookup_from_modules_vector();
        break;

        case IFP_CLIENT_BL_WL:
        whitelist.open(whitelist_filename);
        DR_ASSERT_MSG(!whitelist.fail(), "Can't open whitelist file");
        blacklist.open(blacklist_filename);
        DR_ASSERT_MSG(!blacklist.fail(), "Can't open blacklist file");

        generate_whitelist_from_files(whitelist, blacklist);
        load_lookup_from_modules_vector();
        break;

        default:
        break;
    }

    if(blacklist.is_open()) blacklist.close();
    if(whitelist.is_open()) whitelist.close();
}