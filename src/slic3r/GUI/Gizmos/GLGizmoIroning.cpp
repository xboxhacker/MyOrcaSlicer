#include "GLGizmoIroning.hpp"

#include "libslic3r/Model.hpp"
#include "libslic3r/Print.hpp"
#include "libslic3r/PresetBundle.hpp"

#include "slic3r/GUI/GLCanvas3D.hpp"
#include "slic3r/GUI/GUI_App.hpp"
#include "slic3r/GUI/GUI_ObjectList.hpp"
#include "slic3r/GUI/GUI.hpp"
#include "slic3r/GUI/ImGuiWrapper.hpp"
#include "slic3r/GUI/Plater.hpp"
#include "slic3r/GUI/format.hpp"
#include "slic3r/GUI/3DScene.hpp"
#include "slic3r/Utils/UndoRedo.hpp"
#include "GLGizmoUtils.hpp"

#include <glad/gl.h>
#include <algorithm>
#include <array>
#include <string>

namespace Slic3r::GUI {

GLGizmoIroning::GLGizmoIroning(GLCanvas3D& parent, const std::string& icon_filename, unsigned int sprite_id)
    : GLGizmoPainterBase(parent, icon_filename, sprite_id), m_current_tool(ImGui::CircleButtonIcon)
{
}

void GLGizmoIroning::on_opening()
{
    m_paint_on_overhangs_only = false;
    m_highlight_by_angle_threshold_deg = 0.f;
    m_parent.use_slope(false);
}

void GLGizmoIroning::on_shutdown()
{
    m_parent.use_slope(false);
    m_parent.toggle_model_objects_visibility(true);
}

std::string GLGizmoIroning::on_get_name() const
{
    return _u8L("Paint-on ironing");
}

bool GLGizmoIroning::on_init()
{
    m_shortcut_key = WXK_CONTROL_I;

    m_desc["perform"]            = _L("Perform");
    m_desc["remove_all"]         = _L("Erase all");
    m_desc["tool_type"]          = _L("Tool type");
    m_desc["gap_fill"]           = _L("Gap fill");
    m_desc["cursor_size"]        = _L("Brush size");
    m_desc["smart_fill_angle"]   = _L("Smart fill angle");
    m_desc["gap_area"]           = _L("Gap area");

    const wxString ctrl  = GUI::shortkey_ctrl_prefix();
    const wxString shift = GUI::shortkey_shift_prefix();

    std::pair<wxString, wxString> enforce_shortcut  = {_L("Left mouse button"),         _L("Paint ironing")};
    std::pair<wxString, wxString> block_shortcut    = {_L("Right mouse button"),        _L("Block ironing")};
    std::pair<wxString, wxString> remove_shortcut   = {shift + _L("Left mouse button"), _L("Erase")};

    m_shortcuts_brush = {
        enforce_shortcut,
        block_shortcut,
        remove_shortcut,
        {ctrl + _L("Mouse wheel"), m_desc["cursor_size"]}
    };

    m_shortcuts_bucket_fill = {
        enforce_shortcut,
        block_shortcut,
        remove_shortcut,
        {ctrl + _L("Mouse wheel"),  m_desc["smart_fill_angle"]}
    };

    m_shortcuts_gap_fill = {
        {ctrl + _L("Mouse wheel"), _L("Gap area")}
    };

    return true;
}

void GLGizmoIroning::render_painter_gizmo()
{
    const Selection& selection = m_parent.get_selection();

    glsafe(::glEnable(GL_BLEND));
    glsafe(::glEnable(GL_DEPTH_TEST));

    render_triangles(selection);
    m_c->object_clipper()->render_cut();
    m_c->instances_hider()->render_cut();
    render_cursor();

    glsafe(::glDisable(GL_BLEND));
}

bool GLGizmoIroning::on_key_down_select_tool_type(int keyCode)
{
    switch (keyCode)
    {
    case 'F':
        m_current_tool = ImGui::FillButtonIcon;
        break;
    case 'S':
        m_current_tool = ImGui::SphereButtonIcon;
        break;
    case 'C':
        m_current_tool = ImGui::CircleButtonIcon;
        break;
    case 'G':
        m_current_tool = ImGui::GapFillIcon;
        break;
    default:
        return false;
    }
    return true;
}

void GLGizmoIroning::on_set_state()
{
    GLGizmoPainterBase::on_set_state();

    if (get_state() == Off) {
        ModelObject* mo = m_c->selection_info()->model_object();
        if (mo) Slic3r::save_object_mesh(*mo);
    }
}

void GLGizmoIroning::on_render_input_window(float x, float y, float bottom_limit)
{
    ModelObject *mo = m_c->selection_info()->model_object();
    if (!mo)
        return;

    float  scale       = m_parent.get_scale();
#ifdef WIN32
    int dpi = get_dpi_for_window(wxGetApp().GetTopWindow());
    scale *= (float) dpi / (float) DPI_DEFAULT;
#endif

    wchar_t old_tool = m_current_tool;

    const float approx_height = m_imgui->scaled(23.f);
    y = std::min(y, bottom_limit - approx_height);

    GizmoImguiSetNextWIndowPos(x, y, ImGuiCond_Always, 0.0f, 0.0f);

    ImGuiWrapper::push_toolbar_style(m_parent.get_scale());
    float f_scale = m_parent.get_gizmos_manager().get_layout_scale();
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6.0f, 4.0f * f_scale));

    GizmoImguiBegin(get_name(), ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar);

    const float space_size = m_imgui->get_style_scaling() * 8;
    const float cursor_slider_left      = m_imgui->calc_text_size(m_desc.at("cursor_size")).x + m_imgui->scaled(1.5f);
    const float gap_area_txt_width = m_imgui->calc_text_size(m_desc.at("gap_area")).x + m_imgui->scaled(1.5f);
    const float smart_fill_angle_txt_width = m_imgui->calc_text_size(m_desc.at("smart_fill_angle")).x + m_imgui->scaled(1.5f);
    const float empty_button_width      = m_imgui->calc_button_size("").x;

    const float sliders_left_width = std::max(gap_area_txt_width, std::max(smart_fill_angle_txt_width, cursor_slider_left));
    const float slider_icon_width  = m_imgui->get_slider_icon_size().x;
    const float max_tooltip_width = ImGui::GetFontSize() * 20.0f;

    const float sliders_width = m_imgui->scaled(7.0f);
    const float drag_left_width = ImGui::GetStyle().WindowPadding.x + sliders_width - space_size;

    ImGui::AlignTextToFramePadding();
    m_imgui->text(m_desc.at("tool_type"));
    std::array<wchar_t, 4> tool_ids = { ImGui::CircleButtonIcon, ImGui::SphereButtonIcon, ImGui::FillButtonIcon, ImGui::GapFillIcon };
    std::array<wchar_t, 4> icons;
    if (m_is_dark_mode)
        icons = { ImGui::CircleButtonDarkIcon, ImGui::SphereButtonDarkIcon, ImGui::FillButtonDarkIcon, ImGui::GapFillDarkIcon };
    else
        icons = { ImGui::CircleButtonIcon, ImGui::SphereButtonIcon, ImGui::FillButtonIcon, ImGui::GapFillIcon };

    std::array<wxString, 4> tool_tips = { _L("Circle"), _L("Sphere"), _L("Fill"), _L("Gap Fill") };
    for (int i = 0; i < (int)tool_ids.size(); i++) {
        if (i != 0) ImGui::SameLine((empty_button_width + m_imgui->scaled(1.75f)) * i + m_imgui->scaled(1.3f));

        bool is_active = m_current_tool == tool_ids[i];
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.f);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding  , 3.f * scale);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding   , ImVec2(4.f * scale, 4.f * scale));
        ImGui::PushStyleColor(ImGuiCol_Text         , ImVec4(1,1,1,1));
        ImGui::PushStyleColor(ImGuiCol_Button       , is_active ? ImVec4(0.f, .59f, .53f, .25f) : ImVec4(0,0,0,0));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, is_active ? ImVec4(0.f, .59f, .53f, .25f) : ImVec4(.6f,.6f,.6f,.2f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive , is_active ? ImVec4(0.f, .59f, .53f, .30f) : ImVec4(0,0,0,0));
        ImGui::PushStyleColor(ImGuiCol_Border       , is_active ? ImGuiWrapper::COL_ORCA        : ImVec4(0,0,0,0));
        ImGui::PushStyleColor(ImGuiCol_BorderActive , is_active ? ImGuiWrapper::COL_ORCA        : ImVec4(0,0,0,0));
        bool btn_clicked = m_imgui->glyph_button(icons[i], ImVec2(16.f  * scale, 16.f  * scale));
        ImGui::PopStyleColor(6);
        ImGui::PopStyleVar(3);

        if (btn_clicked && m_current_tool != tool_ids[i]) {
            m_current_tool = tool_ids[i];
            for (auto& triangle_selector : m_triangle_selectors) {
                triangle_selector->seed_fill_unselect_all_triangles();
                triangle_selector->request_update_render_data();
            }
        }

        if (ImGui::IsItemHovered())
            m_imgui->tooltip(tool_tips[i], max_tooltip_width);
    }

    if (m_current_tool != old_tool)
        this->tool_changed(old_tool, m_current_tool);

    ImGui::Dummy(ImVec2(0.0f, ImGui::GetFontSize() * 0.1));

    if (m_current_tool == ImGui::CircleButtonIcon) {
        m_cursor_type = TriangleSelector::CursorType::CIRCLE;
        m_tool_type = ToolType::BRUSH;

        ImGui::AlignTextToFramePadding();
        m_imgui->text(m_desc.at("cursor_size"));
        ImGui::SameLine(sliders_left_width);
        ImGui::PushItemWidth(sliders_width);
        m_imgui->bbl_slider_float_style("##cursor_radius", &m_cursor_radius, CursorRadiusMin, CursorRadiusMax, "%.2f", 1.0f, true);
        ImGui::SameLine(drag_left_width + sliders_left_width);
        ImGui::PushItemWidth(1.5 * slider_icon_width);
        ImGui::BBLDragFloat("##cursor_radius_input", &m_cursor_radius, 0.05f, 0.0f, 0.0f, "%.2f");
    } else if (m_current_tool == ImGui::SphereButtonIcon) {
        m_cursor_type = TriangleSelector::CursorType::SPHERE;
        m_tool_type = ToolType::BRUSH;

        ImGui::AlignTextToFramePadding();
        m_imgui->text(m_desc.at("cursor_size"));
        ImGui::SameLine(sliders_left_width);
        ImGui::PushItemWidth(sliders_width);
        m_imgui->bbl_slider_float_style("##cursor_radius", &m_cursor_radius, CursorRadiusMin, CursorRadiusMax, "%.2f", 1.0f, true);
        ImGui::SameLine(drag_left_width + sliders_left_width);
        ImGui::PushItemWidth(1.5 * slider_icon_width);
        ImGui::BBLDragFloat("##cursor_radius_input", &m_cursor_radius, 0.05f, 0.0f, 0.0f, "%.2f");
    } else if (m_current_tool == ImGui::FillButtonIcon) {
        m_cursor_type = TriangleSelector::CursorType::POINTER;
        m_tool_type = ToolType::SMART_FILL;

        ImGui::AlignTextToFramePadding();
        m_imgui->text(m_desc["smart_fill_angle"]);
        std::string format_str = std::string("%.f") + I18N::translate_utf8("°",
                                                                            "Face angle threshold,"
                                                                            "placed after the number with no whitespace in between.");
        ImGui::SameLine(sliders_left_width);
        ImGui::PushItemWidth(sliders_width);
        if (m_imgui->bbl_slider_float_style("##smart_fill_angle", &m_smart_fill_angle, SmartFillAngleMin, SmartFillAngleMax,
                                            format_str.data(), 1.0f, true))
            for (auto& triangle_selector : m_triangle_selectors) {
                triangle_selector->seed_fill_unselect_all_triangles();
                triangle_selector->request_update_render_data();
            }
        ImGui::SameLine(drag_left_width + sliders_left_width);
        ImGui::PushItemWidth(1.5 * slider_icon_width);
        ImGui::BBLDragFloat("##smart_fill_angle_input", &m_smart_fill_angle, 0.05f, 0.0f, 0.0f, "%.2f");
    } else if (m_current_tool == ImGui::GapFillIcon) {
        m_tool_type = ToolType::GAP_FILL;
        m_cursor_type = TriangleSelector::CursorType::POINTER;

        ImGui::AlignTextToFramePadding();
        m_imgui->text(m_desc["gap_area"]);
        ImGui::SameLine(sliders_left_width);
        ImGui::PushItemWidth(sliders_width);
        std::string format_str = std::string("%.2f") + I18N::translate_utf8("", "Triangle patch area threshold,""triangle patch will be merged to neighbor if its area is less than threshold");
        m_imgui->bbl_slider_float_style("##gap_area", &TriangleSelectorPatch::gap_area, TriangleSelectorPatch::GapAreaMin, TriangleSelectorPatch::GapAreaMax, format_str.data(), 1.0f, true);
        ImGui::SameLine(drag_left_width + sliders_left_width);
        ImGui::PushItemWidth(1.5 * slider_icon_width);
        ImGui::BBLDragFloat("##gap_area_input", &TriangleSelectorPatch::gap_area, 0.05f, 0.0f, 0.0f, "%.2f");

        if (m_imgui->button(m_desc.at("perform"))) {
            Plater::TakeSnapshot snapshot(wxGetApp().plater(), "Reset selection", UndoRedo::SnapshotType::GizmoAction);

            for (int i = 0; i < (int)m_triangle_selectors.size(); i++) {
                TriangleSelectorPatch* ts_mm = dynamic_cast<TriangleSelectorPatch*>(m_triangle_selectors[i].get());
                ts_mm->update_selector_triangles();
                ts_mm->request_update_render_data(true);
            }
            update_model_object();
            m_parent.set_as_dirty();
        }
    }

    ImGui::Separator();

    render_tooltip_button(x, y);

    ImGui::SameLine();
    m_imgui->disabled_begin(mo->is_ironing_painted() == false);
    if (m_imgui->button(m_desc.at("remove_all"))) {
        Plater::TakeSnapshot snapshot(wxGetApp().plater(), _u8L("Reset selection"), UndoRedo::SnapshotType::GizmoAction);
        int                  idx = -1;
        for (ModelVolume *mv : mo->volumes)
            if (mv->is_model_part()) {
                ++idx;
                m_triangle_selectors[idx]->reset();
                m_triangle_selectors[idx]->request_update_render_data(true);
            }

        update_model_object();
        m_parent.set_as_dirty();
    }
    m_imgui->disabled_end();

    ImGui::SameLine();
    GLGizmoUtils::begin_right_aligned_buttons({_L("Done")});
    if (m_imgui->button(_L("Done"))) {
        m_parent.reset_all_gizmos();
    }

    const DynamicPrintConfig &obj_cfg = mo->config.get();
    const DynamicPrintConfig &glb_cfg = wxGetApp().preset_bundle->prints.get_edited_preset().config;
    const bool has_object_ironing_override = obj_cfg.option("ironing_type");
    const IroningType effective_ironing = has_object_ironing_override ? obj_cfg.opt_enum<IroningType>("ironing_type")
                                                                     : glb_cfg.opt_enum<IroningType>("ironing_type");
    if (effective_ironing == IroningType::NoIroning) {
        float font_size = ImGui::GetFontSize();
        ImColor HyperColor = ImGuiWrapper::COL_ORCA;
        ImGui::Separator();
        ImGui::PushStyleColor(ImGuiCol_Text, ImGuiWrapper::COL_WARNING);
        float parent_width = ImGui::GetContentRegionAvail().x;
        m_imgui->text_wrapped(_L("Warning: Ironing is disabled. Enable Top surfaces or Painted ironing so red blocked areas are skipped. Painted ironing also irons extra green-painted surfaces."), parent_width);
        ImGui::PopStyleColor();
        ImGui::PushStyleColor(ImGuiCol_Text, HyperColor.Value);
        ImGui::Dummy(ImVec2(font_size * 1.8f, font_size * 1.3f));
        ImGui::SameLine();
        m_imgui->bold_text(_u8L("Set Ironing Type to Painted ironing for this object"));
        ImGui::PopStyleColor();
        ImVec2 line_end = ImGui::GetItemRectMax();
        line_end.y -= 2.0f;
        ImVec2 line_start = line_end;
        line_start.x = ImGui::GetItemRectMin().x;
        ImGui::GetWindowDrawList()->AddLine(line_start, line_end, HyperColor);
        if (ImGui::IsMouseHoveringRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), true)
            && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            DynamicPrintConfig new_conf = obj_cfg;
            new_conf.set_key_value("ironing_type", new ConfigOptionEnum<IroningType>(IroningType::Painted));
            mo->config.assign_config(new_conf);
        }
    }

    ImGui::PopStyleVar(1);
    GizmoImguiEnd();
    ImGuiWrapper::pop_toolbar_style();
}

