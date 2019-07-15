#include "dr_api.h"
#include "dr_defines.h"
#include "drsyms.h"
#include "symbol_config.hpp"
#include "ifp_string_constants.hpp"
#include <string>
#include <vector>
#include <map>
#include <fstream>

static interflop_client_mode_t interflop_client_mode;

void print_help()
{
    dr_printf(IFP_HELP_STRING);
}

interflop_client_mode_t get_client_mode();

void set_client_mode(interflop_client_mode_t mode);

typedef struct _module_entry
{
    inline _module_entry (const std::string & mod_name, const bool all) : module_name(mod_name), all_symbols(all){}
    bool operator==(struct _module_entry o){return o.module_name == module_name;};
    std::string module_name;
    bool all_symbols;
    std::vector<std::string> symbols;
} module_entry;

/*Note:
TODO : the symbols_vector can contain symbols with a wildcard
whitelist only : lookup_vector contains the modules and symbols instrumented, 
    all_symbols=true means whole module, 
    all_symbols=false means only the symbols in this module in the symbols vector
blacklist only : lookup_vector contains the modules and symbols we shouldn't instrument,
    all_symbols=true means whole module
    all_symbols=false means only the symbols in this module in the symbols vector
blacklist_whitelist : lookup_vector contains the modules and symbols we should instrument
    all_symbols=true means instrument the whole module EXCEPT the symbols in the symbols vector
    all_symbols=false means instrument ONLY the symbols in the symbols vector
*/

static std::vector<module_entry> lookup_vector;
static bool whitelist_parsed=false;

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



/*
//CURRENTLY UNUSED
#ifndef MAX_SYMBOL_NAME_LENGTH
#define MAX_SYMBOL_NAME_LENGTH 1024
#endif

*/

