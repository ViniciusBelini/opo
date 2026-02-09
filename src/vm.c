#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "vm.h"

void retain(Value val) {
    if (val.type == VAL_OBJ && val.as.obj != NULL) {
        val.as.obj->ref_count++;
    }
}

void release(Value val);

static void free_object(HeapObject* obj) {
    switch (obj->type) {
        case OBJ_STRING: {
            ObjString* string = (ObjString*)obj;
            free(string->chars);
            free(string);
            break;
        }
        case OBJ_ARRAY: {
            ObjArray* array = (ObjArray*)obj;
            for (int i = 0; i < array->count; i++) {
                release(array->items[i]);
            }
            free(array->items);
            free(array);
            break;
        }
        case OBJ_STRUCT: {
            ObjStruct* st = (ObjStruct*)obj;
            for (int i = 0; i < st->field_count; i++) {
                free(st->fields[i]);
                release(st->values[i]);
            }
            free(st->fields);
            free(st->values);
            free(st);
            break;
        }
        case OBJ_NATIVE: {
            free(obj);
            break;
        }
        case OBJ_MAP: {
            ObjMap* map = (ObjMap*)obj;
            for (int i = 0; i < map->capacity; i++) {
                if (map->entries[i].is_used) {
                    release(map->entries[i].key);
                    release(map->entries[i].value);
                }
            }
            free(map->entries);
            free(map);
            break;
        }
    }
}

void release(Value val) {
    if (val.type == VAL_OBJ && val.as.obj != NULL) {
        val.as.obj->ref_count--;
        if (val.as.obj->ref_count <= 0) {
            free_object(val.as.obj);
        }
    }
}

ObjString* allocate_string(VM* vm, const char* chars, int length) {
    (void)vm;
    ObjString* string = malloc(sizeof(ObjString));
    string->obj.type = OBJ_STRING;
    string->obj.ref_count = 0;
    string->chars = malloc(length + 1);
    if (chars != NULL) {
        memcpy(string->chars, chars, length);
    }
    string->chars[length] = '\0';
    string->length = length;
    return string;
}

ObjArray* allocate_array(VM* vm) {
    (void)vm;
    ObjArray* array = malloc(sizeof(ObjArray));
    array->obj.type = OBJ_ARRAY;
    array->obj.ref_count = 0;
    array->items = NULL;
    array->count = 0;
    array->capacity = 0;
    return array;
}

ObjMap* allocate_map(VM* vm) {
    (void)vm;
    ObjMap* map = malloc(sizeof(ObjMap));
    map->obj.type = OBJ_MAP;
    map->obj.ref_count = 0;
    map->capacity = 8;
    map->count = 0;
    map->entries = calloc(map->capacity, sizeof(MapEntry));
    return map;
}

static uint32_t hash_value(Value v) {
    switch (v.type) {
        case VAL_INT: return (uint32_t)v.as.i_val;
        case VAL_FLT: {
            union { double d; uint32_t u[2]; } conv;
            conv.d = v.as.f_val;
            return conv.u[0] ^ conv.u[1];
        }
        case VAL_BOOL: return (uint32_t)v.as.b_val;
        case VAL_OBJ:
            if (v.as.obj->type == OBJ_STRING) {
                ObjString* s = (ObjString*)v.as.obj;
                uint32_t hash = 2166136261u;
                for (int i = 0; i < s->length; i++) {
                    hash ^= (uint8_t)s->chars[i];
                    hash *= 16777619;
                }
                return hash;
            }
            return (uint32_t)(uintptr_t)v.as.obj;
        default: return 0;
    }
}

static bool values_equal(Value a, Value b) {
    if (a.type != b.type) return false;
    switch (a.type) {
        case VAL_INT: return a.as.i_val == b.as.i_val;
        case VAL_FLT: return a.as.f_val == b.as.f_val;
        case VAL_BOOL: return a.as.b_val == b.as.b_val;
        case VAL_OBJ:
            if (a.as.obj->type == OBJ_STRING && b.as.obj->type == OBJ_STRING) {
                return strcmp(((ObjString*)a.as.obj)->chars, ((ObjString*)b.as.obj)->chars) == 0;
            }
            return a.as.obj == b.as.obj;
        default: return false;
    }
}

