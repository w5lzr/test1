/********************************************************************
** Copyright (c) 2018-2020 Guan Wenliang
** This file is part of the Berry default interpreter.
** skiars@qq.com, https://github.com/Skiars/berry
** See Copyright Notice in the LICENSE file or at
** https://github.com/Skiars/berry/blob/master/LICENSE
********************************************************************/
#include "be_object.h"
#include "be_func.h"
#include "be_exec.h"
#include "be_map.h"
#include "be_vm.h"

#define map_check_data(vm, argc)                        \
    if (!be_ismap(vm, -1) || be_top(vm) - 1 < argc) {   \
        be_return_nil(vm);                              \
    }

#define map_check_ref(vm)                               \
    if (be_refcontains(vm, 1)) {                        \
        be_pushstring(vm, "{...}");                     \
        be_return(vm);                                  \
    }

static int m_init(bvm *vm)
{
    if (be_top(vm) > 1 && be_ismap(vm, 2)) {
        be_pushvalue(vm, 2);
        be_setmember(vm, 1, ".p");
    } else {
        be_newmap(vm);
        be_setmember(vm, 1, ".p");
    }
    be_return_nil(vm);
}

static void push_key(bvm *vm)
{
    be_toescape(vm, -2, 'x'); /* escape string */
    be_pushvalue(vm, -2); /* push to top */
    be_strconcat(vm, -5);
    be_pop(vm, 1);
}

static void push_value(bvm *vm)
{
    be_toescape(vm, -1, 'x'); /* escape string */
    be_strconcat(vm, -4);
    be_pop(vm, 2);
    if (be_iter_hasnext(vm, -3)) {
        be_pushstring(vm, ", ");
        be_strconcat(vm, -3);
        be_pop(vm, 1);
    }
}

static int m_tostring(bvm *vm)
{
    be_getmember(vm, 1, ".p");
    map_check_data(vm, 1);
    map_check_ref(vm);
    be_refpush(vm, 1);
    be_pushstring(vm, "{");
    be_pushiter(vm, -2); /* map iterator use 1 register */
    while (be_iter_hasnext(vm, -3)) {
        be_iter_next(vm, -3);
        push_key(vm); /* key.tostring() */
        be_pushstring(vm, ": "); /* add ': ' */
        be_strconcat(vm, -5);
        be_pop(vm, 1);
        push_value(vm); /* value.tostring() */
    }
    be_pop(vm, 1); /* pop iterator */
    be_pushstring(vm, "}");
    be_strconcat(vm, -2);
    be_pop(vm, 1);
    be_refpop(vm);
    be_return(vm);
}

static int m_remove(bvm *vm)
{
    be_getmember(vm, 1, ".p");
    map_check_data(vm, 2);
    be_pushvalue(vm, 2);
    be_data_remove(vm, -2);
    be_return_nil(vm);
}

static int m_item(bvm *vm)
{
    be_getmember(vm, 1, ".p");
    map_check_data(vm, 2);
    be_pushvalue(vm, 2);
    if (!be_getindex(vm, -2)) {
        be_raise(vm, "key_error", be_tostring(vm, 2));
    }
    be_return(vm);
}

static int m_setitem(bvm *vm)
{
    be_getmember(vm, 1, ".p");
    map_check_data(vm, 3);
    be_pushvalue(vm, 2);
    be_pushvalue(vm, 3);
    be_setindex(vm, -3);
    be_return_nil(vm);
}

static int m_find(bvm *vm)
{
    int argc = be_top(vm);
    be_getmember(vm, 1, ".p");
    map_check_data(vm, 2);
    be_pushvalue(vm, 2);
    /* not find and has default value */
    if (!be_getindex(vm, -2) && argc >= 3) {
        be_pushvalue(vm, 3);
    }
    be_return(vm);
}

static int m_has(bvm *vm)
{
    be_getmember(vm, 1, ".p");
    map_check_data(vm, 2);
    be_pushvalue(vm, 2);
    be_pushbool(vm, be_getindex(vm, -2));
    be_return(vm);
}

static int m_insert(bvm *vm)
{
    bbool res;
    be_getmember(vm, 1, ".p");
    map_check_data(vm, 3);
    be_pushvalue(vm, 2);
    be_pushvalue(vm, 3);
    res = be_data_insert(vm, -3);
    be_pushbool(vm, res);
    be_return(vm);
}

static int m_size(bvm *vm)
{
    be_getmember(vm, 1, ".p");
    map_check_data(vm, 1);
    be_pushint(vm, be_data_size(vm, -1));
    be_return(vm);
}

static int iter_closure(bvm *vm)
{
    /* for better performance, we operate the upvalues
     * directly without using by the stack. */
    bntvclos *func = var_toobj(vm->cf->func);
    bvalue *uv0 = be_ntvclos_upval(func, 0)->value; /* list value */
    bvalue *uv1 = be_ntvclos_upval(func, 1)->value; /* iter value */
    bmapiter iter = var_toobj(uv1);
    bmapnode *next = be_map_next(var_toobj(uv0), &iter);
    if (next == NULL) {
        be_stop_iteration(vm);
        be_return_nil(vm); /* will not be executed */
    }
    var_setobj(uv1, BE_COMPTR, iter); /* set upvale[1] (iter value) */
    /* push next value to top */
    var_setval(vm->top, &next->value);
    be_incrtop(vm);
    be_return(vm);
}

