#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ncurses.h>

#define DATA_SIZE 256
#define INSTR_SIZE 16
#define REG_COUNT 8
#define HISTORICO_SIZE 100

enum classe_inst {
    tipo_I,
    tipo_J,
    tipo_R
};

struct instrucao { // ins decodificada
    enum classe_inst tipo_inst; //Guarda se é tipo R, I ou J após decodificação.
    char inst_char[INSTR_SIZE + 1];
    int opcode;
    int rs, rt, rd;
    int funct;
    int imm;
    int addr;
};

//lw e sw
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
    int usa_imediato; // 1 = ULA usa o imediato como 2o operando (ADDI, LW, SW)
    int is_branch;    // 1 = instrucao e BEQ (opcode 9)
};

// Guarda a instrução buscada na etapa BI, para passar para DI no próximo ciclo.
struct BI_DI {
    struct instrucao inst;
    int pc;
    int pc_next; // pc + 1
};

struct DI_EX {
    int pc;
    int rs, rt, rd;
    int rs_val, rt_val; // Valores lidos dos registradores rs e rt. São os operandos A e B que vão para a ULA.
    int imm;
    int destino;
    int opcode;
    struct controle ctrl;
};

//Resultado calculado pela ULA no estágio EX
struct EX_MEM {
    int resultado_ula;
    int rt_val;
    int destino;
    int opcode;
    int zero;
    int mem_read;
    int mem_write;
    int reg_write;
    int is_branch; // 1 = instrucao e BEQ (opcode 9)
};

struct MEM_ER {
    int dado_memoria;
    int resultado_ula;
    int destino;
    int reg_write;
    int mem_read;
};

struct simulador {
    struct memoria_dados dmem; //mem de dados
    struct pc pc;
    int reg[REG_COUNT];
    struct instrucao *programa; // ponteiro para as instruções lidas do arquivo e quantas são
    int prog_size;
    struct ULA ula;
    struct controle ctrl;
    struct BI_DI bi_di; // reg busca e decodificação
    struct DI_EX di_ex;   // reg decodificação e execução
    struct EX_MEM ex_mem; //reg execução e memória
    struct MEM_ER mem_er; //reg memória e escrita
    int historico_pc[HISTORICO_SIZE];
    int topo_historico;
};


int executar_ula(struct ULA *ula, int op) {
    switch(op) {
        case 0: ula->resultado = ula->entrada1 + ula->entrada2; break;
        case 1: ula->resultado = ula->entrada1 - ula->entrada2; break;
        case 2: ula->resultado = ula->entrada1 & ula->entrada2; break;
        case 3: ula->resultado = ula->entrada1 | ula->entrada2; break;
        default: ula->resultado = 0; break;
    }
    return ula->resultado; // salva
}

void unidade_controle(struct instrucao *inst, struct controle *ctrl) {
    ctrl->alu_op = 0;
    ctrl->mem_read = 0;
    ctrl->mem_write = 0;
    ctrl->reg_write = 0;
    ctrl->usa_imediato = 0;
    ctrl->is_branch = 0;

    if (inst->tipo_inst == tipo_R) {
        ctrl->reg_write = 1; //tipo R sempre escreve no registrador
        ctrl->alu_op = inst->funct;
    } else if (inst->tipo_inst == tipo_I) {
        if (inst->opcode == 8) { //ADDI
            ctrl->reg_write = 1;
            ctrl->alu_op = 0; //SOMA
            ctrl->usa_imediato = 1; // soma rs + imediato, não rs + rt
        } else if (inst->opcode == 4) { //LW
            ctrl->mem_read = 1;
            ctrl->reg_write = 1;
            ctrl->usa_imediato = 1; // endereco = rs + imediato
        } else if (inst->opcode == 5) { //sw
            ctrl->mem_write = 1;
            ctrl->usa_imediato = 1; // endereco = rs + imediato
        } else if (inst->opcode == 9) { //beq
            ctrl->is_branch = 1; // marca explicitamente como desvio, para nao confundir com "bolhas" do pipeline
        }
    }
}

