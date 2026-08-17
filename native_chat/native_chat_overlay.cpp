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
#include <gdiplus.h>
#include <string>
#include <memory>
#include <cstring>
#include <utility>
#include <vector>
#include <deque>
#include <map>
#include <thread>
#include <mutex>
#include <algorithm>
#include <cwctype>

#pragma comment(lib, "gdiplus.lib")

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

struct NearbyPlayerNative {
    std::wstring userId;
    std::wstring username;
    double distance = 0.0;
    bool speaking = false;
    bool muted = false;
    double volume = 1.0;
    std::wstring language;
    std::wstring server;
};

struct GroupMemberNative {
    std::wstring username;
    bool owner = false;
    std::wstring language;
    std::wstring server;
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
static std::wstring g_groupInvite;
static int g_groupMax = 25;
static std::vector<GroupMemberNative> g_groupMembers;
static int g_nearbyCount = 0;
static std::vector<NearbyPlayerNative> g_nearbyPlayers;

static bool g_voiceEnabled = true;
static bool g_muted = false;
static bool g_deafened = false;
static std::wstring g_proxVoiceStatus = L"Vocal proximité arrêté";
static int g_proxVoiceTone = 0; // 0 muted, 1 green, 2 orange, 3 red
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
static std::wstring g_voiceLabel = L"Vocal proximité";
static std::wstring g_micLabel = L"Micro";
static std::wstring g_soundLabel = L"Son";
static std::wstring g_nearbyLabel = L"Joueurs proches";
static std::wstring g_noNearbyLabel = L"Aucun joueur proche";
static std::wstring g_membersLabel = L"Membres";
static std::wstring g_settingsLabel = L"Réglages";
static std::wstring g_copyLabel = L"Copier";
static std::wstring g_leaveLabel = L"Quitter";

static HFONT g_fontTitle = nullptr;
static HFONT g_fontTab = nullptr;
static HFONT g_fontName = nullptr;
static HFONT g_fontText = nullptr;
static HFONT g_fontSmall = nullptr;
static HFONT g_fontTiny = nullptr;

static ULONG_PTR g_gdiplusToken = 0;
static std::map<std::wstring, std::unique_ptr<Gdiplus::Image>> g_icons;

static RectI g_tabRects[4];
static RectI g_inputRect;
static RectI g_sendRect;
static RectI g_closeRect;
static RectI g_manageRect;
static RectI g_titleDragRect;
static RectI g_minRect;
static RectI g_maxRect;
static RectI g_settingsRect;
static RectI g_voiceToggleRect;
static RectI g_micRect;
static RectI g_deafRect;
static std::vector<RectI> g_nearbyMuteRects;
static RectI g_groupCopyRect;
static RectI g_groupLeaveRect;

static COLORREF C_BG = RGB(24, 19, 30);
static COLORREF C_PANEL = RGB(31, 25, 38);
static COLORREF C_PANEL2 = RGB(40, 32, 48);
static COLORREF C_BORDER = RGB(72, 58, 84);
static COLORREF C_TEXT = RGB(244, 236, 246);
static COLORREF C_MUTED = RGB(170, 154, 178);
static COLORREF C_PINK = RGB(230, 176, 214);
static COLORREF C_GREEN = RGB(103, 201, 141);
static COLORREF C_RED = RGB(225, 108, 125);
static COLORREF C_ORANGE = RGB(229, 183, 103);
static COLORREF C_GREEN_PANEL = RGB(62, 79, 75);
static COLORREF C_GREEN_HOVER = RGB(70, 97, 86);
static COLORREF C_DARK_RED = RGB(110, 50, 72);


static std::wstring ExeDirectory() {
    wchar_t buf[32768]{};
    DWORD n = GetModuleFileNameW(nullptr, buf, (DWORD)(sizeof(buf) / sizeof(buf[0])));
    if (!n) return L".";
    std::wstring p(buf, n);
    size_t s = p.find_last_of(L"\\/");
    return s == std::wstring::npos ? L"." : p.substr(0, s);
}

static std::wstring ParentAssetsPath(const std::wstring& file) {
    return ExeDirectory() + L"\\..\\assets\\" + file;
}

static void LoadIcon(const std::wstring& key, const std::vector<std::wstring>& candidates) {
    for (const auto& name : candidates) {
        std::wstring path = ParentAssetsPath(name);
        DWORD attr = GetFileAttributesW(path.c_str());
        if (attr == INVALID_FILE_ATTRIBUTES || (attr & FILE_ATTRIBUTE_DIRECTORY)) continue;
        auto img = std::make_unique<Gdiplus::Image>(path.c_str());
        if (img && img->GetLastStatus() == Gdiplus::Ok) {
            g_icons[key] = std::move(img);
            return;
        }
    }
}

static void LoadAssets() {
    LoadIcon(L"logo", {L"bavarder.png"});
    LoadIcon(L"general", {L"planete.png", L"general planete.png"});
    LoadIcon(L"server", {L"stockage-serveur.png", L"stockage serveur.png", L"logoserveur.png"});
    LoadIcon(L"proximity", {L"radar.png"});
    LoadIcon(L"group", {L"groupe.png"});
    LoadIcon(L"voice", {L"message-vocal.png", L"message vocal.png"});
    LoadIcon(L"mic_on", {L"cercle.png", L"micro active cercle.png"});
    LoadIcon(L"mic_off", {L"cercle (1).png", L"desactive cercle (1).png"});
    LoadIcon(L"sound_on", {L"son.png", L"caque active son.png"});
    LoadIcon(L"sound_off", {L"du-son.png", L"desactive du-son.png"});
    LoadIcon(L"emoji", {L"lamour.png"});
    LoadIcon(L"send", {L"envoyer-le-message.png"});
    LoadIcon(L"min", {L"moins.png"});
    LoadIcon(L"max", {L"plus.png"});
    LoadIcon(L"settings", {L"reglage-du-son.png"});
    LoadIcon(L"close", {L"multiplication.png"});
}

static bool DrawIcon(HDC dc, const std::wstring& key, int x, int y, int w, int h) {
    auto it = g_icons.find(key);
    if (it == g_icons.end() || !it->second) return false;
    Gdiplus::Graphics gr(dc);
    gr.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
    gr.DrawImage(it->second.get(), Gdiplus::Rect(x, y, w, h));
    return true;
}

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


static double ToDouble(const std::wstring& s, double fallback = 0.0) {
    try { return std::stod(s); } catch (...) { return fallback; }
}

static int ToInt(const std::wstring& s, int fallback = 0) {
    try { return std::stoi(s); } catch (...) { return fallback; }
}

static void CopyUnicodeText(const std::wstring& text) {
    if (text.empty() || !g_hwnd) return;
    if (!OpenClipboard(g_hwnd)) return;
    EmptyClipboard();
    const size_t bytes = (text.size() + 1) * sizeof(wchar_t);
    HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (h) {
        void* p = GlobalLock(h);
        if (p) {
            memcpy(p, text.c_str(), bytes);
            GlobalUnlock(h);
            if (!SetClipboardData(CF_UNICODETEXT, h)) GlobalFree(h);
            h = nullptr;
        }
        if (h) GlobalFree(h);
    }
    CloseClipboard();
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
    const int sideW = std::max(235, std::min(280, w / 3));
    const int bodyRight = w - 16;
    const int sideL = bodyRight - sideW;
    const int chatR = sideL - 8;

    g_titleDragRect = {16, 8, w - 230, 62};
    g_closeRect = {w - 48, 14, w - 14, 48};
    g_settingsRect = {w - 88, 14, w - 54, 48};
    g_maxRect = {w - 128, 14, w - 94, 48};
    g_minRect = {w - 168, 14, w - 134, 48};
    g_manageRect = {w - 278, 14, w - 176, 48};

    int x = 14;
    int tabW = (w - 28 - 18) / 4;
    for (int i=0;i<4;i++) {
        g_tabRects[i] = {x, 70, x + tabW, 112};
        x += tabW + 6;
    }

    g_sendRect = {chatR - 58, h - 59, chatR - 12, h - 17};
    g_inputRect = {28, h - 59, chatR - 66, h - 17};

    g_voiceToggleRect = {w - 58, 139, w - 28, 159};
    g_micRect = {sideL + 13, 174, sideL + (sideW - 20) / 2, 211};
    g_deafRect = {sideL + (sideW - 20) / 2 + 7, 174, w - 28, 211};

    g_groupCopyRect = {44, 153, 142, 184};
    g_groupLeaveRect = {148, 153, 232, 184};
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
    const int sideW = std::max(235, std::min(280, w / 3));
    const int sideL = w - 16 - sideW;
    const int chatR = sideL - 8;
    int areaTop = 124;

    RectI chatPanel{14, 124, chatR, h - 8};
    FillRound(dc, chatPanel, 18, C_PANEL);
    StrokeRound(dc, chatPanel, 18, C_BORDER);

    if (g_channel == L"group" && !g_groupActive) {
        RectI groupBar{27, 137, chatR - 13, 190};
        FillRound(dc, groupBar, 12, RGB(24, 19, 30));
        RECT a{groupBar.l + 12, groupBar.t + 8, groupBar.r - 150, groupBar.b - 8};
        Text(dc, L"Aucun groupe actif", a, g_fontSmall, C_MUTED, DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
        g_manageRect = {groupBar.r - 135, groupBar.t + 10, groupBar.r - 9, groupBar.b - 10};
        FillRound(dc, g_manageRect, 9, RGB(67, 52, 79));
        RECT mr{g_manageRect.l,g_manageRect.t,g_manageRect.r,g_manageRect.b};
        Text(dc, g_manage, mr, g_fontSmall, C_TEXT, DT_CENTER | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
        areaTop = 197;
    } else if (g_channel == L"group" && g_groupActive) {
        RectI groupBar{27, 137, chatR - 13, 190};
        FillRound(dc, groupBar, 12, RGB(24, 19, 30));
        RECT a{groupBar.l + 12, groupBar.t + 5, groupBar.r - 220, groupBar.t + 25};
        Text(dc, L"Votre groupe / lobby privé", a, g_fontTiny, C_MUTED, DT_LEFT | DT_SINGLELINE | DT_NOPREFIX);
        RECT b{groupBar.l + 12, groupBar.t + 25, groupBar.r - 220, groupBar.b - 5};
        std::wstring gtxt = g_groupName;
        if (!g_groupMembers.empty()) {
            gtxt += L"   " + std::to_wstring((int)g_groupMembers.size()) + L"/" + std::to_wstring(g_groupMax);
        }
        Text(dc, gtxt, b, g_fontName, C_TEXT, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
        g_groupCopyRect = {groupBar.r - 196, groupBar.t + 10, groupBar.r - 102, groupBar.b - 10};
        g_groupLeaveRect = {groupBar.r - 96, groupBar.t + 10, groupBar.r - 8, groupBar.b - 10};
        FillRound(dc, g_groupCopyRect, 9, C_PANEL2);
        RECT cr{g_groupCopyRect.l, g_groupCopyRect.t, g_groupCopyRect.r, g_groupCopyRect.b};
        Text(dc, g_copyLabel, cr, g_fontSmall, C_TEXT, DT_CENTER | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
        FillRound(dc, g_groupLeaveRect, 9, C_DARK_RED);
        RECT lr{g_groupLeaveRect.l, g_groupLeaveRect.t, g_groupLeaveRect.r, g_groupLeaveRect.b};
        Text(dc, g_leaveLabel, lr, g_fontSmall, C_TEXT, DT_CENTER | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
        areaTop = 197;
    }

    RectI area{27, areaTop, chatR - 13, h - 73};
    // Le fond principal du chat est déjà dessiné ; ici on garde la zone messages transparente.

    auto it = g_messages.find(g_channel);
    if (it == g_messages.end() || it->second.empty()) {
        std::wstring empty = L"Aucun message dans " + ChannelLabel(g_channel) + L" pour le moment.";
        RECT rc{area.l + 30, area.t + 65, area.r - 30, area.b - 20};
        Text(dc, empty, rc, g_fontText, C_MUTED, DT_CENTER | DT_WORDBREAK | DT_NOPREFIX);
    } else {
        const auto& q = it->second;
        int maxW = area.r - area.l - 20;
        int avail = area.b - area.t - 10;
        int used = 0;
        size_t start = q.size();
        while (start > 0) {
            int mh = CalcMessageHeight(dc, q[start - 1], maxW - 24);
            if (used + mh > avail && start < q.size()) break;
            used += mh;
            --start;
            if (used >= avail) break;
        }

        int y = area.t + 4;
        for (size_t i = start; i < q.size(); ++i) {
            const auto& msg = q[i];
            int mh = CalcMessageHeight(dc, msg, maxW - 24);
            RectI bubble{area.l + 2, y, area.r - 2, std::min(area.b - 4, y + mh - 5)};
            FillRound(dc, bubble, 12, msg.mine ? RGB(75, 59, 88) : C_PANEL2);

            RECT nameRc{bubble.l + 12, bubble.t + 7, bubble.r - 12, bubble.t + 27};
            Text(dc, msg.author.empty() ? L"Joueur" : msg.author, nameRc, g_fontName, C_PINK, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);

            std::wstring meta;
            if (!msg.language.empty()) meta += msg.language;
            if (!msg.server.empty()) {
                if (!meta.empty()) meta += L"  ·  ";
                meta += msg.server;
            }
            if (!meta.empty()) {
                RECT metaRc{bubble.l + 155, bubble.t + 8, bubble.r - 12, bubble.t + 26};
                Text(dc, meta, metaRc, g_fontTiny, C_MUTED, DT_RIGHT | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
            }

            RECT msgRc{bubble.l + 12, bubble.t + 29, bubble.r - 12, bubble.b - 8};
            Text(dc, msg.content, msgRc, g_fontText, C_TEXT, DT_LEFT | DT_WORDBREAK | DT_NOPREFIX);
            y += mh;
            if (y >= area.b) break;
        }
    }

    // Composer comme avant : emoji + champ + envoyer.
    RectI composer{27, h - 65, chatR - 13, h - 14};
    FillRound(dc, composer, 14, C_PANEL2);

    RectI emojiRect{composer.l + 4, composer.t + 7, composer.l + 44, composer.b - 7};
    if (!DrawIcon(dc, L"emoji", emojiRect.l + 8, emojiRect.t + 2, 22, 22)) {
        RECT er{emojiRect.l,emojiRect.t,emojiRect.r,emojiRect.b};
        Text(dc, L"♥", er, g_fontTitle, C_PINK, DT_CENTER | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
    }

    FillRound(dc, g_inputRect, 11, RGB(38, 30, 45));
    std::wstring shown = g_input.empty() ? g_placeholder : g_input;
    RECT inputRc{g_inputRect.l + 12, g_inputRect.t + 5, g_inputRect.r - 10, g_inputRect.b - 5};
    Text(dc, shown, inputRc, g_fontText, g_input.empty() ? C_MUTED : C_TEXT,
         DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);

    FillRound(dc, g_sendRect, 11, RGB(185, 160, 227));
    if (!DrawIcon(dc, L"send", g_sendRect.l + 11, g_sendRect.t + 9, 24, 24)) {
        RECT sr{g_sendRect.l,g_sendRect.t,g_sendRect.r,g_sendRect.b};
        Text(dc, L"➤", sr, g_fontTitle, RGB(33,23,40), DT_CENTER | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
    }
}

static void PaintSidePanel(HDC dc, int w, int h) {
    const int sideW = std::max(235, std::min(280, w / 3));
    const int sideL = w - 16 - sideW;
    RectI side{sideL, 124, w - 14, h - 8};
    FillRound(dc, side, 18, C_PANEL);
    StrokeRound(dc, side, 18, C_BORDER);

    if (!DrawIcon(dc, L"voice", side.l + 13, side.t + 14, 22, 22)) {
        RECT vr{side.l + 13, side.t + 13, side.l + 38, side.t + 39};
        Text(dc, L"🎙", vr, g_fontTitle, C_TEXT, DT_CENTER | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
    }
    RECT vtitle{side.l + 42, side.t + 13, side.r - 55, side.t + 40};
    Text(dc, g_voiceLabel, vtitle, g_fontName, C_TEXT, DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);

    // Petit switch visuel.
    FillRound(dc, g_voiceToggleRect, 18, g_voiceEnabled ? RGB(131, 100, 160) : RGB(79, 68, 87));
    int knob = g_voiceEnabled ? g_voiceToggleRect.r - 17 : g_voiceToggleRect.l + 3;
    RectI kr{knob, g_voiceToggleRect.t + 3, knob + 14, g_voiceToggleRect.b - 3};
    FillRound(dc, kr, 14, RGB(220, 221, 225));

    FillRound(dc, g_micRect, 10, g_muted ? C_DARK_RED : C_GREEN_PANEL);
    if (!DrawIcon(dc, g_muted ? L"mic_off" : L"mic_on", g_micRect.l + 20, g_micRect.t + 9, 18, 18)) {
        RECT mr{g_micRect.l + 8,g_micRect.t,g_micRect.l + 42,g_micRect.b};
        Text(dc, L"●", mr, g_fontText, g_muted ? C_RED : C_GREEN, DT_CENTER | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
    }
    RECT mtxt{g_micRect.l + 43,g_micRect.t,g_micRect.r - 5,g_micRect.b};
    Text(dc, g_micLabel, mtxt, g_fontText, C_TEXT, DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);

    FillRound(dc, g_deafRect, 10, g_deafened ? C_DARK_RED : C_GREEN_PANEL);
    if (!DrawIcon(dc, g_deafened ? L"sound_off" : L"sound_on", g_deafRect.l + 20, g_deafRect.t + 9, 18, 18)) {
        RECT dr{g_deafRect.l + 8,g_deafRect.t,g_deafRect.l + 42,g_deafRect.b};
        Text(dc, L"●", dr, g_fontText, g_deafened ? C_RED : C_GREEN, DT_CENTER | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
    }
    RECT dtxt{g_deafRect.l + 43,g_deafRect.t,g_deafRect.r - 5,g_deafRect.b};
    Text(dc, g_soundLabel, dtxt, g_fontText, C_TEXT, DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);

    COLORREF statusColor = C_MUTED;
    if (g_proxVoiceTone == 1) statusColor = C_GREEN;
    else if (g_proxVoiceTone == 2) statusColor = C_ORANGE;
    else if (g_proxVoiceTone == 3) statusColor = C_RED;
    RECT stat{side.l + 14, side.t + 96, side.r - 12, side.t + 120};
    Text(dc, L"• " + g_proxVoiceStatus, stat, g_fontTiny, statusColor, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);

    g_nearbyMuteRects.clear();

    if (g_channel == L"group") {
        RECT head{side.l + 14, side.t + 137, side.r - 12, side.t + 160};
        Text(dc, g_membersLabel, head, g_fontName, C_PINK, DT_LEFT | DT_SINGLELINE | DT_NOPREFIX);

        std::wstring sub = g_groupActive ? (g_groupName + L" · " + std::to_wstring((int)g_groupMembers.size()) + L"/" + std::to_wstring(g_groupMax))
                                         : L"Aucun groupe actif";
        RECT subr{side.l + 14, side.t + 160, side.r - 12, side.t + 180};
        Text(dc, sub, subr, g_fontTiny, C_MUTED, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);

        int y = side.t + 190;
        for (size_t i=0; i<g_groupMembers.size() && y < side.b - 52; ++i) {
            const auto& gm = g_groupMembers[i];
            RectI row{side.l + 11, y, side.r - 11, y + 54};
            FillRound(dc, row, 11, C_PANEL2);
            RectI av{row.l + 8,row.t + 8,row.l + 44,row.t + 44};
            FillRound(dc, av, 18, RGB(185,160,227));
            std::wstring initial = gm.username.empty() ? L"?" : gm.username.substr(0,1);
            RECT ar{av.l,av.t,av.r,av.b};
            Text(dc, initial, ar, g_fontName, RGB(33,23,40), DT_CENTER | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);

            std::wstring nm = (gm.owner ? L"♛ " : L"") + gm.username;
            RECT nr{row.l + 52,row.t + 7,row.r - 8,row.t + 27};
            Text(dc, nm, nr, g_fontSmall, C_TEXT, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
            std::wstring meta = gm.language;
            if (!gm.server.empty()) {
                if (!meta.empty()) meta += L" · ";
                meta += gm.server;
            }
            RECT mr{row.l + 52,row.t + 28,row.r - 8,row.b - 6};
            Text(dc, meta, mr, g_fontTiny, C_MUTED, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
            y += 59;
        }
        return;
    }

    RECT head{side.l + 14, side.t + 137, side.r - 12, side.t + 160};
    Text(dc, g_nearbyLabel, head, g_fontName, C_PINK, DT_LEFT | DT_SINGLELINE | DT_NOPREFIX);
    RECT subr{side.l + 14, side.t + 160, side.r - 12, side.t + 180};
    std::wstring sub = std::to_wstring((int)g_nearbyPlayers.size()) + L" joueur(s) dans le rayon";
    Text(dc, sub, subr, g_fontTiny, C_MUTED, DT_LEFT | DT_SINGLELINE | DT_NOPREFIX);

    if (g_nearbyPlayers.empty()) {
        RECT empty{side.l + 18, side.t + 205, side.r - 18, side.b - 20};
        Text(dc, g_noNearbyLabel, empty, g_fontText, C_MUTED, DT_CENTER | DT_WORDBREAK | DT_NOPREFIX);
        return;
    }

    int y = side.t + 188;
    for (size_t i=0; i<g_nearbyPlayers.size() && y < side.b - 54; ++i) {
        const auto& p = g_nearbyPlayers[i];
        RectI row{side.l + 10, y, side.r - 10, y + 55};
        FillRound(dc, row, 11, p.speaking ? RGB(47, 41, 55) : RGB(31,25,38));

        RectI av{row.l + 7,row.t + 9,row.l + 43,row.t + 45};
        FillRound(dc, av, 18, RGB(185,160,227));
        std::wstring initial = p.username.empty() ? L"?" : p.username.substr(0,1);
        RECT ar{av.l,av.t,av.r,av.b};
        Text(dc, initial, ar, g_fontName, RGB(33,23,40), DT_CENTER | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);

        RECT nr{row.l + 50,row.t + 6,row.r - 82,row.t + 26};
        Text(dc, p.username, nr, g_fontSmall, C_TEXT, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);

        std::wstring meta = p.language;
        if (!meta.empty()) meta += L" · ";
        meta += std::to_wstring((int)(p.distance + 0.5)) + L" m";
        RECT mr{row.l + 50,row.t + 27,row.r - 82,row.b - 5};
        Text(dc, meta, mr, g_fontTiny, p.speaking ? C_GREEN : C_MUTED, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);

        // Barres toujours vertes.
        int bars = std::max(1, std::min(4, (int)(p.volume * 4.0 + 0.5)));
        RECT br{row.r - 76,row.t + 7,row.r - 38,row.b - 7};
        std::wstring barText;
        for (int k=0;k<4;k++) barText += (k < bars ? L"▮" : L"▯");
        Text(dc, barText, br, g_fontTiny, C_GREEN, DT_CENTER | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);

        RectI muteR{row.r - 36,row.t + 11,row.r - 6,row.b - 11};
        FillRound(dc, muteR, 8, p.muted ? C_DARK_RED : RGB(49,72,63));
        RECT mut{muteR.l,muteR.t,muteR.r,muteR.b};
        Text(dc, p.muted ? L"×" : L"●", mut, g_fontSmall, p.muted ? C_RED : C_GREEN, DT_CENTER | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
        g_nearbyMuteRects.push_back(muteR);

        y += 59;
    }
}

static void PaintWindow(HDC dc, int w, int h) {
    HBRUSH bg = Brush(C_BG);
    RECT all{0,0,w,h};
    FillRect(dc, &all, bg);
    DeleteObject(bg);

    UpdateRects(w, h);

    // Header proche de l'ancienne UI.
    RectI top{0,0,w,62};
    FillRound(dc, top, 0, RGB(29,23,35));
    if (!DrawIcon(dc, L"logo", 14, 14, 34, 34)) {
        RectI logo{14,12,50,48}; FillRound(dc, logo, 12, RGB(51,40,61));
    }

    RECT titleRc{58, 9, w - 290, 34};
    Text(dc, g_title, titleRc, g_fontTitle, C_TEXT, DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);

    std::wstring st = L"● " + g_status;
    if (!g_server.empty()) st += L"  ·  " + g_server;
    RECT statusRc{58, 32, w - 290, 53};
    Text(dc, st, statusRc, g_fontTiny, g_connected ? C_GREEN : C_MUTED, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);

    // LIVE badge
    RectI live{w - 326, 17, w - 282, 45};
    FillRound(dc, live, 9, RGB(52,41,64));
    RECT lr{live.l,live.t,live.r,live.b};
    Text(dc, L"LIVE", lr, g_fontTiny, C_PINK, DT_CENTER | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);

    auto paintTopButton = [&](const RectI& r, const std::wstring& icon, const std::wstring& fallback, COLORREF fg) {
        FillRound(dc, r, 9, C_PANEL2);
        if (!DrawIcon(dc, icon, r.l + 9, r.t + 9, r.r-r.l-18, r.b-r.t-18)) {
            RECT rr{r.l,r.t,r.r,r.b};
            Text(dc, fallback, rr, g_fontTitle, fg, DT_CENTER | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
        }
    };
    paintTopButton(g_minRect, L"min", L"−", C_TEXT);
    paintTopButton(g_maxRect, L"max", L"+", C_TEXT);
    paintTopButton(g_settingsRect, L"settings", L"⚙", C_PINK);
    paintTopButton(g_closeRect, L"close", L"×", C_RED);

    // Onglets.
    std::wstring tabs[4] = {g_tabGeneral, g_tabServer, g_tabProximity, g_tabGroup};
    std::wstring keys[4] = {L"general",L"server",L"proximity",L"group"};
    std::wstring icons[4] = {L"general",L"server",L"proximity",L"group"};
    for (int i=0;i<4;i++) {
        bool active = (g_channel == keys[i]);
        FillRound(dc, g_tabRects[i], 11, active ? RGB(185,160,227) : RGB(24,19,30));
        COLORREF txt = active ? RGB(33,23,40) : C_TEXT;
        int iconX = g_tabRects[i].l + 18;
        if (!DrawIcon(dc, icons[i], iconX, g_tabRects[i].t + 10, 22, 22)) {
            iconX -= 10;
        }
        RECT r{iconX + 28, g_tabRects[i].t, g_tabRects[i].r - 8, g_tabRects[i].b};
        std::wstring label = tabs[i];
        Text(dc, label, r, g_fontTab, txt, DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);
    }

    PaintMessages(dc, w, h);
    PaintSidePanel(dc, w, h);

    if (g_gameMode) {
        RectI badge{w - 96, h - 31, w - 20, h - 10};
        FillRound(dc, badge, 8, RGB(45,65,56));
        RECT br{badge.l,badge.t,badge.r,badge.b};
        Text(dc, L"MODE JEU", br, g_fontTiny, C_GREEN, DT_CENTER | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
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
        if (parts.size() > 8) g_voiceLabel = field(8);
        if (parts.size() > 9) g_micLabel = field(9);
        if (parts.size() > 10) g_soundLabel = field(10);
        if (parts.size() > 11) g_nearbyLabel = field(11);
        if (parts.size() > 12) g_noNearbyLabel = field(12);
        if (parts.size() > 13) g_membersLabel = field(13);
        if (parts.size() > 14) g_settingsLabel = field(14);
        if (parts.size() > 15) g_copyLabel = field(15);
        if (parts.size() > 16) g_leaveLabel = field(16);
        InvalidateRect(g_hwnd, nullptr, FALSE);
    } else if (cmd == "VOICE") {
        g_voiceEnabled = (field(1) == L"1");
        g_muted = (field(2) == L"1");
        g_deafened = (field(3) == L"1");
        InvalidateRect(g_hwnd, nullptr, FALSE);
    } else if (cmd == "PROX_STATUS") {
        g_proxVoiceStatus = field(1);
        g_proxVoiceTone = ToInt(field(2), 0);
        InvalidateRect(g_hwnd, nullptr, FALSE);
    } else if (cmd == "NEARBY_CLEAR") {
        g_nearbyPlayers.clear();
        g_nearbyCount = 0;
        InvalidateRect(g_hwnd, nullptr, FALSE);
    } else if (cmd == "NEARBY_PLAYER") {
        NearbyPlayerNative p;
        p.userId = field(1);
        p.username = field(2);
        p.distance = ToDouble(field(3), 0.0);
        p.speaking = (field(4) == L"1");
        p.muted = (field(5) == L"1");
        p.volume = ToDouble(field(6), 1.0);
        p.language = field(7);
        p.server = field(8);
        g_nearbyPlayers.push_back(std::move(p));
        g_nearbyCount = (int)g_nearbyPlayers.size();
        InvalidateRect(g_hwnd, nullptr, FALSE);
    } else if (cmd == "GROUP_DETAIL") {
        g_groupActive = (field(1) == L"1");
        g_groupName = field(2);
        g_groupInvite = field(3);
        g_groupMax = std::max(1, ToInt(field(4), 25));
        InvalidateRect(g_hwnd, nullptr, FALSE);
    } else if (cmd == "GROUP_MEMBERS_CLEAR") {
        g_groupMembers.clear();
        InvalidateRect(g_hwnd, nullptr, FALSE);
    } else if (cmd == "GROUP_MEMBER") {
        GroupMemberNative gm;
        gm.username = field(1);
        gm.owner = (field(2) == L"1");
        gm.language = field(3);
        gm.server = field(4);
        g_groupMembers.push_back(std::move(gm));
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
        if (g_settingsRect.contains(x,y)) { SendEvent("LEGACY_SETTINGS"); HideOverlay(); return 0; }
        if (g_minRect.contains(x,y)) {
            SetWindowPos(hwnd, HWND_TOPMOST, 0,0, 620, 430, SWP_NOMOVE | SWP_NOACTIVATE);
            InvalidateRect(hwnd,nullptr,FALSE); return 0;
        }
        if (g_maxRect.contains(x,y)) {
            SetWindowPos(hwnd, HWND_TOPMOST, 0,0, 920, 560, SWP_NOMOVE | SWP_NOACTIVATE);
            InvalidateRect(hwnd,nullptr,FALSE); return 0;
        }
        if (g_voiceToggleRect.contains(x,y)) { SendEvent("VOICE_TOGGLE"); return 0; }
        if (g_micRect.contains(x,y)) { SendEvent("MUTE_TOGGLE"); return 0; }
        if (g_deafRect.contains(x,y)) { SendEvent("DEAF_TOGGLE"); return 0; }

        std::wstring keys[4] = {L"general",L"server",L"proximity",L"group"};
        for (int i=0;i<4;i++) if (g_tabRects[i].contains(x,y)) {
            g_channel = keys[i];
            SendEvent("CHANNEL", {g_channel});
            InvalidateRect(hwnd,nullptr,FALSE);
            return 0;
        }

        if (g_channel == L"group" && !g_groupActive && g_manageRect.contains(x,y)) {
            SendEvent("LEGACY_GROUP");
            HideOverlay();
            return 0;
        }
        if (g_channel == L"group" && g_groupActive) {
            if (g_groupCopyRect.contains(x,y)) { CopyUnicodeText(g_groupInvite); return 0; }
            if (g_groupLeaveRect.contains(x,y)) { SendEvent("GROUP_LEAVE"); return 0; }
        }

        for (size_t i=0; i<g_nearbyMuteRects.size() && i<g_nearbyPlayers.size(); ++i) {
            if (g_nearbyMuteRects[i].contains(x,y)) {
                SendEvent("PROX_MUTE", {g_nearbyPlayers[i].userId});
                return 0;
            }
        }

        if (g_sendRect.contains(x,y)) { SubmitInput(); return 0; }
        if (g_titleDragRect.contains(x,y)) {
            g_dragging = true;
            SetCapture(hwnd);
            GetCursorPos(&g_dragStart);
            GetWindowRect(hwnd,&g_windowStart);
            return 0;
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

    const wchar_t* cls = L"SSONativeChatOverlayV3";
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
    g_fontTiny = MakeFont(9, FW_NORMAL);

    Gdiplus::GdiplusStartupInput gdiplusInput;
    if (Gdiplus::GdiplusStartup(&g_gdiplusToken, &gdiplusInput, nullptr) == Gdiplus::Ok) {
        LoadAssets();
    }

    int w = 920, h = 560;
    int sw = GetSystemMetrics(SM_CXSCREEN), sh = GetSystemMetrics(SM_CYSCREEN);
    int x = std::max(20, (sw - w) / 2);
    int y = std::max(20, (sh - h) / 2);

    DWORD ex = WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_TRANSPARENT;
    g_hwnd = CreateWindowExW(ex, cls, L"Chat & Vocal Star Stable", WS_POPUP,
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
    if (g_fontTiny) DeleteObject(g_fontTiny);
    g_icons.clear();
    if (g_gdiplusToken) Gdiplus::GdiplusShutdown(g_gdiplusToken);
    return 0;
}
