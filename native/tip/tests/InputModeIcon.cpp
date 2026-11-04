// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 amanetoki7

// タスクバーの入力モードのアイコン (LangBar.cpp の CreateInputModeIcon) が、Microsoft IME の表示と同じに見えることを確かめる。
// 画面もウィンドウも使わない。Meltype 本体・設定・学習データにはつながない。
#include "CandidateWindow.h"
#include "LangBar.h"
#include "TextService.h"

#include <dwrite.h>
#include <wrl/client.h>

#include <algorithm>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace meltype {
HINSTANCE g_module = nullptr;
LONG g_locks = 0;
void TipLog(const wchar_t*, ...) {}
std::string ToUtf8(const std::wstring&) { return {}; }
std::wstring FromUtf8(const std::string&) { return {}; }
// LangBar.cpp が参照する。このテストではボタンを押さないので何もしない
void TextService::ToggleInputMode(ITfContext*) {}
}  // namespace meltype

namespace {

// Windows 11 (ビルド 26300、100%、明るいテーマ) のタスクバーに出ている Microsoft IME の「あ」「A」。
// Meltype のアイコンが置かれる 16×16 の枠を撮り、背景 (明るさ 235) との差を取ったもの (最大 210 = 黒の 89%、Windows の標準の文字色)。
// Segoe Fluent Icons の U+E986 (FullHiragana) / U+E97E (HalfAlpha) をそのまま描いた絵
constexpr int kMsImeHiragana[16][16] = {
    {1, 0, 0, 0, 1, 138, 49, 0, 1, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 150, 51, 0, 1, 0, 1, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 151, 49, 48, 48, 72, 109, 181, 72, 0, 1, 0},
    {0, 72, 210, 210, 210, 210, 210, 150, 150, 122, 96, 36, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 210, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 210, 0, 0, 0, 137, 37, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 15, 210, 139, 181, 210, 210, 166, 72, 0, 0, 0, 0},
    {0, 0, 0, 84, 196, 210, 60, 27, 60, 138, 60, 150, 166, 17, 0, 0},
    {0, 0, 109, 167, 28, 210, 0, 0, 137, 73, 0, 0, 137, 96, 0, 0},
    {0, 48, 181, 15, 0, 166, 25, 26, 196, 14, 0, 1, 28, 181, 0, 0},
    {0, 122, 72, 1, 0, 151, 49, 122, 96, 0, 0, 0, 0, 210, 0, 0},
    {1, 181, 27, 0, 0, 95, 151, 196, 14, 0, 1, 0, 12, 210, 0, 0},
    {0, 210, 0, 0, 0, 39, 210, 60, 1, 0, 0, 0, 73, 137, 0, 0},
    {0, 166, 86, 0, 71, 196, 72, 0, 1, 0, 0, 16, 195, 37, 0, 0},
    {0, 37, 181, 210, 166, 60, 0, 0, 0, 15, 72, 196, 73, 0, 0, 0},
    {0, 0, 0, 0, 0, 1, 1, 0, 123, 210, 137, 48, 0, 1, 0, 0},
};
constexpr int kMsImeAlpha[16][16] = {
    {1, 0, 0, 0, 1, 0, 0, 96, 97, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 1, 181, 181, 0, 1, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 1, 60, 166, 166, 60, 0, 0, 0, 0, 1, 0},
    {0, 0, 0, 0, 0, 0, 123, 85, 83, 122, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 15, 196, 14, 15, 196, 13, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 72, 137, 0, 0, 137, 72, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 137, 74, 0, 0, 73, 138, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 27, 196, 0, 0, 0, 1, 196, 27, 0, 1, 0, 0},
    {0, 0, 0, 1, 85, 123, 0, 0, 0, 1, 122, 84, 0, 0, 0, 0},
    {0, 0, 0, 0, 180, 37, 0, 0, 0, 0, 36, 181, 1, 0, 0, 0},
    {0, 0, 0, 39, 210, 210, 210, 210, 210, 210, 210, 210, 37, 1, 0, 0},
    {1, 0, 0, 109, 96, 0, 1, 0, 0, 0, 1, 96, 108, 0, 0, 0},
    {0, 0, 0, 196, 26, 1, 0, 0, 1, 0, 0, 25, 196, 0, 0, 0},
    {0, 0, 62, 166, 0, 1, 0, 0, 1, 0, 0, 1, 149, 60, 0, 0},
    {0, 0, 122, 73, 0, 0, 0, 0, 0, 0, 0, 0, 73, 122, 0, 0},
    {0, 0, 181, 13, 0, 1, 1, 0, 0, 0, 0, 0, 15, 181, 0, 0},
};
// 上の行列の背景の明るさ。アイコンのアルファ (0..255) をこの背景に重ねたときの濃さに直して比べる
constexpr int kBackground = 235;
// 許す差分の合計。手元の実測は 100 未満。グリフが 1 ピクセルずれると 8000 を超え、大きさや太さが違えばさらに大きい
constexpr long kTolerance = 1000;

bool HasFont(const wchar_t* family) {
    Microsoft::WRL::ComPtr<IDWriteFactory> dwrite;
    if (FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), reinterpret_cast<IUnknown**>(dwrite.GetAddressOf())))) return false;
    Microsoft::WRL::ComPtr<IDWriteFontCollection> fonts;
    UINT32 index = 0;
    BOOL exists = FALSE;
    return SUCCEEDED(dwrite->GetSystemFontCollection(&fonts)) && SUCCEEDED(fonts->FindFamilyName(family, &index, &exists)) && exists;
}

