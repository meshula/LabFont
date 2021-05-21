#pragma once
#include <zep/display.h>
#include <zep/syntax.h>
#include <zep/window.h>
#include <zep/mode_vim.h>
#include <zep/mode_standard.h>
#include <zep/regress.h>
#include <zep/tab_window.h>
#include "fontstash.h"
#include "zep/extensions/repl/mode_repl.h"

#include <string>

namespace Zep
{

inline Zep::NVec2f GetPixelScale() { return {1,1}; }
const float DemoFontPtSize = 14.0f;

class ZepFont_MeshulaFont : public ZepFont
{
public:
    ZepFont_MeshulaFont(ZepDisplay& display, FONScontext* context, int font_index, int pixelHeight)
        : ZepFont(display)
        , font_index(font_index)
        , context(context)
        , m_fontScale(1.f)
    {
        SetPixelHeight(pixelHeight);
    }

    virtual void SetPixelHeight(int pixelHeight) override
    {
        InvalidateCharCache();
        m_pixelHeight = pixelHeight;
    }

    virtual NVec2f GetTextSize(const uint8_t* pBegin, const uint8_t* pEnd = nullptr) const override
    {
        float bounds[4] = {0,0,0,0};

        auto scale = GetPixelScale();
        float fontSize = (float)GetPixelHeight() * scale.x;

        fonsSetFont(context, font_index);
        fonsSetSize(context, fontSize);
        fonsSetAlign(context, FONS_ALIGN_LEFT | FONS_ALIGN_BOTTOM);

        float text_width = fonsTextBounds(context, 0, 0, (const char*)pBegin, (const char*)pEnd, bounds);
        float text_height = bounds[3] - bounds[1];
        if (text_width == 0.f)
        {
            // Make invalid characters a default fixed_size
            const char chDefault = 'A';
            text_width = fonsTextBounds(context, 0, 0, &chDefault, &chDefault + 1, bounds);
            text_height = bounds[3] - bounds[1];
        }

        return { text_width * m_fontScale, text_height * m_fontScale };
    }
    
private:
    friend class ZepDisplay_MeshulaFont;
    FONScontext* context = nullptr;
    int font_index = -1;
    float m_fontScale = 1.0f;
};

class ZepDisplay_MeshulaFont : public ZepDisplay
{
public:
    ZepDisplay_MeshulaFont(const NVec2f& pixelScale)//,
                           //std::shared_ptr<ZepFont> ui,
                           //std::shared_ptr<ZepFont> text,
                           //std::shared_ptr<ZepFont> h1,
                           //std::shared_ptr<ZepFont> h2,
                           //std::shared_ptr<ZepFont> h3)
        : ZepDisplay(pixelScale)
    {
//        m_fonts[(int)ZepTextType::UI] = ui;
//        m_fonts[(int)ZepTextType::Text] = text;
//        m_fonts[(int)ZepTextType::Heading1] = h1;
//        m_fonts[(int)ZepTextType::Heading2] = h2;
//        m_fonts[(int)ZepTextType::Heading3] = h3;
    }

    void ApplyClip() const
    {
        sgl_push_scissor_rect();
        sgl_scissor_rect((int)m_clipRect.topLeftPx.x, (int)m_clipRect.topLeftPx.y,
            (int)m_clipRect.bottomRightPx.x - (int)m_clipRect.topLeftPx.x,
            (int)m_clipRect.bottomRightPx.y - (int)m_clipRect.topLeftPx.y, true);
    }

    void DrawChars(ZepFont& zfont, const NVec2f& pos, const NVec4f& col, const uint8_t* text_begin, const uint8_t* text_end) const override
    {
        auto font = static_cast<ZepFont_MeshulaFont&>(zfont);
        auto scale = GetPixelScale();
        float fontSize = (float)font.GetPixelHeight() * scale.x;

        fonsSetFont(font.context, font.font_index);
        fonsSetSize(font.context, fontSize);
        fonsSetColor(font.context, ToPackedABGR(col));
        fonsSetAlign(font.context, FONS_ALIGN_LEFT | FONS_ALIGN_BOTTOM);

        float line_height = 0;
        fonsVertMetrics(font.context, NULL, NULL, &line_height);
        line_height *= scale.y;

        bool need_clip = m_clipRect.Width() != 0;
        if (need_clip) 
            ApplyClip();

        fonsDrawText(font.context, pos.x, pos.y + line_height, (const char*)text_begin, (const char*)text_end);

        if (need_clip)
            sgl_pop_scissor_rect();
    }

