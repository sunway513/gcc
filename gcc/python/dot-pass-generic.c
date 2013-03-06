/* This file is part of GCC.

   GCC is free software; you can redistribute it and/or modify it under
   the terms of the GNU General Public License as published by the Free
   Software Foundation; either version 3, or (at your option) any later
   version.

   GCC is distributed in the hope that it will be useful, but WITHOUT ANY
   WARRANTY; without even the implied warranty of MERCHANTABILITY or
   FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
   for more details.

   You should have received a copy of the GNU General Public License
   along with GCC; see the file COPYING3.  If not see
   <http://www.gnu.org/licenses/>.  */

#include "gpython.h"

/* DEFINES */
#define DOT_CURRENT_CONTEXT(x_)						\
  VEC_index (dot_context, x_, (VEC_length (dot_context, x_) - 1))
/* ... ... ... */

/* PROTOTYPES... */
static tree dot_pass_genFndecl_Basic (tree, tree);

static tree dot_pass_getModuleType (const char *, gpy_hash_tab_t *);
static void dot_pass_setupContext (tree, dot_context_t, VEC(tree,gc) *, tree *);
static void dot_pass_generic_TU (gpy_hash_tab_t *, VEC(gpydot,gc) *, VEC(tree,gc) *);

static tree dot_pass_genFindAddr (const char *, const char *, VEC(tree,gc) *);
static void dot_pass_genWalkClass (tree *, tree, tree, VEC(tree,gc) *);
static void dot_pass_setupClassFieldContext (tree, tree, dot_context);

static tree dot_pass_lookupDecl (dot_context_t, const char *);
static int dot_pass_pushDecl (tree, const char *, dot_context);

static tree dot_pass_genFunction (gpy_dot_tree_t *, VEC(dot_context, gc) *, const char *);
static VEC(tree,gc) * dot_pass_genClass (gpy_dot_tree_t *, dot_context_t, tree, const char *);

static tree dot_pass_genScalar (gpy_dot_tree_t *, tree *);
static tree dot_pass_genEnclosure (gpy_dot_tree_t *, tree *, dot_context_t);
static void dot_pass_genCBlock (gpy_dot_tree_t *, tree *, dot_context_t, tree, tree);

static tree dot_pass_lowerExpr (gpy_dot_tree_t *, VEC(dot_context, gc) *, tree *);
static void dot_pass_genPrintStmt (gpy_dot_tree_t * , tree *, dot_context_t);
static tree dot_pass_genModifyExpr (gpy_dot_tree_t *, tree *, dot_context_t);
static tree dot_pass_genBinExpr (gpy_dot_tree_t *, tree *, dot_context_t);
static void dot_pass_genConditional (gpy_dot_tree_t *, tree *, dot_context_t);
static void dot_pass_genSuite (gpy_dot_tree_t * , tree *, dot_context_t);
/* ... ... ... */

static
tree dot_pass_genFndecl_Basic (tree ident, tree fntype)
{
  tree fndecl = build_decl (BUILTINS_LOCATION, FUNCTION_DECL, ident, fntype);

  TREE_STATIC (fndecl) = 0;
  TREE_USED (fndecl) = 1;
  DECL_ARTIFICIAL (fndecl) = 1;
  TREE_PUBLIC (fndecl) = 1;

  tree argslist = NULL_TREE;
  DECL_ARGUMENTS (fndecl) = argslist;
  /* Define the return type (represented by RESULT_DECL) for the main functin */
  tree resdecl = build_decl (BUILTINS_LOCATION, RESULT_DECL,
			     NULL_TREE, TREE_TYPE (fntype));
  DECL_CONTEXT (resdecl) = fndecl;
  DECL_ARTIFICIAL (resdecl) = true;
  DECL_IGNORED_P (resdecl) = true;
  DECL_RESULT (fndecl) = resdecl;

  return fndecl;
}

static
tree dot_pass_getModuleType (const char * s,
			     gpy_hash_tab_t * modules)
{
  tree retval = error_mark_node;

  gpy_hashval_t h = gpy_dd_hash_string (s);
  gpy_hash_entry_t * e = gpy_dd_hash_lookup_table (modules, h);
  if (e)
    {
      if (e->data)
        retval = (tree) e->data;
    }
  return retval;
}

static
void dot_pass_setupClassFieldContext (tree type, tree self,
				      dot_context context)
{
  tree field = NULL_TREE;
  for (field = TYPE_FIELDS (type); field != NULL_TREE;
       field = DECL_CHAIN (field))
    {
      const char * ident = IDENTIFIER_POINTER (DECL_NAME (field));
      tree ref = build3 (COMPONENT_REF, TREE_TYPE (field),
			 build_fold_indirect_ref (self),
			 field, NULL_TREE);
      gcc_assert (dot_pass_pushDecl (ref, ident, context));
    }
}

static
tree dot_pass_genFindAddr (const char * id,
			   const char * parent_ident,
			   VEC(tree,gc) * decls)
{
  tree retval = null_pointer_node;
  tree ident = dot_pass_concat_identifier (parent_ident, id);
  const char * search = IDENTIFIER_POINTER (ident);

  int idx;
  tree decl = NULL_TREE;
  for (idx = 0; VEC_iterate (tree, decls, idx, decl); ++idx)
    {
      tree decl_name = DECL_NAME (decl);
      if (!strcmp (search, IDENTIFIER_POINTER (decl_name)))
	{
	  retval = decl;
	  break;
	}
    }
  return retval;
}

