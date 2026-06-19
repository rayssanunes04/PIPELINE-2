#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DATA_SIZE 256
#define INSTR_SIZE 16
#define REG_COUNT 8
#define HISTORICO_SIZE 100 // RC: arrumar o back para fazer varios - tamanho do historico

enum classe_inst {
    tipo_I,
    tipo_J,
    tipo_R
};

struct instrucao {
    enum classe_inst tipo_inst;
    char inst_char[INSTR_SIZE + 1];
    int opcode;
    int rs, rt, rd;
    int funct;
    int imm;
    int addr;
};

struct memoria_dados {
    int dados[DATA_SIZE];
};

struct pc {
    int pc;
    int prev_pc;
};

struct ULA {
    int entrada1;
    int entrada2;
    int resultado;
};

struct controle {
    int alu_op;
    int mem_read;
    int mem_write;
    int reg_write;
};

// RC: Transportar registrador destino e sinais de controle pelo pipeline
// Registrador de pipeline BI/DI: guarda instrucao buscada e PC
struct BI_DI {
    struct instrucao inst;
    int pc;
    int pc_next;
};

// RC: Transportar registrador destino e sinais de controle pelo pipeline
// Registrador de pipeline DI/EX: guarda valores lidos dos registradores, destino e controle
struct DI_EX {
    int pc;
    int rs, rt, rd;
    int rs_val, rt_val;
    int imm;               // imediato da instrucao
    int destino;           // RC: registrador destino transportado pelo pipeline
    struct controle ctrl;  // RC: sinais de controle transportados pelo pipeline
};

// RC: Transportar registrador destino e sinais de controle pelo pipeline
// Registrador de pipeline EX/MEM: guarda resultado da ULA, destino e controle
struct EX_MEM {
    int resultado_ula;
    int rt_val;
    int destino;   // RC: registrador destino transportado
    int zero;      // RC: flag de desvio para BEQ
    int mem_read;  // RC: sinal de controle transportado
    int mem_write; // RC: sinal de controle transportado
    int reg_write; // RC: sinal de controle transportado
};

// RC: Transportar registrador destino e sinais de controle pelo pipeline
// Registrador de pipeline MEM/ER: guarda dado da memoria, resultado e destino
struct MEM_ER {
    int dado_memoria;
    int resultado_ula;
    int destino;   // RC: registrador destino transportado
    int reg_write; // RC: sinal de controle transportado
};

// RC: Transportar registrador destino e sinais de controle pelo pipeline
// struct simulador atualizada com os 4 registradores de pipeline
struct simulador {
    struct memoria_dados dmem;
    struct pc pc;
    int reg[REG_COUNT];
    struct instrucao *programa;
    int prog_size;

    struct ULA ula;
    struct controle ctrl;

    struct BI_DI bi_di;   // RC: registrador pipeline BI/DI
    struct DI_EX di_ex;   // RC: registrador pipeline DI/EX
    struct EX_MEM ex_mem; // RC: registrador pipeline EX/MEM
    struct MEM_ER mem_er; // RC: registrador pipeline MEM/ER

    // RC: arrumar o back para fazer varios - historico de estados
    int historico_pc[HISTORICO_SIZE];
    int topo_historico;
};


void mostrar_pc(struct simulador *sim) {
    printf("\nPC atual: %d\n", sim->pc.pc);
}

void mostrar_instrucao(struct instrucao *inst) {

    printf("Tipo: ");

    if (inst->tipo_inst == tipo_R) {
        if (inst->funct == 0) printf("ADD\n");
        else if (inst->funct == 1) printf("SUB\n");
        else if (inst->funct == 2) printf("AND\n");
        else if (inst->funct == 3) printf("OR\n");
        else printf("R\n");
    }
    else if (inst->tipo_inst == tipo_I) {
        if (inst->opcode == 8) printf("ADDI\n");
        else if (inst->opcode == 4) printf("LW\n");
        else if (inst->opcode == 5) printf("SW\n");
        else if (inst->opcode == 9) printf("BEQ\n");
        else printf("I\n");
    }
    else {
        printf("JUMP\n");
    }
}