void decodificador(struct instrucao *inst) {
    int i;
    inst->inst_char[INSTR_SIZE] = '\0';
    inst->opcode = 0;

    for (i = 0; i < 4; i++)
        inst->opcode = (inst->opcode << 1) | (inst->inst_char[i] - '0');

    if (inst->opcode == 0)      inst->tipo_inst = tipo_R;
    else if (inst->opcode == 2) inst->tipo_inst = tipo_J;
    else                        inst->tipo_inst = tipo_I;

    if (inst->tipo_inst == tipo_R) {
        inst->rs = inst->rt = inst->rd = inst->funct = 0;
        for (i = 4;  i < 7;  i++) inst->rs    = (inst->rs    << 1) | (inst->inst_char[i] - '0');
        for (i = 7;  i < 10; i++) inst->rt    = (inst->rt    << 1) | (inst->inst_char[i] - '0');
        for (i = 10; i < 13; i++) inst->rd    = (inst->rd    << 1) | (inst->inst_char[i] - '0');
        for (i = 13; i < 16; i++) inst->funct = (inst->funct << 1) | (inst->inst_char[i] - '0');
    } else if (inst->tipo_inst == tipo_I) {
        inst->rs = inst->rt = inst->imm = 0;
        for (i = 4;  i < 7;  i++) inst->rs  = (inst->rs  << 1) | (inst->inst_char[i] - '0');
        for (i = 7;  i < 10; i++) inst->rt  = (inst->rt  << 1) | (inst->inst_char[i] - '0');
        for (i = 10; i < 16; i++) inst->imm = (inst->imm << 1) | (inst->inst_char[i] - '0');
    } else {
        inst->addr = 0;
        for (i = 4; i < 16; i++)
            inst->addr = (inst->addr << 1) | (inst->inst_char[i] - '0');
    }
}

// mostra a instrução decodificada (baseado em mostrar_instrucao do arquivo original)
const char *nome_instrucao(struct instrucao *inst) {
    if (inst->tipo_inst == tipo_R) {
        if (inst->funct == 0) return "ADD";
        else if (inst->funct == 1) return "SUB";
        else if (inst->funct == 2) return "AND";
        else if (inst->funct == 3) return "OR";
        else return "R";
    } else if (inst->tipo_inst == tipo_I) {
        if (inst->opcode == 8) return "ADDI";
        else if (inst->opcode == 4) return "LW";
        else if (inst->opcode == 5) return "SW";
        else if (inst->opcode == 9) return "BEQ";
        else return "I";
    } else {
        return "JUMP";
    }
}

void estagio_busca(struct simulador *sim) {
    if (sim->pc.pc >= sim->prog_size) return;
    sim->bi_di.inst    = sim->programa[sim->pc.pc]; // copia instrucao para BI/DI
    sim->bi_di.pc      = sim->pc.pc; // salva o PC atual no registrador
    sim->bi_di.pc_next = sim->pc.pc + 1; //já calcula o próximo
    sim->pc.pc++; //  avança o PC
}

