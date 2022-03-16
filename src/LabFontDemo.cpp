

#include "LabFont.h"
#include <string>

static struct {
    LabFont* font_japanese = nullptr;
    LabFont* font_normal = nullptr;
    LabFont* font_italic = nullptr;
    LabFont* font_bold = nullptr;
    LabFont* font_cousine = nullptr;
    LabFont* font_robot18 = nullptr;
    float dpi_scale = 2.f;
} font_demo;

// this demo is not about abitrary drawing, it's only about drawing text
inline void line(float, float, float, float) {}

void font_demo_init(const char* path_)
{
    std::string path(path_);
    if (*path.rbegin() != '\\' && *path.rbegin() != '/')
        path += '/';

    static std::string dsj_path = path + "DroidSansJapanese.ttf";
    font_demo.font_japanese = LabFontLoad("sans-japanese", dsj_path.c_str(), 
            LabFontType{ LabFontTypeTTF });

    static std::string dsr_path = path + "DroidSerif-Regular.ttf";
    font_demo.font_normal = LabFontLoad("serif-normal", 
            dsr_path.c_str(), LabFontType{ LabFontTypeTTF });
    
    static std::string dsi_path = path + "DroidSerif-Italic.ttf";
    font_demo.font_italic = LabFontLoad("serif-italic", 
            dsi_path.c_str(), LabFontType{ LabFontTypeTTF });
    
    static std::string dsb_path = path + "DroidSerif-Bold.ttf";
    font_demo.font_bold = LabFontLoad("serif-bold", 
            dsb_path.c_str(), LabFontType{ LabFontTypeTTF });
    
    static std::string csr_path = path + "Cousine-Regular.ttf";
    font_demo.font_cousine = LabFontLoad("cousine-regular", 
            csr_path.c_str(), LabFontType{ LabFontTypeTTF });
    
    static std::string r18_path = path + "hauer-12.png";// "robot-18.png";
    font_demo.font_robot18 = LabFontLoad("robot-18", 
            r18_path.c_str(), LabFontType{ LabFontTypeQuadplay });    
}

