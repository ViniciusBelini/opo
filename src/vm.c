#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <stdarg.h>
#include "vm.h"

void retain(Value val) {
    int kind = TYPE_KIND(val.type);
    if ((kind == VAL_OBJ || kind == VAL_MAP || kind == VAL_ENUM) && val.as.obj != NULL) {
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
        case OBJ_ENUM: {
            ObjEnum* en = (ObjEnum*)obj;
            free(en->enum_name);
            free(en->variant_name);
            if (en->has_payload) release(en->payload);
            free(en);
            break;
        }
    }
}

void release(Value val) {
    int kind = TYPE_KIND(val.type);
    if ((kind == VAL_OBJ || kind == VAL_MAP || kind == VAL_ENUM) && val.as.obj != NULL) {
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
    switch (TYPE_KIND(v.type)) {
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
    if (TYPE_KIND(a.type) != TYPE_KIND(b.type)) return false;
    switch (TYPE_KIND(a.type)) {
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
    if (TYPE_KIND(obj.type) == VAL_OBJ || TYPE_KIND(obj.type) == VAL_MAP) {
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
    if (TYPE_KIND(obj.type) == VAL_OBJ && obj.as.obj->type == OBJ_ARRAY) {
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
    char buf[1024]; // Larger buffer for collections
    if (TYPE_KIND(val.type) == VAL_INT) sprintf(buf, "%ld", val.as.i_val);
    else if (TYPE_KIND(val.type) == VAL_FLT) sprintf(buf, "%g", val.as.f_val);
    else if (TYPE_KIND(val.type) == VAL_BOOL) sprintf(buf, "%s", val.as.b_val ? "tru" : "fls");
    else if (TYPE_KIND(val.type) == VAL_VOID) strcpy(buf, "void");
    else if (TYPE_KIND(val.type) == VAL_STR || (TYPE_KIND(val.type) == VAL_OBJ && val.as.obj->type == OBJ_STRING)) {
        retain(val);
        return val;
    }
    else if (TYPE_KIND(val.type) == VAL_OBJ && val.as.obj->type == OBJ_ARRAY) {
        ObjArray* array = (ObjArray*)val.as.obj;
        strcpy(buf, "[");
        for (int i = 0; i < array->count; i++) {
            Value s = native_str(vm, 1, &array->items[i]);
            strcat(buf, ((ObjString*)s.as.obj)->chars);
            if (i < array->count - 1) strcat(buf, ", ");
            release(s);
        }
        strcat(buf, "]");
    }
    else if ((TYPE_KIND(val.type) == VAL_OBJ || TYPE_KIND(val.type) == VAL_MAP) && val.as.obj->type == OBJ_MAP) {
        ObjMap* map = (ObjMap*)val.as.obj;
        strcpy(buf, "{");
        bool first = true;
        for (int i = 0; i < map->capacity; i++) {
            if (map->entries[i].is_used) {
                if (!first) strcat(buf, ", ");
                Value sk = native_str(vm, 1, &map->entries[i].key);
                Value sv = native_str(vm, 1, &map->entries[i].value);
                strcat(buf, ((ObjString*)sk.as.obj)->chars);
                strcat(buf, " => ");
                strcat(buf, ((ObjString*)sv.as.obj)->chars);
                release(sk); release(sv);
                first = false;
            }
        }
        strcat(buf, "}");
    }
    else if (TYPE_KIND(val.type) == VAL_ERR) {
        Value inner = {VAL_OBJ, val.as};
        Value s = native_str(vm, 1, &inner);
        sprintf(buf, "Error: %s", ((ObjString*)s.as.obj)->chars);
        release(s);
    }
    else if (TYPE_KIND(val.type) == VAL_ENUM) {
        ObjEnum* en = (ObjEnum*)val.as.obj;
        if (TYPE_SUB(val.type) == OPTION_ENUM_ID) {
            if (en->variant_index == 0) strcpy(buf, "none");
            else {
                Value s = native_str(vm, 1, &en->payload);
                sprintf(buf, "some(%s)", ((ObjString*)s.as.obj)->chars);
                release(s);
            }
        } else {
            if (en->has_payload) {
                Value s = native_str(vm, 1, &en->payload);
                sprintf(buf, "enum.variant(%s)", ((ObjString*)s.as.obj)->chars);
                release(s);
            } else {
                sprintf(buf, "enum.variant");
            }
        }
    }
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
    if (TYPE_KIND(val.type) == VAL_INT) return val;
    if (TYPE_KIND(val.type) == VAL_FLT) return (Value){VAL_INT, {.i_val = (int64_t)val.as.f_val}};
    if ((TYPE_KIND(val.type) == VAL_OBJ || TYPE_KIND(val.type) == VAL_MAP) && val.as.obj->type == OBJ_STRING) {
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

static Value native_exit(VM* vm, int arg_count, Value* args) {
    (void)vm;
    int code = 0;
    if (arg_count > 0 && args[0].type == VAL_INT) code = (int)args[0].as.i_val;
    exit(code);
    return (Value){VAL_VOID, {0}};
}

static Value native_clock(VM* vm, int arg_count, Value* args) {
    (void)vm; (void)arg_count; (void)args;
    return (Value){VAL_FLT, {.f_val = (double)clock() / CLOCKS_PER_SEC}};
}

static Value native_system(VM* vm, int arg_count, Value* args) {
    (void)vm;
    if (arg_count != 1 || args[0].type != VAL_OBJ || args[0].as.obj->type != OBJ_STRING) return (Value){VAL_INT, {.i_val = -1}};
    int res = system(((ObjString*)args[0].as.obj)->chars);
    return (Value){VAL_INT, {.i_val = res}};
}

static Value native_keys(VM* vm, int arg_count, Value* args) {
    if (arg_count != 1 || args[0].type != VAL_OBJ || args[0].as.obj->type != OBJ_MAP) return (Value){VAL_VOID, {0}};
    ObjMap* map = (ObjMap*)args[0].as.obj;
    ObjArray* array = allocate_array(vm);
    array->items = malloc(sizeof(Value) * map->count);
    array->capacity = map->count;
    array->count = 0;
    for (int i = 0; i < map->capacity; i++) {
        if (map->entries[i].is_used) {
            array->items[array->count] = map->entries[i].key;
            retain(array->items[array->count]);
            array->count++;
        }
    }
    return (Value){VAL_OBJ, {.obj = (HeapObject*)array}};
}

static Value native_delete(VM* vm, int arg_count, Value* args) {
    (void)vm;
    if (arg_count != 2 || args[0].type != VAL_OBJ || args[0].as.obj->type != OBJ_MAP) return (Value){VAL_VOID, {0}};
    ObjMap* map = (ObjMap*)args[0].as.obj;
    Value key = args[1];
    
    if (map->capacity == 0) return (Value){VAL_VOID, {0}};
    uint32_t hash = hash_value(key);
    int index = hash % map->capacity;
    while (map->entries[index].is_used) {
        if (values_equal(map->entries[index].key, key)) {
            release(map->entries[index].key);
            release(map->entries[index].value);
            map->count--;
            
            // Re-hash the cluster to avoid breaking linear probing
            int i = index;
            int j = i;
            while (true) {
                j = (j + 1) % map->capacity;
                if (!map->entries[j].is_used) break;
                
                uint32_t k_hash = hash_value(map->entries[j].key);
                int k = k_hash % map->capacity;
                
                // Determine if k is cyclically between i and j
                bool between = false;
                if (i <= j) {
                    if (i < k && k <= j) between = true;
                } else {
                    if (i < k || k <= j) between = true;
                }
                
                if (!between) {
                    map->entries[i] = map->entries[j];
                    i = j;
                }
            }
            map->entries[i].is_used = false;
            map->entries[i].key = (Value){VAL_VOID, {0}};
            map->entries[i].value = (Value){VAL_VOID, {0}};
            return (Value){VAL_VOID, {0}};
        }
        index = (index + 1) % map->capacity;
    }
    return (Value){VAL_VOID, {0}};
}

static Value native_ascii(VM* vm, int arg_count, Value* args) {
    (void)vm;
    if (arg_count != 1 || args[0].type != VAL_OBJ || args[0].as.obj->type != OBJ_STRING) return (Value){VAL_INT, {.i_val = 0}};
    ObjString* s = (ObjString*)args[0].as.obj;
    if (s->length == 0) return (Value){VAL_INT, {.i_val = 0}};
    return (Value){VAL_INT, {.i_val = (uint8_t)s->chars[0]}};
}

static Value native_char(VM* vm, int arg_count, Value* args) {
    if (arg_count != 1 || TYPE_KIND(args[0].type) != VAL_INT) return (Value){VAL_VOID, {0}};
    char c = (char)args[0].as.i_val;
    ObjString* s = allocate_string(vm, &c, 1);
    return (Value){VAL_OBJ, {.obj = (HeapObject*)s}};
}

static char* type_to_string(Type t, char* buf) {
    int kind = TYPE_KIND(t);
    int sub = TYPE_SUB(t);
    int key = TYPE_KEY(t);
    
    switch (kind) {
        case VAL_INT: strcpy(buf, "int"); break;
        case VAL_FLT: strcpy(buf, "flt"); break;
        case VAL_BOOL: strcpy(buf, "bol"); break;
        case VAL_STR: strcpy(buf, "str"); break;
        case VAL_VOID: strcpy(buf, "void"); break;
        case VAL_ERR: strcpy(buf, "err"); break;
        case VAL_ANY: strcpy(buf, "any"); break;
        case VAL_OBJ: {
            if (sub == 0 || sub == VAL_STR) strcpy(buf, "str");
            else {
                char sub_buf[64];
                sprintf(buf, "[]%s", type_to_string(sub, sub_buf));
            }
            break;
        }
        case VAL_MAP: {
            char sub_buf[64], key_buf[64];
            sprintf(buf, "{%s:%s}", type_to_string(key, key_buf), type_to_string(sub, sub_buf));
            break;
        }
        case VAL_FUNC:
        case VAL_FUNC_INT:
        case VAL_FUNC_FLT:
        case VAL_FUNC_BOOL:
        case VAL_FUNC_STR:
        case VAL_FUNC_VOID:
            strcpy(buf, "fun");
            break;
        case VAL_ENUM: {
            if (sub == OPTION_ENUM_ID) {
                char key_buf[64];
                sprintf(buf, "%s?", type_to_string(key, key_buf));
            } else {
                strcpy(buf, "enum");
            }
            break;
        }
        default: strcpy(buf, "unknown"); break;
    }
    return buf;
}

static Value native_has(VM* vm, int arg_count, Value* args) {
    (void)vm;
    if (arg_count != 2 || TYPE_KIND(args[0].type) != VAL_OBJ || args[0].as.obj->type != OBJ_MAP) return (Value){VAL_BOOL, {.b_val = false}};
    ObjMap* map = (ObjMap*)args[0].as.obj;
    Value val = map_get(map, args[1]);
    return (Value){VAL_BOOL, {.b_val = TYPE_KIND(val.type) != VAL_VOID}};
}

static Value native_error(VM* vm, int arg_count, Value* args) {
    (void)vm;
    if (arg_count != 1) return (Value){VAL_ERR, {0}};
    Value err = args[0];
    retain(err);
    return (Value){VAL_ERR, err.as};
}

static Value native_time(VM* vm, int arg_count, Value* args) {
    (void)vm; (void)arg_count; (void)args;
    return (Value){VAL_INT, {.i_val = (int64_t)time(NULL)}};
}

static Value native_sqrt(VM* vm, int arg_count, Value* args) {
    (void)vm;
    if (arg_count != 1) return (Value){VAL_FLT, {.f_val = 0}};
    double val = TYPE_KIND(args[0].type) == VAL_INT ? (double)args[0].as.i_val : args[0].as.f_val;
    return (Value){VAL_FLT, {.f_val = sqrt(val)}};
}

static Value native_sin(VM* vm, int arg_count, Value* args) {
    (void)vm;
    if (arg_count != 1) return (Value){VAL_FLT, {.f_val = 0}};
    double val = TYPE_KIND(args[0].type) == VAL_INT ? (double)args[0].as.i_val : args[0].as.f_val;
    return (Value){VAL_FLT, {.f_val = sin(val)}};
}

static Value native_cos(VM* vm, int arg_count, Value* args) {
    (void)vm;
    if (arg_count != 1) return (Value){VAL_FLT, {.f_val = 0}};
    double val = TYPE_KIND(args[0].type) == VAL_INT ? (double)args[0].as.i_val : args[0].as.f_val;
    return (Value){VAL_FLT, {.f_val = cos(val)}};
}

static Value native_tan(VM* vm, int arg_count, Value* args) {
    (void)vm;
    if (arg_count != 1) return (Value){VAL_FLT, {.f_val = 0}};
    double val = TYPE_KIND(args[0].type) == VAL_INT ? (double)args[0].as.i_val : args[0].as.f_val;
    return (Value){VAL_FLT, {.f_val = tan(val)}};
}

static Value native_log(VM* vm, int arg_count, Value* args) {
    (void)vm;
    if (arg_count != 1) return (Value){VAL_FLT, {.f_val = 0}};
    double val = TYPE_KIND(args[0].type) == VAL_INT ? (double)args[0].as.i_val : args[0].as.f_val;
    return (Value){VAL_FLT, {.f_val = log(val)}};
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
    vm->try_ptr = 0;
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
    vm_define_native(vm, "exit", native_exit, 10);
    vm_define_native(vm, "clock", native_clock, 11);
    vm_define_native(vm, "system", native_system, 12);
    vm_define_native(vm, "keys", native_keys, 13);
    vm_define_native(vm, "delete", native_delete, 14);
    vm_define_native(vm, "ascii", native_ascii, 15);
    vm_define_native(vm, "char", native_char, 16);
    vm_define_native(vm, "has", native_has, 17);
    vm_define_native(vm, "error", native_error, 18);
    vm_define_native(vm, "time", native_time, 19);
    vm_define_native(vm, "sqrt", native_sqrt, 20);
    vm_define_native(vm, "sin", native_sin, 21);
    vm_define_native(vm, "cos", native_cos, 22);
    vm_define_native(vm, "tan", native_tan, 23);
    vm_define_native(vm, "log", native_log, 24);
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

static void runtime_error(VM* vm, const char* format, ...) {
    va_list args;
    va_start(args, format);
    char buf[1024];
    vsnprintf(buf, sizeof(buf), format, args);
    va_end(args);

    Value err_msg = (Value){VAL_OBJ, {.obj = (HeapObject*)allocate_string(vm, buf, (int)strlen(buf))}};
    retain(err_msg);
    
    if (vm->try_ptr > 0) {
        TryFrame frame = vm->try_stack[--vm->try_ptr];
        while (vm->stack_ptr > frame.stack_ptr) release(vm_pop(vm));
        while (vm->frame_ptr > frame.frame_ptr) {
            CallFrame* f = &vm->frames[--vm->frame_ptr];
            for (int i = 0; i < LOCALS_PER_FRAME; i++) {
                release(vm->locals[f->locals_offset + i]);
                vm->locals[f->locals_offset + i] = (Value){VAL_VOID, {.i_val = 0}};
            }
        }
        vm_push(vm, err_msg);
        vm->ip = frame.handler_addr;
        release(err_msg);
    } else {
        fprintf(stderr, "Runtime Error: %s\n", buf);
        release(err_msg);
        exit(1);
    }
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
                Value s = native_str(vm, 1, &val);
                printf("%s\n", ((ObjString*)s.as.obj)->chars);
                release(s);
                release(val);
                break;
            }
            case OP_LTE: {
                Value b = vm_pop(vm);
                Value a = vm_pop(vm);
                if (TYPE_KIND(a.type) == VAL_INT && TYPE_KIND(b.type) == VAL_INT) {
                    vm_push(vm, (Value){VAL_BOOL, {.b_val = a.as.i_val <= b.as.i_val}});
                } else if (TYPE_KIND(a.type) == VAL_FLT && TYPE_KIND(b.type) == VAL_FLT) {
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
                if (TYPE_KIND(a.type) == VAL_INT && TYPE_KIND(b.type) == VAL_INT) {
                    vm_push(vm, (Value){VAL_BOOL, {.b_val = a.as.i_val >= b.as.i_val}});
                } else if (TYPE_KIND(a.type) == VAL_FLT && TYPE_KIND(b.type) == VAL_FLT) {
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
                if (TYPE_KIND(a.type) == VAL_INT) {
                    vm_push(vm, (Value){VAL_INT, {.i_val = -a.as.i_val}});
                } else if (TYPE_KIND(a.type) == VAL_FLT) {
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
                if (TYPE_KIND(a.type) == VAL_INT && TYPE_KIND(b.type) == VAL_INT) {
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
                if (TYPE_KIND(a.type) == VAL_INT && TYPE_KIND(b.type) == VAL_INT) {
                    vm_push(vm, (Value){VAL_BOOL, {.b_val = a.as.i_val > b.as.i_val}});
                } else if (TYPE_KIND(a.type) == VAL_FLT && TYPE_KIND(b.type) == VAL_FLT) {
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
                if (TYPE_KIND(a.type) == VAL_INT && TYPE_KIND(b.type) == VAL_INT) {
                    vm_push(vm, (Value){VAL_INT, {.i_val = a.as.i_val + b.as.i_val}});
                } else if (TYPE_KIND(a.type) == VAL_FLT && TYPE_KIND(b.type) == VAL_FLT) {
                    vm_push(vm, (Value){VAL_FLT, {.f_val = a.as.f_val + b.as.f_val}});
                } else if (TYPE_KIND(a.type) == VAL_OBJ && a.as.obj->type == OBJ_STRING && TYPE_KIND(b.type) == VAL_OBJ && b.as.obj->type == OBJ_STRING) {
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
                if (TYPE_KIND(a.type) == VAL_INT && TYPE_KIND(b.type) == VAL_INT) {
                    vm_push(vm, (Value){VAL_INT, {.i_val = a.as.i_val - b.as.i_val}});
                } else if (TYPE_KIND(a.type) == VAL_FLT && TYPE_KIND(b.type) == VAL_FLT) {
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
                if (TYPE_KIND(a.type) == VAL_INT && TYPE_KIND(b.type) == VAL_INT) {
                    vm_push(vm, (Value){VAL_INT, {.i_val = a.as.i_val * b.as.i_val}});
                } else if (TYPE_KIND(a.type) == VAL_FLT && TYPE_KIND(b.type) == VAL_FLT) {
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
                if (TYPE_KIND(a.type) == VAL_INT && TYPE_KIND(b.type) == VAL_INT) {
                    if (b.as.i_val == 0) {
                        release(a); release(b);
                        runtime_error(vm, "Division by zero");
                        break;
                    }
                    vm_push(vm, (Value){VAL_INT, {.i_val = a.as.i_val / b.as.i_val}});
                } else if (TYPE_KIND(a.type) == VAL_FLT && TYPE_KIND(b.type) == VAL_FLT) {
                    if (b.as.f_val == 0) {
                        release(a); release(b);
                        runtime_error(vm, "Division by zero");
                        break;
                    }
                    vm_push(vm, (Value){VAL_FLT, {.f_val = a.as.f_val / b.as.f_val}});
                } else {
                    release(a); release(b);
                    runtime_error(vm, "Type error in DIV");
                    break;
                }
                release(a); release(b);
                break;
            }
            case OP_EQ: {
                Value b = vm_pop(vm);
                Value a = vm_pop(vm);
                bool res = false;
                if (TYPE_KIND(a.type) == TYPE_KIND(b.type)) {
                    if (TYPE_KIND(a.type) == VAL_INT) res = a.as.i_val == b.as.i_val;
                    else if (TYPE_KIND(a.type) == VAL_BOOL) res = a.as.b_val == b.as.b_val;
                    else if (TYPE_KIND(a.type) == VAL_OBJ && TYPE_KIND(b.type) == VAL_OBJ) {
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
                if (TYPE_KIND(a.type) == VAL_INT && TYPE_KIND(b.type) == VAL_INT) {
                    vm_push(vm, (Value){VAL_BOOL, {.b_val = a.as.i_val < b.as.i_val}});
                } else if (TYPE_KIND(a.type) == VAL_FLT && TYPE_KIND(b.type) == VAL_FLT) {
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
                if (vm->frame_ptr >= FRAMES_MAX) { runtime_error(vm, "Stack overflow (frames)\n"); }
                int current_offset = vm->frame_ptr * LOCALS_PER_FRAME;
                CallFrame* frame = &vm->frames[vm->frame_ptr++];
                frame->return_addr = vm->ip;
                frame->locals_offset = current_offset;
                vm->ip = addr;
                break;
            }
            case OP_RET: {
                if (vm->frame_ptr <= 1) { runtime_error(vm, "Stack underflow (frames)\n"); }
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
                char buf[256];
                type_to_string(val.type, buf);
                ObjString* s = allocate_string(vm, buf, strlen(buf));
                vm_push(vm, (Value){VAL_OBJ, {.obj = (HeapObject*)s}});
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
                if (TYPE_KIND(obj.type) == VAL_OBJ && obj.as.obj->type == OBJ_ARRAY) {
                    ObjArray* array = (ObjArray*)obj.as.obj;
                    int idx = (int)index.as.i_val;
                    if (idx < 0 || idx >= array->count) {
                        release(obj); release(index);
                        runtime_error(vm, "Array index %d out of bounds (length %d)", idx, array->count);
                        break;
                    }
                    vm_push(vm, array->items[idx]);
                } else if (TYPE_KIND(obj.type) == VAL_OBJ && obj.as.obj->type == OBJ_STRING) {
                    ObjString* s = (ObjString*)obj.as.obj;
                    int idx = (int)index.as.i_val;
                    if (idx < 0 || idx >= s->length) {
                        release(obj); release(index);
                        runtime_error(vm, "String index %d out of bounds (length %d)", idx, s->length);
                        break;
                    }
                    char buf[2] = {s->chars[idx], '\0'};
                    ObjString* res = allocate_string(vm, buf, 1);
                    vm_push(vm, (Value){VAL_OBJ, {.obj = (HeapObject*)res}});
                } else if (TYPE_KIND(obj.type) == VAL_OBJ && obj.as.obj->type == OBJ_MAP) {
                    ObjMap* map = (ObjMap*)obj.as.obj;
                    Value val = map_get(map, index);
                    if (TYPE_KIND(val.type) == VAL_VOID) {
                        release(obj); release(index);
                        runtime_error(vm, "Key not found in map");
                        break;
                    }
                    vm_push(vm, val);
                } else { 
                    release(obj); release(index);
                    runtime_error(vm, "Can only index arrays, strings or maps. Got type kind %d", TYPE_KIND(obj.type));
                    break;
                }
                release(obj); release(index);
                break;
            }
            case OP_GET_MEMBER: {
                int field_idx = vm->code[vm->ip++];
                Value obj = vm_pop(vm);
                if (TYPE_KIND(obj.type) != VAL_OBJ || obj.as.obj->type != OBJ_STRUCT) {
                    release(obj);
                    runtime_error(vm, "Can only get member");
                    break;
                }
                ObjStruct* st = (ObjStruct*)obj.as.obj;
                vm_push(vm, st->values[field_idx]);
                release(obj);
                break;
            }
            case OP_SET_MEMBER: {
                int field_idx = vm->code[vm->ip++];
                Value obj = vm_pop(vm);
                Value val = vm_pop(vm);
                if (TYPE_KIND(obj.type) != VAL_OBJ || obj.as.obj->type != OBJ_STRUCT) {
                    release(obj); release(val);
                    runtime_error(vm, "Can only set member");
                    break;
                }
                ObjStruct* st = (ObjStruct*)obj.as.obj;
                release(st->values[field_idx]);
                retain(val);
                st->values[field_idx] = val;
                release(obj); release(val);
                break;
            }
            case OP_TRY: {
                int32_t handler = read_int32(vm);
                if (vm->try_ptr >= TRY_STACK_MAX) { runtime_error(vm, "Try stack overflow\n"); }
                vm->try_stack[vm->try_ptr++] = (TryFrame){handler, vm->stack_ptr, vm->frame_ptr};
                break;
            }
            case OP_END_TRY: {
                if (vm->try_ptr > 0) vm->try_ptr--;
                break;
            }
            case OP_THROW: {
                Value err = vm_pop(vm);
                if (vm->try_ptr == 0) {
                    fprintf(stderr, "Unhandled Exception: ");
                    Value s = native_str(vm, 1, &err);
                    printf("%s\n", ((ObjString*)s.as.obj)->chars);
                    release(s);
                    exit(1);
                }
                TryFrame frame = vm->try_stack[--vm->try_ptr];
                while (vm->stack_ptr > frame.stack_ptr) release(vm_pop(vm));
                while (vm->frame_ptr > frame.frame_ptr) {
                    CallFrame* f = &vm->frames[--vm->frame_ptr];
                    for (int i = 0; i < LOCALS_PER_FRAME; i++) {
                        release(vm->locals[f->locals_offset + i]);
                        vm->locals[f->locals_offset + i] = (Value){VAL_VOID, {0}};
                    }
                }
                vm_push(vm, err);
                vm->ip = frame.handler_addr;
                break;
            }
            case OP_SET_INDEX: {
                Value index = vm_pop(vm);
                Value obj = vm_pop(vm);
                Value val = vm_pop(vm);
                if (TYPE_KIND(obj.type) == VAL_OBJ && obj.as.obj->type == OBJ_ARRAY) {
                    ObjArray* array = (ObjArray*)obj.as.obj;
                    int idx = (int)index.as.i_val;
                    if (idx < 0 || idx >= array->count) {
                        release(obj); release(index); release(val);
                        runtime_error(vm, "Array index %d out of bounds in assignment (length %d)", idx, array->count);
                        break;
                    }
                    release(array->items[idx]);
                    retain(val);
                    array->items[idx] = val;
                } else if (TYPE_KIND(obj.type) == VAL_OBJ && obj.as.obj->type == OBJ_MAP) {
                    ObjMap* map = (ObjMap*)obj.as.obj;
                    map_set(map, index, val);
                } else { 
                    release(obj); release(index); release(val);
                    runtime_error(vm, "Can only set index on arrays or maps");
                    break;
                }
                release(obj); release(index); release(val);
                break;
            }
            case OP_ARRAY: {
                Type type = (Type)read_int32(vm);
                int count = vm->code[vm->ip++];
                ObjArray* array = allocate_array(vm);
                array->items = malloc(sizeof(Value) * count);
                array->count = count; array->capacity = count;
                for (int i = count - 1; i >= 0; i--) array->items[i] = vm_pop(vm);
                vm_push(vm, (Value){type, {.obj = (HeapObject*)array}});
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
                Type type = (Type)read_int32(vm);
                int pair_count = vm->code[vm->ip++];
                ObjMap* map = allocate_map(vm);
                for (int i = 0; i < pair_count; i++) {
                    Value val = vm_pop(vm);
                    Value key = vm_pop(vm);
                    map_set(map, key, val);
                    release(key); release(val);
                }
                vm_push(vm, (Value){type, {.obj = (HeapObject*)map}});
                break;
            }
            case OP_ENUM_VARIANT: {
                Type type = (Type)read_int32(vm);
                int variant_id = vm->code[vm->ip++];
                bool has_payload = vm->code[vm->ip++] != 0;
                ObjEnum* en = malloc(sizeof(ObjEnum));
                en->obj.type = OBJ_ENUM;
                en->obj.ref_count = 0;
                en->variant_index = variant_id;
                en->has_payload = has_payload;
                if (has_payload) {
                    en->payload = vm_pop(vm);
                } else {
                    en->payload = (Value){VAL_VOID, {.i_val = 0}};
                }
                en->enum_name = strdup("enum");
                en->variant_name = strdup("variant");
                vm_push(vm, (Value){type, {.obj = (HeapObject*)en}});
                break;
            }
            case OP_CHECK_VARIANT: {
                int32_t variant_id = read_int32(vm);
                Value val = vm->stack[vm->stack_ptr - 1];
                if (TYPE_KIND(val.type) == VAL_ENUM && val.as.obj->type == OBJ_ENUM) {
                    ObjEnum* en = (ObjEnum*)val.as.obj;
                    vm_push(vm, (Value){VAL_BOOL, {.b_val = en->variant_index == variant_id}});
                } else {
                    vm_push(vm, (Value){VAL_BOOL, {.b_val = false}});
                }
                break;
            }
            case OP_IS_TRUTHY: {
                Value val = vm_pop(vm);
                bool truthy = false;
                int kind = TYPE_KIND(val.type);
                if (kind == VAL_BOOL) truthy = val.as.b_val;
                else if (kind == VAL_ENUM) {
                    if (val.as.obj->type == OBJ_ENUM) {
                        ObjEnum* en = (ObjEnum*)val.as.obj;
                        if (TYPE_SUB(val.type) == OPTION_ENUM_ID) {
                            truthy = (en->variant_index != 0);
                        } else {
                            truthy = true;
                        }
                    }
                } else if (kind != VAL_VOID) truthy = true;
                
                vm_push(vm, (Value){VAL_BOOL, {.b_val = truthy}});
                release(val);
                break;
            }
            case OP_EXTRACT_ENUM_PAYLOAD: {
                Value val = vm_pop(vm);
                if (TYPE_KIND(val.type) == VAL_ENUM && val.as.obj->type == OBJ_ENUM) {
                    ObjEnum* en = (ObjEnum*)val.as.obj;
                    retain(en->payload);
                    vm_push(vm, en->payload);
                } else {
                    vm_push(vm, (Value){VAL_VOID, {.i_val = 0}});
                }
                release(val);
                break;
            }
            case OP_GET_ENUM_PAYLOAD: {
                Value val = vm->stack[vm->stack_ptr - 1];
                if (TYPE_KIND(val.type) == VAL_ENUM && val.as.obj->type == OBJ_ENUM) {
                    ObjEnum* en = (ObjEnum*)val.as.obj;
                    retain(en->payload);
                    vm_push(vm, en->payload);
                } else {
                    vm_push(vm, (Value){VAL_VOID, {.i_val = 0}});
                }
                break;
            }
            case OP_INVOKE: {
                int arg_count = vm->code[vm->ip++];
                Value callable = vm_pop(vm);
                if (TYPE_KIND(callable.type) == VAL_OBJ && callable.as.obj->type == OBJ_NATIVE) {
                    ObjNative* native = (ObjNative*)callable.as.obj;
                    Value* args = &vm->stack[vm->stack_ptr - arg_count];
                    Value result = native->function(vm, arg_count, args);
                    retain(result);
                    for (int i = 0; i < arg_count; i++) release(vm_pop(vm));
                    vm_push(vm, result);
                    release(result); release(callable);
                } else if (TYPE_KIND(callable.type) >= VAL_FUNC || TYPE_KIND(callable.type) == VAL_INT) {
                    int32_t addr = (int32_t)callable.as.i_val;
                    release(callable);
                    if (vm->frame_ptr >= FRAMES_MAX) { runtime_error(vm, "Stack overflow\n"); }
                    int current_offset = vm->frame_ptr * LOCALS_PER_FRAME;
                    CallFrame* frame = &vm->frames[vm->frame_ptr++];
                    frame->return_addr = vm->ip; frame->locals_offset = current_offset;
                    vm->ip = addr;
                } else { fprintf(stderr, "Can only invoke functions or natives. Type: %d\n", TYPE_KIND(callable.type)); exit(1); }
                break;
            }
            default:
                fprintf(stderr, "Unknown opcode %d\n", instruction);
                exit(1);
        }
    }
}
