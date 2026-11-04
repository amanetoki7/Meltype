// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 lnkiai

#include "LangBar.h"

#include <d2d1.h>
#include <dwrite.h>
#include <wrl/client.h>

#include <cmath>

#include "CandidateWindow.h"
#include "TextService.h"

namespace meltype {
namespace {

constexpr DWORD kSinkCookie = 0x4D454C54;  // 'MELT'

// Windows 11 のタスクバーは、Microsoft IME の入力モードをアイコンフォント Segoe Fluent Icons のグリフで描いている
// (実機のタスクバーの画素と照合して突き止めた)。同じグリフをアイコンと同じ大きさで描けば、Microsoft IME と見分けがつかない。
// Segoe Fluent Icons が無い Windows 10 では Segoe MDL2 Assets (同じ符号位置。形がわずかに違う)
constexpr wchar_t kIconFonts[][24] = {L"Segoe Fluent Icons", L"Segoe MDL2 Assets"};
constexpr wchar_t kGlyphJapanese = 0xE986;  // FullHiragana「あ」
constexpr wchar_t kGlyphAlphabet = 0xE97E;  // HalfAlpha「A」(Microsoft IME の半角英数と同じ。全角英数の FullAlpha は U+E97F)

// 文字の色。Windows 11 の標準の文字色 (TextFillColorPrimary): 明るいテーマでは黒の 89% (#E4000000)、暗いテーマでは白
constexpr float kLightInkAlpha = 0xE4 / 255.0f;

// フォントが入っているか
bool HasFontFamily(IDWriteFactory* dwrite, const wchar_t* family) {
    Microsoft::WRL::ComPtr<IDWriteFontCollection> fonts;
    UINT32 index = 0;
    BOOL exists = FALSE;
    return SUCCEEDED(dwrite->GetSystemFontCollection(&fonts)) && SUCCEEDED(fonts->FindFamilyName(family, &index, &exists)) && exists;
}

}  // namespace

// タスクバーが暗い (ダークモード) か。テーマ変更に追従するため TextService からも読む
bool TaskbarIsDark() {
    DWORD value = 0, size = sizeof(value);
    if (RegGetValueW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize", L"SystemUsesLightTheme",
                     RRF_RT_REG_DWORD, nullptr, &value, &size) == ERROR_SUCCESS) {
        return value == 0;
    }
    return true;
}

