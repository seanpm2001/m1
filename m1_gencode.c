/*

Code generator.

Visit each node, and generate instructions as appropriate.
See m1_ast.h for an overview of the AST node types. For most
nodes/functions, a m1_reg structure is returned, that holds the
type and number of the register that will hold the result of
the expression for which code was generated.

Example: a node representing a floating point number will load
the number in an N register, and return that register. This happens
in gencode_number().

*/
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include "m1_gencode.h"
#include "m1_ast.h"
#include "m1_compiler.h"

#include "m1_ann.h"

#define OUT	stdout



static m1_reg gencode_expr(M1_compiler *comp, m1_expression *e);

static const char type_chars[4] = {'i', 'n', 's', 'p'};
static const char reg_chars[4] = {'I', 'N', 'S', 'P'};
/*

Allocate new registers as needed.

*/
static m1_reg
gen_reg(M1_compiler *comp, data_type type) {
    /* int, num, string, pmc */
    static int regs[4] = {1,1,1,1};
    m1_reg reg;
    reg.type = type;
//	reg.no   = comp->regs[type]++;   

    reg.no = regs[type]++;
    return reg;
}

/*

Generate label identifiers.

*/
static int
gen_label(M1_compiler *comp) {
	return comp->label++;	
}


static m1_reg
gencode_number(M1_compiler *comp, double value) {
	/*
	deref Nx, CONSTS, <const_id>
	*/
    m1_reg     reg = gen_reg(comp, TYPE_NUM);
    m1_symbol *sym = sym_find_num(comp->floats, value);


    fprintf(OUT, "\tderef\tN%d, CONSTS, %d\n", reg.no, sym->constindex);
    return reg;
}   

static m1_reg
gencode_int(M1_compiler *comp, int value) {
	/*
	deref Ix, CONSTS, <const_id>
	*/
    m1_reg     reg = gen_reg(comp, TYPE_INT);
    m1_symbol *sym = sym_find_int(comp->ints, value);

    fprintf(OUT, "\tderef\tI%d, CONSTS, %d\n", reg.no, sym->constindex);
    return reg;
}

static m1_reg
gencode_string(M1_compiler *comp, NOTNULL(char *value)) {
    m1_reg     reg = gen_reg(comp, TYPE_STRING);
    m1_reg     one = gen_reg(comp, TYPE_INT);
    
    m1_symbol *sym = sym_find_str(comp->strings, value); /* find index of value in CONSTS */
    fprintf(OUT, "\tset_imm\tI%d, 0, %d\n", one.no, sym->constindex);
    fprintf(OUT, "\tderef\tS%d, CONSTS, I%d\n", reg.no, one.no);
    return reg;
}


static m1_reg
gencode_assign(M1_compiler *comp, NOTNULL(m1_assignment *a)) {
	m1_reg lhs, rhs;
    rhs = gencode_expr(comp, a->rhs);
    lhs = gencode_expr(comp, a->lhs);
    fprintf(OUT, "\tset\t%c%d, %c%d, x\n", lhs.type, lhs.no, rhs.type, rhs.no);
	return lhs;
}

static m1_reg
gencode_null(M1_compiler *comp) {
	m1_reg reg;

    return reg;
}   

static m1_reg
gencode_obj(M1_compiler *comp, m1_object *obj) {
	m1_reg reg, oreg;
	
	reg  = gen_reg(comp, 'I');
	oreg = gen_reg(comp, 'I');
	
	
    switch (obj->type) {
        case OBJECT_MAIN:
        	
            fprintf(OUT, "\tset\tI%d, I%d\n", 1000, 2000);
            break;
        case OBJECT_FIELD:
            fprintf(OUT, "\tset_imm\tI%d, %d\n", 1000, 4);
            break;
        case OBJECT_DEREF:
	/* todo */
            break;
        case OBJECT_INDEX:         
            reg = gencode_expr(comp, obj->obj.index);
            break;            
        default:
            break;
    }      
    
    if (obj->next) {
        reg = gencode_obj(comp, obj->next);   
    }
    
    return reg;
}