void estagio_decodificacao(struct simulador *sim) {
    struct instrucao *inst = &sim->bi_di.inst; // pega a instrução que está no BI/DI
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

void estagio_execucao(struct simulador *sim) {
    struct DI_EX *d = &sim->di_ex;
    sim->ula.entrada1 = d->rs_val;
    sim->ula.entrada2 = d->ctrl.usa_imediato ? d->imm : d->rt_val;
    executar_ula(&sim->ula, d->ctrl.alu_op);
    sim->ex_mem.resultado_ula = sim->ula.resultado;
    sim->ex_mem.rt_val        = d->rt_val;
    //  Transportar registrador destino pelo pipeline (DI_EX → EX_MEM)
    sim->ex_mem.destino       = d->destino;
    //  Adicionar desvios no pipeline
    // flag zero: 1 se rs_val == rt_val, usado pelo BEQ na etapa MEM
    sim->ex_mem.zero          = (d->rs_val == d->rt_val) ? 1 : 0;
    // Transportar sinais de controle pelo pipeline (EX → MEM)
    sim->ex_mem.mem_read      = d->ctrl.mem_read;
    sim->ex_mem.mem_write     = d->ctrl.mem_write;
    sim->ex_mem.reg_write     = d->ctrl.reg_write;
    sim->ex_mem.is_branch     = d->ctrl.is_branch;
}

void estagio_memoria(struct simulador *sim) {
    struct EX_MEM *e = &sim->ex_mem;
    sim->mem_er.resultado_ula = e->resultado_ula;
    //  Transportar registrador destino pelo pipeline (EX_MEM → MEM_ER)
    sim->mem_er.destino       = e->destino;
    sim->mem_er.reg_write     = e->reg_write;
    if (e->mem_read) {
        // LW: le da memoria
        sim->mem_er.dado_memoria = sim->dmem.dados[e->resultado_ula];
    } else if (e->mem_write) {
        // SW: escreve na memoria
        sim->dmem.dados[e->resultado_ula] = e->rt_val;
        sim->mem_er.dado_memoria = 0;
    } else {
        sim->mem_er.dado_memoria = 0;
    }
    
    if (e->is_branch && e->zero) {
        // desvio: volta ao PC do BEQ e soma o imediato
        sim->pc.pc = sim->di_ex.pc + 1 + sim->di_ex.imm;
    }
}

void estagio_escrita(struct simulador *sim) {
    struct MEM_ER *m = &sim->mem_er;
    //  Transportar registrador destino pelo pipeline - aqui ele e usado para gravar
    if (m->reg_write && m->destino != 0) {
        // LW escreve dado da memoria, demais escrevem resultado da ULA
        if (sim->ex_mem.mem_read)
            sim->reg[m->destino] = m->dado_memoria;
        else
            sim->reg[m->destino] = m->resultado_ula;
    }
}

void voltar_instrucao(struct simulador *sim) {
    if (sim->topo_historico <= 0) return;
    // desempilha o ultimo PC salvo
    sim->topo_historico--;
    sim->pc.pc = sim->historico_pc[sim->topo_historico];
    // limpa os registradores de pipeline para reexecutar daqui
    memset(&sim->bi_di,  0, sizeof(sim->bi_di));
    memset(&sim->di_ex,  0, sizeof(sim->di_ex));
    memset(&sim->ex_mem, 0, sizeof(sim->ex_mem));
    memset(&sim->mem_er, 0, sizeof(sim->mem_er));
}

/* ========== ncurses ========== */

#define COL_MENU  0
#define COL_INFO  30
#define LIN_START 2

static WINDOW *win_menu;
static WINDOW *win_info;

static void init_ncurses() {
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);

    if (has_colors()) {
        start_color();
        init_pair(1, COLOR_CYAN,   COLOR_BLACK);
        init_pair(2, COLOR_YELLOW, COLOR_BLACK);
        init_pair(3, COLOR_GREEN,  COLOR_BLACK);
        init_pair(4, COLOR_WHITE,  COLOR_BLACK);
    }

    int rows, cols;
    getmaxyx(stdscr, rows, cols);

    win_menu = newwin(rows - 2, 28, 1, 0);
    win_info = newwin(rows - 2, cols - 30, 1, 30);

    box(win_menu, 0, 0);
    box(win_info, 0, 0);

    wrefresh(win_menu);
    wrefresh(win_info);
}

static void desenhar_titulo(const char *arquivo) {
    attron(COLOR_PAIR(1) | A_BOLD);
    mvprintw(0, 0, " MIPS Pipeline Simulator  [%s] ",
             (arquivo && arquivo[0]) ? arquivo : "nenhum arquivo carregado");
    attroff(COLOR_PAIR(1) | A_BOLD);
    refresh();
}

static void desenhar_menu(int sel) {
    const char *itens[] = {
        "1. Carregar arquivo",
        "2. Instrucao + registradores",
        "3. Executar (run)",
        "4. Passo (step)",
        "5. Voltar (back)",
        "6. Ver PC",
        "7. Detalhes de instrucao",
        "8. Mostrar pipeline",
        "0. Sair"
    };
    int n = 9;

    werase(win_menu);
    box(win_menu, 0, 0);
    wattron(win_menu, COLOR_PAIR(2) | A_BOLD);
    mvwprintw(win_menu, 1, 2, "=== MENU ===");
    wattroff(win_menu, COLOR_PAIR(2) | A_BOLD);

    for (int i = 0; i < n; i++) {
        if (i == sel) {
            wattron(win_menu, A_REVERSE);
            mvwprintw(win_menu, 3 + i, 2, "%-24s", itens[i]);
            wattroff(win_menu, A_REVERSE);
        } else {
            mvwprintw(win_menu, 3 + i, 2, "%-24s", itens[i]);
        }
    }

    mvwprintw(win_menu, 14, 2, "Use setas ou tecle");
    mvwprintw(win_menu, 15, 2, "o numero da opcao.");
    wrefresh(win_menu);
}

