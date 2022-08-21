#include <stdio.h>
#include <stdlib.h>
#include "../../dynasm/dasm_proto.h"
#include "../../dynasm/dasm_riscv64.h"
#include <sys/mman.h>
#if !defined(MAP_ANONYMOUS) && defined(MAP_ANON)
#define MAP_ANONYMOUS MAP_ANON
#endif

static void* link_and_encode(dasm_State** d)
{
    size_t sz;
    void* buf;
    dasm_link(d, &sz);
    buf = mmap(0, sz, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    dasm_encode(d, buf);
    mprotect(buf, sz, PROT_READ | PROT_EXEC);
    return buf;
}

#define TAPE_SIZE 30000
#define MAX_NESTING 100

typedef struct bf_state
{
    unsigned char* tape;
    unsigned char (*get_ch)();
    void (*put_ch)(unsigned char);
} bf_state_t;

#define bad_program(s) exit(fprintf(stderr, "bad program near %.16s: %s\n", program, s))

static void(* bf_compile(const char* program) )(bf_state_t*)
{
    unsigned loops[MAX_NESTING];
    int nloops = 0;
    int n;
    dasm_State* d;
    unsigned npc = 8;
    unsigned nextpc = 0;
    |.arch riscv64
    |.section code
    dasm_init(&d, DASM_MAXSECTION);
    |.globals lbl_
    void* labels[lbl__MAX];
    dasm_setupglobal(&d, labels, lbl__MAX);
    |.actionlist bf_actions
    dasm_setup(&d, bf_actions);
    dasm_growpc(&d, npc);
    |.define aPtr, x5
    |.define aState, x6
    |.type state, bf_state_t, aState

    dasm_State** Dst = &d;
    |.code
    |->bf_main:
    | addi aState, x10, 0
    | ld aPtr, 0(aState)
    for(;;) {
        switch(*program++) {
        case '<':
            for(n = 1; *program == '<'; ++n, ++program);
            | addi aPtr, aPtr, -n%TAPE_SIZE
            break;
        case '>':
            for(n = 1; *program == '>'; ++n, ++program);
            | addi aPtr, aPtr, n%TAPE_SIZE
            break;
        case '+':
            for(n = 1; *program == '+'; ++n, ++program);
            | lb x10, 0(aPtr)
            | addi x10, x10, n
            | sb x10, 0(aPtr)
            break;
        case '-':
            for(n = 1; *program == '-'; ++n, ++program);
            | lb x10, 0(aPtr)
            | addi x10, x10, -n
            | sb x10, 0(aPtr)
            break;
        case ',':
            | ld x31, state:aState->get_ch
            | jalr x31
            | sb x10, 0(aPtr)
            break;
        case '.':
            | lb x10, 0(aPtr)
            | ld x31, state:aState->put_ch
            | jalr x31
            break;
        case '[':
            if(nloops == MAX_NESTING)
                bad_program("Nesting too deep");
            if(nextpc == npc) {
                npc *= 2;
                dasm_growpc(&d, npc);
            }
            | lb x10, 0(aPtr)
            | beqz x10, =>nextpc+1
            |=>nextpc:
            loops[nloops++] = nextpc;
            nextpc += 2;
            break;
        case ']':
            if(nloops == 0)
                bad_program("] without matching [");
            --nloops;
            | lb x10, 0(aPtr)
            | bnez x10, =>loops[nloops]
            |=>loops[nloops]+1:
            break;
        case 0:
            if(nloops != 0)
                program = "<EOF>", bad_program("[ without matching ]");
            | li x10, 0
            | ret
            link_and_encode(&d);
            dasm_free(&d);
            return (void(*)(bf_state_t*))labels[lbl_bf_main];
        }
    }
}

static void bf_putchar(unsigned char c)
{
    putchar((int)c);
}

static unsigned char bf_getchar()
{
    return (unsigned char)getchar();
}

static void bf_run(const char* program)
{
    bf_state_t state;
    unsigned char tape[TAPE_SIZE] = {0};
    state.tape = tape;
    state.get_ch = bf_getchar;
    state.put_ch = bf_putchar;
    bf_compile(program)(&state);
}

int main(int argc, char** argv)
{
    if(argc == 2) {
        long sz;
        char* program;
        FILE* f = fopen(argv[1], "r");
        if(!f) {
            fprintf(stderr, "Cannot open %s\n", argv[1]);
            return 1;
        }
        fseek(f, 0, SEEK_END);
        sz = ftell(f);
        program = (char*)malloc(sz + 1);
        fseek(f, 0, SEEK_SET);
        program[fread(program, 1, sz, f)] = 0;
        fclose(f);
        bf_run(program);
        return 0;
    } else {
        fprintf(stderr, "Usage: %s INFILE.bf\n", argv[0]);
        return 1;
    }
}
