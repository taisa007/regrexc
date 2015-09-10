/* Page 312 */

/* List 19.1  正規表現のパターンマッチ */

/*
        DFAによる正規表現のパターンマッチ
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>



#define DEBUG   1

/*************************************************************
        下請けルーチン
 *************************************************************/

/* sizeバイトのメモリを割り当てて、0クリアして返す。
   割り当てに失敗したら、プログラムを停止する */
void *alloc_mem(int size)
{
    void *p;

    if ((p = malloc(size)) == NULL) {
        fprintf(stderr, "Out of memory.\n");
        exit(2);
    }
    memset(p, 0, size);
    return p;
}


/*************************************************************
        正規表現をパースして構文木を生成する

        次の構文で定義される正規表現を受理する

        <regexp>        ::= <term> | <term> "|" <regexp>
        <term>          ::= EMPTY  | <factor> <term>
        <factor>        ::= <primary> | <primary> "*" | <primary> "+"
        <primary>       ::=  CHARACTER | "(" <regexp> ")"
 *************************************************************/

/* token_tはトークンを表す型 */
typedef enum {
    tk_char,            /* 文字（実際の文字はtoken_charにセットされる） */
    tk_union,           /* '|' */
    tk_lpar,            /* '(' */
    tk_rpar,            /* ')' */
    tk_star,            /* '*' */
    tk_plus,            /* '+' */
    tk_end              /* 文字列の終わり */
} token_t;

/* 現在処理中のトークン */
token_t  current_token;

/* current_tokenがtk_charであるときの文字値 */
char  token_char;

/* 解析する文字列へのポインタ */
char *strbuff;

/* op_tはノードの操作を表す型 */
typedef enum {
    op_char,            /* 文字そのもの  */
    op_concat,          /* XY */
    op_union,           /* X|Y */
    op_closure,         /* X* */
    op_empty            /* 空 */
} op_t;

/* tree_tは構文木のノードを表す型 */
typedef struct tree {
    op_t   op;                      /* このノードの操作 */
    union {
                                    /* op == op_charのとき:     */
        char  c;                    /*    文字                  */
        struct {                    /* op == op_char以外のとき: */
            struct tree  *_left;    /*    左の子                */
            struct tree  *_right;   /*    右の子                */
        } x;
    } u;
} tree_t;


/* 構文エラーの後始末をする。
   実際には、メッセージを表示して、プログラムを停止する */
void syntax_error(char *s)
{
    fprintf(stderr, "Syntax error: %s\n", s);
    exit(1);
}

/* トークンを1つ読み込んで返す */
token_t get_token()
{
    char c;

    /* 文字列の終わりに到達したらtk_endを返す */
    if (*strbuff == '\0')
        current_token = tk_end;
    else {
    /* 文字をトークンに変換する */
        c = *strbuff++;
        switch (c) {
        case '|':       current_token = tk_union;       break;
        case '(':       current_token = tk_lpar;        break;
        case ')':       current_token = tk_rpar;        break;
        case '*':       current_token = tk_star;        break;
        case '+':       current_token = tk_plus;        break;
        default:        current_token = tk_char;
                        token_char = c;                 break;
        }
    }
    return current_token;
}

/* 正規表現を解析するパーサを初期化する */
void initialize_regexp_parser(char *str)
{
    strbuff = str;
    get_token();
}

/* 構文木のノードを作成する。
   opはノードが表す演算、leftは左の子、rightは右の子 */
tree_t *make_tree_node(op_t op, tree_t *left, tree_t *right)
{
    tree_t  *p;

    /* ノードを割り当てる */
    p = (tree_t *)alloc_mem(sizeof(tree_t));

    /* ノードに情報を設定する */
    p->op = op;
    p->u.x._left = left;
    p->u.x._right = right;
    return p;
}

/* 構文木の葉を作る。
   cはこの葉が表す文字 */
tree_t *make_atom(char c)
{
    tree_t  *p;

    /* 葉を割り当てる */
    p = (tree_t *)alloc_mem(sizeof(tree_t));

    /* 葉に情報を設定する */
    p->op = op_char;
    p->u.c = c;
    return p;
}

tree_t *regexp(), *term(), *factor(), *primary();

/* <regexp>をパースして、得られた構文木を返す。
   選択X|Yを解析する */
tree_t *regexp()
{
    tree_t  *x;

    x = term();
    while (current_token == tk_union) {
        get_token();
        x = make_tree_node(op_union, x, term());
    }
    return x;
}