void digitar_memoria(struct memoria_dados *mem) {

    int n, pos, valor;

    printf("\n=== MEMORIA DE DADOS ===\n");
    printf("Quantas posicoes deseja preencher? ");
    scanf("%d", &n);

    for (int i = 0; i < n; i++) {

        do {
            printf("Endereco (0 a %d): ", DATA_SIZE-1);
            scanf("%d", &pos);
        } while (pos < 0 || pos >= DATA_SIZE);

        printf("Valor: ");
        scanf("%d", &valor);

        mem->dados[pos] = valor;
    }
}

void definir_registradores(int reg[]) {

    printf("\n=== DEFINIR REGISTRADORES (R0 a R7) ===\n");

    for (int i = 0; i < REG_COUNT; i++) {
        printf("R%d: ", i);
        scanf("%d", &reg[i]);
    }
}

void imprimir_memoria(struct memoria_dados *mem) {
    int i;

    printf("\n=== MEMORIA ===\n");

    for (i = 0; i < DATA_SIZE; i++) {
        if (mem->dados[i] != 0) {
            printf("Mem[%d] = %d\n", i, mem->dados[i]);
        }
    }
}

void mostrar_registradores(int reg[]) {
    int i;

    printf("\n=== REGISTRADORES ===\n");

    for (i = 0; i < REG_COUNT; i++) {
        printf("R%d = %d\n", i, reg[i]);
    }
}

// RC: mostrar estado dos registradores de pipeline
void mostrar_pipeline(struct simulador *sim) {

    printf("\n========== PIPELINE DE REGISTRADORES ==========\n");

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
    printf("DESTINO = %d\n", sim->di_ex.destino);

    printf("\n[EX/MEM]\n");
    printf("RESULTADO ULA = %d\n", sim->ex_mem.resultado_ula);
    printf("DESTINO = %d\n", sim->ex_mem.destino);
    printf("ZERO = %d\n", sim->ex_mem.zero);

    printf("\n[MEM/ER]\n");
    printf("DADO MEMORIA = %d\n", sim->mem_er.dado_memoria);
    printf("RESULTADO ULA = %d\n", sim->mem_er.resultado_ula);
    printf("DESTINO = %d\n", sim->mem_er.destino);

    printf("\n================================================\n");
}

int executar_ula(struct ULA *ula, int op) {

    switch(op) {
        case 0: ula->resultado = ula->entrada1 + ula->entrada2; break;
        case 1: ula->resultado = ula->entrada1 - ula->entrada2; break;
        case 2: ula->resultado = ula->entrada1 & ula->entrada2; break;
        case 3: ula->resultado = ula->entrada1 | ula->entrada2; break;
        default: ula->resultado = 0; break;
    }

    return ula->resultado;
}

void unidade_controle(struct instrucao *inst, struct controle *ctrl) {

    ctrl->alu_op = 0;
    ctrl->mem_read = 0;
    ctrl->mem_write = 0;
    ctrl->reg_write = 0;

    if (inst->tipo_inst == tipo_R) {
        ctrl->reg_write = 1;
        ctrl->alu_op = inst->funct;
    }
    else if (inst->tipo_inst == tipo_I) {
        if (inst->opcode == 8) {
            ctrl->reg_write = 1;
            ctrl->alu_op = 0;
        }
        else if (inst->opcode == 4) {
            ctrl->mem_read = 1;
            ctrl->reg_write = 1;
        }
        else if (inst->opcode == 5) {
            ctrl->mem_write = 1;
        }
    }
}

