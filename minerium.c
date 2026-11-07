/*
 * ============================================================
 *  MINERIUM - Sistema de rastreabilidade de minerais críticos
 *  UNIP - Projeto Integrado Multidisciplinar (PIM)
 * ============================================================
 *  Versão conectada ao SQL Server via ODBC.
 *  Todos os dados ficam no banco "Minerium". Nada é guardado
 *  em vetores na memória.
 *
 *  ANTES DE RODAR
 *    1) Execute no SQL Server (SSMS ou Azure Data Studio):
 *         - banco.sql         (cria o banco e as tabelas)
 *         - SQLQuery1.sql     (índices, views, procedure e dados de teste)
 *    2) Ajuste a CONN_STR logo abaixo (servidor e driver).
 *
 *  COMPILAR (Windows, MinGW):
 *       gcc minerium.c -o minerium.exe -lodbc32 -Wall
 *  COMPILAR (Visual Studio, Developer Prompt):
 *       cl minerium.c odbc32.lib user32.lib
 *
 *  Salvar este arquivo como UTF-8.
 *
 *  Logins de teste (criados pelo SQLQuery1.sql):
 *    ana.souza    / Admin@123  (Administrador)
 *    carlos.lima  / Oper@123   (Operador)
 *    marina.alves / Sup@123    (Supervisor)
 *    paulo.rocha  / Cons@123   (Consulta)
 * ============================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <conio.h>
#include <windows.h>
#include <sql.h>
#include <sqlext.h>
#include <sqlucode.h>

/* ------------------------------------------------------------
 *  CONEXÃO COM O BANCO  (ajuste aqui)
 *
 *  Server=localhost\\SQLEXPRESS  -> troque pelo nome da sua instância
 *  Driver: se "ODBC Driver 17 for SQL Server" não estiver instalado,
 *          use "ODBC Driver 18 for SQL Server" ou "SQL Server".
 *
 *  Para usar login e senha do SQL Server em vez do login do Windows,
 *  troque "Trusted_Connection=yes;" por "UID=sa;PWD=suasenha;".
 * ------------------------------------------------------------ */
#define CONN_STR \
    "Driver={ODBC Driver 17 for SQL Server};" \
    "Server=localhost\\SQLEXPRESS;" \
    "Database=Minerium;" \
    "Trusted_Connection=yes;" \
    "TrustServerCertificate=yes;"

#define COR_RESET     "\033[0m"
#define COR_TITULO    "\033[1;34m"
#define COR_INFO      "\033[0;34m"
#define COR_SUCESSO   "\033[0;32m"
#define COR_ERRO      "\033[0;31m"
#define COR_AVISO     "\033[0;33m"
#define COR_DESTAQUE  "\033[1;33m"

/* ------------------------------------------------------------
 *  Status e etapas
 * ------------------------------------------------------------ */
#define TOTAL_STATUS 5
static const char *NOME_STATUS[TOTAL_STATUS] = {
    "Extraido", "Em processamento", "Armazenado", "Transportado", "Entregue" };
static const char *NOME_ETAPA[TOTAL_STATUS] = {
    "Extracao", "Processamento", "Armazenamento", "Transporte", "Entrega" };
static const char *COR_STATUS[TOTAL_STATUS] = {
    "\033[0;33m", "\033[0;34m", "\033[0;34m", "\033[0;35m", "\033[0;32m" };

#define TOTAL_UNIDADES 4
static const char *NOME_UNIDADE[TOTAL_UNIDADES] = { "kg", "g", "t", "un" };

/* ------------------------------------------------------------
 *  Usuário logado e suas permissões
 * ------------------------------------------------------------ */
#define MAX_PERM 20
static int  g_id_func  = 0;
static int  g_id_perfil = 0;
static char g_nome[100]  = "";
static char g_login[50]  = "";
static char g_perfil[50] = "";
static char g_perms[MAX_PERM][50];
static int  g_nperm = 0;

/* ------------------------------------------------------------
 *  Handles do ODBC
 * ------------------------------------------------------------ */
static SQLHENV g_env = SQL_NULL_HENV;
static SQLHDBC g_dbc = SQL_NULL_HDBC;

/* ============================================================
 *  TEXTO E ENTRADA DE DADOS
 * ============================================================ */

/* conta caracteres UTF-8 (e não bytes), para alinhar texto com acento */
static int utf8Len(const char *s)
{
    int n = 0;
    for (; *s; s++)
        if (((unsigned char)*s & 0xC0) != 0x80) n++;
    return n;
}

static void printPad(const char *s, int larg)
{
    printf("%s", s);
    for (int i = utf8Len(s); i < larg; i++) putchar(' ');
    putchar(' ');
}

// largura das molduras e da área onde os textos são centralizados.

#define LARGURA 64

static void linhaBorda(void)
{
    for (int i = 0; i < LARGURA; i++) putchar('=');
    putchar('\n');
}

// imprime o texto centralizado na LARGURA (cor opcional)
static void centralizar(const char *txt, const char *cor)
{
    int esp = (LARGURA - utf8Len(txt)) / 2;
    if (esp < 0) esp = 0;
    for (int i = 0; i < esp; i++) putchar(' ');
    printf("%s%s%s\n", cor, txt, COR_RESET);
}

static void titulo(const char *txt)
{
    printf("\n" COR_TITULO);
    linhaBorda();
    centralizar(txt, COR_TITULO);
    printf(COR_TITULO);
    linhaBorda();
    printf(COR_RESET);
}

static void ok(const char *txt)   { printf(COR_SUCESSO "  [OK] %s\n" COR_RESET, txt); }
static void erro(const char *txt) { printf(COR_ERRO    "  [ERRO] %s\n" COR_RESET, txt); }

static void lerLinha(const char *msg, char *dest, int tam)
{
    printf("%s", msg);
    if (!fgets(dest, tam, stdin)) {      /* fim da entrada (Ctrl+Z): encerra */
        printf("\n");
        exit(0);
    }
    size_t n = strlen(dest);
    if (n > 0 && dest[n - 1] == '\n') dest[n - 1] = '\0';
    else { int c; while ((c = getchar()) != '\n' && c != EOF); }
    n = strlen(dest);
    if (n > 0 && dest[n - 1] == '\r') dest[n - 1] = '\0';
}

