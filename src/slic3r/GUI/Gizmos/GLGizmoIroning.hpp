#ifndef slic3r_GLGizmoIroning_hpp_
#define slic3r_GLGizmoIroning_hpp_

#include "GLGizmoPainterBase.hpp"

#include "slic3r/GUI/I18N.hpp"

#include <map>

namespace Slic3r::GUI {

class GLGizmoIroning : public GLGizmoPainterBase
{
public:
    GLGizmoIroning(GLCanvas3D& parent, const std::string& icon_filename, unsigned int sprite_id);

    void render_painter_gizmo() override;

    bool on_key_down_select_tool_type(int keyCode);

protected:
    void        on_render_input_window(float x, float y, float bottom_limit) override;
    std::string on_get_name() const override;

    void     on_set_state() override;
    void     render_tooltip_button(float x, float y);
    wxString handle_snapshot_action_name(bool shift_down, Button button_down) const override;

    std::string get_gizmo_entering_text() const override { return _u8L("Entering Paint-on ironing"); }
    std::string get_gizmo_leaving_text() const override { return _u8L("Leaving Paint-on ironing"); }
    std::string get_action_snapshot_name() const override { return _u8L("Paint-on ironing editing"); }

    wchar_t m_current_tool = 0;

private:
    bool on_init() override;

    void update_model_object() override;
    void update_from_model_object(bool first_update) override;
    void tool_changed(wchar_t old_tool, wchar_t new_tool);

    void             on_opening() override;
    void             on_shutdown() override;
    PainterGizmoType get_painter_type() const override;

    std::map<std::string, wxString> m_desc;

    std::vector<std::pair<wxString, wxString>> m_shortcuts_brush;
    std::vector<std::pair<wxString, wxString>> m_shortcuts_bucket_fill;
    std::vector<std::pair<wxString, wxString>> m_shortcuts_gap_fill;
};

} // namespace Slic3r::GUI

#endif // slic3r_GLGizmoIroning_hpp_