void decodificador(struct instrucao *inst) {

    int i;

    inst->inst_char[INSTR_SIZE] = '\0';
    inst->opcode = 0;

    for (i = 0; i < 4; i++)
        inst->opcode = (inst->opcode << 1) | (inst->inst_char[i] - '0');

    if (inst->opcode == 0)       inst->tipo_inst = tipo_R;
    else if (inst->opcode == 2)  inst->tipo_inst = tipo_J;
    else                         inst->tipo_inst = tipo_I;

    if (inst->tipo_inst == tipo_R) {
        inst->rs = inst->rt = inst->rd = inst->funct = 0;
        for (i = 4;  i < 7;  i++) inst->rs    = (inst->rs    << 1) | (inst->inst_char[i] - '0');
        for (i = 7;  i < 10; i++) inst->rt    = (inst->rt    << 1) | (inst->inst_char[i] - '0');
        for (i = 10; i < 13; i++) inst->rd    = (inst->rd    << 1) | (inst->inst_char[i] - '0');
        for (i = 13; i < 16; i++) inst->funct = (inst->funct << 1) | (inst->inst_char[i] - '0');
    }
    else if (inst->tipo_inst == tipo_I) {
        inst->rs = inst->rt = inst->imm = 0;
        for (i = 4;  i < 7;  i++) inst->rs  = (inst->rs  << 1) | (inst->inst_char[i] - '0');
        for (i = 7;  i < 10; i++) inst->rt  = (inst->rt  << 1) | (inst->inst_char[i] - '0');
        for (i = 10; i < 16; i++) inst->imm = (inst->imm << 1) | (inst->inst_char[i] - '0');
    }
    else {
        inst->addr = 0;
        for (i = 4; i < 16; i++)
            inst->addr = (inst->addr << 1) | (inst->inst_char[i] - '0');
    }
}


// ============================================================
// RC: Atualizar run_simulation para pipeline
// Estagio 1 - Busca (BI): le instrucao e avanca PC
// ============================================================
void estagio_busca(struct simulador *sim) {

    if (sim->pc.pc >= sim->prog_size) return;

    sim->bi_di.inst    = sim->programa[sim->pc.pc]; // copia instrucao para BI/DI
    sim->bi_di.pc      = sim->pc.pc;
    sim->bi_di.pc_next = sim->pc.pc + 1;

    sim->pc.pc++;
}

// ============================================================
// RC: Atualizar run_simulation para pipeline
// Estagio 2 - Decodificacao (DI): le registradores e gera controle
// RC: Transportar registrador destino e sinais de controle pelo pipeline
// ============================================================
void estagio_decodificacao(struct simulador *sim) {

    struct instrucao *inst = &sim->bi_di.inst;

    if (inst->inst_char[0] == '\0') return;

    decodificador(inst);

    struct controle ctrl;
    unidade_controle(inst, &ctrl);

    sim->di_ex.pc     = sim->bi_di.pc;
    sim->di_ex.rs     = inst->rs;
    sim->di_ex.rt     = inst->rt;
    sim->di_ex.rd     = inst->rd;
    sim->di_ex.rs_val = sim->reg[inst->rs]; // lendo registrador rs
    sim->di_ex.rt_val = sim->reg[inst->rt]; // lendo registrador rt
    sim->di_ex.imm    = inst->imm;

    // RC: Transportar registrador destino pelo pipeline
    // tipo R usa rd, tipo I usa rt como destino
    if (inst->tipo_inst == tipo_R)
        sim->di_ex.destino = inst->rd;
    else
        sim->di_ex.destino = inst->rt;

    // RC: Transportar sinais de controle pelo pipeline (DI → EX)
    sim->di_ex.ctrl = ctrl;
}

