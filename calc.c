// Mehmet Kaan ULU 231ADB102

//     ls 
//     gcc -std=c11 -O2 -Wall -Wextra -o calc calc.c -lm
//     ./calc test8.txt

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>

#ifdef _WIN32
#define SEP "\\"
#else
#define SEP "/"
#endif

typedef enum {
    T_NUM, T_ADD, T_SUB, T_MUL, T_DIV, T_POW, T_LP, T_RP, T_END
} TokType;

typedef struct {
    TokType type;
    const char *start;
    int pos;
    double value;
} Token;

typedef struct {
    const char *src;
    size_t len;
    size_t i;
    int pos;
    int bol_basi;
} Scanner;

static int g_error = 0;
static void set_error(int p){ if(!g_error) g_error = p; }

static void scan_init(Scanner *S, const char *s, size_t l){
    S->src = s; S->len = l; S->i = 0; S->pos = 1; S->bol_basi = 1;
}

static int peek(Scanner *S){ return (S->i >= S->len)? EOF : (unsigned char)S->src[S->i]; }

static int getc2(Scanner *S){
    if (S->i >= S->len) return EOF;
    int c = (unsigned char)S->src[S->i++];
    S->pos++;
    S->bol_basi = (c == '\n');
    return c;
}

static void skip_space_and_comment(Scanner *S){
    for(;;){
        while(isspace(peek(S))) getc2(S);
        if (S->bol_basi){
            size_t j = S->i;
            while(j < S->len && isspace((unsigned char)S->src[j]) && S->src[j] != '\n') j++;
            if (j < S->len && S->src[j] == '#'){
                while(1){
                    int c = getc2(S);
                    if (c == '\n' || c == EOF) break;
                }
                continue;
            }
        }
        break;
    }
}

static Token next_token(Scanner *S){
    skip_space_and_comment(S);
    Token t; memset(&t, 0, sizeof(t));
    t.start = S->src + S->i;
    t.pos = S->pos;

    int c = peek(S);
    if (c == EOF){ t.type = T_END; return t; }

    if (isdigit(c) || c == '.'){
        char *endp;
        errno = 0;
        double v = strtod(S->src + S->i, &endp);
        if (endp == S->src + S->i){
            getc2(S);
            set_error(t.pos);
            t.type = T_END;
            return t;
        }
        size_t used = (size_t)(endp - (S->src + S->i));
        for(size_t k=0; k<used; k++) getc2(S);
        t.type = T_NUM; t.value = v;
        return t;
    }

    switch(c){
        case '+': getc2(S); t.type = T_ADD; return t;
        case '-': getc2(S); t.type = T_SUB; return t;
        case '*':
            getc2(S);
            if (peek(S) == '*'){ getc2(S); t.type = T_POW; return t; }
            t.type = T_MUL; return t;
        case '/': getc2(S); t.type = T_DIV; return t;
        case '(' : getc2(S); t.type = T_LP; return t;
        case ')' : getc2(S); t.type = T_RP; return t;
        default: getc2(S); set_error(t.pos); t.type = T_END; return t;
    }
}

typedef struct { Token *a; size_t n, cap; } TokVec;
static void tok_push(TokVec *v, Token t){
    if (v->n == v->cap){
        v->cap = v->cap ? v->cap * 2 : 64;
        v->a = realloc(v->a, v->cap * sizeof(Token));
    }
    v->a[v->n++] = t;
}

typedef struct { TokVec toks; size_t i; } Parser;
static Token* p_peek(Parser *P){ return (P->i < P->toks.n)? &P->toks.a[P->i] : NULL; }
static Token* p_adv(Parser *P){ if(P->i < P->toks.n) P->i++; return &P->toks.a[P->i-1]; }
static int p_match(Parser *P, TokType tp){ Token *t=p_peek(P); if(t && t->type==tp){ p_adv(P); return 1;} return 0; }

static double parse_expr(Parser *P);

