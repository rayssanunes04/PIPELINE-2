struct BI_DI {
    struct instrucao inst;
    int pc;
    int pc_next;
};

struct DI_EX {
    int pc;
    int rs, rt, rd;
    int rs_val, rt_val;
    int imm;
    struct controle ctrl;
};

struct EX_MEM {
    int resultado_ula;
    int rt_val;
    int destino;
    int zero;
    int mem_read;
    int mem_write;
    int reg_write;
};

struct MEM_ER {
    int dado_memoria;
    int resultado_ula;
    int destino;
    int reg_write;
};

struct simulador {
    struct memoria_dados dmem;
    struct pc pc;
    int reg[REG_COUNT];

    struct instrucao *programa;
    int prog_size;

    struct ULA ula;
    struct controle ctrl;

    struct BI_DI bi_di;
    struct DI_EX di_ex;
    struct EX_MEM ex_mem;
    struct MEM_ER mem_er;
};