static void map_set(ObjMap* map, Value key, Value value) {
    if (map->count >= map->capacity * 0.7) {
        int old_capacity = map->capacity;
        MapEntry* old_entries = map->entries;
        map->capacity *= 2;
        map->entries = calloc(map->capacity, sizeof(MapEntry));
        map->count = 0;
        for (int i = 0; i < old_capacity; i++) {
            if (old_entries[i].is_used) {
                map_set(map, old_entries[i].key, old_entries[i].value);
                // No need to release here because we are moving references, but wait...
                // map_set will retain them. So we should release the old ones.
                release(old_entries[i].key);
                release(old_entries[i].value);
            }
        }
        free(old_entries);
    }

    uint32_t hash = hash_value(key);
    int index = hash % map->capacity;
    while (map->entries[index].is_used) {
        if (values_equal(map->entries[index].key, key)) {
            release(map->entries[index].value);
            retain(value);
            map->entries[index].value = value;
            return;
        }
        index = (index + 1) % map->capacity;
    }

    retain(key);
    retain(value);
    map->entries[index].key = key;
    map->entries[index].value = value;
    map->entries[index].is_used = true;
    map->count++;
}

static Value map_get(ObjMap* map, Value key) {
    if (map->capacity == 0) return (Value){VAL_VOID, {0}};
    uint32_t hash = hash_value(key);
    int index = hash % map->capacity;
    int start = index;
    while (map->entries[index].is_used) {
        if (values_equal(map->entries[index].key, key)) {
            return map->entries[index].value;
        }
        index = (index + 1) % map->capacity;
        if (index == start) break;
    }
    return (Value){VAL_VOID, {0}};
}

static Value native_len(VM* vm, int arg_count, Value* args) {
    (void)vm;
    if (arg_count != 1) return (Value){VAL_VOID, {0}};
    Value obj = args[0];
    if (obj.type == VAL_OBJ || obj.type == VAL_MAP) {
        if (obj.as.obj->type == OBJ_STRING) return (Value){VAL_INT, {.i_val = ((ObjString*)obj.as.obj)->length}};
        if (obj.as.obj->type == OBJ_ARRAY) return (Value){VAL_INT, {.i_val = ((ObjArray*)obj.as.obj)->count}};
        if (obj.as.obj->type == OBJ_MAP) return (Value){VAL_INT, {.i_val = ((ObjMap*)obj.as.obj)->count}};
    }
    return (Value){VAL_INT, {.i_val = 0}};
}

static Value native_append(VM* vm, int arg_count, Value* args) {
    (void)vm;
    if (arg_count != 2) return (Value){VAL_VOID, {0}};
    Value obj = args[0];
    Value val = args[1];
    if (obj.type == VAL_OBJ && obj.as.obj->type == OBJ_ARRAY) {
        ObjArray* array = (ObjArray*)obj.as.obj;
        if (array->count >= array->capacity) {
            array->capacity = array->capacity < 8 ? 8 : array->capacity * 2;
            array->items = realloc(array->items, sizeof(Value) * array->capacity);
        }
        retain(val);
        array->items[array->count++] = val;
    }
    return obj;
}

static Value native_str(VM* vm, int arg_count, Value* args) {
    if (arg_count != 1) return (Value){VAL_VOID, {0}};
    Value val = args[0];
    char buf[128];
    if (val.type == VAL_INT) sprintf(buf, "%ld", val.as.i_val);
    else if (val.type == VAL_FLT) sprintf(buf, "%g", val.as.f_val);
    else if (val.type == VAL_BOOL) sprintf(buf, "%s", val.as.b_val ? "tru" : "fls");
    else if ((val.type == VAL_OBJ || val.type == VAL_MAP) && val.as.obj->type == OBJ_STRING) {
        retain(val);
        return val;
    }
    else if ((val.type == VAL_OBJ || val.type == VAL_MAP) && val.as.obj->type == OBJ_MAP) sprintf(buf, "<map of %d>", ((ObjMap*)val.as.obj)->count);
    else sprintf(buf, "<obj>");
    
    ObjString* s = allocate_string(vm, buf, (int)strlen(buf));
    return (Value){VAL_OBJ, {.obj = (HeapObject*)s}};
}

static Value native_readFile(VM* vm, int arg_count, Value* args) {
    if (arg_count != 1 || args[0].type != VAL_OBJ || args[0].as.obj->type != OBJ_STRING) return (Value){VAL_VOID, {0}};
    const char* path = ((ObjString*)args[0].as.obj)->chars;
    FILE* file = fopen(path, "rb");
    if (file == NULL) return (Value){VAL_VOID, {0}};
    fseek(file, 0L, SEEK_END);
    long size = ftell(file);
    rewind(file);
    char* buffer = malloc(size + 1);
    size_t bytes = fread(buffer, 1, size, file);
    (void)bytes;
    buffer[size] = '\0';
    fclose(file);
    ObjString* s = allocate_string(vm, buffer, (int)size);
    free(buffer);
    return (Value){VAL_OBJ, {.obj = (HeapObject*)s}};
}