static
void dot_pass_genWalkClass (tree * block, tree type,
			    tree decl,
			    VEC(tree,gc) * ldecls)
{
  const char * type_name = IDENTIFIER_POINTER (TYPE_NAME (type));
  VEC(tree,gc) * attribs = VEC_alloc (tree,gc,0);

  tree field = NULL_TREE;
  int offset = 0;
  for (field = TYPE_FIELDS (type); field != NULL_TREE;
       field = DECL_CHAIN (field))
    {
      const char * ident = IDENTIFIER_POINTER (DECL_NAME (field));
      tree element_size = TYPE_SIZE_UNIT (TREE_TYPE (field));
      tree offs = fold_build2_loc (UNKNOWN_LOCATION, MULT_EXPR, sizetype,
				   build_int_cst (sizetype, offset),
				   element_size);
      tree str = gpy_dot_type_const_string_tree (ident);
      tree fnaddr = dot_pass_genFindAddr (ident, type_name, ldecls);

      tree fnaddr_tmp = build_decl (UNKNOWN_LOCATION, VAR_DECL,
				    create_tmp_var_name ("FNAD"),
				    ptr_type_node);
      tree arguments = NULL_TREE;
      int n = 0;
      if (fnaddr != null_pointer_node)
	{
	  gcc_assert (TREE_CODE (fnaddr) == FUNCTION_DECL);
	  arguments = DECL_ARGUMENTS (fnaddr);
	  tree args;
	  for (args = arguments; args != NULL_TREE;
	       args = DECL_CHAIN (args))
	    n++;
	  append_to_statement_list (build2 (MODIFY_EXPR, ptr_type_node,
					    fnaddr_tmp,
					    build_fold_addr_expr (fnaddr)),
				    block);
	}
      else
	append_to_statement_list (build2 (MODIFY_EXPR, ptr_type_node,
					  fnaddr_tmp,
					  build_int_cst (ptr_type_node, 0)),
				  block);

      tree nargs = build_int_cst (integer_type_node, n);
      tree atdecl = build_decl (UNKNOWN_LOCATION, VAR_DECL,
				create_tmp_var_name ("AT"),
				gpy_attrib_type_ptr);
      tree a = GPY_RR_fold_attrib (build_fold_addr_expr (str),
				   fnaddr_tmp,
				   offs, nargs);
      append_to_statement_list (build2 (MODIFY_EXPR, gpy_object_type_ptr,
					atdecl, a),
				block);
      VEC_safe_push (tree, gc, attribs, atdecl);
      offset++;
    }
  VEC(tree,gc) * args = VEC_alloc (tree,gc,0);
  VEC_safe_push (tree, gc, args,
		 build_int_cst (integer_type_node, VEC_length (tree, attribs)));
  GPY_VEC_stmts_append (tree, args, attribs);

  tree attribs_decl = build_decl (UNKNOWN_LOCATION, VAR_DECL,
				  create_tmp_var_name ("AL"),
				  gpy_attrib_type_ptr_ptr);
  append_to_statement_list (build2 (MODIFY_EXPR, gpy_attrib_type_ptr_ptr,
				    attribs_decl,
				    GPY_RR_fold_attrib_list (args)),
			    block);
  tree class_str = gpy_dot_type_const_string_tree (type_name);
  tree fold_class = GPY_RR_fold_class_decl (attribs_decl, TYPE_SIZE_UNIT (type),
					    build_fold_addr_expr (class_str));

  switch (TREE_CODE (decl))
    {
    case VAR_DECL:
      {
	tree class_tmp = build_decl (UNKNOWN_LOCATION, VAR_DECL,
				     create_tmp_var_name ("ATFC"),
				     gpy_object_type_ptr);
	append_to_statement_list (build2 (MODIFY_EXPR, gpy_object_type_ptr,
					  class_tmp, fold_class),
				  block);
	if (TREE_TYPE (decl) == gpy_object_type_ptr_ptr)
	  {
	    append_to_statement_list (build2 (MODIFY_EXPR, gpy_object_type_ptr_ptr,
					      build_fold_indirect_ref (decl),
					      class_tmp),
				      block);
	  }
	else
	  fatal_error ("serious error at ATFC!\n");
      }
      break;

    default:
      fatal_error ("Error seting up class decl!\n");
      break;
    }
}

static
void dot_pass_setupContext (tree module,
			    dot_context_t context,
			    VEC(tree,gc) * generic,
			    tree * block)
{
  debug ("Setting up context %s for module %s\n",
	 GPY_RR_stack_ptr,
	 GPY_current_module_name);
  dot_context globls = VEC_index (dot_context, context, 0);
  dot_context globls_symbols = VEC_index (dot_context, context, 1);

  tree stack_pointer = build_decl (BUILTINS_LOCATION, VAR_DECL,
				   get_identifier (GPY_RR_stack_ptr),
				   gpy_object_type_ptr_ptr);
  TREE_STATIC (stack_pointer) = 1;
  TREE_PUBLIC (stack_pointer) = 1;
  TREE_USED (stack_pointer) = 1;
  DECL_ARTIFICIAL (stack_pointer) = 1;

  // set it to 0 because its not initilized yet..
  DECL_INITIAL (stack_pointer) = build_int_cst (integer_type_node, 0);
  rest_of_decl_compilation (stack_pointer, 1, 0);
  gpy_dd_hash_insert (gpy_dd_hash_string (GPY_RR_stack_ptr), stack_pointer, globls);
  VEC_safe_push (tree, gc, generic, stack_pointer);

  int offset = 0, field_count = 0;
  tree field;
  for (field = TYPE_FIELDS (module); field != NULL_TREE;
       field = DECL_CHAIN (field))
    field_count++;

  tree type = build_array_type (char_type_node,
				build_index_type (size_int (strlen (GPY_current_module_name))
						  ));
  type = build_qualified_type (type, TYPE_QUAL_CONST);
  gpy_preserve_from_gc (type);
  tree str = build_string (strlen (GPY_current_module_name), GPY_current_module_name);
  TREE_TYPE (str) = type;
  append_to_statement_list (GPY_RR_initRRStack (build_int_cst (integer_type_node,
							       field_count),
						stack_pointer,
						build_fold_addr_expr (str)),
			    block);
  debug ("--- --- --- \n");
  offset = 0;
  field_count = 0;
  for (field = TYPE_FIELDS (module); field != NULL_TREE;
       field = DECL_CHAIN (field))
    {
      gcc_assert (TREE_CODE (field) == FIELD_DECL);
      const char * ident = IDENTIFIER_POINTER (DECL_NAME (field));

      tree element_size = TYPE_SIZE_UNIT (TREE_TYPE (field));
      tree offs = fold_build2_loc (UNKNOWN_LOCATION, MULT_EXPR, sizetype,
				   build_int_cst (sizetype, offset),
				   element_size);
      tree addr = fold_build2_loc (UNKNOWN_LOCATION, POINTER_PLUS_EXPR,
				   TREE_TYPE (stack_pointer),
				   stack_pointer, offs);

      // mangle the name with the module name
      char * mangled = dot_pass_concat (GPY_current_module_name, "__DECL__");
      char * nm = dot_pass_concat (mangled, ident);
      tree addr_decl = build_decl (UNKNOWN_LOCATION, VAR_DECL,
				   get_identifier (nm),
				   gpy_object_type_ptr_ptr);
      TREE_STATIC (addr_decl) = 1;
      TREE_PUBLIC (addr_decl) = 1;
      TREE_USED (addr_decl) = 1;
      DECL_ARTIFICIAL (addr_decl) = 1;
      DECL_INITIAL (addr_decl) = build_int_cst (integer_type_node, 0);

      rest_of_decl_compilation (addr_decl, 1, 0);
      VEC_safe_push (tree, gc, generic, addr_decl);

      append_to_statement_list (build2 (MODIFY_EXPR,
					gpy_object_type_ptr_ptr,
					addr_decl, addr),
				block);

      // sizeof is wrong here need to get the integer cst of the element_size;
      debug ("\t%s :: %i * %lu\n", ident, offset, sizeof (void *));
      gcc_assert (dot_pass_pushDecl (addr_decl, ident, globls_symbols));
      offset--;
      field_count++;
    }
  debug ("--- --- --- \n");
}

