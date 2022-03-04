
#include "../LabZep.h"

#ifdef LABFONT_HAVE_SOKOL
#include "../LabSokol.h"
#endif

#include "../LabFont.h"

#include <zep/display.h>
#include <zep/syntax.h>
#include <zep/window.h>
#include <zep/mode_vim.h>
#include <zep/mode_standard.h>
#include <zep/regress.h>
#include <zep/syntax_providers.h>
#include <zep/tab_window.h>
#include <zep/extensions/repl/mode_repl.h>

#include <string>
#include <vector>

namespace
{

    using namespace Zep;
    using std::vector;

    inline Zep::NVec2f GetPixelScale() { return { 1,1 }; }
    const float DemoFontPtSize = 14.0f;

    class LabVimFont : public ZepFont
    {
    public:
        LabVimFont(ZepDisplay& display, LabFontState* font)
            : ZepFont(display)
            , font(font)
            , m_fontScale(1.f)
        {
            LabFontSize sz = LabFontMeasure("", font);
            SetPixelHeight(sz.height);
        }

        virtual void SetPixelHeight(int pixelHeight) override
        {
            InvalidateCharCache();
            m_pixelHeight = pixelHeight;
        }

        virtual NVec2f GetTextSize(const uint8_t* pBegin, const uint8_t* pEnd = nullptr) const override
        {
            LabFontSize sz = LabFontMeasureSubstring((const char*) pBegin, (const char*) pEnd, font);
            return { sz.width, sz.height };
        }

    private:
        friend class LabZepRender;
        LabFontState* font = nullptr;
        float m_fontScale = 1.0f;
    };

    class LabZepRender : public ZepDisplay
    {
    public:
        LabZepRender(const NVec2f& pixelScale)//,
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
#ifdef LABFONT_HAVE_SOKOL
            sgl_scissor_rect((int)_clipRect.topLeftPx.x, (int)_clipRect.topLeftPx.y,
                (int)_clipRect.bottomRightPx.x - (int)_clipRect.topLeftPx.x,
                (int)_clipRect.bottomRightPx.y - (int)_clipRect.topLeftPx.y, true);
#endif
            _clipStack.push_back(_clipRect);
        }
        void RestoreClip() const
        {
            NRectf rect = { 0, 0, 10000, 10000 };
            if (_clipStack.size()) {
                _clipStack.pop_back();
                if (_clipStack.size()) {
                    rect =_clipStack.back();
                    _clipStack.pop_back();
                }
            }
#ifdef LABFONT_HAVE_SOKOL
            sgl_scissor_rect((int) rect.topLeftPx.x, (int) rect.topLeftPx.y,
                    (int) rect.bottomRightPx.x - (int) rect.topLeftPx.x,
                    (int) rect.bottomRightPx.y - (int) rect.topLeftPx.y, true);
#endif
        }

        void DrawChars(ZepFont& zfont, const NVec2f& pos, const NVec4f& color, 
                const uint8_t* text_begin, const uint8_t* text_end) const override
        {
            auto font = static_cast<LabVimFont&>(zfont);

            bool need_clip = _clipRect.Width() != 0;
            if (need_clip)
                ApplyClip();

            if (text_end == nullptr)
                text_end = text_begin + strlen((const char*) text_begin);

            LabFontColor c;
            c.rgba[0] = uint8_t(color.x * 255);
            c.rgba[1] = uint8_t(color.y * 255);
            c.rgba[2] = uint8_t(color.z * 255);
            c.rgba[3] = uint8_t(color.w * 255);
            LabFontDrawSubstringColor(ds, 
                    (const char*)text_begin, (const char*)text_end, &c, 
                    pos.x, pos.y, font.font);

            if (need_clip)
                RestoreClip();
        }

        void DrawLine(const NVec2f& start, const NVec2f& end, const NVec4f& color, float width) const override
        {
            // Background rect for numbers
            bool need_clip = _clipRect.Width() != 0;
            if (need_clip)
                ApplyClip();

#ifdef LABFONT_HAVE_SOKOL
            sgl_begin_lines();
            sgl_v2f_c4f(start.x, start.y, color.x, color.y, color.z, color.w);
            sgl_v2f_c4f(end.x, end.y, color.x, color.y, color.z, color.w);
            sgl_end();
#endif
            
            if (need_clip)
                RestoreClip();
        }