void GLGizmoIroning::tool_changed(wchar_t old_tool, wchar_t new_tool)
{
    if ((old_tool == ImGui::GapFillIcon && new_tool == ImGui::GapFillIcon) ||
        (old_tool != ImGui::GapFillIcon && new_tool != ImGui::GapFillIcon))
        return;

    for (auto& selector_ptr : m_triangle_selectors) {
        TriangleSelectorPatch* tsp = dynamic_cast<TriangleSelectorPatch*>(selector_ptr.get());
        tsp->set_filter_state(new_tool == ImGui::GapFillIcon);
    }
}

void GLGizmoIroning::render_tooltip_button(float x, float y)
{
    auto get_shortcuts = [this]() -> std::vector<std::pair<wxString, wxString>> {
        switch (m_current_tool) {
            case ImGui::CircleButtonIcon:
            case ImGui::SphereButtonIcon:
                return m_shortcuts_brush;
            case ImGui::FillButtonIcon:
                return m_shortcuts_bucket_fill;
            case ImGui::GapFillIcon:
                return m_shortcuts_gap_fill;
            default:
                return {};
        }
    };

    GLGizmoUtils::render_tooltip_button(m_imgui, m_parent, get_shortcuts(), x, y);
}

void GLGizmoIroning::update_model_object()
{
    bool         updated = false;
    ModelObject *mo      = m_c->selection_info()->model_object();
    int          idx     = -1;
    for (ModelVolume *mv : mo->volumes) {
        if (!mv->is_model_part())
            continue;

        ++idx;
        updated |= mv->ironing_facets.set(*m_triangle_selectors[idx]);
    }

    if (updated) {
        const ModelObjectPtrs &mos = wxGetApp().model().objects;
        wxGetApp().obj_list()->update_info_items(std::find(mos.begin(), mos.end(), mo) - mos.begin());

        m_parent.post_event(SimpleEvent(EVT_GLCANVAS_SCHEDULE_BACKGROUND_PROCESS));
    }
}