void fontDemo(LabFontDrawState* ds, float& dx, float& dy, float sx, float sy) {

    if (font_demo.font_japanese == nullptr)
        return;

    float dpis = 1.f;//font_demo.dpi_scale;
    float sz;

    //-------------------------------------
    sz = 80.f * dpis;
    static LabFontState* a_st = LabFontStateBake(font_demo.font_normal, sz, { {255, 255, 255, 255} }, LabFontAlign{ 0 }, 0, 0);
    static LabFontState* b_st = LabFontStateBake(font_demo.font_italic, sz, { {128, 128, 0, 255} },   LabFontAlign{ 0 }, 0, 0);
    static LabFontState* c_st = LabFontStateBake(font_demo.font_italic, sz, { {255, 255, 255, 255} }, LabFontAlign{ 0 }, 0, 0);
    static LabFontState* d_st = LabFontStateBake(font_demo.font_bold,   sz, { {255, 255, 255, 255} }, LabFontAlign{ 0 }, 0, 0);
    dx = sx;
    dy += 30.f * dpis;
    dx = LabFontDraw(ds, "The quick ", dx, dy, a_st);
    dx = LabFontDraw(ds, "brown ", dx, dy, b_st);
    dx = LabFontDraw(ds, "fox ", dx, dy, a_st);
    dx = LabFontDraw(ds, "jumps over ", dx, dy, c_st);
    dx = LabFontDraw(ds, "the lazy ", dx, dy, c_st);
    dx = LabFontDraw(ds, "dog ", dx, dy, a_st);
    //-------------------------------------
    dx = sx;
    dy += sz * 1.2f;
    sz = 24.f * dpis;
    static LabFontState* e_st = LabFontStateBake(font_demo.font_normal, sz, { {0, 125, 255, 255} }, LabFontAlign{ 0 }, 0, 0);
    LabFontDraw(ds, "Now is the time for all good men to come to the aid of the party.", dx, dy, e_st);
    //-------------------------------------
    dx = sx;
    dy += sz * 1.2f * 2;
    sz = 18.f * dpis;
    static LabFontState* f_st = LabFontStateBake(font_demo.font_italic, sz, { {255, 255, 0, 255} }, LabFontAlign{ 0 }, 0, 0);
    LabFontDraw(ds, "Ég get etið gler án þess að meiða mig.", dx, dy, f_st);
    //-------------------------------------
    sz = 32.f * dpis;
    static LabFontState* j_st = LabFontStateBake(font_demo.font_japanese, sz, { {128, 128, 0, 255} }, LabFontAlign{ 0 }, 0, 0);
    dx = sx;
    dy += sz;
    LabFontDraw(ds, "いろはにほへと ちりぬるを わかよたれそ つねならむ うゐのおくやま けふこえて あさきゆめみし ゑひもせす　京（ん）", dx, dy, j_st);
    //-------------------------------------
    sz = 18.f * dpis;
    static LabFontState* p_st = LabFontStateBake(font_demo.font_normal, sz, { {255, 255, 255, 255} }, LabFontAlign{ LabFontAlignTop }, 0, 0);
    dx = 50 * dpis; dy = 350 * dpis;
    line(dx - 10 * dpis, dy, dx + 250 * dpis, dy);
    dx = LabFontDraw(ds, "Top", dx, dy, p_st);
    static LabFontState* g_st = LabFontStateBake(font_demo.font_normal, sz, { {255, 255, 255, 255} }, LabFontAlign{ LabFontAlignMiddle }, 0, 0);
    dx += 10 * dpis;
    dx = LabFontDraw(ds, "Middle", dx, dy, g_st);
    dx += 10 * dpis;
    static LabFontState* q_st = LabFontStateBake(font_demo.font_normal, sz, { {255, 255, 255, 255} }, LabFontAlign{ LabFontAlignBaseline }, 0, 0);
    dx = LabFontDraw(ds, "Baseline", dx, dy, q_st);
    dx += 10 * dpis;
    static LabFontState* h_st = LabFontStateBake(font_demo.font_normal, sz, { {255, 255, 255, 255} }, LabFontAlign{ LabFontAlignBottom }, 0, 0);
    LabFontDraw(ds, "Bottom", dx, dy, h_st);
    dx = 150 * dpis; dy = 400 * dpis;
    line(dx, dy - 30 * dpis, dx, dy + 80.0f * dpis);
    static LabFontState* i_st = LabFontStateBake(font_demo.font_normal, sz, { {255, 255, 255, 255} }, LabFontAlign{ LabFontAlignLeft | LabFontAlignBaseline }, 0, 0);
    static LabFontState* k_st = LabFontStateBake(font_demo.font_normal, sz, { {255, 255, 255, 255} }, LabFontAlign{ LabFontAlignCenter | LabFontAlignBaseline }, 0, 0);
    static LabFontState* l_st = LabFontStateBake(font_demo.font_normal, sz, { {255, 255, 255, 255} }, LabFontAlign{ LabFontAlignRight | LabFontAlignBaseline }, 0, 0);
    LabFontDraw(ds, "Left", dx, dy, i_st);
    dy += 30 * dpis;
    LabFontDraw(ds, "Center", dx, dy, k_st);
    dy += 30 * dpis;
    LabFontDraw(ds, "Right", dx, dy, l_st);
    //-------------------------------------
    sz = 18.f * dpis;
    static LabFontState* p2_st = LabFontStateBake(font_demo.font_robot18, sz, { {255, 255, 255, 255} }, LabFontAlign{ LabFontAlignTop }, 0, 0);
    dx = 250 * dpis;
    dy = 350 * dpis;
    line(dx - 10 * dpis, dy, dx + 250 * dpis, dy);
    dx = LabFontDraw(ds, "Top", dx, dy, p2_st);
    static LabFontState* g2_st = LabFontStateBake(font_demo.font_robot18, sz, { {255, 255, 255, 255} }, LabFontAlign{ LabFontAlignMiddle }, 0, 0);
    dx += 10 * dpis;
    dx = LabFontDraw(ds, "Middle", dx, dy, g2_st);
    dx += 10 * dpis;
    static LabFontState* q2_st = LabFontStateBake(font_demo.font_robot18, sz, { {255, 255, 255, 255} }, LabFontAlign{ LabFontAlignBaseline }, 0, 0);
    dx = LabFontDraw(ds, "Baseline", dx, dy, q2_st);
    dx += 10 * dpis;
    static LabFontState* h2_st = LabFontStateBake(font_demo.font_robot18, sz, { {255, 255, 255, 255} }, LabFontAlign{ LabFontAlignBottom }, 0, 0);
    LabFontDraw(ds, "Bottom", dx, dy, h2_st);
    dx = 362 * dpis; 
    dy = 380 * dpis;
    line(dx, dy - 30 * dpis, dx, dy + 80.0f * dpis);
    static LabFontState* i2_st = LabFontStateBake(font_demo.font_robot18, sz, { {255, 255, 255, 255} }, LabFontAlign{ LabFontAlignLeft | LabFontAlignBaseline }, 0, 0);
    static LabFontState* k2_st = LabFontStateBake(font_demo.font_robot18, sz, { {255, 255, 255, 255} }, LabFontAlign{ LabFontAlignCenter | LabFontAlignBaseline }, 0, 0);
    static LabFontState* l2_st = LabFontStateBake(font_demo.font_robot18, sz, { {255, 255, 255, 255} }, LabFontAlign{ LabFontAlignRight | LabFontAlignBaseline }, 0, 0);
    LabFontDraw(ds, "Left", dx, dy, i2_st);
    dy += 30 * dpis;
    LabFontDraw(ds, "Center", dx, dy, k2_st);
    dy += 30 * dpis;
    LabFontDraw(ds, "Right", dx, dy, l2_st);
    //-------------------------------------
    dx = 500 * dpis; dy = 350 * dpis;
    sz = 60.f * dpis;
    static LabFontState* m_st = LabFontStateBake(font_demo.font_italic, sz, { {255, 255, 255, 255} }, LabFontAlign{ LabFontAlignLeft | LabFontAlignBaseline }, 5.f * dpis, 10.f);
    LabFontDraw(ds, "Blurry...", dx, dy, m_st);
    //-------------------------------------
    dy += sz;
    sz = 18.f * dpis;
    static LabFontState* n_st = LabFontStateBake(font_demo.font_bold, sz, { {0,0,0, 255} }, LabFontAlign{ LabFontAlignLeft | LabFontAlignBaseline }, 0.f, 3.f);
    static LabFontState* o_st = LabFontStateBake(font_demo.font_bold, sz, { {255, 255, 255, 255} }, LabFontAlign{ LabFontAlignLeft | LabFontAlignBaseline }, 0.f, 0.f);
    LabFontDraw(ds, "DROP THAT SHADOW", dx+5*dpis, dy+5*dpis, n_st);
    LabFontDraw(ds, "DROP THAT SHADOW", dx, dy, o_st);
}