static void limpar_info() {
    werase(win_info);
    box(win_info, 0, 0);
    wrefresh(win_info);
}

static void info_print(int *linha, const char *fmt, ...) {
    int rows, cols;
    getmaxyx(win_info, rows, cols);
    if (*linha >= rows - 2) return;
    va_list ap;
    va_start(ap, fmt);
    wmove(win_info, *linha, 2);
    vw_printw(win_info, fmt, ap);
    va_end(ap);
    (*linha)++;
}

static void aguardar_tecla() {
    int rows, cols;
    getmaxyx(win_info, rows, cols);
    wattron(win_info, COLOR_PAIR(2));
    mvwprintw(win_info, rows - 2, 2, "[ pressione qualquer tecla ]");
    wattroff(win_info, COLOR_PAIR(2));
    wrefresh(win_info);
    wgetch(win_info);
}



static void tela_registradores(struct simulador *sim) {
    limpar_info();
    int l = 1;
    wattron(win_info, COLOR_PAIR(3) | A_BOLD);
    info_print(&l, "=== INSTRUCAO + REGISTRADORES ===");
    wattroff(win_info, COLOR_PAIR(3) | A_BOLD);
    l++;

    struct instrucao *inst = &sim->bi_di.inst;
    if (inst->inst_char[0] == '\0') {
        wattron(win_info, COLOR_PAIR(2));
        info_print(&l, "Nenhuma instrucao decodificada ainda.");
        wattroff(win_info, COLOR_PAIR(2));
    } else {
        info_print(&l, "Instrucao: %s", inst->inst_char);
        info_print(&l, "Tipo      : %s", nome_instrucao(inst));
    }
    l++;

    for (int i = 0; i < REG_COUNT; i++)
        info_print(&l, "R%d = %d", i, sim->reg[i]);
    wrefresh(win_info);
    aguardar_tecla();
}

static void tela_pipeline(struct simulador *sim) {
    limpar_info();
    int l = 1;
    wattron(win_info, COLOR_PAIR(3) | A_BOLD);
    info_print(&l, "=== REGISTRADORES DE PIPELINE ===");
    wattroff(win_info, COLOR_PAIR(3) | A_BOLD);
    l++;
    info_print(&l, "[BI/DI]");
    info_print(&l, "  PC      = %d", sim->bi_di.pc); // PC da instrução que foi buscada
    info_print(&l, "  PC_NEXT = %d", sim->bi_di.pc_next); // PC+1, o próximo
    info_print(&l, "  INST    = %s", sim->bi_di.inst.inst_char); // a instrução em binário
    l++;
    info_print(&l, "[DI/EX]");
    info_print(&l, "  RS      = %d  RT = %d  RD = %d", sim->di_ex.rs, sim->di_ex.rt, sim->di_ex.rd); // rs: fonte 1, rt: fonte 2, rd: destino (tipo R)
    info_print(&l, "  RS_VAL  = %d  RT_VAL = %d", sim->di_ex.rs_val, sim->di_ex.rt_val); // valores lidos de RS e RT
    info_print(&l, "  IMM     = %d  DESTINO = %d", sim->di_ex.imm, sim->di_ex.destino); // imediato (tipo I)
    l++;
    info_print(&l, "[EX/MEM]");
    info_print(&l, "  RESULTADO ULA = %d", sim->ex_mem.resultado_ula); // oq a ULA calculou
    info_print(&l, "  DESTINO = %d  ZERO = %d", sim->ex_mem.destino, sim->ex_mem.zero); // zero: flag do beq
    l++;
    info_print(&l, "[MEM/ER]");
    info_print(&l, "  DADO MEM      = %d", sim->mem_er.dado_memoria); // valor lido da memória (LW)
    info_print(&l, "  RESULTADO ULA = %d", sim->mem_er.resultado_ula); // resultado repassado
    info_print(&l, "  DESTINO = %d", sim->mem_er.destino); // destino final
    wrefresh(win_info);
    aguardar_tecla();
}

