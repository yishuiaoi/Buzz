#include "buzzio.h"
#include <stdio.h>
#include <errno.h>
#include <string.h>

/****************************************/
/****************************************/

#define function_register(TABLE, FNAME, FPOINTER)                       \
   buzzvm_push(vm, (TABLE));                                            \
   buzzvm_pushs(vm, buzzvm_string_register(vm, (FNAME)));               \
   buzzvm_pushcc(vm, buzzvm_function_register(vm, (FPOINTER)));         \
   buzzvm_tput(vm);

#define filehandle_get(VAR)                                 \
   buzzvm_lload(vm, 0);                                     \
   buzzvm_type_assert(vm, 1, BUZZTYPE_TABLE);               \
   buzzvm_pushs(vm, buzzvm_string_register(vm, "handle"));  \
   buzzvm_tget(vm);                                         \
   FILE* VAR = buzzvm_stack_at(vm, 1)->u.value;             \
   buzzvm_pop(vm);

/****************************************/
/****************************************/

static void buzzio_update_error(buzzvm_t vm) {
   /* Get table */
   buzzvm_pushs(vm, buzzvm_string_register(vm, "io"));
   buzzvm_gload(vm);
   buzzobj_t t = buzzvm_stack_at(vm, 1);
   /* Update error id */
   buzzvm_push(vm, t);
   buzzvm_pushs(vm, buzzvm_string_register(vm, "errno"));
   buzzvm_pushi(vm, errno);
   buzzvm_tput(vm);
   /* Update error message */
   buzzvm_push(vm, t);
   buzzvm_pushs(vm, buzzvm_string_register(vm, "error_message"));
   if(errno) buzzvm_pushs(vm, buzzvm_string_register(vm, strerror(errno)));
   else buzzvm_pushs(vm, buzzvm_string_register(vm, "No error"));
   buzzvm_tput(vm);
}

/****************************************/
/****************************************/

int buzzio_register(buzzvm_t vm) {
   /* Make "io" table */
   buzzobj_t t = buzzheap_newobj(vm->heap, BUZZTYPE_TABLE);
   /* Register methods */
   function_register(t, "fopen", buzzio_fopen);
   /* Register "io" table */
   buzzvm_pushs(vm, buzzvm_string_register(vm, "io"));
   buzzvm_push(vm, t);
   buzzvm_gstore(vm);
   /* Register error information */
   buzzio_update_error(vm);
   /* All done */
   return vm->state;
}

/****************************************/
/****************************************/

int buzzio_fopen(buzzvm_t vm) {
   /* Make sure two parameters have been passed */
   buzzvm_lnum_assert(vm, 2);
   /* Get file name */
   buzzvm_lload(vm, 1);
   buzzvm_type_assert(vm, 1, BUZZTYPE_STRING);
   const char* fname = buzzvm_stack_at(vm, 1)->s.value.str;
   buzzvm_pop(vm);
   /* Get open mode */
   buzzvm_lload(vm, 2);
   buzzvm_type_assert(vm, 1, BUZZTYPE_STRING);
   const char* fmode = buzzvm_stack_at(vm, 1)->s.value.str;
   buzzvm_pop(vm);
   /* Try to open the file */
   FILE* f = fopen(fname, fmode);
   /* Register error information */
   buzzio_update_error(vm);
   if(!f) {
      /* Error occurred, return nil */
      buzzvm_pushnil(vm);
   }
   else {
      /* Create new table */
      buzzobj_t t = buzzheap_newobj(vm->heap, BUZZTYPE_TABLE);
      /* Add file handle */
      buzzvm_push(vm, t);
      buzzvm_pushs(vm, buzzvm_string_register(vm, "handle"));
      buzzvm_pushu(vm, f);
      buzzvm_tput(vm);
      /* Add file name */
      buzzvm_push(vm, t);
      buzzvm_pushs(vm, buzzvm_string_register(vm, "name"));
      buzzvm_pushs(vm, buzzvm_string_register(vm, fname));
      buzzvm_tput(vm);
      /* Add methods */
      function_register(t, "close", buzzio_fclose);
      function_register(t, "size", buzzio_fsize);
      function_register(t, "foreach", buzzio_fforeach);
      function_register(t, "write", buzzio_fwrite);
      /* Push the table on the stack */
      buzzvm_push(vm, t);
   }
   /* All done */
   return buzzvm_ret1(vm);
}

