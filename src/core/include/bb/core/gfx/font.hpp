#pragma once

#include <cstdint>
#include <map>
#include <memory>

#include "bb/core/gfx/sprite.hpp"

namespace bb::core::assets {
struct Font;
}

namespace bb::core::gfx {

// A bitmap font: one sprite per Unicode code point, drawn left to right
// from UTF-8 text (the 2002 Fonte). A space advances by the space width;
// characters without a glyph are skipped. A null text draws nothing.
class BitmapFont {
public:
    // Replaces the glyphs by those of font. Every glyph gets the RGB555
    // colour key, whatever its layout (2002 code).
    void assign(const assets::Font& font);
    void clear() noexcept;
    bool loaded() const noexcept { return loaded_; }

    // Text starting at x (print), ending at x (printRight), centred on x
    // (printCentred).
    void print(Pixmap& dst, int x, int y, const char* text) const;
    void printRight(Pixmap& dst, int x, int y, const char* text) const;
    void printCentred(Pixmap& dst, int x, int y, const char* text) const;
    // Text that goes on the next line, back at x, before a glyph would end
    // at or past right (printM).
    void printWrapped(Pixmap& dst, int x, int y, const char* text, int right) const;
    // The same, word by word (printMW): a word goes on the next line when
    // it would end past right.
    void printWordWrapped(Pixmap& dst, int x, int y, const char* text, int right) const;
    // The width of text in pixels.
    int width(const char* text) const;
    // Whether the font has a glyph for the character (a space always prints).
    bool has(std::uint32_t codePoint) const { return codePoint == ' ' || glyph(codePoint) != nullptr; }

private:
    const Sprite* glyph(std::uint32_t codePoint) const;

    std::map<std::uint32_t, std::unique_ptr<Sprite>> glyphs_;
    int spaceWidth_ = 0;
    int lineHeight_ = 0;
    bool loaded_ = false;
};

}  // namespace bb::core::gfx