static
int dot_pass_pushDecl (tree decl, const char * ident,
		       dot_context context)
{
  int retval = 1;
  gpy_hashval_t h = gpy_dd_hash_string (ident);
  void ** slot = gpy_dd_hash_insert (h, decl, context);
  if (slot)
    {
      error ("error pushing decl <%s>!\n", ident);
      retval = 0;
    }
  return retval;
}

static
tree dot_pass_lookupDecl (dot_context_t context,
			  const char * identifier)
{
  tree retval = error_mark_node;
  gpy_hashval_t h = gpy_dd_hash_string (identifier);
  int length = VEC_length (dot_context, context);

  int i;
  for (i = length - 1; i >= 0; --i)
    {
      dot_context ctx = VEC_index (dot_context, context, i);
      gpy_hash_entry_t * o = NULL;
      o = gpy_dd_hash_lookup_table (ctx, h);

      if (o)
	if (o->data)
	  {
	    retval = (tree) o->data;
	    break;
	  }
    }
  return retval;
}

static
tree dot_pass_genScalar (gpy_dot_tree_t * decl, tree * block)
{
  tree retval = error_mark_node;

  gcc_assert (DOT_TYPE (decl) == D_PRIMITIVE);
  gcc_assert (DOT_lhs_T (decl) == D_TD_COM);

  switch (DOT_lhs_TC (decl)->T)
    {
    case D_T_INTEGER:
      {
        retval = build_decl (UNKNOWN_LOCATION, VAR_DECL,
			     create_tmp_var_name ("PI"),
                             gpy_object_type_ptr);
	tree fold_call = GPY_RR_fold_integer (build_int_cst (integer_type_node,
							     DOT_lhs_TC (decl)->o.integer));
        append_to_statement_list (build2 (MODIFY_EXPR,
					  gpy_object_type_ptr,
					  retval, fold_call),
                                  block);
      }
      break;

    default:
      error ("invalid scalar type!\n");
      break;
    }

  return retval;
}

static
tree dot_pass_genEnclosure (gpy_dot_tree_t * decl,
			    tree * block,
			    dot_context_t context)
{
  tree retval = error_mark_node;
  gcc_assert (DOT_TYPE (decl) == D_T_LIST);

  gpy_dot_tree_t * node;
  size_t length = 0;
  for (node = DOT_lhs_TT (decl); node != NULL_DOT;
       node = DOT_CHAIN (node))
    length++;

  VEC(tree,gc) * elms = VEC_alloc (tree, gc, 0);
  VEC_safe_push (tree, gc, elms, build_int_cst (integer_type_node, length));
  for (node = DOT_lhs_TT (decl); node != NULL_DOT;
       node = DOT_CHAIN (node))
    {
      tree tmp = dot_pass_lowerExpr (node, context, block);
      VEC_safe_push (tree, gc, elms, tmp);
    }
  retval = build_decl (UNKNOWN_LOCATION, VAR_DECL,
		       create_tmp_var_name ("PL"),
		       gpy_object_type_ptr);
  tree fold_call = GPY_RR_fold_encList (elms);
  append_to_statement_list (build2 (MODIFY_EXPR, gpy_object_type_ptr,
				    retval, fold_call),
			    block);
  return retval;
}

static
void dot_pass_genSuite (gpy_dot_tree_t * decl,
			tree * block,
			dot_context_t context)
{
  gpy_hash_tab_t suite;
  gpy_dd_hash_init_table (&suite);
  VEC_safe_push (dot_context, gc, context, &suite);

  gpy_dot_tree_t * node;
  for (node = decl; node != NULL_DOT;
       node = DOT_CHAIN (node))
    {
      if (DOT_T_FIELD (node) ==  D_D_EXPR)
	{
	  dot_pass_lowerExpr (node, context, block);
	  continue;
	}
      switch (DOT_TYPE (node))
	{
	case D_PRINT_STMT:
	  dot_pass_genPrintStmt (node, block, context);
	  break;

	default:
	  error ("unhandled syntax within suite");
	  break;
	}
    }
  VEC_pop (dot_context, context);
}