static m1_reg
gencode_while(M1_compiler *comp, m1_whileexpr *w) {
	/*
	
	START:
	<code for cond>
	goto_if END
	<code for block>
	goto START
	END:
	*/
	m1_reg reg;
	int endlabel   = gen_label(comp), 
	    startlabel = gen_label(comp);
	
	fprintf(OUT, "L%d:\n", startlabel);
    reg = gencode_expr(comp, w->cond);
	fprintf(OUT, "\tgoto_if\n");
    gencode_expr(comp, w->block);
    fprintf(OUT, "\tgoto \tL%d\n", startlabel);
	fprintf(OUT, "L%d:\n", endlabel);
	
	return reg;
}

static m1_reg
gencode_dowhile(M1_compiler *comp, m1_whileexpr *w) {
	/*
	START:
	<code for block>
	<code for cond>
	goto_if START
	*/
    m1_reg reg;
    int    startlabel = gen_label(comp);
    
    fprintf(OUT, "L%d:\n", startlabel);
    gencode_expr(comp, w->block);
    
    reg = gencode_expr(comp, w->cond);
    fprintf(OUT, "\tgoto_if\tL%d\n", startlabel);
    
    return reg;
}

static m1_reg
gencode_for(M1_compiler *comp, m1_forexpr *i) {
	/*
	
	<code for init>
	START:
	<code for cond>
	goto_if END
	<code for block>
	<code for step>
	goto START
	END:
	*/
    int startlabel = gen_label(comp),
        endlabel   = gen_label(comp);
        
    m1_reg reg;
    
    if (i->init)
        gencode_expr(comp, i->init);

	fprintf(OUT, "L%d\n", startlabel);
	
    if (i->cond)
        reg = gencode_expr(comp, i->cond);
   
    fprintf(OUT, "\tgoto_if L%d\n", endlabel);
    
    if (i->block) 
        gencode_expr(comp, i->block);
        
    if (i->step)
        gencode_expr(comp, i->step);
    
    fprintf(OUT, "\tgoto L%d\n", startlabel);
    fprintf(OUT, "L%d:\n", endlabel);
    
    
    return reg;
}

static m1_reg 
gencode_if(M1_compiler *comp, m1_ifexpr *i) {
	/*
	
	result1 = <evaluate condition>
	goto_if result1 == 0, ELSE
	<code for ifblock>
	goto END
	ELSE:
	<code for elseblock>
	END:
	*/
	m1_reg cond;
	int endlabel  = gen_label(comp),
		elselabel = gen_label(comp) ;

	
    cond = gencode_expr(comp, i->cond);
	
	fprintf(OUT, "\tgoto_if\tL_IF_%d\n", elselabel);
	
    gencode_expr(comp, i->ifblock);
	fprintf(OUT, "\tgoto L%d\n", endlabel);
	fprintf(OUT, "L_IF_%d:\n", elselabel);
	
    if (i->elseblock) {    	
        gencode_expr(comp, i->elseblock);
    }
    fprintf(OUT, "L_IF_%d:\n", endlabel);
    return cond;       
}

static m1_reg
gencode_deref(M1_compiler *comp, m1_object *o) {
	m1_reg reg;

    reg = gencode_obj(comp, o);
    return reg;
}

static m1_reg
gencode_address(M1_compiler *comp, m1_object *o) {
	m1_reg reg;

    reg = gencode_obj(comp, o);   
    return reg;
}

static m1_reg
gencode_return(M1_compiler *comp, m1_expression *e) {
	m1_reg reg;
    reg = gencode_expr(comp, e);
    return reg;
}

static m1_reg
gencode_or(M1_compiler *comp, m1_binexpr *b) {
	/*
	result1 = <evaluate left>
	goto_if result1 != 0, END
	result2 = <evaluate right>
	END:
	*/
	m1_reg left, right;
	int endlabel;
	
	endlabel = gen_label(comp);
	left     = gencode_expr(comp, b->left);
	
	fprintf(OUT, "\tgoto_if L_OR_%d\n", endlabel);
	
	right = gencode_expr(comp, b->right);
	
	fprintf(OUT, "L_OR_%d:\n", endlabel);
	return left;	
}