        void DrawRectFilled(const NRectf& rc, const NVec4f& color) const override
        {
            // Background rect for numbers
            bool need_clip = _clipRect.Width() != 0;
            if (need_clip)
                ApplyClip();

#ifdef LABFONT_HAVE_SOKOL
            sgl_begin_quads();
            sgl_v2f_c4f(rc.topLeftPx.x, rc.topLeftPx.y, color.x, color.y, color.z, color.w);
            sgl_v2f_c4f(rc.bottomRightPx.x, rc.topLeftPx.y, color.x, color.y, color.z, color.w);
            sgl_v2f_c4f(rc.bottomRightPx.x, rc.bottomRightPx.y, color.x, color.y, color.z, color.w);
            sgl_v2f_c4f(rc.topLeftPx.x, rc.bottomRightPx.y, color.x, color.y, color.z, color.w);
            sgl_end();
#endif
            
            if (need_clip)
                RestoreClip();
        }

        void SetRestoreClipRectix(const NRectf& rc)
        {
           // _restoreClipRect = rc;
        }

        virtual void SetClipRect(const NRectf& rc) override
        {
            _clipRect = rc;
        }

        virtual ZepFont& GetFont(ZepTextType type) override
        {
            return *m_fonts[(int)type];
        }
        
        LabFontDrawState* ds = nullptr;

