#include "sqliteInt.h"
#include "vdbeInt.h"
#include <jit/jit.h>
#include <time.h>

typedef void (*FF) (void);

jit_context_t setupJIT() {
    // Setup LibJIT
    jit_context_t context;

    context = jit_context_create();
    jit_context_build_start(context);
    return context;
}

void teardownJIT(jit_context_t context) {
    jit_context_build_end(context);
}

jit_function_t sqlite3VdbeJITExec(Vdbe* p, jit_context_t context) {
    jit_type_t signature; // The signature of the JIT function
    signature = jit_type_create_signature(
        jit_abi_cdecl, jit_type_void, NULL, 0, 1
    );
    jit_function_t function = jit_function_create(context, signature); // The JITed function to return

    Op *aOp = p->aOp;          /* Copy of p->aOp */
    Op *pOp = aOp;             /* Current operation */

    /* Define the register union here */
    jit_type_t v[5] = {
            jit_type_float64,  // double r
            jit_type_long,     // i64 i
            jit_type_int,      // int nZero
            jit_type_void_ptr, // const char *zPType
            jit_type_void_ptr  // FuncDef *pDef
    };
    jit_type_t mem_type = jit_type_create_union(v, 5, 1);
    jit_type_t mem_type_ptr = jit_type_create_pointer(mem_type, 1);

    // Create registers for LibJIT
    jit_value_t *JIT_registers = calloc(p->nMem, sizeof(jit_value_t));

    for (int i = 0; i < p->nMem; i++) {
        JIT_registers[i] = jit_value_create_nint_constant(function, mem_type_ptr, &(p->aMem[i].u));
    }

    // Start JIT implementation
    // Loop through each opcode and emit instructions
    for (pOp=&aOp[p->pc]; 1; pOp++) {
        switch( pOp->opcode ) {
            case OP_Init: {
                // Just go directly to the next instruction indicated by P2
                pOp = &aOp[pOp->p2 - 1];
                break;
            }
            case OP_Integer: {
                // Stuff integer value in P1 into register P2
                jit_value_t p1 = jit_value_create_nint_constant(function, jit_type_long, pOp->p1);
                jit_insn_store_relative(function, JIT_registers[pOp->p2], 0, p1);
                MemSetTypeFlag(&p->aMem[pOp->p2], MEM_Int);
                break;
            }
            case OP_Real: {
                // Stuff real value in P4 into register P2
                jit_value_t p4 = jit_value_create_float64_constant(function, jit_type_float64, *pOp->p4.pReal);
                jit_insn_store_relative(function, JIT_registers[pOp->p2], 0, p4);
                MemSetTypeFlag(&p->aMem[pOp->p2], MEM_Real);
                break;
            }
            case OP_Goto: {
                // Jump to address in register P2
                pOp = &aOp[pOp->p2 - 1];
                break;
            }
            case OP_Add: {
                // Add the value in register P1 to the value in register P2
                // and store result in register P3.
                jit_type_t p1_type;
                jit_type_t p2_type;

                // Perform compile-time type checking of P1 and P2
                if ((p->aMem[pOp->p1].flags & MEM_Int) != 0) {
                    p1_type = jit_type_long;
                }
                if ((p->aMem[pOp->p1].flags & MEM_Real) != 0) {
                    p1_type = jit_type_float64;
                }
                if ((p->aMem[pOp->p2].flags & MEM_Int) != 0) {
                    p2_type = jit_type_long;
                }
                if ((p->aMem[pOp->p2].flags & MEM_Real) != 0) {
                    p2_type = jit_type_float64;
                }

                jit_value_t p1 = jit_insn_load_relative(function, JIT_registers[pOp->p1], 0, p1_type);
                jit_value_t p2 = jit_insn_load_relative(function, JIT_registers[pOp->p2], 0, p2_type);
                jit_value_t tmp = jit_insn_add(function, p1, p2);
                jit_insn_store_relative(function, JIT_registers[pOp->p3], 0, tmp);

                // Perform compile-time type checking of P3
                jit_type_t tmp_type = jit_value_get_type(tmp);
                int tmp_type_kind = jit_type_get_kind(tmp_type);
                if (tmp_type_kind == JIT_TYPE_INT || tmp_type_kind == JIT_TYPE_LONG) {
                    MemSetTypeFlag(&p->aMem[pOp->p3], MEM_Int);
                }
                if (tmp_type_kind == JIT_TYPE_FLOAT64) {
                    MemSetTypeFlag(&p->aMem[pOp->p3], MEM_Real);
                }

                break;
            }
            case OP_Subtract: {
                jit_type_t p1_type;
                jit_type_t p2_type;

                // Perform compile-time type checking of P1 and P2
                if ((p->aMem[pOp->p1].flags & MEM_Int) != 0) {
                    p1_type = jit_type_long;
                }
                if ((p->aMem[pOp->p1].flags & MEM_Real) != 0) {
                    p1_type = jit_type_float64;
                }
                if ((p->aMem[pOp->p2].flags & MEM_Int) != 0) {
                    p2_type = jit_type_long;
                }
                if ((p->aMem[pOp->p2].flags & MEM_Real) != 0) {
                    p2_type = jit_type_float64;
                }

                jit_value_t p1 = jit_insn_load_relative(function, JIT_registers[pOp->p1], 0, p1_type);
                jit_value_t p2 = jit_insn_load_relative(function, JIT_registers[pOp->p2], 0, p2_type);
                jit_value_t tmp = jit_insn_sub(function, p2, p1);
                jit_insn_store_relative(function, JIT_registers[pOp->p3], 0, tmp);

                // Perform compile-time type checking of P3
                jit_type_t tmp_type = jit_value_get_type(tmp);
                int tmp_type_kind = jit_type_get_kind(tmp_type);
                if (tmp_type_kind == JIT_TYPE_INT || tmp_type_kind == JIT_TYPE_LONG) {
                    MemSetTypeFlag(&p->aMem[pOp->p3], MEM_Int);
                }
                if (tmp_type_kind == JIT_TYPE_FLOAT64) {
                    MemSetTypeFlag(&p->aMem[pOp->p3], MEM_Real);
                }

                break;
            }
            case OP_Multiply: {
                jit_type_t p1_type;
                jit_type_t p2_type;

                // Perform compile-time type checking of P1 and P2
                if ((p->aMem[pOp->p1].flags & MEM_Int) != 0) {
                    p1_type = jit_type_long;
                }
                if ((p->aMem[pOp->p1].flags & MEM_Real) != 0) {
                    p1_type = jit_type_float64;
                }
                if ((p->aMem[pOp->p2].flags & MEM_Int) != 0) {
                    p2_type = jit_type_long;
                }
                if ((p->aMem[pOp->p2].flags & MEM_Real) != 0) {
                    p2_type = jit_type_float64;
                }

                jit_value_t p1 = jit_insn_load_relative(function, JIT_registers[pOp->p1], 0, p1_type);
                jit_value_t p2 = jit_insn_load_relative(function, JIT_registers[pOp->p2], 0, p2_type);
                jit_value_t tmp = jit_insn_mul(function, p1, p2);
                jit_insn_store_relative(function, JIT_registers[pOp->p3], 0, tmp);

                // Perform compile-time type checking of P3
                jit_type_t tmp_type = jit_value_get_type(tmp);
                int tmp_type_kind = jit_type_get_kind(tmp_type);
                if (tmp_type_kind == JIT_TYPE_INT || tmp_type_kind == JIT_TYPE_LONG) {
                    MemSetTypeFlag(&p->aMem[pOp->p3], MEM_Int);
                }
                if (tmp_type_kind == JIT_TYPE_FLOAT64) {
                    MemSetTypeFlag(&p->aMem[pOp->p3], MEM_Real);
                }

                break;
            }
            case OP_Divide: {
                jit_type_t p1_type;
                jit_type_t p2_type;

                // Perform compile-time type checking of P1 and P2
                if ((p->aMem[pOp->p1].flags & MEM_Int) != 0) {
                    p1_type = jit_type_long;
                }
                if ((p->aMem[pOp->p1].flags & MEM_Real) != 0) {
                    p1_type = jit_type_float64;
                }
                if ((p->aMem[pOp->p2].flags & MEM_Int) != 0) {
                    p2_type = jit_type_long;
                }
                if ((p->aMem[pOp->p2].flags & MEM_Real) != 0) {
                    p2_type = jit_type_float64;
                }

                jit_value_t p1 = jit_insn_load_relative(function, JIT_registers[pOp->p1], 0, p1_type);
                jit_value_t p2 = jit_insn_load_relative(function, JIT_registers[pOp->p2], 0, p2_type);
                jit_value_t tmp = jit_insn_div(function, p1, p2);
                jit_insn_store_relative(function, JIT_registers[pOp->p3], 0, tmp);

                // Perform compile-time type checking of P3
                jit_type_t tmp_type = jit_value_get_type(tmp);
                int tmp_type_kind = jit_type_get_kind(tmp_type);
                if (tmp_type_kind == JIT_TYPE_INT || tmp_type_kind == JIT_TYPE_LONG) {
                    MemSetTypeFlag(&p->aMem[pOp->p3], MEM_Int);
                }
                if (tmp_type_kind == JIT_TYPE_FLOAT64) {
                    MemSetTypeFlag(&p->aMem[pOp->p3], MEM_Real);
                }

                break;
            }
            case OP_Remainder: {
                jit_type_t p1_type;
                jit_type_t p2_type;

                // Perform compile-time type checking of P1 and P2
                if ((p->aMem[pOp->p1].flags & MEM_Int) != 0) {
                    p1_type = jit_type_long;
                }
                if ((p->aMem[pOp->p1].flags & MEM_Real) != 0) {
                    p1_type = jit_type_float64;
                }
                if ((p->aMem[pOp->p2].flags & MEM_Int) != 0) {
                    p2_type = jit_type_long;
                }
                if ((p->aMem[pOp->p2].flags & MEM_Real) != 0) {
                    p2_type = jit_type_float64;
                }

                jit_value_t p1 = jit_insn_load_relative(function, JIT_registers[pOp->p1], 0, p1_type);
                jit_value_t p2 = jit_insn_load_relative(function, JIT_registers[pOp->p2], 0, p2_type);
                jit_value_t tmp = jit_insn_rem(function, p1, p2);
                jit_insn_store_relative(function, JIT_registers[pOp->p3], 0, tmp);

                // Perform compile-time type checking of P3
                jit_type_t tmp_type = jit_value_get_type(tmp);
                int tmp_type_kind = jit_type_get_kind(tmp_type);
                if (tmp_type_kind == JIT_TYPE_INT || tmp_type_kind == JIT_TYPE_LONG) {
                    MemSetTypeFlag(&p->aMem[pOp->p3], MEM_Int);
                }
                if (tmp_type_kind == JIT_TYPE_FLOAT64) {
                    MemSetTypeFlag(&p->aMem[pOp->p3], MEM_Real);
                }

                break;
            }
            case OP_ResultRow: {
                // Set ResultSet and update pc
                p->cacheCtr = (p->cacheCtr + 2)|1;
                p->pResultSet = &p->aMem[pOp->p1];
                p->pc = (int)(pOp - aOp) + 1;
                break;
            }
            default: {
                goto exit_loop;
            }
        }
    }
    exit_loop: ;

    // Uncomment the below two lines to see LibJIT IR
//    printf("<Debug> Printing LibJIT IR...\n");
//    jit_dump_function(stdout, function, NULL);

    clock_t t0, t1;
    double jit_elapsed = 0.0;
    double compile_elapsed = 0.0;

    t0 = clock();
    jit_function_compile(function);
    FF compiled_vdbe = jit_function_to_closure(function);
    t1 = clock();
    compile_elapsed = ((double) t1-t0)/CLOCKS_PER_SEC;
    printf("<Benchmarks> JIT compile time took %.6f seconds !\n", compile_elapsed);

    // Uncomment the below two lines to see assembly
//    printf("<Debug> Printing Assembly...\n");
//    jit_dump_function(stdout, function, NULL);

    t0 = clock();
    compiled_vdbe();
    t1 = clock();
    jit_elapsed = ((double) t1-t0)/CLOCKS_PER_SEC;
    printf("<Benchmarks> JIT execution took %.6f seconds !\n", jit_elapsed);

    free(JIT_registers);
    return function;
}