void GLGizmoIroning::update_from_model_object(bool /*first_update*/)
{
    wxBusyCursor wait;

    const ModelObject *mo = m_c->selection_info()->model_object();
    m_triangle_selectors.clear();

    std::vector<ColorRGBA> ebt_colors;
    ebt_colors.push_back(GLVolume::NEUTRAL_COLOR);
    ebt_colors.push_back(TriangleSelectorGUI::enforcers_color);
    ebt_colors.push_back(TriangleSelectorGUI::blockers_color);
    for (const ModelVolume *mv : mo->volumes) {
        if (!mv->is_model_part())
            continue;

        const TriangleMesh* mesh = &mv->mesh();
        m_triangle_selectors.emplace_back(std::make_unique<TriangleSelectorPatch>(*mesh, ebt_colors));
        m_triangle_selectors.back()->deserialize(mv->ironing_facets.get_data(), false);
        m_triangle_selectors.back()->request_update_render_data();
    }
}

PainterGizmoType GLGizmoIroning::get_painter_type() const
{
    return PainterGizmoType::IRONING;
}

wxString GLGizmoIroning::handle_snapshot_action_name(bool shift_down, GLGizmoPainterBase::Button button_down) const
{
    if (shift_down)
        return _L("Erase");
    return (button_down == Button::Left) ? _L("Paint ironing") : _L("Block ironing");
}

} // namespace Slic3r::GUI