    void DrawLine(const NVec2f& start, const NVec2f& end, const NVec4f& color, float width) const override
    {
        // Background rect for numbers
        bool need_clip = m_clipRect.Width() != 0;
        if (need_clip)
            ApplyClip();

        sgl_begin_lines();
        sgl_v2f_c4f(start.x, start.y, color.x, color.y, color.z, color.w);
        sgl_v2f_c4f(end.x, end.y, color.x, color.y, color.z, color.w);
        sgl_end();

        if (need_clip)
            sgl_pop_scissor_rect();
    }

    void DrawRectFilled(const NRectf& rc, const NVec4f& color) const override
    {
        // Background rect for numbers
        bool need_clip = m_clipRect.Width() != 0;
        if (need_clip)
            ApplyClip();

            sgl_begin_quads();
            sgl_v2f_c4f(rc.topLeftPx.x, rc.topLeftPx.y, color.x, color.y, color.z, color.w);
            sgl_v2f_c4f(rc.bottomRightPx.x, rc.topLeftPx.y, color.x, color.y, color.z, color.w);
            sgl_v2f_c4f(rc.bottomRightPx.x, rc.bottomRightPx.y, color.x, color.y, color.z, color.w);
            sgl_v2f_c4f(rc.topLeftPx.x, rc.bottomRightPx.y, color.x, color.y, color.z, color.w);
            sgl_end();

        if (need_clip)
            sgl_pop_scissor_rect();
    }

    virtual void SetClipRect(const NRectf& rc) override
    {
        m_clipRect = rc;
    }

    virtual ZepFont& GetFont(ZepTextType type) override
    {
        return *m_fonts[(int)type];
    }

private:
    NRectf m_clipRect;
}; // ZepDisplay_MeshulaFont


class ZepDisplay_ImGui;
class ZepTabWindow;
class ZepEditor_MeshulaFont : public ZepEditor
{
public:
    ZepEditor_MeshulaFont(const ZepPath& root, const NVec2f& pixelScale, uint32_t flags = 0, IZepFileSystem* pFileSystem = nullptr)
        : ZepEditor(new ZepDisplay_MeshulaFont(pixelScale), root, flags, pFileSystem)
    {
    }

    // returns true if the event was consumed
    bool HandleMouseInput(const ZepBuffer& buffer,
                     float mouse_delta_x, float mouse_delta_y,
                     float mouse_pos_x, float mouse_pos_y,
                     bool lmb_clicked, bool lmb_released,
                     bool rmb_clicked, bool rmb_released)
    {
        bool handled = false;

        if (mouse_delta_x != 0 || mouse_delta_y != 0)
        {
            OnMouseMove(NVec2f{mouse_pos_x, mouse_pos_y});
        }

        if (lmb_clicked)
        {
            if (OnMouseDown(NVec2f{mouse_pos_x, mouse_pos_y}, ZepMouseButton::Left))
            {
                // Hide the mouse click from imgui if we handled it
                handled = true;
            }
        }

        if (rmb_clicked)
        {
            if (OnMouseDown(NVec2f{mouse_pos_x, mouse_pos_y}, ZepMouseButton::Right))
            {
                // Hide the mouse click from imgui if we handled it
                handled = true;
            }
        }

        if (lmb_released)
        {
            if (OnMouseUp(NVec2f{mouse_pos_x, mouse_pos_y}, ZepMouseButton::Left))
            {
                // Hide the mouse click from imgui if we handled it
                handled = true;
            }
        }

        if (rmb_released)
        {
            if (OnMouseUp(NVec2f{mouse_pos_x, mouse_pos_y}, ZepMouseButton::Right))
            {
                // Hide the mouse click from imgui if we handled it
                handled = true;
            }
        }
        
        return handled;
    }