// lê senha mostrando asteriscos
static void lerSenha(const char *msg, char *dest, int tam)
{
    int n = 0, c;
    printf("%s", msg);
    fflush(stdout);
    while ((c = _getch()) != '\r' && c != '\n') {
        if (c == 8 || c == 127) {
            if (n > 0) { n--; printf("\b \b"); }
        } else if (c == 3) {
            printf("\n");
            exit(0);
        } else if (c == 0 || c == 224) {
            _getch();
        } else if (c >= 32 && n < tam - 1) {
            dest[n++] = (char)c;
            putchar('*');
        }
    }
    dest[n] = '\0';
    printf("\n");
}

static int lerInteiro(const char *msg, int min, int max)
{
    char buf[40], *fim;
    for (;;) {
        lerLinha(msg, buf, sizeof buf);
        long v = strtol(buf, &fim, 10);
        if (fim != buf && *fim == '\0' && v >= min && v <= max) return (int)v;
        printf(COR_ERRO "  Valor inválido. Digite um número de %d a %d.\n" COR_RESET, min, max);
    }
}

static double lerDecimal(const char *msg, double minimo, double maximo)
{
    char buf[40], *fim;
    for (;;) {
        lerLinha(msg, buf, sizeof buf);
        for (char *p = buf; *p; p++) if (*p == ',') *p = '.';
        double v = strtod(buf, &fim);
        if (fim != buf && *fim == '\0' && v >= minimo && v <= maximo) return v;
        printf(COR_ERRO "  Valor inválido (use um número entre %.2f e %.2f).\n" COR_RESET,
               minimo, maximo);
    }
}

static int confirmar(const char *msg)
{
    char buf[10];
    printf(COR_AVISO "%s (s/n): " COR_RESET, msg);
    if (!fgets(buf, sizeof buf, stdin)) return 0;
    return tolower((unsigned char)buf[0]) == 's';
}

static void pausa(void)
{
    char buf[10];
    printf(COR_INFO "\n  Pressione Enter para continuar..." COR_RESET);
    if (fgets(buf, sizeof buf, stdin)) { /* ignora */ }
}

static void dataHoje(char *dest)
{
    time_t t = time(NULL);
    struct tm *info = localtime(&t);
    if (info) strftime(dest, 11, "%Y-%m-%d", info);
    else snprintf(dest, 11, "2026-01-01");
}

// valida formato AAAA-MM-DD
static int dataValida(const char *d)
{
    if (strlen(d) != 10 || d[4] != '-' || d[7] != '-') return 0;
    for (int i = 0; i < 10; i++)
        if (i != 4 && i != 7 && !isdigit((unsigned char)d[i])) return 0;
    int mes = atoi(d + 5), dia = atoi(d + 8);
    return mes >= 1 && mes <= 12 && dia >= 1 && dia <= 31;
}

/* ============================================================
 *  CAMADA DE BANCO (ODBC)
 *  Os textos são enviados como Unicode (UTF-16) para os acentos
 *  funcionarem, e as consultas usam parâmetros "?" (evita SQL Injection).
 * ============================================================ */

#define MAX_PARAM 10
#define MAX_TXT   1024

typedef struct {
    SQLHSTMT   h;
    int        np;                         // parâmetros já ligados
    SQLWCHAR   txt[MAX_PARAM][MAX_TXT];
    SQLLEN     ind[MAX_PARAM];
    double     dbl[MAX_PARAM];
    SQLINTEGER inteiro[MAX_PARAM];
} Cmd;

static void utf8ParaW(const char *src, SQLWCHAR *dest, int max)
{
    if (MultiByteToWideChar(CP_UTF8, 0, src, -1, (LPWSTR)dest, max) == 0) dest[0] = 0;
}

static void wParaUtf8(const SQLWCHAR *src, char *dest, int tam)
{
    if (WideCharToMultiByte(CP_UTF8, 0, (LPCWSTR)src, -1, dest, tam, NULL, NULL) == 0)
        dest[0] = '\0';
}

static void mostrarErroODBC(SQLSMALLINT tipo, SQLHANDLE h)
{
    SQLWCHAR estado[8], msg[1024];
    SQLINTEGER nativo;
    SQLSMALLINT tam;
    char msgU[2200];
    SQLSMALLINT i = 1;
    while (SQLGetDiagRecW(tipo, h, i, estado, &nativo, msg, 1024, &tam) == SQL_SUCCESS) {
        wParaUtf8(msg, msgU, sizeof msgU);
        printf(COR_ERRO "  [ERRO NO BANCO] %s\n" COR_RESET, msgU);
        i++;
    }
}

static void cmdFechar(Cmd *c)
{
    if (c->h) {
        SQLFreeHandle(SQL_HANDLE_STMT, c->h);
        c->h = SQL_NULL_HSTMT;
    }
}

// prepara um comando SQL. Retorna 1 se deu certo.
static int cmdAbrir(Cmd *c, const char *sql)
{
    memset(c, 0, sizeof *c);
    if (!SQL_SUCCEEDED(SQLAllocHandle(SQL_HANDLE_STMT, g_dbc, &c->h))) {
        c->h = SQL_NULL_HSTMT;
        return 0;
    }
    if (!SQL_SUCCEEDED(SQLPrepare(c->h, (SQLCHAR *)sql, SQL_NTS))) {
        mostrarErroODBC(SQL_HANDLE_STMT, c->h);
        cmdFechar(c);
        return 0;
    }
    return 1;
}

// os parâmetros devem ser ligados na mesma ordem dos "?" do SQL
static void cmdTexto(Cmd *c, const char *s)
{
    if (!c->h || c->np >= MAX_PARAM) return;
    int i = c->np++;
    utf8ParaW(s, c->txt[i], MAX_TXT);
    c->ind[i] = SQL_NTS;
    int n = 0;
    while (c->txt[i][n]) n++;
    if (n == 0) n = 1;
    SQLBindParameter(c->h, (SQLUSMALLINT)(i + 1), SQL_PARAM_INPUT, SQL_C_WCHAR,
                     SQL_WVARCHAR, (SQLULEN)n, 0, c->txt[i], 0, &c->ind[i]);
}