/****************************************/
/****************************************/

int buzzio_fclose(buzzvm_t vm) {
   /* Make sure there are no parameters */
   buzzvm_lnum_assert(vm, 0);
   /* Get file handle */
   filehandle_get(f);
   /* Close the file */
   fclose(f);
   /* Register error information */
   buzzio_update_error(vm);
   /* All done */
   return buzzvm_ret0(vm);
}

/****************************************/
/****************************************/

int buzzio_fsize(buzzvm_t vm) {
   /* Make sure there are no parameters */
   buzzvm_lnum_assert(vm, 0);
   /* Get file handle */
   filehandle_get(f);
   /* Remember the current position */
   long int cur = ftell(f);
   /* Get the file size */
   fseek(f, 0, SEEK_END);
   long int sz = ftell(f);
   /* Go back to saved position */
   fseek(f, cur, SEEK_SET);
   /* Put size on the stack */
   buzzvm_pushi(vm, sz);
   /* Register error information */
   buzzio_update_error(vm);
   /* All done */
   return buzzvm_ret1(vm);
}

/****************************************/
/****************************************/

int buzzio_fforeach(buzzvm_t vm) {
   /* Make sure there is a parameter */
   buzzvm_lnum_assert(vm, 1);
   /* Get file handle */
   filehandle_get(f);
   /* Get closure */
   buzzvm_lload(vm, 1);
   buzzvm_type_assert(vm, 1, BUZZTYPE_CLOSURE);
   buzzobj_t c = buzzvm_stack_at(vm, 1);
   buzzvm_pop(vm);
   /* Go through the file lines */
   size_t len;
   char* line = fgetln(f, &len);
   int vmstate = vm->state;
   while(line && vmstate == BUZZVM_STATE_READY) {
      /* Add \0 at line end */
      line[len-1] = 0;
      /* Push arguments */
      buzzvm_push(vm, c);
      buzzvm_pushs(vm, buzzvm_string_register(vm, line));
      /* Call closure */
      vmstate = buzzvm_closure_call(vm, 1);
      /* Next line */
      line = fgetln(f, &len);
   }
   /* Register error information */
   buzzio_update_error(vm);
   /* All done */
   return buzzvm_ret0(vm);
}

/****************************************/
/****************************************/

int buzzio_fwrite(buzzvm_t vm) {
   /* Used to store the return value of fprintf */
   int err = 0;
   /* Get file handle */
   filehandle_get(f);
   /* Go through the arguments */
   for(int i = 1; err >= 0 && i < buzzdarray_size(vm->lsyms->syms); ++i) {
      /* Get argument */
      buzzvm_lload(vm, i);
      buzzobj_t o = buzzvm_stack_at(vm, 1);
      buzzvm_pop(vm);
      switch(o->o.type) {
         case BUZZTYPE_NIL:
            err = fprintf(f, "[nil]");
            break;
         case BUZZTYPE_INT:
            err = fprintf(f, "%d", o->i.value);
            break;
         case BUZZTYPE_FLOAT:
            err = fprintf(f, "%f", o->f.value);
            break;
         case BUZZTYPE_TABLE:
            err = fprintf(f, "[table with %" PRIu32" elems]", buzzdict_size(o->t.value));
            break;
         case BUZZTYPE_CLOSURE:
            if(o->c.value.isnative)
               err = fprintf(f, "[n-closure @%" PRId32 "]", o->c.value.ref);
            else
               err = fprintf(f, "[c-closure @%" PRId32 "]", o->c.value.ref);
            break;
         case BUZZTYPE_STRING:
            err = fprintf(f, "%s", o->s.value.str);
            break;
         case BUZZTYPE_USERDATA:
            err = fprintf(f, "[userdata @%p]", o->u.value);
            break;
         default:
            err = -1;
            break;
      }
   }
   /* Add newline at the end */
   if(err >= 0) fprintf(f, "\n");
   /* Register error information */
   buzzio_update_error(vm);
   /* All done */
   return buzzvm_ret0(vm);
}

/****************************************/
/****************************************/