static
void dot_pass_genCBlock (gpy_dot_tree_t * decl,
			 tree * block,
			 dot_context_t context,
			 tree cval, tree endif)
{
  gpy_dot_tree_t * suite = NULL_DOT;
  tree ifcval = error_mark_node;
  if (DOT_TYPE (decl) == D_STRUCT_IF)
    {
      gpy_dot_tree_t * expr = DOT_lhs_TT (decl);
      tree val = dot_pass_lowerExpr (expr, context, block);
      tree fold = build_decl (UNKNOWN_LOCATION, VAR_DECL,
			      create_tmp_var_name ("COND"),
			      boolean_type_node);
      append_to_statement_list (build2 (MODIFY_EXPR, boolean_type_node,
					fold, GPY_RR_eval_boolean (val)),
				block);
      ifcval = fold;
      suite = DOT_rhs_TT (decl);
    }
  else
    {
      gcc_assert (DOT_TYPE (decl) == D_STRUCT_ELSE);
      suite = DOT_lhs_TT (decl);
    }

  tree label_decl = build_decl (UNKNOWN_LOCATION, LABEL_DECL,
				create_tmp_var_name ("LBIF"),
				void_type_node);
  tree label_expr = fold_build1_loc (UNKNOWN_LOCATION, LABEL_EXPR,
				     void_type_node, label_decl);

  tree label_exit_decl =  build_decl (UNKNOWN_LOCATION, LABEL_DECL,
				      create_tmp_var_name ("LBFI"),
				      void_type_node);
  tree label_exit_expr = fold_build1_loc (UNKNOWN_LOCATION, LABEL_EXPR,
					  void_type_node, label_exit_decl);

  DECL_CONTEXT (label_decl) = current_function_decl;
  DECL_CONTEXT (label_exit_decl) = current_function_decl;

  tree condition;
  tree evalto;
  if (ifcval != error_mark_node)
    {
      condition = ifcval;
      evalto = boolean_true_node;
    }
  else
    {
      condition = cval;
      evalto = boolean_false_node;
    }
  tree conditional = fold_build2_loc (UNKNOWN_LOCATION, EQ_EXPR,
				      boolean_type_node,
				      condition, evalto);

  tree cond = build3_loc (UNKNOWN_LOCATION, COND_EXPR, void_type_node,
			  conditional,
			  build1 (GOTO_EXPR, void_type_node, label_decl),
			  build1 (GOTO_EXPR, void_type_node, label_exit_decl));

  append_to_statement_list (cond, block);
  append_to_statement_list (label_expr, block);

  dot_pass_genSuite (suite, block, context);
  append_to_statement_list (build2 (MODIFY_EXPR, boolean_type_node,
				    cval, boolean_true_node),
			    block);
  append_to_statement_list (build1 (GOTO_EXPR, void_type_node, endif),
			    block);
  append_to_statement_list (label_exit_expr, block);
}

static
void dot_pass_genConditional (gpy_dot_tree_t * decl,
			      tree * block,
			      dot_context_t context)
{
  tree cval = build_decl (UNKNOWN_LOCATION, VAR_DECL,
			  create_tmp_var_name ("CELSE"),
			  boolean_type_node);
  DECL_INITIAL (cval) = boolean_false_node;
  append_to_statement_list (cval, block);

  tree endif_label_decl = build_decl (UNKNOWN_LOCATION, LABEL_DECL,
				      create_tmp_var_name ("ENDIF"),
				      void_type_node);
  tree endif_label_expr = fold_build1_loc (UNKNOWN_LOCATION, LABEL_EXPR,
					   void_type_node, endif_label_decl);

  gpy_dot_tree_t * ifblock = DOT_FIELD (decl);
  gpy_dot_tree_t * elifchain = DOT_lhs_TT (decl);
  gpy_dot_tree_t * elseblock = DOT_rhs_TT (decl);

  dot_pass_genCBlock (ifblock, block, context, cval, endif_label_decl);

  gpy_dot_tree_t * elifnode;
  for (elifnode = elifchain; elifnode != NULL_DOT;
       elifnode = DOT_CHAIN (elifnode))
    dot_pass_genCBlock (elifnode, block, context, cval, endif_label_decl);

  if (elseblock)
    dot_pass_genCBlock (elseblock, block, context, cval, endif_label_decl);

  append_to_statement_list (endif_label_expr, block);
}

static
tree dot_pass_genModifyExpr (gpy_dot_tree_t * decl,
			     tree * block,
			     dot_context_t context)
{
  tree retval = error_mark_node;
  gpy_dot_tree_t * lhs = DOT_lhs_TT (decl);
  gpy_dot_tree_t * rhs = DOT_rhs_TT (decl);

  /*
    We dont handle full target lists yet
    all targets are in the lhs tree.

    To implment a target list such as:
    x,y,z = 1

    The lhs should be a DOT_CHAIN of identifiers!
    So we just iterate over them and deal with it as such!
  */

  switch (DOT_TYPE (lhs))
    {
    case D_IDENTIFIER:
      {
	tree addr = dot_pass_lookupDecl (context,
					 DOT_IDENTIFIER_POINTER (lhs));
	/* means id isn't previously declared and we can just make it locally. */
	if (addr == error_mark_node)
	  {
	    dot_context current_context = DOT_CURRENT_CONTEXT (context);
	    addr = build_decl (UNKNOWN_LOCATION, VAR_DECL,
			       get_identifier (DOT_IDENTIFIER_POINTER (lhs)),
			       gpy_object_type_ptr);
	    gcc_assert (!dot_pass_pushDecl (addr, DOT_IDENTIFIER_POINTER (lhs), current_context));
	  }
	tree addr_rhs_tree = dot_pass_lowerExpr (rhs, context, block);

	switch (TREE_CODE (addr))
	  {
	  case PARM_DECL:
	  case COMPONENT_REF:
	  case VAR_DECL:
	    {
	      if (TREE_TYPE (addr) == gpy_object_type_ptr_ptr)
		{
		  /* *T.x = addr */
		  tree refer = build_fold_indirect_ref (addr);
		  append_to_statement_list (build2 (MODIFY_EXPR, gpy_object_type_ptr_ptr,
						    refer,
						    addr_rhs_tree),
					    block);
		  retval = refer;
		}
	      else
		{
		  append_to_statement_list (build2 (MODIFY_EXPR, gpy_object_type_ptr,
						    addr,
						    addr_rhs_tree),
					    block);
		  retval = addr;
		}
	    }
	    break;

	  default:
	    error ("unhandled shizzle!\n");
	    break;
	  }
      }
      break;

    case D_ATTRIB_REF:
      {
	tree addr_rhs_tree = dot_pass_lowerExpr (rhs, context, block);
	tree addr_lhs_tree = dot_pass_lowerExpr (lhs, context, block);

	gcc_assert (TREE_TYPE (addr_lhs_tree) == gpy_object_type_ptr_ptr);
	append_to_statement_list (build2 (MODIFY_EXPR, gpy_object_type_ptr_ptr,
					  build_fold_indirect_ref (addr_lhs_tree),
					  addr_rhs_tree),
				  block);
	retval = addr_lhs_tree;
      }
      break;

    default:
      error ("unhandled target or target list in modify expression\n");
      break;
    }

  return retval;
}

