#ifndef UNICODE
#define UNICODE
#endif

#ifndef _UNICODE
#define _UNICODE
#endif

// windows.h définit sinon des macros min/max qui cassent std::min/std::max.
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <windowsx.h>
#include <string>
#include <vector>
#include <deque>
#include <map>
#include <thread>
#include <mutex>
#include <algorithm>
#include <cwctype>

static const UINT WM_PIPE_LINE = WM_APP + 10;
static const UINT TIMER_GAME_MODE = 101;
static const int MAX_INPUT = 500;

struct ChatMessage {
    std::wstring id;
    std::wstring channel;
    std::wstring author;
    std::wstring content;
    std::wstring language;
    std::wstring server;
    bool mine = false;
};

struct RectI { int l=0,t=0,r=0,b=0; bool contains(int x,int y) const { return x>=l && x<r && y>=t && y<b; } };

static HWND g_hwnd = nullptr;
static HANDLE g_stdout = INVALID_HANDLE_VALUE;
static std::mutex g_writeMutex;
static std::map<std::wstring, std::deque<ChatMessage>> g_messages;
static std::wstring g_channel = L"general";
static std::wstring g_input;
static std::wstring g_server = L"SSO";
static std::wstring g_status = L"Connexion au chat live…";
static bool g_connected = false;
static bool g_visible = false;
static bool g_gameMode = true;
static bool g_groupActive = false;
static std::wstring g_groupName = L"Groupe";
static int g_nearbyCount = 0;
static bool g_dragging = false;
static POINT g_dragStart{};
static RECT g_windowStart{};

static std::wstring g_title = L"Chat Star Stable";
static std::wstring g_tabGeneral = L"Général";
static std::wstring g_tabServer = L"Serveur";
static std::wstring g_tabProximity = L"Proximité";
static std::wstring g_tabGroup = L"Groupe";
static std::wstring g_placeholder = L"Écrire un message…";
static std::wstring g_manage = L"Gérer groupe";

static HFONT g_fontTitle = nullptr;
static HFONT g_fontTab = nullptr;
static HFONT g_fontName = nullptr;
static HFONT g_fontText = nullptr;
static HFONT g_fontSmall = nullptr;

static RectI g_tabRects[4];
static RectI g_inputRect;
static RectI g_sendRect;
static RectI g_closeRect;
static RectI g_manageRect;
static RectI g_titleDragRect;

static COLORREF C_BG = RGB(24, 19, 30);
static COLORREF C_PANEL = RGB(31, 25, 38);
static COLORREF C_PANEL2 = RGB(40, 32, 48);
static COLORREF C_BORDER = RGB(72, 58, 84);
static COLORREF C_TEXT = RGB(244, 236, 246);
static COLORREF C_MUTED = RGB(170, 154, 178);
static COLORREF C_PINK = RGB(230, 176, 214);
static COLORREF C_GREEN = RGB(103, 201, 141);
static COLORREF C_RED = RGB(225, 108, 125);

static std::string HexEncodeUtf8(const std::wstring& w) {
    if (w.empty()) return "";
    int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    if (n <= 0) return "";
    std::string utf8((size_t)n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), utf8.data(), n, nullptr, nullptr);
    static const char* hex = "0123456789abcdef";
    std::string out;
    out.reserve(utf8.size() * 2);
    for (unsigned char c : utf8) {
        out.push_back(hex[(c >> 4) & 0xF]);
        out.push_back(hex[c & 0xF]);
    }
    return out;
}

static int HexVal(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + c - 'a';
    if (c >= 'A' && c <= 'F') return 10 + c - 'A';
    return -1;
}

