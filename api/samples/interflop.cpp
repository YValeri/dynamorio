#include "dr_api.h"
#include "dr_ir_opcodes.h"
#include "dr_ir_opnd.h"
#include "drmgr.h"

//On définit la fonction d'affichage
#ifdef WINDOWS
# define DISPLAY_STRING(msg) dr_messagebox(msg)
#else
# define DISPLAY_STRING(msg) dr_printf("%s\n", msg)
#endif

#ifndef MAX_INSTR_OPND_COUNT
#define MAX_INSTR_OPND_COUNT 4
#endif

#ifndef MAX_OPND_SIZE_BYTES
#define MAX_OPND_SIZE_BYTES 64
#endif

#define INT2OPND(x) (opnd_create_immed_int((x), OPSZ_PTR))

#define INTERFLOP_BUFFER_SIZE (MAX_INSTR_OPND_COUNT*MAX_OPND_SIZE_BYTES)

//Fonction qui sera appelée à la fin de l'analyse de code
static void event_exit(void);

static double* dbuffer;
static double* resultBuffer;
static long double fpubuff;

//Fonction qui sera appelée à chaque block de code
static dr_emit_flags_t event_basic_block(void *drcontext, //Contexte (permet de le passer à d'autres fonctions)
void *tag, //Identifiant unique du block
instrlist_t *bb, //Liste des instructions du block
bool for_trace, //TODO
bool translating); //TODO

//Fonction appelée par dynamoRIO pour setup le client
DR_EXPORT void dr_client_main(client_id_t id, //ID du client
int argc, const char *argv[]) //Paramètres
{
    dbuffer = (double*)malloc(INTERFLOP_BUFFER_SIZE);
    resultBuffer = (double*)malloc(64);
    drmgr_init();
    //On dit à DR quelles fonctions à utiliser
    dr_register_exit_event(event_exit);
    drmgr_register_bb_app2app_event(event_basic_block, NULL);
    //dr_printf("%d\n", reg_is_gpr(DR_REG_ST0));

}

static void event_exit(void)
{
    drmgr_exit();
    free(dbuffer);
    free(resultBuffer);
}

static inline void translate_insert(instr_t* newinstr, instrlist_t* ilist, instr_t* instr)
{
    instr_set_translation(newinstr, instr_get_app_pc(instr));
    instr_set_app(newinstr);
    instrlist_preinsert(ilist,instr, newinstr);
}

