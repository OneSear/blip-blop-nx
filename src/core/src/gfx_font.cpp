#include "bb/core/gfx/font.hpp"

#include <string>
#include <string_view>

#include "bb/core/assets/asset_types.hpp"
#include "bb/core/assets/image16.hpp"
#include "bb/core/assets/text_encoding.hpp"

namespace bb::core::gfx {

void BitmapFont::assign(const assets::Font& font) {
    clear();
    lineHeight_ = font.lineHeight;
    spaceWidth_ = font.spaceWidth;

    const Pixel key =
        assets::toPixel(assets::colorKey16(assets::PixelFormat16::Rgb555), assets::PixelFormat16::Rgb555);
    for (const auto& entry : font.glyphs) {
        Pixmap pixels = assets::toPixmap(entry.second, false);
        pixels.setColorKey(key);
        glyphs_[entry.first] = std::make_unique<Sprite>(std::move(pixels), 0, 0);
    }
    loaded_ = true;
}

void BitmapFont::clear() noexcept {
    glyphs_.clear();
    loaded_ = false;
}

const Sprite* BitmapFont::glyph(std::uint32_t codePoint) const {
    const auto it = glyphs_.find(codePoint);
    return it == glyphs_.end() ? nullptr : it->second.get();
}

void BitmapFont::print(Pixmap& dst, int x, int y, const char* text) const {
    if (text == nullptr) {
        return;
    }
    const std::string_view s(text);
    std::size_t i = 0;
    while (i < s.size()) {
        const std::uint32_t c = assets::nextCodePoint(s, i);
        if (c == ' ') {
            x += spaceWidth_;
        } else if (const Sprite* g = glyph(c)) {
            draw(dst, *g, x, y);
            x += g->width();
        }
    }
}

void BitmapFont::printRight(Pixmap& dst, int x, int y, const char* text) const {
    if (text == nullptr) {
        return;
    }
    print(dst, x - width(text), y, text);
}

void BitmapFont::printCentred(Pixmap& dst, int x, int y, const char* text) const {
    if (text == nullptr) {
        return;
    }
    print(dst, x - (width(text) >> 1), y, text);
}

void BitmapFont::printWrapped(Pixmap& dst, int x, int y, const char* text, int right) const {
    if (text == nullptr) {
        return;
    }
    const std::string_view s(text);
    std::size_t i = 0;
    int curX = x;
    int curY = y;
    while (i < s.size()) {
        const std::uint32_t c = assets::nextCodePoint(s, i);
        if (c == ' ') {
            curX += spaceWidth_;
        } else if (const Sprite* g = glyph(c)) {
            const int nextX = curX + g->width();
            if (nextX >= right) {
                curY += lineHeight_;
                draw(dst, *g, x, curY);
                curX = x + g->width();
            } else {
                draw(dst, *g, curX, curY);
                curX = nextX;
            }
        }
    }
}

int BitmapFont::printWordWrapped(Pixmap& dst, int x, int y, const char* text, int right) const {
    if (text == nullptr) {
        return 0;
    }
    // Words separated by one or more spaces (the 2002 strtok on " ").
    const std::string_view s(text);
    int curX = x;
    int curY = y;
    int breaks = 0;
    bool printed = false;
    std::size_t pos = 0;
    while (pos < s.size()) {
        if (s[pos] == ' ') {
            ++pos;
            continue;
        }
        std::size_t end = s.find(' ', pos);
        if (end == std::string_view::npos) {
            end = s.size();
        }
        const std::string word(s.substr(pos, end - pos));
        const int w = width(word.c_str());
        if (curX + w > right) {
            curY += lineHeight_;
            print(dst, x, curY, word.c_str());
            curX = x + w;
            ++breaks;
        } else {
            print(dst, curX, curY, word.c_str());
            curX += w;
        }
        curX += spaceWidth_;
        printed = true;
        pos = end;
    }
    return printed ? breaks + 1 : 0;
}

int BitmapFont::width(const char* text) const {
    if (text == nullptr) {
        return 0;
    }
    const std::string_view s(text);
    std::size_t i = 0;
    int total = 0;
    while (i < s.size()) {
        const std::uint32_t c = assets::nextCodePoint(s, i);
        if (c == ' ') {
            total += spaceWidth_;
        } else if (const Sprite* g = glyph(c)) {
            total += g->width();
        }
    }
    return total;
}

}  // namespace bb::core::gfx