static double parse_primary(Parser *P, int *outpos){
    Token *t = p_peek(P);
    if (!t){ set_error(1); return 0; }
    if (t->type == T_NUM){ *outpos = t->pos; p_adv(P); return t->value; }
    if (p_match(P, T_LP)){
        int pos0 = P->toks.a[P->i-1].pos;
        double val = parse_expr(P);
        if (!p_match(P, T_RP)){
            Token *u = p_peek(P);
            if (u) set_error(u->pos); else set_error(pos0);
        }
        *outpos = pos0; return val;
    }
    set_error(t->pos); return 0;
}

static double parse_unary(Parser *P, int *outpos){
    int sign = 1, pos = 0;
    while (p_match(P, T_ADD) || p_match(P, T_SUB)){
        Token *prev = &P->toks.a[P->i-1];
        if (prev->type == T_SUB) sign = -sign;
        pos = prev->pos;
    }
    double v = parse_primary(P, &pos);
    *outpos = pos;
    return sign * v;
}

static double parse_power(Parser *P, int *outpos){
    double left = parse_unary(P, outpos);
    if (p_match(P, T_POW)){
        int rpos=0; double right = parse_power(P, &rpos);
        *outpos = P->toks.a[P->i-1].pos;
        return pow(left, right);
    }
    return left;
}

static double parse_term(Parser *P, int *outpos){
    int pos0 = 0; double v = parse_power(P, &pos0);
    for(;;){
        if (p_match(P, T_MUL)){
            int pos1=0; double b = parse_power(P, &pos1);
            v *= b; *outpos = pos1;
        } else if (p_match(P, T_DIV)){
            int pos2=0; double b = parse_power(P, &pos2);
            if (fabs(b) < 1e-12){ set_error(pos2); return 0; }
            v /= b; *outpos = pos2;
        } else break;
    }
    return v;
}

static double parse_expr(Parser *P){
    int pos=0; double val = parse_term(P, &pos);
    for(;;){
        if (p_match(P, T_ADD)){ int p2=0; val += parse_term(P, &p2); }
        else if (p_match(P, T_SUB)){ int p2=0; val -= parse_term(P, &p2); }
        else break;
    }
    return val;
}

static void tokenize_all(Scanner *S, TokVec *out){
    memset(out, 0, sizeof(*out));
    for(;;){
        Token t = next_token(S);
        tok_push(out, t);
        if (t.type == T_END) break;
        if (g_error) break;
    }
}

static int evaluate_and_write(const char *buf, size_t len, FILE *fo){
    g_error = 0;
    Scanner sc; scan_init(&sc, buf, len);
    TokVec tv; tokenize_all(&sc, &tv);

    if (!g_error){
        Parser P; P.toks = tv; P.i=0;
        (void)parse_expr(&P);
        Token *t = p_peek(&P);
        if (t && t->type != T_END) set_error(t->pos);
    }

    if (g_error){
        fprintf(fo, "ERROR:%d\n", g_error);
        free(tv.a);
        return 1;
    } else {
        Parser P2; P2.toks = tv; P2.i=0;
        double val = parse_expr(&P2);
        double near = llround(val);
        if (fabs(val - near) < 1e-12) fprintf(fo, "%lld\n", (long long)near);
        else fprintf(fo, "%.15g\n", val);
        free(tv.a);
        return 0;
    }
}

static const char* getenv_or(const char *k, const char *def){
    const char *v = getenv(k);
    return (v && *v) ? v : def;
}

static int ensure_dir(const char *p){
    struct stat st;
    if (stat(p, &st) == 0){
#ifdef S_ISDIR
        if (S_ISDIR(st.st_mode)) return 0;
#else
        if ((st.st_mode & S_IFMT) == S_IFDIR) return 0;
#endif
        errno = ENOTDIR; return -1;
    }
#ifdef _WIN32
    return mkdir(p);
#else
    return mkdir(p, 0775);
#endif
}