/* <term>をパースして、得られた構文木を返す。
   連結XYを解析する */
tree_t *term()
{
    tree_t  *x;

    if (current_token == tk_union ||
        current_token == tk_rpar ||
        current_token == tk_end)
        x = make_tree_node(op_empty, NULL, NULL);
    else {
        x = factor();
        while (current_token != tk_union &&
               current_token != tk_rpar &&
               current_token != tk_end) {
            x = make_tree_node(op_concat, x, factor());
        }
    }
    return x;
}

/* <factor>をパースして、得られた構文木を返す。
   繰り返しX*、X+を解析する */
tree_t *factor()
{
    tree_t  *x;

    x = primary();
    if (current_token == tk_star) {
        x = make_tree_node(op_closure, x, NULL);
        get_token();
    } else if (current_token == tk_plus) {
        x = make_tree_node(op_concat, x, make_tree_node(op_closure, x, NULL));
        get_token();
    }
    return x;
}

/* <primary>をパースして、得られた構文木を返す。
   文字そのもの、(X)を解析する */
tree_t *primary()
{
    tree_t *x;

    if (current_token == tk_char) {
        x = make_atom(token_char);
        get_token();
    } else if (current_token == tk_lpar) {
        get_token();
        x = regexp();
        if (current_token != tk_rpar)
            syntax_error("Close paren is expected.");
        get_token();
    } else {
        syntax_error("Normal character or open paren is expected.");
    }
    return x;
}

#if     DEBUG
/* 構文木を表示する（デバッグ用） */
void dump_tree(tree_t *p)
{
    switch (p->op) {
    case op_char:
        printf("\"%c\"", p->u.c);
        break;
    case op_concat:
        printf("(concat ");
        dump_tree(p->u.x._left);
        printf(" ");
        dump_tree(p->u.x._right);
        printf(")");
        break;
    case op_union:
        printf("(or ");
        dump_tree(p->u.x._left);
        printf(" ");
        dump_tree(p->u.x._right);
        printf(")");
        break;
    case op_closure:
        printf("(closure ");
        dump_tree(p->u.x._left);
        printf(")");
        break;
    case op_empty:
        printf("EMPTY");
        break;
    default:
        fprintf(stderr, "This cannot happen in <dump_tree>\n");
        exit(2);
    }
}
#endif  /* DEBUG */


/* 正規表現をパースして、正規表現に対応する構文木を返す。
   strは正規表現が入っている文字列 */
tree_t  *parse_regexp(char *str)
{
    tree_t *t;

    /* パーサを初期化する */
    initialize_regexp_parser(str);

    /* 正規表現をパースする */
    t = regexp();

    /* 次のトークンがtk_endでなければエラー */
    if (current_token != tk_end)
        syntax_error("Extra character at end of pattern.");

#if     DEBUG
    /* 得られた構文木の内容を表示する */
    dump_tree(t);
    printf("\n");
#endif  /* DEBUG */

    /* 生成した構文木を返す */
    return t;
}


/*************************************************************
        構文木からNFAを生成する
 *************************************************************/

/* EMPTYはε遷移を表す */
#define EMPTY   -1

/* nlist_tはNFAにおける遷移を表す型。
   文字cによって状態toへと遷移する  */
typedef struct nlist {
    char  c;
    int   to;
    struct nlist  *next;        /* 次のデータへのポインタ */
} nlist_t;

/* NFAの状態数の上限 */
#define NFA_STATE_MAX   128

/* NFAの状態遷移表 */
nlist_t *nfa[NFA_STATE_MAX];

/* NFAの初期状態 */
int  nfa_entry;

/* NFAの終了状態 */
int  nfa_exit;

/* NFAの状態数 */
int  nfa_nstate = 0;

/* ノードに番号を割り当てる */
int gen_node()
{
    /* NFAの状態数の上限をチェックする */
    if (nfa_nstate >= NFA_STATE_MAX) {
        fprintf(stderr, "Too many NFA state.\n");
        exit(2);
    }

    return nfa_nstate++;
}

/* NFAに状態遷移を追加する。
   状態fromに対して、文字cのときに状態toへの遷移を追加する */
void add_transition(int from, int to, char c)
{
    nlist_t  *p;

    p = (nlist_t *)alloc_mem(sizeof(nlist_t));
    p->c = c;
    p->to = to;
    p->next = nfa[from];
    nfa[from] = p;
}

/* 構文木treeに対するNFAを生成する。
   NFAの入口をentry、出口をway_outとする */