// ============================================================
// RC: Atualizar run_simulation para pipeline
// Estagio 3 - Execucao (EX): opera a ULA
// RC: Transportar registrador destino e sinais de controle pelo pipeline
// RC: Adicionar desvios no pipeline (BEQ calcula zero aqui)
// ============================================================
void estagio_execucao(struct simulador *sim) {

    struct DI_EX *d = &sim->di_ex;

    sim->ula.entrada1 = d->rs_val;
    sim->ula.entrada2 = (d->ctrl.mem_read || d->ctrl.mem_write) ? d->imm : d->rt_val;

    executar_ula(&sim->ula, d->ctrl.alu_op);

    sim->ex_mem.resultado_ula = sim->ula.resultado;
    sim->ex_mem.rt_val        = d->rt_val;

    // RC: Transportar registrador destino pelo pipeline (DI_EX → EX_MEM)
    sim->ex_mem.destino   = d->destino;

    // RC: Adicionar desvios no pipeline
    // flag zero: 1 se rs_val == rt_val, usado pelo BEQ na etapa MEM
    sim->ex_mem.zero      = (d->rs_val == d->rt_val) ? 1 : 0;

    // RC: Transportar sinais de controle pelo pipeline (EX → MEM)
    sim->ex_mem.mem_read  = d->ctrl.mem_read;
    sim->ex_mem.mem_write = d->ctrl.mem_write;
    sim->ex_mem.reg_write = d->ctrl.reg_write;
}

// ============================================================
// RC: Atualizar run_simulation para pipeline
// Estagio 4 - Memoria (MEM): acessa memoria de dados
// RC: Adicionar desvios no pipeline (BEQ desvia aqui)
// ============================================================
void estagio_memoria(struct simulador *sim) {

    struct EX_MEM *e = &sim->ex_mem;

    sim->mem_er.resultado_ula = e->resultado_ula;

    // RC: Transportar registrador destino pelo pipeline (EX_MEM → MEM_ER)
    sim->mem_er.destino   = e->destino;
    sim->mem_er.reg_write = e->reg_write;

    if (e->mem_read) {
        // LW: le da memoria
        sim->mem_er.dado_memoria = sim->dmem.dados[e->resultado_ula];
    }
    else if (e->mem_write) {
        // SW: escreve na memoria
        sim->dmem.dados[e->resultado_ula] = e->rt_val;
        sim->mem_er.dado_memoria = 0;
    }
    else {
        sim->mem_er.dado_memoria = 0;
    }

    // RC: Adicionar desvios no pipeline
    // BEQ: se zero=1, corrige o PC (desvio acontece no estagio MEM)
    // A instrucao BEQ tem opcode 9 - verificamos pelo opcode salvo em di_ex
    // Usamos o imediato de di_ex para calcular o alvo do desvio
    if (e->zero && !e->mem_read && !e->mem_write && !e->reg_write) {
        // desvio: volta ao PC do BEQ e soma o imediato
        sim->pc.pc = sim->di_ex.pc + 1 + sim->di_ex.imm;
        printf("[PIPELINE] Desvio BEQ tomado! Novo PC = %d\n", sim->pc.pc);
    }
}

// ============================================================
// RC: Atualizar run_simulation para pipeline
// Estagio 5 - Escrita (ER): escreve resultado no banco de registradores
// ============================================================
void estagio_escrita(struct simulador *sim) {

    struct MEM_ER *m = &sim->mem_er;

    // RC: Transportar registrador destino pelo pipeline - aqui ele e usado para gravar
    if (m->reg_write && m->destino != 0) {
        // LW escreve dado da memoria, demais escrevem resultado da ULA
        if (sim->ex_mem.mem_read)
            sim->reg[m->destino] = m->dado_memoria;
        else
            sim->reg[m->destino] = m->resultado_ula;
    }
}


// ============================================================
// RC: Atualizar run_simulation para pipeline
// step_simulation agora executa os 5 estagios do pipeline em ordem reversa
// (reversa para simular que cada instrucao avanca um estagio por ciclo)
// ============================================================
void step_simulation(struct simulador *sim) {

    if (sim->pc.pc >= sim->prog_size && sim->bi_di.inst.inst_char[0] == '\0') {
        printf("Fim do programa\n");
        return;
    }

    // RC: arrumar o back para fazer varios - salva PC antes de executar
    if (sim->topo_historico < HISTORICO_SIZE) {
        sim->historico_pc[sim->topo_historico++] = sim->pc.pc;
    }

    printf("\n--- Ciclo: PC = %d ---\n", sim->pc.pc);

    // RC: Atualizar run_simulation para pipeline
    // estagios em ordem reversa para nao sobrescrever dados do ciclo atual
    estagio_escrita(sim);            // ER usa MEM_ER
    estagio_memoria(sim);            // MEM usa EX_MEM
    estagio_execucao(sim);           // EX  usa DI_EX
    estagio_decodificacao(sim);      // DI  usa BI_DI
    estagio_busca(sim);              // BI  usa PC

    mostrar_pipeline(sim);
}