static m1_reg
gencode_and(M1_compiler *comp, m1_binexpr *b) {
	/*
	result1 = <evaluate left>
	goto_if result1 == 0, END
	result2 = <evaluate right>
	END:
	*/
	m1_reg left, right;
	int endlabel = gen_label(comp);
	
	left = gencode_expr(comp, b->left);
	fprintf(OUT, "\tgoto_if\tL_AND_%d\n", endlabel);	
	right = gencode_expr(comp, b->right);
	
	fprintf(OUT, "L_AND_%d:\n", endlabel);
	return right;
}

static m1_reg
gencode_eq(M1_compiler *comp, m1_binexpr *b) {
    m1_reg reg;
    
    return reg;   
}

static m1_reg
gencode_binary(M1_compiler *comp, m1_binexpr *b) {
    char  *op = NULL;
    m1_reg left, 
    	   right,
    	   target;
    
    switch(b->op) {
    	case OP_ASSIGN:
    		op = "set"; /* in case of a = b = c; then b = c part is a binary expression */
    		break;
        case OP_PLUS:
            op = "add_i";
            break;
        case OP_MINUS:
            op = "sub_i";
            break;
        case OP_MUL:
            op = "mult_i";
            break;
        case OP_DIV:
            op = "div_i";
            break;
        case OP_MOD:
            op = "mod_i";
            break;
        case OP_XOR:
            op = "xor";
            break;
        case OP_GT:
/*            op = ">";*/
            break;
        case OP_GE:
/*            op = ">=";*/
            break;
        case OP_LT:
/*            op = "<";*/
            break;
        case OP_LE:
/*            op = "<=";*/
            break;
        case OP_EQ:
            return gencode_eq(comp, b);
            break;
        case OP_NE:
/*            op = "!=";*/
            break;
        case OP_AND: /* a && b */
            return gencode_and(comp, b);

        case OP_OR: /* a || b */
            return gencode_or(comp, b);

        case OP_BAND:
            op = "and";
            break;
        case OP_BOR:
            op = "or";
            break;
        default:
            op = "unknown op";
            break;   
    }
    
    left        = gencode_expr(comp, b->left);
    right       = gencode_expr(comp, b->right);  
    target      = gen_reg(comp, left.type);  
    
    fprintf(OUT, "\t%s\t%c%d, %c%d, %c%d\n", op, reg_chars[(int)target.type], target.no, 
           reg_chars[(int)left.type], left.no, reg_chars[(int)right.type], right.no);
    return target;
}


static m1_reg
gencode_not(M1_compiler *comp, m1_unexpr *u) {
    m1_reg reg, temp;
    int label1, label2;
    
    reg  = gencode_expr(comp, u->expr);
    temp = gen_reg(comp, TYPE_INT);
    
    /* if reg is zero, make it nonzero (false->true).
    If it's non-zero, make it zero. (true->false). 
    */
    /*
      goto_if reg, L1 #non-zero, make it zero.
      set_imm Ix, 0, 0
      goto L2
    L1: # zero, make it non-zero
      set_imm Ix, 0, 1
    L2:
      set reg, Ix
    #done
    
    */
    label1 = gen_label(comp);
    label2 = gen_label(comp);
    
    fprintf(OUT, "\tgoto_if\t%c%d, L%d, x\n", reg_chars[(int)reg.type], reg.no, label1);
    fprintf(OUT, "\tset_imm\tI%d, 0, 0\n", temp.no);
    fprintf(OUT, "\tgoto L%d, x, x\n", label2);
    fprintf(OUT, "L%d:\n", label1);
    fprintf(OUT, "\tset_imm\tI%d, 0, 1\n", temp.no);
    fprintf(OUT, "L%d:\n", label2);
    fprintf(OUT, "\tset\t%c%d, I%d\n", reg_chars[(int)reg.type], reg.no, temp.no);
    
    return reg;
}