static int m_iter(bvm *vm)
{
    be_pushntvclosure(vm, iter_closure, 2);
    be_getmember(vm, 1, ".p");
    be_setupval(vm, -2, 0);
    be_pushiter(vm, -1);
    be_setupval(vm, -3, 1);
    be_pop(vm, 2);
    be_return(vm);
}

static int keys_iter_closure(bvm *vm)
{
    /* for better performance, we operate the upvalues
     * directly without using by the stack. */
    bntvclos *func = var_toobj(vm->cf->func);
    bvalue *uv0 = be_ntvclos_upval(func, 0)->value; /* list value */
    bvalue *uv1 = be_ntvclos_upval(func, 1)->value; /* iter value */
    bmapiter iter = var_toobj(uv1);
    bmapnode *next = be_map_next(var_toobj(uv0), &iter);
    if (next == NULL) {
        be_stop_iteration(vm);
        be_return_nil(vm); /* will not be executed */
    }
    var_setobj(uv1, BE_COMPTR, iter); /* set upvale[1] (iter value) */
    /* push next value to top */
    var_setobj(vm->top, next->key.type, next->key.v.p);
    be_incrtop(vm);
    be_return(vm);
}

static int m_keys(bvm *vm)
{
    be_pushntvclosure(vm, keys_iter_closure, 2);
    be_getmember(vm, 1, ".p");
    be_setupval(vm, -2, 0);
    be_pushiter(vm, -1);
    be_setupval(vm, -3, 1);
    be_pop(vm, 2);
    be_return(vm);
}

/* apply a function/closure to each element of a map */
/* `map.reduce(f:function [, initializer:any]) -> any` */
/* Calls for each element `f(key, value, acc) -> any` */
/* `acc` is initialized with `initilizer` if present or `nil` */
/* the return value of the function becomes the next value passed in arg `acc` */
static int m_reduce(bvm *vm)
{
    int argc = be_top(vm);
    if (argc > 1 && be_isfunction(vm, 2)) {
        bbool has_initializer = (argc > 2);
        /* get map internal object */
        be_getmember(vm, 1, ".p");
        bvalue *v = be_indexof(vm, -1);
        bmap *map = cast(bmap*, var_toobj(v));
        /* get the number of slots if any */
        int slots_initial = be_map_size(map);
        /* place-holder for on-going value and return value */
        if (has_initializer) {
            be_pushvalue(vm, 3);
        } else {
            be_pushnil(vm);     /* if no initializer use `nil` */
        }
        for (int i = 0; i < slots_initial; i++) {
            bmapnode * node = map->slots + i;
            if (!var_isnil(&node->key)) {   /* is the key present in this slot? */
                be_pushvalue(vm, 2);        /* push function */

                bvalue kv;                  /* push key on stack */
                kv.type = node->key.type;
                kv.v = node->key.v;
                bvalue *reg = vm->top; 
                var_setval(reg, &kv);
                be_incrtop(vm);

                reg = vm->top;              /* push value on stack */
                var_setval(reg, &node->value);
                be_incrtop(vm);

                be_pushvalue(vm, -4);

                be_call(vm, 3);
                be_pop(vm, 3);   /* pop args, keep return value */
                be_remove(vm, -2);  /* remove previous accumulator, keep return value from function */
            }
            /* check if the map has been resized during the call */
            if (be_map_size(map) != slots_initial) {
                be_raise(vm, "stop_iteration", "map resized within apply");
                break;      /* abort */
            }
        }
        be_return(vm);
    }
    be_raise(vm, "value_error", "needs function as first argument");
    be_return_nil(vm);
}

#if !BE_USE_PRECOMPILED_OBJECT
void be_load_maplib(bvm *vm)
{
    static const bnfuncinfo members[] = {
        { ".p", NULL },
        { "init", m_init },
        { "tostring", m_tostring },
        { "remove", m_remove },
        { "item", m_item },
        { "setitem", m_setitem },
        { "find", m_find },
        { "size", m_size },
        { "insert", m_insert },
        { "iter", m_iter },
        { "keys", m_keys },
        { "reduce", m_reduce },
        { NULL, NULL }
    };
    be_regclass(vm, "map", members);
}
#else
/* @const_object_info_begin
class be_class_map (scope: global, name: map) {
    .p, var
    init, func(m_init)
    tostring, func(m_tostring)
    remove, func(m_remove)
    item, func(m_item)
    setitem, func(m_setitem)
    find, func(m_find)
    has, func(m_has)
    size, func(m_size)
    insert, func(m_insert)
    iter, func(m_iter)
    keys, func(m_keys)
    reduce, func(m_reduce)
}
@const_object_info_end */
#include "../generate/be_fixed_be_class_map.h"
#endif