static void cmdInt(Cmd *c, int v)
{
    if (!c->h || c->np >= MAX_PARAM) return;
    int i = c->np++;
    c->inteiro[i] = v;
    SQLBindParameter(c->h, (SQLUSMALLINT)(i + 1), SQL_PARAM_INPUT, SQL_C_SLONG,
                     SQL_INTEGER, 0, 0, &c->inteiro[i], 0, NULL);
}

static void cmdNum(Cmd *c, double v)
{
    if (!c->h || c->np >= MAX_PARAM) return;
    int i = c->np++;
    c->dbl[i] = v;
    SQLBindParameter(c->h, (SQLUSMALLINT)(i + 1), SQL_PARAM_INPUT, SQL_C_DOUBLE,
                     SQL_DECIMAL, 12, 2, &c->dbl[i], 0, NULL);
}

static int cmdExec(Cmd *c)
{
    if (!c->h) return 0;
    SQLRETURN r = SQLExecute(c->h);
    if (r == SQL_SUCCESS || r == SQL_SUCCESS_WITH_INFO || r == SQL_NO_DATA) return 1;
    mostrarErroODBC(SQL_HANDLE_STMT, c->h);
    return 0;
}

static int cmdProx(Cmd *c)
{
    if (!c->h) return 0;
    return SQL_SUCCEEDED(SQLFetch(c->h));
}

// ler as colunas sempre em ordem crescente
static void cmdTxtCol(Cmd *c, int col, char *dest, int tam)
{
    SQLWCHAR w[MAX_TXT];
    SQLLEN ind = 0;
    dest[0] = '\0';
    SQLRETURN r = SQLGetData(c->h, (SQLUSMALLINT)col, SQL_C_WCHAR, w, sizeof w, &ind);
    if (SQL_SUCCEEDED(r) && ind != SQL_NULL_DATA) wParaUtf8(w, dest, tam);
}

static int cmdIntCol(Cmd *c, int col)
{
    SQLINTEGER v = 0;
    SQLLEN ind = 0;
    SQLGetData(c->h, (SQLUSMALLINT)col, SQL_C_SLONG, &v, 0, &ind);
    return (ind == SQL_NULL_DATA) ? 0 : (int)v;
}

static double cmdDblCol(Cmd *c, int col)
{
    double v = 0;
    SQLLEN ind = 0;
    SQLGetData(c->h, (SQLUSMALLINT)col, SQL_C_DOUBLE, &v, 0, &ind);
    return (ind == SQL_NULL_DATA) ? 0 : v;
}

// executa e devolve o primeiro inteiro da primeira linha (ou -1 se falhar)
static int cmdContar(Cmd *c)
{
    if (!cmdExec(c) || !cmdProx(c)) return -1;
    return cmdIntCol(c, 1);
}

static int conectarBanco(void)
{
    SQLCHAR saida[1024];
    SQLSMALLINT tam;

    if (!SQL_SUCCEEDED(SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &g_env))) return 0;
    SQLSetEnvAttr(g_env, SQL_ATTR_ODBC_VERSION, (SQLPOINTER)SQL_OV_ODBC3, 0);
    if (!SQL_SUCCEEDED(SQLAllocHandle(SQL_HANDLE_DBC, g_env, &g_dbc))) return 0;

    SQLRETURN r = SQLDriverConnect(g_dbc, NULL, (SQLCHAR *)CONN_STR, SQL_NTS,
                                   saida, sizeof saida, &tam, SQL_DRIVER_NOPROMPT);
    if (!SQL_SUCCEEDED(r)) {
        mostrarErroODBC(SQL_HANDLE_DBC, g_dbc);
        return 0;
    }
    return 1;
}

static void desconectarBanco(void)
{
    if (g_dbc) { SQLDisconnect(g_dbc); SQLFreeHandle(SQL_HANDLE_DBC, g_dbc); }
    if (g_env) SQLFreeHandle(SQL_HANDLE_ENV, g_env);
}

// mostra o resultado de uma consulta em uma tabela tabela
static void mostrarTabela(const char *sql, const char *cab[], const int larg[], int n)
{
    Cmd c;
    char buf[300];
    int linhas = 0;

    if (!cmdAbrir(&c, sql) || !cmdExec(&c)) { cmdFechar(&c); return; }

    printf(COR_DESTAQUE "  ");
    for (int i = 0; i < n; i++) printPad(cab[i], larg[i]);
    printf(COR_RESET "\n");

    while (cmdProx(&c)) {
        printf("  ");
        for (int i = 0; i < n; i++) {
            cmdTxtCol(&c, i + 1, buf, sizeof buf);
            printPad(buf, larg[i]);
        }
        printf("\n");
        linhas++;
    }
    if (linhas == 0) printf(COR_AVISO "  Nenhum registro encontrado.\n" COR_RESET);
    cmdFechar(&c);
}

/* lista "id - nome" de uma consulta (colunas: id, nome) e pede a escolha.
   Retorna o id escolhido ou 0 se cancelou. */
static int escolherDaTabela(const char *sql, const char *rotulo, char *nome_out, int tam_nome)
{
    Cmd c;
    int ids[60], n = 0;
    char nomes[60][100];

    if (!cmdAbrir(&c, sql) || !cmdExec(&c)) { cmdFechar(&c); return 0; }
    printf(COR_INFO "  %s:\n" COR_RESET, rotulo);
    while (n < 60 && cmdProx(&c)) {
        ids[n] = cmdIntCol(&c, 1);
        cmdTxtCol(&c, 2, nomes[n], sizeof nomes[n]);
        printf("  [%d] %s\n", ids[n], nomes[n]);
        n++;
    }
    cmdFechar(&c);
    if (n == 0) { erro("Nenhum registro disponível."); return 0; }

    for (;;) {
        int id = lerInteiro("  Escolha o número (0 = cancelar): ", 0, 999999);
        if (id == 0) return 0;
        for (int i = 0; i < n; i++) {
            if (ids[i] == id) {
                if (nome_out) snprintf(nome_out, (size_t)tam_nome, "%s", nomes[i]);
                return id;
            }
        }
        erro("Opção inválida.");
    }
}

// UPDATE com 1 texto e 1 id:  "UPDATE ... SET x = ? WHERE id = ?"
static int atualizarTxt(const char *sql, const char *txt, int id)
{
    Cmd c;
    int r = 0;
    if (cmdAbrir(&c, sql)) {
        cmdTexto(&c, txt);
        cmdInt(&c, id);
        r = cmdExec(&c);
    }
    cmdFechar(&c);
    return r;
}