/* Só imprime o valor do PC atual, case 6 */
static void tela_pc(struct simulador *sim) {
    limpar_info();
    int l = 1;
    wattron(win_info, COLOR_PAIR(3) | A_BOLD);
    info_print(&l, "=== PC ===");
    wattroff(win_info, COLOR_PAIR(3) | A_BOLD);
    l++;
    info_print(&l, "PC atual : %d", sim->pc.pc);
    info_print(&l, "Prog size: %d", sim->prog_size);
    wrefresh(win_info);
    aguardar_tecla();
}

static void tela_step(struct simulador *sim) {
    limpar_info();
    int l = 1;

    if (sim->pc.pc >= sim->prog_size && sim->bi_di.inst.inst_char[0] == '\0') {
        // só para se o PC passou do fim E o BI/DI está vazio
        // enquanto ainda tem instrução no pipeline, continua
        wattron(win_info, COLOR_PAIR(2));
        info_print(&l, "Fim do programa.");
        wattroff(win_info, COLOR_PAIR(2));
        wrefresh(win_info);
        aguardar_tecla();
        return;
    }

    // empilha o PC atual antes de executar, para o back poder voltar aqui
    if (sim->topo_historico < HISTORICO_SIZE)
        sim->historico_pc[sim->topo_historico++] = sim->pc.pc;

    wattron(win_info, COLOR_PAIR(3) | A_BOLD);
    info_print(&l, "--- Ciclo: PC = %d ---", sim->pc.pc);
    wattroff(win_info, COLOR_PAIR(3) | A_BOLD);
    l++;

    estagio_escrita(sim);            // ER usa MEM_ER
    estagio_memoria(sim);            // MEM usa EX_MEM
    estagio_execucao(sim);           // EX  usa DI_EX
    estagio_decodificacao(sim);      // DI  usa BI_DI
    estagio_busca(sim);              // BI  usa PC

    info_print(&l, "[BI/DI]  PC=%d  INST=%s", sim->bi_di.pc, sim->bi_di.inst.inst_char);
    info_print(&l, "[DI/EX]  RS=%d RT=%d RD=%d  rs_v=%d rt_v=%d  imm=%d",
               sim->di_ex.rs, sim->di_ex.rt, sim->di_ex.rd,
               sim->di_ex.rs_val, sim->di_ex.rt_val, sim->di_ex.imm);
    info_print(&l, "[EX/MEM] ula=%d  dest=%d  zero=%d",
               sim->ex_mem.resultado_ula, sim->ex_mem.destino, sim->ex_mem.zero);
    info_print(&l, "[MEM/ER] mem=%d  ula=%d  dest=%d",
               sim->mem_er.dado_memoria, sim->mem_er.resultado_ula, sim->mem_er.destino);

    wrefresh(win_info);
    aguardar_tecla();
}

static void tela_run(struct simulador *sim) {
    limpar_info();
    int l = 1;
    wattron(win_info, COLOR_PAIR(3) | A_BOLD);
    info_print(&l, "=== RUN ===");
    wattroff(win_info, COLOR_PAIR(3) | A_BOLD);
    l++;

    while (sim->pc.pc < sim->prog_size) {
        // continua enquanto ainda ha instrucao no pipeline ou nao chegou ao fim
        if (sim->topo_historico < HISTORICO_SIZE)
            sim->historico_pc[sim->topo_historico++] = sim->pc.pc;
        estagio_escrita(sim);
        estagio_memoria(sim);
        estagio_execucao(sim);
        estagio_decodificacao(sim);
        estagio_busca(sim);
        info_print(&l, "Ciclo PC=%d  ula=%d", sim->bi_di.pc, sim->ex_mem.resultado_ula);
    }

    info_print(&l, "");
    info_print(&l, "Drenando pipeline...");
    int dr = 4;
    // pipeline tem 5 estágios, então após o último fetch
    // ainda existem até 4 instruções dentro dos registradores
    // esses 4 ciclos extras deixam todas elas chegarem ao ER
    while (dr-- > 0) {
        estagio_escrita(sim);
        estagio_memoria(sim);
        estagio_execucao(sim);
        estagio_decodificacao(sim);
    }
    info_print(&l, "Concluido.");
    wrefresh(win_info);
    aguardar_tecla();
}