struct Icon {
    int width = 0, height = 0, bitsPerPixel = 0;
    bool hasMask = false;
    std::vector<BYTE> pixels;  // 乗算済み BGRA、上から下
    BYTE Alpha(int x, int y) const { return pixels[(static_cast<size_t>(y) * width + x) * 4 + 3]; }
};

Icon ReadIcon(HICON icon) {
    Icon result;
    ICONINFO info = {};
    if (icon == nullptr || !GetIconInfo(icon, &info)) return result;
    BITMAP bitmap = {};
    if (info.hbmColor != nullptr && GetObjectW(info.hbmColor, sizeof(bitmap), &bitmap) != 0) {
        result.width = bitmap.bmWidth;
        result.height = bitmap.bmHeight;
        result.bitsPerPixel = bitmap.bmBitsPixel;
        result.pixels.assign(static_cast<size_t>(result.width) * result.height * 4, 0);
        BITMAPINFO header = {};
        header.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        header.bmiHeader.biWidth = result.width;
        header.bmiHeader.biHeight = -result.height;
        header.bmiHeader.biPlanes = 1;
        header.bmiHeader.biBitCount = 32;
        HDC dc = CreateCompatibleDC(nullptr);
        GetDIBits(dc, info.hbmColor, 0, result.height, result.pixels.data(), &header, DIB_RGB_COLORS);
        DeleteDC(dc);
    }
    result.hasMask = info.hbmMask != nullptr;
    if (info.hbmColor != nullptr) DeleteObject(info.hbmColor);
    if (info.hbmMask != nullptr) DeleteObject(info.hbmMask);
    return result;
}

struct Box {
    int left = INT_MAX, top = INT_MAX, right = -1, bottom = -1;
    int Width() const { return right < left ? 0 : right - left + 1; }
    int Height() const { return bottom < top ? 0 : bottom - top + 1; }
};

// 墨のある (アルファが 0 でない) 範囲
Box InkBox(const Icon& icon) {
    Box box;
    for (int y = 0; y < icon.height; y++) {
        for (int x = 0; x < icon.width; x++) {
            if (icon.Alpha(x, y) == 0) continue;
            box.left = std::min(box.left, x);
            box.right = std::max(box.right, x);
            box.top = std::min(box.top, y);
            box.bottom = std::max(box.bottom, y);
        }
    }
    return box;
}

}  // namespace

