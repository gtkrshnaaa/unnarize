// Define a global variable with interning
void defineGlobal(VM* vm, const char* name, Value value) {
    char* key = internString(vm, name, (int)strlen(name));
    unsigned int h = hashPointer(key);
    
    // Check if exists
    VarEntry* entry = vm->globalEnv->buckets[h];
    while (entry) {
        if (entry->key == key) {
            entry->value = value;
            return;
        }
        entry = entry->next;
    }
    
    // Create new
    entry = malloc(sizeof(VarEntry));
    if (!entry) error("Memory allocation failed.", 0);
    entry->key = key;
    entry->keyLength = (int)strlen(key);
    entry->ownsKey = false;
    entry->value = value;
    entry->next = vm->globalEnv->buckets[h];
    vm->globalEnv->buckets[h] = entry;
}

// Define a native function in a specific environment with interning
void defineNative(VM* vm, Environment* env, const char* name, NativeFn fn, int arity) {
    char* key = internString(vm, name, (int)strlen(name));
    unsigned int h = hashPointer(key);
    
    FuncEntry* fe = malloc(sizeof(FuncEntry));
    fe->key = key;
    fe->next = env->funcBuckets[h];
    env->funcBuckets[h] = fe;
    
    Function* func = malloc(sizeof(Function));
    func->isNative = true;
    func->native = fn;
    func->paramCount = arity;
    func->name = (Token){TOKEN_IDENTIFIER, key, (int)strlen(key), 0};
    func->body = NULL;
    func->closure = NULL;
    
    fe->function = func;
}
