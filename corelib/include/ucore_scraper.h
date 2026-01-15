#ifndef UCORE_SCRAPER_H
#define UCORE_SCRAPER_H

#include "vm.h"

// DOM Node Types
typedef enum {
    NODE_ELEMENT,
    NODE_TEXT,
    NODE_COMMENT,
    NODE_DOCUMENT  // Root node
} ScraperNodeType;

// DOM Node Structure
typedef struct ScraperNode {
    ScraperNodeType type;
    char* tagName;       // For elements (e.g., "div", "a"). Canonicalized to lowercase.
    char* textContent;   // For text nodes and comments.
    
    // Hierarchy
    struct ScraperNode* parent;
    struct ScraperNode** children;
    int childCount;
    int childCapacity;
    
    // Attributes (key/value pairs)
    char** attrKeys;
    char** attrValues;
    int attrCount;
    int attrCapacity;
    
    // Quick flags
    bool isSelfClosing;
    
} ScraperNode;

// Parser State
typedef struct {
    const char* source;
    int length;
    int current;
    ScraperNode* document;
    ScraperNode* currentNode;
} HtmlParser;

// Public API
void registerUCoreScraper(VM* vm);

#endif
