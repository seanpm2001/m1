#ifndef __M1_AST_H__
#define __M1_AST_H__

#include "m1_instr.h"

typedef enum data_types {
    TYPE_INT    = 0,
    TYPE_NUM    = 1,
    TYPE_STRING = 2,
    TYPE_PMC    = 3,
    TYPE_VOID   = 4,
    TYPE_USERDEFINED = 5
    
} data_type;

typedef struct m1_chunk {
    data_type rettype;
    char *name;
    struct m1_chunk *next;
    struct m1_expression *block;
    /* TODO: add parameters */
        
} m1_chunk;

typedef struct m1_structfield {
    char     *name; 
    data_type type;     
    unsigned  offset;
    
    struct m1_structfield *next;
    
} m1_structfield;

typedef struct m1_struct {
    char    *name;
    unsigned numfields;
    unsigned size; /* can calculate from fields but better keep a "cached" value */
    
    struct m1_structfield *fields;
    
} m1_struct;


typedef struct m1_assignment {
    struct m1_expression *lhs;
    struct m1_expression *rhs;
    
} m1_assignment;

typedef struct m1_funcall {
    char *name;
    /* TODO: add args */
    
} m1_funcall;

typedef struct m1_newexpr {
	char *type;
	/* TODO handle args */
	
} m1_newexpr;


typedef enum m1_expr_type {
    EXPR_NUMBER,
    EXPR_INT,
    EXPR_BINARY,
    EXPR_UNARY,
    EXPR_FUNCALL,
    EXPR_ASSIGN,
    EXPR_IF,
    EXPR_WHILE,
    EXPR_DOWHILE,
    EXPR_FOR,
    EXPR_RETURN,
    EXPR_NULL,
    EXPR_DEREF,
    EXPR_ADDRESS,
    EXPR_OBJECT,
    EXPR_BREAK,
    EXPR_STRING,
    EXPR_CONSTDECL,
    EXPR_VARDECL,
    EXPR_M0BLOCK,
    EXPR_SWITCH,
    EXPR_NEW,
    EXPR_PRINT   /* temporary? */
} m1_expr_type;


typedef enum m1_binop {
	OP_ASSIGN,
    OP_PLUS,
    OP_MINUS,
    OP_MUL,
    OP_DIV,
    OP_MOD,
    OP_XOR,
    OP_GT,
    OP_GE,
    OP_LT,
    OP_LE,
    OP_EQ,
    OP_NE,
    OP_AND,
    OP_OR,
    OP_BAND,
    OP_BOR,
    OP_RSH,
    OP_LSH
} m1_binop;

typedef struct m1_binexpr {
    struct m1_expression *left;
    struct m1_expression *right;
    m1_binop op;
        
} m1_binexpr;

typedef enum m1_unop {
    UNOP_POSTINC,  /* a++ */
    UNOP_POSTDEC,  /* a-- */
    UNOP_PREINC,   /* ++a */
    UNOP_PREDEC,   /* --a */
    UNOP_MINUS,    /* -a */ 
    UNOP_NOT       /* !a */
} m1_unop;

typedef struct m1_unexpr {
    struct m1_expression *expr;
    m1_unop op;
    
} m1_unexpr;

typedef enum m1_object_type {
    OBJECT_MAIN,  /* a in a.b  */
    OBJECT_FIELD, /* b in a.b  */
    OBJECT_INDEX, /* b in a[b] */
    OBJECT_DEREF, /* b in a->b */
    OBJECT_SCOPE  /* b in a::b */
} m1_object_type;

typedef struct m1_object {
    
    union {
        char *field;  /* for name, field or deref access */
        struct m1_expression *index; /* for array index */        
    } obj;
    
    enum m1_object_type type;
    
    struct m1_object *next;  
      
} m1_object;

/* for while and do-while statements */
typedef struct m1_whileexpr {
    struct m1_expression *cond;    
    struct m1_expression *block;
} m1_whileexpr;

/* for if statements */
typedef struct m1_ifexpr {
    struct m1_expression *cond;
    struct m1_expression *ifblock;
    struct m1_expression *elseblock;
} m1_ifexpr;

/* for for-statements */
typedef struct m1_forexpr {
    struct m1_expression *init;
    struct m1_expression *cond;
    struct m1_expression *step;
    struct m1_expression *block;    
} m1_forexpr;

