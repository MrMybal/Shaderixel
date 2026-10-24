#include "ui/Search.h"
#include "ui/Lang.h"

#include <imgui_internal.h>

#include <algorithm>
#include <cstring>
#include <fstream>
#include <sstream>

#include "core/Json.h"

// ---------------------------------------------------------------- Dear ImGui test engine hooks
void ImGuiTestEngineHook_ItemAdd(ImGuiContext* ctx, ImGuiID id, const ImRect& bb, const ImGuiLastItemData*) {
    const float r[4] = {bb.Min.x, bb.Min.y, bb.Max.x, bb.Max.y};
    sx::GlobalSearch::Get().OnItemAdd(ctx, id, r);
}
void ImGuiTestEngineHook_ItemInfo(ImGuiContext* ctx, ImGuiID id, const char* label, ImGuiItemStatusFlags flags) {
    sx::GlobalSearch::Get().OnItemInfo(ctx, id, label, (int)flags);
}
void ImGuiTestEngineHook_Log(ImGuiContext*, const char*, ...) {}
const char* ImGuiTestEngine_FindItemDebugLabel(ImGuiContext*, ImGuiID) { return nullptr; }

namespace sx {

void FocusPanel(const char* name);  // Workspace.cpp


namespace {
constexpr char kSep = '\x1f';

std::string Join(const std::vector<std::string>& p, const std::string& last = "", char sep = kSep) {
    std::string s;
    for (auto& x : p) { s += x; s.push_back(sep); }
    return s + last;
}

// Shown part of an ImGui label ("Texte##id", "Texte###id").
std::string Clean(const char* label) {
    std::string s = label ? label : "";
    if (size_t p = s.find("##"); p != std::string::npos) s.resize(p);
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t')) s.pop_back();
    size_t b = 0;
    while (b < s.size() && (s[b] == ' ' || s[b] == '\t')) ++b;
    s.erase(0, b);
    if (s.empty() || s.size() > 80 || s.find('\n') != std::string::npos) return {};
    bool letter = false;
    for (unsigned char c : s) letter |= (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c >= 0xC0;
    return letter ? s : std::string();
}

std::string DisplayName(const ImGuiWindow* w) { return w ? Clean(w->Name) : std::string(); }
bool IsMainMenuBar(const ImGuiWindow* w) { return w && std::strcmp(w->Name, "##MainMenuBar") == 0; }
}  // namespace

GlobalSearch& GlobalSearch::Get() {
    static GlobalSearch s;
    return s;
}

std::string GlobalSearch::Fold(const std::string& s) {
    // Lower case; accented letters as their base letter (UTF-8 Latin-1 range + oe).
    static const char* from[] = {"à", "â", "ä", "á", "ã", "å", "À", "Â", "Ä", "Á", "ç", "Ç", "é", "è", "ê", "ë", "É", "È", "Ê", "Ë", "î", "ï", "í", "ì", "Î", "Ï",
                                 "ô", "ö", "ó", "ò", "õ", "Ô", "Ö", "ù", "û", "ü", "ú", "Ù", "Û", "Ü", "ÿ", "ñ", "Ñ", "œ", "Œ", "æ", "Æ", "’"};
    static const char* to[] = {"a", "a", "a", "a", "a", "a", "a", "a", "a", "a", "c", "c", "e", "e", "e", "e", "e", "e", "e", "e", "i", "i", "i", "i", "i", "i",
                               "o", "o", "o", "o", "o", "o", "o", "u", "u", "u", "u", "u", "u", "u", "y", "n", "n", "oe", "oe", "ae", "ae", "'"};
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size();) {
        const unsigned char c = (unsigned char)s[i];
        if (c < 0x80) {
            out.push_back((char)((c >= 'A' && c <= 'Z') ? c + 32 : c));
            ++i;
            continue;
        }
        bool done = false;
        for (size_t k = 0; k < sizeof(from) / sizeof(from[0]) && !done; ++k) {
            const size_t n = std::strlen(from[k]);
            if (s.compare(i, n, from[k]) == 0) {
                out += to[k];
                i += n;
                done = true;
            }
        }
        if (!done) out.push_back(s[i++]);
    }
    return out;
}

std::string GlobalSearch::Key(int kind, const std::vector<std::string>& path, const std::string& label) {
    return std::to_string(kind) + kSep + Join(path, label);
}

void GlobalSearch::Remember(int kind, const std::vector<std::string>& path, const std::string& label, const std::string& window, unsigned) {
    SearchEntry& e = entries[Key(kind, path, label)];
    if (e.label.empty()) {
        e.kind = kind;
        e.label = label;
        e.path = path;
        if (kind == 0) e.context = "Fenêtre / onglet";
        else if (kind == 1) {
            e.context = "Menu";
            for (size_t i = 0; i < path.size(); ++i) e.context += (i ? " > " : " : ") + path[i];
        } else {
            e.context = path.empty() ? std::string() : path[0];
        }
    }
    e.window = window;
    e.seen = ImGui::GetTime();
}

// ---------------------------------------------------------------- hooks

void GlobalSearch::OnItemAdd(ImGuiContext*, unsigned id, const float bb[4]) {
    lastAddId = id;
    std::memcpy(lastBB, bb, sizeof(lastBB));
}

void GlobalSearch::OnItemInfo(ImGuiContext* ctx, unsigned id, const char* label, int flags) {
    ImGuiWindow* w = ctx ? ctx->CurrentWindow : nullptr;
    if (!w || !label) return;
    if (id == w->ID) {
        // A window begins.
        if (w->Flags & ImGuiWindowFlags_ChildMenu) {
            // Menu popup: its path = the parent's path + the header just drawn there.
            ImGuiWindow* parent = w->ParentWindow;
            std::vector<std::string> p;
            if (parent) {
                if (auto it = menuPathOf.find(parent->ID); it != menuPathOf.end()) p = it->second;
                else if ((parent->Flags & ImGuiWindowFlags_Popup) && !(parent->Flags & ImGuiWindowFlags_ChildMenu)) p = {"\x01"};  // context menu: not indexed
                else if (!IsMainMenuBar(parent)) p = {DisplayName(parent)};
                if (auto h = lastHeader.find(parent->ID); h != lastHeader.end()) p.push_back(h->second.first);
            }
            menuPathOf[w->ID] = p;
            return;
        }
        // Docked windows are child windows of their dock host: kept.
        if ((w->Flags & ImGuiWindowFlags_ChildWindow) && !w->DockIsActive) return;
        if (w->Flags & (ImGuiWindowFlags_Popup | ImGuiWindowFlags_Tooltip)) return;
        const std::string name = DisplayName(w);
        if (name.empty() || name.find("DockSpace") != std::string::npos || name.find("WindowOverViewport") != std::string::npos) return;
        Remember(0, {}, name, w->Name, 0);
        return;
    }
    const std::string lab = Clean(label);
    if (lab.empty()) return;
    const bool menuPopup = (w->Flags & ImGuiWindowFlags_ChildMenu) != 0, mainBar = IsMainMenuBar(w);
    if (menuPopup || mainBar) {
        std::vector<std::string> path;
        if (menuPopup) {
            auto it = menuPathOf.find(w->ID);
            if (it == menuPathOf.end()) return;
            path = it->second;
        }
        if (!path.empty() && path[0] == "\x01") return;
        const bool header = (flags & ImGuiItemStatusFlags_Openable) != 0;
        if (mainBar && !header) return;  // document tabs of the menu bar
        liveMenuIds[Join(path, lab)] = id;
        if (header) {
            lastHeader[w->ID] = {lab, id};
            if (crawl.active && crawl.phase == 0) {
                std::vector<std::string> sub = path;
                sub.push_back(lab);
                if (std::find(crawl.done.begin(), crawl.done.end(), sub) == crawl.done.end() &&
                    std::find(crawl.queue.begin(), crawl.queue.end(), sub) == crawl.queue.end())
                    crawl.queue.push_back(sub);
            }
            if (!mainBar) Remember(1, path, lab, "", id);  // a submenu is a result too (opens it)
        } else {
            Remember(1, path, lab, "", id);
        }
        return;
    }
    if (w->Flags & (ImGuiWindowFlags_Popup | ImGuiWindowFlags_Tooltip)) return;  // context menus, combo lists
    ImGuiWindow* root = w->RootWindow ? w->RootWindow : w;
    if (root->Flags & (ImGuiWindowFlags_Popup | ImGuiWindowFlags_Tooltip)) return;
    const std::string rootName = DisplayName(root);
    if (rootName.empty() || IsMainMenuBar(root) || rootName.find("WindowOverViewport") != std::string::npos || rootName.find("DockSpace") != std::string::npos) return;
    Remember(2, {rootName}, lab, root->Name, id);
    const bool bbValid = lastAddId == id;
    // Highlights of the query in the visible panels.
    if (highlight && bbValid && query.size() >= 2) {
        const std::string f = Fold(lab) + " " + Fold(Tr(lab));  // French label or its translation
        std::istringstream ss(Fold(query));
        bool all = true;
        for (std::string word; ss >> word;) all &= f.find(word) != std::string::npos;
        if (all) matches.push_back({w, {lastBB[0], lastBB[1], lastBB[2], lastBB[3]}});
    }
    // Navigation to a property: found when drawn again.
    if (pend.active && pend.kind == 2 && bbValid && lab == pend.label && root->Name == pend.window) {
        if (!pend.found) {
            pend.found = true;
            ImGui::ScrollToRect(w, ImRect(lastBB[0], lastBB[1], lastBB[2], lastBB[3]), ImGuiScrollFlags_KeepVisibleCenterY);
            pend.flashUntil = ImGui::GetTime() + 1.8;
        }
        pend.win = w;
        std::memcpy(pend.bb, lastBB, sizeof(lastBB));
    }
}

// ---------------------------------------------------------------- query

std::vector<const SearchEntry*> GlobalSearch::Query(const std::string& q, size_t max) const {
    std::vector<std::string> words;
    {
        std::istringstream ss(Fold(q));
        for (std::string w; ss >> w;) words.push_back(w);
    }
    if (words.empty()) return {};
    std::vector<std::pair<float, const SearchEntry*>> hits;
    const double now = ImGui::GetTime();
    for (auto& [k, e] : entries) {
        // Labels are French in the code: the translation is searched too (interface language).
        const std::string lab = Fold(e.label) + " " + Fold(Tr(e.label));
        std::string path = Join(e.path, "", ' ');
        std::string hay = lab + " " + Fold(path);
        for (auto& part : e.path) hay += " " + Fold(Tr(part));
        bool all = true;
        for (auto& w : words) all &= hay.find(w) != std::string::npos;
        if (!all) continue;
        float score = 1;
        bool inLabel = true;
        for (auto& w : words) inLabel &= lab.find(w) != std::string::npos;
        if (inLabel) score += 2;
        if (lab.rfind(words[0], 0) == 0) score += 2;
        if (lab == Fold(q)) score += 3;
        score += e.kind == 0 ? 0.6f : e.kind == 1 ? 0.4f : 0.0f;
        if (e.seen > 0 && now - e.seen < 2.0) score += 0.3f;  // on screen now
        score -= (float)e.label.size() * 0.005f;
        hits.push_back({score, &e});
    }
    std::sort(hits.begin(), hits.end(), [](auto& a, auto& b) { return a.first > b.first || (a.first == b.first && a.second->label < b.second->label); });
    std::vector<const SearchEntry*> out;
    for (size_t i = 0; i < hits.size() && i < max; ++i) out.push_back(hits[i].second);
    return out;
}

// ---------------------------------------------------------------- persistence

void GlobalSearch::Load(const std::string& file) {
    std::ifstream f(Utf8Path(file), std::ios::binary);
    std::stringstream ss;
    ss << f.rdbuf();
    Json j;
    if (!Json::Parse(ss.str(), j)) return;
    for (const Json& e : j["entries"].Elements()) {
        std::vector<std::string> path;
        for (const Json& p : e["path"].Elements()) path.push_back(p.S(""));
        const int kind = e["kind"].I(2);
        SearchEntry& s = entries[Key(kind, path, e["label"].S(""))];
        if (!s.label.empty()) continue;
        s.kind = kind;
        s.label = e["label"].S("");
        s.path = path;
        s.window = e["window"].S("");
        s.context = e["context"].S("");
    }
}

void GlobalSearch::Save(const std::string& file) const {
    Json j = Json::Object();
    Json list = Json::Array();
    for (auto& [k, e] : entries) {
        Json o = Json::Object();
        o["kind"] = e.kind;
        o["label"] = e.label;
        Json p = Json::Array();
        for (auto& x : e.path) p.Push(x);
        o["path"] = p;
        if (!e.window.empty()) o["window"] = e.window;
        o["context"] = e.context;
        list.Push(o);
    }
    j["entries"] = list;
    std::ofstream(Utf8Path(file), std::ios::binary) << j.Dump(0);
}

// ---------------------------------------------------------------- frame

void GlobalSearch::Activate(const SearchEntry& e) {
    pend = Pending{};
    pend.active = true;
    pend.kind = e.kind;
    if (e.kind == 1) {
        pend.chain = e.path;
        pend.chain.push_back(e.label);
    } else {
        pend.window = e.window;
        pend.label = e.label;
        if (!e.window.empty()) {
            if (reveal) reveal(e.window);
            pend.focusIn = 2;  // once its side tab / zone is visible
        }
    }
}

void GlobalSearch::StartCrawl() {
    crawl = Crawl{};
    crawl.active = true;
    for (auto& [k, id] : liveMenuIds)
        if (k.find(kSep) == std::string::npos) crawl.queue.push_back({k});  // top-level menus (drawn every frame)
    std::sort(crawl.queue.begin(), crawl.queue.end());
}

void GlobalSearch::BeginFrame() {
    ImGuiContext& g = *ImGui::GetCurrentContext();
    g.TestEngineHookItems = true;
    matches.clear();
    // A menu result: its headers opened one per frame, then the item activated.
    auto stepChain = [&](const std::vector<std::string>& chain, size_t& step, int& wait, int& tries) -> int {
        if (wait > 0) { --wait; return 0; }
        std::vector<std::string> prefix(chain.begin(), chain.begin() + (long long)step);
        auto it = liveMenuIds.find(Join(prefix, chain[step]));
        if (it == liveMenuIds.end()) return ++tries > 45 ? -1 : 0;
        ImGui::ActivateItemByID(it->second);
        ++step;
        wait = 2;
        tries = 0;
        return step >= chain.size() ? 1 : 0;
    };
    if (pend.active && pend.kind == 1) {
        const int r = stepChain(pend.chain, pend.step, pend.wait, pend.tries);
        if (r != 0) pend.active = false;
    } else if (pend.active && pend.focusIn >= 0) {
        if (pend.focusIn-- == 0) {
            FocusPanel(pend.window.c_str());
            if (pend.kind == 0) pend.active = false;
        }
    } else if (pend.active && pend.kind == 2) {
        if (!pend.found && ++pend.tries > 120) pend.active = false;
        if (pend.found && ImGui::GetTime() > pend.flashUntil) pend.active = false;
    }
    if (!crawl.active) return;
    if (crawl.phase == 0) {
        // Each menu opened in turn (invisible), its submenus queued as they appear.
        if (crawl.index >= crawl.queue.size()) {
            ImGui::ClosePopupToLevel(0, true);
            // Tabs next: every docked window not visible is brought forward once.
            crawl.phase = 1;
            crawl.index = 0;
            crawl.step = 0;
            for (ImGuiWindow* w : g.Windows) {
                if (w->Flags & (ImGuiWindowFlags_ChildWindow | ImGuiWindowFlags_Popup | ImGuiWindowFlags_Tooltip) || !w->DockNode) continue;
                if (DisplayName(w).empty()) continue;
                if (w->DockIsActive && !w->Hidden) crawl.restore.push_back(w->Name);
                else crawl.tabs.push_back(w->Name);
            }
            if (g.NavWindow) crawl.restore.push_back(g.NavWindow->RootWindow ? g.NavWindow->RootWindow->Name : g.NavWindow->Name);
            return;
        }
        const auto& chain = crawl.queue[crawl.index];
        int tries = 0;
        if (crawl.wait > 0) { --crawl.wait; return; }
        if (crawl.step < chain.size()) {
            const int r = stepChain(chain, crawl.step, crawl.wait, tries);
            if (r < 0) crawl.step = chain.size();  // not reachable: skip
            return;
        }
        crawl.done.push_back(chain);
        ImGui::ClosePopupToLevel(0, true);
        ++crawl.index;
        crawl.step = 0;
        crawl.wait = 1;
        return;
    }
    if (crawl.wait > 0) { --crawl.wait; return; }
    if (crawl.index < crawl.tabs.size()) {
        // Shown (side tab), then focused when visible, then indexed while drawn.
        const std::string& name = crawl.tabs[crawl.index];
        if (crawl.step == 0) {
            if (reveal) reveal(name);
            crawl.step = 1;
            crawl.wait = 2;
        } else {
            FocusPanel(name.c_str());
            crawl.step = 0;
            ++crawl.index;
            crawl.wait = 3;
        }
        return;
    }
    for (auto& n : crawl.restore) {
        if (reveal) reveal(n);
        FocusPanel(n.c_str());
    }
    crawl.active = false;
}

void GlobalSearch::EndFrame() {
    ImGuiContext& g = *ImGui::GetCurrentContext();
    const ImU32 col = IM_COL32(255, 170, 40, 220);
    for (auto& m : matches) {
        if (!m.window || !m.window->WasActive || !m.window->Viewport) continue;
        ImDrawList* dl = ImGui::GetForegroundDrawList(m.window->Viewport);
        dl->PushClipRect(m.window->ClipRect.Min, m.window->ClipRect.Max, true);
        dl->AddRect(ImVec2(m.bb[0] - 2, m.bb[1] - 2), ImVec2(m.bb[2] + 2, m.bb[3] + 2), col, 3, 0, 1.5f);
        dl->PopClipRect();
    }
    // The window may have been closed since (a floating window loses its viewport): still listed, active and with a viewport.
    bool winOk = false;
    if (pend.found && pend.win)
        for (ImGuiWindow* w : g.Windows) winOk |= w == pend.win;
    if (winOk && (!pend.win->WasActive || !pend.win->Viewport)) winOk = false;
    if (winOk && ImGui::GetTime() < pend.flashUntil) {
        // The reached property blinks.
        const float a = 0.5f + 0.5f * std::sin((float)ImGui::GetTime() * 12.0f);
        ImDrawList* dl = ImGui::GetForegroundDrawList(pend.win->Viewport);
        dl->AddRect(ImVec2(pend.bb[0] - 4, pend.bb[1] - 4), ImVec2(pend.bb[2] + 4, pend.bb[3] + 4), IM_COL32(255, 200, 60, (int)(255 * a)), 4, 0, 3.0f);
    }
    (void)g;
}

}  // namespace sx