static m1_reg
gencode_uminus(M1_compiler *comp, m1_unexpr *u) {
    m1_reg reg;
    
    reg = gencode_expr(comp, u->expr);
    
    /* let's say u is 42. Then substracting 42 should be 0.
    If not, it was a negative number to begin with. For a positive 
    number, substract the value twice to get the negative value. 
    For a negative number, add the value to itself twice. 
    */
    
    return reg;    
}

static m1_reg
gencode_unary(M1_compiler *comp, NOTNULL(m1_unexpr *u)) {
    char  *op;
    int    postfix = 0;
    m1_reg reg;
    m1_reg target = gen_reg(comp, TYPE_INT); /* for final value */
    m1_reg one    = gen_reg(comp, TYPE_INT); /* to store "1" */
    
    switch (u->op) {
        case UNOP_POSTINC:
        case UNOP_POSTDEC:
            postfix = 1;
            break;
        case UNOP_PREINC:
        case UNOP_PREDEC:
            postfix = 0; 
            break;
        case UNOP_MINUS:
            return gencode_uminus(comp, u);
        case UNOP_NOT:
            return gencode_not(comp, u);
        default:
            op = "unknown op";
            break;   
    }   
    
    /* generate code for the pre/post ++ expression */ 
    reg = gencode_expr(comp, u->expr);
    
    fprintf(OUT, "\tset_imm\tI%d, 0, 1\n", one.no);
    fprintf(OUT, "\tadd_i\tI%d, I%d, I%d\n", target.no, reg.no, one.no);
    
    if (postfix == 0) { 
        /* prefix; return reg containing value before adding 1 */
    	reg.no = target.no; 
    }	
    
   	return reg;	    
    
}

static m1_reg
gencode_break(M1_compiler *comp) {
	m1_reg reg;
	/* pop label from compiler's label stack (todo!) and jump there. */
    fprintf(OUT, "\tgoto\tL??\n");
    return reg;
}

static m1_reg
gencode_funcall(M1_compiler *comp, m1_funcall *f) {
	m1_reg reg;
	m1_reg pmcreg;
	m1_reg offsetreg;
    m1_symbol *fun = sym_find_chunk(comp->globals, f->name);
    
    if (fun == NULL) {
        fprintf(stderr, "Cant find function %s\n", f->name);   
        return reg;
    }
    reg = gen_reg(comp, TYPE_INT);
    fprintf(OUT, "\tset_imm\tI%d, 0, %d\n", reg.no, fun->constindex);
    pmcreg = gen_reg(comp, TYPE_PMC);
    offsetreg = gen_reg(comp, TYPE_INT);
    fprintf(OUT, "\tset_imm\tI%d, 0, %d\n", offsetreg.no, 0);
    fprintf(OUT, "\tderef\tP%d, CONSTS, I%d\n", pmcreg.no, reg.no);
    fprintf(OUT, "\tgoto_chunk\tP%d, I%d, x\n", pmcreg.no, offsetreg.no);
    return reg;
}


static m1_reg
gencode_print(M1_compiler *comp, m1_expression *expr) {

    m1_reg reg;
    m1_reg one;
    reg = gencode_expr(comp, expr);
    one = gen_reg(comp, TYPE_INT);
    
    fprintf(OUT, "\tset_imm\tI%d, 0, 1\n",  one.no);
	fprintf(OUT, "\tprint_%c\tI%d, %c%d, x\n", type_chars[(int)reg.type], one.no, reg_chars[(int)reg.type], reg.no);
	return reg;
}

static m1_reg
gencode_new(M1_compiler *comp, m1_newexpr *expr) {
	m1_reg reg     = gen_reg(comp, TYPE_INT);
	m1_reg sizereg = gen_reg(comp, TYPE_INT);
	unsigned size  = 128; /* fix; should be size of object requested */
	fprintf(OUT, "\tset_imm I%d, 0, %d\n", sizereg.no, size);
	fprintf(OUT, "\tgc_alloc\tI%d, I%d, 0\n", reg.no, sizereg.no);
	return reg;	
}

