
// nb: This file is encoded in utf-8.

#include <string>
#include <array>
#include <map>


std::wstring fontChars = L"\
ABCDEFGHIJKLMNOPQRSTUVWXYZ↑↓;:,.\
abcdefghijklmnopqrstuvwxyz←→<>◀▶\
0123456789+-()~!@#$%^&*_=?¥€£¬∩∪\
⁰¹²³⁴⁵⁶⁷⁸⁹⁺⁻⁽⁾ᵃᵝⁱʲˣᵏᵘⁿ≟≠≤≥≈{}[]★\
ᵈᵉʰᵐᵒʳˢᵗⓌⒶⓈⒹⒾⒿⓀⓁ⍐⍇⍗⍈ⓛⓡ▼∈∞°¼½¾⅓⅔⅕\
«»ΓΔмнкΘ¿¡Λ⊢∙Ξ×ΠİΣ♆ℵΦ©ΨΩ∅ŞĞ\\/|`'\
αβγδεζηθικλμνξ§πρστυϕχψωςşğ⌊⌋⌈⌉\"\
ÆÀÁÂÃÄÅÇÈÉÊËÌÍÎÏØÒÓÔÕÖŒÑẞÙÚÛÜБ✓Д\
æàáâãäåçèéêëìíîïøòóôõöœñßùúûüбгд\
ЖЗИЙЛПЦЧШЩЭЮЯЪЫЬ±⊗↖↗втⓔ⦸ⓝⓗ○●◻◼△▲\
жзийлпцчшщэюяъыь∫❖↙↘…‖ʸᶻⓩ⑤⑥♠♥♣♦✜\
ⓐⓑⓒⓓⓕⓖⓟⓠⓥⓧⓨ⬙⬗⬖⬘Ⓞ⍍▣⧉☰⒧⒭①②③④⑦⑧⑨⓪⊖⊕\
␣Ɛ⏎ҕﯼડƠ⇥\
⬁⬀⌥     \
";


std::array<int, 256> build_quadplay_font_map()
{
    std::map<wchar_t, int> qp_wchar_map;
    int index = 0;
    char32_t max_code_point = 0;
    for (wchar_t c : fontChars) {
        qp_wchar_map[c] = index;
        ++index;
    }

    std::array<int, 256> qp_ascii_map;
    for (int i = 0; i < 255; ++i)      // 0-127 ascii, 128-255 latin1 which map straight on to wchar
    {
        auto idx = qp_wchar_map.find(wchar_t(i));
        qp_ascii_map[i] = idx == qp_wchar_map.end()? 0 : idx->second;
    }

    qp_ascii_map[32] = 32 * 14 - 1;

    return qp_ascii_map;
}
