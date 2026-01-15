#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "vm.h"
#include "ucore_scraper.h"

// ============================================================================
// DOM Node Management
// ============================================================================

static ScraperNode* createNode(ScraperNodeType type) {
    ScraperNode* node = malloc(sizeof(ScraperNode));
    if (!node) return NULL;
    
    node->type = type;
    node->tagName = NULL;
    node->textContent = NULL;
    node->parent = NULL;
    node->children = NULL;
    node->childCount = 0;
    node->childCapacity = 0;
    node->attrKeys = NULL;
    node->attrValues = NULL;
    node->attrCount = 0;
    node->attrCapacity = 0;
    node->isSelfClosing = false;
    
    return node;
}

static void __attribute__((unused)) freeNode(ScraperNode* node) {
    if (!node) return;
    
    if (node->tagName) free(node->tagName);
    if (node->textContent) free(node->textContent);
    
    for (int i = 0; i < node->childCount; i++) {
        freeNode(node->children[i]);
    }
    if (node->children) free(node->children);
    
    for (int i = 0; i < node->attrCount; i++) {
        free(node->attrKeys[i]);
        free(node->attrValues[i]);
    }
    if (node->attrKeys) free(node->attrKeys);
    if (node->attrValues) free(node->attrValues);
    
    free(node);
}

static void appendChild(ScraperNode* parent, ScraperNode* child) {
    if (parent->childCapacity < parent->childCount + 1) {
        int oldCapacity = parent->childCapacity;
        parent->childCapacity = (oldCapacity < 8) ? 8 : oldCapacity * 2;
        parent->children = realloc(parent->children, sizeof(ScraperNode*) * parent->childCapacity);
    }
    parent->children[parent->childCount++] = child;
    child->parent = parent;
}

// Will be used in Phase 2
static void __attribute__((unused)) addAttribute(ScraperNode* node, const char* key, const char* value) {
    if (node->attrCapacity < node->attrCount + 1) {
        int oldCapacity = node->attrCapacity;
        node->attrCapacity = (oldCapacity < 4) ? 4 : oldCapacity * 2;
        node->attrKeys = realloc(node->attrKeys, sizeof(char*) * node->attrCapacity);
        node->attrValues = realloc(node->attrValues, sizeof(char*) * node->attrCapacity);
    }
    node->attrKeys[node->attrCount] = strdup(key);
    node->attrValues[node->attrCount] = strdup(value ? value : "");
    node->attrCount++;
}

// ============================================================================
// HTML Parser (Tokenizer & Tree Builder)
// ============================================================================

// Very simplified for Phase 1 skeleton
static ScraperNode* parseHtml(const char* source) {
    (void)source; // Suppress unused warning
    ScraperNode* root = createNode(NODE_DOCUMENT);
    // TODO: Implement actual tokenizer loop here
    
    // Dummy implementation for checking build integration
    ScraperNode* body = createNode(NODE_ELEMENT);
    body->tagName = strdup("body");
    appendChild(root, body);
    
    ScraperNode* text = createNode(NODE_TEXT);
    text->textContent = strdup("Hello Scraper!");
    appendChild(body, text);
    
    return root;
}

// ============================================================================
// Native Bindings
// ============================================================================

// ucoreScraper.parse(htmlString) -> DocumentObject
static Value scraper_parse(VM* vm, Value* args, int argCount) {
    (void)vm; // Suppress unused warning
    if (argCount != 1 || !IS_STRING(args[0])) {
        return NIL_VAL; // Should throw error ideally
    }
    
    ObjString* html = AS_STRING(args[0]);
    ScraperNode* doc = parseHtml(html->chars);
    (void)doc; // Suppress unused variable warning for now
    
    // For now, return a simple map representation to test binding
    // Phase 3 will wrap this in a proper UserData or Instance object
    return NIL_VAL;
}

void registerUCoreScraper(VM* vm) {
    ObjString* modNameObj = internString(vm, "ucoreScraper", 12);
    char* modName = modNameObj->chars;
    
    Module* mod = ALLOCATE_OBJ(vm, Module, OBJ_MODULE);
    mod->name = strdup(modName);
    mod->obj.isMarked = true;
    mod->obj.isPermanent = true;
    
    Environment* modEnv = ALLOCATE_OBJ(vm, Environment, OBJ_ENVIRONMENT);
    memset(modEnv->buckets, 0, sizeof(modEnv->buckets));
    memset(modEnv->funcBuckets, 0, sizeof(modEnv->funcBuckets));
    modEnv->enclosing = NULL;
    modEnv->obj.isMarked = true;
    modEnv->obj.isPermanent = true;
    mod->env = modEnv;
    
    defineNative(vm, mod->env, "parse", scraper_parse, 1);
    
    Value vMod = OBJ_VAL(mod);
    defineGlobal(vm, "ucoreScraper", vMod);
}