static std::wstring HexDecodeUtf8(const std::string& s) {
    std::string bytes;
    bytes.reserve(s.size() / 2);
    for (size_t i = 0; i + 1 < s.size(); i += 2) {
        int a = HexVal(s[i]);
        int b = HexVal(s[i + 1]);
        if (a < 0 || b < 0) continue;
        bytes.push_back((char)((a << 4) | b));
    }
    if (bytes.empty()) return L"";
    int n = MultiByteToWideChar(CP_UTF8, 0, bytes.data(), (int)bytes.size(), nullptr, 0);
    if (n <= 0) return L"";
    std::wstring out((size_t)n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, bytes.data(), (int)bytes.size(), out.data(), n);
    return out;
}

static std::vector<std::string> SplitTabs(const std::string& line) {
    std::vector<std::string> out;
    size_t start = 0;
    for (;;) {
        size_t p = line.find('\t', start);
        if (p == std::string::npos) {
            out.push_back(line.substr(start));
            break;
        }
        out.push_back(line.substr(start, p - start));
        start = p + 1;
    }
    return out;
}

static void SendEvent(const std::string& name, const std::vector<std::wstring>& fields = {}) {
    if (g_stdout == INVALID_HANDLE_VALUE || g_stdout == nullptr) return;
    std::string line = name;
    for (const auto& f : fields) {
        line.push_back('\t');
        line += HexEncodeUtf8(f);
    }
    line.push_back('\n');
    std::lock_guard<std::mutex> lock(g_writeMutex);
    DWORD written = 0;
    WriteFile(g_stdout, line.data(), (DWORD)line.size(), &written, nullptr);
    FlushFileBuffers(g_stdout);
}

static std::wstring Lower(std::wstring s) {
    std::transform(s.begin(), s.end(), s.begin(), [](wchar_t c){ return (wchar_t)towlower(c); });
    return s;
}

static std::wstring BaseName(std::wstring path) {
    size_t p = path.find_last_of(L"\\/");
    return (p == std::wstring::npos) ? path : path.substr(p + 1);
}

static std::wstring ForegroundExeName() {
    HWND fg = GetForegroundWindow();
    if (!fg) return L"";
    DWORD pid = 0;
    GetWindowThreadProcessId(fg, &pid);
    if (!pid) return L"";
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!h) return L"";
    wchar_t buf[32768];
    DWORD len = (DWORD)(sizeof(buf) / sizeof(buf[0]));
    std::wstring out;
    if (QueryFullProcessImageNameW(h, 0, buf, &len)) out.assign(buf, len);
    CloseHandle(h);
    return Lower(BaseName(out));
}

static bool IsStarStableForeground() {
    std::wstring n = ForegroundExeName();
    std::wstring flat;
    for (wchar_t c : n) if (c != L' ' && c != L'-' && c != L'_') flat.push_back(c);
    return flat.find(L"starstableonline") != std::wstring::npos || flat == L"starstable.exe";
}

static void ApplyGameMode(bool gameMode) {
    if (!g_hwnd) return;
    g_gameMode = gameMode;
    LONG_PTR ex = GetWindowLongPtrW(g_hwnd, GWL_EXSTYLE);
    ex |= WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW;
    if (gameMode) {
        ex |= WS_EX_TRANSPARENT | WS_EX_NOACTIVATE;
    } else {
        ex &= ~((LONG_PTR)WS_EX_TRANSPARENT);
        ex &= ~((LONG_PTR)WS_EX_NOACTIVATE);
    }
    SetWindowLongPtrW(g_hwnd, GWL_EXSTYLE, ex);
    SetLayeredWindowAttributes(g_hwnd, 0, 245, LWA_ALPHA);
    SetWindowPos(g_hwnd, HWND_TOPMOST, 0,0,0,0,
        SWP_NOMOVE | SWP_NOSIZE | SWP_FRAMECHANGED | (gameMode ? SWP_NOACTIVATE : 0));
    if (!gameMode) {
        ShowWindow(g_hwnd, SW_SHOWNORMAL);
        BringWindowToTop(g_hwnd);
        SetForegroundWindow(g_hwnd);
        SetActiveWindow(g_hwnd);
        SetFocus(g_hwnd);
    }
    InvalidateRect(g_hwnd, nullptr, FALSE);
}