/* UPDATE com 1 número inteiro e 1 id */
static int atualizarInt(const char *sql, int valor, int id)
{
    Cmd c;
    int r = 0;
    if (cmdAbrir(&c, sql)) {
        cmdInt(&c, valor);
        cmdInt(&c, id);
        r = cmdExec(&c);
    }
    cmdFechar(&c);
    return r;
}

/* ============================================================
 *  PERMISSÕES (tabelas Permissao e Perfil_Permissao)
 * ============================================================ */

static void carregarPermissoes(void)
{
    Cmd c;
    g_nperm = 0;
    if (!cmdAbrir(&c, "SELECT pm.nome FROM dbo.Permissao pm "
                      "JOIN dbo.Perfil_Permissao pp ON pp.id_permissao = pm.id_permissao "
                      "WHERE pp.id_perfil = ?")) return;
    cmdInt(&c, g_id_perfil);
    if (cmdExec(&c)) {
        while (g_nperm < MAX_PERM && cmdProx(&c)) {
            cmdTxtCol(&c, 1, g_perms[g_nperm], sizeof g_perms[g_nperm]);
            g_nperm++;
        }
    }
    cmdFechar(&c);
}

static int temPermissao(const char *nome)
{
    for (int i = 0; i < g_nperm; i++)
        if (strcmp(g_perms[i], nome) == 0) return 1;
    return 0;
}

static int exigirPermissao(const char *nome)
{
    if (temPermissao(nome)) return 1;
    erro("Seu perfil não tem permissão para esta operação.");
    return 0;
}

/* ============================================================
 *  LOGIN
 * ============================================================ */

static int fazerLogin(void)
{
    titulo("MINERIUM");
    const char *sub = "Sistema de rastreabilidade de minerais críticos";
    centralizar(sub, COR_INFO);

    for (int tentativa = 1; tentativa <= 3; tentativa++) {
        char login[50], senha[50];
        int achou = 0;
        Cmd c;

        lerLinha("\n  Login: ", login, sizeof login);
        lerSenha("  Senha: ", senha, sizeof senha);

        if (!cmdAbrir(&c,
            "SELECT f.id_funcionario, f.nome, f.id_perfil, p.nome "
            "FROM dbo.Funcionario f JOIN dbo.Perfil p ON p.id_perfil = f.id_perfil "
            "WHERE f.login = ? AND f.status = 'Ativo' "
            "AND f.senha_hash = CONVERT(VARCHAR(255), HASHBYTES('SHA2_256', CAST(? AS VARCHAR(100))), 2)"))
            return 0;
        cmdTexto(&c, login);
        cmdTexto(&c, senha);
        if (!cmdExec(&c)) { cmdFechar(&c); return 0; }

        if (cmdProx(&c)) {
            g_id_func   = cmdIntCol(&c, 1);
            cmdTxtCol(&c, 2, g_nome, sizeof g_nome);
            g_id_perfil = cmdIntCol(&c, 3);
            cmdTxtCol(&c, 4, g_perfil, sizeof g_perfil);
            snprintf(g_login, sizeof g_login, "%s", login);
            achou = 1;
        }
        cmdFechar(&c);

        if (achou) {
            carregarPermissoes();
            printf(COR_SUCESSO "\n  Bem-vindo(a), %s! (%s)\n" COR_RESET, g_nome, g_perfil);
            return 1;
        }
        printf(COR_ERRO "  [ERRO] Login ou senha incorretos (%d/3).\n" COR_RESET, tentativa);
    }
    return 0;
}

/* ============================================================
 *  LOTES E RASTREABILIDADE
 * ============================================================ */

static int indiceStatus(const char *s)
{
    for (int i = 0; i < TOTAL_STATUS; i++)
        if (strcmp(NOME_STATUS[i], s) == 0) return i;
    return -1;
}

static int indiceEtapa(const char *s)
{
    for (int i = 0; i < TOTAL_STATUS; i++)
        if (strcmp(NOME_ETAPA[i], s) == 0) return i;
    return -1;
}

// aceita "LT-000001", "lt-1" ou apenas "1" e devolve "LT-000001"
static int normalizarCodigo(const char *txt, char *dest, size_t tam)
{
    const char *p = txt;
    while (*p == ' ') p++;
    if (toupper((unsigned char)p[0]) == 'L' && toupper((unsigned char)p[1]) == 'T' && p[2] == '-')
        p += 3;
    char *fim;
    long id = strtol(p, &fim, 10);
    if (fim == p || id <= 0 || id > 999999) return 0;
    snprintf(dest, tam, "LT-%06ld", id);
    return 1;
}

static int lerCodigoLote(char *codigo, size_t tam)
{
    char cod[40];
    lerLinha("  Código do lote (ex: LT-000001): ", cod, sizeof cod);
    if (!normalizarCodigo(cod, codigo, tam)) { erro("Código inválido."); return 0; }
    return 1;
}

static void listarLotes(void)
{
    if (!exigirPermissao("CONSULTAR_RASTREABILIDADE")) return;
    titulo("LOTES CADASTRADOS");
    const char *cab[] = { "Código", "Quantidade", "Un.", "Status", "Mineral", "Mina" };
    const int larg[]  = { 10, 12, 4, 17, 10, 20 };
    mostrarTabela(
        "SELECT l.codigo_lote, l.quantidade, l.unidade_medida, l.status, mn.nome, mi.nome "
        "FROM dbo.Lote l "
        "JOIN dbo.Mineral mn ON mn.id_mineral = l.id_mineral "
        "JOIN dbo.Mina mi ON mi.id_mina = l.id_mina "
        "ORDER BY l.id_lote",
        cab, larg, 6);
}