void gen_nfa(tree_t *tree, int entry, int way_out)
{
    int  a1, a2;

    switch (tree->op) {
    case op_char:
        add_transition(entry, way_out, tree->u.c);
        break;
    case op_empty:
        add_transition(entry, way_out, EMPTY);
        break;
    case op_union:
        gen_nfa(tree->u.x._left, entry, way_out);
        gen_nfa(tree->u.x._right, entry, way_out);
        break;
    case op_closure:
        a1 = gen_node();
        a2 = gen_node();
        add_transition(entry, a1, EMPTY);
        gen_nfa(tree->u.x._left, a1, a2);
        add_transition(a2, a1, EMPTY);
        add_transition(a1, way_out, EMPTY);
        break;
    case op_concat:
        a1 = gen_node();
        gen_nfa(tree->u.x._left, entry, a1);
        gen_nfa(tree->u.x._right, a1, way_out);
        break;
    default:
        fprintf(stderr, "This cannot happen in <gen_nfa>\n");
        exit(2);
    }
}

/* 構文木treeに対応するNFAを生成する */
void build_nfa(tree_t *tree)
{
    /* NFAの初期状態のノードを割り当てる */
    nfa_entry = gen_node();

    /* NFAの終了状態のノードを割り当てる */
    nfa_exit  = gen_node();

    /* NFAを生成する */
    gen_nfa(tree, nfa_entry, nfa_exit);
}

#if     DEBUG
/* NFAを表示する（デバッグ用） */
void dump_nfa()
{
    int  i;
    nlist_t *p;

    for (i = 0; i <NFA_STATE_MAX; i++) {
        if (nfa[i] != NULL) {
            printf("state %3d: ", i);
            for (p = nfa[i]; p != NULL; p = p->next) {
                printf("(%c . %d) ", p->c == EMPTY ? '?' : p->c, p->to);
            }
            printf("\n");
        }
    }
}
#endif  /* DEBUG */


/*************************************************************
        NFAをDFAへ変換する
 *************************************************************/

/* NFA状態集合に必要なバイト数 */
#define NFA_VECTOR_SIZE (NFA_STATE_MAX/8)

/* N_state_set_tは、NFA状態集合を表す型。
   実際にはビットベクトルで表現されている */
typedef struct {
    unsigned char vec[NFA_VECTOR_SIZE];
} N_state_set_t;

/* NFA状態集合stateに状態sが含まれているか? */
#define check_N_state(state, s) ((state)->vec[(s)/8] & (1 << ((s)%8)))

/* dlist_tは遷移可能なNFA状態のリストを表す型。
   （関数compute_reachable_N_stateがこの型の値を返す）
   文字cによってNFA状態集合toへ遷移する */
typedef struct dlist {
    char        c;
    N_state_set_t       to;
    struct dlist *next;         /* 次のデータへのポインタ */
} dlist_t;

/* DFAの状態数の上限 */
#define DFA_STATE_MAX   100

/* dslist_tは遷移先のリストを表す型。
   文字cによって状態toへ遷移する */
typedef struct dslist {
    char               c;
    struct D_state_t   *to;
    struct dslist      *next;   /* 次のデータへのポインタ */
} dslist_t;

/* D_state_tはDFA状態を表す型 */
typedef struct D_state_t {
    N_state_set_t  *state;      /* このDFA状態が表すNFA状態集合 */
    int         visited;        /* 処理済みなら1 */
    int         accepted;       /* 終了状態を含むなら1 */
    dslist_t    *next;          /* 遷移先のリスト */
} D_state_t;

/* DFAの状態遷移表 */
D_state_t dfa[DFA_STATE_MAX];

/* DFAの初期状態 */
D_state_t *initial_dfa_state;

/* 現時点でのDFAの状態数 */
int  dfa_nstate = 0;

#if     DEBUG
/* NFA状能集合を表示する（デバッグ用） */
void dump_N_state_set(N_state_set_t *p)
{
    int  i;

    for (i = 0; i < nfa_nstate; i++) {
        if (check_N_state(p, i))
            printf("%d ", i);
    }
}

/* DFAを表示する（デバッグ用） */
void dump_dfa()
{
    dslist_t *l;
    int  i;

    for (i = 0; i < dfa_nstate; i++) {
        printf("%2d%c: ", i, dfa[i].accepted ? 'A' : ' ');
        for (l = dfa[i].next; l != NULL; l = l->next)
            printf("%c=>%d ", l->c, l->to - dfa);
        printf("\n");
    }

    for (i = 0; i < dfa_nstate; i++) {
        printf("state %2d%c = {", i, dfa[i].accepted ? 'A' : ' ');
        dump_N_state_set(dfa[i].state);
        printf("}\n");
    }
}
#endif  /* DEBUG */

