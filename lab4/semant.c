#include <stdio.h>
#include <assert.h>
#include <string.h>
#include "util.h"
#include "errormsg.h"
#include "symbol.h"
#include "absyn.h"
#include "types.h"
#include "helper.h"
#include "env.h"
#include "semant.h"

/*Lab4: Your implementation of lab4*/


typedef void* Tr_exp;
struct expty 
{
	Tr_exp exp; 
	Ty_ty ty;
};

//In Lab4, the first argument exp should always be **NULL**.
struct expty expTy(Tr_exp exp, Ty_ty ty)
{
	struct expty e;

	e.exp = exp;
	e.ty = ty;

	return e;
}

Ty_ty actual_ty(Ty_ty ty){
	while(ty && ty->kind == Ty_name)
		ty = ty->u.name.ty;
	return ty;
}

struct expty transExp (S_table venv, S_table tenv, A_exp a){
    switch (a->kind) {
		case A_opExp:{
			A_oper oper = a->u.op.oper;
			struct expty left = transExp(venv, tenv, a->u.op.left);
			struct expty right = transExp(venv, tenv, a->u.op.right);
			if(oper == A_plusOp || oper == A_minusOp 
			|| oper == A_timesOp || oper == A_divideOp) {
				if(actual_ty(left.ty)->kind != Ty_int)
					EM_error(a->u.op.left->pos, "integer required");
				if(actual_ty(right.ty)->kind != Ty_int)
					EM_error(a->u.op.right->pos, "integer required");
				return expTy(NULL, Ty_Int());
			}
			else{//eq neq lt le gt ge
				if(actual_ty(left.ty) != actual_ty(right.ty))
					EM_error(a->pos, "same type required");
				return expTy(NULL, Ty_Int());
			}
		}
		case A_letExp:{
			// printf("let\n");
			S_beginScope(venv);
			S_beginScope(tenv);
			struct expty exp;
			A_decList d;
			for(d = a->u.let.decs; d; d = d->tail)
				transDec(venv, tenv, d->head);
			exp = transExp(venv, tenv, a->u.let.body);
			S_endScope(tenv);  
			S_endScope(venv);
			return exp;	
    	}
		case A_varExp:
			return transVar(venv, tenv, a->u.var);
		case A_nilExp:
			return expTy(NULL, Ty_Nil());
		case A_intExp:
			return expTy(NULL, Ty_Int());
		case A_stringExp:
			return expTy(NULL, Ty_String());
		case A_breakExp:
			return expTy(NULL, Ty_Void());
		case A_callExp:{
			E_enventry x = S_look(venv, a->u.call.func);
			if(x && x->kind == E_funEntry){   //check formals
				Ty_tyList expect = x->u.fun.formals;
				A_expList actual = a->u.call.args;
				while(expect && actual){
					struct expty exp_arg = transExp(venv, tenv, actual->head);
					if(actual_ty(exp_arg.ty)->kind != actual_ty(expect->head)->kind)
						EM_error(actual->head->pos, "para type mismatch");
					expect = expect->tail;
					actual = actual->tail;
				}
				if(expect || actual)
					EM_error(actual->head->pos, "too many params in function %s", S_name(a->u.call.func));
				return expTy(NULL, actual_ty(x->u.fun.result));
			}
			else{
				EM_error(a->pos, "undefined function %s", S_name(a->u.call.func));
				return expTy(NULL, Ty_Int());
			}
		}
		case A_recordExp:{
			// printf("record\n");
			Ty_ty ty = S_look(tenv, a->u.record.typ);
			ty = actual_ty(ty);
			if(!ty){
				EM_error(a->pos, "undefined type %s", S_name(a->u.record.typ));
				return expTy(NULL, Ty_Int());
			}
			if(ty->kind != Ty_record)
				EM_error(a->pos, "not record  %s", S_name(a->u.record.typ));
			A_efieldList actual = a->u.record.fields;
			Ty_fieldList expect = ty->u.record;
			while(actual && expect){
				if(actual->head->name != expect->head->name)
					EM_error(a->pos, "record name doesn't exist %s", S_name(a->u.record.typ));
				struct expty exp_rec = transExp(venv, tenv, actual->head->exp);
				if(actual_ty(exp_rec.ty) != actual_ty(expect->head->typ))
					EM_error(a->pos, "para type mismatch");
				expect = expect->tail;
				actual = actual->tail;
			}
			if(expect || actual)
				EM_error(a->pos, "less or more field %s", S_name(a->u.record.typ));
			return expTy(NULL, ty);
		}
		case A_arrayExp:{
			// printf("arrayExp\n");
			Ty_ty ty = S_look(tenv, a->u.array.typ);
			ty = actual_ty(ty);
			if(!ty)
				EM_error(a->pos, "undefined array  %s", S_name(a->u.array.typ));
			if(ty->kind != Ty_array)
				EM_error(a->pos, "not array  %s", S_name(a->u.array.typ));
			struct expty size = transExp(venv, tenv, a->u.array.size);
			struct expty init = transExp(venv, tenv, a->u.array.init);
			if(actual_ty(size.ty)->kind != Ty_int)
				EM_error(a->pos, "size of array should be int %s", S_name(a->u.array.typ));
			if(actual_ty(init.ty) != actual_ty(ty->u.array))
				EM_error(a->pos, "type mismatch");
			return expTy(NULL, ty);
		}
		case A_seqExp:{
			// printf("seqExp\n");
			A_expList seq = a->u.seq;
			struct expty ty;
			while(seq){
				ty = transExp(venv, tenv, seq->head);
				seq = seq->tail;
			}
			return ty;
		}
		case A_assignExp:{
			// printf("assignExp");
			if(a->u.assign.var->kind == A_simpleVar){
				E_enventry x = S_look(venv, a->u.assign.var->u.simple);
				if(x && x->readonly == 1)
					EM_error(a->pos, "loop variable can't be assigned");
			}
			struct expty var = transVar(venv, tenv, a->u.assign.var);
			struct expty exp = transExp(venv, tenv, a->u.assign.exp);
			if(actual_ty(var.ty) != actual_ty(exp.ty))
				EM_error(a->pos, "unmatched assign exp");
			return expTy(NULL, Ty_Void());
		}
		case A_ifExp:{
			struct expty test = transExp(venv, tenv, a->u.iff.test);
			// if(actual_ty(test.ty)->kind != Ty_int)
			// 	EM_error(a->pos, "");
			struct expty then = transExp(venv, tenv, a->u.iff.then);
			if(actual_ty(then.ty)->kind != Ty_void)
				EM_error(a->pos, "if-then exp's body must produce no value");
			if(a->u.iff.elsee){
				struct expty elsee = transExp(venv, tenv, a->u.iff.elsee);
				if(actual_ty(then.ty) != actual_ty(elsee.ty))
					EM_error(a->u.iff.then->pos, "then exp and else exp type mismatch");
			}
			return expTy(NULL, then.ty);
		}
		case A_forExp:{
			struct expty lo = transExp(venv, tenv, a->u.forr.lo);
			struct expty hi = transExp(venv, tenv, a->u.forr.hi);
			if(actual_ty(lo.ty)->kind != Ty_int)
				EM_error(a->pos, "for exp's range type is not integer");
			if(actual_ty(hi.ty)->kind != Ty_int)
				EM_error(a->pos, "for exp's range type is not integer");
			S_enter(venv, a->u.forr.var, E_ROVarEntry(Ty_Int()));
			S_beginScope(venv);
			struct expty body = transExp(venv, tenv, a->u.forr.body);
			S_endScope(venv);
			if(actual_ty(body.ty)->kind != Ty_void)
				EM_error(a->pos, "the body of for should be void ");
			return expTy(NULL, Ty_Void());
		}
		case A_whileExp:{
			struct expty test = transExp(venv, tenv, a->u.whilee.test);
			if(actual_ty(test.ty)->kind != Ty_int)
				EM_error(a->pos, "test part of while should be int");
			struct expty body = transExp(venv, tenv, a->u.whilee.body);
			if(actual_ty(body.ty)->kind != Ty_void)
				EM_error(a->pos, "while body must produce no value");
			return expTy(NULL, Ty_Void());
		}
		default: 
			break;
	}
}