// ============================================================
// RC: Atualizar run_simulation para pipeline
// run_simulation agora roda ciclos ate esvaziar o pipeline
// ============================================================
void run_simulation(struct simulador *sim) {

    // continua enquanto ainda ha instrucao no pipeline ou nao chegou ao fim
    while (sim->pc.pc < sim->prog_size) {
        step_simulation(sim);
    }

    // drena os estagios restantes do pipeline (flush)
    printf("\n[PIPELINE] Drenando pipeline...\n");
    int ciclos_drenagem = 4;
    while (ciclos_drenagem-- > 0) {
        estagio_escrita(sim);
        estagio_memoria(sim);
        estagio_execucao(sim);
        estagio_decodificacao(sim);
        mostrar_pipeline(sim);
    }
}


// ============================================================
// RC: Arrumar o back para fazer varios
// agora usa pilha de historico para voltar multiplos passos
// ============================================================
void voltar_instrucao(struct simulador *sim) {

    if (sim->topo_historico <= 0) {
        printf("\nNao ha instrucoes anteriores para voltar.\n");
        return;
    }

    // desempilha o ultimo PC salvo
    sim->topo_historico--;
    sim->pc.pc = sim->historico_pc[sim->topo_historico];

    // limpa os registradores de pipeline para reexecutar daqui
    memset(&sim->bi_di,  0, sizeof(sim->bi_di));
    memset(&sim->di_ex,  0, sizeof(sim->di_ex));
    memset(&sim->ex_mem, 0, sizeof(sim->ex_mem));
    memset(&sim->mem_er, 0, sizeof(sim->mem_er));

    printf("\n[BACK] Voltou para instrucao %d (historico: %d restantes)\n",
           sim->pc.pc, sim->topo_historico);
}


void menu(struct simulador *sim) {

    int op;

    do {
        printf("\nMenu principal:\n");
        printf("1. Imprimir memorias\n");
        printf("2. Imprimir registradores\n");
        printf("3. Executar programa (run)\n");
        printf("4. Executar passo (step)\n");
        printf("5. Voltar instrucao (back)\n");
        printf("6. Ver valor do PC\n");
        printf("7. Digitar memoria de dados\n");
        printf("8. Definir valores dos registradores\n");
        printf("9. Mostrar pipeline\n");
        printf("0. Sair\n");
        printf("Opcao: ");
        scanf("%d", &op);

        switch(op) {
            case 1: imprimir_memoria(&sim->dmem);      break;
            case 2: mostrar_registradores(sim->reg);   break;
            case 3: run_simulation(sim);               break;
            case 4: step_simulation(sim);              break;
            case 5: voltar_instrucao(sim);             break;
            case 6: mostrar_pc(sim);                   break;
            case 7: digitar_memoria(&sim->dmem);       break;
            case 8: definir_registradores(sim->reg);   break;
            case 9: mostrar_pipeline(sim);             break;
            case 0: break;
            default: printf("Opcao invalida\n");       break;
        }

    } while(op != 0);
}


int main() {

    FILE *arq = fopen("teste.mem", "r");

    if (!arq) {
        printf("Erro ao abrir teste.mem\n");
        return 1;
    }

    struct instrucao programa[100];
    int i = 0;

    while (fscanf(arq, "%s", programa[i].inst_char) != EOF) {
        i++;
    }

    fclose(arq);

    struct simulador sim;
    memset(&sim, 0, sizeof(sim));
    sim.pc.pc          = 0;
    sim.pc.prev_pc     = -1;
    sim.programa       = programa;
    sim.prog_size      = i;
    sim.topo_historico = 0;

    menu(&sim);

    return 0;
}