/* NFA状態集合stateにNFA状態sを追加する */
void add_N_state(N_state_set_t *state, int s)
{
    state->vec[s/8] |= (1 << (s%8));
}

/* NFA状態集合stateにNFA状態sを追加する。
   同時にNFA状態sからε遷移で移動できるNFA状態も追加する */
void mark_empty_transition(N_state_set_t *state, int s)
{
    nlist_t  *p;

    add_N_state(state, s);
    for (p = nfa[s]; p != NULL; p = p->next) {
        if (p->c == EMPTY &&
            !check_N_state(state, p->to))
            mark_empty_transition(state, p->to);
    }
}

/* NFA状態集合stateに対してε-follow操作を実行する。
   ε遷移で遷移可能なすべてのNFA状態を追加する */
void collect_empty_transition(N_state_set_t *state)
{
    int  i;

    for (i = 0; i < nfa_nstate; i++) {
        if (check_N_state(state, i))
            mark_empty_transition(state, i);
    }
}

/* 2つのNFA状態集合aとbが等しいか調べる。
   等しければ1、等しくなければ0を返す */
int  equal_N_state_set(N_state_set_t *a, N_state_set_t *b)
{
    int  i;

    for (i = 0; i < NFA_VECTOR_SIZE; i++) {
        if (a->vec[i] != b->vec[i])
            return 0;
    }
    return 1;
}

/* NFA状態集合sをDFAに登録して、DFA状態へのポインタを返す。
   sが終了状態を含んでいれば、acceptedフラグをセットする。
   すでにsがDFAに登録されていたら何もしない */
D_state_t  *register_D_state(N_state_set_t *s)
{
    int  i;

    /* NFA状態sがすでにDFAに登録されていたら、何もしないでリターンする */
    for (i = 0; i < dfa_nstate; i++) {
        if (equal_N_state_set(dfa[i].state, s))
            return &dfa[i];
    }

    /* DFA状態数が多すぎないかチェック */
    if (dfa_nstate >= DFA_STATE_MAX) {
        fprintf(stderr, "Too many DFA state.\n");
        exit(2);
    }

    /* DFAに必要な情報をセットする */
    dfa[dfa_nstate].state = s;
    dfa[dfa_nstate].visited = 0;
    dfa[dfa_nstate].accepted = check_N_state(s, nfa_exit) ? 1 : 0;
    dfa[dfa_nstate].next  = NULL;
    return &dfa[dfa_nstate++];
}

/* 処理済みの印がついていないDFA状態を探す。
   見つからなければNULLを返す */
D_state_t *fetch_unvisited_D_state()
{
    int  i;

    for (i = 0; i < dfa_nstate; i++) {
        if (dfa[i].visited == 0)
            return &dfa[i];
    }
    return NULL;
}

/* DFA状態dstateから遷移可能なNFA状態を探して、リストにして返す */
dlist_t *compute_reachable_N_state(D_state_t *dstate)
{
    int  i;
    nlist_t  *p;
    dlist_t  *result, *a, *b;
    N_state_set_t *state;

    state = dstate->state;
    result = NULL;

    /* すべてのNFA状態を順に調べる */
    for (i = 0; i < nfa_nstate; i++) {

        /* NFA状態iがDFA状態dstateに含まれていれば、以下の処理を行う */
        if (check_N_state(state, i)) {

            /* NFA状態iから遷移可能なNFA状態を
               すべて調べてリストにする */
            for (p = nfa[i]; p != NULL; p = p->next) {
                if (p->c != EMPTY) { /* ただし、ε遷移は除外する */
                    for (a = result; a != NULL; a = a->next) {
                        if (a->c == p->c) {
                            add_N_state(&a->to, p->to);
                            goto added;
                        }
                    }
                    b = alloc_mem(sizeof(dlist_t));
                    b->c = p->c;
                    add_N_state(&b->to, p->to);
                    b->next = result;
                    result = b;
                added:
                    ;
                }
            }
        }
    }

    /* 作成したリストを返す */
    return result;
}