struct expty transVar (S_table venv, S_table tenv, A_var v){
    switch (v->kind) {
		case A_simpleVar:{
			E_enventry x = S_look(venv, v->u.simple) ;
			if(x && x->kind == E_varEntry) 
				return expTy(NULL, actual_ty(x->u.var.ty));
			else{
				EM_error(v->pos, "undefined variable %s", S_name(v->u.simple));
				return expTy(NULL, Ty_Int());
			}
		}
		case A_fieldVar:{//record a:b
			struct expty var = transVar(venv, tenv, v->u.field.var);
			if(var.ty->kind != Ty_record){
				EM_error(v->pos, "not a record type");
				return expTy(NULL, Ty_Int());
			}
			Ty_fieldList fields = var.ty->u.record;
			while(fields != NULL && fields->head->name != v->u.field.sym){
				fields = fields->tail;
			}
			if(!fields){
				EM_error(v->pos,"field %s doesn't exist", S_name(v->u.field.sym));
				return expTy(NULL, Ty_Int());
			}
			return expTy(NULL, actual_ty(fields->head->ty));
		}
		case A_subscriptVar:{//array a,b,c
			struct expty var = transVar(venv, tenv, v->u.subscript.var);
			struct expty exp = transExp(venv, tenv, v->u.subscript.exp);
			if(var.ty->kind != Ty_array){
				EM_error(v->pos, "array type required");
				return expTy(NULL, Ty_Int());
			}
			if(exp.ty->kind != Ty_int){
				EM_error(v->pos, "subscriptvar 22");
				return expTy(NULL, Ty_Int());
			}
		}
		default:
			break;
	}
}

