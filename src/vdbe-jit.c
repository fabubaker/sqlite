#include "sqliteInt.h"
#include "vdbeInt.h"
#include <jit/jit.h>

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
                jit_value_t p1 = jit_insn_load_relative(function, JIT_registers[pOp->p1], 0, jit_type_long);
                jit_value_t p2 = jit_insn_load_relative(function, JIT_registers[pOp->p2], 0, jit_type_long);
                jit_value_t tmp = jit_insn_add(function, p1, p2);
                jit_insn_store_relative(function, JIT_registers[pOp->p3], 0, tmp);
                break;
            }
            case OP_ResultRow: {
                // Set ResultSet and update pc
                p->cacheCtr = (p->cacheCtr + 2)|1;
                p->pResultSet = &p->aMem[pOp->p1];
                p->pc = (int)(pOp - aOp) + 1;
                MemSetTypeFlag(&p->aMem[1], MEM_Int);
                break;
            }
            default: {
                goto exit_loop;
            }
        }
    }
    exit_loop: ;

    jit_dump_function(stdout, function, "union");
    jit_function_compile(function);
    jit_dump_function(stdout, function, "union-asm");
    jit_function_apply(function, NULL, NULL);

    printf("POP P3 Register: %lld\n", &(p->aMem[1].u));
    printf("HERE IS RESULT: %lld \n", p->aMem[1].u.i);

    return function;
}

void emit_OP_Init(Vdbe* p) {
    Op *aOp = p->aOp;          /* Copy of p->aOp */
    Op *pOp = aOp;             /* Current operation */
}