static std::string getSymbolName(module_data_t* module, app_pc intr_pc)
{
    std::string name;
    if(module)
    {
        if(drsym_module_has_symbols(module->full_path) == DRSYM_SUCCESS)
        {
            drsym_info_t sym;
            sym.file=NULL;
            sym.name=NULL;
            sym.struct_size = sizeof(sym);
            drsym_error_t sym_error = drsym_lookup_address(module->full_path, intr_pc-module->start, &sym, DRSYM_DEFAULT_FLAGS);
            if(sym_error == DRSYM_SUCCESS || sym_error == DRSYM_ERROR_LINE_NOT_AVAILABLE)
            {
                dr_printf("%u\n", sym.name_available_size);
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

bool needsToInstrument(instrlist_t* ilist)
{
    if(ilist != nullptr)
    {
        if(interflop_client_mode == IFP_CLIENT_NOLOOKUP)
        {
            return true;
        }
        if(interflop_client_mode == IFP_CLIENT_GENERATE || interflop_client_mode == IFP_CLIENT_HELP)
        {
            return false;
        }
        instr_t * instr = instrlist_first_app(ilist);
        if(instr)
        {
            app_pc pc = instr_get_app_pc(instr);
            if(pc)
            {
                module_data_t* mod = dr_lookup_module(pc);
                if(mod)
                {
                    module_entry entry(std::string(dr_module_preferred_name(mod)), false);
                    size_t pos = vec_idx_of(lookup_vector, entry);
                    if(pos == lookup_vector.size()) 
                    {
                        //Didn't find the module
                        return interflop_client_mode != IFP_CLIENT_WL_ONLY || interflop_client_mode != IFP_CLIENT_BL_WL; //Instrument if the list isn't a whitelist
                    }else
                    {
                        //Found the module
                        if(lookup_vector[pos].all_symbols)
                        {
                            //Whole module
                            switch(interflop_client_mode)
                            {
                                case IFP_CLIENT_BL_WL:
                                if(lookup_vector[pos].symbols.empty()) //No exceptions to the whitelist
                                {
                                    return true;
                                }else
                                {
                                    std::string name = getSymbolName(mod, pc);
                                    if(!name.empty())
                                    {
                                        size_t sympos = vec_idx_of(lookup_vector[pos].symbols, name);
                                        return sympos == lookup_vector[pos].symbols.size(); //Instrument if the symbol isn't in the list
                                    }
                                }
                                break;
                                case IFP_CLIENT_WL_ONLY:
                                return true;
                                break;
                                default:
                                return false;
                                
                            }
                        }else
                        {
                            //Only some symbols
                            std::string name = getSymbolName(mod, pc);
                            if(!name.empty())
                            {
                                size_t sympos = vec_idx_of(lookup_vector[pos].symbols, name);
                                if(sympos == lookup_vector[pos].symbols.size())
                                {
                                    //Didn't find symbol
                                    return interflop_client_mode == IFP_CLIENT_BL_ONLY;
                                }else
                                {
                                    //Found symbol
                                    return interflop_client_mode == IFP_CLIENT_WL_ONLY || interflop_client_mode == IFP_CLIENT_BL_WL;
                                }
                            }
                            
                        }
                    }    
                }
            }
        }
    }else
    {
        //ilist == nullptr
        return false;
    }
    //If we don't find the module and/or symbol for any reason we consider it not in the list
    return interflop_client_mode == IFP_CLIENT_NOLOOKUP || interflop_client_mode == IFP_CLIENT_BL_ONLY; 
    
}

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
        std::string module="", symbol="";
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
        bool exists = (pos = vec_idx_of<module_entry>(lookup_vector, entry))!=lookup_vector.size();
        if(parsing_whitelist)
        {
            //Whitelist
            if(exists)
            {
                if(whole)
                {
                    lookup_vector[pos].symbols.clear();
                    lookup_vector[pos].all_symbols=true;
                }else
                {
                    if(!lookup_vector[pos].all_symbols && vec_idx_of<std::string>(lookup_vector[pos].symbols, symbol) == lookup_vector[pos].symbols.size())
                    {
                        lookup_vector[pos].symbols.push_back(symbol);
                    }
                }
            }else
            {
                lookup_vector.push_back(entry);
                if(!whole)
                {
                    lookup_vector[pos].symbols.push_back(symbol);
                }
            }
        }else
        {
            //Blacklist
            bool bl_only = interflop_client_mode == IFP_CLIENT_BL_ONLY;
            if(exists)
            {
                if(whole)
                {
                    if(bl_only)
                    {
                        lookup_vector[pos].symbols.clear();
                        lookup_vector[pos].all_symbols=true;
                    }else
                    {
                        lookup_vector.erase(lookup_vector.begin()+pos);
                    }
                }else
                {
                    if(bl_only)
                    {
                        lookup_vector[pos].symbols.push_back(symbol);
                    }else
                    {
                        if(lookup_vector[pos].all_symbols)
                        {
                            lookup_vector[pos].symbols.push_back(symbol);
                        }else 
                        {
                            size_t sympos= vec_idx_of<std::string>(lookup_vector[pos].symbols, symbol);
                            if(sympos != lookup_vector[pos].symbols.size())
                            {
                                lookup_vector[pos].symbols.erase(lookup_vector[pos].symbols.begin() + sympos);
                            }
                        }
                        
                    }
                }
            }else
            {
                if(whole)
                {
                    if(bl_only)
                    {
                        lookup_vector.push_back(entry);
                    }
                }else
                {
                    if(bl_only)
                    {
                        entry.symbols.push_back(symbol);
                        lookup_vector.push_back(entry);
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
}

static void generate_whitelist_from_file(std::ifstream& whitelist)
{
    std::ifstream bl;
    generate_whitelist_from_files(whitelist, bl);
}


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
        }else if(arg == "--whitelist" || "-w") //Whitelist
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
            
        }else if(arg == "--blacklist" || "-b") //Whitelist
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
            break;
        }else if(arg == "--help" || arg == "-h")
        {
            interflop_client_mode = IFP_CLIENT_HELP;
            break;
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
        break;

        case IFP_CLIENT_WL_ONLY:
        whitelist.open(whitelist_filename);
        DR_ASSERT_MSG(!whitelist.fail(), "Can't open whitelist file");

        generate_whitelist_from_file(whitelist);
        break;

        case IFP_CLIENT_BL_WL:
        whitelist.open(whitelist_filename);
        DR_ASSERT_MSG(!whitelist.fail(), "Can't open whitelist file");
        blacklist.open(blacklist_filename);
        DR_ASSERT_MSG(!blacklist.fail(), "Can't open blacklist file");

        generate_whitelist_from_files(whitelist, blacklist);
        break;

        default:
        break;
    }

    if(blacklist.is_open()) blacklist.close();
    if(whitelist.is_open()) whitelist.close();
}

void logSymbol(instrlist_t* ilist)
{
    static size_t oldModule = 0;
    static size_t oldPos = 0;
    instr_t * instr = instrlist_first_app(ilist);
    app_pc pc;
    module_data_t* mod;
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
                    if(lookup_vector.size() > oldModule)
                    {
                        
                        if(lookup_vector[oldModule] == entry)
                        {
                            if(lookup_vector[oldModule].symbols.size() <= oldPos 
                            || ((lookup_vector[oldModule].symbols[oldPos] != symbolName) 
                            && vec_idx_of(lookup_vector[oldModule].symbols, symbolName) == lookup_vector[oldModule].symbols.size()))
                            {
                                lookup_vector[oldModule].symbols.push_back(symbolName);
                                oldPos=lookup_vector[oldModule].symbols.size()-1;
                            }
                            
                        }else
                        {
                            size_t pos = vec_idx_of(lookup_vector, entry);
                            if(pos == lookup_vector.size())
                            {
                                lookup_vector.push_back(entry);
                                oldModule = pos;
                            }
                            if(vec_idx_of(lookup_vector[pos].symbols, symbolName) == lookup_vector[pos].symbols.size())
                            {
                                oldPos=lookup_vector[oldModule].symbols.size();
                                lookup_vector[pos].symbols.push_back(symbolName);
                            }
                            
                        }
                    }else
                    {
                        size_t pos = vec_idx_of(lookup_vector, entry);
                        if(pos == lookup_vector.size())
                        {
                            lookup_vector.push_back(entry);
                            oldModule = pos;
                        }
                        if(vec_idx_of(lookup_vector[pos].symbols, symbolName) == lookup_vector[pos].symbols.size())
                        {
                            oldPos=lookup_vector[oldModule].symbols.size();
                            lookup_vector[pos].symbols.push_back(symbolName);
                        }
                        
                    }
                }
            }
        }
    }
}