static void ShowInteractive() {
    g_visible = true;
    ShowWindow(g_hwnd, SW_SHOWNORMAL);
    ApplyGameMode(false);
}

static void HideOverlay() {
    g_visible = false;
    ShowWindow(g_hwnd, SW_HIDE);
    SendEvent("HIDDEN");
}

static HBRUSH Brush(COLORREF c) { return CreateSolidBrush(c); }

static void FillRound(HDC dc, const RectI& r, int radius, COLORREF c) {
    HBRUSH b = Brush(c);
    HPEN p = CreatePen(PS_SOLID, 1, c);
    HGDIOBJ oldB = SelectObject(dc, b);
    HGDIOBJ oldP = SelectObject(dc, p);
    RoundRect(dc, r.l, r.t, r.r, r.b, radius, radius);
    SelectObject(dc, oldP); SelectObject(dc, oldB);
    DeleteObject(p); DeleteObject(b);
}

static void StrokeRound(HDC dc, const RectI& r, int radius, COLORREF c) {
    HBRUSH b = (HBRUSH)GetStockObject(NULL_BRUSH);
    HPEN p = CreatePen(PS_SOLID, 1, c);
    HGDIOBJ oldB = SelectObject(dc, b);
    HGDIOBJ oldP = SelectObject(dc, p);
    RoundRect(dc, r.l, r.t, r.r, r.b, radius, radius);
    SelectObject(dc, oldP); SelectObject(dc, oldB);
    DeleteObject(p);
}

static void Text(HDC dc, const std::wstring& s, RECT r, HFONT font, COLORREF c, UINT flags) {
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, c);
    HGDIOBJ old = SelectObject(dc, font);
    DrawTextW(dc, s.c_str(), (int)s.size(), &r, flags);
    SelectObject(dc, old);
}

static std::wstring ChannelLabel(const std::wstring& c) {
    if (c == L"server") return g_tabServer;
    if (c == L"proximity") return g_tabProximity;
    if (c == L"group") return g_tabGroup;
    return g_tabGeneral;
}

static void UpdateRects(int w, int h) {
    g_titleDragRect = {18, 10, w - 185, 48};
    g_closeRect = {w - 45, 13, w - 15, 43};
    g_manageRect = {w - 162, 13, w - 53, 43};
    int x = 18;
    int tabW = (w - 36 - 18) / 4;
    for (int i=0;i<4;i++) {
        g_tabRects[i] = {x, 58, x + tabW, 91};
        x += tabW + 6;
    }
    g_sendRect = {w - 98, h - 61, w - 18, h - 17};
    g_inputRect = {18, h - 61, w - 107, h - 17};
}

static int CalcMessageHeight(HDC dc, const ChatMessage& m, int width) {
    RECT rc{0,0,width,0};
    HGDIOBJ old = SelectObject(dc, g_fontText);
    DrawTextW(dc, m.content.c_str(), (int)m.content.size(), &rc, DT_WORDBREAK | DT_CALCRECT | DT_NOPREFIX);
    SelectObject(dc, old);
    int contentH = std::max(18, (int)(rc.bottom - rc.top));
    return 27 + contentH + 13;
}