// 入力モードのアイコン (グリフか文字だけ。背景は透明)。GDI の DrawText ではなく DirectWrite で描く:
// 16px では GDI の文字は線が細く形も崩れ、タスクバーのほかの表示 (DirectWrite で描かれている) より粗く見えたため
HICON CreateInputModeIcon(bool japanese, int size, bool dark) {
    using Microsoft::WRL::ComPtr;
    if (size <= 0) return nullptr;
    ID2D1Factory* d2d = nullptr;
    IDWriteFactory* dwrite = nullptr;
    if (!CandidateWindow::SharedFactories(&d2d, &dwrite)) return nullptr;

    // アイコンフォントのグリフ。アイコンフォントが無ければ文字 (Yu Gothic UI の Bold。標準の太さでは 16px で線が細く薄く見える)
    const wchar_t* family = nullptr;
    for (const wchar_t* candidate : kIconFonts) {
        if (HasFontFamily(dwrite, candidate)) {
            family = candidate;
            break;
        }
    }
    const bool iconFont = family != nullptr;
    if (!iconFont) family = L"Yu Gothic UI";
    const wchar_t text[2] = {iconFont ? (japanese ? kGlyphJapanese : kGlyphAlphabet) : (japanese ? L'あ' : L'A'), L'\0'};

    const auto side = static_cast<float>(size);
    ComPtr<IDWriteTextFormat> format;
    if (FAILED(dwrite->CreateTextFormat(family, nullptr, iconFont ? DWRITE_FONT_WEIGHT_NORMAL : DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL,
                                        DWRITE_FONT_STRETCH_NORMAL, side, L"ja-jp", &format))) {
        return nullptr;
    }
    format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
    ComPtr<IDWriteTextLayout> layout;
    if (FAILED(dwrite->CreateTextLayout(text, 1, format.Get(), side, side, &layout))) return nullptr;
    // アイコンフォントのグリフは、アイコンと同じ大きさの升目に収まるように作られているので、升目をそのまま (0,0) に置く
    // (タスクバーは Microsoft IME のグリフをアイコンの枠と同じ位置に描くので、これで位置も同じになる: 実測)。
    // 文字は、墨の部分 (字面) の中心をアイコンの中心に合わせる (行の高さで中央に置くと「A」は下の空き (ディセンダー) の分だけ上に寄るため)。
    // はみ出し (overhang) は (0,0)-(size,size) に対する字面の出っ張り (内側なら負)。ぼやけないように 1 ピクセル単位で動かす
    D2D1_POINT_2F origin = D2D1::Point2F(0, 0);
    if (!iconFont) {
        DWRITE_OVERHANG_METRICS overhang = {};
        layout->GetOverhangMetrics(&overhang);
        const float inkLeft = -overhang.left, inkRight = side + overhang.right;
        const float inkTop = -overhang.top, inkBottom = side + overhang.bottom;
        origin = D2D1::Point2F(std::round((side - inkLeft - inkRight) / 2), std::round((side - inkTop - inkBottom) / 2));
    }

    // 32 ビットの DIB に描く。Direct2D の DC 用のレンダーターゲットは、乗算済みアルファのまま DIB に写す
    BITMAPINFO info = {};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = size;
    info.bmiHeader.biHeight = -size;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    HDC dc = CreateCompatibleDC(nullptr);
    HBITMAP color = CreateDIBSection(dc, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (color == nullptr) {
        DeleteDC(dc);
        return nullptr;
    }
    HGDIOBJ oldBitmap = SelectObject(dc, color);
    bool drawn = false;
    {
        // ソフトウェアで描く (GPU に依らない。アプリのプロセスの中で、小さな絵を入力モードが変わるたびに 1 回描くだけ)。
        // DPI は 96 に固定して、1 DIP = 1 ピクセルにする
        const D2D1_RENDER_TARGET_PROPERTIES props = D2D1::RenderTargetProperties(
            D2D1_RENDER_TARGET_TYPE_SOFTWARE, D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED), 96, 96);
        const RECT rect = {0, 0, size, size};
        ComPtr<ID2D1DCRenderTarget> target;
        ComPtr<ID2D1SolidColorBrush> brush;
        const D2D1_COLOR_F ink = dark ? D2D1::ColorF(D2D1::ColorF::White) : D2D1::ColorF(0, 0, 0, kLightInkAlpha);
        if (SUCCEEDED(d2d->CreateDCRenderTarget(&props, &target)) && SUCCEEDED(target->BindDC(dc, &rect)) && SUCCEEDED(target->CreateSolidColorBrush(ink, &brush))) {
            // 背景が透明なので ClearType (色の縁取り) は使えない。グレースケールで滑らかにする (タスクバーと同じ)
            target->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
            if (!iconFont) {
                // 文字は、縦方向をピクセルの境に合わせ (ヒンティング)、横方向だけ滑らかにする (小さな文字向けの描き方)。
                // フォントの推奨 (既定。縦横とも滑らか) では 16px で線がにじんで太く見えたため。ガンマなどはシステムの設定のまま
                ComPtr<IDWriteRenderingParams> system, params;
                if (SUCCEEDED(dwrite->CreateRenderingParams(&system)) &&
                    SUCCEEDED(dwrite->CreateCustomRenderingParams(system->GetGamma(), system->GetEnhancedContrast(), system->GetClearTypeLevel(),
                                                                  system->GetPixelGeometry(), DWRITE_RENDERING_MODE_NATURAL, &params))) {
                    target->SetTextRenderingParams(params.Get());
                }
            }
            target->BeginDraw();
            target->Clear(D2D1::ColorF(0, 0, 0, 0));
            target->DrawTextLayout(origin, layout.Get(), brush.Get());
            drawn = SUCCEEDED(target->EndDraw());
        }
    }
    GdiFlush();
    SelectObject(dc, oldBitmap);
    DeleteDC(dc);

    HICON icon = nullptr;
    if (drawn) {
        // マスク (1 = 透明)。32 ビットのアイコンではアルファが使われるが、アルファを見ない描き方のために、文字の無い所を透明にしておく
        const int stride = ((size + 15) / 16) * 2;  // 1 ビット / ピクセル。行は WORD 境界
        std::vector<BYTE> maskBits(static_cast<size_t>(stride) * size, 0);
        const auto* pixels = static_cast<const BYTE*>(bits);
        for (int y = 0; y < size; y++) {
            for (int x = 0; x < size; x++) {
                if (pixels[(y * size + x) * 4 + 3] == 0) maskBits[y * stride + x / 8] |= static_cast<BYTE>(0x80 >> (x % 8));
            }
        }
        HBITMAP mask = CreateBitmap(size, size, 1, 1, maskBits.data());
        if (mask != nullptr) {
            ICONINFO iconInfo = {};
            iconInfo.fIcon = TRUE;
            iconInfo.hbmColor = color;
            iconInfo.hbmMask = mask;
            icon = CreateIconIndirect(&iconInfo);
            DeleteObject(mask);
        }
    }
    DeleteObject(color);
    return icon;
}

LangBarButton::LangBarButton(TextService* service) : service_(service) { DllAddRef(); }

LangBarButton::~LangBarButton() {
    if (sink_ != nullptr) sink_->Release();
    DllRelease();
}