static m1_reg
gencode_expr(M1_compiler *comp, m1_expression *e) {

    m1_reg reg;
    if (e == NULL) {
    	
        return reg;
    }
        
    switch (e->type) {
        case EXPR_NUMBER:
            reg = gencode_number(comp, e->expr.floatval);
            break;
        case EXPR_INT:
            reg = gencode_int(comp, e->expr.intval);
            break;
        case EXPR_STRING:
            reg = gencode_string(comp, e->expr.str);     
            break;
        case EXPR_BINARY:
            reg = gencode_binary(comp, e->expr.b);
            break;
        case EXPR_UNARY:
            reg = gencode_unary(comp, e->expr.u);
            break;
        case EXPR_FUNCALL:
            reg = gencode_funcall(comp, e->expr.f);
            break;
        case EXPR_ASSIGN:
            reg = gencode_assign(comp, e->expr.a);
            break;
        case EXPR_IF:   
            reg = gencode_if(comp, e->expr.i);
            break;
        case EXPR_WHILE:
            reg = gencode_while(comp, e->expr.w);
            break;
        case EXPR_DOWHILE:
            reg = gencode_dowhile(comp, e->expr.w);
            break;
        case EXPR_FOR:
            reg = gencode_for(comp, e->expr.o);
            break;
        case EXPR_RETURN:
            reg = gencode_return(comp, e->expr.e);
            break;
        case EXPR_NULL:
            reg = gencode_null(comp);
            break;
        case EXPR_DEREF:
            reg = gencode_deref(comp, e->expr.t);
            break;
        case EXPR_ADDRESS:
            reg = gencode_address(comp, e->expr.t);
            break;
        case EXPR_OBJECT:
            reg = gencode_obj(comp, e->expr.t);
            break;
        case EXPR_BREAK:
            reg = gencode_break(comp);
            break;
        case EXPR_CONSTDECL:
        	break;
        case EXPR_VARDECL:
            break;
        case EXPR_SWITCH:
        	break;
        case EXPR_NEW:
        	reg = gencode_new(comp, e->expr.n);
        	break;
        case EXPR_PRINT:
            gencode_print(comp, e->expr.e);   
            break; 
        default:
            fprintf(stderr, "unknown expr type");   
            exit(EXIT_FAILURE);
    }   
    return reg;
}

static void
print_consts(NOTNULL(m1_symboltable *table)) {
	m1_symbol *iter;
	iter = table->syms;
	while (iter != NULL) {
		
		switch (iter->type) {
			case VAL_STRING:
				fprintf(OUT, "%d %s\n", iter->constindex, iter->value.str);
				break;
			case VAL_FLOAT:
				fprintf(OUT, "%d %f\n", iter->constindex, iter->value.fval);
				break;
			case VAL_INT:
				fprintf(OUT, "%d %d\n", iter->constindex, iter->value.ival);
				break;
	        case VAL_CHUNK:
	            fprintf(OUT, "%d &%s\n", iter->constindex, iter->value.str);
	            break;
			default:
				fprintf(stderr, "unknown symbol type");
				exit(EXIT_FAILURE);
		}
		iter = iter->next;	
	}
}
static void
gencode_consts(M1_compiler *comp) {
	fprintf(OUT, ".constants\n");
	print_consts(comp->strings);	
	print_consts(comp->floats);	
	print_consts(comp->ints);	
	print_consts(comp->globals);
}

static void
gencode_metadata(M1_compiler *comp) {
	fprintf(OUT, ".metadata\n");	
}

static void 
gencode_chunk(M1_compiler *comp, m1_chunk *c) {
    m1_expression *iter;
    
    fprintf(OUT, ".chunk \"%s\"\n", c->name);
    
    gencode_consts(comp);
    gencode_metadata(comp);
    
    fprintf(OUT, ".bytecode\n");
    
    /* generate code for statements */
    iter = c->block;
    while (iter != NULL) {
        (void)gencode_expr(comp, iter);
        iter = iter->next;
    }
}

void 
gencode(M1_compiler *comp, m1_chunk *ast) {
    m1_chunk *iter = ast;
     
    fprintf(OUT, ".version 0\n");
    while (iter != NULL) {        
        gencode_chunk(comp, iter);
        iter = iter->next;   
    }
}