static void push_result_to_register(void* drcontext,instrlist_t *ilist, instr_t* instr, bool removeInstr)
{
    if(instr && ilist && drcontext)
    {
        if(!removeInstr)
        {
            instr_t* copy = instr_clone(drcontext, instr);
            instr_set_translation(copy, instr_get_app_pc(instr));
            instr_set_app(copy);
            instrlist_preinsert(ilist,instr, copy);
        }
        

        int num_dst = instr_num_dsts(instr);
        if(num_dst > 0)
        {
            opnd_t op = instr_get_dst(instr, 0);
            dr_print_opnd(drcontext, STDERR, op, "DST : ");
            instr_t* mov = NULL;
            if(opnd_is_reg(op))
            {
                reg_t reg = opnd_get_reg(op);
                if(reg_is_simd(reg)) //SIMD scalar
                {
                    //dr_printf("simd");
                    mov = INSTR_CREATE_movsd(drcontext, op,opnd_create_rel_addr(resultBuffer, OPSZ_8));
                    translate_insert(mov, ilist, instr);
                }else if(reg_is_fp(reg)) //x87 FPU
                {
                    //dr_printf("fp");
                    if(reg != DR_REG_ST0) //Not top of the stack, we need to swap the register 
                    {
                        mov = INSTR_CREATE_fxch(drcontext, op);
                        translate_insert(mov, ilist, instr);
                    }
                    //Pops the first
                    mov = INSTR_CREATE_fstp(drcontext, opnd_create_reg(DR_REG_ST0));
                    translate_insert(mov, ilist, instr);
                    //load the value
                    mov = INSTR_CREATE_fld(drcontext, opnd_create_rel_addr(resultBuffer, OPSZ_8));
                    translate_insert(mov, ilist, instr);

                    if(reg != DR_REG_ST0) //Not top of the stack, we need to swap the register 
                    {
                        mov = INSTR_CREATE_fxch(drcontext, op);
                        translate_insert(mov, ilist, instr);
                    }

                }else if(reg_is_mmx(reg)) //Intel MMX
                {
                    //dr_printf("mmx");
                    mov = INSTR_CREATE_movq(drcontext, op, opnd_create_rel_addr(resultBuffer, OPSZ_8));
                    translate_insert(mov, ilist, instr);
                }else //General purpose register
                {
                    //dr_printf("gpr");
                    mov = INSTR_CREATE_mov_ld(drcontext, op, opnd_create_rel_addr(resultBuffer, OPSZ_8));
                    translate_insert(mov, ilist, instr);
                }
                //TODO complete if necessary
            }else if(opnd_is_immed(op)) // Immediate value
            {
                //dr_printf("immed");
                mov = INSTR_CREATE_mov_imm(drcontext, op, opnd_create_rel_addr(resultBuffer, OPSZ_8));
                translate_insert(mov, ilist, instr);
            }else if(opnd_is_memory_reference(op))
            {
                if(opnd_is_base_disp(op)) //Register value + displacement
                {
                    //dr_printf("base_disp");
                    //This case needs special care because it's a memory address not accessible directly
                    //We can't mov from adress to adress so we'll copy the content in a register, 
                    //then copy the register to memory
                    instr_t* push = INSTR_CREATE_push(drcontext, opnd_create_reg(DR_REG_START_64));
                    translate_insert(push, ilist, instr);

                    mov = INSTR_CREATE_movq(drcontext, opnd_create_reg(DR_REG_START_64), opnd_create_rel_addr(resultBuffer, OPSZ_8));
                    translate_insert(mov, ilist, instr);
                    mov = INSTR_CREATE_movq(drcontext, op, opnd_create_reg(DR_REG_START_64));
                    translate_insert(mov, ilist, instr);

                    instr_t* pop = INSTR_CREATE_pop(drcontext, opnd_create_reg(DR_REG_START_64));
                    translate_insert(pop, ilist, instr);
                }else // Absolute/relative adress : direct access
                {
                    //dr_printf("abs/rel");
                    *(double*)opnd_get_addr(op) = *(resultBuffer);
                }
            }
        }
        
        instrlist_remove(ilist, instr);
        instr_destroy(drcontext, instr);

    }
}

static void printbuffer(int num_src, int opcode)
{
    //for(int i=0; i<num_src; i++)
    dr_printf("buffer : %lf\t%lf\n",*(dbuffer), *(dbuffer+1));
    printf("%Lf\n", fpubuff);
    unsigned char* c = (unsigned char*)&fpubuff;
    for(int i=0; i<10; i++)
        dr_printf("%02X", c[i]);
    *resultBuffer = *(dbuffer)+*(dbuffer+1);
    dr_printf("res : %lf\n",*(resultBuffer));
}