static Value native_writeFile(VM* vm, int arg_count, Value* args) {
    (void)vm;
    if (arg_count != 2 || args[0].type != VAL_OBJ || args[0].as.obj->type != OBJ_STRING ||
        args[1].type != VAL_OBJ || args[1].as.obj->type != OBJ_STRING) return (Value){VAL_VOID, {0}};
    const char* path = ((ObjString*)args[0].as.obj)->chars;
    const char* content = ((ObjString*)args[1].as.obj)->chars;
    FILE* file = fopen(path, "wb");
    if (file == NULL) return (Value){VAL_BOOL, {.b_val = false}};
    fwrite(content, 1, strlen(content), file);
    fclose(file);
    return (Value){VAL_BOOL, {.b_val = true}};
}

static Value native_args(VM* vm, int arg_count, Value* args) {
    (void)arg_count; (void)args;
    ObjArray* array = allocate_array(vm);
    array->items = malloc(sizeof(Value) * vm->argc);
    array->count = vm->argc;
    array->capacity = vm->argc;
    for (int i = 0; i < vm->argc; i++) {
        ObjString* s = allocate_string(vm, vm->argv[i], (int)strlen(vm->argv[i]));
        array->items[i] = (Value){VAL_OBJ, {.obj = (HeapObject*)s}};
        retain(array->items[i]);
    }
    return (Value){VAL_OBJ, {.obj = (HeapObject*)array}};
}

static Value native_int(VM* vm, int arg_count, Value* args) {
    (void)vm;
    if (arg_count != 1) return (Value){VAL_VOID, {0}};
    Value val = args[0];
    if (val.type == VAL_INT) return val;
    if (val.type == VAL_FLT) return (Value){VAL_INT, {.i_val = (int64_t)val.as.f_val}};
    if ((val.type == VAL_OBJ || val.type == VAL_MAP) && val.as.obj->type == OBJ_STRING) {
        return (Value){VAL_INT, {.i_val = strtoll(((ObjString*)val.as.obj)->chars, NULL, 10)}};
    }
    return (Value){VAL_INT, {.i_val = 0}};
}

static Value native_print(VM* vm, int arg_count, Value* args) {
    for (int i = 0; i < arg_count; i++) {
        Value s = native_str(vm, 1, &args[i]);
        if ((s.type == VAL_OBJ || s.type == VAL_MAP) && s.as.obj->type == OBJ_STRING) {
            printf("%s", ((ObjString*)s.as.obj)->chars);
        }
        release(s);
    }
    return (Value){VAL_VOID, {0}};
}

static Value native_println(VM* vm, int arg_count, Value* args) {
    native_print(vm, arg_count, args);
    printf("\n");
    return (Value){VAL_VOID, {0}};
}

static Value native_readLine(VM* vm, int arg_count, Value* args) {
    (void)arg_count; (void)args;
    char buf[1024];
    if (fgets(buf, sizeof(buf), stdin)) {
        size_t len = strlen(buf);
        if (len > 0 && buf[len-1] == '\n') {
            buf[len-1] = '\0';
            len--;
        }
        ObjString* s = allocate_string(vm, buf, (int)len);
        return (Value){VAL_OBJ, {.obj = (HeapObject*)s}};
    }
    return (Value){VAL_OBJ, {.obj = (HeapObject*)allocate_string(vm, "", 0)}};
}

void vm_define_native(VM* vm, const char* name, NativeFn function, int index) {
    ObjNative* native = malloc(sizeof(ObjNative));
    native->obj.type = OBJ_NATIVE;
    native->obj.ref_count = 1;
    native->name = name;
    native->function = function;
    vm->locals[index] = (Value){VAL_OBJ, {.obj = (HeapObject*)native}};
}

