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

// ============================================================================
// HTML Tokenizer
// ============================================================================

typedef enum {
    SCRAPER_TOKEN_TAG_OPEN,    // <div
    SCRAPER_TOKEN_TAG_CLOSE,   // </div>
    SCRAPER_TOKEN_TEXT,        // Plain text
    SCRAPER_TOKEN_EOF,
    SCRAPER_TOKEN_ERROR
} ScraperTokenType;

typedef struct {
    ScraperTokenType type;
    const char* start;
    int length;
    // For tags
    const char* tagName;
    int tagNameLen;
    bool isSelfClosing; // <br/>
} ScraperToken;

static bool isAlpha(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

static bool isAlphaNumeric(char c) {
    return isAlpha(c) || (c >= '0' && c <= '9') || c == '-' || c == '_' || c == ':';
}

static void skipWhitespace(HtmlParser* parser) {
    while (parser->current < parser->length) {
        char c = parser->source[parser->current];
        if (c == ' ' || c == '\r' || c == '\t' || c == '\n') {
            parser->current++;
        } else {
            break;
        }
    }
}

// Parse attributes for the current node
// Simplified: assumes we are inside <tag ... >
static void parseAttributes(HtmlParser* parser, ScraperNode* node) {
    while (parser->current < parser->length) {
        skipWhitespace(parser);
        char c = parser->source[parser->current];
        
        if (c == '>' || c == '/') return; // End of tag
        
        // Attribute Name
        int nameStart = parser->current;
        while (parser->current < parser->length && 
               isAlphaNumeric(parser->source[parser->current])) {
            parser->current++;
        }
        int nameLen = parser->current - nameStart;
        if (nameLen == 0) { parser->current++; continue; } // Skip garbage
        
        char* name = malloc(nameLen + 1);
        memcpy(name, parser->source + nameStart, nameLen);
        name[nameLen] = '\0';
        
        // Check for =
        skipWhitespace(parser);
        char* value = NULL;
        
        if (parser->source[parser->current] == '=') {
            parser->current++; // skip =
            skipWhitespace(parser);
            
            char quote = parser->source[parser->current];
            if (quote == '"' || quote == '\'') {
                parser->current++; // skip quote
                int valStart = parser->current;
                while (parser->current < parser->length && 
                       parser->source[parser->current] != quote) {
                    parser->current++;
                }
                int valLen = parser->current - valStart;
                value = malloc(valLen + 1);
                memcpy(value, parser->source + valStart, valLen);
                value[valLen] = '\0';
                if (parser->current < parser->length) parser->current++; // skip closing quote
            } else {
                // Unquoted value
                int valStart = parser->current;
                while (parser->current < parser->length && 
                       !isspace(parser->source[parser->current]) && 
                       parser->source[parser->current] != '>') {
                    parser->current++;
                }
                int valLen = parser->current - valStart;
                value = malloc(valLen + 1);
                memcpy(value, parser->source + valStart, valLen);
                value[valLen] = '\0';
            }
        }
        
        addAttribute(node, name, value);
        free(name);
        if (value) free(value);
    }
}

// ============================================================================
// Tree Builder
// ============================================================================

static ScraperNode* parseHtml(const char* source) {
    HtmlParser parser;
    parser.source = source;
    parser.length = strlen(source);
    parser.current = 0;
    
    ScraperNode* root = createNode(NODE_DOCUMENT);
    ScraperNode* currentParent = root;
    
    // Simple stack for hierarchy (using parent pointers instead of explicit stack array)
    
    while (parser.current < parser.length) {
        char c = parser.source[parser.current];
        
        if (c == '<') {
            // Tag or Comment
            if (parser.current + 3 < parser.length && 
                strncmp(parser.source + parser.current, "<!--", 4) == 0) {
                // Comment
                parser.current += 4;
                int start = parser.current;
                (void)start; // Suppress unused warning
                while (parser.current + 2 < parser.length && 
                       strncmp(parser.source + parser.current, "-->", 3) != 0) {
                    parser.current++;
                }
                // Skip comments for now to save memory
                parser.current += 3;
                continue;
            }
            
            if (parser.current + 1 < parser.length && parser.source[parser.current+1] == '/') {
                // Closing tag: </div>
                parser.current += 2;
                int start = parser.current;
                (void)start; // Suppress unused warning
                while (parser.current < parser.length && parser.source[parser.current] != '>') {
                    parser.current++;
                }
                // Pop stack (traverse up)
                if (parser.current < parser.length) parser.current++; // skip >
                
                // TODO: Verify tag name matches currentParent->tagName to handle unclosed tags properly
                // For now, simple "pop" logic:
                if (currentParent->parent) currentParent = currentParent->parent;
                continue;
            }
            
            // Opening tag: <div>
            parser.current++; // skip <
            int nameStart = parser.current;
            while (parser.current < parser.length && isAlphaNumeric(parser.source[parser.current])) {
                parser.current++;
            }
            int nameLen = parser.current - nameStart;
            
            if (nameLen > 0) {
                char* tagName = malloc(nameLen + 1);
                memcpy(tagName, parser.source + nameStart, nameLen);
                tagName[nameLen] = '\0';
                // Lowercase canonicalization
                for(int i=0; i<nameLen; i++) tagName[i] = tolower(tagName[i]);
                
                ScraperNode* element = createNode(NODE_ELEMENT);
                element->tagName = tagName;
                
                parseAttributes(&parser, element);
                
                // Check self-closing / void elements
                bool isVoid = false;
                if (strcmp(tagName, "img") == 0 || strcmp(tagName, "br") == 0 || 
                    strcmp(tagName, "meta") == 0 || strcmp(tagName, "hr") == 0 ||
                    strcmp(tagName, "input") == 0 || strcmp(tagName, "link") == 0) {
                    isVoid = true;
                }
                
                if (parser.current < parser.length && parser.source[parser.current] == '/') {
                    isVoid = true; // <div /> style
                }
                
                while (parser.current < parser.length && parser.source[parser.current] != '>') {
                    parser.current++;
                }
                if (parser.current < parser.length) parser.current++; // skip >
                
                appendChild(currentParent, element);
                
                if (!isVoid) {
                    currentParent = element;
                }
            } else {
                parser.current++; // stray <
            }
            
        } else {
            // Text Content
            int start = parser.current;
            while (parser.current < parser.length && parser.source[parser.current] != '<') {
                parser.current++;
            }
            int len = parser.current - start;
            if (len > 0) {
                // Only add if not just whitespace (optional optimization, keeping it strict for now)
                // Actually, let's keep all text but trim later if needed.
                char* text = malloc(len + 1);
                memcpy(text, parser.source + start, len);
                text[len] = '\0';
                
                ScraperNode* textNode = createNode(NODE_TEXT);
                textNode->textContent = text;
                appendChild(currentParent, textNode);
            }
        }
    }
    
    return root;
}

// ============================================================================
// Native Bindings
// ============================================================================

// ucoreScraper.parse(htmlString) -> DocumentObject
// (Previous code block was malformed)

// Function to recursively print DOM (Depth-First)
static void printNode(ScraperNode* node, int depth) {
    for (int i = 0; i < depth; i++) printf("  ");
    
    if (node->type == NODE_DOCUMENT) {
        printf("#document\n");
    } else if (node->type == NODE_ELEMENT) {
        printf("<%s", node->tagName);
        for (int i=0; i<node->attrCount; i++) {
            printf(" %s=\"%s\"", node->attrKeys[i], node->attrValues[i]);
        }
        printf(">\n");
    } else if (node->type == NODE_TEXT) {
        // Trim whitespace for display
        char* txt = node->textContent;
        while(isspace(*txt)) txt++;
        if (strlen(txt) > 0) printf("#text: \"%s\"\n", txt);
    }
    
    for (int i = 0; i < node->childCount; i++) {
        printNode(node->children[i], depth + 1);
    }
}

static Value __attribute__((unused)) scraper_dump(VM* vm, Value* args, int argCount) {
    // In real API we would pass the Document object
    // But since we are creating new objects every parse and not keeping them alive yet (GC issue),
    // we can't easily implement this without an object wrapper.
    // For now, let's keep it simple: WE CAN'T DUMP from a separate call unless we store the last parsed doc globally (bad)
    // or return a wrapped object.
    
    // TEMPORARY: Move printNode logic into parse for debugging?
    // OR: Wrap ScraperNode in a simple structure we can pass back.
    // Let's implement Phase 3 Object Wrapper quickly to make this usable.
    
    // Actually, for this step, let's just make `parse` print the tree if a 2nd arg is true.
    // Simple solution for immediate verification.
    (void)vm; (void)args; (void)argCount;
    return NIL_VAL;
}

// ucoreScraper.parse(htmlString, [debugPrint]) -> Document
static Value scraper_parse(VM* vm, Value* args, int argCount) {
    (void)vm; // Suppress unused parameter
    if (argCount < 1 || !IS_STRING(args[0])) {
        return NIL_VAL;
    }
    
    ObjString* html = AS_STRING(args[0]);
    ScraperNode* doc = parseHtml(html->chars);
    
    // If 2nd arg is TRUE, dump the tree
    if (argCount == 2 && IS_BOOL(args[1]) && AS_BOOL(args[1])) {
        printNode(doc, 0);
    }
    
    // Free the tree immediately since we don't transfer ownership to VM yet
    // (This prevents memory leaks during testing until we have GC wrapper)
    freeNode(doc);
    
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