static void make_default_outdir(const char *input, char *out, size_t n){
    const char *slash = strrchr(input, SEP[0]);
    const char *base = slash ? slash+1 : input;
    char base_noext[256]; memset(base_noext, 0, sizeof(base_noext));
    strncpy(base_noext, base, sizeof(base_noext)-1);
    char *dot = strrchr(base_noext, '.'); if(dot) *dot = '\0';
    const char *user = getenv_or("USER","student");
    const char *sid = getenv_or("STUDENT_ID","231ADB102");
    snprintf(out, n, "%s_%s_%s", base_noext, user, sid);
}

static void make_output_filename(const char *input, char *out, size_t n){
    const char *slash = strrchr(input, SEP[0]);
    const char *base = slash ? slash+1 : input;
    const char *nm = getenv_or("NAME","Mehmet");
    const char *ln = getenv_or("LASTNAME","Ulu");
    const char *sid = getenv_or("STUDENT_ID","231ADB102");
    snprintf(out, n, "%s_%s_%s_%s.txt", base, nm, ln, sid);
}

static char* read_all(const char *p, size_t *olen){
    FILE *f = fopen(p,"rb"); if(!f) return NULL;
    fseek(f,0,SEEK_END); long sz = ftell(f); if(sz<0){ fclose(f); return NULL; }
    fseek(f,0,SEEK_SET);
    char *buf = malloc((size_t)sz+1); if(!buf){ fclose(f); return NULL; }
    size_t n = fread(buf,1,(size_t)sz,f);
    fclose(f);
    buf[n] = '\0'; if(olen) *olen = n;
    return buf;
}

static int ends_with(const char *s, const char *suf){
    size_t a=strlen(s), b=strlen(suf);
    if(b>a) return 0;
    return strncmp(s+(a-b),suf,b)==0;
}

static int process_file(const char *in_path, const char *out_dir){
    size_t len=0; char *buf = read_all(in_path,&len);
    if(!buf){ fprintf(stderr,"read fail: %s\n", in_path); return 1; }
    if (ensure_dir(out_dir)!=0){ fprintf(stderr,"outdir fail: %s\n", out_dir); free(buf); return 1; }

    char outname[512]; make_output_filename(in_path, outname, sizeof(outname));
    char outpath[1024]; snprintf(outpath, sizeof(outpath), "%s%s%s", out_dir, SEP, outname);
    FILE *fo = fopen(outpath, "wb");
    if(!fo){ fprintf(stderr,"write fail: %s\n", outpath); free(buf); return 1; }

    (void)evaluate_and_write(buf, len, fo);
    fclose(fo); free(buf); return 0;
}

static void usage(void){
    fprintf(stderr, "Usage:\n");
    fprintf(stderr, "  calc [-d DIR|--dir DIR] [-o OUTDIR|--output-dir OUTDIR] input.txt\n");
}

int main(int argc, char **argv){
    const char *indir = NULL, *outdir = NULL, *input = NULL;
    for(int i=1;i<argc;i++){
        if(!strcmp(argv[i],"-d")||!strcmp(argv[i],"--dir")){
            if(i+1>=argc){ usage(); return 1; }
            indir = argv[++i];
        }
        else if(!strcmp(argv[i],"-o")||!strcmp(argv[i],"--output-dir")){
            if(i+1>=argc){ usage(); return 1; }
            outdir = argv[++i];
        }
        else if(argv[i][0]=='-'){ usage(); return 1; }
        else input = argv[i];
    }

    if(!input){ usage(); return 1; }

    char def_out[512]={0};
    if(!outdir){ make_default_outdir(input, def_out, sizeof(def_out)); outdir = def_out; }

    int rc = 0;
    if(indir){
        DIR *d = opendir(indir);
        if(!d){ fprintf(stderr,"dir open fail: %s\n", indir); return 1; }
        struct dirent *e;
        while((e = readdir(d))){
            if(e->d_name[0] == '.') continue;
            if(!ends_with(e->d_name, ".txt")) continue;
            char inpath[1024];
            snprintf(inpath, sizeof(inpath), "%s%s%s", indir, SEP, e->d_name);
            rc |= process_file(inpath, outdir);
        }
        closedir(d);
    } else {
        rc = process_file(input, outdir);
    }
    return rc ? 1 : 0;
}