/* NFAを等価なDFAへと変換する */
void convert_nfa_to_dfa()
{
    N_state_set_t  *initial_state;
    D_state_t  *t;
    dlist_t  *x;
    dslist_t  *p;

    /* DFAの初期状態を登録する */
    initial_state = alloc_mem(sizeof(N_state_set_t));
    add_N_state(initial_state, nfa_entry);
    collect_empty_transition(initial_state);
    initial_dfa_state = register_D_state(initial_state);

    /* 未処理のDFA状態があれば、それを取り出して処理する。
       （注目しているDFA状態をtとする） */
    while ((t = fetch_unvisited_D_state()) != NULL ) {

        /* 処理済みの印をつける */
        t->visited = 1;

        /* 状態tから遷移可能なDFA状態をすべてDFAに登録する。
           xは(文字, NFA状態集合)の対になっている */
        for (x = compute_reachable_N_state(t);
             x != NULL; x = x->next) {

            /* NFA状態集合のε-closureを求める */
            collect_empty_transition(&x->to);
            p = alloc_mem(sizeof(dslist_t));
            p->c = x->c;

            /* NFA状態集合をDFAに登録する */
            p->to = register_D_state(&x->to);
            p->next = t->next;
            t->next = p;
        }
    }

#if     DEBUG
    printf("--- DFA -----\n");
    dump_dfa();
#endif  /* DEBUG */
}

/*************************************************************
        DFAを使ってパターンマッチを行う
 *************************************************************/

/* 状態stateから文字cによって遷移して、遷移後の状態を返す。
   文字cによって遷移できなければNULLを返す */
D_state_t *next_dfa_state(D_state_t *state, char c)
{
    dslist_t  *p;

    for (p = state->next; p != NULL; p = p->next) {
        if (c == p->c)
            return p->to;
    }
    return NULL;
}

/* DFAを使って文字列stringに対してパターンマッチを行う。
   from: マッチした部分の先頭へのポインタを返す。
   to:   マッチした部分の直後へのポインタを返す。
   パターンマッチに失敗したらfromにNULLをセットする */
void do_match(char *string, char **from, char **to)
{
    char *start;
    char *p, *max_match;
    D_state_t  *state;

    /* 文字列stringの先頭から、1文字ずつずらしてパターンマッチする */
    for (start = string; *start != '\0'; start++) {

        /* DFAを初期状態にする */
        state = initial_dfa_state;
        max_match = NULL;
        p = start;

        /* DFAが遷移できる限り繰り返す */
        while (state != NULL) {

            /* もし終了状態であれば場所を記録しておく。
               結果として、最も長くパターンにマッチした部分が記録される */
            if (state->accepted)
                max_match = p;

            /* 文字列stringの末尾までマッチした */
            if (*p == '\0')
                break;

            /* DFAを状態遷移させる */
            state = next_dfa_state(state, *p++);
        }

        /* パターンマッチが成功していれば、fromとtoに位置を
           セットしてリターンする。
           （ただし、空文字列にマッチした場合は、リターンしない） */
        if (max_match != NULL && max_match != start) {
            *from = start;
            *to   = max_match;
            return;
        }
    }

    /* パターンマッチに失敗したのでfromにNULLをセット */
    *from = NULL;
}


/*************************************************************
        メインルーチン
 *************************************************************/

int main(int argc, char **argv)
{
    tree_t *tree;
    char  buff[1024];
    char  *p, *from, *to;

    /* コマンド引数の個数をチェックする */
    if (argc != 2) {
        fprintf(stderr, "Usage: regexp regular-expression\n");
        exit(1);
    }

    /* コマンド引数で指定された正規表現をパースして構文木を得る */
    tree = parse_regexp(argv[1]);

    /* 構文木からNFAを生成する */
    build_nfa(tree);

#if     DEBUG
    /* 生成したNFAの内容を表示する */
    dump_nfa();
#endif  /* DEBUG */

    /* NFAからDFAを生成する */
    convert_nfa_to_dfa();

    /* 標準入力から1行ずつ読み込んだ行に対して、パターンマッチをする */
    while (fgets(buff, sizeof(buff) - 1, stdin) != NULL) {

        /* 行末の改行文字を取り除き、末尾に\0をセットする */
        buff[sizeof(buff)] = '\0';
        if ((p = strchr(buff, '\n')) != NULL)
            *p = '\0';

        /* DFAを使ってパターンマッチをしてみる */
        do_match(buff, &from ,&to);

        /* もし、マッチに成功したら、この行を表示してマッチした部分に
           --を表示する */
        if (from != NULL) {
            printf("%s\n", buff);
            for (p = buff; p < from; p++)
                printf(" ");
            for (; p < to; p++)
                printf("-");
            printf("\n");
        }
    }

    return 0;
}