void vm_init(VM* vm, uint8_t* code, char** strings, int strings_count, int argc, char** argv) {
    vm->code = code;
    vm->ip = 0;
    vm->stack_ptr = 0;
    vm->locals_ptr = 0;
    vm->frame_ptr = 1;
    vm->frames[0].locals_offset = 0;
    vm->frames[0].return_addr = -1;
    vm->strings = strings;
    vm->strings_count = strings_count;
    vm->argc = argc;
    vm->argv = argv;
    for (int i = 0; i < LOCALS_MAX; i++) {
        vm->locals[i].type = VAL_VOID;
    }
    
    vm_define_native(vm, "len", native_len, 0);
    vm_define_native(vm, "append", native_append, 1);
    vm_define_native(vm, "str", native_str, 2);
    vm_define_native(vm, "readFile", native_readFile, 3);
    vm_define_native(vm, "writeFile", native_writeFile, 4);
    vm_define_native(vm, "args", native_args, 5);
    vm_define_native(vm, "int", native_int, 6);
    vm_define_native(vm, "print", native_print, 7);
    vm_define_native(vm, "println", native_println, 8);
    vm_define_native(vm, "readLine", native_readLine, 9);
}

void vm_push(VM* vm, Value val) {
    if (vm->stack_ptr >= STACK_MAX) {
        fprintf(stderr, "Stack overflow\n");
        exit(1);
    }
    retain(val);
    vm->stack[vm->stack_ptr++] = val;
}

Value vm_pop(VM* vm) {
    if (vm->stack_ptr <= 0) {
        fprintf(stderr, "Stack underflow\n");
        exit(1);
    }
    return vm->stack[--vm->stack_ptr];
}

static int32_t read_int32(VM* vm) {
    int32_t val = 0;
    for (int i = 0; i < 4; i++) {
        val |= (int32_t)vm->code[vm->ip++] << (i * 8);
    }
    return val;
}

static int64_t read_int64(VM* vm) {
    int64_t val = 0;
    for (int i = 0; i < 8; i++) {
        val |= (int64_t)vm->code[vm->ip++] << (i * 8);
    }
    return val;
}

