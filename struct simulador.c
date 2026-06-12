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