static
tree dot_pass_cleanRef (tree decl, tree * block)
{
  tree retval = decl;
  if (TREE_TYPE (decl) == gpy_object_type_ptr_ptr)
    {
      tree fold = build_decl (UNKNOWN_LOCATION, VAR_DECL,
			      create_tmp_var_name ("CLRF"),
			      gpy_object_type_ptr);
      append_to_statement_list (build2 (MODIFY_EXPR, gpy_object_type_ptr,
					fold, build_fold_indirect_ref (decl)),
				block);
      retval = fold;
    }
  return retval;
}

static
tree dot_pass_genBinExpr (gpy_dot_tree_t * decl, tree * block,
			  dot_context_t context)
{
  tree retval = error_mark_node;
  gcc_assert (DOT_T_FIELD (decl) == D_D_EXPR);

  gpy_dot_tree_t * lhs = DOT_lhs_TT (decl);
  gpy_dot_tree_t * rhs = DOT_rhs_TT (decl);
  tree lhs_eval = dot_pass_lowerExpr (lhs, context, block);
  tree rhs_eval = dot_pass_lowerExpr (rhs, context, block);

  lhs_eval = dot_pass_cleanRef (lhs_eval, block);
  rhs_eval = dot_pass_cleanRef (rhs_eval, block);

  tree op = error_mark_node;
  switch (DOT_TYPE (decl))
    {
      // @see libgpython/runtime/gpy-module-stack.c::gpy_rr_eval_expression
    case D_ADD_EXPR:
      op = GPY_RR_eval_expression (lhs_eval, rhs_eval,
				   build_int_cst (integer_type_node, 1));
      break;

    case D_MINUS_EXPR:
      op = GPY_RR_eval_expression (lhs_eval, rhs_eval,
				   build_int_cst (integer_type_node, 2));
      break;

    case D_LESS_EXPR:
      op = GPY_RR_eval_expression (lhs_eval, rhs_eval,
				   build_int_cst (integer_type_node, 4));
      break;

    case D_GREATER_EXPR:
      op = GPY_RR_eval_expression (lhs_eval, rhs_eval,
				   build_int_cst (integer_type_node, 5));
      break;

    case D_EQ_EQ_EXPR:
      op = GPY_RR_eval_expression (lhs_eval, rhs_eval,
				   build_int_cst (integer_type_node, 6));
      break;

      // .... THE REST OF THE BIN OPERATORS

    default:
      error ("unhandled binary operation type!\n");
      break;
    }
  gcc_assert (op != error_mark_node);

  tree retaddr = build_decl (UNKNOWN_LOCATION, VAR_DECL,
			     create_tmp_var_name ("T"),
                             gpy_object_type_ptr);
  append_to_statement_list (build2 (MODIFY_EXPR, gpy_object_type_ptr,
				    retaddr, op),
			    block);
  retval = retaddr;
  return retval;
}

static
void dot_pass_genPrintStmt (gpy_dot_tree_t * decl, tree * block,
			    dot_context_t context)
{
  gpy_dot_tree_t * arguments = decl->opa.t;

  VEC(tree,gc) * callvec_tmp = VEC_alloc (tree,gc,0);
  gpy_dot_tree_t * it = NULL;
  for (it = arguments; it != NULL; it = DOT_CHAIN (it))
    {
      tree lexpr = dot_pass_lowerExpr (it, context, block);
      tree tmp = lexpr;
      if (TREE_TYPE (lexpr) == gpy_object_type_ptr_ptr)
	{
	  /* lets fold */
	  tmp = build_decl (UNKNOWN_LOCATION, VAR_DECL,
			    create_tmp_var_name ("ATAR"),
			    gpy_object_type_ptr);
	  append_to_statement_list (build2 (MODIFY_EXPR, gpy_object_type_ptr,
					    tmp,
					    build_fold_indirect_ref (lexpr)),
				    block);
	}
      VEC_safe_push (tree, gc, callvec_tmp, tmp);
    }
  VEC(tree,gc) * callvec = VEC_alloc (tree,gc,0);
  VEC_safe_push (tree, gc, callvec, build_int_cst (integer_type_node, 1));
  VEC_safe_push (tree, gc, callvec, build_int_cst (integer_type_node,
						   VEC_length (tree, callvec_tmp)));

  GPY_VEC_stmts_append (tree, callvec, callvec_tmp);
  append_to_statement_list (GPY_RR_eval_print (callvec), block);
}