    // returns true if the event was consumed
    bool HandleKeyInput(const ZepBuffer& buffer,
        uint32_t key, bool ctrl, bool shift)
    {
        bool handled = false;
        uint32_t mod = (ctrl ? Zep::ModifierKey::Ctrl : 0) |
                       (shift ? Zep::ModifierKey::Shift : 0);

        switch (key)
        {
        case Zep::ExtKeys::TAB:
        case Zep::ExtKeys::ESCAPE:
        case Zep::ExtKeys::RETURN:
        case Zep::ExtKeys::DEL:
        case Zep::ExtKeys::HOME:
        case Zep::ExtKeys::END:
        case Zep::ExtKeys::BACKSPACE:
        case Zep::ExtKeys::RIGHT:
        case Zep::ExtKeys::LEFT:
        case Zep::ExtKeys::UP:
        case Zep::ExtKeys::DOWN:
        case Zep::ExtKeys::PAGEDOWN:
        case Zep::ExtKeys::PAGEUP:
        case Zep::ExtKeys::F1:
        case Zep::ExtKeys::F2:
        case Zep::ExtKeys::F3:
        case Zep::ExtKeys::F4:
        case Zep::ExtKeys::F5:
        case Zep::ExtKeys::F6:
        case Zep::ExtKeys::F7:
        case Zep::ExtKeys::F8:
        case Zep::ExtKeys::F9:
        case Zep::ExtKeys::F10:
        case Zep::ExtKeys::F11:
        case Zep::ExtKeys::F12:
            buffer.GetMode()->AddKeyPress(key, mod);
            return true;
        }

        if (ctrl)
        {
            if (key == '1')
            {
                SetGlobalMode(ZepMode_Standard::StaticName());
                handled = true;
            }
            else if (key == '2')
            {
                SetGlobalMode(ZepMode_Vim::StaticName());
                handled = true;
            }
            else if (key >= 'A' && key <= 'Z')
            {
                buffer.GetMode()->AddKeyPress(key - 'A' + 'a', mod);
                handled = true;
            }
            else if (key == ' ')
            {
                buffer.GetMode()->AddKeyPress(' ', mod);
                handled = true;
            }
        }

        if (!handled)
        {
            handled = true;
            if (key != '\r')
            {
                buffer.GetMode()->AddKeyPress(key, mod);
            }
        }

        return handled;
    }
private:
};

// A helper struct to init the editor and handle callbacks
struct ZepContainer_MeshulaFont : public IZepComponent, public IZepReplProvider
{
    ZepContainer_MeshulaFont(const std::string& startupFilePath, const std::string& configPath, 
        FONScontext* fs, int ui_font, int fontPixelHeight)
        : spEditor(std::make_unique<ZepEditor_MeshulaFont>(configPath, GetPixelScale()))
        //, fileWatcher(spEditor->GetFileSystem().GetConfigPath(), std::chrono::seconds(2))
    {
//        chibi_init(scheme, SDL_GetBasePath());

        auto& display = static_cast<ZepDisplay_MeshulaFont&>(spEditor->GetDisplay());

        display.SetFont(ZepTextType::UI, std::make_shared<ZepFont_MeshulaFont>(display, fs, ui_font, fontPixelHeight));
        display.SetFont(ZepTextType::Text, std::make_shared<ZepFont_MeshulaFont>(display, fs, ui_font, fontPixelHeight));
        display.SetFont(ZepTextType::Heading1, std::make_shared<ZepFont_MeshulaFont>(display, fs, ui_font, int(fontPixelHeight * 1.75)));
        display.SetFont(ZepTextType::Heading2, std::make_shared<ZepFont_MeshulaFont>(display, fs, ui_font, int(fontPixelHeight * 1.5)));
        display.SetFont(ZepTextType::Heading3, std::make_shared<ZepFont_MeshulaFont>(display, fs, ui_font, int(fontPixelHeight * 1.25)));

        spEditor->RegisterCallback(this);

       // ZepMode_Orca::Register(*spEditor);

        ZepRegressExCommand::Register(*spEditor);

        // Repl
        ZepReplExCommand::Register(*spEditor, this);
        ZepReplEvaluateOuterCommand::Register(*spEditor, this);
        ZepReplEvaluateInnerCommand::Register(*spEditor, this);
        ZepReplEvaluateCommand::Register(*spEditor, this);

        if (!startupFilePath.empty())
        {
            spEditor->InitWithFileOrDir(startupFilePath);
        }
        else
        {
            spEditor->InitWithText("README.md", "Hello World\nThis a test\nA third line of text that is very long and I wonder if it will wrap and oh hey look at this it is pretty long, but is it long enough? Not sure\n");
        }

        // File watcher not used on apple yet ; needs investigating as to why it doesn't compile/run
        // The watcher is being used currently to update the config path, but clients may want to do more interesting things
        // by setting up watches for the current dir, etc.
        /*fileWatcher.start([=](std::string path, FileStatus status) {
            if (spEditor)
            {
                ZLOG(DBG, "Config File Change: " << path);
                spEditor->OnFileChanged(spEditor->GetFileSystem().GetConfigPath() / path);
            }
        });*/
    }