static void tela_back(struct simulador *sim) {
    limpar_info();
    int l = 1;
    if (sim->topo_historico <= 0) {
        wattron(win_info, COLOR_PAIR(2));
        info_print(&l, "Nao ha instrucoes anteriores.");
        wattroff(win_info, COLOR_PAIR(2));
    } else {
        voltar_instrucao(sim);
        wattron(win_info, COLOR_PAIR(3));
        info_print(&l, "Voltou para PC = %d", sim->pc.pc);
        info_print(&l, "Historico restante: %d", sim->topo_historico);
        wattroff(win_info, COLOR_PAIR(3));
    }
    wrefresh(win_info);
    aguardar_tecla();
}

/* mostra os detalhes de uma instrucao do arquivo carregado (pelo indice no programa) */
static void tela_detalhe_instrucao(struct simulador *sim) {
    limpar_info();
    echo();
    curs_set(1);
    int l = 1;
    wattron(win_info, COLOR_PAIR(3) | A_BOLD);
    info_print(&l, "=== DETALHES DE INSTRUCAO DO ARQUIVO ===");
    wattroff(win_info, COLOR_PAIR(3) | A_BOLD);
    l++;

    if (sim->prog_size <= 0) {
        wattron(win_info, COLOR_PAIR(2));
        info_print(&l, "Nenhum arquivo carregado ainda.");
        wattroff(win_info, COLOR_PAIR(2));
        noecho();
        curs_set(0);
        wrefresh(win_info);
        aguardar_tecla();
        return;
    }

    char buf[32];
    mvwprintw(win_info, l, 2, "Indice (0-%d): ", sim->prog_size - 1);
    wrefresh(win_info);
    wgetnstr(win_info, buf, sizeof(buf) - 1);
    l += 2;
    int idx = atoi(buf);

    noecho();
    curs_set(0);

    if (idx < 0 || idx >= sim->prog_size) {
        wattron(win_info, COLOR_PAIR(2));
        info_print(&l, "Indice invalido.");
        wattroff(win_info, COLOR_PAIR(2));
    } else {
        // pega a instrucao direto do arquivo carregado (sim->programa) e decodifica
        struct instrucao inst = sim->programa[idx];
        decodificador(&inst);

        wattron(win_info, COLOR_PAIR(3));
        info_print(&l, "Instrucao : %s", inst.inst_char);
        info_print(&l, "Tipo      : %s", nome_instrucao(&inst));
        info_print(&l, "Opcode    : %d", inst.opcode);
        wattroff(win_info, COLOR_PAIR(3));

        if (inst.tipo_inst == tipo_R) {
            info_print(&l, "RS = %d  RT = %d  RD = %d  FUNCT = %d",
                       inst.rs, inst.rt, inst.rd, inst.funct);
        } else if (inst.tipo_inst == tipo_I) {
            info_print(&l, "RS = %d  RT = %d  IMM = %d", inst.rs, inst.rt, inst.imm);
        } else {
            info_print(&l, "ADDR = %d", inst.addr);
        }
    }

    wrefresh(win_info);
    aguardar_tecla();
}

/* ========== main ========== */

int carregar_arquivo(struct simulador *sim, struct instrucao *buf, const char *path) {
    FILE *arq = fopen(path, "r");
    if (!arq) return 0;
    int i = 0;
    while (i < 100 && fscanf(arq, "%s", buf[i].inst_char) != EOF)
        i++;
    fclose(arq);
    sim->programa  = buf;
    sim->prog_size = i;
    return i;
}