    private:
        NRectf _clipRect;
        mutable vector<NRectf> _clipStack;
    };


    //class ZepDisplay_ImGui;
    //class ZepTabWindow;
    class LabZepEventHandler : public ZepEditor
    {
    public:
        LabZepEventHandler(
            const ZepPath& root, const NVec2f& pixelScale,
            LabZepRender* renderer,
            uint32_t flags = 0, IZepFileSystem* pFileSystem = nullptr)
            : ZepEditor(renderer, root, flags, pFileSystem)
        {
        }

        // returns true if the event was consumed
        bool HandleMouseInput(const ZepBuffer& buffer,
            float mouse_pos_x, float mouse_pos_y,
            bool lmb_clicked, bool lmb_released,
            bool rmb_clicked, bool rmb_released)
        {
            bool handled = false;

            static float prev_x = 0;
            static float prev_y = 0;

            if (mouse_pos_x != prev_x || mouse_pos_y != prev_y)
            {
                prev_x = mouse_pos_x;
                prev_y = mouse_pos_y;
                OnMouseMove(NVec2f{ mouse_pos_x, mouse_pos_y });
            }

            if (lmb_clicked)
            {
                if (OnMouseDown(NVec2f{ mouse_pos_x, mouse_pos_y }, ZepMouseButton::Left))
                {
                    // Hide the mouse click from imgui if we handled it
                    handled = true;
                }
            }

            if (rmb_clicked)
            {
                if (OnMouseDown(NVec2f{ mouse_pos_x, mouse_pos_y }, ZepMouseButton::Right))
                {
                    // Hide the mouse click from imgui if we handled it
                    handled = true;
                }
            }

            if (lmb_released)
            {
                if (OnMouseUp(NVec2f{ mouse_pos_x, mouse_pos_y }, ZepMouseButton::Left))
                {
                    // Hide the mouse click from imgui if we handled it
                    handled = true;
                }
            }

            if (rmb_released)
            {
                if (OnMouseUp(NVec2f{ mouse_pos_x, mouse_pos_y }, ZepMouseButton::Right))
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
    struct LabZepDetail : public IZepComponent, public IZepReplProvider
    {
        LabZepRender* renderer = nullptr;

        LabZepDetail(const std::string& startupFilePath, const std::string& configPath,
            LabFontState* font, const char* initialText)
            //, fileWatcher(spEditor->GetFileSystem().GetConfigPath(), std::chrono::seconds(2))
        {
            //        chibi_init(scheme, SDL_GetBasePath());

            renderer = new LabZepRender(GetPixelScale());
            spEditor = std::make_unique<LabZepEventHandler>(configPath, GetPixelScale(), renderer);

            auto& display = static_cast<LabZepRender&>(spEditor->GetDisplay());

            display.SetFont(ZepTextType::UI, std::make_shared<LabVimFont>(display, font));
            display.SetFont(ZepTextType::Text, std::make_shared<LabVimFont>(display, font));
            display.SetFont(ZepTextType::Heading1, std::make_shared<LabVimFont>(display, font));
            display.SetFont(ZepTextType::Heading2, std::make_shared<LabVimFont>(display, font));
            display.SetFont(ZepTextType::Heading3, std::make_shared<LabVimFont>(display, font));

            spEditor->RegisterCallback(this);

            // ZepMode_Orca::Register(*spEditor);

            ZepRegressExCommand::Register(*spEditor);

            // Repl
            ZepReplExCommand::Register(*spEditor, this);
            ZepReplEvaluateOuterCommand::Register(*spEditor, this);
            ZepReplEvaluateInnerCommand::Register(*spEditor, this);
            ZepReplEvaluateCommand::Register(*spEditor, this);

            spEditor->SetGlobalMode(ZepMode_Vim::StaticName());
            spEditor->GetConfig().autoHideCommandRegion = false;
            Zep::RegisterSyntaxProviders(*spEditor);

            ZepBuffer* newBuffer = nullptr;
            if (!startupFilePath.empty())
            {
                newBuffer = spEditor->InitWithFileOrDir(startupFilePath);
            }
            else
            {
                if (initialText)
                    newBuffer = spEditor->InitWithText("Shader.glsl", initialText);
                else
                    newBuffer = spEditor->InitWithText("README.md", "Hello World\nThis a test\nA third line of text that is very long and I wonder if it will wrap and oh hey look at this it is pretty long, but is it long enough? Not sure\n");
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

        ~LabZepDetail()
        {
        }

        void Destroy()
        {
            spEditor->UnRegisterCallback(this);
            spEditor.reset();
            delete renderer;
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
        std::unique_ptr<LabZepEventHandler> spEditor;
        //FileWatcher fileWatcher;
    };


} // anon

extern "C" struct LabZep
{
    LabZepDetail* zep;
};

extern "C" LabZep * LabZep_create(
    LabFontState* font,
    const char* filename, const char* text)
{
    LabZep* zp = new LabZep();
    std::string startupFilePath;
    std::string configPath;
    zp->zep = new LabZepDetail(startupFilePath, configPath, font, text);
    zp->zep->GetEditor().SetGlobalMode(Zep::ZepMode_Vim::StaticName());
    zp->zep->GetEditor().GetTheme().SetThemeType(Zep::ThemeType::Dark);

    // Display the editor inside this window
    Zep::ZepBuffer& buffer = zp->zep->spEditor->GetActiveTabWindow()->GetActiveWindow()->GetBuffer();
    buffer.SetFilePath(filename);
    zp->zep->spEditor->SetBufferSyntax(buffer);
    zp->zep->spEditor->Broadcast(std::make_shared<Zep::BufferMessage>(&buffer, Zep::BufferMessageType::TextAdded, buffer.Begin(), buffer.End()));
    return zp;
}

extern "C" void LabZep_free(LabZep * zp)
{
    if (zp)
    {
        delete zp->zep;
        delete zp;
    }
}

static struct {
    float mouse_delta_x; float mouse_delta_y;
    float mouse_pos_x; float mouse_pos_y;
    bool lmb_clicked; bool lmb_released;
    bool rmb_clicked; bool rmb_released;

    std::deque<uint32_t> keys;
} zi;

static uint32_t sappZepKey(int c)
{
#ifdef LABFONT_HAVE_SOKOL
    switch (c)
    {
    case SAPP_KEYCODE_TAB: return Zep::ExtKeys::TAB;
    case SAPP_KEYCODE_ESCAPE: return Zep::ExtKeys::ESCAPE;
    case SAPP_KEYCODE_ENTER: return Zep::ExtKeys::RETURN;
    case SAPP_KEYCODE_DELETE: return Zep::ExtKeys::DEL;
    case SAPP_KEYCODE_HOME: return Zep::ExtKeys::HOME;
    case SAPP_KEYCODE_END: return Zep::ExtKeys::END;
    case SAPP_KEYCODE_BACKSPACE: return Zep::ExtKeys::BACKSPACE;
    case SAPP_KEYCODE_RIGHT: return Zep::ExtKeys::RIGHT;
    case SAPP_KEYCODE_LEFT: return Zep::ExtKeys::LEFT;
    case SAPP_KEYCODE_UP: return Zep::ExtKeys::UP;
    case SAPP_KEYCODE_DOWN: return Zep::ExtKeys::DOWN;
    case SAPP_KEYCODE_PAGE_DOWN: return Zep::ExtKeys::PAGEDOWN;
    case SAPP_KEYCODE_PAGE_UP: return Zep::ExtKeys::PAGEUP;
    case SAPP_KEYCODE_F1: return Zep::ExtKeys::F1;
    case SAPP_KEYCODE_F2: return Zep::ExtKeys::F2;
    case SAPP_KEYCODE_F3: return Zep::ExtKeys::F3;
    case SAPP_KEYCODE_F4: return Zep::ExtKeys::F4;
    case SAPP_KEYCODE_F5: return Zep::ExtKeys::F5;
    case SAPP_KEYCODE_F6: return Zep::ExtKeys::F6;
    case SAPP_KEYCODE_F7: return Zep::ExtKeys::F7;
    case SAPP_KEYCODE_F8: return Zep::ExtKeys::F8;
    case SAPP_KEYCODE_F9: return Zep::ExtKeys::F9;
    case SAPP_KEYCODE_F10: return Zep::ExtKeys::F10;
    case SAPP_KEYCODE_F11: return Zep::ExtKeys::F11;
    case SAPP_KEYCODE_F12: return Zep::ExtKeys::F12;
    default: return c;
    }
#else
    return c;
#endif
}


extern "C" void LabZep_input_sokol_key(LabZep*, int sapp_key, bool shift, bool ctrl)
{
    uint32_t test_key = sappZepKey(sapp_key);
    if (test_key < 32) {
        uint32_t key = shift ? Zep::ModifierKey::Shift : (ctrl ? Zep::ModifierKey::Ctrl : 0);
        zi.keys.push_back(key | test_key);
    }
    else {
        zi.keys.push_back(sapp_key);
    }
}

extern "C" void LabZep_render(LabZep* zep)
{
    if (zep) { 
        zep->zep->spEditor->Display();
        zep->zep->renderer->RestoreClip();
    }
}

extern "C" void LabZep_process_events(LabZep* zep, 
    float mouse_x, float mouse_y,
    bool lmb_clicked, bool lmb_released, bool rmb_clicked, bool rmb_released)
{
    if (!zep) {
        zi.keys.clear();
        return;
    }

    // Display the editor inside this window
    const Zep::ZepBuffer& buffer = zep->zep->spEditor->GetActiveTabWindow()->GetActiveWindow()->GetBuffer();

    zep->zep->spEditor->HandleMouseInput(buffer,
        mouse_x, mouse_y, lmb_clicked, lmb_released,
        rmb_clicked, rmb_released);

    while (!zi.keys.empty()) {
        uint32_t key = zi.keys.front();
        zi.keys.pop_front();

        const Zep::ZepBuffer& buffer = zep->zep->spEditor->GetActiveTabWindow()->GetActiveWindow()->GetBuffer();
        //printf("Key: %d %c\n", key, key & 0xff);
        zep->zep->spEditor->HandleKeyInput(buffer,
            key & 0xff,
            key & (Zep::ModifierKey::Ctrl << 8),
            key & (Zep::ModifierKey::Shift << 8));
    }
}

extern "C" void LabZep_position_editor(LabZep * zep,
    int x, int y, int w, int h)
{
  //  zep->zep->renderer->SetRestoreClipRect({ (float)x, (float)y, (float)w, (float)h });
    zep->zep->spEditor->SetDisplayRegion({ (float)x, (float)y }, {(float)x + w, (float)y + h });
}

extern "C" bool LabZep_update_required(LabZep * zep)
{
    // if there was nothing else going on, this could be used to skip
    // rendering - it would only be true when the cursor blink needs updating
    if (!zep)
        return false;

    return zep->zep->spEditor->RefreshRequired();
}

extern "C" void LabZep_open(LabZep * zep, const char* filename)
{
    if (!zep)
        return;

    auto pBuffer = zep->zep->spEditor->GetFileBuffer(filename);
    zep->zep->spEditor->GetActiveTabWindow()->GetActiveWindow()->SetBuffer(pBuffer);
}