Ty_tyList makeFormalTyList(S_table tenv, A_fieldList fields){
	if(!fields)
		return NULL;
	Ty_ty ty = S_look(tenv, fields->head->typ);
	return Ty_TyList(ty, makeFormalTyList(tenv, fields->tail));
}

void transDec (S_table venv, S_table tenv, A_dec d){
    switch(d->kind){
		case A_varDec:{
			struct expty e = transExp(venv, tenv, d->u.var.init);
			if(d->u.var.typ){
				Ty_ty type = S_look(tenv, d->u.var.typ);
				if(!type){
					EM_error(d->u.var.init->pos, "type not exist %s", S_name(d->u.var.typ));
				}
				if(actual_ty(type) != actual_ty(e.ty)) {
					EM_error(d->u.var.init->pos, "type mismatch");
				}
			}
			else if (actual_ty(e.ty)->kind == Ty_nil)
				EM_error(d->u.var.init->pos, "init should not be nil without type specified");
			S_enter(venv, d->u.var.var, E_VarEntry(e.ty));
			break;
		}
		case A_typeDec:{
			A_nametyList types = d->u.type;
			while(types){	//add name to the table
				if(S_look(tenv, types->head->name)){
					EM_error(d->pos, "two types have the same name");
				}
				else
					S_enter(tenv, types->head->name, Ty_Name(types->head->name, NULL));
				types = types->tail;	
			}
			types = d->u.type;
			while(types){	//put binding
				Ty_ty ty = S_look(tenv, types->head->name);
				if(!ty)
					continue;
				ty->u.name.ty = transTy(tenv, types->head->ty);
				types = types->tail;
			}
			types = d->u.type;
			while(types){	//cycle
				Ty_ty ty = S_look(tenv, types->head->name);
				Ty_ty temp = ty->u.name.ty;
				for(; temp->kind == Ty_name && temp->u.name.ty; temp = temp->u.name.ty){
					if(temp == ty){
						EM_error(d->pos, "illegal type cycle");
						ty->u.name.ty = Ty_Int();
						break;
					}
				}
				types = types->tail;
			}
			break;
		}
		case A_functionDec:{
			A_fundecList fs = d->u.function;
			while(fs){
				if(S_look(venv, fs->head->name)){
					EM_error(d->pos,"two functions have the same name");
					fs = fs->tail;
					continue;
				}
				Ty_ty resultTy;
				if(fs->head->result){
					resultTy = S_look(tenv, fs->head->result);
					if (!resultTy){
						EM_error(fs->head->pos, "undefined result %s", S_name(fs->head->result));
						resultTy = Ty_Void();
					}
				}
				else{
					resultTy = Ty_Void();
				}
				Ty_tyList formalTys = makeFormalTyList(tenv, fs->head->params);
				S_enter(venv, fs->head->name, E_FunEntry(formalTys, resultTy));
				fs = fs->tail;
			}
			fs = d->u.function;
			while(fs){ //only scan the second time can check the body
				S_beginScope(venv); 
				Ty_tyList formalTys = makeFormalTyList(tenv, fs->head->params);
				A_fieldList params = fs->head->params;
				
				for(; params; params = params->tail, formalTys = formalTys->tail)
					S_enter(venv, params->head->name, E_VarEntry(formalTys->head));
				struct expty exp = transExp(venv, tenv, fs->head->body);
				E_enventry x = S_look(venv, fs->head->name);
				if(actual_ty(x->u.fun.result)->kind == Ty_void && 
					actual_ty(exp.ty)->kind != Ty_void)
					EM_error(fs->head->pos,"procedure returns value");
				S_endScope(venv);
				fs = fs->tail;
			}
			break;
		}
		default:
			break;
    }
}

Ty_fieldList makeFieldList(S_table tenv, A_fieldList fields){
	A_field field = fields->head;
	Ty_ty ty = S_look(tenv, field->typ);
	if(!ty)
		EM_error(field->pos,"undefined type %s",S_name(field->typ));
	Ty_field head = Ty_Field(field->name, ty);
	if(fields->tail)
		return Ty_FieldList(head, makeFieldList(tenv, fields->tail));
	else
		return Ty_FieldList(head, NULL);
}

Ty_ty transTy(S_table tenv, A_ty a){
	switch(a->kind){
		case A_nameTy:{
			Ty_ty ty = S_look(tenv, a->u.name);
			if(!ty){
				EM_error(a->pos,"undefined nameTy %s",S_name(a->u.name));
				return Ty_Int();
			}
			return Ty_Name(a->u.name, ty);
		}
		case A_recordTy:{
			return Ty_Record(makeFieldList(tenv, a->u.record));
		}
		case A_arrayTy:{
			Ty_ty ty = S_look(tenv, a->u.array);
			if(!ty){
				EM_error(a->pos,"undefined arrayTy %s",S_name(a->u.array));
				return Ty_Int();
			}
			return Ty_Array(ty);
		}
		default:
			break;
	}
}
void SEM_transProg(A_exp exp){
	transExp(E_base_venv(), E_base_tenv(), exp);
}