static void cadastrarLote(void)
{
    if (!exigirPermissao("CADASTRAR_LOTE")) return;
    titulo("CADASTRAR LOTE");

    char nome_mina[100];
    int id_mina = escolherDaTabela(
        "SELECT id_mina, nome FROM dbo.Mina WHERE status = 'Ativa' ORDER BY id_mina",
        "Mina de origem", nome_mina, sizeof nome_mina);
    if (id_mina == 0) return;

    int id_mineral = escolherDaTabela(
        "SELECT id_mineral, nome FROM dbo.Mineral ORDER BY id_mineral",
        "Mineral", NULL, 0);
    if (id_mineral == 0) return;

    double qtd = lerDecimal("  Quantidade (> 0): ", 0.01, 9999999999.0);

    for (int i = 0; i < TOTAL_UNIDADES; i++) printf("  [%d] %s\n", i + 1, NOME_UNIDADE[i]);
    int u = lerInteiro("  Unidade de medida: ", 1, TOTAL_UNIDADES);
    const char *unidade = NOME_UNIDADE[u - 1];

    char data[40];
    for (;;) {
        lerLinha("  Data de extração AAAA-MM-DD (Enter = hoje): ", data, sizeof data);
        if (data[0] == '\0') { dataHoje(data); break; }
        if (dataValida(data)) break;
        erro("Data inválida.");
    }

    // 1) insere o lote. O código (LT-000001...) é gerado pelo próprio banco
    Cmd c;
    char codigo[20] = "";
    if (!cmdAbrir(&c,
        "INSERT INTO dbo.Lote (id_mina, id_mineral, id_funcionario, quantidade, unidade_medida, data_extracao) "
        "OUTPUT INSERTED.codigo_lote "
        "VALUES (?, ?, ?, ?, ?, CAST(? AS DATE))")) return;
    cmdInt(&c, id_mina);
    cmdInt(&c, id_mineral);
    cmdInt(&c, g_id_func);
    cmdNum(&c, qtd);
    cmdTexto(&c, unidade);
    cmdTexto(&c, data);
    if (cmdExec(&c) && cmdProx(&c)) cmdTxtCol(&c, 1, codigo, sizeof codigo);
    cmdFechar(&c);

    if (codigo[0] == '\0') { erro("Não foi possível cadastrar o lote."); return; }

    // 2 - primeira linha do histórico: a própria extração
    if (cmdAbrir(&c, "EXEC dbo.sp_registrar_etapa ?, ?, ?, ?, ?, ?, ?, ?")) {
        cmdTexto(&c, codigo);
        cmdTexto(&c, g_login);
        cmdTexto(&c, "Extracao");
        cmdTexto(&c, "Extraido");
        cmdTexto(&c, nome_mina);
        cmdTexto(&c, "Cadastro do lote / extração do minério bruto");
        cmdNum(&c, qtd);
        cmdNum(&c, 0);
        cmdExec(&c);
    }
    cmdFechar(&c);

    printf(COR_SUCESSO "  [OK] Lote cadastrado! Código gerado: " COR_DESTAQUE "%s\n" COR_RESET, codigo);
}

static void mostrarFicha(const char *codigo)
{
    Cmd c;
    char mina[100], mineral[100], qtd[40], un[10], status[40], dt_ext[20], resp[100];
    char dt[30], etapa[40], local[120], desc[1000], mov[40], perda[40], resp_et[100];
    int primeira = 1, tem_hist = 0;

    if (!cmdAbrir(&c,
        "SELECT mina_origem, mineral, quantidade_atual, unidade_medida, status_atual, "
        "       CONVERT(VARCHAR(10), data_extracao, 23), responsavel_cadastro, "
        "       CONVERT(VARCHAR(19), data_hora, 120), etapa, [local], descricao_atividade, "
        "       qtd_movimentada, qtd_perda, responsavel_etapa "
        "FROM dbo.vw_rastreabilidade_lote WHERE codigo_lote = ? ORDER BY data_hora")) return;
    cmdTexto(&c, codigo);
    if (!cmdExec(&c)) { cmdFechar(&c); return; }

    while (cmdProx(&c)) {
        cmdTxtCol(&c, 1, mina, sizeof mina);
        cmdTxtCol(&c, 2, mineral, sizeof mineral);
        cmdTxtCol(&c, 3, qtd, sizeof qtd);
        cmdTxtCol(&c, 4, un, sizeof un);
        cmdTxtCol(&c, 5, status, sizeof status);
        cmdTxtCol(&c, 6, dt_ext, sizeof dt_ext);
        cmdTxtCol(&c, 7, resp, sizeof resp);
        cmdTxtCol(&c, 8, dt, sizeof dt);
        cmdTxtCol(&c, 9, etapa, sizeof etapa);
        cmdTxtCol(&c, 10, local, sizeof local);
        cmdTxtCol(&c, 11, desc, sizeof desc);
        cmdTxtCol(&c, 12, mov, sizeof mov);
        cmdTxtCol(&c, 13, perda, sizeof perda);
        cmdTxtCol(&c, 14, resp_et, sizeof resp_et);

        if (primeira) {
            int s = indiceStatus(status);
            titulo(codigo);
            printf(COR_DESTAQUE "  Mineral:        " COR_RESET "%s\n", mineral);
            printf(COR_DESTAQUE "  Mina de origem: " COR_RESET "%s\n", mina);
            printf(COR_DESTAQUE "  Quantidade:     " COR_RESET "%s %s\n", qtd, un);
            printf(COR_DESTAQUE "  Status atual:   " COR_RESET "%s%s" COR_RESET "\n",
                   s >= 0 ? COR_STATUS[s] : "", status);
            printf(COR_DESTAQUE "  Data extração:  " COR_RESET "%s\n", dt_ext);
            printf(COR_DESTAQUE "  Cadastrado por: " COR_RESET "%s\n", resp);
            printf(COR_TITULO "\n  --- Histórico / rastreabilidade ---\n" COR_RESET);
            primeira = 0;
        }
        if (etapa[0] != '\0') {
            int e = indiceEtapa(etapa);
            tem_hist = 1;
            printf("  %s | %s%-13s" COR_RESET " | %s\n", dt, e >= 0 ? COR_STATUS[e] : "", etapa, local);
            printf(COR_INFO "     %s\n     movimentado: %s | perda: %s | resp.: %s\n" COR_RESET,
                   desc, mov, perda, resp_et);
        }
    }
    cmdFechar(&c);

    if (primeira) erro("Lote não encontrado.");
    else if (!tem_hist) printf(COR_AVISO "  Sem registros no histórico.\n" COR_RESET);
}

static void consultarLote(void)
{
    if (!exigirPermissao("CONSULTAR_RASTREABILIDADE")) return;
    char codigo[20];
    if (!lerCodigoLote(codigo, sizeof codigo)) return;
    mostrarFicha(codigo);
}