static void PaintMessages(HDC dc, int w, int h) {
    RectI area{18, 101, w - 18, h - 74};
    FillRound(dc, area, 16, C_PANEL);
    StrokeRound(dc, area, 16, C_BORDER);

    auto it = g_messages.find(g_channel);
    if (it == g_messages.end() || it->second.empty()) {
        std::wstring empty = L"Aucun message dans " + ChannelLabel(g_channel) + L" pour le moment.";
        RECT rc{area.l + 30, area.t + 80, area.r - 30, area.b - 20};
        Text(dc, empty, rc, g_fontText, C_MUTED, DT_CENTER | DT_WORDBREAK | DT_NOPREFIX);
        return;
    }

    const auto& q = it->second;
    int maxW = area.r - area.l - 34;
    int avail = area.b - area.t - 22;
    int used = 0;
    size_t start = q.size();
    while (start > 0) {
        int mh = CalcMessageHeight(dc, q[start - 1], maxW - 28);
        if (used + mh > avail && start < q.size()) break;
        used += mh;
        --start;
        if (used >= avail) break;
    }

    int y = area.t + 12;
    for (size_t i = start; i < q.size(); ++i) {
        const auto& m = q[i];
        int mh = CalcMessageHeight(dc, m, maxW - 28);
        RectI bubble{area.l + 10, y, area.r - 10, std::min(area.b - 8, y + mh - 4)};
        FillRound(dc, bubble, 12, m.mine ? RGB(75, 59, 88) : C_PANEL2);

        RECT nameRc{bubble.l + 12, bubble.t + 7, bubble.r - 12, bubble.t + 27};
        Text(dc, m.author.empty() ? L"Joueur" : m.author, nameRc, g_fontName, C_PINK, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);

        std::wstring meta;
        if (!m.language.empty()) meta += L"  " + m.language;
        if (!m.server.empty()) meta += L"  ·  " + m.server;
        if (!meta.empty()) {
            RECT metaRc{bubble.l + 150, bubble.t + 8, bubble.r - 12, bubble.t + 26};
            Text(dc, meta, metaRc, g_fontSmall, C_MUTED, DT_RIGHT | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
        }

        RECT msgRc{bubble.l + 12, bubble.t + 29, bubble.r - 12, bubble.b - 8};
        Text(dc, m.content, msgRc, g_fontText, C_TEXT, DT_LEFT | DT_WORDBREAK | DT_NOPREFIX);
        y += mh;
        if (y >= area.b) break;
    }
}

static void PaintWindow(HDC dc, int w, int h) {
    HBRUSH bg = Brush(C_BG);
    RECT all{0,0,w,h};
    FillRect(dc, &all, bg);
    DeleteObject(bg);

    UpdateRects(w, h);

    RECT titleRc{18, 12, w - 180, 39};
    Text(dc, g_title, titleRc, g_fontTitle, C_TEXT, DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);

    std::wstring st = L"● " + g_status;
    if (!g_server.empty()) st += L"  ·  " + g_server;
    RECT statusRc{18, 38, w - 190, 56};
    Text(dc, st, statusRc, g_fontSmall, g_connected ? C_GREEN : C_MUTED, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);

    FillRound(dc, g_manageRect, 10, C_PANEL2);
    RECT manageRc{g_manageRect.l, g_manageRect.t, g_manageRect.r, g_manageRect.b};
    Text(dc, g_manage, manageRc, g_fontSmall, C_MUTED, DT_CENTER | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);

    FillRound(dc, g_closeRect, 10, RGB(62, 40, 50));
    RECT closeRc{g_closeRect.l, g_closeRect.t, g_closeRect.r, g_closeRect.b};
    Text(dc, L"×", closeRc, g_fontTitle, C_RED, DT_CENTER | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);

    std::wstring tabs[4] = {g_tabGeneral, g_tabServer, g_tabProximity, g_tabGroup};
    std::wstring keys[4] = {L"general",L"server",L"proximity",L"group"};
    for (int i=0;i<4;i++) {
        bool active = (g_channel == keys[i]);
        FillRound(dc, g_tabRects[i], 10, active ? RGB(77, 58, 86) : C_PANEL2);
        if (active) StrokeRound(dc, g_tabRects[i], 10, C_PINK);
        RECT r{g_tabRects[i].l, g_tabRects[i].t, g_tabRects[i].r, g_tabRects[i].b};
        std::wstring label = tabs[i];
        if (i == 2 && g_nearbyCount > 0) label += L" (" + std::to_wstring(g_nearbyCount) + L")";
        if (i == 3 && g_groupActive) label += L" ✓";
        Text(dc, label, r, g_fontTab, active ? C_TEXT : C_MUTED, DT_CENTER | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);
    }

    PaintMessages(dc, w, h);

    FillRound(dc, g_inputRect, 13, C_PANEL2);
    StrokeRound(dc, g_inputRect, 13, g_gameMode ? C_BORDER : C_PINK);
    std::wstring shown = g_input.empty() ? g_placeholder : g_input;
    RECT inputRc{g_inputRect.l + 13, g_inputRect.t + 5, g_inputRect.r - 12, g_inputRect.b - 5};
    Text(dc, shown, inputRc, g_fontText, g_input.empty() ? C_MUTED : C_TEXT,
         DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);

    FillRound(dc, g_sendRect, 13, C_PINK);
    RECT sendRc{g_sendRect.l, g_sendRect.t, g_sendRect.r, g_sendRect.b};
    Text(dc, L"Envoyer", sendRc, g_fontTab, RGB(45,34,50), DT_CENTER | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);

    if (g_gameMode) {
        RectI badge{w - 90, h - 92, w - 18, h - 70};
        FillRound(dc, badge, 9, RGB(45, 65, 56));
        RECT br{badge.l,badge.t,badge.r,badge.b};
        Text(dc, L"MODE JEU", br, g_fontSmall, C_GREEN, DT_CENTER | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
    }
}

static void SubmitInput() {
    if (g_input.empty()) return;
    SendEvent("SEND", {g_channel, g_input});
    g_input.clear();
    InvalidateRect(g_hwnd, nullptr, FALSE);
}

static void PasteClipboard() {
    if (!OpenClipboard(g_hwnd)) return;
    HANDLE h = GetClipboardData(CF_UNICODETEXT);
    if (h) {
        const wchar_t* p = (const wchar_t*)GlobalLock(h);
        if (p) {
            std::wstring s(p);
            GlobalUnlock(h);
            s.erase(std::remove(s.begin(), s.end(), L'\r'), s.end());
            std::replace(s.begin(), s.end(), L'\n', L' ');
            if ((int)(g_input.size() + s.size()) > MAX_INPUT) s.resize(std::max(0, MAX_INPUT - (int)g_input.size()));
            g_input += s;
            InvalidateRect(g_hwnd, nullptr, FALSE);
        }
    }
    CloseClipboard();
}

static void HandleCommand(const std::string& line) {
    auto parts = SplitTabs(line);
    if (parts.empty()) return;
    const std::string& cmd = parts[0];
    auto field = [&](size_t i)->std::wstring { return i < parts.size() ? HexDecodeUtf8(parts[i]) : L""; };

    if (cmd == "SHOW") {
        std::wstring c = field(1);
        if (!c.empty()) g_channel = c;
        ShowInteractive();
    } else if (cmd == "HIDE") {
        HideOverlay();
    } else if (cmd == "MESSAGE") {
        ChatMessage m;
        m.id = field(1); m.channel = field(2); m.author = field(3); m.content = field(4);
        m.language = field(5); m.server = field(6); m.mine = (field(7) == L"1");
        if (m.channel.empty()) m.channel = L"general";
        auto& q = g_messages[m.channel];
        q.push_back(std::move(m));
        while (q.size() > 100) q.pop_front();
        InvalidateRect(g_hwnd, nullptr, FALSE);
    } else if (cmd == "DELETE") {
        std::wstring id = field(1);
        for (auto& kv : g_messages) {
            auto& q = kv.second;
            q.erase(std::remove_if(q.begin(), q.end(), [&](const ChatMessage& m){ return m.id == id; }), q.end());
        }
        InvalidateRect(g_hwnd, nullptr, FALSE);
    } else if (cmd == "CLEAR") {
        g_messages.clear(); InvalidateRect(g_hwnd, nullptr, FALSE);
    } else if (cmd == "STATUS") {
        g_status = field(1); g_connected = (field(2) == L"1"); InvalidateRect(g_hwnd, nullptr, FALSE);
    } else if (cmd == "STATE") {
        g_server = field(1); InvalidateRect(g_hwnd, nullptr, FALSE);
    } else if (cmd == "GROUP") {
        g_groupActive = (field(1) == L"1"); g_groupName = field(2); InvalidateRect(g_hwnd, nullptr, FALSE);
    } else if (cmd == "NEARBY") {
        try { g_nearbyCount = std::stoi(field(1)); } catch (...) { g_nearbyCount = 0; }
        InvalidateRect(g_hwnd, nullptr, FALSE);
    } else if (cmd == "LABELS") {
        g_title = field(1); g_tabGeneral = field(2); g_tabServer = field(3); g_tabProximity = field(4);
        g_tabGroup = field(5); g_placeholder = field(6); g_manage = field(7);
        InvalidateRect(g_hwnd, nullptr, FALSE);
    } else if (cmd == "QUIT") {
        PostMessageW(g_hwnd, WM_CLOSE, 0, 0);
    }
}

static DWORD WINAPI PipeReaderThread(LPVOID) {
    HANDLE in = GetStdHandle(STD_INPUT_HANDLE);
    if (in == INVALID_HANDLE_VALUE || in == nullptr) return 0;
    char buf[4096];
    std::string pending;
    for (;;) {
        DWORD got = 0;
        if (!ReadFile(in, buf, sizeof(buf), &got, nullptr) || got == 0) break;
        pending.append(buf, buf + got);
        for (;;) {
            size_t p = pending.find('\n');
            if (p == std::string::npos) break;
            std::string line = pending.substr(0, p);
            pending.erase(0, p + 1);
            if (!line.empty() && line.back() == '\r') line.pop_back();
            auto* heapLine = new std::string(std::move(line));
            if (!PostMessageW(g_hwnd, WM_PIPE_LINE, 0, (LPARAM)heapLine)) delete heapLine;
        }
    }
    return 0;
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE:
        SetTimer(hwnd, TIMER_GAME_MODE, 50, nullptr);
        return 0;
    case WM_TIMER:
        if (wp == TIMER_GAME_MODE && g_visible && !g_gameMode && IsStarStableForeground()) {
            ApplyGameMode(true);
            SendEvent("GAMEMODE");
        }
        return 0;
    case WM_PIPE_LINE: {
        auto* line = reinterpret_cast<std::string*>(lp);
        if (line) { HandleCommand(*line); delete line; }
        return 0;
    }
    case WM_MOUSEACTIVATE:
        if (g_gameMode) return MA_NOACTIVATE;
        break;
    case WM_NCHITTEST:
        if (g_gameMode) return HTTRANSPARENT;
        return HTCLIENT;
    case WM_LBUTTONDOWN: {
        if (g_gameMode) return 0;
        int x = GET_X_LPARAM(lp), y = GET_Y_LPARAM(lp);
        SetFocus(hwnd);
        if (g_closeRect.contains(x,y)) { HideOverlay(); return 0; }
        if (g_manageRect.contains(x,y)) { SendEvent("LEGACY"); HideOverlay(); return 0; }
        std::wstring keys[4] = {L"general",L"server",L"proximity",L"group"};
        for (int i=0;i<4;i++) if (g_tabRects[i].contains(x,y)) {
            g_channel = keys[i]; SendEvent("CHANNEL", {g_channel}); InvalidateRect(hwnd,nullptr,FALSE); return 0;
        }
        if (g_sendRect.contains(x,y)) { SubmitInput(); return 0; }
        if (g_titleDragRect.contains(x,y)) {
            g_dragging = true; SetCapture(hwnd); GetCursorPos(&g_dragStart); GetWindowRect(hwnd,&g_windowStart); return 0;
        }
        return 0;
    }
    case WM_MOUSEMOVE:
        if (g_dragging && !g_gameMode) {
            POINT p; GetCursorPos(&p);
            int nx = g_windowStart.left + (p.x - g_dragStart.x);
            int ny = g_windowStart.top + (p.y - g_dragStart.y);
            SetWindowPos(hwnd, HWND_TOPMOST, nx, ny, 0,0, SWP_NOSIZE | SWP_NOACTIVATE);
        }
        return 0;
    case WM_LBUTTONUP:
        if (g_dragging) { g_dragging = false; ReleaseCapture(); }
        return 0;
    case WM_KEYDOWN:
        if (g_gameMode) return 0;
        if ((GetKeyState(VK_CONTROL) & 0x8000) && wp == 'V') { PasteClipboard(); return 0; }
        if (wp == VK_ESCAPE) { ApplyGameMode(true); return 0; }
        if (wp == VK_RETURN) { SubmitInput(); return 0; }
        if (wp == VK_BACK && !g_input.empty()) { g_input.pop_back(); InvalidateRect(hwnd,nullptr,FALSE); return 0; }
        return 0;
    case WM_CHAR:
        if (!g_gameMode && wp >= 32 && wp != 127 && (int)g_input.size() < MAX_INPUT) {
            g_input.push_back((wchar_t)wp);
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(hwnd, &ps);
        RECT rc; GetClientRect(hwnd, &rc);
        int w = rc.right - rc.left, h = rc.bottom - rc.top;
        HDC mem = CreateCompatibleDC(dc);
        HBITMAP bmp = CreateCompatibleBitmap(dc, w, h);
        HGDIOBJ oldBmp = SelectObject(mem, bmp);
        PaintWindow(mem, w, h);
        BitBlt(dc, 0,0,w,h, mem,0,0, SRCCOPY);
        SelectObject(mem, oldBmp); DeleteObject(bmp); DeleteDC(mem);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;
    case WM_CLOSE:
        g_visible = false;
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        KillTimer(hwnd, TIMER_GAME_MODE);
        SendEvent("EXIT");
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static HFONT MakeFont(int px, int weight) {
    HDC dc = GetDC(nullptr);
    int dpi = GetDeviceCaps(dc, LOGPIXELSY);
    ReleaseDC(nullptr, dc);
    int logical = -MulDiv(px, dpi, 96);
    return CreateFontW(logical, 0,0,0, weight, FALSE,FALSE,FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
}

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE, PWSTR, int) {
    SetProcessDPIAware();
    g_stdout = GetStdHandle(STD_OUTPUT_HANDLE);

    const wchar_t* cls = L"SSONativeChatOverlayV1";
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;
    wc.lpszClassName = cls;
    if (!RegisterClassExW(&wc)) return 2;

    g_fontTitle = MakeFont(18, FW_BOLD);
    g_fontTab = MakeFont(12, FW_SEMIBOLD);
    g_fontName = MakeFont(13, FW_BOLD);
    g_fontText = MakeFont(12, FW_NORMAL);
    g_fontSmall = MakeFont(10, FW_NORMAL);

    int w = 760, h = 560;
    int sw = GetSystemMetrics(SM_CXSCREEN), sh = GetSystemMetrics(SM_CYSCREEN);
    int x = std::max(20, (sw - w) / 2);
    int y = std::max(20, (sh - h) / 2);

    DWORD ex = WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_TRANSPARENT;
    g_hwnd = CreateWindowExW(ex, cls, L"Chat Star Stable", WS_POPUP,
        x, y, w, h, nullptr, nullptr, inst, nullptr);
    if (!g_hwnd) return 3;

    HRGN rgn = CreateRoundRectRgn(0,0,w+1,h+1,28,28);
    SetWindowRgn(g_hwnd, rgn, TRUE);
    SetLayeredWindowAttributes(g_hwnd, 0, 245, LWA_ALPHA);

    HANDLE th = CreateThread(nullptr, 0, PipeReaderThread, nullptr, 0, nullptr);
    if (th) CloseHandle(th);

    SendEvent("READY");

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0,0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    if (g_fontTitle) DeleteObject(g_fontTitle);
    if (g_fontTab) DeleteObject(g_fontTab);
    if (g_fontName) DeleteObject(g_fontName);
    if (g_fontText) DeleteObject(g_fontText);
    if (g_fontSmall) DeleteObject(g_fontSmall);
    return 0;
}
