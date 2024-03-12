
#line 1 "ext/dtext/dtext.cpp.rl"
#include "dtext.h"
#include "url.h"

#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include <regex>

#ifdef DEBUG
#undef g_debug
#define STRINGIFY(x) XSTRINGIFY(x)
#define XSTRINGIFY(x) #x
#define g_debug(fmt, ...) fprintf(stderr, "\x1B[1;32mDEBUG\x1B[0m %-28.28s %-24.24s " fmt "\n", __FILE__ ":" STRINGIFY(__LINE__), __func__, ##__VA_ARGS__)
#else
#undef g_debug
#define g_debug(...)
#endif

static const size_t MAX_STACK_DEPTH = 512;

// Strip qualifier from tag: "Artoria Pendragon (Lancer) (Fate)" -> "Artoria Pendragon (Lancer)"
static const std::regex tag_qualifier_regex("[ _]\\([^)]+?\\)$");

// Permitted HTML attribute names.
static const std::unordered_map<std::string_view, const std::unordered_set<std::string_view>> permitted_attribute_names = {
  { "thead",    { "align" } },
  { "tbody",    { "align" } },
  { "tr",       { "align" } },
  { "td",       { "align", "colspan", "rowspan" } },
  { "th",       { "align", "colspan", "rowspan" } },
  { "col",      { "align", "span" } },
  { "colgroup", {} },
};

// Permitted HTML attribute values.
static const std::unordered_set<std::string_view> align_values = { "left", "center", "right", "justify" };
static const std::unordered_map<std::string_view, std::function<bool(std::string_view)>> permitted_attribute_values = {
  { "align",   [](auto value) { return align_values.find(value) != align_values.end(); } },
  { "span",    [](auto value) { return std::all_of(value.begin(), value.end(), isdigit); } },
  { "colspan", [](auto value) { return std::all_of(value.begin(), value.end(), isdigit); } },
  { "rowspan", [](auto value) { return std::all_of(value.begin(), value.end(), isdigit); } },
};


#line 785 "ext/dtext/dtext.cpp.rl"



#line 52 "ext/dtext/dtext.cpp"
static const int dtext_start = 1367;
static const int dtext_first_final = 1367;
static const int dtext_error = 0;

static const int dtext_en_basic_inline = 1386;
static const int dtext_en_inline = 1389;
static const int dtext_en_code = 1720;
static const int dtext_en_nodtext = 1724;
static const int dtext_en_table = 1728;
static const int dtext_en_main = 1367;


#line 788 "ext/dtext/dtext.cpp.rl"

static void dstack_push(StateMachine * sm, element_t element) {
  sm->dstack.push_back(element);
}

static element_t dstack_pop(StateMachine * sm) {
  if (sm->dstack.empty()) {
    g_debug("dstack pop empty stack");
    return DSTACK_EMPTY;
  } else {
    auto element = sm->dstack.back();
    sm->dstack.pop_back();
    return element;
  }
}

static element_t dstack_peek(const StateMachine * sm) {
  return sm->dstack.empty() ? DSTACK_EMPTY : sm->dstack.back();
}

static bool dstack_check(const StateMachine * sm, element_t expected_element) {
  return dstack_peek(sm) == expected_element;
}

// Return true if the given tag is currently open.
static bool dstack_is_open(const StateMachine * sm, element_t element) {
  return std::find(sm->dstack.begin(), sm->dstack.end(), element) != sm->dstack.end();
}

static int dstack_count(const StateMachine * sm, element_t element) {
  return std::count(sm->dstack.begin(), sm->dstack.end(), element);
}

static bool is_internal_url(StateMachine * sm, const std::string_view url) {
  if (url.starts_with("/")) {
    return true;
  } else if (sm->options.domain.empty() || url.empty()) {
    return false;
  } else {
    // Matches the domain name part of a URL.
    static const std::regex url_regex("^https?://(?:[^/?#]*@)?([^/?#:]+)", std::regex_constants::icase);

    std::match_results<std::string_view::const_iterator> matches;
    std::regex_search(url.begin(), url.end(), matches, url_regex);
    return matches[1] == sm->options.domain;
  }
}

static void append(StateMachine * sm, const auto c) {
  sm->output += c;
}

static void append(StateMachine * sm, const char * a, const char * b) {
  append(sm, std::string_view(a, b));
}

static void append_html_escaped(StateMachine * sm, char s) {
  switch (s) {
    case '<': append(sm, "&lt;"); break;
    case '>': append(sm, "&gt;"); break;
    case '&': append(sm, "&amp;"); break;
    case '"': append(sm, "&quot;"); break;
    default:  append(sm, s);
  }
}

static void append_html_escaped(StateMachine * sm, const std::string_view string) {
  for (const unsigned char c : string) {
    append_html_escaped(sm, c);
  }
}

static void append_uri_escaped(StateMachine * sm, const std::string_view string) {
  static const char hex[] = "0123456789ABCDEF";

  for (const unsigned char c : string) {
    if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '-' || c == '_' || c == '.' || c == '~') {
      append(sm, c);
    } else {
      append(sm, '%');
      append(sm, hex[c >> 4]);
      append(sm, hex[c & 0x0F]);
    }
  }
}

static void append_relative_url(StateMachine * sm, const auto url) {
  if ((url[0] == '/' || url[0] == '#') && !sm->options.base_url.empty()) {
    append_html_escaped(sm, sm->options.base_url);
  }

  append_html_escaped(sm, url);
}

static void append_absolute_link(StateMachine * sm, const std::string_view url, const std::string_view title, bool internal_url, bool escape_title) {
  if (internal_url) {
    append(sm, "<a class=\"dtext-link\" href=\"");
  } else if (url == title) {
    append(sm, "<a rel=\"external nofollow noreferrer\" class=\"dtext-link dtext-external-link\" href=\"");
  } else {
    append(sm, "<a rel=\"external nofollow noreferrer\" class=\"dtext-link dtext-external-link dtext-named-external-link\" href=\"");
  }

  append_html_escaped(sm, url);
  append(sm, "\">");

  if (escape_title) {
    append_html_escaped(sm, title);
  } else {
    append(sm, title);
  }

  append(sm, "</a>");
}

static void append_mention(StateMachine * sm, const std::string_view name) {
  append(sm, "<a class=\"dtext-link dtext-user-mention-link\" data-user-name=\"");
  append_html_escaped(sm, name);
  append(sm, "\" href=\"");
  append_relative_url(sm, "/users?name=");
  append_uri_escaped(sm, name);
  append(sm, "\">@");
  append_html_escaped(sm, name);
  append(sm, "</a>");
}

static void append_id_link(StateMachine * sm, const char * title, const char * id_name, const char * url, const std::string_view id) {
  if (url[0] == '/') {
    append(sm, "<a class=\"dtext-link dtext-id-link dtext-");
    append(sm, id_name);
    append(sm, "-id-link\" href=\"");
    append_relative_url(sm, url);
  } else {
    append(sm, "<a rel=\"external nofollow noreferrer\" class=\"dtext-link dtext-id-link dtext-");
    append(sm, id_name);
    append(sm, "-id-link\" href=\"");
    append_html_escaped(sm, url);
  }

  append_uri_escaped(sm, id);
  append(sm, "\">");
  append(sm, title);
  append(sm, " #");
  append_html_escaped(sm, id);
  append(sm, "</a>");
}

static void append_bare_unnamed_url(StateMachine * sm, const std::string_view url) {
  auto [trimmed_url, leftovers] = trim_url(url);
  append_unnamed_url(sm, trimmed_url);
  append_html_escaped(sm, leftovers);
}

static void append_unnamed_url(StateMachine * sm, const std::string_view url) {
  DText::URL parsed_url(url);

  if (sm->options.internal_domains.find(std::string(parsed_url.domain)) != sm->options.internal_domains.end()) {
    append_internal_url(sm, parsed_url);
  } else {
    append_absolute_link(sm, url, url, parsed_url.domain == sm->options.domain);
  }
}

static void append_internal_url(StateMachine * sm, const DText::URL& url) {
  auto path_components = url.path_components();
  auto query = url.query;
  auto fragment = url.fragment;

  if (path_components.size() == 2) {
    auto controller = path_components.at(0);
    auto id = path_components.at(1);

    if (!id.empty() && std::all_of(id.begin(), id.end(), ::isdigit)) {
      if (controller == "post" && fragment.empty()) {
        // https://danbooru.donmai.us/posts/6000000#comment_2288996
        return append_id_link(sm, "post", "post", "/posts/", id);
      } else if (controller == "pool" && query.empty()) {
        // https://danbooru.donmai.us/pools/903?page=2
        return append_id_link(sm, "pool", "pool", "/pools/", id);
      } else if (controller == "comment") {
        return append_id_link(sm, "comment", "comment", "/comments/", id);
      } else if (controller == "forum") {
        return append_id_link(sm, "forum", "forum-post", "/forums/", id);
      } else if (controller == "forum" && query.empty() && fragment.empty()) {
        // https://danbooru.donmai.us/forum_topics/1234?page=2
        // https://danbooru.donmai.us/forum_topics/1234#forum_post_5678
        return append_id_link(sm, "topic", "forum-topic", "/forums/", id);
      } else if (controller == "user") {
        return append_id_link(sm, "user", "user", "/users/", id);
      } else if (controller == "artist") {
        return append_id_link(sm, "artist", "artist", "/artists/", id);
      } else if (controller == "wiki" && fragment.empty()) {
        // http://danbooru.donmai.us/wiki_pages/10933#dtext-self-upload
        return append_id_link(sm, "wiki", "wiki-page", "/wiki/", id);
      }
    } else if (controller == "wiki" && fragment.empty()) {
      return append_wiki_link(sm, {}, id, {}, id, {});
    }
  } else if (path_components.size() >= 3) {
    // http://danbooru.donmai.us/post/show/1234/touhou
    auto controller = path_components.at(0);
    auto action = path_components.at(1);
    auto id = path_components.at(2);

    if (!id.empty() && std::all_of(id.begin(), id.end(), ::isdigit)) {
      if (controller == "post" && action == "show") {
        return append_id_link(sm, "post", "post", "/posts/", id);
      }
    }
  }

  append_absolute_link(sm, url.url, url.url, url.domain == sm->options.domain);
}

static void append_named_url(StateMachine * sm, const std::string_view url, const std::string_view title) {
  auto parsed_title = sm->parse_basic_inline(title);

  // protocol-relative url; treat `//example.com` like `http://example.com`
  if (url.size() > 2 && url.starts_with("//")) {
    auto full_url = "http:" + std::string(url);
    append_absolute_link(sm, full_url, parsed_title, is_internal_url(sm, full_url), false);
  } else if (url[0] == '/' || url[0] == '#') {
    append(sm, "<a class=\"dtext-link\" href=\"");
    append_relative_url(sm, url);
    append(sm, "\">");
    append(sm, parsed_title);
    append(sm, "</a>");
  } else if (url == title) {
    append_unnamed_url(sm, url);
  } else {
    append_absolute_link(sm, url, parsed_title, is_internal_url(sm, url), false);
  }
}

static void append_bare_named_url(StateMachine * sm, const std::string_view url, std::string_view title) {
  auto [trimmed_url, leftovers] = trim_url(url);
  append_named_url(sm, trimmed_url, title);
  append_html_escaped(sm, leftovers);
}

static void append_post_search_link(StateMachine * sm, const std::string_view prefix, const std::string_view search, const std::string_view title, const std::string_view suffix) {
  auto normalized_title = std::string(title);

  append(sm, "<a class=\"dtext-link dtext-post-search-link\" href=\"");
  append_relative_url(sm, "/post?tags=");
  append_uri_escaped(sm, search);
  append(sm, "\">");

  // 19{{60s}} -> {{60s|1960s}}
  if (!prefix.empty()) {
    normalized_title.insert(0, prefix);
  }

  // {{pokemon_(creature)|}} -> {{pokemon_(creature)|pokemon}}
  if (title.empty()) {
    std::regex_replace(std::back_inserter(normalized_title), search.begin(), search.end(), tag_qualifier_regex, "");
  }

  // {{cat}}s -> {{cat|cats}}
  if (!suffix.empty()) {
    normalized_title.append(suffix);
  }

  append_html_escaped(sm, normalized_title);
  append(sm, "</a>");

  clear_matches(sm);
}

static void append_wiki_link(StateMachine * sm, const std::string_view prefix, const std::string_view tag, const std::string_view anchor, const std::string_view title, const std::string_view suffix) {
  auto normalized_tag = std::string(tag);
  auto title_string = std::string(title);

  // "Kantai Collection" -> "kantai_collection"
  std::transform(normalized_tag.cbegin(), normalized_tag.cend(), normalized_tag.begin(), [](unsigned char c) { return c == ' ' ? '_' : std::tolower(c); });

  // [[2019]] -> [[~2019]]
  if (std::all_of(normalized_tag.cbegin(), normalized_tag.cend(), ::isdigit)) {
    normalized_tag.insert(0, "~");
  }

  // Pipe trick: [[Kaga (Kantai Collection)|]] -> [[kaga_(kantai_collection)|Kaga]]
  if (title_string.empty()) {
    std::regex_replace(std::back_inserter(title_string), tag.cbegin(), tag.cend(), tag_qualifier_regex, "");
  }

  // 19[[60s]] -> [[60s|1960s]]
  if (!prefix.empty()) {
    title_string.insert(0, prefix);
  }

  // [[cat]]s -> [[cat|cats]]
  if (!suffix.empty()) {
    title_string.append(suffix);
  }

  append(sm, "<a class=\"dtext-link dtext-wiki-link\" href=\"");
  append_relative_url(sm, "/wiki/");
  append_uri_escaped(sm, normalized_tag);

  if (!anchor.empty()) {
    std::string normalized_anchor(anchor);
    std::transform(normalized_anchor.begin(), normalized_anchor.end(), normalized_anchor.begin(), [](char c) { return isalnum(c) ? tolower(c) : '-'; });
    append_html_escaped(sm, "#dtext-");
    append_html_escaped(sm, normalized_anchor);
  }

  append(sm, "\">");
  append_html_escaped(sm, title_string);
  append(sm, "</a>");

  sm->wiki_pages.insert(std::string(tag));

  clear_matches(sm);
}

static void append_paged_link(StateMachine * sm, const char * title, const char * tag, const char * href, const char * param) {
  append(sm, tag);
  append_relative_url(sm, href);
  append(sm, sm->a1, sm->a2);
  append(sm, param);
  append(sm, sm->b1, sm->b2);
  append(sm, "\">");
  append(sm, title);
  append(sm, sm->a1, sm->a2);
  append(sm, "/p");
  append(sm, sm->b1, sm->b2);
  append(sm, "</a>");
}

static void append_dmail_key_link(StateMachine * sm) {
  append(sm, "<a class=\"dtext-link dtext-id-link dtext-dmail-id-link\" href=\"");
  append_relative_url(sm, "/dmails/");
  append(sm, sm->a1, sm->a2);
  append(sm, "?key=");
  append_uri_escaped(sm, { sm->b1, sm->b2 });
  append(sm, "\">");
  append(sm, "dmail #");
  append(sm, sm->a1, sm->a2);
  append(sm, "</a>");
}

static void append_code_fence(StateMachine * sm, const std::string_view code, const std::string_view language) {
  if (language.empty()) {
    append_block(sm, "<pre>");
    append_html_escaped(sm, code);
    append_block(sm, "</pre>");
  } else {
    append_block(sm, "<pre class=\"language-");
    append_html_escaped(sm, language);
    append_block(sm, "\">");
    append_html_escaped(sm, code);
    append_block(sm, "</pre>");
  }
}

static void append_inline_code(StateMachine * sm, const std::string_view language = {}) {
  if (language.empty()) {
    dstack_open_element(sm, INLINE_CODE, "<code>");
  } else {
    dstack_open_element(sm, INLINE_CODE, "<code class=\"language-");
    append_html_escaped(sm, language);
    append(sm, "\">");
  }
}

static void append_block_code(StateMachine * sm, const std::string_view language = {}) {
  dstack_close_leaf_blocks(sm);

  if (language.empty()) {
    dstack_open_element(sm, BLOCK_CODE, "<pre>");
  } else {
    dstack_open_element(sm, BLOCK_CODE, "<pre class=\"language-");
    append_html_escaped(sm, language);
    append(sm, "\">");
  }
}

static void append_header(StateMachine * sm, char header, const std::string_view id) {
  static element_t blocks[] = { BLOCK_H1, BLOCK_H2, BLOCK_H3, BLOCK_H4, BLOCK_H5, BLOCK_H6 };
  element_t block = blocks[header - '1'];

  if (id.empty()) {
    dstack_open_element(sm, block, "<h");
    append_block(sm, header);
    append_block(sm, ">");
  } else {
    auto normalized_id = std::string(id);
    std::transform(id.begin(), id.end(), normalized_id.begin(), [](char c) { return isalnum(c) ? tolower(c) : '-'; });

    dstack_open_element(sm, block, "<h");
    append_block(sm, header);
    append_block(sm, " id=\"dtext-");
    append_block(sm, normalized_id);
    append_block(sm, "\">");
  }

  sm->header_mode = true;
}

static void append_block(StateMachine * sm, const auto s) {
  if (!sm->options.f_inline) {
    append(sm, s);
  }
}

static void append_block_html_escaped(StateMachine * sm, const std::string_view string) {
  if (!sm->options.f_inline) {
    append_html_escaped(sm, string);
  }
}

static void append_closing_p(StateMachine * sm) {
  g_debug("append closing p");

  if (sm->output.size() > 4 && sm->output.ends_with("<br>")) {
    g_debug("trim last <br>");
    sm->output.resize(sm->output.size() - 4);
  }

  if (sm->output.size() > 3 && sm->output.ends_with("<p>")) {
    g_debug("trim last <p>");
    sm->output.resize(sm->output.size() - 3);
    return;
  }

  append_block(sm, "</p>");
}

static void dstack_open_element(StateMachine * sm, element_t type, const char * html) {
  g_debug("opening %s", html);

  dstack_push(sm, type);

  if (type >= INLINE) {
    append(sm, html);
  } else {
    append_block(sm, html);
  }
}

static void dstack_open_element(StateMachine * sm, element_t type, std::string_view tag_name, const StateMachine::TagAttributes& tag_attributes) {
  dstack_push(sm, type);
  append_block(sm, "<");
  append_block(sm, tag_name);

  auto& permitted_names = permitted_attribute_names.at(tag_name);
  for (auto& [name, value] : tag_attributes) {
    if (permitted_names.find(name) != permitted_names.end()) {
      auto validate_value = permitted_attribute_values.at(name);

      if (validate_value(value)) {
        append_block(sm, " ");
        append_block_html_escaped(sm, name);
        append_block(sm, "=\"");
        append_block_html_escaped(sm, value);
        append_block(sm, "\"");
      }
    }
  }

  append_block(sm, ">");
  clear_tag_attributes(sm);
}

static bool dstack_close_element(StateMachine * sm, element_t type) {
  if (dstack_check(sm, type)) {
    dstack_rewind(sm);
    return true;
  } else if (type >= INLINE && dstack_peek(sm) >= INLINE) {
    g_debug("out-of-order close %s; closing %s instead", element_names[type], element_names[dstack_peek(sm)]);
    dstack_rewind(sm);
    return true;
  } else if (type >= INLINE) {
    g_debug("out-of-order closing %s", element_names[type]);
    append_html_escaped(sm, { sm->ts, sm->te });
    return false;
  } else {
    g_debug("out-of-order closing %s", element_names[type]);
    append_block_html_escaped(sm, { sm->ts, sm->te });
    return false;
  }
}

// Close the last open tag.
static void dstack_rewind(StateMachine * sm) {
  element_t element = dstack_pop(sm);
  g_debug("dstack rewind %s", element_names[element]);

  switch(element) {
    case BLOCK_P: append_closing_p(sm); break;
    case INLINE_SPOILER: append(sm, "</span>"); break;
    case BLOCK_SPOILER: append_block(sm, "</div>"); break;
    case BLOCK_QUOTE: append_block(sm, "</blockquote>"); break;
    case BLOCK_EXPAND: append_block(sm, "</div></details>"); break;
    case BLOCK_NODTEXT: append_block(sm, "</p>"); break;
    case BLOCK_CODE: append_block(sm, "</pre>"); break;
    case BLOCK_TD: append_block(sm, "</td>"); break;
    case BLOCK_TH: append_block(sm, "</th>"); break;

    case INLINE_NODTEXT: break;
    case INLINE_B: append(sm, "</strong>"); break;
    case INLINE_I: append(sm, "</em>"); break;
    case INLINE_U: append(sm, "</u>"); break;
    case INLINE_S: append(sm, "</s>"); break;
    case INLINE_TN: append(sm, "</span>"); break;
    case INLINE_CENTER: append(sm, "</div>"); break;
    case INLINE_COLOR: append(sm, "</span>"); break;
    case INLINE_CODE: append(sm, "</code>"); break;

    case BLOCK_TN: append_closing_p(sm); break;
    case BLOCK_CENTER: append_closing_p(sm); break;
    case BLOCK_COLOR: append_closing_p(sm); break;
    case BLOCK_TABLE: append_block(sm, "</table>"); break;
    case BLOCK_COLGROUP: append_block(sm, "</colgroup>"); break;
    case BLOCK_THEAD: append_block(sm, "</thead>"); break;
    case BLOCK_TBODY: append_block(sm, "</tbody>"); break;
    case BLOCK_TR: append_block(sm, "</tr>"); break;
    case BLOCK_UL: append_block(sm, "</ul>"); break;
    case BLOCK_LI: append_block(sm, "</li>"); break;
    case BLOCK_H6: append_block(sm, "</h6>"); sm->header_mode = false; break;
    case BLOCK_H5: append_block(sm, "</h5>"); sm->header_mode = false; break;
    case BLOCK_H4: append_block(sm, "</h4>"); sm->header_mode = false; break;
    case BLOCK_H3: append_block(sm, "</h3>"); sm->header_mode = false; break;
    case BLOCK_H2: append_block(sm, "</h2>"); sm->header_mode = false; break;
    case BLOCK_H1: append_block(sm, "</h1>"); sm->header_mode = false; break;

    // Should never happen.
    case INLINE: break;
    case DSTACK_EMPTY: break;
  } 
}

// container blocks: [spoiler], [quote], [expand], [tn], [center], [color]
// leaf blocks: [nodtext], [code], [table], [td]?, [th]?, <h1>, <p>, <li>, <ul>
static void dstack_close_leaf_blocks(StateMachine * sm) {
  g_debug("dstack close leaf blocks");

  while (!sm->dstack.empty() && !dstack_check(sm, BLOCK_QUOTE) && !dstack_check(sm, BLOCK_SPOILER) && !dstack_check(sm, BLOCK_EXPAND) && !dstack_check(sm, BLOCK_TN) && !dstack_check(sm, BLOCK_CENTER) && !dstack_check(sm, BLOCK_COLOR)) {
    dstack_rewind(sm);
  }
}

// Close all open tags up to and including the given tag.
static void dstack_close_until(StateMachine * sm, element_t element) {
  while (!sm->dstack.empty() && !dstack_check(sm, element)) {
    dstack_rewind(sm);
  }

  dstack_rewind(sm);
}

// Close all remaining open tags.
static void dstack_close_all(StateMachine * sm) {
  while (!sm->dstack.empty()) {
    dstack_rewind(sm);
  }
}

static void dstack_open_list(StateMachine * sm, int depth) {
  g_debug("open list");

  if (dstack_is_open(sm, BLOCK_LI)) {
    dstack_close_until(sm, BLOCK_LI);
  } else {
    dstack_close_leaf_blocks(sm);
  }

  while (dstack_count(sm, BLOCK_UL) < depth) {
    dstack_open_element(sm, BLOCK_UL, "<ul>");
  }

  while (dstack_count(sm, BLOCK_UL) > depth) {
    dstack_close_until(sm, BLOCK_UL);
  }

  dstack_open_element(sm, BLOCK_LI, "<li>");
}

static void dstack_close_list(StateMachine * sm) {
  while (dstack_is_open(sm, BLOCK_UL)) {
    dstack_close_until(sm, BLOCK_UL);
  }
}

static void save_tag_attribute(StateMachine * sm, const std::string_view name, const std::string_view value) {
  sm->tag_attributes[name] = value;
}

static void clear_tag_attributes(StateMachine * sm) {
  sm->tag_attributes.clear();
}

static void clear_matches(StateMachine * sm) {
  sm->a1 = NULL;
  sm->a2 = NULL;
  sm->b1 = NULL;
  sm->b2 = NULL;
  sm->c1 = NULL;
  sm->c2 = NULL;
  sm->d1 = NULL;
  sm->d2 = NULL;
  sm->e1 = NULL;
  sm->e2 = NULL;
}

// True if a mention is allowed to start after this character.
static bool is_mention_boundary(unsigned char c) {
  switch (c) {
    case '\0': return true;
    case '\r': return true;
    case '\n': return true;
    case ' ':  return true;
    case '/':  return true;
    case '"':  return true;
    case '\'': return true;
    case '(':  return true;
    case ')':  return true;
    case '[':  return true;
    case ']':  return true;
    case '{':  return true;
    case '}':  return true;
    default:   return false;
  }
}

// Trim trailing unbalanced ')' characters from the URL.
static std::tuple<std::string_view, std::string_view> trim_url(const std::string_view url) {
  std::string_view trimmed = url;

  while (!trimmed.empty() && trimmed.back() == ')' && std::count(trimmed.begin(), trimmed.end(), ')') > std::count(trimmed.begin(), trimmed.end(), '(')) {
    trimmed.remove_suffix(1);
  }

  return { trimmed, { trimmed.end(), url.end() } };
}

// Replace CRLF sequences with LF.
static void replace_newlines(const std::string_view input, std::string& output) {
  size_t pos, last = 0;

  while (std::string::npos != (pos = input.find("\r\n", last))) {
    output.append(input, last, pos - last);
    output.append("\n");
    last = pos + 2;
  }

  output.append(input, last, pos - last);
}

StateMachine::StateMachine(const auto string, int initial_state, const DTextOptions options) : options(options) {
  // Add null bytes to the beginning and end of the string as start and end of string markers.
  input.reserve(string.size());
  input.append(1, '\0');
  replace_newlines(string, input);
  input.append(1, '\0');

  output.reserve(string.size() * 1.5);
  stack.reserve(16);
  dstack.reserve(16);

  p = input.c_str();
  pb = input.c_str();
  pe = input.c_str() + input.size();
  eof = pe;
  cs = initial_state;
}

std::string StateMachine::parse_inline(const std::string_view dtext) {
  StateMachine sm(dtext, dtext_en_inline, options);
  return sm.parse();
}

std::string StateMachine::parse_basic_inline(const std::string_view dtext) {
  StateMachine sm(dtext, dtext_en_basic_inline, options);
  return sm.parse();
}

StateMachine::ParseResult StateMachine::parse_dtext(const std::string_view dtext, DTextOptions options) {
  StateMachine sm(dtext, dtext_en_main, options);
  return { sm.parse(), sm.wiki_pages };
}

std::string StateMachine::parse() {
  StateMachine* sm = this;
  g_debug("parse '%.*s'", (int)(sm->input.size() - 2), sm->input.c_str() + 1);

  
#line 754 "ext/dtext/dtext.cpp"
	{
	( sm->top) = 0;
	( sm->ts) = 0;
	( sm->te) = 0;
	( sm->act) = 0;
	}

#line 1476 "ext/dtext/dtext.cpp.rl"
  
#line 764 "ext/dtext/dtext.cpp"
	{
	short _widec;
	if ( ( sm->p) == ( sm->pe) )
		goto _test_eof;
	goto _resume;

_again:
	switch (  sm->cs ) {
		case 1367: goto st1367;
		case 1368: goto st1368;
		case 1: goto st1;
		case 1369: goto st1369;
		case 2: goto st2;
		case 3: goto st3;
		case 4: goto st4;
		case 5: goto st5;
		case 6: goto st6;
		case 7: goto st7;
		case 8: goto st8;
		case 9: goto st9;
		case 10: goto st10;
		case 11: goto st11;
		case 12: goto st12;
		case 1370: goto st1370;
		case 13: goto st13;
		case 14: goto st14;
		case 15: goto st15;
		case 16: goto st16;
		case 17: goto st17;
		case 18: goto st18;
		case 19: goto st19;
		case 20: goto st20;
		case 21: goto st21;
		case 22: goto st22;
		case 23: goto st23;
		case 24: goto st24;
		case 25: goto st25;
		case 26: goto st26;
		case 27: goto st27;
		case 28: goto st28;
		case 29: goto st29;
		case 30: goto st30;
		case 31: goto st31;
		case 1371: goto st1371;
		case 32: goto st32;
		case 1372: goto st1372;
		case 1373: goto st1373;
		case 33: goto st33;
		case 1374: goto st1374;
		case 34: goto st34;
		case 35: goto st35;
		case 36: goto st36;
		case 37: goto st37;
		case 38: goto st38;
		case 39: goto st39;
		case 40: goto st40;
		case 41: goto st41;
		case 42: goto st42;
		case 43: goto st43;
		case 1375: goto st1375;
		case 44: goto st44;
		case 45: goto st45;
		case 46: goto st46;
		case 47: goto st47;
		case 48: goto st48;
		case 49: goto st49;
		case 50: goto st50;
		case 1376: goto st1376;
		case 51: goto st51;
		case 1377: goto st1377;
		case 52: goto st52;
		case 53: goto st53;
		case 54: goto st54;
		case 55: goto st55;
		case 56: goto st56;
		case 57: goto st57;
		case 58: goto st58;
		case 59: goto st59;
		case 60: goto st60;
		case 61: goto st61;
		case 62: goto st62;
		case 63: goto st63;
		case 64: goto st64;
		case 65: goto st65;
		case 66: goto st66;
		case 1378: goto st1378;
		case 67: goto st67;
		case 1379: goto st1379;
		case 68: goto st68;
		case 69: goto st69;
		case 70: goto st70;
		case 71: goto st71;
		case 72: goto st72;
		case 73: goto st73;
		case 74: goto st74;
		case 1380: goto st1380;
		case 75: goto st75;
		case 76: goto st76;
		case 77: goto st77;
		case 78: goto st78;
		case 79: goto st79;
		case 80: goto st80;
		case 81: goto st81;
		case 82: goto st82;
		case 1381: goto st1381;
		case 83: goto st83;
		case 84: goto st84;
		case 85: goto st85;
		case 1382: goto st1382;
		case 86: goto st86;
		case 87: goto st87;
		case 88: goto st88;
		case 1383: goto st1383;
		case 1384: goto st1384;
		case 89: goto st89;
		case 90: goto st90;
		case 91: goto st91;
		case 92: goto st92;
		case 93: goto st93;
		case 94: goto st94;
		case 95: goto st95;
		case 96: goto st96;
		case 97: goto st97;
		case 98: goto st98;
		case 99: goto st99;
		case 100: goto st100;
		case 101: goto st101;
		case 102: goto st102;
		case 103: goto st103;
		case 104: goto st104;
		case 105: goto st105;
		case 106: goto st106;
		case 107: goto st107;
		case 108: goto st108;
		case 109: goto st109;
		case 110: goto st110;
		case 111: goto st111;
		case 112: goto st112;
		case 113: goto st113;
		case 114: goto st114;
		case 115: goto st115;
		case 116: goto st116;
		case 117: goto st117;
		case 118: goto st118;
		case 119: goto st119;
		case 120: goto st120;
		case 121: goto st121;
		case 122: goto st122;
		case 123: goto st123;
		case 124: goto st124;
		case 125: goto st125;
		case 126: goto st126;
		case 127: goto st127;
		case 128: goto st128;
		case 129: goto st129;
		case 130: goto st130;
		case 131: goto st131;
		case 132: goto st132;
		case 1385: goto st1385;
		case 133: goto st133;
		case 134: goto st134;
		case 135: goto st135;
		case 136: goto st136;
		case 137: goto st137;
		case 138: goto st138;
		case 139: goto st139;
		case 140: goto st140;
		case 141: goto st141;
		case 142: goto st142;
		case 1386: goto st1386;
		case 1387: goto st1387;
		case 143: goto st143;
		case 144: goto st144;
		case 145: goto st145;
		case 146: goto st146;
		case 147: goto st147;
		case 148: goto st148;
		case 149: goto st149;
		case 150: goto st150;
		case 151: goto st151;
		case 152: goto st152;
		case 153: goto st153;
		case 154: goto st154;
		case 155: goto st155;
		case 156: goto st156;
		case 157: goto st157;
		case 158: goto st158;
		case 159: goto st159;
		case 160: goto st160;
		case 161: goto st161;
		case 1388: goto st1388;
		case 162: goto st162;
		case 163: goto st163;
		case 164: goto st164;
		case 165: goto st165;
		case 166: goto st166;
		case 167: goto st167;
		case 168: goto st168;
		case 169: goto st169;
		case 170: goto st170;
		case 1389: goto st1389;
		case 1390: goto st1390;
		case 1391: goto st1391;
		case 171: goto st171;
		case 172: goto st172;
		case 173: goto st173;
		case 1392: goto st1392;
		case 1393: goto st1393;
		case 1394: goto st1394;
		case 174: goto st174;
		case 1395: goto st1395;
		case 175: goto st175;
		case 1396: goto st1396;
		case 176: goto st176;
		case 177: goto st177;
		case 178: goto st178;
		case 179: goto st179;
		case 180: goto st180;
		case 1397: goto st1397;
		case 181: goto st181;
		case 182: goto st182;
		case 183: goto st183;
		case 184: goto st184;
		case 185: goto st185;
		case 186: goto st186;
		case 187: goto st187;
		case 188: goto st188;
		case 189: goto st189;
		case 190: goto st190;
		case 191: goto st191;
		case 192: goto st192;
		case 193: goto st193;
		case 194: goto st194;
		case 195: goto st195;
		case 196: goto st196;
		case 197: goto st197;
		case 198: goto st198;
		case 199: goto st199;
		case 200: goto st200;
		case 201: goto st201;
		case 202: goto st202;
		case 203: goto st203;
		case 204: goto st204;
		case 205: goto st205;
		case 206: goto st206;
		case 207: goto st207;
		case 208: goto st208;
		case 209: goto st209;
		case 210: goto st210;
		case 1398: goto st1398;
		case 211: goto st211;
		case 212: goto st212;
		case 213: goto st213;
		case 214: goto st214;
		case 215: goto st215;
		case 216: goto st216;
		case 217: goto st217;
		case 218: goto st218;
		case 1399: goto st1399;
		case 219: goto st219;
		case 220: goto st220;
		case 221: goto st221;
		case 222: goto st222;
		case 223: goto st223;
		case 224: goto st224;
		case 225: goto st225;
		case 226: goto st226;
		case 227: goto st227;
		case 228: goto st228;
		case 229: goto st229;
		case 230: goto st230;
		case 231: goto st231;
		case 232: goto st232;
		case 233: goto st233;
		case 234: goto st234;
		case 235: goto st235;
		case 236: goto st236;
		case 237: goto st237;
		case 238: goto st238;
		case 239: goto st239;
		case 240: goto st240;
		case 241: goto st241;
		case 242: goto st242;
		case 243: goto st243;
		case 244: goto st244;
		case 1400: goto st1400;
		case 1401: goto st1401;
		case 245: goto st245;
		case 246: goto st246;
		case 247: goto st247;
		case 248: goto st248;
		case 249: goto st249;
		case 250: goto st250;
		case 251: goto st251;
		case 252: goto st252;
		case 253: goto st253;
		case 254: goto st254;
		case 255: goto st255;
		case 256: goto st256;
		case 1402: goto st1402;
		case 257: goto st257;
		case 258: goto st258;
		case 259: goto st259;
		case 260: goto st260;
		case 261: goto st261;
		case 262: goto st262;
		case 1403: goto st1403;
		case 263: goto st263;
		case 264: goto st264;
		case 265: goto st265;
		case 266: goto st266;
		case 267: goto st267;
		case 268: goto st268;
		case 269: goto st269;
		case 270: goto st270;
		case 271: goto st271;
		case 272: goto st272;
		case 273: goto st273;
		case 274: goto st274;
		case 275: goto st275;
		case 276: goto st276;
		case 277: goto st277;
		case 278: goto st278;
		case 279: goto st279;
		case 280: goto st280;
		case 281: goto st281;
		case 282: goto st282;
		case 283: goto st283;
		case 284: goto st284;
		case 285: goto st285;
		case 286: goto st286;
		case 287: goto st287;
		case 288: goto st288;
		case 289: goto st289;
		case 290: goto st290;
		case 291: goto st291;
		case 292: goto st292;
		case 293: goto st293;
		case 1404: goto st1404;
		case 294: goto st294;
		case 295: goto st295;
		case 296: goto st296;
		case 297: goto st297;
		case 298: goto st298;
		case 299: goto st299;
		case 300: goto st300;
		case 301: goto st301;
		case 302: goto st302;
		case 303: goto st303;
		case 304: goto st304;
		case 305: goto st305;
		case 306: goto st306;
		case 307: goto st307;
		case 308: goto st308;
		case 309: goto st309;
		case 310: goto st310;
		case 311: goto st311;
		case 312: goto st312;
		case 313: goto st313;
		case 314: goto st314;
		case 315: goto st315;
		case 316: goto st316;
		case 317: goto st317;
		case 318: goto st318;
		case 319: goto st319;
		case 320: goto st320;
		case 321: goto st321;
		case 322: goto st322;
		case 323: goto st323;
		case 324: goto st324;
		case 325: goto st325;
		case 326: goto st326;
		case 327: goto st327;
		case 328: goto st328;
		case 329: goto st329;
		case 330: goto st330;
		case 331: goto st331;
		case 332: goto st332;
		case 333: goto st333;
		case 334: goto st334;
		case 1405: goto st1405;
		case 335: goto st335;
		case 336: goto st336;
		case 337: goto st337;
		case 1406: goto st1406;
		case 338: goto st338;
		case 339: goto st339;
		case 340: goto st340;
		case 341: goto st341;
		case 342: goto st342;
		case 343: goto st343;
		case 344: goto st344;
		case 345: goto st345;
		case 346: goto st346;
		case 347: goto st347;
		case 348: goto st348;
		case 1407: goto st1407;
		case 349: goto st349;
		case 350: goto st350;
		case 351: goto st351;
		case 352: goto st352;
		case 353: goto st353;
		case 354: goto st354;
		case 355: goto st355;
		case 356: goto st356;
		case 357: goto st357;
		case 358: goto st358;
		case 359: goto st359;
		case 360: goto st360;
		case 361: goto st361;
		case 1408: goto st1408;
		case 362: goto st362;
		case 363: goto st363;
		case 364: goto st364;
		case 365: goto st365;
		case 366: goto st366;
		case 367: goto st367;
		case 368: goto st368;
		case 369: goto st369;
		case 370: goto st370;
		case 371: goto st371;
		case 372: goto st372;
		case 373: goto st373;
		case 374: goto st374;
		case 375: goto st375;
		case 376: goto st376;
		case 377: goto st377;
		case 378: goto st378;
		case 379: goto st379;
		case 380: goto st380;
		case 381: goto st381;
		case 382: goto st382;
		case 383: goto st383;
		case 1409: goto st1409;
		case 384: goto st384;
		case 385: goto st385;
		case 386: goto st386;
		case 387: goto st387;
		case 388: goto st388;
		case 389: goto st389;
		case 390: goto st390;
		case 391: goto st391;
		case 392: goto st392;
		case 393: goto st393;
		case 394: goto st394;
		case 1410: goto st1410;
		case 395: goto st395;
		case 396: goto st396;
		case 397: goto st397;
		case 398: goto st398;
		case 399: goto st399;
		case 400: goto st400;
		case 401: goto st401;
		case 402: goto st402;
		case 403: goto st403;
		case 404: goto st404;
		case 405: goto st405;
		case 1411: goto st1411;
		case 406: goto st406;
		case 407: goto st407;
		case 408: goto st408;
		case 409: goto st409;
		case 410: goto st410;
		case 411: goto st411;
		case 412: goto st412;
		case 413: goto st413;
		case 414: goto st414;
		case 1412: goto st1412;
		case 1413: goto st1413;
		case 415: goto st415;
		case 416: goto st416;
		case 417: goto st417;
		case 418: goto st418;
		case 1414: goto st1414;
		case 1415: goto st1415;
		case 419: goto st419;
		case 420: goto st420;
		case 421: goto st421;
		case 422: goto st422;
		case 423: goto st423;
		case 424: goto st424;
		case 425: goto st425;
		case 426: goto st426;
		case 427: goto st427;
		case 1416: goto st1416;
		case 1417: goto st1417;
		case 428: goto st428;
		case 429: goto st429;
		case 430: goto st430;
		case 431: goto st431;
		case 432: goto st432;
		case 433: goto st433;
		case 434: goto st434;
		case 435: goto st435;
		case 436: goto st436;
		case 437: goto st437;
		case 438: goto st438;
		case 439: goto st439;
		case 440: goto st440;
		case 441: goto st441;
		case 442: goto st442;
		case 443: goto st443;
		case 444: goto st444;
		case 445: goto st445;
		case 446: goto st446;
		case 447: goto st447;
		case 448: goto st448;
		case 449: goto st449;
		case 450: goto st450;
		case 451: goto st451;
		case 452: goto st452;
		case 453: goto st453;
		case 454: goto st454;
		case 455: goto st455;
		case 456: goto st456;
		case 457: goto st457;
		case 458: goto st458;
		case 459: goto st459;
		case 460: goto st460;
		case 461: goto st461;
		case 462: goto st462;
		case 463: goto st463;
		case 464: goto st464;
		case 465: goto st465;
		case 466: goto st466;
		case 467: goto st467;
		case 468: goto st468;
		case 469: goto st469;
		case 470: goto st470;
		case 1418: goto st1418;
		case 1419: goto st1419;
		case 471: goto st471;
		case 1420: goto st1420;
		case 1421: goto st1421;
		case 472: goto st472;
		case 473: goto st473;
		case 474: goto st474;
		case 475: goto st475;
		case 476: goto st476;
		case 477: goto st477;
		case 478: goto st478;
		case 479: goto st479;
		case 1422: goto st1422;
		case 1423: goto st1423;
		case 480: goto st480;
		case 1424: goto st1424;
		case 481: goto st481;
		case 482: goto st482;
		case 483: goto st483;
		case 484: goto st484;
		case 485: goto st485;
		case 486: goto st486;
		case 487: goto st487;
		case 488: goto st488;
		case 489: goto st489;
		case 490: goto st490;
		case 491: goto st491;
		case 492: goto st492;
		case 493: goto st493;
		case 494: goto st494;
		case 495: goto st495;
		case 496: goto st496;
		case 497: goto st497;
		case 498: goto st498;
		case 1425: goto st1425;
		case 499: goto st499;
		case 500: goto st500;
		case 501: goto st501;
		case 502: goto st502;
		case 503: goto st503;
		case 504: goto st504;
		case 505: goto st505;
		case 506: goto st506;
		case 507: goto st507;
		case 0: goto st0;
		case 1426: goto st1426;
		case 1427: goto st1427;
		case 1428: goto st1428;
		case 1429: goto st1429;
		case 1430: goto st1430;
		case 1431: goto st1431;
		case 508: goto st508;
		case 509: goto st509;
		case 1432: goto st1432;
		case 1433: goto st1433;
		case 1434: goto st1434;
		case 1435: goto st1435;
		case 1436: goto st1436;
		case 1437: goto st1437;
		case 1438: goto st1438;
		case 1439: goto st1439;
		case 1440: goto st1440;
		case 1441: goto st1441;
		case 1442: goto st1442;
		case 1443: goto st1443;
		case 510: goto st510;
		case 511: goto st511;
		case 512: goto st512;
		case 513: goto st513;
		case 514: goto st514;
		case 515: goto st515;
		case 516: goto st516;
		case 517: goto st517;
		case 518: goto st518;
		case 519: goto st519;
		case 1444: goto st1444;
		case 1445: goto st1445;
		case 1446: goto st1446;
		case 1447: goto st1447;
		case 520: goto st520;
		case 521: goto st521;
		case 1448: goto st1448;
		case 1449: goto st1449;
		case 1450: goto st1450;
		case 1451: goto st1451;
		case 1452: goto st1452;
		case 1453: goto st1453;
		case 1454: goto st1454;
		case 1455: goto st1455;
		case 1456: goto st1456;
		case 1457: goto st1457;
		case 1458: goto st1458;
		case 1459: goto st1459;
		case 522: goto st522;
		case 523: goto st523;
		case 524: goto st524;
		case 525: goto st525;
		case 526: goto st526;
		case 527: goto st527;
		case 528: goto st528;
		case 529: goto st529;
		case 530: goto st530;
		case 531: goto st531;
		case 1460: goto st1460;
		case 1461: goto st1461;
		case 1462: goto st1462;
		case 1463: goto st1463;
		case 1464: goto st1464;
		case 1465: goto st1465;
		case 1466: goto st1466;
		case 532: goto st532;
		case 533: goto st533;
		case 1467: goto st1467;
		case 1468: goto st1468;
		case 1469: goto st1469;
		case 1470: goto st1470;
		case 1471: goto st1471;
		case 1472: goto st1472;
		case 1473: goto st1473;
		case 1474: goto st1474;
		case 1475: goto st1475;
		case 1476: goto st1476;
		case 1477: goto st1477;
		case 1478: goto st1478;
		case 534: goto st534;
		case 535: goto st535;
		case 536: goto st536;
		case 537: goto st537;
		case 538: goto st538;
		case 539: goto st539;
		case 540: goto st540;
		case 541: goto st541;
		case 542: goto st542;
		case 543: goto st543;
		case 1479: goto st1479;
		case 1480: goto st1480;
		case 1481: goto st1481;
		case 1482: goto st1482;
		case 1483: goto st1483;
		case 544: goto st544;
		case 545: goto st545;
		case 1484: goto st1484;
		case 546: goto st546;
		case 1485: goto st1485;
		case 1486: goto st1486;
		case 1487: goto st1487;
		case 1488: goto st1488;
		case 1489: goto st1489;
		case 1490: goto st1490;
		case 1491: goto st1491;
		case 1492: goto st1492;
		case 1493: goto st1493;
		case 1494: goto st1494;
		case 1495: goto st1495;
		case 1496: goto st1496;
		case 547: goto st547;
		case 548: goto st548;
		case 549: goto st549;
		case 550: goto st550;
		case 551: goto st551;
		case 552: goto st552;
		case 553: goto st553;
		case 554: goto st554;
		case 555: goto st555;
		case 556: goto st556;
		case 1497: goto st1497;
		case 1498: goto st1498;
		case 1499: goto st1499;
		case 1500: goto st1500;
		case 1501: goto st1501;
		case 557: goto st557;
		case 558: goto st558;
		case 1502: goto st1502;
		case 1503: goto st1503;
		case 1504: goto st1504;
		case 1505: goto st1505;
		case 1506: goto st1506;
		case 1507: goto st1507;
		case 1508: goto st1508;
		case 1509: goto st1509;
		case 1510: goto st1510;
		case 1511: goto st1511;
		case 1512: goto st1512;
		case 1513: goto st1513;
		case 559: goto st559;
		case 560: goto st560;
		case 561: goto st561;
		case 562: goto st562;
		case 563: goto st563;
		case 564: goto st564;
		case 565: goto st565;
		case 566: goto st566;
		case 567: goto st567;
		case 568: goto st568;
		case 1514: goto st1514;
		case 1515: goto st1515;
		case 1516: goto st1516;
		case 1517: goto st1517;
		case 569: goto st569;
		case 570: goto st570;
		case 571: goto st571;
		case 572: goto st572;
		case 573: goto st573;
		case 574: goto st574;
		case 575: goto st575;
		case 576: goto st576;
		case 577: goto st577;
		case 1518: goto st1518;
		case 578: goto st578;
		case 579: goto st579;
		case 580: goto st580;
		case 581: goto st581;
		case 582: goto st582;
		case 583: goto st583;
		case 584: goto st584;
		case 585: goto st585;
		case 586: goto st586;
		case 587: goto st587;
		case 1519: goto st1519;
		case 588: goto st588;
		case 589: goto st589;
		case 590: goto st590;
		case 591: goto st591;
		case 592: goto st592;
		case 593: goto st593;
		case 594: goto st594;
		case 595: goto st595;
		case 596: goto st596;
		case 597: goto st597;
		case 598: goto st598;
		case 1520: goto st1520;
		case 599: goto st599;
		case 600: goto st600;
		case 601: goto st601;
		case 602: goto st602;
		case 603: goto st603;
		case 604: goto st604;
		case 605: goto st605;
		case 606: goto st606;
		case 607: goto st607;
		case 608: goto st608;
		case 609: goto st609;
		case 610: goto st610;
		case 611: goto st611;
		case 1521: goto st1521;
		case 612: goto st612;
		case 613: goto st613;
		case 614: goto st614;
		case 615: goto st615;
		case 616: goto st616;
		case 617: goto st617;
		case 618: goto st618;
		case 619: goto st619;
		case 620: goto st620;
		case 621: goto st621;
		case 1522: goto st1522;
		case 1523: goto st1523;
		case 1524: goto st1524;
		case 1525: goto st1525;
		case 1526: goto st1526;
		case 622: goto st622;
		case 623: goto st623;
		case 624: goto st624;
		case 625: goto st625;
		case 626: goto st626;
		case 627: goto st627;
		case 628: goto st628;
		case 629: goto st629;
		case 630: goto st630;
		case 1527: goto st1527;
		case 1528: goto st1528;
		case 1529: goto st1529;
		case 1530: goto st1530;
		case 1531: goto st1531;
		case 1532: goto st1532;
		case 1533: goto st1533;
		case 1534: goto st1534;
		case 1535: goto st1535;
		case 1536: goto st1536;
		case 1537: goto st1537;
		case 1538: goto st1538;
		case 631: goto st631;
		case 632: goto st632;
		case 633: goto st633;
		case 634: goto st634;
		case 635: goto st635;
		case 636: goto st636;
		case 637: goto st637;
		case 638: goto st638;
		case 639: goto st639;
		case 640: goto st640;
		case 1539: goto st1539;
		case 1540: goto st1540;
		case 1541: goto st1541;
		case 1542: goto st1542;
		case 1543: goto st1543;
		case 641: goto st641;
		case 642: goto st642;
		case 643: goto st643;
		case 644: goto st644;
		case 645: goto st645;
		case 1544: goto st1544;
		case 646: goto st646;
		case 647: goto st647;
		case 648: goto st648;
		case 649: goto st649;
		case 650: goto st650;
		case 651: goto st651;
		case 652: goto st652;
		case 653: goto st653;
		case 654: goto st654;
		case 655: goto st655;
		case 656: goto st656;
		case 657: goto st657;
		case 658: goto st658;
		case 659: goto st659;
		case 660: goto st660;
		case 661: goto st661;
		case 662: goto st662;
		case 663: goto st663;
		case 664: goto st664;
		case 665: goto st665;
		case 666: goto st666;
		case 1545: goto st1545;
		case 1546: goto st1546;
		case 1547: goto st1547;
		case 667: goto st667;
		case 668: goto st668;
		case 1548: goto st1548;
		case 1549: goto st1549;
		case 1550: goto st1550;
		case 1551: goto st1551;
		case 1552: goto st1552;
		case 1553: goto st1553;
		case 1554: goto st1554;
		case 1555: goto st1555;
		case 1556: goto st1556;
		case 1557: goto st1557;
		case 1558: goto st1558;
		case 1559: goto st1559;
		case 669: goto st669;
		case 670: goto st670;
		case 671: goto st671;
		case 672: goto st672;
		case 673: goto st673;
		case 674: goto st674;
		case 675: goto st675;
		case 676: goto st676;
		case 677: goto st677;
		case 678: goto st678;
		case 1560: goto st1560;
		case 1561: goto st1561;
		case 679: goto st679;
		case 680: goto st680;
		case 1562: goto st1562;
		case 1563: goto st1563;
		case 1564: goto st1564;
		case 1565: goto st1565;
		case 1566: goto st1566;
		case 1567: goto st1567;
		case 1568: goto st1568;
		case 1569: goto st1569;
		case 1570: goto st1570;
		case 1571: goto st1571;
		case 1572: goto st1572;
		case 1573: goto st1573;
		case 681: goto st681;
		case 682: goto st682;
		case 683: goto st683;
		case 684: goto st684;
		case 685: goto st685;
		case 686: goto st686;
		case 687: goto st687;
		case 688: goto st688;
		case 689: goto st689;
		case 690: goto st690;
		case 1574: goto st1574;
		case 1575: goto st1575;
		case 1576: goto st1576;
		case 1577: goto st1577;
		case 1578: goto st1578;
		case 1579: goto st1579;
		case 691: goto st691;
		case 692: goto st692;
		case 1580: goto st1580;
		case 1581: goto st1581;
		case 1582: goto st1582;
		case 1583: goto st1583;
		case 1584: goto st1584;
		case 1585: goto st1585;
		case 1586: goto st1586;
		case 1587: goto st1587;
		case 1588: goto st1588;
		case 1589: goto st1589;
		case 1590: goto st1590;
		case 1591: goto st1591;
		case 693: goto st693;
		case 694: goto st694;
		case 695: goto st695;
		case 696: goto st696;
		case 697: goto st697;
		case 698: goto st698;
		case 699: goto st699;
		case 700: goto st700;
		case 701: goto st701;
		case 702: goto st702;
		case 1592: goto st1592;
		case 1593: goto st1593;
		case 1594: goto st1594;
		case 1595: goto st1595;
		case 1596: goto st1596;
		case 1597: goto st1597;
		case 703: goto st703;
		case 704: goto st704;
		case 1598: goto st1598;
		case 1599: goto st1599;
		case 1600: goto st1600;
		case 1601: goto st1601;
		case 1602: goto st1602;
		case 1603: goto st1603;
		case 1604: goto st1604;
		case 1605: goto st1605;
		case 1606: goto st1606;
		case 1607: goto st1607;
		case 1608: goto st1608;
		case 1609: goto st1609;
		case 705: goto st705;
		case 706: goto st706;
		case 707: goto st707;
		case 708: goto st708;
		case 709: goto st709;
		case 710: goto st710;
		case 711: goto st711;
		case 712: goto st712;
		case 713: goto st713;
		case 714: goto st714;
		case 1610: goto st1610;
		case 1611: goto st1611;
		case 1612: goto st1612;
		case 715: goto st715;
		case 716: goto st716;
		case 717: goto st717;
		case 718: goto st718;
		case 719: goto st719;
		case 720: goto st720;
		case 721: goto st721;
		case 722: goto st722;
		case 1613: goto st1613;
		case 1614: goto st1614;
		case 1615: goto st1615;
		case 1616: goto st1616;
		case 1617: goto st1617;
		case 1618: goto st1618;
		case 1619: goto st1619;
		case 1620: goto st1620;
		case 1621: goto st1621;
		case 1622: goto st1622;
		case 1623: goto st1623;
		case 1624: goto st1624;
		case 723: goto st723;
		case 724: goto st724;
		case 725: goto st725;
		case 726: goto st726;
		case 727: goto st727;
		case 728: goto st728;
		case 729: goto st729;
		case 730: goto st730;
		case 731: goto st731;
		case 732: goto st732;
		case 733: goto st733;
		case 734: goto st734;
		case 735: goto st735;
		case 736: goto st736;
		case 737: goto st737;
		case 738: goto st738;
		case 739: goto st739;
		case 740: goto st740;
		case 741: goto st741;
		case 742: goto st742;
		case 743: goto st743;
		case 744: goto st744;
		case 745: goto st745;
		case 1625: goto st1625;
		case 1626: goto st1626;
		case 1627: goto st1627;
		case 1628: goto st1628;
		case 1629: goto st1629;
		case 1630: goto st1630;
		case 1631: goto st1631;
		case 1632: goto st1632;
		case 1633: goto st1633;
		case 1634: goto st1634;
		case 1635: goto st1635;
		case 1636: goto st1636;
		case 746: goto st746;
		case 747: goto st747;
		case 748: goto st748;
		case 749: goto st749;
		case 750: goto st750;
		case 751: goto st751;
		case 752: goto st752;
		case 753: goto st753;
		case 754: goto st754;
		case 755: goto st755;
		case 756: goto st756;
		case 757: goto st757;
		case 758: goto st758;
		case 759: goto st759;
		case 760: goto st760;
		case 761: goto st761;
		case 762: goto st762;
		case 763: goto st763;
		case 764: goto st764;
		case 765: goto st765;
		case 766: goto st766;
		case 767: goto st767;
		case 768: goto st768;
		case 1637: goto st1637;
		case 1638: goto st1638;
		case 1639: goto st1639;
		case 1640: goto st1640;
		case 1641: goto st1641;
		case 1642: goto st1642;
		case 1643: goto st1643;
		case 1644: goto st1644;
		case 1645: goto st1645;
		case 1646: goto st1646;
		case 1647: goto st1647;
		case 1648: goto st1648;
		case 769: goto st769;
		case 770: goto st770;
		case 771: goto st771;
		case 772: goto st772;
		case 773: goto st773;
		case 774: goto st774;
		case 775: goto st775;
		case 776: goto st776;
		case 777: goto st777;
		case 778: goto st778;
		case 1649: goto st1649;
		case 1650: goto st1650;
		case 1651: goto st1651;
		case 1652: goto st1652;
		case 779: goto st779;
		case 780: goto st780;
		case 1653: goto st1653;
		case 781: goto st781;
		case 782: goto st782;
		case 1654: goto st1654;
		case 1655: goto st1655;
		case 1656: goto st1656;
		case 1657: goto st1657;
		case 1658: goto st1658;
		case 1659: goto st1659;
		case 1660: goto st1660;
		case 1661: goto st1661;
		case 1662: goto st1662;
		case 1663: goto st1663;
		case 1664: goto st1664;
		case 1665: goto st1665;
		case 783: goto st783;
		case 784: goto st784;
		case 785: goto st785;
		case 786: goto st786;
		case 787: goto st787;
		case 788: goto st788;
		case 789: goto st789;
		case 790: goto st790;
		case 791: goto st791;
		case 792: goto st792;
		case 1666: goto st1666;
		case 1667: goto st1667;
		case 1668: goto st1668;
		case 1669: goto st1669;
		case 793: goto st793;
		case 794: goto st794;
		case 1670: goto st1670;
		case 1671: goto st1671;
		case 1672: goto st1672;
		case 1673: goto st1673;
		case 1674: goto st1674;
		case 1675: goto st1675;
		case 1676: goto st1676;
		case 1677: goto st1677;
		case 1678: goto st1678;
		case 1679: goto st1679;
		case 1680: goto st1680;
		case 1681: goto st1681;
		case 795: goto st795;
		case 796: goto st796;
		case 797: goto st797;
		case 798: goto st798;
		case 799: goto st799;
		case 800: goto st800;
		case 801: goto st801;
		case 802: goto st802;
		case 803: goto st803;
		case 804: goto st804;
		case 805: goto st805;
		case 806: goto st806;
		case 807: goto st807;
		case 808: goto st808;
		case 809: goto st809;
		case 810: goto st810;
		case 811: goto st811;
		case 812: goto st812;
		case 1682: goto st1682;
		case 1683: goto st1683;
		case 1684: goto st1684;
		case 1685: goto st1685;
		case 1686: goto st1686;
		case 1687: goto st1687;
		case 1688: goto st1688;
		case 1689: goto st1689;
		case 1690: goto st1690;
		case 1691: goto st1691;
		case 1692: goto st1692;
		case 1693: goto st1693;
		case 813: goto st813;
		case 814: goto st814;
		case 815: goto st815;
		case 816: goto st816;
		case 817: goto st817;
		case 818: goto st818;
		case 819: goto st819;
		case 820: goto st820;
		case 821: goto st821;
		case 822: goto st822;
		case 1694: goto st1694;
		case 1695: goto st1695;
		case 1696: goto st1696;
		case 1697: goto st1697;
		case 823: goto st823;
		case 824: goto st824;
		case 1698: goto st1698;
		case 1699: goto st1699;
		case 1700: goto st1700;
		case 1701: goto st1701;
		case 1702: goto st1702;
		case 1703: goto st1703;
		case 1704: goto st1704;
		case 1705: goto st1705;
		case 1706: goto st1706;
		case 1707: goto st1707;
		case 1708: goto st1708;
		case 1709: goto st1709;
		case 825: goto st825;
		case 826: goto st826;
		case 827: goto st827;
		case 828: goto st828;
		case 829: goto st829;
		case 830: goto st830;
		case 831: goto st831;
		case 832: goto st832;
		case 833: goto st833;
		case 834: goto st834;
		case 1710: goto st1710;
		case 835: goto st835;
		case 836: goto st836;
		case 837: goto st837;
		case 838: goto st838;
		case 839: goto st839;
		case 840: goto st840;
		case 841: goto st841;
		case 842: goto st842;
		case 843: goto st843;
		case 844: goto st844;
		case 845: goto st845;
		case 846: goto st846;
		case 847: goto st847;
		case 848: goto st848;
		case 849: goto st849;
		case 850: goto st850;
		case 851: goto st851;
		case 852: goto st852;
		case 853: goto st853;
		case 1711: goto st1711;
		case 854: goto st854;
		case 1712: goto st1712;
		case 855: goto st855;
		case 856: goto st856;
		case 857: goto st857;
		case 858: goto st858;
		case 859: goto st859;
		case 860: goto st860;
		case 861: goto st861;
		case 862: goto st862;
		case 863: goto st863;
		case 864: goto st864;
		case 865: goto st865;
		case 866: goto st866;
		case 867: goto st867;
		case 868: goto st868;
		case 869: goto st869;
		case 870: goto st870;
		case 871: goto st871;
		case 872: goto st872;
		case 873: goto st873;
		case 874: goto st874;
		case 875: goto st875;
		case 876: goto st876;
		case 877: goto st877;
		case 878: goto st878;
		case 879: goto st879;
		case 880: goto st880;
		case 881: goto st881;
		case 882: goto st882;
		case 883: goto st883;
		case 884: goto st884;
		case 885: goto st885;
		case 886: goto st886;
		case 887: goto st887;
		case 888: goto st888;
		case 889: goto st889;
		case 890: goto st890;
		case 1713: goto st1713;
		case 891: goto st891;
		case 892: goto st892;
		case 893: goto st893;
		case 894: goto st894;
		case 895: goto st895;
		case 896: goto st896;
		case 897: goto st897;
		case 898: goto st898;
		case 899: goto st899;
		case 900: goto st900;
		case 901: goto st901;
		case 902: goto st902;
		case 903: goto st903;
		case 904: goto st904;
		case 905: goto st905;
		case 906: goto st906;
		case 907: goto st907;
		case 908: goto st908;
		case 909: goto st909;
		case 910: goto st910;
		case 911: goto st911;
		case 912: goto st912;
		case 913: goto st913;
		case 914: goto st914;
		case 915: goto st915;
		case 916: goto st916;
		case 917: goto st917;
		case 918: goto st918;
		case 919: goto st919;
		case 920: goto st920;
		case 921: goto st921;
		case 922: goto st922;
		case 923: goto st923;
		case 924: goto st924;
		case 925: goto st925;
		case 926: goto st926;
		case 927: goto st927;
		case 928: goto st928;
		case 929: goto st929;
		case 930: goto st930;
		case 931: goto st931;
		case 932: goto st932;
		case 933: goto st933;
		case 934: goto st934;
		case 935: goto st935;
		case 936: goto st936;
		case 937: goto st937;
		case 938: goto st938;
		case 939: goto st939;
		case 940: goto st940;
		case 941: goto st941;
		case 942: goto st942;
		case 943: goto st943;
		case 944: goto st944;
		case 945: goto st945;
		case 946: goto st946;
		case 947: goto st947;
		case 948: goto st948;
		case 949: goto st949;
		case 950: goto st950;
		case 951: goto st951;
		case 952: goto st952;
		case 953: goto st953;
		case 954: goto st954;
		case 955: goto st955;
		case 956: goto st956;
		case 957: goto st957;
		case 958: goto st958;
		case 959: goto st959;
		case 960: goto st960;
		case 961: goto st961;
		case 962: goto st962;
		case 963: goto st963;
		case 964: goto st964;
		case 965: goto st965;
		case 966: goto st966;
		case 967: goto st967;
		case 968: goto st968;
		case 969: goto st969;
		case 970: goto st970;
		case 971: goto st971;
		case 1714: goto st1714;
		case 1715: goto st1715;
		case 972: goto st972;
		case 973: goto st973;
		case 974: goto st974;
		case 975: goto st975;
		case 976: goto st976;
		case 977: goto st977;
		case 978: goto st978;
		case 979: goto st979;
		case 980: goto st980;
		case 981: goto st981;
		case 982: goto st982;
		case 983: goto st983;
		case 984: goto st984;
		case 985: goto st985;
		case 986: goto st986;
		case 987: goto st987;
		case 988: goto st988;
		case 989: goto st989;
		case 990: goto st990;
		case 991: goto st991;
		case 992: goto st992;
		case 993: goto st993;
		case 994: goto st994;
		case 995: goto st995;
		case 996: goto st996;
		case 997: goto st997;
		case 998: goto st998;
		case 999: goto st999;
		case 1000: goto st1000;
		case 1001: goto st1001;
		case 1002: goto st1002;
		case 1003: goto st1003;
		case 1004: goto st1004;
		case 1005: goto st1005;
		case 1006: goto st1006;
		case 1007: goto st1007;
		case 1008: goto st1008;
		case 1009: goto st1009;
		case 1010: goto st1010;
		case 1011: goto st1011;
		case 1012: goto st1012;
		case 1013: goto st1013;
		case 1014: goto st1014;
		case 1015: goto st1015;
		case 1016: goto st1016;
		case 1017: goto st1017;
		case 1018: goto st1018;
		case 1019: goto st1019;
		case 1020: goto st1020;
		case 1021: goto st1021;
		case 1022: goto st1022;
		case 1023: goto st1023;
		case 1024: goto st1024;
		case 1025: goto st1025;
		case 1026: goto st1026;
		case 1027: goto st1027;
		case 1028: goto st1028;
		case 1029: goto st1029;
		case 1030: goto st1030;
		case 1031: goto st1031;
		case 1032: goto st1032;
		case 1033: goto st1033;
		case 1034: goto st1034;
		case 1035: goto st1035;
		case 1036: goto st1036;
		case 1037: goto st1037;
		case 1038: goto st1038;
		case 1039: goto st1039;
		case 1040: goto st1040;
		case 1041: goto st1041;
		case 1042: goto st1042;
		case 1043: goto st1043;
		case 1044: goto st1044;
		case 1045: goto st1045;
		case 1046: goto st1046;
		case 1047: goto st1047;
		case 1048: goto st1048;
		case 1049: goto st1049;
		case 1050: goto st1050;
		case 1051: goto st1051;
		case 1052: goto st1052;
		case 1053: goto st1053;
		case 1054: goto st1054;
		case 1055: goto st1055;
		case 1056: goto st1056;
		case 1057: goto st1057;
		case 1058: goto st1058;
		case 1059: goto st1059;
		case 1060: goto st1060;
		case 1061: goto st1061;
		case 1062: goto st1062;
		case 1063: goto st1063;
		case 1064: goto st1064;
		case 1065: goto st1065;
		case 1066: goto st1066;
		case 1067: goto st1067;
		case 1068: goto st1068;
		case 1069: goto st1069;
		case 1070: goto st1070;
		case 1071: goto st1071;
		case 1072: goto st1072;
		case 1073: goto st1073;
		case 1074: goto st1074;
		case 1075: goto st1075;
		case 1076: goto st1076;
		case 1716: goto st1716;
		case 1077: goto st1077;
		case 1078: goto st1078;
		case 1717: goto st1717;
		case 1079: goto st1079;
		case 1080: goto st1080;
		case 1081: goto st1081;
		case 1082: goto st1082;
		case 1718: goto st1718;
		case 1083: goto st1083;
		case 1084: goto st1084;
		case 1085: goto st1085;
		case 1086: goto st1086;
		case 1087: goto st1087;
		case 1088: goto st1088;
		case 1089: goto st1089;
		case 1090: goto st1090;
		case 1091: goto st1091;
		case 1092: goto st1092;
		case 1719: goto st1719;
		case 1093: goto st1093;
		case 1094: goto st1094;
		case 1095: goto st1095;
		case 1096: goto st1096;
		case 1097: goto st1097;
		case 1098: goto st1098;
		case 1099: goto st1099;
		case 1100: goto st1100;
		case 1101: goto st1101;
		case 1102: goto st1102;
		case 1103: goto st1103;
		case 1104: goto st1104;
		case 1105: goto st1105;
		case 1106: goto st1106;
		case 1107: goto st1107;
		case 1108: goto st1108;
		case 1109: goto st1109;
		case 1110: goto st1110;
		case 1111: goto st1111;
		case 1112: goto st1112;
		case 1720: goto st1720;
		case 1721: goto st1721;
		case 1113: goto st1113;
		case 1114: goto st1114;
		case 1115: goto st1115;
		case 1116: goto st1116;
		case 1117: goto st1117;
		case 1118: goto st1118;
		case 1119: goto st1119;
		case 1120: goto st1120;
		case 1121: goto st1121;
		case 1122: goto st1122;
		case 1123: goto st1123;
		case 1124: goto st1124;
		case 1722: goto st1722;
		case 1723: goto st1723;
		case 1724: goto st1724;
		case 1725: goto st1725;
		case 1125: goto st1125;
		case 1126: goto st1126;
		case 1127: goto st1127;
		case 1128: goto st1128;
		case 1129: goto st1129;
		case 1130: goto st1130;
		case 1131: goto st1131;
		case 1132: goto st1132;
		case 1133: goto st1133;
		case 1134: goto st1134;
		case 1135: goto st1135;
		case 1136: goto st1136;
		case 1137: goto st1137;
		case 1138: goto st1138;
		case 1139: goto st1139;
		case 1140: goto st1140;
		case 1141: goto st1141;
		case 1142: goto st1142;
		case 1726: goto st1726;
		case 1727: goto st1727;
		case 1728: goto st1728;
		case 1729: goto st1729;
		case 1143: goto st1143;
		case 1144: goto st1144;
		case 1145: goto st1145;
		case 1146: goto st1146;
		case 1147: goto st1147;
		case 1148: goto st1148;
		case 1149: goto st1149;
		case 1150: goto st1150;
		case 1151: goto st1151;
		case 1152: goto st1152;
		case 1153: goto st1153;
		case 1154: goto st1154;
		case 1155: goto st1155;
		case 1156: goto st1156;
		case 1157: goto st1157;
		case 1158: goto st1158;
		case 1159: goto st1159;
		case 1160: goto st1160;
		case 1161: goto st1161;
		case 1162: goto st1162;
		case 1163: goto st1163;
		case 1164: goto st1164;
		case 1165: goto st1165;
		case 1166: goto st1166;
		case 1167: goto st1167;
		case 1168: goto st1168;
		case 1169: goto st1169;
		case 1170: goto st1170;
		case 1171: goto st1171;
		case 1172: goto st1172;
		case 1173: goto st1173;
		case 1174: goto st1174;
		case 1175: goto st1175;
		case 1176: goto st1176;
		case 1177: goto st1177;
		case 1178: goto st1178;
		case 1179: goto st1179;
		case 1180: goto st1180;
		case 1181: goto st1181;
		case 1182: goto st1182;
		case 1183: goto st1183;
		case 1184: goto st1184;
		case 1185: goto st1185;
		case 1186: goto st1186;
		case 1187: goto st1187;
		case 1188: goto st1188;
		case 1189: goto st1189;
		case 1190: goto st1190;
		case 1191: goto st1191;
		case 1192: goto st1192;
		case 1193: goto st1193;
		case 1194: goto st1194;
		case 1195: goto st1195;
		case 1196: goto st1196;
		case 1197: goto st1197;
		case 1198: goto st1198;
		case 1199: goto st1199;
		case 1200: goto st1200;
		case 1201: goto st1201;
		case 1202: goto st1202;
		case 1203: goto st1203;
		case 1204: goto st1204;
		case 1205: goto st1205;
		case 1206: goto st1206;
		case 1207: goto st1207;
		case 1208: goto st1208;
		case 1209: goto st1209;
		case 1210: goto st1210;
		case 1211: goto st1211;
		case 1212: goto st1212;
		case 1213: goto st1213;
		case 1214: goto st1214;
		case 1215: goto st1215;
		case 1216: goto st1216;
		case 1217: goto st1217;
		case 1218: goto st1218;
		case 1219: goto st1219;
		case 1220: goto st1220;
		case 1221: goto st1221;
		case 1222: goto st1222;
		case 1223: goto st1223;
		case 1224: goto st1224;
		case 1225: goto st1225;
		case 1226: goto st1226;
		case 1227: goto st1227;
		case 1228: goto st1228;
		case 1229: goto st1229;
		case 1230: goto st1230;
		case 1231: goto st1231;
		case 1232: goto st1232;
		case 1233: goto st1233;
		case 1234: goto st1234;
		case 1235: goto st1235;
		case 1236: goto st1236;
		case 1237: goto st1237;
		case 1238: goto st1238;
		case 1239: goto st1239;
		case 1240: goto st1240;
		case 1241: goto st1241;
		case 1242: goto st1242;
		case 1243: goto st1243;
		case 1244: goto st1244;
		case 1245: goto st1245;
		case 1246: goto st1246;
		case 1247: goto st1247;
		case 1248: goto st1248;
		case 1249: goto st1249;
		case 1250: goto st1250;
		case 1251: goto st1251;
		case 1252: goto st1252;
		case 1253: goto st1253;
		case 1254: goto st1254;
		case 1730: goto st1730;
		case 1255: goto st1255;
		case 1256: goto st1256;
		case 1257: goto st1257;
		case 1258: goto st1258;
		case 1259: goto st1259;
		case 1260: goto st1260;
		case 1261: goto st1261;
		case 1262: goto st1262;
		case 1263: goto st1263;
		case 1264: goto st1264;
		case 1265: goto st1265;
		case 1266: goto st1266;
		case 1267: goto st1267;
		case 1268: goto st1268;
		case 1269: goto st1269;
		case 1270: goto st1270;
		case 1271: goto st1271;
		case 1272: goto st1272;
		case 1273: goto st1273;
		case 1274: goto st1274;
		case 1275: goto st1275;
		case 1276: goto st1276;
		case 1277: goto st1277;
		case 1278: goto st1278;
		case 1279: goto st1279;
		case 1280: goto st1280;
		case 1281: goto st1281;
		case 1282: goto st1282;
		case 1283: goto st1283;
		case 1284: goto st1284;
		case 1285: goto st1285;
		case 1286: goto st1286;
		case 1287: goto st1287;
		case 1288: goto st1288;
		case 1289: goto st1289;
		case 1290: goto st1290;
		case 1291: goto st1291;
		case 1292: goto st1292;
		case 1293: goto st1293;
		case 1294: goto st1294;
		case 1295: goto st1295;
		case 1296: goto st1296;
		case 1297: goto st1297;
		case 1298: goto st1298;
		case 1299: goto st1299;
		case 1300: goto st1300;
		case 1301: goto st1301;
		case 1302: goto st1302;
		case 1303: goto st1303;
		case 1304: goto st1304;
		case 1305: goto st1305;
		case 1306: goto st1306;
		case 1307: goto st1307;
		case 1308: goto st1308;
		case 1309: goto st1309;
		case 1310: goto st1310;
		case 1311: goto st1311;
		case 1312: goto st1312;
		case 1313: goto st1313;
		case 1314: goto st1314;
		case 1315: goto st1315;
		case 1316: goto st1316;
		case 1317: goto st1317;
		case 1318: goto st1318;
		case 1319: goto st1319;
		case 1320: goto st1320;
		case 1321: goto st1321;
		case 1322: goto st1322;
		case 1323: goto st1323;
		case 1324: goto st1324;
		case 1325: goto st1325;
		case 1326: goto st1326;
		case 1327: goto st1327;
		case 1328: goto st1328;
		case 1329: goto st1329;
		case 1330: goto st1330;
		case 1331: goto st1331;
		case 1332: goto st1332;
		case 1333: goto st1333;
		case 1334: goto st1334;
		case 1335: goto st1335;
		case 1336: goto st1336;
		case 1337: goto st1337;
		case 1338: goto st1338;
		case 1339: goto st1339;
		case 1340: goto st1340;
		case 1341: goto st1341;
		case 1342: goto st1342;
		case 1343: goto st1343;
		case 1344: goto st1344;
		case 1345: goto st1345;
		case 1346: goto st1346;
		case 1347: goto st1347;
		case 1348: goto st1348;
		case 1349: goto st1349;
		case 1350: goto st1350;
		case 1351: goto st1351;
		case 1352: goto st1352;
		case 1353: goto st1353;
		case 1354: goto st1354;
		case 1355: goto st1355;
		case 1356: goto st1356;
		case 1357: goto st1357;
		case 1358: goto st1358;
		case 1359: goto st1359;
		case 1360: goto st1360;
		case 1361: goto st1361;
		case 1362: goto st1362;
		case 1363: goto st1363;
		case 1364: goto st1364;
		case 1365: goto st1365;
		case 1366: goto st1366;
	default: break;
	}

	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof;
_resume:
	switch (  sm->cs )
	{
tr0:
#line 769 "ext/dtext/dtext.cpp.rl"
	{{( sm->p) = ((( sm->te)))-1;}{
    g_debug("block blank line(s)");
  }}
	goto st1367;
tr3:
#line 773 "ext/dtext/dtext.cpp.rl"
	{{( sm->p) = ((( sm->te)))-1;}{
    g_debug("block char");
    ( sm->p)--;

    if (sm->dstack.empty() || dstack_check(sm, BLOCK_QUOTE) || dstack_check(sm, BLOCK_SPOILER) || dstack_check(sm, BLOCK_EXPAND)) {
      dstack_open_element(sm, BLOCK_P, "<p>");
    }

    {
  size_t len = sm->stack.size();

  if (len > MAX_STACK_DEPTH) {
    // Should never happen.
    throw DTextError("too many nested elements");
  }

  if (sm->top >= len) {
    g_debug("growing sm->stack %zi", len + 16);
    sm->stack.resize(len + 16, 0);
  }
{( (sm->stack.data()))[( sm->top)++] = 1367;goto st1389;}}
  }}
	goto st1367;
tr15:
#line 751 "ext/dtext/dtext.cpp.rl"
	{( sm->te) = ( sm->p)+1;{
    g_debug("block [center]");
    dstack_open_element(sm, BLOCK_CENTER, "<p class=\"center\">");
    {
  size_t len = sm->stack.size();

  if (len > MAX_STACK_DEPTH) {
    // Should never happen.
    throw DTextError("too many nested elements");
  }

  if (sm->top >= len) {
    g_debug("growing sm->stack %zi", len + 16);
    sm->stack.resize(len + 16, 0);
  }
{( (sm->stack.data()))[( sm->top)++] = 1367;goto st1389;}}
  }}
	goto st1367;
tr23:
#line 740 "ext/dtext/dtext.cpp.rl"
	{( sm->te) = ( sm->p)+1;{
    dstack_close_leaf_blocks(sm);
    dstack_open_element(sm, BLOCK_TABLE, "<table class=\"highlightable\">");
    {
  size_t len = sm->stack.size();

  if (len > MAX_STACK_DEPTH) {
    // Should never happen.
    throw DTextError("too many nested elements");
  }

  if (sm->top >= len) {
    g_debug("growing sm->stack %zi", len + 16);
    sm->stack.resize(len + 16, 0);
  }
{( (sm->stack.data()))[( sm->top)++] = 1367;goto st1728;}}
  }}
	goto st1367;
tr61:
#line 696 "ext/dtext/dtext.cpp.rl"
	{{( sm->p) = ((( sm->te)))-1;}{
    append_block_code(sm, { sm->a1, sm->a2 });
    {
  size_t len = sm->stack.size();

  if (len > MAX_STACK_DEPTH) {
    // Should never happen.
    throw DTextError("too many nested elements");
  }

  if (sm->top >= len) {
    g_debug("growing sm->stack %zi", len + 16);
    sm->stack.resize(len + 16, 0);
  }
{( (sm->stack.data()))[( sm->top)++] = 1367;goto st1720;}}
  }}
	goto st1367;
tr62:
#line 696 "ext/dtext/dtext.cpp.rl"
	{( sm->te) = ( sm->p)+1;{
    append_block_code(sm, { sm->a1, sm->a2 });
    {
  size_t len = sm->stack.size();

  if (len > MAX_STACK_DEPTH) {
    // Should never happen.
    throw DTextError("too many nested elements");
  }

  if (sm->top >= len) {
    g_debug("growing sm->stack %zi", len + 16);
    sm->stack.resize(len + 16, 0);
  }
{( (sm->stack.data()))[( sm->top)++] = 1367;goto st1720;}}
  }}
	goto st1367;
tr64:
#line 691 "ext/dtext/dtext.cpp.rl"
	{{( sm->p) = ((( sm->te)))-1;}{
    append_block_code(sm);
    {
  size_t len = sm->stack.size();

  if (len > MAX_STACK_DEPTH) {
    // Should never happen.
    throw DTextError("too many nested elements");
  }

  if (sm->top >= len) {
    g_debug("growing sm->stack %zi", len + 16);
    sm->stack.resize(len + 16, 0);
  }
{( (sm->stack.data()))[( sm->top)++] = 1367;goto st1720;}}
  }}
	goto st1367;
tr65:
#line 691 "ext/dtext/dtext.cpp.rl"
	{( sm->te) = ( sm->p)+1;{
    append_block_code(sm);
    {
  size_t len = sm->stack.size();

  if (len > MAX_STACK_DEPTH) {
    // Should never happen.
    throw DTextError("too many nested elements");
  }

  if (sm->top >= len) {
    g_debug("growing sm->stack %zi", len + 16);
    sm->stack.resize(len + 16, 0);
  }
{( (sm->stack.data()))[( sm->top)++] = 1367;goto st1720;}}
  }}
	goto st1367;
tr71:
#line 720 "ext/dtext/dtext.cpp.rl"
	{( sm->te) = ( sm->p)+1;{
    g_debug("block [color]");
    dstack_open_element(sm, BLOCK_COLOR, "<p style=\"color:#FF761C;\">");
    {
  size_t len = sm->stack.size();

  if (len > MAX_STACK_DEPTH) {
    // Should never happen.
    throw DTextError("too many nested elements");
  }

  if (sm->top >= len) {
    g_debug("growing sm->stack %zi", len + 16);
    sm->stack.resize(len + 16, 0);
  }
{( (sm->stack.data()))[( sm->top)++] = 1367;goto st1389;}}
  }}
	goto st1367;
tr75:
#line 71 "ext/dtext/dtext.cpp.rl"
	{ sm->a1 = sm->p; }
#line 72 "ext/dtext/dtext.cpp.rl"
	{ sm->a2 = sm->p; }
#line 726 "ext/dtext/dtext.cpp.rl"
	{( sm->te) = ( sm->p)+1;{
    g_debug("block [color=]");
    dstack_open_element(sm, BLOCK_COLOR, "<p style=\"color:");
    append_block_html_escaped(sm, { sm->a1, sm->a2 });
    append_block(sm, "\">");
    {
  size_t len = sm->stack.size();

  if (len > MAX_STACK_DEPTH) {
    // Should never happen.
    throw DTextError("too many nested elements");
  }

  if (sm->top >= len) {
    g_debug("growing sm->stack %zi", len + 16);
    sm->stack.resize(len + 16, 0);
  }
{( (sm->stack.data()))[( sm->top)++] = 1367;goto st1389;}}
  }}
	goto st1367;
tr77:
#line 72 "ext/dtext/dtext.cpp.rl"
	{ sm->a2 = sm->p; }
#line 726 "ext/dtext/dtext.cpp.rl"
	{( sm->te) = ( sm->p)+1;{
    g_debug("block [color=]");
    dstack_open_element(sm, BLOCK_COLOR, "<p style=\"color:");
    append_block_html_escaped(sm, { sm->a1, sm->a2 });
    append_block(sm, "\">");
    {
  size_t len = sm->stack.size();

  if (len > MAX_STACK_DEPTH) {
    // Should never happen.
    throw DTextError("too many nested elements");
  }

  if (sm->top >= len) {
    g_debug("growing sm->stack %zi", len + 16);
    sm->stack.resize(len + 16, 0);
  }
{( (sm->stack.data()))[( sm->top)++] = 1367;goto st1389;}}
  }}
	goto st1367;
tr99:
#line 734 "ext/dtext/dtext.cpp.rl"
	{{( sm->p) = ((( sm->te)))-1;}{
    dstack_close_leaf_blocks(sm);
    dstack_open_element(sm, BLOCK_NODTEXT, "<p>");
    {
  size_t len = sm->stack.size();

  if (len > MAX_STACK_DEPTH) {
    // Should never happen.
    throw DTextError("too many nested elements");
  }

  if (sm->top >= len) {
    g_debug("growing sm->stack %zi", len + 16);
    sm->stack.resize(len + 16, 0);
  }
{( (sm->stack.data()))[( sm->top)++] = 1367;goto st1724;}}
  }}
	goto st1367;
tr100:
#line 734 "ext/dtext/dtext.cpp.rl"
	{( sm->te) = ( sm->p)+1;{
    dstack_close_leaf_blocks(sm);
    dstack_open_element(sm, BLOCK_NODTEXT, "<p>");
    {
  size_t len = sm->stack.size();

  if (len > MAX_STACK_DEPTH) {
    // Should never happen.
    throw DTextError("too many nested elements");
  }

  if (sm->top >= len) {
    g_debug("growing sm->stack %zi", len + 16);
    sm->stack.resize(len + 16, 0);
  }
{( (sm->stack.data()))[( sm->top)++] = 1367;goto st1724;}}
  }}
	goto st1367;
tr111:
#line 746 "ext/dtext/dtext.cpp.rl"
	{( sm->te) = ( sm->p)+1;{
    dstack_open_element(sm, BLOCK_TN, "<p class=\"tn\">");
    {
  size_t len = sm->stack.size();

  if (len > MAX_STACK_DEPTH) {
    // Should never happen.
    throw DTextError("too many nested elements");
  }

  if (sm->top >= len) {
    g_debug("growing sm->stack %zi", len + 16);
    sm->stack.resize(len + 16, 0);
  }
{( (sm->stack.data()))[( sm->top)++] = 1367;goto st1389;}}
  }}
	goto st1367;
tr173:
#line 701 "ext/dtext/dtext.cpp.rl"
	{( sm->te) = ( sm->p)+1;{
    append_code_fence(sm, { sm->b1, sm->b2 }, { sm->a1, sm->a2 });
  }}
	goto st1367;
tr1716:
#line 773 "ext/dtext/dtext.cpp.rl"
	{( sm->te) = ( sm->p)+1;{
    g_debug("block char");
    ( sm->p)--;

    if (sm->dstack.empty() || dstack_check(sm, BLOCK_QUOTE) || dstack_check(sm, BLOCK_SPOILER) || dstack_check(sm, BLOCK_EXPAND)) {
      dstack_open_element(sm, BLOCK_P, "<p>");
    }

    {
  size_t len = sm->stack.size();

  if (len > MAX_STACK_DEPTH) {
    // Should never happen.
    throw DTextError("too many nested elements");
  }

  if (sm->top >= len) {
    g_debug("growing sm->stack %zi", len + 16);
    sm->stack.resize(len + 16, 0);
  }
{( (sm->stack.data()))[( sm->top)++] = 1367;goto st1389;}}
  }}
	goto st1367;
tr1723:
#line 769 "ext/dtext/dtext.cpp.rl"
	{( sm->te) = ( sm->p);( sm->p)--;{
    g_debug("block blank line(s)");
  }}
	goto st1367;
tr1724:
#line 773 "ext/dtext/dtext.cpp.rl"
	{( sm->te) = ( sm->p);( sm->p)--;{
    g_debug("block char");
    ( sm->p)--;

    if (sm->dstack.empty() || dstack_check(sm, BLOCK_QUOTE) || dstack_check(sm, BLOCK_SPOILER) || dstack_check(sm, BLOCK_EXPAND)) {
      dstack_open_element(sm, BLOCK_P, "<p>");
    }

    {
  size_t len = sm->stack.size();

  if (len > MAX_STACK_DEPTH) {
    // Should never happen.
    throw DTextError("too many nested elements");
  }

  if (sm->top >= len) {
    g_debug("growing sm->stack %zi", len + 16);
    sm->stack.resize(len + 16, 0);
  }
{( (sm->stack.data()))[( sm->top)++] = 1367;goto st1389;}}
  }}
	goto st1367;
tr1725:
#line 757 "ext/dtext/dtext.cpp.rl"
	{( sm->te) = ( sm->p);( sm->p)--;{
    g_debug("write '<hr>' (pos: %ld)", sm->ts - sm->pb);
    append_block(sm, "<hr>");
  }}
	goto st1367;
tr1726:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ sm->b2 = sm->p; }
#line 762 "ext/dtext/dtext.cpp.rl"
	{( sm->te) = ( sm->p);( sm->p)--;{
    g_debug("block list");
    dstack_open_list(sm, sm->a2 - sm->a1);
    {( sm->p) = (( sm->b1))-1;}
    {
  size_t len = sm->stack.size();

  if (len > MAX_STACK_DEPTH) {
    // Should never happen.
    throw DTextError("too many nested elements");
  }

  if (sm->top >= len) {
    g_debug("growing sm->stack %zi", len + 16);
    sm->stack.resize(len + 16, 0);
  }
{( (sm->stack.data()))[( sm->top)++] = 1367;goto st1389;}}
  }}
	goto st1367;
tr1734:
#line 681 "ext/dtext/dtext.cpp.rl"
	{( sm->te) = ( sm->p);( sm->p)--;{
    dstack_close_leaf_blocks(sm);
    dstack_open_element(sm, BLOCK_QUOTE, "<blockquote>");
  }}
	goto st1367;
tr1735:
#line 696 "ext/dtext/dtext.cpp.rl"
	{( sm->te) = ( sm->p);( sm->p)--;{
    append_block_code(sm, { sm->a1, sm->a2 });
    {
  size_t len = sm->stack.size();

  if (len > MAX_STACK_DEPTH) {
    // Should never happen.
    throw DTextError("too many nested elements");
  }

  if (sm->top >= len) {
    g_debug("growing sm->stack %zi", len + 16);
    sm->stack.resize(len + 16, 0);
  }
{( (sm->stack.data()))[( sm->top)++] = 1367;goto st1720;}}
  }}
	goto st1367;
tr1736:
#line 691 "ext/dtext/dtext.cpp.rl"
	{( sm->te) = ( sm->p);( sm->p)--;{
    append_block_code(sm);
    {
  size_t len = sm->stack.size();

  if (len > MAX_STACK_DEPTH) {
    // Should never happen.
    throw DTextError("too many nested elements");
  }

  if (sm->top >= len) {
    g_debug("growing sm->stack %zi", len + 16);
    sm->stack.resize(len + 16, 0);
  }
{( (sm->stack.data()))[( sm->top)++] = 1367;goto st1720;}}
  }}
	goto st1367;
tr1737:
#line 711 "ext/dtext/dtext.cpp.rl"
	{( sm->te) = ( sm->p);( sm->p)--;{
    g_debug("block [expand=]");
    dstack_close_leaf_blocks(sm);
    dstack_open_element(sm, BLOCK_EXPAND, "<details>");
    append_block(sm, "<summary>");
    append_block_html_escaped(sm, { sm->a1, sm->a2 });
    append_block(sm, "</summary><div>");
  }}
	goto st1367;
tr1739:
#line 705 "ext/dtext/dtext.cpp.rl"
	{( sm->te) = ( sm->p);( sm->p)--;{
    dstack_close_leaf_blocks(sm);
    dstack_open_element(sm, BLOCK_EXPAND, "<details>");
    append_block(sm, "<summary>Show</summary><div>");
  }}
	goto st1367;
tr1740:
#line 734 "ext/dtext/dtext.cpp.rl"
	{( sm->te) = ( sm->p);( sm->p)--;{
    dstack_close_leaf_blocks(sm);
    dstack_open_element(sm, BLOCK_NODTEXT, "<p>");
    {
  size_t len = sm->stack.size();

  if (len > MAX_STACK_DEPTH) {
    // Should never happen.
    throw DTextError("too many nested elements");
  }

  if (sm->top >= len) {
    g_debug("growing sm->stack %zi", len + 16);
    sm->stack.resize(len + 16, 0);
  }
{( (sm->stack.data()))[( sm->top)++] = 1367;goto st1724;}}
  }}
	goto st1367;
tr1741:
#line 686 "ext/dtext/dtext.cpp.rl"
	{( sm->te) = ( sm->p);( sm->p)--;{
    dstack_close_leaf_blocks(sm);
    dstack_open_element(sm, BLOCK_SPOILER, "<div class=\"spoiler\">");
  }}
	goto st1367;
tr1743:
#line 676 "ext/dtext/dtext.cpp.rl"
	{( sm->te) = ( sm->p);( sm->p)--;{
    append_header(sm, *sm->a1, { sm->b1, sm->b2 });
    {
  size_t len = sm->stack.size();

  if (len > MAX_STACK_DEPTH) {
    // Should never happen.
    throw DTextError("too many nested elements");
  }

  if (sm->top >= len) {
    g_debug("growing sm->stack %zi", len + 16);
    sm->stack.resize(len + 16, 0);
  }
{( (sm->stack.data()))[( sm->top)++] = 1367;goto st1389;}}
  }}
	goto st1367;
st1367:
#line 1 "NONE"
	{( sm->ts) = 0;}
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1367;
case 1367:
#line 1 "NONE"
	{( sm->ts) = ( sm->p);}
#line 2998 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 0: goto tr1;
		case 9: goto tr1717;
		case 10: goto tr1;
		case 32: goto tr1717;
		case 42: goto tr1718;
		case 60: goto tr1719;
		case 72: goto tr1720;
		case 91: goto tr1721;
		case 96: goto tr1722;
		case 104: goto tr1720;
	}
	goto tr1716;
tr1:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
	goto st1368;
st1368:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1368;
case 1368:
#line 3020 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 0: goto tr1;
		case 9: goto st1;
		case 10: goto tr1;
		case 32: goto st1;
	}
	goto tr1723;
st1:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1;
case 1:
	switch( (*( sm->p)) ) {
		case 0: goto tr1;
		case 9: goto st1;
		case 10: goto tr1;
		case 32: goto st1;
	}
	goto tr0;
tr1717:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
	goto st1369;
st1369:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1369;
case 1369:
#line 3047 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 0: goto tr1;
		case 9: goto st2;
		case 10: goto tr1;
		case 32: goto st2;
		case 60: goto st3;
		case 91: goto st18;
	}
	goto tr1724;
st2:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof2;
case 2:
	switch( (*( sm->p)) ) {
		case 0: goto tr1;
		case 9: goto st2;
		case 10: goto tr1;
		case 32: goto st2;
		case 60: goto st3;
		case 91: goto st18;
	}
	goto tr3;
st3:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof3;
case 3:
	switch( (*( sm->p)) ) {
		case 67: goto st4;
		case 72: goto st10;
		case 84: goto st13;
		case 99: goto st4;
		case 104: goto st10;
		case 116: goto st13;
	}
	goto tr3;
st4:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof4;
case 4:
	switch( (*( sm->p)) ) {
		case 69: goto st5;
		case 101: goto st5;
	}
	goto tr3;
st5:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof5;
case 5:
	switch( (*( sm->p)) ) {
		case 78: goto st6;
		case 110: goto st6;
	}
	goto tr3;
st6:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof6;
case 6:
	switch( (*( sm->p)) ) {
		case 84: goto st7;
		case 116: goto st7;
	}
	goto tr3;
st7:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof7;
case 7:
	switch( (*( sm->p)) ) {
		case 69: goto st8;
		case 101: goto st8;
	}
	goto tr3;
st8:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof8;
case 8:
	switch( (*( sm->p)) ) {
		case 82: goto st9;
		case 114: goto st9;
	}
	goto tr3;
st9:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof9;
case 9:
	if ( (*( sm->p)) == 62 )
		goto tr15;
	goto tr3;
st10:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof10;
case 10:
	switch( (*( sm->p)) ) {
		case 82: goto st11;
		case 114: goto st11;
	}
	goto tr3;
st11:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof11;
case 11:
	if ( (*( sm->p)) == 62 )
		goto st12;
	goto tr3;
st12:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof12;
case 12:
	switch( (*( sm->p)) ) {
		case 0: goto st1370;
		case 9: goto st12;
		case 10: goto st1370;
		case 32: goto st12;
	}
	goto tr3;
st1370:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1370;
case 1370:
	switch( (*( sm->p)) ) {
		case 0: goto st1370;
		case 10: goto st1370;
	}
	goto tr1725;
st13:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof13;
case 13:
	switch( (*( sm->p)) ) {
		case 65: goto st14;
		case 97: goto st14;
	}
	goto tr3;
st14:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof14;
case 14:
	switch( (*( sm->p)) ) {
		case 66: goto st15;
		case 98: goto st15;
	}
	goto tr3;
st15:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof15;
case 15:
	switch( (*( sm->p)) ) {
		case 76: goto st16;
		case 108: goto st16;
	}
	goto tr3;
st16:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof16;
case 16:
	switch( (*( sm->p)) ) {
		case 69: goto st17;
		case 101: goto st17;
	}
	goto tr3;
st17:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof17;
case 17:
	if ( (*( sm->p)) == 62 )
		goto tr23;
	goto tr3;
st18:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof18;
case 18:
	switch( (*( sm->p)) ) {
		case 67: goto st19;
		case 72: goto st25;
		case 84: goto st27;
		case 99: goto st19;
		case 104: goto st25;
		case 116: goto st27;
	}
	goto tr3;
st19:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof19;
case 19:
	switch( (*( sm->p)) ) {
		case 69: goto st20;
		case 101: goto st20;
	}
	goto tr3;
st20:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof20;
case 20:
	switch( (*( sm->p)) ) {
		case 78: goto st21;
		case 110: goto st21;
	}
	goto tr3;
st21:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof21;
case 21:
	switch( (*( sm->p)) ) {
		case 84: goto st22;
		case 116: goto st22;
	}
	goto tr3;
st22:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof22;
case 22:
	switch( (*( sm->p)) ) {
		case 69: goto st23;
		case 101: goto st23;
	}
	goto tr3;
st23:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof23;
case 23:
	switch( (*( sm->p)) ) {
		case 82: goto st24;
		case 114: goto st24;
	}
	goto tr3;
st24:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof24;
case 24:
	if ( (*( sm->p)) == 93 )
		goto tr15;
	goto tr3;
st25:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof25;
case 25:
	switch( (*( sm->p)) ) {
		case 82: goto st26;
		case 114: goto st26;
	}
	goto tr3;
st26:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof26;
case 26:
	if ( (*( sm->p)) == 93 )
		goto st12;
	goto tr3;
st27:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof27;
case 27:
	switch( (*( sm->p)) ) {
		case 65: goto st28;
		case 97: goto st28;
	}
	goto tr3;
st28:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof28;
case 28:
	switch( (*( sm->p)) ) {
		case 66: goto st29;
		case 98: goto st29;
	}
	goto tr3;
st29:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof29;
case 29:
	switch( (*( sm->p)) ) {
		case 76: goto st30;
		case 108: goto st30;
	}
	goto tr3;
st30:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof30;
case 30:
	switch( (*( sm->p)) ) {
		case 69: goto st31;
		case 101: goto st31;
	}
	goto tr3;
st31:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof31;
case 31:
	if ( (*( sm->p)) == 93 )
		goto tr23;
	goto tr3;
tr1718:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 71 "ext/dtext/dtext.cpp.rl"
	{ sm->a1 = sm->p; }
	goto st1371;
st1371:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1371;
case 1371:
#line 3348 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 9: goto tr39;
		case 32: goto tr39;
		case 42: goto st33;
	}
	goto tr1724;
tr39:
#line 72 "ext/dtext/dtext.cpp.rl"
	{ sm->a2 = sm->p; }
	goto st32;
st32:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof32;
case 32:
#line 3363 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 0: goto tr3;
		case 9: goto tr38;
		case 10: goto tr3;
		case 13: goto tr3;
		case 32: goto tr38;
	}
	goto tr37;
tr37:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ sm->b1 = sm->p; }
	goto st1372;
st1372:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1372;
case 1372:
#line 3380 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 0: goto tr1726;
		case 10: goto tr1726;
		case 13: goto tr1726;
	}
	goto st1372;
tr38:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ sm->b1 = sm->p; }
	goto st1373;
st1373:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1373;
case 1373:
#line 3395 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 0: goto tr1726;
		case 9: goto tr38;
		case 10: goto tr1726;
		case 13: goto tr1726;
		case 32: goto tr38;
	}
	goto tr37;
st33:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof33;
case 33:
	switch( (*( sm->p)) ) {
		case 9: goto tr39;
		case 32: goto tr39;
		case 42: goto st33;
	}
	goto tr3;
tr1719:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
	goto st1374;
st1374:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1374;
case 1374:
#line 3422 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 66: goto st34;
		case 67: goto st44;
		case 69: goto st59;
		case 72: goto st10;
		case 78: goto st68;
		case 81: goto st39;
		case 83: goto st76;
		case 84: goto st84;
		case 98: goto st34;
		case 99: goto st44;
		case 101: goto st59;
		case 104: goto st10;
		case 110: goto st68;
		case 113: goto st39;
		case 115: goto st76;
		case 116: goto st84;
	}
	goto tr1724;
st34:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof34;
case 34:
	switch( (*( sm->p)) ) {
		case 76: goto st35;
		case 108: goto st35;
	}
	goto tr3;
st35:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof35;
case 35:
	switch( (*( sm->p)) ) {
		case 79: goto st36;
		case 111: goto st36;
	}
	goto tr3;
st36:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof36;
case 36:
	switch( (*( sm->p)) ) {
		case 67: goto st37;
		case 99: goto st37;
	}
	goto tr3;
st37:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof37;
case 37:
	switch( (*( sm->p)) ) {
		case 75: goto st38;
		case 107: goto st38;
	}
	goto tr3;
st38:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof38;
case 38:
	switch( (*( sm->p)) ) {
		case 81: goto st39;
		case 113: goto st39;
	}
	goto tr3;
st39:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof39;
case 39:
	switch( (*( sm->p)) ) {
		case 85: goto st40;
		case 117: goto st40;
	}
	goto tr3;
st40:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof40;
case 40:
	switch( (*( sm->p)) ) {
		case 79: goto st41;
		case 111: goto st41;
	}
	goto tr3;
st41:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof41;
case 41:
	switch( (*( sm->p)) ) {
		case 84: goto st42;
		case 116: goto st42;
	}
	goto tr3;
st42:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof42;
case 42:
	switch( (*( sm->p)) ) {
		case 69: goto st43;
		case 101: goto st43;
	}
	goto tr3;
st43:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof43;
case 43:
	if ( (*( sm->p)) == 62 )
		goto st1375;
	goto tr3;
st1375:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1375;
case 1375:
	if ( (*( sm->p)) == 32 )
		goto st1375;
	if ( 9 <= (*( sm->p)) && (*( sm->p)) <= 13 )
		goto st1375;
	goto tr1734;
st44:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof44;
case 44:
	switch( (*( sm->p)) ) {
		case 69: goto st5;
		case 79: goto st45;
		case 101: goto st5;
		case 111: goto st45;
	}
	goto tr3;
st45:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof45;
case 45:
	switch( (*( sm->p)) ) {
		case 68: goto st46;
		case 76: goto st53;
		case 100: goto st46;
		case 108: goto st53;
	}
	goto tr3;
st46:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof46;
case 46:
	switch( (*( sm->p)) ) {
		case 69: goto st47;
		case 101: goto st47;
	}
	goto tr3;
st47:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof47;
case 47:
	switch( (*( sm->p)) ) {
		case 9: goto st48;
		case 32: goto st48;
		case 61: goto st49;
		case 62: goto tr57;
	}
	goto tr3;
st48:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof48;
case 48:
	switch( (*( sm->p)) ) {
		case 9: goto st48;
		case 32: goto st48;
		case 61: goto st49;
	}
	goto tr3;
st49:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof49;
case 49:
	switch( (*( sm->p)) ) {
		case 9: goto st49;
		case 32: goto st49;
	}
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr58;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto tr58;
	} else
		goto tr58;
	goto tr3;
tr58:
#line 71 "ext/dtext/dtext.cpp.rl"
	{ sm->a1 = sm->p; }
	goto st50;
st50:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof50;
case 50:
#line 3616 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) == 62 )
		goto tr60;
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st50;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st50;
	} else
		goto st50;
	goto tr3;
tr60:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 72 "ext/dtext/dtext.cpp.rl"
	{ sm->a2 = sm->p; }
	goto st1376;
st1376:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1376;
case 1376:
#line 3638 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 0: goto tr62;
		case 9: goto st51;
		case 10: goto tr62;
		case 32: goto st51;
	}
	goto tr1735;
st51:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof51;
case 51:
	switch( (*( sm->p)) ) {
		case 0: goto tr62;
		case 9: goto st51;
		case 10: goto tr62;
		case 32: goto st51;
	}
	goto tr61;
tr57:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
	goto st1377;
st1377:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1377;
case 1377:
#line 3665 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 0: goto tr65;
		case 9: goto st52;
		case 10: goto tr65;
		case 32: goto st52;
	}
	goto tr1736;
st52:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof52;
case 52:
	switch( (*( sm->p)) ) {
		case 0: goto tr65;
		case 9: goto st52;
		case 10: goto tr65;
		case 32: goto st52;
	}
	goto tr64;
st53:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof53;
case 53:
	switch( (*( sm->p)) ) {
		case 79: goto st54;
		case 111: goto st54;
	}
	goto tr3;
st54:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof54;
case 54:
	switch( (*( sm->p)) ) {
		case 82: goto st55;
		case 114: goto st55;
	}
	goto tr3;
st55:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof55;
case 55:
	switch( (*( sm->p)) ) {
		case 9: goto st56;
		case 32: goto st56;
		case 61: goto st58;
		case 62: goto tr71;
	}
	goto tr3;
tr73:
#line 71 "ext/dtext/dtext.cpp.rl"
	{ sm->a1 = sm->p; }
	goto st56;
st56:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof56;
case 56:
#line 3721 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 0: goto tr3;
		case 9: goto tr73;
		case 10: goto tr3;
		case 13: goto tr3;
		case 32: goto tr73;
		case 61: goto tr74;
		case 62: goto tr75;
	}
	goto tr72;
tr72:
#line 71 "ext/dtext/dtext.cpp.rl"
	{ sm->a1 = sm->p; }
	goto st57;
st57:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof57;
case 57:
#line 3740 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 0: goto tr3;
		case 10: goto tr3;
		case 13: goto tr3;
		case 62: goto tr77;
	}
	goto st57;
tr74:
#line 71 "ext/dtext/dtext.cpp.rl"
	{ sm->a1 = sm->p; }
	goto st58;
st58:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof58;
case 58:
#line 3756 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 0: goto tr3;
		case 9: goto tr74;
		case 10: goto tr3;
		case 13: goto tr3;
		case 32: goto tr74;
		case 62: goto tr75;
	}
	goto tr72;
st59:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof59;
case 59:
	switch( (*( sm->p)) ) {
		case 88: goto st60;
		case 120: goto st60;
	}
	goto tr3;
st60:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof60;
case 60:
	switch( (*( sm->p)) ) {
		case 80: goto st61;
		case 112: goto st61;
	}
	goto tr3;
st61:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof61;
case 61:
	switch( (*( sm->p)) ) {
		case 65: goto st62;
		case 97: goto st62;
	}
	goto tr3;
st62:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof62;
case 62:
	switch( (*( sm->p)) ) {
		case 78: goto st63;
		case 110: goto st63;
	}
	goto tr3;
st63:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof63;
case 63:
	switch( (*( sm->p)) ) {
		case 68: goto st64;
		case 100: goto st64;
	}
	goto tr3;
st64:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof64;
case 64:
	switch( (*( sm->p)) ) {
		case 9: goto st65;
		case 32: goto st65;
		case 61: goto st67;
		case 62: goto st1379;
	}
	goto tr3;
tr87:
#line 71 "ext/dtext/dtext.cpp.rl"
	{ sm->a1 = sm->p; }
	goto st65;
st65:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof65;
case 65:
#line 3830 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 0: goto tr3;
		case 9: goto tr87;
		case 10: goto tr3;
		case 13: goto tr3;
		case 32: goto tr87;
		case 61: goto tr88;
		case 62: goto tr89;
	}
	goto tr86;
tr86:
#line 71 "ext/dtext/dtext.cpp.rl"
	{ sm->a1 = sm->p; }
	goto st66;
st66:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof66;
case 66:
#line 3849 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 0: goto tr3;
		case 10: goto tr3;
		case 13: goto tr3;
		case 62: goto tr91;
	}
	goto st66;
tr91:
#line 72 "ext/dtext/dtext.cpp.rl"
	{ sm->a2 = sm->p; }
	goto st1378;
tr89:
#line 71 "ext/dtext/dtext.cpp.rl"
	{ sm->a1 = sm->p; }
#line 72 "ext/dtext/dtext.cpp.rl"
	{ sm->a2 = sm->p; }
	goto st1378;
st1378:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1378;
case 1378:
#line 3871 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) == 32 )
		goto st1378;
	if ( 9 <= (*( sm->p)) && (*( sm->p)) <= 13 )
		goto st1378;
	goto tr1737;
tr88:
#line 71 "ext/dtext/dtext.cpp.rl"
	{ sm->a1 = sm->p; }
	goto st67;
st67:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof67;
case 67:
#line 3885 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 0: goto tr3;
		case 9: goto tr88;
		case 10: goto tr3;
		case 13: goto tr3;
		case 32: goto tr88;
		case 62: goto tr89;
	}
	goto tr86;
st1379:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1379;
case 1379:
	if ( (*( sm->p)) == 32 )
		goto st1379;
	if ( 9 <= (*( sm->p)) && (*( sm->p)) <= 13 )
		goto st1379;
	goto tr1739;
st68:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof68;
case 68:
	switch( (*( sm->p)) ) {
		case 79: goto st69;
		case 111: goto st69;
	}
	goto tr3;
st69:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof69;
case 69:
	switch( (*( sm->p)) ) {
		case 68: goto st70;
		case 100: goto st70;
	}
	goto tr3;
st70:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof70;
case 70:
	switch( (*( sm->p)) ) {
		case 84: goto st71;
		case 116: goto st71;
	}
	goto tr3;
st71:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof71;
case 71:
	switch( (*( sm->p)) ) {
		case 69: goto st72;
		case 101: goto st72;
	}
	goto tr3;
st72:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof72;
case 72:
	switch( (*( sm->p)) ) {
		case 88: goto st73;
		case 120: goto st73;
	}
	goto tr3;
st73:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof73;
case 73:
	switch( (*( sm->p)) ) {
		case 84: goto st74;
		case 116: goto st74;
	}
	goto tr3;
st74:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof74;
case 74:
	if ( (*( sm->p)) == 62 )
		goto tr98;
	goto tr3;
tr98:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
	goto st1380;
st1380:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1380;
case 1380:
#line 3973 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 0: goto tr100;
		case 9: goto st75;
		case 10: goto tr100;
		case 32: goto st75;
	}
	goto tr1740;
st75:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof75;
case 75:
	switch( (*( sm->p)) ) {
		case 0: goto tr100;
		case 9: goto st75;
		case 10: goto tr100;
		case 32: goto st75;
	}
	goto tr99;
st76:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof76;
case 76:
	switch( (*( sm->p)) ) {
		case 80: goto st77;
		case 112: goto st77;
	}
	goto tr3;
st77:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof77;
case 77:
	switch( (*( sm->p)) ) {
		case 79: goto st78;
		case 111: goto st78;
	}
	goto tr3;
st78:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof78;
case 78:
	switch( (*( sm->p)) ) {
		case 73: goto st79;
		case 105: goto st79;
	}
	goto tr3;
st79:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof79;
case 79:
	switch( (*( sm->p)) ) {
		case 76: goto st80;
		case 108: goto st80;
	}
	goto tr3;
st80:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof80;
case 80:
	switch( (*( sm->p)) ) {
		case 69: goto st81;
		case 101: goto st81;
	}
	goto tr3;
st81:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof81;
case 81:
	switch( (*( sm->p)) ) {
		case 82: goto st82;
		case 114: goto st82;
	}
	goto tr3;
st82:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof82;
case 82:
	switch( (*( sm->p)) ) {
		case 62: goto st1381;
		case 83: goto st83;
		case 115: goto st83;
	}
	goto tr3;
st1381:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1381;
case 1381:
	if ( (*( sm->p)) == 32 )
		goto st1381;
	if ( 9 <= (*( sm->p)) && (*( sm->p)) <= 13 )
		goto st1381;
	goto tr1741;
st83:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof83;
case 83:
	if ( (*( sm->p)) == 62 )
		goto st1381;
	goto tr3;
st84:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof84;
case 84:
	switch( (*( sm->p)) ) {
		case 65: goto st14;
		case 78: goto st85;
		case 97: goto st14;
		case 110: goto st85;
	}
	goto tr3;
st85:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof85;
case 85:
	if ( (*( sm->p)) == 62 )
		goto tr111;
	goto tr3;
tr1720:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
	goto st1382;
st1382:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1382;
case 1382:
#line 4098 "ext/dtext/dtext.cpp"
	if ( 49 <= (*( sm->p)) && (*( sm->p)) <= 54 )
		goto tr1742;
	goto tr1724;
tr1742:
#line 71 "ext/dtext/dtext.cpp.rl"
	{ sm->a1 = sm->p; }
	goto st86;
st86:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof86;
case 86:
#line 4110 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 35: goto tr112;
		case 46: goto tr113;
	}
	goto tr3;
tr112:
#line 72 "ext/dtext/dtext.cpp.rl"
	{ sm->a2 = sm->p; }
	goto st87;
st87:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof87;
case 87:
#line 4124 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 33: goto tr114;
		case 35: goto tr114;
		case 38: goto tr114;
		case 45: goto tr114;
		case 95: goto tr114;
	}
	if ( (*( sm->p)) < 65 ) {
		if ( 47 <= (*( sm->p)) && (*( sm->p)) <= 58 )
			goto tr114;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto tr114;
	} else
		goto tr114;
	goto tr3;
tr114:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ sm->b1 = sm->p; }
	goto st88;
st88:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof88;
case 88:
#line 4149 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 33: goto st88;
		case 35: goto st88;
		case 38: goto st88;
		case 46: goto tr116;
		case 95: goto st88;
	}
	if ( (*( sm->p)) < 65 ) {
		if ( 45 <= (*( sm->p)) && (*( sm->p)) <= 58 )
			goto st88;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st88;
	} else
		goto st88;
	goto tr3;
tr113:
#line 72 "ext/dtext/dtext.cpp.rl"
	{ sm->a2 = sm->p; }
#line 73 "ext/dtext/dtext.cpp.rl"
	{ sm->b1 = sm->p; }
#line 74 "ext/dtext/dtext.cpp.rl"
	{ sm->b2 = sm->p; }
	goto st1383;
tr116:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ sm->b2 = sm->p; }
	goto st1383;
st1383:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1383;
case 1383:
#line 4182 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 9: goto st1383;
		case 32: goto st1383;
	}
	goto tr1743;
tr1721:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
	goto st1384;
st1384:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1384;
case 1384:
#line 4196 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 67: goto st89;
		case 69: goto st102;
		case 72: goto st25;
		case 78: goto st111;
		case 81: goto st118;
		case 83: goto st123;
		case 84: goto st131;
		case 99: goto st89;
		case 101: goto st102;
		case 104: goto st25;
		case 110: goto st111;
		case 113: goto st118;
		case 115: goto st123;
		case 116: goto st131;
	}
	goto tr1724;
st89:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof89;
case 89:
	switch( (*( sm->p)) ) {
		case 69: goto st20;
		case 79: goto st90;
		case 101: goto st20;
		case 111: goto st90;
	}
	goto tr3;
st90:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof90;
case 90:
	switch( (*( sm->p)) ) {
		case 68: goto st91;
		case 76: goto st96;
		case 100: goto st91;
		case 108: goto st96;
	}
	goto tr3;
st91:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof91;
case 91:
	switch( (*( sm->p)) ) {
		case 69: goto st92;
		case 101: goto st92;
	}
	goto tr3;
st92:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof92;
case 92:
	switch( (*( sm->p)) ) {
		case 9: goto st93;
		case 32: goto st93;
		case 61: goto st94;
		case 93: goto tr57;
	}
	goto tr3;
st93:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof93;
case 93:
	switch( (*( sm->p)) ) {
		case 9: goto st93;
		case 32: goto st93;
		case 61: goto st94;
	}
	goto tr3;
st94:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof94;
case 94:
	switch( (*( sm->p)) ) {
		case 9: goto st94;
		case 32: goto st94;
	}
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr123;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto tr123;
	} else
		goto tr123;
	goto tr3;
tr123:
#line 71 "ext/dtext/dtext.cpp.rl"
	{ sm->a1 = sm->p; }
	goto st95;
st95:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof95;
case 95:
#line 4291 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) == 93 )
		goto tr60;
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st95;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st95;
	} else
		goto st95;
	goto tr3;
st96:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof96;
case 96:
	switch( (*( sm->p)) ) {
		case 79: goto st97;
		case 111: goto st97;
	}
	goto tr3;
st97:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof97;
case 97:
	switch( (*( sm->p)) ) {
		case 82: goto st98;
		case 114: goto st98;
	}
	goto tr3;
st98:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof98;
case 98:
	switch( (*( sm->p)) ) {
		case 9: goto st99;
		case 32: goto st99;
		case 61: goto st101;
		case 93: goto tr71;
	}
	goto tr3;
tr130:
#line 71 "ext/dtext/dtext.cpp.rl"
	{ sm->a1 = sm->p; }
	goto st99;
st99:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof99;
case 99:
#line 4340 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 0: goto tr3;
		case 9: goto tr130;
		case 10: goto tr3;
		case 13: goto tr3;
		case 32: goto tr130;
		case 61: goto tr131;
		case 93: goto tr75;
	}
	goto tr129;
tr129:
#line 71 "ext/dtext/dtext.cpp.rl"
	{ sm->a1 = sm->p; }
	goto st100;
st100:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof100;
case 100:
#line 4359 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 0: goto tr3;
		case 10: goto tr3;
		case 13: goto tr3;
		case 93: goto tr77;
	}
	goto st100;
tr131:
#line 71 "ext/dtext/dtext.cpp.rl"
	{ sm->a1 = sm->p; }
	goto st101;
st101:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof101;
case 101:
#line 4375 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 0: goto tr3;
		case 9: goto tr131;
		case 10: goto tr3;
		case 13: goto tr3;
		case 32: goto tr131;
		case 93: goto tr75;
	}
	goto tr129;
st102:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof102;
case 102:
	switch( (*( sm->p)) ) {
		case 88: goto st103;
		case 120: goto st103;
	}
	goto tr3;
st103:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof103;
case 103:
	switch( (*( sm->p)) ) {
		case 80: goto st104;
		case 112: goto st104;
	}
	goto tr3;
st104:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof104;
case 104:
	switch( (*( sm->p)) ) {
		case 65: goto st105;
		case 97: goto st105;
	}
	goto tr3;
st105:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof105;
case 105:
	switch( (*( sm->p)) ) {
		case 78: goto st106;
		case 110: goto st106;
	}
	goto tr3;
st106:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof106;
case 106:
	switch( (*( sm->p)) ) {
		case 68: goto st107;
		case 100: goto st107;
	}
	goto tr3;
st107:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof107;
case 107:
	switch( (*( sm->p)) ) {
		case 9: goto st108;
		case 32: goto st108;
		case 61: goto st110;
		case 93: goto st1379;
	}
	goto tr3;
tr141:
#line 71 "ext/dtext/dtext.cpp.rl"
	{ sm->a1 = sm->p; }
	goto st108;
st108:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof108;
case 108:
#line 4449 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 0: goto tr3;
		case 9: goto tr141;
		case 10: goto tr3;
		case 13: goto tr3;
		case 32: goto tr141;
		case 61: goto tr142;
		case 93: goto tr89;
	}
	goto tr140;
tr140:
#line 71 "ext/dtext/dtext.cpp.rl"
	{ sm->a1 = sm->p; }
	goto st109;
st109:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof109;
case 109:
#line 4468 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 0: goto tr3;
		case 10: goto tr3;
		case 13: goto tr3;
		case 93: goto tr91;
	}
	goto st109;
tr142:
#line 71 "ext/dtext/dtext.cpp.rl"
	{ sm->a1 = sm->p; }
	goto st110;
st110:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof110;
case 110:
#line 4484 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 0: goto tr3;
		case 9: goto tr142;
		case 10: goto tr3;
		case 13: goto tr3;
		case 32: goto tr142;
		case 93: goto tr89;
	}
	goto tr140;
st111:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof111;
case 111:
	switch( (*( sm->p)) ) {
		case 79: goto st112;
		case 111: goto st112;
	}
	goto tr3;
st112:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof112;
case 112:
	switch( (*( sm->p)) ) {
		case 68: goto st113;
		case 100: goto st113;
	}
	goto tr3;
st113:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof113;
case 113:
	switch( (*( sm->p)) ) {
		case 84: goto st114;
		case 116: goto st114;
	}
	goto tr3;
st114:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof114;
case 114:
	switch( (*( sm->p)) ) {
		case 69: goto st115;
		case 101: goto st115;
	}
	goto tr3;
st115:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof115;
case 115:
	switch( (*( sm->p)) ) {
		case 88: goto st116;
		case 120: goto st116;
	}
	goto tr3;
st116:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof116;
case 116:
	switch( (*( sm->p)) ) {
		case 84: goto st117;
		case 116: goto st117;
	}
	goto tr3;
st117:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof117;
case 117:
	if ( (*( sm->p)) == 93 )
		goto tr98;
	goto tr3;
st118:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof118;
case 118:
	switch( (*( sm->p)) ) {
		case 85: goto st119;
		case 117: goto st119;
	}
	goto tr3;
st119:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof119;
case 119:
	switch( (*( sm->p)) ) {
		case 79: goto st120;
		case 111: goto st120;
	}
	goto tr3;
st120:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof120;
case 120:
	switch( (*( sm->p)) ) {
		case 84: goto st121;
		case 116: goto st121;
	}
	goto tr3;
st121:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof121;
case 121:
	switch( (*( sm->p)) ) {
		case 69: goto st122;
		case 101: goto st122;
	}
	goto tr3;
st122:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof122;
case 122:
	if ( (*( sm->p)) == 93 )
		goto st1375;
	goto tr3;
st123:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof123;
case 123:
	switch( (*( sm->p)) ) {
		case 80: goto st124;
		case 112: goto st124;
	}
	goto tr3;
st124:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof124;
case 124:
	switch( (*( sm->p)) ) {
		case 79: goto st125;
		case 111: goto st125;
	}
	goto tr3;
st125:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof125;
case 125:
	switch( (*( sm->p)) ) {
		case 73: goto st126;
		case 105: goto st126;
	}
	goto tr3;
st126:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof126;
case 126:
	switch( (*( sm->p)) ) {
		case 76: goto st127;
		case 108: goto st127;
	}
	goto tr3;
st127:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof127;
case 127:
	switch( (*( sm->p)) ) {
		case 69: goto st128;
		case 101: goto st128;
	}
	goto tr3;
st128:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof128;
case 128:
	switch( (*( sm->p)) ) {
		case 82: goto st129;
		case 114: goto st129;
	}
	goto tr3;
st129:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof129;
case 129:
	switch( (*( sm->p)) ) {
		case 83: goto st130;
		case 93: goto st1381;
		case 115: goto st130;
	}
	goto tr3;
st130:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof130;
case 130:
	if ( (*( sm->p)) == 93 )
		goto st1381;
	goto tr3;
st131:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof131;
case 131:
	switch( (*( sm->p)) ) {
		case 65: goto st28;
		case 78: goto st132;
		case 97: goto st28;
		case 110: goto st132;
	}
	goto tr3;
st132:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof132;
case 132:
	if ( (*( sm->p)) == 93 )
		goto tr111;
	goto tr3;
tr1722:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
	goto st1385;
st1385:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1385;
case 1385:
#line 4695 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) == 96 )
		goto st133;
	goto tr1724;
st133:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof133;
case 133:
	if ( (*( sm->p)) == 96 )
		goto st134;
	goto tr3;
tr164:
#line 71 "ext/dtext/dtext.cpp.rl"
	{ sm->a1 = sm->p; }
#line 72 "ext/dtext/dtext.cpp.rl"
	{ sm->a2 = sm->p; }
	goto st134;
st134:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof134;
case 134:
#line 4716 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 0: goto tr163;
		case 9: goto tr164;
		case 10: goto tr163;
		case 32: goto tr164;
	}
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr165;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto tr165;
	} else
		goto tr165;
	goto tr3;
tr174:
#line 72 "ext/dtext/dtext.cpp.rl"
	{ sm->a2 = sm->p; }
	goto st135;
tr163:
#line 71 "ext/dtext/dtext.cpp.rl"
	{ sm->a1 = sm->p; }
#line 72 "ext/dtext/dtext.cpp.rl"
	{ sm->a2 = sm->p; }
	goto st135;
st135:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof135;
case 135:
#line 4746 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 0: goto tr167;
		case 10: goto tr167;
	}
	goto tr166;
tr166:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ sm->b1 = sm->p; }
	goto st136;
st136:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof136;
case 136:
#line 4760 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 0: goto tr169;
		case 10: goto tr169;
	}
	goto st136;
tr169:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ sm->b2 = sm->p; }
	goto st137;
tr167:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ sm->b1 = sm->p; }
#line 74 "ext/dtext/dtext.cpp.rl"
	{ sm->b2 = sm->p; }
	goto st137;
st137:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof137;
case 137:
#line 4780 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 0: goto tr169;
		case 10: goto tr169;
		case 96: goto st138;
	}
	goto st136;
st138:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof138;
case 138:
	switch( (*( sm->p)) ) {
		case 0: goto tr169;
		case 10: goto tr169;
		case 96: goto st139;
	}
	goto st136;
st139:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof139;
case 139:
	switch( (*( sm->p)) ) {
		case 0: goto tr169;
		case 10: goto tr169;
		case 96: goto st140;
	}
	goto st136;
st140:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof140;
case 140:
	switch( (*( sm->p)) ) {
		case 0: goto tr173;
		case 9: goto st140;
		case 10: goto tr173;
		case 32: goto st140;
	}
	goto st136;
tr165:
#line 71 "ext/dtext/dtext.cpp.rl"
	{ sm->a1 = sm->p; }
	goto st141;
st141:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof141;
case 141:
#line 4826 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 0: goto tr174;
		case 9: goto tr175;
		case 10: goto tr174;
		case 32: goto tr175;
	}
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st141;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st141;
	} else
		goto st141;
	goto tr3;
tr175:
#line 72 "ext/dtext/dtext.cpp.rl"
	{ sm->a2 = sm->p; }
	goto st142;
st142:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof142;
case 142:
#line 4850 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 0: goto st135;
		case 9: goto st142;
		case 10: goto st135;
		case 32: goto st142;
	}
	goto tr3;
tr179:
#line 291 "ext/dtext/dtext.cpp.rl"
	{{( sm->p) = ((( sm->te)))-1;}{ append_html_escaped(sm, (*( sm->p))); }}
	goto st1386;
tr185:
#line 283 "ext/dtext/dtext.cpp.rl"
	{( sm->te) = ( sm->p)+1;{ dstack_close_element(sm, INLINE_B); }}
	goto st1386;
tr186:
#line 285 "ext/dtext/dtext.cpp.rl"
	{( sm->te) = ( sm->p)+1;{ dstack_close_element(sm, INLINE_I); }}
	goto st1386;
tr187:
#line 287 "ext/dtext/dtext.cpp.rl"
	{( sm->te) = ( sm->p)+1;{ dstack_close_element(sm, INLINE_S); }}
	goto st1386;
tr192:
#line 289 "ext/dtext/dtext.cpp.rl"
	{( sm->te) = ( sm->p)+1;{ dstack_close_element(sm, INLINE_U); }}
	goto st1386;
tr193:
#line 282 "ext/dtext/dtext.cpp.rl"
	{( sm->te) = ( sm->p)+1;{ dstack_open_element(sm,  INLINE_B, "<strong>"); }}
	goto st1386;
tr195:
#line 284 "ext/dtext/dtext.cpp.rl"
	{( sm->te) = ( sm->p)+1;{ dstack_open_element(sm,  INLINE_I, "<em>"); }}
	goto st1386;
tr196:
#line 286 "ext/dtext/dtext.cpp.rl"
	{( sm->te) = ( sm->p)+1;{ dstack_open_element(sm,  INLINE_S, "<s>"); }}
	goto st1386;
tr202:
#line 288 "ext/dtext/dtext.cpp.rl"
	{( sm->te) = ( sm->p)+1;{ dstack_open_element(sm,  INLINE_U, "<u>"); }}
	goto st1386;
tr1752:
#line 291 "ext/dtext/dtext.cpp.rl"
	{( sm->te) = ( sm->p)+1;{ append_html_escaped(sm, (*( sm->p))); }}
	goto st1386;
tr1753:
#line 290 "ext/dtext/dtext.cpp.rl"
	{( sm->te) = ( sm->p)+1;}
	goto st1386;
tr1756:
#line 291 "ext/dtext/dtext.cpp.rl"
	{( sm->te) = ( sm->p);( sm->p)--;{ append_html_escaped(sm, (*( sm->p))); }}
	goto st1386;
st1386:
#line 1 "NONE"
	{( sm->ts) = 0;}
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1386;
case 1386:
#line 1 "NONE"
	{( sm->ts) = ( sm->p);}
#line 4914 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 0: goto tr1753;
		case 60: goto tr1754;
		case 91: goto tr1755;
	}
	goto tr1752;
tr1754:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
	goto st1387;
st1387:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1387;
case 1387:
#line 4929 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 47: goto st143;
		case 66: goto st153;
		case 69: goto st154;
		case 73: goto st155;
		case 83: goto st156;
		case 85: goto st161;
		case 98: goto st153;
		case 101: goto st154;
		case 105: goto st155;
		case 115: goto st156;
		case 117: goto st161;
	}
	goto tr1756;
st143:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof143;
case 143:
	switch( (*( sm->p)) ) {
		case 66: goto st144;
		case 69: goto st145;
		case 73: goto st146;
		case 83: goto st147;
		case 85: goto st152;
		case 98: goto st144;
		case 101: goto st145;
		case 105: goto st146;
		case 115: goto st147;
		case 117: goto st152;
	}
	goto tr179;
st144:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof144;
case 144:
	if ( (*( sm->p)) == 62 )
		goto tr185;
	goto tr179;
st145:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof145;
case 145:
	switch( (*( sm->p)) ) {
		case 77: goto st146;
		case 109: goto st146;
	}
	goto tr179;
st146:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof146;
case 146:
	if ( (*( sm->p)) == 62 )
		goto tr186;
	goto tr179;
st147:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof147;
case 147:
	switch( (*( sm->p)) ) {
		case 62: goto tr187;
		case 84: goto st148;
		case 116: goto st148;
	}
	goto tr179;
st148:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof148;
case 148:
	switch( (*( sm->p)) ) {
		case 82: goto st149;
		case 114: goto st149;
	}
	goto tr179;
st149:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof149;
case 149:
	switch( (*( sm->p)) ) {
		case 79: goto st150;
		case 111: goto st150;
	}
	goto tr179;
st150:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof150;
case 150:
	switch( (*( sm->p)) ) {
		case 78: goto st151;
		case 110: goto st151;
	}
	goto tr179;
st151:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof151;
case 151:
	switch( (*( sm->p)) ) {
		case 71: goto st144;
		case 103: goto st144;
	}
	goto tr179;
st152:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof152;
case 152:
	if ( (*( sm->p)) == 62 )
		goto tr192;
	goto tr179;
st153:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof153;
case 153:
	if ( (*( sm->p)) == 62 )
		goto tr193;
	goto tr179;
st154:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof154;
case 154:
	switch( (*( sm->p)) ) {
		case 77: goto st155;
		case 109: goto st155;
	}
	goto tr179;
st155:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof155;
case 155:
	if ( (*( sm->p)) == 62 )
		goto tr195;
	goto tr179;
st156:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof156;
case 156:
	switch( (*( sm->p)) ) {
		case 62: goto tr196;
		case 84: goto st157;
		case 116: goto st157;
	}
	goto tr179;
st157:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof157;
case 157:
	switch( (*( sm->p)) ) {
		case 82: goto st158;
		case 114: goto st158;
	}
	goto tr179;
st158:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof158;
case 158:
	switch( (*( sm->p)) ) {
		case 79: goto st159;
		case 111: goto st159;
	}
	goto tr179;
st159:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof159;
case 159:
	switch( (*( sm->p)) ) {
		case 78: goto st160;
		case 110: goto st160;
	}
	goto tr179;
st160:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof160;
case 160:
	switch( (*( sm->p)) ) {
		case 71: goto st153;
		case 103: goto st153;
	}
	goto tr179;
st161:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof161;
case 161:
	if ( (*( sm->p)) == 62 )
		goto tr202;
	goto tr179;
tr1755:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
	goto st1388;
st1388:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1388;
case 1388:
#line 5121 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 47: goto st162;
		case 66: goto st167;
		case 73: goto st168;
		case 83: goto st169;
		case 85: goto st170;
		case 98: goto st167;
		case 105: goto st168;
		case 115: goto st169;
		case 117: goto st170;
	}
	goto tr1756;
st162:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof162;
case 162:
	switch( (*( sm->p)) ) {
		case 66: goto st163;
		case 73: goto st164;
		case 83: goto st165;
		case 85: goto st166;
		case 98: goto st163;
		case 105: goto st164;
		case 115: goto st165;
		case 117: goto st166;
	}
	goto tr179;
st163:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof163;
case 163:
	if ( (*( sm->p)) == 93 )
		goto tr185;
	goto tr179;
st164:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof164;
case 164:
	if ( (*( sm->p)) == 93 )
		goto tr186;
	goto tr179;
st165:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof165;
case 165:
	if ( (*( sm->p)) == 93 )
		goto tr187;
	goto tr179;
st166:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof166;
case 166:
	if ( (*( sm->p)) == 93 )
		goto tr192;
	goto tr179;
st167:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof167;
case 167:
	if ( (*( sm->p)) == 93 )
		goto tr193;
	goto tr179;
st168:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof168;
case 168:
	if ( (*( sm->p)) == 93 )
		goto tr195;
	goto tr179;
st169:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof169;
case 169:
	if ( (*( sm->p)) == 93 )
		goto tr196;
	goto tr179;
st170:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof170;
case 170:
	if ( (*( sm->p)) == 93 )
		goto tr202;
	goto tr179;
tr207:
#line 1 "NONE"
	{	switch( ( sm->act) ) {
	case 11:
	{{( sm->p) = ((( sm->te)))-1;} append_id_link(sm, "post", "post", "/posts/", { sm->a1, sm->a2 }); }
	break;
	case 12:
	{{( sm->p) = ((( sm->te)))-1;} append_id_link(sm, "forum", "forum-post", "/forums/", { sm->a1, sm->a2 }); }
	break;
	case 13:
	{{( sm->p) = ((( sm->te)))-1;} append_id_link(sm, "topic", "forum-topic", "/forums/", { sm->a1, sm->a2 }); }
	break;
	case 14:
	{{( sm->p) = ((( sm->te)))-1;} append_id_link(sm, "comment", "comment", "/comments/", { sm->a1, sm->a2 }); }
	break;
	case 15:
	{{( sm->p) = ((( sm->te)))-1;} append_id_link(sm, "dmail", "dmail", "/dmails/", { sm->a1, sm->a2 }); }
	break;
	case 16:
	{{( sm->p) = ((( sm->te)))-1;} append_id_link(sm, "pool", "pool", "/pools/", { sm->a1, sm->a2 }); }
	break;
	case 17:
	{{( sm->p) = ((( sm->te)))-1;} append_id_link(sm, "user", "user", "/users/", { sm->a1, sm->a2 }); }
	break;
	case 18:
	{{( sm->p) = ((( sm->te)))-1;} append_id_link(sm, "artist", "artist", "/artists/", { sm->a1, sm->a2 }); }
	break;
	case 19:
	{{( sm->p) = ((( sm->te)))-1;} append_id_link(sm, "user report", "user-report", "/user_flags/", { sm->a1, sm->a2 }); }
	break;
	case 20:
	{{( sm->p) = ((( sm->te)))-1;} append_id_link(sm, "tag alias", "tag-alias", "https://beta.sankakucomplex.com/tag_aliases?id[0]=", { sm->a1, sm->a2 }); }
	break;
	case 21:
	{{( sm->p) = ((( sm->te)))-1;} append_id_link(sm, "tag implication", "tag-implication", "https://beta.sankakucomplex.com/tag_implications?id[0]=", { sm->a1, sm->a2 }); }
	break;
	case 22:
	{{( sm->p) = ((( sm->te)))-1;} append_id_link(sm, "tag translation", "tag-translation", "https://beta.sankakucomplex.com/tag_translations?id[0]=", { sm->a1, sm->a2 }); }
	break;
	case 23:
	{{( sm->p) = ((( sm->te)))-1;} append_id_link(sm, "book", "book", "https://beta.sankakucomplex.com/books/", { sm->a1, sm->a2 }); }
	break;
	case 24:
	{{( sm->p) = ((( sm->te)))-1;} append_id_link(sm, "series", "series", "https://beta.sankakucomplex.com/series/", { sm->a1, sm->a2 }); }
	break;
	case 25:
	{{( sm->p) = ((( sm->te)))-1;} append_id_link(sm, "mod action", "mod-action", "/mod_actions?id=", { sm->a1, sm->a2 }); }
	break;
	case 26:
	{{( sm->p) = ((( sm->te)))-1;} append_id_link(sm, "record", "user-record", "/user_records?id=", { sm->a1, sm->a2 }); }
	break;
	case 27:
	{{( sm->p) = ((( sm->te)))-1;} append_id_link(sm, "wiki", "wiki-page", "/wiki/", { sm->a1, sm->a2 }); }
	break;
	case 35:
	{{( sm->p) = ((( sm->te)))-1;}
    append_bare_named_url(sm, { sm->b1, sm->b2 + 1 }, { sm->a1, sm->a2 });
  }
	break;
	case 38:
	{{( sm->p) = ((( sm->te)))-1;}
    append_bare_unnamed_url(sm, { sm->ts, sm->te });
  }
	break;
	case 40:
	{{( sm->p) = ((( sm->te)))-1;}
    append_mention(sm, { sm->a1, sm->a2 + 1 });
  }
	break;
	case 76:
	{{( sm->p) = ((( sm->te)))-1;}
    g_debug("inline newline2");

    if (dstack_check(sm, BLOCK_P)) {
      dstack_rewind(sm);
    } else if (sm->header_mode) {
      dstack_close_leaf_blocks(sm);
    } else {
      dstack_close_list(sm);
    }

    if (sm->options.f_inline) {
      append(sm, " ");
    }

    { sm->cs = ( (sm->stack.data()))[--( sm->top)];goto _again;}
  }
	break;
	case 77:
	{{( sm->p) = ((( sm->te)))-1;}
    g_debug("inline newline");

    if (sm->header_mode) {
      dstack_close_leaf_blocks(sm);
      { sm->cs = ( (sm->stack.data()))[--( sm->top)];goto _again;}
    } else if (dstack_is_open(sm, BLOCK_UL)) {
      dstack_close_list(sm);
      { sm->cs = ( (sm->stack.data()))[--( sm->top)];goto _again;}
    } else {
      append(sm, "<br>");
    }
  }
	break;
	case 80:
	{{( sm->p) = ((( sm->te)))-1;}
    append(sm, std::string_view { sm->ts, sm->te });
  }
	break;
	case 81:
	{{( sm->p) = ((( sm->te)))-1;}
    append_html_escaped(sm, (*( sm->p)));
  }
	break;
	default:
	{{( sm->p) = ((( sm->te)))-1;}}
	break;
	}
	}
	goto st1389;
tr210:
#line 583 "ext/dtext/dtext.cpp.rl"
	{{( sm->p) = ((( sm->te)))-1;}{
    append(sm, std::string_view { sm->ts, sm->te });
  }}
	goto st1389;
tr214:
#line 587 "ext/dtext/dtext.cpp.rl"
	{{( sm->p) = ((( sm->te)))-1;}{
    append_html_escaped(sm, (*( sm->p)));
  }}
	goto st1389;
tr216:
#line 563 "ext/dtext/dtext.cpp.rl"
	{{( sm->p) = ((( sm->te)))-1;}{
    g_debug("inline newline");

    if (sm->header_mode) {
      dstack_close_leaf_blocks(sm);
      { sm->cs = ( (sm->stack.data()))[--( sm->top)];goto _again;}
    } else if (dstack_is_open(sm, BLOCK_UL)) {
      dstack_close_list(sm);
      { sm->cs = ( (sm->stack.data()))[--( sm->top)];goto _again;}
    } else {
      append(sm, "<br>");
    }
  }}
	goto st1389;
tr234:
#line 453 "ext/dtext/dtext.cpp.rl"
	{( sm->te) = ( sm->p)+1;{
    dstack_close_leaf_blocks(sm);
    {( sm->p) = (( sm->ts))-1;}
    { sm->cs = ( (sm->stack.data()))[--( sm->top)];goto _again;}
  }}
	goto st1389;
tr239:
#line 520 "ext/dtext/dtext.cpp.rl"
	{( sm->te) = ( sm->p)+1;{
    dstack_close_leaf_blocks(sm);
    {( sm->p) = (( sm->ts))-1;}
    { sm->cs = ( (sm->stack.data()))[--( sm->top)];goto _again;}
  }}
	goto st1389;
tr255:
#line 545 "ext/dtext/dtext.cpp.rl"
	{{( sm->p) = ((( sm->te)))-1;}{
    g_debug("inline newline2");

    if (dstack_check(sm, BLOCK_P)) {
      dstack_rewind(sm);
    } else if (sm->header_mode) {
      dstack_close_leaf_blocks(sm);
    } else {
      dstack_close_list(sm);
    }

    if (sm->options.f_inline) {
      append(sm, " ");
    }

    { sm->cs = ( (sm->stack.data()))[--( sm->top)];goto _again;}
  }}
	goto st1389;
tr269:
#line 419 "ext/dtext/dtext.cpp.rl"
	{( sm->te) = ( sm->p)+1;{
    g_debug("inline [/color]");

    if (dstack_check(sm, INLINE_COLOR)) {
      dstack_close_element(sm, INLINE_COLOR);
    } else if (dstack_close_element(sm, BLOCK_COLOR)) {
      { sm->cs = ( (sm->stack.data()))[--( sm->top)];goto _again;}
    }
  }}
	goto st1389;
tr273:
#line 532 "ext/dtext/dtext.cpp.rl"
	{( sm->te) = ( sm->p)+1;{
    if (dstack_close_element(sm, BLOCK_TD)) {
      { sm->cs = ( (sm->stack.data()))[--( sm->top)];goto _again;}
    }
  }}
	goto st1389;
tr274:
#line 526 "ext/dtext/dtext.cpp.rl"
	{( sm->te) = ( sm->p)+1;{
    if (dstack_close_element(sm, BLOCK_TH)) {
      { sm->cs = ( (sm->stack.data()))[--( sm->top)];goto _again;}
    }
  }}
	goto st1389;
tr275:
#line 382 "ext/dtext/dtext.cpp.rl"
	{( sm->te) = ( sm->p)+1;{
    g_debug("inline [/tn]");

    if (dstack_check(sm, INLINE_TN)) {
      dstack_close_element(sm, INLINE_TN);
    } else if (dstack_close_element(sm, BLOCK_TN)) {
      { sm->cs = ( (sm->stack.data()))[--( sm->top)];goto _again;}
    }
  }}
	goto st1389;
tr328:
#line 463 "ext/dtext/dtext.cpp.rl"
	{( sm->te) = ( sm->p)+1;{
    if (dstack_is_open(sm, INLINE_SPOILER)) {
      dstack_close_element(sm, INLINE_SPOILER);
    } else if (dstack_is_open(sm, BLOCK_SPOILER)) {
      dstack_close_until(sm, BLOCK_SPOILER);
      { sm->cs = ( (sm->stack.data()))[--( sm->top)];goto _again;}
    } else {
      append_html_escaped(sm, { sm->ts, sm->te });
    }
  }}
	goto st1389;
tr335:
#line 482 "ext/dtext/dtext.cpp.rl"
	{( sm->te) = ( sm->p)+1;{
    dstack_close_leaf_blocks(sm);
    {( sm->p) = (( sm->ts))-1;}
    { sm->cs = ( (sm->stack.data()))[--( sm->top)];goto _again;}
  }}
	goto st1389;
tr338:
#line 72 "ext/dtext/dtext.cpp.rl"
	{ sm->a2 = sm->p; }
#line 482 "ext/dtext/dtext.cpp.rl"
	{( sm->te) = ( sm->p)+1;{
    dstack_close_leaf_blocks(sm);
    {( sm->p) = (( sm->ts))-1;}
    { sm->cs = ( (sm->stack.data()))[--( sm->top)];goto _again;}
  }}
	goto st1389;
tr393:
#line 447 "ext/dtext/dtext.cpp.rl"
	{( sm->te) = ( sm->p)+1;{
    dstack_close_leaf_blocks(sm);
    {( sm->p) = (( sm->ts))-1;}
    { sm->cs = ( (sm->stack.data()))[--( sm->top)];goto _again;}
  }}
	goto st1389;
tr406:
#line 334 "ext/dtext/dtext.cpp.rl"
	{{( sm->p) = ((( sm->te)))-1;}{
    append_bare_named_url(sm, { sm->b1, sm->b2 + 1 }, { sm->a1, sm->a2 });
  }}
	goto st1389;
tr472:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ sm->b2 = sm->p; }
#line 338 "ext/dtext/dtext.cpp.rl"
	{( sm->te) = ( sm->p)+1;{
    append_named_url(sm, { sm->b1, sm->b2 }, { sm->a1, sm->a2 });
  }}
	goto st1389;
tr698:
#line 299 "ext/dtext/dtext.cpp.rl"
	{{( sm->p) = ((( sm->te)))-1;}{ append_id_link(sm, "dmail", "dmail", "/dmails/", { sm->a1, sm->a2 }); }}
	goto st1389;
tr746:
#line 346 "ext/dtext/dtext.cpp.rl"
	{{( sm->p) = ((( sm->te)))-1;}{
    append_bare_unnamed_url(sm, { sm->ts, sm->te });
  }}
	goto st1389;
tr951:
#line 297 "ext/dtext/dtext.cpp.rl"
	{{( sm->p) = ((( sm->te)))-1;}{ append_id_link(sm, "topic", "forum-topic", "/forums/", { sm->a1, sm->a2 }); }}
	goto st1389;
tr1014:
#line 370 "ext/dtext/dtext.cpp.rl"
	{( sm->te) = ( sm->p)+1;{ dstack_close_element(sm, INLINE_B); }}
	goto st1389;
tr1015:
#line 372 "ext/dtext/dtext.cpp.rl"
	{( sm->te) = ( sm->p)+1;{ dstack_close_element(sm, INLINE_I); }}
	goto st1389;
tr1016:
#line 374 "ext/dtext/dtext.cpp.rl"
	{( sm->te) = ( sm->p)+1;{ dstack_close_element(sm, INLINE_S); }}
	goto st1389;
tr1017:
#line 376 "ext/dtext/dtext.cpp.rl"
	{( sm->te) = ( sm->p)+1;{ dstack_close_element(sm, INLINE_U); }}
	goto st1389;
tr1019:
#line 369 "ext/dtext/dtext.cpp.rl"
	{( sm->te) = ( sm->p)+1;{ dstack_open_element(sm,  INLINE_B, "<strong>"); }}
	goto st1389;
tr1020:
#line 429 "ext/dtext/dtext.cpp.rl"
	{( sm->te) = ( sm->p)+1;{
    if (sm->header_mode) {
      append_html_escaped(sm, "<br>");
    } else {
      append(sm, "<br>");
    };
  }}
	goto st1389;
tr1027:
#line 392 "ext/dtext/dtext.cpp.rl"
	{( sm->te) = ( sm->p)+1;{
    g_debug("inline [center]");
    dstack_open_element(sm, INLINE_CENTER, "<div class=\"center\">");
  }}
	goto st1389;
tr1037:
#line 442 "ext/dtext/dtext.cpp.rl"
	{{( sm->p) = ((( sm->te)))-1;}{
    append_inline_code(sm, { sm->a1, sm->a2 });
    {
  size_t len = sm->stack.size();

  if (len > MAX_STACK_DEPTH) {
    // Should never happen.
    throw DTextError("too many nested elements");
  }

  if (sm->top >= len) {
    g_debug("growing sm->stack %zi", len + 16);
    sm->stack.resize(len + 16, 0);
  }
{( (sm->stack.data()))[( sm->top)++] = 1389;goto st1720;}}
  }}
	goto st1389;
tr1038:
#line 442 "ext/dtext/dtext.cpp.rl"
	{( sm->te) = ( sm->p)+1;{
    append_inline_code(sm, { sm->a1, sm->a2 });
    {
  size_t len = sm->stack.size();

  if (len > MAX_STACK_DEPTH) {
    // Should never happen.
    throw DTextError("too many nested elements");
  }

  if (sm->top >= len) {
    g_debug("growing sm->stack %zi", len + 16);
    sm->stack.resize(len + 16, 0);
  }
{( (sm->stack.data()))[( sm->top)++] = 1389;goto st1720;}}
  }}
	goto st1389;
tr1040:
#line 437 "ext/dtext/dtext.cpp.rl"
	{{( sm->p) = ((( sm->te)))-1;}{
    append_inline_code(sm);
    {
  size_t len = sm->stack.size();

  if (len > MAX_STACK_DEPTH) {
    // Should never happen.
    throw DTextError("too many nested elements");
  }

  if (sm->top >= len) {
    g_debug("growing sm->stack %zi", len + 16);
    sm->stack.resize(len + 16, 0);
  }
{( (sm->stack.data()))[( sm->top)++] = 1389;goto st1720;}}
  }}
	goto st1389;
tr1041:
#line 437 "ext/dtext/dtext.cpp.rl"
	{( sm->te) = ( sm->p)+1;{
    append_inline_code(sm);
    {
  size_t len = sm->stack.size();

  if (len > MAX_STACK_DEPTH) {
    // Should never happen.
    throw DTextError("too many nested elements");
  }

  if (sm->top >= len) {
    g_debug("growing sm->stack %zi", len + 16);
    sm->stack.resize(len + 16, 0);
  }
{( (sm->stack.data()))[( sm->top)++] = 1389;goto st1720;}}
  }}
	goto st1389;
tr1047:
#line 407 "ext/dtext/dtext.cpp.rl"
	{( sm->te) = ( sm->p)+1;{
    g_debug("inline [color]");
    dstack_open_element(sm, INLINE_COLOR, "<span style=\"color:#FF761C;\">");
  }}
	goto st1389;
tr1051:
#line 71 "ext/dtext/dtext.cpp.rl"
	{ sm->a1 = sm->p; }
#line 72 "ext/dtext/dtext.cpp.rl"
	{ sm->a2 = sm->p; }
#line 412 "ext/dtext/dtext.cpp.rl"
	{( sm->te) = ( sm->p)+1;{
    g_debug("inline [color=]");
    dstack_open_element(sm, INLINE_COLOR, "<span style=\"color:");
    append_html_escaped(sm, { sm->a1, sm->a2 });
    append(sm, "\">");
  }}
	goto st1389;
tr1053:
#line 72 "ext/dtext/dtext.cpp.rl"
	{ sm->a2 = sm->p; }
#line 412 "ext/dtext/dtext.cpp.rl"
	{( sm->te) = ( sm->p)+1;{
    g_debug("inline [color=]");
    dstack_open_element(sm, INLINE_COLOR, "<span style=\"color:");
    append_html_escaped(sm, { sm->a1, sm->a2 });
    append(sm, "\">");
  }}
	goto st1389;
tr1061:
#line 507 "ext/dtext/dtext.cpp.rl"
	{( sm->te) = ( sm->p)+1;{
    g_debug("inline [expand]");
    dstack_close_leaf_blocks(sm);
    {( sm->p) = (( sm->ts))-1;}
    { sm->cs = ( (sm->stack.data()))[--( sm->top)];goto _again;}
  }}
	goto st1389;
tr1065:
#line 71 "ext/dtext/dtext.cpp.rl"
	{ sm->a1 = sm->p; }
#line 72 "ext/dtext/dtext.cpp.rl"
	{ sm->a2 = sm->p; }
#line 507 "ext/dtext/dtext.cpp.rl"
	{( sm->te) = ( sm->p)+1;{
    g_debug("inline [expand]");
    dstack_close_leaf_blocks(sm);
    {( sm->p) = (( sm->ts))-1;}
    { sm->cs = ( (sm->stack.data()))[--( sm->top)];goto _again;}
  }}
	goto st1389;
tr1067:
#line 72 "ext/dtext/dtext.cpp.rl"
	{ sm->a2 = sm->p; }
#line 507 "ext/dtext/dtext.cpp.rl"
	{( sm->te) = ( sm->p)+1;{
    g_debug("inline [expand]");
    dstack_close_leaf_blocks(sm);
    {( sm->p) = (( sm->ts))-1;}
    { sm->cs = ( (sm->stack.data()))[--( sm->top)];goto _again;}
  }}
	goto st1389;
tr1080:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ sm->b2 = sm->p; }
#line 342 "ext/dtext/dtext.cpp.rl"
	{( sm->te) = ( sm->p)+1;{
    append_named_url(sm, { sm->a1, sm->a2 }, { sm->b1, sm->b2 });
  }}
	goto st1389;
tr1081:
#line 371 "ext/dtext/dtext.cpp.rl"
	{( sm->te) = ( sm->p)+1;{ dstack_open_element(sm,  INLINE_I, "<em>"); }}
	goto st1389;
tr1089:
#line 474 "ext/dtext/dtext.cpp.rl"
	{{( sm->p) = ((( sm->te)))-1;}{
    dstack_open_element(sm, INLINE_NODTEXT, "");
    {
  size_t len = sm->stack.size();

  if (len > MAX_STACK_DEPTH) {
    // Should never happen.
    throw DTextError("too many nested elements");
  }

  if (sm->top >= len) {
    g_debug("growing sm->stack %zi", len + 16);
    sm->stack.resize(len + 16, 0);
  }
{( (sm->stack.data()))[( sm->top)++] = 1389;goto st1724;}}
  }}
	goto st1389;
tr1090:
#line 474 "ext/dtext/dtext.cpp.rl"
	{( sm->te) = ( sm->p)+1;{
    dstack_open_element(sm, INLINE_NODTEXT, "");
    {
  size_t len = sm->stack.size();

  if (len > MAX_STACK_DEPTH) {
    // Should never happen.
    throw DTextError("too many nested elements");
  }

  if (sm->top >= len) {
    g_debug("growing sm->stack %zi", len + 16);
    sm->stack.resize(len + 16, 0);
  }
{( (sm->stack.data()))[( sm->top)++] = 1389;goto st1724;}}
  }}
	goto st1389;
tr1096:
#line 494 "ext/dtext/dtext.cpp.rl"
	{( sm->te) = ( sm->p)+1;{
    g_debug("inline [quote]");
    dstack_close_leaf_blocks(sm);
    {( sm->p) = (( sm->ts))-1;}
    { sm->cs = ( (sm->stack.data()))[--( sm->top)];goto _again;}
  }}
	goto st1389;
tr1098:
#line 373 "ext/dtext/dtext.cpp.rl"
	{( sm->te) = ( sm->p)+1;{ dstack_open_element(sm,  INLINE_S, "<s>"); }}
	goto st1389;
tr1105:
#line 459 "ext/dtext/dtext.cpp.rl"
	{( sm->te) = ( sm->p)+1;{
    dstack_open_element(sm, INLINE_SPOILER, "<span class=\"spoiler\">");
  }}
	goto st1389;
tr1107:
#line 378 "ext/dtext/dtext.cpp.rl"
	{( sm->te) = ( sm->p)+1;{
    dstack_open_element(sm, INLINE_TN, "<span class=\"tn\">");
  }}
	goto st1389;
tr1109:
#line 375 "ext/dtext/dtext.cpp.rl"
	{( sm->te) = ( sm->p)+1;{ dstack_open_element(sm,  INLINE_U, "<u>"); }}
	goto st1389;
tr1135:
#line 338 "ext/dtext/dtext.cpp.rl"
	{( sm->te) = ( sm->p)+1;{
    append_named_url(sm, { sm->b1, sm->b2 }, { sm->a1, sm->a2 });
  }}
	goto st1389;
tr1173:
#line 350 "ext/dtext/dtext.cpp.rl"
	{( sm->te) = ( sm->p)+1;{
    append_unnamed_url(sm, { sm->a1, sm->a2 });
  }}
	goto st1389;
tr1212:
#line 342 "ext/dtext/dtext.cpp.rl"
	{( sm->te) = ( sm->p)+1;{
    append_named_url(sm, { sm->a1, sm->a2 }, { sm->b1, sm->b2 });
  }}
	goto st1389;
tr1271:
#line 72 "ext/dtext/dtext.cpp.rl"
	{ sm->a2 = sm->p; }
#line 350 "ext/dtext/dtext.cpp.rl"
	{( sm->te) = ( sm->p)+1;{
    append_unnamed_url(sm, { sm->a1, sm->a2 });
  }}
	goto st1389;
tr1293:
#line 72 "ext/dtext/dtext.cpp.rl"
	{ sm->a2 = sm->p; }
#line 358 "ext/dtext/dtext.cpp.rl"
	{( sm->te) = ( sm->p)+1;{
    g_debug("delimited mention: <@%.*s>", (int)(sm->a2 - sm->a1), sm->a1);
    append_mention(sm, { sm->a1, sm->a2 });
  }}
	goto st1389;
tr1766:
#line 587 "ext/dtext/dtext.cpp.rl"
	{( sm->te) = ( sm->p)+1;{
    append_html_escaped(sm, (*( sm->p)));
  }}
	goto st1389;
tr1773:
#line 577 "ext/dtext/dtext.cpp.rl"
	{( sm->te) = ( sm->p)+1;{
    append(sm, ' ');
  }}
	goto st1389;
tr1795:
#line 587 "ext/dtext/dtext.cpp.rl"
	{( sm->te) = ( sm->p);( sm->p)--;{
    append_html_escaped(sm, (*( sm->p)));
  }}
	goto st1389;
tr1796:
#line 583 "ext/dtext/dtext.cpp.rl"
	{( sm->te) = ( sm->p);( sm->p)--;{
    append(sm, std::string_view { sm->ts, sm->te });
  }}
	goto st1389;
tr1798:
#line 563 "ext/dtext/dtext.cpp.rl"
	{( sm->te) = ( sm->p);( sm->p)--;{
    g_debug("inline newline");

    if (sm->header_mode) {
      dstack_close_leaf_blocks(sm);
      { sm->cs = ( (sm->stack.data()))[--( sm->top)];goto _again;}
    } else if (dstack_is_open(sm, BLOCK_UL)) {
      dstack_close_list(sm);
      { sm->cs = ( (sm->stack.data()))[--( sm->top)];goto _again;}
    } else {
      append(sm, "<br>");
    }
  }}
	goto st1389;
tr1805:
#line 538 "ext/dtext/dtext.cpp.rl"
	{( sm->te) = ( sm->p);( sm->p)--;{
    g_debug("inline [hr] (pos: %ld)", sm->ts - sm->pb);
    dstack_close_leaf_blocks(sm);
    {( sm->p) = (( sm->ts))-1;}
    { sm->cs = ( (sm->stack.data()))[--( sm->top)];goto _again;}
  }}
	goto st1389;
tr1806:
#line 545 "ext/dtext/dtext.cpp.rl"
	{( sm->te) = ( sm->p);( sm->p)--;{
    g_debug("inline newline2");

    if (dstack_check(sm, BLOCK_P)) {
      dstack_rewind(sm);
    } else if (sm->header_mode) {
      dstack_close_leaf_blocks(sm);
    } else {
      dstack_close_list(sm);
    }

    if (sm->options.f_inline) {
      append(sm, " ");
    }

    { sm->cs = ( (sm->stack.data()))[--( sm->top)];goto _again;}
  }}
	goto st1389;
tr1809:
#line 397 "ext/dtext/dtext.cpp.rl"
	{( sm->te) = ( sm->p);( sm->p)--;{
    g_debug("inline [/center]");

    if (dstack_check(sm, INLINE_CENTER)) {
      dstack_close_element(sm, INLINE_CENTER);
    } else if (dstack_close_element(sm, BLOCK_CENTER)) {
      { sm->cs = ( (sm->stack.data()))[--( sm->top)];goto _again;}
    }
  }}
	goto st1389;
tr1810:
#line 397 "ext/dtext/dtext.cpp.rl"
	{( sm->te) = ( sm->p)+1;{
    g_debug("inline [/center]");

    if (dstack_check(sm, INLINE_CENTER)) {
      dstack_close_element(sm, INLINE_CENTER);
    } else if (dstack_close_element(sm, BLOCK_CENTER)) {
      { sm->cs = ( (sm->stack.data()))[--( sm->top)];goto _again;}
    }
  }}
	goto st1389;
tr1811:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ sm->b2 = sm->p; }
#line 363 "ext/dtext/dtext.cpp.rl"
	{( sm->te) = ( sm->p);( sm->p)--;{
    g_debug("inline list");
    {( sm->p) = (( sm->ts + 1))-1;}
    { sm->cs = ( (sm->stack.data()))[--( sm->top)];goto _again;}
  }}
	goto st1389;
tr1813:
#line 501 "ext/dtext/dtext.cpp.rl"
	{( sm->te) = ( sm->p);( sm->p)--;{
    g_debug("inline [/quote]");
    dstack_close_until(sm, BLOCK_QUOTE);
    { sm->cs = ( (sm->stack.data()))[--( sm->top)];goto _again;}
  }}
	goto st1389;
tr1814:
#line 514 "ext/dtext/dtext.cpp.rl"
	{( sm->te) = ( sm->p);( sm->p)--;{
    g_debug("inline [/expand]");
    dstack_close_until(sm, BLOCK_EXPAND);
    { sm->cs = ( (sm->stack.data()))[--( sm->top)];goto _again;}
  }}
	goto st1389;
tr1815:
#line 488 "ext/dtext/dtext.cpp.rl"
	{( sm->te) = ( sm->p);( sm->p)--;{
    dstack_close_leaf_blocks(sm);
    {( sm->p) = (( sm->ts))-1;}
    { sm->cs = ( (sm->stack.data()))[--( sm->top)];goto _again;}
  }}
	goto st1389;
tr1818:
#line 334 "ext/dtext/dtext.cpp.rl"
	{( sm->te) = ( sm->p);( sm->p)--;{
    append_bare_named_url(sm, { sm->b1, sm->b2 + 1 }, { sm->a1, sm->a2 });
  }}
	goto st1389;
tr1822:
#line 79 "ext/dtext/dtext.cpp.rl"
	{ sm->e1 = sm->p; }
#line 80 "ext/dtext/dtext.cpp.rl"
	{ sm->e2 = sm->p; }
#line 326 "ext/dtext/dtext.cpp.rl"
	{( sm->te) = ( sm->p);( sm->p)--;{
    append_wiki_link(sm, { sm->a1, sm->a2 }, { sm->b1, sm->b2 }, { sm->c1, sm->c2 }, { sm->b1, sm->b2 }, { sm->e1, sm->e2 });
  }}
	goto st1389;
tr1824:
#line 80 "ext/dtext/dtext.cpp.rl"
	{ sm->e2 = sm->p; }
#line 326 "ext/dtext/dtext.cpp.rl"
	{( sm->te) = ( sm->p);( sm->p)--;{
    append_wiki_link(sm, { sm->a1, sm->a2 }, { sm->b1, sm->b2 }, { sm->c1, sm->c2 }, { sm->b1, sm->b2 }, { sm->e1, sm->e2 });
  }}
	goto st1389;
tr1826:
#line 79 "ext/dtext/dtext.cpp.rl"
	{ sm->e1 = sm->p; }
#line 80 "ext/dtext/dtext.cpp.rl"
	{ sm->e2 = sm->p; }
#line 330 "ext/dtext/dtext.cpp.rl"
	{( sm->te) = ( sm->p);( sm->p)--;{
    append_wiki_link(sm, { sm->a1, sm->a2 }, { sm->b1, sm->b2 }, { sm->c1, sm->c2 }, { sm->d1, sm->d2 }, { sm->e1, sm->e2 });
  }}
	goto st1389;
tr1828:
#line 80 "ext/dtext/dtext.cpp.rl"
	{ sm->e2 = sm->p; }
#line 330 "ext/dtext/dtext.cpp.rl"
	{( sm->te) = ( sm->p);( sm->p)--;{
    append_wiki_link(sm, { sm->a1, sm->a2 }, { sm->b1, sm->b2 }, { sm->c1, sm->c2 }, { sm->d1, sm->d2 }, { sm->e1, sm->e2 });
  }}
	goto st1389;
tr1832:
#line 77 "ext/dtext/dtext.cpp.rl"
	{ sm->d1 = sm->p; }
#line 78 "ext/dtext/dtext.cpp.rl"
	{ sm->d2 = sm->p; }
#line 322 "ext/dtext/dtext.cpp.rl"
	{( sm->te) = ( sm->p);( sm->p)--;{
    append_post_search_link(sm, { sm->a1, sm->a2 }, { sm->b1, sm->b2 }, { sm->c1, sm->c2 }, { sm->d1, sm->d2 });
  }}
	goto st1389;
tr1834:
#line 78 "ext/dtext/dtext.cpp.rl"
	{ sm->d2 = sm->p; }
#line 322 "ext/dtext/dtext.cpp.rl"
	{( sm->te) = ( sm->p);( sm->p)--;{
    append_post_search_link(sm, { sm->a1, sm->a2 }, { sm->b1, sm->b2 }, { sm->c1, sm->c2 }, { sm->d1, sm->d2 });
  }}
	goto st1389;
tr1836:
#line 77 "ext/dtext/dtext.cpp.rl"
	{ sm->d1 = sm->p; }
#line 78 "ext/dtext/dtext.cpp.rl"
	{ sm->d2 = sm->p; }
#line 318 "ext/dtext/dtext.cpp.rl"
	{( sm->te) = ( sm->p);( sm->p)--;{
    append_post_search_link(sm, { sm->a1, sm->a2 }, { sm->b1, sm->b2 }, { sm->b1, sm->b2 }, { sm->d1, sm->d2 });
  }}
	goto st1389;
tr1838:
#line 78 "ext/dtext/dtext.cpp.rl"
	{ sm->d2 = sm->p; }
#line 318 "ext/dtext/dtext.cpp.rl"
	{( sm->te) = ( sm->p);( sm->p)--;{
    append_post_search_link(sm, { sm->a1, sm->a2 }, { sm->b1, sm->b2 }, { sm->b1, sm->b2 }, { sm->d1, sm->d2 });
  }}
	goto st1389;
tr1849:
#line 72 "ext/dtext/dtext.cpp.rl"
	{ sm->a2 = sm->p; }
#line 302 "ext/dtext/dtext.cpp.rl"
	{( sm->te) = ( sm->p);( sm->p)--;{ append_id_link(sm, "artist", "artist", "/artists/", { sm->a1, sm->a2 }); }}
	goto st1389;
tr1864:
#line 72 "ext/dtext/dtext.cpp.rl"
	{ sm->a2 = sm->p; }
#line 307 "ext/dtext/dtext.cpp.rl"
	{( sm->te) = ( sm->p);( sm->p)--;{ append_id_link(sm, "book", "book", "https://beta.sankakucomplex.com/books/", { sm->a1, sm->a2 }); }}
	goto st1389;
tr1882:
#line 72 "ext/dtext/dtext.cpp.rl"
	{ sm->a2 = sm->p; }
#line 298 "ext/dtext/dtext.cpp.rl"
	{( sm->te) = ( sm->p);( sm->p)--;{ append_id_link(sm, "comment", "comment", "/comments/", { sm->a1, sm->a2 }); }}
	goto st1389;
tr1898:
#line 72 "ext/dtext/dtext.cpp.rl"
	{ sm->a2 = sm->p; }
#line 299 "ext/dtext/dtext.cpp.rl"
	{( sm->te) = ( sm->p);( sm->p)--;{ append_id_link(sm, "dmail", "dmail", "/dmails/", { sm->a1, sm->a2 }); }}
	goto st1389;
tr1901:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ sm->b2 = sm->p; }
#line 313 "ext/dtext/dtext.cpp.rl"
	{( sm->te) = ( sm->p);( sm->p)--;{ append_dmail_key_link(sm); }}
	goto st1389;
tr1917:
#line 72 "ext/dtext/dtext.cpp.rl"
	{ sm->a2 = sm->p; }
#line 296 "ext/dtext/dtext.cpp.rl"
	{( sm->te) = ( sm->p);( sm->p)--;{ append_id_link(sm, "forum", "forum-post", "/forums/", { sm->a1, sm->a2 }); }}
	goto st1389;
tr1933:
#line 346 "ext/dtext/dtext.cpp.rl"
	{( sm->te) = ( sm->p);( sm->p)--;{
    append_bare_unnamed_url(sm, { sm->ts, sm->te });
  }}
	goto st1389;
tr1937:
#line 72 "ext/dtext/dtext.cpp.rl"
	{ sm->a2 = sm->p; }
#line 309 "ext/dtext/dtext.cpp.rl"
	{( sm->te) = ( sm->p);( sm->p)--;{ append_id_link(sm, "mod action", "mod-action", "/mod_actions?id=", { sm->a1, sm->a2 }); }}
	goto st1389;
tr1954:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ sm->b2 = sm->p; }
#line 316 "ext/dtext/dtext.cpp.rl"
	{( sm->te) = ( sm->p);( sm->p)--;{ append_paged_link(sm, "pixiv #", "<a rel=\"external nofollow noreferrer\" class=\"dtext-link dtext-id-link dtext-pixiv-id-link\" href=\"", "https://www.pixiv.net/artworks/", "#"); }}
	goto st1389;
tr1960:
#line 72 "ext/dtext/dtext.cpp.rl"
	{ sm->a2 = sm->p; }
#line 300 "ext/dtext/dtext.cpp.rl"
	{( sm->te) = ( sm->p);( sm->p)--;{ append_id_link(sm, "pool", "pool", "/pools/", { sm->a1, sm->a2 }); }}
	goto st1389;
tr1973:
#line 72 "ext/dtext/dtext.cpp.rl"
	{ sm->a2 = sm->p; }
#line 295 "ext/dtext/dtext.cpp.rl"
	{( sm->te) = ( sm->p);( sm->p)--;{ append_id_link(sm, "post", "post", "/posts/", { sm->a1, sm->a2 }); }}
	goto st1389;
tr1990:
#line 72 "ext/dtext/dtext.cpp.rl"
	{ sm->a2 = sm->p; }
#line 310 "ext/dtext/dtext.cpp.rl"
	{( sm->te) = ( sm->p);( sm->p)--;{ append_id_link(sm, "record", "user-record", "/user_records?id=", { sm->a1, sm->a2 }); }}
	goto st1389;
tr2007:
#line 72 "ext/dtext/dtext.cpp.rl"
	{ sm->a2 = sm->p; }
#line 308 "ext/dtext/dtext.cpp.rl"
	{( sm->te) = ( sm->p);( sm->p)--;{ append_id_link(sm, "series", "series", "https://beta.sankakucomplex.com/series/", { sm->a1, sm->a2 }); }}
	goto st1389;
tr2022:
#line 72 "ext/dtext/dtext.cpp.rl"
	{ sm->a2 = sm->p; }
#line 304 "ext/dtext/dtext.cpp.rl"
	{( sm->te) = ( sm->p);( sm->p)--;{ append_id_link(sm, "tag alias", "tag-alias", "https://beta.sankakucomplex.com/tag_aliases?id[0]=", { sm->a1, sm->a2 }); }}
	goto st1389;
tr2033:
#line 72 "ext/dtext/dtext.cpp.rl"
	{ sm->a2 = sm->p; }
#line 305 "ext/dtext/dtext.cpp.rl"
	{( sm->te) = ( sm->p);( sm->p)--;{ append_id_link(sm, "tag implication", "tag-implication", "https://beta.sankakucomplex.com/tag_implications?id[0]=", { sm->a1, sm->a2 }); }}
	goto st1389;
tr2044:
#line 72 "ext/dtext/dtext.cpp.rl"
	{ sm->a2 = sm->p; }
#line 306 "ext/dtext/dtext.cpp.rl"
	{( sm->te) = ( sm->p);( sm->p)--;{ append_id_link(sm, "tag translation", "tag-translation", "https://beta.sankakucomplex.com/tag_translations?id[0]=", { sm->a1, sm->a2 }); }}
	goto st1389;
tr2059:
#line 72 "ext/dtext/dtext.cpp.rl"
	{ sm->a2 = sm->p; }
#line 297 "ext/dtext/dtext.cpp.rl"
	{( sm->te) = ( sm->p);( sm->p)--;{ append_id_link(sm, "topic", "forum-topic", "/forums/", { sm->a1, sm->a2 }); }}
	goto st1389;
tr2062:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ sm->b2 = sm->p; }
#line 315 "ext/dtext/dtext.cpp.rl"
	{( sm->te) = ( sm->p);( sm->p)--;{ append_paged_link(sm, "topic #", "<a class=\"dtext-link dtext-id-link dtext-forum-topic-id-link\" href=\"", "/forums/", "?page="); }}
	goto st1389;
tr2077:
#line 72 "ext/dtext/dtext.cpp.rl"
	{ sm->a2 = sm->p; }
#line 301 "ext/dtext/dtext.cpp.rl"
	{( sm->te) = ( sm->p);( sm->p)--;{ append_id_link(sm, "user", "user", "/users/", { sm->a1, sm->a2 }); }}
	goto st1389;
tr2088:
#line 72 "ext/dtext/dtext.cpp.rl"
	{ sm->a2 = sm->p; }
#line 303 "ext/dtext/dtext.cpp.rl"
	{( sm->te) = ( sm->p);( sm->p)--;{ append_id_link(sm, "user report", "user-report", "/user_flags/", { sm->a1, sm->a2 }); }}
	goto st1389;
tr2103:
#line 72 "ext/dtext/dtext.cpp.rl"
	{ sm->a2 = sm->p; }
#line 311 "ext/dtext/dtext.cpp.rl"
	{( sm->te) = ( sm->p);( sm->p)--;{ append_id_link(sm, "wiki", "wiki-page", "/wiki/", { sm->a1, sm->a2 }); }}
	goto st1389;
tr2125:
#line 442 "ext/dtext/dtext.cpp.rl"
	{( sm->te) = ( sm->p);( sm->p)--;{
    append_inline_code(sm, { sm->a1, sm->a2 });
    {
  size_t len = sm->stack.size();

  if (len > MAX_STACK_DEPTH) {
    // Should never happen.
    throw DTextError("too many nested elements");
  }

  if (sm->top >= len) {
    g_debug("growing sm->stack %zi", len + 16);
    sm->stack.resize(len + 16, 0);
  }
{( (sm->stack.data()))[( sm->top)++] = 1389;goto st1720;}}
  }}
	goto st1389;
tr2126:
#line 437 "ext/dtext/dtext.cpp.rl"
	{( sm->te) = ( sm->p);( sm->p)--;{
    append_inline_code(sm);
    {
  size_t len = sm->stack.size();

  if (len > MAX_STACK_DEPTH) {
    // Should never happen.
    throw DTextError("too many nested elements");
  }

  if (sm->top >= len) {
    g_debug("growing sm->stack %zi", len + 16);
    sm->stack.resize(len + 16, 0);
  }
{( (sm->stack.data()))[( sm->top)++] = 1389;goto st1720;}}
  }}
	goto st1389;
tr2127:
#line 474 "ext/dtext/dtext.cpp.rl"
	{( sm->te) = ( sm->p);( sm->p)--;{
    dstack_open_element(sm, INLINE_NODTEXT, "");
    {
  size_t len = sm->stack.size();

  if (len > MAX_STACK_DEPTH) {
    // Should never happen.
    throw DTextError("too many nested elements");
  }

  if (sm->top >= len) {
    g_debug("growing sm->stack %zi", len + 16);
    sm->stack.resize(len + 16, 0);
  }
{( (sm->stack.data()))[( sm->top)++] = 1389;goto st1724;}}
  }}
	goto st1389;
tr2147:
#line 354 "ext/dtext/dtext.cpp.rl"
	{( sm->te) = ( sm->p);( sm->p)--;{
    append_mention(sm, { sm->a1, sm->a2 + 1 });
  }}
	goto st1389;
st1389:
#line 1 "NONE"
	{( sm->ts) = 0;}
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1389;
case 1389:
#line 1 "NONE"
	{( sm->ts) = ( sm->p);}
#line 6188 "ext/dtext/dtext.cpp"
	_widec = (*( sm->p));
	if ( (*( sm->p)) > 60 ) {
		if ( 64 <= (*( sm->p)) && (*( sm->p)) <= 64 ) {
			_widec = (short)(1152 + ((*( sm->p)) - -128));
			if ( 
#line 82 "ext/dtext/dtext.cpp.rl"
 is_mention_boundary(p[-1])  ) _widec += 256;
			if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 512;
		}
	} else if ( (*( sm->p)) >= 60 ) {
		_widec = (short)(640 + ((*( sm->p)) - -128));
		if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
	}
	switch( _widec ) {
		case 0: goto tr1770;
		case 9: goto tr1771;
		case 10: goto tr1772;
		case 13: goto tr1773;
		case 32: goto tr1771;
		case 34: goto tr1774;
		case 65: goto tr1777;
		case 66: goto tr1778;
		case 67: goto tr1779;
		case 68: goto tr1780;
		case 70: goto tr1781;
		case 72: goto tr1782;
		case 77: goto tr1783;
		case 80: goto tr1784;
		case 82: goto tr1785;
		case 83: goto tr1786;
		case 84: goto tr1787;
		case 85: goto tr1788;
		case 87: goto tr1789;
		case 91: goto tr1790;
		case 97: goto tr1777;
		case 98: goto tr1778;
		case 99: goto tr1779;
		case 100: goto tr1780;
		case 102: goto tr1781;
		case 104: goto tr1782;
		case 109: goto tr1783;
		case 112: goto tr1784;
		case 114: goto tr1785;
		case 115: goto tr1786;
		case 116: goto tr1787;
		case 117: goto tr1788;
		case 119: goto tr1789;
		case 123: goto tr1791;
		case 828: goto tr1792;
		case 1084: goto tr1793;
		case 1344: goto tr1766;
		case 1600: goto tr1766;
		case 1856: goto tr1766;
		case 2112: goto tr1794;
	}
	if ( _widec < 48 ) {
		if ( _widec < -32 ) {
			if ( _widec > -63 ) {
				if ( -62 <= _widec && _widec <= -33 )
					goto st1390;
			} else
				goto tr1766;
		} else if ( _widec > -17 ) {
			if ( _widec > -12 ) {
				if ( -11 <= _widec && _widec <= 47 )
					goto tr1766;
			} else if ( _widec >= -16 )
				goto tr1769;
		} else
			goto tr1768;
	} else if ( _widec > 57 ) {
		if ( _widec < 69 ) {
			if ( _widec > 59 ) {
				if ( 61 <= _widec && _widec <= 63 )
					goto tr1766;
			} else if ( _widec >= 58 )
				goto tr1766;
		} else if ( _widec > 90 ) {
			if ( _widec < 101 ) {
				if ( 92 <= _widec && _widec <= 96 )
					goto tr1766;
			} else if ( _widec > 122 ) {
				if ( 124 <= _widec )
					goto tr1766;
			} else
				goto tr1775;
		} else
			goto tr1775;
	} else
		goto tr1775;
	goto st0;
st1390:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1390;
case 1390:
	if ( (*( sm->p)) <= -65 )
		goto tr208;
	goto tr1795;
tr208:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 583 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 80;}
	goto st1391;
st1391:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1391;
case 1391:
#line 6301 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < -32 ) {
		if ( -62 <= (*( sm->p)) && (*( sm->p)) <= -33 )
			goto st171;
	} else if ( (*( sm->p)) > -17 ) {
		if ( -16 <= (*( sm->p)) && (*( sm->p)) <= -12 )
			goto st173;
	} else
		goto st172;
	goto tr1796;
st171:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof171;
case 171:
	if ( (*( sm->p)) <= -65 )
		goto tr208;
	goto tr207;
st172:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof172;
case 172:
	if ( (*( sm->p)) <= -65 )
		goto st171;
	goto tr207;
st173:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof173;
case 173:
	if ( (*( sm->p)) <= -65 )
		goto st172;
	goto tr210;
tr1768:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 587 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 81;}
	goto st1392;
st1392:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1392;
case 1392:
#line 6342 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) <= -65 )
		goto st171;
	goto tr1795;
tr1769:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 587 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 81;}
	goto st1393;
st1393:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1393;
case 1393:
#line 6356 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) <= -65 )
		goto st172;
	goto tr1795;
tr212:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 545 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 76;}
	goto st1394;
tr1770:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 581 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 79;}
	goto st1394;
st1394:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1394;
case 1394:
#line 6376 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 0: goto tr212;
		case 9: goto st174;
		case 10: goto tr212;
		case 32: goto st174;
	}
	goto tr207;
st174:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof174;
case 174:
	switch( (*( sm->p)) ) {
		case 0: goto tr212;
		case 9: goto st174;
		case 10: goto tr212;
		case 32: goto st174;
	}
	goto tr207;
tr1771:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 587 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 81;}
	goto st1395;
st1395:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1395;
case 1395:
#line 6405 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 0: goto st174;
		case 9: goto st175;
		case 10: goto st174;
		case 32: goto st175;
	}
	goto tr1795;
st175:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof175;
case 175:
	switch( (*( sm->p)) ) {
		case 0: goto st174;
		case 9: goto st175;
		case 10: goto st174;
		case 32: goto st175;
	}
	goto tr214;
tr1772:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 563 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 77;}
	goto st1396;
st1396:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1396;
case 1396:
#line 6434 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 0: goto tr212;
		case 9: goto st176;
		case 10: goto tr1799;
		case 32: goto st176;
		case 42: goto tr1800;
		case 60: goto st245;
		case 72: goto st290;
		case 91: goto st294;
		case 96: goto st324;
		case 104: goto st290;
	}
	goto tr1798;
st176:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof176;
case 176:
	switch( (*( sm->p)) ) {
		case 0: goto tr212;
		case 9: goto st176;
		case 10: goto tr212;
		case 32: goto st176;
		case 60: goto st177;
		case 91: goto st195;
	}
	goto tr216;
st177:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof177;
case 177:
	switch( (*( sm->p)) ) {
		case 72: goto st178;
		case 83: goto st181;
		case 84: goto st190;
		case 104: goto st178;
		case 115: goto st181;
		case 116: goto st190;
	}
	goto tr216;
st178:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof178;
case 178:
	switch( (*( sm->p)) ) {
		case 82: goto st179;
		case 114: goto st179;
	}
	goto tr216;
st179:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof179;
case 179:
	if ( (*( sm->p)) == 62 )
		goto st180;
	goto tr216;
st180:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof180;
case 180:
	switch( (*( sm->p)) ) {
		case 0: goto st1397;
		case 9: goto st180;
		case 10: goto st1397;
		case 32: goto st180;
	}
	goto tr216;
st1397:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1397;
case 1397:
	switch( (*( sm->p)) ) {
		case 0: goto st1397;
		case 10: goto st1397;
	}
	goto tr1805;
st181:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof181;
case 181:
	switch( (*( sm->p)) ) {
		case 80: goto st182;
		case 112: goto st182;
	}
	goto tr216;
st182:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof182;
case 182:
	switch( (*( sm->p)) ) {
		case 79: goto st183;
		case 111: goto st183;
	}
	goto tr216;
st183:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof183;
case 183:
	switch( (*( sm->p)) ) {
		case 73: goto st184;
		case 105: goto st184;
	}
	goto tr216;
st184:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof184;
case 184:
	switch( (*( sm->p)) ) {
		case 76: goto st185;
		case 108: goto st185;
	}
	goto tr216;
st185:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof185;
case 185:
	switch( (*( sm->p)) ) {
		case 69: goto st186;
		case 101: goto st186;
	}
	goto tr216;
st186:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof186;
case 186:
	switch( (*( sm->p)) ) {
		case 82: goto st187;
		case 114: goto st187;
	}
	goto tr216;
st187:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof187;
case 187:
	switch( (*( sm->p)) ) {
		case 62: goto st188;
		case 83: goto st189;
		case 115: goto st189;
	}
	goto tr216;
st188:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof188;
case 188:
	switch( (*( sm->p)) ) {
		case 0: goto tr234;
		case 9: goto st188;
		case 10: goto tr234;
		case 32: goto st188;
	}
	goto tr216;
st189:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof189;
case 189:
	if ( (*( sm->p)) == 62 )
		goto st188;
	goto tr216;
st190:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof190;
case 190:
	switch( (*( sm->p)) ) {
		case 65: goto st191;
		case 97: goto st191;
	}
	goto tr216;
st191:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof191;
case 191:
	switch( (*( sm->p)) ) {
		case 66: goto st192;
		case 98: goto st192;
	}
	goto tr216;
st192:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof192;
case 192:
	switch( (*( sm->p)) ) {
		case 76: goto st193;
		case 108: goto st193;
	}
	goto tr216;
st193:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof193;
case 193:
	switch( (*( sm->p)) ) {
		case 69: goto st194;
		case 101: goto st194;
	}
	goto tr216;
st194:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof194;
case 194:
	if ( (*( sm->p)) == 62 )
		goto tr239;
	goto tr216;
st195:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof195;
case 195:
	switch( (*( sm->p)) ) {
		case 72: goto st196;
		case 83: goto st198;
		case 84: goto st206;
		case 104: goto st196;
		case 115: goto st198;
		case 116: goto st206;
	}
	goto tr216;
st196:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof196;
case 196:
	switch( (*( sm->p)) ) {
		case 82: goto st197;
		case 114: goto st197;
	}
	goto tr216;
st197:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof197;
case 197:
	if ( (*( sm->p)) == 93 )
		goto st180;
	goto tr216;
st198:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof198;
case 198:
	switch( (*( sm->p)) ) {
		case 80: goto st199;
		case 112: goto st199;
	}
	goto tr216;
st199:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof199;
case 199:
	switch( (*( sm->p)) ) {
		case 79: goto st200;
		case 111: goto st200;
	}
	goto tr216;
st200:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof200;
case 200:
	switch( (*( sm->p)) ) {
		case 73: goto st201;
		case 105: goto st201;
	}
	goto tr216;
st201:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof201;
case 201:
	switch( (*( sm->p)) ) {
		case 76: goto st202;
		case 108: goto st202;
	}
	goto tr216;
st202:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof202;
case 202:
	switch( (*( sm->p)) ) {
		case 69: goto st203;
		case 101: goto st203;
	}
	goto tr216;
st203:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof203;
case 203:
	switch( (*( sm->p)) ) {
		case 82: goto st204;
		case 114: goto st204;
	}
	goto tr216;
st204:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof204;
case 204:
	switch( (*( sm->p)) ) {
		case 83: goto st205;
		case 93: goto st188;
		case 115: goto st205;
	}
	goto tr216;
st205:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof205;
case 205:
	if ( (*( sm->p)) == 93 )
		goto st188;
	goto tr216;
st206:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof206;
case 206:
	switch( (*( sm->p)) ) {
		case 65: goto st207;
		case 97: goto st207;
	}
	goto tr216;
st207:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof207;
case 207:
	switch( (*( sm->p)) ) {
		case 66: goto st208;
		case 98: goto st208;
	}
	goto tr216;
st208:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof208;
case 208:
	switch( (*( sm->p)) ) {
		case 76: goto st209;
		case 108: goto st209;
	}
	goto tr216;
st209:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof209;
case 209:
	switch( (*( sm->p)) ) {
		case 69: goto st210;
		case 101: goto st210;
	}
	goto tr216;
st210:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof210;
case 210:
	if ( (*( sm->p)) == 93 )
		goto tr239;
	goto tr216;
tr1799:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 545 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 76;}
	goto st1398;
st1398:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1398;
case 1398:
#line 6788 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 0: goto tr212;
		case 9: goto st174;
		case 10: goto tr1799;
		case 32: goto st174;
		case 60: goto st211;
		case 91: goto st227;
	}
	goto tr1806;
st211:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof211;
case 211:
	if ( (*( sm->p)) == 47 )
		goto st212;
	goto tr255;
st212:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof212;
case 212:
	switch( (*( sm->p)) ) {
		case 67: goto st213;
		case 84: goto st223;
		case 99: goto st213;
		case 116: goto st223;
	}
	goto tr255;
st213:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof213;
case 213:
	switch( (*( sm->p)) ) {
		case 69: goto st214;
		case 79: goto st219;
		case 101: goto st214;
		case 111: goto st219;
	}
	goto tr207;
st214:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof214;
case 214:
	switch( (*( sm->p)) ) {
		case 78: goto st215;
		case 110: goto st215;
	}
	goto tr207;
st215:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof215;
case 215:
	switch( (*( sm->p)) ) {
		case 84: goto st216;
		case 116: goto st216;
	}
	goto tr207;
st216:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof216;
case 216:
	switch( (*( sm->p)) ) {
		case 69: goto st217;
		case 101: goto st217;
	}
	goto tr207;
st217:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof217;
case 217:
	switch( (*( sm->p)) ) {
		case 82: goto st218;
		case 114: goto st218;
	}
	goto tr207;
st218:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof218;
case 218:
	if ( (*( sm->p)) == 62 )
		goto st1399;
	goto tr207;
st1399:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1399;
case 1399:
	if ( (*( sm->p)) == 10 )
		goto tr1810;
	goto tr1809;
st219:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof219;
case 219:
	switch( (*( sm->p)) ) {
		case 76: goto st220;
		case 108: goto st220;
	}
	goto tr207;
st220:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof220;
case 220:
	switch( (*( sm->p)) ) {
		case 79: goto st221;
		case 111: goto st221;
	}
	goto tr207;
st221:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof221;
case 221:
	switch( (*( sm->p)) ) {
		case 82: goto st222;
		case 114: goto st222;
	}
	goto tr207;
st222:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof222;
case 222:
	_widec = (*( sm->p));
	if ( 62 <= (*( sm->p)) && (*( sm->p)) <= 62 ) {
		_widec = (short)(2176 + ((*( sm->p)) - -128));
		if ( 
#line 86 "ext/dtext/dtext.cpp.rl"
 dstack_is_open(sm, BLOCK_COLOR)  ) _widec += 256;
	}
	switch( _widec ) {
		case 2366: goto tr269;
		case 2622: goto tr269;
	}
	goto tr207;
st223:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof223;
case 223:
	switch( (*( sm->p)) ) {
		case 68: goto st224;
		case 72: goto st225;
		case 78: goto st226;
		case 100: goto st224;
		case 104: goto st225;
		case 110: goto st226;
	}
	goto tr207;
st224:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof224;
case 224:
	if ( (*( sm->p)) == 62 )
		goto tr273;
	goto tr207;
st225:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof225;
case 225:
	if ( (*( sm->p)) == 62 )
		goto tr274;
	goto tr207;
st226:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof226;
case 226:
	if ( (*( sm->p)) == 62 )
		goto tr275;
	goto tr207;
st227:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof227;
case 227:
	if ( (*( sm->p)) == 47 )
		goto st228;
	goto tr255;
st228:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof228;
case 228:
	switch( (*( sm->p)) ) {
		case 67: goto st229;
		case 84: goto st239;
		case 99: goto st229;
		case 116: goto st239;
	}
	goto tr255;
st229:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof229;
case 229:
	switch( (*( sm->p)) ) {
		case 69: goto st230;
		case 79: goto st235;
		case 101: goto st230;
		case 111: goto st235;
	}
	goto tr207;
st230:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof230;
case 230:
	switch( (*( sm->p)) ) {
		case 78: goto st231;
		case 110: goto st231;
	}
	goto tr207;
st231:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof231;
case 231:
	switch( (*( sm->p)) ) {
		case 84: goto st232;
		case 116: goto st232;
	}
	goto tr207;
st232:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof232;
case 232:
	switch( (*( sm->p)) ) {
		case 69: goto st233;
		case 101: goto st233;
	}
	goto tr207;
st233:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof233;
case 233:
	switch( (*( sm->p)) ) {
		case 82: goto st234;
		case 114: goto st234;
	}
	goto tr207;
st234:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof234;
case 234:
	if ( (*( sm->p)) == 93 )
		goto st1399;
	goto tr207;
st235:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof235;
case 235:
	switch( (*( sm->p)) ) {
		case 76: goto st236;
		case 108: goto st236;
	}
	goto tr207;
st236:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof236;
case 236:
	switch( (*( sm->p)) ) {
		case 79: goto st237;
		case 111: goto st237;
	}
	goto tr207;
st237:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof237;
case 237:
	switch( (*( sm->p)) ) {
		case 82: goto st238;
		case 114: goto st238;
	}
	goto tr207;
st238:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof238;
case 238:
	_widec = (*( sm->p));
	if ( 93 <= (*( sm->p)) && (*( sm->p)) <= 93 ) {
		_widec = (short)(2176 + ((*( sm->p)) - -128));
		if ( 
#line 86 "ext/dtext/dtext.cpp.rl"
 dstack_is_open(sm, BLOCK_COLOR)  ) _widec += 256;
	}
	switch( _widec ) {
		case 2397: goto tr269;
		case 2653: goto tr269;
	}
	goto tr207;
st239:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof239;
case 239:
	switch( (*( sm->p)) ) {
		case 68: goto st240;
		case 72: goto st241;
		case 78: goto st242;
		case 100: goto st240;
		case 104: goto st241;
		case 110: goto st242;
	}
	goto tr207;
st240:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof240;
case 240:
	if ( (*( sm->p)) == 93 )
		goto tr273;
	goto tr207;
st241:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof241;
case 241:
	if ( (*( sm->p)) == 93 )
		goto tr274;
	goto tr207;
st242:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof242;
case 242:
	if ( (*( sm->p)) == 93 )
		goto tr275;
	goto tr207;
tr1800:
#line 71 "ext/dtext/dtext.cpp.rl"
	{ sm->a1 = sm->p; }
	goto st243;
st243:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof243;
case 243:
#line 7111 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 9: goto tr291;
		case 32: goto tr291;
		case 42: goto st243;
	}
	goto tr216;
tr291:
#line 72 "ext/dtext/dtext.cpp.rl"
	{ sm->a2 = sm->p; }
	goto st244;
st244:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof244;
case 244:
#line 7126 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 0: goto tr216;
		case 9: goto tr294;
		case 10: goto tr216;
		case 13: goto tr216;
		case 32: goto tr294;
	}
	goto tr293;
tr293:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ sm->b1 = sm->p; }
	goto st1400;
st1400:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1400;
case 1400:
#line 7143 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 0: goto tr1811;
		case 10: goto tr1811;
		case 13: goto tr1811;
	}
	goto st1400;
tr294:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ sm->b1 = sm->p; }
	goto st1401;
st1401:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1401;
case 1401:
#line 7158 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 0: goto tr1811;
		case 9: goto tr294;
		case 10: goto tr1811;
		case 13: goto tr1811;
		case 32: goto tr294;
	}
	goto tr293;
st245:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof245;
case 245:
	switch( (*( sm->p)) ) {
		case 47: goto st246;
		case 67: goto st276;
		case 72: goto st178;
		case 78: goto st283;
		case 83: goto st181;
		case 84: goto st190;
		case 99: goto st276;
		case 104: goto st178;
		case 110: goto st283;
		case 115: goto st181;
		case 116: goto st190;
	}
	goto tr216;
st246:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof246;
case 246:
	switch( (*( sm->p)) ) {
		case 66: goto st247;
		case 67: goto st213;
		case 69: goto st257;
		case 81: goto st263;
		case 83: goto st268;
		case 84: goto st223;
		case 98: goto st247;
		case 99: goto st213;
		case 101: goto st257;
		case 113: goto st263;
		case 115: goto st268;
		case 116: goto st223;
	}
	goto tr216;
st247:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof247;
case 247:
	switch( (*( sm->p)) ) {
		case 76: goto st248;
		case 108: goto st248;
	}
	goto tr216;
st248:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof248;
case 248:
	switch( (*( sm->p)) ) {
		case 79: goto st249;
		case 111: goto st249;
	}
	goto tr207;
st249:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof249;
case 249:
	switch( (*( sm->p)) ) {
		case 67: goto st250;
		case 99: goto st250;
	}
	goto tr207;
st250:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof250;
case 250:
	switch( (*( sm->p)) ) {
		case 75: goto st251;
		case 107: goto st251;
	}
	goto tr207;
st251:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof251;
case 251:
	switch( (*( sm->p)) ) {
		case 81: goto st252;
		case 113: goto st252;
	}
	goto tr207;
st252:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof252;
case 252:
	switch( (*( sm->p)) ) {
		case 85: goto st253;
		case 117: goto st253;
	}
	goto tr207;
st253:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof253;
case 253:
	switch( (*( sm->p)) ) {
		case 79: goto st254;
		case 111: goto st254;
	}
	goto tr207;
st254:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof254;
case 254:
	switch( (*( sm->p)) ) {
		case 84: goto st255;
		case 116: goto st255;
	}
	goto tr207;
st255:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof255;
case 255:
	switch( (*( sm->p)) ) {
		case 69: goto st256;
		case 101: goto st256;
	}
	goto tr207;
st256:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof256;
case 256:
	_widec = (*( sm->p));
	if ( 93 <= (*( sm->p)) && (*( sm->p)) <= 93 ) {
		_widec = (short)(2688 + ((*( sm->p)) - -128));
		if ( 
#line 84 "ext/dtext/dtext.cpp.rl"
 dstack_is_open(sm, BLOCK_QUOTE)  ) _widec += 256;
	}
	if ( _widec == 3165 )
		goto st1402;
	goto tr207;
st1402:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1402;
case 1402:
	switch( (*( sm->p)) ) {
		case 9: goto st1402;
		case 32: goto st1402;
	}
	goto tr1813;
st257:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof257;
case 257:
	switch( (*( sm->p)) ) {
		case 88: goto st258;
		case 120: goto st258;
	}
	goto tr216;
st258:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof258;
case 258:
	switch( (*( sm->p)) ) {
		case 80: goto st259;
		case 112: goto st259;
	}
	goto tr207;
st259:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof259;
case 259:
	switch( (*( sm->p)) ) {
		case 65: goto st260;
		case 97: goto st260;
	}
	goto tr207;
st260:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof260;
case 260:
	switch( (*( sm->p)) ) {
		case 78: goto st261;
		case 110: goto st261;
	}
	goto tr207;
st261:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof261;
case 261:
	switch( (*( sm->p)) ) {
		case 68: goto st262;
		case 100: goto st262;
	}
	goto tr207;
st262:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof262;
case 262:
	_widec = (*( sm->p));
	if ( 62 <= (*( sm->p)) && (*( sm->p)) <= 62 ) {
		_widec = (short)(3200 + ((*( sm->p)) - -128));
		if ( 
#line 85 "ext/dtext/dtext.cpp.rl"
 dstack_is_open(sm, BLOCK_EXPAND)  ) _widec += 256;
	}
	if ( _widec == 3646 )
		goto st1403;
	goto tr207;
st1403:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1403;
case 1403:
	switch( (*( sm->p)) ) {
		case 9: goto st1403;
		case 32: goto st1403;
	}
	goto tr1814;
st263:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof263;
case 263:
	switch( (*( sm->p)) ) {
		case 85: goto st264;
		case 117: goto st264;
	}
	goto tr207;
st264:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof264;
case 264:
	switch( (*( sm->p)) ) {
		case 79: goto st265;
		case 111: goto st265;
	}
	goto tr207;
st265:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof265;
case 265:
	switch( (*( sm->p)) ) {
		case 84: goto st266;
		case 116: goto st266;
	}
	goto tr207;
st266:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof266;
case 266:
	switch( (*( sm->p)) ) {
		case 69: goto st267;
		case 101: goto st267;
	}
	goto tr207;
st267:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof267;
case 267:
	_widec = (*( sm->p));
	if ( 62 <= (*( sm->p)) && (*( sm->p)) <= 62 ) {
		_widec = (short)(2688 + ((*( sm->p)) - -128));
		if ( 
#line 84 "ext/dtext/dtext.cpp.rl"
 dstack_is_open(sm, BLOCK_QUOTE)  ) _widec += 256;
	}
	if ( _widec == 3134 )
		goto st1402;
	goto tr207;
st268:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof268;
case 268:
	switch( (*( sm->p)) ) {
		case 80: goto st269;
		case 112: goto st269;
	}
	goto tr216;
st269:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof269;
case 269:
	switch( (*( sm->p)) ) {
		case 79: goto st270;
		case 111: goto st270;
	}
	goto tr207;
st270:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof270;
case 270:
	switch( (*( sm->p)) ) {
		case 73: goto st271;
		case 105: goto st271;
	}
	goto tr207;
st271:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof271;
case 271:
	switch( (*( sm->p)) ) {
		case 76: goto st272;
		case 108: goto st272;
	}
	goto tr207;
st272:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof272;
case 272:
	switch( (*( sm->p)) ) {
		case 69: goto st273;
		case 101: goto st273;
	}
	goto tr207;
st273:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof273;
case 273:
	switch( (*( sm->p)) ) {
		case 82: goto st274;
		case 114: goto st274;
	}
	goto tr207;
st274:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof274;
case 274:
	switch( (*( sm->p)) ) {
		case 62: goto tr328;
		case 83: goto st275;
		case 115: goto st275;
	}
	goto tr207;
st275:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof275;
case 275:
	if ( (*( sm->p)) == 62 )
		goto tr328;
	goto tr207;
st276:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof276;
case 276:
	switch( (*( sm->p)) ) {
		case 79: goto st277;
		case 111: goto st277;
	}
	goto tr216;
st277:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof277;
case 277:
	switch( (*( sm->p)) ) {
		case 68: goto st278;
		case 100: goto st278;
	}
	goto tr216;
st278:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof278;
case 278:
	switch( (*( sm->p)) ) {
		case 69: goto st279;
		case 101: goto st279;
	}
	goto tr216;
st279:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof279;
case 279:
	switch( (*( sm->p)) ) {
		case 9: goto st280;
		case 32: goto st280;
		case 61: goto st281;
		case 62: goto tr335;
	}
	goto tr216;
st280:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof280;
case 280:
	switch( (*( sm->p)) ) {
		case 9: goto st280;
		case 32: goto st280;
		case 61: goto st281;
	}
	goto tr216;
st281:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof281;
case 281:
	switch( (*( sm->p)) ) {
		case 9: goto st281;
		case 32: goto st281;
	}
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr336;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto tr336;
	} else
		goto tr336;
	goto tr216;
tr336:
#line 71 "ext/dtext/dtext.cpp.rl"
	{ sm->a1 = sm->p; }
	goto st282;
st282:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof282;
case 282:
#line 7570 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) == 62 )
		goto tr338;
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st282;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st282;
	} else
		goto st282;
	goto tr216;
st283:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof283;
case 283:
	switch( (*( sm->p)) ) {
		case 79: goto st284;
		case 111: goto st284;
	}
	goto tr216;
st284:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof284;
case 284:
	switch( (*( sm->p)) ) {
		case 68: goto st285;
		case 100: goto st285;
	}
	goto tr216;
st285:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof285;
case 285:
	switch( (*( sm->p)) ) {
		case 84: goto st286;
		case 116: goto st286;
	}
	goto tr216;
st286:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof286;
case 286:
	switch( (*( sm->p)) ) {
		case 69: goto st287;
		case 101: goto st287;
	}
	goto tr216;
st287:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof287;
case 287:
	switch( (*( sm->p)) ) {
		case 88: goto st288;
		case 120: goto st288;
	}
	goto tr216;
st288:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof288;
case 288:
	switch( (*( sm->p)) ) {
		case 84: goto st289;
		case 116: goto st289;
	}
	goto tr216;
st289:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof289;
case 289:
	if ( (*( sm->p)) == 62 )
		goto tr335;
	goto tr216;
st290:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof290;
case 290:
	if ( 49 <= (*( sm->p)) && (*( sm->p)) <= 54 )
		goto tr345;
	goto tr216;
tr345:
#line 71 "ext/dtext/dtext.cpp.rl"
	{ sm->a1 = sm->p; }
	goto st291;
st291:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof291;
case 291:
#line 7658 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 35: goto tr346;
		case 46: goto tr347;
	}
	goto tr216;
tr346:
#line 72 "ext/dtext/dtext.cpp.rl"
	{ sm->a2 = sm->p; }
	goto st292;
st292:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof292;
case 292:
#line 7672 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 33: goto tr348;
		case 35: goto tr348;
		case 38: goto tr348;
		case 45: goto tr348;
		case 95: goto tr348;
	}
	if ( (*( sm->p)) < 65 ) {
		if ( 47 <= (*( sm->p)) && (*( sm->p)) <= 58 )
			goto tr348;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto tr348;
	} else
		goto tr348;
	goto tr216;
tr348:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ sm->b1 = sm->p; }
	goto st293;
st293:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof293;
case 293:
#line 7697 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 33: goto st293;
		case 35: goto st293;
		case 38: goto st293;
		case 46: goto tr350;
		case 95: goto st293;
	}
	if ( (*( sm->p)) < 65 ) {
		if ( 45 <= (*( sm->p)) && (*( sm->p)) <= 58 )
			goto st293;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st293;
	} else
		goto st293;
	goto tr216;
tr347:
#line 72 "ext/dtext/dtext.cpp.rl"
	{ sm->a2 = sm->p; }
#line 73 "ext/dtext/dtext.cpp.rl"
	{ sm->b1 = sm->p; }
#line 74 "ext/dtext/dtext.cpp.rl"
	{ sm->b2 = sm->p; }
	goto st1404;
tr350:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ sm->b2 = sm->p; }
	goto st1404;
st1404:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1404;
case 1404:
#line 7730 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 9: goto st1404;
		case 32: goto st1404;
	}
	goto tr1815;
st294:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof294;
case 294:
	switch( (*( sm->p)) ) {
		case 47: goto st295;
		case 67: goto st310;
		case 72: goto st196;
		case 78: goto st317;
		case 83: goto st198;
		case 84: goto st206;
		case 99: goto st310;
		case 104: goto st196;
		case 110: goto st317;
		case 115: goto st198;
		case 116: goto st206;
	}
	goto tr216;
st295:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof295;
case 295:
	switch( (*( sm->p)) ) {
		case 67: goto st229;
		case 69: goto st296;
		case 81: goto st252;
		case 83: goto st302;
		case 84: goto st239;
		case 99: goto st229;
		case 101: goto st296;
		case 113: goto st252;
		case 115: goto st302;
		case 116: goto st239;
	}
	goto tr216;
st296:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof296;
case 296:
	switch( (*( sm->p)) ) {
		case 88: goto st297;
		case 120: goto st297;
	}
	goto tr207;
st297:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof297;
case 297:
	switch( (*( sm->p)) ) {
		case 80: goto st298;
		case 112: goto st298;
	}
	goto tr207;
st298:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof298;
case 298:
	switch( (*( sm->p)) ) {
		case 65: goto st299;
		case 97: goto st299;
	}
	goto tr207;
st299:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof299;
case 299:
	switch( (*( sm->p)) ) {
		case 78: goto st300;
		case 110: goto st300;
	}
	goto tr207;
st300:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof300;
case 300:
	switch( (*( sm->p)) ) {
		case 68: goto st301;
		case 100: goto st301;
	}
	goto tr207;
st301:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof301;
case 301:
	_widec = (*( sm->p));
	if ( 93 <= (*( sm->p)) && (*( sm->p)) <= 93 ) {
		_widec = (short)(3200 + ((*( sm->p)) - -128));
		if ( 
#line 85 "ext/dtext/dtext.cpp.rl"
 dstack_is_open(sm, BLOCK_EXPAND)  ) _widec += 256;
	}
	if ( _widec == 3677 )
		goto st1403;
	goto tr207;
st302:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof302;
case 302:
	switch( (*( sm->p)) ) {
		case 80: goto st303;
		case 112: goto st303;
	}
	goto tr216;
st303:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof303;
case 303:
	switch( (*( sm->p)) ) {
		case 79: goto st304;
		case 111: goto st304;
	}
	goto tr207;
st304:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof304;
case 304:
	switch( (*( sm->p)) ) {
		case 73: goto st305;
		case 105: goto st305;
	}
	goto tr207;
st305:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof305;
case 305:
	switch( (*( sm->p)) ) {
		case 76: goto st306;
		case 108: goto st306;
	}
	goto tr207;
st306:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof306;
case 306:
	switch( (*( sm->p)) ) {
		case 69: goto st307;
		case 101: goto st307;
	}
	goto tr207;
st307:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof307;
case 307:
	switch( (*( sm->p)) ) {
		case 82: goto st308;
		case 114: goto st308;
	}
	goto tr207;
st308:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof308;
case 308:
	switch( (*( sm->p)) ) {
		case 83: goto st309;
		case 93: goto tr328;
		case 115: goto st309;
	}
	goto tr207;
st309:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof309;
case 309:
	if ( (*( sm->p)) == 93 )
		goto tr328;
	goto tr207;
st310:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof310;
case 310:
	switch( (*( sm->p)) ) {
		case 79: goto st311;
		case 111: goto st311;
	}
	goto tr216;
st311:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof311;
case 311:
	switch( (*( sm->p)) ) {
		case 68: goto st312;
		case 100: goto st312;
	}
	goto tr216;
st312:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof312;
case 312:
	switch( (*( sm->p)) ) {
		case 69: goto st313;
		case 101: goto st313;
	}
	goto tr216;
st313:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof313;
case 313:
	switch( (*( sm->p)) ) {
		case 9: goto st314;
		case 32: goto st314;
		case 61: goto st315;
		case 93: goto tr335;
	}
	goto tr216;
st314:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof314;
case 314:
	switch( (*( sm->p)) ) {
		case 9: goto st314;
		case 32: goto st314;
		case 61: goto st315;
	}
	goto tr216;
st315:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof315;
case 315:
	switch( (*( sm->p)) ) {
		case 9: goto st315;
		case 32: goto st315;
	}
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr373;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto tr373;
	} else
		goto tr373;
	goto tr216;
tr373:
#line 71 "ext/dtext/dtext.cpp.rl"
	{ sm->a1 = sm->p; }
	goto st316;
st316:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof316;
case 316:
#line 7974 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) == 93 )
		goto tr338;
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st316;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st316;
	} else
		goto st316;
	goto tr216;
st317:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof317;
case 317:
	switch( (*( sm->p)) ) {
		case 79: goto st318;
		case 111: goto st318;
	}
	goto tr216;
st318:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof318;
case 318:
	switch( (*( sm->p)) ) {
		case 68: goto st319;
		case 100: goto st319;
	}
	goto tr216;
st319:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof319;
case 319:
	switch( (*( sm->p)) ) {
		case 84: goto st320;
		case 116: goto st320;
	}
	goto tr216;
st320:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof320;
case 320:
	switch( (*( sm->p)) ) {
		case 69: goto st321;
		case 101: goto st321;
	}
	goto tr216;
st321:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof321;
case 321:
	switch( (*( sm->p)) ) {
		case 88: goto st322;
		case 120: goto st322;
	}
	goto tr216;
st322:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof322;
case 322:
	switch( (*( sm->p)) ) {
		case 84: goto st323;
		case 116: goto st323;
	}
	goto tr216;
st323:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof323;
case 323:
	if ( (*( sm->p)) == 93 )
		goto tr335;
	goto tr216;
st324:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof324;
case 324:
	if ( (*( sm->p)) == 96 )
		goto st325;
	goto tr216;
st325:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof325;
case 325:
	if ( (*( sm->p)) == 96 )
		goto st326;
	goto tr216;
tr384:
#line 71 "ext/dtext/dtext.cpp.rl"
	{ sm->a1 = sm->p; }
#line 72 "ext/dtext/dtext.cpp.rl"
	{ sm->a2 = sm->p; }
	goto st326;
st326:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof326;
case 326:
#line 8071 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 0: goto tr383;
		case 9: goto tr384;
		case 10: goto tr383;
		case 32: goto tr384;
	}
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr385;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto tr385;
	} else
		goto tr385;
	goto tr216;
tr394:
#line 72 "ext/dtext/dtext.cpp.rl"
	{ sm->a2 = sm->p; }
	goto st327;
tr383:
#line 71 "ext/dtext/dtext.cpp.rl"
	{ sm->a1 = sm->p; }
#line 72 "ext/dtext/dtext.cpp.rl"
	{ sm->a2 = sm->p; }
	goto st327;
st327:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof327;
case 327:
#line 8101 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 0: goto tr387;
		case 10: goto tr387;
	}
	goto tr386;
tr386:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ sm->b1 = sm->p; }
	goto st328;
st328:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof328;
case 328:
#line 8115 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 0: goto tr389;
		case 10: goto tr389;
	}
	goto st328;
tr389:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ sm->b2 = sm->p; }
	goto st329;
tr387:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ sm->b1 = sm->p; }
#line 74 "ext/dtext/dtext.cpp.rl"
	{ sm->b2 = sm->p; }
	goto st329;
st329:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof329;
case 329:
#line 8135 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 0: goto tr389;
		case 10: goto tr389;
		case 96: goto st330;
	}
	goto st328;
st330:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof330;
case 330:
	switch( (*( sm->p)) ) {
		case 0: goto tr389;
		case 10: goto tr389;
		case 96: goto st331;
	}
	goto st328;
st331:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof331;
case 331:
	switch( (*( sm->p)) ) {
		case 0: goto tr389;
		case 10: goto tr389;
		case 96: goto st332;
	}
	goto st328;
st332:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof332;
case 332:
	switch( (*( sm->p)) ) {
		case 0: goto tr393;
		case 9: goto st332;
		case 10: goto tr393;
		case 32: goto st332;
	}
	goto st328;
tr385:
#line 71 "ext/dtext/dtext.cpp.rl"
	{ sm->a1 = sm->p; }
	goto st333;
st333:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof333;
case 333:
#line 8181 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 0: goto tr394;
		case 9: goto tr395;
		case 10: goto tr394;
		case 32: goto tr395;
	}
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st333;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st333;
	} else
		goto st333;
	goto tr216;
tr395:
#line 72 "ext/dtext/dtext.cpp.rl"
	{ sm->a2 = sm->p; }
	goto st334;
st334:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof334;
case 334:
#line 8205 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 0: goto st327;
		case 9: goto st334;
		case 10: goto st327;
		case 32: goto st334;
	}
	goto tr216;
tr1774:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 587 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 81;}
	goto st1405;
st1405:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1405;
case 1405:
#line 8223 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) == 34 )
		goto tr1795;
	goto tr1817;
tr1817:
#line 71 "ext/dtext/dtext.cpp.rl"
	{ sm->a1 = sm->p; }
	goto st335;
st335:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof335;
case 335:
#line 8235 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) == 34 )
		goto tr400;
	goto st335;
tr400:
#line 72 "ext/dtext/dtext.cpp.rl"
	{ sm->a2 = sm->p; }
	goto st336;
st336:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof336;
case 336:
#line 8247 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) == 58 )
		goto st337;
	goto tr214;
st337:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof337;
case 337:
	switch( (*( sm->p)) ) {
		case 35: goto tr402;
		case 47: goto tr403;
		case 72: goto tr404;
		case 91: goto st396;
		case 104: goto tr404;
	}
	goto tr214;
tr402:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 73 "ext/dtext/dtext.cpp.rl"
	{ sm->b1 = sm->p; }
#line 74 "ext/dtext/dtext.cpp.rl"
	{ sm->b2 = sm->p; }
	goto st1406;
tr407:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 74 "ext/dtext/dtext.cpp.rl"
	{ sm->b2 = sm->p; }
	goto st1406;
st1406:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1406;
case 1406:
#line 8281 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case -30: goto st340;
		case -29: goto st342;
		case -17: goto st344;
		case 32: goto tr1818;
		case 34: goto st348;
		case 35: goto tr1818;
		case 39: goto st348;
		case 44: goto st348;
		case 46: goto st348;
		case 60: goto tr1818;
		case 62: goto tr1818;
		case 63: goto st348;
		case 91: goto tr1818;
		case 93: goto tr1818;
	}
	if ( (*( sm->p)) < -16 ) {
		if ( (*( sm->p)) < -62 ) {
			if ( (*( sm->p)) <= -63 )
				goto tr1818;
		} else if ( (*( sm->p)) > -33 ) {
			if ( -32 <= (*( sm->p)) && (*( sm->p)) <= -18 )
				goto st339;
		} else
			goto st338;
	} else if ( (*( sm->p)) > -12 ) {
		if ( (*( sm->p)) < 9 ) {
			if ( -11 <= (*( sm->p)) && (*( sm->p)) <= 0 )
				goto tr1818;
		} else if ( (*( sm->p)) > 13 ) {
			if ( 58 <= (*( sm->p)) && (*( sm->p)) <= 59 )
				goto st348;
		} else
			goto tr1818;
	} else
		goto st347;
	goto tr407;
st338:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof338;
case 338:
	if ( (*( sm->p)) <= -65 )
		goto tr407;
	goto tr406;
st339:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof339;
case 339:
	if ( (*( sm->p)) <= -65 )
		goto st338;
	goto tr406;
st340:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof340;
case 340:
	if ( (*( sm->p)) == -99 )
		goto st341;
	if ( (*( sm->p)) <= -65 )
		goto st338;
	goto tr406;
st341:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof341;
case 341:
	if ( (*( sm->p)) > -84 ) {
		if ( -82 <= (*( sm->p)) && (*( sm->p)) <= -65 )
			goto tr407;
	} else
		goto tr407;
	goto tr406;
st342:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof342;
case 342:
	if ( (*( sm->p)) == -128 )
		goto st343;
	if ( -127 <= (*( sm->p)) && (*( sm->p)) <= -65 )
		goto st338;
	goto tr406;
st343:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof343;
case 343:
	if ( (*( sm->p)) < -110 ) {
		if ( -125 <= (*( sm->p)) && (*( sm->p)) <= -121 )
			goto tr407;
	} else if ( (*( sm->p)) > -109 ) {
		if ( -99 <= (*( sm->p)) && (*( sm->p)) <= -65 )
			goto tr407;
	} else
		goto tr407;
	goto tr406;
st344:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof344;
case 344:
	switch( (*( sm->p)) ) {
		case -68: goto st345;
		case -67: goto st346;
	}
	if ( (*( sm->p)) <= -65 )
		goto st338;
	goto tr406;
st345:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof345;
case 345:
	if ( (*( sm->p)) < -118 ) {
		if ( (*( sm->p)) <= -120 )
			goto tr407;
	} else if ( (*( sm->p)) > -68 ) {
		if ( -66 <= (*( sm->p)) && (*( sm->p)) <= -65 )
			goto tr407;
	} else
		goto tr407;
	goto tr406;
st346:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof346;
case 346:
	if ( (*( sm->p)) < -98 ) {
		if ( (*( sm->p)) <= -100 )
			goto tr407;
	} else if ( (*( sm->p)) > -97 ) {
		if ( (*( sm->p)) > -94 ) {
			if ( -92 <= (*( sm->p)) && (*( sm->p)) <= -65 )
				goto tr407;
		} else if ( (*( sm->p)) >= -95 )
			goto tr407;
	} else
		goto tr407;
	goto tr406;
st347:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof347;
case 347:
	if ( (*( sm->p)) <= -65 )
		goto st339;
	goto tr406;
st348:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof348;
case 348:
	switch( (*( sm->p)) ) {
		case -30: goto st340;
		case -29: goto st342;
		case -17: goto st344;
		case 32: goto tr406;
		case 34: goto st348;
		case 35: goto tr406;
		case 39: goto st348;
		case 44: goto st348;
		case 46: goto st348;
		case 60: goto tr406;
		case 62: goto tr406;
		case 63: goto st348;
		case 91: goto tr406;
		case 93: goto tr406;
	}
	if ( (*( sm->p)) < -16 ) {
		if ( (*( sm->p)) < -62 ) {
			if ( (*( sm->p)) <= -63 )
				goto tr406;
		} else if ( (*( sm->p)) > -33 ) {
			if ( -32 <= (*( sm->p)) && (*( sm->p)) <= -18 )
				goto st339;
		} else
			goto st338;
	} else if ( (*( sm->p)) > -12 ) {
		if ( (*( sm->p)) < 9 ) {
			if ( -11 <= (*( sm->p)) && (*( sm->p)) <= 0 )
				goto tr406;
		} else if ( (*( sm->p)) > 13 ) {
			if ( 58 <= (*( sm->p)) && (*( sm->p)) <= 59 )
				goto st348;
		} else
			goto tr406;
	} else
		goto st347;
	goto tr407;
tr403:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 73 "ext/dtext/dtext.cpp.rl"
	{ sm->b1 = sm->p; }
#line 74 "ext/dtext/dtext.cpp.rl"
	{ sm->b2 = sm->p; }
#line 334 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 35;}
	goto st1407;
tr419:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 74 "ext/dtext/dtext.cpp.rl"
	{ sm->b2 = sm->p; }
#line 334 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 35;}
	goto st1407;
st1407:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1407;
case 1407:
#line 8484 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case -30: goto st351;
		case -29: goto st353;
		case -17: goto st355;
		case 32: goto tr1818;
		case 34: goto st359;
		case 35: goto tr407;
		case 39: goto st359;
		case 44: goto st359;
		case 46: goto st359;
		case 60: goto tr1818;
		case 62: goto tr1818;
		case 63: goto st360;
		case 91: goto tr1818;
		case 93: goto tr1818;
	}
	if ( (*( sm->p)) < -16 ) {
		if ( (*( sm->p)) < -62 ) {
			if ( (*( sm->p)) <= -63 )
				goto tr1818;
		} else if ( (*( sm->p)) > -33 ) {
			if ( -32 <= (*( sm->p)) && (*( sm->p)) <= -18 )
				goto st350;
		} else
			goto st349;
	} else if ( (*( sm->p)) > -12 ) {
		if ( (*( sm->p)) < 9 ) {
			if ( -11 <= (*( sm->p)) && (*( sm->p)) <= 0 )
				goto tr1818;
		} else if ( (*( sm->p)) > 13 ) {
			if ( 58 <= (*( sm->p)) && (*( sm->p)) <= 59 )
				goto st359;
		} else
			goto tr1818;
	} else
		goto st358;
	goto tr419;
st349:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof349;
case 349:
	if ( (*( sm->p)) <= -65 )
		goto tr419;
	goto tr406;
st350:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof350;
case 350:
	if ( (*( sm->p)) <= -65 )
		goto st349;
	goto tr406;
st351:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof351;
case 351:
	if ( (*( sm->p)) == -99 )
		goto st352;
	if ( (*( sm->p)) <= -65 )
		goto st349;
	goto tr406;
st352:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof352;
case 352:
	if ( (*( sm->p)) > -84 ) {
		if ( -82 <= (*( sm->p)) && (*( sm->p)) <= -65 )
			goto tr419;
	} else
		goto tr419;
	goto tr406;
st353:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof353;
case 353:
	if ( (*( sm->p)) == -128 )
		goto st354;
	if ( -127 <= (*( sm->p)) && (*( sm->p)) <= -65 )
		goto st349;
	goto tr406;
st354:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof354;
case 354:
	if ( (*( sm->p)) < -110 ) {
		if ( -125 <= (*( sm->p)) && (*( sm->p)) <= -121 )
			goto tr419;
	} else if ( (*( sm->p)) > -109 ) {
		if ( -99 <= (*( sm->p)) && (*( sm->p)) <= -65 )
			goto tr419;
	} else
		goto tr419;
	goto tr406;
st355:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof355;
case 355:
	switch( (*( sm->p)) ) {
		case -68: goto st356;
		case -67: goto st357;
	}
	if ( (*( sm->p)) <= -65 )
		goto st349;
	goto tr406;
st356:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof356;
case 356:
	if ( (*( sm->p)) < -118 ) {
		if ( (*( sm->p)) <= -120 )
			goto tr419;
	} else if ( (*( sm->p)) > -68 ) {
		if ( -66 <= (*( sm->p)) && (*( sm->p)) <= -65 )
			goto tr419;
	} else
		goto tr419;
	goto tr406;
st357:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof357;
case 357:
	if ( (*( sm->p)) < -98 ) {
		if ( (*( sm->p)) <= -100 )
			goto tr419;
	} else if ( (*( sm->p)) > -97 ) {
		if ( (*( sm->p)) > -94 ) {
			if ( -92 <= (*( sm->p)) && (*( sm->p)) <= -65 )
				goto tr419;
		} else if ( (*( sm->p)) >= -95 )
			goto tr419;
	} else
		goto tr419;
	goto tr406;
st358:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof358;
case 358:
	if ( (*( sm->p)) <= -65 )
		goto st350;
	goto tr406;
st359:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof359;
case 359:
	switch( (*( sm->p)) ) {
		case -30: goto st351;
		case -29: goto st353;
		case -17: goto st355;
		case 32: goto tr406;
		case 34: goto st359;
		case 35: goto tr407;
		case 39: goto st359;
		case 44: goto st359;
		case 46: goto st359;
		case 60: goto tr406;
		case 62: goto tr406;
		case 63: goto st360;
		case 91: goto tr406;
		case 93: goto tr406;
	}
	if ( (*( sm->p)) < -16 ) {
		if ( (*( sm->p)) < -62 ) {
			if ( (*( sm->p)) <= -63 )
				goto tr406;
		} else if ( (*( sm->p)) > -33 ) {
			if ( -32 <= (*( sm->p)) && (*( sm->p)) <= -18 )
				goto st350;
		} else
			goto st349;
	} else if ( (*( sm->p)) > -12 ) {
		if ( (*( sm->p)) < 9 ) {
			if ( -11 <= (*( sm->p)) && (*( sm->p)) <= 0 )
				goto tr406;
		} else if ( (*( sm->p)) > 13 ) {
			if ( 58 <= (*( sm->p)) && (*( sm->p)) <= 59 )
				goto st359;
		} else
			goto tr406;
	} else
		goto st358;
	goto tr419;
st360:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof360;
case 360:
	switch( (*( sm->p)) ) {
		case -30: goto st363;
		case -29: goto st365;
		case -17: goto st367;
		case 32: goto tr207;
		case 34: goto st360;
		case 35: goto tr407;
		case 39: goto st360;
		case 44: goto st360;
		case 46: goto st360;
		case 63: goto st360;
	}
	if ( (*( sm->p)) < -16 ) {
		if ( (*( sm->p)) < -62 ) {
			if ( (*( sm->p)) <= -63 )
				goto tr207;
		} else if ( (*( sm->p)) > -33 ) {
			if ( -32 <= (*( sm->p)) && (*( sm->p)) <= -18 )
				goto st362;
		} else
			goto st361;
	} else if ( (*( sm->p)) > -12 ) {
		if ( (*( sm->p)) < 9 ) {
			if ( -11 <= (*( sm->p)) && (*( sm->p)) <= 0 )
				goto tr207;
		} else if ( (*( sm->p)) > 13 ) {
			if ( 58 <= (*( sm->p)) && (*( sm->p)) <= 59 )
				goto st360;
		} else
			goto tr207;
	} else
		goto st370;
	goto tr438;
st361:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof361;
case 361:
	if ( (*( sm->p)) <= -65 )
		goto tr438;
	goto tr207;
tr438:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 74 "ext/dtext/dtext.cpp.rl"
	{ sm->b2 = sm->p; }
#line 334 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 35;}
	goto st1408;
st1408:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1408;
case 1408:
#line 8721 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case -30: goto st363;
		case -29: goto st365;
		case -17: goto st367;
		case 32: goto tr1818;
		case 34: goto st360;
		case 35: goto tr407;
		case 39: goto st360;
		case 44: goto st360;
		case 46: goto st360;
		case 63: goto st360;
	}
	if ( (*( sm->p)) < -16 ) {
		if ( (*( sm->p)) < -62 ) {
			if ( (*( sm->p)) <= -63 )
				goto tr1818;
		} else if ( (*( sm->p)) > -33 ) {
			if ( -32 <= (*( sm->p)) && (*( sm->p)) <= -18 )
				goto st362;
		} else
			goto st361;
	} else if ( (*( sm->p)) > -12 ) {
		if ( (*( sm->p)) < 9 ) {
			if ( -11 <= (*( sm->p)) && (*( sm->p)) <= 0 )
				goto tr1818;
		} else if ( (*( sm->p)) > 13 ) {
			if ( 58 <= (*( sm->p)) && (*( sm->p)) <= 59 )
				goto st360;
		} else
			goto tr1818;
	} else
		goto st370;
	goto tr438;
st362:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof362;
case 362:
	if ( (*( sm->p)) <= -65 )
		goto st361;
	goto tr207;
st363:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof363;
case 363:
	if ( (*( sm->p)) == -99 )
		goto st364;
	if ( (*( sm->p)) <= -65 )
		goto st361;
	goto tr207;
st364:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof364;
case 364:
	if ( (*( sm->p)) > -84 ) {
		if ( -82 <= (*( sm->p)) && (*( sm->p)) <= -65 )
			goto tr438;
	} else
		goto tr438;
	goto tr207;
st365:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof365;
case 365:
	if ( (*( sm->p)) == -128 )
		goto st366;
	if ( -127 <= (*( sm->p)) && (*( sm->p)) <= -65 )
		goto st361;
	goto tr207;
st366:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof366;
case 366:
	if ( (*( sm->p)) < -110 ) {
		if ( -125 <= (*( sm->p)) && (*( sm->p)) <= -121 )
			goto tr438;
	} else if ( (*( sm->p)) > -109 ) {
		if ( -99 <= (*( sm->p)) && (*( sm->p)) <= -65 )
			goto tr438;
	} else
		goto tr438;
	goto tr207;
st367:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof367;
case 367:
	switch( (*( sm->p)) ) {
		case -68: goto st368;
		case -67: goto st369;
	}
	if ( (*( sm->p)) <= -65 )
		goto st361;
	goto tr207;
st368:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof368;
case 368:
	if ( (*( sm->p)) < -118 ) {
		if ( (*( sm->p)) <= -120 )
			goto tr438;
	} else if ( (*( sm->p)) > -68 ) {
		if ( -66 <= (*( sm->p)) && (*( sm->p)) <= -65 )
			goto tr438;
	} else
		goto tr438;
	goto tr207;
st369:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof369;
case 369:
	if ( (*( sm->p)) < -98 ) {
		if ( (*( sm->p)) <= -100 )
			goto tr438;
	} else if ( (*( sm->p)) > -97 ) {
		if ( (*( sm->p)) > -94 ) {
			if ( -92 <= (*( sm->p)) && (*( sm->p)) <= -65 )
				goto tr438;
		} else if ( (*( sm->p)) >= -95 )
			goto tr438;
	} else
		goto tr438;
	goto tr207;
st370:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof370;
case 370:
	if ( (*( sm->p)) <= -65 )
		goto st362;
	goto tr207;
tr404:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ sm->b1 = sm->p; }
	goto st371;
st371:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof371;
case 371:
#line 8858 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 84: goto st372;
		case 116: goto st372;
	}
	goto tr214;
st372:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof372;
case 372:
	switch( (*( sm->p)) ) {
		case 84: goto st373;
		case 116: goto st373;
	}
	goto tr214;
st373:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof373;
case 373:
	switch( (*( sm->p)) ) {
		case 80: goto st374;
		case 112: goto st374;
	}
	goto tr214;
st374:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof374;
case 374:
	switch( (*( sm->p)) ) {
		case 58: goto st375;
		case 83: goto st395;
		case 115: goto st395;
	}
	goto tr214;
st375:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof375;
case 375:
	if ( (*( sm->p)) == 47 )
		goto st376;
	goto tr214;
st376:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof376;
case 376:
	if ( (*( sm->p)) == 47 )
		goto st377;
	goto tr214;
st377:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof377;
case 377:
	switch( (*( sm->p)) ) {
		case 45: goto st379;
		case 95: goto st379;
	}
	if ( (*( sm->p)) < -16 ) {
		if ( (*( sm->p)) > -33 ) {
			if ( -32 <= (*( sm->p)) && (*( sm->p)) <= -17 )
				goto st380;
		} else if ( (*( sm->p)) >= -62 )
			goto st378;
	} else if ( (*( sm->p)) > -12 ) {
		if ( (*( sm->p)) < 65 ) {
			if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
				goto st379;
		} else if ( (*( sm->p)) > 90 ) {
			if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
				goto st379;
		} else
			goto st379;
	} else
		goto st381;
	goto tr214;
st378:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof378;
case 378:
	if ( (*( sm->p)) <= -65 )
		goto st379;
	goto tr214;
st379:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof379;
case 379:
	switch( (*( sm->p)) ) {
		case 45: goto st379;
		case 46: goto st382;
		case 95: goto st379;
	}
	if ( (*( sm->p)) < -16 ) {
		if ( (*( sm->p)) > -33 ) {
			if ( -32 <= (*( sm->p)) && (*( sm->p)) <= -17 )
				goto st380;
		} else if ( (*( sm->p)) >= -62 )
			goto st378;
	} else if ( (*( sm->p)) > -12 ) {
		if ( (*( sm->p)) < 65 ) {
			if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
				goto st379;
		} else if ( (*( sm->p)) > 90 ) {
			if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
				goto st379;
		} else
			goto st379;
	} else
		goto st381;
	goto tr214;
st380:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof380;
case 380:
	if ( (*( sm->p)) <= -65 )
		goto st378;
	goto tr214;
st381:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof381;
case 381:
	if ( (*( sm->p)) <= -65 )
		goto st380;
	goto tr214;
st382:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof382;
case 382:
	switch( (*( sm->p)) ) {
		case -30: goto st385;
		case -29: goto st388;
		case -17: goto st390;
		case 45: goto tr461;
		case 95: goto tr461;
	}
	if ( (*( sm->p)) < -16 ) {
		if ( (*( sm->p)) > -33 ) {
			if ( -32 <= (*( sm->p)) && (*( sm->p)) <= -18 )
				goto st384;
		} else if ( (*( sm->p)) >= -62 )
			goto st383;
	} else if ( (*( sm->p)) > -12 ) {
		if ( (*( sm->p)) < 65 ) {
			if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
				goto tr461;
		} else if ( (*( sm->p)) > 90 ) {
			if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
				goto tr461;
		} else
			goto tr461;
	} else
		goto st393;
	goto tr207;
st383:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof383;
case 383:
	if ( (*( sm->p)) <= -65 )
		goto tr461;
	goto tr207;
tr461:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 74 "ext/dtext/dtext.cpp.rl"
	{ sm->b2 = sm->p; }
#line 334 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 35;}
	goto st1409;
st1409:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1409;
case 1409:
#line 9028 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case -30: goto st385;
		case -29: goto st388;
		case -17: goto st390;
		case 35: goto tr407;
		case 46: goto st382;
		case 47: goto tr419;
		case 58: goto st394;
		case 63: goto st360;
		case 95: goto tr461;
	}
	if ( (*( sm->p)) < -16 ) {
		if ( (*( sm->p)) > -33 ) {
			if ( -32 <= (*( sm->p)) && (*( sm->p)) <= -18 )
				goto st384;
		} else if ( (*( sm->p)) >= -62 )
			goto st383;
	} else if ( (*( sm->p)) > -12 ) {
		if ( (*( sm->p)) < 65 ) {
			if ( 45 <= (*( sm->p)) && (*( sm->p)) <= 57 )
				goto tr461;
		} else if ( (*( sm->p)) > 90 ) {
			if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
				goto tr461;
		} else
			goto tr461;
	} else
		goto st393;
	goto tr1818;
st384:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof384;
case 384:
	if ( (*( sm->p)) <= -65 )
		goto st383;
	goto tr207;
st385:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof385;
case 385:
	if ( (*( sm->p)) == -99 )
		goto st386;
	if ( (*( sm->p)) <= -65 )
		goto st383;
	goto tr207;
st386:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof386;
case 386:
	if ( (*( sm->p)) == -83 )
		goto st387;
	if ( (*( sm->p)) <= -65 )
		goto tr461;
	goto tr207;
st387:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof387;
case 387:
	switch( (*( sm->p)) ) {
		case -30: goto st385;
		case -29: goto st388;
		case -17: goto st390;
		case 35: goto tr407;
		case 46: goto st382;
		case 47: goto tr419;
		case 58: goto st394;
		case 63: goto st360;
		case 95: goto tr461;
	}
	if ( (*( sm->p)) < -16 ) {
		if ( (*( sm->p)) > -33 ) {
			if ( -32 <= (*( sm->p)) && (*( sm->p)) <= -18 )
				goto st384;
		} else if ( (*( sm->p)) >= -62 )
			goto st383;
	} else if ( (*( sm->p)) > -12 ) {
		if ( (*( sm->p)) < 65 ) {
			if ( 45 <= (*( sm->p)) && (*( sm->p)) <= 57 )
				goto tr461;
		} else if ( (*( sm->p)) > 90 ) {
			if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
				goto tr461;
		} else
			goto tr461;
	} else
		goto st393;
	goto tr207;
st388:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof388;
case 388:
	if ( (*( sm->p)) == -128 )
		goto st389;
	if ( -127 <= (*( sm->p)) && (*( sm->p)) <= -65 )
		goto st383;
	goto tr207;
st389:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof389;
case 389:
	if ( (*( sm->p)) < -120 ) {
		if ( (*( sm->p)) > -126 ) {
			if ( -125 <= (*( sm->p)) && (*( sm->p)) <= -121 )
				goto tr461;
		} else
			goto st387;
	} else if ( (*( sm->p)) > -111 ) {
		if ( (*( sm->p)) < -108 ) {
			if ( -110 <= (*( sm->p)) && (*( sm->p)) <= -109 )
				goto tr461;
		} else if ( (*( sm->p)) > -100 ) {
			if ( -99 <= (*( sm->p)) && (*( sm->p)) <= -65 )
				goto tr461;
		} else
			goto st387;
	} else
		goto st387;
	goto tr207;
st390:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof390;
case 390:
	switch( (*( sm->p)) ) {
		case -68: goto st391;
		case -67: goto st392;
	}
	if ( (*( sm->p)) <= -65 )
		goto st383;
	goto tr207;
st391:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof391;
case 391:
	switch( (*( sm->p)) ) {
		case -119: goto st387;
		case -67: goto st387;
	}
	if ( (*( sm->p)) <= -65 )
		goto tr461;
	goto tr207;
st392:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof392;
case 392:
	switch( (*( sm->p)) ) {
		case -99: goto st387;
		case -96: goto st387;
		case -93: goto st387;
	}
	if ( (*( sm->p)) <= -65 )
		goto tr461;
	goto tr207;
st393:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof393;
case 393:
	if ( (*( sm->p)) <= -65 )
		goto st384;
	goto tr207;
st394:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof394;
case 394:
	if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
		goto tr468;
	goto tr207;
tr468:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 74 "ext/dtext/dtext.cpp.rl"
	{ sm->b2 = sm->p; }
#line 334 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 35;}
	goto st1410;
st1410:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1410;
case 1410:
#line 9207 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 35: goto tr407;
		case 47: goto tr419;
		case 63: goto st360;
	}
	if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
		goto tr468;
	goto tr1818;
st395:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof395;
case 395:
	if ( (*( sm->p)) == 58 )
		goto st375;
	goto tr214;
st396:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof396;
case 396:
	switch( (*( sm->p)) ) {
		case 35: goto tr469;
		case 47: goto tr469;
		case 72: goto tr470;
		case 104: goto tr470;
	}
	goto tr214;
tr469:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ sm->b1 = sm->p; }
	goto st397;
st397:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof397;
case 397:
#line 9242 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 0: goto tr214;
		case 32: goto tr214;
		case 93: goto tr472;
	}
	if ( 9 <= (*( sm->p)) && (*( sm->p)) <= 13 )
		goto tr214;
	goto st397;
tr470:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ sm->b1 = sm->p; }
	goto st398;
st398:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof398;
case 398:
#line 9259 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 84: goto st399;
		case 116: goto st399;
	}
	goto tr214;
st399:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof399;
case 399:
	switch( (*( sm->p)) ) {
		case 84: goto st400;
		case 116: goto st400;
	}
	goto tr214;
st400:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof400;
case 400:
	switch( (*( sm->p)) ) {
		case 80: goto st401;
		case 112: goto st401;
	}
	goto tr214;
st401:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof401;
case 401:
	switch( (*( sm->p)) ) {
		case 58: goto st402;
		case 83: goto st405;
		case 115: goto st405;
	}
	goto tr214;
st402:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof402;
case 402:
	if ( (*( sm->p)) == 47 )
		goto st403;
	goto tr214;
st403:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof403;
case 403:
	if ( (*( sm->p)) == 47 )
		goto st404;
	goto tr214;
st404:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof404;
case 404:
	switch( (*( sm->p)) ) {
		case 0: goto tr214;
		case 32: goto tr214;
	}
	if ( 9 <= (*( sm->p)) && (*( sm->p)) <= 13 )
		goto tr214;
	goto st397;
st405:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof405;
case 405:
	if ( (*( sm->p)) == 58 )
		goto st402;
	goto tr214;
tr1819:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 583 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 80;}
	goto st1411;
tr1775:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 71 "ext/dtext/dtext.cpp.rl"
	{ sm->a1 = sm->p; }
#line 583 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 80;}
	goto st1411;
st1411:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1411;
case 1411:
#line 9343 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 91: goto tr1820;
		case 123: goto tr1821;
	}
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1819;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto tr1819;
	} else
		goto tr1819;
	goto tr1796;
tr1820:
#line 72 "ext/dtext/dtext.cpp.rl"
	{ sm->a2 = sm->p; }
	goto st406;
st406:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof406;
case 406:
#line 9365 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) == 91 )
		goto st407;
	goto tr210;
tr482:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ sm->b1 = sm->p; }
	goto st407;
st407:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof407;
case 407:
#line 9377 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 9: goto tr482;
		case 32: goto tr482;
		case 58: goto tr484;
		case 60: goto tr485;
		case 62: goto tr486;
		case 92: goto tr487;
		case 93: goto tr207;
		case 124: goto tr488;
	}
	if ( 10 <= (*( sm->p)) && (*( sm->p)) <= 13 )
		goto tr483;
	goto tr481;
tr481:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ sm->b1 = sm->p; }
	goto st408;
st408:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof408;
case 408:
#line 9399 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 9: goto tr490;
		case 32: goto tr490;
		case 35: goto tr492;
		case 93: goto tr493;
		case 124: goto tr494;
	}
	if ( 10 <= (*( sm->p)) && (*( sm->p)) <= 13 )
		goto st410;
	goto st408;
tr490:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ sm->b2 = sm->p; }
	goto st409;
st409:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof409;
case 409:
#line 9418 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 9: goto st409;
		case 32: goto st409;
		case 35: goto st411;
		case 93: goto st414;
		case 124: goto st415;
	}
	if ( 10 <= (*( sm->p)) && (*( sm->p)) <= 13 )
		goto st410;
	goto st408;
tr483:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ sm->b1 = sm->p; }
	goto st410;
st410:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof410;
case 410:
#line 9437 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 32: goto st410;
		case 93: goto tr207;
		case 124: goto tr207;
	}
	if ( 9 <= (*( sm->p)) && (*( sm->p)) <= 13 )
		goto st410;
	goto st408;
tr492:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ sm->b2 = sm->p; }
	goto st411;
st411:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof411;
case 411:
#line 9454 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 9: goto tr490;
		case 32: goto tr490;
		case 35: goto tr492;
		case 93: goto tr493;
		case 124: goto tr494;
	}
	if ( (*( sm->p)) > 13 ) {
		if ( 65 <= (*( sm->p)) && (*( sm->p)) <= 90 )
			goto tr499;
	} else if ( (*( sm->p)) >= 10 )
		goto st410;
	goto st408;
tr499:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ sm->c1 = sm->p; }
	goto st412;
st412:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof412;
case 412:
#line 9476 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 9: goto tr500;
		case 32: goto tr501;
		case 45: goto st420;
		case 93: goto tr504;
		case 95: goto st420;
		case 124: goto tr505;
	}
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st412;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st412;
	} else
		goto st412;
	goto tr207;
tr500:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ sm->c2 = sm->p; }
	goto st413;
st413:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof413;
case 413:
#line 9502 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 9: goto st413;
		case 32: goto st413;
		case 93: goto st414;
		case 124: goto st415;
	}
	goto tr207;
tr493:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ sm->b2 = sm->p; }
	goto st414;
tr504:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ sm->c2 = sm->p; }
	goto st414;
st414:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof414;
case 414:
#line 9522 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) == 93 )
		goto st1412;
	goto tr207;
st1412:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1412;
case 1412:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1823;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto tr1823;
	} else
		goto tr1823;
	goto tr1822;
tr1823:
#line 79 "ext/dtext/dtext.cpp.rl"
	{ sm->e1 = sm->p; }
	goto st1413;
st1413:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1413;
case 1413:
#line 9547 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st1413;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st1413;
	} else
		goto st1413;
	goto tr1824;
tr494:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ sm->b2 = sm->p; }
	goto st415;
tr505:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ sm->c2 = sm->p; }
	goto st415;
tr509:
#line 77 "ext/dtext/dtext.cpp.rl"
	{ sm->d1 = sm->p; }
#line 78 "ext/dtext/dtext.cpp.rl"
	{ sm->d2 = sm->p; }
	goto st415;
st415:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof415;
case 415:
#line 9575 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 9: goto tr509;
		case 32: goto tr509;
		case 93: goto tr510;
		case 124: goto tr207;
	}
	if ( 10 <= (*( sm->p)) && (*( sm->p)) <= 13 )
		goto tr207;
	goto tr508;
tr508:
#line 77 "ext/dtext/dtext.cpp.rl"
	{ sm->d1 = sm->p; }
	goto st416;
st416:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof416;
case 416:
#line 9593 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 9: goto tr512;
		case 32: goto tr512;
		case 93: goto tr513;
		case 124: goto tr207;
	}
	if ( 10 <= (*( sm->p)) && (*( sm->p)) <= 13 )
		goto tr207;
	goto st416;
tr512:
#line 78 "ext/dtext/dtext.cpp.rl"
	{ sm->d2 = sm->p; }
	goto st417;
st417:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof417;
case 417:
#line 9611 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 9: goto st417;
		case 32: goto st417;
		case 93: goto st418;
		case 124: goto tr207;
	}
	if ( 10 <= (*( sm->p)) && (*( sm->p)) <= 13 )
		goto tr207;
	goto st416;
tr510:
#line 77 "ext/dtext/dtext.cpp.rl"
	{ sm->d1 = sm->p; }
#line 78 "ext/dtext/dtext.cpp.rl"
	{ sm->d2 = sm->p; }
	goto st418;
tr513:
#line 78 "ext/dtext/dtext.cpp.rl"
	{ sm->d2 = sm->p; }
	goto st418;
st418:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof418;
case 418:
#line 9635 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) == 93 )
		goto st1414;
	goto tr207;
st1414:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1414;
case 1414:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1827;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto tr1827;
	} else
		goto tr1827;
	goto tr1826;
tr1827:
#line 79 "ext/dtext/dtext.cpp.rl"
	{ sm->e1 = sm->p; }
	goto st1415;
st1415:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1415;
case 1415:
#line 9660 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st1415;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st1415;
	} else
		goto st1415;
	goto tr1828;
tr501:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ sm->c2 = sm->p; }
	goto st419;
st419:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof419;
case 419:
#line 9678 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 9: goto st413;
		case 32: goto st419;
		case 45: goto st420;
		case 93: goto st414;
		case 95: goto st420;
		case 124: goto st415;
	}
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st412;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st412;
	} else
		goto st412;
	goto tr207;
st420:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof420;
case 420:
	switch( (*( sm->p)) ) {
		case 32: goto st420;
		case 45: goto st420;
		case 95: goto st420;
	}
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st412;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st412;
	} else
		goto st412;
	goto tr207;
tr484:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ sm->b1 = sm->p; }
	goto st421;
st421:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof421;
case 421:
#line 9722 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 9: goto tr490;
		case 32: goto tr490;
		case 35: goto tr492;
		case 93: goto tr493;
		case 124: goto tr518;
	}
	if ( 10 <= (*( sm->p)) && (*( sm->p)) <= 13 )
		goto st410;
	goto st408;
tr518:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ sm->b2 = sm->p; }
	goto st422;
st422:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof422;
case 422:
#line 9741 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 9: goto tr519;
		case 32: goto tr519;
		case 35: goto tr520;
		case 93: goto tr521;
		case 124: goto tr207;
	}
	if ( 10 <= (*( sm->p)) && (*( sm->p)) <= 13 )
		goto tr207;
	goto tr508;
tr522:
#line 77 "ext/dtext/dtext.cpp.rl"
	{ sm->d1 = sm->p; }
#line 78 "ext/dtext/dtext.cpp.rl"
	{ sm->d2 = sm->p; }
	goto st423;
tr519:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ sm->b2 = sm->p; }
#line 77 "ext/dtext/dtext.cpp.rl"
	{ sm->d1 = sm->p; }
#line 78 "ext/dtext/dtext.cpp.rl"
	{ sm->d2 = sm->p; }
	goto st423;
st423:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof423;
case 423:
#line 9770 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 9: goto tr522;
		case 32: goto tr522;
		case 35: goto tr523;
		case 93: goto tr524;
		case 124: goto tr207;
	}
	if ( 10 <= (*( sm->p)) && (*( sm->p)) <= 13 )
		goto tr207;
	goto tr508;
tr558:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ sm->b2 = sm->p; }
	goto st424;
tr523:
#line 77 "ext/dtext/dtext.cpp.rl"
	{ sm->d1 = sm->p; }
	goto st424;
tr520:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ sm->b2 = sm->p; }
#line 77 "ext/dtext/dtext.cpp.rl"
	{ sm->d1 = sm->p; }
	goto st424;
st424:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof424;
case 424:
#line 9799 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 9: goto tr512;
		case 32: goto tr512;
		case 93: goto tr513;
		case 124: goto tr207;
	}
	if ( (*( sm->p)) > 13 ) {
		if ( 65 <= (*( sm->p)) && (*( sm->p)) <= 90 )
			goto tr525;
	} else if ( (*( sm->p)) >= 10 )
		goto tr207;
	goto st416;
tr525:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ sm->c1 = sm->p; }
	goto st425;
st425:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof425;
case 425:
#line 9820 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 9: goto tr526;
		case 32: goto tr527;
		case 45: goto st429;
		case 93: goto tr530;
		case 95: goto st429;
		case 124: goto tr207;
	}
	if ( (*( sm->p)) < 48 ) {
		if ( 10 <= (*( sm->p)) && (*( sm->p)) <= 13 )
			goto tr207;
	} else if ( (*( sm->p)) > 57 ) {
		if ( (*( sm->p)) > 90 ) {
			if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
				goto st425;
		} else if ( (*( sm->p)) >= 65 )
			goto st425;
	} else
		goto st425;
	goto st416;
tr526:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ sm->c2 = sm->p; }
#line 78 "ext/dtext/dtext.cpp.rl"
	{ sm->d2 = sm->p; }
	goto st426;
st426:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof426;
case 426:
#line 9851 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 9: goto st426;
		case 32: goto st426;
		case 93: goto st427;
		case 124: goto tr207;
	}
	if ( 10 <= (*( sm->p)) && (*( sm->p)) <= 13 )
		goto tr207;
	goto st416;
tr524:
#line 77 "ext/dtext/dtext.cpp.rl"
	{ sm->d1 = sm->p; }
#line 78 "ext/dtext/dtext.cpp.rl"
	{ sm->d2 = sm->p; }
	goto st427;
tr521:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ sm->b2 = sm->p; }
#line 77 "ext/dtext/dtext.cpp.rl"
	{ sm->d1 = sm->p; }
#line 78 "ext/dtext/dtext.cpp.rl"
	{ sm->d2 = sm->p; }
	goto st427;
tr530:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ sm->c2 = sm->p; }
#line 78 "ext/dtext/dtext.cpp.rl"
	{ sm->d2 = sm->p; }
	goto st427;
tr559:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ sm->b2 = sm->p; }
#line 78 "ext/dtext/dtext.cpp.rl"
	{ sm->d2 = sm->p; }
	goto st427;
st427:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof427;
case 427:
#line 9891 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) == 93 )
		goto st1416;
	goto tr207;
st1416:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1416;
case 1416:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1830;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto tr1830;
	} else
		goto tr1830;
	goto tr1822;
tr1830:
#line 79 "ext/dtext/dtext.cpp.rl"
	{ sm->e1 = sm->p; }
	goto st1417;
st1417:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1417;
case 1417:
#line 9916 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st1417;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st1417;
	} else
		goto st1417;
	goto tr1824;
tr527:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ sm->c2 = sm->p; }
#line 78 "ext/dtext/dtext.cpp.rl"
	{ sm->d2 = sm->p; }
	goto st428;
st428:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof428;
case 428:
#line 9936 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 9: goto st426;
		case 32: goto st428;
		case 45: goto st429;
		case 93: goto st427;
		case 95: goto st429;
		case 124: goto tr207;
	}
	if ( (*( sm->p)) < 48 ) {
		if ( 10 <= (*( sm->p)) && (*( sm->p)) <= 13 )
			goto tr207;
	} else if ( (*( sm->p)) > 57 ) {
		if ( (*( sm->p)) > 90 ) {
			if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
				goto st425;
		} else if ( (*( sm->p)) >= 65 )
			goto st425;
	} else
		goto st425;
	goto st416;
st429:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof429;
case 429:
	switch( (*( sm->p)) ) {
		case 9: goto tr512;
		case 32: goto tr535;
		case 45: goto st429;
		case 93: goto tr513;
		case 95: goto st429;
		case 124: goto tr207;
	}
	if ( (*( sm->p)) < 48 ) {
		if ( 10 <= (*( sm->p)) && (*( sm->p)) <= 13 )
			goto tr207;
	} else if ( (*( sm->p)) > 57 ) {
		if ( (*( sm->p)) > 90 ) {
			if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
				goto st425;
		} else if ( (*( sm->p)) >= 65 )
			goto st425;
	} else
		goto st425;
	goto st416;
tr535:
#line 78 "ext/dtext/dtext.cpp.rl"
	{ sm->d2 = sm->p; }
	goto st430;
st430:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof430;
case 430:
#line 9989 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 9: goto st417;
		case 32: goto st430;
		case 45: goto st429;
		case 93: goto st418;
		case 95: goto st429;
		case 124: goto tr207;
	}
	if ( (*( sm->p)) < 48 ) {
		if ( 10 <= (*( sm->p)) && (*( sm->p)) <= 13 )
			goto tr207;
	} else if ( (*( sm->p)) > 57 ) {
		if ( (*( sm->p)) > 90 ) {
			if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
				goto st425;
		} else if ( (*( sm->p)) >= 65 )
			goto st425;
	} else
		goto st425;
	goto st416;
tr485:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ sm->b1 = sm->p; }
	goto st431;
st431:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof431;
case 431:
#line 10018 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 9: goto tr490;
		case 32: goto tr490;
		case 35: goto tr492;
		case 93: goto tr493;
		case 124: goto tr537;
	}
	if ( 10 <= (*( sm->p)) && (*( sm->p)) <= 13 )
		goto st410;
	goto st408;
tr537:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ sm->b2 = sm->p; }
	goto st432;
st432:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof432;
case 432:
#line 10037 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 9: goto tr509;
		case 32: goto tr509;
		case 62: goto tr538;
		case 93: goto tr510;
		case 124: goto tr207;
	}
	if ( 10 <= (*( sm->p)) && (*( sm->p)) <= 13 )
		goto tr207;
	goto tr508;
tr538:
#line 77 "ext/dtext/dtext.cpp.rl"
	{ sm->d1 = sm->p; }
	goto st433;
st433:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof433;
case 433:
#line 10056 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 9: goto tr512;
		case 32: goto tr512;
		case 93: goto tr513;
		case 95: goto st434;
		case 124: goto tr207;
	}
	if ( 10 <= (*( sm->p)) && (*( sm->p)) <= 13 )
		goto tr207;
	goto st416;
st434:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof434;
case 434:
	switch( (*( sm->p)) ) {
		case 9: goto tr512;
		case 32: goto tr512;
		case 60: goto st435;
		case 93: goto tr513;
		case 124: goto tr207;
	}
	if ( 10 <= (*( sm->p)) && (*( sm->p)) <= 13 )
		goto tr207;
	goto st416;
st435:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof435;
case 435:
	switch( (*( sm->p)) ) {
		case 9: goto tr512;
		case 32: goto tr512;
		case 93: goto tr513;
		case 124: goto st436;
	}
	if ( 10 <= (*( sm->p)) && (*( sm->p)) <= 13 )
		goto tr207;
	goto st416;
st436:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof436;
case 436:
	if ( (*( sm->p)) == 62 )
		goto st437;
	goto tr207;
st437:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof437;
case 437:
	switch( (*( sm->p)) ) {
		case 9: goto tr543;
		case 32: goto tr543;
		case 35: goto tr544;
		case 93: goto tr493;
	}
	goto tr207;
tr543:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ sm->b2 = sm->p; }
	goto st438;
st438:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof438;
case 438:
#line 10120 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 9: goto st438;
		case 32: goto st438;
		case 35: goto st439;
		case 93: goto st414;
	}
	goto tr207;
tr544:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ sm->b2 = sm->p; }
	goto st439;
st439:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof439;
case 439:
#line 10136 "ext/dtext/dtext.cpp"
	if ( 65 <= (*( sm->p)) && (*( sm->p)) <= 90 )
		goto tr547;
	goto tr207;
tr547:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ sm->c1 = sm->p; }
	goto st440;
st440:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof440;
case 440:
#line 10148 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 9: goto tr548;
		case 32: goto tr549;
		case 45: goto st443;
		case 93: goto tr504;
		case 95: goto st443;
	}
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st440;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st440;
	} else
		goto st440;
	goto tr207;
tr548:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ sm->c2 = sm->p; }
	goto st441;
st441:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof441;
case 441:
#line 10173 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 9: goto st441;
		case 32: goto st441;
		case 93: goto st414;
	}
	goto tr207;
tr549:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ sm->c2 = sm->p; }
	goto st442;
st442:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof442;
case 442:
#line 10188 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 9: goto st441;
		case 32: goto st442;
		case 45: goto st443;
		case 93: goto st414;
		case 95: goto st443;
	}
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st440;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st440;
	} else
		goto st440;
	goto tr207;
st443:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof443;
case 443:
	switch( (*( sm->p)) ) {
		case 32: goto st443;
		case 45: goto st443;
		case 95: goto st443;
	}
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st440;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st440;
	} else
		goto st440;
	goto tr207;
tr486:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ sm->b1 = sm->p; }
	goto st444;
st444:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof444;
case 444:
#line 10231 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 9: goto tr490;
		case 32: goto tr490;
		case 35: goto tr492;
		case 58: goto st421;
		case 93: goto tr493;
		case 124: goto tr555;
	}
	if ( 10 <= (*( sm->p)) && (*( sm->p)) <= 13 )
		goto st410;
	goto st408;
tr555:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ sm->b2 = sm->p; }
	goto st445;
st445:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof445;
case 445:
#line 10251 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 9: goto tr509;
		case 32: goto tr509;
		case 51: goto tr556;
		case 93: goto tr510;
		case 124: goto tr207;
	}
	if ( 10 <= (*( sm->p)) && (*( sm->p)) <= 13 )
		goto tr207;
	goto tr508;
tr556:
#line 77 "ext/dtext/dtext.cpp.rl"
	{ sm->d1 = sm->p; }
	goto st446;
st446:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof446;
case 446:
#line 10270 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 9: goto tr557;
		case 32: goto tr557;
		case 35: goto tr558;
		case 93: goto tr559;
		case 124: goto tr207;
	}
	if ( 10 <= (*( sm->p)) && (*( sm->p)) <= 13 )
		goto tr207;
	goto st416;
tr557:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ sm->b2 = sm->p; }
#line 78 "ext/dtext/dtext.cpp.rl"
	{ sm->d2 = sm->p; }
	goto st447;
st447:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof447;
case 447:
#line 10291 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 9: goto st447;
		case 32: goto st447;
		case 35: goto st424;
		case 93: goto st427;
		case 124: goto tr207;
	}
	if ( 10 <= (*( sm->p)) && (*( sm->p)) <= 13 )
		goto tr207;
	goto st416;
tr487:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ sm->b1 = sm->p; }
	goto st448;
st448:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof448;
case 448:
#line 10310 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 9: goto tr490;
		case 32: goto tr490;
		case 35: goto tr492;
		case 93: goto tr493;
		case 124: goto tr562;
	}
	if ( 10 <= (*( sm->p)) && (*( sm->p)) <= 13 )
		goto st410;
	goto st408;
tr562:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ sm->b2 = sm->p; }
	goto st449;
st449:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof449;
case 449:
#line 10329 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 9: goto tr509;
		case 32: goto tr509;
		case 93: goto tr510;
		case 124: goto st450;
	}
	if ( 10 <= (*( sm->p)) && (*( sm->p)) <= 13 )
		goto tr207;
	goto tr508;
st450:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof450;
case 450:
	if ( (*( sm->p)) == 47 )
		goto st437;
	goto tr207;
tr488:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ sm->b1 = sm->p; }
	goto st451;
st451:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof451;
case 451:
#line 10354 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 95: goto st455;
		case 119: goto st456;
		case 124: goto st457;
	}
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st452;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st452;
	} else
		goto st452;
	goto tr207;
st452:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof452;
case 452:
	switch( (*( sm->p)) ) {
		case 9: goto tr568;
		case 32: goto tr568;
		case 35: goto tr569;
		case 93: goto tr493;
		case 124: goto tr494;
	}
	goto tr207;
tr568:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ sm->b2 = sm->p; }
	goto st453;
st453:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof453;
case 453:
#line 10389 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 9: goto st453;
		case 32: goto st453;
		case 35: goto st454;
		case 93: goto st414;
		case 124: goto st415;
	}
	goto tr207;
tr569:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ sm->b2 = sm->p; }
	goto st454;
st454:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof454;
case 454:
#line 10406 "ext/dtext/dtext.cpp"
	if ( 65 <= (*( sm->p)) && (*( sm->p)) <= 90 )
		goto tr499;
	goto tr207;
st455:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof455;
case 455:
	if ( (*( sm->p)) == 124 )
		goto st452;
	goto tr207;
st456:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof456;
case 456:
	switch( (*( sm->p)) ) {
		case 9: goto tr568;
		case 32: goto tr568;
		case 35: goto tr569;
		case 93: goto tr493;
		case 124: goto tr518;
	}
	goto tr207;
st457:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof457;
case 457:
	if ( (*( sm->p)) == 95 )
		goto st458;
	goto tr207;
st458:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof458;
case 458:
	if ( (*( sm->p)) == 124 )
		goto st455;
	goto tr207;
tr1821:
#line 72 "ext/dtext/dtext.cpp.rl"
	{ sm->a2 = sm->p; }
	goto st459;
st459:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof459;
case 459:
#line 10451 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) == 123 )
		goto st460;
	goto tr210;
st460:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof460;
case 460:
	switch( (*( sm->p)) ) {
		case 0: goto tr207;
		case 9: goto st460;
		case 32: goto st460;
		case 45: goto tr575;
		case 58: goto tr576;
		case 60: goto tr577;
		case 62: goto tr578;
		case 92: goto tr579;
		case 124: goto tr580;
		case 126: goto tr575;
	}
	if ( (*( sm->p)) > 13 ) {
		if ( 123 <= (*( sm->p)) && (*( sm->p)) <= 125 )
			goto tr207;
	} else if ( (*( sm->p)) >= 10 )
		goto tr207;
	goto tr574;
tr574:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ sm->b1 = sm->p; }
	goto st461;
st461:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof461;
case 461:
#line 10485 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 0: goto tr207;
		case 9: goto tr582;
		case 32: goto tr582;
		case 123: goto tr207;
		case 124: goto tr583;
		case 125: goto tr584;
	}
	if ( 10 <= (*( sm->p)) && (*( sm->p)) <= 13 )
		goto tr207;
	goto st461;
tr582:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ sm->b2 = sm->p; }
	goto st462;
st462:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof462;
case 462:
#line 10505 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 0: goto tr207;
		case 9: goto st462;
		case 32: goto st462;
		case 45: goto st463;
		case 58: goto st464;
		case 60: goto st499;
		case 62: goto st500;
		case 92: goto st502;
		case 123: goto tr207;
		case 124: goto st493;
		case 125: goto st471;
		case 126: goto st463;
	}
	if ( 10 <= (*( sm->p)) && (*( sm->p)) <= 13 )
		goto tr207;
	goto st461;
tr575:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ sm->b1 = sm->p; }
	goto st463;
st463:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof463;
case 463:
#line 10531 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 0: goto tr207;
		case 9: goto tr582;
		case 32: goto tr582;
		case 58: goto st464;
		case 60: goto st499;
		case 62: goto st500;
		case 92: goto st502;
		case 123: goto tr207;
		case 124: goto tr593;
		case 125: goto tr584;
	}
	if ( 10 <= (*( sm->p)) && (*( sm->p)) <= 13 )
		goto tr207;
	goto st461;
tr576:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ sm->b1 = sm->p; }
	goto st464;
st464:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof464;
case 464:
#line 10555 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 0: goto tr207;
		case 9: goto tr582;
		case 32: goto tr582;
		case 123: goto st465;
		case 124: goto tr595;
		case 125: goto tr596;
	}
	if ( 10 <= (*( sm->p)) && (*( sm->p)) <= 13 )
		goto tr207;
	goto st461;
st465:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof465;
case 465:
	switch( (*( sm->p)) ) {
		case 9: goto tr582;
		case 32: goto tr582;
		case 124: goto tr583;
		case 125: goto tr584;
	}
	goto tr207;
tr583:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ sm->b2 = sm->p; }
	goto st466;
tr598:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ sm->c1 = sm->p; }
#line 76 "ext/dtext/dtext.cpp.rl"
	{ sm->c2 = sm->p; }
	goto st466;
tr610:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ sm->b2 = sm->p; }
#line 75 "ext/dtext/dtext.cpp.rl"
	{ sm->c1 = sm->p; }
	goto st466;
st466:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof466;
case 466:
#line 10598 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 0: goto tr207;
		case 9: goto tr598;
		case 10: goto tr207;
		case 13: goto tr207;
		case 32: goto tr598;
		case 125: goto tr600;
	}
	if ( 11 <= (*( sm->p)) && (*( sm->p)) <= 12 )
		goto tr599;
	goto tr597;
tr597:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ sm->c1 = sm->p; }
	goto st467;
st467:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof467;
case 467:
#line 10618 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 0: goto tr207;
		case 9: goto tr602;
		case 10: goto tr207;
		case 13: goto tr207;
		case 32: goto tr602;
		case 125: goto tr604;
	}
	if ( 11 <= (*( sm->p)) && (*( sm->p)) <= 12 )
		goto st469;
	goto st467;
tr602:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ sm->c2 = sm->p; }
	goto st468;
st468:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof468;
case 468:
#line 10638 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 0: goto tr207;
		case 9: goto st468;
		case 10: goto tr207;
		case 13: goto tr207;
		case 32: goto st468;
		case 125: goto st470;
	}
	if ( 11 <= (*( sm->p)) && (*( sm->p)) <= 12 )
		goto st469;
	goto st467;
tr599:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ sm->c1 = sm->p; }
	goto st469;
st469:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof469;
case 469:
#line 10658 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 0: goto tr207;
		case 10: goto tr207;
		case 13: goto tr207;
		case 32: goto st469;
		case 125: goto tr207;
	}
	if ( 9 <= (*( sm->p)) && (*( sm->p)) <= 12 )
		goto st469;
	goto st467;
tr604:
#line 76 "ext/dtext/dtext.cpp.rl"
	{ sm->c2 = sm->p; }
	goto st470;
tr600:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ sm->c1 = sm->p; }
#line 76 "ext/dtext/dtext.cpp.rl"
	{ sm->c2 = sm->p; }
	goto st470;
st470:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof470;
case 470:
#line 10683 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) == 125 )
		goto st1418;
	goto tr207;
st1418:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1418;
case 1418:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1833;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto tr1833;
	} else
		goto tr1833;
	goto tr1832;
tr1833:
#line 77 "ext/dtext/dtext.cpp.rl"
	{ sm->d1 = sm->p; }
	goto st1419;
st1419:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1419;
case 1419:
#line 10708 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st1419;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st1419;
	} else
		goto st1419;
	goto tr1834;
tr584:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ sm->b2 = sm->p; }
	goto st471;
st471:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof471;
case 471:
#line 10726 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) == 125 )
		goto st1420;
	goto tr207;
tr1842:
#line 77 "ext/dtext/dtext.cpp.rl"
	{ sm->d1 = sm->p; }
#line 78 "ext/dtext/dtext.cpp.rl"
	{ sm->d2 = sm->p; }
	goto st1420;
st1420:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1420;
case 1420:
#line 10740 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1837;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto tr1837;
	} else
		goto tr1837;
	goto tr1836;
tr1837:
#line 77 "ext/dtext/dtext.cpp.rl"
	{ sm->d1 = sm->p; }
	goto st1421;
st1421:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1421;
case 1421:
#line 10758 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st1421;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st1421;
	} else
		goto st1421;
	goto tr1838;
tr595:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ sm->b2 = sm->p; }
	goto st472;
st472:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof472;
case 472:
#line 10776 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 0: goto tr207;
		case 9: goto tr609;
		case 10: goto tr207;
		case 13: goto tr207;
		case 32: goto tr609;
		case 124: goto tr610;
		case 125: goto tr611;
	}
	if ( 11 <= (*( sm->p)) && (*( sm->p)) <= 12 )
		goto tr599;
	goto tr597;
tr613:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ sm->c1 = sm->p; }
#line 76 "ext/dtext/dtext.cpp.rl"
	{ sm->c2 = sm->p; }
	goto st473;
tr609:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ sm->b2 = sm->p; }
#line 75 "ext/dtext/dtext.cpp.rl"
	{ sm->c1 = sm->p; }
#line 76 "ext/dtext/dtext.cpp.rl"
	{ sm->c2 = sm->p; }
	goto st473;
st473:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof473;
case 473:
#line 10807 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 0: goto tr207;
		case 9: goto tr613;
		case 10: goto tr207;
		case 13: goto tr207;
		case 32: goto tr613;
		case 45: goto tr614;
		case 58: goto tr615;
		case 60: goto tr616;
		case 62: goto tr617;
		case 92: goto tr618;
		case 123: goto tr597;
		case 124: goto tr619;
		case 125: goto tr620;
		case 126: goto tr614;
	}
	if ( 11 <= (*( sm->p)) && (*( sm->p)) <= 12 )
		goto tr599;
	goto tr612;
tr612:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ sm->c1 = sm->p; }
	goto st474;
st474:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof474;
case 474:
#line 10835 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 0: goto tr207;
		case 9: goto tr622;
		case 10: goto tr207;
		case 13: goto tr207;
		case 32: goto tr622;
		case 123: goto st467;
		case 124: goto tr583;
		case 125: goto tr623;
	}
	if ( 11 <= (*( sm->p)) && (*( sm->p)) <= 12 )
		goto st469;
	goto st474;
tr622:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ sm->b2 = sm->p; }
#line 76 "ext/dtext/dtext.cpp.rl"
	{ sm->c2 = sm->p; }
	goto st475;
st475:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof475;
case 475:
#line 10859 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 0: goto tr207;
		case 9: goto st475;
		case 10: goto tr207;
		case 13: goto tr207;
		case 32: goto st475;
		case 45: goto st476;
		case 58: goto st477;
		case 60: goto st481;
		case 62: goto st487;
		case 92: goto st490;
		case 123: goto st467;
		case 124: goto st493;
		case 125: goto st479;
		case 126: goto st476;
	}
	if ( 11 <= (*( sm->p)) && (*( sm->p)) <= 12 )
		goto st469;
	goto st474;
tr614:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ sm->c1 = sm->p; }
	goto st476;
st476:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof476;
case 476:
#line 10887 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 0: goto tr207;
		case 9: goto tr622;
		case 10: goto tr207;
		case 13: goto tr207;
		case 32: goto tr622;
		case 58: goto st477;
		case 60: goto st481;
		case 62: goto st487;
		case 92: goto st490;
		case 123: goto st467;
		case 124: goto tr593;
		case 125: goto tr623;
	}
	if ( 11 <= (*( sm->p)) && (*( sm->p)) <= 12 )
		goto st469;
	goto st474;
tr615:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ sm->c1 = sm->p; }
	goto st477;
st477:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof477;
case 477:
#line 10913 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 0: goto tr207;
		case 9: goto tr622;
		case 10: goto tr207;
		case 13: goto tr207;
		case 32: goto tr622;
		case 123: goto st478;
		case 124: goto tr595;
		case 125: goto tr632;
	}
	if ( 11 <= (*( sm->p)) && (*( sm->p)) <= 12 )
		goto st469;
	goto st474;
tr642:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ sm->c1 = sm->p; }
	goto st478;
st478:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof478;
case 478:
#line 10935 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 0: goto tr207;
		case 9: goto tr622;
		case 10: goto tr207;
		case 13: goto tr207;
		case 32: goto tr622;
		case 124: goto tr583;
		case 125: goto tr623;
	}
	if ( 11 <= (*( sm->p)) && (*( sm->p)) <= 12 )
		goto st469;
	goto st467;
tr620:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ sm->c1 = sm->p; }
#line 76 "ext/dtext/dtext.cpp.rl"
	{ sm->c2 = sm->p; }
	goto st479;
tr611:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ sm->b2 = sm->p; }
#line 75 "ext/dtext/dtext.cpp.rl"
	{ sm->c1 = sm->p; }
#line 76 "ext/dtext/dtext.cpp.rl"
	{ sm->c2 = sm->p; }
	goto st479;
tr623:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ sm->b2 = sm->p; }
#line 76 "ext/dtext/dtext.cpp.rl"
	{ sm->c2 = sm->p; }
	goto st479;
st479:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof479;
case 479:
#line 10972 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) == 125 )
		goto st1422;
	goto tr207;
st1422:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1422;
case 1422:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1840;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto tr1840;
	} else
		goto tr1840;
	goto tr1836;
tr1840:
#line 77 "ext/dtext/dtext.cpp.rl"
	{ sm->d1 = sm->p; }
	goto st1423;
st1423:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1423;
case 1423:
#line 10997 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st1423;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st1423;
	} else
		goto st1423;
	goto tr1838;
tr632:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ sm->b2 = sm->p; }
#line 76 "ext/dtext/dtext.cpp.rl"
	{ sm->c2 = sm->p; }
	goto st480;
st480:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof480;
case 480:
#line 11017 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 9: goto tr582;
		case 32: goto tr582;
		case 124: goto tr583;
		case 125: goto tr634;
	}
	goto tr207;
tr634:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ sm->b2 = sm->p; }
	goto st1424;
st1424:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1424;
case 1424:
#line 11033 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) == 125 )
		goto tr1842;
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1840;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto tr1840;
	} else
		goto tr1840;
	goto tr1836;
tr616:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ sm->c1 = sm->p; }
	goto st481;
st481:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof481;
case 481:
#line 11053 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 0: goto tr207;
		case 9: goto tr622;
		case 10: goto tr207;
		case 13: goto tr207;
		case 32: goto tr622;
		case 123: goto st467;
		case 124: goto tr635;
		case 125: goto tr623;
	}
	if ( 11 <= (*( sm->p)) && (*( sm->p)) <= 12 )
		goto st469;
	goto st474;
tr635:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ sm->b2 = sm->p; }
	goto st482;
st482:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof482;
case 482:
#line 11075 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 0: goto tr207;
		case 9: goto tr598;
		case 10: goto tr207;
		case 13: goto tr207;
		case 32: goto tr598;
		case 62: goto tr636;
		case 125: goto tr600;
	}
	if ( 11 <= (*( sm->p)) && (*( sm->p)) <= 12 )
		goto tr599;
	goto tr597;
tr636:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ sm->c1 = sm->p; }
	goto st483;
st483:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof483;
case 483:
#line 11096 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 0: goto tr207;
		case 9: goto tr602;
		case 10: goto tr207;
		case 13: goto tr207;
		case 32: goto tr602;
		case 95: goto st484;
		case 125: goto tr604;
	}
	if ( 11 <= (*( sm->p)) && (*( sm->p)) <= 12 )
		goto st469;
	goto st467;
st484:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof484;
case 484:
	switch( (*( sm->p)) ) {
		case 0: goto tr207;
		case 9: goto tr602;
		case 10: goto tr207;
		case 13: goto tr207;
		case 32: goto tr602;
		case 60: goto st485;
		case 125: goto tr604;
	}
	if ( 11 <= (*( sm->p)) && (*( sm->p)) <= 12 )
		goto st469;
	goto st467;
st485:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof485;
case 485:
	switch( (*( sm->p)) ) {
		case 0: goto tr207;
		case 9: goto tr602;
		case 10: goto tr207;
		case 13: goto tr207;
		case 32: goto tr602;
		case 124: goto st486;
		case 125: goto tr604;
	}
	if ( 11 <= (*( sm->p)) && (*( sm->p)) <= 12 )
		goto st469;
	goto st467;
st486:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof486;
case 486:
	switch( (*( sm->p)) ) {
		case 0: goto tr207;
		case 9: goto tr602;
		case 10: goto tr207;
		case 13: goto tr207;
		case 32: goto tr602;
		case 62: goto st478;
		case 125: goto tr604;
	}
	if ( 11 <= (*( sm->p)) && (*( sm->p)) <= 12 )
		goto st469;
	goto st467;
tr617:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ sm->c1 = sm->p; }
	goto st487;
st487:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof487;
case 487:
#line 11165 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 0: goto tr207;
		case 9: goto tr622;
		case 10: goto tr207;
		case 13: goto tr207;
		case 32: goto tr622;
		case 58: goto st488;
		case 123: goto st467;
		case 124: goto tr641;
		case 125: goto tr623;
	}
	if ( 11 <= (*( sm->p)) && (*( sm->p)) <= 12 )
		goto st469;
	goto st474;
st488:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof488;
case 488:
	switch( (*( sm->p)) ) {
		case 0: goto tr207;
		case 9: goto tr622;
		case 10: goto tr207;
		case 13: goto tr207;
		case 32: goto tr622;
		case 123: goto st467;
		case 124: goto tr595;
		case 125: goto tr623;
	}
	if ( 11 <= (*( sm->p)) && (*( sm->p)) <= 12 )
		goto st469;
	goto st474;
tr641:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ sm->b2 = sm->p; }
	goto st489;
st489:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof489;
case 489:
#line 11205 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 0: goto tr207;
		case 9: goto tr598;
		case 10: goto tr207;
		case 13: goto tr207;
		case 32: goto tr598;
		case 51: goto tr642;
		case 125: goto tr600;
	}
	if ( 11 <= (*( sm->p)) && (*( sm->p)) <= 12 )
		goto tr599;
	goto tr597;
tr618:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ sm->c1 = sm->p; }
	goto st490;
st490:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof490;
case 490:
#line 11226 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 0: goto tr207;
		case 9: goto tr622;
		case 10: goto tr207;
		case 13: goto tr207;
		case 32: goto tr622;
		case 123: goto st467;
		case 124: goto tr643;
		case 125: goto tr623;
	}
	if ( 11 <= (*( sm->p)) && (*( sm->p)) <= 12 )
		goto st469;
	goto st474;
tr643:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ sm->b2 = sm->p; }
	goto st491;
st491:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof491;
case 491:
#line 11248 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 0: goto tr207;
		case 9: goto tr598;
		case 10: goto tr207;
		case 13: goto tr207;
		case 32: goto tr598;
		case 124: goto tr644;
		case 125: goto tr600;
	}
	if ( 11 <= (*( sm->p)) && (*( sm->p)) <= 12 )
		goto tr599;
	goto tr597;
tr644:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ sm->c1 = sm->p; }
	goto st492;
st492:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof492;
case 492:
#line 11269 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 0: goto tr207;
		case 9: goto tr602;
		case 10: goto tr207;
		case 13: goto tr207;
		case 32: goto tr602;
		case 47: goto st478;
		case 125: goto tr604;
	}
	if ( 11 <= (*( sm->p)) && (*( sm->p)) <= 12 )
		goto st469;
	goto st467;
tr593:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ sm->b2 = sm->p; }
	goto st493;
tr619:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ sm->c1 = sm->p; }
	goto st493;
st493:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof493;
case 493:
#line 11294 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 0: goto tr207;
		case 9: goto tr598;
		case 10: goto tr207;
		case 13: goto tr207;
		case 32: goto tr598;
		case 95: goto tr645;
		case 119: goto tr646;
		case 124: goto tr647;
		case 125: goto tr600;
	}
	if ( (*( sm->p)) < 48 ) {
		if ( 11 <= (*( sm->p)) && (*( sm->p)) <= 12 )
			goto tr599;
	} else if ( (*( sm->p)) > 57 ) {
		if ( (*( sm->p)) > 90 ) {
			if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
				goto tr642;
		} else if ( (*( sm->p)) >= 65 )
			goto tr642;
	} else
		goto tr642;
	goto tr597;
tr645:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ sm->c1 = sm->p; }
	goto st494;
st494:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof494;
case 494:
#line 11326 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 0: goto tr207;
		case 9: goto tr602;
		case 10: goto tr207;
		case 13: goto tr207;
		case 32: goto tr602;
		case 124: goto st478;
		case 125: goto tr604;
	}
	if ( 11 <= (*( sm->p)) && (*( sm->p)) <= 12 )
		goto st469;
	goto st467;
tr646:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ sm->c1 = sm->p; }
	goto st495;
st495:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof495;
case 495:
#line 11347 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 0: goto tr207;
		case 9: goto tr622;
		case 10: goto tr207;
		case 13: goto tr207;
		case 32: goto tr622;
		case 124: goto tr595;
		case 125: goto tr623;
	}
	if ( 11 <= (*( sm->p)) && (*( sm->p)) <= 12 )
		goto st469;
	goto st467;
tr647:
#line 75 "ext/dtext/dtext.cpp.rl"
	{ sm->c1 = sm->p; }
	goto st496;
st496:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof496;
case 496:
#line 11368 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 0: goto tr207;
		case 9: goto tr602;
		case 10: goto tr207;
		case 13: goto tr207;
		case 32: goto tr602;
		case 95: goto st497;
		case 125: goto tr604;
	}
	if ( 11 <= (*( sm->p)) && (*( sm->p)) <= 12 )
		goto st469;
	goto st467;
st497:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof497;
case 497:
	switch( (*( sm->p)) ) {
		case 0: goto tr207;
		case 9: goto tr602;
		case 10: goto tr207;
		case 13: goto tr207;
		case 32: goto tr602;
		case 124: goto st494;
		case 125: goto tr604;
	}
	if ( 11 <= (*( sm->p)) && (*( sm->p)) <= 12 )
		goto st469;
	goto st467;
tr596:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ sm->b2 = sm->p; }
	goto st498;
st498:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof498;
case 498:
#line 11405 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 9: goto tr582;
		case 32: goto tr582;
		case 124: goto tr583;
		case 125: goto tr650;
	}
	goto tr207;
tr650:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ sm->b2 = sm->p; }
	goto st1425;
st1425:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1425;
case 1425:
#line 11421 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) == 125 )
		goto st1420;
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1837;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto tr1837;
	} else
		goto tr1837;
	goto tr1836;
tr577:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ sm->b1 = sm->p; }
	goto st499;
st499:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof499;
case 499:
#line 11441 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 0: goto tr207;
		case 9: goto tr582;
		case 32: goto tr582;
		case 123: goto tr207;
		case 124: goto tr635;
		case 125: goto tr584;
	}
	if ( 10 <= (*( sm->p)) && (*( sm->p)) <= 13 )
		goto tr207;
	goto st461;
tr578:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ sm->b1 = sm->p; }
	goto st500;
st500:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof500;
case 500:
#line 11461 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 0: goto tr207;
		case 9: goto tr582;
		case 32: goto tr582;
		case 58: goto st501;
		case 123: goto tr207;
		case 124: goto tr641;
		case 125: goto tr584;
	}
	if ( 10 <= (*( sm->p)) && (*( sm->p)) <= 13 )
		goto tr207;
	goto st461;
st501:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof501;
case 501:
	switch( (*( sm->p)) ) {
		case 0: goto tr207;
		case 9: goto tr582;
		case 32: goto tr582;
		case 123: goto tr207;
		case 124: goto tr595;
		case 125: goto tr584;
	}
	if ( 10 <= (*( sm->p)) && (*( sm->p)) <= 13 )
		goto tr207;
	goto st461;
tr579:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ sm->b1 = sm->p; }
	goto st502;
st502:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof502;
case 502:
#line 11497 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 0: goto tr207;
		case 9: goto tr582;
		case 32: goto tr582;
		case 123: goto tr207;
		case 124: goto tr643;
		case 125: goto tr584;
	}
	if ( 10 <= (*( sm->p)) && (*( sm->p)) <= 13 )
		goto tr207;
	goto st461;
tr580:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ sm->b1 = sm->p; }
	goto st503;
st503:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof503;
case 503:
#line 11517 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 95: goto st504;
		case 119: goto st505;
		case 124: goto st506;
	}
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st465;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st465;
	} else
		goto st465;
	goto tr207;
st504:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof504;
case 504:
	if ( (*( sm->p)) == 124 )
		goto st465;
	goto tr207;
st505:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof505;
case 505:
	switch( (*( sm->p)) ) {
		case 9: goto tr582;
		case 32: goto tr582;
		case 124: goto tr595;
		case 125: goto tr584;
	}
	goto tr207;
st506:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof506;
case 506:
	if ( (*( sm->p)) == 95 )
		goto st507;
	goto tr207;
st507:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof507;
case 507:
	if ( (*( sm->p)) == 124 )
		goto st504;
	goto tr207;
st0:
 sm->cs = 0;
	goto _out;
tr1777:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 71 "ext/dtext/dtext.cpp.rl"
	{ sm->a1 = sm->p; }
#line 583 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 80;}
	goto st1426;
st1426:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1426;
case 1426:
#line 11579 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 82: goto tr1843;
		case 91: goto tr1820;
		case 114: goto tr1843;
		case 123: goto tr1821;
	}
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1819;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto tr1819;
	} else
		goto tr1819;
	goto tr1796;
tr1843:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 583 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 80;}
	goto st1427;
st1427:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1427;
case 1427:
#line 11605 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 84: goto tr1844;
		case 91: goto tr1820;
		case 116: goto tr1844;
		case 123: goto tr1821;
	}
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1819;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto tr1819;
	} else
		goto tr1819;
	goto tr1796;
tr1844:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 583 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 80;}
	goto st1428;
st1428:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1428;
case 1428:
#line 11631 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 73: goto tr1845;
		case 91: goto tr1820;
		case 105: goto tr1845;
		case 123: goto tr1821;
	}
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1819;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto tr1819;
	} else
		goto tr1819;
	goto tr1796;
tr1845:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 583 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 80;}
	goto st1429;
st1429:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1429;
case 1429:
#line 11657 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 83: goto tr1846;
		case 91: goto tr1820;
		case 115: goto tr1846;
		case 123: goto tr1821;
	}
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1819;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto tr1819;
	} else
		goto tr1819;
	goto tr1796;
tr1846:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 583 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 80;}
	goto st1430;
st1430:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1430;
case 1430:
#line 11683 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 84: goto tr1847;
		case 91: goto tr1820;
		case 116: goto tr1847;
		case 123: goto tr1821;
	}
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1819;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto tr1819;
	} else
		goto tr1819;
	goto tr1796;
tr1847:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 583 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 80;}
	goto st1431;
st1431:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1431;
case 1431:
#line 11709 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 32: goto st508;
		case 91: goto tr1820;
		case 123: goto tr1821;
	}
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1819;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto tr1819;
	} else
		goto tr1819;
	goto tr1796;
st508:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof508;
case 508:
	if ( (*( sm->p)) == 35 )
		goto st509;
	goto tr210;
st509:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof509;
case 509:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr657;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto tr658;
	} else
		goto tr658;
	goto tr210;
tr657:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 71 "ext/dtext/dtext.cpp.rl"
	{ sm->a1 = sm->p; }
#line 302 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 18;}
	goto st1432;
st1432:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1432;
case 1432:
#line 11756 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1850;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st518;
	} else
		goto st518;
	goto tr1849;
tr1850:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 302 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 18;}
	goto st1433;
st1433:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1433;
case 1433:
#line 11776 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1851;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st517;
	} else
		goto st517;
	goto tr1849;
tr1851:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 302 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 18;}
	goto st1434;
st1434:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1434;
case 1434:
#line 11796 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1852;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st516;
	} else
		goto st516;
	goto tr1849;
tr1852:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 302 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 18;}
	goto st1435;
st1435:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1435;
case 1435:
#line 11816 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1853;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st515;
	} else
		goto st515;
	goto tr1849;
tr1853:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 302 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 18;}
	goto st1436;
st1436:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1436;
case 1436:
#line 11836 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1854;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st514;
	} else
		goto st514;
	goto tr1849;
tr1854:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 302 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 18;}
	goto st1437;
st1437:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1437;
case 1437:
#line 11856 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1855;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st513;
	} else
		goto st513;
	goto tr1849;
tr1855:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 302 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 18;}
	goto st1438;
st1438:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1438;
case 1438:
#line 11876 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1856;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st512;
	} else
		goto st512;
	goto tr1849;
tr1856:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 302 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 18;}
	goto st1439;
st1439:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1439;
case 1439:
#line 11896 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1857;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st511;
	} else
		goto st511;
	goto tr1849;
tr1857:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 302 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 18;}
	goto st1440;
st1440:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1440;
case 1440:
#line 11916 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st1441;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st510;
	} else
		goto st510;
	goto tr1849;
st1441:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1441;
case 1441:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st1442;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st1443;
	} else
		goto st1443;
	goto tr1849;
st1442:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1442;
case 1442:
	if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
		goto st1442;
	goto tr1849;
st1443:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1443;
case 1443:
	goto tr1849;
st510:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof510;
case 510:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st1443;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st1443;
	} else
		goto st1443;
	goto tr207;
st511:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof511;
case 511:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st510;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st510;
	} else
		goto st510;
	goto tr207;
st512:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof512;
case 512:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st511;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st511;
	} else
		goto st511;
	goto tr207;
st513:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof513;
case 513:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st512;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st512;
	} else
		goto st512;
	goto tr207;
st514:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof514;
case 514:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st513;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st513;
	} else
		goto st513;
	goto tr207;
st515:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof515;
case 515:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st514;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st514;
	} else
		goto st514;
	goto tr207;
st516:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof516;
case 516:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st515;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st515;
	} else
		goto st515;
	goto tr207;
st517:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof517;
case 517:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st516;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st516;
	} else
		goto st516;
	goto tr207;
st518:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof518;
case 518:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st517;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st517;
	} else
		goto st517;
	goto tr207;
tr658:
#line 71 "ext/dtext/dtext.cpp.rl"
	{ sm->a1 = sm->p; }
	goto st519;
st519:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof519;
case 519:
#line 12076 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st518;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st518;
	} else
		goto st518;
	goto tr210;
tr1778:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 71 "ext/dtext/dtext.cpp.rl"
	{ sm->a1 = sm->p; }
#line 583 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 80;}
	goto st1444;
st1444:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1444;
case 1444:
#line 12098 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 79: goto tr1860;
		case 91: goto tr1820;
		case 111: goto tr1860;
		case 123: goto tr1821;
	}
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1819;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto tr1819;
	} else
		goto tr1819;
	goto tr1796;
tr1860:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 583 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 80;}
	goto st1445;
st1445:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1445;
case 1445:
#line 12124 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 79: goto tr1861;
		case 91: goto tr1820;
		case 111: goto tr1861;
		case 123: goto tr1821;
	}
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1819;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto tr1819;
	} else
		goto tr1819;
	goto tr1796;
tr1861:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 583 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 80;}
	goto st1446;
st1446:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1446;
case 1446:
#line 12150 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 75: goto tr1862;
		case 91: goto tr1820;
		case 107: goto tr1862;
		case 123: goto tr1821;
	}
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1819;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto tr1819;
	} else
		goto tr1819;
	goto tr1796;
tr1862:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 583 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 80;}
	goto st1447;
st1447:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1447;
case 1447:
#line 12176 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 32: goto st520;
		case 91: goto tr1820;
		case 123: goto tr1821;
	}
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1819;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto tr1819;
	} else
		goto tr1819;
	goto tr1796;
st520:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof520;
case 520:
	if ( (*( sm->p)) == 35 )
		goto st521;
	goto tr210;
st521:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof521;
case 521:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr670;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto tr671;
	} else
		goto tr671;
	goto tr210;
tr670:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 71 "ext/dtext/dtext.cpp.rl"
	{ sm->a1 = sm->p; }
#line 307 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 23;}
	goto st1448;
st1448:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1448;
case 1448:
#line 12223 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1865;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st530;
	} else
		goto st530;
	goto tr1864;
tr1865:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 307 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 23;}
	goto st1449;
st1449:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1449;
case 1449:
#line 12243 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1866;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st529;
	} else
		goto st529;
	goto tr1864;
tr1866:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 307 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 23;}
	goto st1450;
st1450:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1450;
case 1450:
#line 12263 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1867;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st528;
	} else
		goto st528;
	goto tr1864;
tr1867:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 307 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 23;}
	goto st1451;
st1451:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1451;
case 1451:
#line 12283 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1868;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st527;
	} else
		goto st527;
	goto tr1864;
tr1868:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 307 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 23;}
	goto st1452;
st1452:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1452;
case 1452:
#line 12303 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1869;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st526;
	} else
		goto st526;
	goto tr1864;
tr1869:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 307 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 23;}
	goto st1453;
st1453:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1453;
case 1453:
#line 12323 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1870;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st525;
	} else
		goto st525;
	goto tr1864;
tr1870:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 307 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 23;}
	goto st1454;
st1454:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1454;
case 1454:
#line 12343 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1871;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st524;
	} else
		goto st524;
	goto tr1864;
tr1871:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 307 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 23;}
	goto st1455;
st1455:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1455;
case 1455:
#line 12363 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1872;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st523;
	} else
		goto st523;
	goto tr1864;
tr1872:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 307 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 23;}
	goto st1456;
st1456:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1456;
case 1456:
#line 12383 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st1457;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st522;
	} else
		goto st522;
	goto tr1864;
st1457:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1457;
case 1457:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st1458;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st1459;
	} else
		goto st1459;
	goto tr1864;
st1458:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1458;
case 1458:
	if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
		goto st1458;
	goto tr1864;
st1459:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1459;
case 1459:
	goto tr1864;
st522:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof522;
case 522:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st1459;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st1459;
	} else
		goto st1459;
	goto tr207;
st523:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof523;
case 523:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st522;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st522;
	} else
		goto st522;
	goto tr207;
st524:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof524;
case 524:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st523;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st523;
	} else
		goto st523;
	goto tr207;
st525:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof525;
case 525:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st524;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st524;
	} else
		goto st524;
	goto tr207;
st526:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof526;
case 526:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st525;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st525;
	} else
		goto st525;
	goto tr207;
st527:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof527;
case 527:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st526;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st526;
	} else
		goto st526;
	goto tr207;
st528:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof528;
case 528:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st527;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st527;
	} else
		goto st527;
	goto tr207;
st529:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof529;
case 529:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st528;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st528;
	} else
		goto st528;
	goto tr207;
st530:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof530;
case 530:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st529;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st529;
	} else
		goto st529;
	goto tr207;
tr671:
#line 71 "ext/dtext/dtext.cpp.rl"
	{ sm->a1 = sm->p; }
	goto st531;
st531:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof531;
case 531:
#line 12543 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st530;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st530;
	} else
		goto st530;
	goto tr210;
tr1779:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 71 "ext/dtext/dtext.cpp.rl"
	{ sm->a1 = sm->p; }
#line 583 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 80;}
	goto st1460;
st1460:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1460;
case 1460:
#line 12565 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 79: goto tr1875;
		case 91: goto tr1820;
		case 111: goto tr1875;
		case 123: goto tr1821;
	}
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1819;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto tr1819;
	} else
		goto tr1819;
	goto tr1796;
tr1875:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 583 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 80;}
	goto st1461;
st1461:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1461;
case 1461:
#line 12591 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 77: goto tr1876;
		case 91: goto tr1820;
		case 109: goto tr1876;
		case 123: goto tr1821;
	}
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1819;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto tr1819;
	} else
		goto tr1819;
	goto tr1796;
tr1876:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 583 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 80;}
	goto st1462;
st1462:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1462;
case 1462:
#line 12617 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 77: goto tr1877;
		case 91: goto tr1820;
		case 109: goto tr1877;
		case 123: goto tr1821;
	}
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1819;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto tr1819;
	} else
		goto tr1819;
	goto tr1796;
tr1877:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 583 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 80;}
	goto st1463;
st1463:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1463;
case 1463:
#line 12643 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 69: goto tr1878;
		case 91: goto tr1820;
		case 101: goto tr1878;
		case 123: goto tr1821;
	}
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1819;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto tr1819;
	} else
		goto tr1819;
	goto tr1796;
tr1878:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 583 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 80;}
	goto st1464;
st1464:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1464;
case 1464:
#line 12669 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 78: goto tr1879;
		case 91: goto tr1820;
		case 110: goto tr1879;
		case 123: goto tr1821;
	}
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1819;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto tr1819;
	} else
		goto tr1819;
	goto tr1796;
tr1879:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 583 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 80;}
	goto st1465;
st1465:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1465;
case 1465:
#line 12695 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 84: goto tr1880;
		case 91: goto tr1820;
		case 116: goto tr1880;
		case 123: goto tr1821;
	}
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1819;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto tr1819;
	} else
		goto tr1819;
	goto tr1796;
tr1880:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 583 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 80;}
	goto st1466;
st1466:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1466;
case 1466:
#line 12721 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 32: goto st532;
		case 91: goto tr1820;
		case 123: goto tr1821;
	}
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1819;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto tr1819;
	} else
		goto tr1819;
	goto tr1796;
st532:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof532;
case 532:
	if ( (*( sm->p)) == 35 )
		goto st533;
	goto tr210;
st533:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof533;
case 533:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr683;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto tr684;
	} else
		goto tr684;
	goto tr210;
tr683:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 71 "ext/dtext/dtext.cpp.rl"
	{ sm->a1 = sm->p; }
#line 298 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 14;}
	goto st1467;
st1467:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1467;
case 1467:
#line 12768 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1883;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st542;
	} else
		goto st542;
	goto tr1882;
tr1883:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 298 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 14;}
	goto st1468;
st1468:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1468;
case 1468:
#line 12788 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1884;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st541;
	} else
		goto st541;
	goto tr1882;
tr1884:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 298 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 14;}
	goto st1469;
st1469:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1469;
case 1469:
#line 12808 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1885;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st540;
	} else
		goto st540;
	goto tr1882;
tr1885:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 298 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 14;}
	goto st1470;
st1470:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1470;
case 1470:
#line 12828 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1886;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st539;
	} else
		goto st539;
	goto tr1882;
tr1886:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 298 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 14;}
	goto st1471;
st1471:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1471;
case 1471:
#line 12848 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1887;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st538;
	} else
		goto st538;
	goto tr1882;
tr1887:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 298 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 14;}
	goto st1472;
st1472:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1472;
case 1472:
#line 12868 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1888;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st537;
	} else
		goto st537;
	goto tr1882;
tr1888:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 298 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 14;}
	goto st1473;
st1473:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1473;
case 1473:
#line 12888 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1889;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st536;
	} else
		goto st536;
	goto tr1882;
tr1889:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 298 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 14;}
	goto st1474;
st1474:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1474;
case 1474:
#line 12908 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1890;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st535;
	} else
		goto st535;
	goto tr1882;
tr1890:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 298 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 14;}
	goto st1475;
st1475:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1475;
case 1475:
#line 12928 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st1476;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st534;
	} else
		goto st534;
	goto tr1882;
st1476:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1476;
case 1476:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st1477;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st1478;
	} else
		goto st1478;
	goto tr1882;
st1477:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1477;
case 1477:
	if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
		goto st1477;
	goto tr1882;
st1478:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1478;
case 1478:
	goto tr1882;
st534:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof534;
case 534:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st1478;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st1478;
	} else
		goto st1478;
	goto tr207;
st535:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof535;
case 535:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st534;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st534;
	} else
		goto st534;
	goto tr207;
st536:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof536;
case 536:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st535;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st535;
	} else
		goto st535;
	goto tr207;
st537:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof537;
case 537:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st536;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st536;
	} else
		goto st536;
	goto tr207;
st538:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof538;
case 538:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st537;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st537;
	} else
		goto st537;
	goto tr207;
st539:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof539;
case 539:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st538;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st538;
	} else
		goto st538;
	goto tr207;
st540:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof540;
case 540:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st539;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st539;
	} else
		goto st539;
	goto tr207;
st541:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof541;
case 541:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st540;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st540;
	} else
		goto st540;
	goto tr207;
st542:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof542;
case 542:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st541;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st541;
	} else
		goto st541;
	goto tr207;
tr684:
#line 71 "ext/dtext/dtext.cpp.rl"
	{ sm->a1 = sm->p; }
	goto st543;
st543:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof543;
case 543:
#line 13088 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st542;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st542;
	} else
		goto st542;
	goto tr210;
tr1780:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 71 "ext/dtext/dtext.cpp.rl"
	{ sm->a1 = sm->p; }
#line 583 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 80;}
	goto st1479;
st1479:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1479;
case 1479:
#line 13110 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 77: goto tr1893;
		case 91: goto tr1820;
		case 109: goto tr1893;
		case 123: goto tr1821;
	}
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1819;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto tr1819;
	} else
		goto tr1819;
	goto tr1796;
tr1893:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 583 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 80;}
	goto st1480;
st1480:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1480;
case 1480:
#line 13136 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 65: goto tr1894;
		case 91: goto tr1820;
		case 97: goto tr1894;
		case 123: goto tr1821;
	}
	if ( (*( sm->p)) < 66 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1819;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 98 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto tr1819;
	} else
		goto tr1819;
	goto tr1796;
tr1894:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 583 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 80;}
	goto st1481;
st1481:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1481;
case 1481:
#line 13162 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 73: goto tr1895;
		case 91: goto tr1820;
		case 105: goto tr1895;
		case 123: goto tr1821;
	}
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1819;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto tr1819;
	} else
		goto tr1819;
	goto tr1796;
tr1895:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 583 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 80;}
	goto st1482;
st1482:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1482;
case 1482:
#line 13188 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 76: goto tr1896;
		case 91: goto tr1820;
		case 108: goto tr1896;
		case 123: goto tr1821;
	}
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1819;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto tr1819;
	} else
		goto tr1819;
	goto tr1796;
tr1896:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 583 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 80;}
	goto st1483;
st1483:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1483;
case 1483:
#line 13214 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 32: goto st544;
		case 91: goto tr1820;
		case 123: goto tr1821;
	}
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1819;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto tr1819;
	} else
		goto tr1819;
	goto tr1796;
st544:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof544;
case 544:
	if ( (*( sm->p)) == 35 )
		goto st545;
	goto tr210;
st545:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof545;
case 545:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr696;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto tr697;
	} else
		goto tr697;
	goto tr210;
tr696:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 71 "ext/dtext/dtext.cpp.rl"
	{ sm->a1 = sm->p; }
#line 299 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 15;}
	goto st1484;
st1484:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1484;
case 1484:
#line 13261 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) == 47 )
		goto tr1899;
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1900;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st555;
	} else
		goto st555;
	goto tr1898;
tr1899:
#line 72 "ext/dtext/dtext.cpp.rl"
	{ sm->a2 = sm->p; }
	goto st546;
st546:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof546;
case 546:
#line 13281 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 45: goto tr699;
		case 61: goto tr699;
	}
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr699;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto tr699;
	} else
		goto tr699;
	goto tr698;
tr699:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ sm->b1 = sm->p; }
	goto st1485;
st1485:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1485;
case 1485:
#line 13303 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 45: goto st1485;
		case 61: goto st1485;
	}
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st1485;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st1485;
	} else
		goto st1485;
	goto tr1901;
tr1900:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 299 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 15;}
	goto st1486;
st1486:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1486;
case 1486:
#line 13327 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) == 47 )
		goto tr1899;
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1903;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st554;
	} else
		goto st554;
	goto tr1898;
tr1903:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 299 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 15;}
	goto st1487;
st1487:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1487;
case 1487:
#line 13349 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) == 47 )
		goto tr1899;
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1904;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st553;
	} else
		goto st553;
	goto tr1898;
tr1904:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 299 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 15;}
	goto st1488;
st1488:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1488;
case 1488:
#line 13371 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) == 47 )
		goto tr1899;
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1905;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st552;
	} else
		goto st552;
	goto tr1898;
tr1905:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 299 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 15;}
	goto st1489;
st1489:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1489;
case 1489:
#line 13393 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) == 47 )
		goto tr1899;
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1906;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st551;
	} else
		goto st551;
	goto tr1898;
tr1906:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 299 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 15;}
	goto st1490;
st1490:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1490;
case 1490:
#line 13415 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) == 47 )
		goto tr1899;
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1907;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st550;
	} else
		goto st550;
	goto tr1898;
tr1907:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 299 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 15;}
	goto st1491;
st1491:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1491;
case 1491:
#line 13437 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) == 47 )
		goto tr1899;
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1908;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st549;
	} else
		goto st549;
	goto tr1898;
tr1908:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 299 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 15;}
	goto st1492;
st1492:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1492;
case 1492:
#line 13459 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) == 47 )
		goto tr1899;
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1909;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st548;
	} else
		goto st548;
	goto tr1898;
tr1909:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 299 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 15;}
	goto st1493;
st1493:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1493;
case 1493:
#line 13481 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) == 47 )
		goto tr1899;
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1910;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st547;
	} else
		goto st547;
	goto tr1898;
tr1910:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
	goto st1494;
st1494:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1494;
case 1494:
#line 13501 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) == 47 )
		goto tr1899;
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1911;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto tr700;
	} else
		goto tr700;
	goto tr1898;
tr1911:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
	goto st1495;
st1495:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1495;
case 1495:
#line 13521 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) == 47 )
		goto tr1899;
	if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
		goto tr1911;
	goto tr1898;
tr700:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
	goto st1496;
st1496:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1496;
case 1496:
#line 13535 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) == 47 )
		goto tr1899;
	goto tr1898;
st547:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof547;
case 547:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr700;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto tr700;
	} else
		goto tr700;
	goto tr207;
st548:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof548;
case 548:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st547;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st547;
	} else
		goto st547;
	goto tr207;
st549:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof549;
case 549:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st548;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st548;
	} else
		goto st548;
	goto tr207;
st550:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof550;
case 550:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st549;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st549;
	} else
		goto st549;
	goto tr207;
st551:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof551;
case 551:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st550;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st550;
	} else
		goto st550;
	goto tr207;
st552:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof552;
case 552:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st551;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st551;
	} else
		goto st551;
	goto tr207;
st553:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof553;
case 553:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st552;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st552;
	} else
		goto st552;
	goto tr207;
st554:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof554;
case 554:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st553;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st553;
	} else
		goto st553;
	goto tr207;
st555:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof555;
case 555:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st554;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st554;
	} else
		goto st554;
	goto tr207;
tr697:
#line 71 "ext/dtext/dtext.cpp.rl"
	{ sm->a1 = sm->p; }
	goto st556;
st556:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof556;
case 556:
#line 13664 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st555;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st555;
	} else
		goto st555;
	goto tr210;
tr1781:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 71 "ext/dtext/dtext.cpp.rl"
	{ sm->a1 = sm->p; }
#line 583 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 80;}
	goto st1497;
st1497:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1497;
case 1497:
#line 13686 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 79: goto tr1912;
		case 91: goto tr1820;
		case 111: goto tr1912;
		case 123: goto tr1821;
	}
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1819;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto tr1819;
	} else
		goto tr1819;
	goto tr1796;
tr1912:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 583 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 80;}
	goto st1498;
st1498:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1498;
case 1498:
#line 13712 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 82: goto tr1913;
		case 91: goto tr1820;
		case 114: goto tr1913;
		case 123: goto tr1821;
	}
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1819;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto tr1819;
	} else
		goto tr1819;
	goto tr1796;
tr1913:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 583 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 80;}
	goto st1499;
st1499:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1499;
case 1499:
#line 13738 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 85: goto tr1914;
		case 91: goto tr1820;
		case 117: goto tr1914;
		case 123: goto tr1821;
	}
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1819;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto tr1819;
	} else
		goto tr1819;
	goto tr1796;
tr1914:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 583 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 80;}
	goto st1500;
st1500:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1500;
case 1500:
#line 13764 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 77: goto tr1915;
		case 91: goto tr1820;
		case 109: goto tr1915;
		case 123: goto tr1821;
	}
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1819;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto tr1819;
	} else
		goto tr1819;
	goto tr1796;
tr1915:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 583 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 80;}
	goto st1501;
st1501:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1501;
case 1501:
#line 13790 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 32: goto st557;
		case 91: goto tr1820;
		case 123: goto tr1821;
	}
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1819;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto tr1819;
	} else
		goto tr1819;
	goto tr1796;
st557:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof557;
case 557:
	if ( (*( sm->p)) == 35 )
		goto st558;
	goto tr210;
st558:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof558;
case 558:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr711;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto tr712;
	} else
		goto tr712;
	goto tr210;
tr711:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 71 "ext/dtext/dtext.cpp.rl"
	{ sm->a1 = sm->p; }
#line 296 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 12;}
	goto st1502;
st1502:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1502;
case 1502:
#line 13837 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1918;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st567;
	} else
		goto st567;
	goto tr1917;
tr1918:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 296 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 12;}
	goto st1503;
st1503:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1503;
case 1503:
#line 13857 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1919;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st566;
	} else
		goto st566;
	goto tr1917;
tr1919:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 296 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 12;}
	goto st1504;
st1504:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1504;
case 1504:
#line 13877 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1920;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st565;
	} else
		goto st565;
	goto tr1917;
tr1920:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 296 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 12;}
	goto st1505;
st1505:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1505;
case 1505:
#line 13897 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1921;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st564;
	} else
		goto st564;
	goto tr1917;
tr1921:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 296 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 12;}
	goto st1506;
st1506:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1506;
case 1506:
#line 13917 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1922;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st563;
	} else
		goto st563;
	goto tr1917;
tr1922:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 296 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 12;}
	goto st1507;
st1507:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1507;
case 1507:
#line 13937 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1923;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st562;
	} else
		goto st562;
	goto tr1917;
tr1923:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 296 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 12;}
	goto st1508;
st1508:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1508;
case 1508:
#line 13957 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1924;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st561;
	} else
		goto st561;
	goto tr1917;
tr1924:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 296 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 12;}
	goto st1509;
st1509:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1509;
case 1509:
#line 13977 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1925;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st560;
	} else
		goto st560;
	goto tr1917;
tr1925:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 296 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 12;}
	goto st1510;
st1510:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1510;
case 1510:
#line 13997 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st1511;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st559;
	} else
		goto st559;
	goto tr1917;
st1511:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1511;
case 1511:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st1512;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st1513;
	} else
		goto st1513;
	goto tr1917;
st1512:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1512;
case 1512:
	if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
		goto st1512;
	goto tr1917;
st1513:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1513;
case 1513:
	goto tr1917;
st559:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof559;
case 559:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st1513;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st1513;
	} else
		goto st1513;
	goto tr207;
st560:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof560;
case 560:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st559;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st559;
	} else
		goto st559;
	goto tr207;
st561:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof561;
case 561:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st560;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st560;
	} else
		goto st560;
	goto tr207;
st562:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof562;
case 562:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st561;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st561;
	} else
		goto st561;
	goto tr207;
st563:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof563;
case 563:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st562;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st562;
	} else
		goto st562;
	goto tr207;
st564:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof564;
case 564:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st563;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st563;
	} else
		goto st563;
	goto tr207;
st565:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof565;
case 565:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st564;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st564;
	} else
		goto st564;
	goto tr207;
st566:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof566;
case 566:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st565;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st565;
	} else
		goto st565;
	goto tr207;
st567:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof567;
case 567:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st566;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st566;
	} else
		goto st566;
	goto tr207;
tr712:
#line 71 "ext/dtext/dtext.cpp.rl"
	{ sm->a1 = sm->p; }
	goto st568;
st568:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof568;
case 568:
#line 14157 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st567;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st567;
	} else
		goto st567;
	goto tr210;
tr1782:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 71 "ext/dtext/dtext.cpp.rl"
	{ sm->a1 = sm->p; }
#line 583 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 80;}
	goto st1514;
st1514:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1514;
case 1514:
#line 14179 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 84: goto tr1928;
		case 91: goto tr1820;
		case 116: goto tr1928;
		case 123: goto tr1821;
	}
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1819;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto tr1819;
	} else
		goto tr1819;
	goto tr1796;
tr1928:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 583 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 80;}
	goto st1515;
st1515:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1515;
case 1515:
#line 14205 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 84: goto tr1929;
		case 91: goto tr1820;
		case 116: goto tr1929;
		case 123: goto tr1821;
	}
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1819;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto tr1819;
	} else
		goto tr1819;
	goto tr1796;
tr1929:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 583 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 80;}
	goto st1516;
st1516:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1516;
case 1516:
#line 14231 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 80: goto tr1930;
		case 91: goto tr1820;
		case 112: goto tr1930;
		case 123: goto tr1821;
	}
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1819;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto tr1819;
	} else
		goto tr1819;
	goto tr1796;
tr1930:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 583 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 80;}
	goto st1517;
st1517:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1517;
case 1517:
#line 14257 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 58: goto st569;
		case 83: goto tr1932;
		case 91: goto tr1820;
		case 115: goto tr1932;
		case 123: goto tr1821;
	}
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1819;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto tr1819;
	} else
		goto tr1819;
	goto tr1796;
st569:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof569;
case 569:
	if ( (*( sm->p)) == 47 )
		goto st570;
	goto tr210;
st570:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof570;
case 570:
	if ( (*( sm->p)) == 47 )
		goto st571;
	goto tr210;
st571:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof571;
case 571:
	switch( (*( sm->p)) ) {
		case 45: goto st573;
		case 95: goto st573;
	}
	if ( (*( sm->p)) < -16 ) {
		if ( (*( sm->p)) > -33 ) {
			if ( -32 <= (*( sm->p)) && (*( sm->p)) <= -17 )
				goto st574;
		} else if ( (*( sm->p)) >= -62 )
			goto st572;
	} else if ( (*( sm->p)) > -12 ) {
		if ( (*( sm->p)) < 65 ) {
			if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
				goto st573;
		} else if ( (*( sm->p)) > 90 ) {
			if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
				goto st573;
		} else
			goto st573;
	} else
		goto st575;
	goto tr210;
st572:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof572;
case 572:
	if ( (*( sm->p)) <= -65 )
		goto st573;
	goto tr210;
st573:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof573;
case 573:
	switch( (*( sm->p)) ) {
		case 45: goto st573;
		case 46: goto st576;
		case 95: goto st573;
	}
	if ( (*( sm->p)) < -16 ) {
		if ( (*( sm->p)) > -33 ) {
			if ( -32 <= (*( sm->p)) && (*( sm->p)) <= -17 )
				goto st574;
		} else if ( (*( sm->p)) >= -62 )
			goto st572;
	} else if ( (*( sm->p)) > -12 ) {
		if ( (*( sm->p)) < 65 ) {
			if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
				goto st573;
		} else if ( (*( sm->p)) > 90 ) {
			if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
				goto st573;
		} else
			goto st573;
	} else
		goto st575;
	goto tr210;
st574:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof574;
case 574:
	if ( (*( sm->p)) <= -65 )
		goto st572;
	goto tr210;
st575:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof575;
case 575:
	if ( (*( sm->p)) <= -65 )
		goto st574;
	goto tr210;
st576:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof576;
case 576:
	switch( (*( sm->p)) ) {
		case -30: goto st579;
		case -29: goto st582;
		case -17: goto st584;
		case 45: goto tr736;
		case 95: goto tr736;
	}
	if ( (*( sm->p)) < -16 ) {
		if ( (*( sm->p)) > -33 ) {
			if ( -32 <= (*( sm->p)) && (*( sm->p)) <= -18 )
				goto st578;
		} else if ( (*( sm->p)) >= -62 )
			goto st577;
	} else if ( (*( sm->p)) > -12 ) {
		if ( (*( sm->p)) < 65 ) {
			if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
				goto tr736;
		} else if ( (*( sm->p)) > 90 ) {
			if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
				goto tr736;
		} else
			goto tr736;
	} else
		goto st587;
	goto tr207;
st577:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof577;
case 577:
	if ( (*( sm->p)) <= -65 )
		goto tr736;
	goto tr207;
tr736:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 346 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 38;}
	goto st1518;
st1518:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1518;
case 1518:
#line 14408 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case -30: goto st579;
		case -29: goto st582;
		case -17: goto st584;
		case 35: goto tr739;
		case 46: goto st576;
		case 47: goto tr740;
		case 58: goto st621;
		case 63: goto st610;
		case 95: goto tr736;
	}
	if ( (*( sm->p)) < -16 ) {
		if ( (*( sm->p)) > -33 ) {
			if ( -32 <= (*( sm->p)) && (*( sm->p)) <= -18 )
				goto st578;
		} else if ( (*( sm->p)) >= -62 )
			goto st577;
	} else if ( (*( sm->p)) > -12 ) {
		if ( (*( sm->p)) < 65 ) {
			if ( 45 <= (*( sm->p)) && (*( sm->p)) <= 57 )
				goto tr736;
		} else if ( (*( sm->p)) > 90 ) {
			if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
				goto tr736;
		} else
			goto tr736;
	} else
		goto st587;
	goto tr1933;
st578:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof578;
case 578:
	if ( (*( sm->p)) <= -65 )
		goto st577;
	goto tr207;
st579:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof579;
case 579:
	if ( (*( sm->p)) == -99 )
		goto st580;
	if ( (*( sm->p)) <= -65 )
		goto st577;
	goto tr207;
st580:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof580;
case 580:
	if ( (*( sm->p)) == -83 )
		goto st581;
	if ( (*( sm->p)) <= -65 )
		goto tr736;
	goto tr207;
st581:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof581;
case 581:
	switch( (*( sm->p)) ) {
		case -30: goto st579;
		case -29: goto st582;
		case -17: goto st584;
		case 35: goto tr739;
		case 46: goto st576;
		case 47: goto tr740;
		case 58: goto st621;
		case 63: goto st610;
		case 95: goto tr736;
	}
	if ( (*( sm->p)) < -16 ) {
		if ( (*( sm->p)) > -33 ) {
			if ( -32 <= (*( sm->p)) && (*( sm->p)) <= -18 )
				goto st578;
		} else if ( (*( sm->p)) >= -62 )
			goto st577;
	} else if ( (*( sm->p)) > -12 ) {
		if ( (*( sm->p)) < 65 ) {
			if ( 45 <= (*( sm->p)) && (*( sm->p)) <= 57 )
				goto tr736;
		} else if ( (*( sm->p)) > 90 ) {
			if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
				goto tr736;
		} else
			goto tr736;
	} else
		goto st587;
	goto tr207;
st582:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof582;
case 582:
	if ( (*( sm->p)) == -128 )
		goto st583;
	if ( -127 <= (*( sm->p)) && (*( sm->p)) <= -65 )
		goto st577;
	goto tr207;
st583:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof583;
case 583:
	if ( (*( sm->p)) < -120 ) {
		if ( (*( sm->p)) > -126 ) {
			if ( -125 <= (*( sm->p)) && (*( sm->p)) <= -121 )
				goto tr736;
		} else
			goto st581;
	} else if ( (*( sm->p)) > -111 ) {
		if ( (*( sm->p)) < -108 ) {
			if ( -110 <= (*( sm->p)) && (*( sm->p)) <= -109 )
				goto tr736;
		} else if ( (*( sm->p)) > -100 ) {
			if ( -99 <= (*( sm->p)) && (*( sm->p)) <= -65 )
				goto tr736;
		} else
			goto st581;
	} else
		goto st581;
	goto tr207;
st584:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof584;
case 584:
	switch( (*( sm->p)) ) {
		case -68: goto st585;
		case -67: goto st586;
	}
	if ( (*( sm->p)) <= -65 )
		goto st577;
	goto tr207;
st585:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof585;
case 585:
	switch( (*( sm->p)) ) {
		case -119: goto st581;
		case -67: goto st581;
	}
	if ( (*( sm->p)) <= -65 )
		goto tr736;
	goto tr207;
st586:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof586;
case 586:
	switch( (*( sm->p)) ) {
		case -99: goto st581;
		case -96: goto st581;
		case -93: goto st581;
	}
	if ( (*( sm->p)) <= -65 )
		goto tr736;
	goto tr207;
st587:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof587;
case 587:
	if ( (*( sm->p)) <= -65 )
		goto st578;
	goto tr207;
tr739:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
	goto st1519;
st1519:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1519;
case 1519:
#line 14576 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case -30: goto st590;
		case -29: goto st592;
		case -17: goto st594;
		case 32: goto tr1933;
		case 34: goto st598;
		case 35: goto tr1933;
		case 39: goto st598;
		case 44: goto st598;
		case 46: goto st598;
		case 60: goto tr1933;
		case 62: goto tr1933;
		case 63: goto st598;
		case 91: goto tr1933;
		case 93: goto tr1933;
	}
	if ( (*( sm->p)) < -16 ) {
		if ( (*( sm->p)) < -62 ) {
			if ( (*( sm->p)) <= -63 )
				goto tr1933;
		} else if ( (*( sm->p)) > -33 ) {
			if ( -32 <= (*( sm->p)) && (*( sm->p)) <= -18 )
				goto st589;
		} else
			goto st588;
	} else if ( (*( sm->p)) > -12 ) {
		if ( (*( sm->p)) < 9 ) {
			if ( -11 <= (*( sm->p)) && (*( sm->p)) <= 0 )
				goto tr1933;
		} else if ( (*( sm->p)) > 13 ) {
			if ( 58 <= (*( sm->p)) && (*( sm->p)) <= 59 )
				goto st598;
		} else
			goto tr1933;
	} else
		goto st597;
	goto tr739;
st588:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof588;
case 588:
	if ( (*( sm->p)) <= -65 )
		goto tr739;
	goto tr746;
st589:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof589;
case 589:
	if ( (*( sm->p)) <= -65 )
		goto st588;
	goto tr746;
st590:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof590;
case 590:
	if ( (*( sm->p)) == -99 )
		goto st591;
	if ( (*( sm->p)) <= -65 )
		goto st588;
	goto tr746;
st591:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof591;
case 591:
	if ( (*( sm->p)) > -84 ) {
		if ( -82 <= (*( sm->p)) && (*( sm->p)) <= -65 )
			goto tr739;
	} else
		goto tr739;
	goto tr746;
st592:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof592;
case 592:
	if ( (*( sm->p)) == -128 )
		goto st593;
	if ( -127 <= (*( sm->p)) && (*( sm->p)) <= -65 )
		goto st588;
	goto tr746;
st593:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof593;
case 593:
	if ( (*( sm->p)) < -110 ) {
		if ( -125 <= (*( sm->p)) && (*( sm->p)) <= -121 )
			goto tr739;
	} else if ( (*( sm->p)) > -109 ) {
		if ( -99 <= (*( sm->p)) && (*( sm->p)) <= -65 )
			goto tr739;
	} else
		goto tr739;
	goto tr746;
st594:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof594;
case 594:
	switch( (*( sm->p)) ) {
		case -68: goto st595;
		case -67: goto st596;
	}
	if ( (*( sm->p)) <= -65 )
		goto st588;
	goto tr746;
st595:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof595;
case 595:
	if ( (*( sm->p)) < -118 ) {
		if ( (*( sm->p)) <= -120 )
			goto tr739;
	} else if ( (*( sm->p)) > -68 ) {
		if ( -66 <= (*( sm->p)) && (*( sm->p)) <= -65 )
			goto tr739;
	} else
		goto tr739;
	goto tr746;
st596:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof596;
case 596:
	if ( (*( sm->p)) < -98 ) {
		if ( (*( sm->p)) <= -100 )
			goto tr739;
	} else if ( (*( sm->p)) > -97 ) {
		if ( (*( sm->p)) > -94 ) {
			if ( -92 <= (*( sm->p)) && (*( sm->p)) <= -65 )
				goto tr739;
		} else if ( (*( sm->p)) >= -95 )
			goto tr739;
	} else
		goto tr739;
	goto tr746;
st597:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof597;
case 597:
	if ( (*( sm->p)) <= -65 )
		goto st589;
	goto tr746;
st598:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof598;
case 598:
	switch( (*( sm->p)) ) {
		case -30: goto st590;
		case -29: goto st592;
		case -17: goto st594;
		case 32: goto tr746;
		case 34: goto st598;
		case 35: goto tr746;
		case 39: goto st598;
		case 44: goto st598;
		case 46: goto st598;
		case 60: goto tr746;
		case 62: goto tr746;
		case 63: goto st598;
		case 91: goto tr746;
		case 93: goto tr746;
	}
	if ( (*( sm->p)) < -16 ) {
		if ( (*( sm->p)) < -62 ) {
			if ( (*( sm->p)) <= -63 )
				goto tr746;
		} else if ( (*( sm->p)) > -33 ) {
			if ( -32 <= (*( sm->p)) && (*( sm->p)) <= -18 )
				goto st589;
		} else
			goto st588;
	} else if ( (*( sm->p)) > -12 ) {
		if ( (*( sm->p)) < 9 ) {
			if ( -11 <= (*( sm->p)) && (*( sm->p)) <= 0 )
				goto tr746;
		} else if ( (*( sm->p)) > 13 ) {
			if ( 58 <= (*( sm->p)) && (*( sm->p)) <= 59 )
				goto st598;
		} else
			goto tr746;
	} else
		goto st597;
	goto tr739;
tr740:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 346 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 38;}
	goto st1520;
st1520:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1520;
case 1520:
#line 14767 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case -30: goto st601;
		case -29: goto st603;
		case -17: goto st605;
		case 32: goto tr1933;
		case 34: goto st609;
		case 35: goto tr739;
		case 39: goto st609;
		case 44: goto st609;
		case 46: goto st609;
		case 60: goto tr1933;
		case 62: goto tr1933;
		case 63: goto st610;
		case 91: goto tr1933;
		case 93: goto tr1933;
	}
	if ( (*( sm->p)) < -16 ) {
		if ( (*( sm->p)) < -62 ) {
			if ( (*( sm->p)) <= -63 )
				goto tr1933;
		} else if ( (*( sm->p)) > -33 ) {
			if ( -32 <= (*( sm->p)) && (*( sm->p)) <= -18 )
				goto st600;
		} else
			goto st599;
	} else if ( (*( sm->p)) > -12 ) {
		if ( (*( sm->p)) < 9 ) {
			if ( -11 <= (*( sm->p)) && (*( sm->p)) <= 0 )
				goto tr1933;
		} else if ( (*( sm->p)) > 13 ) {
			if ( 58 <= (*( sm->p)) && (*( sm->p)) <= 59 )
				goto st609;
		} else
			goto tr1933;
	} else
		goto st608;
	goto tr740;
st599:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof599;
case 599:
	if ( (*( sm->p)) <= -65 )
		goto tr740;
	goto tr746;
st600:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof600;
case 600:
	if ( (*( sm->p)) <= -65 )
		goto st599;
	goto tr746;
st601:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof601;
case 601:
	if ( (*( sm->p)) == -99 )
		goto st602;
	if ( (*( sm->p)) <= -65 )
		goto st599;
	goto tr746;
st602:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof602;
case 602:
	if ( (*( sm->p)) > -84 ) {
		if ( -82 <= (*( sm->p)) && (*( sm->p)) <= -65 )
			goto tr740;
	} else
		goto tr740;
	goto tr746;
st603:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof603;
case 603:
	if ( (*( sm->p)) == -128 )
		goto st604;
	if ( -127 <= (*( sm->p)) && (*( sm->p)) <= -65 )
		goto st599;
	goto tr746;
st604:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof604;
case 604:
	if ( (*( sm->p)) < -110 ) {
		if ( -125 <= (*( sm->p)) && (*( sm->p)) <= -121 )
			goto tr740;
	} else if ( (*( sm->p)) > -109 ) {
		if ( -99 <= (*( sm->p)) && (*( sm->p)) <= -65 )
			goto tr740;
	} else
		goto tr740;
	goto tr746;
st605:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof605;
case 605:
	switch( (*( sm->p)) ) {
		case -68: goto st606;
		case -67: goto st607;
	}
	if ( (*( sm->p)) <= -65 )
		goto st599;
	goto tr746;
st606:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof606;
case 606:
	if ( (*( sm->p)) < -118 ) {
		if ( (*( sm->p)) <= -120 )
			goto tr740;
	} else if ( (*( sm->p)) > -68 ) {
		if ( -66 <= (*( sm->p)) && (*( sm->p)) <= -65 )
			goto tr740;
	} else
		goto tr740;
	goto tr746;
st607:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof607;
case 607:
	if ( (*( sm->p)) < -98 ) {
		if ( (*( sm->p)) <= -100 )
			goto tr740;
	} else if ( (*( sm->p)) > -97 ) {
		if ( (*( sm->p)) > -94 ) {
			if ( -92 <= (*( sm->p)) && (*( sm->p)) <= -65 )
				goto tr740;
		} else if ( (*( sm->p)) >= -95 )
			goto tr740;
	} else
		goto tr740;
	goto tr746;
st608:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof608;
case 608:
	if ( (*( sm->p)) <= -65 )
		goto st600;
	goto tr746;
st609:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof609;
case 609:
	switch( (*( sm->p)) ) {
		case -30: goto st601;
		case -29: goto st603;
		case -17: goto st605;
		case 32: goto tr746;
		case 34: goto st609;
		case 35: goto tr739;
		case 39: goto st609;
		case 44: goto st609;
		case 46: goto st609;
		case 60: goto tr746;
		case 62: goto tr746;
		case 63: goto st610;
		case 91: goto tr746;
		case 93: goto tr746;
	}
	if ( (*( sm->p)) < -16 ) {
		if ( (*( sm->p)) < -62 ) {
			if ( (*( sm->p)) <= -63 )
				goto tr746;
		} else if ( (*( sm->p)) > -33 ) {
			if ( -32 <= (*( sm->p)) && (*( sm->p)) <= -18 )
				goto st600;
		} else
			goto st599;
	} else if ( (*( sm->p)) > -12 ) {
		if ( (*( sm->p)) < 9 ) {
			if ( -11 <= (*( sm->p)) && (*( sm->p)) <= 0 )
				goto tr746;
		} else if ( (*( sm->p)) > 13 ) {
			if ( 58 <= (*( sm->p)) && (*( sm->p)) <= 59 )
				goto st609;
		} else
			goto tr746;
	} else
		goto st608;
	goto tr740;
st610:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof610;
case 610:
	switch( (*( sm->p)) ) {
		case -30: goto st613;
		case -29: goto st615;
		case -17: goto st617;
		case 32: goto tr207;
		case 34: goto st610;
		case 35: goto tr739;
		case 39: goto st610;
		case 44: goto st610;
		case 46: goto st610;
		case 63: goto st610;
	}
	if ( (*( sm->p)) < -16 ) {
		if ( (*( sm->p)) < -62 ) {
			if ( (*( sm->p)) <= -63 )
				goto tr207;
		} else if ( (*( sm->p)) > -33 ) {
			if ( -32 <= (*( sm->p)) && (*( sm->p)) <= -18 )
				goto st612;
		} else
			goto st611;
	} else if ( (*( sm->p)) > -12 ) {
		if ( (*( sm->p)) < 9 ) {
			if ( -11 <= (*( sm->p)) && (*( sm->p)) <= 0 )
				goto tr207;
		} else if ( (*( sm->p)) > 13 ) {
			if ( 58 <= (*( sm->p)) && (*( sm->p)) <= 59 )
				goto st610;
		} else
			goto tr207;
	} else
		goto st620;
	goto tr775;
st611:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof611;
case 611:
	if ( (*( sm->p)) <= -65 )
		goto tr775;
	goto tr207;
tr775:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 346 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 38;}
	goto st1521;
st1521:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1521;
case 1521:
#line 15002 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case -30: goto st613;
		case -29: goto st615;
		case -17: goto st617;
		case 32: goto tr1933;
		case 34: goto st610;
		case 35: goto tr739;
		case 39: goto st610;
		case 44: goto st610;
		case 46: goto st610;
		case 63: goto st610;
	}
	if ( (*( sm->p)) < -16 ) {
		if ( (*( sm->p)) < -62 ) {
			if ( (*( sm->p)) <= -63 )
				goto tr1933;
		} else if ( (*( sm->p)) > -33 ) {
			if ( -32 <= (*( sm->p)) && (*( sm->p)) <= -18 )
				goto st612;
		} else
			goto st611;
	} else if ( (*( sm->p)) > -12 ) {
		if ( (*( sm->p)) < 9 ) {
			if ( -11 <= (*( sm->p)) && (*( sm->p)) <= 0 )
				goto tr1933;
		} else if ( (*( sm->p)) > 13 ) {
			if ( 58 <= (*( sm->p)) && (*( sm->p)) <= 59 )
				goto st610;
		} else
			goto tr1933;
	} else
		goto st620;
	goto tr775;
st612:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof612;
case 612:
	if ( (*( sm->p)) <= -65 )
		goto st611;
	goto tr207;
st613:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof613;
case 613:
	if ( (*( sm->p)) == -99 )
		goto st614;
	if ( (*( sm->p)) <= -65 )
		goto st611;
	goto tr207;
st614:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof614;
case 614:
	if ( (*( sm->p)) > -84 ) {
		if ( -82 <= (*( sm->p)) && (*( sm->p)) <= -65 )
			goto tr775;
	} else
		goto tr775;
	goto tr207;
st615:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof615;
case 615:
	if ( (*( sm->p)) == -128 )
		goto st616;
	if ( -127 <= (*( sm->p)) && (*( sm->p)) <= -65 )
		goto st611;
	goto tr207;
st616:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof616;
case 616:
	if ( (*( sm->p)) < -110 ) {
		if ( -125 <= (*( sm->p)) && (*( sm->p)) <= -121 )
			goto tr775;
	} else if ( (*( sm->p)) > -109 ) {
		if ( -99 <= (*( sm->p)) && (*( sm->p)) <= -65 )
			goto tr775;
	} else
		goto tr775;
	goto tr207;
st617:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof617;
case 617:
	switch( (*( sm->p)) ) {
		case -68: goto st618;
		case -67: goto st619;
	}
	if ( (*( sm->p)) <= -65 )
		goto st611;
	goto tr207;
st618:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof618;
case 618:
	if ( (*( sm->p)) < -118 ) {
		if ( (*( sm->p)) <= -120 )
			goto tr775;
	} else if ( (*( sm->p)) > -68 ) {
		if ( -66 <= (*( sm->p)) && (*( sm->p)) <= -65 )
			goto tr775;
	} else
		goto tr775;
	goto tr207;
st619:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof619;
case 619:
	if ( (*( sm->p)) < -98 ) {
		if ( (*( sm->p)) <= -100 )
			goto tr775;
	} else if ( (*( sm->p)) > -97 ) {
		if ( (*( sm->p)) > -94 ) {
			if ( -92 <= (*( sm->p)) && (*( sm->p)) <= -65 )
				goto tr775;
		} else if ( (*( sm->p)) >= -95 )
			goto tr775;
	} else
		goto tr775;
	goto tr207;
st620:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof620;
case 620:
	if ( (*( sm->p)) <= -65 )
		goto st612;
	goto tr207;
st621:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof621;
case 621:
	if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
		goto tr780;
	goto tr207;
tr780:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 346 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 38;}
	goto st1522;
st1522:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1522;
case 1522:
#line 15148 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 35: goto tr739;
		case 47: goto tr740;
		case 63: goto st610;
	}
	if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
		goto tr780;
	goto tr1933;
tr1932:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 583 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 80;}
	goto st1523;
st1523:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1523;
case 1523:
#line 15167 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 58: goto st569;
		case 91: goto tr1820;
		case 123: goto tr1821;
	}
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1819;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto tr1819;
	} else
		goto tr1819;
	goto tr1796;
tr1783:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 71 "ext/dtext/dtext.cpp.rl"
	{ sm->a1 = sm->p; }
#line 583 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 80;}
	goto st1524;
st1524:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1524;
case 1524:
#line 15194 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 79: goto tr1934;
		case 91: goto tr1820;
		case 111: goto tr1934;
		case 123: goto tr1821;
	}
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1819;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto tr1819;
	} else
		goto tr1819;
	goto tr1796;
tr1934:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 583 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 80;}
	goto st1525;
st1525:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1525;
case 1525:
#line 15220 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 68: goto tr1935;
		case 91: goto tr1820;
		case 100: goto tr1935;
		case 123: goto tr1821;
	}
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1819;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto tr1819;
	} else
		goto tr1819;
	goto tr1796;
tr1935:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 583 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 80;}
	goto st1526;
st1526:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1526;
case 1526:
#line 15246 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 32: goto st622;
		case 91: goto tr1820;
		case 123: goto tr1821;
	}
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1819;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto tr1819;
	} else
		goto tr1819;
	goto tr1796;
st622:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof622;
case 622:
	switch( (*( sm->p)) ) {
		case 65: goto st623;
		case 97: goto st623;
	}
	goto tr210;
st623:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof623;
case 623:
	switch( (*( sm->p)) ) {
		case 67: goto st624;
		case 99: goto st624;
	}
	goto tr210;
st624:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof624;
case 624:
	switch( (*( sm->p)) ) {
		case 84: goto st625;
		case 116: goto st625;
	}
	goto tr210;
st625:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof625;
case 625:
	switch( (*( sm->p)) ) {
		case 73: goto st626;
		case 105: goto st626;
	}
	goto tr210;
st626:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof626;
case 626:
	switch( (*( sm->p)) ) {
		case 79: goto st627;
		case 111: goto st627;
	}
	goto tr210;
st627:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof627;
case 627:
	switch( (*( sm->p)) ) {
		case 78: goto st628;
		case 110: goto st628;
	}
	goto tr210;
st628:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof628;
case 628:
	if ( (*( sm->p)) == 32 )
		goto st629;
	goto tr210;
st629:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof629;
case 629:
	if ( (*( sm->p)) == 35 )
		goto st630;
	goto tr210;
st630:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof630;
case 630:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr789;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto tr790;
	} else
		goto tr790;
	goto tr210;
tr789:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 71 "ext/dtext/dtext.cpp.rl"
	{ sm->a1 = sm->p; }
#line 309 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 25;}
	goto st1527;
st1527:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1527;
case 1527:
#line 15354 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1938;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st639;
	} else
		goto st639;
	goto tr1937;
tr1938:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 309 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 25;}
	goto st1528;
st1528:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1528;
case 1528:
#line 15374 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1939;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st638;
	} else
		goto st638;
	goto tr1937;
tr1939:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 309 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 25;}
	goto st1529;
st1529:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1529;
case 1529:
#line 15394 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1940;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st637;
	} else
		goto st637;
	goto tr1937;
tr1940:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 309 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 25;}
	goto st1530;
st1530:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1530;
case 1530:
#line 15414 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1941;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st636;
	} else
		goto st636;
	goto tr1937;
tr1941:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 309 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 25;}
	goto st1531;
st1531:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1531;
case 1531:
#line 15434 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1942;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st635;
	} else
		goto st635;
	goto tr1937;
tr1942:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 309 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 25;}
	goto st1532;
st1532:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1532;
case 1532:
#line 15454 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1943;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st634;
	} else
		goto st634;
	goto tr1937;
tr1943:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 309 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 25;}
	goto st1533;
st1533:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1533;
case 1533:
#line 15474 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1944;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st633;
	} else
		goto st633;
	goto tr1937;
tr1944:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 309 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 25;}
	goto st1534;
st1534:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1534;
case 1534:
#line 15494 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1945;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st632;
	} else
		goto st632;
	goto tr1937;
tr1945:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 309 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 25;}
	goto st1535;
st1535:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1535;
case 1535:
#line 15514 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st1536;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st631;
	} else
		goto st631;
	goto tr1937;
st1536:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1536;
case 1536:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st1537;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st1538;
	} else
		goto st1538;
	goto tr1937;
st1537:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1537;
case 1537:
	if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
		goto st1537;
	goto tr1937;
st1538:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1538;
case 1538:
	goto tr1937;
st631:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof631;
case 631:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st1538;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st1538;
	} else
		goto st1538;
	goto tr207;
st632:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof632;
case 632:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st631;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st631;
	} else
		goto st631;
	goto tr207;
st633:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof633;
case 633:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st632;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st632;
	} else
		goto st632;
	goto tr207;
st634:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof634;
case 634:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st633;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st633;
	} else
		goto st633;
	goto tr207;
st635:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof635;
case 635:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st634;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st634;
	} else
		goto st634;
	goto tr207;
st636:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof636;
case 636:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st635;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st635;
	} else
		goto st635;
	goto tr207;
st637:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof637;
case 637:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st636;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st636;
	} else
		goto st636;
	goto tr207;
st638:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof638;
case 638:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st637;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st637;
	} else
		goto st637;
	goto tr207;
st639:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof639;
case 639:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st638;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st638;
	} else
		goto st638;
	goto tr207;
tr790:
#line 71 "ext/dtext/dtext.cpp.rl"
	{ sm->a1 = sm->p; }
	goto st640;
st640:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof640;
case 640:
#line 15674 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st639;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st639;
	} else
		goto st639;
	goto tr210;
tr1784:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 71 "ext/dtext/dtext.cpp.rl"
	{ sm->a1 = sm->p; }
#line 583 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 80;}
	goto st1539;
st1539:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1539;
case 1539:
#line 15696 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 73: goto tr1948;
		case 79: goto tr1949;
		case 91: goto tr1820;
		case 105: goto tr1948;
		case 111: goto tr1949;
		case 123: goto tr1821;
	}
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1819;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto tr1819;
	} else
		goto tr1819;
	goto tr1796;
tr1948:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 583 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 80;}
	goto st1540;
st1540:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1540;
case 1540:
#line 15724 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 88: goto tr1950;
		case 91: goto tr1820;
		case 120: goto tr1950;
		case 123: goto tr1821;
	}
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1819;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto tr1819;
	} else
		goto tr1819;
	goto tr1796;
tr1950:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 583 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 80;}
	goto st1541;
st1541:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1541;
case 1541:
#line 15750 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 73: goto tr1951;
		case 91: goto tr1820;
		case 105: goto tr1951;
		case 123: goto tr1821;
	}
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1819;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto tr1819;
	} else
		goto tr1819;
	goto tr1796;
tr1951:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 583 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 80;}
	goto st1542;
st1542:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1542;
case 1542:
#line 15776 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 86: goto tr1952;
		case 91: goto tr1820;
		case 118: goto tr1952;
		case 123: goto tr1821;
	}
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1819;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto tr1819;
	} else
		goto tr1819;
	goto tr1796;
tr1952:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 583 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 80;}
	goto st1543;
st1543:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1543;
case 1543:
#line 15802 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 32: goto st641;
		case 91: goto tr1820;
		case 123: goto tr1821;
	}
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1819;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto tr1819;
	} else
		goto tr1819;
	goto tr1796;
st641:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof641;
case 641:
	if ( (*( sm->p)) == 35 )
		goto st642;
	goto tr210;
st642:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof642;
case 642:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr802;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto tr803;
	} else
		goto tr803;
	goto tr210;
tr802:
#line 71 "ext/dtext/dtext.cpp.rl"
	{ sm->a1 = sm->p; }
	goto st643;
st643:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof643;
case 643:
#line 15845 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) == 47 )
		goto tr804;
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st646;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st665;
	} else
		goto st665;
	goto tr210;
tr804:
#line 72 "ext/dtext/dtext.cpp.rl"
	{ sm->a2 = sm->p; }
	goto st644;
st644:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof644;
case 644:
#line 15865 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 80: goto st645;
		case 112: goto st645;
	}
	goto tr210;
st645:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof645;
case 645:
	if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
		goto tr808;
	goto tr210;
tr808:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ sm->b1 = sm->p; }
	goto st1544;
st1544:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1544;
case 1544:
#line 15886 "ext/dtext/dtext.cpp"
	if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
		goto st1544;
	goto tr1954;
st646:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof646;
case 646:
	if ( (*( sm->p)) == 47 )
		goto tr804;
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st647;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st664;
	} else
		goto st664;
	goto tr210;
st647:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof647;
case 647:
	if ( (*( sm->p)) == 47 )
		goto tr804;
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st648;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st663;
	} else
		goto st663;
	goto tr210;
st648:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof648;
case 648:
	if ( (*( sm->p)) == 47 )
		goto tr804;
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st649;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st662;
	} else
		goto st662;
	goto tr210;
st649:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof649;
case 649:
	if ( (*( sm->p)) == 47 )
		goto tr804;
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st650;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st661;
	} else
		goto st661;
	goto tr210;
st650:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof650;
case 650:
	if ( (*( sm->p)) == 47 )
		goto tr804;
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st651;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st660;
	} else
		goto st660;
	goto tr210;
st651:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof651;
case 651:
	if ( (*( sm->p)) == 47 )
		goto tr804;
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st652;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st659;
	} else
		goto st659;
	goto tr210;
st652:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof652;
case 652:
	if ( (*( sm->p)) == 47 )
		goto tr804;
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st653;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st658;
	} else
		goto st658;
	goto tr210;
st653:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof653;
case 653:
	if ( (*( sm->p)) == 47 )
		goto tr804;
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st654;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st657;
	} else
		goto st657;
	goto tr210;
st654:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof654;
case 654:
	if ( (*( sm->p)) == 47 )
		goto tr804;
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st655;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st656;
	} else
		goto st656;
	goto tr210;
st655:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof655;
case 655:
	if ( (*( sm->p)) == 47 )
		goto tr804;
	if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
		goto st655;
	goto tr210;
st656:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof656;
case 656:
	if ( (*( sm->p)) == 47 )
		goto tr804;
	goto tr210;
st657:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof657;
case 657:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st656;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st656;
	} else
		goto st656;
	goto tr210;
st658:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof658;
case 658:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st657;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st657;
	} else
		goto st657;
	goto tr210;
st659:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof659;
case 659:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st658;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st658;
	} else
		goto st658;
	goto tr210;
st660:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof660;
case 660:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st659;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st659;
	} else
		goto st659;
	goto tr210;
st661:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof661;
case 661:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st660;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st660;
	} else
		goto st660;
	goto tr210;
st662:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof662;
case 662:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st661;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st661;
	} else
		goto st661;
	goto tr210;
st663:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof663;
case 663:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st662;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st662;
	} else
		goto st662;
	goto tr210;
st664:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof664;
case 664:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st663;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st663;
	} else
		goto st663;
	goto tr210;
st665:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof665;
case 665:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st664;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st664;
	} else
		goto st664;
	goto tr210;
tr803:
#line 71 "ext/dtext/dtext.cpp.rl"
	{ sm->a1 = sm->p; }
	goto st666;
st666:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof666;
case 666:
#line 16166 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st665;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st665;
	} else
		goto st665;
	goto tr210;
tr1949:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 583 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 80;}
	goto st1545;
st1545:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1545;
case 1545:
#line 16186 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 79: goto tr1956;
		case 83: goto tr1957;
		case 91: goto tr1820;
		case 111: goto tr1956;
		case 115: goto tr1957;
		case 123: goto tr1821;
	}
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1819;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto tr1819;
	} else
		goto tr1819;
	goto tr1796;
tr1956:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 583 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 80;}
	goto st1546;
st1546:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1546;
case 1546:
#line 16214 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 76: goto tr1958;
		case 91: goto tr1820;
		case 108: goto tr1958;
		case 123: goto tr1821;
	}
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1819;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto tr1819;
	} else
		goto tr1819;
	goto tr1796;
tr1958:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 583 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 80;}
	goto st1547;
st1547:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1547;
case 1547:
#line 16240 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 32: goto st667;
		case 91: goto tr1820;
		case 123: goto tr1821;
	}
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1819;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto tr1819;
	} else
		goto tr1819;
	goto tr1796;
st667:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof667;
case 667:
	if ( (*( sm->p)) == 35 )
		goto st668;
	goto tr210;
st668:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof668;
case 668:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr828;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto tr829;
	} else
		goto tr829;
	goto tr210;
tr828:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 71 "ext/dtext/dtext.cpp.rl"
	{ sm->a1 = sm->p; }
#line 300 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 16;}
	goto st1548;
st1548:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1548;
case 1548:
#line 16287 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1961;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st677;
	} else
		goto st677;
	goto tr1960;
tr1961:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 300 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 16;}
	goto st1549;
st1549:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1549;
case 1549:
#line 16307 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1962;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st676;
	} else
		goto st676;
	goto tr1960;
tr1962:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 300 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 16;}
	goto st1550;
st1550:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1550;
case 1550:
#line 16327 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1963;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st675;
	} else
		goto st675;
	goto tr1960;
tr1963:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 300 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 16;}
	goto st1551;
st1551:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1551;
case 1551:
#line 16347 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1964;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st674;
	} else
		goto st674;
	goto tr1960;
tr1964:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 300 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 16;}
	goto st1552;
st1552:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1552;
case 1552:
#line 16367 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1965;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st673;
	} else
		goto st673;
	goto tr1960;
tr1965:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 300 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 16;}
	goto st1553;
st1553:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1553;
case 1553:
#line 16387 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1966;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st672;
	} else
		goto st672;
	goto tr1960;
tr1966:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 300 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 16;}
	goto st1554;
st1554:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1554;
case 1554:
#line 16407 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1967;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st671;
	} else
		goto st671;
	goto tr1960;
tr1967:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 300 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 16;}
	goto st1555;
st1555:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1555;
case 1555:
#line 16427 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1968;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st670;
	} else
		goto st670;
	goto tr1960;
tr1968:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 300 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 16;}
	goto st1556;
st1556:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1556;
case 1556:
#line 16447 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st1557;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st669;
	} else
		goto st669;
	goto tr1960;
st1557:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1557;
case 1557:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st1558;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st1559;
	} else
		goto st1559;
	goto tr1960;
st1558:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1558;
case 1558:
	if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
		goto st1558;
	goto tr1960;
st1559:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1559;
case 1559:
	goto tr1960;
st669:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof669;
case 669:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st1559;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st1559;
	} else
		goto st1559;
	goto tr207;
st670:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof670;
case 670:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st669;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st669;
	} else
		goto st669;
	goto tr207;
st671:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof671;
case 671:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st670;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st670;
	} else
		goto st670;
	goto tr207;
st672:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof672;
case 672:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st671;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st671;
	} else
		goto st671;
	goto tr207;
st673:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof673;
case 673:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st672;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st672;
	} else
		goto st672;
	goto tr207;
st674:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof674;
case 674:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st673;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st673;
	} else
		goto st673;
	goto tr207;
st675:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof675;
case 675:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st674;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st674;
	} else
		goto st674;
	goto tr207;
st676:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof676;
case 676:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st675;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st675;
	} else
		goto st675;
	goto tr207;
st677:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof677;
case 677:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st676;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st676;
	} else
		goto st676;
	goto tr207;
tr829:
#line 71 "ext/dtext/dtext.cpp.rl"
	{ sm->a1 = sm->p; }
	goto st678;
st678:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof678;
case 678:
#line 16607 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st677;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st677;
	} else
		goto st677;
	goto tr210;
tr1957:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 583 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 80;}
	goto st1560;
st1560:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1560;
case 1560:
#line 16627 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 84: goto tr1971;
		case 91: goto tr1820;
		case 116: goto tr1971;
		case 123: goto tr1821;
	}
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1819;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto tr1819;
	} else
		goto tr1819;
	goto tr1796;
tr1971:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 583 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 80;}
	goto st1561;
st1561:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1561;
case 1561:
#line 16653 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 32: goto st679;
		case 91: goto tr1820;
		case 123: goto tr1821;
	}
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1819;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto tr1819;
	} else
		goto tr1819;
	goto tr1796;
st679:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof679;
case 679:
	if ( (*( sm->p)) == 35 )
		goto st680;
	goto tr210;
st680:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof680;
case 680:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr841;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto tr842;
	} else
		goto tr842;
	goto tr210;
tr841:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 71 "ext/dtext/dtext.cpp.rl"
	{ sm->a1 = sm->p; }
#line 295 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 11;}
	goto st1562;
st1562:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1562;
case 1562:
#line 16700 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1974;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st689;
	} else
		goto st689;
	goto tr1973;
tr1974:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 295 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 11;}
	goto st1563;
st1563:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1563;
case 1563:
#line 16720 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1975;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st688;
	} else
		goto st688;
	goto tr1973;
tr1975:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 295 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 11;}
	goto st1564;
st1564:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1564;
case 1564:
#line 16740 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1976;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st687;
	} else
		goto st687;
	goto tr1973;
tr1976:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 295 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 11;}
	goto st1565;
st1565:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1565;
case 1565:
#line 16760 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1977;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st686;
	} else
		goto st686;
	goto tr1973;
tr1977:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 295 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 11;}
	goto st1566;
st1566:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1566;
case 1566:
#line 16780 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1978;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st685;
	} else
		goto st685;
	goto tr1973;
tr1978:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 295 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 11;}
	goto st1567;
st1567:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1567;
case 1567:
#line 16800 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1979;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st684;
	} else
		goto st684;
	goto tr1973;
tr1979:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 295 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 11;}
	goto st1568;
st1568:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1568;
case 1568:
#line 16820 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1980;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st683;
	} else
		goto st683;
	goto tr1973;
tr1980:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 295 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 11;}
	goto st1569;
st1569:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1569;
case 1569:
#line 16840 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1981;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st682;
	} else
		goto st682;
	goto tr1973;
tr1981:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 295 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 11;}
	goto st1570;
st1570:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1570;
case 1570:
#line 16860 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st1571;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st681;
	} else
		goto st681;
	goto tr1973;
st1571:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1571;
case 1571:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st1572;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st1573;
	} else
		goto st1573;
	goto tr1973;
st1572:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1572;
case 1572:
	if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
		goto st1572;
	goto tr1973;
st1573:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1573;
case 1573:
	goto tr1973;
st681:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof681;
case 681:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st1573;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st1573;
	} else
		goto st1573;
	goto tr207;
st682:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof682;
case 682:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st681;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st681;
	} else
		goto st681;
	goto tr207;
st683:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof683;
case 683:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st682;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st682;
	} else
		goto st682;
	goto tr207;
st684:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof684;
case 684:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st683;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st683;
	} else
		goto st683;
	goto tr207;
st685:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof685;
case 685:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st684;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st684;
	} else
		goto st684;
	goto tr207;
st686:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof686;
case 686:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st685;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st685;
	} else
		goto st685;
	goto tr207;
st687:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof687;
case 687:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st686;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st686;
	} else
		goto st686;
	goto tr207;
st688:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof688;
case 688:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st687;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st687;
	} else
		goto st687;
	goto tr207;
st689:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof689;
case 689:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st688;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st688;
	} else
		goto st688;
	goto tr207;
tr842:
#line 71 "ext/dtext/dtext.cpp.rl"
	{ sm->a1 = sm->p; }
	goto st690;
st690:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof690;
case 690:
#line 17020 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st689;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st689;
	} else
		goto st689;
	goto tr210;
tr1785:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 71 "ext/dtext/dtext.cpp.rl"
	{ sm->a1 = sm->p; }
#line 583 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 80;}
	goto st1574;
st1574:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1574;
case 1574:
#line 17042 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 69: goto tr1984;
		case 91: goto tr1820;
		case 101: goto tr1984;
		case 123: goto tr1821;
	}
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1819;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto tr1819;
	} else
		goto tr1819;
	goto tr1796;
tr1984:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 583 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 80;}
	goto st1575;
st1575:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1575;
case 1575:
#line 17068 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 67: goto tr1985;
		case 91: goto tr1820;
		case 99: goto tr1985;
		case 123: goto tr1821;
	}
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1819;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto tr1819;
	} else
		goto tr1819;
	goto tr1796;
tr1985:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 583 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 80;}
	goto st1576;
st1576:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1576;
case 1576:
#line 17094 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 79: goto tr1986;
		case 91: goto tr1820;
		case 111: goto tr1986;
		case 123: goto tr1821;
	}
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1819;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto tr1819;
	} else
		goto tr1819;
	goto tr1796;
tr1986:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 583 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 80;}
	goto st1577;
st1577:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1577;
case 1577:
#line 17120 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 82: goto tr1987;
		case 91: goto tr1820;
		case 114: goto tr1987;
		case 123: goto tr1821;
	}
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1819;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto tr1819;
	} else
		goto tr1819;
	goto tr1796;
tr1987:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 583 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 80;}
	goto st1578;
st1578:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1578;
case 1578:
#line 17146 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 68: goto tr1988;
		case 91: goto tr1820;
		case 100: goto tr1988;
		case 123: goto tr1821;
	}
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1819;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto tr1819;
	} else
		goto tr1819;
	goto tr1796;
tr1988:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 583 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 80;}
	goto st1579;
st1579:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1579;
case 1579:
#line 17172 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 32: goto st691;
		case 91: goto tr1820;
		case 123: goto tr1821;
	}
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1819;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto tr1819;
	} else
		goto tr1819;
	goto tr1796;
st691:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof691;
case 691:
	if ( (*( sm->p)) == 35 )
		goto st692;
	goto tr210;
st692:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof692;
case 692:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr854;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto tr855;
	} else
		goto tr855;
	goto tr210;
tr854:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 71 "ext/dtext/dtext.cpp.rl"
	{ sm->a1 = sm->p; }
#line 310 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 26;}
	goto st1580;
st1580:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1580;
case 1580:
#line 17219 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1991;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st701;
	} else
		goto st701;
	goto tr1990;
tr1991:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 310 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 26;}
	goto st1581;
st1581:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1581;
case 1581:
#line 17239 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1992;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st700;
	} else
		goto st700;
	goto tr1990;
tr1992:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 310 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 26;}
	goto st1582;
st1582:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1582;
case 1582:
#line 17259 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1993;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st699;
	} else
		goto st699;
	goto tr1990;
tr1993:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 310 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 26;}
	goto st1583;
st1583:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1583;
case 1583:
#line 17279 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1994;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st698;
	} else
		goto st698;
	goto tr1990;
tr1994:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 310 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 26;}
	goto st1584;
st1584:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1584;
case 1584:
#line 17299 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1995;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st697;
	} else
		goto st697;
	goto tr1990;
tr1995:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 310 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 26;}
	goto st1585;
st1585:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1585;
case 1585:
#line 17319 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1996;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st696;
	} else
		goto st696;
	goto tr1990;
tr1996:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 310 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 26;}
	goto st1586;
st1586:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1586;
case 1586:
#line 17339 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1997;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st695;
	} else
		goto st695;
	goto tr1990;
tr1997:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 310 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 26;}
	goto st1587;
st1587:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1587;
case 1587:
#line 17359 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1998;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st694;
	} else
		goto st694;
	goto tr1990;
tr1998:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 310 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 26;}
	goto st1588;
st1588:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1588;
case 1588:
#line 17379 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st1589;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st693;
	} else
		goto st693;
	goto tr1990;
st1589:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1589;
case 1589:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st1590;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st1591;
	} else
		goto st1591;
	goto tr1990;
st1590:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1590;
case 1590:
	if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
		goto st1590;
	goto tr1990;
st1591:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1591;
case 1591:
	goto tr1990;
st693:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof693;
case 693:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st1591;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st1591;
	} else
		goto st1591;
	goto tr207;
st694:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof694;
case 694:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st693;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st693;
	} else
		goto st693;
	goto tr207;
st695:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof695;
case 695:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st694;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st694;
	} else
		goto st694;
	goto tr207;
st696:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof696;
case 696:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st695;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st695;
	} else
		goto st695;
	goto tr207;
st697:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof697;
case 697:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st696;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st696;
	} else
		goto st696;
	goto tr207;
st698:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof698;
case 698:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st697;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st697;
	} else
		goto st697;
	goto tr207;
st699:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof699;
case 699:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st698;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st698;
	} else
		goto st698;
	goto tr207;
st700:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof700;
case 700:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st699;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st699;
	} else
		goto st699;
	goto tr207;
st701:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof701;
case 701:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st700;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st700;
	} else
		goto st700;
	goto tr207;
tr855:
#line 71 "ext/dtext/dtext.cpp.rl"
	{ sm->a1 = sm->p; }
	goto st702;
st702:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof702;
case 702:
#line 17539 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st701;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st701;
	} else
		goto st701;
	goto tr210;
tr1786:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 71 "ext/dtext/dtext.cpp.rl"
	{ sm->a1 = sm->p; }
#line 583 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 80;}
	goto st1592;
st1592:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1592;
case 1592:
#line 17561 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 69: goto tr2001;
		case 91: goto tr1820;
		case 101: goto tr2001;
		case 123: goto tr1821;
	}
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1819;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto tr1819;
	} else
		goto tr1819;
	goto tr1796;
tr2001:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 583 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 80;}
	goto st1593;
st1593:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1593;
case 1593:
#line 17587 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 82: goto tr2002;
		case 91: goto tr1820;
		case 114: goto tr2002;
		case 123: goto tr1821;
	}
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1819;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto tr1819;
	} else
		goto tr1819;
	goto tr1796;
tr2002:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 583 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 80;}
	goto st1594;
st1594:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1594;
case 1594:
#line 17613 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 73: goto tr2003;
		case 91: goto tr1820;
		case 105: goto tr2003;
		case 123: goto tr1821;
	}
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1819;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto tr1819;
	} else
		goto tr1819;
	goto tr1796;
tr2003:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 583 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 80;}
	goto st1595;
st1595:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1595;
case 1595:
#line 17639 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 69: goto tr2004;
		case 91: goto tr1820;
		case 101: goto tr2004;
		case 123: goto tr1821;
	}
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1819;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto tr1819;
	} else
		goto tr1819;
	goto tr1796;
tr2004:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 583 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 80;}
	goto st1596;
st1596:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1596;
case 1596:
#line 17665 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 83: goto tr2005;
		case 91: goto tr1820;
		case 115: goto tr2005;
		case 123: goto tr1821;
	}
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1819;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto tr1819;
	} else
		goto tr1819;
	goto tr1796;
tr2005:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 583 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 80;}
	goto st1597;
st1597:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1597;
case 1597:
#line 17691 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 32: goto st703;
		case 91: goto tr1820;
		case 123: goto tr1821;
	}
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1819;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto tr1819;
	} else
		goto tr1819;
	goto tr1796;
st703:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof703;
case 703:
	if ( (*( sm->p)) == 35 )
		goto st704;
	goto tr210;
st704:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof704;
case 704:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr867;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto tr868;
	} else
		goto tr868;
	goto tr210;
tr867:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 71 "ext/dtext/dtext.cpp.rl"
	{ sm->a1 = sm->p; }
#line 308 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 24;}
	goto st1598;
st1598:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1598;
case 1598:
#line 17738 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr2008;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st713;
	} else
		goto st713;
	goto tr2007;
tr2008:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 308 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 24;}
	goto st1599;
st1599:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1599;
case 1599:
#line 17758 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr2009;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st712;
	} else
		goto st712;
	goto tr2007;
tr2009:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 308 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 24;}
	goto st1600;
st1600:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1600;
case 1600:
#line 17778 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr2010;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st711;
	} else
		goto st711;
	goto tr2007;
tr2010:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 308 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 24;}
	goto st1601;
st1601:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1601;
case 1601:
#line 17798 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr2011;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st710;
	} else
		goto st710;
	goto tr2007;
tr2011:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 308 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 24;}
	goto st1602;
st1602:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1602;
case 1602:
#line 17818 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr2012;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st709;
	} else
		goto st709;
	goto tr2007;
tr2012:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 308 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 24;}
	goto st1603;
st1603:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1603;
case 1603:
#line 17838 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr2013;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st708;
	} else
		goto st708;
	goto tr2007;
tr2013:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 308 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 24;}
	goto st1604;
st1604:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1604;
case 1604:
#line 17858 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr2014;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st707;
	} else
		goto st707;
	goto tr2007;
tr2014:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 308 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 24;}
	goto st1605;
st1605:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1605;
case 1605:
#line 17878 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr2015;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st706;
	} else
		goto st706;
	goto tr2007;
tr2015:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 308 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 24;}
	goto st1606;
st1606:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1606;
case 1606:
#line 17898 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st1607;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st705;
	} else
		goto st705;
	goto tr2007;
st1607:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1607;
case 1607:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st1608;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st1609;
	} else
		goto st1609;
	goto tr2007;
st1608:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1608;
case 1608:
	if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
		goto st1608;
	goto tr2007;
st1609:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1609;
case 1609:
	goto tr2007;
st705:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof705;
case 705:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st1609;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st1609;
	} else
		goto st1609;
	goto tr207;
st706:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof706;
case 706:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st705;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st705;
	} else
		goto st705;
	goto tr207;
st707:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof707;
case 707:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st706;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st706;
	} else
		goto st706;
	goto tr207;
st708:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof708;
case 708:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st707;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st707;
	} else
		goto st707;
	goto tr207;
st709:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof709;
case 709:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st708;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st708;
	} else
		goto st708;
	goto tr207;
st710:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof710;
case 710:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st709;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st709;
	} else
		goto st709;
	goto tr207;
st711:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof711;
case 711:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st710;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st710;
	} else
		goto st710;
	goto tr207;
st712:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof712;
case 712:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st711;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st711;
	} else
		goto st711;
	goto tr207;
st713:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof713;
case 713:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st712;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st712;
	} else
		goto st712;
	goto tr207;
tr868:
#line 71 "ext/dtext/dtext.cpp.rl"
	{ sm->a1 = sm->p; }
	goto st714;
st714:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof714;
case 714:
#line 18058 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st713;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st713;
	} else
		goto st713;
	goto tr210;
tr1787:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 71 "ext/dtext/dtext.cpp.rl"
	{ sm->a1 = sm->p; }
#line 583 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 80;}
	goto st1610;
st1610:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1610;
case 1610:
#line 18080 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 65: goto tr2018;
		case 79: goto tr2019;
		case 91: goto tr1820;
		case 97: goto tr2018;
		case 111: goto tr2019;
		case 123: goto tr1821;
	}
	if ( (*( sm->p)) < 66 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1819;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 98 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto tr1819;
	} else
		goto tr1819;
	goto tr1796;
tr2018:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 583 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 80;}
	goto st1611;
st1611:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1611;
case 1611:
#line 18108 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 71: goto tr2020;
		case 91: goto tr1820;
		case 103: goto tr2020;
		case 123: goto tr1821;
	}
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1819;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto tr1819;
	} else
		goto tr1819;
	goto tr1796;
tr2020:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 583 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 80;}
	goto st1612;
st1612:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1612;
case 1612:
#line 18134 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 32: goto st715;
		case 91: goto tr1820;
		case 123: goto tr1821;
	}
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1819;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto tr1819;
	} else
		goto tr1819;
	goto tr1796;
st715:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof715;
case 715:
	switch( (*( sm->p)) ) {
		case 65: goto st716;
		case 73: goto st733;
		case 84: goto st756;
		case 97: goto st716;
		case 105: goto st733;
		case 116: goto st756;
	}
	goto tr210;
st716:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof716;
case 716:
	switch( (*( sm->p)) ) {
		case 76: goto st717;
		case 108: goto st717;
	}
	goto tr210;
st717:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof717;
case 717:
	switch( (*( sm->p)) ) {
		case 73: goto st718;
		case 105: goto st718;
	}
	goto tr210;
st718:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof718;
case 718:
	switch( (*( sm->p)) ) {
		case 65: goto st719;
		case 97: goto st719;
	}
	goto tr210;
st719:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof719;
case 719:
	switch( (*( sm->p)) ) {
		case 83: goto st720;
		case 115: goto st720;
	}
	goto tr210;
st720:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof720;
case 720:
	if ( (*( sm->p)) == 32 )
		goto st721;
	goto tr210;
st721:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof721;
case 721:
	if ( (*( sm->p)) == 35 )
		goto st722;
	goto tr210;
st722:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof722;
case 722:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr888;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto tr889;
	} else
		goto tr889;
	goto tr210;
tr888:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 71 "ext/dtext/dtext.cpp.rl"
	{ sm->a1 = sm->p; }
#line 304 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 20;}
	goto st1613;
st1613:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1613;
case 1613:
#line 18237 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr2023;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st731;
	} else
		goto st731;
	goto tr2022;
tr2023:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 304 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 20;}
	goto st1614;
st1614:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1614;
case 1614:
#line 18257 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr2024;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st730;
	} else
		goto st730;
	goto tr2022;
tr2024:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 304 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 20;}
	goto st1615;
st1615:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1615;
case 1615:
#line 18277 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr2025;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st729;
	} else
		goto st729;
	goto tr2022;
tr2025:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 304 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 20;}
	goto st1616;
st1616:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1616;
case 1616:
#line 18297 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr2026;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st728;
	} else
		goto st728;
	goto tr2022;
tr2026:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 304 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 20;}
	goto st1617;
st1617:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1617;
case 1617:
#line 18317 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr2027;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st727;
	} else
		goto st727;
	goto tr2022;
tr2027:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 304 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 20;}
	goto st1618;
st1618:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1618;
case 1618:
#line 18337 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr2028;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st726;
	} else
		goto st726;
	goto tr2022;
tr2028:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 304 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 20;}
	goto st1619;
st1619:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1619;
case 1619:
#line 18357 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr2029;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st725;
	} else
		goto st725;
	goto tr2022;
tr2029:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 304 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 20;}
	goto st1620;
st1620:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1620;
case 1620:
#line 18377 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr2030;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st724;
	} else
		goto st724;
	goto tr2022;
tr2030:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 304 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 20;}
	goto st1621;
st1621:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1621;
case 1621:
#line 18397 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st1622;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st723;
	} else
		goto st723;
	goto tr2022;
st1622:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1622;
case 1622:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st1623;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st1624;
	} else
		goto st1624;
	goto tr2022;
st1623:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1623;
case 1623:
	if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
		goto st1623;
	goto tr2022;
st1624:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1624;
case 1624:
	goto tr2022;
st723:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof723;
case 723:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st1624;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st1624;
	} else
		goto st1624;
	goto tr207;
st724:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof724;
case 724:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st723;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st723;
	} else
		goto st723;
	goto tr207;
st725:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof725;
case 725:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st724;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st724;
	} else
		goto st724;
	goto tr207;
st726:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof726;
case 726:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st725;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st725;
	} else
		goto st725;
	goto tr207;
st727:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof727;
case 727:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st726;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st726;
	} else
		goto st726;
	goto tr207;
st728:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof728;
case 728:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st727;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st727;
	} else
		goto st727;
	goto tr207;
st729:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof729;
case 729:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st728;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st728;
	} else
		goto st728;
	goto tr207;
st730:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof730;
case 730:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st729;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st729;
	} else
		goto st729;
	goto tr207;
st731:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof731;
case 731:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st730;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st730;
	} else
		goto st730;
	goto tr207;
tr889:
#line 71 "ext/dtext/dtext.cpp.rl"
	{ sm->a1 = sm->p; }
	goto st732;
st732:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof732;
case 732:
#line 18557 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st731;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st731;
	} else
		goto st731;
	goto tr210;
st733:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof733;
case 733:
	switch( (*( sm->p)) ) {
		case 77: goto st734;
		case 109: goto st734;
	}
	goto tr210;
st734:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof734;
case 734:
	switch( (*( sm->p)) ) {
		case 80: goto st735;
		case 112: goto st735;
	}
	goto tr210;
st735:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof735;
case 735:
	switch( (*( sm->p)) ) {
		case 76: goto st736;
		case 108: goto st736;
	}
	goto tr210;
st736:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof736;
case 736:
	switch( (*( sm->p)) ) {
		case 73: goto st737;
		case 105: goto st737;
	}
	goto tr210;
st737:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof737;
case 737:
	switch( (*( sm->p)) ) {
		case 67: goto st738;
		case 99: goto st738;
	}
	goto tr210;
st738:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof738;
case 738:
	switch( (*( sm->p)) ) {
		case 65: goto st739;
		case 97: goto st739;
	}
	goto tr210;
st739:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof739;
case 739:
	switch( (*( sm->p)) ) {
		case 84: goto st740;
		case 116: goto st740;
	}
	goto tr210;
st740:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof740;
case 740:
	switch( (*( sm->p)) ) {
		case 73: goto st741;
		case 105: goto st741;
	}
	goto tr210;
st741:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof741;
case 741:
	switch( (*( sm->p)) ) {
		case 79: goto st742;
		case 111: goto st742;
	}
	goto tr210;
st742:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof742;
case 742:
	switch( (*( sm->p)) ) {
		case 78: goto st743;
		case 110: goto st743;
	}
	goto tr210;
st743:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof743;
case 743:
	if ( (*( sm->p)) == 32 )
		goto st744;
	goto tr210;
st744:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof744;
case 744:
	if ( (*( sm->p)) == 35 )
		goto st745;
	goto tr210;
st745:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof745;
case 745:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr912;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto tr913;
	} else
		goto tr913;
	goto tr210;
tr912:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 71 "ext/dtext/dtext.cpp.rl"
	{ sm->a1 = sm->p; }
#line 305 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 21;}
	goto st1625;
st1625:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1625;
case 1625:
#line 18696 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr2034;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st754;
	} else
		goto st754;
	goto tr2033;
tr2034:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 305 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 21;}
	goto st1626;
st1626:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1626;
case 1626:
#line 18716 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr2035;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st753;
	} else
		goto st753;
	goto tr2033;
tr2035:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 305 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 21;}
	goto st1627;
st1627:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1627;
case 1627:
#line 18736 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr2036;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st752;
	} else
		goto st752;
	goto tr2033;
tr2036:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 305 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 21;}
	goto st1628;
st1628:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1628;
case 1628:
#line 18756 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr2037;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st751;
	} else
		goto st751;
	goto tr2033;
tr2037:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 305 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 21;}
	goto st1629;
st1629:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1629;
case 1629:
#line 18776 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr2038;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st750;
	} else
		goto st750;
	goto tr2033;
tr2038:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 305 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 21;}
	goto st1630;
st1630:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1630;
case 1630:
#line 18796 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr2039;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st749;
	} else
		goto st749;
	goto tr2033;
tr2039:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 305 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 21;}
	goto st1631;
st1631:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1631;
case 1631:
#line 18816 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr2040;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st748;
	} else
		goto st748;
	goto tr2033;
tr2040:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 305 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 21;}
	goto st1632;
st1632:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1632;
case 1632:
#line 18836 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr2041;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st747;
	} else
		goto st747;
	goto tr2033;
tr2041:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 305 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 21;}
	goto st1633;
st1633:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1633;
case 1633:
#line 18856 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st1634;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st746;
	} else
		goto st746;
	goto tr2033;
st1634:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1634;
case 1634:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st1635;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st1636;
	} else
		goto st1636;
	goto tr2033;
st1635:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1635;
case 1635:
	if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
		goto st1635;
	goto tr2033;
st1636:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1636;
case 1636:
	goto tr2033;
st746:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof746;
case 746:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st1636;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st1636;
	} else
		goto st1636;
	goto tr207;
st747:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof747;
case 747:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st746;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st746;
	} else
		goto st746;
	goto tr207;
st748:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof748;
case 748:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st747;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st747;
	} else
		goto st747;
	goto tr207;
st749:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof749;
case 749:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st748;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st748;
	} else
		goto st748;
	goto tr207;
st750:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof750;
case 750:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st749;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st749;
	} else
		goto st749;
	goto tr207;
st751:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof751;
case 751:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st750;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st750;
	} else
		goto st750;
	goto tr207;
st752:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof752;
case 752:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st751;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st751;
	} else
		goto st751;
	goto tr207;
st753:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof753;
case 753:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st752;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st752;
	} else
		goto st752;
	goto tr207;
st754:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof754;
case 754:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st753;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st753;
	} else
		goto st753;
	goto tr207;
tr913:
#line 71 "ext/dtext/dtext.cpp.rl"
	{ sm->a1 = sm->p; }
	goto st755;
st755:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof755;
case 755:
#line 19016 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st754;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st754;
	} else
		goto st754;
	goto tr210;
st756:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof756;
case 756:
	switch( (*( sm->p)) ) {
		case 82: goto st757;
		case 114: goto st757;
	}
	goto tr210;
st757:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof757;
case 757:
	switch( (*( sm->p)) ) {
		case 65: goto st758;
		case 97: goto st758;
	}
	goto tr210;
st758:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof758;
case 758:
	switch( (*( sm->p)) ) {
		case 78: goto st759;
		case 110: goto st759;
	}
	goto tr210;
st759:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof759;
case 759:
	switch( (*( sm->p)) ) {
		case 83: goto st760;
		case 115: goto st760;
	}
	goto tr210;
st760:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof760;
case 760:
	switch( (*( sm->p)) ) {
		case 76: goto st761;
		case 108: goto st761;
	}
	goto tr210;
st761:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof761;
case 761:
	switch( (*( sm->p)) ) {
		case 65: goto st762;
		case 97: goto st762;
	}
	goto tr210;
st762:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof762;
case 762:
	switch( (*( sm->p)) ) {
		case 84: goto st763;
		case 116: goto st763;
	}
	goto tr210;
st763:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof763;
case 763:
	switch( (*( sm->p)) ) {
		case 73: goto st764;
		case 105: goto st764;
	}
	goto tr210;
st764:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof764;
case 764:
	switch( (*( sm->p)) ) {
		case 79: goto st765;
		case 111: goto st765;
	}
	goto tr210;
st765:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof765;
case 765:
	switch( (*( sm->p)) ) {
		case 78: goto st766;
		case 110: goto st766;
	}
	goto tr210;
st766:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof766;
case 766:
	if ( (*( sm->p)) == 32 )
		goto st767;
	goto tr210;
st767:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof767;
case 767:
	if ( (*( sm->p)) == 35 )
		goto st768;
	goto tr210;
st768:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof768;
case 768:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr936;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto tr937;
	} else
		goto tr937;
	goto tr210;
tr936:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 71 "ext/dtext/dtext.cpp.rl"
	{ sm->a1 = sm->p; }
#line 306 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 22;}
	goto st1637;
st1637:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1637;
case 1637:
#line 19155 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr2045;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st777;
	} else
		goto st777;
	goto tr2044;
tr2045:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 306 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 22;}
	goto st1638;
st1638:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1638;
case 1638:
#line 19175 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr2046;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st776;
	} else
		goto st776;
	goto tr2044;
tr2046:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 306 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 22;}
	goto st1639;
st1639:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1639;
case 1639:
#line 19195 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr2047;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st775;
	} else
		goto st775;
	goto tr2044;
tr2047:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 306 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 22;}
	goto st1640;
st1640:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1640;
case 1640:
#line 19215 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr2048;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st774;
	} else
		goto st774;
	goto tr2044;
tr2048:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 306 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 22;}
	goto st1641;
st1641:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1641;
case 1641:
#line 19235 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr2049;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st773;
	} else
		goto st773;
	goto tr2044;
tr2049:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 306 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 22;}
	goto st1642;
st1642:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1642;
case 1642:
#line 19255 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr2050;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st772;
	} else
		goto st772;
	goto tr2044;
tr2050:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 306 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 22;}
	goto st1643;
st1643:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1643;
case 1643:
#line 19275 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr2051;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st771;
	} else
		goto st771;
	goto tr2044;
tr2051:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 306 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 22;}
	goto st1644;
st1644:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1644;
case 1644:
#line 19295 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr2052;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st770;
	} else
		goto st770;
	goto tr2044;
tr2052:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 306 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 22;}
	goto st1645;
st1645:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1645;
case 1645:
#line 19315 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st1646;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st769;
	} else
		goto st769;
	goto tr2044;
st1646:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1646;
case 1646:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st1647;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st1648;
	} else
		goto st1648;
	goto tr2044;
st1647:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1647;
case 1647:
	if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
		goto st1647;
	goto tr2044;
st1648:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1648;
case 1648:
	goto tr2044;
st769:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof769;
case 769:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st1648;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st1648;
	} else
		goto st1648;
	goto tr207;
st770:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof770;
case 770:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st769;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st769;
	} else
		goto st769;
	goto tr207;
st771:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof771;
case 771:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st770;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st770;
	} else
		goto st770;
	goto tr207;
st772:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof772;
case 772:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st771;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st771;
	} else
		goto st771;
	goto tr207;
st773:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof773;
case 773:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st772;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st772;
	} else
		goto st772;
	goto tr207;
st774:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof774;
case 774:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st773;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st773;
	} else
		goto st773;
	goto tr207;
st775:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof775;
case 775:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st774;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st774;
	} else
		goto st774;
	goto tr207;
st776:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof776;
case 776:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st775;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st775;
	} else
		goto st775;
	goto tr207;
st777:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof777;
case 777:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st776;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st776;
	} else
		goto st776;
	goto tr207;
tr937:
#line 71 "ext/dtext/dtext.cpp.rl"
	{ sm->a1 = sm->p; }
	goto st778;
st778:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof778;
case 778:
#line 19475 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st777;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st777;
	} else
		goto st777;
	goto tr210;
tr2019:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 583 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 80;}
	goto st1649;
st1649:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1649;
case 1649:
#line 19495 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 80: goto tr2055;
		case 91: goto tr1820;
		case 112: goto tr2055;
		case 123: goto tr1821;
	}
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1819;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto tr1819;
	} else
		goto tr1819;
	goto tr1796;
tr2055:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 583 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 80;}
	goto st1650;
st1650:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1650;
case 1650:
#line 19521 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 73: goto tr2056;
		case 91: goto tr1820;
		case 105: goto tr2056;
		case 123: goto tr1821;
	}
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1819;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto tr1819;
	} else
		goto tr1819;
	goto tr1796;
tr2056:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 583 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 80;}
	goto st1651;
st1651:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1651;
case 1651:
#line 19547 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 67: goto tr2057;
		case 91: goto tr1820;
		case 99: goto tr2057;
		case 123: goto tr1821;
	}
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1819;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto tr1819;
	} else
		goto tr1819;
	goto tr1796;
tr2057:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 583 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 80;}
	goto st1652;
st1652:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1652;
case 1652:
#line 19573 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 32: goto st779;
		case 91: goto tr1820;
		case 123: goto tr1821;
	}
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1819;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto tr1819;
	} else
		goto tr1819;
	goto tr1796;
st779:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof779;
case 779:
	if ( (*( sm->p)) == 35 )
		goto st780;
	goto tr210;
st780:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof780;
case 780:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr949;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto tr950;
	} else
		goto tr950;
	goto tr210;
tr949:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 71 "ext/dtext/dtext.cpp.rl"
	{ sm->a1 = sm->p; }
#line 297 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 13;}
	goto st1653;
st1653:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1653;
case 1653:
#line 19620 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) == 47 )
		goto tr2060;
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr2061;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st791;
	} else
		goto st791;
	goto tr2059;
tr2060:
#line 72 "ext/dtext/dtext.cpp.rl"
	{ sm->a2 = sm->p; }
	goto st781;
st781:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof781;
case 781:
#line 19640 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 80: goto st782;
		case 112: goto st782;
	}
	goto tr951;
st782:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof782;
case 782:
	if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
		goto tr953;
	goto tr951;
tr953:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ sm->b1 = sm->p; }
	goto st1654;
st1654:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1654;
case 1654:
#line 19661 "ext/dtext/dtext.cpp"
	if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
		goto st1654;
	goto tr2062;
tr2061:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 297 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 13;}
	goto st1655;
st1655:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1655;
case 1655:
#line 19675 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) == 47 )
		goto tr2060;
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr2064;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st790;
	} else
		goto st790;
	goto tr2059;
tr2064:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 297 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 13;}
	goto st1656;
st1656:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1656;
case 1656:
#line 19697 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) == 47 )
		goto tr2060;
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr2065;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st789;
	} else
		goto st789;
	goto tr2059;
tr2065:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 297 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 13;}
	goto st1657;
st1657:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1657;
case 1657:
#line 19719 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) == 47 )
		goto tr2060;
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr2066;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st788;
	} else
		goto st788;
	goto tr2059;
tr2066:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 297 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 13;}
	goto st1658;
st1658:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1658;
case 1658:
#line 19741 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) == 47 )
		goto tr2060;
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr2067;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st787;
	} else
		goto st787;
	goto tr2059;
tr2067:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 297 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 13;}
	goto st1659;
st1659:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1659;
case 1659:
#line 19763 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) == 47 )
		goto tr2060;
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr2068;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st786;
	} else
		goto st786;
	goto tr2059;
tr2068:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 297 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 13;}
	goto st1660;
st1660:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1660;
case 1660:
#line 19785 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) == 47 )
		goto tr2060;
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr2069;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st785;
	} else
		goto st785;
	goto tr2059;
tr2069:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 297 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 13;}
	goto st1661;
st1661:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1661;
case 1661:
#line 19807 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) == 47 )
		goto tr2060;
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr2070;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st784;
	} else
		goto st784;
	goto tr2059;
tr2070:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 297 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 13;}
	goto st1662;
st1662:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1662;
case 1662:
#line 19829 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) == 47 )
		goto tr2060;
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr2071;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st783;
	} else
		goto st783;
	goto tr2059;
tr2071:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
	goto st1663;
st1663:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1663;
case 1663:
#line 19849 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) == 47 )
		goto tr2060;
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr2072;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto tr954;
	} else
		goto tr954;
	goto tr2059;
tr2072:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
	goto st1664;
st1664:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1664;
case 1664:
#line 19869 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) == 47 )
		goto tr2060;
	if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
		goto tr2072;
	goto tr2059;
tr954:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
	goto st1665;
st1665:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1665;
case 1665:
#line 19883 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) == 47 )
		goto tr2060;
	goto tr2059;
st783:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof783;
case 783:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr954;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto tr954;
	} else
		goto tr954;
	goto tr207;
st784:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof784;
case 784:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st783;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st783;
	} else
		goto st783;
	goto tr207;
st785:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof785;
case 785:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st784;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st784;
	} else
		goto st784;
	goto tr207;
st786:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof786;
case 786:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st785;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st785;
	} else
		goto st785;
	goto tr207;
st787:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof787;
case 787:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st786;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st786;
	} else
		goto st786;
	goto tr207;
st788:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof788;
case 788:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st787;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st787;
	} else
		goto st787;
	goto tr207;
st789:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof789;
case 789:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st788;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st788;
	} else
		goto st788;
	goto tr207;
st790:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof790;
case 790:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st789;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st789;
	} else
		goto st789;
	goto tr207;
st791:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof791;
case 791:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st790;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st790;
	} else
		goto st790;
	goto tr207;
tr950:
#line 71 "ext/dtext/dtext.cpp.rl"
	{ sm->a1 = sm->p; }
	goto st792;
st792:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof792;
case 792:
#line 20012 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st791;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st791;
	} else
		goto st791;
	goto tr210;
tr1788:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 71 "ext/dtext/dtext.cpp.rl"
	{ sm->a1 = sm->p; }
#line 583 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 80;}
	goto st1666;
st1666:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1666;
case 1666:
#line 20034 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 83: goto tr2073;
		case 91: goto tr1820;
		case 115: goto tr2073;
		case 123: goto tr1821;
	}
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1819;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto tr1819;
	} else
		goto tr1819;
	goto tr1796;
tr2073:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 583 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 80;}
	goto st1667;
st1667:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1667;
case 1667:
#line 20060 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 69: goto tr2074;
		case 91: goto tr1820;
		case 101: goto tr2074;
		case 123: goto tr1821;
	}
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1819;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto tr1819;
	} else
		goto tr1819;
	goto tr1796;
tr2074:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 583 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 80;}
	goto st1668;
st1668:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1668;
case 1668:
#line 20086 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 82: goto tr2075;
		case 91: goto tr1820;
		case 114: goto tr2075;
		case 123: goto tr1821;
	}
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1819;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto tr1819;
	} else
		goto tr1819;
	goto tr1796;
tr2075:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 583 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 80;}
	goto st1669;
st1669:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1669;
case 1669:
#line 20112 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 32: goto st793;
		case 91: goto tr1820;
		case 123: goto tr1821;
	}
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1819;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto tr1819;
	} else
		goto tr1819;
	goto tr1796;
st793:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof793;
case 793:
	switch( (*( sm->p)) ) {
		case 35: goto st794;
		case 82: goto st805;
		case 114: goto st805;
	}
	goto tr210;
st794:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof794;
case 794:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr966;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto tr967;
	} else
		goto tr967;
	goto tr210;
tr966:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 71 "ext/dtext/dtext.cpp.rl"
	{ sm->a1 = sm->p; }
#line 301 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 17;}
	goto st1670;
st1670:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1670;
case 1670:
#line 20162 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr2078;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st803;
	} else
		goto st803;
	goto tr2077;
tr2078:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 301 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 17;}
	goto st1671;
st1671:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1671;
case 1671:
#line 20182 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr2079;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st802;
	} else
		goto st802;
	goto tr2077;
tr2079:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 301 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 17;}
	goto st1672;
st1672:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1672;
case 1672:
#line 20202 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr2080;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st801;
	} else
		goto st801;
	goto tr2077;
tr2080:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 301 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 17;}
	goto st1673;
st1673:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1673;
case 1673:
#line 20222 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr2081;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st800;
	} else
		goto st800;
	goto tr2077;
tr2081:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 301 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 17;}
	goto st1674;
st1674:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1674;
case 1674:
#line 20242 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr2082;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st799;
	} else
		goto st799;
	goto tr2077;
tr2082:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 301 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 17;}
	goto st1675;
st1675:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1675;
case 1675:
#line 20262 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr2083;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st798;
	} else
		goto st798;
	goto tr2077;
tr2083:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 301 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 17;}
	goto st1676;
st1676:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1676;
case 1676:
#line 20282 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr2084;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st797;
	} else
		goto st797;
	goto tr2077;
tr2084:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 301 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 17;}
	goto st1677;
st1677:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1677;
case 1677:
#line 20302 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr2085;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st796;
	} else
		goto st796;
	goto tr2077;
tr2085:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 301 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 17;}
	goto st1678;
st1678:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1678;
case 1678:
#line 20322 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st1679;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st795;
	} else
		goto st795;
	goto tr2077;
st1679:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1679;
case 1679:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st1680;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st1681;
	} else
		goto st1681;
	goto tr2077;
st1680:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1680;
case 1680:
	if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
		goto st1680;
	goto tr2077;
st1681:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1681;
case 1681:
	goto tr2077;
st795:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof795;
case 795:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st1681;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st1681;
	} else
		goto st1681;
	goto tr207;
st796:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof796;
case 796:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st795;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st795;
	} else
		goto st795;
	goto tr207;
st797:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof797;
case 797:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st796;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st796;
	} else
		goto st796;
	goto tr207;
st798:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof798;
case 798:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st797;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st797;
	} else
		goto st797;
	goto tr207;
st799:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof799;
case 799:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st798;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st798;
	} else
		goto st798;
	goto tr207;
st800:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof800;
case 800:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st799;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st799;
	} else
		goto st799;
	goto tr207;
st801:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof801;
case 801:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st800;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st800;
	} else
		goto st800;
	goto tr207;
st802:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof802;
case 802:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st801;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st801;
	} else
		goto st801;
	goto tr207;
st803:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof803;
case 803:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st802;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st802;
	} else
		goto st802;
	goto tr207;
tr967:
#line 71 "ext/dtext/dtext.cpp.rl"
	{ sm->a1 = sm->p; }
	goto st804;
st804:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof804;
case 804:
#line 20482 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st803;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st803;
	} else
		goto st803;
	goto tr210;
st805:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof805;
case 805:
	switch( (*( sm->p)) ) {
		case 69: goto st806;
		case 101: goto st806;
	}
	goto tr210;
st806:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof806;
case 806:
	switch( (*( sm->p)) ) {
		case 80: goto st807;
		case 112: goto st807;
	}
	goto tr210;
st807:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof807;
case 807:
	switch( (*( sm->p)) ) {
		case 79: goto st808;
		case 111: goto st808;
	}
	goto tr210;
st808:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof808;
case 808:
	switch( (*( sm->p)) ) {
		case 82: goto st809;
		case 114: goto st809;
	}
	goto tr210;
st809:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof809;
case 809:
	switch( (*( sm->p)) ) {
		case 84: goto st810;
		case 116: goto st810;
	}
	goto tr210;
st810:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof810;
case 810:
	if ( (*( sm->p)) == 32 )
		goto st811;
	goto tr210;
st811:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof811;
case 811:
	if ( (*( sm->p)) == 35 )
		goto st812;
	goto tr210;
st812:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof812;
case 812:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr985;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto tr986;
	} else
		goto tr986;
	goto tr210;
tr985:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 71 "ext/dtext/dtext.cpp.rl"
	{ sm->a1 = sm->p; }
#line 303 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 19;}
	goto st1682;
st1682:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1682;
case 1682:
#line 20576 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr2089;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st821;
	} else
		goto st821;
	goto tr2088;
tr2089:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 303 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 19;}
	goto st1683;
st1683:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1683;
case 1683:
#line 20596 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr2090;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st820;
	} else
		goto st820;
	goto tr2088;
tr2090:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 303 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 19;}
	goto st1684;
st1684:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1684;
case 1684:
#line 20616 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr2091;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st819;
	} else
		goto st819;
	goto tr2088;
tr2091:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 303 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 19;}
	goto st1685;
st1685:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1685;
case 1685:
#line 20636 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr2092;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st818;
	} else
		goto st818;
	goto tr2088;
tr2092:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 303 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 19;}
	goto st1686;
st1686:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1686;
case 1686:
#line 20656 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr2093;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st817;
	} else
		goto st817;
	goto tr2088;
tr2093:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 303 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 19;}
	goto st1687;
st1687:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1687;
case 1687:
#line 20676 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr2094;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st816;
	} else
		goto st816;
	goto tr2088;
tr2094:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 303 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 19;}
	goto st1688;
st1688:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1688;
case 1688:
#line 20696 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr2095;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st815;
	} else
		goto st815;
	goto tr2088;
tr2095:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 303 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 19;}
	goto st1689;
st1689:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1689;
case 1689:
#line 20716 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr2096;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st814;
	} else
		goto st814;
	goto tr2088;
tr2096:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 303 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 19;}
	goto st1690;
st1690:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1690;
case 1690:
#line 20736 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st1691;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st813;
	} else
		goto st813;
	goto tr2088;
st1691:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1691;
case 1691:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st1692;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st1693;
	} else
		goto st1693;
	goto tr2088;
st1692:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1692;
case 1692:
	if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
		goto st1692;
	goto tr2088;
st1693:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1693;
case 1693:
	goto tr2088;
st813:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof813;
case 813:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st1693;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st1693;
	} else
		goto st1693;
	goto tr207;
st814:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof814;
case 814:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st813;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st813;
	} else
		goto st813;
	goto tr207;
st815:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof815;
case 815:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st814;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st814;
	} else
		goto st814;
	goto tr207;
st816:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof816;
case 816:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st815;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st815;
	} else
		goto st815;
	goto tr207;
st817:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof817;
case 817:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st816;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st816;
	} else
		goto st816;
	goto tr207;
st818:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof818;
case 818:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st817;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st817;
	} else
		goto st817;
	goto tr207;
st819:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof819;
case 819:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st818;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st818;
	} else
		goto st818;
	goto tr207;
st820:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof820;
case 820:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st819;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st819;
	} else
		goto st819;
	goto tr207;
st821:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof821;
case 821:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st820;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st820;
	} else
		goto st820;
	goto tr207;
tr986:
#line 71 "ext/dtext/dtext.cpp.rl"
	{ sm->a1 = sm->p; }
	goto st822;
st822:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof822;
case 822:
#line 20896 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st821;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st821;
	} else
		goto st821;
	goto tr210;
tr1789:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 71 "ext/dtext/dtext.cpp.rl"
	{ sm->a1 = sm->p; }
#line 583 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 80;}
	goto st1694;
st1694:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1694;
case 1694:
#line 20918 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 73: goto tr2099;
		case 91: goto tr1820;
		case 105: goto tr2099;
		case 123: goto tr1821;
	}
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1819;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto tr1819;
	} else
		goto tr1819;
	goto tr1796;
tr2099:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 583 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 80;}
	goto st1695;
st1695:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1695;
case 1695:
#line 20944 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 75: goto tr2100;
		case 91: goto tr1820;
		case 107: goto tr2100;
		case 123: goto tr1821;
	}
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1819;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto tr1819;
	} else
		goto tr1819;
	goto tr1796;
tr2100:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 583 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 80;}
	goto st1696;
st1696:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1696;
case 1696:
#line 20970 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 73: goto tr2101;
		case 91: goto tr1820;
		case 105: goto tr2101;
		case 123: goto tr1821;
	}
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1819;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto tr1819;
	} else
		goto tr1819;
	goto tr1796;
tr2101:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 583 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 80;}
	goto st1697;
st1697:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1697;
case 1697:
#line 20996 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 32: goto st823;
		case 91: goto tr1820;
		case 123: goto tr1821;
	}
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1819;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto tr1819;
	} else
		goto tr1819;
	goto tr1796;
st823:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof823;
case 823:
	if ( (*( sm->p)) == 35 )
		goto st824;
	goto tr210;
st824:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof824;
case 824:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr998;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto tr999;
	} else
		goto tr999;
	goto tr210;
tr998:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 71 "ext/dtext/dtext.cpp.rl"
	{ sm->a1 = sm->p; }
#line 311 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 27;}
	goto st1698;
st1698:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1698;
case 1698:
#line 21043 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr2104;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st833;
	} else
		goto st833;
	goto tr2103;
tr2104:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 311 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 27;}
	goto st1699;
st1699:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1699;
case 1699:
#line 21063 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr2105;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st832;
	} else
		goto st832;
	goto tr2103;
tr2105:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 311 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 27;}
	goto st1700;
st1700:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1700;
case 1700:
#line 21083 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr2106;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st831;
	} else
		goto st831;
	goto tr2103;
tr2106:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 311 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 27;}
	goto st1701;
st1701:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1701;
case 1701:
#line 21103 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr2107;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st830;
	} else
		goto st830;
	goto tr2103;
tr2107:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 311 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 27;}
	goto st1702;
st1702:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1702;
case 1702:
#line 21123 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr2108;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st829;
	} else
		goto st829;
	goto tr2103;
tr2108:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 311 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 27;}
	goto st1703;
st1703:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1703;
case 1703:
#line 21143 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr2109;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st828;
	} else
		goto st828;
	goto tr2103;
tr2109:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 311 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 27;}
	goto st1704;
st1704:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1704;
case 1704:
#line 21163 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr2110;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st827;
	} else
		goto st827;
	goto tr2103;
tr2110:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 311 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 27;}
	goto st1705;
st1705:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1705;
case 1705:
#line 21183 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr2111;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st826;
	} else
		goto st826;
	goto tr2103;
tr2111:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 311 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 27;}
	goto st1706;
st1706:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1706;
case 1706:
#line 21203 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st1707;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st825;
	} else
		goto st825;
	goto tr2103;
st1707:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1707;
case 1707:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st1708;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st1709;
	} else
		goto st1709;
	goto tr2103;
st1708:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1708;
case 1708:
	if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
		goto st1708;
	goto tr2103;
st1709:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1709;
case 1709:
	goto tr2103;
st825:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof825;
case 825:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st1709;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st1709;
	} else
		goto st1709;
	goto tr207;
st826:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof826;
case 826:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st825;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st825;
	} else
		goto st825;
	goto tr207;
st827:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof827;
case 827:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st826;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st826;
	} else
		goto st826;
	goto tr207;
st828:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof828;
case 828:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st827;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st827;
	} else
		goto st827;
	goto tr207;
st829:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof829;
case 829:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st828;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st828;
	} else
		goto st828;
	goto tr207;
st830:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof830;
case 830:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st829;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st829;
	} else
		goto st829;
	goto tr207;
st831:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof831;
case 831:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st830;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st830;
	} else
		goto st830;
	goto tr207;
st832:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof832;
case 832:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st831;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st831;
	} else
		goto st831;
	goto tr207;
st833:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof833;
case 833:
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st832;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st832;
	} else
		goto st832;
	goto tr207;
tr999:
#line 71 "ext/dtext/dtext.cpp.rl"
	{ sm->a1 = sm->p; }
	goto st834;
st834:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof834;
case 834:
#line 21363 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st833;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st833;
	} else
		goto st833;
	goto tr210;
tr1790:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 71 "ext/dtext/dtext.cpp.rl"
	{ sm->a1 = sm->p; }
#line 72 "ext/dtext/dtext.cpp.rl"
	{ sm->a2 = sm->p; }
#line 587 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 81;}
	goto st1710;
st1710:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1710;
case 1710:
#line 21387 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 47: goto st835;
		case 66: goto st840;
		case 67: goto st842;
		case 69: goto st862;
		case 72: goto tr2118;
		case 73: goto st883;
		case 78: goto st884;
		case 81: goto st892;
		case 83: goto st897;
		case 84: goto st905;
		case 85: goto st907;
		case 91: goto st407;
		case 98: goto st840;
		case 99: goto st842;
		case 101: goto st862;
		case 104: goto tr2118;
		case 105: goto st883;
		case 110: goto st884;
		case 113: goto st892;
		case 115: goto st897;
		case 116: goto st905;
		case 117: goto st907;
	}
	goto tr1795;
st835:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof835;
case 835:
	switch( (*( sm->p)) ) {
		case 66: goto st836;
		case 67: goto st229;
		case 69: goto st296;
		case 73: goto st837;
		case 81: goto st252;
		case 83: goto st838;
		case 84: goto st239;
		case 85: goto st839;
		case 98: goto st836;
		case 99: goto st229;
		case 101: goto st296;
		case 105: goto st837;
		case 113: goto st252;
		case 115: goto st838;
		case 116: goto st239;
		case 117: goto st839;
	}
	goto tr214;
st836:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof836;
case 836:
	if ( (*( sm->p)) == 93 )
		goto tr1014;
	goto tr214;
st837:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof837;
case 837:
	if ( (*( sm->p)) == 93 )
		goto tr1015;
	goto tr214;
st838:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof838;
case 838:
	switch( (*( sm->p)) ) {
		case 80: goto st303;
		case 93: goto tr1016;
		case 112: goto st303;
	}
	goto tr214;
st839:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof839;
case 839:
	if ( (*( sm->p)) == 93 )
		goto tr1017;
	goto tr214;
st840:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof840;
case 840:
	switch( (*( sm->p)) ) {
		case 82: goto st841;
		case 93: goto tr1019;
		case 114: goto st841;
	}
	goto tr214;
st841:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof841;
case 841:
	if ( (*( sm->p)) == 93 )
		goto tr1020;
	goto tr214;
st842:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof842;
case 842:
	switch( (*( sm->p)) ) {
		case 69: goto st843;
		case 79: goto st848;
		case 101: goto st843;
		case 111: goto st848;
	}
	goto tr214;
st843:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof843;
case 843:
	switch( (*( sm->p)) ) {
		case 78: goto st844;
		case 110: goto st844;
	}
	goto tr214;
st844:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof844;
case 844:
	switch( (*( sm->p)) ) {
		case 84: goto st845;
		case 116: goto st845;
	}
	goto tr214;
st845:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof845;
case 845:
	switch( (*( sm->p)) ) {
		case 69: goto st846;
		case 101: goto st846;
	}
	goto tr214;
st846:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof846;
case 846:
	switch( (*( sm->p)) ) {
		case 82: goto st847;
		case 114: goto st847;
	}
	goto tr214;
st847:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof847;
case 847:
	if ( (*( sm->p)) == 93 )
		goto tr1027;
	goto tr214;
st848:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof848;
case 848:
	switch( (*( sm->p)) ) {
		case 68: goto st849;
		case 76: goto st856;
		case 100: goto st849;
		case 108: goto st856;
	}
	goto tr214;
st849:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof849;
case 849:
	switch( (*( sm->p)) ) {
		case 69: goto st850;
		case 101: goto st850;
	}
	goto tr214;
st850:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof850;
case 850:
	switch( (*( sm->p)) ) {
		case 9: goto st851;
		case 32: goto st851;
		case 61: goto st852;
		case 93: goto tr1033;
	}
	goto tr214;
st851:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof851;
case 851:
	switch( (*( sm->p)) ) {
		case 9: goto st851;
		case 32: goto st851;
		case 61: goto st852;
	}
	goto tr214;
st852:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof852;
case 852:
	switch( (*( sm->p)) ) {
		case 9: goto st852;
		case 32: goto st852;
	}
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1034;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto tr1034;
	} else
		goto tr1034;
	goto tr214;
tr1034:
#line 71 "ext/dtext/dtext.cpp.rl"
	{ sm->a1 = sm->p; }
	goto st853;
st853:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof853;
case 853:
#line 21604 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) == 93 )
		goto tr1036;
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st853;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st853;
	} else
		goto st853;
	goto tr214;
tr1036:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 72 "ext/dtext/dtext.cpp.rl"
	{ sm->a2 = sm->p; }
	goto st1711;
st1711:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1711;
case 1711:
#line 21626 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 0: goto tr1038;
		case 9: goto st854;
		case 10: goto tr1038;
		case 32: goto st854;
	}
	goto tr2125;
st854:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof854;
case 854:
	switch( (*( sm->p)) ) {
		case 0: goto tr1038;
		case 9: goto st854;
		case 10: goto tr1038;
		case 32: goto st854;
	}
	goto tr1037;
tr1033:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
	goto st1712;
st1712:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1712;
case 1712:
#line 21653 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 0: goto tr1041;
		case 9: goto st855;
		case 10: goto tr1041;
		case 32: goto st855;
	}
	goto tr2126;
st855:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof855;
case 855:
	switch( (*( sm->p)) ) {
		case 0: goto tr1041;
		case 9: goto st855;
		case 10: goto tr1041;
		case 32: goto st855;
	}
	goto tr1040;
st856:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof856;
case 856:
	switch( (*( sm->p)) ) {
		case 79: goto st857;
		case 111: goto st857;
	}
	goto tr214;
st857:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof857;
case 857:
	switch( (*( sm->p)) ) {
		case 82: goto st858;
		case 114: goto st858;
	}
	goto tr214;
st858:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof858;
case 858:
	switch( (*( sm->p)) ) {
		case 9: goto st859;
		case 32: goto st859;
		case 61: goto st861;
		case 93: goto tr1047;
	}
	goto tr214;
tr1049:
#line 71 "ext/dtext/dtext.cpp.rl"
	{ sm->a1 = sm->p; }
	goto st859;
st859:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof859;
case 859:
#line 21709 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 0: goto tr214;
		case 9: goto tr1049;
		case 10: goto tr214;
		case 13: goto tr214;
		case 32: goto tr1049;
		case 61: goto tr1050;
		case 93: goto tr1051;
	}
	goto tr1048;
tr1048:
#line 71 "ext/dtext/dtext.cpp.rl"
	{ sm->a1 = sm->p; }
	goto st860;
st860:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof860;
case 860:
#line 21728 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 0: goto tr214;
		case 10: goto tr214;
		case 13: goto tr214;
		case 93: goto tr1053;
	}
	goto st860;
tr1050:
#line 71 "ext/dtext/dtext.cpp.rl"
	{ sm->a1 = sm->p; }
	goto st861;
st861:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof861;
case 861:
#line 21744 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 0: goto tr214;
		case 9: goto tr1050;
		case 10: goto tr214;
		case 13: goto tr214;
		case 32: goto tr1050;
		case 93: goto tr1051;
	}
	goto tr1048;
st862:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof862;
case 862:
	switch( (*( sm->p)) ) {
		case 88: goto st863;
		case 120: goto st863;
	}
	goto tr214;
st863:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof863;
case 863:
	switch( (*( sm->p)) ) {
		case 80: goto st864;
		case 112: goto st864;
	}
	goto tr214;
st864:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof864;
case 864:
	switch( (*( sm->p)) ) {
		case 65: goto st865;
		case 97: goto st865;
	}
	goto tr214;
st865:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof865;
case 865:
	switch( (*( sm->p)) ) {
		case 78: goto st866;
		case 110: goto st866;
	}
	goto tr214;
st866:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof866;
case 866:
	switch( (*( sm->p)) ) {
		case 68: goto st867;
		case 100: goto st867;
	}
	goto tr214;
st867:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof867;
case 867:
	switch( (*( sm->p)) ) {
		case 9: goto st868;
		case 32: goto st868;
		case 61: goto st870;
		case 93: goto tr1061;
	}
	goto tr214;
tr1063:
#line 71 "ext/dtext/dtext.cpp.rl"
	{ sm->a1 = sm->p; }
	goto st868;
st868:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof868;
case 868:
#line 21818 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 0: goto tr214;
		case 9: goto tr1063;
		case 10: goto tr214;
		case 13: goto tr214;
		case 32: goto tr1063;
		case 61: goto tr1064;
		case 93: goto tr1065;
	}
	goto tr1062;
tr1062:
#line 71 "ext/dtext/dtext.cpp.rl"
	{ sm->a1 = sm->p; }
	goto st869;
st869:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof869;
case 869:
#line 21837 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 0: goto tr214;
		case 10: goto tr214;
		case 13: goto tr214;
		case 93: goto tr1067;
	}
	goto st869;
tr1064:
#line 71 "ext/dtext/dtext.cpp.rl"
	{ sm->a1 = sm->p; }
	goto st870;
st870:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof870;
case 870:
#line 21853 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 0: goto tr214;
		case 9: goto tr1064;
		case 10: goto tr214;
		case 13: goto tr214;
		case 32: goto tr1064;
		case 93: goto tr1065;
	}
	goto tr1062;
tr2118:
#line 71 "ext/dtext/dtext.cpp.rl"
	{ sm->a1 = sm->p; }
	goto st871;
st871:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof871;
case 871:
#line 21871 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 84: goto st872;
		case 116: goto st872;
	}
	goto tr214;
st872:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof872;
case 872:
	switch( (*( sm->p)) ) {
		case 84: goto st873;
		case 116: goto st873;
	}
	goto tr214;
st873:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof873;
case 873:
	switch( (*( sm->p)) ) {
		case 80: goto st874;
		case 112: goto st874;
	}
	goto tr214;
st874:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof874;
case 874:
	switch( (*( sm->p)) ) {
		case 58: goto st875;
		case 83: goto st882;
		case 115: goto st882;
	}
	goto tr214;
st875:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof875;
case 875:
	if ( (*( sm->p)) == 47 )
		goto st876;
	goto tr214;
st876:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof876;
case 876:
	if ( (*( sm->p)) == 47 )
		goto st877;
	goto tr214;
st877:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof877;
case 877:
	switch( (*( sm->p)) ) {
		case 0: goto tr214;
		case 32: goto tr214;
	}
	if ( 9 <= (*( sm->p)) && (*( sm->p)) <= 13 )
		goto tr214;
	goto st878;
st878:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof878;
case 878:
	switch( (*( sm->p)) ) {
		case 0: goto tr214;
		case 32: goto tr214;
		case 93: goto tr1076;
	}
	if ( 9 <= (*( sm->p)) && (*( sm->p)) <= 13 )
		goto tr214;
	goto st878;
tr1076:
#line 72 "ext/dtext/dtext.cpp.rl"
	{ sm->a2 = sm->p; }
	goto st879;
st879:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof879;
case 879:
#line 21950 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 0: goto tr214;
		case 32: goto tr214;
		case 40: goto st880;
		case 93: goto tr1076;
	}
	if ( 9 <= (*( sm->p)) && (*( sm->p)) <= 13 )
		goto tr214;
	goto st878;
st880:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof880;
case 880:
	switch( (*( sm->p)) ) {
		case 0: goto tr214;
		case 10: goto tr214;
		case 13: goto tr214;
	}
	goto tr1078;
tr1078:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ sm->b1 = sm->p; }
	goto st881;
st881:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof881;
case 881:
#line 21978 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 0: goto tr214;
		case 10: goto tr214;
		case 13: goto tr214;
		case 41: goto tr1080;
	}
	goto st881;
st882:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof882;
case 882:
	if ( (*( sm->p)) == 58 )
		goto st875;
	goto tr214;
st883:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof883;
case 883:
	if ( (*( sm->p)) == 93 )
		goto tr1081;
	goto tr214;
st884:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof884;
case 884:
	switch( (*( sm->p)) ) {
		case 79: goto st885;
		case 111: goto st885;
	}
	goto tr214;
st885:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof885;
case 885:
	switch( (*( sm->p)) ) {
		case 68: goto st886;
		case 100: goto st886;
	}
	goto tr214;
st886:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof886;
case 886:
	switch( (*( sm->p)) ) {
		case 84: goto st887;
		case 116: goto st887;
	}
	goto tr214;
st887:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof887;
case 887:
	switch( (*( sm->p)) ) {
		case 69: goto st888;
		case 101: goto st888;
	}
	goto tr214;
st888:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof888;
case 888:
	switch( (*( sm->p)) ) {
		case 88: goto st889;
		case 120: goto st889;
	}
	goto tr214;
st889:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof889;
case 889:
	switch( (*( sm->p)) ) {
		case 84: goto st890;
		case 116: goto st890;
	}
	goto tr214;
st890:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof890;
case 890:
	if ( (*( sm->p)) == 93 )
		goto tr1088;
	goto tr214;
tr1088:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
	goto st1713;
st1713:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1713;
case 1713:
#line 22069 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 0: goto tr1090;
		case 9: goto st891;
		case 10: goto tr1090;
		case 32: goto st891;
	}
	goto tr2127;
st891:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof891;
case 891:
	switch( (*( sm->p)) ) {
		case 0: goto tr1090;
		case 9: goto st891;
		case 10: goto tr1090;
		case 32: goto st891;
	}
	goto tr1089;
st892:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof892;
case 892:
	switch( (*( sm->p)) ) {
		case 85: goto st893;
		case 117: goto st893;
	}
	goto tr214;
st893:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof893;
case 893:
	switch( (*( sm->p)) ) {
		case 79: goto st894;
		case 111: goto st894;
	}
	goto tr214;
st894:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof894;
case 894:
	switch( (*( sm->p)) ) {
		case 84: goto st895;
		case 116: goto st895;
	}
	goto tr214;
st895:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof895;
case 895:
	switch( (*( sm->p)) ) {
		case 69: goto st896;
		case 101: goto st896;
	}
	goto tr214;
st896:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof896;
case 896:
	if ( (*( sm->p)) == 93 )
		goto tr1096;
	goto tr214;
st897:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof897;
case 897:
	switch( (*( sm->p)) ) {
		case 80: goto st898;
		case 93: goto tr1098;
		case 112: goto st898;
	}
	goto tr214;
st898:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof898;
case 898:
	switch( (*( sm->p)) ) {
		case 79: goto st899;
		case 111: goto st899;
	}
	goto tr214;
st899:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof899;
case 899:
	switch( (*( sm->p)) ) {
		case 73: goto st900;
		case 105: goto st900;
	}
	goto tr214;
st900:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof900;
case 900:
	switch( (*( sm->p)) ) {
		case 76: goto st901;
		case 108: goto st901;
	}
	goto tr214;
st901:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof901;
case 901:
	switch( (*( sm->p)) ) {
		case 69: goto st902;
		case 101: goto st902;
	}
	goto tr214;
st902:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof902;
case 902:
	switch( (*( sm->p)) ) {
		case 82: goto st903;
		case 114: goto st903;
	}
	goto tr214;
st903:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof903;
case 903:
	switch( (*( sm->p)) ) {
		case 83: goto st904;
		case 93: goto tr1105;
		case 115: goto st904;
	}
	goto tr214;
st904:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof904;
case 904:
	if ( (*( sm->p)) == 93 )
		goto tr1105;
	goto tr214;
st905:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof905;
case 905:
	switch( (*( sm->p)) ) {
		case 78: goto st906;
		case 110: goto st906;
	}
	goto tr214;
st906:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof906;
case 906:
	if ( (*( sm->p)) == 93 )
		goto tr1107;
	goto tr214;
st907:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof907;
case 907:
	switch( (*( sm->p)) ) {
		case 82: goto st908;
		case 93: goto tr1109;
		case 114: goto st908;
	}
	goto tr214;
st908:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof908;
case 908:
	switch( (*( sm->p)) ) {
		case 76: goto st909;
		case 108: goto st909;
	}
	goto tr214;
st909:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof909;
case 909:
	switch( (*( sm->p)) ) {
		case 9: goto st910;
		case 32: goto st910;
		case 61: goto st911;
		case 93: goto st951;
	}
	goto tr214;
st910:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof910;
case 910:
	switch( (*( sm->p)) ) {
		case 9: goto st910;
		case 32: goto st910;
		case 61: goto st911;
	}
	goto tr214;
st911:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof911;
case 911:
	switch( (*( sm->p)) ) {
		case 9: goto st911;
		case 32: goto st911;
		case 34: goto st912;
		case 35: goto tr1115;
		case 39: goto st933;
		case 47: goto tr1115;
		case 72: goto tr1117;
		case 104: goto tr1117;
	}
	goto tr214;
st912:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof912;
case 912:
	switch( (*( sm->p)) ) {
		case 35: goto tr1118;
		case 47: goto tr1118;
		case 72: goto tr1119;
		case 104: goto tr1119;
	}
	goto tr214;
tr1118:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ sm->b1 = sm->p; }
	goto st913;
st913:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof913;
case 913:
#line 22293 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 0: goto tr214;
		case 32: goto tr214;
		case 34: goto tr1121;
	}
	if ( 9 <= (*( sm->p)) && (*( sm->p)) <= 13 )
		goto tr214;
	goto st913;
tr1121:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ sm->b2 = sm->p; }
	goto st914;
st914:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof914;
case 914:
#line 22310 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 9: goto st914;
		case 32: goto st914;
		case 93: goto st915;
	}
	goto tr214;
tr1144:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ sm->b2 = sm->p; }
	goto st915;
st915:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof915;
case 915:
#line 22325 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 0: goto tr214;
		case 9: goto tr1125;
		case 10: goto tr214;
		case 13: goto tr214;
		case 32: goto tr1125;
	}
	goto tr1124;
tr1124:
#line 71 "ext/dtext/dtext.cpp.rl"
	{ sm->a1 = sm->p; }
	goto st916;
st916:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof916;
case 916:
#line 22342 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 0: goto tr214;
		case 9: goto tr1127;
		case 10: goto tr214;
		case 13: goto tr214;
		case 32: goto tr1127;
		case 91: goto tr1128;
	}
	goto st916;
tr1127:
#line 72 "ext/dtext/dtext.cpp.rl"
	{ sm->a2 = sm->p; }
	goto st917;
st917:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof917;
case 917:
#line 22360 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 9: goto st917;
		case 32: goto st917;
		case 91: goto st918;
	}
	goto tr214;
tr1128:
#line 72 "ext/dtext/dtext.cpp.rl"
	{ sm->a2 = sm->p; }
	goto st918;
st918:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof918;
case 918:
#line 22375 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) == 47 )
		goto st919;
	goto tr214;
st919:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof919;
case 919:
	switch( (*( sm->p)) ) {
		case 85: goto st920;
		case 117: goto st920;
	}
	goto tr214;
st920:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof920;
case 920:
	switch( (*( sm->p)) ) {
		case 82: goto st921;
		case 114: goto st921;
	}
	goto tr214;
st921:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof921;
case 921:
	switch( (*( sm->p)) ) {
		case 76: goto st922;
		case 108: goto st922;
	}
	goto tr214;
st922:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof922;
case 922:
	if ( (*( sm->p)) == 93 )
		goto tr1135;
	goto tr214;
tr1125:
#line 71 "ext/dtext/dtext.cpp.rl"
	{ sm->a1 = sm->p; }
	goto st923;
st923:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof923;
case 923:
#line 22421 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 0: goto tr214;
		case 9: goto tr1127;
		case 10: goto tr214;
		case 13: goto tr214;
		case 32: goto tr1127;
		case 91: goto tr1128;
	}
	goto tr1124;
tr1119:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ sm->b1 = sm->p; }
	goto st924;
st924:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof924;
case 924:
#line 22439 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 84: goto st925;
		case 116: goto st925;
	}
	goto tr214;
st925:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof925;
case 925:
	switch( (*( sm->p)) ) {
		case 84: goto st926;
		case 116: goto st926;
	}
	goto tr214;
st926:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof926;
case 926:
	switch( (*( sm->p)) ) {
		case 80: goto st927;
		case 112: goto st927;
	}
	goto tr214;
st927:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof927;
case 927:
	switch( (*( sm->p)) ) {
		case 58: goto st928;
		case 83: goto st931;
		case 115: goto st931;
	}
	goto tr214;
st928:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof928;
case 928:
	if ( (*( sm->p)) == 47 )
		goto st929;
	goto tr214;
st929:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof929;
case 929:
	if ( (*( sm->p)) == 47 )
		goto st930;
	goto tr214;
st930:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof930;
case 930:
	switch( (*( sm->p)) ) {
		case 0: goto tr214;
		case 32: goto tr214;
	}
	if ( 9 <= (*( sm->p)) && (*( sm->p)) <= 13 )
		goto tr214;
	goto st913;
st931:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof931;
case 931:
	if ( (*( sm->p)) == 58 )
		goto st928;
	goto tr214;
tr1115:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ sm->b1 = sm->p; }
	goto st932;
st932:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof932;
case 932:
#line 22513 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 0: goto tr214;
		case 9: goto tr1121;
		case 32: goto tr1121;
		case 93: goto tr1144;
	}
	if ( 10 <= (*( sm->p)) && (*( sm->p)) <= 13 )
		goto tr214;
	goto st932;
st933:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof933;
case 933:
	switch( (*( sm->p)) ) {
		case 35: goto tr1145;
		case 47: goto tr1145;
		case 72: goto tr1146;
		case 104: goto tr1146;
	}
	goto tr214;
tr1145:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ sm->b1 = sm->p; }
	goto st934;
st934:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof934;
case 934:
#line 22542 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 0: goto tr214;
		case 32: goto tr214;
		case 39: goto tr1121;
	}
	if ( 9 <= (*( sm->p)) && (*( sm->p)) <= 13 )
		goto tr214;
	goto st934;
tr1146:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ sm->b1 = sm->p; }
	goto st935;
st935:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof935;
case 935:
#line 22559 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 84: goto st936;
		case 116: goto st936;
	}
	goto tr214;
st936:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof936;
case 936:
	switch( (*( sm->p)) ) {
		case 84: goto st937;
		case 116: goto st937;
	}
	goto tr214;
st937:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof937;
case 937:
	switch( (*( sm->p)) ) {
		case 80: goto st938;
		case 112: goto st938;
	}
	goto tr214;
st938:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof938;
case 938:
	switch( (*( sm->p)) ) {
		case 58: goto st939;
		case 83: goto st942;
		case 115: goto st942;
	}
	goto tr214;
st939:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof939;
case 939:
	if ( (*( sm->p)) == 47 )
		goto st940;
	goto tr214;
st940:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof940;
case 940:
	if ( (*( sm->p)) == 47 )
		goto st941;
	goto tr214;
st941:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof941;
case 941:
	switch( (*( sm->p)) ) {
		case 0: goto tr214;
		case 32: goto tr214;
	}
	if ( 9 <= (*( sm->p)) && (*( sm->p)) <= 13 )
		goto tr214;
	goto st934;
st942:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof942;
case 942:
	if ( (*( sm->p)) == 58 )
		goto st939;
	goto tr214;
tr1117:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ sm->b1 = sm->p; }
	goto st943;
st943:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof943;
case 943:
#line 22633 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 84: goto st944;
		case 116: goto st944;
	}
	goto tr214;
st944:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof944;
case 944:
	switch( (*( sm->p)) ) {
		case 84: goto st945;
		case 116: goto st945;
	}
	goto tr214;
st945:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof945;
case 945:
	switch( (*( sm->p)) ) {
		case 80: goto st946;
		case 112: goto st946;
	}
	goto tr214;
st946:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof946;
case 946:
	switch( (*( sm->p)) ) {
		case 58: goto st947;
		case 83: goto st950;
		case 115: goto st950;
	}
	goto tr214;
st947:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof947;
case 947:
	if ( (*( sm->p)) == 47 )
		goto st948;
	goto tr214;
st948:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof948;
case 948:
	if ( (*( sm->p)) == 47 )
		goto st949;
	goto tr214;
st949:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof949;
case 949:
	switch( (*( sm->p)) ) {
		case 0: goto tr214;
		case 32: goto tr214;
	}
	if ( 9 <= (*( sm->p)) && (*( sm->p)) <= 13 )
		goto tr214;
	goto st932;
st950:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof950;
case 950:
	if ( (*( sm->p)) == 58 )
		goto st947;
	goto tr214;
st951:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof951;
case 951:
	switch( (*( sm->p)) ) {
		case 9: goto st951;
		case 32: goto st951;
		case 35: goto tr1162;
		case 47: goto tr1162;
		case 72: goto tr1163;
		case 104: goto tr1163;
	}
	goto tr214;
tr1162:
#line 71 "ext/dtext/dtext.cpp.rl"
	{ sm->a1 = sm->p; }
	goto st952;
st952:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof952;
case 952:
#line 22720 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 0: goto tr214;
		case 9: goto tr1165;
		case 32: goto tr1165;
		case 91: goto tr1166;
	}
	if ( 10 <= (*( sm->p)) && (*( sm->p)) <= 13 )
		goto tr214;
	goto st952;
tr1165:
#line 72 "ext/dtext/dtext.cpp.rl"
	{ sm->a2 = sm->p; }
	goto st953;
st953:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof953;
case 953:
#line 22738 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 9: goto st953;
		case 32: goto st953;
		case 91: goto st954;
	}
	goto tr214;
st954:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof954;
case 954:
	if ( (*( sm->p)) == 47 )
		goto st955;
	goto tr214;
st955:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof955;
case 955:
	switch( (*( sm->p)) ) {
		case 85: goto st956;
		case 117: goto st956;
	}
	goto tr214;
st956:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof956;
case 956:
	switch( (*( sm->p)) ) {
		case 82: goto st957;
		case 114: goto st957;
	}
	goto tr214;
st957:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof957;
case 957:
	switch( (*( sm->p)) ) {
		case 76: goto st958;
		case 108: goto st958;
	}
	goto tr214;
st958:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof958;
case 958:
	if ( (*( sm->p)) == 93 )
		goto tr1173;
	goto tr214;
tr1166:
#line 72 "ext/dtext/dtext.cpp.rl"
	{ sm->a2 = sm->p; }
	goto st959;
st959:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof959;
case 959:
#line 22794 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 0: goto tr214;
		case 9: goto tr1165;
		case 32: goto tr1165;
		case 47: goto st960;
		case 91: goto tr1166;
	}
	if ( 10 <= (*( sm->p)) && (*( sm->p)) <= 13 )
		goto tr214;
	goto st952;
st960:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof960;
case 960:
	switch( (*( sm->p)) ) {
		case 0: goto tr214;
		case 9: goto tr1165;
		case 32: goto tr1165;
		case 85: goto st961;
		case 91: goto tr1166;
		case 117: goto st961;
	}
	if ( 10 <= (*( sm->p)) && (*( sm->p)) <= 13 )
		goto tr214;
	goto st952;
st961:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof961;
case 961:
	switch( (*( sm->p)) ) {
		case 0: goto tr214;
		case 9: goto tr1165;
		case 32: goto tr1165;
		case 82: goto st962;
		case 91: goto tr1166;
		case 114: goto st962;
	}
	if ( 10 <= (*( sm->p)) && (*( sm->p)) <= 13 )
		goto tr214;
	goto st952;
st962:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof962;
case 962:
	switch( (*( sm->p)) ) {
		case 0: goto tr214;
		case 9: goto tr1165;
		case 32: goto tr1165;
		case 76: goto st963;
		case 91: goto tr1166;
		case 108: goto st963;
	}
	if ( 10 <= (*( sm->p)) && (*( sm->p)) <= 13 )
		goto tr214;
	goto st952;
st963:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof963;
case 963:
	switch( (*( sm->p)) ) {
		case 0: goto tr214;
		case 9: goto tr1165;
		case 32: goto tr1165;
		case 91: goto tr1166;
		case 93: goto tr1173;
	}
	if ( 10 <= (*( sm->p)) && (*( sm->p)) <= 13 )
		goto tr214;
	goto st952;
tr1163:
#line 71 "ext/dtext/dtext.cpp.rl"
	{ sm->a1 = sm->p; }
	goto st964;
st964:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof964;
case 964:
#line 22872 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 84: goto st965;
		case 116: goto st965;
	}
	goto tr214;
st965:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof965;
case 965:
	switch( (*( sm->p)) ) {
		case 84: goto st966;
		case 116: goto st966;
	}
	goto tr214;
st966:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof966;
case 966:
	switch( (*( sm->p)) ) {
		case 80: goto st967;
		case 112: goto st967;
	}
	goto tr214;
st967:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof967;
case 967:
	switch( (*( sm->p)) ) {
		case 58: goto st968;
		case 83: goto st971;
		case 115: goto st971;
	}
	goto tr214;
st968:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof968;
case 968:
	if ( (*( sm->p)) == 47 )
		goto st969;
	goto tr214;
st969:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof969;
case 969:
	if ( (*( sm->p)) == 47 )
		goto st970;
	goto tr214;
st970:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof970;
case 970:
	switch( (*( sm->p)) ) {
		case 0: goto tr214;
		case 32: goto tr214;
	}
	if ( 9 <= (*( sm->p)) && (*( sm->p)) <= 13 )
		goto tr214;
	goto st952;
st971:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof971;
case 971:
	if ( (*( sm->p)) == 58 )
		goto st968;
	goto tr214;
tr1791:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 71 "ext/dtext/dtext.cpp.rl"
	{ sm->a1 = sm->p; }
#line 72 "ext/dtext/dtext.cpp.rl"
	{ sm->a2 = sm->p; }
#line 587 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 81;}
	goto st1714;
st1714:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1714;
case 1714:
#line 22952 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) == 123 )
		goto st460;
	goto tr1795;
tr1792:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 587 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 81;}
	goto st1715;
st1715:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1715;
case 1715:
#line 22966 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 47: goto st972;
		case 65: goto st983;
		case 66: goto st1006;
		case 67: goto st1017;
		case 69: goto st1035;
		case 72: goto tr2133;
		case 73: goto st1036;
		case 78: goto st1054;
		case 81: goto st1011;
		case 83: goto st1061;
		case 84: goto st1074;
		case 85: goto st1076;
		case 97: goto st983;
		case 98: goto st1006;
		case 99: goto st1017;
		case 101: goto st1035;
		case 104: goto tr2133;
		case 105: goto st1036;
		case 110: goto st1054;
		case 113: goto st1011;
		case 115: goto st1061;
		case 116: goto st1074;
		case 117: goto st1076;
	}
	goto tr1795;
st972:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof972;
case 972:
	switch( (*( sm->p)) ) {
		case 66: goto st973;
		case 67: goto st213;
		case 69: goto st974;
		case 73: goto st975;
		case 81: goto st263;
		case 83: goto st976;
		case 84: goto st223;
		case 85: goto st982;
		case 98: goto st973;
		case 99: goto st213;
		case 101: goto st974;
		case 105: goto st975;
		case 113: goto st263;
		case 115: goto st976;
		case 116: goto st223;
		case 117: goto st982;
	}
	goto tr214;
st973:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof973;
case 973:
	switch( (*( sm->p)) ) {
		case 62: goto tr1014;
		case 76: goto st248;
		case 108: goto st248;
	}
	goto tr214;
st974:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof974;
case 974:
	switch( (*( sm->p)) ) {
		case 77: goto st975;
		case 88: goto st258;
		case 109: goto st975;
		case 120: goto st258;
	}
	goto tr214;
st975:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof975;
case 975:
	if ( (*( sm->p)) == 62 )
		goto tr1015;
	goto tr214;
st976:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof976;
case 976:
	switch( (*( sm->p)) ) {
		case 62: goto tr1016;
		case 80: goto st269;
		case 84: goto st977;
		case 112: goto st269;
		case 116: goto st977;
	}
	goto tr214;
st977:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof977;
case 977:
	switch( (*( sm->p)) ) {
		case 82: goto st978;
		case 114: goto st978;
	}
	goto tr214;
st978:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof978;
case 978:
	switch( (*( sm->p)) ) {
		case 79: goto st979;
		case 111: goto st979;
	}
	goto tr214;
st979:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof979;
case 979:
	switch( (*( sm->p)) ) {
		case 78: goto st980;
		case 110: goto st980;
	}
	goto tr214;
st980:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof980;
case 980:
	switch( (*( sm->p)) ) {
		case 71: goto st981;
		case 103: goto st981;
	}
	goto tr214;
st981:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof981;
case 981:
	if ( (*( sm->p)) == 62 )
		goto tr1014;
	goto tr214;
st982:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof982;
case 982:
	if ( (*( sm->p)) == 62 )
		goto tr1017;
	goto tr214;
st983:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof983;
case 983:
	switch( (*( sm->p)) ) {
		case 9: goto st984;
		case 32: goto st984;
	}
	goto tr214;
st984:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof984;
case 984:
	switch( (*( sm->p)) ) {
		case 9: goto st984;
		case 32: goto st984;
		case 72: goto st985;
		case 104: goto st985;
	}
	goto tr214;
st985:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof985;
case 985:
	switch( (*( sm->p)) ) {
		case 82: goto st986;
		case 114: goto st986;
	}
	goto tr214;
st986:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof986;
case 986:
	switch( (*( sm->p)) ) {
		case 69: goto st987;
		case 101: goto st987;
	}
	goto tr214;
st987:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof987;
case 987:
	switch( (*( sm->p)) ) {
		case 70: goto st988;
		case 102: goto st988;
	}
	goto tr214;
st988:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof988;
case 988:
	if ( (*( sm->p)) == 61 )
		goto st989;
	goto tr214;
st989:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof989;
case 989:
	if ( (*( sm->p)) == 34 )
		goto st990;
	goto tr214;
st990:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof990;
case 990:
	switch( (*( sm->p)) ) {
		case 35: goto tr1202;
		case 47: goto tr1202;
		case 72: goto tr1203;
		case 104: goto tr1203;
	}
	goto tr214;
tr1202:
#line 71 "ext/dtext/dtext.cpp.rl"
	{ sm->a1 = sm->p; }
	goto st991;
st991:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof991;
case 991:
#line 23186 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 0: goto tr214;
		case 32: goto tr214;
		case 34: goto tr1205;
	}
	if ( 9 <= (*( sm->p)) && (*( sm->p)) <= 13 )
		goto tr214;
	goto st991;
tr1205:
#line 72 "ext/dtext/dtext.cpp.rl"
	{ sm->a2 = sm->p; }
	goto st992;
st992:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof992;
case 992:
#line 23203 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 0: goto tr214;
		case 32: goto tr214;
		case 34: goto tr1205;
		case 62: goto st993;
	}
	if ( 9 <= (*( sm->p)) && (*( sm->p)) <= 13 )
		goto tr214;
	goto st991;
st993:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof993;
case 993:
	switch( (*( sm->p)) ) {
		case 0: goto tr214;
		case 10: goto tr214;
		case 13: goto tr214;
	}
	goto tr1207;
tr1207:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ sm->b1 = sm->p; }
	goto st994;
st994:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof994;
case 994:
#line 23231 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 0: goto tr214;
		case 10: goto tr214;
		case 13: goto tr214;
		case 60: goto tr1209;
	}
	goto st994;
tr1209:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ sm->b2 = sm->p; }
	goto st995;
st995:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof995;
case 995:
#line 23247 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 0: goto tr214;
		case 10: goto tr214;
		case 13: goto tr214;
		case 47: goto st996;
		case 60: goto tr1209;
	}
	goto st994;
st996:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof996;
case 996:
	switch( (*( sm->p)) ) {
		case 0: goto tr214;
		case 10: goto tr214;
		case 13: goto tr214;
		case 60: goto tr1209;
		case 65: goto st997;
		case 97: goto st997;
	}
	goto st994;
st997:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof997;
case 997:
	switch( (*( sm->p)) ) {
		case 0: goto tr214;
		case 10: goto tr214;
		case 13: goto tr214;
		case 60: goto tr1209;
		case 62: goto tr1212;
	}
	goto st994;
tr1203:
#line 71 "ext/dtext/dtext.cpp.rl"
	{ sm->a1 = sm->p; }
	goto st998;
st998:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof998;
case 998:
#line 23289 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 84: goto st999;
		case 116: goto st999;
	}
	goto tr214;
st999:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof999;
case 999:
	switch( (*( sm->p)) ) {
		case 84: goto st1000;
		case 116: goto st1000;
	}
	goto tr214;
st1000:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1000;
case 1000:
	switch( (*( sm->p)) ) {
		case 80: goto st1001;
		case 112: goto st1001;
	}
	goto tr214;
st1001:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1001;
case 1001:
	switch( (*( sm->p)) ) {
		case 58: goto st1002;
		case 83: goto st1005;
		case 115: goto st1005;
	}
	goto tr214;
st1002:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1002;
case 1002:
	if ( (*( sm->p)) == 47 )
		goto st1003;
	goto tr214;
st1003:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1003;
case 1003:
	if ( (*( sm->p)) == 47 )
		goto st1004;
	goto tr214;
st1004:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1004;
case 1004:
	switch( (*( sm->p)) ) {
		case 0: goto tr214;
		case 32: goto tr214;
	}
	if ( 9 <= (*( sm->p)) && (*( sm->p)) <= 13 )
		goto tr214;
	goto st991;
st1005:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1005;
case 1005:
	if ( (*( sm->p)) == 58 )
		goto st1002;
	goto tr214;
st1006:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1006;
case 1006:
	switch( (*( sm->p)) ) {
		case 62: goto tr1019;
		case 76: goto st1007;
		case 82: goto st1016;
		case 108: goto st1007;
		case 114: goto st1016;
	}
	goto tr214;
st1007:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1007;
case 1007:
	switch( (*( sm->p)) ) {
		case 79: goto st1008;
		case 111: goto st1008;
	}
	goto tr214;
st1008:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1008;
case 1008:
	switch( (*( sm->p)) ) {
		case 67: goto st1009;
		case 99: goto st1009;
	}
	goto tr214;
st1009:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1009;
case 1009:
	switch( (*( sm->p)) ) {
		case 75: goto st1010;
		case 107: goto st1010;
	}
	goto tr214;
st1010:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1010;
case 1010:
	switch( (*( sm->p)) ) {
		case 81: goto st1011;
		case 113: goto st1011;
	}
	goto tr214;
st1011:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1011;
case 1011:
	switch( (*( sm->p)) ) {
		case 85: goto st1012;
		case 117: goto st1012;
	}
	goto tr214;
st1012:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1012;
case 1012:
	switch( (*( sm->p)) ) {
		case 79: goto st1013;
		case 111: goto st1013;
	}
	goto tr214;
st1013:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1013;
case 1013:
	switch( (*( sm->p)) ) {
		case 84: goto st1014;
		case 116: goto st1014;
	}
	goto tr214;
st1014:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1014;
case 1014:
	switch( (*( sm->p)) ) {
		case 69: goto st1015;
		case 101: goto st1015;
	}
	goto tr214;
st1015:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1015;
case 1015:
	if ( (*( sm->p)) == 62 )
		goto tr1096;
	goto tr214;
st1016:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1016;
case 1016:
	if ( (*( sm->p)) == 62 )
		goto tr1020;
	goto tr214;
st1017:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1017;
case 1017:
	switch( (*( sm->p)) ) {
		case 69: goto st1018;
		case 79: goto st1023;
		case 101: goto st1018;
		case 111: goto st1023;
	}
	goto tr214;
st1018:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1018;
case 1018:
	switch( (*( sm->p)) ) {
		case 78: goto st1019;
		case 110: goto st1019;
	}
	goto tr214;
st1019:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1019;
case 1019:
	switch( (*( sm->p)) ) {
		case 84: goto st1020;
		case 116: goto st1020;
	}
	goto tr214;
st1020:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1020;
case 1020:
	switch( (*( sm->p)) ) {
		case 69: goto st1021;
		case 101: goto st1021;
	}
	goto tr214;
st1021:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1021;
case 1021:
	switch( (*( sm->p)) ) {
		case 82: goto st1022;
		case 114: goto st1022;
	}
	goto tr214;
st1022:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1022;
case 1022:
	if ( (*( sm->p)) == 62 )
		goto tr1027;
	goto tr214;
st1023:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1023;
case 1023:
	switch( (*( sm->p)) ) {
		case 68: goto st1024;
		case 76: goto st1029;
		case 100: goto st1024;
		case 108: goto st1029;
	}
	goto tr214;
st1024:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1024;
case 1024:
	switch( (*( sm->p)) ) {
		case 69: goto st1025;
		case 101: goto st1025;
	}
	goto tr214;
st1025:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1025;
case 1025:
	switch( (*( sm->p)) ) {
		case 9: goto st1026;
		case 32: goto st1026;
		case 61: goto st1027;
		case 62: goto tr1033;
	}
	goto tr214;
st1026:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1026;
case 1026:
	switch( (*( sm->p)) ) {
		case 9: goto st1026;
		case 32: goto st1026;
		case 61: goto st1027;
	}
	goto tr214;
st1027:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1027;
case 1027:
	switch( (*( sm->p)) ) {
		case 9: goto st1027;
		case 32: goto st1027;
	}
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1241;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto tr1241;
	} else
		goto tr1241;
	goto tr214;
tr1241:
#line 71 "ext/dtext/dtext.cpp.rl"
	{ sm->a1 = sm->p; }
	goto st1028;
st1028:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1028;
case 1028:
#line 23573 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) == 62 )
		goto tr1036;
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st1028;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st1028;
	} else
		goto st1028;
	goto tr214;
st1029:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1029;
case 1029:
	switch( (*( sm->p)) ) {
		case 79: goto st1030;
		case 111: goto st1030;
	}
	goto tr214;
st1030:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1030;
case 1030:
	switch( (*( sm->p)) ) {
		case 82: goto st1031;
		case 114: goto st1031;
	}
	goto tr214;
st1031:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1031;
case 1031:
	switch( (*( sm->p)) ) {
		case 9: goto st1032;
		case 32: goto st1032;
		case 61: goto st1034;
		case 62: goto tr1047;
	}
	goto tr214;
tr1248:
#line 71 "ext/dtext/dtext.cpp.rl"
	{ sm->a1 = sm->p; }
	goto st1032;
st1032:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1032;
case 1032:
#line 23622 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 0: goto tr214;
		case 9: goto tr1248;
		case 10: goto tr214;
		case 13: goto tr214;
		case 32: goto tr1248;
		case 61: goto tr1249;
		case 62: goto tr1051;
	}
	goto tr1247;
tr1247:
#line 71 "ext/dtext/dtext.cpp.rl"
	{ sm->a1 = sm->p; }
	goto st1033;
st1033:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1033;
case 1033:
#line 23641 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 0: goto tr214;
		case 10: goto tr214;
		case 13: goto tr214;
		case 62: goto tr1053;
	}
	goto st1033;
tr1249:
#line 71 "ext/dtext/dtext.cpp.rl"
	{ sm->a1 = sm->p; }
	goto st1034;
st1034:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1034;
case 1034:
#line 23657 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 0: goto tr214;
		case 9: goto tr1249;
		case 10: goto tr214;
		case 13: goto tr214;
		case 32: goto tr1249;
		case 62: goto tr1051;
	}
	goto tr1247;
st1035:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1035;
case 1035:
	switch( (*( sm->p)) ) {
		case 77: goto st1036;
		case 88: goto st1037;
		case 109: goto st1036;
		case 120: goto st1037;
	}
	goto tr214;
st1036:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1036;
case 1036:
	if ( (*( sm->p)) == 62 )
		goto tr1081;
	goto tr214;
st1037:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1037;
case 1037:
	switch( (*( sm->p)) ) {
		case 80: goto st1038;
		case 112: goto st1038;
	}
	goto tr214;
st1038:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1038;
case 1038:
	switch( (*( sm->p)) ) {
		case 65: goto st1039;
		case 97: goto st1039;
	}
	goto tr214;
st1039:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1039;
case 1039:
	switch( (*( sm->p)) ) {
		case 78: goto st1040;
		case 110: goto st1040;
	}
	goto tr214;
st1040:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1040;
case 1040:
	switch( (*( sm->p)) ) {
		case 68: goto st1041;
		case 100: goto st1041;
	}
	goto tr214;
st1041:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1041;
case 1041:
	switch( (*( sm->p)) ) {
		case 9: goto st1042;
		case 32: goto st1042;
		case 61: goto st1044;
		case 62: goto tr1061;
	}
	goto tr214;
tr1260:
#line 71 "ext/dtext/dtext.cpp.rl"
	{ sm->a1 = sm->p; }
	goto st1042;
st1042:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1042;
case 1042:
#line 23740 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 0: goto tr214;
		case 9: goto tr1260;
		case 10: goto tr214;
		case 13: goto tr214;
		case 32: goto tr1260;
		case 61: goto tr1261;
		case 62: goto tr1065;
	}
	goto tr1259;
tr1259:
#line 71 "ext/dtext/dtext.cpp.rl"
	{ sm->a1 = sm->p; }
	goto st1043;
st1043:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1043;
case 1043:
#line 23759 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 0: goto tr214;
		case 10: goto tr214;
		case 13: goto tr214;
		case 62: goto tr1067;
	}
	goto st1043;
tr1261:
#line 71 "ext/dtext/dtext.cpp.rl"
	{ sm->a1 = sm->p; }
	goto st1044;
st1044:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1044;
case 1044:
#line 23775 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 0: goto tr214;
		case 9: goto tr1261;
		case 10: goto tr214;
		case 13: goto tr214;
		case 32: goto tr1261;
		case 62: goto tr1065;
	}
	goto tr1259;
tr2133:
#line 71 "ext/dtext/dtext.cpp.rl"
	{ sm->a1 = sm->p; }
	goto st1045;
st1045:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1045;
case 1045:
#line 23793 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 84: goto st1046;
		case 116: goto st1046;
	}
	goto tr214;
st1046:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1046;
case 1046:
	switch( (*( sm->p)) ) {
		case 84: goto st1047;
		case 116: goto st1047;
	}
	goto tr214;
st1047:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1047;
case 1047:
	switch( (*( sm->p)) ) {
		case 80: goto st1048;
		case 112: goto st1048;
	}
	goto tr214;
st1048:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1048;
case 1048:
	switch( (*( sm->p)) ) {
		case 58: goto st1049;
		case 83: goto st1053;
		case 115: goto st1053;
	}
	goto tr214;
st1049:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1049;
case 1049:
	if ( (*( sm->p)) == 47 )
		goto st1050;
	goto tr214;
st1050:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1050;
case 1050:
	if ( (*( sm->p)) == 47 )
		goto st1051;
	goto tr214;
st1051:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1051;
case 1051:
	switch( (*( sm->p)) ) {
		case 0: goto tr214;
		case 32: goto tr214;
	}
	if ( 9 <= (*( sm->p)) && (*( sm->p)) <= 13 )
		goto tr214;
	goto st1052;
st1052:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1052;
case 1052:
	switch( (*( sm->p)) ) {
		case 0: goto tr214;
		case 32: goto tr214;
		case 62: goto tr1271;
	}
	if ( 9 <= (*( sm->p)) && (*( sm->p)) <= 13 )
		goto tr214;
	goto st1052;
st1053:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1053;
case 1053:
	if ( (*( sm->p)) == 58 )
		goto st1049;
	goto tr214;
st1054:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1054;
case 1054:
	switch( (*( sm->p)) ) {
		case 79: goto st1055;
		case 111: goto st1055;
	}
	goto tr214;
st1055:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1055;
case 1055:
	switch( (*( sm->p)) ) {
		case 68: goto st1056;
		case 100: goto st1056;
	}
	goto tr214;
st1056:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1056;
case 1056:
	switch( (*( sm->p)) ) {
		case 84: goto st1057;
		case 116: goto st1057;
	}
	goto tr214;
st1057:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1057;
case 1057:
	switch( (*( sm->p)) ) {
		case 69: goto st1058;
		case 101: goto st1058;
	}
	goto tr214;
st1058:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1058;
case 1058:
	switch( (*( sm->p)) ) {
		case 88: goto st1059;
		case 120: goto st1059;
	}
	goto tr214;
st1059:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1059;
case 1059:
	switch( (*( sm->p)) ) {
		case 84: goto st1060;
		case 116: goto st1060;
	}
	goto tr214;
st1060:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1060;
case 1060:
	if ( (*( sm->p)) == 62 )
		goto tr1088;
	goto tr214;
st1061:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1061;
case 1061:
	switch( (*( sm->p)) ) {
		case 62: goto tr1098;
		case 80: goto st1062;
		case 84: goto st1069;
		case 112: goto st1062;
		case 116: goto st1069;
	}
	goto tr214;
st1062:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1062;
case 1062:
	switch( (*( sm->p)) ) {
		case 79: goto st1063;
		case 111: goto st1063;
	}
	goto tr214;
st1063:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1063;
case 1063:
	switch( (*( sm->p)) ) {
		case 73: goto st1064;
		case 105: goto st1064;
	}
	goto tr214;
st1064:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1064;
case 1064:
	switch( (*( sm->p)) ) {
		case 76: goto st1065;
		case 108: goto st1065;
	}
	goto tr214;
st1065:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1065;
case 1065:
	switch( (*( sm->p)) ) {
		case 69: goto st1066;
		case 101: goto st1066;
	}
	goto tr214;
st1066:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1066;
case 1066:
	switch( (*( sm->p)) ) {
		case 82: goto st1067;
		case 114: goto st1067;
	}
	goto tr214;
st1067:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1067;
case 1067:
	switch( (*( sm->p)) ) {
		case 62: goto tr1105;
		case 83: goto st1068;
		case 115: goto st1068;
	}
	goto tr214;
st1068:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1068;
case 1068:
	if ( (*( sm->p)) == 62 )
		goto tr1105;
	goto tr214;
st1069:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1069;
case 1069:
	switch( (*( sm->p)) ) {
		case 82: goto st1070;
		case 114: goto st1070;
	}
	goto tr214;
st1070:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1070;
case 1070:
	switch( (*( sm->p)) ) {
		case 79: goto st1071;
		case 111: goto st1071;
	}
	goto tr214;
st1071:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1071;
case 1071:
	switch( (*( sm->p)) ) {
		case 78: goto st1072;
		case 110: goto st1072;
	}
	goto tr214;
st1072:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1072;
case 1072:
	switch( (*( sm->p)) ) {
		case 71: goto st1073;
		case 103: goto st1073;
	}
	goto tr214;
st1073:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1073;
case 1073:
	if ( (*( sm->p)) == 62 )
		goto tr1019;
	goto tr214;
st1074:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1074;
case 1074:
	switch( (*( sm->p)) ) {
		case 78: goto st1075;
		case 110: goto st1075;
	}
	goto tr214;
st1075:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1075;
case 1075:
	if ( (*( sm->p)) == 62 )
		goto tr1107;
	goto tr214;
st1076:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1076;
case 1076:
	if ( (*( sm->p)) == 62 )
		goto tr1109;
	goto tr214;
tr1793:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 587 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 81;}
	goto st1716;
st1716:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1716;
case 1716:
#line 24082 "ext/dtext/dtext.cpp"
	_widec = (*( sm->p));
	if ( 64 <= (*( sm->p)) && (*( sm->p)) <= 64 ) {
		_widec = (short)(640 + ((*( sm->p)) - -128));
		if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
	}
	switch( _widec ) {
		case 47: goto st972;
		case 65: goto st983;
		case 66: goto st1006;
		case 67: goto st1017;
		case 69: goto st1035;
		case 72: goto tr2133;
		case 73: goto st1036;
		case 78: goto st1054;
		case 81: goto st1011;
		case 83: goto st1061;
		case 84: goto st1074;
		case 85: goto st1076;
		case 97: goto st983;
		case 98: goto st1006;
		case 99: goto st1017;
		case 101: goto st1035;
		case 104: goto tr2133;
		case 105: goto st1036;
		case 110: goto st1054;
		case 113: goto st1011;
		case 115: goto st1061;
		case 116: goto st1074;
		case 117: goto st1076;
		case 1088: goto st1077;
	}
	goto tr1795;
st1077:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1077;
case 1077:
	_widec = (*( sm->p));
	if ( (*( sm->p)) < 1 ) {
		if ( (*( sm->p)) <= -1 ) {
			_widec = (short)(640 + ((*( sm->p)) - -128));
			if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
		}
	} else if ( (*( sm->p)) > 8 ) {
		if ( (*( sm->p)) > 31 ) {
			if ( 33 <= (*( sm->p)) )
 {				_widec = (short)(640 + ((*( sm->p)) - -128));
				if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
			}
		} else if ( (*( sm->p)) >= 14 ) {
			_widec = (short)(640 + ((*( sm->p)) - -128));
			if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
		}
	} else {
		_widec = (short)(640 + ((*( sm->p)) - -128));
		if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
	}
	if ( _widec < 1025 ) {
		if ( 896 <= _widec && _widec <= 1023 )
			goto tr1291;
	} else if ( _widec > 1032 ) {
		if ( _widec > 1055 ) {
			if ( 1057 <= _widec && _widec <= 1151 )
				goto tr1291;
		} else if ( _widec >= 1038 )
			goto tr1291;
	} else
		goto tr1291;
	goto tr214;
tr1291:
#line 71 "ext/dtext/dtext.cpp.rl"
	{ sm->a1 = sm->p; }
	goto st1078;
st1078:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1078;
case 1078:
#line 24169 "ext/dtext/dtext.cpp"
	_widec = (*( sm->p));
	if ( (*( sm->p)) < 11 ) {
		if ( (*( sm->p)) > -1 ) {
			if ( 1 <= (*( sm->p)) && (*( sm->p)) <= 9 ) {
				_widec = (short)(640 + ((*( sm->p)) - -128));
				if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
			}
		} else {
			_widec = (short)(640 + ((*( sm->p)) - -128));
			if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
		}
	} else if ( (*( sm->p)) > 12 ) {
		if ( (*( sm->p)) < 62 ) {
			if ( 14 <= (*( sm->p)) && (*( sm->p)) <= 61 ) {
				_widec = (short)(640 + ((*( sm->p)) - -128));
				if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
			}
		} else if ( (*( sm->p)) > 62 ) {
			if ( 63 <= (*( sm->p)) )
 {				_widec = (short)(640 + ((*( sm->p)) - -128));
				if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
			}
		} else {
			_widec = (short)(640 + ((*( sm->p)) - -128));
			if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
		}
	} else {
		_widec = (short)(640 + ((*( sm->p)) - -128));
		if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
	}
	if ( _widec == 1086 )
		goto tr1293;
	if ( _widec < 1025 ) {
		if ( 896 <= _widec && _widec <= 1023 )
			goto st1078;
	} else if ( _widec > 1033 ) {
		if ( _widec > 1036 ) {
			if ( 1038 <= _widec && _widec <= 1151 )
				goto st1078;
		} else if ( _widec >= 1035 )
			goto st1078;
	} else
		goto st1078;
	goto tr214;
tr1794:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 587 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 81;}
	goto st1717;
st1717:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1717;
case 1717:
#line 24236 "ext/dtext/dtext.cpp"
	_widec = (*( sm->p));
	if ( (*( sm->p)) < 1 ) {
		if ( (*( sm->p)) < -29 ) {
			if ( (*( sm->p)) < -32 ) {
				if ( -62 <= (*( sm->p)) && (*( sm->p)) <= -33 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( sm->p)) > -31 ) {
				if ( -30 <= (*( sm->p)) && (*( sm->p)) <= -30 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( sm->p)) - -128));
				if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
			}
		} else if ( (*( sm->p)) > -29 ) {
			if ( (*( sm->p)) < -17 ) {
				if ( -28 <= (*( sm->p)) && (*( sm->p)) <= -18 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( sm->p)) > -17 ) {
				if ( -16 <= (*( sm->p)) && (*( sm->p)) <= -12 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( sm->p)) - -128));
				if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
			}
		} else {
			_widec = (short)(640 + ((*( sm->p)) - -128));
			if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
		}
	} else if ( (*( sm->p)) > 8 ) {
		if ( (*( sm->p)) < 65 ) {
			if ( (*( sm->p)) < 46 ) {
				if ( 14 <= (*( sm->p)) && (*( sm->p)) <= 31 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( sm->p)) > 46 ) {
				if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( sm->p)) - -128));
				if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
			}
		} else if ( (*( sm->p)) > 90 ) {
			if ( (*( sm->p)) < 97 ) {
				if ( 95 <= (*( sm->p)) && (*( sm->p)) <= 95 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( sm->p)) > 122 ) {
				if ( 127 <= (*( sm->p)) )
 {					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( sm->p)) - -128));
				if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
			}
		} else {
			_widec = (short)(640 + ((*( sm->p)) - -128));
			if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
		}
	} else {
		_widec = (short)(640 + ((*( sm->p)) - -128));
		if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
	}
	switch( _widec ) {
		case 994: goto tr2141;
		case 995: goto tr2142;
		case 1007: goto tr2143;
		case 1070: goto tr2146;
		case 1119: goto tr2146;
		case 1151: goto tr2145;
	}
	if ( _widec < 1025 ) {
		if ( _widec < 992 ) {
			if ( 962 <= _widec && _widec <= 991 )
				goto tr2139;
		} else if ( _widec > 1006 ) {
			if ( 1008 <= _widec && _widec <= 1012 )
				goto tr2144;
		} else
			goto tr2140;
	} else if ( _widec > 1032 ) {
		if ( _widec < 1072 ) {
			if ( 1038 <= _widec && _widec <= 1055 )
				goto tr2145;
		} else if ( _widec > 1081 ) {
			if ( _widec > 1114 ) {
				if ( 1121 <= _widec && _widec <= 1146 )
					goto tr2145;
			} else if ( _widec >= 1089 )
				goto tr2145;
		} else
			goto tr2145;
	} else
		goto tr2145;
	goto tr1795;
tr2139:
#line 71 "ext/dtext/dtext.cpp.rl"
	{ sm->a1 = sm->p; }
	goto st1079;
st1079:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1079;
case 1079:
#line 24382 "ext/dtext/dtext.cpp"
	_widec = (*( sm->p));
	if ( (*( sm->p)) <= -65 ) {
		_widec = (short)(640 + ((*( sm->p)) - -128));
		if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
	}
	if ( 896 <= _widec && _widec <= 959 )
		goto st1080;
	goto tr214;
tr2145:
#line 71 "ext/dtext/dtext.cpp.rl"
	{ sm->a1 = sm->p; }
	goto st1080;
st1080:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1080;
case 1080:
#line 24401 "ext/dtext/dtext.cpp"
	_widec = (*( sm->p));
	if ( (*( sm->p)) < 1 ) {
		if ( (*( sm->p)) < -29 ) {
			if ( (*( sm->p)) < -62 ) {
				if ( (*( sm->p)) <= -63 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( sm->p)) > -33 ) {
				if ( (*( sm->p)) > -31 ) {
					if ( -30 <= (*( sm->p)) && (*( sm->p)) <= -30 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) >= -32 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( sm->p)) - -128));
				if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
			}
		} else if ( (*( sm->p)) > -29 ) {
			if ( (*( sm->p)) < -17 ) {
				if ( -28 <= (*( sm->p)) && (*( sm->p)) <= -18 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( sm->p)) > -17 ) {
				if ( (*( sm->p)) > -12 ) {
					if ( -11 <= (*( sm->p)) && (*( sm->p)) <= -1 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) >= -16 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( sm->p)) - -128));
				if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
			}
		} else {
			_widec = (short)(640 + ((*( sm->p)) - -128));
			if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
		}
	} else if ( (*( sm->p)) > 8 ) {
		if ( (*( sm->p)) < 45 ) {
			if ( (*( sm->p)) < 33 ) {
				if ( 14 <= (*( sm->p)) && (*( sm->p)) <= 31 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( sm->p)) > 33 ) {
				if ( (*( sm->p)) > 39 ) {
					if ( 43 <= (*( sm->p)) && (*( sm->p)) <= 43 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) >= 39 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( sm->p)) - -128));
				if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
			}
		} else if ( (*( sm->p)) > 47 ) {
			if ( (*( sm->p)) < 95 ) {
				if ( (*( sm->p)) > 57 ) {
					if ( 65 <= (*( sm->p)) && (*( sm->p)) <= 90 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) >= 48 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( sm->p)) > 95 ) {
				if ( (*( sm->p)) > 122 ) {
					if ( 127 <= (*( sm->p)) )
 {						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) >= 97 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( sm->p)) - -128));
				if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
			}
		} else {
			_widec = (short)(640 + ((*( sm->p)) - -128));
			if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
		}
	} else {
		_widec = (short)(640 + ((*( sm->p)) - -128));
		if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
	}
	switch( _widec ) {
		case 994: goto st1094;
		case 995: goto st1096;
		case 1007: goto st1098;
		case 1057: goto st1080;
		case 1063: goto st1102;
		case 1067: goto st1080;
		case 1119: goto st1080;
		case 1151: goto tr1302;
	}
	if ( _widec < 1025 ) {
		if ( _widec < 992 ) {
			if ( _widec > 961 ) {
				if ( 962 <= _widec && _widec <= 991 )
					goto st1092;
			} else if ( _widec >= 896 )
				goto st1081;
		} else if ( _widec > 1006 ) {
			if ( _widec > 1012 ) {
				if ( 1013 <= _widec && _widec <= 1023 )
					goto st1081;
			} else if ( _widec >= 1008 )
				goto st1101;
		} else
			goto st1093;
	} else if ( _widec > 1032 ) {
		if ( _widec < 1072 ) {
			if ( _widec > 1055 ) {
				if ( 1069 <= _widec && _widec <= 1071 )
					goto st1080;
			} else if ( _widec >= 1038 )
				goto tr1302;
		} else if ( _widec > 1081 ) {
			if ( _widec > 1114 ) {
				if ( 1121 <= _widec && _widec <= 1146 )
					goto tr1302;
			} else if ( _widec >= 1089 )
				goto tr1302;
		} else
			goto tr1302;
	} else
		goto tr1302;
	goto tr207;
st1081:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1081;
case 1081:
	_widec = (*( sm->p));
	if ( (*( sm->p)) < 1 ) {
		if ( (*( sm->p)) < -29 ) {
			if ( (*( sm->p)) < -62 ) {
				if ( (*( sm->p)) <= -63 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( sm->p)) > -33 ) {
				if ( (*( sm->p)) > -31 ) {
					if ( -30 <= (*( sm->p)) && (*( sm->p)) <= -30 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) >= -32 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( sm->p)) - -128));
				if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
			}
		} else if ( (*( sm->p)) > -29 ) {
			if ( (*( sm->p)) < -17 ) {
				if ( -28 <= (*( sm->p)) && (*( sm->p)) <= -18 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( sm->p)) > -17 ) {
				if ( (*( sm->p)) > -12 ) {
					if ( -11 <= (*( sm->p)) && (*( sm->p)) <= -1 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) >= -16 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( sm->p)) - -128));
				if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
			}
		} else {
			_widec = (short)(640 + ((*( sm->p)) - -128));
			if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
		}
	} else if ( (*( sm->p)) > 8 ) {
		if ( (*( sm->p)) < 45 ) {
			if ( (*( sm->p)) < 33 ) {
				if ( 14 <= (*( sm->p)) && (*( sm->p)) <= 31 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( sm->p)) > 33 ) {
				if ( (*( sm->p)) > 39 ) {
					if ( 43 <= (*( sm->p)) && (*( sm->p)) <= 43 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) >= 39 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( sm->p)) - -128));
				if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
			}
		} else if ( (*( sm->p)) > 47 ) {
			if ( (*( sm->p)) < 95 ) {
				if ( (*( sm->p)) > 57 ) {
					if ( 65 <= (*( sm->p)) && (*( sm->p)) <= 90 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) >= 48 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( sm->p)) > 95 ) {
				if ( (*( sm->p)) > 122 ) {
					if ( 127 <= (*( sm->p)) )
 {						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) >= 97 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( sm->p)) - -128));
				if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
			}
		} else {
			_widec = (short)(640 + ((*( sm->p)) - -128));
			if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
		}
	} else {
		_widec = (short)(640 + ((*( sm->p)) - -128));
		if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
	}
	switch( _widec ) {
		case 994: goto st1084;
		case 995: goto st1086;
		case 1007: goto st1088;
		case 1057: goto st1081;
		case 1063: goto st1081;
		case 1067: goto st1081;
		case 1119: goto st1081;
		case 1151: goto tr1310;
	}
	if ( _widec < 1025 ) {
		if ( _widec < 992 ) {
			if ( _widec > 961 ) {
				if ( 962 <= _widec && _widec <= 991 )
					goto st1082;
			} else if ( _widec >= 896 )
				goto st1081;
		} else if ( _widec > 1006 ) {
			if ( _widec > 1012 ) {
				if ( 1013 <= _widec && _widec <= 1023 )
					goto st1081;
			} else if ( _widec >= 1008 )
				goto st1090;
		} else
			goto st1083;
	} else if ( _widec > 1032 ) {
		if ( _widec < 1072 ) {
			if ( _widec > 1055 ) {
				if ( 1069 <= _widec && _widec <= 1071 )
					goto st1081;
			} else if ( _widec >= 1038 )
				goto tr1310;
		} else if ( _widec > 1081 ) {
			if ( _widec > 1114 ) {
				if ( 1121 <= _widec && _widec <= 1146 )
					goto tr1310;
			} else if ( _widec >= 1089 )
				goto tr1310;
		} else
			goto tr1310;
	} else
		goto tr1310;
	goto tr207;
st1082:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1082;
case 1082:
	_widec = (*( sm->p));
	if ( (*( sm->p)) < 1 ) {
		if ( (*( sm->p)) < -30 ) {
			if ( (*( sm->p)) < -64 ) {
				if ( (*( sm->p)) <= -65 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( sm->p)) > -63 ) {
				if ( (*( sm->p)) > -33 ) {
					if ( -32 <= (*( sm->p)) && (*( sm->p)) <= -31 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) >= -62 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( sm->p)) - -128));
				if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
			}
		} else if ( (*( sm->p)) > -30 ) {
			if ( (*( sm->p)) < -17 ) {
				if ( (*( sm->p)) > -29 ) {
					if ( -28 <= (*( sm->p)) && (*( sm->p)) <= -18 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) >= -29 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( sm->p)) > -17 ) {
				if ( (*( sm->p)) > -12 ) {
					if ( -11 <= (*( sm->p)) && (*( sm->p)) <= -1 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) >= -16 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( sm->p)) - -128));
				if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
			}
		} else {
			_widec = (short)(640 + ((*( sm->p)) - -128));
			if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
		}
	} else if ( (*( sm->p)) > 8 ) {
		if ( (*( sm->p)) < 45 ) {
			if ( (*( sm->p)) < 33 ) {
				if ( 14 <= (*( sm->p)) && (*( sm->p)) <= 31 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( sm->p)) > 33 ) {
				if ( (*( sm->p)) > 39 ) {
					if ( 43 <= (*( sm->p)) && (*( sm->p)) <= 43 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) >= 39 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( sm->p)) - -128));
				if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
			}
		} else if ( (*( sm->p)) > 47 ) {
			if ( (*( sm->p)) < 95 ) {
				if ( (*( sm->p)) > 57 ) {
					if ( 65 <= (*( sm->p)) && (*( sm->p)) <= 90 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) >= 48 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( sm->p)) > 95 ) {
				if ( (*( sm->p)) > 122 ) {
					if ( 127 <= (*( sm->p)) )
 {						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) >= 97 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( sm->p)) - -128));
				if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
			}
		} else {
			_widec = (short)(640 + ((*( sm->p)) - -128));
			if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
		}
	} else {
		_widec = (short)(640 + ((*( sm->p)) - -128));
		if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
	}
	switch( _widec ) {
		case 994: goto st1084;
		case 995: goto st1086;
		case 1007: goto st1088;
		case 1057: goto st1081;
		case 1063: goto st1081;
		case 1067: goto st1081;
		case 1119: goto st1081;
		case 1151: goto tr1310;
	}
	if ( _widec < 1013 ) {
		if ( _widec < 962 ) {
			if ( _widec > 959 ) {
				if ( 960 <= _widec && _widec <= 961 )
					goto st1081;
			} else if ( _widec >= 896 )
				goto tr1310;
		} else if ( _widec > 991 ) {
			if ( _widec > 1006 ) {
				if ( 1008 <= _widec && _widec <= 1012 )
					goto st1090;
			} else if ( _widec >= 992 )
				goto st1083;
		} else
			goto st1082;
	} else if ( _widec > 1023 ) {
		if ( _widec < 1069 ) {
			if ( _widec > 1032 ) {
				if ( 1038 <= _widec && _widec <= 1055 )
					goto tr1310;
			} else if ( _widec >= 1025 )
				goto tr1310;
		} else if ( _widec > 1071 ) {
			if ( _widec < 1089 ) {
				if ( 1072 <= _widec && _widec <= 1081 )
					goto tr1310;
			} else if ( _widec > 1114 ) {
				if ( 1121 <= _widec && _widec <= 1146 )
					goto tr1310;
			} else
				goto tr1310;
		} else
			goto st1081;
	} else
		goto st1081;
	goto tr207;
tr1310:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 72 "ext/dtext/dtext.cpp.rl"
	{ sm->a2 = sm->p; }
#line 354 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 40;}
	goto st1718;
st1718:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1718;
case 1718:
#line 24981 "ext/dtext/dtext.cpp"
	_widec = (*( sm->p));
	if ( (*( sm->p)) < 1 ) {
		if ( (*( sm->p)) < -29 ) {
			if ( (*( sm->p)) < -62 ) {
				if ( (*( sm->p)) <= -63 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( sm->p)) > -33 ) {
				if ( (*( sm->p)) > -31 ) {
					if ( -30 <= (*( sm->p)) && (*( sm->p)) <= -30 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) >= -32 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( sm->p)) - -128));
				if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
			}
		} else if ( (*( sm->p)) > -29 ) {
			if ( (*( sm->p)) < -17 ) {
				if ( -28 <= (*( sm->p)) && (*( sm->p)) <= -18 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( sm->p)) > -17 ) {
				if ( (*( sm->p)) > -12 ) {
					if ( -11 <= (*( sm->p)) && (*( sm->p)) <= -1 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) >= -16 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( sm->p)) - -128));
				if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
			}
		} else {
			_widec = (short)(640 + ((*( sm->p)) - -128));
			if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
		}
	} else if ( (*( sm->p)) > 8 ) {
		if ( (*( sm->p)) < 45 ) {
			if ( (*( sm->p)) < 33 ) {
				if ( 14 <= (*( sm->p)) && (*( sm->p)) <= 31 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( sm->p)) > 33 ) {
				if ( (*( sm->p)) > 39 ) {
					if ( 43 <= (*( sm->p)) && (*( sm->p)) <= 43 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) >= 39 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( sm->p)) - -128));
				if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
			}
		} else if ( (*( sm->p)) > 47 ) {
			if ( (*( sm->p)) < 95 ) {
				if ( (*( sm->p)) > 57 ) {
					if ( 65 <= (*( sm->p)) && (*( sm->p)) <= 90 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) >= 48 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( sm->p)) > 95 ) {
				if ( (*( sm->p)) > 122 ) {
					if ( 127 <= (*( sm->p)) )
 {						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) >= 97 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( sm->p)) - -128));
				if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
			}
		} else {
			_widec = (short)(640 + ((*( sm->p)) - -128));
			if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
		}
	} else {
		_widec = (short)(640 + ((*( sm->p)) - -128));
		if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
	}
	switch( _widec ) {
		case 994: goto st1084;
		case 995: goto st1086;
		case 1007: goto st1088;
		case 1057: goto st1081;
		case 1063: goto st1081;
		case 1067: goto st1081;
		case 1119: goto st1081;
		case 1151: goto tr1310;
	}
	if ( _widec < 1025 ) {
		if ( _widec < 992 ) {
			if ( _widec > 961 ) {
				if ( 962 <= _widec && _widec <= 991 )
					goto st1082;
			} else if ( _widec >= 896 )
				goto st1081;
		} else if ( _widec > 1006 ) {
			if ( _widec > 1012 ) {
				if ( 1013 <= _widec && _widec <= 1023 )
					goto st1081;
			} else if ( _widec >= 1008 )
				goto st1090;
		} else
			goto st1083;
	} else if ( _widec > 1032 ) {
		if ( _widec < 1072 ) {
			if ( _widec > 1055 ) {
				if ( 1069 <= _widec && _widec <= 1071 )
					goto st1081;
			} else if ( _widec >= 1038 )
				goto tr1310;
		} else if ( _widec > 1081 ) {
			if ( _widec > 1114 ) {
				if ( 1121 <= _widec && _widec <= 1146 )
					goto tr1310;
			} else if ( _widec >= 1089 )
				goto tr1310;
		} else
			goto tr1310;
	} else
		goto tr1310;
	goto tr2147;
st1083:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1083;
case 1083:
	_widec = (*( sm->p));
	if ( (*( sm->p)) < 1 ) {
		if ( (*( sm->p)) < -30 ) {
			if ( (*( sm->p)) < -64 ) {
				if ( (*( sm->p)) <= -65 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( sm->p)) > -63 ) {
				if ( (*( sm->p)) > -33 ) {
					if ( -32 <= (*( sm->p)) && (*( sm->p)) <= -31 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) >= -62 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( sm->p)) - -128));
				if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
			}
		} else if ( (*( sm->p)) > -30 ) {
			if ( (*( sm->p)) < -17 ) {
				if ( (*( sm->p)) > -29 ) {
					if ( -28 <= (*( sm->p)) && (*( sm->p)) <= -18 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) >= -29 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( sm->p)) > -17 ) {
				if ( (*( sm->p)) > -12 ) {
					if ( -11 <= (*( sm->p)) && (*( sm->p)) <= -1 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) >= -16 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( sm->p)) - -128));
				if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
			}
		} else {
			_widec = (short)(640 + ((*( sm->p)) - -128));
			if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
		}
	} else if ( (*( sm->p)) > 8 ) {
		if ( (*( sm->p)) < 45 ) {
			if ( (*( sm->p)) < 33 ) {
				if ( 14 <= (*( sm->p)) && (*( sm->p)) <= 31 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( sm->p)) > 33 ) {
				if ( (*( sm->p)) > 39 ) {
					if ( 43 <= (*( sm->p)) && (*( sm->p)) <= 43 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) >= 39 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( sm->p)) - -128));
				if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
			}
		} else if ( (*( sm->p)) > 47 ) {
			if ( (*( sm->p)) < 95 ) {
				if ( (*( sm->p)) > 57 ) {
					if ( 65 <= (*( sm->p)) && (*( sm->p)) <= 90 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) >= 48 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( sm->p)) > 95 ) {
				if ( (*( sm->p)) > 122 ) {
					if ( 127 <= (*( sm->p)) )
 {						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) >= 97 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( sm->p)) - -128));
				if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
			}
		} else {
			_widec = (short)(640 + ((*( sm->p)) - -128));
			if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
		}
	} else {
		_widec = (short)(640 + ((*( sm->p)) - -128));
		if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
	}
	switch( _widec ) {
		case 994: goto st1084;
		case 995: goto st1086;
		case 1007: goto st1088;
		case 1057: goto st1081;
		case 1063: goto st1081;
		case 1067: goto st1081;
		case 1119: goto st1081;
		case 1151: goto tr1310;
	}
	if ( _widec < 1013 ) {
		if ( _widec < 962 ) {
			if ( _widec > 959 ) {
				if ( 960 <= _widec && _widec <= 961 )
					goto st1081;
			} else if ( _widec >= 896 )
				goto st1082;
		} else if ( _widec > 991 ) {
			if ( _widec > 1006 ) {
				if ( 1008 <= _widec && _widec <= 1012 )
					goto st1090;
			} else if ( _widec >= 992 )
				goto st1083;
		} else
			goto st1082;
	} else if ( _widec > 1023 ) {
		if ( _widec < 1069 ) {
			if ( _widec > 1032 ) {
				if ( 1038 <= _widec && _widec <= 1055 )
					goto tr1310;
			} else if ( _widec >= 1025 )
				goto tr1310;
		} else if ( _widec > 1071 ) {
			if ( _widec < 1089 ) {
				if ( 1072 <= _widec && _widec <= 1081 )
					goto tr1310;
			} else if ( _widec > 1114 ) {
				if ( 1121 <= _widec && _widec <= 1146 )
					goto tr1310;
			} else
				goto tr1310;
		} else
			goto st1081;
	} else
		goto st1081;
	goto tr207;
st1084:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1084;
case 1084:
	_widec = (*( sm->p));
	if ( (*( sm->p)) < -11 ) {
		if ( (*( sm->p)) < -32 ) {
			if ( (*( sm->p)) < -98 ) {
				if ( (*( sm->p)) > -100 ) {
					if ( -99 <= (*( sm->p)) && (*( sm->p)) <= -99 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( sm->p)) > -65 ) {
				if ( (*( sm->p)) > -63 ) {
					if ( -62 <= (*( sm->p)) && (*( sm->p)) <= -33 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) >= -64 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( sm->p)) - -128));
				if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
			}
		} else if ( (*( sm->p)) > -31 ) {
			if ( (*( sm->p)) < -28 ) {
				if ( (*( sm->p)) > -30 ) {
					if ( -29 <= (*( sm->p)) && (*( sm->p)) <= -29 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) >= -30 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( sm->p)) > -18 ) {
				if ( (*( sm->p)) > -17 ) {
					if ( -16 <= (*( sm->p)) && (*( sm->p)) <= -12 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) >= -17 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( sm->p)) - -128));
				if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
			}
		} else {
			_widec = (short)(640 + ((*( sm->p)) - -128));
			if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
		}
	} else if ( (*( sm->p)) > -1 ) {
		if ( (*( sm->p)) < 45 ) {
			if ( (*( sm->p)) < 33 ) {
				if ( (*( sm->p)) > 8 ) {
					if ( 14 <= (*( sm->p)) && (*( sm->p)) <= 31 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) >= 1 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( sm->p)) > 33 ) {
				if ( (*( sm->p)) > 39 ) {
					if ( 43 <= (*( sm->p)) && (*( sm->p)) <= 43 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) >= 39 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( sm->p)) - -128));
				if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
			}
		} else if ( (*( sm->p)) > 47 ) {
			if ( (*( sm->p)) < 95 ) {
				if ( (*( sm->p)) > 57 ) {
					if ( 65 <= (*( sm->p)) && (*( sm->p)) <= 90 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) >= 48 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( sm->p)) > 95 ) {
				if ( (*( sm->p)) > 122 ) {
					if ( 127 <= (*( sm->p)) )
 {						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) >= 97 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( sm->p)) - -128));
				if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
			}
		} else {
			_widec = (short)(640 + ((*( sm->p)) - -128));
			if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
		}
	} else {
		_widec = (short)(640 + ((*( sm->p)) - -128));
		if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
	}
	switch( _widec ) {
		case 925: goto st1085;
		case 994: goto st1084;
		case 995: goto st1086;
		case 1007: goto st1088;
		case 1057: goto st1081;
		case 1063: goto st1081;
		case 1067: goto st1081;
		case 1119: goto st1081;
		case 1151: goto tr1310;
	}
	if ( _widec < 1013 ) {
		if ( _widec < 962 ) {
			if ( _widec > 959 ) {
				if ( 960 <= _widec && _widec <= 961 )
					goto st1081;
			} else if ( _widec >= 896 )
				goto st1082;
		} else if ( _widec > 991 ) {
			if ( _widec > 1006 ) {
				if ( 1008 <= _widec && _widec <= 1012 )
					goto st1090;
			} else if ( _widec >= 992 )
				goto st1083;
		} else
			goto st1082;
	} else if ( _widec > 1023 ) {
		if ( _widec < 1069 ) {
			if ( _widec > 1032 ) {
				if ( 1038 <= _widec && _widec <= 1055 )
					goto tr1310;
			} else if ( _widec >= 1025 )
				goto tr1310;
		} else if ( _widec > 1071 ) {
			if ( _widec < 1089 ) {
				if ( 1072 <= _widec && _widec <= 1081 )
					goto tr1310;
			} else if ( _widec > 1114 ) {
				if ( 1121 <= _widec && _widec <= 1146 )
					goto tr1310;
			} else
				goto tr1310;
		} else
			goto st1081;
	} else
		goto st1081;
	goto tr207;
st1085:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1085;
case 1085:
	_widec = (*( sm->p));
	if ( (*( sm->p)) < -11 ) {
		if ( (*( sm->p)) < -32 ) {
			if ( (*( sm->p)) < -82 ) {
				if ( (*( sm->p)) > -84 ) {
					if ( -83 <= (*( sm->p)) && (*( sm->p)) <= -83 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( sm->p)) > -65 ) {
				if ( (*( sm->p)) > -63 ) {
					if ( -62 <= (*( sm->p)) && (*( sm->p)) <= -33 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) >= -64 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( sm->p)) - -128));
				if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
			}
		} else if ( (*( sm->p)) > -31 ) {
			if ( (*( sm->p)) < -28 ) {
				if ( (*( sm->p)) > -30 ) {
					if ( -29 <= (*( sm->p)) && (*( sm->p)) <= -29 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) >= -30 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( sm->p)) > -18 ) {
				if ( (*( sm->p)) > -17 ) {
					if ( -16 <= (*( sm->p)) && (*( sm->p)) <= -12 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) >= -17 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( sm->p)) - -128));
				if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
			}
		} else {
			_widec = (short)(640 + ((*( sm->p)) - -128));
			if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
		}
	} else if ( (*( sm->p)) > -1 ) {
		if ( (*( sm->p)) < 45 ) {
			if ( (*( sm->p)) < 33 ) {
				if ( (*( sm->p)) > 8 ) {
					if ( 14 <= (*( sm->p)) && (*( sm->p)) <= 31 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) >= 1 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( sm->p)) > 33 ) {
				if ( (*( sm->p)) > 39 ) {
					if ( 43 <= (*( sm->p)) && (*( sm->p)) <= 43 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) >= 39 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( sm->p)) - -128));
				if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
			}
		} else if ( (*( sm->p)) > 47 ) {
			if ( (*( sm->p)) < 95 ) {
				if ( (*( sm->p)) > 57 ) {
					if ( 65 <= (*( sm->p)) && (*( sm->p)) <= 90 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) >= 48 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( sm->p)) > 95 ) {
				if ( (*( sm->p)) > 122 ) {
					if ( 127 <= (*( sm->p)) )
 {						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) >= 97 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( sm->p)) - -128));
				if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
			}
		} else {
			_widec = (short)(640 + ((*( sm->p)) - -128));
			if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
		}
	} else {
		_widec = (short)(640 + ((*( sm->p)) - -128));
		if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
	}
	switch( _widec ) {
		case 941: goto st1081;
		case 994: goto st1084;
		case 995: goto st1086;
		case 1007: goto st1088;
		case 1057: goto st1081;
		case 1063: goto st1081;
		case 1067: goto st1081;
		case 1119: goto st1081;
		case 1151: goto tr1310;
	}
	if ( _widec < 1013 ) {
		if ( _widec < 962 ) {
			if ( _widec > 959 ) {
				if ( 960 <= _widec && _widec <= 961 )
					goto st1081;
			} else if ( _widec >= 896 )
				goto tr1310;
		} else if ( _widec > 991 ) {
			if ( _widec > 1006 ) {
				if ( 1008 <= _widec && _widec <= 1012 )
					goto st1090;
			} else if ( _widec >= 992 )
				goto st1083;
		} else
			goto st1082;
	} else if ( _widec > 1023 ) {
		if ( _widec < 1069 ) {
			if ( _widec > 1032 ) {
				if ( 1038 <= _widec && _widec <= 1055 )
					goto tr1310;
			} else if ( _widec >= 1025 )
				goto tr1310;
		} else if ( _widec > 1071 ) {
			if ( _widec < 1089 ) {
				if ( 1072 <= _widec && _widec <= 1081 )
					goto tr1310;
			} else if ( _widec > 1114 ) {
				if ( 1121 <= _widec && _widec <= 1146 )
					goto tr1310;
			} else
				goto tr1310;
		} else
			goto st1081;
	} else
		goto st1081;
	goto tr207;
st1086:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1086;
case 1086:
	_widec = (*( sm->p));
	if ( (*( sm->p)) < -11 ) {
		if ( (*( sm->p)) < -32 ) {
			if ( (*( sm->p)) < -127 ) {
				if ( (*( sm->p)) <= -128 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( sm->p)) > -65 ) {
				if ( (*( sm->p)) > -63 ) {
					if ( -62 <= (*( sm->p)) && (*( sm->p)) <= -33 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) >= -64 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( sm->p)) - -128));
				if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
			}
		} else if ( (*( sm->p)) > -31 ) {
			if ( (*( sm->p)) < -28 ) {
				if ( (*( sm->p)) > -30 ) {
					if ( -29 <= (*( sm->p)) && (*( sm->p)) <= -29 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) >= -30 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( sm->p)) > -18 ) {
				if ( (*( sm->p)) > -17 ) {
					if ( -16 <= (*( sm->p)) && (*( sm->p)) <= -12 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) >= -17 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( sm->p)) - -128));
				if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
			}
		} else {
			_widec = (short)(640 + ((*( sm->p)) - -128));
			if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
		}
	} else if ( (*( sm->p)) > -1 ) {
		if ( (*( sm->p)) < 45 ) {
			if ( (*( sm->p)) < 33 ) {
				if ( (*( sm->p)) > 8 ) {
					if ( 14 <= (*( sm->p)) && (*( sm->p)) <= 31 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) >= 1 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( sm->p)) > 33 ) {
				if ( (*( sm->p)) > 39 ) {
					if ( 43 <= (*( sm->p)) && (*( sm->p)) <= 43 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) >= 39 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( sm->p)) - -128));
				if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
			}
		} else if ( (*( sm->p)) > 47 ) {
			if ( (*( sm->p)) < 95 ) {
				if ( (*( sm->p)) > 57 ) {
					if ( 65 <= (*( sm->p)) && (*( sm->p)) <= 90 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) >= 48 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( sm->p)) > 95 ) {
				if ( (*( sm->p)) > 122 ) {
					if ( 127 <= (*( sm->p)) )
 {						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) >= 97 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( sm->p)) - -128));
				if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
			}
		} else {
			_widec = (short)(640 + ((*( sm->p)) - -128));
			if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
		}
	} else {
		_widec = (short)(640 + ((*( sm->p)) - -128));
		if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
	}
	switch( _widec ) {
		case 896: goto st1087;
		case 994: goto st1084;
		case 995: goto st1086;
		case 1007: goto st1088;
		case 1057: goto st1081;
		case 1063: goto st1081;
		case 1067: goto st1081;
		case 1119: goto st1081;
		case 1151: goto tr1310;
	}
	if ( _widec < 1013 ) {
		if ( _widec < 962 ) {
			if ( _widec > 959 ) {
				if ( 960 <= _widec && _widec <= 961 )
					goto st1081;
			} else if ( _widec >= 897 )
				goto st1082;
		} else if ( _widec > 991 ) {
			if ( _widec > 1006 ) {
				if ( 1008 <= _widec && _widec <= 1012 )
					goto st1090;
			} else if ( _widec >= 992 )
				goto st1083;
		} else
			goto st1082;
	} else if ( _widec > 1023 ) {
		if ( _widec < 1069 ) {
			if ( _widec > 1032 ) {
				if ( 1038 <= _widec && _widec <= 1055 )
					goto tr1310;
			} else if ( _widec >= 1025 )
				goto tr1310;
		} else if ( _widec > 1071 ) {
			if ( _widec < 1089 ) {
				if ( 1072 <= _widec && _widec <= 1081 )
					goto tr1310;
			} else if ( _widec > 1114 ) {
				if ( 1121 <= _widec && _widec <= 1146 )
					goto tr1310;
			} else
				goto tr1310;
		} else
			goto st1081;
	} else
		goto st1081;
	goto tr207;
st1087:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1087;
case 1087:
	_widec = (*( sm->p));
	if ( (*( sm->p)) < -17 ) {
		if ( (*( sm->p)) < -99 ) {
			if ( (*( sm->p)) < -120 ) {
				if ( (*( sm->p)) > -126 ) {
					if ( -125 <= (*( sm->p)) && (*( sm->p)) <= -121 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( sm->p)) > -111 ) {
				if ( (*( sm->p)) > -109 ) {
					if ( -108 <= (*( sm->p)) && (*( sm->p)) <= -100 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) >= -110 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( sm->p)) - -128));
				if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
			}
		} else if ( (*( sm->p)) > -65 ) {
			if ( (*( sm->p)) < -32 ) {
				if ( (*( sm->p)) > -63 ) {
					if ( -62 <= (*( sm->p)) && (*( sm->p)) <= -33 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) >= -64 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( sm->p)) > -31 ) {
				if ( (*( sm->p)) < -29 ) {
					if ( -30 <= (*( sm->p)) && (*( sm->p)) <= -30 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) > -29 ) {
					if ( -28 <= (*( sm->p)) && (*( sm->p)) <= -18 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( sm->p)) - -128));
				if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
			}
		} else {
			_widec = (short)(640 + ((*( sm->p)) - -128));
			if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
		}
	} else if ( (*( sm->p)) > -17 ) {
		if ( (*( sm->p)) < 43 ) {
			if ( (*( sm->p)) < 1 ) {
				if ( (*( sm->p)) > -12 ) {
					if ( -11 <= (*( sm->p)) && (*( sm->p)) <= -1 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) >= -16 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( sm->p)) > 8 ) {
				if ( (*( sm->p)) < 33 ) {
					if ( 14 <= (*( sm->p)) && (*( sm->p)) <= 31 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) > 33 ) {
					if ( 39 <= (*( sm->p)) && (*( sm->p)) <= 39 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( sm->p)) - -128));
				if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
			}
		} else if ( (*( sm->p)) > 43 ) {
			if ( (*( sm->p)) < 65 ) {
				if ( (*( sm->p)) > 47 ) {
					if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) >= 45 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( sm->p)) > 90 ) {
				if ( (*( sm->p)) < 97 ) {
					if ( 95 <= (*( sm->p)) && (*( sm->p)) <= 95 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) > 122 ) {
					if ( 127 <= (*( sm->p)) )
 {						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( sm->p)) - -128));
				if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
			}
		} else {
			_widec = (short)(640 + ((*( sm->p)) - -128));
			if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
		}
	} else {
		_widec = (short)(640 + ((*( sm->p)) - -128));
		if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
	}
	switch( _widec ) {
		case 994: goto st1084;
		case 995: goto st1086;
		case 1007: goto st1088;
		case 1057: goto st1081;
		case 1063: goto st1081;
		case 1067: goto st1081;
		case 1119: goto st1081;
		case 1151: goto tr1310;
	}
	if ( _widec < 992 ) {
		if ( _widec < 914 ) {
			if ( _widec < 899 ) {
				if ( 896 <= _widec && _widec <= 898 )
					goto st1081;
			} else if ( _widec > 903 ) {
				if ( 904 <= _widec && _widec <= 913 )
					goto st1081;
			} else
				goto tr1310;
		} else if ( _widec > 915 ) {
			if ( _widec < 925 ) {
				if ( 916 <= _widec && _widec <= 924 )
					goto st1081;
			} else if ( _widec > 959 ) {
				if ( _widec > 961 ) {
					if ( 962 <= _widec && _widec <= 991 )
						goto st1082;
				} else if ( _widec >= 960 )
					goto st1081;
			} else
				goto tr1310;
		} else
			goto tr1310;
	} else if ( _widec > 1006 ) {
		if ( _widec < 1038 ) {
			if ( _widec < 1013 ) {
				if ( 1008 <= _widec && _widec <= 1012 )
					goto st1090;
			} else if ( _widec > 1023 ) {
				if ( 1025 <= _widec && _widec <= 1032 )
					goto tr1310;
			} else
				goto st1081;
		} else if ( _widec > 1055 ) {
			if ( _widec < 1072 ) {
				if ( 1069 <= _widec && _widec <= 1071 )
					goto st1081;
			} else if ( _widec > 1081 ) {
				if ( _widec > 1114 ) {
					if ( 1121 <= _widec && _widec <= 1146 )
						goto tr1310;
				} else if ( _widec >= 1089 )
					goto tr1310;
			} else
				goto tr1310;
		} else
			goto tr1310;
	} else
		goto st1083;
	goto tr207;
st1088:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1088;
case 1088:
	_widec = (*( sm->p));
	if ( (*( sm->p)) < -16 ) {
		if ( (*( sm->p)) < -62 ) {
			if ( (*( sm->p)) < -67 ) {
				if ( (*( sm->p)) > -69 ) {
					if ( -68 <= (*( sm->p)) && (*( sm->p)) <= -68 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( sm->p)) > -67 ) {
				if ( (*( sm->p)) > -65 ) {
					if ( -64 <= (*( sm->p)) && (*( sm->p)) <= -63 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) >= -66 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( sm->p)) - -128));
				if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
			}
		} else if ( (*( sm->p)) > -33 ) {
			if ( (*( sm->p)) < -29 ) {
				if ( (*( sm->p)) > -31 ) {
					if ( -30 <= (*( sm->p)) && (*( sm->p)) <= -30 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) >= -32 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( sm->p)) > -29 ) {
				if ( (*( sm->p)) > -18 ) {
					if ( -17 <= (*( sm->p)) && (*( sm->p)) <= -17 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) >= -28 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( sm->p)) - -128));
				if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
			}
		} else {
			_widec = (short)(640 + ((*( sm->p)) - -128));
			if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
		}
	} else if ( (*( sm->p)) > -12 ) {
		if ( (*( sm->p)) < 43 ) {
			if ( (*( sm->p)) < 14 ) {
				if ( (*( sm->p)) > -1 ) {
					if ( 1 <= (*( sm->p)) && (*( sm->p)) <= 8 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) >= -11 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( sm->p)) > 31 ) {
				if ( (*( sm->p)) > 33 ) {
					if ( 39 <= (*( sm->p)) && (*( sm->p)) <= 39 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) >= 33 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( sm->p)) - -128));
				if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
			}
		} else if ( (*( sm->p)) > 43 ) {
			if ( (*( sm->p)) < 65 ) {
				if ( (*( sm->p)) > 47 ) {
					if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) >= 45 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( sm->p)) > 90 ) {
				if ( (*( sm->p)) < 97 ) {
					if ( 95 <= (*( sm->p)) && (*( sm->p)) <= 95 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) > 122 ) {
					if ( 127 <= (*( sm->p)) )
 {						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( sm->p)) - -128));
				if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
			}
		} else {
			_widec = (short)(640 + ((*( sm->p)) - -128));
			if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
		}
	} else {
		_widec = (short)(640 + ((*( sm->p)) - -128));
		if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
	}
	switch( _widec ) {
		case 956: goto st1089;
		case 957: goto st1091;
		case 994: goto st1084;
		case 995: goto st1086;
		case 1007: goto st1088;
		case 1057: goto st1081;
		case 1063: goto st1081;
		case 1067: goto st1081;
		case 1119: goto st1081;
		case 1151: goto tr1310;
	}
	if ( _widec < 1013 ) {
		if ( _widec < 962 ) {
			if ( _widec > 959 ) {
				if ( 960 <= _widec && _widec <= 961 )
					goto st1081;
			} else if ( _widec >= 896 )
				goto st1082;
		} else if ( _widec > 991 ) {
			if ( _widec > 1006 ) {
				if ( 1008 <= _widec && _widec <= 1012 )
					goto st1090;
			} else if ( _widec >= 992 )
				goto st1083;
		} else
			goto st1082;
	} else if ( _widec > 1023 ) {
		if ( _widec < 1069 ) {
			if ( _widec > 1032 ) {
				if ( 1038 <= _widec && _widec <= 1055 )
					goto tr1310;
			} else if ( _widec >= 1025 )
				goto tr1310;
		} else if ( _widec > 1071 ) {
			if ( _widec < 1089 ) {
				if ( 1072 <= _widec && _widec <= 1081 )
					goto tr1310;
			} else if ( _widec > 1114 ) {
				if ( 1121 <= _widec && _widec <= 1146 )
					goto tr1310;
			} else
				goto tr1310;
		} else
			goto st1081;
	} else
		goto st1081;
	goto tr207;
st1089:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1089;
case 1089:
	_widec = (*( sm->p));
	if ( (*( sm->p)) < -16 ) {
		if ( (*( sm->p)) < -64 ) {
			if ( (*( sm->p)) < -118 ) {
				if ( (*( sm->p)) > -120 ) {
					if ( -119 <= (*( sm->p)) && (*( sm->p)) <= -119 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( sm->p)) > -68 ) {
				if ( (*( sm->p)) > -67 ) {
					if ( -66 <= (*( sm->p)) && (*( sm->p)) <= -65 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) >= -67 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( sm->p)) - -128));
				if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
			}
		} else if ( (*( sm->p)) > -63 ) {
			if ( (*( sm->p)) < -30 ) {
				if ( (*( sm->p)) > -33 ) {
					if ( -32 <= (*( sm->p)) && (*( sm->p)) <= -31 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) >= -62 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( sm->p)) > -30 ) {
				if ( (*( sm->p)) < -28 ) {
					if ( -29 <= (*( sm->p)) && (*( sm->p)) <= -29 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) > -18 ) {
					if ( -17 <= (*( sm->p)) && (*( sm->p)) <= -17 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( sm->p)) - -128));
				if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
			}
		} else {
			_widec = (short)(640 + ((*( sm->p)) - -128));
			if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
		}
	} else if ( (*( sm->p)) > -12 ) {
		if ( (*( sm->p)) < 43 ) {
			if ( (*( sm->p)) < 14 ) {
				if ( (*( sm->p)) > -1 ) {
					if ( 1 <= (*( sm->p)) && (*( sm->p)) <= 8 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) >= -11 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( sm->p)) > 31 ) {
				if ( (*( sm->p)) > 33 ) {
					if ( 39 <= (*( sm->p)) && (*( sm->p)) <= 39 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) >= 33 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( sm->p)) - -128));
				if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
			}
		} else if ( (*( sm->p)) > 43 ) {
			if ( (*( sm->p)) < 65 ) {
				if ( (*( sm->p)) > 47 ) {
					if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) >= 45 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( sm->p)) > 90 ) {
				if ( (*( sm->p)) < 97 ) {
					if ( 95 <= (*( sm->p)) && (*( sm->p)) <= 95 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) > 122 ) {
					if ( 127 <= (*( sm->p)) )
 {						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( sm->p)) - -128));
				if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
			}
		} else {
			_widec = (short)(640 + ((*( sm->p)) - -128));
			if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
		}
	} else {
		_widec = (short)(640 + ((*( sm->p)) - -128));
		if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
	}
	switch( _widec ) {
		case 905: goto st1081;
		case 957: goto st1081;
		case 994: goto st1084;
		case 995: goto st1086;
		case 1007: goto st1088;
		case 1057: goto st1081;
		case 1063: goto st1081;
		case 1067: goto st1081;
		case 1119: goto st1081;
		case 1151: goto tr1310;
	}
	if ( _widec < 1013 ) {
		if ( _widec < 962 ) {
			if ( _widec > 959 ) {
				if ( 960 <= _widec && _widec <= 961 )
					goto st1081;
			} else if ( _widec >= 896 )
				goto tr1310;
		} else if ( _widec > 991 ) {
			if ( _widec > 1006 ) {
				if ( 1008 <= _widec && _widec <= 1012 )
					goto st1090;
			} else if ( _widec >= 992 )
				goto st1083;
		} else
			goto st1082;
	} else if ( _widec > 1023 ) {
		if ( _widec < 1069 ) {
			if ( _widec > 1032 ) {
				if ( 1038 <= _widec && _widec <= 1055 )
					goto tr1310;
			} else if ( _widec >= 1025 )
				goto tr1310;
		} else if ( _widec > 1071 ) {
			if ( _widec < 1089 ) {
				if ( 1072 <= _widec && _widec <= 1081 )
					goto tr1310;
			} else if ( _widec > 1114 ) {
				if ( 1121 <= _widec && _widec <= 1146 )
					goto tr1310;
			} else
				goto tr1310;
		} else
			goto st1081;
	} else
		goto st1081;
	goto tr207;
st1090:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1090;
case 1090:
	_widec = (*( sm->p));
	if ( (*( sm->p)) < 1 ) {
		if ( (*( sm->p)) < -30 ) {
			if ( (*( sm->p)) < -64 ) {
				if ( (*( sm->p)) <= -65 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( sm->p)) > -63 ) {
				if ( (*( sm->p)) > -33 ) {
					if ( -32 <= (*( sm->p)) && (*( sm->p)) <= -31 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) >= -62 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( sm->p)) - -128));
				if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
			}
		} else if ( (*( sm->p)) > -30 ) {
			if ( (*( sm->p)) < -17 ) {
				if ( (*( sm->p)) > -29 ) {
					if ( -28 <= (*( sm->p)) && (*( sm->p)) <= -18 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) >= -29 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( sm->p)) > -17 ) {
				if ( (*( sm->p)) > -12 ) {
					if ( -11 <= (*( sm->p)) && (*( sm->p)) <= -1 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) >= -16 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( sm->p)) - -128));
				if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
			}
		} else {
			_widec = (short)(640 + ((*( sm->p)) - -128));
			if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
		}
	} else if ( (*( sm->p)) > 8 ) {
		if ( (*( sm->p)) < 45 ) {
			if ( (*( sm->p)) < 33 ) {
				if ( 14 <= (*( sm->p)) && (*( sm->p)) <= 31 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( sm->p)) > 33 ) {
				if ( (*( sm->p)) > 39 ) {
					if ( 43 <= (*( sm->p)) && (*( sm->p)) <= 43 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) >= 39 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( sm->p)) - -128));
				if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
			}
		} else if ( (*( sm->p)) > 47 ) {
			if ( (*( sm->p)) < 95 ) {
				if ( (*( sm->p)) > 57 ) {
					if ( 65 <= (*( sm->p)) && (*( sm->p)) <= 90 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) >= 48 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( sm->p)) > 95 ) {
				if ( (*( sm->p)) > 122 ) {
					if ( 127 <= (*( sm->p)) )
 {						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) >= 97 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( sm->p)) - -128));
				if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
			}
		} else {
			_widec = (short)(640 + ((*( sm->p)) - -128));
			if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
		}
	} else {
		_widec = (short)(640 + ((*( sm->p)) - -128));
		if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
	}
	switch( _widec ) {
		case 994: goto st1084;
		case 995: goto st1086;
		case 1007: goto st1088;
		case 1057: goto st1081;
		case 1063: goto st1081;
		case 1067: goto st1081;
		case 1119: goto st1081;
		case 1151: goto tr1310;
	}
	if ( _widec < 1013 ) {
		if ( _widec < 962 ) {
			if ( _widec > 959 ) {
				if ( 960 <= _widec && _widec <= 961 )
					goto st1081;
			} else if ( _widec >= 896 )
				goto st1083;
		} else if ( _widec > 991 ) {
			if ( _widec > 1006 ) {
				if ( 1008 <= _widec && _widec <= 1012 )
					goto st1090;
			} else if ( _widec >= 992 )
				goto st1083;
		} else
			goto st1082;
	} else if ( _widec > 1023 ) {
		if ( _widec < 1069 ) {
			if ( _widec > 1032 ) {
				if ( 1038 <= _widec && _widec <= 1055 )
					goto tr1310;
			} else if ( _widec >= 1025 )
				goto tr1310;
		} else if ( _widec > 1071 ) {
			if ( _widec < 1089 ) {
				if ( 1072 <= _widec && _widec <= 1081 )
					goto tr1310;
			} else if ( _widec > 1114 ) {
				if ( 1121 <= _widec && _widec <= 1146 )
					goto tr1310;
			} else
				goto tr1310;
		} else
			goto st1081;
	} else
		goto st1081;
	goto tr207;
st1091:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1091;
case 1091:
	_widec = (*( sm->p));
	if ( (*( sm->p)) < -17 ) {
		if ( (*( sm->p)) < -92 ) {
			if ( (*( sm->p)) < -98 ) {
				if ( (*( sm->p)) > -100 ) {
					if ( -99 <= (*( sm->p)) && (*( sm->p)) <= -99 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( sm->p)) > -97 ) {
				if ( (*( sm->p)) < -95 ) {
					if ( -96 <= (*( sm->p)) && (*( sm->p)) <= -96 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) > -94 ) {
					if ( -93 <= (*( sm->p)) && (*( sm->p)) <= -93 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( sm->p)) - -128));
				if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
			}
		} else if ( (*( sm->p)) > -65 ) {
			if ( (*( sm->p)) < -32 ) {
				if ( (*( sm->p)) > -63 ) {
					if ( -62 <= (*( sm->p)) && (*( sm->p)) <= -33 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) >= -64 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( sm->p)) > -31 ) {
				if ( (*( sm->p)) < -29 ) {
					if ( -30 <= (*( sm->p)) && (*( sm->p)) <= -30 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) > -29 ) {
					if ( -28 <= (*( sm->p)) && (*( sm->p)) <= -18 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( sm->p)) - -128));
				if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
			}
		} else {
			_widec = (short)(640 + ((*( sm->p)) - -128));
			if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
		}
	} else if ( (*( sm->p)) > -17 ) {
		if ( (*( sm->p)) < 43 ) {
			if ( (*( sm->p)) < 1 ) {
				if ( (*( sm->p)) > -12 ) {
					if ( -11 <= (*( sm->p)) && (*( sm->p)) <= -1 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) >= -16 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( sm->p)) > 8 ) {
				if ( (*( sm->p)) < 33 ) {
					if ( 14 <= (*( sm->p)) && (*( sm->p)) <= 31 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) > 33 ) {
					if ( 39 <= (*( sm->p)) && (*( sm->p)) <= 39 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( sm->p)) - -128));
				if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
			}
		} else if ( (*( sm->p)) > 43 ) {
			if ( (*( sm->p)) < 65 ) {
				if ( (*( sm->p)) > 47 ) {
					if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) >= 45 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( sm->p)) > 90 ) {
				if ( (*( sm->p)) < 97 ) {
					if ( 95 <= (*( sm->p)) && (*( sm->p)) <= 95 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) > 122 ) {
					if ( 127 <= (*( sm->p)) )
 {						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( sm->p)) - -128));
				if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
			}
		} else {
			_widec = (short)(640 + ((*( sm->p)) - -128));
			if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
		}
	} else {
		_widec = (short)(640 + ((*( sm->p)) - -128));
		if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
	}
	switch( _widec ) {
		case 925: goto st1081;
		case 928: goto st1081;
		case 931: goto st1081;
		case 994: goto st1084;
		case 995: goto st1086;
		case 1007: goto st1088;
		case 1057: goto st1081;
		case 1063: goto st1081;
		case 1067: goto st1081;
		case 1119: goto st1081;
		case 1151: goto tr1310;
	}
	if ( _widec < 1013 ) {
		if ( _widec < 962 ) {
			if ( _widec > 959 ) {
				if ( 960 <= _widec && _widec <= 961 )
					goto st1081;
			} else if ( _widec >= 896 )
				goto tr1310;
		} else if ( _widec > 991 ) {
			if ( _widec > 1006 ) {
				if ( 1008 <= _widec && _widec <= 1012 )
					goto st1090;
			} else if ( _widec >= 992 )
				goto st1083;
		} else
			goto st1082;
	} else if ( _widec > 1023 ) {
		if ( _widec < 1069 ) {
			if ( _widec > 1032 ) {
				if ( 1038 <= _widec && _widec <= 1055 )
					goto tr1310;
			} else if ( _widec >= 1025 )
				goto tr1310;
		} else if ( _widec > 1071 ) {
			if ( _widec < 1089 ) {
				if ( 1072 <= _widec && _widec <= 1081 )
					goto tr1310;
			} else if ( _widec > 1114 ) {
				if ( 1121 <= _widec && _widec <= 1146 )
					goto tr1310;
			} else
				goto tr1310;
		} else
			goto st1081;
	} else
		goto st1081;
	goto tr207;
st1092:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1092;
case 1092:
	_widec = (*( sm->p));
	if ( (*( sm->p)) < 1 ) {
		if ( (*( sm->p)) < -30 ) {
			if ( (*( sm->p)) < -64 ) {
				if ( (*( sm->p)) <= -65 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( sm->p)) > -63 ) {
				if ( (*( sm->p)) > -33 ) {
					if ( -32 <= (*( sm->p)) && (*( sm->p)) <= -31 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) >= -62 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( sm->p)) - -128));
				if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
			}
		} else if ( (*( sm->p)) > -30 ) {
			if ( (*( sm->p)) < -17 ) {
				if ( (*( sm->p)) > -29 ) {
					if ( -28 <= (*( sm->p)) && (*( sm->p)) <= -18 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) >= -29 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( sm->p)) > -17 ) {
				if ( (*( sm->p)) > -12 ) {
					if ( -11 <= (*( sm->p)) && (*( sm->p)) <= -1 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) >= -16 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( sm->p)) - -128));
				if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
			}
		} else {
			_widec = (short)(640 + ((*( sm->p)) - -128));
			if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
		}
	} else if ( (*( sm->p)) > 8 ) {
		if ( (*( sm->p)) < 45 ) {
			if ( (*( sm->p)) < 33 ) {
				if ( 14 <= (*( sm->p)) && (*( sm->p)) <= 31 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( sm->p)) > 33 ) {
				if ( (*( sm->p)) > 39 ) {
					if ( 43 <= (*( sm->p)) && (*( sm->p)) <= 43 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) >= 39 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( sm->p)) - -128));
				if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
			}
		} else if ( (*( sm->p)) > 47 ) {
			if ( (*( sm->p)) < 95 ) {
				if ( (*( sm->p)) > 57 ) {
					if ( 65 <= (*( sm->p)) && (*( sm->p)) <= 90 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) >= 48 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( sm->p)) > 95 ) {
				if ( (*( sm->p)) > 122 ) {
					if ( 127 <= (*( sm->p)) )
 {						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) >= 97 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( sm->p)) - -128));
				if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
			}
		} else {
			_widec = (short)(640 + ((*( sm->p)) - -128));
			if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
		}
	} else {
		_widec = (short)(640 + ((*( sm->p)) - -128));
		if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
	}
	switch( _widec ) {
		case 994: goto st1084;
		case 995: goto st1086;
		case 1007: goto st1088;
		case 1057: goto st1081;
		case 1063: goto st1081;
		case 1067: goto st1081;
		case 1119: goto st1081;
		case 1151: goto tr1310;
	}
	if ( _widec < 1013 ) {
		if ( _widec < 962 ) {
			if ( _widec > 959 ) {
				if ( 960 <= _widec && _widec <= 961 )
					goto st1081;
			} else if ( _widec >= 896 )
				goto tr1302;
		} else if ( _widec > 991 ) {
			if ( _widec > 1006 ) {
				if ( 1008 <= _widec && _widec <= 1012 )
					goto st1090;
			} else if ( _widec >= 992 )
				goto st1083;
		} else
			goto st1082;
	} else if ( _widec > 1023 ) {
		if ( _widec < 1069 ) {
			if ( _widec > 1032 ) {
				if ( 1038 <= _widec && _widec <= 1055 )
					goto tr1310;
			} else if ( _widec >= 1025 )
				goto tr1310;
		} else if ( _widec > 1071 ) {
			if ( _widec < 1089 ) {
				if ( 1072 <= _widec && _widec <= 1081 )
					goto tr1310;
			} else if ( _widec > 1114 ) {
				if ( 1121 <= _widec && _widec <= 1146 )
					goto tr1310;
			} else
				goto tr1310;
		} else
			goto st1081;
	} else
		goto st1081;
	goto tr207;
tr1302:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
#line 72 "ext/dtext/dtext.cpp.rl"
	{ sm->a2 = sm->p; }
#line 354 "ext/dtext/dtext.cpp.rl"
	{( sm->act) = 40;}
	goto st1719;
st1719:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1719;
case 1719:
#line 27333 "ext/dtext/dtext.cpp"
	_widec = (*( sm->p));
	if ( (*( sm->p)) < 1 ) {
		if ( (*( sm->p)) < -29 ) {
			if ( (*( sm->p)) < -62 ) {
				if ( (*( sm->p)) <= -63 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( sm->p)) > -33 ) {
				if ( (*( sm->p)) > -31 ) {
					if ( -30 <= (*( sm->p)) && (*( sm->p)) <= -30 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) >= -32 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( sm->p)) - -128));
				if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
			}
		} else if ( (*( sm->p)) > -29 ) {
			if ( (*( sm->p)) < -17 ) {
				if ( -28 <= (*( sm->p)) && (*( sm->p)) <= -18 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( sm->p)) > -17 ) {
				if ( (*( sm->p)) > -12 ) {
					if ( -11 <= (*( sm->p)) && (*( sm->p)) <= -1 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) >= -16 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( sm->p)) - -128));
				if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
			}
		} else {
			_widec = (short)(640 + ((*( sm->p)) - -128));
			if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
		}
	} else if ( (*( sm->p)) > 8 ) {
		if ( (*( sm->p)) < 45 ) {
			if ( (*( sm->p)) < 33 ) {
				if ( 14 <= (*( sm->p)) && (*( sm->p)) <= 31 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( sm->p)) > 33 ) {
				if ( (*( sm->p)) > 39 ) {
					if ( 43 <= (*( sm->p)) && (*( sm->p)) <= 43 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) >= 39 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( sm->p)) - -128));
				if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
			}
		} else if ( (*( sm->p)) > 47 ) {
			if ( (*( sm->p)) < 95 ) {
				if ( (*( sm->p)) > 57 ) {
					if ( 65 <= (*( sm->p)) && (*( sm->p)) <= 90 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) >= 48 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( sm->p)) > 95 ) {
				if ( (*( sm->p)) > 122 ) {
					if ( 127 <= (*( sm->p)) )
 {						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) >= 97 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( sm->p)) - -128));
				if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
			}
		} else {
			_widec = (short)(640 + ((*( sm->p)) - -128));
			if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
		}
	} else {
		_widec = (short)(640 + ((*( sm->p)) - -128));
		if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
	}
	switch( _widec ) {
		case 994: goto st1094;
		case 995: goto st1096;
		case 1007: goto st1098;
		case 1057: goto st1080;
		case 1063: goto st1102;
		case 1067: goto st1080;
		case 1119: goto st1080;
		case 1151: goto tr1302;
	}
	if ( _widec < 1025 ) {
		if ( _widec < 992 ) {
			if ( _widec > 961 ) {
				if ( 962 <= _widec && _widec <= 991 )
					goto st1092;
			} else if ( _widec >= 896 )
				goto st1081;
		} else if ( _widec > 1006 ) {
			if ( _widec > 1012 ) {
				if ( 1013 <= _widec && _widec <= 1023 )
					goto st1081;
			} else if ( _widec >= 1008 )
				goto st1101;
		} else
			goto st1093;
	} else if ( _widec > 1032 ) {
		if ( _widec < 1072 ) {
			if ( _widec > 1055 ) {
				if ( 1069 <= _widec && _widec <= 1071 )
					goto st1080;
			} else if ( _widec >= 1038 )
				goto tr1302;
		} else if ( _widec > 1081 ) {
			if ( _widec > 1114 ) {
				if ( 1121 <= _widec && _widec <= 1146 )
					goto tr1302;
			} else if ( _widec >= 1089 )
				goto tr1302;
		} else
			goto tr1302;
	} else
		goto tr1302;
	goto tr2147;
st1093:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1093;
case 1093:
	_widec = (*( sm->p));
	if ( (*( sm->p)) < 1 ) {
		if ( (*( sm->p)) < -30 ) {
			if ( (*( sm->p)) < -64 ) {
				if ( (*( sm->p)) <= -65 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( sm->p)) > -63 ) {
				if ( (*( sm->p)) > -33 ) {
					if ( -32 <= (*( sm->p)) && (*( sm->p)) <= -31 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) >= -62 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( sm->p)) - -128));
				if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
			}
		} else if ( (*( sm->p)) > -30 ) {
			if ( (*( sm->p)) < -17 ) {
				if ( (*( sm->p)) > -29 ) {
					if ( -28 <= (*( sm->p)) && (*( sm->p)) <= -18 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) >= -29 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( sm->p)) > -17 ) {
				if ( (*( sm->p)) > -12 ) {
					if ( -11 <= (*( sm->p)) && (*( sm->p)) <= -1 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) >= -16 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( sm->p)) - -128));
				if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
			}
		} else {
			_widec = (short)(640 + ((*( sm->p)) - -128));
			if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
		}
	} else if ( (*( sm->p)) > 8 ) {
		if ( (*( sm->p)) < 45 ) {
			if ( (*( sm->p)) < 33 ) {
				if ( 14 <= (*( sm->p)) && (*( sm->p)) <= 31 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( sm->p)) > 33 ) {
				if ( (*( sm->p)) > 39 ) {
					if ( 43 <= (*( sm->p)) && (*( sm->p)) <= 43 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) >= 39 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( sm->p)) - -128));
				if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
			}
		} else if ( (*( sm->p)) > 47 ) {
			if ( (*( sm->p)) < 95 ) {
				if ( (*( sm->p)) > 57 ) {
					if ( 65 <= (*( sm->p)) && (*( sm->p)) <= 90 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) >= 48 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( sm->p)) > 95 ) {
				if ( (*( sm->p)) > 122 ) {
					if ( 127 <= (*( sm->p)) )
 {						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) >= 97 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( sm->p)) - -128));
				if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
			}
		} else {
			_widec = (short)(640 + ((*( sm->p)) - -128));
			if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
		}
	} else {
		_widec = (short)(640 + ((*( sm->p)) - -128));
		if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
	}
	switch( _widec ) {
		case 994: goto st1084;
		case 995: goto st1086;
		case 1007: goto st1088;
		case 1057: goto st1081;
		case 1063: goto st1081;
		case 1067: goto st1081;
		case 1119: goto st1081;
		case 1151: goto tr1310;
	}
	if ( _widec < 1013 ) {
		if ( _widec < 962 ) {
			if ( _widec > 959 ) {
				if ( 960 <= _widec && _widec <= 961 )
					goto st1081;
			} else if ( _widec >= 896 )
				goto st1092;
		} else if ( _widec > 991 ) {
			if ( _widec > 1006 ) {
				if ( 1008 <= _widec && _widec <= 1012 )
					goto st1090;
			} else if ( _widec >= 992 )
				goto st1083;
		} else
			goto st1082;
	} else if ( _widec > 1023 ) {
		if ( _widec < 1069 ) {
			if ( _widec > 1032 ) {
				if ( 1038 <= _widec && _widec <= 1055 )
					goto tr1310;
			} else if ( _widec >= 1025 )
				goto tr1310;
		} else if ( _widec > 1071 ) {
			if ( _widec < 1089 ) {
				if ( 1072 <= _widec && _widec <= 1081 )
					goto tr1310;
			} else if ( _widec > 1114 ) {
				if ( 1121 <= _widec && _widec <= 1146 )
					goto tr1310;
			} else
				goto tr1310;
		} else
			goto st1081;
	} else
		goto st1081;
	goto tr207;
st1094:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1094;
case 1094:
	_widec = (*( sm->p));
	if ( (*( sm->p)) < -11 ) {
		if ( (*( sm->p)) < -32 ) {
			if ( (*( sm->p)) < -98 ) {
				if ( (*( sm->p)) > -100 ) {
					if ( -99 <= (*( sm->p)) && (*( sm->p)) <= -99 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( sm->p)) > -65 ) {
				if ( (*( sm->p)) > -63 ) {
					if ( -62 <= (*( sm->p)) && (*( sm->p)) <= -33 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) >= -64 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( sm->p)) - -128));
				if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
			}
		} else if ( (*( sm->p)) > -31 ) {
			if ( (*( sm->p)) < -28 ) {
				if ( (*( sm->p)) > -30 ) {
					if ( -29 <= (*( sm->p)) && (*( sm->p)) <= -29 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) >= -30 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( sm->p)) > -18 ) {
				if ( (*( sm->p)) > -17 ) {
					if ( -16 <= (*( sm->p)) && (*( sm->p)) <= -12 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) >= -17 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( sm->p)) - -128));
				if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
			}
		} else {
			_widec = (short)(640 + ((*( sm->p)) - -128));
			if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
		}
	} else if ( (*( sm->p)) > -1 ) {
		if ( (*( sm->p)) < 45 ) {
			if ( (*( sm->p)) < 33 ) {
				if ( (*( sm->p)) > 8 ) {
					if ( 14 <= (*( sm->p)) && (*( sm->p)) <= 31 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) >= 1 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( sm->p)) > 33 ) {
				if ( (*( sm->p)) > 39 ) {
					if ( 43 <= (*( sm->p)) && (*( sm->p)) <= 43 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) >= 39 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( sm->p)) - -128));
				if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
			}
		} else if ( (*( sm->p)) > 47 ) {
			if ( (*( sm->p)) < 95 ) {
				if ( (*( sm->p)) > 57 ) {
					if ( 65 <= (*( sm->p)) && (*( sm->p)) <= 90 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) >= 48 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( sm->p)) > 95 ) {
				if ( (*( sm->p)) > 122 ) {
					if ( 127 <= (*( sm->p)) )
 {						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) >= 97 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( sm->p)) - -128));
				if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
			}
		} else {
			_widec = (short)(640 + ((*( sm->p)) - -128));
			if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
		}
	} else {
		_widec = (short)(640 + ((*( sm->p)) - -128));
		if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
	}
	switch( _widec ) {
		case 925: goto st1095;
		case 994: goto st1084;
		case 995: goto st1086;
		case 1007: goto st1088;
		case 1057: goto st1081;
		case 1063: goto st1081;
		case 1067: goto st1081;
		case 1119: goto st1081;
		case 1151: goto tr1310;
	}
	if ( _widec < 1013 ) {
		if ( _widec < 962 ) {
			if ( _widec > 959 ) {
				if ( 960 <= _widec && _widec <= 961 )
					goto st1081;
			} else if ( _widec >= 896 )
				goto st1092;
		} else if ( _widec > 991 ) {
			if ( _widec > 1006 ) {
				if ( 1008 <= _widec && _widec <= 1012 )
					goto st1090;
			} else if ( _widec >= 992 )
				goto st1083;
		} else
			goto st1082;
	} else if ( _widec > 1023 ) {
		if ( _widec < 1069 ) {
			if ( _widec > 1032 ) {
				if ( 1038 <= _widec && _widec <= 1055 )
					goto tr1310;
			} else if ( _widec >= 1025 )
				goto tr1310;
		} else if ( _widec > 1071 ) {
			if ( _widec < 1089 ) {
				if ( 1072 <= _widec && _widec <= 1081 )
					goto tr1310;
			} else if ( _widec > 1114 ) {
				if ( 1121 <= _widec && _widec <= 1146 )
					goto tr1310;
			} else
				goto tr1310;
		} else
			goto st1081;
	} else
		goto st1081;
	goto tr207;
st1095:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1095;
case 1095:
	_widec = (*( sm->p));
	if ( (*( sm->p)) < -11 ) {
		if ( (*( sm->p)) < -32 ) {
			if ( (*( sm->p)) < -82 ) {
				if ( (*( sm->p)) > -84 ) {
					if ( -83 <= (*( sm->p)) && (*( sm->p)) <= -83 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( sm->p)) > -65 ) {
				if ( (*( sm->p)) > -63 ) {
					if ( -62 <= (*( sm->p)) && (*( sm->p)) <= -33 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) >= -64 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( sm->p)) - -128));
				if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
			}
		} else if ( (*( sm->p)) > -31 ) {
			if ( (*( sm->p)) < -28 ) {
				if ( (*( sm->p)) > -30 ) {
					if ( -29 <= (*( sm->p)) && (*( sm->p)) <= -29 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) >= -30 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( sm->p)) > -18 ) {
				if ( (*( sm->p)) > -17 ) {
					if ( -16 <= (*( sm->p)) && (*( sm->p)) <= -12 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) >= -17 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( sm->p)) - -128));
				if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
			}
		} else {
			_widec = (short)(640 + ((*( sm->p)) - -128));
			if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
		}
	} else if ( (*( sm->p)) > -1 ) {
		if ( (*( sm->p)) < 45 ) {
			if ( (*( sm->p)) < 33 ) {
				if ( (*( sm->p)) > 8 ) {
					if ( 14 <= (*( sm->p)) && (*( sm->p)) <= 31 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) >= 1 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( sm->p)) > 33 ) {
				if ( (*( sm->p)) > 39 ) {
					if ( 43 <= (*( sm->p)) && (*( sm->p)) <= 43 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) >= 39 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( sm->p)) - -128));
				if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
			}
		} else if ( (*( sm->p)) > 47 ) {
			if ( (*( sm->p)) < 95 ) {
				if ( (*( sm->p)) > 57 ) {
					if ( 65 <= (*( sm->p)) && (*( sm->p)) <= 90 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) >= 48 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( sm->p)) > 95 ) {
				if ( (*( sm->p)) > 122 ) {
					if ( 127 <= (*( sm->p)) )
 {						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) >= 97 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( sm->p)) - -128));
				if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
			}
		} else {
			_widec = (short)(640 + ((*( sm->p)) - -128));
			if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
		}
	} else {
		_widec = (short)(640 + ((*( sm->p)) - -128));
		if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
	}
	switch( _widec ) {
		case 941: goto st1080;
		case 994: goto st1084;
		case 995: goto st1086;
		case 1007: goto st1088;
		case 1057: goto st1081;
		case 1063: goto st1081;
		case 1067: goto st1081;
		case 1119: goto st1081;
		case 1151: goto tr1310;
	}
	if ( _widec < 1013 ) {
		if ( _widec < 962 ) {
			if ( _widec > 959 ) {
				if ( 960 <= _widec && _widec <= 961 )
					goto st1081;
			} else if ( _widec >= 896 )
				goto tr1302;
		} else if ( _widec > 991 ) {
			if ( _widec > 1006 ) {
				if ( 1008 <= _widec && _widec <= 1012 )
					goto st1090;
			} else if ( _widec >= 992 )
				goto st1083;
		} else
			goto st1082;
	} else if ( _widec > 1023 ) {
		if ( _widec < 1069 ) {
			if ( _widec > 1032 ) {
				if ( 1038 <= _widec && _widec <= 1055 )
					goto tr1310;
			} else if ( _widec >= 1025 )
				goto tr1310;
		} else if ( _widec > 1071 ) {
			if ( _widec < 1089 ) {
				if ( 1072 <= _widec && _widec <= 1081 )
					goto tr1310;
			} else if ( _widec > 1114 ) {
				if ( 1121 <= _widec && _widec <= 1146 )
					goto tr1310;
			} else
				goto tr1310;
		} else
			goto st1081;
	} else
		goto st1081;
	goto tr207;
st1096:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1096;
case 1096:
	_widec = (*( sm->p));
	if ( (*( sm->p)) < -11 ) {
		if ( (*( sm->p)) < -32 ) {
			if ( (*( sm->p)) < -127 ) {
				if ( (*( sm->p)) <= -128 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( sm->p)) > -65 ) {
				if ( (*( sm->p)) > -63 ) {
					if ( -62 <= (*( sm->p)) && (*( sm->p)) <= -33 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) >= -64 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( sm->p)) - -128));
				if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
			}
		} else if ( (*( sm->p)) > -31 ) {
			if ( (*( sm->p)) < -28 ) {
				if ( (*( sm->p)) > -30 ) {
					if ( -29 <= (*( sm->p)) && (*( sm->p)) <= -29 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) >= -30 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( sm->p)) > -18 ) {
				if ( (*( sm->p)) > -17 ) {
					if ( -16 <= (*( sm->p)) && (*( sm->p)) <= -12 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) >= -17 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( sm->p)) - -128));
				if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
			}
		} else {
			_widec = (short)(640 + ((*( sm->p)) - -128));
			if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
		}
	} else if ( (*( sm->p)) > -1 ) {
		if ( (*( sm->p)) < 45 ) {
			if ( (*( sm->p)) < 33 ) {
				if ( (*( sm->p)) > 8 ) {
					if ( 14 <= (*( sm->p)) && (*( sm->p)) <= 31 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) >= 1 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( sm->p)) > 33 ) {
				if ( (*( sm->p)) > 39 ) {
					if ( 43 <= (*( sm->p)) && (*( sm->p)) <= 43 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) >= 39 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( sm->p)) - -128));
				if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
			}
		} else if ( (*( sm->p)) > 47 ) {
			if ( (*( sm->p)) < 95 ) {
				if ( (*( sm->p)) > 57 ) {
					if ( 65 <= (*( sm->p)) && (*( sm->p)) <= 90 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) >= 48 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( sm->p)) > 95 ) {
				if ( (*( sm->p)) > 122 ) {
					if ( 127 <= (*( sm->p)) )
 {						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) >= 97 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( sm->p)) - -128));
				if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
			}
		} else {
			_widec = (short)(640 + ((*( sm->p)) - -128));
			if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
		}
	} else {
		_widec = (short)(640 + ((*( sm->p)) - -128));
		if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
	}
	switch( _widec ) {
		case 896: goto st1097;
		case 994: goto st1084;
		case 995: goto st1086;
		case 1007: goto st1088;
		case 1057: goto st1081;
		case 1063: goto st1081;
		case 1067: goto st1081;
		case 1119: goto st1081;
		case 1151: goto tr1310;
	}
	if ( _widec < 1013 ) {
		if ( _widec < 962 ) {
			if ( _widec > 959 ) {
				if ( 960 <= _widec && _widec <= 961 )
					goto st1081;
			} else if ( _widec >= 897 )
				goto st1092;
		} else if ( _widec > 991 ) {
			if ( _widec > 1006 ) {
				if ( 1008 <= _widec && _widec <= 1012 )
					goto st1090;
			} else if ( _widec >= 992 )
				goto st1083;
		} else
			goto st1082;
	} else if ( _widec > 1023 ) {
		if ( _widec < 1069 ) {
			if ( _widec > 1032 ) {
				if ( 1038 <= _widec && _widec <= 1055 )
					goto tr1310;
			} else if ( _widec >= 1025 )
				goto tr1310;
		} else if ( _widec > 1071 ) {
			if ( _widec < 1089 ) {
				if ( 1072 <= _widec && _widec <= 1081 )
					goto tr1310;
			} else if ( _widec > 1114 ) {
				if ( 1121 <= _widec && _widec <= 1146 )
					goto tr1310;
			} else
				goto tr1310;
		} else
			goto st1081;
	} else
		goto st1081;
	goto tr207;
st1097:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1097;
case 1097:
	_widec = (*( sm->p));
	if ( (*( sm->p)) < -17 ) {
		if ( (*( sm->p)) < -99 ) {
			if ( (*( sm->p)) < -120 ) {
				if ( (*( sm->p)) > -126 ) {
					if ( -125 <= (*( sm->p)) && (*( sm->p)) <= -121 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( sm->p)) > -111 ) {
				if ( (*( sm->p)) > -109 ) {
					if ( -108 <= (*( sm->p)) && (*( sm->p)) <= -100 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) >= -110 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( sm->p)) - -128));
				if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
			}
		} else if ( (*( sm->p)) > -65 ) {
			if ( (*( sm->p)) < -32 ) {
				if ( (*( sm->p)) > -63 ) {
					if ( -62 <= (*( sm->p)) && (*( sm->p)) <= -33 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) >= -64 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( sm->p)) > -31 ) {
				if ( (*( sm->p)) < -29 ) {
					if ( -30 <= (*( sm->p)) && (*( sm->p)) <= -30 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) > -29 ) {
					if ( -28 <= (*( sm->p)) && (*( sm->p)) <= -18 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( sm->p)) - -128));
				if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
			}
		} else {
			_widec = (short)(640 + ((*( sm->p)) - -128));
			if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
		}
	} else if ( (*( sm->p)) > -17 ) {
		if ( (*( sm->p)) < 43 ) {
			if ( (*( sm->p)) < 1 ) {
				if ( (*( sm->p)) > -12 ) {
					if ( -11 <= (*( sm->p)) && (*( sm->p)) <= -1 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) >= -16 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( sm->p)) > 8 ) {
				if ( (*( sm->p)) < 33 ) {
					if ( 14 <= (*( sm->p)) && (*( sm->p)) <= 31 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) > 33 ) {
					if ( 39 <= (*( sm->p)) && (*( sm->p)) <= 39 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( sm->p)) - -128));
				if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
			}
		} else if ( (*( sm->p)) > 43 ) {
			if ( (*( sm->p)) < 65 ) {
				if ( (*( sm->p)) > 47 ) {
					if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) >= 45 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( sm->p)) > 90 ) {
				if ( (*( sm->p)) < 97 ) {
					if ( 95 <= (*( sm->p)) && (*( sm->p)) <= 95 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) > 122 ) {
					if ( 127 <= (*( sm->p)) )
 {						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( sm->p)) - -128));
				if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
			}
		} else {
			_widec = (short)(640 + ((*( sm->p)) - -128));
			if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
		}
	} else {
		_widec = (short)(640 + ((*( sm->p)) - -128));
		if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
	}
	switch( _widec ) {
		case 994: goto st1084;
		case 995: goto st1086;
		case 1007: goto st1088;
		case 1057: goto st1081;
		case 1063: goto st1081;
		case 1067: goto st1081;
		case 1119: goto st1081;
		case 1151: goto tr1310;
	}
	if ( _widec < 992 ) {
		if ( _widec < 914 ) {
			if ( _widec < 899 ) {
				if ( 896 <= _widec && _widec <= 898 )
					goto st1080;
			} else if ( _widec > 903 ) {
				if ( 904 <= _widec && _widec <= 913 )
					goto st1080;
			} else
				goto tr1302;
		} else if ( _widec > 915 ) {
			if ( _widec < 925 ) {
				if ( 916 <= _widec && _widec <= 924 )
					goto st1080;
			} else if ( _widec > 959 ) {
				if ( _widec > 961 ) {
					if ( 962 <= _widec && _widec <= 991 )
						goto st1082;
				} else if ( _widec >= 960 )
					goto st1081;
			} else
				goto tr1302;
		} else
			goto tr1302;
	} else if ( _widec > 1006 ) {
		if ( _widec < 1038 ) {
			if ( _widec < 1013 ) {
				if ( 1008 <= _widec && _widec <= 1012 )
					goto st1090;
			} else if ( _widec > 1023 ) {
				if ( 1025 <= _widec && _widec <= 1032 )
					goto tr1310;
			} else
				goto st1081;
		} else if ( _widec > 1055 ) {
			if ( _widec < 1072 ) {
				if ( 1069 <= _widec && _widec <= 1071 )
					goto st1081;
			} else if ( _widec > 1081 ) {
				if ( _widec > 1114 ) {
					if ( 1121 <= _widec && _widec <= 1146 )
						goto tr1310;
				} else if ( _widec >= 1089 )
					goto tr1310;
			} else
				goto tr1310;
		} else
			goto tr1310;
	} else
		goto st1083;
	goto tr207;
st1098:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1098;
case 1098:
	_widec = (*( sm->p));
	if ( (*( sm->p)) < -16 ) {
		if ( (*( sm->p)) < -62 ) {
			if ( (*( sm->p)) < -67 ) {
				if ( (*( sm->p)) > -69 ) {
					if ( -68 <= (*( sm->p)) && (*( sm->p)) <= -68 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( sm->p)) > -67 ) {
				if ( (*( sm->p)) > -65 ) {
					if ( -64 <= (*( sm->p)) && (*( sm->p)) <= -63 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) >= -66 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( sm->p)) - -128));
				if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
			}
		} else if ( (*( sm->p)) > -33 ) {
			if ( (*( sm->p)) < -29 ) {
				if ( (*( sm->p)) > -31 ) {
					if ( -30 <= (*( sm->p)) && (*( sm->p)) <= -30 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) >= -32 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( sm->p)) > -29 ) {
				if ( (*( sm->p)) > -18 ) {
					if ( -17 <= (*( sm->p)) && (*( sm->p)) <= -17 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) >= -28 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( sm->p)) - -128));
				if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
			}
		} else {
			_widec = (short)(640 + ((*( sm->p)) - -128));
			if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
		}
	} else if ( (*( sm->p)) > -12 ) {
		if ( (*( sm->p)) < 43 ) {
			if ( (*( sm->p)) < 14 ) {
				if ( (*( sm->p)) > -1 ) {
					if ( 1 <= (*( sm->p)) && (*( sm->p)) <= 8 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) >= -11 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( sm->p)) > 31 ) {
				if ( (*( sm->p)) > 33 ) {
					if ( 39 <= (*( sm->p)) && (*( sm->p)) <= 39 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) >= 33 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( sm->p)) - -128));
				if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
			}
		} else if ( (*( sm->p)) > 43 ) {
			if ( (*( sm->p)) < 65 ) {
				if ( (*( sm->p)) > 47 ) {
					if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) >= 45 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( sm->p)) > 90 ) {
				if ( (*( sm->p)) < 97 ) {
					if ( 95 <= (*( sm->p)) && (*( sm->p)) <= 95 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) > 122 ) {
					if ( 127 <= (*( sm->p)) )
 {						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( sm->p)) - -128));
				if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
			}
		} else {
			_widec = (short)(640 + ((*( sm->p)) - -128));
			if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
		}
	} else {
		_widec = (short)(640 + ((*( sm->p)) - -128));
		if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
	}
	switch( _widec ) {
		case 956: goto st1099;
		case 957: goto st1100;
		case 994: goto st1084;
		case 995: goto st1086;
		case 1007: goto st1088;
		case 1057: goto st1081;
		case 1063: goto st1081;
		case 1067: goto st1081;
		case 1119: goto st1081;
		case 1151: goto tr1310;
	}
	if ( _widec < 1013 ) {
		if ( _widec < 962 ) {
			if ( _widec > 959 ) {
				if ( 960 <= _widec && _widec <= 961 )
					goto st1081;
			} else if ( _widec >= 896 )
				goto st1092;
		} else if ( _widec > 991 ) {
			if ( _widec > 1006 ) {
				if ( 1008 <= _widec && _widec <= 1012 )
					goto st1090;
			} else if ( _widec >= 992 )
				goto st1083;
		} else
			goto st1082;
	} else if ( _widec > 1023 ) {
		if ( _widec < 1069 ) {
			if ( _widec > 1032 ) {
				if ( 1038 <= _widec && _widec <= 1055 )
					goto tr1310;
			} else if ( _widec >= 1025 )
				goto tr1310;
		} else if ( _widec > 1071 ) {
			if ( _widec < 1089 ) {
				if ( 1072 <= _widec && _widec <= 1081 )
					goto tr1310;
			} else if ( _widec > 1114 ) {
				if ( 1121 <= _widec && _widec <= 1146 )
					goto tr1310;
			} else
				goto tr1310;
		} else
			goto st1081;
	} else
		goto st1081;
	goto tr207;
st1099:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1099;
case 1099:
	_widec = (*( sm->p));
	if ( (*( sm->p)) < -16 ) {
		if ( (*( sm->p)) < -64 ) {
			if ( (*( sm->p)) < -118 ) {
				if ( (*( sm->p)) > -120 ) {
					if ( -119 <= (*( sm->p)) && (*( sm->p)) <= -119 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( sm->p)) > -68 ) {
				if ( (*( sm->p)) > -67 ) {
					if ( -66 <= (*( sm->p)) && (*( sm->p)) <= -65 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) >= -67 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( sm->p)) - -128));
				if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
			}
		} else if ( (*( sm->p)) > -63 ) {
			if ( (*( sm->p)) < -30 ) {
				if ( (*( sm->p)) > -33 ) {
					if ( -32 <= (*( sm->p)) && (*( sm->p)) <= -31 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) >= -62 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( sm->p)) > -30 ) {
				if ( (*( sm->p)) < -28 ) {
					if ( -29 <= (*( sm->p)) && (*( sm->p)) <= -29 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) > -18 ) {
					if ( -17 <= (*( sm->p)) && (*( sm->p)) <= -17 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( sm->p)) - -128));
				if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
			}
		} else {
			_widec = (short)(640 + ((*( sm->p)) - -128));
			if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
		}
	} else if ( (*( sm->p)) > -12 ) {
		if ( (*( sm->p)) < 43 ) {
			if ( (*( sm->p)) < 14 ) {
				if ( (*( sm->p)) > -1 ) {
					if ( 1 <= (*( sm->p)) && (*( sm->p)) <= 8 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) >= -11 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( sm->p)) > 31 ) {
				if ( (*( sm->p)) > 33 ) {
					if ( 39 <= (*( sm->p)) && (*( sm->p)) <= 39 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) >= 33 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( sm->p)) - -128));
				if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
			}
		} else if ( (*( sm->p)) > 43 ) {
			if ( (*( sm->p)) < 65 ) {
				if ( (*( sm->p)) > 47 ) {
					if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) >= 45 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( sm->p)) > 90 ) {
				if ( (*( sm->p)) < 97 ) {
					if ( 95 <= (*( sm->p)) && (*( sm->p)) <= 95 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) > 122 ) {
					if ( 127 <= (*( sm->p)) )
 {						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( sm->p)) - -128));
				if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
			}
		} else {
			_widec = (short)(640 + ((*( sm->p)) - -128));
			if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
		}
	} else {
		_widec = (short)(640 + ((*( sm->p)) - -128));
		if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
	}
	switch( _widec ) {
		case 905: goto st1080;
		case 957: goto st1080;
		case 994: goto st1084;
		case 995: goto st1086;
		case 1007: goto st1088;
		case 1057: goto st1081;
		case 1063: goto st1081;
		case 1067: goto st1081;
		case 1119: goto st1081;
		case 1151: goto tr1310;
	}
	if ( _widec < 1013 ) {
		if ( _widec < 962 ) {
			if ( _widec > 959 ) {
				if ( 960 <= _widec && _widec <= 961 )
					goto st1081;
			} else if ( _widec >= 896 )
				goto tr1302;
		} else if ( _widec > 991 ) {
			if ( _widec > 1006 ) {
				if ( 1008 <= _widec && _widec <= 1012 )
					goto st1090;
			} else if ( _widec >= 992 )
				goto st1083;
		} else
			goto st1082;
	} else if ( _widec > 1023 ) {
		if ( _widec < 1069 ) {
			if ( _widec > 1032 ) {
				if ( 1038 <= _widec && _widec <= 1055 )
					goto tr1310;
			} else if ( _widec >= 1025 )
				goto tr1310;
		} else if ( _widec > 1071 ) {
			if ( _widec < 1089 ) {
				if ( 1072 <= _widec && _widec <= 1081 )
					goto tr1310;
			} else if ( _widec > 1114 ) {
				if ( 1121 <= _widec && _widec <= 1146 )
					goto tr1310;
			} else
				goto tr1310;
		} else
			goto st1081;
	} else
		goto st1081;
	goto tr207;
st1100:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1100;
case 1100:
	_widec = (*( sm->p));
	if ( (*( sm->p)) < -17 ) {
		if ( (*( sm->p)) < -92 ) {
			if ( (*( sm->p)) < -98 ) {
				if ( (*( sm->p)) > -100 ) {
					if ( -99 <= (*( sm->p)) && (*( sm->p)) <= -99 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( sm->p)) > -97 ) {
				if ( (*( sm->p)) < -95 ) {
					if ( -96 <= (*( sm->p)) && (*( sm->p)) <= -96 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) > -94 ) {
					if ( -93 <= (*( sm->p)) && (*( sm->p)) <= -93 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( sm->p)) - -128));
				if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
			}
		} else if ( (*( sm->p)) > -65 ) {
			if ( (*( sm->p)) < -32 ) {
				if ( (*( sm->p)) > -63 ) {
					if ( -62 <= (*( sm->p)) && (*( sm->p)) <= -33 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) >= -64 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( sm->p)) > -31 ) {
				if ( (*( sm->p)) < -29 ) {
					if ( -30 <= (*( sm->p)) && (*( sm->p)) <= -30 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) > -29 ) {
					if ( -28 <= (*( sm->p)) && (*( sm->p)) <= -18 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( sm->p)) - -128));
				if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
			}
		} else {
			_widec = (short)(640 + ((*( sm->p)) - -128));
			if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
		}
	} else if ( (*( sm->p)) > -17 ) {
		if ( (*( sm->p)) < 43 ) {
			if ( (*( sm->p)) < 1 ) {
				if ( (*( sm->p)) > -12 ) {
					if ( -11 <= (*( sm->p)) && (*( sm->p)) <= -1 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) >= -16 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( sm->p)) > 8 ) {
				if ( (*( sm->p)) < 33 ) {
					if ( 14 <= (*( sm->p)) && (*( sm->p)) <= 31 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) > 33 ) {
					if ( 39 <= (*( sm->p)) && (*( sm->p)) <= 39 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( sm->p)) - -128));
				if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
			}
		} else if ( (*( sm->p)) > 43 ) {
			if ( (*( sm->p)) < 65 ) {
				if ( (*( sm->p)) > 47 ) {
					if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) >= 45 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( sm->p)) > 90 ) {
				if ( (*( sm->p)) < 97 ) {
					if ( 95 <= (*( sm->p)) && (*( sm->p)) <= 95 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) > 122 ) {
					if ( 127 <= (*( sm->p)) )
 {						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( sm->p)) - -128));
				if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
			}
		} else {
			_widec = (short)(640 + ((*( sm->p)) - -128));
			if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
		}
	} else {
		_widec = (short)(640 + ((*( sm->p)) - -128));
		if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
	}
	switch( _widec ) {
		case 925: goto st1080;
		case 928: goto st1080;
		case 931: goto st1080;
		case 994: goto st1084;
		case 995: goto st1086;
		case 1007: goto st1088;
		case 1057: goto st1081;
		case 1063: goto st1081;
		case 1067: goto st1081;
		case 1119: goto st1081;
		case 1151: goto tr1310;
	}
	if ( _widec < 1013 ) {
		if ( _widec < 962 ) {
			if ( _widec > 959 ) {
				if ( 960 <= _widec && _widec <= 961 )
					goto st1081;
			} else if ( _widec >= 896 )
				goto tr1302;
		} else if ( _widec > 991 ) {
			if ( _widec > 1006 ) {
				if ( 1008 <= _widec && _widec <= 1012 )
					goto st1090;
			} else if ( _widec >= 992 )
				goto st1083;
		} else
			goto st1082;
	} else if ( _widec > 1023 ) {
		if ( _widec < 1069 ) {
			if ( _widec > 1032 ) {
				if ( 1038 <= _widec && _widec <= 1055 )
					goto tr1310;
			} else if ( _widec >= 1025 )
				goto tr1310;
		} else if ( _widec > 1071 ) {
			if ( _widec < 1089 ) {
				if ( 1072 <= _widec && _widec <= 1081 )
					goto tr1310;
			} else if ( _widec > 1114 ) {
				if ( 1121 <= _widec && _widec <= 1146 )
					goto tr1310;
			} else
				goto tr1310;
		} else
			goto st1081;
	} else
		goto st1081;
	goto tr207;
st1101:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1101;
case 1101:
	_widec = (*( sm->p));
	if ( (*( sm->p)) < 1 ) {
		if ( (*( sm->p)) < -30 ) {
			if ( (*( sm->p)) < -64 ) {
				if ( (*( sm->p)) <= -65 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( sm->p)) > -63 ) {
				if ( (*( sm->p)) > -33 ) {
					if ( -32 <= (*( sm->p)) && (*( sm->p)) <= -31 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) >= -62 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( sm->p)) - -128));
				if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
			}
		} else if ( (*( sm->p)) > -30 ) {
			if ( (*( sm->p)) < -17 ) {
				if ( (*( sm->p)) > -29 ) {
					if ( -28 <= (*( sm->p)) && (*( sm->p)) <= -18 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) >= -29 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( sm->p)) > -17 ) {
				if ( (*( sm->p)) > -12 ) {
					if ( -11 <= (*( sm->p)) && (*( sm->p)) <= -1 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) >= -16 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( sm->p)) - -128));
				if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
			}
		} else {
			_widec = (short)(640 + ((*( sm->p)) - -128));
			if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
		}
	} else if ( (*( sm->p)) > 8 ) {
		if ( (*( sm->p)) < 45 ) {
			if ( (*( sm->p)) < 33 ) {
				if ( 14 <= (*( sm->p)) && (*( sm->p)) <= 31 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( sm->p)) > 33 ) {
				if ( (*( sm->p)) > 39 ) {
					if ( 43 <= (*( sm->p)) && (*( sm->p)) <= 43 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) >= 39 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( sm->p)) - -128));
				if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
			}
		} else if ( (*( sm->p)) > 47 ) {
			if ( (*( sm->p)) < 95 ) {
				if ( (*( sm->p)) > 57 ) {
					if ( 65 <= (*( sm->p)) && (*( sm->p)) <= 90 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) >= 48 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( sm->p)) > 95 ) {
				if ( (*( sm->p)) > 122 ) {
					if ( 127 <= (*( sm->p)) )
 {						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) >= 97 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( sm->p)) - -128));
				if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
			}
		} else {
			_widec = (short)(640 + ((*( sm->p)) - -128));
			if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
		}
	} else {
		_widec = (short)(640 + ((*( sm->p)) - -128));
		if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
	}
	switch( _widec ) {
		case 994: goto st1084;
		case 995: goto st1086;
		case 1007: goto st1088;
		case 1057: goto st1081;
		case 1063: goto st1081;
		case 1067: goto st1081;
		case 1119: goto st1081;
		case 1151: goto tr1310;
	}
	if ( _widec < 1013 ) {
		if ( _widec < 962 ) {
			if ( _widec > 959 ) {
				if ( 960 <= _widec && _widec <= 961 )
					goto st1081;
			} else if ( _widec >= 896 )
				goto st1093;
		} else if ( _widec > 991 ) {
			if ( _widec > 1006 ) {
				if ( 1008 <= _widec && _widec <= 1012 )
					goto st1090;
			} else if ( _widec >= 992 )
				goto st1083;
		} else
			goto st1082;
	} else if ( _widec > 1023 ) {
		if ( _widec < 1069 ) {
			if ( _widec > 1032 ) {
				if ( 1038 <= _widec && _widec <= 1055 )
					goto tr1310;
			} else if ( _widec >= 1025 )
				goto tr1310;
		} else if ( _widec > 1071 ) {
			if ( _widec < 1089 ) {
				if ( 1072 <= _widec && _widec <= 1081 )
					goto tr1310;
			} else if ( _widec > 1114 ) {
				if ( 1121 <= _widec && _widec <= 1146 )
					goto tr1310;
			} else
				goto tr1310;
		} else
			goto st1081;
	} else
		goto st1081;
	goto tr207;
st1102:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1102;
case 1102:
	_widec = (*( sm->p));
	if ( (*( sm->p)) < 33 ) {
		if ( (*( sm->p)) < -28 ) {
			if ( (*( sm->p)) < -32 ) {
				if ( (*( sm->p)) > -63 ) {
					if ( -62 <= (*( sm->p)) && (*( sm->p)) <= -33 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( sm->p)) > -31 ) {
				if ( (*( sm->p)) > -30 ) {
					if ( -29 <= (*( sm->p)) && (*( sm->p)) <= -29 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) >= -30 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( sm->p)) - -128));
				if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
			}
		} else if ( (*( sm->p)) > -18 ) {
			if ( (*( sm->p)) < -11 ) {
				if ( (*( sm->p)) > -17 ) {
					if ( -16 <= (*( sm->p)) && (*( sm->p)) <= -12 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) >= -17 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( sm->p)) > -1 ) {
				if ( (*( sm->p)) > 8 ) {
					if ( 14 <= (*( sm->p)) && (*( sm->p)) <= 31 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) >= 1 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( sm->p)) - -128));
				if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
			}
		} else {
			_widec = (short)(640 + ((*( sm->p)) - -128));
			if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
		}
	} else if ( (*( sm->p)) > 33 ) {
		if ( (*( sm->p)) < 95 ) {
			if ( (*( sm->p)) < 45 ) {
				if ( (*( sm->p)) > 39 ) {
					if ( 43 <= (*( sm->p)) && (*( sm->p)) <= 43 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) >= 39 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( sm->p)) > 47 ) {
				if ( (*( sm->p)) > 57 ) {
					if ( 65 <= (*( sm->p)) && (*( sm->p)) <= 90 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) >= 48 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( sm->p)) - -128));
				if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
			}
		} else if ( (*( sm->p)) > 95 ) {
			if ( (*( sm->p)) < 101 ) {
				if ( (*( sm->p)) > 99 ) {
					if ( 100 <= (*( sm->p)) && (*( sm->p)) <= 100 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) >= 97 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( sm->p)) > 114 ) {
				if ( (*( sm->p)) < 116 ) {
					if ( 115 <= (*( sm->p)) && (*( sm->p)) <= 115 ) {
						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else if ( (*( sm->p)) > 122 ) {
					if ( 127 <= (*( sm->p)) )
 {						_widec = (short)(640 + ((*( sm->p)) - -128));
						if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
					}
				} else {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( sm->p)) - -128));
				if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
			}
		} else {
			_widec = (short)(640 + ((*( sm->p)) - -128));
			if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
		}
	} else {
		_widec = (short)(640 + ((*( sm->p)) - -128));
		if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
	}
	switch( _widec ) {
		case 994: goto st1094;
		case 995: goto st1096;
		case 1007: goto st1098;
		case 1057: goto st1080;
		case 1063: goto st1102;
		case 1067: goto st1080;
		case 1119: goto st1080;
		case 1124: goto st1080;
		case 1139: goto st1080;
		case 1151: goto tr1302;
	}
	if ( _widec < 1025 ) {
		if ( _widec < 992 ) {
			if ( _widec > 961 ) {
				if ( 962 <= _widec && _widec <= 991 )
					goto st1092;
			} else if ( _widec >= 896 )
				goto st1081;
		} else if ( _widec > 1006 ) {
			if ( _widec > 1012 ) {
				if ( 1013 <= _widec && _widec <= 1023 )
					goto st1081;
			} else if ( _widec >= 1008 )
				goto st1101;
		} else
			goto st1093;
	} else if ( _widec > 1032 ) {
		if ( _widec < 1072 ) {
			if ( _widec > 1055 ) {
				if ( 1069 <= _widec && _widec <= 1071 )
					goto st1080;
			} else if ( _widec >= 1038 )
				goto tr1302;
		} else if ( _widec > 1081 ) {
			if ( _widec > 1114 ) {
				if ( 1121 <= _widec && _widec <= 1146 )
					goto tr1302;
			} else if ( _widec >= 1089 )
				goto tr1302;
		} else
			goto tr1302;
	} else
		goto tr1302;
	goto tr207;
tr2140:
#line 71 "ext/dtext/dtext.cpp.rl"
	{ sm->a1 = sm->p; }
	goto st1103;
st1103:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1103;
case 1103:
#line 29701 "ext/dtext/dtext.cpp"
	_widec = (*( sm->p));
	if ( (*( sm->p)) <= -65 ) {
		_widec = (short)(640 + ((*( sm->p)) - -128));
		if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
	}
	if ( 896 <= _widec && _widec <= 959 )
		goto st1079;
	goto tr214;
tr2141:
#line 71 "ext/dtext/dtext.cpp.rl"
	{ sm->a1 = sm->p; }
	goto st1104;
st1104:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1104;
case 1104:
#line 29720 "ext/dtext/dtext.cpp"
	_widec = (*( sm->p));
	if ( (*( sm->p)) < -99 ) {
		if ( (*( sm->p)) <= -100 ) {
			_widec = (short)(640 + ((*( sm->p)) - -128));
			if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
		}
	} else if ( (*( sm->p)) > -99 ) {
		if ( -98 <= (*( sm->p)) && (*( sm->p)) <= -65 ) {
			_widec = (short)(640 + ((*( sm->p)) - -128));
			if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
		}
	} else {
		_widec = (short)(640 + ((*( sm->p)) - -128));
		if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
	}
	if ( _widec == 925 )
		goto st1105;
	if ( 896 <= _widec && _widec <= 959 )
		goto st1079;
	goto tr214;
st1105:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1105;
case 1105:
	_widec = (*( sm->p));
	if ( (*( sm->p)) > -84 ) {
		if ( -82 <= (*( sm->p)) && (*( sm->p)) <= -65 ) {
			_widec = (short)(640 + ((*( sm->p)) - -128));
			if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
		}
	} else {
		_widec = (short)(640 + ((*( sm->p)) - -128));
		if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
	}
	if ( _widec > 940 ) {
		if ( 942 <= _widec && _widec <= 959 )
			goto st1080;
	} else if ( _widec >= 896 )
		goto st1080;
	goto tr214;
tr2142:
#line 71 "ext/dtext/dtext.cpp.rl"
	{ sm->a1 = sm->p; }
	goto st1106;
st1106:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1106;
case 1106:
#line 29779 "ext/dtext/dtext.cpp"
	_widec = (*( sm->p));
	if ( (*( sm->p)) > -128 ) {
		if ( -127 <= (*( sm->p)) && (*( sm->p)) <= -65 ) {
			_widec = (short)(640 + ((*( sm->p)) - -128));
			if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
		}
	} else {
		_widec = (short)(640 + ((*( sm->p)) - -128));
		if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
	}
	if ( _widec == 896 )
		goto st1107;
	if ( 897 <= _widec && _widec <= 959 )
		goto st1079;
	goto tr214;
st1107:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1107;
case 1107:
	_widec = (*( sm->p));
	if ( (*( sm->p)) < -110 ) {
		if ( -125 <= (*( sm->p)) && (*( sm->p)) <= -121 ) {
			_widec = (short)(640 + ((*( sm->p)) - -128));
			if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
		}
	} else if ( (*( sm->p)) > -109 ) {
		if ( -99 <= (*( sm->p)) && (*( sm->p)) <= -65 ) {
			_widec = (short)(640 + ((*( sm->p)) - -128));
			if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
		}
	} else {
		_widec = (short)(640 + ((*( sm->p)) - -128));
		if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
	}
	if ( _widec < 914 ) {
		if ( 899 <= _widec && _widec <= 903 )
			goto st1080;
	} else if ( _widec > 915 ) {
		if ( 925 <= _widec && _widec <= 959 )
			goto st1080;
	} else
		goto st1080;
	goto tr214;
tr2143:
#line 71 "ext/dtext/dtext.cpp.rl"
	{ sm->a1 = sm->p; }
	goto st1108;
st1108:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1108;
case 1108:
#line 29841 "ext/dtext/dtext.cpp"
	_widec = (*( sm->p));
	if ( (*( sm->p)) < -68 ) {
		if ( (*( sm->p)) <= -69 ) {
			_widec = (short)(640 + ((*( sm->p)) - -128));
			if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
		}
	} else if ( (*( sm->p)) > -68 ) {
		if ( (*( sm->p)) > -67 ) {
			if ( -66 <= (*( sm->p)) && (*( sm->p)) <= -65 ) {
				_widec = (short)(640 + ((*( sm->p)) - -128));
				if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
			}
		} else if ( (*( sm->p)) >= -67 ) {
			_widec = (short)(640 + ((*( sm->p)) - -128));
			if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
		}
	} else {
		_widec = (short)(640 + ((*( sm->p)) - -128));
		if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
	}
	switch( _widec ) {
		case 956: goto st1109;
		case 957: goto st1110;
	}
	if ( 896 <= _widec && _widec <= 959 )
		goto st1079;
	goto tr214;
st1109:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1109;
case 1109:
	_widec = (*( sm->p));
	if ( (*( sm->p)) < -118 ) {
		if ( (*( sm->p)) <= -120 ) {
			_widec = (short)(640 + ((*( sm->p)) - -128));
			if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
		}
	} else if ( (*( sm->p)) > -68 ) {
		if ( -66 <= (*( sm->p)) && (*( sm->p)) <= -65 ) {
			_widec = (short)(640 + ((*( sm->p)) - -128));
			if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
		}
	} else {
		_widec = (short)(640 + ((*( sm->p)) - -128));
		if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
	}
	if ( _widec < 906 ) {
		if ( 896 <= _widec && _widec <= 904 )
			goto st1080;
	} else if ( _widec > 956 ) {
		if ( 958 <= _widec && _widec <= 959 )
			goto st1080;
	} else
		goto st1080;
	goto tr214;
st1110:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1110;
case 1110:
	_widec = (*( sm->p));
	if ( (*( sm->p)) < -98 ) {
		if ( (*( sm->p)) <= -100 ) {
			_widec = (short)(640 + ((*( sm->p)) - -128));
			if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
		}
	} else if ( (*( sm->p)) > -97 ) {
		if ( (*( sm->p)) > -94 ) {
			if ( -92 <= (*( sm->p)) && (*( sm->p)) <= -65 ) {
				_widec = (short)(640 + ((*( sm->p)) - -128));
				if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
			}
		} else if ( (*( sm->p)) >= -95 ) {
			_widec = (short)(640 + ((*( sm->p)) - -128));
			if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
		}
	} else {
		_widec = (short)(640 + ((*( sm->p)) - -128));
		if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
	}
	if ( _widec < 926 ) {
		if ( 896 <= _widec && _widec <= 924 )
			goto st1080;
	} else if ( _widec > 927 ) {
		if ( _widec > 930 ) {
			if ( 932 <= _widec && _widec <= 959 )
				goto st1080;
		} else if ( _widec >= 929 )
			goto st1080;
	} else
		goto st1080;
	goto tr214;
tr2144:
#line 71 "ext/dtext/dtext.cpp.rl"
	{ sm->a1 = sm->p; }
	goto st1111;
st1111:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1111;
case 1111:
#line 29963 "ext/dtext/dtext.cpp"
	_widec = (*( sm->p));
	if ( (*( sm->p)) <= -65 ) {
		_widec = (short)(640 + ((*( sm->p)) - -128));
		if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
	}
	if ( 896 <= _widec && _widec <= 959 )
		goto st1103;
	goto tr214;
tr2146:
#line 71 "ext/dtext/dtext.cpp.rl"
	{ sm->a1 = sm->p; }
	goto st1112;
st1112:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1112;
case 1112:
#line 29982 "ext/dtext/dtext.cpp"
	_widec = (*( sm->p));
	if ( (*( sm->p)) < -16 ) {
		if ( (*( sm->p)) < -30 ) {
			if ( (*( sm->p)) > -33 ) {
				if ( -32 <= (*( sm->p)) && (*( sm->p)) <= -31 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( sm->p)) >= -62 ) {
				_widec = (short)(640 + ((*( sm->p)) - -128));
				if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
			}
		} else if ( (*( sm->p)) > -30 ) {
			if ( (*( sm->p)) < -28 ) {
				if ( -29 <= (*( sm->p)) && (*( sm->p)) <= -29 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( sm->p)) > -18 ) {
				if ( -17 <= (*( sm->p)) && (*( sm->p)) <= -17 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( sm->p)) - -128));
				if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
			}
		} else {
			_widec = (short)(640 + ((*( sm->p)) - -128));
			if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
		}
	} else if ( (*( sm->p)) > -12 ) {
		if ( (*( sm->p)) < 48 ) {
			if ( (*( sm->p)) > 8 ) {
				if ( 14 <= (*( sm->p)) && (*( sm->p)) <= 31 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( sm->p)) >= 1 ) {
				_widec = (short)(640 + ((*( sm->p)) - -128));
				if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
			}
		} else if ( (*( sm->p)) > 57 ) {
			if ( (*( sm->p)) < 97 ) {
				if ( 65 <= (*( sm->p)) && (*( sm->p)) <= 90 ) {
					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else if ( (*( sm->p)) > 122 ) {
				if ( 127 <= (*( sm->p)) )
 {					_widec = (short)(640 + ((*( sm->p)) - -128));
					if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
				}
			} else {
				_widec = (short)(640 + ((*( sm->p)) - -128));
				if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
			}
		} else {
			_widec = (short)(640 + ((*( sm->p)) - -128));
			if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
		}
	} else {
		_widec = (short)(640 + ((*( sm->p)) - -128));
		if ( 
#line 83 "ext/dtext/dtext.cpp.rl"
 sm->options.f_mentions  ) _widec += 256;
	}
	switch( _widec ) {
		case 994: goto st1104;
		case 995: goto st1106;
		case 1007: goto st1108;
		case 1151: goto st1080;
	}
	if ( _widec < 1025 ) {
		if ( _widec < 992 ) {
			if ( 962 <= _widec && _widec <= 991 )
				goto st1079;
		} else if ( _widec > 1006 ) {
			if ( 1008 <= _widec && _widec <= 1012 )
				goto st1111;
		} else
			goto st1103;
	} else if ( _widec > 1032 ) {
		if ( _widec < 1072 ) {
			if ( 1038 <= _widec && _widec <= 1055 )
				goto st1080;
		} else if ( _widec > 1081 ) {
			if ( _widec > 1114 ) {
				if ( 1121 <= _widec && _widec <= 1146 )
					goto st1080;
			} else if ( _widec >= 1089 )
				goto st1080;
		} else
			goto st1080;
	} else
		goto st1080;
	goto tr214;
tr1329:
#line 600 "ext/dtext/dtext.cpp.rl"
	{{( sm->p) = ((( sm->te)))-1;}{
    append_html_escaped(sm, (*( sm->p)));
  }}
	goto st1720;
tr1335:
#line 593 "ext/dtext/dtext.cpp.rl"
	{( sm->te) = ( sm->p)+1;{
    dstack_rewind(sm);
    { sm->cs = ( (sm->stack.data()))[--( sm->top)];goto _again;}
  }}
	goto st1720;
tr2148:
#line 600 "ext/dtext/dtext.cpp.rl"
	{( sm->te) = ( sm->p)+1;{
    append_html_escaped(sm, (*( sm->p)));
  }}
	goto st1720;
tr2149:
#line 598 "ext/dtext/dtext.cpp.rl"
	{( sm->te) = ( sm->p)+1;}
	goto st1720;
tr2153:
#line 600 "ext/dtext/dtext.cpp.rl"
	{( sm->te) = ( sm->p);( sm->p)--;{
    append_html_escaped(sm, (*( sm->p)));
  }}
	goto st1720;
st1720:
#line 1 "NONE"
	{( sm->ts) = 0;}
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1720;
case 1720:
#line 1 "NONE"
	{( sm->ts) = ( sm->p);}
#line 30141 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 0: goto tr2149;
		case 10: goto tr2150;
		case 60: goto tr2151;
		case 91: goto tr2152;
	}
	goto tr2148;
tr2150:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
	goto st1721;
st1721:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1721;
case 1721:
#line 30157 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 60: goto st1113;
		case 91: goto st1119;
	}
	goto tr2153;
st1113:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1113;
case 1113:
	if ( (*( sm->p)) == 47 )
		goto st1114;
	goto tr1329;
st1114:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1114;
case 1114:
	switch( (*( sm->p)) ) {
		case 67: goto st1115;
		case 99: goto st1115;
	}
	goto tr1329;
st1115:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1115;
case 1115:
	switch( (*( sm->p)) ) {
		case 79: goto st1116;
		case 111: goto st1116;
	}
	goto tr1329;
st1116:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1116;
case 1116:
	switch( (*( sm->p)) ) {
		case 68: goto st1117;
		case 100: goto st1117;
	}
	goto tr1329;
st1117:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1117;
case 1117:
	switch( (*( sm->p)) ) {
		case 69: goto st1118;
		case 101: goto st1118;
	}
	goto tr1329;
st1118:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1118;
case 1118:
	if ( (*( sm->p)) == 62 )
		goto tr1335;
	goto tr1329;
st1119:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1119;
case 1119:
	if ( (*( sm->p)) == 47 )
		goto st1120;
	goto tr1329;
st1120:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1120;
case 1120:
	switch( (*( sm->p)) ) {
		case 67: goto st1121;
		case 99: goto st1121;
	}
	goto tr1329;
st1121:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1121;
case 1121:
	switch( (*( sm->p)) ) {
		case 79: goto st1122;
		case 111: goto st1122;
	}
	goto tr1329;
st1122:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1122;
case 1122:
	switch( (*( sm->p)) ) {
		case 68: goto st1123;
		case 100: goto st1123;
	}
	goto tr1329;
st1123:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1123;
case 1123:
	switch( (*( sm->p)) ) {
		case 69: goto st1124;
		case 101: goto st1124;
	}
	goto tr1329;
st1124:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1124;
case 1124:
	if ( (*( sm->p)) == 93 )
		goto tr1335;
	goto tr1329;
tr2151:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
	goto st1722;
st1722:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1722;
case 1722:
#line 30271 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) == 47 )
		goto st1114;
	goto tr2153;
tr2152:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
	goto st1723;
st1723:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1723;
case 1723:
#line 30283 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) == 47 )
		goto st1120;
	goto tr2153;
tr1341:
#line 613 "ext/dtext/dtext.cpp.rl"
	{{( sm->p) = ((( sm->te)))-1;}{
    append_html_escaped(sm, (*( sm->p)));
  }}
	goto st1724;
tr1350:
#line 606 "ext/dtext/dtext.cpp.rl"
	{( sm->te) = ( sm->p)+1;{
    dstack_rewind(sm);
    { sm->cs = ( (sm->stack.data()))[--( sm->top)];goto _again;}
  }}
	goto st1724;
tr2156:
#line 613 "ext/dtext/dtext.cpp.rl"
	{( sm->te) = ( sm->p)+1;{
    append_html_escaped(sm, (*( sm->p)));
  }}
	goto st1724;
tr2157:
#line 611 "ext/dtext/dtext.cpp.rl"
	{( sm->te) = ( sm->p)+1;}
	goto st1724;
tr2161:
#line 613 "ext/dtext/dtext.cpp.rl"
	{( sm->te) = ( sm->p);( sm->p)--;{
    append_html_escaped(sm, (*( sm->p)));
  }}
	goto st1724;
st1724:
#line 1 "NONE"
	{( sm->ts) = 0;}
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1724;
case 1724:
#line 1 "NONE"
	{( sm->ts) = ( sm->p);}
#line 30324 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 0: goto tr2157;
		case 10: goto tr2158;
		case 60: goto tr2159;
		case 91: goto tr2160;
	}
	goto tr2156;
tr2158:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
	goto st1725;
st1725:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1725;
case 1725:
#line 30340 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 60: goto st1125;
		case 91: goto st1134;
	}
	goto tr2161;
st1125:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1125;
case 1125:
	if ( (*( sm->p)) == 47 )
		goto st1126;
	goto tr1341;
st1126:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1126;
case 1126:
	switch( (*( sm->p)) ) {
		case 78: goto st1127;
		case 110: goto st1127;
	}
	goto tr1341;
st1127:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1127;
case 1127:
	switch( (*( sm->p)) ) {
		case 79: goto st1128;
		case 111: goto st1128;
	}
	goto tr1341;
st1128:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1128;
case 1128:
	switch( (*( sm->p)) ) {
		case 68: goto st1129;
		case 100: goto st1129;
	}
	goto tr1341;
st1129:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1129;
case 1129:
	switch( (*( sm->p)) ) {
		case 84: goto st1130;
		case 116: goto st1130;
	}
	goto tr1341;
st1130:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1130;
case 1130:
	switch( (*( sm->p)) ) {
		case 69: goto st1131;
		case 101: goto st1131;
	}
	goto tr1341;
st1131:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1131;
case 1131:
	switch( (*( sm->p)) ) {
		case 88: goto st1132;
		case 120: goto st1132;
	}
	goto tr1341;
st1132:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1132;
case 1132:
	switch( (*( sm->p)) ) {
		case 84: goto st1133;
		case 116: goto st1133;
	}
	goto tr1341;
st1133:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1133;
case 1133:
	if ( (*( sm->p)) == 62 )
		goto tr1350;
	goto tr1341;
st1134:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1134;
case 1134:
	if ( (*( sm->p)) == 47 )
		goto st1135;
	goto tr1341;
st1135:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1135;
case 1135:
	switch( (*( sm->p)) ) {
		case 78: goto st1136;
		case 110: goto st1136;
	}
	goto tr1341;
st1136:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1136;
case 1136:
	switch( (*( sm->p)) ) {
		case 79: goto st1137;
		case 111: goto st1137;
	}
	goto tr1341;
st1137:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1137;
case 1137:
	switch( (*( sm->p)) ) {
		case 68: goto st1138;
		case 100: goto st1138;
	}
	goto tr1341;
st1138:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1138;
case 1138:
	switch( (*( sm->p)) ) {
		case 84: goto st1139;
		case 116: goto st1139;
	}
	goto tr1341;
st1139:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1139;
case 1139:
	switch( (*( sm->p)) ) {
		case 69: goto st1140;
		case 101: goto st1140;
	}
	goto tr1341;
st1140:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1140;
case 1140:
	switch( (*( sm->p)) ) {
		case 88: goto st1141;
		case 120: goto st1141;
	}
	goto tr1341;
st1141:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1141;
case 1141:
	switch( (*( sm->p)) ) {
		case 84: goto st1142;
		case 116: goto st1142;
	}
	goto tr1341;
st1142:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1142;
case 1142:
	if ( (*( sm->p)) == 93 )
		goto tr1350;
	goto tr1341;
tr2159:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
	goto st1726;
st1726:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1726;
case 1726:
#line 30508 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) == 47 )
		goto st1126;
	goto tr2161;
tr2160:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
	goto st1727;
st1727:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1727;
case 1727:
#line 30520 "ext/dtext/dtext.cpp"
	if ( (*( sm->p)) == 47 )
		goto st1135;
	goto tr2161;
tr1359:
#line 672 "ext/dtext/dtext.cpp.rl"
	{{( sm->p) = ((( sm->te)))-1;}}
	goto st1728;
tr1369:
#line 623 "ext/dtext/dtext.cpp.rl"
	{( sm->te) = ( sm->p)+1;{
    dstack_close_element(sm, BLOCK_COLGROUP);
  }}
	goto st1728;
tr1377:
#line 666 "ext/dtext/dtext.cpp.rl"
	{( sm->te) = ( sm->p)+1;{
    if (dstack_close_element(sm, BLOCK_TABLE)) {
      { sm->cs = ( (sm->stack.data()))[--( sm->top)];goto _again;}
    }
  }}
	goto st1728;
tr1381:
#line 644 "ext/dtext/dtext.cpp.rl"
	{( sm->te) = ( sm->p)+1;{
    dstack_close_element(sm, BLOCK_TBODY);
  }}
	goto st1728;
tr1385:
#line 636 "ext/dtext/dtext.cpp.rl"
	{( sm->te) = ( sm->p)+1;{
    dstack_close_element(sm, BLOCK_THEAD);
  }}
	goto st1728;
tr1386:
#line 657 "ext/dtext/dtext.cpp.rl"
	{( sm->te) = ( sm->p)+1;{
    dstack_close_element(sm, BLOCK_TR);
  }}
	goto st1728;
tr1390:
#line 627 "ext/dtext/dtext.cpp.rl"
	{( sm->te) = ( sm->p)+1;{
    dstack_open_element(sm, BLOCK_COL, "col", sm->tag_attributes);
    dstack_pop(sm); // XXX [col] has no end tag
  }}
	goto st1728;
tr1405:
#line 87 "ext/dtext/dtext.cpp.rl"
	{ save_tag_attribute(sm, { sm->a1, sm->a2 }, { sm->b1, sm->b2 }); }
#line 627 "ext/dtext/dtext.cpp.rl"
	{( sm->te) = ( sm->p)+1;{
    dstack_open_element(sm, BLOCK_COL, "col", sm->tag_attributes);
    dstack_pop(sm); // XXX [col] has no end tag
  }}
	goto st1728;
tr1410:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ sm->b2 = sm->p; }
#line 87 "ext/dtext/dtext.cpp.rl"
	{ save_tag_attribute(sm, { sm->a1, sm->a2 }, { sm->b1, sm->b2 }); }
#line 627 "ext/dtext/dtext.cpp.rl"
	{( sm->te) = ( sm->p)+1;{
    dstack_open_element(sm, BLOCK_COL, "col", sm->tag_attributes);
    dstack_pop(sm); // XXX [col] has no end tag
  }}
	goto st1728;
tr1416:
#line 619 "ext/dtext/dtext.cpp.rl"
	{( sm->te) = ( sm->p)+1;{
    dstack_open_element(sm, BLOCK_COLGROUP, "colgroup", sm->tag_attributes);
  }}
	goto st1728;
tr1430:
#line 87 "ext/dtext/dtext.cpp.rl"
	{ save_tag_attribute(sm, { sm->a1, sm->a2 }, { sm->b1, sm->b2 }); }
#line 619 "ext/dtext/dtext.cpp.rl"
	{( sm->te) = ( sm->p)+1;{
    dstack_open_element(sm, BLOCK_COLGROUP, "colgroup", sm->tag_attributes);
  }}
	goto st1728;
tr1435:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ sm->b2 = sm->p; }
#line 87 "ext/dtext/dtext.cpp.rl"
	{ save_tag_attribute(sm, { sm->a1, sm->a2 }, { sm->b1, sm->b2 }); }
#line 619 "ext/dtext/dtext.cpp.rl"
	{( sm->te) = ( sm->p)+1;{
    dstack_open_element(sm, BLOCK_COLGROUP, "colgroup", sm->tag_attributes);
  }}
	goto st1728;
tr1444:
#line 640 "ext/dtext/dtext.cpp.rl"
	{( sm->te) = ( sm->p)+1;{
    dstack_open_element(sm, BLOCK_TBODY, "tbody", sm->tag_attributes);
  }}
	goto st1728;
tr1458:
#line 87 "ext/dtext/dtext.cpp.rl"
	{ save_tag_attribute(sm, { sm->a1, sm->a2 }, { sm->b1, sm->b2 }); }
#line 640 "ext/dtext/dtext.cpp.rl"
	{( sm->te) = ( sm->p)+1;{
    dstack_open_element(sm, BLOCK_TBODY, "tbody", sm->tag_attributes);
  }}
	goto st1728;
tr1463:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ sm->b2 = sm->p; }
#line 87 "ext/dtext/dtext.cpp.rl"
	{ save_tag_attribute(sm, { sm->a1, sm->a2 }, { sm->b1, sm->b2 }); }
#line 640 "ext/dtext/dtext.cpp.rl"
	{( sm->te) = ( sm->p)+1;{
    dstack_open_element(sm, BLOCK_TBODY, "tbody", sm->tag_attributes);
  }}
	goto st1728;
tr1465:
#line 661 "ext/dtext/dtext.cpp.rl"
	{( sm->te) = ( sm->p)+1;{
    dstack_open_element(sm, BLOCK_TD, "td", sm->tag_attributes);
    {
  size_t len = sm->stack.size();

  if (len > MAX_STACK_DEPTH) {
    // Should never happen.
    throw DTextError("too many nested elements");
  }

  if (sm->top >= len) {
    g_debug("growing sm->stack %zi", len + 16);
    sm->stack.resize(len + 16, 0);
  }
{( (sm->stack.data()))[( sm->top)++] = 1728;goto st1389;}}
  }}
	goto st1728;
tr1479:
#line 87 "ext/dtext/dtext.cpp.rl"
	{ save_tag_attribute(sm, { sm->a1, sm->a2 }, { sm->b1, sm->b2 }); }
#line 661 "ext/dtext/dtext.cpp.rl"
	{( sm->te) = ( sm->p)+1;{
    dstack_open_element(sm, BLOCK_TD, "td", sm->tag_attributes);
    {
  size_t len = sm->stack.size();

  if (len > MAX_STACK_DEPTH) {
    // Should never happen.
    throw DTextError("too many nested elements");
  }

  if (sm->top >= len) {
    g_debug("growing sm->stack %zi", len + 16);
    sm->stack.resize(len + 16, 0);
  }
{( (sm->stack.data()))[( sm->top)++] = 1728;goto st1389;}}
  }}
	goto st1728;
tr1484:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ sm->b2 = sm->p; }
#line 87 "ext/dtext/dtext.cpp.rl"
	{ save_tag_attribute(sm, { sm->a1, sm->a2 }, { sm->b1, sm->b2 }); }
#line 661 "ext/dtext/dtext.cpp.rl"
	{( sm->te) = ( sm->p)+1;{
    dstack_open_element(sm, BLOCK_TD, "td", sm->tag_attributes);
    {
  size_t len = sm->stack.size();

  if (len > MAX_STACK_DEPTH) {
    // Should never happen.
    throw DTextError("too many nested elements");
  }

  if (sm->top >= len) {
    g_debug("growing sm->stack %zi", len + 16);
    sm->stack.resize(len + 16, 0);
  }
{( (sm->stack.data()))[( sm->top)++] = 1728;goto st1389;}}
  }}
	goto st1728;
tr1486:
#line 648 "ext/dtext/dtext.cpp.rl"
	{( sm->te) = ( sm->p)+1;{
    dstack_open_element(sm, BLOCK_TH, "th", sm->tag_attributes);
    {
  size_t len = sm->stack.size();

  if (len > MAX_STACK_DEPTH) {
    // Should never happen.
    throw DTextError("too many nested elements");
  }

  if (sm->top >= len) {
    g_debug("growing sm->stack %zi", len + 16);
    sm->stack.resize(len + 16, 0);
  }
{( (sm->stack.data()))[( sm->top)++] = 1728;goto st1389;}}
  }}
	goto st1728;
tr1501:
#line 87 "ext/dtext/dtext.cpp.rl"
	{ save_tag_attribute(sm, { sm->a1, sm->a2 }, { sm->b1, sm->b2 }); }
#line 648 "ext/dtext/dtext.cpp.rl"
	{( sm->te) = ( sm->p)+1;{
    dstack_open_element(sm, BLOCK_TH, "th", sm->tag_attributes);
    {
  size_t len = sm->stack.size();

  if (len > MAX_STACK_DEPTH) {
    // Should never happen.
    throw DTextError("too many nested elements");
  }

  if (sm->top >= len) {
    g_debug("growing sm->stack %zi", len + 16);
    sm->stack.resize(len + 16, 0);
  }
{( (sm->stack.data()))[( sm->top)++] = 1728;goto st1389;}}
  }}
	goto st1728;
tr1506:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ sm->b2 = sm->p; }
#line 87 "ext/dtext/dtext.cpp.rl"
	{ save_tag_attribute(sm, { sm->a1, sm->a2 }, { sm->b1, sm->b2 }); }
#line 648 "ext/dtext/dtext.cpp.rl"
	{( sm->te) = ( sm->p)+1;{
    dstack_open_element(sm, BLOCK_TH, "th", sm->tag_attributes);
    {
  size_t len = sm->stack.size();

  if (len > MAX_STACK_DEPTH) {
    // Should never happen.
    throw DTextError("too many nested elements");
  }

  if (sm->top >= len) {
    g_debug("growing sm->stack %zi", len + 16);
    sm->stack.resize(len + 16, 0);
  }
{( (sm->stack.data()))[( sm->top)++] = 1728;goto st1389;}}
  }}
	goto st1728;
tr1510:
#line 632 "ext/dtext/dtext.cpp.rl"
	{( sm->te) = ( sm->p)+1;{
    dstack_open_element(sm, BLOCK_THEAD, "thead", sm->tag_attributes);
  }}
	goto st1728;
tr1524:
#line 87 "ext/dtext/dtext.cpp.rl"
	{ save_tag_attribute(sm, { sm->a1, sm->a2 }, { sm->b1, sm->b2 }); }
#line 632 "ext/dtext/dtext.cpp.rl"
	{( sm->te) = ( sm->p)+1;{
    dstack_open_element(sm, BLOCK_THEAD, "thead", sm->tag_attributes);
  }}
	goto st1728;
tr1529:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ sm->b2 = sm->p; }
#line 87 "ext/dtext/dtext.cpp.rl"
	{ save_tag_attribute(sm, { sm->a1, sm->a2 }, { sm->b1, sm->b2 }); }
#line 632 "ext/dtext/dtext.cpp.rl"
	{( sm->te) = ( sm->p)+1;{
    dstack_open_element(sm, BLOCK_THEAD, "thead", sm->tag_attributes);
  }}
	goto st1728;
tr1531:
#line 653 "ext/dtext/dtext.cpp.rl"
	{( sm->te) = ( sm->p)+1;{
    dstack_open_element(sm, BLOCK_TR, "tr", sm->tag_attributes);
  }}
	goto st1728;
tr1545:
#line 87 "ext/dtext/dtext.cpp.rl"
	{ save_tag_attribute(sm, { sm->a1, sm->a2 }, { sm->b1, sm->b2 }); }
#line 653 "ext/dtext/dtext.cpp.rl"
	{( sm->te) = ( sm->p)+1;{
    dstack_open_element(sm, BLOCK_TR, "tr", sm->tag_attributes);
  }}
	goto st1728;
tr1550:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ sm->b2 = sm->p; }
#line 87 "ext/dtext/dtext.cpp.rl"
	{ save_tag_attribute(sm, { sm->a1, sm->a2 }, { sm->b1, sm->b2 }); }
#line 653 "ext/dtext/dtext.cpp.rl"
	{( sm->te) = ( sm->p)+1;{
    dstack_open_element(sm, BLOCK_TR, "tr", sm->tag_attributes);
  }}
	goto st1728;
tr2164:
#line 672 "ext/dtext/dtext.cpp.rl"
	{( sm->te) = ( sm->p)+1;}
	goto st1728;
tr2167:
#line 672 "ext/dtext/dtext.cpp.rl"
	{( sm->te) = ( sm->p);( sm->p)--;}
	goto st1728;
st1728:
#line 1 "NONE"
	{( sm->ts) = 0;}
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1728;
case 1728:
#line 1 "NONE"
	{( sm->ts) = ( sm->p);}
#line 30825 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 60: goto tr2165;
		case 91: goto tr2166;
	}
	goto tr2164;
tr2165:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
	goto st1729;
st1729:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1729;
case 1729:
#line 30839 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 47: goto st1143;
		case 67: goto st1166;
		case 84: goto st1194;
		case 99: goto st1166;
		case 116: goto st1194;
	}
	goto tr2167;
st1143:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1143;
case 1143:
	switch( (*( sm->p)) ) {
		case 67: goto st1144;
		case 84: goto st1152;
		case 99: goto st1144;
		case 116: goto st1152;
	}
	goto tr1359;
st1144:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1144;
case 1144:
	switch( (*( sm->p)) ) {
		case 79: goto st1145;
		case 111: goto st1145;
	}
	goto tr1359;
st1145:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1145;
case 1145:
	switch( (*( sm->p)) ) {
		case 76: goto st1146;
		case 108: goto st1146;
	}
	goto tr1359;
st1146:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1146;
case 1146:
	switch( (*( sm->p)) ) {
		case 71: goto st1147;
		case 103: goto st1147;
	}
	goto tr1359;
st1147:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1147;
case 1147:
	switch( (*( sm->p)) ) {
		case 82: goto st1148;
		case 114: goto st1148;
	}
	goto tr1359;
st1148:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1148;
case 1148:
	switch( (*( sm->p)) ) {
		case 79: goto st1149;
		case 111: goto st1149;
	}
	goto tr1359;
st1149:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1149;
case 1149:
	switch( (*( sm->p)) ) {
		case 85: goto st1150;
		case 117: goto st1150;
	}
	goto tr1359;
st1150:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1150;
case 1150:
	switch( (*( sm->p)) ) {
		case 80: goto st1151;
		case 112: goto st1151;
	}
	goto tr1359;
st1151:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1151;
case 1151:
	if ( (*( sm->p)) == 62 )
		goto tr1369;
	goto tr1359;
st1152:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1152;
case 1152:
	switch( (*( sm->p)) ) {
		case 65: goto st1153;
		case 66: goto st1157;
		case 72: goto st1161;
		case 82: goto st1165;
		case 97: goto st1153;
		case 98: goto st1157;
		case 104: goto st1161;
		case 114: goto st1165;
	}
	goto tr1359;
st1153:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1153;
case 1153:
	switch( (*( sm->p)) ) {
		case 66: goto st1154;
		case 98: goto st1154;
	}
	goto tr1359;
st1154:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1154;
case 1154:
	switch( (*( sm->p)) ) {
		case 76: goto st1155;
		case 108: goto st1155;
	}
	goto tr1359;
st1155:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1155;
case 1155:
	switch( (*( sm->p)) ) {
		case 69: goto st1156;
		case 101: goto st1156;
	}
	goto tr1359;
st1156:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1156;
case 1156:
	if ( (*( sm->p)) == 62 )
		goto tr1377;
	goto tr1359;
st1157:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1157;
case 1157:
	switch( (*( sm->p)) ) {
		case 79: goto st1158;
		case 111: goto st1158;
	}
	goto tr1359;
st1158:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1158;
case 1158:
	switch( (*( sm->p)) ) {
		case 68: goto st1159;
		case 100: goto st1159;
	}
	goto tr1359;
st1159:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1159;
case 1159:
	switch( (*( sm->p)) ) {
		case 89: goto st1160;
		case 121: goto st1160;
	}
	goto tr1359;
st1160:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1160;
case 1160:
	if ( (*( sm->p)) == 62 )
		goto tr1381;
	goto tr1359;
st1161:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1161;
case 1161:
	switch( (*( sm->p)) ) {
		case 69: goto st1162;
		case 101: goto st1162;
	}
	goto tr1359;
st1162:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1162;
case 1162:
	switch( (*( sm->p)) ) {
		case 65: goto st1163;
		case 97: goto st1163;
	}
	goto tr1359;
st1163:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1163;
case 1163:
	switch( (*( sm->p)) ) {
		case 68: goto st1164;
		case 100: goto st1164;
	}
	goto tr1359;
st1164:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1164;
case 1164:
	if ( (*( sm->p)) == 62 )
		goto tr1385;
	goto tr1359;
st1165:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1165;
case 1165:
	if ( (*( sm->p)) == 62 )
		goto tr1386;
	goto tr1359;
st1166:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1166;
case 1166:
	switch( (*( sm->p)) ) {
		case 79: goto st1167;
		case 111: goto st1167;
	}
	goto tr1359;
st1167:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1167;
case 1167:
	switch( (*( sm->p)) ) {
		case 76: goto st1168;
		case 108: goto st1168;
	}
	goto tr1359;
st1168:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1168;
case 1168:
	switch( (*( sm->p)) ) {
		case 9: goto st1169;
		case 32: goto st1169;
		case 62: goto tr1390;
		case 71: goto st1179;
		case 103: goto st1179;
	}
	goto tr1359;
tr1404:
#line 87 "ext/dtext/dtext.cpp.rl"
	{ save_tag_attribute(sm, { sm->a1, sm->a2 }, { sm->b1, sm->b2 }); }
	goto st1169;
tr1408:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ sm->b2 = sm->p; }
#line 87 "ext/dtext/dtext.cpp.rl"
	{ save_tag_attribute(sm, { sm->a1, sm->a2 }, { sm->b1, sm->b2 }); }
	goto st1169;
st1169:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1169;
case 1169:
#line 31097 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 9: goto st1169;
		case 32: goto st1169;
	}
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1392;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto tr1392;
	} else
		goto tr1392;
	goto tr1359;
tr1392:
#line 71 "ext/dtext/dtext.cpp.rl"
	{ sm->a1 = sm->p; }
	goto st1170;
st1170:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1170;
case 1170:
#line 31119 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 9: goto tr1393;
		case 32: goto tr1393;
		case 61: goto tr1395;
	}
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st1170;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st1170;
	} else
		goto st1170;
	goto tr1359;
tr1393:
#line 72 "ext/dtext/dtext.cpp.rl"
	{ sm->a2 = sm->p; }
	goto st1171;
st1171:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1171;
case 1171:
#line 31142 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 9: goto st1171;
		case 32: goto st1171;
		case 61: goto st1172;
	}
	goto tr1359;
tr1395:
#line 72 "ext/dtext/dtext.cpp.rl"
	{ sm->a2 = sm->p; }
	goto st1172;
st1172:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1172;
case 1172:
#line 31157 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 9: goto st1172;
		case 32: goto st1172;
		case 34: goto st1173;
		case 39: goto st1176;
	}
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1400;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto tr1400;
	} else
		goto tr1400;
	goto tr1359;
st1173:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1173;
case 1173:
	switch( (*( sm->p)) ) {
		case 0: goto tr1359;
		case 10: goto tr1359;
		case 13: goto tr1359;
	}
	goto tr1401;
tr1401:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ sm->b1 = sm->p; }
	goto st1174;
st1174:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1174;
case 1174:
#line 31191 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 0: goto tr1359;
		case 10: goto tr1359;
		case 13: goto tr1359;
		case 34: goto tr1403;
	}
	goto st1174;
tr1403:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ sm->b2 = sm->p; }
	goto st1175;
st1175:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1175;
case 1175:
#line 31207 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 9: goto tr1404;
		case 32: goto tr1404;
		case 62: goto tr1405;
	}
	goto tr1359;
st1176:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1176;
case 1176:
	switch( (*( sm->p)) ) {
		case 0: goto tr1359;
		case 10: goto tr1359;
		case 13: goto tr1359;
	}
	goto tr1406;
tr1406:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ sm->b1 = sm->p; }
	goto st1177;
st1177:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1177;
case 1177:
#line 31232 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 0: goto tr1359;
		case 10: goto tr1359;
		case 13: goto tr1359;
		case 39: goto tr1403;
	}
	goto st1177;
tr1400:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ sm->b1 = sm->p; }
	goto st1178;
st1178:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1178;
case 1178:
#line 31248 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 9: goto tr1408;
		case 32: goto tr1408;
		case 62: goto tr1410;
	}
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st1178;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st1178;
	} else
		goto st1178;
	goto tr1359;
st1179:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1179;
case 1179:
	switch( (*( sm->p)) ) {
		case 82: goto st1180;
		case 114: goto st1180;
	}
	goto tr1359;
st1180:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1180;
case 1180:
	switch( (*( sm->p)) ) {
		case 79: goto st1181;
		case 111: goto st1181;
	}
	goto tr1359;
st1181:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1181;
case 1181:
	switch( (*( sm->p)) ) {
		case 85: goto st1182;
		case 117: goto st1182;
	}
	goto tr1359;
st1182:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1182;
case 1182:
	switch( (*( sm->p)) ) {
		case 80: goto st1183;
		case 112: goto st1183;
	}
	goto tr1359;
st1183:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1183;
case 1183:
	switch( (*( sm->p)) ) {
		case 9: goto st1184;
		case 32: goto st1184;
		case 62: goto tr1416;
	}
	goto tr1359;
tr1429:
#line 87 "ext/dtext/dtext.cpp.rl"
	{ save_tag_attribute(sm, { sm->a1, sm->a2 }, { sm->b1, sm->b2 }); }
	goto st1184;
tr1433:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ sm->b2 = sm->p; }
#line 87 "ext/dtext/dtext.cpp.rl"
	{ save_tag_attribute(sm, { sm->a1, sm->a2 }, { sm->b1, sm->b2 }); }
	goto st1184;
st1184:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1184;
case 1184:
#line 31323 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 9: goto st1184;
		case 32: goto st1184;
	}
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1417;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto tr1417;
	} else
		goto tr1417;
	goto tr1359;
tr1417:
#line 71 "ext/dtext/dtext.cpp.rl"
	{ sm->a1 = sm->p; }
	goto st1185;
st1185:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1185;
case 1185:
#line 31345 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 9: goto tr1418;
		case 32: goto tr1418;
		case 61: goto tr1420;
	}
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st1185;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st1185;
	} else
		goto st1185;
	goto tr1359;
tr1418:
#line 72 "ext/dtext/dtext.cpp.rl"
	{ sm->a2 = sm->p; }
	goto st1186;
st1186:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1186;
case 1186:
#line 31368 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 9: goto st1186;
		case 32: goto st1186;
		case 61: goto st1187;
	}
	goto tr1359;
tr1420:
#line 72 "ext/dtext/dtext.cpp.rl"
	{ sm->a2 = sm->p; }
	goto st1187;
st1187:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1187;
case 1187:
#line 31383 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 9: goto st1187;
		case 32: goto st1187;
		case 34: goto st1188;
		case 39: goto st1191;
	}
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1425;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto tr1425;
	} else
		goto tr1425;
	goto tr1359;
st1188:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1188;
case 1188:
	switch( (*( sm->p)) ) {
		case 0: goto tr1359;
		case 10: goto tr1359;
		case 13: goto tr1359;
	}
	goto tr1426;
tr1426:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ sm->b1 = sm->p; }
	goto st1189;
st1189:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1189;
case 1189:
#line 31417 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 0: goto tr1359;
		case 10: goto tr1359;
		case 13: goto tr1359;
		case 34: goto tr1428;
	}
	goto st1189;
tr1428:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ sm->b2 = sm->p; }
	goto st1190;
st1190:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1190;
case 1190:
#line 31433 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 9: goto tr1429;
		case 32: goto tr1429;
		case 62: goto tr1430;
	}
	goto tr1359;
st1191:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1191;
case 1191:
	switch( (*( sm->p)) ) {
		case 0: goto tr1359;
		case 10: goto tr1359;
		case 13: goto tr1359;
	}
	goto tr1431;
tr1431:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ sm->b1 = sm->p; }
	goto st1192;
st1192:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1192;
case 1192:
#line 31458 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 0: goto tr1359;
		case 10: goto tr1359;
		case 13: goto tr1359;
		case 39: goto tr1428;
	}
	goto st1192;
tr1425:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ sm->b1 = sm->p; }
	goto st1193;
st1193:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1193;
case 1193:
#line 31474 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 9: goto tr1433;
		case 32: goto tr1433;
		case 62: goto tr1435;
	}
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st1193;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st1193;
	} else
		goto st1193;
	goto tr1359;
st1194:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1194;
case 1194:
	switch( (*( sm->p)) ) {
		case 66: goto st1195;
		case 68: goto st1209;
		case 72: goto st1220;
		case 82: goto st1244;
		case 98: goto st1195;
		case 100: goto st1209;
		case 104: goto st1220;
		case 114: goto st1244;
	}
	goto tr1359;
st1195:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1195;
case 1195:
	switch( (*( sm->p)) ) {
		case 79: goto st1196;
		case 111: goto st1196;
	}
	goto tr1359;
st1196:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1196;
case 1196:
	switch( (*( sm->p)) ) {
		case 68: goto st1197;
		case 100: goto st1197;
	}
	goto tr1359;
st1197:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1197;
case 1197:
	switch( (*( sm->p)) ) {
		case 89: goto st1198;
		case 121: goto st1198;
	}
	goto tr1359;
st1198:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1198;
case 1198:
	switch( (*( sm->p)) ) {
		case 9: goto st1199;
		case 32: goto st1199;
		case 62: goto tr1444;
	}
	goto tr1359;
tr1457:
#line 87 "ext/dtext/dtext.cpp.rl"
	{ save_tag_attribute(sm, { sm->a1, sm->a2 }, { sm->b1, sm->b2 }); }
	goto st1199;
tr1461:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ sm->b2 = sm->p; }
#line 87 "ext/dtext/dtext.cpp.rl"
	{ save_tag_attribute(sm, { sm->a1, sm->a2 }, { sm->b1, sm->b2 }); }
	goto st1199;
st1199:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1199;
case 1199:
#line 31555 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 9: goto st1199;
		case 32: goto st1199;
	}
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1445;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto tr1445;
	} else
		goto tr1445;
	goto tr1359;
tr1445:
#line 71 "ext/dtext/dtext.cpp.rl"
	{ sm->a1 = sm->p; }
	goto st1200;
st1200:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1200;
case 1200:
#line 31577 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 9: goto tr1446;
		case 32: goto tr1446;
		case 61: goto tr1448;
	}
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st1200;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st1200;
	} else
		goto st1200;
	goto tr1359;
tr1446:
#line 72 "ext/dtext/dtext.cpp.rl"
	{ sm->a2 = sm->p; }
	goto st1201;
st1201:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1201;
case 1201:
#line 31600 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 9: goto st1201;
		case 32: goto st1201;
		case 61: goto st1202;
	}
	goto tr1359;
tr1448:
#line 72 "ext/dtext/dtext.cpp.rl"
	{ sm->a2 = sm->p; }
	goto st1202;
st1202:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1202;
case 1202:
#line 31615 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 9: goto st1202;
		case 32: goto st1202;
		case 34: goto st1203;
		case 39: goto st1206;
	}
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1453;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto tr1453;
	} else
		goto tr1453;
	goto tr1359;
st1203:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1203;
case 1203:
	switch( (*( sm->p)) ) {
		case 0: goto tr1359;
		case 10: goto tr1359;
		case 13: goto tr1359;
	}
	goto tr1454;
tr1454:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ sm->b1 = sm->p; }
	goto st1204;
st1204:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1204;
case 1204:
#line 31649 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 0: goto tr1359;
		case 10: goto tr1359;
		case 13: goto tr1359;
		case 34: goto tr1456;
	}
	goto st1204;
tr1456:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ sm->b2 = sm->p; }
	goto st1205;
st1205:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1205;
case 1205:
#line 31665 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 9: goto tr1457;
		case 32: goto tr1457;
		case 62: goto tr1458;
	}
	goto tr1359;
st1206:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1206;
case 1206:
	switch( (*( sm->p)) ) {
		case 0: goto tr1359;
		case 10: goto tr1359;
		case 13: goto tr1359;
	}
	goto tr1459;
tr1459:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ sm->b1 = sm->p; }
	goto st1207;
st1207:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1207;
case 1207:
#line 31690 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 0: goto tr1359;
		case 10: goto tr1359;
		case 13: goto tr1359;
		case 39: goto tr1456;
	}
	goto st1207;
tr1453:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ sm->b1 = sm->p; }
	goto st1208;
st1208:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1208;
case 1208:
#line 31706 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 9: goto tr1461;
		case 32: goto tr1461;
		case 62: goto tr1463;
	}
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st1208;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st1208;
	} else
		goto st1208;
	goto tr1359;
st1209:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1209;
case 1209:
	switch( (*( sm->p)) ) {
		case 9: goto st1210;
		case 32: goto st1210;
		case 62: goto tr1465;
	}
	goto tr1359;
tr1478:
#line 87 "ext/dtext/dtext.cpp.rl"
	{ save_tag_attribute(sm, { sm->a1, sm->a2 }, { sm->b1, sm->b2 }); }
	goto st1210;
tr1482:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ sm->b2 = sm->p; }
#line 87 "ext/dtext/dtext.cpp.rl"
	{ save_tag_attribute(sm, { sm->a1, sm->a2 }, { sm->b1, sm->b2 }); }
	goto st1210;
st1210:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1210;
case 1210:
#line 31745 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 9: goto st1210;
		case 32: goto st1210;
	}
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1466;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto tr1466;
	} else
		goto tr1466;
	goto tr1359;
tr1466:
#line 71 "ext/dtext/dtext.cpp.rl"
	{ sm->a1 = sm->p; }
	goto st1211;
st1211:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1211;
case 1211:
#line 31767 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 9: goto tr1467;
		case 32: goto tr1467;
		case 61: goto tr1469;
	}
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st1211;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st1211;
	} else
		goto st1211;
	goto tr1359;
tr1467:
#line 72 "ext/dtext/dtext.cpp.rl"
	{ sm->a2 = sm->p; }
	goto st1212;
st1212:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1212;
case 1212:
#line 31790 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 9: goto st1212;
		case 32: goto st1212;
		case 61: goto st1213;
	}
	goto tr1359;
tr1469:
#line 72 "ext/dtext/dtext.cpp.rl"
	{ sm->a2 = sm->p; }
	goto st1213;
st1213:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1213;
case 1213:
#line 31805 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 9: goto st1213;
		case 32: goto st1213;
		case 34: goto st1214;
		case 39: goto st1217;
	}
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1474;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto tr1474;
	} else
		goto tr1474;
	goto tr1359;
st1214:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1214;
case 1214:
	switch( (*( sm->p)) ) {
		case 0: goto tr1359;
		case 10: goto tr1359;
		case 13: goto tr1359;
	}
	goto tr1475;
tr1475:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ sm->b1 = sm->p; }
	goto st1215;
st1215:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1215;
case 1215:
#line 31839 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 0: goto tr1359;
		case 10: goto tr1359;
		case 13: goto tr1359;
		case 34: goto tr1477;
	}
	goto st1215;
tr1477:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ sm->b2 = sm->p; }
	goto st1216;
st1216:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1216;
case 1216:
#line 31855 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 9: goto tr1478;
		case 32: goto tr1478;
		case 62: goto tr1479;
	}
	goto tr1359;
st1217:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1217;
case 1217:
	switch( (*( sm->p)) ) {
		case 0: goto tr1359;
		case 10: goto tr1359;
		case 13: goto tr1359;
	}
	goto tr1480;
tr1480:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ sm->b1 = sm->p; }
	goto st1218;
st1218:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1218;
case 1218:
#line 31880 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 0: goto tr1359;
		case 10: goto tr1359;
		case 13: goto tr1359;
		case 39: goto tr1477;
	}
	goto st1218;
tr1474:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ sm->b1 = sm->p; }
	goto st1219;
st1219:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1219;
case 1219:
#line 31896 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 9: goto tr1482;
		case 32: goto tr1482;
		case 62: goto tr1484;
	}
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st1219;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st1219;
	} else
		goto st1219;
	goto tr1359;
st1220:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1220;
case 1220:
	switch( (*( sm->p)) ) {
		case 9: goto st1221;
		case 32: goto st1221;
		case 62: goto tr1486;
		case 69: goto st1231;
		case 101: goto st1231;
	}
	goto tr1359;
tr1500:
#line 87 "ext/dtext/dtext.cpp.rl"
	{ save_tag_attribute(sm, { sm->a1, sm->a2 }, { sm->b1, sm->b2 }); }
	goto st1221;
tr1504:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ sm->b2 = sm->p; }
#line 87 "ext/dtext/dtext.cpp.rl"
	{ save_tag_attribute(sm, { sm->a1, sm->a2 }, { sm->b1, sm->b2 }); }
	goto st1221;
st1221:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1221;
case 1221:
#line 31937 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 9: goto st1221;
		case 32: goto st1221;
	}
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1488;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto tr1488;
	} else
		goto tr1488;
	goto tr1359;
tr1488:
#line 71 "ext/dtext/dtext.cpp.rl"
	{ sm->a1 = sm->p; }
	goto st1222;
st1222:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1222;
case 1222:
#line 31959 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 9: goto tr1489;
		case 32: goto tr1489;
		case 61: goto tr1491;
	}
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st1222;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st1222;
	} else
		goto st1222;
	goto tr1359;
tr1489:
#line 72 "ext/dtext/dtext.cpp.rl"
	{ sm->a2 = sm->p; }
	goto st1223;
st1223:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1223;
case 1223:
#line 31982 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 9: goto st1223;
		case 32: goto st1223;
		case 61: goto st1224;
	}
	goto tr1359;
tr1491:
#line 72 "ext/dtext/dtext.cpp.rl"
	{ sm->a2 = sm->p; }
	goto st1224;
st1224:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1224;
case 1224:
#line 31997 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 9: goto st1224;
		case 32: goto st1224;
		case 34: goto st1225;
		case 39: goto st1228;
	}
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1496;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto tr1496;
	} else
		goto tr1496;
	goto tr1359;
st1225:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1225;
case 1225:
	switch( (*( sm->p)) ) {
		case 0: goto tr1359;
		case 10: goto tr1359;
		case 13: goto tr1359;
	}
	goto tr1497;
tr1497:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ sm->b1 = sm->p; }
	goto st1226;
st1226:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1226;
case 1226:
#line 32031 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 0: goto tr1359;
		case 10: goto tr1359;
		case 13: goto tr1359;
		case 34: goto tr1499;
	}
	goto st1226;
tr1499:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ sm->b2 = sm->p; }
	goto st1227;
st1227:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1227;
case 1227:
#line 32047 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 9: goto tr1500;
		case 32: goto tr1500;
		case 62: goto tr1501;
	}
	goto tr1359;
st1228:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1228;
case 1228:
	switch( (*( sm->p)) ) {
		case 0: goto tr1359;
		case 10: goto tr1359;
		case 13: goto tr1359;
	}
	goto tr1502;
tr1502:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ sm->b1 = sm->p; }
	goto st1229;
st1229:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1229;
case 1229:
#line 32072 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 0: goto tr1359;
		case 10: goto tr1359;
		case 13: goto tr1359;
		case 39: goto tr1499;
	}
	goto st1229;
tr1496:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ sm->b1 = sm->p; }
	goto st1230;
st1230:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1230;
case 1230:
#line 32088 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 9: goto tr1504;
		case 32: goto tr1504;
		case 62: goto tr1506;
	}
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st1230;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st1230;
	} else
		goto st1230;
	goto tr1359;
st1231:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1231;
case 1231:
	switch( (*( sm->p)) ) {
		case 65: goto st1232;
		case 97: goto st1232;
	}
	goto tr1359;
st1232:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1232;
case 1232:
	switch( (*( sm->p)) ) {
		case 68: goto st1233;
		case 100: goto st1233;
	}
	goto tr1359;
st1233:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1233;
case 1233:
	switch( (*( sm->p)) ) {
		case 9: goto st1234;
		case 32: goto st1234;
		case 62: goto tr1510;
	}
	goto tr1359;
tr1523:
#line 87 "ext/dtext/dtext.cpp.rl"
	{ save_tag_attribute(sm, { sm->a1, sm->a2 }, { sm->b1, sm->b2 }); }
	goto st1234;
tr1527:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ sm->b2 = sm->p; }
#line 87 "ext/dtext/dtext.cpp.rl"
	{ save_tag_attribute(sm, { sm->a1, sm->a2 }, { sm->b1, sm->b2 }); }
	goto st1234;
st1234:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1234;
case 1234:
#line 32145 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 9: goto st1234;
		case 32: goto st1234;
	}
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1511;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto tr1511;
	} else
		goto tr1511;
	goto tr1359;
tr1511:
#line 71 "ext/dtext/dtext.cpp.rl"
	{ sm->a1 = sm->p; }
	goto st1235;
st1235:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1235;
case 1235:
#line 32167 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 9: goto tr1512;
		case 32: goto tr1512;
		case 61: goto tr1514;
	}
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st1235;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st1235;
	} else
		goto st1235;
	goto tr1359;
tr1512:
#line 72 "ext/dtext/dtext.cpp.rl"
	{ sm->a2 = sm->p; }
	goto st1236;
st1236:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1236;
case 1236:
#line 32190 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 9: goto st1236;
		case 32: goto st1236;
		case 61: goto st1237;
	}
	goto tr1359;
tr1514:
#line 72 "ext/dtext/dtext.cpp.rl"
	{ sm->a2 = sm->p; }
	goto st1237;
st1237:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1237;
case 1237:
#line 32205 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 9: goto st1237;
		case 32: goto st1237;
		case 34: goto st1238;
		case 39: goto st1241;
	}
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1519;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto tr1519;
	} else
		goto tr1519;
	goto tr1359;
st1238:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1238;
case 1238:
	switch( (*( sm->p)) ) {
		case 0: goto tr1359;
		case 10: goto tr1359;
		case 13: goto tr1359;
	}
	goto tr1520;
tr1520:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ sm->b1 = sm->p; }
	goto st1239;
st1239:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1239;
case 1239:
#line 32239 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 0: goto tr1359;
		case 10: goto tr1359;
		case 13: goto tr1359;
		case 34: goto tr1522;
	}
	goto st1239;
tr1522:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ sm->b2 = sm->p; }
	goto st1240;
st1240:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1240;
case 1240:
#line 32255 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 9: goto tr1523;
		case 32: goto tr1523;
		case 62: goto tr1524;
	}
	goto tr1359;
st1241:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1241;
case 1241:
	switch( (*( sm->p)) ) {
		case 0: goto tr1359;
		case 10: goto tr1359;
		case 13: goto tr1359;
	}
	goto tr1525;
tr1525:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ sm->b1 = sm->p; }
	goto st1242;
st1242:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1242;
case 1242:
#line 32280 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 0: goto tr1359;
		case 10: goto tr1359;
		case 13: goto tr1359;
		case 39: goto tr1522;
	}
	goto st1242;
tr1519:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ sm->b1 = sm->p; }
	goto st1243;
st1243:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1243;
case 1243:
#line 32296 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 9: goto tr1527;
		case 32: goto tr1527;
		case 62: goto tr1529;
	}
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st1243;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st1243;
	} else
		goto st1243;
	goto tr1359;
st1244:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1244;
case 1244:
	switch( (*( sm->p)) ) {
		case 9: goto st1245;
		case 32: goto st1245;
		case 62: goto tr1531;
	}
	goto tr1359;
tr1544:
#line 87 "ext/dtext/dtext.cpp.rl"
	{ save_tag_attribute(sm, { sm->a1, sm->a2 }, { sm->b1, sm->b2 }); }
	goto st1245;
tr1548:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ sm->b2 = sm->p; }
#line 87 "ext/dtext/dtext.cpp.rl"
	{ save_tag_attribute(sm, { sm->a1, sm->a2 }, { sm->b1, sm->b2 }); }
	goto st1245;
st1245:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1245;
case 1245:
#line 32335 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 9: goto st1245;
		case 32: goto st1245;
	}
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1532;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto tr1532;
	} else
		goto tr1532;
	goto tr1359;
tr1532:
#line 71 "ext/dtext/dtext.cpp.rl"
	{ sm->a1 = sm->p; }
	goto st1246;
st1246:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1246;
case 1246:
#line 32357 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 9: goto tr1533;
		case 32: goto tr1533;
		case 61: goto tr1535;
	}
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st1246;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st1246;
	} else
		goto st1246;
	goto tr1359;
tr1533:
#line 72 "ext/dtext/dtext.cpp.rl"
	{ sm->a2 = sm->p; }
	goto st1247;
st1247:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1247;
case 1247:
#line 32380 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 9: goto st1247;
		case 32: goto st1247;
		case 61: goto st1248;
	}
	goto tr1359;
tr1535:
#line 72 "ext/dtext/dtext.cpp.rl"
	{ sm->a2 = sm->p; }
	goto st1248;
st1248:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1248;
case 1248:
#line 32395 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 9: goto st1248;
		case 32: goto st1248;
		case 34: goto st1249;
		case 39: goto st1252;
	}
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1540;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto tr1540;
	} else
		goto tr1540;
	goto tr1359;
st1249:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1249;
case 1249:
	switch( (*( sm->p)) ) {
		case 0: goto tr1359;
		case 10: goto tr1359;
		case 13: goto tr1359;
	}
	goto tr1541;
tr1541:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ sm->b1 = sm->p; }
	goto st1250;
st1250:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1250;
case 1250:
#line 32429 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 0: goto tr1359;
		case 10: goto tr1359;
		case 13: goto tr1359;
		case 34: goto tr1543;
	}
	goto st1250;
tr1543:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ sm->b2 = sm->p; }
	goto st1251;
st1251:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1251;
case 1251:
#line 32445 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 9: goto tr1544;
		case 32: goto tr1544;
		case 62: goto tr1545;
	}
	goto tr1359;
st1252:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1252;
case 1252:
	switch( (*( sm->p)) ) {
		case 0: goto tr1359;
		case 10: goto tr1359;
		case 13: goto tr1359;
	}
	goto tr1546;
tr1546:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ sm->b1 = sm->p; }
	goto st1253;
st1253:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1253;
case 1253:
#line 32470 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 0: goto tr1359;
		case 10: goto tr1359;
		case 13: goto tr1359;
		case 39: goto tr1543;
	}
	goto st1253;
tr1540:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ sm->b1 = sm->p; }
	goto st1254;
st1254:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1254;
case 1254:
#line 32486 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 9: goto tr1548;
		case 32: goto tr1548;
		case 62: goto tr1550;
	}
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st1254;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st1254;
	} else
		goto st1254;
	goto tr1359;
tr2166:
#line 1 "NONE"
	{( sm->te) = ( sm->p)+1;}
	goto st1730;
st1730:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1730;
case 1730:
#line 32509 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 47: goto st1255;
		case 67: goto st1278;
		case 84: goto st1306;
		case 99: goto st1278;
		case 116: goto st1306;
	}
	goto tr2167;
st1255:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1255;
case 1255:
	switch( (*( sm->p)) ) {
		case 67: goto st1256;
		case 84: goto st1264;
		case 99: goto st1256;
		case 116: goto st1264;
	}
	goto tr1359;
st1256:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1256;
case 1256:
	switch( (*( sm->p)) ) {
		case 79: goto st1257;
		case 111: goto st1257;
	}
	goto tr1359;
st1257:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1257;
case 1257:
	switch( (*( sm->p)) ) {
		case 76: goto st1258;
		case 108: goto st1258;
	}
	goto tr1359;
st1258:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1258;
case 1258:
	switch( (*( sm->p)) ) {
		case 71: goto st1259;
		case 103: goto st1259;
	}
	goto tr1359;
st1259:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1259;
case 1259:
	switch( (*( sm->p)) ) {
		case 82: goto st1260;
		case 114: goto st1260;
	}
	goto tr1359;
st1260:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1260;
case 1260:
	switch( (*( sm->p)) ) {
		case 79: goto st1261;
		case 111: goto st1261;
	}
	goto tr1359;
st1261:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1261;
case 1261:
	switch( (*( sm->p)) ) {
		case 85: goto st1262;
		case 117: goto st1262;
	}
	goto tr1359;
st1262:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1262;
case 1262:
	switch( (*( sm->p)) ) {
		case 80: goto st1263;
		case 112: goto st1263;
	}
	goto tr1359;
st1263:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1263;
case 1263:
	if ( (*( sm->p)) == 93 )
		goto tr1369;
	goto tr1359;
st1264:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1264;
case 1264:
	switch( (*( sm->p)) ) {
		case 65: goto st1265;
		case 66: goto st1269;
		case 72: goto st1273;
		case 82: goto st1277;
		case 97: goto st1265;
		case 98: goto st1269;
		case 104: goto st1273;
		case 114: goto st1277;
	}
	goto tr1359;
st1265:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1265;
case 1265:
	switch( (*( sm->p)) ) {
		case 66: goto st1266;
		case 98: goto st1266;
	}
	goto tr1359;
st1266:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1266;
case 1266:
	switch( (*( sm->p)) ) {
		case 76: goto st1267;
		case 108: goto st1267;
	}
	goto tr1359;
st1267:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1267;
case 1267:
	switch( (*( sm->p)) ) {
		case 69: goto st1268;
		case 101: goto st1268;
	}
	goto tr1359;
st1268:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1268;
case 1268:
	if ( (*( sm->p)) == 93 )
		goto tr1377;
	goto tr1359;
st1269:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1269;
case 1269:
	switch( (*( sm->p)) ) {
		case 79: goto st1270;
		case 111: goto st1270;
	}
	goto tr1359;
st1270:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1270;
case 1270:
	switch( (*( sm->p)) ) {
		case 68: goto st1271;
		case 100: goto st1271;
	}
	goto tr1359;
st1271:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1271;
case 1271:
	switch( (*( sm->p)) ) {
		case 89: goto st1272;
		case 121: goto st1272;
	}
	goto tr1359;
st1272:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1272;
case 1272:
	if ( (*( sm->p)) == 93 )
		goto tr1381;
	goto tr1359;
st1273:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1273;
case 1273:
	switch( (*( sm->p)) ) {
		case 69: goto st1274;
		case 101: goto st1274;
	}
	goto tr1359;
st1274:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1274;
case 1274:
	switch( (*( sm->p)) ) {
		case 65: goto st1275;
		case 97: goto st1275;
	}
	goto tr1359;
st1275:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1275;
case 1275:
	switch( (*( sm->p)) ) {
		case 68: goto st1276;
		case 100: goto st1276;
	}
	goto tr1359;
st1276:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1276;
case 1276:
	if ( (*( sm->p)) == 93 )
		goto tr1385;
	goto tr1359;
st1277:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1277;
case 1277:
	if ( (*( sm->p)) == 93 )
		goto tr1386;
	goto tr1359;
st1278:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1278;
case 1278:
	switch( (*( sm->p)) ) {
		case 79: goto st1279;
		case 111: goto st1279;
	}
	goto tr1359;
st1279:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1279;
case 1279:
	switch( (*( sm->p)) ) {
		case 76: goto st1280;
		case 108: goto st1280;
	}
	goto tr1359;
st1280:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1280;
case 1280:
	switch( (*( sm->p)) ) {
		case 9: goto st1281;
		case 32: goto st1281;
		case 71: goto st1291;
		case 93: goto tr1390;
		case 103: goto st1291;
	}
	goto tr1359;
tr1589:
#line 87 "ext/dtext/dtext.cpp.rl"
	{ save_tag_attribute(sm, { sm->a1, sm->a2 }, { sm->b1, sm->b2 }); }
	goto st1281;
tr1592:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ sm->b2 = sm->p; }
#line 87 "ext/dtext/dtext.cpp.rl"
	{ save_tag_attribute(sm, { sm->a1, sm->a2 }, { sm->b1, sm->b2 }); }
	goto st1281;
st1281:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1281;
case 1281:
#line 32767 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 9: goto st1281;
		case 32: goto st1281;
	}
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1577;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto tr1577;
	} else
		goto tr1577;
	goto tr1359;
tr1577:
#line 71 "ext/dtext/dtext.cpp.rl"
	{ sm->a1 = sm->p; }
	goto st1282;
st1282:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1282;
case 1282:
#line 32789 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 9: goto tr1578;
		case 32: goto tr1578;
		case 61: goto tr1580;
	}
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st1282;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st1282;
	} else
		goto st1282;
	goto tr1359;
tr1578:
#line 72 "ext/dtext/dtext.cpp.rl"
	{ sm->a2 = sm->p; }
	goto st1283;
st1283:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1283;
case 1283:
#line 32812 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 9: goto st1283;
		case 32: goto st1283;
		case 61: goto st1284;
	}
	goto tr1359;
tr1580:
#line 72 "ext/dtext/dtext.cpp.rl"
	{ sm->a2 = sm->p; }
	goto st1284;
st1284:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1284;
case 1284:
#line 32827 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 9: goto st1284;
		case 32: goto st1284;
		case 34: goto st1285;
		case 39: goto st1288;
	}
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1585;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto tr1585;
	} else
		goto tr1585;
	goto tr1359;
st1285:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1285;
case 1285:
	switch( (*( sm->p)) ) {
		case 0: goto tr1359;
		case 10: goto tr1359;
		case 13: goto tr1359;
	}
	goto tr1586;
tr1586:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ sm->b1 = sm->p; }
	goto st1286;
st1286:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1286;
case 1286:
#line 32861 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 0: goto tr1359;
		case 10: goto tr1359;
		case 13: goto tr1359;
		case 34: goto tr1588;
	}
	goto st1286;
tr1588:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ sm->b2 = sm->p; }
	goto st1287;
st1287:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1287;
case 1287:
#line 32877 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 9: goto tr1589;
		case 32: goto tr1589;
		case 93: goto tr1405;
	}
	goto tr1359;
st1288:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1288;
case 1288:
	switch( (*( sm->p)) ) {
		case 0: goto tr1359;
		case 10: goto tr1359;
		case 13: goto tr1359;
	}
	goto tr1590;
tr1590:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ sm->b1 = sm->p; }
	goto st1289;
st1289:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1289;
case 1289:
#line 32902 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 0: goto tr1359;
		case 10: goto tr1359;
		case 13: goto tr1359;
		case 39: goto tr1588;
	}
	goto st1289;
tr1585:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ sm->b1 = sm->p; }
	goto st1290;
st1290:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1290;
case 1290:
#line 32918 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 9: goto tr1592;
		case 32: goto tr1592;
		case 93: goto tr1410;
	}
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st1290;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st1290;
	} else
		goto st1290;
	goto tr1359;
st1291:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1291;
case 1291:
	switch( (*( sm->p)) ) {
		case 82: goto st1292;
		case 114: goto st1292;
	}
	goto tr1359;
st1292:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1292;
case 1292:
	switch( (*( sm->p)) ) {
		case 79: goto st1293;
		case 111: goto st1293;
	}
	goto tr1359;
st1293:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1293;
case 1293:
	switch( (*( sm->p)) ) {
		case 85: goto st1294;
		case 117: goto st1294;
	}
	goto tr1359;
st1294:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1294;
case 1294:
	switch( (*( sm->p)) ) {
		case 80: goto st1295;
		case 112: goto st1295;
	}
	goto tr1359;
st1295:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1295;
case 1295:
	switch( (*( sm->p)) ) {
		case 9: goto st1296;
		case 32: goto st1296;
		case 93: goto tr1416;
	}
	goto tr1359;
tr1611:
#line 87 "ext/dtext/dtext.cpp.rl"
	{ save_tag_attribute(sm, { sm->a1, sm->a2 }, { sm->b1, sm->b2 }); }
	goto st1296;
tr1614:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ sm->b2 = sm->p; }
#line 87 "ext/dtext/dtext.cpp.rl"
	{ save_tag_attribute(sm, { sm->a1, sm->a2 }, { sm->b1, sm->b2 }); }
	goto st1296;
st1296:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1296;
case 1296:
#line 32993 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 9: goto st1296;
		case 32: goto st1296;
	}
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1599;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto tr1599;
	} else
		goto tr1599;
	goto tr1359;
tr1599:
#line 71 "ext/dtext/dtext.cpp.rl"
	{ sm->a1 = sm->p; }
	goto st1297;
st1297:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1297;
case 1297:
#line 33015 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 9: goto tr1600;
		case 32: goto tr1600;
		case 61: goto tr1602;
	}
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st1297;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st1297;
	} else
		goto st1297;
	goto tr1359;
tr1600:
#line 72 "ext/dtext/dtext.cpp.rl"
	{ sm->a2 = sm->p; }
	goto st1298;
st1298:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1298;
case 1298:
#line 33038 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 9: goto st1298;
		case 32: goto st1298;
		case 61: goto st1299;
	}
	goto tr1359;
tr1602:
#line 72 "ext/dtext/dtext.cpp.rl"
	{ sm->a2 = sm->p; }
	goto st1299;
st1299:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1299;
case 1299:
#line 33053 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 9: goto st1299;
		case 32: goto st1299;
		case 34: goto st1300;
		case 39: goto st1303;
	}
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1607;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto tr1607;
	} else
		goto tr1607;
	goto tr1359;
st1300:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1300;
case 1300:
	switch( (*( sm->p)) ) {
		case 0: goto tr1359;
		case 10: goto tr1359;
		case 13: goto tr1359;
	}
	goto tr1608;
tr1608:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ sm->b1 = sm->p; }
	goto st1301;
st1301:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1301;
case 1301:
#line 33087 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 0: goto tr1359;
		case 10: goto tr1359;
		case 13: goto tr1359;
		case 34: goto tr1610;
	}
	goto st1301;
tr1610:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ sm->b2 = sm->p; }
	goto st1302;
st1302:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1302;
case 1302:
#line 33103 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 9: goto tr1611;
		case 32: goto tr1611;
		case 93: goto tr1430;
	}
	goto tr1359;
st1303:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1303;
case 1303:
	switch( (*( sm->p)) ) {
		case 0: goto tr1359;
		case 10: goto tr1359;
		case 13: goto tr1359;
	}
	goto tr1612;
tr1612:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ sm->b1 = sm->p; }
	goto st1304;
st1304:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1304;
case 1304:
#line 33128 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 0: goto tr1359;
		case 10: goto tr1359;
		case 13: goto tr1359;
		case 39: goto tr1610;
	}
	goto st1304;
tr1607:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ sm->b1 = sm->p; }
	goto st1305;
st1305:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1305;
case 1305:
#line 33144 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 9: goto tr1614;
		case 32: goto tr1614;
		case 93: goto tr1435;
	}
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st1305;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st1305;
	} else
		goto st1305;
	goto tr1359;
st1306:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1306;
case 1306:
	switch( (*( sm->p)) ) {
		case 66: goto st1307;
		case 68: goto st1321;
		case 72: goto st1332;
		case 82: goto st1356;
		case 98: goto st1307;
		case 100: goto st1321;
		case 104: goto st1332;
		case 114: goto st1356;
	}
	goto tr1359;
st1307:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1307;
case 1307:
	switch( (*( sm->p)) ) {
		case 79: goto st1308;
		case 111: goto st1308;
	}
	goto tr1359;
st1308:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1308;
case 1308:
	switch( (*( sm->p)) ) {
		case 68: goto st1309;
		case 100: goto st1309;
	}
	goto tr1359;
st1309:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1309;
case 1309:
	switch( (*( sm->p)) ) {
		case 89: goto st1310;
		case 121: goto st1310;
	}
	goto tr1359;
st1310:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1310;
case 1310:
	switch( (*( sm->p)) ) {
		case 9: goto st1311;
		case 32: goto st1311;
		case 93: goto tr1444;
	}
	goto tr1359;
tr1636:
#line 87 "ext/dtext/dtext.cpp.rl"
	{ save_tag_attribute(sm, { sm->a1, sm->a2 }, { sm->b1, sm->b2 }); }
	goto st1311;
tr1639:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ sm->b2 = sm->p; }
#line 87 "ext/dtext/dtext.cpp.rl"
	{ save_tag_attribute(sm, { sm->a1, sm->a2 }, { sm->b1, sm->b2 }); }
	goto st1311;
st1311:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1311;
case 1311:
#line 33225 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 9: goto st1311;
		case 32: goto st1311;
	}
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1624;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto tr1624;
	} else
		goto tr1624;
	goto tr1359;
tr1624:
#line 71 "ext/dtext/dtext.cpp.rl"
	{ sm->a1 = sm->p; }
	goto st1312;
st1312:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1312;
case 1312:
#line 33247 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 9: goto tr1625;
		case 32: goto tr1625;
		case 61: goto tr1627;
	}
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st1312;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st1312;
	} else
		goto st1312;
	goto tr1359;
tr1625:
#line 72 "ext/dtext/dtext.cpp.rl"
	{ sm->a2 = sm->p; }
	goto st1313;
st1313:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1313;
case 1313:
#line 33270 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 9: goto st1313;
		case 32: goto st1313;
		case 61: goto st1314;
	}
	goto tr1359;
tr1627:
#line 72 "ext/dtext/dtext.cpp.rl"
	{ sm->a2 = sm->p; }
	goto st1314;
st1314:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1314;
case 1314:
#line 33285 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 9: goto st1314;
		case 32: goto st1314;
		case 34: goto st1315;
		case 39: goto st1318;
	}
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1632;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto tr1632;
	} else
		goto tr1632;
	goto tr1359;
st1315:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1315;
case 1315:
	switch( (*( sm->p)) ) {
		case 0: goto tr1359;
		case 10: goto tr1359;
		case 13: goto tr1359;
	}
	goto tr1633;
tr1633:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ sm->b1 = sm->p; }
	goto st1316;
st1316:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1316;
case 1316:
#line 33319 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 0: goto tr1359;
		case 10: goto tr1359;
		case 13: goto tr1359;
		case 34: goto tr1635;
	}
	goto st1316;
tr1635:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ sm->b2 = sm->p; }
	goto st1317;
st1317:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1317;
case 1317:
#line 33335 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 9: goto tr1636;
		case 32: goto tr1636;
		case 93: goto tr1458;
	}
	goto tr1359;
st1318:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1318;
case 1318:
	switch( (*( sm->p)) ) {
		case 0: goto tr1359;
		case 10: goto tr1359;
		case 13: goto tr1359;
	}
	goto tr1637;
tr1637:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ sm->b1 = sm->p; }
	goto st1319;
st1319:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1319;
case 1319:
#line 33360 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 0: goto tr1359;
		case 10: goto tr1359;
		case 13: goto tr1359;
		case 39: goto tr1635;
	}
	goto st1319;
tr1632:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ sm->b1 = sm->p; }
	goto st1320;
st1320:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1320;
case 1320:
#line 33376 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 9: goto tr1639;
		case 32: goto tr1639;
		case 93: goto tr1463;
	}
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st1320;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st1320;
	} else
		goto st1320;
	goto tr1359;
st1321:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1321;
case 1321:
	switch( (*( sm->p)) ) {
		case 9: goto st1322;
		case 32: goto st1322;
		case 93: goto tr1465;
	}
	goto tr1359;
tr1654:
#line 87 "ext/dtext/dtext.cpp.rl"
	{ save_tag_attribute(sm, { sm->a1, sm->a2 }, { sm->b1, sm->b2 }); }
	goto st1322;
tr1657:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ sm->b2 = sm->p; }
#line 87 "ext/dtext/dtext.cpp.rl"
	{ save_tag_attribute(sm, { sm->a1, sm->a2 }, { sm->b1, sm->b2 }); }
	goto st1322;
st1322:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1322;
case 1322:
#line 33415 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 9: goto st1322;
		case 32: goto st1322;
	}
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1642;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto tr1642;
	} else
		goto tr1642;
	goto tr1359;
tr1642:
#line 71 "ext/dtext/dtext.cpp.rl"
	{ sm->a1 = sm->p; }
	goto st1323;
st1323:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1323;
case 1323:
#line 33437 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 9: goto tr1643;
		case 32: goto tr1643;
		case 61: goto tr1645;
	}
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st1323;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st1323;
	} else
		goto st1323;
	goto tr1359;
tr1643:
#line 72 "ext/dtext/dtext.cpp.rl"
	{ sm->a2 = sm->p; }
	goto st1324;
st1324:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1324;
case 1324:
#line 33460 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 9: goto st1324;
		case 32: goto st1324;
		case 61: goto st1325;
	}
	goto tr1359;
tr1645:
#line 72 "ext/dtext/dtext.cpp.rl"
	{ sm->a2 = sm->p; }
	goto st1325;
st1325:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1325;
case 1325:
#line 33475 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 9: goto st1325;
		case 32: goto st1325;
		case 34: goto st1326;
		case 39: goto st1329;
	}
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1650;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto tr1650;
	} else
		goto tr1650;
	goto tr1359;
st1326:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1326;
case 1326:
	switch( (*( sm->p)) ) {
		case 0: goto tr1359;
		case 10: goto tr1359;
		case 13: goto tr1359;
	}
	goto tr1651;
tr1651:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ sm->b1 = sm->p; }
	goto st1327;
st1327:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1327;
case 1327:
#line 33509 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 0: goto tr1359;
		case 10: goto tr1359;
		case 13: goto tr1359;
		case 34: goto tr1653;
	}
	goto st1327;
tr1653:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ sm->b2 = sm->p; }
	goto st1328;
st1328:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1328;
case 1328:
#line 33525 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 9: goto tr1654;
		case 32: goto tr1654;
		case 93: goto tr1479;
	}
	goto tr1359;
st1329:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1329;
case 1329:
	switch( (*( sm->p)) ) {
		case 0: goto tr1359;
		case 10: goto tr1359;
		case 13: goto tr1359;
	}
	goto tr1655;
tr1655:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ sm->b1 = sm->p; }
	goto st1330;
st1330:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1330;
case 1330:
#line 33550 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 0: goto tr1359;
		case 10: goto tr1359;
		case 13: goto tr1359;
		case 39: goto tr1653;
	}
	goto st1330;
tr1650:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ sm->b1 = sm->p; }
	goto st1331;
st1331:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1331;
case 1331:
#line 33566 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 9: goto tr1657;
		case 32: goto tr1657;
		case 93: goto tr1484;
	}
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st1331;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st1331;
	} else
		goto st1331;
	goto tr1359;
st1332:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1332;
case 1332:
	switch( (*( sm->p)) ) {
		case 9: goto st1333;
		case 32: goto st1333;
		case 69: goto st1343;
		case 93: goto tr1486;
		case 101: goto st1343;
	}
	goto tr1359;
tr1673:
#line 87 "ext/dtext/dtext.cpp.rl"
	{ save_tag_attribute(sm, { sm->a1, sm->a2 }, { sm->b1, sm->b2 }); }
	goto st1333;
tr1676:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ sm->b2 = sm->p; }
#line 87 "ext/dtext/dtext.cpp.rl"
	{ save_tag_attribute(sm, { sm->a1, sm->a2 }, { sm->b1, sm->b2 }); }
	goto st1333;
st1333:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1333;
case 1333:
#line 33607 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 9: goto st1333;
		case 32: goto st1333;
	}
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1661;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto tr1661;
	} else
		goto tr1661;
	goto tr1359;
tr1661:
#line 71 "ext/dtext/dtext.cpp.rl"
	{ sm->a1 = sm->p; }
	goto st1334;
st1334:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1334;
case 1334:
#line 33629 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 9: goto tr1662;
		case 32: goto tr1662;
		case 61: goto tr1664;
	}
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st1334;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st1334;
	} else
		goto st1334;
	goto tr1359;
tr1662:
#line 72 "ext/dtext/dtext.cpp.rl"
	{ sm->a2 = sm->p; }
	goto st1335;
st1335:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1335;
case 1335:
#line 33652 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 9: goto st1335;
		case 32: goto st1335;
		case 61: goto st1336;
	}
	goto tr1359;
tr1664:
#line 72 "ext/dtext/dtext.cpp.rl"
	{ sm->a2 = sm->p; }
	goto st1336;
st1336:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1336;
case 1336:
#line 33667 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 9: goto st1336;
		case 32: goto st1336;
		case 34: goto st1337;
		case 39: goto st1340;
	}
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1669;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto tr1669;
	} else
		goto tr1669;
	goto tr1359;
st1337:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1337;
case 1337:
	switch( (*( sm->p)) ) {
		case 0: goto tr1359;
		case 10: goto tr1359;
		case 13: goto tr1359;
	}
	goto tr1670;
tr1670:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ sm->b1 = sm->p; }
	goto st1338;
st1338:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1338;
case 1338:
#line 33701 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 0: goto tr1359;
		case 10: goto tr1359;
		case 13: goto tr1359;
		case 34: goto tr1672;
	}
	goto st1338;
tr1672:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ sm->b2 = sm->p; }
	goto st1339;
st1339:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1339;
case 1339:
#line 33717 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 9: goto tr1673;
		case 32: goto tr1673;
		case 93: goto tr1501;
	}
	goto tr1359;
st1340:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1340;
case 1340:
	switch( (*( sm->p)) ) {
		case 0: goto tr1359;
		case 10: goto tr1359;
		case 13: goto tr1359;
	}
	goto tr1674;
tr1674:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ sm->b1 = sm->p; }
	goto st1341;
st1341:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1341;
case 1341:
#line 33742 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 0: goto tr1359;
		case 10: goto tr1359;
		case 13: goto tr1359;
		case 39: goto tr1672;
	}
	goto st1341;
tr1669:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ sm->b1 = sm->p; }
	goto st1342;
st1342:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1342;
case 1342:
#line 33758 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 9: goto tr1676;
		case 32: goto tr1676;
		case 93: goto tr1506;
	}
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st1342;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st1342;
	} else
		goto st1342;
	goto tr1359;
st1343:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1343;
case 1343:
	switch( (*( sm->p)) ) {
		case 65: goto st1344;
		case 97: goto st1344;
	}
	goto tr1359;
st1344:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1344;
case 1344:
	switch( (*( sm->p)) ) {
		case 68: goto st1345;
		case 100: goto st1345;
	}
	goto tr1359;
st1345:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1345;
case 1345:
	switch( (*( sm->p)) ) {
		case 9: goto st1346;
		case 32: goto st1346;
		case 93: goto tr1510;
	}
	goto tr1359;
tr1693:
#line 87 "ext/dtext/dtext.cpp.rl"
	{ save_tag_attribute(sm, { sm->a1, sm->a2 }, { sm->b1, sm->b2 }); }
	goto st1346;
tr1696:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ sm->b2 = sm->p; }
#line 87 "ext/dtext/dtext.cpp.rl"
	{ save_tag_attribute(sm, { sm->a1, sm->a2 }, { sm->b1, sm->b2 }); }
	goto st1346;
st1346:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1346;
case 1346:
#line 33815 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 9: goto st1346;
		case 32: goto st1346;
	}
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1681;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto tr1681;
	} else
		goto tr1681;
	goto tr1359;
tr1681:
#line 71 "ext/dtext/dtext.cpp.rl"
	{ sm->a1 = sm->p; }
	goto st1347;
st1347:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1347;
case 1347:
#line 33837 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 9: goto tr1682;
		case 32: goto tr1682;
		case 61: goto tr1684;
	}
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st1347;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st1347;
	} else
		goto st1347;
	goto tr1359;
tr1682:
#line 72 "ext/dtext/dtext.cpp.rl"
	{ sm->a2 = sm->p; }
	goto st1348;
st1348:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1348;
case 1348:
#line 33860 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 9: goto st1348;
		case 32: goto st1348;
		case 61: goto st1349;
	}
	goto tr1359;
tr1684:
#line 72 "ext/dtext/dtext.cpp.rl"
	{ sm->a2 = sm->p; }
	goto st1349;
st1349:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1349;
case 1349:
#line 33875 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 9: goto st1349;
		case 32: goto st1349;
		case 34: goto st1350;
		case 39: goto st1353;
	}
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1689;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto tr1689;
	} else
		goto tr1689;
	goto tr1359;
st1350:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1350;
case 1350:
	switch( (*( sm->p)) ) {
		case 0: goto tr1359;
		case 10: goto tr1359;
		case 13: goto tr1359;
	}
	goto tr1690;
tr1690:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ sm->b1 = sm->p; }
	goto st1351;
st1351:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1351;
case 1351:
#line 33909 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 0: goto tr1359;
		case 10: goto tr1359;
		case 13: goto tr1359;
		case 34: goto tr1692;
	}
	goto st1351;
tr1692:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ sm->b2 = sm->p; }
	goto st1352;
st1352:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1352;
case 1352:
#line 33925 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 9: goto tr1693;
		case 32: goto tr1693;
		case 93: goto tr1524;
	}
	goto tr1359;
st1353:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1353;
case 1353:
	switch( (*( sm->p)) ) {
		case 0: goto tr1359;
		case 10: goto tr1359;
		case 13: goto tr1359;
	}
	goto tr1694;
tr1694:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ sm->b1 = sm->p; }
	goto st1354;
st1354:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1354;
case 1354:
#line 33950 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 0: goto tr1359;
		case 10: goto tr1359;
		case 13: goto tr1359;
		case 39: goto tr1692;
	}
	goto st1354;
tr1689:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ sm->b1 = sm->p; }
	goto st1355;
st1355:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1355;
case 1355:
#line 33966 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 9: goto tr1696;
		case 32: goto tr1696;
		case 93: goto tr1529;
	}
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st1355;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st1355;
	} else
		goto st1355;
	goto tr1359;
st1356:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1356;
case 1356:
	switch( (*( sm->p)) ) {
		case 9: goto st1357;
		case 32: goto st1357;
		case 93: goto tr1531;
	}
	goto tr1359;
tr1711:
#line 87 "ext/dtext/dtext.cpp.rl"
	{ save_tag_attribute(sm, { sm->a1, sm->a2 }, { sm->b1, sm->b2 }); }
	goto st1357;
tr1714:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ sm->b2 = sm->p; }
#line 87 "ext/dtext/dtext.cpp.rl"
	{ save_tag_attribute(sm, { sm->a1, sm->a2 }, { sm->b1, sm->b2 }); }
	goto st1357;
st1357:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1357;
case 1357:
#line 34005 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 9: goto st1357;
		case 32: goto st1357;
	}
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1699;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto tr1699;
	} else
		goto tr1699;
	goto tr1359;
tr1699:
#line 71 "ext/dtext/dtext.cpp.rl"
	{ sm->a1 = sm->p; }
	goto st1358;
st1358:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1358;
case 1358:
#line 34027 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 9: goto tr1700;
		case 32: goto tr1700;
		case 61: goto tr1702;
	}
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st1358;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st1358;
	} else
		goto st1358;
	goto tr1359;
tr1700:
#line 72 "ext/dtext/dtext.cpp.rl"
	{ sm->a2 = sm->p; }
	goto st1359;
st1359:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1359;
case 1359:
#line 34050 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 9: goto st1359;
		case 32: goto st1359;
		case 61: goto st1360;
	}
	goto tr1359;
tr1702:
#line 72 "ext/dtext/dtext.cpp.rl"
	{ sm->a2 = sm->p; }
	goto st1360;
st1360:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1360;
case 1360:
#line 34065 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 9: goto st1360;
		case 32: goto st1360;
		case 34: goto st1361;
		case 39: goto st1364;
	}
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto tr1707;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto tr1707;
	} else
		goto tr1707;
	goto tr1359;
st1361:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1361;
case 1361:
	switch( (*( sm->p)) ) {
		case 0: goto tr1359;
		case 10: goto tr1359;
		case 13: goto tr1359;
	}
	goto tr1708;
tr1708:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ sm->b1 = sm->p; }
	goto st1362;
st1362:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1362;
case 1362:
#line 34099 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 0: goto tr1359;
		case 10: goto tr1359;
		case 13: goto tr1359;
		case 34: goto tr1710;
	}
	goto st1362;
tr1710:
#line 74 "ext/dtext/dtext.cpp.rl"
	{ sm->b2 = sm->p; }
	goto st1363;
st1363:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1363;
case 1363:
#line 34115 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 9: goto tr1711;
		case 32: goto tr1711;
		case 93: goto tr1545;
	}
	goto tr1359;
st1364:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1364;
case 1364:
	switch( (*( sm->p)) ) {
		case 0: goto tr1359;
		case 10: goto tr1359;
		case 13: goto tr1359;
	}
	goto tr1712;
tr1712:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ sm->b1 = sm->p; }
	goto st1365;
st1365:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1365;
case 1365:
#line 34140 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 0: goto tr1359;
		case 10: goto tr1359;
		case 13: goto tr1359;
		case 39: goto tr1710;
	}
	goto st1365;
tr1707:
#line 73 "ext/dtext/dtext.cpp.rl"
	{ sm->b1 = sm->p; }
	goto st1366;
st1366:
	if ( ++( sm->p) == ( sm->pe) )
		goto _test_eof1366;
case 1366:
#line 34156 "ext/dtext/dtext.cpp"
	switch( (*( sm->p)) ) {
		case 9: goto tr1714;
		case 32: goto tr1714;
		case 93: goto tr1550;
	}
	if ( (*( sm->p)) < 65 ) {
		if ( 48 <= (*( sm->p)) && (*( sm->p)) <= 57 )
			goto st1366;
	} else if ( (*( sm->p)) > 90 ) {
		if ( 97 <= (*( sm->p)) && (*( sm->p)) <= 122 )
			goto st1366;
	} else
		goto st1366;
	goto tr1359;
	}
	_test_eof1367:  sm->cs = 1367; goto _test_eof; 
	_test_eof1368:  sm->cs = 1368; goto _test_eof; 
	_test_eof1:  sm->cs = 1; goto _test_eof; 
	_test_eof1369:  sm->cs = 1369; goto _test_eof; 
	_test_eof2:  sm->cs = 2; goto _test_eof; 
	_test_eof3:  sm->cs = 3; goto _test_eof; 
	_test_eof4:  sm->cs = 4; goto _test_eof; 
	_test_eof5:  sm->cs = 5; goto _test_eof; 
	_test_eof6:  sm->cs = 6; goto _test_eof; 
	_test_eof7:  sm->cs = 7; goto _test_eof; 
	_test_eof8:  sm->cs = 8; goto _test_eof; 
	_test_eof9:  sm->cs = 9; goto _test_eof; 
	_test_eof10:  sm->cs = 10; goto _test_eof; 
	_test_eof11:  sm->cs = 11; goto _test_eof; 
	_test_eof12:  sm->cs = 12; goto _test_eof; 
	_test_eof1370:  sm->cs = 1370; goto _test_eof; 
	_test_eof13:  sm->cs = 13; goto _test_eof; 
	_test_eof14:  sm->cs = 14; goto _test_eof; 
	_test_eof15:  sm->cs = 15; goto _test_eof; 
	_test_eof16:  sm->cs = 16; goto _test_eof; 
	_test_eof17:  sm->cs = 17; goto _test_eof; 
	_test_eof18:  sm->cs = 18; goto _test_eof; 
	_test_eof19:  sm->cs = 19; goto _test_eof; 
	_test_eof20:  sm->cs = 20; goto _test_eof; 
	_test_eof21:  sm->cs = 21; goto _test_eof; 
	_test_eof22:  sm->cs = 22; goto _test_eof; 
	_test_eof23:  sm->cs = 23; goto _test_eof; 
	_test_eof24:  sm->cs = 24; goto _test_eof; 
	_test_eof25:  sm->cs = 25; goto _test_eof; 
	_test_eof26:  sm->cs = 26; goto _test_eof; 
	_test_eof27:  sm->cs = 27; goto _test_eof; 
	_test_eof28:  sm->cs = 28; goto _test_eof; 
	_test_eof29:  sm->cs = 29; goto _test_eof; 
	_test_eof30:  sm->cs = 30; goto _test_eof; 
	_test_eof31:  sm->cs = 31; goto _test_eof; 
	_test_eof1371:  sm->cs = 1371; goto _test_eof; 
	_test_eof32:  sm->cs = 32; goto _test_eof; 
	_test_eof1372:  sm->cs = 1372; goto _test_eof; 
	_test_eof1373:  sm->cs = 1373; goto _test_eof; 
	_test_eof33:  sm->cs = 33; goto _test_eof; 
	_test_eof1374:  sm->cs = 1374; goto _test_eof; 
	_test_eof34:  sm->cs = 34; goto _test_eof; 
	_test_eof35:  sm->cs = 35; goto _test_eof; 
	_test_eof36:  sm->cs = 36; goto _test_eof; 
	_test_eof37:  sm->cs = 37; goto _test_eof; 
	_test_eof38:  sm->cs = 38; goto _test_eof; 
	_test_eof39:  sm->cs = 39; goto _test_eof; 
	_test_eof40:  sm->cs = 40; goto _test_eof; 
	_test_eof41:  sm->cs = 41; goto _test_eof; 
	_test_eof42:  sm->cs = 42; goto _test_eof; 
	_test_eof43:  sm->cs = 43; goto _test_eof; 
	_test_eof1375:  sm->cs = 1375; goto _test_eof; 
	_test_eof44:  sm->cs = 44; goto _test_eof; 
	_test_eof45:  sm->cs = 45; goto _test_eof; 
	_test_eof46:  sm->cs = 46; goto _test_eof; 
	_test_eof47:  sm->cs = 47; goto _test_eof; 
	_test_eof48:  sm->cs = 48; goto _test_eof; 
	_test_eof49:  sm->cs = 49; goto _test_eof; 
	_test_eof50:  sm->cs = 50; goto _test_eof; 
	_test_eof1376:  sm->cs = 1376; goto _test_eof; 
	_test_eof51:  sm->cs = 51; goto _test_eof; 
	_test_eof1377:  sm->cs = 1377; goto _test_eof; 
	_test_eof52:  sm->cs = 52; goto _test_eof; 
	_test_eof53:  sm->cs = 53; goto _test_eof; 
	_test_eof54:  sm->cs = 54; goto _test_eof; 
	_test_eof55:  sm->cs = 55; goto _test_eof; 
	_test_eof56:  sm->cs = 56; goto _test_eof; 
	_test_eof57:  sm->cs = 57; goto _test_eof; 
	_test_eof58:  sm->cs = 58; goto _test_eof; 
	_test_eof59:  sm->cs = 59; goto _test_eof; 
	_test_eof60:  sm->cs = 60; goto _test_eof; 
	_test_eof61:  sm->cs = 61; goto _test_eof; 
	_test_eof62:  sm->cs = 62; goto _test_eof; 
	_test_eof63:  sm->cs = 63; goto _test_eof; 
	_test_eof64:  sm->cs = 64; goto _test_eof; 
	_test_eof65:  sm->cs = 65; goto _test_eof; 
	_test_eof66:  sm->cs = 66; goto _test_eof; 
	_test_eof1378:  sm->cs = 1378; goto _test_eof; 
	_test_eof67:  sm->cs = 67; goto _test_eof; 
	_test_eof1379:  sm->cs = 1379; goto _test_eof; 
	_test_eof68:  sm->cs = 68; goto _test_eof; 
	_test_eof69:  sm->cs = 69; goto _test_eof; 
	_test_eof70:  sm->cs = 70; goto _test_eof; 
	_test_eof71:  sm->cs = 71; goto _test_eof; 
	_test_eof72:  sm->cs = 72; goto _test_eof; 
	_test_eof73:  sm->cs = 73; goto _test_eof; 
	_test_eof74:  sm->cs = 74; goto _test_eof; 
	_test_eof1380:  sm->cs = 1380; goto _test_eof; 
	_test_eof75:  sm->cs = 75; goto _test_eof; 
	_test_eof76:  sm->cs = 76; goto _test_eof; 
	_test_eof77:  sm->cs = 77; goto _test_eof; 
	_test_eof78:  sm->cs = 78; goto _test_eof; 
	_test_eof79:  sm->cs = 79; goto _test_eof; 
	_test_eof80:  sm->cs = 80; goto _test_eof; 
	_test_eof81:  sm->cs = 81; goto _test_eof; 
	_test_eof82:  sm->cs = 82; goto _test_eof; 
	_test_eof1381:  sm->cs = 1381; goto _test_eof; 
	_test_eof83:  sm->cs = 83; goto _test_eof; 
	_test_eof84:  sm->cs = 84; goto _test_eof; 
	_test_eof85:  sm->cs = 85; goto _test_eof; 
	_test_eof1382:  sm->cs = 1382; goto _test_eof; 
	_test_eof86:  sm->cs = 86; goto _test_eof; 
	_test_eof87:  sm->cs = 87; goto _test_eof; 
	_test_eof88:  sm->cs = 88; goto _test_eof; 
	_test_eof1383:  sm->cs = 1383; goto _test_eof; 
	_test_eof1384:  sm->cs = 1384; goto _test_eof; 
	_test_eof89:  sm->cs = 89; goto _test_eof; 
	_test_eof90:  sm->cs = 90; goto _test_eof; 
	_test_eof91:  sm->cs = 91; goto _test_eof; 
	_test_eof92:  sm->cs = 92; goto _test_eof; 
	_test_eof93:  sm->cs = 93; goto _test_eof; 
	_test_eof94:  sm->cs = 94; goto _test_eof; 
	_test_eof95:  sm->cs = 95; goto _test_eof; 
	_test_eof96:  sm->cs = 96; goto _test_eof; 
	_test_eof97:  sm->cs = 97; goto _test_eof; 
	_test_eof98:  sm->cs = 98; goto _test_eof; 
	_test_eof99:  sm->cs = 99; goto _test_eof; 
	_test_eof100:  sm->cs = 100; goto _test_eof; 
	_test_eof101:  sm->cs = 101; goto _test_eof; 
	_test_eof102:  sm->cs = 102; goto _test_eof; 
	_test_eof103:  sm->cs = 103; goto _test_eof; 
	_test_eof104:  sm->cs = 104; goto _test_eof; 
	_test_eof105:  sm->cs = 105; goto _test_eof; 
	_test_eof106:  sm->cs = 106; goto _test_eof; 
	_test_eof107:  sm->cs = 107; goto _test_eof; 
	_test_eof108:  sm->cs = 108; goto _test_eof; 
	_test_eof109:  sm->cs = 109; goto _test_eof; 
	_test_eof110:  sm->cs = 110; goto _test_eof; 
	_test_eof111:  sm->cs = 111; goto _test_eof; 
	_test_eof112:  sm->cs = 112; goto _test_eof; 
	_test_eof113:  sm->cs = 113; goto _test_eof; 
	_test_eof114:  sm->cs = 114; goto _test_eof; 
	_test_eof115:  sm->cs = 115; goto _test_eof; 
	_test_eof116:  sm->cs = 116; goto _test_eof; 
	_test_eof117:  sm->cs = 117; goto _test_eof; 
	_test_eof118:  sm->cs = 118; goto _test_eof; 
	_test_eof119:  sm->cs = 119; goto _test_eof; 
	_test_eof120:  sm->cs = 120; goto _test_eof; 
	_test_eof121:  sm->cs = 121; goto _test_eof; 
	_test_eof122:  sm->cs = 122; goto _test_eof; 
	_test_eof123:  sm->cs = 123; goto _test_eof; 
	_test_eof124:  sm->cs = 124; goto _test_eof; 
	_test_eof125:  sm->cs = 125; goto _test_eof; 
	_test_eof126:  sm->cs = 126; goto _test_eof; 
	_test_eof127:  sm->cs = 127; goto _test_eof; 
	_test_eof128:  sm->cs = 128; goto _test_eof; 
	_test_eof129:  sm->cs = 129; goto _test_eof; 
	_test_eof130:  sm->cs = 130; goto _test_eof; 
	_test_eof131:  sm->cs = 131; goto _test_eof; 
	_test_eof132:  sm->cs = 132; goto _test_eof; 
	_test_eof1385:  sm->cs = 1385; goto _test_eof; 
	_test_eof133:  sm->cs = 133; goto _test_eof; 
	_test_eof134:  sm->cs = 134; goto _test_eof; 
	_test_eof135:  sm->cs = 135; goto _test_eof; 
	_test_eof136:  sm->cs = 136; goto _test_eof; 
	_test_eof137:  sm->cs = 137; goto _test_eof; 
	_test_eof138:  sm->cs = 138; goto _test_eof; 
	_test_eof139:  sm->cs = 139; goto _test_eof; 
	_test_eof140:  sm->cs = 140; goto _test_eof; 
	_test_eof141:  sm->cs = 141; goto _test_eof; 
	_test_eof142:  sm->cs = 142; goto _test_eof; 
	_test_eof1386:  sm->cs = 1386; goto _test_eof; 
	_test_eof1387:  sm->cs = 1387; goto _test_eof; 
	_test_eof143:  sm->cs = 143; goto _test_eof; 
	_test_eof144:  sm->cs = 144; goto _test_eof; 
	_test_eof145:  sm->cs = 145; goto _test_eof; 
	_test_eof146:  sm->cs = 146; goto _test_eof; 
	_test_eof147:  sm->cs = 147; goto _test_eof; 
	_test_eof148:  sm->cs = 148; goto _test_eof; 
	_test_eof149:  sm->cs = 149; goto _test_eof; 
	_test_eof150:  sm->cs = 150; goto _test_eof; 
	_test_eof151:  sm->cs = 151; goto _test_eof; 
	_test_eof152:  sm->cs = 152; goto _test_eof; 
	_test_eof153:  sm->cs = 153; goto _test_eof; 
	_test_eof154:  sm->cs = 154; goto _test_eof; 
	_test_eof155:  sm->cs = 155; goto _test_eof; 
	_test_eof156:  sm->cs = 156; goto _test_eof; 
	_test_eof157:  sm->cs = 157; goto _test_eof; 
	_test_eof158:  sm->cs = 158; goto _test_eof; 
	_test_eof159:  sm->cs = 159; goto _test_eof; 
	_test_eof160:  sm->cs = 160; goto _test_eof; 
	_test_eof161:  sm->cs = 161; goto _test_eof; 
	_test_eof1388:  sm->cs = 1388; goto _test_eof; 
	_test_eof162:  sm->cs = 162; goto _test_eof; 
	_test_eof163:  sm->cs = 163; goto _test_eof; 
	_test_eof164:  sm->cs = 164; goto _test_eof; 
	_test_eof165:  sm->cs = 165; goto _test_eof; 
	_test_eof166:  sm->cs = 166; goto _test_eof; 
	_test_eof167:  sm->cs = 167; goto _test_eof; 
	_test_eof168:  sm->cs = 168; goto _test_eof; 
	_test_eof169:  sm->cs = 169; goto _test_eof; 
	_test_eof170:  sm->cs = 170; goto _test_eof; 
	_test_eof1389:  sm->cs = 1389; goto _test_eof; 
	_test_eof1390:  sm->cs = 1390; goto _test_eof; 
	_test_eof1391:  sm->cs = 1391; goto _test_eof; 
	_test_eof171:  sm->cs = 171; goto _test_eof; 
	_test_eof172:  sm->cs = 172; goto _test_eof; 
	_test_eof173:  sm->cs = 173; goto _test_eof; 
	_test_eof1392:  sm->cs = 1392; goto _test_eof; 
	_test_eof1393:  sm->cs = 1393; goto _test_eof; 
	_test_eof1394:  sm->cs = 1394; goto _test_eof; 
	_test_eof174:  sm->cs = 174; goto _test_eof; 
	_test_eof1395:  sm->cs = 1395; goto _test_eof; 
	_test_eof175:  sm->cs = 175; goto _test_eof; 
	_test_eof1396:  sm->cs = 1396; goto _test_eof; 
	_test_eof176:  sm->cs = 176; goto _test_eof; 
	_test_eof177:  sm->cs = 177; goto _test_eof; 
	_test_eof178:  sm->cs = 178; goto _test_eof; 
	_test_eof179:  sm->cs = 179; goto _test_eof; 
	_test_eof180:  sm->cs = 180; goto _test_eof; 
	_test_eof1397:  sm->cs = 1397; goto _test_eof; 
	_test_eof181:  sm->cs = 181; goto _test_eof; 
	_test_eof182:  sm->cs = 182; goto _test_eof; 
	_test_eof183:  sm->cs = 183; goto _test_eof; 
	_test_eof184:  sm->cs = 184; goto _test_eof; 
	_test_eof185:  sm->cs = 185; goto _test_eof; 
	_test_eof186:  sm->cs = 186; goto _test_eof; 
	_test_eof187:  sm->cs = 187; goto _test_eof; 
	_test_eof188:  sm->cs = 188; goto _test_eof; 
	_test_eof189:  sm->cs = 189; goto _test_eof; 
	_test_eof190:  sm->cs = 190; goto _test_eof; 
	_test_eof191:  sm->cs = 191; goto _test_eof; 
	_test_eof192:  sm->cs = 192; goto _test_eof; 
	_test_eof193:  sm->cs = 193; goto _test_eof; 
	_test_eof194:  sm->cs = 194; goto _test_eof; 
	_test_eof195:  sm->cs = 195; goto _test_eof; 
	_test_eof196:  sm->cs = 196; goto _test_eof; 
	_test_eof197:  sm->cs = 197; goto _test_eof; 
	_test_eof198:  sm->cs = 198; goto _test_eof; 
	_test_eof199:  sm->cs = 199; goto _test_eof; 
	_test_eof200:  sm->cs = 200; goto _test_eof; 
	_test_eof201:  sm->cs = 201; goto _test_eof; 
	_test_eof202:  sm->cs = 202; goto _test_eof; 
	_test_eof203:  sm->cs = 203; goto _test_eof; 
	_test_eof204:  sm->cs = 204; goto _test_eof; 
	_test_eof205:  sm->cs = 205; goto _test_eof; 
	_test_eof206:  sm->cs = 206; goto _test_eof; 
	_test_eof207:  sm->cs = 207; goto _test_eof; 
	_test_eof208:  sm->cs = 208; goto _test_eof; 
	_test_eof209:  sm->cs = 209; goto _test_eof; 
	_test_eof210:  sm->cs = 210; goto _test_eof; 
	_test_eof1398:  sm->cs = 1398; goto _test_eof; 
	_test_eof211:  sm->cs = 211; goto _test_eof; 
	_test_eof212:  sm->cs = 212; goto _test_eof; 
	_test_eof213:  sm->cs = 213; goto _test_eof; 
	_test_eof214:  sm->cs = 214; goto _test_eof; 
	_test_eof215:  sm->cs = 215; goto _test_eof; 
	_test_eof216:  sm->cs = 216; goto _test_eof; 
	_test_eof217:  sm->cs = 217; goto _test_eof; 
	_test_eof218:  sm->cs = 218; goto _test_eof; 
	_test_eof1399:  sm->cs = 1399; goto _test_eof; 
	_test_eof219:  sm->cs = 219; goto _test_eof; 
	_test_eof220:  sm->cs = 220; goto _test_eof; 
	_test_eof221:  sm->cs = 221; goto _test_eof; 
	_test_eof222:  sm->cs = 222; goto _test_eof; 
	_test_eof223:  sm->cs = 223; goto _test_eof; 
	_test_eof224:  sm->cs = 224; goto _test_eof; 
	_test_eof225:  sm->cs = 225; goto _test_eof; 
	_test_eof226:  sm->cs = 226; goto _test_eof; 
	_test_eof227:  sm->cs = 227; goto _test_eof; 
	_test_eof228:  sm->cs = 228; goto _test_eof; 
	_test_eof229:  sm->cs = 229; goto _test_eof; 
	_test_eof230:  sm->cs = 230; goto _test_eof; 
	_test_eof231:  sm->cs = 231; goto _test_eof; 
	_test_eof232:  sm->cs = 232; goto _test_eof; 
	_test_eof233:  sm->cs = 233; goto _test_eof; 
	_test_eof234:  sm->cs = 234; goto _test_eof; 
	_test_eof235:  sm->cs = 235; goto _test_eof; 
	_test_eof236:  sm->cs = 236; goto _test_eof; 
	_test_eof237:  sm->cs = 237; goto _test_eof; 
	_test_eof238:  sm->cs = 238; goto _test_eof; 
	_test_eof239:  sm->cs = 239; goto _test_eof; 
	_test_eof240:  sm->cs = 240; goto _test_eof; 
	_test_eof241:  sm->cs = 241; goto _test_eof; 
	_test_eof242:  sm->cs = 242; goto _test_eof; 
	_test_eof243:  sm->cs = 243; goto _test_eof; 
	_test_eof244:  sm->cs = 244; goto _test_eof; 
	_test_eof1400:  sm->cs = 1400; goto _test_eof; 
	_test_eof1401:  sm->cs = 1401; goto _test_eof; 
	_test_eof245:  sm->cs = 245; goto _test_eof; 
	_test_eof246:  sm->cs = 246; goto _test_eof; 
	_test_eof247:  sm->cs = 247; goto _test_eof; 
	_test_eof248:  sm->cs = 248; goto _test_eof; 
	_test_eof249:  sm->cs = 249; goto _test_eof; 
	_test_eof250:  sm->cs = 250; goto _test_eof; 
	_test_eof251:  sm->cs = 251; goto _test_eof; 
	_test_eof252:  sm->cs = 252; goto _test_eof; 
	_test_eof253:  sm->cs = 253; goto _test_eof; 
	_test_eof254:  sm->cs = 254; goto _test_eof; 
	_test_eof255:  sm->cs = 255; goto _test_eof; 
	_test_eof256:  sm->cs = 256; goto _test_eof; 
	_test_eof1402:  sm->cs = 1402; goto _test_eof; 
	_test_eof257:  sm->cs = 257; goto _test_eof; 
	_test_eof258:  sm->cs = 258; goto _test_eof; 
	_test_eof259:  sm->cs = 259; goto _test_eof; 
	_test_eof260:  sm->cs = 260; goto _test_eof; 
	_test_eof261:  sm->cs = 261; goto _test_eof; 
	_test_eof262:  sm->cs = 262; goto _test_eof; 
	_test_eof1403:  sm->cs = 1403; goto _test_eof; 
	_test_eof263:  sm->cs = 263; goto _test_eof; 
	_test_eof264:  sm->cs = 264; goto _test_eof; 
	_test_eof265:  sm->cs = 265; goto _test_eof; 
	_test_eof266:  sm->cs = 266; goto _test_eof; 
	_test_eof267:  sm->cs = 267; goto _test_eof; 
	_test_eof268:  sm->cs = 268; goto _test_eof; 
	_test_eof269:  sm->cs = 269; goto _test_eof; 
	_test_eof270:  sm->cs = 270; goto _test_eof; 
	_test_eof271:  sm->cs = 271; goto _test_eof; 
	_test_eof272:  sm->cs = 272; goto _test_eof; 
	_test_eof273:  sm->cs = 273; goto _test_eof; 
	_test_eof274:  sm->cs = 274; goto _test_eof; 
	_test_eof275:  sm->cs = 275; goto _test_eof; 
	_test_eof276:  sm->cs = 276; goto _test_eof; 
	_test_eof277:  sm->cs = 277; goto _test_eof; 
	_test_eof278:  sm->cs = 278; goto _test_eof; 
	_test_eof279:  sm->cs = 279; goto _test_eof; 
	_test_eof280:  sm->cs = 280; goto _test_eof; 
	_test_eof281:  sm->cs = 281; goto _test_eof; 
	_test_eof282:  sm->cs = 282; goto _test_eof; 
	_test_eof283:  sm->cs = 283; goto _test_eof; 
	_test_eof284:  sm->cs = 284; goto _test_eof; 
	_test_eof285:  sm->cs = 285; goto _test_eof; 
	_test_eof286:  sm->cs = 286; goto _test_eof; 
	_test_eof287:  sm->cs = 287; goto _test_eof; 
	_test_eof288:  sm->cs = 288; goto _test_eof; 
	_test_eof289:  sm->cs = 289; goto _test_eof; 
	_test_eof290:  sm->cs = 290; goto _test_eof; 
	_test_eof291:  sm->cs = 291; goto _test_eof; 
	_test_eof292:  sm->cs = 292; goto _test_eof; 
	_test_eof293:  sm->cs = 293; goto _test_eof; 
	_test_eof1404:  sm->cs = 1404; goto _test_eof; 
	_test_eof294:  sm->cs = 294; goto _test_eof; 
	_test_eof295:  sm->cs = 295; goto _test_eof; 
	_test_eof296:  sm->cs = 296; goto _test_eof; 
	_test_eof297:  sm->cs = 297; goto _test_eof; 
	_test_eof298:  sm->cs = 298; goto _test_eof; 
	_test_eof299:  sm->cs = 299; goto _test_eof; 
	_test_eof300:  sm->cs = 300; goto _test_eof; 
	_test_eof301:  sm->cs = 301; goto _test_eof; 
	_test_eof302:  sm->cs = 302; goto _test_eof; 
	_test_eof303:  sm->cs = 303; goto _test_eof; 
	_test_eof304:  sm->cs = 304; goto _test_eof; 
	_test_eof305:  sm->cs = 305; goto _test_eof; 
	_test_eof306:  sm->cs = 306; goto _test_eof; 
	_test_eof307:  sm->cs = 307; goto _test_eof; 
	_test_eof308:  sm->cs = 308; goto _test_eof; 
	_test_eof309:  sm->cs = 309; goto _test_eof; 
	_test_eof310:  sm->cs = 310; goto _test_eof; 
	_test_eof311:  sm->cs = 311; goto _test_eof; 
	_test_eof312:  sm->cs = 312; goto _test_eof; 
	_test_eof313:  sm->cs = 313; goto _test_eof; 
	_test_eof314:  sm->cs = 314; goto _test_eof; 
	_test_eof315:  sm->cs = 315; goto _test_eof; 
	_test_eof316:  sm->cs = 316; goto _test_eof; 
	_test_eof317:  sm->cs = 317; goto _test_eof; 
	_test_eof318:  sm->cs = 318; goto _test_eof; 
	_test_eof319:  sm->cs = 319; goto _test_eof; 
	_test_eof320:  sm->cs = 320; goto _test_eof; 
	_test_eof321:  sm->cs = 321; goto _test_eof; 
	_test_eof322:  sm->cs = 322; goto _test_eof; 
	_test_eof323:  sm->cs = 323; goto _test_eof; 
	_test_eof324:  sm->cs = 324; goto _test_eof; 
	_test_eof325:  sm->cs = 325; goto _test_eof; 
	_test_eof326:  sm->cs = 326; goto _test_eof; 
	_test_eof327:  sm->cs = 327; goto _test_eof; 
	_test_eof328:  sm->cs = 328; goto _test_eof; 
	_test_eof329:  sm->cs = 329; goto _test_eof; 
	_test_eof330:  sm->cs = 330; goto _test_eof; 
	_test_eof331:  sm->cs = 331; goto _test_eof; 
	_test_eof332:  sm->cs = 332; goto _test_eof; 
	_test_eof333:  sm->cs = 333; goto _test_eof; 
	_test_eof334:  sm->cs = 334; goto _test_eof; 
	_test_eof1405:  sm->cs = 1405; goto _test_eof; 
	_test_eof335:  sm->cs = 335; goto _test_eof; 
	_test_eof336:  sm->cs = 336; goto _test_eof; 
	_test_eof337:  sm->cs = 337; goto _test_eof; 
	_test_eof1406:  sm->cs = 1406; goto _test_eof; 
	_test_eof338:  sm->cs = 338; goto _test_eof; 
	_test_eof339:  sm->cs = 339; goto _test_eof; 
	_test_eof340:  sm->cs = 340; goto _test_eof; 
	_test_eof341:  sm->cs = 341; goto _test_eof; 
	_test_eof342:  sm->cs = 342; goto _test_eof; 
	_test_eof343:  sm->cs = 343; goto _test_eof; 
	_test_eof344:  sm->cs = 344; goto _test_eof; 
	_test_eof345:  sm->cs = 345; goto _test_eof; 
	_test_eof346:  sm->cs = 346; goto _test_eof; 
	_test_eof347:  sm->cs = 347; goto _test_eof; 
	_test_eof348:  sm->cs = 348; goto _test_eof; 
	_test_eof1407:  sm->cs = 1407; goto _test_eof; 
	_test_eof349:  sm->cs = 349; goto _test_eof; 
	_test_eof350:  sm->cs = 350; goto _test_eof; 
	_test_eof351:  sm->cs = 351; goto _test_eof; 
	_test_eof352:  sm->cs = 352; goto _test_eof; 
	_test_eof353:  sm->cs = 353; goto _test_eof; 
	_test_eof354:  sm->cs = 354; goto _test_eof; 
	_test_eof355:  sm->cs = 355; goto _test_eof; 
	_test_eof356:  sm->cs = 356; goto _test_eof; 
	_test_eof357:  sm->cs = 357; goto _test_eof; 
	_test_eof358:  sm->cs = 358; goto _test_eof; 
	_test_eof359:  sm->cs = 359; goto _test_eof; 
	_test_eof360:  sm->cs = 360; goto _test_eof; 
	_test_eof361:  sm->cs = 361; goto _test_eof; 
	_test_eof1408:  sm->cs = 1408; goto _test_eof; 
	_test_eof362:  sm->cs = 362; goto _test_eof; 
	_test_eof363:  sm->cs = 363; goto _test_eof; 
	_test_eof364:  sm->cs = 364; goto _test_eof; 
	_test_eof365:  sm->cs = 365; goto _test_eof; 
	_test_eof366:  sm->cs = 366; goto _test_eof; 
	_test_eof367:  sm->cs = 367; goto _test_eof; 
	_test_eof368:  sm->cs = 368; goto _test_eof; 
	_test_eof369:  sm->cs = 369; goto _test_eof; 
	_test_eof370:  sm->cs = 370; goto _test_eof; 
	_test_eof371:  sm->cs = 371; goto _test_eof; 
	_test_eof372:  sm->cs = 372; goto _test_eof; 
	_test_eof373:  sm->cs = 373; goto _test_eof; 
	_test_eof374:  sm->cs = 374; goto _test_eof; 
	_test_eof375:  sm->cs = 375; goto _test_eof; 
	_test_eof376:  sm->cs = 376; goto _test_eof; 
	_test_eof377:  sm->cs = 377; goto _test_eof; 
	_test_eof378:  sm->cs = 378; goto _test_eof; 
	_test_eof379:  sm->cs = 379; goto _test_eof; 
	_test_eof380:  sm->cs = 380; goto _test_eof; 
	_test_eof381:  sm->cs = 381; goto _test_eof; 
	_test_eof382:  sm->cs = 382; goto _test_eof; 
	_test_eof383:  sm->cs = 383; goto _test_eof; 
	_test_eof1409:  sm->cs = 1409; goto _test_eof; 
	_test_eof384:  sm->cs = 384; goto _test_eof; 
	_test_eof385:  sm->cs = 385; goto _test_eof; 
	_test_eof386:  sm->cs = 386; goto _test_eof; 
	_test_eof387:  sm->cs = 387; goto _test_eof; 
	_test_eof388:  sm->cs = 388; goto _test_eof; 
	_test_eof389:  sm->cs = 389; goto _test_eof; 
	_test_eof390:  sm->cs = 390; goto _test_eof; 
	_test_eof391:  sm->cs = 391; goto _test_eof; 
	_test_eof392:  sm->cs = 392; goto _test_eof; 
	_test_eof393:  sm->cs = 393; goto _test_eof; 
	_test_eof394:  sm->cs = 394; goto _test_eof; 
	_test_eof1410:  sm->cs = 1410; goto _test_eof; 
	_test_eof395:  sm->cs = 395; goto _test_eof; 
	_test_eof396:  sm->cs = 396; goto _test_eof; 
	_test_eof397:  sm->cs = 397; goto _test_eof; 
	_test_eof398:  sm->cs = 398; goto _test_eof; 
	_test_eof399:  sm->cs = 399; goto _test_eof; 
	_test_eof400:  sm->cs = 400; goto _test_eof; 
	_test_eof401:  sm->cs = 401; goto _test_eof; 
	_test_eof402:  sm->cs = 402; goto _test_eof; 
	_test_eof403:  sm->cs = 403; goto _test_eof; 
	_test_eof404:  sm->cs = 404; goto _test_eof; 
	_test_eof405:  sm->cs = 405; goto _test_eof; 
	_test_eof1411:  sm->cs = 1411; goto _test_eof; 
	_test_eof406:  sm->cs = 406; goto _test_eof; 
	_test_eof407:  sm->cs = 407; goto _test_eof; 
	_test_eof408:  sm->cs = 408; goto _test_eof; 
	_test_eof409:  sm->cs = 409; goto _test_eof; 
	_test_eof410:  sm->cs = 410; goto _test_eof; 
	_test_eof411:  sm->cs = 411; goto _test_eof; 
	_test_eof412:  sm->cs = 412; goto _test_eof; 
	_test_eof413:  sm->cs = 413; goto _test_eof; 
	_test_eof414:  sm->cs = 414; goto _test_eof; 
	_test_eof1412:  sm->cs = 1412; goto _test_eof; 
	_test_eof1413:  sm->cs = 1413; goto _test_eof; 
	_test_eof415:  sm->cs = 415; goto _test_eof; 
	_test_eof416:  sm->cs = 416; goto _test_eof; 
	_test_eof417:  sm->cs = 417; goto _test_eof; 
	_test_eof418:  sm->cs = 418; goto _test_eof; 
	_test_eof1414:  sm->cs = 1414; goto _test_eof; 
	_test_eof1415:  sm->cs = 1415; goto _test_eof; 
	_test_eof419:  sm->cs = 419; goto _test_eof; 
	_test_eof420:  sm->cs = 420; goto _test_eof; 
	_test_eof421:  sm->cs = 421; goto _test_eof; 
	_test_eof422:  sm->cs = 422; goto _test_eof; 
	_test_eof423:  sm->cs = 423; goto _test_eof; 
	_test_eof424:  sm->cs = 424; goto _test_eof; 
	_test_eof425:  sm->cs = 425; goto _test_eof; 
	_test_eof426:  sm->cs = 426; goto _test_eof; 
	_test_eof427:  sm->cs = 427; goto _test_eof; 
	_test_eof1416:  sm->cs = 1416; goto _test_eof; 
	_test_eof1417:  sm->cs = 1417; goto _test_eof; 
	_test_eof428:  sm->cs = 428; goto _test_eof; 
	_test_eof429:  sm->cs = 429; goto _test_eof; 
	_test_eof430:  sm->cs = 430; goto _test_eof; 
	_test_eof431:  sm->cs = 431; goto _test_eof; 
	_test_eof432:  sm->cs = 432; goto _test_eof; 
	_test_eof433:  sm->cs = 433; goto _test_eof; 
	_test_eof434:  sm->cs = 434; goto _test_eof; 
	_test_eof435:  sm->cs = 435; goto _test_eof; 
	_test_eof436:  sm->cs = 436; goto _test_eof; 
	_test_eof437:  sm->cs = 437; goto _test_eof; 
	_test_eof438:  sm->cs = 438; goto _test_eof; 
	_test_eof439:  sm->cs = 439; goto _test_eof; 
	_test_eof440:  sm->cs = 440; goto _test_eof; 
	_test_eof441:  sm->cs = 441; goto _test_eof; 
	_test_eof442:  sm->cs = 442; goto _test_eof; 
	_test_eof443:  sm->cs = 443; goto _test_eof; 
	_test_eof444:  sm->cs = 444; goto _test_eof; 
	_test_eof445:  sm->cs = 445; goto _test_eof; 
	_test_eof446:  sm->cs = 446; goto _test_eof; 
	_test_eof447:  sm->cs = 447; goto _test_eof; 
	_test_eof448:  sm->cs = 448; goto _test_eof; 
	_test_eof449:  sm->cs = 449; goto _test_eof; 
	_test_eof450:  sm->cs = 450; goto _test_eof; 
	_test_eof451:  sm->cs = 451; goto _test_eof; 
	_test_eof452:  sm->cs = 452; goto _test_eof; 
	_test_eof453:  sm->cs = 453; goto _test_eof; 
	_test_eof454:  sm->cs = 454; goto _test_eof; 
	_test_eof455:  sm->cs = 455; goto _test_eof; 
	_test_eof456:  sm->cs = 456; goto _test_eof; 
	_test_eof457:  sm->cs = 457; goto _test_eof; 
	_test_eof458:  sm->cs = 458; goto _test_eof; 
	_test_eof459:  sm->cs = 459; goto _test_eof; 
	_test_eof460:  sm->cs = 460; goto _test_eof; 
	_test_eof461:  sm->cs = 461; goto _test_eof; 
	_test_eof462:  sm->cs = 462; goto _test_eof; 
	_test_eof463:  sm->cs = 463; goto _test_eof; 
	_test_eof464:  sm->cs = 464; goto _test_eof; 
	_test_eof465:  sm->cs = 465; goto _test_eof; 
	_test_eof466:  sm->cs = 466; goto _test_eof; 
	_test_eof467:  sm->cs = 467; goto _test_eof; 
	_test_eof468:  sm->cs = 468; goto _test_eof; 
	_test_eof469:  sm->cs = 469; goto _test_eof; 
	_test_eof470:  sm->cs = 470; goto _test_eof; 
	_test_eof1418:  sm->cs = 1418; goto _test_eof; 
	_test_eof1419:  sm->cs = 1419; goto _test_eof; 
	_test_eof471:  sm->cs = 471; goto _test_eof; 
	_test_eof1420:  sm->cs = 1420; goto _test_eof; 
	_test_eof1421:  sm->cs = 1421; goto _test_eof; 
	_test_eof472:  sm->cs = 472; goto _test_eof; 
	_test_eof473:  sm->cs = 473; goto _test_eof; 
	_test_eof474:  sm->cs = 474; goto _test_eof; 
	_test_eof475:  sm->cs = 475; goto _test_eof; 
	_test_eof476:  sm->cs = 476; goto _test_eof; 
	_test_eof477:  sm->cs = 477; goto _test_eof; 
	_test_eof478:  sm->cs = 478; goto _test_eof; 
	_test_eof479:  sm->cs = 479; goto _test_eof; 
	_test_eof1422:  sm->cs = 1422; goto _test_eof; 
	_test_eof1423:  sm->cs = 1423; goto _test_eof; 
	_test_eof480:  sm->cs = 480; goto _test_eof; 
	_test_eof1424:  sm->cs = 1424; goto _test_eof; 
	_test_eof481:  sm->cs = 481; goto _test_eof; 
	_test_eof482:  sm->cs = 482; goto _test_eof; 
	_test_eof483:  sm->cs = 483; goto _test_eof; 
	_test_eof484:  sm->cs = 484; goto _test_eof; 
	_test_eof485:  sm->cs = 485; goto _test_eof; 
	_test_eof486:  sm->cs = 486; goto _test_eof; 
	_test_eof487:  sm->cs = 487; goto _test_eof; 
	_test_eof488:  sm->cs = 488; goto _test_eof; 
	_test_eof489:  sm->cs = 489; goto _test_eof; 
	_test_eof490:  sm->cs = 490; goto _test_eof; 
	_test_eof491:  sm->cs = 491; goto _test_eof; 
	_test_eof492:  sm->cs = 492; goto _test_eof; 
	_test_eof493:  sm->cs = 493; goto _test_eof; 
	_test_eof494:  sm->cs = 494; goto _test_eof; 
	_test_eof495:  sm->cs = 495; goto _test_eof; 
	_test_eof496:  sm->cs = 496; goto _test_eof; 
	_test_eof497:  sm->cs = 497; goto _test_eof; 
	_test_eof498:  sm->cs = 498; goto _test_eof; 
	_test_eof1425:  sm->cs = 1425; goto _test_eof; 
	_test_eof499:  sm->cs = 499; goto _test_eof; 
	_test_eof500:  sm->cs = 500; goto _test_eof; 
	_test_eof501:  sm->cs = 501; goto _test_eof; 
	_test_eof502:  sm->cs = 502; goto _test_eof; 
	_test_eof503:  sm->cs = 503; goto _test_eof; 
	_test_eof504:  sm->cs = 504; goto _test_eof; 
	_test_eof505:  sm->cs = 505; goto _test_eof; 
	_test_eof506:  sm->cs = 506; goto _test_eof; 
	_test_eof507:  sm->cs = 507; goto _test_eof; 
	_test_eof1426:  sm->cs = 1426; goto _test_eof; 
	_test_eof1427:  sm->cs = 1427; goto _test_eof; 
	_test_eof1428:  sm->cs = 1428; goto _test_eof; 
	_test_eof1429:  sm->cs = 1429; goto _test_eof; 
	_test_eof1430:  sm->cs = 1430; goto _test_eof; 
	_test_eof1431:  sm->cs = 1431; goto _test_eof; 
	_test_eof508:  sm->cs = 508; goto _test_eof; 
	_test_eof509:  sm->cs = 509; goto _test_eof; 
	_test_eof1432:  sm->cs = 1432; goto _test_eof; 
	_test_eof1433:  sm->cs = 1433; goto _test_eof; 
	_test_eof1434:  sm->cs = 1434; goto _test_eof; 
	_test_eof1435:  sm->cs = 1435; goto _test_eof; 
	_test_eof1436:  sm->cs = 1436; goto _test_eof; 
	_test_eof1437:  sm->cs = 1437; goto _test_eof; 
	_test_eof1438:  sm->cs = 1438; goto _test_eof; 
	_test_eof1439:  sm->cs = 1439; goto _test_eof; 
	_test_eof1440:  sm->cs = 1440; goto _test_eof; 
	_test_eof1441:  sm->cs = 1441; goto _test_eof; 
	_test_eof1442:  sm->cs = 1442; goto _test_eof; 
	_test_eof1443:  sm->cs = 1443; goto _test_eof; 
	_test_eof510:  sm->cs = 510; goto _test_eof; 
	_test_eof511:  sm->cs = 511; goto _test_eof; 
	_test_eof512:  sm->cs = 512; goto _test_eof; 
	_test_eof513:  sm->cs = 513; goto _test_eof; 
	_test_eof514:  sm->cs = 514; goto _test_eof; 
	_test_eof515:  sm->cs = 515; goto _test_eof; 
	_test_eof516:  sm->cs = 516; goto _test_eof; 
	_test_eof517:  sm->cs = 517; goto _test_eof; 
	_test_eof518:  sm->cs = 518; goto _test_eof; 
	_test_eof519:  sm->cs = 519; goto _test_eof; 
	_test_eof1444:  sm->cs = 1444; goto _test_eof; 
	_test_eof1445:  sm->cs = 1445; goto _test_eof; 
	_test_eof1446:  sm->cs = 1446; goto _test_eof; 
	_test_eof1447:  sm->cs = 1447; goto _test_eof; 
	_test_eof520:  sm->cs = 520; goto _test_eof; 
	_test_eof521:  sm->cs = 521; goto _test_eof; 
	_test_eof1448:  sm->cs = 1448; goto _test_eof; 
	_test_eof1449:  sm->cs = 1449; goto _test_eof; 
	_test_eof1450:  sm->cs = 1450; goto _test_eof; 
	_test_eof1451:  sm->cs = 1451; goto _test_eof; 
	_test_eof1452:  sm->cs = 1452; goto _test_eof; 
	_test_eof1453:  sm->cs = 1453; goto _test_eof; 
	_test_eof1454:  sm->cs = 1454; goto _test_eof; 
	_test_eof1455:  sm->cs = 1455; goto _test_eof; 
	_test_eof1456:  sm->cs = 1456; goto _test_eof; 
	_test_eof1457:  sm->cs = 1457; goto _test_eof; 
	_test_eof1458:  sm->cs = 1458; goto _test_eof; 
	_test_eof1459:  sm->cs = 1459; goto _test_eof; 
	_test_eof522:  sm->cs = 522; goto _test_eof; 
	_test_eof523:  sm->cs = 523; goto _test_eof; 
	_test_eof524:  sm->cs = 524; goto _test_eof; 
	_test_eof525:  sm->cs = 525; goto _test_eof; 
	_test_eof526:  sm->cs = 526; goto _test_eof; 
	_test_eof527:  sm->cs = 527; goto _test_eof; 
	_test_eof528:  sm->cs = 528; goto _test_eof; 
	_test_eof529:  sm->cs = 529; goto _test_eof; 
	_test_eof530:  sm->cs = 530; goto _test_eof; 
	_test_eof531:  sm->cs = 531; goto _test_eof; 
	_test_eof1460:  sm->cs = 1460; goto _test_eof; 
	_test_eof1461:  sm->cs = 1461; goto _test_eof; 
	_test_eof1462:  sm->cs = 1462; goto _test_eof; 
	_test_eof1463:  sm->cs = 1463; goto _test_eof; 
	_test_eof1464:  sm->cs = 1464; goto _test_eof; 
	_test_eof1465:  sm->cs = 1465; goto _test_eof; 
	_test_eof1466:  sm->cs = 1466; goto _test_eof; 
	_test_eof532:  sm->cs = 532; goto _test_eof; 
	_test_eof533:  sm->cs = 533; goto _test_eof; 
	_test_eof1467:  sm->cs = 1467; goto _test_eof; 
	_test_eof1468:  sm->cs = 1468; goto _test_eof; 
	_test_eof1469:  sm->cs = 1469; goto _test_eof; 
	_test_eof1470:  sm->cs = 1470; goto _test_eof; 
	_test_eof1471:  sm->cs = 1471; goto _test_eof; 
	_test_eof1472:  sm->cs = 1472; goto _test_eof; 
	_test_eof1473:  sm->cs = 1473; goto _test_eof; 
	_test_eof1474:  sm->cs = 1474; goto _test_eof; 
	_test_eof1475:  sm->cs = 1475; goto _test_eof; 
	_test_eof1476:  sm->cs = 1476; goto _test_eof; 
	_test_eof1477:  sm->cs = 1477; goto _test_eof; 
	_test_eof1478:  sm->cs = 1478; goto _test_eof; 
	_test_eof534:  sm->cs = 534; goto _test_eof; 
	_test_eof535:  sm->cs = 535; goto _test_eof; 
	_test_eof536:  sm->cs = 536; goto _test_eof; 
	_test_eof537:  sm->cs = 537; goto _test_eof; 
	_test_eof538:  sm->cs = 538; goto _test_eof; 
	_test_eof539:  sm->cs = 539; goto _test_eof; 
	_test_eof540:  sm->cs = 540; goto _test_eof; 
	_test_eof541:  sm->cs = 541; goto _test_eof; 
	_test_eof542:  sm->cs = 542; goto _test_eof; 
	_test_eof543:  sm->cs = 543; goto _test_eof; 
	_test_eof1479:  sm->cs = 1479; goto _test_eof; 
	_test_eof1480:  sm->cs = 1480; goto _test_eof; 
	_test_eof1481:  sm->cs = 1481; goto _test_eof; 
	_test_eof1482:  sm->cs = 1482; goto _test_eof; 
	_test_eof1483:  sm->cs = 1483; goto _test_eof; 
	_test_eof544:  sm->cs = 544; goto _test_eof; 
	_test_eof545:  sm->cs = 545; goto _test_eof; 
	_test_eof1484:  sm->cs = 1484; goto _test_eof; 
	_test_eof546:  sm->cs = 546; goto _test_eof; 
	_test_eof1485:  sm->cs = 1485; goto _test_eof; 
	_test_eof1486:  sm->cs = 1486; goto _test_eof; 
	_test_eof1487:  sm->cs = 1487; goto _test_eof; 
	_test_eof1488:  sm->cs = 1488; goto _test_eof; 
	_test_eof1489:  sm->cs = 1489; goto _test_eof; 
	_test_eof1490:  sm->cs = 1490; goto _test_eof; 
	_test_eof1491:  sm->cs = 1491; goto _test_eof; 
	_test_eof1492:  sm->cs = 1492; goto _test_eof; 
	_test_eof1493:  sm->cs = 1493; goto _test_eof; 
	_test_eof1494:  sm->cs = 1494; goto _test_eof; 
	_test_eof1495:  sm->cs = 1495; goto _test_eof; 
	_test_eof1496:  sm->cs = 1496; goto _test_eof; 
	_test_eof547:  sm->cs = 547; goto _test_eof; 
	_test_eof548:  sm->cs = 548; goto _test_eof; 
	_test_eof549:  sm->cs = 549; goto _test_eof; 
	_test_eof550:  sm->cs = 550; goto _test_eof; 
	_test_eof551:  sm->cs = 551; goto _test_eof; 
	_test_eof552:  sm->cs = 552; goto _test_eof; 
	_test_eof553:  sm->cs = 553; goto _test_eof; 
	_test_eof554:  sm->cs = 554; goto _test_eof; 
	_test_eof555:  sm->cs = 555; goto _test_eof; 
	_test_eof556:  sm->cs = 556; goto _test_eof; 
	_test_eof1497:  sm->cs = 1497; goto _test_eof; 
	_test_eof1498:  sm->cs = 1498; goto _test_eof; 
	_test_eof1499:  sm->cs = 1499; goto _test_eof; 
	_test_eof1500:  sm->cs = 1500; goto _test_eof; 
	_test_eof1501:  sm->cs = 1501; goto _test_eof; 
	_test_eof557:  sm->cs = 557; goto _test_eof; 
	_test_eof558:  sm->cs = 558; goto _test_eof; 
	_test_eof1502:  sm->cs = 1502; goto _test_eof; 
	_test_eof1503:  sm->cs = 1503; goto _test_eof; 
	_test_eof1504:  sm->cs = 1504; goto _test_eof; 
	_test_eof1505:  sm->cs = 1505; goto _test_eof; 
	_test_eof1506:  sm->cs = 1506; goto _test_eof; 
	_test_eof1507:  sm->cs = 1507; goto _test_eof; 
	_test_eof1508:  sm->cs = 1508; goto _test_eof; 
	_test_eof1509:  sm->cs = 1509; goto _test_eof; 
	_test_eof1510:  sm->cs = 1510; goto _test_eof; 
	_test_eof1511:  sm->cs = 1511; goto _test_eof; 
	_test_eof1512:  sm->cs = 1512; goto _test_eof; 
	_test_eof1513:  sm->cs = 1513; goto _test_eof; 
	_test_eof559:  sm->cs = 559; goto _test_eof; 
	_test_eof560:  sm->cs = 560; goto _test_eof; 
	_test_eof561:  sm->cs = 561; goto _test_eof; 
	_test_eof562:  sm->cs = 562; goto _test_eof; 
	_test_eof563:  sm->cs = 563; goto _test_eof; 
	_test_eof564:  sm->cs = 564; goto _test_eof; 
	_test_eof565:  sm->cs = 565; goto _test_eof; 
	_test_eof566:  sm->cs = 566; goto _test_eof; 
	_test_eof567:  sm->cs = 567; goto _test_eof; 
	_test_eof568:  sm->cs = 568; goto _test_eof; 
	_test_eof1514:  sm->cs = 1514; goto _test_eof; 
	_test_eof1515:  sm->cs = 1515; goto _test_eof; 
	_test_eof1516:  sm->cs = 1516; goto _test_eof; 
	_test_eof1517:  sm->cs = 1517; goto _test_eof; 
	_test_eof569:  sm->cs = 569; goto _test_eof; 
	_test_eof570:  sm->cs = 570; goto _test_eof; 
	_test_eof571:  sm->cs = 571; goto _test_eof; 
	_test_eof572:  sm->cs = 572; goto _test_eof; 
	_test_eof573:  sm->cs = 573; goto _test_eof; 
	_test_eof574:  sm->cs = 574; goto _test_eof; 
	_test_eof575:  sm->cs = 575; goto _test_eof; 
	_test_eof576:  sm->cs = 576; goto _test_eof; 
	_test_eof577:  sm->cs = 577; goto _test_eof; 
	_test_eof1518:  sm->cs = 1518; goto _test_eof; 
	_test_eof578:  sm->cs = 578; goto _test_eof; 
	_test_eof579:  sm->cs = 579; goto _test_eof; 
	_test_eof580:  sm->cs = 580; goto _test_eof; 
	_test_eof581:  sm->cs = 581; goto _test_eof; 
	_test_eof582:  sm->cs = 582; goto _test_eof; 
	_test_eof583:  sm->cs = 583; goto _test_eof; 
	_test_eof584:  sm->cs = 584; goto _test_eof; 
	_test_eof585:  sm->cs = 585; goto _test_eof; 
	_test_eof586:  sm->cs = 586; goto _test_eof; 
	_test_eof587:  sm->cs = 587; goto _test_eof; 
	_test_eof1519:  sm->cs = 1519; goto _test_eof; 
	_test_eof588:  sm->cs = 588; goto _test_eof; 
	_test_eof589:  sm->cs = 589; goto _test_eof; 
	_test_eof590:  sm->cs = 590; goto _test_eof; 
	_test_eof591:  sm->cs = 591; goto _test_eof; 
	_test_eof592:  sm->cs = 592; goto _test_eof; 
	_test_eof593:  sm->cs = 593; goto _test_eof; 
	_test_eof594:  sm->cs = 594; goto _test_eof; 
	_test_eof595:  sm->cs = 595; goto _test_eof; 
	_test_eof596:  sm->cs = 596; goto _test_eof; 
	_test_eof597:  sm->cs = 597; goto _test_eof; 
	_test_eof598:  sm->cs = 598; goto _test_eof; 
	_test_eof1520:  sm->cs = 1520; goto _test_eof; 
	_test_eof599:  sm->cs = 599; goto _test_eof; 
	_test_eof600:  sm->cs = 600; goto _test_eof; 
	_test_eof601:  sm->cs = 601; goto _test_eof; 
	_test_eof602:  sm->cs = 602; goto _test_eof; 
	_test_eof603:  sm->cs = 603; goto _test_eof; 
	_test_eof604:  sm->cs = 604; goto _test_eof; 
	_test_eof605:  sm->cs = 605; goto _test_eof; 
	_test_eof606:  sm->cs = 606; goto _test_eof; 
	_test_eof607:  sm->cs = 607; goto _test_eof; 
	_test_eof608:  sm->cs = 608; goto _test_eof; 
	_test_eof609:  sm->cs = 609; goto _test_eof; 
	_test_eof610:  sm->cs = 610; goto _test_eof; 
	_test_eof611:  sm->cs = 611; goto _test_eof; 
	_test_eof1521:  sm->cs = 1521; goto _test_eof; 
	_test_eof612:  sm->cs = 612; goto _test_eof; 
	_test_eof613:  sm->cs = 613; goto _test_eof; 
	_test_eof614:  sm->cs = 614; goto _test_eof; 
	_test_eof615:  sm->cs = 615; goto _test_eof; 
	_test_eof616:  sm->cs = 616; goto _test_eof; 
	_test_eof617:  sm->cs = 617; goto _test_eof; 
	_test_eof618:  sm->cs = 618; goto _test_eof; 
	_test_eof619:  sm->cs = 619; goto _test_eof; 
	_test_eof620:  sm->cs = 620; goto _test_eof; 
	_test_eof621:  sm->cs = 621; goto _test_eof; 
	_test_eof1522:  sm->cs = 1522; goto _test_eof; 
	_test_eof1523:  sm->cs = 1523; goto _test_eof; 
	_test_eof1524:  sm->cs = 1524; goto _test_eof; 
	_test_eof1525:  sm->cs = 1525; goto _test_eof; 
	_test_eof1526:  sm->cs = 1526; goto _test_eof; 
	_test_eof622:  sm->cs = 622; goto _test_eof; 
	_test_eof623:  sm->cs = 623; goto _test_eof; 
	_test_eof624:  sm->cs = 624; goto _test_eof; 
	_test_eof625:  sm->cs = 625; goto _test_eof; 
	_test_eof626:  sm->cs = 626; goto _test_eof; 
	_test_eof627:  sm->cs = 627; goto _test_eof; 
	_test_eof628:  sm->cs = 628; goto _test_eof; 
	_test_eof629:  sm->cs = 629; goto _test_eof; 
	_test_eof630:  sm->cs = 630; goto _test_eof; 
	_test_eof1527:  sm->cs = 1527; goto _test_eof; 
	_test_eof1528:  sm->cs = 1528; goto _test_eof; 
	_test_eof1529:  sm->cs = 1529; goto _test_eof; 
	_test_eof1530:  sm->cs = 1530; goto _test_eof; 
	_test_eof1531:  sm->cs = 1531; goto _test_eof; 
	_test_eof1532:  sm->cs = 1532; goto _test_eof; 
	_test_eof1533:  sm->cs = 1533; goto _test_eof; 
	_test_eof1534:  sm->cs = 1534; goto _test_eof; 
	_test_eof1535:  sm->cs = 1535; goto _test_eof; 
	_test_eof1536:  sm->cs = 1536; goto _test_eof; 
	_test_eof1537:  sm->cs = 1537; goto _test_eof; 
	_test_eof1538:  sm->cs = 1538; goto _test_eof; 
	_test_eof631:  sm->cs = 631; goto _test_eof; 
	_test_eof632:  sm->cs = 632; goto _test_eof; 
	_test_eof633:  sm->cs = 633; goto _test_eof; 
	_test_eof634:  sm->cs = 634; goto _test_eof; 
	_test_eof635:  sm->cs = 635; goto _test_eof; 
	_test_eof636:  sm->cs = 636; goto _test_eof; 
	_test_eof637:  sm->cs = 637; goto _test_eof; 
	_test_eof638:  sm->cs = 638; goto _test_eof; 
	_test_eof639:  sm->cs = 639; goto _test_eof; 
	_test_eof640:  sm->cs = 640; goto _test_eof; 
	_test_eof1539:  sm->cs = 1539; goto _test_eof; 
	_test_eof1540:  sm->cs = 1540; goto _test_eof; 
	_test_eof1541:  sm->cs = 1541; goto _test_eof; 
	_test_eof1542:  sm->cs = 1542; goto _test_eof; 
	_test_eof1543:  sm->cs = 1543; goto _test_eof; 
	_test_eof641:  sm->cs = 641; goto _test_eof; 
	_test_eof642:  sm->cs = 642; goto _test_eof; 
	_test_eof643:  sm->cs = 643; goto _test_eof; 
	_test_eof644:  sm->cs = 644; goto _test_eof; 
	_test_eof645:  sm->cs = 645; goto _test_eof; 
	_test_eof1544:  sm->cs = 1544; goto _test_eof; 
	_test_eof646:  sm->cs = 646; goto _test_eof; 
	_test_eof647:  sm->cs = 647; goto _test_eof; 
	_test_eof648:  sm->cs = 648; goto _test_eof; 
	_test_eof649:  sm->cs = 649; goto _test_eof; 
	_test_eof650:  sm->cs = 650; goto _test_eof; 
	_test_eof651:  sm->cs = 651; goto _test_eof; 
	_test_eof652:  sm->cs = 652; goto _test_eof; 
	_test_eof653:  sm->cs = 653; goto _test_eof; 
	_test_eof654:  sm->cs = 654; goto _test_eof; 
	_test_eof655:  sm->cs = 655; goto _test_eof; 
	_test_eof656:  sm->cs = 656; goto _test_eof; 
	_test_eof657:  sm->cs = 657; goto _test_eof; 
	_test_eof658:  sm->cs = 658; goto _test_eof; 
	_test_eof659:  sm->cs = 659; goto _test_eof; 
	_test_eof660:  sm->cs = 660; goto _test_eof; 
	_test_eof661:  sm->cs = 661; goto _test_eof; 
	_test_eof662:  sm->cs = 662; goto _test_eof; 
	_test_eof663:  sm->cs = 663; goto _test_eof; 
	_test_eof664:  sm->cs = 664; goto _test_eof; 
	_test_eof665:  sm->cs = 665; goto _test_eof; 
	_test_eof666:  sm->cs = 666; goto _test_eof; 
	_test_eof1545:  sm->cs = 1545; goto _test_eof; 
	_test_eof1546:  sm->cs = 1546; goto _test_eof; 
	_test_eof1547:  sm->cs = 1547; goto _test_eof; 
	_test_eof667:  sm->cs = 667; goto _test_eof; 
	_test_eof668:  sm->cs = 668; goto _test_eof; 
	_test_eof1548:  sm->cs = 1548; goto _test_eof; 
	_test_eof1549:  sm->cs = 1549; goto _test_eof; 
	_test_eof1550:  sm->cs = 1550; goto _test_eof; 
	_test_eof1551:  sm->cs = 1551; goto _test_eof; 
	_test_eof1552:  sm->cs = 1552; goto _test_eof; 
	_test_eof1553:  sm->cs = 1553; goto _test_eof; 
	_test_eof1554:  sm->cs = 1554; goto _test_eof; 
	_test_eof1555:  sm->cs = 1555; goto _test_eof; 
	_test_eof1556:  sm->cs = 1556; goto _test_eof; 
	_test_eof1557:  sm->cs = 1557; goto _test_eof; 
	_test_eof1558:  sm->cs = 1558; goto _test_eof; 
	_test_eof1559:  sm->cs = 1559; goto _test_eof; 
	_test_eof669:  sm->cs = 669; goto _test_eof; 
	_test_eof670:  sm->cs = 670; goto _test_eof; 
	_test_eof671:  sm->cs = 671; goto _test_eof; 
	_test_eof672:  sm->cs = 672; goto _test_eof; 
	_test_eof673:  sm->cs = 673; goto _test_eof; 
	_test_eof674:  sm->cs = 674; goto _test_eof; 
	_test_eof675:  sm->cs = 675; goto _test_eof; 
	_test_eof676:  sm->cs = 676; goto _test_eof; 
	_test_eof677:  sm->cs = 677; goto _test_eof; 
	_test_eof678:  sm->cs = 678; goto _test_eof; 
	_test_eof1560:  sm->cs = 1560; goto _test_eof; 
	_test_eof1561:  sm->cs = 1561; goto _test_eof; 
	_test_eof679:  sm->cs = 679; goto _test_eof; 
	_test_eof680:  sm->cs = 680; goto _test_eof; 
	_test_eof1562:  sm->cs = 1562; goto _test_eof; 
	_test_eof1563:  sm->cs = 1563; goto _test_eof; 
	_test_eof1564:  sm->cs = 1564; goto _test_eof; 
	_test_eof1565:  sm->cs = 1565; goto _test_eof; 
	_test_eof1566:  sm->cs = 1566; goto _test_eof; 
	_test_eof1567:  sm->cs = 1567; goto _test_eof; 
	_test_eof1568:  sm->cs = 1568; goto _test_eof; 
	_test_eof1569:  sm->cs = 1569; goto _test_eof; 
	_test_eof1570:  sm->cs = 1570; goto _test_eof; 
	_test_eof1571:  sm->cs = 1571; goto _test_eof; 
	_test_eof1572:  sm->cs = 1572; goto _test_eof; 
	_test_eof1573:  sm->cs = 1573; goto _test_eof; 
	_test_eof681:  sm->cs = 681; goto _test_eof; 
	_test_eof682:  sm->cs = 682; goto _test_eof; 
	_test_eof683:  sm->cs = 683; goto _test_eof; 
	_test_eof684:  sm->cs = 684; goto _test_eof; 
	_test_eof685:  sm->cs = 685; goto _test_eof; 
	_test_eof686:  sm->cs = 686; goto _test_eof; 
	_test_eof687:  sm->cs = 687; goto _test_eof; 
	_test_eof688:  sm->cs = 688; goto _test_eof; 
	_test_eof689:  sm->cs = 689; goto _test_eof; 
	_test_eof690:  sm->cs = 690; goto _test_eof; 
	_test_eof1574:  sm->cs = 1574; goto _test_eof; 
	_test_eof1575:  sm->cs = 1575; goto _test_eof; 
	_test_eof1576:  sm->cs = 1576; goto _test_eof; 
	_test_eof1577:  sm->cs = 1577; goto _test_eof; 
	_test_eof1578:  sm->cs = 1578; goto _test_eof; 
	_test_eof1579:  sm->cs = 1579; goto _test_eof; 
	_test_eof691:  sm->cs = 691; goto _test_eof; 
	_test_eof692:  sm->cs = 692; goto _test_eof; 
	_test_eof1580:  sm->cs = 1580; goto _test_eof; 
	_test_eof1581:  sm->cs = 1581; goto _test_eof; 
	_test_eof1582:  sm->cs = 1582; goto _test_eof; 
	_test_eof1583:  sm->cs = 1583; goto _test_eof; 
	_test_eof1584:  sm->cs = 1584; goto _test_eof; 
	_test_eof1585:  sm->cs = 1585; goto _test_eof; 
	_test_eof1586:  sm->cs = 1586; goto _test_eof; 
	_test_eof1587:  sm->cs = 1587; goto _test_eof; 
	_test_eof1588:  sm->cs = 1588; goto _test_eof; 
	_test_eof1589:  sm->cs = 1589; goto _test_eof; 
	_test_eof1590:  sm->cs = 1590; goto _test_eof; 
	_test_eof1591:  sm->cs = 1591; goto _test_eof; 
	_test_eof693:  sm->cs = 693; goto _test_eof; 
	_test_eof694:  sm->cs = 694; goto _test_eof; 
	_test_eof695:  sm->cs = 695; goto _test_eof; 
	_test_eof696:  sm->cs = 696; goto _test_eof; 
	_test_eof697:  sm->cs = 697; goto _test_eof; 
	_test_eof698:  sm->cs = 698; goto _test_eof; 
	_test_eof699:  sm->cs = 699; goto _test_eof; 
	_test_eof700:  sm->cs = 700; goto _test_eof; 
	_test_eof701:  sm->cs = 701; goto _test_eof; 
	_test_eof702:  sm->cs = 702; goto _test_eof; 
	_test_eof1592:  sm->cs = 1592; goto _test_eof; 
	_test_eof1593:  sm->cs = 1593; goto _test_eof; 
	_test_eof1594:  sm->cs = 1594; goto _test_eof; 
	_test_eof1595:  sm->cs = 1595; goto _test_eof; 
	_test_eof1596:  sm->cs = 1596; goto _test_eof; 
	_test_eof1597:  sm->cs = 1597; goto _test_eof; 
	_test_eof703:  sm->cs = 703; goto _test_eof; 
	_test_eof704:  sm->cs = 704; goto _test_eof; 
	_test_eof1598:  sm->cs = 1598; goto _test_eof; 
	_test_eof1599:  sm->cs = 1599; goto _test_eof; 
	_test_eof1600:  sm->cs = 1600; goto _test_eof; 
	_test_eof1601:  sm->cs = 1601; goto _test_eof; 
	_test_eof1602:  sm->cs = 1602; goto _test_eof; 
	_test_eof1603:  sm->cs = 1603; goto _test_eof; 
	_test_eof1604:  sm->cs = 1604; goto _test_eof; 
	_test_eof1605:  sm->cs = 1605; goto _test_eof; 
	_test_eof1606:  sm->cs = 1606; goto _test_eof; 
	_test_eof1607:  sm->cs = 1607; goto _test_eof; 
	_test_eof1608:  sm->cs = 1608; goto _test_eof; 
	_test_eof1609:  sm->cs = 1609; goto _test_eof; 
	_test_eof705:  sm->cs = 705; goto _test_eof; 
	_test_eof706:  sm->cs = 706; goto _test_eof; 
	_test_eof707:  sm->cs = 707; goto _test_eof; 
	_test_eof708:  sm->cs = 708; goto _test_eof; 
	_test_eof709:  sm->cs = 709; goto _test_eof; 
	_test_eof710:  sm->cs = 710; goto _test_eof; 
	_test_eof711:  sm->cs = 711; goto _test_eof; 
	_test_eof712:  sm->cs = 712; goto _test_eof; 
	_test_eof713:  sm->cs = 713; goto _test_eof; 
	_test_eof714:  sm->cs = 714; goto _test_eof; 
	_test_eof1610:  sm->cs = 1610; goto _test_eof; 
	_test_eof1611:  sm->cs = 1611; goto _test_eof; 
	_test_eof1612:  sm->cs = 1612; goto _test_eof; 
	_test_eof715:  sm->cs = 715; goto _test_eof; 
	_test_eof716:  sm->cs = 716; goto _test_eof; 
	_test_eof717:  sm->cs = 717; goto _test_eof; 
	_test_eof718:  sm->cs = 718; goto _test_eof; 
	_test_eof719:  sm->cs = 719; goto _test_eof; 
	_test_eof720:  sm->cs = 720; goto _test_eof; 
	_test_eof721:  sm->cs = 721; goto _test_eof; 
	_test_eof722:  sm->cs = 722; goto _test_eof; 
	_test_eof1613:  sm->cs = 1613; goto _test_eof; 
	_test_eof1614:  sm->cs = 1614; goto _test_eof; 
	_test_eof1615:  sm->cs = 1615; goto _test_eof; 
	_test_eof1616:  sm->cs = 1616; goto _test_eof; 
	_test_eof1617:  sm->cs = 1617; goto _test_eof; 
	_test_eof1618:  sm->cs = 1618; goto _test_eof; 
	_test_eof1619:  sm->cs = 1619; goto _test_eof; 
	_test_eof1620:  sm->cs = 1620; goto _test_eof; 
	_test_eof1621:  sm->cs = 1621; goto _test_eof; 
	_test_eof1622:  sm->cs = 1622; goto _test_eof; 
	_test_eof1623:  sm->cs = 1623; goto _test_eof; 
	_test_eof1624:  sm->cs = 1624; goto _test_eof; 
	_test_eof723:  sm->cs = 723; goto _test_eof; 
	_test_eof724:  sm->cs = 724; goto _test_eof; 
	_test_eof725:  sm->cs = 725; goto _test_eof; 
	_test_eof726:  sm->cs = 726; goto _test_eof; 
	_test_eof727:  sm->cs = 727; goto _test_eof; 
	_test_eof728:  sm->cs = 728; goto _test_eof; 
	_test_eof729:  sm->cs = 729; goto _test_eof; 
	_test_eof730:  sm->cs = 730; goto _test_eof; 
	_test_eof731:  sm->cs = 731; goto _test_eof; 
	_test_eof732:  sm->cs = 732; goto _test_eof; 
	_test_eof733:  sm->cs = 733; goto _test_eof; 
	_test_eof734:  sm->cs = 734; goto _test_eof; 
	_test_eof735:  sm->cs = 735; goto _test_eof; 
	_test_eof736:  sm->cs = 736; goto _test_eof; 
	_test_eof737:  sm->cs = 737; goto _test_eof; 
	_test_eof738:  sm->cs = 738; goto _test_eof; 
	_test_eof739:  sm->cs = 739; goto _test_eof; 
	_test_eof740:  sm->cs = 740; goto _test_eof; 
	_test_eof741:  sm->cs = 741; goto _test_eof; 
	_test_eof742:  sm->cs = 742; goto _test_eof; 
	_test_eof743:  sm->cs = 743; goto _test_eof; 
	_test_eof744:  sm->cs = 744; goto _test_eof; 
	_test_eof745:  sm->cs = 745; goto _test_eof; 
	_test_eof1625:  sm->cs = 1625; goto _test_eof; 
	_test_eof1626:  sm->cs = 1626; goto _test_eof; 
	_test_eof1627:  sm->cs = 1627; goto _test_eof; 
	_test_eof1628:  sm->cs = 1628; goto _test_eof; 
	_test_eof1629:  sm->cs = 1629; goto _test_eof; 
	_test_eof1630:  sm->cs = 1630; goto _test_eof; 
	_test_eof1631:  sm->cs = 1631; goto _test_eof; 
	_test_eof1632:  sm->cs = 1632; goto _test_eof; 
	_test_eof1633:  sm->cs = 1633; goto _test_eof; 
	_test_eof1634:  sm->cs = 1634; goto _test_eof; 
	_test_eof1635:  sm->cs = 1635; goto _test_eof; 
	_test_eof1636:  sm->cs = 1636; goto _test_eof; 
	_test_eof746:  sm->cs = 746; goto _test_eof; 
	_test_eof747:  sm->cs = 747; goto _test_eof; 
	_test_eof748:  sm->cs = 748; goto _test_eof; 
	_test_eof749:  sm->cs = 749; goto _test_eof; 
	_test_eof750:  sm->cs = 750; goto _test_eof; 
	_test_eof751:  sm->cs = 751; goto _test_eof; 
	_test_eof752:  sm->cs = 752; goto _test_eof; 
	_test_eof753:  sm->cs = 753; goto _test_eof; 
	_test_eof754:  sm->cs = 754; goto _test_eof; 
	_test_eof755:  sm->cs = 755; goto _test_eof; 
	_test_eof756:  sm->cs = 756; goto _test_eof; 
	_test_eof757:  sm->cs = 757; goto _test_eof; 
	_test_eof758:  sm->cs = 758; goto _test_eof; 
	_test_eof759:  sm->cs = 759; goto _test_eof; 
	_test_eof760:  sm->cs = 760; goto _test_eof; 
	_test_eof761:  sm->cs = 761; goto _test_eof; 
	_test_eof762:  sm->cs = 762; goto _test_eof; 
	_test_eof763:  sm->cs = 763; goto _test_eof; 
	_test_eof764:  sm->cs = 764; goto _test_eof; 
	_test_eof765:  sm->cs = 765; goto _test_eof; 
	_test_eof766:  sm->cs = 766; goto _test_eof; 
	_test_eof767:  sm->cs = 767; goto _test_eof; 
	_test_eof768:  sm->cs = 768; goto _test_eof; 
	_test_eof1637:  sm->cs = 1637; goto _test_eof; 
	_test_eof1638:  sm->cs = 1638; goto _test_eof; 
	_test_eof1639:  sm->cs = 1639; goto _test_eof; 
	_test_eof1640:  sm->cs = 1640; goto _test_eof; 
	_test_eof1641:  sm->cs = 1641; goto _test_eof; 
	_test_eof1642:  sm->cs = 1642; goto _test_eof; 
	_test_eof1643:  sm->cs = 1643; goto _test_eof; 
	_test_eof1644:  sm->cs = 1644; goto _test_eof; 
	_test_eof1645:  sm->cs = 1645; goto _test_eof; 
	_test_eof1646:  sm->cs = 1646; goto _test_eof; 
	_test_eof1647:  sm->cs = 1647; goto _test_eof; 
	_test_eof1648:  sm->cs = 1648; goto _test_eof; 
	_test_eof769:  sm->cs = 769; goto _test_eof; 
	_test_eof770:  sm->cs = 770; goto _test_eof; 
	_test_eof771:  sm->cs = 771; goto _test_eof; 
	_test_eof772:  sm->cs = 772; goto _test_eof; 
	_test_eof773:  sm->cs = 773; goto _test_eof; 
	_test_eof774:  sm->cs = 774; goto _test_eof; 
	_test_eof775:  sm->cs = 775; goto _test_eof; 
	_test_eof776:  sm->cs = 776; goto _test_eof; 
	_test_eof777:  sm->cs = 777; goto _test_eof; 
	_test_eof778:  sm->cs = 778; goto _test_eof; 
	_test_eof1649:  sm->cs = 1649; goto _test_eof; 
	_test_eof1650:  sm->cs = 1650; goto _test_eof; 
	_test_eof1651:  sm->cs = 1651; goto _test_eof; 
	_test_eof1652:  sm->cs = 1652; goto _test_eof; 
	_test_eof779:  sm->cs = 779; goto _test_eof; 
	_test_eof780:  sm->cs = 780; goto _test_eof; 
	_test_eof1653:  sm->cs = 1653; goto _test_eof; 
	_test_eof781:  sm->cs = 781; goto _test_eof; 
	_test_eof782:  sm->cs = 782; goto _test_eof; 
	_test_eof1654:  sm->cs = 1654; goto _test_eof; 
	_test_eof1655:  sm->cs = 1655; goto _test_eof; 
	_test_eof1656:  sm->cs = 1656; goto _test_eof; 
	_test_eof1657:  sm->cs = 1657; goto _test_eof; 
	_test_eof1658:  sm->cs = 1658; goto _test_eof; 
	_test_eof1659:  sm->cs = 1659; goto _test_eof; 
	_test_eof1660:  sm->cs = 1660; goto _test_eof; 
	_test_eof1661:  sm->cs = 1661; goto _test_eof; 
	_test_eof1662:  sm->cs = 1662; goto _test_eof; 
	_test_eof1663:  sm->cs = 1663; goto _test_eof; 
	_test_eof1664:  sm->cs = 1664; goto _test_eof; 
	_test_eof1665:  sm->cs = 1665; goto _test_eof; 
	_test_eof783:  sm->cs = 783; goto _test_eof; 
	_test_eof784:  sm->cs = 784; goto _test_eof; 
	_test_eof785:  sm->cs = 785; goto _test_eof; 
	_test_eof786:  sm->cs = 786; goto _test_eof; 
	_test_eof787:  sm->cs = 787; goto _test_eof; 
	_test_eof788:  sm->cs = 788; goto _test_eof; 
	_test_eof789:  sm->cs = 789; goto _test_eof; 
	_test_eof790:  sm->cs = 790; goto _test_eof; 
	_test_eof791:  sm->cs = 791; goto _test_eof; 
	_test_eof792:  sm->cs = 792; goto _test_eof; 
	_test_eof1666:  sm->cs = 1666; goto _test_eof; 
	_test_eof1667:  sm->cs = 1667; goto _test_eof; 
	_test_eof1668:  sm->cs = 1668; goto _test_eof; 
	_test_eof1669:  sm->cs = 1669; goto _test_eof; 
	_test_eof793:  sm->cs = 793; goto _test_eof; 
	_test_eof794:  sm->cs = 794; goto _test_eof; 
	_test_eof1670:  sm->cs = 1670; goto _test_eof; 
	_test_eof1671:  sm->cs = 1671; goto _test_eof; 
	_test_eof1672:  sm->cs = 1672; goto _test_eof; 
	_test_eof1673:  sm->cs = 1673; goto _test_eof; 
	_test_eof1674:  sm->cs = 1674; goto _test_eof; 
	_test_eof1675:  sm->cs = 1675; goto _test_eof; 
	_test_eof1676:  sm->cs = 1676; goto _test_eof; 
	_test_eof1677:  sm->cs = 1677; goto _test_eof; 
	_test_eof1678:  sm->cs = 1678; goto _test_eof; 
	_test_eof1679:  sm->cs = 1679; goto _test_eof; 
	_test_eof1680:  sm->cs = 1680; goto _test_eof; 
	_test_eof1681:  sm->cs = 1681; goto _test_eof; 
	_test_eof795:  sm->cs = 795; goto _test_eof; 
	_test_eof796:  sm->cs = 796; goto _test_eof; 
	_test_eof797:  sm->cs = 797; goto _test_eof; 
	_test_eof798:  sm->cs = 798; goto _test_eof; 
	_test_eof799:  sm->cs = 799; goto _test_eof; 
	_test_eof800:  sm->cs = 800; goto _test_eof; 
	_test_eof801:  sm->cs = 801; goto _test_eof; 
	_test_eof802:  sm->cs = 802; goto _test_eof; 
	_test_eof803:  sm->cs = 803; goto _test_eof; 
	_test_eof804:  sm->cs = 804; goto _test_eof; 
	_test_eof805:  sm->cs = 805; goto _test_eof; 
	_test_eof806:  sm->cs = 806; goto _test_eof; 
	_test_eof807:  sm->cs = 807; goto _test_eof; 
	_test_eof808:  sm->cs = 808; goto _test_eof; 
	_test_eof809:  sm->cs = 809; goto _test_eof; 
	_test_eof810:  sm->cs = 810; goto _test_eof; 
	_test_eof811:  sm->cs = 811; goto _test_eof; 
	_test_eof812:  sm->cs = 812; goto _test_eof; 
	_test_eof1682:  sm->cs = 1682; goto _test_eof; 
	_test_eof1683:  sm->cs = 1683; goto _test_eof; 
	_test_eof1684:  sm->cs = 1684; goto _test_eof; 
	_test_eof1685:  sm->cs = 1685; goto _test_eof; 
	_test_eof1686:  sm->cs = 1686; goto _test_eof; 
	_test_eof1687:  sm->cs = 1687; goto _test_eof; 
	_test_eof1688:  sm->cs = 1688; goto _test_eof; 
	_test_eof1689:  sm->cs = 1689; goto _test_eof; 
	_test_eof1690:  sm->cs = 1690; goto _test_eof; 
	_test_eof1691:  sm->cs = 1691; goto _test_eof; 
	_test_eof1692:  sm->cs = 1692; goto _test_eof; 
	_test_eof1693:  sm->cs = 1693; goto _test_eof; 
	_test_eof813:  sm->cs = 813; goto _test_eof; 
	_test_eof814:  sm->cs = 814; goto _test_eof; 
	_test_eof815:  sm->cs = 815; goto _test_eof; 
	_test_eof816:  sm->cs = 816; goto _test_eof; 
	_test_eof817:  sm->cs = 817; goto _test_eof; 
	_test_eof818:  sm->cs = 818; goto _test_eof; 
	_test_eof819:  sm->cs = 819; goto _test_eof; 
	_test_eof820:  sm->cs = 820; goto _test_eof; 
	_test_eof821:  sm->cs = 821; goto _test_eof; 
	_test_eof822:  sm->cs = 822; goto _test_eof; 
	_test_eof1694:  sm->cs = 1694; goto _test_eof; 
	_test_eof1695:  sm->cs = 1695; goto _test_eof; 
	_test_eof1696:  sm->cs = 1696; goto _test_eof; 
	_test_eof1697:  sm->cs = 1697; goto _test_eof; 
	_test_eof823:  sm->cs = 823; goto _test_eof; 
	_test_eof824:  sm->cs = 824; goto _test_eof; 
	_test_eof1698:  sm->cs = 1698; goto _test_eof; 
	_test_eof1699:  sm->cs = 1699; goto _test_eof; 
	_test_eof1700:  sm->cs = 1700; goto _test_eof; 
	_test_eof1701:  sm->cs = 1701; goto _test_eof; 
	_test_eof1702:  sm->cs = 1702; goto _test_eof; 
	_test_eof1703:  sm->cs = 1703; goto _test_eof; 
	_test_eof1704:  sm->cs = 1704; goto _test_eof; 
	_test_eof1705:  sm->cs = 1705; goto _test_eof; 
	_test_eof1706:  sm->cs = 1706; goto _test_eof; 
	_test_eof1707:  sm->cs = 1707; goto _test_eof; 
	_test_eof1708:  sm->cs = 1708; goto _test_eof; 
	_test_eof1709:  sm->cs = 1709; goto _test_eof; 
	_test_eof825:  sm->cs = 825; goto _test_eof; 
	_test_eof826:  sm->cs = 826; goto _test_eof; 
	_test_eof827:  sm->cs = 827; goto _test_eof; 
	_test_eof828:  sm->cs = 828; goto _test_eof; 
	_test_eof829:  sm->cs = 829; goto _test_eof; 
	_test_eof830:  sm->cs = 830; goto _test_eof; 
	_test_eof831:  sm->cs = 831; goto _test_eof; 
	_test_eof832:  sm->cs = 832; goto _test_eof; 
	_test_eof833:  sm->cs = 833; goto _test_eof; 
	_test_eof834:  sm->cs = 834; goto _test_eof; 
	_test_eof1710:  sm->cs = 1710; goto _test_eof; 
	_test_eof835:  sm->cs = 835; goto _test_eof; 
	_test_eof836:  sm->cs = 836; goto _test_eof; 
	_test_eof837:  sm->cs = 837; goto _test_eof; 
	_test_eof838:  sm->cs = 838; goto _test_eof; 
	_test_eof839:  sm->cs = 839; goto _test_eof; 
	_test_eof840:  sm->cs = 840; goto _test_eof; 
	_test_eof841:  sm->cs = 841; goto _test_eof; 
	_test_eof842:  sm->cs = 842; goto _test_eof; 
	_test_eof843:  sm->cs = 843; goto _test_eof; 
	_test_eof844:  sm->cs = 844; goto _test_eof; 
	_test_eof845:  sm->cs = 845; goto _test_eof; 
	_test_eof846:  sm->cs = 846; goto _test_eof; 
	_test_eof847:  sm->cs = 847; goto _test_eof; 
	_test_eof848:  sm->cs = 848; goto _test_eof; 
	_test_eof849:  sm->cs = 849; goto _test_eof; 
	_test_eof850:  sm->cs = 850; goto _test_eof; 
	_test_eof851:  sm->cs = 851; goto _test_eof; 
	_test_eof852:  sm->cs = 852; goto _test_eof; 
	_test_eof853:  sm->cs = 853; goto _test_eof; 
	_test_eof1711:  sm->cs = 1711; goto _test_eof; 
	_test_eof854:  sm->cs = 854; goto _test_eof; 
	_test_eof1712:  sm->cs = 1712; goto _test_eof; 
	_test_eof855:  sm->cs = 855; goto _test_eof; 
	_test_eof856:  sm->cs = 856; goto _test_eof; 
	_test_eof857:  sm->cs = 857; goto _test_eof; 
	_test_eof858:  sm->cs = 858; goto _test_eof; 
	_test_eof859:  sm->cs = 859; goto _test_eof; 
	_test_eof860:  sm->cs = 860; goto _test_eof; 
	_test_eof861:  sm->cs = 861; goto _test_eof; 
	_test_eof862:  sm->cs = 862; goto _test_eof; 
	_test_eof863:  sm->cs = 863; goto _test_eof; 
	_test_eof864:  sm->cs = 864; goto _test_eof; 
	_test_eof865:  sm->cs = 865; goto _test_eof; 
	_test_eof866:  sm->cs = 866; goto _test_eof; 
	_test_eof867:  sm->cs = 867; goto _test_eof; 
	_test_eof868:  sm->cs = 868; goto _test_eof; 
	_test_eof869:  sm->cs = 869; goto _test_eof; 
	_test_eof870:  sm->cs = 870; goto _test_eof; 
	_test_eof871:  sm->cs = 871; goto _test_eof; 
	_test_eof872:  sm->cs = 872; goto _test_eof; 
	_test_eof873:  sm->cs = 873; goto _test_eof; 
	_test_eof874:  sm->cs = 874; goto _test_eof; 
	_test_eof875:  sm->cs = 875; goto _test_eof; 
	_test_eof876:  sm->cs = 876; goto _test_eof; 
	_test_eof877:  sm->cs = 877; goto _test_eof; 
	_test_eof878:  sm->cs = 878; goto _test_eof; 
	_test_eof879:  sm->cs = 879; goto _test_eof; 
	_test_eof880:  sm->cs = 880; goto _test_eof; 
	_test_eof881:  sm->cs = 881; goto _test_eof; 
	_test_eof882:  sm->cs = 882; goto _test_eof; 
	_test_eof883:  sm->cs = 883; goto _test_eof; 
	_test_eof884:  sm->cs = 884; goto _test_eof; 
	_test_eof885:  sm->cs = 885; goto _test_eof; 
	_test_eof886:  sm->cs = 886; goto _test_eof; 
	_test_eof887:  sm->cs = 887; goto _test_eof; 
	_test_eof888:  sm->cs = 888; goto _test_eof; 
	_test_eof889:  sm->cs = 889; goto _test_eof; 
	_test_eof890:  sm->cs = 890; goto _test_eof; 
	_test_eof1713:  sm->cs = 1713; goto _test_eof; 
	_test_eof891:  sm->cs = 891; goto _test_eof; 
	_test_eof892:  sm->cs = 892; goto _test_eof; 
	_test_eof893:  sm->cs = 893; goto _test_eof; 
	_test_eof894:  sm->cs = 894; goto _test_eof; 
	_test_eof895:  sm->cs = 895; goto _test_eof; 
	_test_eof896:  sm->cs = 896; goto _test_eof; 
	_test_eof897:  sm->cs = 897; goto _test_eof; 
	_test_eof898:  sm->cs = 898; goto _test_eof; 
	_test_eof899:  sm->cs = 899; goto _test_eof; 
	_test_eof900:  sm->cs = 900; goto _test_eof; 
	_test_eof901:  sm->cs = 901; goto _test_eof; 
	_test_eof902:  sm->cs = 902; goto _test_eof; 
	_test_eof903:  sm->cs = 903; goto _test_eof; 
	_test_eof904:  sm->cs = 904; goto _test_eof; 
	_test_eof905:  sm->cs = 905; goto _test_eof; 
	_test_eof906:  sm->cs = 906; goto _test_eof; 
	_test_eof907:  sm->cs = 907; goto _test_eof; 
	_test_eof908:  sm->cs = 908; goto _test_eof; 
	_test_eof909:  sm->cs = 909; goto _test_eof; 
	_test_eof910:  sm->cs = 910; goto _test_eof; 
	_test_eof911:  sm->cs = 911; goto _test_eof; 
	_test_eof912:  sm->cs = 912; goto _test_eof; 
	_test_eof913:  sm->cs = 913; goto _test_eof; 
	_test_eof914:  sm->cs = 914; goto _test_eof; 
	_test_eof915:  sm->cs = 915; goto _test_eof; 
	_test_eof916:  sm->cs = 916; goto _test_eof; 
	_test_eof917:  sm->cs = 917; goto _test_eof; 
	_test_eof918:  sm->cs = 918; goto _test_eof; 
	_test_eof919:  sm->cs = 919; goto _test_eof; 
	_test_eof920:  sm->cs = 920; goto _test_eof; 
	_test_eof921:  sm->cs = 921; goto _test_eof; 
	_test_eof922:  sm->cs = 922; goto _test_eof; 
	_test_eof923:  sm->cs = 923; goto _test_eof; 
	_test_eof924:  sm->cs = 924; goto _test_eof; 
	_test_eof925:  sm->cs = 925; goto _test_eof; 
	_test_eof926:  sm->cs = 926; goto _test_eof; 
	_test_eof927:  sm->cs = 927; goto _test_eof; 
	_test_eof928:  sm->cs = 928; goto _test_eof; 
	_test_eof929:  sm->cs = 929; goto _test_eof; 
	_test_eof930:  sm->cs = 930; goto _test_eof; 
	_test_eof931:  sm->cs = 931; goto _test_eof; 
	_test_eof932:  sm->cs = 932; goto _test_eof; 
	_test_eof933:  sm->cs = 933; goto _test_eof; 
	_test_eof934:  sm->cs = 934; goto _test_eof; 
	_test_eof935:  sm->cs = 935; goto _test_eof; 
	_test_eof936:  sm->cs = 936; goto _test_eof; 
	_test_eof937:  sm->cs = 937; goto _test_eof; 
	_test_eof938:  sm->cs = 938; goto _test_eof; 
	_test_eof939:  sm->cs = 939; goto _test_eof; 
	_test_eof940:  sm->cs = 940; goto _test_eof; 
	_test_eof941:  sm->cs = 941; goto _test_eof; 
	_test_eof942:  sm->cs = 942; goto _test_eof; 
	_test_eof943:  sm->cs = 943; goto _test_eof; 
	_test_eof944:  sm->cs = 944; goto _test_eof; 
	_test_eof945:  sm->cs = 945; goto _test_eof; 
	_test_eof946:  sm->cs = 946; goto _test_eof; 
	_test_eof947:  sm->cs = 947; goto _test_eof; 
	_test_eof948:  sm->cs = 948; goto _test_eof; 
	_test_eof949:  sm->cs = 949; goto _test_eof; 
	_test_eof950:  sm->cs = 950; goto _test_eof; 
	_test_eof951:  sm->cs = 951; goto _test_eof; 
	_test_eof952:  sm->cs = 952; goto _test_eof; 
	_test_eof953:  sm->cs = 953; goto _test_eof; 
	_test_eof954:  sm->cs = 954; goto _test_eof; 
	_test_eof955:  sm->cs = 955; goto _test_eof; 
	_test_eof956:  sm->cs = 956; goto _test_eof; 
	_test_eof957:  sm->cs = 957; goto _test_eof; 
	_test_eof958:  sm->cs = 958; goto _test_eof; 
	_test_eof959:  sm->cs = 959; goto _test_eof; 
	_test_eof960:  sm->cs = 960; goto _test_eof; 
	_test_eof961:  sm->cs = 961; goto _test_eof; 
	_test_eof962:  sm->cs = 962; goto _test_eof; 
	_test_eof963:  sm->cs = 963; goto _test_eof; 
	_test_eof964:  sm->cs = 964; goto _test_eof; 
	_test_eof965:  sm->cs = 965; goto _test_eof; 
	_test_eof966:  sm->cs = 966; goto _test_eof; 
	_test_eof967:  sm->cs = 967; goto _test_eof; 
	_test_eof968:  sm->cs = 968; goto _test_eof; 
	_test_eof969:  sm->cs = 969; goto _test_eof; 
	_test_eof970:  sm->cs = 970; goto _test_eof; 
	_test_eof971:  sm->cs = 971; goto _test_eof; 
	_test_eof1714:  sm->cs = 1714; goto _test_eof; 
	_test_eof1715:  sm->cs = 1715; goto _test_eof; 
	_test_eof972:  sm->cs = 972; goto _test_eof; 
	_test_eof973:  sm->cs = 973; goto _test_eof; 
	_test_eof974:  sm->cs = 974; goto _test_eof; 
	_test_eof975:  sm->cs = 975; goto _test_eof; 
	_test_eof976:  sm->cs = 976; goto _test_eof; 
	_test_eof977:  sm->cs = 977; goto _test_eof; 
	_test_eof978:  sm->cs = 978; goto _test_eof; 
	_test_eof979:  sm->cs = 979; goto _test_eof; 
	_test_eof980:  sm->cs = 980; goto _test_eof; 
	_test_eof981:  sm->cs = 981; goto _test_eof; 
	_test_eof982:  sm->cs = 982; goto _test_eof; 
	_test_eof983:  sm->cs = 983; goto _test_eof; 
	_test_eof984:  sm->cs = 984; goto _test_eof; 
	_test_eof985:  sm->cs = 985; goto _test_eof; 
	_test_eof986:  sm->cs = 986; goto _test_eof; 
	_test_eof987:  sm->cs = 987; goto _test_eof; 
	_test_eof988:  sm->cs = 988; goto _test_eof; 
	_test_eof989:  sm->cs = 989; goto _test_eof; 
	_test_eof990:  sm->cs = 990; goto _test_eof; 
	_test_eof991:  sm->cs = 991; goto _test_eof; 
	_test_eof992:  sm->cs = 992; goto _test_eof; 
	_test_eof993:  sm->cs = 993; goto _test_eof; 
	_test_eof994:  sm->cs = 994; goto _test_eof; 
	_test_eof995:  sm->cs = 995; goto _test_eof; 
	_test_eof996:  sm->cs = 996; goto _test_eof; 
	_test_eof997:  sm->cs = 997; goto _test_eof; 
	_test_eof998:  sm->cs = 998; goto _test_eof; 
	_test_eof999:  sm->cs = 999; goto _test_eof; 
	_test_eof1000:  sm->cs = 1000; goto _test_eof; 
	_test_eof1001:  sm->cs = 1001; goto _test_eof; 
	_test_eof1002:  sm->cs = 1002; goto _test_eof; 
	_test_eof1003:  sm->cs = 1003; goto _test_eof; 
	_test_eof1004:  sm->cs = 1004; goto _test_eof; 
	_test_eof1005:  sm->cs = 1005; goto _test_eof; 
	_test_eof1006:  sm->cs = 1006; goto _test_eof; 
	_test_eof1007:  sm->cs = 1007; goto _test_eof; 
	_test_eof1008:  sm->cs = 1008; goto _test_eof; 
	_test_eof1009:  sm->cs = 1009; goto _test_eof; 
	_test_eof1010:  sm->cs = 1010; goto _test_eof; 
	_test_eof1011:  sm->cs = 1011; goto _test_eof; 
	_test_eof1012:  sm->cs = 1012; goto _test_eof; 
	_test_eof1013:  sm->cs = 1013; goto _test_eof; 
	_test_eof1014:  sm->cs = 1014; goto _test_eof; 
	_test_eof1015:  sm->cs = 1015; goto _test_eof; 
	_test_eof1016:  sm->cs = 1016; goto _test_eof; 
	_test_eof1017:  sm->cs = 1017; goto _test_eof; 
	_test_eof1018:  sm->cs = 1018; goto _test_eof; 
	_test_eof1019:  sm->cs = 1019; goto _test_eof; 
	_test_eof1020:  sm->cs = 1020; goto _test_eof; 
	_test_eof1021:  sm->cs = 1021; goto _test_eof; 
	_test_eof1022:  sm->cs = 1022; goto _test_eof; 
	_test_eof1023:  sm->cs = 1023; goto _test_eof; 
	_test_eof1024:  sm->cs = 1024; goto _test_eof; 
	_test_eof1025:  sm->cs = 1025; goto _test_eof; 
	_test_eof1026:  sm->cs = 1026; goto _test_eof; 
	_test_eof1027:  sm->cs = 1027; goto _test_eof; 
	_test_eof1028:  sm->cs = 1028; goto _test_eof; 
	_test_eof1029:  sm->cs = 1029; goto _test_eof; 
	_test_eof1030:  sm->cs = 1030; goto _test_eof; 
	_test_eof1031:  sm->cs = 1031; goto _test_eof; 
	_test_eof1032:  sm->cs = 1032; goto _test_eof; 
	_test_eof1033:  sm->cs = 1033; goto _test_eof; 
	_test_eof1034:  sm->cs = 1034; goto _test_eof; 
	_test_eof1035:  sm->cs = 1035; goto _test_eof; 
	_test_eof1036:  sm->cs = 1036; goto _test_eof; 
	_test_eof1037:  sm->cs = 1037; goto _test_eof; 
	_test_eof1038:  sm->cs = 1038; goto _test_eof; 
	_test_eof1039:  sm->cs = 1039; goto _test_eof; 
	_test_eof1040:  sm->cs = 1040; goto _test_eof; 
	_test_eof1041:  sm->cs = 1041; goto _test_eof; 
	_test_eof1042:  sm->cs = 1042; goto _test_eof; 
	_test_eof1043:  sm->cs = 1043; goto _test_eof; 
	_test_eof1044:  sm->cs = 1044; goto _test_eof; 
	_test_eof1045:  sm->cs = 1045; goto _test_eof; 
	_test_eof1046:  sm->cs = 1046; goto _test_eof; 
	_test_eof1047:  sm->cs = 1047; goto _test_eof; 
	_test_eof1048:  sm->cs = 1048; goto _test_eof; 
	_test_eof1049:  sm->cs = 1049; goto _test_eof; 
	_test_eof1050:  sm->cs = 1050; goto _test_eof; 
	_test_eof1051:  sm->cs = 1051; goto _test_eof; 
	_test_eof1052:  sm->cs = 1052; goto _test_eof; 
	_test_eof1053:  sm->cs = 1053; goto _test_eof; 
	_test_eof1054:  sm->cs = 1054; goto _test_eof; 
	_test_eof1055:  sm->cs = 1055; goto _test_eof; 
	_test_eof1056:  sm->cs = 1056; goto _test_eof; 
	_test_eof1057:  sm->cs = 1057; goto _test_eof; 
	_test_eof1058:  sm->cs = 1058; goto _test_eof; 
	_test_eof1059:  sm->cs = 1059; goto _test_eof; 
	_test_eof1060:  sm->cs = 1060; goto _test_eof; 
	_test_eof1061:  sm->cs = 1061; goto _test_eof; 
	_test_eof1062:  sm->cs = 1062; goto _test_eof; 
	_test_eof1063:  sm->cs = 1063; goto _test_eof; 
	_test_eof1064:  sm->cs = 1064; goto _test_eof; 
	_test_eof1065:  sm->cs = 1065; goto _test_eof; 
	_test_eof1066:  sm->cs = 1066; goto _test_eof; 
	_test_eof1067:  sm->cs = 1067; goto _test_eof; 
	_test_eof1068:  sm->cs = 1068; goto _test_eof; 
	_test_eof1069:  sm->cs = 1069; goto _test_eof; 
	_test_eof1070:  sm->cs = 1070; goto _test_eof; 
	_test_eof1071:  sm->cs = 1071; goto _test_eof; 
	_test_eof1072:  sm->cs = 1072; goto _test_eof; 
	_test_eof1073:  sm->cs = 1073; goto _test_eof; 
	_test_eof1074:  sm->cs = 1074; goto _test_eof; 
	_test_eof1075:  sm->cs = 1075; goto _test_eof; 
	_test_eof1076:  sm->cs = 1076; goto _test_eof; 
	_test_eof1716:  sm->cs = 1716; goto _test_eof; 
	_test_eof1077:  sm->cs = 1077; goto _test_eof; 
	_test_eof1078:  sm->cs = 1078; goto _test_eof; 
	_test_eof1717:  sm->cs = 1717; goto _test_eof; 
	_test_eof1079:  sm->cs = 1079; goto _test_eof; 
	_test_eof1080:  sm->cs = 1080; goto _test_eof; 
	_test_eof1081:  sm->cs = 1081; goto _test_eof; 
	_test_eof1082:  sm->cs = 1082; goto _test_eof; 
	_test_eof1718:  sm->cs = 1718; goto _test_eof; 
	_test_eof1083:  sm->cs = 1083; goto _test_eof; 
	_test_eof1084:  sm->cs = 1084; goto _test_eof; 
	_test_eof1085:  sm->cs = 1085; goto _test_eof; 
	_test_eof1086:  sm->cs = 1086; goto _test_eof; 
	_test_eof1087:  sm->cs = 1087; goto _test_eof; 
	_test_eof1088:  sm->cs = 1088; goto _test_eof; 
	_test_eof1089:  sm->cs = 1089; goto _test_eof; 
	_test_eof1090:  sm->cs = 1090; goto _test_eof; 
	_test_eof1091:  sm->cs = 1091; goto _test_eof; 
	_test_eof1092:  sm->cs = 1092; goto _test_eof; 
	_test_eof1719:  sm->cs = 1719; goto _test_eof; 
	_test_eof1093:  sm->cs = 1093; goto _test_eof; 
	_test_eof1094:  sm->cs = 1094; goto _test_eof; 
	_test_eof1095:  sm->cs = 1095; goto _test_eof; 
	_test_eof1096:  sm->cs = 1096; goto _test_eof; 
	_test_eof1097:  sm->cs = 1097; goto _test_eof; 
	_test_eof1098:  sm->cs = 1098; goto _test_eof; 
	_test_eof1099:  sm->cs = 1099; goto _test_eof; 
	_test_eof1100:  sm->cs = 1100; goto _test_eof; 
	_test_eof1101:  sm->cs = 1101; goto _test_eof; 
	_test_eof1102:  sm->cs = 1102; goto _test_eof; 
	_test_eof1103:  sm->cs = 1103; goto _test_eof; 
	_test_eof1104:  sm->cs = 1104; goto _test_eof; 
	_test_eof1105:  sm->cs = 1105; goto _test_eof; 
	_test_eof1106:  sm->cs = 1106; goto _test_eof; 
	_test_eof1107:  sm->cs = 1107; goto _test_eof; 
	_test_eof1108:  sm->cs = 1108; goto _test_eof; 
	_test_eof1109:  sm->cs = 1109; goto _test_eof; 
	_test_eof1110:  sm->cs = 1110; goto _test_eof; 
	_test_eof1111:  sm->cs = 1111; goto _test_eof; 
	_test_eof1112:  sm->cs = 1112; goto _test_eof; 
	_test_eof1720:  sm->cs = 1720; goto _test_eof; 
	_test_eof1721:  sm->cs = 1721; goto _test_eof; 
	_test_eof1113:  sm->cs = 1113; goto _test_eof; 
	_test_eof1114:  sm->cs = 1114; goto _test_eof; 
	_test_eof1115:  sm->cs = 1115; goto _test_eof; 
	_test_eof1116:  sm->cs = 1116; goto _test_eof; 
	_test_eof1117:  sm->cs = 1117; goto _test_eof; 
	_test_eof1118:  sm->cs = 1118; goto _test_eof; 
	_test_eof1119:  sm->cs = 1119; goto _test_eof; 
	_test_eof1120:  sm->cs = 1120; goto _test_eof; 
	_test_eof1121:  sm->cs = 1121; goto _test_eof; 
	_test_eof1122:  sm->cs = 1122; goto _test_eof; 
	_test_eof1123:  sm->cs = 1123; goto _test_eof; 
	_test_eof1124:  sm->cs = 1124; goto _test_eof; 
	_test_eof1722:  sm->cs = 1722; goto _test_eof; 
	_test_eof1723:  sm->cs = 1723; goto _test_eof; 
	_test_eof1724:  sm->cs = 1724; goto _test_eof; 
	_test_eof1725:  sm->cs = 1725; goto _test_eof; 
	_test_eof1125:  sm->cs = 1125; goto _test_eof; 
	_test_eof1126:  sm->cs = 1126; goto _test_eof; 
	_test_eof1127:  sm->cs = 1127; goto _test_eof; 
	_test_eof1128:  sm->cs = 1128; goto _test_eof; 
	_test_eof1129:  sm->cs = 1129; goto _test_eof; 
	_test_eof1130:  sm->cs = 1130; goto _test_eof; 
	_test_eof1131:  sm->cs = 1131; goto _test_eof; 
	_test_eof1132:  sm->cs = 1132; goto _test_eof; 
	_test_eof1133:  sm->cs = 1133; goto _test_eof; 
	_test_eof1134:  sm->cs = 1134; goto _test_eof; 
	_test_eof1135:  sm->cs = 1135; goto _test_eof; 
	_test_eof1136:  sm->cs = 1136; goto _test_eof; 
	_test_eof1137:  sm->cs = 1137; goto _test_eof; 
	_test_eof1138:  sm->cs = 1138; goto _test_eof; 
	_test_eof1139:  sm->cs = 1139; goto _test_eof; 
	_test_eof1140:  sm->cs = 1140; goto _test_eof; 
	_test_eof1141:  sm->cs = 1141; goto _test_eof; 
	_test_eof1142:  sm->cs = 1142; goto _test_eof; 
	_test_eof1726:  sm->cs = 1726; goto _test_eof; 
	_test_eof1727:  sm->cs = 1727; goto _test_eof; 
	_test_eof1728:  sm->cs = 1728; goto _test_eof; 
	_test_eof1729:  sm->cs = 1729; goto _test_eof; 
	_test_eof1143:  sm->cs = 1143; goto _test_eof; 
	_test_eof1144:  sm->cs = 1144; goto _test_eof; 
	_test_eof1145:  sm->cs = 1145; goto _test_eof; 
	_test_eof1146:  sm->cs = 1146; goto _test_eof; 
	_test_eof1147:  sm->cs = 1147; goto _test_eof; 
	_test_eof1148:  sm->cs = 1148; goto _test_eof; 
	_test_eof1149:  sm->cs = 1149; goto _test_eof; 
	_test_eof1150:  sm->cs = 1150; goto _test_eof; 
	_test_eof1151:  sm->cs = 1151; goto _test_eof; 
	_test_eof1152:  sm->cs = 1152; goto _test_eof; 
	_test_eof1153:  sm->cs = 1153; goto _test_eof; 
	_test_eof1154:  sm->cs = 1154; goto _test_eof; 
	_test_eof1155:  sm->cs = 1155; goto _test_eof; 
	_test_eof1156:  sm->cs = 1156; goto _test_eof; 
	_test_eof1157:  sm->cs = 1157; goto _test_eof; 
	_test_eof1158:  sm->cs = 1158; goto _test_eof; 
	_test_eof1159:  sm->cs = 1159; goto _test_eof; 
	_test_eof1160:  sm->cs = 1160; goto _test_eof; 
	_test_eof1161:  sm->cs = 1161; goto _test_eof; 
	_test_eof1162:  sm->cs = 1162; goto _test_eof; 
	_test_eof1163:  sm->cs = 1163; goto _test_eof; 
	_test_eof1164:  sm->cs = 1164; goto _test_eof; 
	_test_eof1165:  sm->cs = 1165; goto _test_eof; 
	_test_eof1166:  sm->cs = 1166; goto _test_eof; 
	_test_eof1167:  sm->cs = 1167; goto _test_eof; 
	_test_eof1168:  sm->cs = 1168; goto _test_eof; 
	_test_eof1169:  sm->cs = 1169; goto _test_eof; 
	_test_eof1170:  sm->cs = 1170; goto _test_eof; 
	_test_eof1171:  sm->cs = 1171; goto _test_eof; 
	_test_eof1172:  sm->cs = 1172; goto _test_eof; 
	_test_eof1173:  sm->cs = 1173; goto _test_eof; 
	_test_eof1174:  sm->cs = 1174; goto _test_eof; 
	_test_eof1175:  sm->cs = 1175; goto _test_eof; 
	_test_eof1176:  sm->cs = 1176; goto _test_eof; 
	_test_eof1177:  sm->cs = 1177; goto _test_eof; 
	_test_eof1178:  sm->cs = 1178; goto _test_eof; 
	_test_eof1179:  sm->cs = 1179; goto _test_eof; 
	_test_eof1180:  sm->cs = 1180; goto _test_eof; 
	_test_eof1181:  sm->cs = 1181; goto _test_eof; 
	_test_eof1182:  sm->cs = 1182; goto _test_eof; 
	_test_eof1183:  sm->cs = 1183; goto _test_eof; 
	_test_eof1184:  sm->cs = 1184; goto _test_eof; 
	_test_eof1185:  sm->cs = 1185; goto _test_eof; 
	_test_eof1186:  sm->cs = 1186; goto _test_eof; 
	_test_eof1187:  sm->cs = 1187; goto _test_eof; 
	_test_eof1188:  sm->cs = 1188; goto _test_eof; 
	_test_eof1189:  sm->cs = 1189; goto _test_eof; 
	_test_eof1190:  sm->cs = 1190; goto _test_eof; 
	_test_eof1191:  sm->cs = 1191; goto _test_eof; 
	_test_eof1192:  sm->cs = 1192; goto _test_eof; 
	_test_eof1193:  sm->cs = 1193; goto _test_eof; 
	_test_eof1194:  sm->cs = 1194; goto _test_eof; 
	_test_eof1195:  sm->cs = 1195; goto _test_eof; 
	_test_eof1196:  sm->cs = 1196; goto _test_eof; 
	_test_eof1197:  sm->cs = 1197; goto _test_eof; 
	_test_eof1198:  sm->cs = 1198; goto _test_eof; 
	_test_eof1199:  sm->cs = 1199; goto _test_eof; 
	_test_eof1200:  sm->cs = 1200; goto _test_eof; 
	_test_eof1201:  sm->cs = 1201; goto _test_eof; 
	_test_eof1202:  sm->cs = 1202; goto _test_eof; 
	_test_eof1203:  sm->cs = 1203; goto _test_eof; 
	_test_eof1204:  sm->cs = 1204; goto _test_eof; 
	_test_eof1205:  sm->cs = 1205; goto _test_eof; 
	_test_eof1206:  sm->cs = 1206; goto _test_eof; 
	_test_eof1207:  sm->cs = 1207; goto _test_eof; 
	_test_eof1208:  sm->cs = 1208; goto _test_eof; 
	_test_eof1209:  sm->cs = 1209; goto _test_eof; 
	_test_eof1210:  sm->cs = 1210; goto _test_eof; 
	_test_eof1211:  sm->cs = 1211; goto _test_eof; 
	_test_eof1212:  sm->cs = 1212; goto _test_eof; 
	_test_eof1213:  sm->cs = 1213; goto _test_eof; 
	_test_eof1214:  sm->cs = 1214; goto _test_eof; 
	_test_eof1215:  sm->cs = 1215; goto _test_eof; 
	_test_eof1216:  sm->cs = 1216; goto _test_eof; 
	_test_eof1217:  sm->cs = 1217; goto _test_eof; 
	_test_eof1218:  sm->cs = 1218; goto _test_eof; 
	_test_eof1219:  sm->cs = 1219; goto _test_eof; 
	_test_eof1220:  sm->cs = 1220; goto _test_eof; 
	_test_eof1221:  sm->cs = 1221; goto _test_eof; 
	_test_eof1222:  sm->cs = 1222; goto _test_eof; 
	_test_eof1223:  sm->cs = 1223; goto _test_eof; 
	_test_eof1224:  sm->cs = 1224; goto _test_eof; 
	_test_eof1225:  sm->cs = 1225; goto _test_eof; 
	_test_eof1226:  sm->cs = 1226; goto _test_eof; 
	_test_eof1227:  sm->cs = 1227; goto _test_eof; 
	_test_eof1228:  sm->cs = 1228; goto _test_eof; 
	_test_eof1229:  sm->cs = 1229; goto _test_eof; 
	_test_eof1230:  sm->cs = 1230; goto _test_eof; 
	_test_eof1231:  sm->cs = 1231; goto _test_eof; 
	_test_eof1232:  sm->cs = 1232; goto _test_eof; 
	_test_eof1233:  sm->cs = 1233; goto _test_eof; 
	_test_eof1234:  sm->cs = 1234; goto _test_eof; 
	_test_eof1235:  sm->cs = 1235; goto _test_eof; 
	_test_eof1236:  sm->cs = 1236; goto _test_eof; 
	_test_eof1237:  sm->cs = 1237; goto _test_eof; 
	_test_eof1238:  sm->cs = 1238; goto _test_eof; 
	_test_eof1239:  sm->cs = 1239; goto _test_eof; 
	_test_eof1240:  sm->cs = 1240; goto _test_eof; 
	_test_eof1241:  sm->cs = 1241; goto _test_eof; 
	_test_eof1242:  sm->cs = 1242; goto _test_eof; 
	_test_eof1243:  sm->cs = 1243; goto _test_eof; 
	_test_eof1244:  sm->cs = 1244; goto _test_eof; 
	_test_eof1245:  sm->cs = 1245; goto _test_eof; 
	_test_eof1246:  sm->cs = 1246; goto _test_eof; 
	_test_eof1247:  sm->cs = 1247; goto _test_eof; 
	_test_eof1248:  sm->cs = 1248; goto _test_eof; 
	_test_eof1249:  sm->cs = 1249; goto _test_eof; 
	_test_eof1250:  sm->cs = 1250; goto _test_eof; 
	_test_eof1251:  sm->cs = 1251; goto _test_eof; 
	_test_eof1252:  sm->cs = 1252; goto _test_eof; 
	_test_eof1253:  sm->cs = 1253; goto _test_eof; 
	_test_eof1254:  sm->cs = 1254; goto _test_eof; 
	_test_eof1730:  sm->cs = 1730; goto _test_eof; 
	_test_eof1255:  sm->cs = 1255; goto _test_eof; 
	_test_eof1256:  sm->cs = 1256; goto _test_eof; 
	_test_eof1257:  sm->cs = 1257; goto _test_eof; 
	_test_eof1258:  sm->cs = 1258; goto _test_eof; 
	_test_eof1259:  sm->cs = 1259; goto _test_eof; 
	_test_eof1260:  sm->cs = 1260; goto _test_eof; 
	_test_eof1261:  sm->cs = 1261; goto _test_eof; 
	_test_eof1262:  sm->cs = 1262; goto _test_eof; 
	_test_eof1263:  sm->cs = 1263; goto _test_eof; 
	_test_eof1264:  sm->cs = 1264; goto _test_eof; 
	_test_eof1265:  sm->cs = 1265; goto _test_eof; 
	_test_eof1266:  sm->cs = 1266; goto _test_eof; 
	_test_eof1267:  sm->cs = 1267; goto _test_eof; 
	_test_eof1268:  sm->cs = 1268; goto _test_eof; 
	_test_eof1269:  sm->cs = 1269; goto _test_eof; 
	_test_eof1270:  sm->cs = 1270; goto _test_eof; 
	_test_eof1271:  sm->cs = 1271; goto _test_eof; 
	_test_eof1272:  sm->cs = 1272; goto _test_eof; 
	_test_eof1273:  sm->cs = 1273; goto _test_eof; 
	_test_eof1274:  sm->cs = 1274; goto _test_eof; 
	_test_eof1275:  sm->cs = 1275; goto _test_eof; 
	_test_eof1276:  sm->cs = 1276; goto _test_eof; 
	_test_eof1277:  sm->cs = 1277; goto _test_eof; 
	_test_eof1278:  sm->cs = 1278; goto _test_eof; 
	_test_eof1279:  sm->cs = 1279; goto _test_eof; 
	_test_eof1280:  sm->cs = 1280; goto _test_eof; 
	_test_eof1281:  sm->cs = 1281; goto _test_eof; 
	_test_eof1282:  sm->cs = 1282; goto _test_eof; 
	_test_eof1283:  sm->cs = 1283; goto _test_eof; 
	_test_eof1284:  sm->cs = 1284; goto _test_eof; 
	_test_eof1285:  sm->cs = 1285; goto _test_eof; 
	_test_eof1286:  sm->cs = 1286; goto _test_eof; 
	_test_eof1287:  sm->cs = 1287; goto _test_eof; 
	_test_eof1288:  sm->cs = 1288; goto _test_eof; 
	_test_eof1289:  sm->cs = 1289; goto _test_eof; 
	_test_eof1290:  sm->cs = 1290; goto _test_eof; 
	_test_eof1291:  sm->cs = 1291; goto _test_eof; 
	_test_eof1292:  sm->cs = 1292; goto _test_eof; 
	_test_eof1293:  sm->cs = 1293; goto _test_eof; 
	_test_eof1294:  sm->cs = 1294; goto _test_eof; 
	_test_eof1295:  sm->cs = 1295; goto _test_eof; 
	_test_eof1296:  sm->cs = 1296; goto _test_eof; 
	_test_eof1297:  sm->cs = 1297; goto _test_eof; 
	_test_eof1298:  sm->cs = 1298; goto _test_eof; 
	_test_eof1299:  sm->cs = 1299; goto _test_eof; 
	_test_eof1300:  sm->cs = 1300; goto _test_eof; 
	_test_eof1301:  sm->cs = 1301; goto _test_eof; 
	_test_eof1302:  sm->cs = 1302; goto _test_eof; 
	_test_eof1303:  sm->cs = 1303; goto _test_eof; 
	_test_eof1304:  sm->cs = 1304; goto _test_eof; 
	_test_eof1305:  sm->cs = 1305; goto _test_eof; 
	_test_eof1306:  sm->cs = 1306; goto _test_eof; 
	_test_eof1307:  sm->cs = 1307; goto _test_eof; 
	_test_eof1308:  sm->cs = 1308; goto _test_eof; 
	_test_eof1309:  sm->cs = 1309; goto _test_eof; 
	_test_eof1310:  sm->cs = 1310; goto _test_eof; 
	_test_eof1311:  sm->cs = 1311; goto _test_eof; 
	_test_eof1312:  sm->cs = 1312; goto _test_eof; 
	_test_eof1313:  sm->cs = 1313; goto _test_eof; 
	_test_eof1314:  sm->cs = 1314; goto _test_eof; 
	_test_eof1315:  sm->cs = 1315; goto _test_eof; 
	_test_eof1316:  sm->cs = 1316; goto _test_eof; 
	_test_eof1317:  sm->cs = 1317; goto _test_eof; 
	_test_eof1318:  sm->cs = 1318; goto _test_eof; 
	_test_eof1319:  sm->cs = 1319; goto _test_eof; 
	_test_eof1320:  sm->cs = 1320; goto _test_eof; 
	_test_eof1321:  sm->cs = 1321; goto _test_eof; 
	_test_eof1322:  sm->cs = 1322; goto _test_eof; 
	_test_eof1323:  sm->cs = 1323; goto _test_eof; 
	_test_eof1324:  sm->cs = 1324; goto _test_eof; 
	_test_eof1325:  sm->cs = 1325; goto _test_eof; 
	_test_eof1326:  sm->cs = 1326; goto _test_eof; 
	_test_eof1327:  sm->cs = 1327; goto _test_eof; 
	_test_eof1328:  sm->cs = 1328; goto _test_eof; 
	_test_eof1329:  sm->cs = 1329; goto _test_eof; 
	_test_eof1330:  sm->cs = 1330; goto _test_eof; 
	_test_eof1331:  sm->cs = 1331; goto _test_eof; 
	_test_eof1332:  sm->cs = 1332; goto _test_eof; 
	_test_eof1333:  sm->cs = 1333; goto _test_eof; 
	_test_eof1334:  sm->cs = 1334; goto _test_eof; 
	_test_eof1335:  sm->cs = 1335; goto _test_eof; 
	_test_eof1336:  sm->cs = 1336; goto _test_eof; 
	_test_eof1337:  sm->cs = 1337; goto _test_eof; 
	_test_eof1338:  sm->cs = 1338; goto _test_eof; 
	_test_eof1339:  sm->cs = 1339; goto _test_eof; 
	_test_eof1340:  sm->cs = 1340; goto _test_eof; 
	_test_eof1341:  sm->cs = 1341; goto _test_eof; 
	_test_eof1342:  sm->cs = 1342; goto _test_eof; 
	_test_eof1343:  sm->cs = 1343; goto _test_eof; 
	_test_eof1344:  sm->cs = 1344; goto _test_eof; 
	_test_eof1345:  sm->cs = 1345; goto _test_eof; 
	_test_eof1346:  sm->cs = 1346; goto _test_eof; 
	_test_eof1347:  sm->cs = 1347; goto _test_eof; 
	_test_eof1348:  sm->cs = 1348; goto _test_eof; 
	_test_eof1349:  sm->cs = 1349; goto _test_eof; 
	_test_eof1350:  sm->cs = 1350; goto _test_eof; 
	_test_eof1351:  sm->cs = 1351; goto _test_eof; 
	_test_eof1352:  sm->cs = 1352; goto _test_eof; 
	_test_eof1353:  sm->cs = 1353; goto _test_eof; 
	_test_eof1354:  sm->cs = 1354; goto _test_eof; 
	_test_eof1355:  sm->cs = 1355; goto _test_eof; 
	_test_eof1356:  sm->cs = 1356; goto _test_eof; 
	_test_eof1357:  sm->cs = 1357; goto _test_eof; 
	_test_eof1358:  sm->cs = 1358; goto _test_eof; 
	_test_eof1359:  sm->cs = 1359; goto _test_eof; 
	_test_eof1360:  sm->cs = 1360; goto _test_eof; 
	_test_eof1361:  sm->cs = 1361; goto _test_eof; 
	_test_eof1362:  sm->cs = 1362; goto _test_eof; 
	_test_eof1363:  sm->cs = 1363; goto _test_eof; 
	_test_eof1364:  sm->cs = 1364; goto _test_eof; 
	_test_eof1365:  sm->cs = 1365; goto _test_eof; 
	_test_eof1366:  sm->cs = 1366; goto _test_eof; 

	_test_eof: {}
	if ( ( sm->p) == ( sm->eof) )
	{
	switch (  sm->cs ) {
	case 1368: goto tr1723;
	case 1: goto tr0;
	case 1369: goto tr1724;
	case 2: goto tr3;
	case 3: goto tr3;
	case 4: goto tr3;
	case 5: goto tr3;
	case 6: goto tr3;
	case 7: goto tr3;
	case 8: goto tr3;
	case 9: goto tr3;
	case 10: goto tr3;
	case 11: goto tr3;
	case 12: goto tr3;
	case 1370: goto tr1725;
	case 13: goto tr3;
	case 14: goto tr3;
	case 15: goto tr3;
	case 16: goto tr3;
	case 17: goto tr3;
	case 18: goto tr3;
	case 19: goto tr3;
	case 20: goto tr3;
	case 21: goto tr3;
	case 22: goto tr3;
	case 23: goto tr3;
	case 24: goto tr3;
	case 25: goto tr3;
	case 26: goto tr3;
	case 27: goto tr3;
	case 28: goto tr3;
	case 29: goto tr3;
	case 30: goto tr3;
	case 31: goto tr3;
	case 1371: goto tr1724;
	case 32: goto tr3;
	case 1372: goto tr1726;
	case 1373: goto tr1726;
	case 33: goto tr3;
	case 1374: goto tr1724;
	case 34: goto tr3;
	case 35: goto tr3;
	case 36: goto tr3;
	case 37: goto tr3;
	case 38: goto tr3;
	case 39: goto tr3;
	case 40: goto tr3;
	case 41: goto tr3;
	case 42: goto tr3;
	case 43: goto tr3;
	case 1375: goto tr1734;
	case 44: goto tr3;
	case 45: goto tr3;
	case 46: goto tr3;
	case 47: goto tr3;
	case 48: goto tr3;
	case 49: goto tr3;
	case 50: goto tr3;
	case 1376: goto tr1735;
	case 51: goto tr61;
	case 1377: goto tr1736;
	case 52: goto tr64;
	case 53: goto tr3;
	case 54: goto tr3;
	case 55: goto tr3;
	case 56: goto tr3;
	case 57: goto tr3;
	case 58: goto tr3;
	case 59: goto tr3;
	case 60: goto tr3;
	case 61: goto tr3;
	case 62: goto tr3;
	case 63: goto tr3;
	case 64: goto tr3;
	case 65: goto tr3;
	case 66: goto tr3;
	case 1378: goto tr1737;
	case 67: goto tr3;
	case 1379: goto tr1739;
	case 68: goto tr3;
	case 69: goto tr3;
	case 70: goto tr3;
	case 71: goto tr3;
	case 72: goto tr3;
	case 73: goto tr3;
	case 74: goto tr3;
	case 1380: goto tr1740;
	case 75: goto tr99;
	case 76: goto tr3;
	case 77: goto tr3;
	case 78: goto tr3;
	case 79: goto tr3;
	case 80: goto tr3;
	case 81: goto tr3;
	case 82: goto tr3;
	case 1381: goto tr1741;
	case 83: goto tr3;
	case 84: goto tr3;
	case 85: goto tr3;
	case 1382: goto tr1724;
	case 86: goto tr3;
	case 87: goto tr3;
	case 88: goto tr3;
	case 1383: goto tr1743;
	case 1384: goto tr1724;
	case 89: goto tr3;
	case 90: goto tr3;
	case 91: goto tr3;
	case 92: goto tr3;
	case 93: goto tr3;
	case 94: goto tr3;
	case 95: goto tr3;
	case 96: goto tr3;
	case 97: goto tr3;
	case 98: goto tr3;
	case 99: goto tr3;
	case 100: goto tr3;
	case 101: goto tr3;
	case 102: goto tr3;
	case 103: goto tr3;
	case 104: goto tr3;
	case 105: goto tr3;
	case 106: goto tr3;
	case 107: goto tr3;
	case 108: goto tr3;
	case 109: goto tr3;
	case 110: goto tr3;
	case 111: goto tr3;
	case 112: goto tr3;
	case 113: goto tr3;
	case 114: goto tr3;
	case 115: goto tr3;
	case 116: goto tr3;
	case 117: goto tr3;
	case 118: goto tr3;
	case 119: goto tr3;
	case 120: goto tr3;
	case 121: goto tr3;
	case 122: goto tr3;
	case 123: goto tr3;
	case 124: goto tr3;
	case 125: goto tr3;
	case 126: goto tr3;
	case 127: goto tr3;
	case 128: goto tr3;
	case 129: goto tr3;
	case 130: goto tr3;
	case 131: goto tr3;
	case 132: goto tr3;
	case 1385: goto tr1724;
	case 133: goto tr3;
	case 134: goto tr3;
	case 135: goto tr3;
	case 136: goto tr3;
	case 137: goto tr3;
	case 138: goto tr3;
	case 139: goto tr3;
	case 140: goto tr3;
	case 141: goto tr3;
	case 142: goto tr3;
	case 1387: goto tr1756;
	case 143: goto tr179;
	case 144: goto tr179;
	case 145: goto tr179;
	case 146: goto tr179;
	case 147: goto tr179;
	case 148: goto tr179;
	case 149: goto tr179;
	case 150: goto tr179;
	case 151: goto tr179;
	case 152: goto tr179;
	case 153: goto tr179;
	case 154: goto tr179;
	case 155: goto tr179;
	case 156: goto tr179;
	case 157: goto tr179;
	case 158: goto tr179;
	case 159: goto tr179;
	case 160: goto tr179;
	case 161: goto tr179;
	case 1388: goto tr1756;
	case 162: goto tr179;
	case 163: goto tr179;
	case 164: goto tr179;
	case 165: goto tr179;
	case 166: goto tr179;
	case 167: goto tr179;
	case 168: goto tr179;
	case 169: goto tr179;
	case 170: goto tr179;
	case 1390: goto tr1795;
	case 1391: goto tr1796;
	case 171: goto tr207;
	case 172: goto tr207;
	case 173: goto tr210;
	case 1392: goto tr1795;
	case 1393: goto tr1795;
	case 1394: goto tr207;
	case 174: goto tr207;
	case 1395: goto tr1795;
	case 175: goto tr214;
	case 1396: goto tr1798;
	case 176: goto tr216;
	case 177: goto tr216;
	case 178: goto tr216;
	case 179: goto tr216;
	case 180: goto tr216;
	case 1397: goto tr1805;
	case 181: goto tr216;
	case 182: goto tr216;
	case 183: goto tr216;
	case 184: goto tr216;
	case 185: goto tr216;
	case 186: goto tr216;
	case 187: goto tr216;
	case 188: goto tr216;
	case 189: goto tr216;
	case 190: goto tr216;
	case 191: goto tr216;
	case 192: goto tr216;
	case 193: goto tr216;
	case 194: goto tr216;
	case 195: goto tr216;
	case 196: goto tr216;
	case 197: goto tr216;
	case 198: goto tr216;
	case 199: goto tr216;
	case 200: goto tr216;
	case 201: goto tr216;
	case 202: goto tr216;
	case 203: goto tr216;
	case 204: goto tr216;
	case 205: goto tr216;
	case 206: goto tr216;
	case 207: goto tr216;
	case 208: goto tr216;
	case 209: goto tr216;
	case 210: goto tr216;
	case 1398: goto tr1806;
	case 211: goto tr255;
	case 212: goto tr255;
	case 213: goto tr207;
	case 214: goto tr207;
	case 215: goto tr207;
	case 216: goto tr207;
	case 217: goto tr207;
	case 218: goto tr207;
	case 1399: goto tr1809;
	case 219: goto tr207;
	case 220: goto tr207;
	case 221: goto tr207;
	case 222: goto tr207;
	case 223: goto tr207;
	case 224: goto tr207;
	case 225: goto tr207;
	case 226: goto tr207;
	case 227: goto tr255;
	case 228: goto tr255;
	case 229: goto tr207;
	case 230: goto tr207;
	case 231: goto tr207;
	case 232: goto tr207;
	case 233: goto tr207;
	case 234: goto tr207;
	case 235: goto tr207;
	case 236: goto tr207;
	case 237: goto tr207;
	case 238: goto tr207;
	case 239: goto tr207;
	case 240: goto tr207;
	case 241: goto tr207;
	case 242: goto tr207;
	case 243: goto tr216;
	case 244: goto tr216;
	case 1400: goto tr1811;
	case 1401: goto tr1811;
	case 245: goto tr216;
	case 246: goto tr216;
	case 247: goto tr216;
	case 248: goto tr207;
	case 249: goto tr207;
	case 250: goto tr207;
	case 251: goto tr207;
	case 252: goto tr207;
	case 253: goto tr207;
	case 254: goto tr207;
	case 255: goto tr207;
	case 256: goto tr207;
	case 1402: goto tr1813;
	case 257: goto tr216;
	case 258: goto tr207;
	case 259: goto tr207;
	case 260: goto tr207;
	case 261: goto tr207;
	case 262: goto tr207;
	case 1403: goto tr1814;
	case 263: goto tr207;
	case 264: goto tr207;
	case 265: goto tr207;
	case 266: goto tr207;
	case 267: goto tr207;
	case 268: goto tr216;
	case 269: goto tr207;
	case 270: goto tr207;
	case 271: goto tr207;
	case 272: goto tr207;
	case 273: goto tr207;
	case 274: goto tr207;
	case 275: goto tr207;
	case 276: goto tr216;
	case 277: goto tr216;
	case 278: goto tr216;
	case 279: goto tr216;
	case 280: goto tr216;
	case 281: goto tr216;
	case 282: goto tr216;
	case 283: goto tr216;
	case 284: goto tr216;
	case 285: goto tr216;
	case 286: goto tr216;
	case 287: goto tr216;
	case 288: goto tr216;
	case 289: goto tr216;
	case 290: goto tr216;
	case 291: goto tr216;
	case 292: goto tr216;
	case 293: goto tr216;
	case 1404: goto tr1815;
	case 294: goto tr216;
	case 295: goto tr216;
	case 296: goto tr207;
	case 297: goto tr207;
	case 298: goto tr207;
	case 299: goto tr207;
	case 300: goto tr207;
	case 301: goto tr207;
	case 302: goto tr216;
	case 303: goto tr207;
	case 304: goto tr207;
	case 305: goto tr207;
	case 306: goto tr207;
	case 307: goto tr207;
	case 308: goto tr207;
	case 309: goto tr207;
	case 310: goto tr216;
	case 311: goto tr216;
	case 312: goto tr216;
	case 313: goto tr216;
	case 314: goto tr216;
	case 315: goto tr216;
	case 316: goto tr216;
	case 317: goto tr216;
	case 318: goto tr216;
	case 319: goto tr216;
	case 320: goto tr216;
	case 321: goto tr216;
	case 322: goto tr216;
	case 323: goto tr216;
	case 324: goto tr216;
	case 325: goto tr216;
	case 326: goto tr216;
	case 327: goto tr216;
	case 328: goto tr216;
	case 329: goto tr216;
	case 330: goto tr216;
	case 331: goto tr216;
	case 332: goto tr216;
	case 333: goto tr216;
	case 334: goto tr216;
	case 1405: goto tr1795;
	case 335: goto tr214;
	case 336: goto tr214;
	case 337: goto tr214;
	case 1406: goto tr1818;
	case 338: goto tr406;
	case 339: goto tr406;
	case 340: goto tr406;
	case 341: goto tr406;
	case 342: goto tr406;
	case 343: goto tr406;
	case 344: goto tr406;
	case 345: goto tr406;
	case 346: goto tr406;
	case 347: goto tr406;
	case 348: goto tr406;
	case 1407: goto tr1818;
	case 349: goto tr406;
	case 350: goto tr406;
	case 351: goto tr406;
	case 352: goto tr406;
	case 353: goto tr406;
	case 354: goto tr406;
	case 355: goto tr406;
	case 356: goto tr406;
	case 357: goto tr406;
	case 358: goto tr406;
	case 359: goto tr406;
	case 360: goto tr207;
	case 361: goto tr207;
	case 1408: goto tr1818;
	case 362: goto tr207;
	case 363: goto tr207;
	case 364: goto tr207;
	case 365: goto tr207;
	case 366: goto tr207;
	case 367: goto tr207;
	case 368: goto tr207;
	case 369: goto tr207;
	case 370: goto tr207;
	case 371: goto tr214;
	case 372: goto tr214;
	case 373: goto tr214;
	case 374: goto tr214;
	case 375: goto tr214;
	case 376: goto tr214;
	case 377: goto tr214;
	case 378: goto tr214;
	case 379: goto tr214;
	case 380: goto tr214;
	case 381: goto tr214;
	case 382: goto tr207;
	case 383: goto tr207;
	case 1409: goto tr1818;
	case 384: goto tr207;
	case 385: goto tr207;
	case 386: goto tr207;
	case 387: goto tr207;
	case 388: goto tr207;
	case 389: goto tr207;
	case 390: goto tr207;
	case 391: goto tr207;
	case 392: goto tr207;
	case 393: goto tr207;
	case 394: goto tr207;
	case 1410: goto tr1818;
	case 395: goto tr214;
	case 396: goto tr214;
	case 397: goto tr214;
	case 398: goto tr214;
	case 399: goto tr214;
	case 400: goto tr214;
	case 401: goto tr214;
	case 402: goto tr214;
	case 403: goto tr214;
	case 404: goto tr214;
	case 405: goto tr214;
	case 1411: goto tr1796;
	case 406: goto tr210;
	case 407: goto tr207;
	case 408: goto tr207;
	case 409: goto tr207;
	case 410: goto tr207;
	case 411: goto tr207;
	case 412: goto tr207;
	case 413: goto tr207;
	case 414: goto tr207;
	case 1412: goto tr1822;
	case 1413: goto tr1824;
	case 415: goto tr207;
	case 416: goto tr207;
	case 417: goto tr207;
	case 418: goto tr207;
	case 1414: goto tr1826;
	case 1415: goto tr1828;
	case 419: goto tr207;
	case 420: goto tr207;
	case 421: goto tr207;
	case 422: goto tr207;
	case 423: goto tr207;
	case 424: goto tr207;
	case 425: goto tr207;
	case 426: goto tr207;
	case 427: goto tr207;
	case 1416: goto tr1822;
	case 1417: goto tr1824;
	case 428: goto tr207;
	case 429: goto tr207;
	case 430: goto tr207;
	case 431: goto tr207;
	case 432: goto tr207;
	case 433: goto tr207;
	case 434: goto tr207;
	case 435: goto tr207;
	case 436: goto tr207;
	case 437: goto tr207;
	case 438: goto tr207;
	case 439: goto tr207;
	case 440: goto tr207;
	case 441: goto tr207;
	case 442: goto tr207;
	case 443: goto tr207;
	case 444: goto tr207;
	case 445: goto tr207;
	case 446: goto tr207;
	case 447: goto tr207;
	case 448: goto tr207;
	case 449: goto tr207;
	case 450: goto tr207;
	case 451: goto tr207;
	case 452: goto tr207;
	case 453: goto tr207;
	case 454: goto tr207;
	case 455: goto tr207;
	case 456: goto tr207;
	case 457: goto tr207;
	case 458: goto tr207;
	case 459: goto tr210;
	case 460: goto tr207;
	case 461: goto tr207;
	case 462: goto tr207;
	case 463: goto tr207;
	case 464: goto tr207;
	case 465: goto tr207;
	case 466: goto tr207;
	case 467: goto tr207;
	case 468: goto tr207;
	case 469: goto tr207;
	case 470: goto tr207;
	case 1418: goto tr1832;
	case 1419: goto tr1834;
	case 471: goto tr207;
	case 1420: goto tr1836;
	case 1421: goto tr1838;
	case 472: goto tr207;
	case 473: goto tr207;
	case 474: goto tr207;
	case 475: goto tr207;
	case 476: goto tr207;
	case 477: goto tr207;
	case 478: goto tr207;
	case 479: goto tr207;
	case 1422: goto tr1836;
	case 1423: goto tr1838;
	case 480: goto tr207;
	case 1424: goto tr1836;
	case 481: goto tr207;
	case 482: goto tr207;
	case 483: goto tr207;
	case 484: goto tr207;
	case 485: goto tr207;
	case 486: goto tr207;
	case 487: goto tr207;
	case 488: goto tr207;
	case 489: goto tr207;
	case 490: goto tr207;
	case 491: goto tr207;
	case 492: goto tr207;
	case 493: goto tr207;
	case 494: goto tr207;
	case 495: goto tr207;
	case 496: goto tr207;
	case 497: goto tr207;
	case 498: goto tr207;
	case 1425: goto tr1836;
	case 499: goto tr207;
	case 500: goto tr207;
	case 501: goto tr207;
	case 502: goto tr207;
	case 503: goto tr207;
	case 504: goto tr207;
	case 505: goto tr207;
	case 506: goto tr207;
	case 507: goto tr207;
	case 1426: goto tr1796;
	case 1427: goto tr1796;
	case 1428: goto tr1796;
	case 1429: goto tr1796;
	case 1430: goto tr1796;
	case 1431: goto tr1796;
	case 508: goto tr210;
	case 509: goto tr210;
	case 1432: goto tr1849;
	case 1433: goto tr1849;
	case 1434: goto tr1849;
	case 1435: goto tr1849;
	case 1436: goto tr1849;
	case 1437: goto tr1849;
	case 1438: goto tr1849;
	case 1439: goto tr1849;
	case 1440: goto tr1849;
	case 1441: goto tr1849;
	case 1442: goto tr1849;
	case 1443: goto tr1849;
	case 510: goto tr207;
	case 511: goto tr207;
	case 512: goto tr207;
	case 513: goto tr207;
	case 514: goto tr207;
	case 515: goto tr207;
	case 516: goto tr207;
	case 517: goto tr207;
	case 518: goto tr207;
	case 519: goto tr210;
	case 1444: goto tr1796;
	case 1445: goto tr1796;
	case 1446: goto tr1796;
	case 1447: goto tr1796;
	case 520: goto tr210;
	case 521: goto tr210;
	case 1448: goto tr1864;
	case 1449: goto tr1864;
	case 1450: goto tr1864;
	case 1451: goto tr1864;
	case 1452: goto tr1864;
	case 1453: goto tr1864;
	case 1454: goto tr1864;
	case 1455: goto tr1864;
	case 1456: goto tr1864;
	case 1457: goto tr1864;
	case 1458: goto tr1864;
	case 1459: goto tr1864;
	case 522: goto tr207;
	case 523: goto tr207;
	case 524: goto tr207;
	case 525: goto tr207;
	case 526: goto tr207;
	case 527: goto tr207;
	case 528: goto tr207;
	case 529: goto tr207;
	case 530: goto tr207;
	case 531: goto tr210;
	case 1460: goto tr1796;
	case 1461: goto tr1796;
	case 1462: goto tr1796;
	case 1463: goto tr1796;
	case 1464: goto tr1796;
	case 1465: goto tr1796;
	case 1466: goto tr1796;
	case 532: goto tr210;
	case 533: goto tr210;
	case 1467: goto tr1882;
	case 1468: goto tr1882;
	case 1469: goto tr1882;
	case 1470: goto tr1882;
	case 1471: goto tr1882;
	case 1472: goto tr1882;
	case 1473: goto tr1882;
	case 1474: goto tr1882;
	case 1475: goto tr1882;
	case 1476: goto tr1882;
	case 1477: goto tr1882;
	case 1478: goto tr1882;
	case 534: goto tr207;
	case 535: goto tr207;
	case 536: goto tr207;
	case 537: goto tr207;
	case 538: goto tr207;
	case 539: goto tr207;
	case 540: goto tr207;
	case 541: goto tr207;
	case 542: goto tr207;
	case 543: goto tr210;
	case 1479: goto tr1796;
	case 1480: goto tr1796;
	case 1481: goto tr1796;
	case 1482: goto tr1796;
	case 1483: goto tr1796;
	case 544: goto tr210;
	case 545: goto tr210;
	case 1484: goto tr1898;
	case 546: goto tr698;
	case 1485: goto tr1901;
	case 1486: goto tr1898;
	case 1487: goto tr1898;
	case 1488: goto tr1898;
	case 1489: goto tr1898;
	case 1490: goto tr1898;
	case 1491: goto tr1898;
	case 1492: goto tr1898;
	case 1493: goto tr1898;
	case 1494: goto tr1898;
	case 1495: goto tr1898;
	case 1496: goto tr1898;
	case 547: goto tr207;
	case 548: goto tr207;
	case 549: goto tr207;
	case 550: goto tr207;
	case 551: goto tr207;
	case 552: goto tr207;
	case 553: goto tr207;
	case 554: goto tr207;
	case 555: goto tr207;
	case 556: goto tr210;
	case 1497: goto tr1796;
	case 1498: goto tr1796;
	case 1499: goto tr1796;
	case 1500: goto tr1796;
	case 1501: goto tr1796;
	case 557: goto tr210;
	case 558: goto tr210;
	case 1502: goto tr1917;
	case 1503: goto tr1917;
	case 1504: goto tr1917;
	case 1505: goto tr1917;
	case 1506: goto tr1917;
	case 1507: goto tr1917;
	case 1508: goto tr1917;
	case 1509: goto tr1917;
	case 1510: goto tr1917;
	case 1511: goto tr1917;
	case 1512: goto tr1917;
	case 1513: goto tr1917;
	case 559: goto tr207;
	case 560: goto tr207;
	case 561: goto tr207;
	case 562: goto tr207;
	case 563: goto tr207;
	case 564: goto tr207;
	case 565: goto tr207;
	case 566: goto tr207;
	case 567: goto tr207;
	case 568: goto tr210;
	case 1514: goto tr1796;
	case 1515: goto tr1796;
	case 1516: goto tr1796;
	case 1517: goto tr1796;
	case 569: goto tr210;
	case 570: goto tr210;
	case 571: goto tr210;
	case 572: goto tr210;
	case 573: goto tr210;
	case 574: goto tr210;
	case 575: goto tr210;
	case 576: goto tr207;
	case 577: goto tr207;
	case 1518: goto tr1933;
	case 578: goto tr207;
	case 579: goto tr207;
	case 580: goto tr207;
	case 581: goto tr207;
	case 582: goto tr207;
	case 583: goto tr207;
	case 584: goto tr207;
	case 585: goto tr207;
	case 586: goto tr207;
	case 587: goto tr207;
	case 1519: goto tr1933;
	case 588: goto tr746;
	case 589: goto tr746;
	case 590: goto tr746;
	case 591: goto tr746;
	case 592: goto tr746;
	case 593: goto tr746;
	case 594: goto tr746;
	case 595: goto tr746;
	case 596: goto tr746;
	case 597: goto tr746;
	case 598: goto tr746;
	case 1520: goto tr1933;
	case 599: goto tr746;
	case 600: goto tr746;
	case 601: goto tr746;
	case 602: goto tr746;
	case 603: goto tr746;
	case 604: goto tr746;
	case 605: goto tr746;
	case 606: goto tr746;
	case 607: goto tr746;
	case 608: goto tr746;
	case 609: goto tr746;
	case 610: goto tr207;
	case 611: goto tr207;
	case 1521: goto tr1933;
	case 612: goto tr207;
	case 613: goto tr207;
	case 614: goto tr207;
	case 615: goto tr207;
	case 616: goto tr207;
	case 617: goto tr207;
	case 618: goto tr207;
	case 619: goto tr207;
	case 620: goto tr207;
	case 621: goto tr207;
	case 1522: goto tr1933;
	case 1523: goto tr1796;
	case 1524: goto tr1796;
	case 1525: goto tr1796;
	case 1526: goto tr1796;
	case 622: goto tr210;
	case 623: goto tr210;
	case 624: goto tr210;
	case 625: goto tr210;
	case 626: goto tr210;
	case 627: goto tr210;
	case 628: goto tr210;
	case 629: goto tr210;
	case 630: goto tr210;
	case 1527: goto tr1937;
	case 1528: goto tr1937;
	case 1529: goto tr1937;
	case 1530: goto tr1937;
	case 1531: goto tr1937;
	case 1532: goto tr1937;
	case 1533: goto tr1937;
	case 1534: goto tr1937;
	case 1535: goto tr1937;
	case 1536: goto tr1937;
	case 1537: goto tr1937;
	case 1538: goto tr1937;
	case 631: goto tr207;
	case 632: goto tr207;
	case 633: goto tr207;
	case 634: goto tr207;
	case 635: goto tr207;
	case 636: goto tr207;
	case 637: goto tr207;
	case 638: goto tr207;
	case 639: goto tr207;
	case 640: goto tr210;
	case 1539: goto tr1796;
	case 1540: goto tr1796;
	case 1541: goto tr1796;
	case 1542: goto tr1796;
	case 1543: goto tr1796;
	case 641: goto tr210;
	case 642: goto tr210;
	case 643: goto tr210;
	case 644: goto tr210;
	case 645: goto tr210;
	case 1544: goto tr1954;
	case 646: goto tr210;
	case 647: goto tr210;
	case 648: goto tr210;
	case 649: goto tr210;
	case 650: goto tr210;
	case 651: goto tr210;
	case 652: goto tr210;
	case 653: goto tr210;
	case 654: goto tr210;
	case 655: goto tr210;
	case 656: goto tr210;
	case 657: goto tr210;
	case 658: goto tr210;
	case 659: goto tr210;
	case 660: goto tr210;
	case 661: goto tr210;
	case 662: goto tr210;
	case 663: goto tr210;
	case 664: goto tr210;
	case 665: goto tr210;
	case 666: goto tr210;
	case 1545: goto tr1796;
	case 1546: goto tr1796;
	case 1547: goto tr1796;
	case 667: goto tr210;
	case 668: goto tr210;
	case 1548: goto tr1960;
	case 1549: goto tr1960;
	case 1550: goto tr1960;
	case 1551: goto tr1960;
	case 1552: goto tr1960;
	case 1553: goto tr1960;
	case 1554: goto tr1960;
	case 1555: goto tr1960;
	case 1556: goto tr1960;
	case 1557: goto tr1960;
	case 1558: goto tr1960;
	case 1559: goto tr1960;
	case 669: goto tr207;
	case 670: goto tr207;
	case 671: goto tr207;
	case 672: goto tr207;
	case 673: goto tr207;
	case 674: goto tr207;
	case 675: goto tr207;
	case 676: goto tr207;
	case 677: goto tr207;
	case 678: goto tr210;
	case 1560: goto tr1796;
	case 1561: goto tr1796;
	case 679: goto tr210;
	case 680: goto tr210;
	case 1562: goto tr1973;
	case 1563: goto tr1973;
	case 1564: goto tr1973;
	case 1565: goto tr1973;
	case 1566: goto tr1973;
	case 1567: goto tr1973;
	case 1568: goto tr1973;
	case 1569: goto tr1973;
	case 1570: goto tr1973;
	case 1571: goto tr1973;
	case 1572: goto tr1973;
	case 1573: goto tr1973;
	case 681: goto tr207;
	case 682: goto tr207;
	case 683: goto tr207;
	case 684: goto tr207;
	case 685: goto tr207;
	case 686: goto tr207;
	case 687: goto tr207;
	case 688: goto tr207;
	case 689: goto tr207;
	case 690: goto tr210;
	case 1574: goto tr1796;
	case 1575: goto tr1796;
	case 1576: goto tr1796;
	case 1577: goto tr1796;
	case 1578: goto tr1796;
	case 1579: goto tr1796;
	case 691: goto tr210;
	case 692: goto tr210;
	case 1580: goto tr1990;
	case 1581: goto tr1990;
	case 1582: goto tr1990;
	case 1583: goto tr1990;
	case 1584: goto tr1990;
	case 1585: goto tr1990;
	case 1586: goto tr1990;
	case 1587: goto tr1990;
	case 1588: goto tr1990;
	case 1589: goto tr1990;
	case 1590: goto tr1990;
	case 1591: goto tr1990;
	case 693: goto tr207;
	case 694: goto tr207;
	case 695: goto tr207;
	case 696: goto tr207;
	case 697: goto tr207;
	case 698: goto tr207;
	case 699: goto tr207;
	case 700: goto tr207;
	case 701: goto tr207;
	case 702: goto tr210;
	case 1592: goto tr1796;
	case 1593: goto tr1796;
	case 1594: goto tr1796;
	case 1595: goto tr1796;
	case 1596: goto tr1796;
	case 1597: goto tr1796;
	case 703: goto tr210;
	case 704: goto tr210;
	case 1598: goto tr2007;
	case 1599: goto tr2007;
	case 1600: goto tr2007;
	case 1601: goto tr2007;
	case 1602: goto tr2007;
	case 1603: goto tr2007;
	case 1604: goto tr2007;
	case 1605: goto tr2007;
	case 1606: goto tr2007;
	case 1607: goto tr2007;
	case 1608: goto tr2007;
	case 1609: goto tr2007;
	case 705: goto tr207;
	case 706: goto tr207;
	case 707: goto tr207;
	case 708: goto tr207;
	case 709: goto tr207;
	case 710: goto tr207;
	case 711: goto tr207;
	case 712: goto tr207;
	case 713: goto tr207;
	case 714: goto tr210;
	case 1610: goto tr1796;
	case 1611: goto tr1796;
	case 1612: goto tr1796;
	case 715: goto tr210;
	case 716: goto tr210;
	case 717: goto tr210;
	case 718: goto tr210;
	case 719: goto tr210;
	case 720: goto tr210;
	case 721: goto tr210;
	case 722: goto tr210;
	case 1613: goto tr2022;
	case 1614: goto tr2022;
	case 1615: goto tr2022;
	case 1616: goto tr2022;
	case 1617: goto tr2022;
	case 1618: goto tr2022;
	case 1619: goto tr2022;
	case 1620: goto tr2022;
	case 1621: goto tr2022;
	case 1622: goto tr2022;
	case 1623: goto tr2022;
	case 1624: goto tr2022;
	case 723: goto tr207;
	case 724: goto tr207;
	case 725: goto tr207;
	case 726: goto tr207;
	case 727: goto tr207;
	case 728: goto tr207;
	case 729: goto tr207;
	case 730: goto tr207;
	case 731: goto tr207;
	case 732: goto tr210;
	case 733: goto tr210;
	case 734: goto tr210;
	case 735: goto tr210;
	case 736: goto tr210;
	case 737: goto tr210;
	case 738: goto tr210;
	case 739: goto tr210;
	case 740: goto tr210;
	case 741: goto tr210;
	case 742: goto tr210;
	case 743: goto tr210;
	case 744: goto tr210;
	case 745: goto tr210;
	case 1625: goto tr2033;
	case 1626: goto tr2033;
	case 1627: goto tr2033;
	case 1628: goto tr2033;
	case 1629: goto tr2033;
	case 1630: goto tr2033;
	case 1631: goto tr2033;
	case 1632: goto tr2033;
	case 1633: goto tr2033;
	case 1634: goto tr2033;
	case 1635: goto tr2033;
	case 1636: goto tr2033;
	case 746: goto tr207;
	case 747: goto tr207;
	case 748: goto tr207;
	case 749: goto tr207;
	case 750: goto tr207;
	case 751: goto tr207;
	case 752: goto tr207;
	case 753: goto tr207;
	case 754: goto tr207;
	case 755: goto tr210;
	case 756: goto tr210;
	case 757: goto tr210;
	case 758: goto tr210;
	case 759: goto tr210;
	case 760: goto tr210;
	case 761: goto tr210;
	case 762: goto tr210;
	case 763: goto tr210;
	case 764: goto tr210;
	case 765: goto tr210;
	case 766: goto tr210;
	case 767: goto tr210;
	case 768: goto tr210;
	case 1637: goto tr2044;
	case 1638: goto tr2044;
	case 1639: goto tr2044;
	case 1640: goto tr2044;
	case 1641: goto tr2044;
	case 1642: goto tr2044;
	case 1643: goto tr2044;
	case 1644: goto tr2044;
	case 1645: goto tr2044;
	case 1646: goto tr2044;
	case 1647: goto tr2044;
	case 1648: goto tr2044;
	case 769: goto tr207;
	case 770: goto tr207;
	case 771: goto tr207;
	case 772: goto tr207;
	case 773: goto tr207;
	case 774: goto tr207;
	case 775: goto tr207;
	case 776: goto tr207;
	case 777: goto tr207;
	case 778: goto tr210;
	case 1649: goto tr1796;
	case 1650: goto tr1796;
	case 1651: goto tr1796;
	case 1652: goto tr1796;
	case 779: goto tr210;
	case 780: goto tr210;
	case 1653: goto tr2059;
	case 781: goto tr951;
	case 782: goto tr951;
	case 1654: goto tr2062;
	case 1655: goto tr2059;
	case 1656: goto tr2059;
	case 1657: goto tr2059;
	case 1658: goto tr2059;
	case 1659: goto tr2059;
	case 1660: goto tr2059;
	case 1661: goto tr2059;
	case 1662: goto tr2059;
	case 1663: goto tr2059;
	case 1664: goto tr2059;
	case 1665: goto tr2059;
	case 783: goto tr207;
	case 784: goto tr207;
	case 785: goto tr207;
	case 786: goto tr207;
	case 787: goto tr207;
	case 788: goto tr207;
	case 789: goto tr207;
	case 790: goto tr207;
	case 791: goto tr207;
	case 792: goto tr210;
	case 1666: goto tr1796;
	case 1667: goto tr1796;
	case 1668: goto tr1796;
	case 1669: goto tr1796;
	case 793: goto tr210;
	case 794: goto tr210;
	case 1670: goto tr2077;
	case 1671: goto tr2077;
	case 1672: goto tr2077;
	case 1673: goto tr2077;
	case 1674: goto tr2077;
	case 1675: goto tr2077;
	case 1676: goto tr2077;
	case 1677: goto tr2077;
	case 1678: goto tr2077;
	case 1679: goto tr2077;
	case 1680: goto tr2077;
	case 1681: goto tr2077;
	case 795: goto tr207;
	case 796: goto tr207;
	case 797: goto tr207;
	case 798: goto tr207;
	case 799: goto tr207;
	case 800: goto tr207;
	case 801: goto tr207;
	case 802: goto tr207;
	case 803: goto tr207;
	case 804: goto tr210;
	case 805: goto tr210;
	case 806: goto tr210;
	case 807: goto tr210;
	case 808: goto tr210;
	case 809: goto tr210;
	case 810: goto tr210;
	case 811: goto tr210;
	case 812: goto tr210;
	case 1682: goto tr2088;
	case 1683: goto tr2088;
	case 1684: goto tr2088;
	case 1685: goto tr2088;
	case 1686: goto tr2088;
	case 1687: goto tr2088;
	case 1688: goto tr2088;
	case 1689: goto tr2088;
	case 1690: goto tr2088;
	case 1691: goto tr2088;
	case 1692: goto tr2088;
	case 1693: goto tr2088;
	case 813: goto tr207;
	case 814: goto tr207;
	case 815: goto tr207;
	case 816: goto tr207;
	case 817: goto tr207;
	case 818: goto tr207;
	case 819: goto tr207;
	case 820: goto tr207;
	case 821: goto tr207;
	case 822: goto tr210;
	case 1694: goto tr1796;
	case 1695: goto tr1796;
	case 1696: goto tr1796;
	case 1697: goto tr1796;
	case 823: goto tr210;
	case 824: goto tr210;
	case 1698: goto tr2103;
	case 1699: goto tr2103;
	case 1700: goto tr2103;
	case 1701: goto tr2103;
	case 1702: goto tr2103;
	case 1703: goto tr2103;
	case 1704: goto tr2103;
	case 1705: goto tr2103;
	case 1706: goto tr2103;
	case 1707: goto tr2103;
	case 1708: goto tr2103;
	case 1709: goto tr2103;
	case 825: goto tr207;
	case 826: goto tr207;
	case 827: goto tr207;
	case 828: goto tr207;
	case 829: goto tr207;
	case 830: goto tr207;
	case 831: goto tr207;
	case 832: goto tr207;
	case 833: goto tr207;
	case 834: goto tr210;
	case 1710: goto tr1795;
	case 835: goto tr214;
	case 836: goto tr214;
	case 837: goto tr214;
	case 838: goto tr214;
	case 839: goto tr214;
	case 840: goto tr214;
	case 841: goto tr214;
	case 842: goto tr214;
	case 843: goto tr214;
	case 844: goto tr214;
	case 845: goto tr214;
	case 846: goto tr214;
	case 847: goto tr214;
	case 848: goto tr214;
	case 849: goto tr214;
	case 850: goto tr214;
	case 851: goto tr214;
	case 852: goto tr214;
	case 853: goto tr214;
	case 1711: goto tr2125;
	case 854: goto tr1037;
	case 1712: goto tr2126;
	case 855: goto tr1040;
	case 856: goto tr214;
	case 857: goto tr214;
	case 858: goto tr214;
	case 859: goto tr214;
	case 860: goto tr214;
	case 861: goto tr214;
	case 862: goto tr214;
	case 863: goto tr214;
	case 864: goto tr214;
	case 865: goto tr214;
	case 866: goto tr214;
	case 867: goto tr214;
	case 868: goto tr214;
	case 869: goto tr214;
	case 870: goto tr214;
	case 871: goto tr214;
	case 872: goto tr214;
	case 873: goto tr214;
	case 874: goto tr214;
	case 875: goto tr214;
	case 876: goto tr214;
	case 877: goto tr214;
	case 878: goto tr214;
	case 879: goto tr214;
	case 880: goto tr214;
	case 881: goto tr214;
	case 882: goto tr214;
	case 883: goto tr214;
	case 884: goto tr214;
	case 885: goto tr214;
	case 886: goto tr214;
	case 887: goto tr214;
	case 888: goto tr214;
	case 889: goto tr214;
	case 890: goto tr214;
	case 1713: goto tr2127;
	case 891: goto tr1089;
	case 892: goto tr214;
	case 893: goto tr214;
	case 894: goto tr214;
	case 895: goto tr214;
	case 896: goto tr214;
	case 897: goto tr214;
	case 898: goto tr214;
	case 899: goto tr214;
	case 900: goto tr214;
	case 901: goto tr214;
	case 902: goto tr214;
	case 903: goto tr214;
	case 904: goto tr214;
	case 905: goto tr214;
	case 906: goto tr214;
	case 907: goto tr214;
	case 908: goto tr214;
	case 909: goto tr214;
	case 910: goto tr214;
	case 911: goto tr214;
	case 912: goto tr214;
	case 913: goto tr214;
	case 914: goto tr214;
	case 915: goto tr214;
	case 916: goto tr214;
	case 917: goto tr214;
	case 918: goto tr214;
	case 919: goto tr214;
	case 920: goto tr214;
	case 921: goto tr214;
	case 922: goto tr214;
	case 923: goto tr214;
	case 924: goto tr214;
	case 925: goto tr214;
	case 926: goto tr214;
	case 927: goto tr214;
	case 928: goto tr214;
	case 929: goto tr214;
	case 930: goto tr214;
	case 931: goto tr214;
	case 932: goto tr214;
	case 933: goto tr214;
	case 934: goto tr214;
	case 935: goto tr214;
	case 936: goto tr214;
	case 937: goto tr214;
	case 938: goto tr214;
	case 939: goto tr214;
	case 940: goto tr214;
	case 941: goto tr214;
	case 942: goto tr214;
	case 943: goto tr214;
	case 944: goto tr214;
	case 945: goto tr214;
	case 946: goto tr214;
	case 947: goto tr214;
	case 948: goto tr214;
	case 949: goto tr214;
	case 950: goto tr214;
	case 951: goto tr214;
	case 952: goto tr214;
	case 953: goto tr214;
	case 954: goto tr214;
	case 955: goto tr214;
	case 956: goto tr214;
	case 957: goto tr214;
	case 958: goto tr214;
	case 959: goto tr214;
	case 960: goto tr214;
	case 961: goto tr214;
	case 962: goto tr214;
	case 963: goto tr214;
	case 964: goto tr214;
	case 965: goto tr214;
	case 966: goto tr214;
	case 967: goto tr214;
	case 968: goto tr214;
	case 969: goto tr214;
	case 970: goto tr214;
	case 971: goto tr214;
	case 1714: goto tr1795;
	case 1715: goto tr1795;
	case 972: goto tr214;
	case 973: goto tr214;
	case 974: goto tr214;
	case 975: goto tr214;
	case 976: goto tr214;
	case 977: goto tr214;
	case 978: goto tr214;
	case 979: goto tr214;
	case 980: goto tr214;
	case 981: goto tr214;
	case 982: goto tr214;
	case 983: goto tr214;
	case 984: goto tr214;
	case 985: goto tr214;
	case 986: goto tr214;
	case 987: goto tr214;
	case 988: goto tr214;
	case 989: goto tr214;
	case 990: goto tr214;
	case 991: goto tr214;
	case 992: goto tr214;
	case 993: goto tr214;
	case 994: goto tr214;
	case 995: goto tr214;
	case 996: goto tr214;
	case 997: goto tr214;
	case 998: goto tr214;
	case 999: goto tr214;
	case 1000: goto tr214;
	case 1001: goto tr214;
	case 1002: goto tr214;
	case 1003: goto tr214;
	case 1004: goto tr214;
	case 1005: goto tr214;
	case 1006: goto tr214;
	case 1007: goto tr214;
	case 1008: goto tr214;
	case 1009: goto tr214;
	case 1010: goto tr214;
	case 1011: goto tr214;
	case 1012: goto tr214;
	case 1013: goto tr214;
	case 1014: goto tr214;
	case 1015: goto tr214;
	case 1016: goto tr214;
	case 1017: goto tr214;
	case 1018: goto tr214;
	case 1019: goto tr214;
	case 1020: goto tr214;
	case 1021: goto tr214;
	case 1022: goto tr214;
	case 1023: goto tr214;
	case 1024: goto tr214;
	case 1025: goto tr214;
	case 1026: goto tr214;
	case 1027: goto tr214;
	case 1028: goto tr214;
	case 1029: goto tr214;
	case 1030: goto tr214;
	case 1031: goto tr214;
	case 1032: goto tr214;
	case 1033: goto tr214;
	case 1034: goto tr214;
	case 1035: goto tr214;
	case 1036: goto tr214;
	case 1037: goto tr214;
	case 1038: goto tr214;
	case 1039: goto tr214;
	case 1040: goto tr214;
	case 1041: goto tr214;
	case 1042: goto tr214;
	case 1043: goto tr214;
	case 1044: goto tr214;
	case 1045: goto tr214;
	case 1046: goto tr214;
	case 1047: goto tr214;
	case 1048: goto tr214;
	case 1049: goto tr214;
	case 1050: goto tr214;
	case 1051: goto tr214;
	case 1052: goto tr214;
	case 1053: goto tr214;
	case 1054: goto tr214;
	case 1055: goto tr214;
	case 1056: goto tr214;
	case 1057: goto tr214;
	case 1058: goto tr214;
	case 1059: goto tr214;
	case 1060: goto tr214;
	case 1061: goto tr214;
	case 1062: goto tr214;
	case 1063: goto tr214;
	case 1064: goto tr214;
	case 1065: goto tr214;
	case 1066: goto tr214;
	case 1067: goto tr214;
	case 1068: goto tr214;
	case 1069: goto tr214;
	case 1070: goto tr214;
	case 1071: goto tr214;
	case 1072: goto tr214;
	case 1073: goto tr214;
	case 1074: goto tr214;
	case 1075: goto tr214;
	case 1076: goto tr214;
	case 1716: goto tr1795;
	case 1077: goto tr214;
	case 1078: goto tr214;
	case 1717: goto tr1795;
	case 1079: goto tr214;
	case 1080: goto tr207;
	case 1081: goto tr207;
	case 1082: goto tr207;
	case 1718: goto tr2147;
	case 1083: goto tr207;
	case 1084: goto tr207;
	case 1085: goto tr207;
	case 1086: goto tr207;
	case 1087: goto tr207;
	case 1088: goto tr207;
	case 1089: goto tr207;
	case 1090: goto tr207;
	case 1091: goto tr207;
	case 1092: goto tr207;
	case 1719: goto tr2147;
	case 1093: goto tr207;
	case 1094: goto tr207;
	case 1095: goto tr207;
	case 1096: goto tr207;
	case 1097: goto tr207;
	case 1098: goto tr207;
	case 1099: goto tr207;
	case 1100: goto tr207;
	case 1101: goto tr207;
	case 1102: goto tr207;
	case 1103: goto tr214;
	case 1104: goto tr214;
	case 1105: goto tr214;
	case 1106: goto tr214;
	case 1107: goto tr214;
	case 1108: goto tr214;
	case 1109: goto tr214;
	case 1110: goto tr214;
	case 1111: goto tr214;
	case 1112: goto tr214;
	case 1721: goto tr2153;
	case 1113: goto tr1329;
	case 1114: goto tr1329;
	case 1115: goto tr1329;
	case 1116: goto tr1329;
	case 1117: goto tr1329;
	case 1118: goto tr1329;
	case 1119: goto tr1329;
	case 1120: goto tr1329;
	case 1121: goto tr1329;
	case 1122: goto tr1329;
	case 1123: goto tr1329;
	case 1124: goto tr1329;
	case 1722: goto tr2153;
	case 1723: goto tr2153;
	case 1725: goto tr2161;
	case 1125: goto tr1341;
	case 1126: goto tr1341;
	case 1127: goto tr1341;
	case 1128: goto tr1341;
	case 1129: goto tr1341;
	case 1130: goto tr1341;
	case 1131: goto tr1341;
	case 1132: goto tr1341;
	case 1133: goto tr1341;
	case 1134: goto tr1341;
	case 1135: goto tr1341;
	case 1136: goto tr1341;
	case 1137: goto tr1341;
	case 1138: goto tr1341;
	case 1139: goto tr1341;
	case 1140: goto tr1341;
	case 1141: goto tr1341;
	case 1142: goto tr1341;
	case 1726: goto tr2161;
	case 1727: goto tr2161;
	case 1729: goto tr2167;
	case 1143: goto tr1359;
	case 1144: goto tr1359;
	case 1145: goto tr1359;
	case 1146: goto tr1359;
	case 1147: goto tr1359;
	case 1148: goto tr1359;
	case 1149: goto tr1359;
	case 1150: goto tr1359;
	case 1151: goto tr1359;
	case 1152: goto tr1359;
	case 1153: goto tr1359;
	case 1154: goto tr1359;
	case 1155: goto tr1359;
	case 1156: goto tr1359;
	case 1157: goto tr1359;
	case 1158: goto tr1359;
	case 1159: goto tr1359;
	case 1160: goto tr1359;
	case 1161: goto tr1359;
	case 1162: goto tr1359;
	case 1163: goto tr1359;
	case 1164: goto tr1359;
	case 1165: goto tr1359;
	case 1166: goto tr1359;
	case 1167: goto tr1359;
	case 1168: goto tr1359;
	case 1169: goto tr1359;
	case 1170: goto tr1359;
	case 1171: goto tr1359;
	case 1172: goto tr1359;
	case 1173: goto tr1359;
	case 1174: goto tr1359;
	case 1175: goto tr1359;
	case 1176: goto tr1359;
	case 1177: goto tr1359;
	case 1178: goto tr1359;
	case 1179: goto tr1359;
	case 1180: goto tr1359;
	case 1181: goto tr1359;
	case 1182: goto tr1359;
	case 1183: goto tr1359;
	case 1184: goto tr1359;
	case 1185: goto tr1359;
	case 1186: goto tr1359;
	case 1187: goto tr1359;
	case 1188: goto tr1359;
	case 1189: goto tr1359;
	case 1190: goto tr1359;
	case 1191: goto tr1359;
	case 1192: goto tr1359;
	case 1193: goto tr1359;
	case 1194: goto tr1359;
	case 1195: goto tr1359;
	case 1196: goto tr1359;
	case 1197: goto tr1359;
	case 1198: goto tr1359;
	case 1199: goto tr1359;
	case 1200: goto tr1359;
	case 1201: goto tr1359;
	case 1202: goto tr1359;
	case 1203: goto tr1359;
	case 1204: goto tr1359;
	case 1205: goto tr1359;
	case 1206: goto tr1359;
	case 1207: goto tr1359;
	case 1208: goto tr1359;
	case 1209: goto tr1359;
	case 1210: goto tr1359;
	case 1211: goto tr1359;
	case 1212: goto tr1359;
	case 1213: goto tr1359;
	case 1214: goto tr1359;
	case 1215: goto tr1359;
	case 1216: goto tr1359;
	case 1217: goto tr1359;
	case 1218: goto tr1359;
	case 1219: goto tr1359;
	case 1220: goto tr1359;
	case 1221: goto tr1359;
	case 1222: goto tr1359;
	case 1223: goto tr1359;
	case 1224: goto tr1359;
	case 1225: goto tr1359;
	case 1226: goto tr1359;
	case 1227: goto tr1359;
	case 1228: goto tr1359;
	case 1229: goto tr1359;
	case 1230: goto tr1359;
	case 1231: goto tr1359;
	case 1232: goto tr1359;
	case 1233: goto tr1359;
	case 1234: goto tr1359;
	case 1235: goto tr1359;
	case 1236: goto tr1359;
	case 1237: goto tr1359;
	case 1238: goto tr1359;
	case 1239: goto tr1359;
	case 1240: goto tr1359;
	case 1241: goto tr1359;
	case 1242: goto tr1359;
	case 1243: goto tr1359;
	case 1244: goto tr1359;
	case 1245: goto tr1359;
	case 1246: goto tr1359;
	case 1247: goto tr1359;
	case 1248: goto tr1359;
	case 1249: goto tr1359;
	case 1250: goto tr1359;
	case 1251: goto tr1359;
	case 1252: goto tr1359;
	case 1253: goto tr1359;
	case 1254: goto tr1359;
	case 1730: goto tr2167;
	case 1255: goto tr1359;
	case 1256: goto tr1359;
	case 1257: goto tr1359;
	case 1258: goto tr1359;
	case 1259: goto tr1359;
	case 1260: goto tr1359;
	case 1261: goto tr1359;
	case 1262: goto tr1359;
	case 1263: goto tr1359;
	case 1264: goto tr1359;
	case 1265: goto tr1359;
	case 1266: goto tr1359;
	case 1267: goto tr1359;
	case 1268: goto tr1359;
	case 1269: goto tr1359;
	case 1270: goto tr1359;
	case 1271: goto tr1359;
	case 1272: goto tr1359;
	case 1273: goto tr1359;
	case 1274: goto tr1359;
	case 1275: goto tr1359;
	case 1276: goto tr1359;
	case 1277: goto tr1359;
	case 1278: goto tr1359;
	case 1279: goto tr1359;
	case 1280: goto tr1359;
	case 1281: goto tr1359;
	case 1282: goto tr1359;
	case 1283: goto tr1359;
	case 1284: goto tr1359;
	case 1285: goto tr1359;
	case 1286: goto tr1359;
	case 1287: goto tr1359;
	case 1288: goto tr1359;
	case 1289: goto tr1359;
	case 1290: goto tr1359;
	case 1291: goto tr1359;
	case 1292: goto tr1359;
	case 1293: goto tr1359;
	case 1294: goto tr1359;
	case 1295: goto tr1359;
	case 1296: goto tr1359;
	case 1297: goto tr1359;
	case 1298: goto tr1359;
	case 1299: goto tr1359;
	case 1300: goto tr1359;
	case 1301: goto tr1359;
	case 1302: goto tr1359;
	case 1303: goto tr1359;
	case 1304: goto tr1359;
	case 1305: goto tr1359;
	case 1306: goto tr1359;
	case 1307: goto tr1359;
	case 1308: goto tr1359;
	case 1309: goto tr1359;
	case 1310: goto tr1359;
	case 1311: goto tr1359;
	case 1312: goto tr1359;
	case 1313: goto tr1359;
	case 1314: goto tr1359;
	case 1315: goto tr1359;
	case 1316: goto tr1359;
	case 1317: goto tr1359;
	case 1318: goto tr1359;
	case 1319: goto tr1359;
	case 1320: goto tr1359;
	case 1321: goto tr1359;
	case 1322: goto tr1359;
	case 1323: goto tr1359;
	case 1324: goto tr1359;
	case 1325: goto tr1359;
	case 1326: goto tr1359;
	case 1327: goto tr1359;
	case 1328: goto tr1359;
	case 1329: goto tr1359;
	case 1330: goto tr1359;
	case 1331: goto tr1359;
	case 1332: goto tr1359;
	case 1333: goto tr1359;
	case 1334: goto tr1359;
	case 1335: goto tr1359;
	case 1336: goto tr1359;
	case 1337: goto tr1359;
	case 1338: goto tr1359;
	case 1339: goto tr1359;
	case 1340: goto tr1359;
	case 1341: goto tr1359;
	case 1342: goto tr1359;
	case 1343: goto tr1359;
	case 1344: goto tr1359;
	case 1345: goto tr1359;
	case 1346: goto tr1359;
	case 1347: goto tr1359;
	case 1348: goto tr1359;
	case 1349: goto tr1359;
	case 1350: goto tr1359;
	case 1351: goto tr1359;
	case 1352: goto tr1359;
	case 1353: goto tr1359;
	case 1354: goto tr1359;
	case 1355: goto tr1359;
	case 1356: goto tr1359;
	case 1357: goto tr1359;
	case 1358: goto tr1359;
	case 1359: goto tr1359;
	case 1360: goto tr1359;
	case 1361: goto tr1359;
	case 1362: goto tr1359;
	case 1363: goto tr1359;
	case 1364: goto tr1359;
	case 1365: goto tr1359;
	case 1366: goto tr1359;
	}
	}

	_out: {}
	}

#line 1477 "ext/dtext/dtext.cpp.rl"

  g_debug("EOF; closing stray blocks");
  dstack_close_all(sm);
  g_debug("done");

  return sm->output;
}

/* Everything below is optional, it's only needed to build bin/cdtext.exe. */
#ifdef CDTEXT

#include <glib.h>
#include <iostream>

static void parse_file(FILE* input, FILE* output) {
  std::stringstream ss;
  ss << std::cin.rdbuf();
  std::string dtext = ss.str();

  try {
    auto result = StateMachine::parse_dtext(dtext, options);

    if (fwrite(result.c_str(), 1, result.size(), output) != result.size()) {
      perror("fwrite failed");
      exit(1);
    }
  } catch (std::exception& e) {
    fprintf(stderr, "dtext parse error: %s\n", e.what());
    exit(1);
  }
}

int main(int argc, char* argv[]) {
  GError* error = NULL;
  bool opt_verbose = FALSE;
  bool opt_inline = FALSE;
  bool opt_no_mentions = FALSE;

  GOptionEntry options[] = {
    { "no-mentions", 'm', 0, G_OPTION_ARG_NONE, &opt_no_mentions, "Don't parse @mentions", NULL },
    { "inline",      'i', 0, G_OPTION_ARG_NONE, &opt_inline,      "Parse in inline mode", NULL },
    { "verbose",     'v', 0, G_OPTION_ARG_NONE, &opt_verbose,     "Print debug output", NULL },
    { NULL }
  };

  g_autoptr(GOptionContext) context = g_option_context_new("[FILE...]");
  g_option_context_add_main_entries(context, options, NULL);

  if (!g_option_context_parse(context, &argc, &argv, &error)) {
    fprintf(stderr, "option parsing failed: %s\n", error->message);
    g_clear_error(&error);
    return 1;
  }

  if (opt_verbose) {
    g_setenv("G_MESSAGES_DEBUG", "all", TRUE);
  }

  /* skip first argument (progname) */
  argc--, argv++;

  if (argc == 0) {
    parse_file(stdin, stdout, { .f_inline = opt_inline, .f_mentions = !opt_no_mentions });
    return 0;
  }

  for (const char* filename = *argv; argc > 0; argc--, argv++) {
    FILE* input = fopen(filename, "r");
    if (!input) {
      perror("fopen failed");
      return 1;
    }

    parse_file(input, stdout, opt_inline, !opt_no_mentions);
    fclose(input);
  }

  return 0;
}

#endif
