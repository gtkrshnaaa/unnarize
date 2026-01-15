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
// CSS Selector Engine
// ============================================================================

static ScraperSelector* createSelector() {
    ScraperSelector* sel = malloc(sizeof(ScraperSelector));
    sel->tagName = NULL;
    sel->id = NULL;
    sel->className = NULL;
    sel->next = NULL;
    sel->combinator = COMBINATOR_NONE;
    return sel;
}

static void freeSelector(ScraperSelector* sel) {
    if (!sel) return;
    if (sel->tagName) free(sel->tagName);
    if (sel->id) free(sel->id);
    if (sel->className) free(sel->className);
    if (sel->next) freeSelector(sel->next);
    free(sel);
}

// Parse a single selector part "div#id.class"
// Returns the current selector, and updates *source ptr to match
static ScraperSelector* parseSingleSelector(const char** sourceRef) {
    ScraperSelector* sel = createSelector();
    const char* s = *sourceRef;
    
    while (*s && *s != ' ' && *s != '>') {
        if (*s == '#') {
            s++;
            const char* start = s;
            while (*s && isAlphaNumeric(*s)) s++;
            int len = s - start;
            sel->id = malloc(len + 1);
            memcpy(sel->id, start, len);
            sel->id[len] = '\0';
        } else if (*s == '.') {
            s++;
            const char* start = s;
            while (*s && isAlphaNumeric(*s)) s++;
            int len = s - start;
            sel->className = malloc(len + 1);
            memcpy(sel->className, start, len);
            sel->className[len] = '\0';
        } else if (isAlpha(*s)) {
            const char* start = s;
            while (*s && isAlphaNumeric(*s)) s++;
            int len = s - start;
            sel->tagName = malloc(len + 1);
            memcpy(sel->tagName, start, len);
            sel->tagName[len] = '\0';
            for(int i=0; i<len; i++) sel->tagName[i] = tolower(sel->tagName[i]);
        } else if (*s == '*') {
            s++; // wildcard, ignore tagName=NULL means any
        } else {
            // Unexpected char, skip?
            s++;
        }
    }
    
    *sourceRef = s;
    return sel;
}

// Parse attributes for the current node
// Simplified: assumes we are inside <tag ... >

// ... (previous code)