static
tree dot_pass_lowerExpr (gpy_dot_tree_t * dot,
			 VEC(dot_context, gc) * context,
			 tree * block)
{
  tree retval = error_mark_node;
  switch (DOT_TYPE (dot))
    {
    case D_PRIMITIVE:
      retval = dot_pass_genScalar (dot, block);
      break;

    case D_T_LIST:
      retval = dot_pass_genEnclosure (dot, block, context);
      break;

    case D_IDENTIFIER:
      {
	tree lookup = dot_pass_lookupDecl (context,
					   DOT_IDENTIFIER_POINTER (dot));
	gcc_assert (lookup != error_mark_node);
	switch (TREE_CODE (lookup))
	  {
	  case VAR_DECL:
	  case PARM_DECL:
	  case COMPONENT_REF:
	    retval = lookup;
	    break;

	  default:
	    {
	      tree tmp = build_decl (UNKNOWN_LOCATION, VAR_DECL,
				     create_tmp_var_name ("VA"),
				     gpy_object_type_ptr_ptr);
	      append_to_statement_list (build2 (MODIFY_EXPR,
						gpy_object_type_ptr_ptr,
						tmp, lookup),
					block);
	      tree atmp = build_decl (UNKNOWN_LOCATION, VAR_DECL,
				      create_tmp_var_name ("VP"),
				      gpy_object_type_ptr);
	      append_to_statement_list (build2 (MODIFY_EXPR,
						gpy_object_type_ptr,
						atmp, build_fold_indirect_ref (tmp)),
					block);
	      retval = atmp;
	    }
	    break;
	  }
      }
      break;

    case D_ATTRIB_REF:
      {
	tree addr_1 = build_decl (UNKNOWN_LOCATION, VAR_DECL,
				  create_tmp_var_name ("ATRD"),
				  gpy_object_type_ptr_ptr);
	gpy_dot_tree_t * xlhs = DOT_lhs_TT (dot);
	gpy_dot_tree_t * xrhs = DOT_rhs_TT (dot);

	gcc_assert (DOT_TYPE (xrhs) == D_IDENTIFIER);
	tree lhs_tree = dot_pass_lowerExpr (xlhs, context, block);
	char * attrib_ident = DOT_IDENTIFIER_POINTER (xrhs);

	tree str = gpy_dot_type_const_string_tree (attrib_ident);
	tree lhs_tree_fold = lhs_tree;
	if (TREE_TYPE (lhs_tree) == gpy_object_type_ptr_ptr)
	  {
	    lhs_tree_fold = build_decl (UNKNOWN_LOCATION, VAR_DECL,
					create_tmp_var_name ("ATRDF"),
					gpy_object_type_ptr);
	    append_to_statement_list (build2 (MODIFY_EXPR, gpy_object_type_ptr,
					      lhs_tree_fold,
					      build_fold_indirect_ref (lhs_tree)),
				      block);
	  }
	tree attrib_ref = build2 (MODIFY_EXPR, gpy_object_type_ptr_ptr, addr_1,
				  GPY_RR_fold_attrib_ref (lhs_tree_fold,
							  build_fold_addr_expr (str))
				  );
	append_to_statement_list (attrib_ref, block);
	retval = addr_1;
      }
      break;

    case D_CALL_EXPR:
      {
	gpy_dot_tree_t * callid = DOT_lhs_TT (dot);

	tree lcall_decl = dot_pass_lowerExpr (callid, context, block);
	tree call_decl = error_mark_node;
	if (TREE_TYPE (lcall_decl) == gpy_object_type_ptr_ptr)
	  {
	    /* lets fold */
	    call_decl = build_decl (UNKNOWN_LOCATION, VAR_DECL,
				    create_tmp_var_name ("ATAR"),
				    gpy_object_type_ptr);
	    append_to_statement_list (build2 (MODIFY_EXPR, gpy_object_type_ptr,
					      call_decl,
					      build_fold_indirect_ref (lcall_decl)),
				      block);
	  }

	gpy_dot_tree_t * argslist;
	VEC(tree,gc) * argsvec = VEC_alloc (tree,gc,0);
	if (DOT_TYPE (callid) == D_ATTRIB_REF)
	  {
	    tree basetree = dot_pass_lowerExpr (DOT_lhs_TT (callid),
						context, block);
	    tree basetree_fold = basetree;
	    if (TREE_TYPE (basetree) == gpy_object_type_ptr_ptr)
	      {
		basetree_fold = build_decl (UNKNOWN_LOCATION, VAR_DECL,
					    create_tmp_var_name ("ATRDF"),
					    gpy_object_type_ptr);
		append_to_statement_list (build2 (MODIFY_EXPR, gpy_object_type_ptr,
						  basetree_fold,
						  build_fold_indirect_ref (basetree)),
					  block);
	      }
	    VEC_safe_push (tree, gc, argsvec, basetree_fold);
	  }

	for (argslist = DOT_rhs_TT (dot); argslist != NULL_DOT;
	     argslist = DOT_CHAIN (argslist))
	  {
	    tree lexpr = dot_pass_lowerExpr (argslist, context, block);
	    tree argument = lexpr;
	    if (TREE_TYPE (argument) == gpy_object_type_ptr_ptr)
	      {
		argument = build_decl (UNKNOWN_LOCATION, VAR_DECL,
				       create_tmp_var_name ("ATRDF"),
				       gpy_object_type_ptr);
		append_to_statement_list (build2 (MODIFY_EXPR, gpy_object_type_ptr,
						  argument,
						  build_fold_indirect_ref (lexpr)),
					  block);
	      }
	    VEC_safe_push (tree, gc, argsvec, argument);
	  }
	VEC(tree,gc) * args = VEC_alloc (tree,gc,0);
	VEC_safe_push (tree, gc, args, call_decl);
	VEC_safe_push (tree, gc, args, build_int_cst (integer_type_node,
						      VEC_length (tree, argsvec)));
	GPY_VEC_stmts_append (tree, args, argsvec);
	tree retaddr = build_decl (UNKNOWN_LOCATION, VAR_DECL,
				   create_tmp_var_name ("RET"),
				   gpy_object_type_ptr);
	append_to_statement_list (build2 (MODIFY_EXPR, gpy_object_type_ptr,
					  retaddr,
					  GPY_RR_fold_call (args)),
				  block);
	retval = retaddr;
      }
      break;

    default:
      {
	switch (DOT_TYPE (dot))
          {
	  case D_MODIFY_EXPR:
	    retval = dot_pass_genModifyExpr (dot, block, context);
	    break;

	  case D_ADD_EXPR:
	  case D_MINUS_EXPR:
	  case D_LESS_EXPR:
	  case D_GREATER_EXPR:
	  case D_EQ_EQ_EXPR:
	    retval = dot_pass_genBinExpr (dot, block, context);
	    break;

	  default:
	    error ("unhandled operation type!\n");
	    break;
	  }
      }
      break;
    }
  return retval;
}