STDMETHODIMP LangBarButton::QueryInterface(REFIID riid, void** ppv) {
    if (ppv == nullptr) return E_INVALIDARG;
    *ppv = nullptr;
    if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_ITfLangBarItem) || IsEqualIID(riid, IID_ITfLangBarItemButton)) {
        *ppv = static_cast<ITfLangBarItemButton*>(this);
    } else if (IsEqualIID(riid, IID_ITfSource)) {
        *ppv = static_cast<ITfSource*>(this);
    }
    if (*ppv == nullptr) return E_NOINTERFACE;
    AddRef();
    return S_OK;
}

STDMETHODIMP_(ULONG) LangBarButton::AddRef() { return InterlockedIncrement(&refs_); }

STDMETHODIMP_(ULONG) LangBarButton::Release() {
    ULONG count = InterlockedDecrement(&refs_);
    if (count == 0) delete this;
    return count;
}

STDMETHODIMP LangBarButton::GetInfo(TF_LANGBARITEMINFO* info) {
    if (info == nullptr) return E_INVALIDARG;
    info->clsidService = CLSID_TextService;
    // Windows 8 以降のタスクバーの入力モードの表示は GUID_LBI_INPUTMODE の項目を見る
    info->guidItem = GUID_LBI_INPUTMODE;
    info->dwStyle = TF_LBI_STYLE_BTN_BUTTON | TF_LBI_STYLE_SHOWNINTRAY;
    info->ulSort = 0;
    wcscpy_s(info->szDescription, L"Meltype の入力モード");
    return S_OK;
}

STDMETHODIMP LangBarButton::GetStatus(DWORD* status) {
    if (status == nullptr) return E_INVALIDARG;
    *status = 0;
    return S_OK;
}

STDMETHODIMP LangBarButton::GetTooltipString(BSTR* tooltip) {
    if (tooltip == nullptr) return E_INVALIDARG;
    bool open = service_ == nullptr || service_->IsOpen();
    *tooltip = SysAllocString(open ? L"Meltype: 日本語 (クリックで英数)" : L"Meltype: 英数 (クリックで日本語)");
    return *tooltip ? S_OK : E_OUTOFMEMORY;
}

STDMETHODIMP LangBarButton::OnClick(TfLBIClick, POINT, const RECT*) {
    if (service_ != nullptr) service_->ToggleInputMode(nullptr);
    return S_OK;
}

STDMETHODIMP LangBarButton::GetIcon(HICON* icon) {
    if (icon == nullptr) return E_INVALIDARG;
    // タスクバーが実際に描く大きさでアイコンを作る。GetSystemMetrics(SM_CXSMICON) はこの TIP が載っているアプリの
    // DPI の扱いで決まり、DPI 非対応のアプリでは 100% の 16px になる。それをタスクバー (DPI 対応) が拡大するとぼやける
    // (225% ではタスクバーの枠は 36px)。GetDpiForWindow(Shell_TrayWnd) はアプリの DPI の扱いに関係なくタスクバーの
    // 実 DPI を返すので、それに合わせた大きさで作れば、どのアプリに載っていても拡大されず鮮明になる。
    UINT dpi = 0;
    if (HWND tray = FindWindowW(L"Shell_TrayWnd", nullptr)) dpi = GetDpiForWindow(tray);
    if (dpi == 0) dpi = 96;
    int size = GetSystemMetricsForDpi(SM_CXSMICON, dpi);
    if (size <= 0) size = 16;
    *icon = CreateInputModeIcon(service_ == nullptr || service_->IsOpen(), size, TaskbarIsDark());
    return *icon ? S_OK : E_FAIL;
}

STDMETHODIMP LangBarButton::GetText(BSTR* text) {
    if (text == nullptr) return E_INVALIDARG;
    *text = SysAllocString(service_ == nullptr || service_->IsOpen() ? L"あ" : L"A");
    return *text ? S_OK : E_OUTOFMEMORY;
}

STDMETHODIMP LangBarButton::AdviseSink(REFIID riid, IUnknown* sink, DWORD* cookie) {
    if (!IsEqualIID(riid, IID_ITfLangBarItemSink)) return CONNECT_E_CANNOTCONNECT;
    if (sink_ != nullptr) return CONNECT_E_ADVISELIMIT;
    if (sink == nullptr || cookie == nullptr) return E_INVALIDARG;
    if (FAILED(sink->QueryInterface(IID_ITfLangBarItemSink, reinterpret_cast<void**>(&sink_)))) {
        sink_ = nullptr;
        return E_NOINTERFACE;
    }
    *cookie = kSinkCookie;
    return S_OK;
}

STDMETHODIMP LangBarButton::UnadviseSink(DWORD cookie) {
    if (cookie != kSinkCookie || sink_ == nullptr) return CONNECT_E_NOCONNECTION;
    sink_->Release();
    sink_ = nullptr;
    return S_OK;
}

void LangBarButton::Update() {
    if (sink_ != nullptr) sink_->OnUpdate(TF_LBI_ICON | TF_LBI_TEXT | TF_LBI_TOOLTIP);
}

}  // namespace meltype