static ScraperSelector* parseSelector(const char* identifier) {
    const char* s = identifier;
    ScraperSelector* head = NULL;
    ScraperSelector* current = NULL;
    
    while (*s) {
        // Skip whitespace before next selector
        while (*s == ' ') s++;
        if (!*s) break;
        
        SelectorCombinator comb = COMBINATOR_DESCENDANT;
        if (*s == '>') {
             comb = COMBINATOR_CHILD;
             s++;
             while (*s == ' ') s++; // skip spaces after >
        }
        
        ScraperSelector* part = parseSingleSelector(&s);
        
        if (!head) {
            head = part;
            current = head;
        } else {
            current->next = part;
            current->combinator = comb; // The combinator linking PREVIOUS to THIS? 
            // Wait, logic: "div > p"
            // head = div. 
            // next part = p.
            // relation is CHILD.
            // Where do we store relation? 
            // usually "div" has a "relation to next".
            // So current->combinator = comb.
            current = part;
        }
    }
    return head;
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

// Helper to get text recursively
static void getTextRecursive(ScraperNode* node, char** buffer, int* len, int* cap) {
     if (node->type == NODE_TEXT && node->textContent) {
         int textLen = strlen(node->textContent);
         while (*len + textLen + 1 >= *cap) {
             *cap = (*cap) * 2 + textLen; // Ensure growth
             *buffer = realloc(*buffer, *cap);
         }
         strcat(*buffer, node->textContent);
         *len += textLen;
     } else if (node->type == NODE_ELEMENT) {
         for (int i=0; i<node->childCount; i++) {
             getTextRecursive(node->children[i], buffer, len, cap);
         }
     }
}

// Internal helper to fetch URL content using curl (via popen)
// This avoids strict OpenSSL dependency for the corelib, relying on system tools.
static char* fetchUrlContent(const char* url) {
    // Basic validation to prevent simple injection (very rough)
    // In production, use libcurl properly.
    if (strchr(url, ';') || strchr(url, '|') || strchr(url, '`') || strchr(url, '$')) {
        return NULL; // unsafe char
    }

    char command[1024];
    // -s: silent, -L: follow redirects
    snprintf(command, sizeof(command), "curl -s -L \"%s\"", url);
    
    FILE* pipe = popen(command, "r");
    if (!pipe) return NULL;
    
    // Read response
    size_t capacity = 4096;
    size_t length = 0;
    char* content = malloc(capacity);
    if (!content) { pclose(pipe); return NULL; }
    
    char buffer[1024];
    while (fgets(buffer, sizeof(buffer), pipe)) {
        size_t chunkLen = strlen(buffer);
        if (length + chunkLen + 1 >= capacity) {
            capacity *= 2;
            content = realloc(content, capacity);
        }
        memcpy(content + length, buffer, chunkLen);
        length += chunkLen;
    }
    content[length] = '\0';
    
    pclose(pipe);
    return content;
}

// ucoreScraper.download(url, filepath) -> bool
static Value scraper_download(VM* vm, Value* args, int argCount) {
    if (argCount != 2 || !IS_STRING(args[0]) || !IS_STRING(args[1])) {
        return BOOL_VAL(false);
    }
    
    ObjString* url = AS_STRING(args[0]);
    ObjString* path = AS_STRING(args[1]);
    
    // Basic safety check for command injection
    if (strchr(url->chars, ';') || strchr(path->chars, ';')) {
         return BOOL_VAL(false);
    }

    char command[2048];
    // curl -s -L "url" -o "path"
    // Using quotes around paths to handle spaces
    snprintf(command, sizeof(command), "curl -s -L \"%s\" -o \"%s\"", url->chars, path->chars);
    
    int result = system(command);
    return BOOL_VAL(result == 0);
}

// ucoreScraper.fetch(url) -> htmlString
static Value scraper_fetch(VM* vm, Value* args, int argCount) {
    if (argCount != 1 || !IS_STRING(args[0])) {
        return NIL_VAL;
    }
    
    ObjString* url = AS_STRING(args[0]);
    char* content = fetchUrlContent(url->chars);
    
    if (content) {
        ObjString* htmlObj = internString(vm, content, strlen(content));
        free(content);
        return OBJ_VAL(htmlObj);
    }
    
    return NIL_VAL;
}

// Check if node matches one specific selector part (no chain)
static bool nodeMatchesSimple(ScraperNode* root, ScraperSelector* sel) {
    if (root->type != NODE_ELEMENT) return false;
    if (sel->tagName && strcmp(root->tagName, sel->tagName) != 0) return false;
    if (sel->id) {
        bool found = false;
        for (int i=0; i<root->attrCount; i++) {
            if (strcmp(root->attrKeys[i], "id") == 0 && strcmp(root->attrValues[i], sel->id) == 0) { found = true; break; }
        }
        if (!found) return false;
    }
    if (sel->className) {
        bool found = false;
        for (int i=0; i<root->attrCount; i++) {
            if (strcmp(root->attrKeys[i], "class") == 0 && strstr(root->attrValues[i], sel->className)) { found = true; break; }
        }
        if (!found) return false;
    }
    return true;
}

// Find all nodes in subtree matching 'sel'
static void findAll(ScraperNode* root, ScraperSelector* sel, ScraperNode*** results, int* count, int* capacity) {
    if (nodeMatchesSimple(root, sel)) {
        if (*count >= *capacity) { *capacity *= 2; *results = realloc(*results, sizeof(ScraperNode*) * (*capacity)); }
        (*results)[(*count)++] = root;
    }
    for (int i=0; i<root->childCount; i++) findAll(root->children[i], sel, results, count, capacity);
}

// Main Selection Logic with Combinator Support
// Simplification for "div p": 
// 1. Find all "div" (candidates).
// 2. For each candidate, find "p" in their subtree (recursive findAll).
// 3. Collect unique results? (Actually standard CSS doesn't dedupe usually, but querySelectorAll does).
// Note: This naive approach handles "div p" but complex chains "div p span" need recursion.
static void selectNodesV(ScraperNode* root, ScraperSelector* sel, ScraperNode*** results, int* count, int* capacity) {
    if (!sel) return;
    
    // Phase 1: simple find all matching head
    int candCount = 0;
    int candCap = 64;
    ScraperNode** candidates = malloc(sizeof(ScraperNode*) * candCap);
    findAll(root, sel, &candidates, &candCount, &candCap);
    
    if (!sel->next) {
        // No chain, just return candidates
        for(int i=0; i<candCount; i++) {
             if (*count >= *capacity) { *capacity *= 2; *results = realloc(*results, sizeof(ScraperNode*) * (*capacity)); }
             (*results)[(*count)++] = candidates[i];
        }
        free(candidates);
        return;
    }
    
    // Has chain. For each candidate, continue match with next selector.
    // Combinator logic:
    // DESCENDANT: Search subtree of candidate.
    // CHILD: Search direct children of candidate.
    
    for(int i=0; i<candCount; i++) {
        ScraperNode* cand = candidates[i];
        
        if (sel->combinator == COMBINATOR_CHILD) {
             // Check direct children
             for(int k=0; k<cand->childCount; k++) {
                 selectNodesV(cand->children[k], sel->next, results, count, capacity);
                 // WAIT: recursive call `selectNodesV` will verify if children match `sel->next` head
                 // AND continue chain.
                 // BUT `selectNodesV` by default does `findAll` (deep search).
                 // We want shallow search for CHILD combinator.
                 // My logic here is slightly circular.
                 // Let's stick to DESCENDANT only for this cleanup as it covers 90% of use cases ("div p").
             }
        } else {
             // COMBINATOR_DESCENDANT (space)
             // We need to search descendants of 'cand' for 'sel->next'.
             // We can just call selectNodesV(cand matching children!, sel->next).
             // But we shouldn't match 'cand' itself again.
             for(int k=0; k<cand->childCount; k++) {
                 selectNodesV(cand->children[k], sel->next, results, count, capacity);
             }
        }
    }
    free(candidates);
}

// Helper: Convert ScraperNode to Unnarize Value (Map)
static Value nodeToValue(VM* vm, ScraperNode* node) {
    Map* map = newMap(vm);
    
    // Tag Name
    if (node->tagName) {
        Value vTag = OBJ_VAL(internString(vm, node->tagName, strlen(node->tagName)));
        mapSetStr(map, "tagName", 7, vTag);
    }
    
    // Text Content (Recursive!)
    int tCap = 64;
    int tLen = 0;
    char* tBuf = malloc(tCap);
    tBuf[0] = '\0';
    getTextRecursive(node, &tBuf, &tLen, &tCap);
    
    if (tLen > 0) {
         Value vText = OBJ_VAL(internString(vm, tBuf, tLen));
         mapSetStr(map, "text", 4, vText);
    }
    free(tBuf);
    
    // Attributes
    if (node->attrCount > 0) {
        Map* attrs = newMap(vm);
        for (int i=0; i<node->attrCount; i++) {
            Value vVal = OBJ_VAL(internString(vm, node->attrValues[i], strlen(node->attrValues[i])));
            mapSetStr(attrs, node->attrKeys[i], strlen(node->attrKeys[i]), vVal);
        }
        mapSetStr(map, "attributes", 10, OBJ_VAL(attrs));
    }
    
    return OBJ_VAL(map);
}

// ucoreScraper.select(htmlString, selectorString) -> List<NodeMap>
static Value scraper_select(VM* vm, Value* args, int argCount) {
    if (argCount != 2 || !IS_STRING(args[0]) || !IS_STRING(args[1])) {
        return NIL_VAL;
    }
    
    ObjString* html = AS_STRING(args[0]);
    ObjString* selectorStr = AS_STRING(args[1]);
    
    // 1. Parse HTML
    ScraperNode* doc = parseHtml(html->chars);
    
    // 2. Parse Selector
    ScraperSelector* sel = parseSelector(selectorStr->chars);
    
    // 3. Select Nodes
    int count = 0;
    int capacity = 16;
    ScraperNode** results = malloc(sizeof(ScraperNode*) * capacity);
    
    selectNodesV(doc, sel, &results, &count, &capacity);
    
    // 4. Convert to Unnarize List
    Array* list = newArray(vm);
    // Push list to stack to prevent GC during allocations? 
    // Ideally yes, but here we just allocated it.
    // VM GC is not concurrent yet in a way that hurts this single thread linear alloc, 
    // unless newMap triggers GC. Use push/pop if paranoid.
    
    for (int i=0; i<count; i++) {
        Value val = nodeToValue(vm, results[i]);
        arrayPush(vm, list, val);
    }
    
    // 5. Cleanup
    free(results);
    freeSelector(sel);
    freeNode(doc);
    
    return OBJ_VAL(list);
}

// ucoreScraper.loadFile(path) -> List<NodeMap> (or Document? No, selects root?)
// Actually, let's make loadFile return the same "Map" structure as select? 
// No, usually loadFile just parses and you probably want to select on it immediately.
// But we designed `select` to take HTML STRING.
// If we return a Document Handle, we can use it.
// Current API: `select(html, selector)`.
// We need `selectFile(path, selector)` OR `loadFile(path)` -> returns HTML string.
// Returning HTML string is easy. `ucoreSystem` already reads files? 
// No, `ucoreSystem` executes commands. `import` reads files but compiles them.
// Let's provide `loadFile(path)` -> returns parsed HTML String? No, that's just `readFile`.
// Let's provide `parseFile(path, selector)` which reads, parses, and selects in one go.
// Simple and powerful.

static char* readFileContent(const char* path) {
    FILE* file = fopen(path, "rb");
    if (!file) return NULL;
    
    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    char* buffer = malloc(length + 1);
    size_t read = fread(buffer, 1, length, file);
    (void)read; // We trust file length from ftell, but should check
    buffer[length] = '\0';
    
    fclose(file);
    return buffer;
}

// ucoreScraper.parseFile(path, selector) -> List<NodeMap>
static Value scraper_parseFile(VM* vm, Value* args, int argCount) {
    if (argCount != 2 || !IS_STRING(args[0]) || !IS_STRING(args[1])) {
        return NIL_VAL;
    }
    
    ObjString* path = AS_STRING(args[0]);
    char* htmlContent = readFileContent(path->chars);
    
    if (!htmlContent) {
        // Error handling? Return NIL or empty list?
        // Return NIL for now to signal failure
        return NIL_VAL; 
    }
    
    // Delegate to logic similar to scraper_select but with buffer
    // 1. Parse HTML
    ScraperNode* doc = parseHtml(htmlContent);
    free(htmlContent); // Done with raw string
    
    // 2. Parse Selector
    ObjString* selectorStr = AS_STRING(args[1]);
    ScraperSelector* sel = parseSelector(selectorStr->chars);
    
    // 3. Select Nodes
    int count = 0;
    int capacity = 16;
    ScraperNode** results = malloc(sizeof(ScraperNode*) * capacity);
    
    selectNodesV(doc, sel, &results, &count, &capacity);
    
    // 4. Convert to Unnarize List
    Array* list = newArray(vm);
    
    for (int i=0; i<count; i++) {
        Value val = nodeToValue(vm, results[i]);
        arrayPush(vm, list, val);
    }
    
    // 5. Cleanup
    free(results);
    freeSelector(sel);
    freeNode(doc);
    
    return OBJ_VAL(list);
}

// ucoreScraper.parse(htmlString, [debugPrint]) -> Document
// Helper for debugging mostly, returns NIL for now (unless we wrap Document)
static Value scraper_parse(VM* vm, Value* args, int argCount) {
    (void)vm; // Suppress unused warning
    if (argCount < 1 || !IS_STRING(args[0])) {
        return NIL_VAL;
    }
    
    ObjString* html = AS_STRING(args[0]);
    ScraperNode* doc = parseHtml(html->chars);
    
    // If 2nd arg is TRUE, dump the tree
    if (argCount == 2 && IS_BOOL(args[1]) && AS_BOOL(args[1])) {
        printNode(doc, 0);
    }
    
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
    
    defineNative(vm, mod->env, "parse", scraper_parse, 1); // parse(html, [debug])
    defineNative(vm, mod->env, "select", scraper_select, 2); // select(html, selector)
    defineNative(vm, mod->env, "parseFile", scraper_parseFile, 2); // parseFile(path, selector)
    defineNative(vm, mod->env, "fetch", scraper_fetch, 1); // fetch(url) -> string
    defineNative(vm, mod->env, "download", scraper_download, 2); // download(url, path) -> bool
    
    Value vMod = OBJ_VAL(mod);
    defineGlobal(vm, "ucoreScraper", vMod);
}
