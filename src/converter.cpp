#include "mojo/converter.hpp"
#include "vendor/html2md.h"
#include <gumbo.h>
#include <sstream>
#include <vector>
#include <string>

namespace Mojo {

namespace {

void collect_links(GumboNode* node, std::vector<std::string>& links) {
    if (node->type != GUMBO_NODE_ELEMENT) return;
    
    if (node->v.element.tag == GUMBO_TAG_A) {
        GumboAttribute* href = gumbo_get_attribute(&node->v.element.attributes, "href");
        if (href) {
            links.emplace_back(href->value);
        }
    }
    
    const GumboVector* children = &node->v.element.children;
    for (unsigned int i = 0; i < children->length; ++i) {
        collect_links(static_cast<GumboNode*>(children->data[i]), links);
    }
}

}

std::string Converter::to_markdown(const std::string& html) {
    return html2md::Convert(html);
}

std::vector<std::string> Converter::extract_links(const std::string& html) {
    std::vector<std::string> links;
    if (html.empty()) return links;
    
    GumboOutput* output = gumbo_parse(html.c_str());
    collect_links(output->root, links);
    gumbo_destroy_output(&kGumboDefaultOptions, output);
    
    return links;
}

}