static
VEC(tree,gc) * dot_pass_genClass (gpy_dot_tree_t * dot,
				  dot_context_t context,
				  tree class_type,
				  const char * pid)
{
  VEC(tree,gc) * lowered_decls = VEC_alloc (tree,gc,0);

  tree class_type_ptr = build_pointer_type (class_type);
  tree fntype = build_function_type_list (void_type_node,
					  class_type_ptr,
					  NULL_TREE);
  tree ident = dot_pass_concat_identifier (GPY_current_module_name,
					   DOT_IDENTIFIER_POINTER (DOT_FIELD (dot)));
  ident = dot_pass_concat_identifier (IDENTIFIER_POINTER (ident),
				      "__field_init__");
  tree fndecl = dot_pass_genFndecl_Basic (ident, fntype);
  current_function_decl = fndecl;

  tree arglist = NULL_TREE;
  tree self_parm_decl = build_decl (BUILTINS_LOCATION, PARM_DECL,
                                    get_identifier ("__object_state__"),
                                    class_type_ptr);
  DECL_CONTEXT (self_parm_decl) = fndecl;
  DECL_ARG_TYPE (self_parm_decl) = TREE_VALUE (TYPE_ARG_TYPES (TREE_TYPE (fndecl)));
  TREE_READONLY (self_parm_decl) = 1;
  arglist = chainon (arglist, self_parm_decl);
  TREE_USED (self_parm_decl) = 1;
  DECL_ARGUMENTS (fndecl) = arglist;

  gpy_hash_tab_t field_type_namespace;
  gpy_dd_hash_init_table (&field_type_namespace);
  dot_pass_setupClassFieldContext (class_type, self_parm_decl, &field_type_namespace);

  VEC_safe_push (dot_context, gc, context, &field_type_namespace);

  tree block = alloc_stmt_list ();
  gpy_dot_tree_t * node;
  for (node = DOT_lhs_TT (dot); node != NULL_DOT;
       node = DOT_CHAIN (node))
    {
      if (DOT_T_FIELD (node) ==  D_D_EXPR)
	{
	  dot_pass_lowerExpr (node, context, &block);
	  continue;
	}
      switch (DOT_TYPE (node))
	{
	case D_PRINT_STMT:
	  dot_pass_genPrintStmt (node, &block, context);
	  break;

	case D_STRUCT_METHOD:
	  {
	    char * modID = DOT_IDENTIFIER_POINTER (DOT_FIELD (dot));
	    const char * npid = dot_pass_concat (pid, modID);
	    tree attrib = dot_pass_genFunction (node, context, npid);
	    VEC_safe_push (tree, gc, lowered_decls, attrib);
	  }
	  break;

	default:
	  error ("unhandled syntax within class!\n");
	  break;
	}
    }
  tree bind = NULL_TREE;
  tree bl = build_block (DECL_RESULT (fndecl), NULL_TREE, fndecl, NULL_TREE);
  DECL_INITIAL (fndecl) = bl;
  TREE_USED (bl) = 1;

  bind = build3 (BIND_EXPR, void_type_node, BLOCK_VARS(bl),
		 NULL_TREE, bl);
  TREE_SIDE_EFFECTS (bind) = 1;
  /* Finalize the main function */
  BIND_EXPR_BODY (bind) = block;
  block = bind;
  DECL_SAVED_TREE (fndecl) = block;

  gimplify_function_tree (fndecl);
  cgraph_finalize_function (fndecl, false);

  VEC_safe_push (tree, gc, lowered_decls, fndecl);

  VEC_pop (dot_context, context);
  return lowered_decls;
}

static
tree dot_pass_genFunction (gpy_dot_tree_t * dot,
			   VEC(dot_context, gc) * context,
			   const char * parentID)
{
  /* setup next context */
  gpy_hash_tab_t ctx;
  gpy_dd_hash_init_table (&ctx);
  VEC_safe_push (dot_context, gc, context, &ctx);

  gpy_dot_tree_t * pnode;
  tree params = NULL_TREE;
  for (pnode = DOT_lhs_TT (dot); pnode != NULL_DOT;
       pnode = DOT_CHAIN (pnode))
    chainon (params, tree_cons (NULL_TREE, gpy_object_type_ptr, NULL_TREE));
  chainon (params, tree_cons (NULL_TREE, void_type_node, NULL_TREE));

  tree fntype = build_function_type_list (void_type_node, params);
  tree ident =  dot_pass_concat_identifier (parentID,
					    DOT_IDENTIFIER_POINTER (DOT_FIELD (dot)));
  tree fndecl = dot_pass_genFndecl_Basic (ident, fntype);
  current_function_decl = fndecl;

  tree arglist = NULL_TREE;
  for (pnode = DOT_lhs_TT (dot); pnode != NULL_DOT;
       pnode = DOT_CHAIN (pnode))
    {
      const char * parmid = DOT_IDENTIFIER_POINTER (pnode);
      tree parm_decl = build_decl (BUILTINS_LOCATION, PARM_DECL,
				   get_identifier (parmid),
				   gpy_object_type_ptr);
      DECL_CONTEXT (parm_decl) = fndecl;
      DECL_ARG_TYPE (parm_decl) = gpy_object_type_ptr;
      TREE_READONLY (parm_decl) = 1;
      arglist = chainon (arglist, parm_decl);
      TREE_USED (parm_decl) = 1;

      gcc_assert (dot_pass_pushDecl (parm_decl, parmid, &ctx));
    }
  DECL_ARGUMENTS (fndecl) = arglist;

  tree block = alloc_stmt_list ();
  gpy_dot_tree_t * node;
  for (node = DOT_rhs_TT (dot); node != NULL_DOT;
       node = DOT_CHAIN (node))
    {
      if (DOT_T_FIELD (node) ==  D_D_EXPR)
	{
	  dot_pass_lowerExpr (node, context, &block);
	  continue;
	}
      switch (DOT_TYPE (node))
	{
	case D_PRINT_STMT:
	  dot_pass_genPrintStmt (node, &block, context);
	  break;

	case D_STRUCT_CONDITIONAL:
	  dot_pass_genConditional (node, &block, context);
	  break;

	default:
	  error ("unhandled syntax within toplevel function!\n");
	  break;
	}
    }
  tree bind = NULL_TREE;
  tree bl = build_block (DECL_RESULT (fndecl), NULL_TREE, fndecl, NULL_TREE);
  DECL_INITIAL (fndecl) = bl;
  TREE_USED (bl) = 1;

  bind = build3 (BIND_EXPR, void_type_node, BLOCK_VARS(bl),
		 NULL_TREE, bl);
  TREE_SIDE_EFFECTS (bind) = 1;
  /* Finalize the main function */
  BIND_EXPR_BODY (bind) = block;
  block = bind;
  DECL_SAVED_TREE (fndecl) = block;

  gimplify_function_tree (fndecl);
  cgraph_finalize_function (fndecl, false);

  VEC_pop (dot_context, context);
  return fndecl;
}