void vm_run(VM* vm) {
    while (true) {
        uint8_t instruction = vm->code[vm->ip++];
        switch (instruction) {
            case OP_HALT:
                return;
            case OP_PUSH_INT: {
                int64_t val = read_int64(vm);
                vm_push(vm, (Value){VAL_INT, {.i_val = val}});
                break;
            }
            case OP_PUSH_FLT: {
                union { double f; uint64_t u; } conv;
                conv.u = (uint64_t)read_int64(vm);
                vm_push(vm, (Value){VAL_FLT, {.f_val = conv.f}});
                break;
            }
            case OP_PUSH_STR: {
                int index = vm->code[vm->ip++];
                ObjString* s = allocate_string(vm, vm->strings[index], (int)strlen(vm->strings[index]));
                vm_push(vm, (Value){VAL_OBJ, {.obj = (HeapObject*)s}});
                break;
            }
            case OP_PUSH_BOOL: {
                bool val = vm->code[vm->ip++];
                vm_push(vm, (Value){VAL_BOOL, {.b_val = val}});
                break;
            }
            case OP_PRINT: {
                Value val = vm_pop(vm);
                switch (val.type) {
                    case VAL_INT: printf("%ld\n", val.as.i_val); break;
                    case VAL_FLT: printf("%g\n", val.as.f_val); break;
                    case VAL_BOOL: printf(val.as.b_val ? "tru\n" : "fls\n"); break;
                    case VAL_STR: printf("%s\n", vm->strings[val.as.s_idx]); break;
                    case VAL_VOID: printf("void\n"); break;
                    case VAL_FUNC:
                    case VAL_FUNC_INT:
                    case VAL_FUNC_FLT:
                    case VAL_FUNC_BOOL:
                    case VAL_FUNC_STR:
                    case VAL_FUNC_VOID:
                    case VAL_IMP:
                        printf("<fun at %ld>\n", val.as.i_val);
                        break;
                    case VAL_MAP:
                        printf("<map>\n");
                        break;
                    case VAL_ANY:
                        printf("<any>\n");
                        break;
                    case VAL_OBJ: {
                        HeapObject* obj = val.as.obj;
                        switch (obj->type) {
                            case OBJ_STRING: printf("%s\n", ((ObjString*)obj)->chars); break;
                            case OBJ_ARRAY: printf("<array of %d>\n", ((ObjArray*)obj)->count); break;
                            case OBJ_STRUCT: printf("<struct>\n"); break;
                            case OBJ_NATIVE: printf("<native fun %s>\n", ((ObjNative*)obj)->name); break;
                            case OBJ_MAP: printf("<map of %d>\n", ((ObjMap*)obj)->count); break;
                        }
                        break;
                    }
                }
                release(val);
                break;
            }
            case OP_LTE: {
                Value b = vm_pop(vm);
                Value a = vm_pop(vm);
                if (a.type == VAL_INT && b.type == VAL_INT) {
                    vm_push(vm, (Value){VAL_BOOL, {.b_val = a.as.i_val <= b.as.i_val}});
                } else if (a.type == VAL_FLT && b.type == VAL_FLT) {
                    vm_push(vm, (Value){VAL_BOOL, {.b_val = a.as.f_val <= b.as.f_val}});
                } else {
                    fprintf(stderr, "Type error in LTE\n");
                    exit(1);
                }
                release(a); release(b);
                break;
            }
            case OP_GTE: {
                Value b = vm_pop(vm);
                Value a = vm_pop(vm);
                if (a.type == VAL_INT && b.type == VAL_INT) {
                    vm_push(vm, (Value){VAL_BOOL, {.b_val = a.as.i_val >= b.as.i_val}});
                } else if (a.type == VAL_FLT && b.type == VAL_FLT) {
                    vm_push(vm, (Value){VAL_BOOL, {.b_val = a.as.f_val >= b.as.f_val}});
                } else {
                    fprintf(stderr, "Type error in GTE\n");
                    exit(1);
                }
                release(a); release(b);
                break;
            }
            case OP_NEG: {
                Value a = vm_pop(vm);
                if (a.type == VAL_INT) {
                    vm_push(vm, (Value){VAL_INT, {.i_val = -a.as.i_val}});
                } else if (a.type == VAL_FLT) {
                    vm_push(vm, (Value){VAL_FLT, {.f_val = -a.as.f_val}});
                } else {
                    fprintf(stderr, "Type error in NEG\n");
                    exit(1);
                }
                release(a);
                break;
            }
            case OP_MOD: {
                Value b = vm_pop(vm);
                Value a = vm_pop(vm);
                if (a.type == VAL_INT && b.type == VAL_INT) {
                    if (b.as.i_val == 0) {
                        fprintf(stderr, "Division by zero\n");
                        exit(1);
                    }
                    vm_push(vm, (Value){VAL_INT, {.i_val = a.as.i_val % b.as.i_val}});
                } else {
                    fprintf(stderr, "Type error in MOD\n");
                    exit(1);
                }
                release(a); release(b);
                break;
            }
            case OP_AND: {
                Value b = vm_pop(vm);
                Value a = vm_pop(vm);
                vm_push(vm, (Value){VAL_BOOL, {.b_val = a.as.b_val && b.as.b_val}});
                release(a); release(b);
                break;
            }
            case OP_OR: {
                Value b = vm_pop(vm);
                Value a = vm_pop(vm);
                vm_push(vm, (Value){VAL_BOOL, {.b_val = a.as.b_val || b.as.b_val}});
                release(a); release(b);
                break;
            }
            case OP_NOT: {
                Value a = vm_pop(vm);
                vm_push(vm, (Value){VAL_BOOL, {.b_val = !a.as.b_val}});
                release(a);
                break;
            }
            case OP_GT: {
                Value b = vm_pop(vm);
                Value a = vm_pop(vm);
                if (a.type == VAL_INT && b.type == VAL_INT) {
                    vm_push(vm, (Value){VAL_BOOL, {.b_val = a.as.i_val > b.as.i_val}});
                } else if (a.type == VAL_FLT && b.type == VAL_FLT) {
                    vm_push(vm, (Value){VAL_BOOL, {.b_val = a.as.f_val > b.as.f_val}});
                } else {
                    fprintf(stderr, "Type error in GT\n");
                    exit(1);
                }
                release(a); release(b);
                break;
            }
            case OP_ADD: {
                Value b = vm_pop(vm);
                Value a = vm_pop(vm);
                if (a.type == VAL_INT && b.type == VAL_INT) {
                    vm_push(vm, (Value){VAL_INT, {.i_val = a.as.i_val + b.as.i_val}});
                } else if (a.type == VAL_FLT && b.type == VAL_FLT) {
                    vm_push(vm, (Value){VAL_FLT, {.f_val = a.as.f_val + b.as.f_val}});
                } else if (a.type == VAL_OBJ && a.as.obj->type == OBJ_STRING && b.type == VAL_OBJ && b.as.obj->type == OBJ_STRING) {
                    ObjString* sa = (ObjString*)a.as.obj;
                    ObjString* sb = (ObjString*)b.as.obj;
                    int new_len = sa->length + sb->length;
                    ObjString* res = allocate_string(vm, NULL, new_len);
                    memcpy(res->chars, sa->chars, sa->length);
                    memcpy(res->chars + sa->length, sb->chars, sb->length);
                    vm_push(vm, (Value){VAL_OBJ, {.obj = (HeapObject*)res}});
                } else {
                    fprintf(stderr, "Type error in ADD\n");
                    exit(1);
                }
                release(a); release(b);
                break;
            }
            case OP_SUB: {
                Value b = vm_pop(vm);
                Value a = vm_pop(vm);
                if (a.type == VAL_INT && b.type == VAL_INT) {
                    vm_push(vm, (Value){VAL_INT, {.i_val = a.as.i_val - b.as.i_val}});
                } else if (a.type == VAL_FLT && b.type == VAL_FLT) {
                    vm_push(vm, (Value){VAL_FLT, {.f_val = a.as.f_val - b.as.f_val}});
                } else {
                    fprintf(stderr, "Type error in SUB\n");
                    exit(1);
                }
                release(a); release(b);
                break;
            }
            case OP_MUL: {
                Value b = vm_pop(vm);
                Value a = vm_pop(vm);
                if (a.type == VAL_INT && b.type == VAL_INT) {
                    vm_push(vm, (Value){VAL_INT, {.i_val = a.as.i_val * b.as.i_val}});
                } else if (a.type == VAL_FLT && b.type == VAL_FLT) {
                    vm_push(vm, (Value){VAL_FLT, {.f_val = a.as.f_val * b.as.f_val}});
                } else {
                    fprintf(stderr, "Type error in MUL\n");
                    exit(1);
                }
                release(a); release(b);
                break;
            }
            case OP_DIV: {
                Value b = vm_pop(vm);
                Value a = vm_pop(vm);
                if (a.type == VAL_INT && b.type == VAL_INT) {
                    if (b.as.i_val == 0) { fprintf(stderr, "Division by zero\n"); exit(1); }
                    vm_push(vm, (Value){VAL_INT, {.i_val = a.as.i_val / b.as.i_val}});
                } else if (a.type == VAL_FLT && b.type == VAL_FLT) {
                    if (b.as.f_val == 0) { fprintf(stderr, "Division by zero\n"); exit(1); }
                    vm_push(vm, (Value){VAL_FLT, {.f_val = a.as.f_val / b.as.f_val}});
                } else {
                    fprintf(stderr, "Type error in DIV\n");
                    exit(1);
                }
                release(a); release(b);
                break;
            }
            case OP_EQ: {
                Value b = vm_pop(vm);
                Value a = vm_pop(vm);
                bool res = false;
                if (a.type == b.type) {
                    if (a.type == VAL_INT) res = a.as.i_val == b.as.i_val;
                    else if (a.type == VAL_BOOL) res = a.as.b_val == b.as.b_val;
                    else if (a.type == VAL_OBJ && b.type == VAL_OBJ) {
                        if (a.as.obj->type == OBJ_STRING && b.as.obj->type == OBJ_STRING) {
                            res = strcmp(((ObjString*)a.as.obj)->chars, ((ObjString*)b.as.obj)->chars) == 0;
                        } else {
                            res = a.as.obj == b.as.obj;
                        }
                    }
                }
                vm_push(vm, (Value){VAL_BOOL, {.b_val = res}});
                release(a); release(b);
                break;
            }
            case OP_LT: {
                Value b = vm_pop(vm);
                Value a = vm_pop(vm);
                if (a.type == VAL_INT && b.type == VAL_INT) {
                    vm_push(vm, (Value){VAL_BOOL, {.b_val = a.as.i_val < b.as.i_val}});
                } else if (a.type == VAL_FLT && b.type == VAL_FLT) {
                    vm_push(vm, (Value){VAL_BOOL, {.b_val = a.as.f_val < b.as.f_val}});
                } else {
                    fprintf(stderr, "Type error in LT\n");
                    exit(1);
                }
                release(a); release(b);
                break;
            }
            case OP_STORE: {
                int index = vm->code[vm->ip++];
                int locals_offset = vm->frames[vm->frame_ptr-1].locals_offset;
                Value val = vm_pop(vm);
                release(vm->locals[locals_offset + index]);
                vm->locals[locals_offset + index] = val;
                break;
            }
            case OP_LOAD: {
                int index = vm->code[vm->ip++];
                int locals_offset = vm->frames[vm->frame_ptr-1].locals_offset;
                vm_push(vm, vm->locals[locals_offset + index]);
                break;
            }
            case OP_LOAD_G: {
                int index = vm->code[vm->ip++];
                vm_push(vm, vm->locals[index]);
                break;
            }
            case OP_POP: {
                release(vm_pop(vm));
                break;
            }
            case OP_JUMP_IF_F: {
                int32_t addr = read_int32(vm);
                Value cond = vm_pop(vm);
                bool b = cond.as.b_val;
                release(cond);
                if (!b) vm->ip = addr;
                break;
            }
            case OP_JUMP: {
                int32_t addr = read_int32(vm);
                vm->ip = addr;
                break;
            }
            case OP_CALL: {
                int32_t addr = read_int32(vm);
                if (vm->frame_ptr >= FRAMES_MAX) { fprintf(stderr, "Stack overflow (frames)\n"); exit(1); }
                int current_offset = vm->frame_ptr * LOCALS_PER_FRAME;
                CallFrame* frame = &vm->frames[vm->frame_ptr++];
                frame->return_addr = vm->ip;
                frame->locals_offset = current_offset;
                vm->ip = addr;
                break;
            }
            case OP_RET: {
                if (vm->frame_ptr <= 1) { fprintf(stderr, "Stack underflow (frames)\n"); exit(1); }
                CallFrame* frame = &vm->frames[--vm->frame_ptr];
                for (int i = 0; i < LOCALS_PER_FRAME; i++) {
                    release(vm->locals[frame->locals_offset + i]);
                    vm->locals[frame->locals_offset + i] = (Value){VAL_VOID, {0}};
                }
                vm->ip = frame->return_addr;
                break;
            }
            case OP_TYPEOF: {
                Value val = vm_pop(vm);
                int type_idx = (int)val.type;
                if (type_idx == VAL_OBJ) {
                    HeapObject* obj = val.as.obj;
                    switch (obj->type) {
                        case OBJ_STRING: type_idx = 3; break;
                        case OBJ_ARRAY: type_idx = 5; break;
                        case OBJ_STRUCT: type_idx = 5; break;
                        case OBJ_NATIVE: type_idx = 5; break;
                        case OBJ_MAP: type_idx = 5; break;
                    }
                }
                if (type_idx > 5) type_idx = 5;
                vm_push(vm, (Value){VAL_STR, {.s_idx = type_idx}});
                release(val);
                break;
            }
            case OP_PUSH_FUNC: {
                int64_t addr = read_int64(vm);
                uint8_t type = vm->code[vm->ip++];
                vm_push(vm, (Value){(ValueType)type, {.i_val = addr}});
                break;
            }
            case OP_INDEX: {
                Value index = vm_pop(vm);
                Value obj = vm_pop(vm);
                if (obj.type == VAL_OBJ && obj.as.obj->type == OBJ_ARRAY) {
                    ObjArray* array = (ObjArray*)obj.as.obj;
                    int idx = (int)index.as.i_val;
                    if (idx < 0 || idx >= array->count) { fprintf(stderr, "Array index out of bounds\n"); exit(1); }
                    vm_push(vm, array->items[idx]);
                } else if (obj.type == VAL_OBJ && obj.as.obj->type == OBJ_STRING) {
                    ObjString* s = (ObjString*)obj.as.obj;
                    int idx = (int)index.as.i_val;
                    if (idx < 0 || idx >= s->length) { fprintf(stderr, "String index out of bounds\n"); exit(1); }
                    char buf[2] = {s->chars[idx], '\0'};
                    ObjString* res = allocate_string(vm, buf, 1);
                    vm_push(vm, (Value){VAL_OBJ, {.obj = (HeapObject*)res}});
                } else if (obj.type == VAL_OBJ && obj.as.obj->type == OBJ_MAP) {
                    ObjMap* map = (ObjMap*)obj.as.obj;
                    Value val = map_get(map, index);
                    vm_push(vm, val);
                } else { fprintf(stderr, "Can only index arrays, strings or maps\n"); exit(1); }
                release(obj); release(index);
                break;
            }
            case OP_GET_MEMBER: {
                int field_idx = vm->code[vm->ip++];
                Value obj = vm_pop(vm);
                if (obj.type != VAL_OBJ || obj.as.obj->type != OBJ_STRUCT) { fprintf(stderr, "Can only get member\n"); exit(1); }
                ObjStruct* st = (ObjStruct*)obj.as.obj;
                vm_push(vm, st->values[field_idx]);
                release(obj);
                break;
            }
            case OP_SET_MEMBER: {
                int field_idx = vm->code[vm->ip++];
                Value obj = vm_pop(vm);
                Value val = vm_pop(vm);
                if (obj.type != VAL_OBJ || obj.as.obj->type != OBJ_STRUCT) { fprintf(stderr, "Can only set member\n"); exit(1); }
                ObjStruct* st = (ObjStruct*)obj.as.obj;
                release(st->values[field_idx]);
                retain(val);
                st->values[field_idx] = val;
                release(obj); release(val);
                break;
            }
            case OP_SET_INDEX: {
                Value index = vm_pop(vm);
                Value obj = vm_pop(vm);
                Value val = vm_pop(vm);
                if (obj.type == VAL_OBJ && obj.as.obj->type == OBJ_ARRAY) {
                    ObjArray* array = (ObjArray*)obj.as.obj;
                    int idx = (int)index.as.i_val;
                    if (idx < 0 || idx >= array->count) { fprintf(stderr, "Array index out of bounds\n"); exit(1); }
                    release(array->items[idx]);
                    retain(val);
                    array->items[idx] = val;
                } else if (obj.type == VAL_OBJ && obj.as.obj->type == OBJ_MAP) {
                    ObjMap* map = (ObjMap*)obj.as.obj;
                    map_set(map, index, val);
                } else { fprintf(stderr, "Can only set index on arrays or maps\n"); exit(1); }
                release(obj); release(index); release(val);
                break;
            }
            case OP_ARRAY: {
                int count = vm->code[vm->ip++];
                ObjArray* array = allocate_array(vm);
                array->items = malloc(sizeof(Value) * count);
                array->count = count; array->capacity = count;
                for (int i = count - 1; i >= 0; i--) array->items[i] = vm_pop(vm);
                vm_push(vm, (Value){VAL_OBJ, {.obj = (HeapObject*)array}});
                break;
            }
            case OP_STRUCT: {
                int field_count = vm->code[vm->ip++];
                ObjStruct* st = malloc(sizeof(ObjStruct));
                st->obj.type = OBJ_STRUCT; st->obj.ref_count = 0;
                st->field_count = field_count;
                st->fields = malloc(sizeof(char*) * field_count);
                st->values = malloc(sizeof(Value) * field_count);
                for (int i = field_count - 1; i >= 0; i--) {
                    st->values[i] = vm_pop(vm);
                    st->fields[i] = NULL;
                }
                vm_push(vm, (Value){VAL_OBJ, {.obj = (HeapObject*)st}});
                break;
            }
            case OP_MAP: {
                int pair_count = vm->code[vm->ip++];
                ObjMap* map = allocate_map(vm);
                for (int i = 0; i < pair_count; i++) {
                    Value val = vm_pop(vm);
                    Value key = vm_pop(vm);
                    map_set(map, key, val);
                    release(key); release(val);
                }
                vm_push(vm, (Value){VAL_OBJ, {.obj = (HeapObject*)map}});
                break;
            }
            case OP_INVOKE: {
                int arg_count = vm->code[vm->ip++];
                Value callable = vm_pop(vm);
                if (callable.type == VAL_OBJ && callable.as.obj->type == OBJ_NATIVE) {
                    ObjNative* native = (ObjNative*)callable.as.obj;
                    Value* args = &vm->stack[vm->stack_ptr - arg_count];
                    Value result = native->function(vm, arg_count, args);
                    retain(result);
                    for (int i = 0; i < arg_count; i++) release(vm_pop(vm));
                    vm_push(vm, result);
                    release(result); release(callable);
                } else if (callable.type >= VAL_FUNC || callable.type == VAL_INT) {
                    int32_t addr = (int32_t)callable.as.i_val;
                    release(callable);
                    if (vm->frame_ptr >= FRAMES_MAX) { fprintf(stderr, "Stack overflow\n"); exit(1); }
                    int current_offset = vm->frame_ptr * LOCALS_PER_FRAME;
                    CallFrame* frame = &vm->frames[vm->frame_ptr++];
                    frame->return_addr = vm->ip; frame->locals_offset = current_offset;
                    vm->ip = addr;
                } else { fprintf(stderr, "Can only invoke functions or natives. Type: %d\n", callable.type); exit(1); }
                break;
            }
            default:
                fprintf(stderr, "Unknown opcode %d\n", instruction);
                exit(1);
        }
    }
}