static void registrarEtapa(void)
{
    if (!exigirPermissao("REGISTRAR_ETAPA")) return;
    titulo("REGISTRAR ETAPA / MOVIMENTAÇÃO");

    char codigo[20];
    if (!lerCodigoLote(codigo, sizeof codigo)) return;

    /* busca o status e a quantidade atuais do lote */
    Cmd c;
    char status[40] = "", un[10] = "";
    double qtd_atual = 0;
    int achou = 0;
    if (!cmdAbrir(&c, "SELECT status, quantidade, unidade_medida FROM dbo.Lote WHERE codigo_lote = ?")) return;
    cmdTexto(&c, codigo);
    if (cmdExec(&c) && cmdProx(&c)) {
        cmdTxtCol(&c, 1, status, sizeof status);
        qtd_atual = cmdDblCol(&c, 2);
        cmdTxtCol(&c, 3, un, sizeof un);
        achou = 1;
    }
    cmdFechar(&c);
    if (!achou) { erro("Lote não encontrado."); return; }

    int s = indiceStatus(status);
    printf("  Status atual: %s%s" COR_RESET " | Quantidade: %.2f %s\n",
           s >= 0 ? COR_STATUS[s] : "", status, qtd_atual, un);
    if (s < 0) { erro("Status desconhecido no banco."); return; }
    if (s == TOTAL_STATUS - 1) { erro("Lote já foi entregue."); return; }

    printf(COR_INFO "  Etapas disponíveis:\n" COR_RESET);
    for (int i = s + 1; i < TOTAL_STATUS; i++) printf("  [%d] %s\n", i + 1, NOME_ETAPA[i]);
    int e = lerInteiro("  Etapa: ", s + 2, TOTAL_STATUS) - 1;

    char local[100], desc[300];
    do { lerLinha("  Local: ", local, sizeof local); } while (local[0] == '\0');
    lerLinha("  Descrição da atividade: ", desc, sizeof desc);
    double mov   = lerDecimal("  Quantidade movimentada (>= 0): ", 0, 9999999999.0);
    double perda = lerDecimal("  Quantidade perdida (>= 0): ", 0, 9999999999.0);
    if (perda >= qtd_atual) {
        erro("A perda não pode ser maior ou igual à quantidade atual.");
        return;
    }

    // a procedure grava o histórico e atualiza o lote
    if (!cmdAbrir(&c, "EXEC dbo.sp_registrar_etapa ?, ?, ?, ?, ?, ?, ?, ?")) return;
    cmdTexto(&c, codigo);
    cmdTexto(&c, g_login);
    cmdTexto(&c, NOME_ETAPA[e]);
    cmdTexto(&c, NOME_STATUS[e]);
    cmdTexto(&c, local);
    cmdTexto(&c, desc);
    cmdNum(&c, mov);
    cmdNum(&c, perda);
    if (cmdExec(&c)) ok("Etapa registrada e status atualizado!");
    cmdFechar(&c);
}

static void menuLotes(void)
{
    int op;
    do {
        titulo("LOTES E RASTREABILIDADE");
        printf("  [1] Cadastrar lote\n"
               "  [2] Listar lotes\n"
               "  [3] Consultar ficha do lote\n"
               "  [4] Registrar etapa/movimentação\n"
               "  [0] Voltar\n");
        op = lerInteiro("  Opção: ", 0, 4);
        switch (op) {
            case 1: cadastrarLote();  break;
            case 2: listarLotes();    break;
            case 3: consultarLote();  break;
            case 4: registrarEtapa(); break;
        }
        if (op != 0) pausa();
    } while (op != 0);
}

/* ============================================================
 *  USUÁRIOS (Funcionario / Perfil)
 * ============================================================ */

#define SQL_HASH_PARAM "CONVERT(VARCHAR(255), HASHBYTES('SHA2_256', CAST(? AS VARCHAR(100))), 2)"

static void listarUsuarios(void)
{
    titulo("USUÁRIOS CADASTRADOS");
    const char *cab[] = { "ID", "Nome", "Login", "Perfil", "Status" };
    const int larg[]  = { 4, 24, 16, 14, 8 };
    mostrarTabela(
        "SELECT f.id_funcionario, f.nome, f.login, p.nome, f.status "
        "FROM dbo.Funcionario f JOIN dbo.Perfil p ON p.id_perfil = f.id_perfil "
        "ORDER BY f.id_funcionario",
        cab, larg, 5);
}

static int contarAdminsAtivos(void)
{
    Cmd c;
    int n = -1;
    if (cmdAbrir(&c,
        "SELECT COUNT(*) FROM dbo.Funcionario f JOIN dbo.Perfil p ON p.id_perfil = f.id_perfil "
        "WHERE p.nome = 'Administrador' AND f.status = 'Ativo'"))
        n = cmdContar(&c);
    cmdFechar(&c);
    return n;
}

static int dadosUsuario(int id, char *perfil, int tp, char *status, int ts)
{
    Cmd c;
    int achou = 0;
    if (cmdAbrir(&c,
        "SELECT p.nome, f.status FROM dbo.Funcionario f "
        "JOIN dbo.Perfil p ON p.id_perfil = f.id_perfil WHERE f.id_funcionario = ?")) {
        cmdInt(&c, id);
        if (cmdExec(&c) && cmdProx(&c)) {
            cmdTxtCol(&c, 1, perfil, tp);
            cmdTxtCol(&c, 2, status, ts);
            achou = 1;
        }
    }
    cmdFechar(&c);
    return achou;
}