static
void dot_pass_generic_TU (gpy_hash_tab_t * types,
			  VEC(gpydot,gc) * decls,
			  VEC(tree,gc) * generic)
{
  gpy_hash_tab_t toplvl, topnxt;
  gpy_dd_hash_init_table (&toplvl);
  gpy_dd_hash_init_table (&topnxt);

  dot_context_t context = VEC_alloc (dot_context, gc, 0);
  VEC_safe_push (dot_context, gc, context, &toplvl);
  VEC_safe_push (dot_context, gc, context, &topnxt);

  tree block = alloc_stmt_list ();
  tree module = dot_pass_getModuleType (GPY_current_module_name, types);
  dot_pass_setupContext (module, context, generic, &block);

  tree fntype = build_function_type_list (void_type_node, NULL_TREE);
  tree ident =  dot_pass_concat_identifier (GPY_current_module_name,
					    "__main_start__");
  tree fndecl = dot_pass_genFndecl_Basic (ident, fntype);
  current_function_decl = fndecl;

  int i;
  gpy_dot_tree_t * dot = NULL_DOT;
  for (i = 0; VEC_iterate (gpydot, decls, i, dot); ++i)
    {
      if (DOT_T_FIELD (dot) ==  D_D_EXPR)
	{
	  dot_pass_lowerExpr (dot, context, &block);
	  continue;
	}

      switch (DOT_TYPE (dot))
        {
	case D_PRINT_STMT:
	  dot_pass_genPrintStmt (dot, &block, context);
	  break;

	case D_STRUCT_CONDITIONAL:
	  dot_pass_genConditional (dot, &block, context);
	  break;

        case D_STRUCT_METHOD:
	  {
	    tree func = dot_pass_genFunction (dot, context, GPY_current_module_name);
	    /* assign the function to the decl */
	    const char * funcid = DOT_IDENTIFIER_POINTER (DOT_FIELD (dot));
	    tree funcdecl = dot_pass_lookupDecl (context, funcid);
	    int n = 0;
	    gpy_dot_tree_t * pnode;
	    for (pnode = DOT_lhs_TT (dot); pnode != NULL_DOT;
		 pnode = DOT_CHAIN (pnode))
	      n++;
	    tree nargs = build_int_cst (integer_type_node, n);
	    tree str = gpy_dot_type_const_string_tree (funcid);
	    tree fold_functor = GPY_RR_fold_func_decl (build_fold_addr_expr (str),
						       func, nargs);
	    tree decl_func = build_decl (UNKNOWN_LOCATION, VAR_DECL,
					 create_tmp_var_name ("FN"),
					 gpy_object_type_ptr);
	    append_to_statement_list (build2 (MODIFY_EXPR, gpy_object_type_ptr,
					      decl_func, fold_functor),
				      &block);
	    append_to_statement_list (build2 (MODIFY_EXPR, gpy_object_type_ptr,
					      build_fold_indirect_ref (funcdecl),
					      decl_func),
				      &block);
	    VEC_safe_push (tree, gc, generic, func);
	  }
	  break;

	case D_STRUCT_CLASS:
	  {
	    char * classID =  dot_pass_concat (GPY_current_module_name,
					       DOT_IDENTIFIER_POINTER (DOT_FIELD (dot)));
	    tree classType = dot_pass_getModuleType (classID, types);
	    VEC(tree,gc) * cdecls = dot_pass_genClass (dot, context, classType,
						       GPY_current_module_name);
	    GPY_VEC_stmts_append (tree, generic, cdecls);
	    tree class_decl_ptr = dot_pass_lookupDecl (context,
						       DOT_IDENTIFIER_POINTER (DOT_FIELD (dot)));
	    dot_pass_genWalkClass (&block, classType, class_decl_ptr, cdecls);
	  }
	  break;

	default:
	  fatal_error ("unhandled tree!\n");
	  break;
	}
    }
  tree entry_fntype = gpy_unsigned_char_ptr;
  tree entry_decl = build_decl (BUILTINS_LOCATION, VAR_DECL,
				get_identifier (GPY_RR_entry),
				entry_fntype);
  TREE_STATIC (entry_decl) = 1;
  TREE_PUBLIC (entry_decl) = 1;
  TREE_USED (entry_decl) = 1;
  DECL_ARTIFICIAL (entry_decl) = 1;
  DECL_EXTERNAL (entry_decl) = 0;
  DECL_INITIAL (entry_decl) = build_fold_addr_expr (fndecl);
  rest_of_decl_compilation (entry_decl, 1, 0);

  tree bind = NULL_TREE;
  tree bl = build_block (DECL_RESULT (fndecl), NULL_TREE, fndecl, NULL_TREE);
  DECL_INITIAL (fndecl) = bl;
  TREE_USED (bl) = 1;

  bind = build3 (BIND_EXPR, void_type_node, BLOCK_VARS(bl),
		 NULL_TREE, bl);
  TREE_SIDE_EFFECTS (bind) = 1;
  /* Finalize the main function */
  BIND_EXPR_BODY (bind) = block;
  block = bind;
  DECL_SAVED_TREE (fndecl) = block;

  gimplify_function_tree (fndecl);
  cgraph_finalize_function (fndecl, false);

  /* Here we need to write out export data to object */
  char main_HEADER[512];
  snprintf (main_HEADER, 511, "GPY MODULE NAME %s",
	    GPY_current_module_name );
  const char * main_FNDECL = IDENTIFIER_POINTER (DECL_NAME (fndecl));
  gpy_write_export_data (main_HEADER, strlen (main_HEADER));
  gpy_write_export_data (main_FNDECL, strlen (main_FNDECL));

  VEC_safe_push (tree, gc, generic, fndecl);
  VEC_safe_push (tree, gc, generic, entry_decl);
}

VEC(tree,gc) * dot_pass_genericify (VEC(tree,gc) * modules,
				    VEC(gpydot,gc) * decls)
{
  VEC(tree,gc) * retval = VEC_alloc (tree, gc, 0);
  gpy_hash_tab_t types;
  gpy_dd_hash_init_table (&types);

  int i;
  tree type = NULL_TREE;
  for (i = 0; VEC_iterate (tree, modules, i, type); ++i)
    {
      gpy_hashval_t h = gpy_dd_hash_string (IDENTIFIER_POINTER (TYPE_NAME(type)));
      void ** e = gpy_dd_hash_insert (h, type, &types);
      if (e)
        fatal_error ("module <%s> is already defined!\n",
		     IDENTIFIER_POINTER (DECL_NAME (type)));
    }

  debug (" **** Lowering pyDOT IL to GENERIC for GCC middle end\n");
  dot_pass_generic_TU (&types, decls, retval);
  debug (" **** Finished Lowering pyDOT IL to GENERIC\n");

  if (types.array)
    free (types.array);
  return retval;
}