int main() {
    int failed = 0;
    auto check = [&](bool condition, const std::string& name) {
        std::printf("%s %s\n", condition ? "PASS" : "FAIL", name.c_str());
        if (!condition) ++failed;
    };
    const bool fluent = HasFont(L"Segoe Fluent Icons");
    const bool iconFont = fluent || HasFont(L"Segoe MDL2 Assets");
    std::printf("icon font: %s\n", fluent ? "Segoe Fluent Icons" : iconFont ? "Segoe MDL2 Assets (shapes differ slightly; not compared with the reference)" : "(none: text fallback)");

    check(meltype::CreateInputModeIcon(true, 0, false) == nullptr, "size 0 gives no icon");

    // 16 = 100%、24 = 150%、36 = 225% のタスクバーの小さいアイコンの大きさ。GetIcon はタスクバーの DPI に合わせてこの大きさで作る
    for (int size : {16, 24, 36}) {
        for (bool dark : {false, true}) {
            for (bool japanese : {true, false}) {
                char prefix[64];
                std::snprintf(prefix, sizeof(prefix), "%s %dpx %s: ", japanese ? "hiragana" : "alpha", size, dark ? "dark" : "light");
                HICON handle = meltype::CreateInputModeIcon(japanese, size, dark);
                Icon icon = ReadIcon(handle);
                check(handle != nullptr && icon.width == size && icon.height == size && icon.bitsPerPixel == 32 && icon.hasMask,
                      std::string(prefix) + "32bpp icon with a mask, of the requested size");
                if (icon.width == 0) {
                    if (handle != nullptr) DestroyIcon(handle);
                    continue;
                }
                bool premultiplied = true, inkColor = true;
                int maxAlpha = 0, antialiased = 0;
                for (size_t i = 0; i < icon.pixels.size(); i += 4) {
                    const int b = icon.pixels[i], g = icon.pixels[i + 1], r = icon.pixels[i + 2], a = icon.pixels[i + 3];
                    premultiplied = premultiplied && r <= a && g <= a && b <= a;
                    // 暗いテーマは白 (乗算済みなので色 = アルファ)、明るいテーマは黒
                    inkColor = inkColor && (dark ? (r == a && g == a && b == a) : (r == 0 && g == 0 && b == 0));
                    maxAlpha = std::max(maxAlpha, a);
                    if (a > 0 && a < 128) ++antialiased;
                }
                check(premultiplied, std::string(prefix) + "premultiplied alpha");
                check(inkColor, std::string(prefix) + (dark ? "white ink" : "black ink"));
                // 明るいテーマは Windows の標準の文字色 (黒の 89% = 0xE4) で、Microsoft IME の表示と同じ濃さにする
                check(dark ? maxAlpha == 255 : (maxAlpha >= 0xE0 && maxAlpha <= 0xE8), std::string(prefix) + (dark ? "fully opaque" : "89% opaque like Windows' text color"));
                check(antialiased > 0, std::string(prefix) + "anti-aliased edges");
                const Box box = InkBox(icon);
                std::printf("  ink %dx%d at (%d,%d), max alpha %d\n", box.Width(), box.Height(), box.left, box.top, maxAlpha);
                if (iconFont) {
                    // Microsoft IME と同じく、アイコンの高さいっぱいに描く (以前の文字の描画では 16px で 11〜14px しか無く、小さく見えた)
                    check(box.Height() >= size * 14 / 16 && box.Height() <= size, std::string(prefix) + "glyph fills the icon height like Microsoft IME");
                    check(box.Width() >= size * 10 / 16 && box.Width() < size, std::string(prefix) + "glyph width");
                }
                if (fluent && size == 16 && !dark) {
                    long difference = 0;
                    for (int y = 0; y < 16; y++) {
                        for (int x = 0; x < 16; x++) {
                            const int ours = icon.Alpha(x, y) * kBackground / 255;
                            difference += std::abs(ours - (japanese ? kMsImeHiragana : kMsImeAlpha)[y][x]);
                        }
                    }
                    std::printf("  difference from Microsoft IME's taskbar glyph: %ld (tolerance %ld)\n", difference, kTolerance);
                    check(difference <= kTolerance, std::string(prefix) + "same glyph, size, position and color as Microsoft IME's taskbar display");
                }
                DestroyIcon(handle);
            }
        }
    }
    meltype::CandidateWindow::ReleaseFactories();
    std::printf("%s\n", failed == 0 ? "all passed" : "FAILED");
    return failed == 0 ? 0 : 1;
}
