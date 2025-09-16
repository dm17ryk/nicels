#include "icons.h"
#include <unordered_map>
#include <algorithm>

namespace nls {

static std::string ext(std::string_view name) {
    auto pos = name.rfind('.');
    if (pos == std::string_view::npos) return {};
    std::string e(name.substr(pos+1));
    std::transform(e.begin(), e.end(), e.begin(), [](unsigned char c){ return (char)std::tolower(c); });
    return e;
}

// A tiny starter set (Nerd Font glyphs). Users must use a Nerd Font in the terminal.
static const std::unordered_map<std::string, const char*> EXT_ICONS = {
    {"cpp", u8"\ue61d"}, // nf-dev-cplusplus
    {"cc",  u8"\ue61d"},
    {"cxx", u8"\ue61d"},
    {"hpp", u8"\ue61d"},
    {"h",   u8"\uf0fd"}, // generic file
    {"rb",  u8"\ue21e"}, // nf-dev-ruby
    {"py",  u8"\ue235"}, // nf-dev-python
    {"js",  u8"\ue74e"}, // nf-dev-javascript
    {"ts",  u8"\ue628"}, // nf-dev-typescript
    {"json",u8"\ue60b"}, // nf-dev-json
    {"md",  u8"\uf48a"}, // nf-oct-markdown
    {"txt", u8"\uf15c"}, // nf-fa-file_text_o
    {"png", u8"\uf1c5"}, // nf-fa-file_image_o
    {"jpg", u8"\uf1c5"},
    {"jpeg",u8"\uf1c5"},
    {"gif", u8"\uf1c5"},
    {"svg", u8"\uf1c5"},
    {"zip", u8"\uf1c6"}, // archive
    {"gz",  u8"\uf1c6"},
    {"7z",  u8"\uf1c6"},
    {"pdf", u8"\uf1c1"},
    {"exe", u8"\uf489"},
    {"bat", u8"\uf489"},
    {"sh",  u8"\uf489"},
};

std::string icon_for(std::string_view name, bool is_dir, bool is_exec) {
    if (is_dir) return u8"\uf07b"; // nf-fa-folder
    if (is_exec) return u8"\uf144"; // play (rough stand-in for executable)
    auto e = ext(name);
    auto it = EXT_ICONS.find(e);
    if (it != EXT_ICONS.end()) return it->second;
    return u8"\uf15b"; // generic file
}

} // namespace nls