static void push_instr_to_doublebuffer(void *drcontext, instrlist_t *ilist, instr_t* instr)
{
    if(instr && ilist && drcontext)
    {
        int num_src = instr_num_srcs(instr);
        int i=0;
        for(; i<num_src; i++)
        {
            opnd_t op = instr_get_src(instr, i);
            dr_print_opnd(drcontext, STDERR, op, "\nOP :");
            instr_t* mov = NULL;
            if(opnd_is_reg(op))
            {
                reg_t reg = opnd_get_reg(op);
                if(reg_is_simd(reg)) //SIMD scalar
                {
                    //dr_printf("simd");
                    mov = INSTR_CREATE_movsd(drcontext, opnd_create_rel_addr(dbuffer+i, OPSZ_8), op);
                    translate_insert(mov, ilist, instr);
                }else if(reg_is_fp(reg)) //x87 FPU
                {
                    //dr_printf("fp");
                    //Annoying case where we can copy out of the stack only the top of the stack
                    if(reg != DR_REG_ST0) //Not top of the stack, we need to swap the register 
                    {
                        mov = INSTR_CREATE_fxch(drcontext, op);
                        translate_insert(mov, ilist, instr);
                    }
                    mov = INSTR_CREATE_fst(drcontext, opnd_create_rel_addr(&fpubuff, OPSZ_8));
                    *(dbuffer+i) = (double)fpubuff;
                    translate_insert(mov, ilist, instr);
                    if(reg != DR_REG_ST0) //Not top of the stack, we need to swap the register 
                    {
                        mov = INSTR_CREATE_fxch(drcontext, op);
                        translate_insert(mov, ilist, instr);
                    }
                    
                    
                }else if(reg_is_mmx(reg)) //Intel MMX
                {
                    //dr_printf("mmx");
                    mov = INSTR_CREATE_movq(drcontext, opnd_create_rel_addr(dbuffer+i, OPSZ_8), op);
                    translate_insert(mov, ilist, instr);
                }else //General purpose register
                {
                    //dr_printf("gpr");
                    mov = INSTR_CREATE_movq(drcontext, opnd_create_rel_addr(dbuffer+i, OPSZ_8),op);
                    translate_insert(mov, ilist, instr);
                }
                //TODO complete if necessary
            }else if(opnd_is_immed(op)) // Immediate value
            {
                //dr_printf("immed");
                mov = INSTR_CREATE_mov_imm(drcontext, opnd_create_rel_addr(dbuffer+i, OPSZ_8), op);
                translate_insert(mov, ilist, instr);
            }else if(opnd_is_memory_reference(op))
            {
                if(opnd_is_base_disp(op)) //Register value + displacement
                {
                    //dr_printf("base_disp");
                    //This case needs special care because it's a memory address not accessible directly
                    //We can't mov from adress to adress so we'll copy the content in a register, 
                    //then copy the register to memory
                    instr_t* push = INSTR_CREATE_push(drcontext, opnd_create_reg(DR_REG_START_64));
                    translate_insert(push, ilist, instr);

                    mov = INSTR_CREATE_movq(drcontext, opnd_create_reg(DR_REG_START_64),op);
                    translate_insert(mov, ilist, instr);
                    mov = INSTR_CREATE_movq(drcontext, opnd_create_rel_addr(dbuffer+i, OPSZ_8), opnd_create_reg(DR_REG_START_64));
                    translate_insert(mov, ilist, instr);

                    instr_t* pop = INSTR_CREATE_pop(drcontext, opnd_create_reg(DR_REG_START_64));
                    translate_insert(pop, ilist, instr);
                }else // Absolute/relative adress : direct access
                {
                    //dr_printf("abs/rel");
                    *(dbuffer+i) = *(double*)opnd_get_addr(op);
                }
            }
        }
        
    }
}

static dr_emit_flags_t event_basic_block(void *drcontext, void* tag, instrlist_t *bb, bool for_trace, bool translating)
{
    instr_t *instr, *next_instr;
    bool found=false;
    static bool stoppre = false;
    for(instr = instrlist_first(bb); instr != NULL; instr = next_instr)
    {
        next_instr = instr_get_next(instr);
        if(instr_get_opcode(instr) == OP_fadd || instr_get_opcode(instr)==OP_faddp || instr_get_opcode(instr)==OP_addsd)
        {
            dr_print_instr(drcontext, STDERR, instr, "Found : ");
            push_instr_to_doublebuffer(drcontext, bb, instr);
            
            dr_insert_clean_call(drcontext, bb, instr, (void*)printbuffer, false, 2, INT2OPND(instr_num_srcs(instr)), INT2OPND(instr_get_opcode(instr)));

            push_result_to_register(drcontext, bb, instr, true);

            found = true;
            stoppre=true;
            //instrlist_remove(bb, instr);
            //instr_destroy(drcontext, instr);
        }

    }

    /*for(instr = instrlist_first(bb); found && instr != NULL; instr = instr_get_next(instr))
    {
        
        dr_print_instr(drcontext, STDERR, instr, "");
        
    }
    for(instr = instrlist_first(bb); instr != NULL && found; instr = next_instr)
    {
        next_instr = instr_get_next(instr);
        dr_print_instr(drcontext, STDERR, instr, "POST :");
    }*/
    
    return DR_EMIT_DEFAULT;
}