static void cadastrarUsuario(void)
{
    if (!exigirPermissao("CADASTRAR_FUNCIONARIO")) return;
    titulo("CADASTRAR USUÁRIO");

    char nome[100], login[50], senha[50];
    Cmd c;

    do { lerLinha("  Nome completo: ", nome, sizeof nome); } while (nome[0] == '\0');

    for (;;) {
        lerLinha("  Login (sem espaços): ", login, sizeof login);
        if (login[0] == '\0' || strchr(login, ' ')) { erro("Login inválido."); continue; }
        int n = -1;
        if (cmdAbrir(&c, "SELECT COUNT(*) FROM dbo.Funcionario WHERE login = ?")) {
            cmdTexto(&c, login);
            n = cmdContar(&c);
        }
        cmdFechar(&c);
        if (n < 0) return;
        if (n > 0) { erro("Esse login já existe."); continue; }
        break;
    }

    for (;;) {
        lerSenha("  Senha (mínimo 6 caracteres): ", senha, sizeof senha);
        if (strlen(senha) >= 6) break;
        erro("Senha muito curta.");
    }

    int id_perfil = escolherDaTabela(
        "SELECT id_perfil, nome FROM dbo.Perfil ORDER BY id_perfil", "Perfil", NULL, 0);
    if (id_perfil == 0) return;

    if (cmdAbrir(&c,
        "INSERT INTO dbo.Funcionario (id_perfil, nome, login, senha_hash) "
        "VALUES (?, ?, ?, " SQL_HASH_PARAM ")")) {
        cmdInt(&c, id_perfil);
        cmdTexto(&c, nome);
        cmdTexto(&c, login);
        cmdTexto(&c, senha);
        if (cmdExec(&c)) ok("Usuário cadastrado com sucesso!");
    }
    cmdFechar(&c);
}

static void alterarUsuario(void)
{
    if (!exigirPermissao("CADASTRAR_FUNCIONARIO")) return;
    titulo("ALTERAR USUÁRIO");
    listarUsuarios();

    int id = lerInteiro("\n  ID do usuário (0 = cancelar): ", 0, 999999);
    if (id == 0) return;

    char perfil[50], status[20];
    if (!dadosUsuario(id, perfil, sizeof perfil, status, sizeof status)) {
        erro("Usuário não encontrado.");
        return;
    }
    int ativo = strcmp(status, "Ativo") == 0;
    int adminAtivo = ativo && strcmp(perfil, "Administrador") == 0;

    printf("\n  [1] Alterar nome\n"
           "  [2] Redefinir senha\n"
           "  [3] Alterar perfil\n"
           "  [4] %s\n"
           "  [0] Cancelar\n", ativo ? "Inativar usuário" : "Reativar usuário");
    int op = lerInteiro("  Opção: ", 0, 4);

    if (op == 1) {
        char nome[100];
        do { lerLinha("  Novo nome: ", nome, sizeof nome); } while (nome[0] == '\0');
        if (atualizarTxt("UPDATE dbo.Funcionario SET nome = ? WHERE id_funcionario = ?", nome, id))
            ok("Nome atualizado.");
    } else if (op == 2) {
        char senha[50];
        for (;;) {
            lerSenha("  Nova senha (mínimo 6 caracteres): ", senha, sizeof senha);
            if (strlen(senha) >= 6) break;
            erro("Senha muito curta.");
        }
        if (atualizarTxt("UPDATE dbo.Funcionario SET senha_hash = " SQL_HASH_PARAM
                         " WHERE id_funcionario = ?", senha, id))
            ok("Senha redefinida.");
    } else if (op == 3) {
        if (adminAtivo && contarAdminsAtivos() <= 1) {
            erro("Não é possível tirar o perfil do único administrador.");
            return;
        }
        int id_perfil = escolherDaTabela(
            "SELECT id_perfil, nome FROM dbo.Perfil ORDER BY id_perfil", "Novo perfil", NULL, 0);
        if (id_perfil == 0) return;
        if (atualizarInt("UPDATE dbo.Funcionario SET id_perfil = ? WHERE id_funcionario = ?",
                         id_perfil, id))
            ok("Perfil atualizado.");
    } else if (op == 4) {
        if (id == g_id_func) { erro("Você não pode inativar o usuário logado."); return; }
        if (adminAtivo && contarAdminsAtivos() <= 1) {
            erro("Não é possível inativar o único administrador.");
            return;
        }
        /* exclusão lógica: preserva o histórico dos lotes (chaves estrangeiras) */
        if (confirmar(ativo ? "  Confirmar inativação" : "  Confirmar reativação")) {
            if (atualizarTxt("UPDATE dbo.Funcionario SET status = ? WHERE id_funcionario = ?",
                             ativo ? "Inativo" : "Ativo", id))
                ok(ativo ? "Usuário inativado." : "Usuário reativado.");
        }
    }
}

static void menuUsuarios(void)
{
    int op;
    do {
        titulo("GESTÃO DE USUÁRIOS");
        printf("  [1] Cadastrar usuário\n"
               "  [2] Listar usuários\n"
               "  [3] Alterar / inativar usuário\n"
               "  [0] Voltar\n");
        op = lerInteiro("  Opção: ", 0, 3);
        switch (op) {
            case 1: cadastrarUsuario(); break;
            case 2: listarUsuarios();   break;
            case 3: alterarUsuario();   break;
        }
        if (op != 0) pausa();
    } while (op != 0);
}

/* ============================================================
 *  CLIENTES INDUSTRIAIS
 * ============================================================ */

static void listarClientes(void)
{
    if (!exigirPermissao("CADASTRAR_CLIENTE")) return;
    titulo("CLIENTES INDUSTRIAIS");
    const char *cab[] = { "ID", "Razão social", "CNPJ", "Contato", "E-mail", "Status" };
    const int larg[]  = { 4, 26, 15, 16, 32, 8 };
    mostrarTabela(
        "SELECT id_cliente, razao_social, cnpj, contato, email, status "
        "FROM dbo.Cliente ORDER BY id_cliente",
        cab, larg, 6);
}