/* tela do menu (opcao 1): pede o caminho e carrega o arquivo sem sair do ncurses */
static void tela_carregar_arquivo(struct simulador *sim, struct instrucao *buf,
                                   char *caminho, size_t cam_size) {
    limpar_info();
    echo();
    curs_set(1);
    int l = 1;
    wattron(win_info, COLOR_PAIR(3) | A_BOLD);
    info_print(&l, "=== CARREGAR ARQUIVO ===");
    wattroff(win_info, COLOR_PAIR(3) | A_BOLD);
    l++;

    char novo_caminho[256] = "";
    mvwprintw(win_info, l, 2, "Caminho do arquivo .mem: ");
    wrefresh(win_info);
    wgetnstr(win_info, novo_caminho, sizeof(novo_caminho) - 1);
    l += 2;

    noecho();
    curs_set(0);

    if (novo_caminho[0] == '\0') {
        wattron(win_info, COLOR_PAIR(2));
        info_print(&l, "Nenhum caminho informado.");
        wattroff(win_info, COLOR_PAIR(2));
    } else {
        int n = carregar_arquivo(sim, buf, novo_caminho);
        if (n <= 0) {
            wattron(win_info, COLOR_PAIR(2));
            info_print(&l, "Erro ao abrir '%s'.", novo_caminho);
            wattroff(win_info, COLOR_PAIR(2));
        } else {
            strncpy(caminho, novo_caminho, cam_size - 1);
            caminho[cam_size - 1] = '\0';

            /* reinicia o estado de execucao para o programa recem-carregado */
            sim->pc.pc = 0;
            sim->pc.prev_pc = -1;
            sim->topo_historico = 0;
            memset(&sim->bi_di,  0, sizeof(sim->bi_di));
            memset(&sim->di_ex,  0, sizeof(sim->di_ex));
            memset(&sim->ex_mem, 0, sizeof(sim->ex_mem));
            memset(&sim->mem_er, 0, sizeof(sim->mem_er));

            wattron(win_info, COLOR_PAIR(3));
            info_print(&l, "Arquivo carregado: %s", caminho);
            info_print(&l, "Instrucoes lidas : %d", n);
            wattroff(win_info, COLOR_PAIR(3));
        }
    }

    wrefresh(win_info);
    aguardar_tecla();
}

int main(int argc, char *argv[]) {
    static struct instrucao programa[100];
    struct simulador sim;
    memset(&sim, 0, sizeof(sim));
    sim.pc.pc      = 0;
    sim.pc.prev_pc = -1;
    sim.topo_historico = 0;

    char caminho[256] = "";

    if (argc >= 2) {
        strncpy(caminho, argv[1], sizeof(caminho) - 1);
        if (!carregar_arquivo(&sim, programa, caminho)) {
            fprintf(stderr, "Aviso: nao foi possivel abrir '%s'.\n", caminho);
            fprintf(stderr, "Use a opcao 1 do menu para carregar o arquivo.\n");
            caminho[0] = '\0';
            sim.programa  = programa;
            sim.prog_size = 0;
        }
    } else {
        sim.programa  = programa;
        sim.prog_size = 0;
    }

    init_ncurses();
    desenhar_titulo(caminho);

    const int opcoes[] = {1,2,3,4,5,6,7,8,0};
    int sel = 0;
    int n_itens = 9;

    desenhar_menu(sel);
    limpar_info();

    int ch;
    while (1) {
        desenhar_menu(sel);
        ch = wgetch(win_menu);

        /* navegação por setas */
        if (ch == KEY_UP) {
            sel = (sel - 1 + n_itens) % n_itens;
            continue;
        }
        if (ch == KEY_DOWN) {
            sel = (sel + 1) % n_itens;
            continue;
        }

        int op = -1;
        if (ch == '\n' || ch == '\r') {
            op = opcoes[sel];
        } else if (ch >= '0' && ch <= '9') {
            op = ch - '0';
            /* sincroniza sel visual */
            for (int i = 0; i < n_itens; i++)
                if (opcoes[i] == op) { sel = i; break; }
        }

        if (op == -1) continue;
        if (op == 0) break;

        switch (op) {
            case 1: tela_carregar_arquivo(&sim, programa, caminho, sizeof(caminho)); break;
            case 2: tela_registradores(&sim);        break;
            case 3: tela_run(&sim);                  break;
            case 4: tela_step(&sim);                 break;
            case 5: tela_back(&sim);                 break;
            case 6: tela_pc(&sim);                   break;
            case 7: tela_detalhe_instrucao(&sim);    break;
            case 8: tela_pipeline(&sim);             break;
        }

        limpar_info();
        desenhar_titulo(caminho);
    }

    endwin();
    return 0;
}