    ~ZepContainer_MeshulaFont()
    {
    }

    void Destroy()
    {
        spEditor->UnRegisterCallback(this);
        spEditor.reset();
    }

    virtual std::string ReplParse(ZepBuffer& buffer, const GlyphIterator& cursorOffset, ReplParseType type) override
    {
        ZEP_UNUSED(cursorOffset);
        ZEP_UNUSED(type);

        GlyphRange range;
        if (type == ReplParseType::OuterExpression)
        {
            range = buffer.GetExpression(ExpressionType::Outer, cursorOffset, { '(' }, { ')' });
        }
        else if (type == ReplParseType::SubExpression)
        {
            range = buffer.GetExpression(ExpressionType::Inner, cursorOffset, { '(' }, { ')' });
        }
        else
        {
            range = GlyphRange(buffer.Begin(), buffer.End());
        }

        if (range.first >= range.second)
            return "<No Expression>";

        const auto& text = buffer.GetWorkingBuffer();
        auto eval = std::string(text.begin() + range.first.Index(), text.begin() + range.second.Index());

        // Flash the evaluated expression
        FlashType flashType = FlashType::Flash;
        float time = 1.0f;
        buffer.BeginFlash(time, flashType, range);

        std::string ret = "OK";//chibi_repl(scheme, NULL, eval);
        ret = RTrim(ret);

        GetEditor().SetCommandText(ret);
        return ret;
    }

    virtual std::string ReplParse(const std::string& str) override
    {
        std::string ret = "OK";//chibi_repl(scheme, NULL, str);
        ret = RTrim(ret);
        return ret;
    }

    virtual bool ReplIsFormComplete(const std::string& str, int& indent) override
    {
        int count = 0;
        for (auto& ch : str)
        {
            if (ch == '(')
                count++;
            if (ch == ')')
                count--;
        }

        if (count < 0)
        {
            indent = -1;
            return false;
        }
        else if (count == 0)
        {
            return true;
        }

        int count2 = 0;
        indent = 1;
        for (auto& ch : str)
        {
            if (ch == '(')
                count2++;
            if (ch == ')')
                count2--;
            if (count2 == count)
            {
                break;
            }
            indent++;
        }
        return false;
    }

    // Inherited via IZepComponent
    virtual void Notify(std::shared_ptr<ZepMessage> message) override
    {
        if (message->messageId == Msg::GetClipBoard)
        {
            //clip::get_text(message->str);
            message->handled = true;
        }
        else if (message->messageId == Msg::SetClipBoard)
        {
            //clip::set_text(message->str);
            message->handled = true;
        }
        else if (message->messageId == Msg::RequestQuit)
        {
            quit = true;
        }
        else if (message->messageId == Msg::ToolTip)
        {
            auto spTipMsg = std::static_pointer_cast<ToolTipMessage>(message);
            if (spTipMsg->location.Valid() && spTipMsg->pBuffer)
            {
                auto pSyntax = spTipMsg->pBuffer->GetSyntax();
                if (pSyntax)
                {
                    if (pSyntax->GetSyntaxAt(spTipMsg->location).foreground == ThemeColor::Identifier)
                    {
                        auto spMarker = std::make_shared<RangeMarker>(*spTipMsg->pBuffer);
                        spMarker->SetDescription("This is an identifier");
                        spMarker->SetHighlightColor(ThemeColor::Identifier);
                        spMarker->SetTextColor(ThemeColor::Text);
                        spTipMsg->spMarker = spMarker;
                        spTipMsg->handled = true;
                    }
                    else if (pSyntax->GetSyntaxAt(spTipMsg->location).foreground == ThemeColor::Keyword)
                    {
                        auto spMarker = std::make_shared<RangeMarker>(*spTipMsg->pBuffer);
                        spMarker->SetDescription("This is a keyword");
                        spMarker->SetHighlightColor(ThemeColor::Keyword);
                        spMarker->SetTextColor(ThemeColor::Text);
                        spTipMsg->spMarker = spMarker;
                        spTipMsg->handled = true;
                    }
                }
            }
        }
    }

    virtual ZepEditor& GetEditor() const override
    {
        return *spEditor;
    }

    bool quit = false;
    std::unique_ptr<ZepEditor_MeshulaFont> spEditor;
    //FileWatcher fileWatcher;
};


} // namespace Zep