static void cadastrarCliente(void)
{
    if (!exigirPermissao("CADASTRAR_CLIENTE")) return;
    titulo("CADASTRAR CLIENTE INDUSTRIAL");

    char razao[150], cnpj_in[40], cnpj[20], contato[100], email[120], senha[50];
    Cmd c;

    do { lerLinha("  Razão social: ", razao, sizeof razao); } while (razao[0] == '\0');

    for (;;) {
        lerLinha("  CNPJ (14 dígitos): ", cnpj_in, sizeof cnpj_in);
        int n = 0;
        for (char *p = cnpj_in; *p; p++)
            if (isdigit((unsigned char)*p) && n < 19) cnpj[n++] = *p;
        cnpj[n] = '\0';
        if (n == 14) break;
        erro("O CNPJ deve ter 14 dígitos.");
    }

    do { lerLinha("  Nome do contato: ", contato, sizeof contato); } while (contato[0] == '\0');

    for (;;) {
        lerLinha("  E-mail: ", email, sizeof email);
        if (strchr(email, '@') && strchr(email, '.')) break;
        erro("E-mail inválido.");
    }

    int n = -1;
    if (cmdAbrir(&c, "SELECT COUNT(*) FROM dbo.Cliente WHERE cnpj = ? OR email = ?")) {
        cmdTexto(&c, cnpj);
        cmdTexto(&c, email);
        n = cmdContar(&c);
    }
    cmdFechar(&c);
    if (n < 0) return;
    if (n > 0) { erro("Já existe cliente com esse CNPJ ou e-mail."); return; }

    for (;;) {
        lerSenha("  Senha inicial do cliente (mínimo 6): ", senha, sizeof senha);
        if (strlen(senha) >= 6) break;
        erro("Senha muito curta.");
    }

    if (cmdAbrir(&c,
        "INSERT INTO dbo.Cliente (razao_social, cnpj, contato, email, senha_hash) "
        "VALUES (?, ?, ?, ?, " SQL_HASH_PARAM ")")) {
        cmdTexto(&c, razao);
        cmdTexto(&c, cnpj);
        cmdTexto(&c, contato);
        cmdTexto(&c, email);
        cmdTexto(&c, senha);
        if (cmdExec(&c)) ok("Cliente cadastrado com sucesso!");
    }
    cmdFechar(&c);
}

static void menuClientes(void)
{
    int op;
    do {
        titulo("CLIENTES INDUSTRIAIS");
        printf("  [1] Cadastrar cliente\n"
               "  [2] Listar clientes\n"
               "  [0] Voltar\n");
        op = lerInteiro("  Opção: ", 0, 2);
        switch (op) {
            case 1: cadastrarCliente(); break;
            case 2: listarClientes();   break;
        }
        if (op != 0) pausa();
    } while (op != 0);
}

/* ============================================================
 *  RELATÓRIOS (usam as views do banco)
 * ============================================================ */

static void relatorioOrigem(void)
{
    titulo("ORIGEM DOS MATERIAIS");
    const char *cab[] = { "Mina", "Mineral", "Un.", "Lotes", "Quantidade total" };
    const int larg[]  = { 20, 10, 4, 6, 16 };
    mostrarTabela("SELECT mina, mineral, unidade_medida, qtd_lotes, quantidade_total "
                  "FROM dbo.vw_origem_materiais ORDER BY mina, mineral",
                  cab, larg, 5);
}

static void relatorioEntregas(void)
{
    titulo("ENTREGAS");
    const char *cab[] = { "Cliente", "Nota fiscal", "Produto", "Qtd.", "Un.", "Lote",
                          "Status", "Data", "Responsável" };
    const int larg[]  = { 22, 11, 18, 9, 4, 10, 13, 10, 14 };
    mostrarTabela(
        "SELECT cliente, numero_nota_fiscal, produto, quantidade_entregue, unidade_medida, "
        "       lote_origem, status_entrega, ISNULL(CONVERT(VARCHAR(10), data_entrega, 23), '-'), "
        "       responsavel_entrega "
        "FROM dbo.vw_entregas_realizadas ORDER BY id_entrega",
        cab, larg, 9);
}

static void relatorioLotesPorStatus(void)
{
    titulo("LOTES POR STATUS");
    const char *cab[] = { "Status", "Lotes", "Quantidade (unidades mistas)" };
    const int larg[]  = { 17, 6, 28 };
    mostrarTabela("SELECT status, COUNT(*), SUM(quantidade) FROM dbo.Lote GROUP BY status",
                  cab, larg, 3);
}

static void menuRelatorios(void)
{
    if (!exigirPermissao("GERAR_RELATORIO")) { pausa(); return; }
    int op;
    do {
        titulo("RELATÓRIOS");
        printf("  [1] Origem dos materiais\n"
               "  [2] Entregas realizadas\n"
               "  [3] Lotes por status\n"
               "  [0] Voltar\n");
        op = lerInteiro("  Opção: ", 0, 3);
        switch (op) {
            case 1: relatorioOrigem();        break;
            case 2: relatorioEntregas();      break;
            case 3: relatorioLotesPorStatus(); break;
        }
        if (op != 0) pausa();
    } while (op != 0);
}

/* ============================================================
 *  MENU PRINCIPAL E MAIN
 * ============================================================ */

static void menuPrincipal(void)
{
    int op;
    do {
        titulo("MENU PRINCIPAL");
        char info[200];
        snprintf(info, sizeof info, "Usuário: %s | Perfil: %s", g_nome, g_perfil);
        centralizar(info, COR_INFO);
        printf("\n");

        printf("  [1] Lotes e rastreabilidade\n"
               "  [2] Clientes industriais\n"
               "  [3] Relatórios\n"
               "  [4] Gestão de usuários\n"
               "  [0] Sair\n");
        op = lerInteiro("  Opção: ", 0, 4);

        switch (op) {
            case 1: menuLotes();      break;
            case 2:
                if (exigirPermissao("CADASTRAR_CLIENTE")) menuClientes();
                else pausa();
                break;
            case 3: menuRelatorios(); break;
            case 4:
                if (exigirPermissao("CADASTRAR_FUNCIONARIO")) menuUsuarios();
                else pausa();
                break;
        }
    } while (op != 0);
}

int main(void)
{
    SetConsoleOutputCP(CP_UTF8);           // acentos no terminal do Windows
    SetConsoleCP(CP_UTF8);
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD modo = 0;
    if (GetConsoleMode(h, &modo))          // ativa as cores
        SetConsoleMode(h, modo | 0x0004);

    printf("Conectando ao banco de dados...\n");
    if (!conectarBanco()) {
        printf(COR_ERRO "\n  Não foi possível conectar ao SQL Server.\n"
               "  Verifique a CONN_STR (servidor e driver) e se o banco Minerium existe.\n" COR_RESET);
        desconectarBanco();
        return 1;
    }

    if (!fazerLogin()) {
        printf(COR_ERRO "\n  Acesso negado. Encerrando.\n" COR_RESET);
        desconectarBanco();
        return 1;
    }

    menuPrincipal();
    desconectarBanco();
    printf(COR_SUCESSO "\n  Até logo!\n" COR_RESET);
    return 0;
}