/* const declarations */
typedef struct m1_const {
    data_type             type;
    char                 *name;
    struct m1_expression *value;
} m1_const;

/* variable declarations */
typedef struct m1_var {
    data_type             type;
    char                 *name;
    struct m1_expression *init;
    unsigned              size; /* 1 for non-arrays, larger for arrays */
    
} m1_var;


typedef struct m1_case {
	int selector;
	struct m1_expression *block;
	
	struct m1_case *next;
	
} m1_case;

typedef struct m1_switch {
	struct m1_expression *selector;
	struct m1_case       *cases;
	struct m1_expression *defaultstat;
	
} m1_switch;


typedef struct m0_block {
    struct m0_instr *instr;
    
} m0_block;

/* to represent statements */
typedef struct m1_expression {
    union {
        struct m1_unexpr     *u;
        struct m1_binexpr    *b;
        double               floatval;
        int                  intval;
        char                 *str;   
        struct m1_funcall    *f;  
        struct m1_assignment *a; 
        struct m1_whileexpr  *w;  
        struct m1_forexpr    *o;
        struct m1_ifexpr     *i;
        struct m1_expression *e; 
        struct m1_object     *t;
        struct m1_const      *c;
        struct m1_var        *v;
        struct m0_block      *m0;
        struct m1_switch     *s;
        struct m1_newexpr    *n;
    } expr;
    
    m1_expr_type      type;
    struct m1_symbol *sym;
    
    struct m1_expression *next;
    
} m1_expression;


extern m1_chunk *chunk(int rettype, char *name, m1_expression *block);



extern m1_expression *expression(m1_expr_type type);
extern void expr_set_num(m1_expression *e, double v);
extern void expr_set_int(m1_expression *e, int v);


extern void expr_set_unexpr(m1_expression *node, m1_expression *exp, m1_unop op);             
       
extern m1_expression *funcall(char *name);


extern void expr_set_expr(m1_expression *node, m1_expression *expr);
extern void expr_set_obj(m1_expression *node, m1_object *obj);

extern void expr_set_assign(m1_expression *node, 
                            m1_expression *lhs, int assignop, m1_expression *rhs);

extern void obj_set_ident(m1_object *node, char *ident);
extern void obj_set_index(m1_object *node, m1_expression *index);

            
extern m1_object *object(m1_object_type type);            

extern m1_structfield * structfield(char *name, data_type type);

extern m1_struct *newstruct(char *name, m1_structfield *fields);

extern void expr_set_string(m1_expression *node, char *str);

extern m1_expression *ifexpr(m1_expression *cond, m1_expression *ifblock, m1_expression *elseblock);
extern m1_expression *whileexpr(m1_expression *cond, m1_expression *block);
extern m1_expression *dowhileexpr(m1_expression *cond, m1_expression *block);
extern m1_expression *forexpr(m1_expression *init, m1_expression *cond, m1_expression *step, m1_expression *stat);

extern m1_expression *inc_or_dec(m1_expression *obj, m1_unop optype);
extern m1_expression *returnexpr(m1_expression *retexp);
extern m1_expression *assignexpr(m1_expression *lhs, int assignop, m1_expression *rhs);
extern m1_expression *objectexpr(m1_object *obj, m1_expr_type type);

extern m1_expression *binexpr(m1_expression *e1, m1_binop op, m1_expression *e2);
extern m1_expression *number(double value);
extern m1_expression *integer(int value);
extern m1_expression *string(char *str);
extern m1_expression *unaryexpr(m1_unop op, m1_expression *e);
extern m1_object *arrayindex(m1_expression *index);
extern m1_object *objectfield(char *field);
extern m1_object *objectderef(char *field);
extern m1_expression *printexpr(m1_expression *e);
extern m1_expression *constdecl(data_type type, char *name, m1_expression *expr);
extern m1_expression *vardecl(data_type type, m1_var *v);

extern m1_var *var(char *name, m1_expression *init);
extern m1_var *array(char *name, unsigned size);

extern unsigned field_size(struct m1_structfield *field);

extern m1_expression *switchexpr(m1_expression *expr, m1_case *cases, m1_expression *defaultstat);
extern m1_case *switchcase(int selector, m1_expression *block);

extern m1_expression *newexpr(char *type);

#endif

