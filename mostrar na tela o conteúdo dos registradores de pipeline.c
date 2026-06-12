void mostrar_pipeline(struct simulador *sim) {

    printf("\n========== REGISTRADORES PIPELINE ==========\n");

    printf("\n[BI/DI]\n");
    printf("PC = %d\n", sim->bi_di.pc);
    printf("PC_NEXT = %d\n", sim->bi_di.pc_next);
    printf("INST = %s\n", sim->bi_di.inst.inst_char);

    printf("\n[DI/EX]\n");
    printf("RS = %d\n", sim->di_ex.rs);
    printf("RT = %d\n", sim->di_ex.rt);
    printf("RD = %d\n", sim->di_ex.rd);

    printf("RS_VAL = %d\n", sim->di_ex.rs_val);
    printf("RT_VAL = %d\n", sim->di_ex.rt_val);
    printf("IMM = %d\n", sim->di_ex.imm);

    printf("\n[EX/MEM]\n");
    printf("RESULTADO ULA = %d\n", sim->ex_mem.resultado_ula);
    printf("DESTINO = %d\n", sim->ex_mem.destino);

    printf("\n[MEM/ER]\n");
    printf("DADO MEMORIA = %d\n", sim->mem_er.dado_memoria);
    printf("RESULTADO ULA = %d\n", sim->mem_er.resultado_ula);
    printf("DESTINO = %d\n", sim->mem_er.destino);

    printf("\n============================================\n");
}
