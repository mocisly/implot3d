//--------------------------------------------------
// ImPlot3D v0.3 WIP
// implot3d.cpp
// Date: 2024-11-16
// Author: Breno Cunha Queiroz (brenocq.com)
//
// Acknowledgments:
//  ImPlot3D is heavily inspired by ImPlot
//  (https://github.com/epezent/implot) by Evan Pezent,
//  and follows a similar code style and structure to
//  maintain consistency with ImPlot's API.
//--------------------------------------------------

// Table of Contents:
// [SECTION] Includes
// [SECTION] Macros
// [SECTION] Context
// [SECTION] Text Utils
// [SECTION] Legend Utils
// [SECTION] Mouse Position Utils
// [SECTION] Plot Box Utils
// [SECTION] Formatter
// [SECTION] Locator
// [SECTION] Context Menus
// [SECTION] Begin/End Plot
// [SECTION] Setup
// [SECTION] Plot Utils
// [SECTION] Setup Utils
// [SECTION] Miscellaneous
// [SECTION] Styles
// [SECTION] Colormaps
// [SECTION] Context Utils
// [SECTION] Style Utils
// [SECTION] ImPlot3DPoint
// [SECTION] ImPlot3DBox
// [SECTION] ImPlot3DRange
// [SECTION] ImPlot3DQuat
// [SECTION] ImDrawList3D
// [SECTION] ImPlot3DAxis
// [SECTION] ImPlot3DPlot
// [SECTION] ImPlot3DStyle
// [SECTION] Metrics

//-----------------------------------------------------------------------------
// [SECTION] Includes
//-----------------------------------------------------------------------------

#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif

// We define this to avoid accidentally using the deprecated API
#ifndef IMPLOT_DISABLE_OBSOLETE_FUNCTIONS
#define IMPLOT_DISABLE_OBSOLETE_FUNCTIONS
#endif

#include "implot3d.h"
#include "implot3d_internal.h"

#ifndef IMGUI_DISABLE

//-----------------------------------------------------------------------------
// [SECTION] Macros
//-----------------------------------------------------------------------------

#define IMPLOT3D_CHECK_CTX()                                                                                                                         \
    IM_ASSERT_USER_ERROR(GImPlot3D != nullptr, "No current context. Did you call ImPlot3D::CreateContext() or ImPlot3D::SetCurrentContext()?")
#define IMPLOT3D_CHECK_PLOT() IM_ASSERT_USER_ERROR(GImPlot3D->CurrentPlot != nullptr, "No active plot. Did you call ImPlot3D::BeginPlot()?")

//-----------------------------------------------------------------------------
// [SECTION] Context
//-----------------------------------------------------------------------------

namespace ImPlot3D {

// Global ImPlot3D context
#ifndef GImPlot3D
ImPlot3DContext* GImPlot3D = nullptr;
#endif

ImPlot3DContext* CreateContext() {
    ImPlot3DContext* ctx = IM_NEW(ImPlot3DContext)();
    if (GImPlot3D == nullptr)
        SetCurrentContext(ctx);
    InitializeContext(ctx);
    return ctx;
}

void DestroyContext(ImPlot3DContext* ctx) {
    if (ctx == nullptr)
        ctx = GImPlot3D;
    if (GImPlot3D == ctx)
        SetCurrentContext(nullptr);
    IM_DELETE(ctx);
}

ImPlot3DContext* GetCurrentContext() { return GImPlot3D; }

void SetCurrentContext(ImPlot3DContext* ctx) { GImPlot3D = ctx; }

//-----------------------------------------------------------------------------
// [SECTION] Text Utils
//-----------------------------------------------------------------------------

void AddTextRotated(ImDrawList* draw_list, ImVec2 pos, float angle, ImU32 col, const char* text_begin, const char* text_end) {
    if (!text_end)
        text_end = text_begin + strlen(text_begin);

    ImGuiContext& g = *GImGui;
    ImFont* font = g.Font;

#ifdef IMGUI_HAS_TEXTURES
    ImFontBaked* fontBaked = g.Font->GetFontBaked(g.FontSize);
    const float scale = g.FontSize / fontBaked->Size;
#else
    const float scale = g.FontSize / font->FontSize;
#endif

    // Align to be pixel perfect
    pos = ImFloor(pos);

    // Measure the size of the text in unrotated coordinates
    ImVec2 text_size = font->CalcTextSizeA(g.FontSize, FLT_MAX, 0.0f, text_begin, text_end, nullptr);

    // Precompute sine and cosine of the angle (note: angle should be positive for rotation in ImGui)
    float cos_a = cosf(-angle);
    float sin_a = sinf(-angle);

    const char* s = text_begin;
    int chars_total = (int)(text_end - s);
    int chars_rendered = 0;
    const int vtx_count_max = chars_total * 4;
    const int idx_count_max = chars_total * 6;
    draw_list->PrimReserve(idx_count_max, vtx_count_max);

    // Adjust pen position to center the text
    ImVec2 pen = ImVec2(-text_size.x * 0.5f, -text_size.y * 0.5f);

    while (s < text_end) {
        unsigned int c = (unsigned int)*s;
        if (c < 0x80) {
            s += 1;
        } else {
            s += ImTextCharFromUtf8(&c, s, text_end);
            if (c == 0) // Malformed UTF-8?
                break;
        }

#ifdef IMGUI_HAS_TEXTURES
        const ImFontGlyph* glyph = fontBaked->FindGlyph((ImWchar)c);
#else
        const ImFontGlyph* glyph = font->FindGlyph((ImWchar)c);
#endif
        if (glyph == nullptr) {
            continue;
        }

        // Glyph dimensions and positions
        ImVec2 glyph_offset = ImVec2(glyph->X0, glyph->Y0) * scale;
        ImVec2 glyph_size = ImVec2(glyph->X1 - glyph->X0, glyph->Y1 - glyph->Y0) * scale;

        // Corners of the glyph quad in unrotated space
        ImVec2 corners[4];
        corners[0] = pen + glyph_offset;
        corners[1] = pen + glyph_offset + ImVec2(glyph_size.x, 0);
        corners[2] = pen + glyph_offset + glyph_size;
        corners[3] = pen + glyph_offset + ImVec2(0, glyph_size.y);

        // Rotate and translate the corners
        for (int i = 0; i < 4; i++) {
            float x = corners[i].x;
            float y = corners[i].y;
            corners[i].x = x * cos_a - y * sin_a + pos.x;
            corners[i].y = x * sin_a + y * cos_a + pos.y;
        }

        // Texture coordinates
        ImVec2 uv0 = ImVec2(glyph->U0, glyph->V0);
        ImVec2 uv1 = ImVec2(glyph->U1, glyph->V1);

        // Render the glyph quad
        draw_list->PrimQuadUV(corners[0], corners[1], corners[2], corners[3], uv0, ImVec2(glyph->U1, glyph->V0), uv1, ImVec2(glyph->U0, glyph->V1),
                              col);

        // Advance the pen position
        pen.x += glyph->AdvanceX * scale;

        chars_rendered++;
    }

    // Return unused vertices
    int chars_skipped = chars_total - chars_rendered;
    draw_list->PrimUnreserve(chars_skipped * 6, chars_skipped * 4);
}

void AddTextCentered(ImDrawList* draw_list, ImVec2 top_center, ImU32 col, const char* text_begin) {
    const char* text_end = ImGui::FindRenderedTextEnd(text_begin);
    ImVec2 text_size = ImGui::CalcTextSize(text_begin, text_end, true);
    draw_list->AddText(ImVec2(top_center.x - text_size.x * 0.5f, top_center.y), col, text_begin, text_end);
}

//-----------------------------------------------------------------------------
// [SECTION] Legend Utils
//-----------------------------------------------------------------------------

static const char* short_legend_location[9] = {"C", "N", "S", "W", "E", "NW", "NE", "SW", "SE"};

static const char* GetShortLegendLocationName(ImPlot3DLocation loc) {
    if (loc == ImPlot3DLocation_Center)
        return short_legend_location[0];
    if (loc == ImPlot3DLocation_North)
        return short_legend_location[1];
    if (loc == ImPlot3DLocation_South)
        return short_legend_location[2];
    if (loc == ImPlot3DLocation_West)
        return short_legend_location[3];
    if (loc == ImPlot3DLocation_East)
        return short_legend_location[4];
    if (loc == ImPlot3DLocation_NorthWest)
        return short_legend_location[5];
    if (loc == ImPlot3DLocation_NorthEast)
        return short_legend_location[6];
    if (loc == ImPlot3DLocation_SouthWest)
        return short_legend_location[7];
    if (loc == ImPlot3DLocation_SouthEast)
        return short_legend_location[8];
    return nullptr;
}

ImVec2 GetLocationPos(const ImRect& outer_rect, const ImVec2& inner_size, ImPlot3DLocation loc, const ImVec2& pad) {
    ImVec2 pos;
    // Legend x coordinate
    if (ImPlot3D::ImHasFlag(loc, ImPlot3DLocation_West) && !ImPlot3D::ImHasFlag(loc, ImPlot3DLocation_East))
        pos.x = outer_rect.Min.x + pad.x;
    else if (!ImPlot3D::ImHasFlag(loc, ImPlot3DLocation_West) && ImPlot3D::ImHasFlag(loc, ImPlot3DLocation_East))
        pos.x = outer_rect.Max.x - pad.x - inner_size.x;
    else
        pos.x = outer_rect.GetCenter().x - inner_size.x * 0.5f;
    // Legend y coordinate
    if (ImPlot3D::ImHasFlag(loc, ImPlot3DLocation_North) && !ImPlot3D::ImHasFlag(loc, ImPlot3DLocation_South))
        pos.y = outer_rect.Min.y + pad.y;
    else if (!ImPlot3D::ImHasFlag(loc, ImPlot3DLocation_North) && ImPlot3D::ImHasFlag(loc, ImPlot3DLocation_South))
        pos.y = outer_rect.Max.y - pad.y - inner_size.y;
    else
        pos.y = outer_rect.GetCenter().y - inner_size.y * 0.5f;
    pos.x = IM_ROUND(pos.x);
    pos.y = IM_ROUND(pos.y);
    return pos;
}

ImVec2 CalcLegendSize(ImPlot3DItemGroup& items, const ImVec2& pad, const ImVec2& spacing, bool vertical) {
    const int nItems = items.GetLegendCount();
    const float txt_ht = ImGui::GetTextLineHeight();
    const float icon_size = txt_ht;
    // Get label max width
    float max_label_width = 0;
    float sum_label_width = 0;
    for (int i = 0; i < nItems; i++) {
        const char* label = items.GetLegendLabel(i);
        const float label_width = ImGui::CalcTextSize(label, nullptr, true).x;
        max_label_width = label_width > max_label_width ? label_width : max_label_width;
        sum_label_width += label_width;
    }
    // Compute legend size
    const ImVec2 legend_size = vertical ? ImVec2(pad.x * 2 + icon_size + max_label_width, pad.y * 2 + nItems * txt_ht + (nItems - 1) * spacing.y)
                                        : ImVec2(pad.x * 2 + icon_size * nItems + sum_label_width + (nItems - 1) * spacing.x, pad.y * 2 + txt_ht);
    return legend_size;
}

void ShowLegendEntries(ImPlot3DItemGroup& items, const ImRect& legend_bb, bool hovered, const ImVec2& pad, const ImVec2& spacing, bool vertical,
                       ImDrawList& draw_list) {
    const float txt_ht = ImGui::GetTextLineHeight();
    const float icon_size = txt_ht;
    const float icon_shrink = 2;
    ImU32 col_txt = GetStyleColorU32(ImPlot3DCol_LegendText);
    ImU32 col_txt_dis = ImAlphaU32(col_txt, 0.25f);
    float sum_label_width = 0;

    const int num_items = items.GetLegendCount();
    if (num_items == 0)
        return;

    // Render legend items
    for (int i = 0; i < num_items; i++) {
        const int idx = i;
        ImPlot3DItem* item = items.GetLegendItem(idx);
        const char* label = items.GetLegendLabel(idx);
        const float label_width = ImGui::CalcTextSize(label, nullptr, true).x;
        const ImVec2 top_left = vertical ? legend_bb.Min + pad + ImVec2(0, i * (txt_ht + spacing.y))
                                         : legend_bb.Min + pad + ImVec2(i * (icon_size + spacing.x) + sum_label_width, 0);
        sum_label_width += label_width;
        ImRect icon_bb;
        icon_bb.Min = top_left + ImVec2(icon_shrink, icon_shrink);
        icon_bb.Max = top_left + ImVec2(icon_size - icon_shrink, icon_size - icon_shrink);
        ImRect label_bb;
        label_bb.Min = top_left;
        label_bb.Max = top_left + ImVec2(label_width + icon_size, icon_size);
        ImU32 col_txt_hl;
        ImU32 col_item = ImAlphaU32(item->Color, 1);

        ImRect button_bb(icon_bb.Min, label_bb.Max);

        ImGui::KeepAliveID(item->ID);

        bool item_hov = false;
        bool item_hld = false;
        bool item_clk = ImPlot3D::ImHasFlag(items.Legend.Flags, ImPlot3DLegendFlags_NoButtons)
                            ? false
                            : ImGui::ButtonBehavior(button_bb, item->ID, &item_hov, &item_hld);

        if (item_clk)
            item->Show = !item->Show;

        const bool hovering = item_hov && !ImPlot3D::ImHasFlag(items.Legend.Flags, ImPlot3DLegendFlags_NoHighlightItem);

        if (hovering) {
            item->LegendHovered = true;
            col_txt_hl = ImPlot3D::ImMixU32(col_txt, col_item, 64);
        } else {
            item->LegendHovered = false;
            col_txt_hl = ImGui::GetColorU32(col_txt);
        }

        ImU32 col_icon;
        if (item_hld)
            col_icon = item->Show ? ImAlphaU32(col_item, 0.5f) : ImGui::GetColorU32(ImGuiCol_TextDisabled, 0.5f);
        else if (item_hov)
            col_icon = item->Show ? ImAlphaU32(col_item, 0.75f) : ImGui::GetColorU32(ImGuiCol_TextDisabled, 0.75f);
        else
            col_icon = item->Show ? col_item : col_txt_dis;

        draw_list.AddRectFilled(icon_bb.Min, icon_bb.Max, col_icon);
        const char* text_display_end = ImGui::FindRenderedTextEnd(label, nullptr);
        if (label != text_display_end)
            draw_list.AddText(top_left + ImVec2(icon_size, 0), item->Show ? col_txt_hl : col_txt_dis, label, text_display_end);
    }
}

void RenderLegend() {
    ImPlot3DContext& gp = *GImPlot3D;
    ImPlot3DPlot& plot = *gp.CurrentPlot;
    if (ImPlot3D::ImHasFlag(plot.Flags, ImPlot3DFlags_NoLegend) || plot.Items.GetLegendCount() == 0)
        return;
    ImGuiContext& g = *GImGui;
    ImGuiWindow* window = g.CurrentWindow;
    ImDrawList* draw_list = window->DrawList;
    const ImGuiIO& IO = ImGui::GetIO();

    ImPlot3DLegend& legend = plot.Items.Legend;
    const bool legend_horz = ImPlot3D::ImHasFlag(legend.Flags, ImPlot3DLegendFlags_Horizontal);
    const ImVec2 legend_size = CalcLegendSize(plot.Items, gp.Style.LegendInnerPadding, gp.Style.LegendSpacing, !legend_horz);
    const ImVec2 legend_pos = GetLocationPos(plot.PlotRect, legend_size, legend.Location, gp.Style.LegendPadding);
    legend.Rect = ImRect(legend_pos, legend_pos + legend_size);

    // Test hover
    legend.Hovered = legend.Rect.Contains(IO.MousePos);

    // Render background
    ImU32 col_bg = GetStyleColorU32(ImPlot3DCol_LegendBg);
    ImU32 col_bd = GetStyleColorU32(ImPlot3DCol_LegendBorder);
    draw_list->AddRectFilled(legend.Rect.Min, legend.Rect.Max, col_bg);
    draw_list->AddRect(legend.Rect.Min, legend.Rect.Max, col_bd);

    // Render legends
    ShowLegendEntries(plot.Items, legend.Rect, legend.Hovered, gp.Style.LegendInnerPadding, gp.Style.LegendSpacing, !legend_horz, *draw_list);
}

//-----------------------------------------------------------------------------
// [SECTION] Mouse Position Utils
//-----------------------------------------------------------------------------

void RenderMousePos() {
    ImPlot3DContext& gp = *GImPlot3D;
    ImPlot3DPlot& plot = *gp.CurrentPlot;
    if (ImPlot3D::ImHasFlag(plot.Flags, ImPlot3DFlags_NoMouseText))
        return;

    ImVec2 mouse_pos = ImGui::GetMousePos();
    ImPlot3DPoint mouse_plot_pos = PixelsToPlotPlane(mouse_pos, ImPlane3D_YZ, true);
    if (mouse_plot_pos.IsNaN())
        mouse_plot_pos = PixelsToPlotPlane(mouse_pos, ImPlane3D_XZ, true);
    if (mouse_plot_pos.IsNaN())
        mouse_plot_pos = PixelsToPlotPlane(mouse_pos, ImPlane3D_XY, true);

    char buff[IMPLOT3D_LABEL_MAX_SIZE];
    if (!mouse_plot_pos.IsNaN()) {
        ImGuiTextBuffer builder;
        builder.append("(");
        for (int i = 0; i < 3; i++) {
            ImPlot3DAxis& axis = plot.Axes[i];
            if (i > 0)
                builder.append(", ");
            axis.Formatter(mouse_plot_pos[i], buff, IMPLOT3D_LABEL_MAX_SIZE, axis.FormatterData);
            builder.append(buff);
        }
        builder.append(")");

        const ImVec2 size = ImGui::CalcTextSize(builder.c_str());
        // TODO custom location/padding
        const ImVec2 pos = GetLocationPos(plot.PlotRect, size, ImPlot3DLocation_SouthEast, ImVec2(10, 10));
        ImDrawList& draw_list = *ImGui::GetWindowDrawList();
        draw_list.AddText(pos, GetStyleColorU32(ImPlot3DCol_InlayText), builder.c_str());
    }
}

//-----------------------------------------------------------------------------
// [SECTION] Plot Box Utils
//-----------------------------------------------------------------------------

// Faces of the box (defined by 4 corner indices)
static const int faces[6][4] = {
    {0, 3, 7, 4}, // X-min face
    {0, 4, 5, 1}, // Y-min face
    {0, 1, 2, 3}, // Z-min face
    {1, 2, 6, 5}, // X-max face
    {3, 7, 6, 2}, // Y-max face
    {4, 5, 6, 7}, // Z-max face
};

// Edges of the box (defined by 2 corner indices)
static const int edges[12][2] = {
    // Bottom face edges
    {0, 1},
    {1, 2},
    {2, 3},
    {3, 0},
    // Top face edges
    {4, 5},
    {5, 6},
    {6, 7},
    {7, 4},
    // Vertical edges
    {0, 4},
    {1, 5},
    {2, 6},
    {3, 7},
};

// Face edges (4 edge indices for each face)
static const int face_edges[6][4] = {
    {3, 11, 8, 7},  // X-min face
    {0, 8, 4, 9},   // Y-min face
    {0, 1, 2, 3},   // Z-min face
    {1, 9, 5, 10},  // X-max face
    {2, 10, 6, 11}, // Y-max face
    {4, 5, 6, 7},   // Z-max face
};

// Lookup table for axis_corners based on active_faces (3D plot)
static const int axis_corners_lookup_3d[8][3][2] = {
    // Index 0: active_faces = {0, 0, 0}
    {{3, 2}, {1, 2}, {1, 5}},
    // Index 1: active_faces = {0, 0, 1}
    {{7, 6}, {5, 6}, {1, 5}},
    // Index 2: active_faces = {0, 1, 0}
    {{0, 1}, {1, 2}, {2, 6}},
    // Index 3: active_faces = {0, 1, 1}
    {{4, 5}, {5, 6}, {2, 6}},
    // Index 4: active_faces = {1, 0, 0}
    {{3, 2}, {0, 3}, {0, 4}},
    // Index 5: active_faces = {1, 0, 1}
    {{7, 6}, {4, 7}, {0, 4}},
    // Index 6: active_faces = {1, 1, 0}
    {{0, 1}, {0, 3}, {3, 7}},
    // Index 7: active_faces = {1, 1, 1}
    {{4, 5}, {4, 7}, {3, 7}},
};

// Lookup table for axis_edges based on active_faces (3D plot)
static const int axis_edges_lookup_3d[8][3] = {
    // Index 0: active_faces = {0, 0, 0}
    {2, 1, 9},
    // Index 1: active_faces = {0, 0, 1}
    {6, 5, 9},
    // Index 2: active_faces = {0, 1, 0}
    {0, 1, 10},
    // Index 3: active_faces = {0, 1, 1}
    {4, 5, 10},
    // Index 4: active_faces = {1, 0, 0}
    {2, 3, 8},
    // Index 5: active_faces = {1, 0, 1}
    {6, 7, 8},
    // Index 6: active_faces = {1, 1, 0}
    {0, 3, 11},
    // Index 7: active_faces = {1, 1, 1}
    {4, 7, 11},
};

// Convert the X, Y and Z active_faces array to a integer value representation that can be looked up in axis_corners_lookup_3d
int Active3DFacesToAxisLookupIndex(const bool* active_faces) {
    return ((int)active_faces[0] << 2) | ((int)active_faces[1] << 1) | ((int)active_faces[2]);
}

int GetMouseOverPlane(const ImPlot3DPlot& plot, const bool* active_faces, const ImVec2* corners_pix, int* plane_out = nullptr) {
    ImGuiIO& io = ImGui::GetIO();
    ImVec2 mouse_pos = io.MousePos;
    if (plane_out)
        *plane_out = -1;

    // Check each active face
    for (int a = 0; a < 3; a++) {
        int face_idx = a + 3 * active_faces[a];
        ImVec2 p0 = corners_pix[faces[face_idx][0]];
        ImVec2 p1 = corners_pix[faces[face_idx][1]];
        ImVec2 p2 = corners_pix[faces[face_idx][2]];
        ImVec2 p3 = corners_pix[faces[face_idx][3]];

        // Check if the mouse is inside the face's quad (using a triangle check)
        if (ImTriangleContainsPoint(p0, p1, p2, mouse_pos) || ImTriangleContainsPoint(p2, p3, p0, mouse_pos)) {
            if (plane_out)
                *plane_out = a;
            return a; // Return the plane index: 0 -> YZ, 1 -> XZ, 2 -> XY
        }
    }

    return -1; // Not over any active plane
}

int GetMouseOverAxis(const ImPlot3DPlot& plot, const bool* active_faces, const ImVec2* corners_pix, const int plane_2d, int* edge_out = nullptr) {
    const float axis_proximity_threshold = 15.0f; // Distance in pixels to consider the mouse "close" to an axis

    ImGuiIO& io = ImGui::GetIO();
    ImVec2 mouse_pos = io.MousePos;
    if (edge_out)
        *edge_out = -1;

    bool visible_edges[12];
    for (int i = 0; i < 12; i++)
        visible_edges[i] = false;
    for (int a = 0; a < 3; a++) {
        int face_idx = a + 3 * active_faces[a];
        if (plane_2d != -1 && a != plane_2d)
            continue;
        for (size_t i = 0; i < 4; i++)
            visible_edges[face_edges[face_idx][i]] = true;
    }

    // Check each edge for proximity to the mouse
    for (int edge = 0; edge < 12; edge++) {
        if (!visible_edges[edge])
            continue;

        ImVec2 p0 = corners_pix[edges[edge][0]];
        ImVec2 p1 = corners_pix[edges[edge][1]];

        // Check distance to the edge
        ImVec2 closest_point = ImLineClosestPoint(p0, p1, mouse_pos);
        float dist = ImLengthSqr(mouse_pos - closest_point);
        if (dist <= axis_proximity_threshold) {
            if (edge_out)
                *edge_out = edge;

            // Determine which axis the edge belongs to
            if (edge == 0 || edge == 2 || edge == 4 || edge == 6)
                return 0; // X-axis
            else if (edge == 1 || edge == 3 || edge == 5 || edge == 7)
                return 1; // Y-axis
            else
                return 2; // Z-axis
        }
    }

    return -1; // Not over any axis
}

void RenderPlotBackground(ImDrawList* draw_list, const ImPlot3DPlot& plot, const ImVec2* corners_pix, const bool* active_faces, const int plane_2d) {
    const ImVec4 col_bg = GetStyleColorVec4(ImPlot3DCol_PlotBg);
    const ImVec4 col_bg_hov = col_bg + ImVec4(0.03f, 0.03f, 0.03f, 0.0f);

    int hovered_plane = -1;
    if (!plot.Held) {
        // If the mouse is not held, highlight plane hovering when mouse over it
        hovered_plane = GetMouseOverPlane(plot, active_faces, corners_pix);
        if (GetMouseOverAxis(plot, active_faces, corners_pix, plane_2d) != -1)
            hovered_plane = -1;
    } else {
        // If the mouse is held, highlight the held plane
        hovered_plane = plot.HeldPlaneIdx;
    }

    for (int a = 0; a < 3; a++) {
        int idx[4]; // Corner indices
        for (int i = 0; i < 4; i++)
            idx[i] = faces[a + 3 * active_faces[a]][i];
        const ImU32 col = ImGui::ColorConvertFloat4ToU32((hovered_plane == a) ? col_bg_hov : col_bg);
        draw_list->AddQuadFilled(corners_pix[idx[0]], corners_pix[idx[1]], corners_pix[idx[2]], corners_pix[idx[3]], col);
    }
}

void RenderPlotBorder(ImDrawList* draw_list, const ImPlot3DPlot& plot, const ImVec2* corners_pix, const bool* active_faces, const int plane_2d) {
    int hovered_edge = -1;
    if (!plot.Held)
        GetMouseOverAxis(plot, active_faces, corners_pix, plane_2d, &hovered_edge);
    else
        hovered_edge = plot.HeldEdgeIdx;

    bool render_edge[12];
    for (int i = 0; i < 12; i++)
        render_edge[i] = false;
    for (int a = 0; a < 3; a++) {
        int face_idx = a + 3 * active_faces[a];
        if (plane_2d != -1 && a != plane_2d)
            continue;
        for (size_t i = 0; i < 4; i++)
            render_edge[face_edges[face_idx][i]] = true;
    }

    ImU32 col_bd = GetStyleColorU32(ImPlot3DCol_PlotBorder);
    for (int i = 0; i < 12; i++) {
        if (render_edge[i]) {
            int idx0 = edges[i][0];
            int idx1 = edges[i][1];
            float thickness = i == hovered_edge ? 3.0f : 1.0f;
            draw_list->AddLine(corners_pix[idx0], corners_pix[idx1], col_bd, thickness);
        }
    }
}

void RenderGrid(ImDrawList* draw_list, const ImPlot3DPlot& plot, const ImPlot3DPoint* corners, const bool* active_faces, const int plane_2d) {
    ImVec4 col_grid = GetStyleColorVec4(ImPlot3DCol_AxisGrid);
    ImU32 col_grid_minor = ImGui::GetColorU32(col_grid * ImVec4(1, 1, 1, 0.3f));
    ImU32 col_grid_major = ImGui::GetColorU32(col_grid * ImVec4(1, 1, 1, 0.6f));
    for (int face = 0; face < 3; face++) {
        if (plane_2d != -1 && face != plane_2d)
            continue;
        int face_idx = face + 3 * active_faces[face];
        const ImPlot3DAxis& axis_u = plot.Axes[(face + 1) % 3];
        const ImPlot3DAxis& axis_v = plot.Axes[(face + 2) % 3];

        // Get the two axes (u and v) that define the face plane
        int idx0 = faces[face_idx][0];
        int idx1 = faces[face_idx][1];
        int idx3 = faces[face_idx][3];

        // Corners of the face in plot space
        ImPlot3DPoint p0 = corners[idx0];
        ImPlot3DPoint p1 = corners[idx1];
        ImPlot3DPoint p3 = corners[idx3];

        // Vectors along the edges
        ImPlot3DPoint u_vec = p1 - p0;
        ImPlot3DPoint v_vec = p3 - p0;

        // Render grid lines along u axis (axis_u)
        if (!ImPlot3D::ImHasFlag(axis_u.Flags, ImPlot3DAxisFlags_NoGridLines))
            for (int t = 0; t < axis_u.Ticker.TickCount(); ++t) {
                const ImPlot3DTick& tick = axis_u.Ticker.Ticks[t];

                // Compute position along u
                float t_u = (tick.PlotPos - axis_u.Range.Min) / (axis_u.Range.Max - axis_u.Range.Min);

                // Skip ticks that are out of range
                if (t_u < 0.0f || t_u > 1.0f)
                    continue;

                ImPlot3DPoint p_start = p0 + u_vec * t_u;
                ImPlot3DPoint p_end = p3 + u_vec * t_u;

                // Convert to pixel coordinates
                ImVec2 p_start_pix = PlotToPixels(p_start);
                ImVec2 p_end_pix = PlotToPixels(p_end);

                // Get color
                ImU32 col_line = tick.Major ? col_grid_major : col_grid_minor;

                // Draw the grid line
                draw_list->AddLine(p_start_pix, p_end_pix, col_line);
            }

        // Render grid lines along v axis (axis_v)
        if (!ImPlot3D::ImHasFlag(axis_v.Flags, ImPlot3DAxisFlags_NoGridLines))
            for (int t = 0; t < axis_v.Ticker.TickCount(); ++t) {
                const ImPlot3DTick& tick = axis_v.Ticker.Ticks[t];

                // Compute position along v
                float t_v = (tick.PlotPos - axis_v.Range.Min) / (axis_v.Range.Max - axis_v.Range.Min);

                // Skip ticks that are out of range
                if (t_v < 0.0f || t_v > 1.0f)
                    continue;

                ImPlot3DPoint p_start = p0 + v_vec * t_v;
                ImPlot3DPoint p_end = p1 + v_vec * t_v;

                // Convert to pixel coordinates
                ImVec2 p_start_pix = PlotToPixels(p_start);
                ImVec2 p_end_pix = PlotToPixels(p_end);

                // Get color
                ImU32 col_line = tick.Major ? col_grid_major : col_grid_minor;

                // Draw the grid line
                draw_list->AddLine(p_start_pix, p_end_pix, col_line);
            }
    }
}

void RenderTickMarks(ImDrawList* draw_list, const ImPlot3DPlot& plot, const ImPlot3DPoint* corners, const ImVec2* corners_pix,
                     const int axis_corners[3][2], const int plane_2d) {
    ImU32 col_tick = GetStyleColorU32(ImPlot3DCol_AxisTick);

    auto DeterminePlaneForAxis = [&](int axis_idx) {
        if (plane_2d != -1)
            return plane_2d;
        // If no plane chosen (-1), use:
        // X or Y axis -> XY plane (2)
        // Z axis -> YZ plane (0)
        if (axis_idx == 2)
            return 1; // Z-axis use XZ plane
        else
            return 2; // X or Y-axis use XY plane
    };

    for (int a = 0; a < 3; a++) {
        const ImPlot3DAxis& axis = plot.Axes[a];
        if (ImPlot3D::ImHasFlag(axis.Flags, ImPlot3DAxisFlags_NoTickMarks))
            continue;

        int idx0 = axis_corners[a][0];
        int idx1 = axis_corners[a][1];
        if (idx0 == idx1) // axis not visible or invalid
            continue;

        ImPlot3DPoint axis_start = corners[idx0];
        ImPlot3DPoint axis_end = corners[idx1];
        ImPlot3DPoint axis_dir = axis_end - axis_start;
        float axis_len = axis_dir.Length();
        if (axis_len < 1e-12f)
            continue;
        axis_dir /= axis_len;

        // Draw axis line
        ImVec2 axis_start_pix = corners_pix[idx0];
        ImVec2 axis_end_pix = corners_pix[idx1];
        draw_list->AddLine(axis_start_pix, axis_end_pix, col_tick);

        // Choose plane
        int chosen_plane = DeterminePlaneForAxis(a);

        // Project axis_dir onto chosen plane
        ImPlot3DPoint proj_dir = axis_dir;
        if (chosen_plane == 0) {
            // YZ plane: zero out x
            proj_dir.x = 0.0f;
        } else if (chosen_plane == 1) {
            // XZ plane: zero out y
            proj_dir.y = 0.0f;
        } else if (chosen_plane == 2) {
            // XY plane: zero out z
            proj_dir.z = 0.0f;
        }

        float proj_len = proj_dir.Length();
        if (proj_len < 1e-12f) {
            // Axis is parallel to plane normal or something degenerate, skip ticks
            continue;
        }
        proj_dir /= proj_len;

        // Rotate 90 degrees in chosen plane
        ImPlot3DPoint tick_dir;
        if (chosen_plane == 0) {
            // YZ plane
            // proj_dir=(0,py,pz), rotate 90°: (py,pz) -> (-pz,py)
            tick_dir = ImPlot3DPoint(0, -proj_dir.z, proj_dir.y);
        } else if (chosen_plane == 1) {
            // XZ plane (plane=1)
            // proj_dir=(px,0,pz), rotate 90°: (px,pz) -> (-pz,px)
            tick_dir = ImPlot3DPoint(-proj_dir.z, 0, proj_dir.x);
        } else {
            // XY plane
            // proj_dir=(px,py,0), rotate by 90°: (px,py) -> (-py,px)
            tick_dir = ImPlot3DPoint(-proj_dir.y, proj_dir.x, 0);
        }
        tick_dir.Normalize();

        // Tick lengths in NDC units
        const float major_size_ndc = 0.06f;
        const float minor_size_ndc = 0.03f;

        for (int t = 0; t < axis.Ticker.TickCount(); ++t) {
            const ImPlot3DTick& tick = axis.Ticker.Ticks[t];
            float v = (tick.PlotPos - axis.Range.Min) / (axis.Range.Max - axis.Range.Min);

            // Skip ticks that are out of range
            if (v < 0.0f || v > 1.0f)
                continue;
            ImPlot3DPoint tick_pos_ndc = PlotToNDC(axis_start + axis_dir * (v * axis_len));

            // Half tick on each side of the axis line
            float size_tick_ndc = tick.Major ? major_size_ndc : minor_size_ndc;
            ImPlot3DPoint half_tick_ndc = tick_dir * (size_tick_ndc * 0.5f);

            ImPlot3DPoint T1_ndc = tick_pos_ndc - half_tick_ndc;
            ImPlot3DPoint T2_ndc = tick_pos_ndc + half_tick_ndc;

            ImVec2 T1_screen = NDCToPixels(T1_ndc);
            ImVec2 T2_screen = NDCToPixels(T2_ndc);

            draw_list->AddLine(T1_screen, T2_screen, col_tick);
        }
    }
}

void RenderTickLabels(ImDrawList* draw_list, const ImPlot3DPlot& plot, const ImPlot3DPoint* corners, const ImVec2* corners_pix,
                      const int axis_corners[3][2]) {
    ImU32 col_tick_txt = GetStyleColorU32(ImPlot3DCol_AxisText);

    for (int a = 0; a < 3; a++) {
        const ImPlot3DAxis& axis = plot.Axes[a];
        if (ImPlot3D::ImHasFlag(axis.Flags, ImPlot3DAxisFlags_NoTickLabels))
            continue;

        // Corner indices for this axis
        int idx0 = axis_corners[a][0];
        int idx1 = axis_corners[a][1];

        // If normal to the 2D plot, ignore the ticks
        if (idx0 == idx1)
            continue;

        // Start and end points of the axis in plot space
        ImPlot3DPoint axis_start = corners[idx0];
        ImPlot3DPoint axis_end = corners[idx1];

        // Direction vector along the axis
        ImPlot3DPoint axis_dir = axis_end - axis_start;

        // Convert axis start and end to screen space
        ImVec2 axis_start_pix = corners_pix[idx0];
        ImVec2 axis_end_pix = corners_pix[idx1];

        // Screen space axis direction
        ImVec2 axis_screen_dir = axis_end_pix - axis_start_pix;
        float axis_length = ImSqrt(ImLengthSqr(axis_screen_dir));
        if (axis_length != 0.0f)
            axis_screen_dir /= axis_length;
        else
            axis_screen_dir = ImVec2(1.0f, 0.0f); // Default direction if length is zero

        // Perpendicular direction in screen space
        ImVec2 offset_dir_pix = ImVec2(-axis_screen_dir.y, axis_screen_dir.x);

        // Make sure direction points away from cube center
        ImVec2 box_center_pix = PlotToPixels(plot.RangeCenter());
        ImVec2 axis_center_pix = (axis_start_pix + axis_end_pix) * 0.5f;
        ImVec2 center_to_axis_pix = axis_center_pix - box_center_pix;
        center_to_axis_pix /= ImSqrt(ImLengthSqr(center_to_axis_pix));
        if (ImDot(offset_dir_pix, center_to_axis_pix) < 0.0f)
            offset_dir_pix = -offset_dir_pix;

        // Adjust the offset magnitude
        float offset_magnitude = 20.0f; // TODO Calculate based on label size
        ImVec2 offset_pix = offset_dir_pix * offset_magnitude;

        // Compute angle perpendicular to axis in screen space
        float angle = atan2f(-axis_screen_dir.y, axis_screen_dir.x) + IM_PI * 0.5f;

        // Normalize angle to be between -π and π
        if (angle > IM_PI)
            angle -= 2 * IM_PI;
        if (angle < -IM_PI)
            angle += 2 * IM_PI;

        // Adjust angle to keep labels upright
        if (angle > IM_PI * 0.5f)
            angle -= IM_PI;
        if (angle < -IM_PI * 0.5f)
            angle += IM_PI;

        // Loop over ticks
        for (int t = 0; t < axis.Ticker.TickCount(); ++t) {
            const ImPlot3DTick& tick = axis.Ticker.Ticks[t];
            if (!tick.ShowLabel)
                continue;

            // Compute position along the axis
            float t_axis = (tick.PlotPos - axis.Range.Min) / (axis.Range.Max - axis.Range.Min);

            // Skip ticks that are out of range
            if (t_axis < 0.0f || t_axis > 1.0f)
                continue;
            ImPlot3DPoint tick_pos = axis_start + axis_dir * t_axis;

            // Convert to pixel coordinates
            ImVec2 tick_pos_pix = PlotToPixels(tick_pos);

            // Get the tick label text
            const char* label = axis.Ticker.GetText(tick);

            // Adjust label position by offset
            ImVec2 label_pos_pix = tick_pos_pix + offset_pix;

            // Render the tick label
            AddTextRotated(draw_list, label_pos_pix, angle, col_tick_txt, label);
        }
    }
}

void RenderAxisLabels(ImDrawList* draw_list, const ImPlot3DPlot& plot, const ImPlot3DPoint* corners, const ImVec2* corners_pix,
                      const int axis_corners[3][2]) {
    for (int a = 0; a < 3; a++) {
        const ImPlot3DAxis& axis = plot.Axes[a];
        if (!axis.HasLabel())
            continue;

        const char* label = axis.GetLabel();

        // Corner indices
        int idx0 = axis_corners[a][0];
        int idx1 = axis_corners[a][1];

        // If normal to the 2D plot, ignore axis label
        if (idx0 == idx1)
            continue;

        // Position at the end of the axis
        ImPlot3DPoint label_pos = (PlotToNDC(corners[idx0]) + PlotToNDC(corners[idx1])) * 0.5f;
        ImPlot3DPoint center_dir = label_pos.Normalized();
        // Add offset
        label_pos += center_dir * 0.3f;

        // Convert to pixel coordinates
        ImVec2 label_pos_pix = NDCToPixels(label_pos);

        // Adjust label position and angle
        ImU32 col_ax_txt = GetStyleColorU32(ImPlot3DCol_AxisText);

        // Compute text angle
        ImVec2 screen_delta = corners_pix[idx1] - corners_pix[idx0];
        float angle = atan2f(-screen_delta.y, screen_delta.x);
        if (angle > IM_PI * 0.5f)
            angle -= IM_PI;
        if (angle < -IM_PI * 0.5f)
            angle += IM_PI;

        AddTextRotated(draw_list, label_pos_pix, angle, col_ax_txt, label);
    }
}

// Function to compute active faces based on the rotation
// If the plot is close to 2D, plane_2d is set to the plane index (0 -> YZ, 1 -> XZ, 2 -> XY)
// plane_2d is set to -1 otherwise
void ComputeActiveFaces(bool* active_faces, const ImPlot3DQuat& rotation, const ImPlot3DAxis* axes, int* plane_2d = nullptr) {
    if (plane_2d)
        *plane_2d = -1;

    ImPlot3DPoint rot_face_n[3] = {
        rotation * ImPlot3DPoint(1.0f, 0.0f, 0.0f),
        rotation * ImPlot3DPoint(0.0f, 1.0f, 0.0f),
        rotation * ImPlot3DPoint(0.0f, 0.0f, 1.0f),
    };

    int num_deg = 0; // Check number of planes that are degenerate (seen as a line)
    for (int i = 0; i < 3; i++) {
        // Determine the active face based on the Z component
        if (fabs(rot_face_n[i].z) < 0.025) {
            // If aligned with the plane, choose the min face for bottom/left
            active_faces[i] = rot_face_n[i].x + rot_face_n[i].y < 0.0f;
            num_deg++;
        } else {
            // Otherwise, determine based on the Z component
            bool is_inverted = ImHasFlag(axes[i].Flags, ImPlot3DAxisFlags_Invert);
            active_faces[i] = is_inverted ? (rot_face_n[i].z > 0.0f) : (rot_face_n[i].z < 0.0f);
            // Set this plane as possible 2d plane
            if (plane_2d)
                *plane_2d = i;
        }
    }
    // Only return 2d plane if there are exactly 2 degenerate planes
    if (num_deg != 2 && plane_2d)
        *plane_2d = -1;
}

// Function to compute the box corners in plot space
void ComputeBoxCorners(ImPlot3DPoint* corners, const ImPlot3DPoint& range_min, const ImPlot3DPoint& range_max) {
    corners[0] = ImPlot3DPoint(range_min.x, range_min.y, range_min.z); // 0
    corners[1] = ImPlot3DPoint(range_max.x, range_min.y, range_min.z); // 1
    corners[2] = ImPlot3DPoint(range_max.x, range_max.y, range_min.z); // 2
    corners[3] = ImPlot3DPoint(range_min.x, range_max.y, range_min.z); // 3
    corners[4] = ImPlot3DPoint(range_min.x, range_min.y, range_max.z); // 4
    corners[5] = ImPlot3DPoint(range_max.x, range_min.y, range_max.z); // 5
    corners[6] = ImPlot3DPoint(range_max.x, range_max.y, range_max.z); // 6
    corners[7] = ImPlot3DPoint(range_min.x, range_max.y, range_max.z); // 7
}

// Convert a position in the plot's NDC to pixels
ImVec2 NDCToPixels(const ImPlot3DPlot& plot, const ImPlot3DPoint& point) {
    ImVec2 center = plot.PlotRect.GetCenter();
    ImPlot3DPoint point_pix = plot.GetBoxZoom() * (plot.Rotation * point);
    point_pix.y *= -1.0f; // Invert y-axis
    point_pix.x += center.x;
    point_pix.y += center.y;

    return {point_pix.x, point_pix.y};
}

// Convert a position in the plot's coordinate system to the plot's normalized device coordinate system (NDC)
ImPlot3DPoint PlotToNDC(const ImPlot3DPlot& plot, const ImPlot3DPoint& point) {
    ImPlot3DPoint ndc_point;
    for (int i = 0; i < 3; i++) {
        const ImPlot3DAxis& axis = plot.Axes[i];
        float ndc_range = 0.5f * plot.BoxScale[i];
        float t = (point[i] - axis.Range.Min) / (axis.Range.Max - axis.Range.Min);
        t *= plot.BoxScale[i];
        ndc_point[i] = ImPlot3D::ImHasFlag(axis.Flags, ImPlot3DAxisFlags_Invert) ? (ndc_range - t) : (t - ndc_range);
    }
    return ndc_point;
}

// Convert a position in the plot's coordinate system to pixels
ImVec2 PlotToPixels(const ImPlot3DPlot& plot, const ImPlot3DPoint& point) { return NDCToPixels(plot, PlotToNDC(plot, point)); }

// Function to compute the box corners in pixel space
void ComputeBoxCornersPix(const ImPlot3DPlot& plot, ImVec2* corners_pix, const ImPlot3DPoint* corners) {
    for (int i = 0; i < 8; i++) {
        corners_pix[i] = PlotToPixels(plot, corners[i]);
    }
}

// Function to get axes parameters which can then be used later down the line
void GetAxesParameters(const ImPlot3DPlot& plot, bool* active_faces, ImVec2* corners_pix, ImPlot3DPoint* corners, int& plane_2d,
                       int axis_corners[3][2]) {
    // Get plot parameters
    const ImPlot3DQuat& rotation = plot.Rotation;
    ImPlot3DPoint range_min = plot.RangeMin();
    ImPlot3DPoint range_max = plot.RangeMax();

    // Compute active faces
    ComputeActiveFaces(active_faces, rotation, plot.Axes, &plane_2d);
    bool is_2d = plane_2d != -1;

    // Compute box corners in plot space
    ComputeBoxCorners(corners, range_min, range_max);

    // Compute box corners in pixel space
    ComputeBoxCornersPix(plot, corners_pix, corners);

    // Compute axes start and end corners (given current rotation)
    if (is_2d) {
        int face = plane_2d + 3 * active_faces[plane_2d]; // Face of the 2D plot
        int common_edges[2] = {-1, -1};                   // Edges shared by the 3 faces

        // Find the common edges between the 3 faces
        for (int i = 0; i < 4; i++) {
            int edge = face_edges[face][i];
            for (int j = 0; j < 2; j++) {
                int axis = (plane_2d + 1 + j) % 3;
                int face_idx = axis + active_faces[axis] * 3;
                for (int k = 0; k < 4; k++) {
                    if (face_edges[face_idx][k] == edge) {
                        common_edges[j] = edge;
                        break;
                    }
                }
            }
        }

        // Get corners from 2 edges (origin is the corner in common)
        int origin_corner = -1;
        int x_corner = -1;
        int y_corner = -1;
        for (int i = 0; i < 2; i++)
            for (int j = 0; j < 2; j++)
                if (edges[common_edges[0]][i] == edges[common_edges[1]][j]) {
                    origin_corner = edges[common_edges[0]][i];
                    x_corner = edges[common_edges[0]][!i];
                    y_corner = edges[common_edges[1]][!j];
                }

        // Swap x and y if they are flipped
        ImVec2 x_vec = corners_pix[x_corner] - corners_pix[origin_corner];
        ImVec2 y_vec = corners_pix[y_corner] - corners_pix[origin_corner];
        if (y_vec.x > x_vec.x)
            ImSwap(x_corner, y_corner);

        // Check which 3d axis the 2d axis refers to
        ImPlot3DPoint origin_3d = corners[origin_corner];
        ImPlot3DPoint x_3d = (corners[x_corner] - origin_3d).Normalized();
        ImPlot3DPoint y_3d = (corners[y_corner] - origin_3d).Normalized();
        int x_axis = -1;
        bool x_inverted = false;
        int y_axis = -1;
        bool y_inverted = false;
        for (int i = 0; i < 2; i++) {
            int axis_i = (plane_2d + 1 + i) % 3;
            if (y_axis != -1 || (ImAbs(x_3d[axis_i]) > 1e-8f && x_axis == -1)) {
                x_axis = axis_i;
                x_inverted = x_3d[axis_i] < 0.0f;
            } else {
                y_axis = axis_i;
                y_inverted = y_3d[axis_i] < 0.0f;
            }
        }

        // Set the 3d axis corners based on the 2d axis corners
        axis_corners[plane_2d][0] = -1;
        axis_corners[plane_2d][1] = -1;
        if (x_inverted) {
            axis_corners[x_axis][0] = x_corner;
            axis_corners[x_axis][1] = origin_corner;
        } else {
            axis_corners[x_axis][0] = origin_corner;
            axis_corners[x_axis][1] = x_corner;
        }
        if (y_inverted) {
            axis_corners[y_axis][0] = y_corner;
            axis_corners[y_axis][1] = origin_corner;
        } else {
            axis_corners[y_axis][0] = origin_corner;
            axis_corners[y_axis][1] = y_corner;
        }
    } else {
        int index = Active3DFacesToAxisLookupIndex(active_faces);
        for (int a = 0; a < 3; a++) {
            axis_corners[a][0] = axis_corners_lookup_3d[index][a][0];
            axis_corners[a][1] = axis_corners_lookup_3d[index][a][1];
        }
    }
}

void RenderPlotBox(ImDrawList* draw_list, const ImPlot3DPlot& plot) {
    bool active_faces[3];
    ImVec2 corners_pix[8];
    ImPlot3DPoint corners[8];
    int plane_2d;
    int axis_corners[3][2];

    GetAxesParameters(plot, active_faces, corners_pix, corners, plane_2d, axis_corners);

    // Render components
    RenderPlotBackground(draw_list, plot, corners_pix, active_faces, plane_2d);
    RenderPlotBorder(draw_list, plot, corners_pix, active_faces, plane_2d);
    RenderGrid(draw_list, plot, corners, active_faces, plane_2d);
    RenderTickMarks(draw_list, plot, corners, corners_pix, axis_corners, plane_2d);
    RenderTickLabels(draw_list, plot, corners, corners_pix, axis_corners);
    RenderAxisLabels(draw_list, plot, corners, corners_pix, axis_corners);
}

//-----------------------------------------------------------------------------
// [SECTION] Formatter
//-----------------------------------------------------------------------------

int Formatter_Default(float value, char* buff, int size, void* data) {
    char* fmt = (char*)data;
    return ImFormatString(buff, size, fmt, value);
}

//------------------------------------------------------------------------------
// [SECTION] Locator
//------------------------------------------------------------------------------

double NiceNum(double x, bool round) {
    double f;
    double nf;
    int expv = (int)floor(ImLog10((float)x));
    f = x / ImPow(10.0, (double)expv);
    if (round)
        if (f < 1.5)
            nf = 1;
        else if (f < 3)
            nf = 2;
        else if (f < 7)
            nf = 5;
        else
            nf = 10;
    else if (f <= 1)
        nf = 1;
    else if (f <= 2)
        nf = 2;
    else if (f <= 5)
        nf = 5;
    else
        nf = 10;
    return nf * ImPow(10.0, expv);
}

void Locator_Default(ImPlot3DTicker& ticker, const ImPlot3DRange& range, float pixels, ImPlot3DFormatter formatter, void* formatter_data) {
    if (range.Min == range.Max)
        return;
    const int nMinor = ImMin(ImMax(1, (int)IM_ROUND(pixels / 30.0f)), 5);
    const int nMajor = ImMax(2, (int)IM_ROUND(pixels / 80.0f));
    const int max_ticks_labels = 7;
    const double nice_range = NiceNum(range.Size() * 0.99, false);
    const double interval = NiceNum(nice_range / (nMajor - 1), true);
    const double graphmin = floor(range.Min / interval) * interval;
    const double graphmax = ceil(range.Max / interval) * interval;
    bool first_major_set = false;
    int first_major_idx = 0;
    const int idx0 = ticker.TickCount(); // Ticker may have user custom ticks
    for (double major = graphmin; major < graphmax + 0.5 * interval; major += interval) {
        // Is this zero? combat zero formatting issues
        if (major - interval < 0 && major + interval > 0)
            major = 0;
        if (range.Contains((float)major)) {
            if (!first_major_set) {
                first_major_idx = ticker.TickCount();
                first_major_set = true;
            }
            ticker.AddTick(major, true, true, formatter, formatter_data);
        }
        for (int i = 1; i < nMinor; i++) {
            double minor = major + i * interval / nMinor;
            if (range.Contains((float)minor)) {
                ticker.AddTick(minor, false, true, formatter, formatter_data);
            }
        }
    }

    // Prune tick labels
    if (ticker.TickCount() > max_ticks_labels) {
        for (int i = first_major_idx - 1; i >= idx0; i -= 2)
            ticker.Ticks[i].ShowLabel = false;
        for (int i = first_major_idx + 1; i < ticker.TickCount(); i += 2)
            ticker.Ticks[i].ShowLabel = false;
    }
}

void AddTicksCustom(const double* values, const char* const labels[], int n, ImPlot3DTicker& ticker, ImPlot3DFormatter formatter, void* data) {
    for (int i = 0; i < n; i++) {
        if (labels != nullptr)
            ticker.AddTick(values[i], false, true, labels[i]);
        else
            ticker.AddTick(values[i], false, true, formatter, data);
    }
}

//------------------------------------------------------------------------------
// [SECTION] Context Menus
//------------------------------------------------------------------------------

static const char* axis_contexts[3] = {"##XAxisContext", "##YAxisContext", "##ZAxisContext"};
static const char* axis_labels[3] = {"X-Axis", "Y-Axis", "Z-Axis"};
static const char* plane_contexts[3] = {"##YZPlaneContext", "##XZPlaneContext", "##XYPlaneContext"};
static const char* plane_labels[3] = {"YZ-Plane", "XZ-Plane", "XY-Plane"};

bool ShowLegendContextMenu(ImPlot3DLegend& legend, bool visible) {
    const float s = ImGui::GetFrameHeight();
    bool ret = false;
    if (ImGui::Checkbox("Show", &visible))
        ret = true;
    if (ImGui::RadioButton("H", ImPlot3D::ImHasFlag(legend.Flags, ImPlot3DLegendFlags_Horizontal)))
        legend.Flags |= ImPlot3DLegendFlags_Horizontal;
    ImGui::SameLine();
    if (ImGui::RadioButton("V", !ImPlot3D::ImHasFlag(legend.Flags, ImPlot3DLegendFlags_Horizontal)))
        legend.Flags &= ~ImPlot3DLegendFlags_Horizontal;
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(2, 2));
    // clang-format off
    if (ImGui::Button(GetShortLegendLocationName(ImPlot3DLocation_NorthWest),ImVec2(1.5f*s,s))) { legend.Location = ImPlot3DLocation_NorthWest; } ImGui::SameLine();
    if (ImGui::Button(GetShortLegendLocationName(ImPlot3DLocation_North) , ImVec2(1.5f*s,s))) { legend.Location = ImPlot3DLocation_North;     } ImGui::SameLine();
    if (ImGui::Button(GetShortLegendLocationName(ImPlot3DLocation_NorthEast), ImVec2(1.5f*s,s))) { legend.Location = ImPlot3DLocation_NorthEast; }
    if (ImGui::Button(GetShortLegendLocationName(ImPlot3DLocation_West), ImVec2(1.5f*s,s))) { legend.Location = ImPlot3DLocation_West;      } ImGui::SameLine();
    if (ImGui::InvisibleButton(GetShortLegendLocationName(ImPlot3DLocation_Center), ImVec2(1.5f*s,s))) {     } ImGui::SameLine();
    if (ImGui::Button(GetShortLegendLocationName(ImPlot3DLocation_East), ImVec2(1.5f*s,s))) { legend.Location = ImPlot3DLocation_East;      }
    if (ImGui::Button(GetShortLegendLocationName(ImPlot3DLocation_SouthWest),ImVec2(1.5f*s,s))) { legend.Location = ImPlot3DLocation_SouthWest; } ImGui::SameLine();
    if (ImGui::Button(GetShortLegendLocationName(ImPlot3DLocation_South), ImVec2(1.5f*s,s))) { legend.Location = ImPlot3DLocation_South;     } ImGui::SameLine();
    if (ImGui::Button(GetShortLegendLocationName(ImPlot3DLocation_SouthEast),ImVec2(1.5f*s,s))) { legend.Location = ImPlot3DLocation_SouthEast; }
    // clang-format on
    ImGui::PopStyleVar();
    return ret;
}

void ShowAxisContextMenu(ImPlot3DAxis& axis) {
    ImGui::PushItemWidth(75);
    bool always_locked = axis.IsRangeLocked() || axis.IsAutoFitting();
    bool label = axis.HasLabel();
    bool grid = axis.HasGridLines();
    bool ticks = axis.HasTickMarks();
    bool labels = axis.HasTickLabels();
    double drag_speed =
        (axis.Range.Size() <= FLT_EPSILON) ? FLT_EPSILON * 1.0e+13 : 0.01 * axis.Range.Size(); // recover from almost equal axis limits.

    ImGui::BeginDisabled(always_locked);
    ImGui::CheckboxFlags("##LockMin", (unsigned int*)&axis.Flags, ImPlot3DAxisFlags_LockMin);
    ImGui::EndDisabled();
    ImGui::SameLine();

    ImGui::BeginDisabled(axis.IsLockedMin() || always_locked);
    float temp_min = axis.Range.Min;
    if (ImGui::DragFloat("Min", &temp_min, (float)drag_speed, -HUGE_VAL, axis.Range.Max - FLT_EPSILON)) {
        axis.SetMin(temp_min, true);
    }
    ImGui::EndDisabled();

    ImGui::BeginDisabled(always_locked);
    ImGui::CheckboxFlags("##LockMax", (unsigned int*)&axis.Flags, ImPlot3DAxisFlags_LockMax);
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(axis.IsLockedMax() || always_locked);
    float temp_max = axis.Range.Max;
    if (ImGui::DragFloat("Max", &temp_max, (float)drag_speed, axis.Range.Min + FLT_EPSILON, HUGE_VAL)) {
        axis.SetMax(temp_max, true);
    }
    ImGui::EndDisabled();

    ImGui::Separator();

    // Flags
    ImGui::CheckboxFlags("Auto-Fit", (unsigned int*)&axis.Flags, ImPlot3DAxisFlags_AutoFit);
    ImGui::Separator();

    bool inverted = ImPlot3D::ImHasFlag(axis.Flags, ImPlot3DAxisFlags_Invert);
    if (ImGui::Checkbox("Invert", &inverted))
        ImFlipFlag(axis.Flags, ImPlot3DAxisFlags_Invert);

    ImGui::Separator();

    ImGui::BeginDisabled(axis.Label.empty());
    if (ImGui::Checkbox("Label", &label))
        ImFlipFlag(axis.Flags, ImPlot3DAxisFlags_NoLabel);
    ImGui::EndDisabled();

    if (ImGui::Checkbox("Grid Lines", &grid))
        ImFlipFlag(axis.Flags, ImPlot3DAxisFlags_NoGridLines);
    if (ImGui::Checkbox("Tick Marks", &ticks))
        ImFlipFlag(axis.Flags, ImPlot3DAxisFlags_NoTickMarks);
    if (ImGui::Checkbox("Tick Labels", &labels))
        ImFlipFlag(axis.Flags, ImPlot3DAxisFlags_NoTickLabels);
}

void ShowPlaneContextMenu(ImPlot3DPlot& plot, int plane_idx) {
    for (int i = 0; i < 3; i++) {
        if (i == plane_idx)
            continue;
        ImPlot3DAxis& axis = plot.Axes[i];
        ImGui::PushID(i);
        if (ImGui::BeginMenu(axis.HasLabel() ? axis.GetLabel() : axis_labels[i])) {
            ShowAxisContextMenu(axis);
            ImGui::EndMenu();
        }
        ImGui::PopID();
    }
}

void ShowPlotContextMenu(ImPlot3DPlot& plot) {
    char buf[16] = {};
    for (int i = 0; i < 3; i++) {
        ImPlot3DAxis& axis = plot.Axes[i];
        ImGui::PushID(i);
        ImFormatString(buf, sizeof(buf) - 1, i == 0 ? "X-Axis" : "X-Axis %d", i + 1);
        if (ImGui::BeginMenu(axis.HasLabel() ? axis.GetLabel() : axis_labels[i])) {
            ShowAxisContextMenu(axis);
            ImGui::EndMenu();
        }
        ImGui::PopID();
    }

    ImGui::Separator();

    if ((ImGui::BeginMenu("Box"))) {
        ImGui::PushItemWidth(75);
        float temp_scale[3] = {plot.BoxScale[0], plot.BoxScale[1], plot.BoxScale[2]};
        if (ImGui::DragFloat("Scale X", &temp_scale[0], 0.01f, 0.1f, 3.0f))
            plot.BoxScale[0] = ImMax(temp_scale[0], 0.01f);
        if (ImGui::DragFloat("Scale Y", &temp_scale[1], 0.01f, 0.1f, 3.0f))
            plot.BoxScale[1] = ImMax(temp_scale[1], 0.01f);
        if (ImGui::DragFloat("Scale Z", &temp_scale[2], 0.01f, 0.1f, 3.0f))
            plot.BoxScale[2] = ImMax(temp_scale[2], 0.01f);
        ImGui::PopItemWidth();
        ImGui::EndMenu();
    }

    ImGui::Separator();

    if ((ImGui::BeginMenu("Legend"))) {
        if (ShowLegendContextMenu(plot.Items.Legend, !ImPlot3D::ImHasFlag(plot.Flags, ImPlot3DFlags_NoLegend)))
            ImFlipFlag(plot.Flags, ImPlot3DFlags_NoLegend);
        ImGui::EndMenu();
    }

    if ((ImGui::BeginMenu("Settings"))) {
        ImGui::BeginDisabled(plot.Title.empty());
        if (ImGui::MenuItem("Title", nullptr, plot.HasTitle()))
            ImFlipFlag(plot.Flags, ImPlot3DFlags_NoTitle);
        ImGui::EndDisabled();
        if (ImGui::MenuItem("Clip", nullptr, !ImHasFlag(plot.Flags, ImPlot3DFlags_NoClip)))
            ImFlipFlag(plot.Flags, ImPlot3DFlags_NoClip);
        if (ImGui::MenuItem("Mouse Position", nullptr, !ImHasFlag(plot.Flags, ImPlot3DFlags_NoMouseText)))
            ImFlipFlag(plot.Flags, ImPlot3DFlags_NoMouseText);
        ImGui::EndMenu();
    }
}

//-----------------------------------------------------------------------------
// [SECTION] Begin/End Plot
//-----------------------------------------------------------------------------

bool BeginPlot(const char* title_id, const ImVec2& size, ImPlot3DFlags flags) {
    IMPLOT3D_CHECK_CTX();
    ImPlot3DContext& gp = *GImPlot3D;
    IM_ASSERT_USER_ERROR(gp.CurrentPlot == nullptr, "Mismatched BeginPlot()/EndPlot()!");

    // Get window
    ImGuiContext& g = *GImGui;
    ImGuiWindow* window = g.CurrentWindow;

    // Skip if needed
    if (window->SkipItems)
        return false;

    // Get or create plot
    const ImGuiID ID = window->GetID(title_id);
    const bool just_created = gp.Plots.GetByKey(ID) == nullptr;
    gp.CurrentPlot = gp.Plots.GetOrAddByKey(ID);
    gp.CurrentItems = &gp.CurrentPlot->Items;
    ImPlot3DPlot& plot = *gp.CurrentPlot;

    // Populate plot
    plot.ID = ID;
    plot.JustCreated = just_created;
    if (just_created) {
        plot.Rotation = plot.InitialRotation;
        plot.FitThisFrame = true;
        for (int i = 0; i < 3; i++) {
            plot.Axes[i] = ImPlot3DAxis();
            plot.Axes[i].FitThisFrame = true;
        }
    }
    if (plot.PreviousFlags != flags)
        plot.Flags = flags;
    plot.PreviousFlags = flags;
    plot.SetupLocked = false;
    plot.OpenContextThisFrame = false;
    plot.RotationCond = ImPlot3DCond_None;

    // Populate title
    plot.SetTitle(title_id);

    // Calculate frame size
    ImVec2 frame_size = ImGui::CalcItemSize(size, gp.Style.PlotDefaultSize.x, gp.Style.PlotDefaultSize.y);
    if (frame_size.x < gp.Style.PlotMinSize.x && size.x < 0.0f)
        frame_size.x = gp.Style.PlotMinSize.x;
    if (frame_size.y < gp.Style.PlotMinSize.y && size.y < 0.0f)
        frame_size.y = gp.Style.PlotMinSize.y;

    // Create plot ImGui item
    plot.FrameRect = ImRect(window->DC.CursorPos, window->DC.CursorPos + frame_size);
    ImGui::ItemSize(plot.FrameRect);
    if (!ImGui::ItemAdd(plot.FrameRect, plot.ID, &plot.FrameRect)) {
        gp.CurrentPlot = nullptr;
        gp.CurrentItems = nullptr;
        gp.CurrentItem = nullptr;
        return false;
    }

    // Reset legend
    plot.Items.Legend.Reset();

    // Reset axes
    for (int i = 0; i < ImAxis3D_COUNT; i++)
        plot.Axes[i].Reset();

    // Push frame rect clipping
    ImGui::PushClipRect(plot.FrameRect.Min, plot.FrameRect.Max, true);
    plot.DrawList._Flags = window->DrawList->Flags;
    plot.DrawList._SharedData = ImGui::GetDrawListSharedData();

    return true;
}

void EndPlot() {
    IMPLOT3D_CHECK_CTX();
    ImPlot3DContext& gp = *GImPlot3D;
    IM_ASSERT_USER_ERROR(gp.CurrentPlot != nullptr, "Mismatched BeginPlot()/EndPlot()!");
    ImPlot3DPlot& plot = *gp.CurrentPlot;

    // Move triangles from 3D draw list to ImGui draw list
    plot.DrawList.SortedMoveToImGuiDrawList();

    // Handle data fitting
    if (plot.FitThisFrame) {
        plot.FitThisFrame = false;
        for (int i = 0; i < 3; i++) {
            if (plot.Axes[i].FitThisFrame) {
                plot.Axes[i].FitThisFrame = false;
                plot.Axes[i].ApplyFit();
            }
        }
    }

    // Lock setup if not already done
    SetupLock();

    // Reset legend hover
    plot.Items.Legend.Hovered = false;

    // Render legend
    RenderLegend();

    // Render mouse position
    RenderMousePos();

    // Legend context menu
    if (ImGui::BeginPopup("##LegendContext")) {
        ImGui::Text("Legend");
        ImGui::Separator();
        if (ShowLegendContextMenu(plot.Items.Legend, !ImPlot3D::ImHasFlag(plot.Flags, ImPlot3DFlags_NoLegend)))
            ImFlipFlag(plot.Flags, ImPlot3DFlags_NoLegend);
        ImGui::EndPopup();
    }

    // Axis context menus
    for (int i = 0; i < 3; i++) {
        ImPlot3DAxis& axis = plot.Axes[i];
        if (ImGui::BeginPopup(axis_contexts[i])) {
            ImGui::Text(axis.HasLabel() ? axis.GetLabel() : "%c-Axis", 'X' + i);
            ImGui::Separator();
            ShowAxisContextMenu(axis);
            ImGui::EndPopup();
        }
    }

    // Plane context menus
    for (int i = 0; i < 3; i++) {
        if (ImGui::BeginPopup(plane_contexts[i])) {
            ImGui::Text("%s", plane_labels[i]);
            ImGui::Separator();
            ShowPlaneContextMenu(plot, i);
            ImGui::EndPopup();
        }
    }

    // Plot context menu
    if (ImGui::BeginPopup("##PlotContext")) {
        ShowPlotContextMenu(plot);
        ImGui::EndPopup();
    }

    // Pop frame rect clipping
    ImGui::PopClipRect();

    // Reset current plot
    gp.CurrentPlot = nullptr;
    gp.CurrentItems = nullptr;
    gp.CurrentItem = nullptr;

    // Reset the plot items for the next frame
    for (int i = 0; i < plot.Items.GetItemCount(); i++)
        plot.Items.GetItemByIndex(i)->SeenThisFrame = false;
}

//-----------------------------------------------------------------------------
// [SECTION] Setup
//-----------------------------------------------------------------------------

static const float ANIMATION_ANGULAR_VELOCITY = 2 * 3.1415f;

float CalcAnimationTime(ImPlot3DQuat q0, ImPlot3DQuat q1) {
    // Compute the angular distance between orientations
    float dot_product = ImClamp(q0.Dot(q1), -1.0f, 1.0f);
    float angle = 2.0f * acosf(fabsf(dot_product));

    // Calculate animation time for constant the angular velocity
    return angle / ANIMATION_ANGULAR_VELOCITY;
}

void SetupAxis(ImAxis3D idx, const char* label, ImPlot3DAxisFlags flags) {
    ImPlot3DContext& gp = *GImPlot3D;
    IM_ASSERT_USER_ERROR(gp.CurrentPlot != nullptr && !gp.CurrentPlot->SetupLocked,
                         "SetupAxis() needs to be called after BeginPlot() and before any setup locking functions (e.g. PlotX)!");

    // Get plot and axis
    ImPlot3DPlot& plot = *gp.CurrentPlot;
    ImPlot3DAxis& axis = plot.Axes[idx];
    if (axis.PreviousFlags != flags)
        axis.Flags = flags;
    axis.PreviousFlags = flags;
    axis.SetLabel(label);
}

void SetupAxisLimits(ImAxis3D idx, double min_lim, double max_lim, ImPlot3DCond cond) {
    ImPlot3DContext& gp = *GImPlot3D;
    IM_ASSERT_USER_ERROR(gp.CurrentPlot != nullptr && !gp.CurrentPlot->SetupLocked,
                         "SetupAxisLimits() needs to be called after BeginPlot and before any setup locking functions (e.g. PlotX)!");
    ImPlot3DPlot& plot = *gp.CurrentPlot;
    ImPlot3DAxis& axis = plot.Axes[idx];
    if (!plot.Initialized || cond == ImPlot3DCond_Always) {
        axis.SetRange(min_lim, max_lim);
        axis.RangeCond = cond;
        axis.FitThisFrame = false;
    }
}

void SetupAxisFormat(ImAxis3D idx, ImPlot3DFormatter formatter, void* data) {
    ImPlot3DContext& gp = *GImPlot3D;
    IM_ASSERT_USER_ERROR(gp.CurrentPlot != nullptr && !gp.CurrentPlot->SetupLocked,
                         "Setup needs to be called after BeginPlot and before any setup locking functions (e.g. PlotX)!");
    ImPlot3DPlot& plot = *gp.CurrentPlot;
    ImPlot3DAxis& axis = plot.Axes[idx];
    axis.Formatter = formatter;
    axis.FormatterData = data;
}

void SetupAxisTicks(ImAxis3D idx, const double* values, int n_ticks, const char* const labels[], bool keep_default) {
    ImPlot3DContext& gp = *GImPlot3D;
    IM_ASSERT_USER_ERROR(gp.CurrentPlot != nullptr && !gp.CurrentPlot->SetupLocked,
                         "Setup needs to be called after BeginPlot and before any setup locking functions (e.g. PlotX)!");
    ImPlot3DPlot& plot = *gp.CurrentPlot;
    ImPlot3DAxis& axis = plot.Axes[idx];
    axis.ShowDefaultTicks = keep_default;
    AddTicksCustom(values, labels, n_ticks, axis.Ticker, axis.Formatter ? axis.Formatter : Formatter_Default,
                   (axis.Formatter && axis.FormatterData) ? axis.FormatterData : (void*)IMPLOT3D_LABEL_FORMAT);
}

void SetupAxisTicks(ImAxis3D idx, double v_min, double v_max, int n_ticks, const char* const labels[], bool keep_default) {
    ImPlot3DContext& gp = *GImPlot3D;
    IM_ASSERT_USER_ERROR(gp.CurrentPlot != nullptr && !gp.CurrentPlot->SetupLocked,
                         "Setup needs to be called after BeginPlot and before any setup locking functions (e.g. PlotX)!");
    n_ticks = n_ticks < 2 ? 2 : n_ticks;
    ImVector<double> temp;
    FillRange(temp, n_ticks, v_min, v_max);
    SetupAxisTicks(idx, temp.Data, n_ticks, labels, keep_default);
}

void SetupAxisLimitsConstraints(ImAxis3D idx, double v_min, double v_max) {
    ImPlot3DContext& gp = *GImPlot3D;
    IM_ASSERT_USER_ERROR(gp.CurrentPlot != nullptr && !gp.CurrentPlot->SetupLocked,
                         "Setup needs to be called after BeginPlot and before any setup locking functions (e.g. PlotX)!");
    ImPlot3DPlot& plot = *gp.CurrentPlot;
    ImPlot3DAxis& axis = plot.Axes[idx];
    axis.ConstraintRange.Min = (float)v_min;
    axis.ConstraintRange.Max = (float)v_max;
}

void SetupAxisZoomConstraints(ImAxis3D idx, double z_min, double z_max) {
    ImPlot3DContext& gp = *GImPlot3D;
    IM_ASSERT_USER_ERROR(gp.CurrentPlot != nullptr && !gp.CurrentPlot->SetupLocked,
                         "Setup needs to be called after BeginPlot and before any setup locking functions (e.g. PlotX)!");
    ImPlot3DPlot& plot = *gp.CurrentPlot;
    ImPlot3DAxis& axis = plot.Axes[idx];
    axis.ConstraintZoom.Min = (float)z_min;
    axis.ConstraintZoom.Max = (float)z_max;
}

void SetupAxes(const char* x_label, const char* y_label, const char* z_label, ImPlot3DAxisFlags x_flags, ImPlot3DAxisFlags y_flags,
               ImPlot3DAxisFlags z_flags) {
    SetupAxis(ImAxis3D_X, x_label, x_flags);
    SetupAxis(ImAxis3D_Y, y_label, y_flags);
    SetupAxis(ImAxis3D_Z, z_label, z_flags);
}

void SetupAxesLimits(double x_min, double x_max, double y_min, double y_max, double z_min, double z_max, ImPlot3DCond cond) {
    SetupAxisLimits(ImAxis3D_X, x_min, x_max, cond);
    SetupAxisLimits(ImAxis3D_Y, y_min, y_max, cond);
    SetupAxisLimits(ImAxis3D_Z, z_min, z_max, cond);
    if (cond == ImPlot3DCond_Once)
        GImPlot3D->CurrentPlot->FitThisFrame = false;
}

void SetupBoxRotation(float elevation, float azimuth, bool animate, ImPlot3DCond cond) {
    // Convert angles from degrees to radians
    float elev_rad = elevation * IM_PI / 180.0f;
    float azim_rad = azimuth * IM_PI / 180.0f;

    // Call quaternion SetupBoxRotation
    SetupBoxRotation(ImPlot3DQuat::FromElAz(elev_rad, azim_rad), animate, cond);
}

void SetupBoxRotation(ImPlot3DQuat rotation, bool animate, ImPlot3DCond cond) {
    ImPlot3DContext& gp = *GImPlot3D;
    IM_ASSERT_USER_ERROR(gp.CurrentPlot != nullptr && !gp.CurrentPlot->SetupLocked,
                         "SetupBoxRotation() needs to be called after BeginPlot and before any setup locking functions (e.g. PlotX)!");
    ImPlot3DPlot& plot = *gp.CurrentPlot;

    if (!plot.Initialized || cond == ImPlot3DCond_Always) {
        if (!animate) {
            plot.Rotation = rotation;
            plot.AnimationTime = 0.0f; // Force any running rotation animation to stop
        } else {
            plot.RotationAnimationEnd = rotation;
            plot.AnimationTime = CalcAnimationTime(plot.Rotation, plot.RotationAnimationEnd);
        }
        plot.RotationCond = cond;
    }
}

void SetupBoxInitialRotation(float elevation, float azimuth) {
    // Convert angles from degrees to radians
    float elev_rad = elevation * IM_PI / 180.0f;
    float azim_rad = azimuth * IM_PI / 180.0f;

    // Call quaternion SetupBoxInitialRotation
    SetupBoxInitialRotation(ImPlot3DQuat::FromElAz(elev_rad, azim_rad));
}

void SetupBoxInitialRotation(ImPlot3DQuat rotation) {
    ImPlot3DContext& gp = *GImPlot3D;
    IM_ASSERT_USER_ERROR(gp.CurrentPlot != nullptr && !gp.CurrentPlot->SetupLocked,
                         "SetupBoxInitialRotation() needs to be called after BeginPlot and before any setup locking functions (e.g. PlotX)!");
    ImPlot3DPlot& plot = *gp.CurrentPlot;

    plot.InitialRotation = rotation;
}

void SetupBoxScale(float x, float y, float z) {
    ImPlot3DContext& gp = *GImPlot3D;
    IM_ASSERT_USER_ERROR(gp.CurrentPlot != nullptr && !gp.CurrentPlot->SetupLocked,
                         "SetupBoxScale() needs to be called after BeginPlot() and before any setup locking functions (e.g. PlotX)!");
    IM_ASSERT_USER_ERROR(x > 0.0f && y > 0.0f && z > 0.0f, "SetupBoxScale() requires all aspect ratios to be greater than 0!");
    ImPlot3DPlot& plot = *gp.CurrentPlot;
    plot.BoxScale = ImPlot3DPoint(x, y, z);
}

void SetupLegend(ImPlot3DLocation location, ImPlot3DLegendFlags flags) {
    ImPlot3DContext& gp = *GImPlot3D;
    IM_ASSERT_USER_ERROR(gp.CurrentPlot != nullptr && !gp.CurrentPlot->SetupLocked,
                         "SetupLegend() needs to be called after BeginPlot() and before any setup locking functions (e.g. PlotX)!");
    IM_ASSERT_USER_ERROR(gp.CurrentItems != nullptr, "SetupLegend() needs to be called within an itemized context!");
    ImPlot3DLegend& legend = gp.CurrentItems->Legend;
    if (legend.PreviousLocation != location)
        legend.Location = location;
    legend.PreviousLocation = location;
    if (legend.PreviousFlags != flags)
        legend.Flags = flags;
    legend.PreviousFlags = flags;
}

//-----------------------------------------------------------------------------
// [SECTION] Plot Utils
//-----------------------------------------------------------------------------

ImPlot3DPlot* GetCurrentPlot() { return GImPlot3D->CurrentPlot; }

void BustPlotCache() {
    ImPlot3DContext& gp = *GImPlot3D;
    gp.Plots.Clear();
}

ImVec2 PlotToPixels(const ImPlot3DPoint& point) {
    ImPlot3DContext& gp = *GImPlot3D;
    IM_ASSERT_USER_ERROR(gp.CurrentPlot != nullptr, "PlotToPixels() needs to be called between BeginPlot() and EndPlot()!");
    return NDCToPixels(PlotToNDC(point));
}

ImVec2 PlotToPixels(double x, double y, double z) { return PlotToPixels(ImPlot3DPoint((float)x, (float)y, (float)z)); }

ImPlot3DRay PixelsToPlotRay(const ImVec2& pix) {
    ImPlot3DContext& gp = *GImPlot3D;
    IM_ASSERT_USER_ERROR(gp.CurrentPlot != nullptr, "PixelsToPlotRay() needs to be called between BeginPlot() and EndPlot()!");
    return NDCRayToPlotRay(PixelsToNDCRay(pix));
}

ImPlot3DRay PixelsToPlotRay(double x, double y) { return PixelsToPlotRay(ImVec2((float)x, (float)y)); }

ImPlot3DPoint PixelsToPlotPlane(const ImVec2& pix, ImPlane3D plane, bool mask) {
    ImPlot3DContext& gp = *GImPlot3D;
    IM_ASSERT_USER_ERROR(gp.CurrentPlot != nullptr, "PixelsToPlotPlane() needs to be called between BeginPlot() and EndPlot()!");

    ImPlot3DPlot& plot = *gp.CurrentPlot;
    ImPlot3DRay ray = PixelsToNDCRay(pix);
    const ImPlot3DPoint& O = ray.Origin;
    const ImPlot3DPoint& D = ray.Direction;

    // Helper lambda to check intersection with a given coordinate and return intersection point if valid.
    auto IntersectPlane = [&](float coord) -> ImPlot3DPoint {
        // Solve for t in O[axis] + D[axis]*t = coord
        float denom = 0.0f;
        float numer = 0.0f;
        if (plane == ImPlane3D_YZ) {
            denom = D.x;
            numer = coord - O.x;
        } else if (plane == ImPlane3D_XZ) {
            denom = D.y;
            numer = coord - O.y;
        } else if (plane == ImPlane3D_XY) {
            denom = D.z;
            numer = coord - O.z;
        }

        if (ImAbs(denom) < 1e-12f) {
            // Ray is parallel or nearly parallel to the plane
            return ImPlot3DPoint(NAN, NAN, NAN);
        }

        float t = numer / denom;
        if (t < 0.0f) {
            // Intersection behind the ray origin
            return ImPlot3DPoint(NAN, NAN, NAN);
        }

        return O + D * t;
    };

    // Compute which plane to intersect with
    bool active_faces[3];
    ComputeActiveFaces(active_faces, plot.Rotation, plot.Axes);

    // Calculate intersection point with the planes
    ImPlot3DPoint P = IntersectPlane(active_faces[plane] ? 0.5f * plot.BoxScale[plane] : -0.5f * plot.BoxScale[plane]);
    if (P.IsNaN())
        return P;

    // Helper lambda to check if point P is within the plot box
    auto InRange = [&](const ImPlot3DPoint& P) {
        return P.x >= -0.5f * plot.BoxScale.x && P.x <= 0.5f * plot.BoxScale.x && P.y >= -0.5f * plot.BoxScale.y && P.y <= 0.5f * plot.BoxScale.y &&
               P.z >= -0.5f * plot.BoxScale.z && P.z <= 0.5f * plot.BoxScale.z;
    };

    // Handle mask (if one of the intersections is out of range, set it to NAN)
    if (mask) {
        switch (plane) {
            case ImPlane3D_YZ:
                if (!InRange(ImPlot3DPoint(0.0, P.y, P.z)))
                    return ImPlot3DPoint(NAN, NAN, NAN);
                break;
            case ImPlane3D_XZ:
                if (!InRange(ImPlot3DPoint(P.x, 0.0, P.z)))
                    return ImPlot3DPoint(NAN, NAN, NAN);
                break;
            case ImPlane3D_XY:
                if (!InRange(ImPlot3DPoint(P.x, P.y, 0.0)))
                    return ImPlot3DPoint(NAN, NAN, NAN);
                break;
        }
    }

    return NDCToPlot(P);
}

ImPlot3DPoint PixelsToPlotPlane(double x, double y, ImPlane3D plane, bool mask) { return PixelsToPlotPlane(ImVec2((float)x, (float)y), plane, mask); }

ImVec2 GetPlotPos() {
    ImPlot3DContext& gp = *GImPlot3D;
    IM_ASSERT_USER_ERROR(gp.CurrentPlot != nullptr, "GetPlotPos() needs to be called between BeginPlot() and EndPlot()!");
    SetupLock();
    return gp.CurrentPlot->PlotRect.Min;
}

ImVec2 GetPlotSize() {
    ImPlot3DContext& gp = *GImPlot3D;
    IM_ASSERT_USER_ERROR(gp.CurrentPlot != nullptr, "GetPlotSize() needs to be called between BeginPlot() and EndPlot()!");
    SetupLock();
    return gp.CurrentPlot->PlotRect.GetSize();
}

ImVec2 GetFramePos() {
    ImPlot3DContext& gp = *GImPlot3D;
    IM_ASSERT_USER_ERROR(gp.CurrentPlot != nullptr, "GetFramePos() needs to be called between BeginPlot() and EndPlot()!");
    return gp.CurrentPlot->FrameRect.Min;
}

ImVec2 GetFrameSize() {
    ImPlot3DContext& gp = *GImPlot3D;
    IM_ASSERT_USER_ERROR(gp.CurrentPlot != nullptr, "GetFrameSize() needs to be called between BeginPlot() and EndPlot()!");
    return gp.CurrentPlot->FrameRect.GetSize();
}

ImPlot3DPoint PlotToNDC(const ImPlot3DPoint& point) {
    ImPlot3DContext& gp = *GImPlot3D;
    IM_ASSERT_USER_ERROR(gp.CurrentPlot != nullptr, "PlotToNDC() needs to be called between BeginPlot() and EndPlot()!");
    ImPlot3DPlot& plot = *gp.CurrentPlot;
    SetupLock();

    return PlotToNDC(plot, point);
}

ImPlot3DPoint NDCToPlot(const ImPlot3DPoint& point) {
    ImPlot3DContext& gp = *GImPlot3D;
    IM_ASSERT_USER_ERROR(gp.CurrentPlot != nullptr, "NDCToPlot() needs to be called between BeginPlot() and EndPlot()!");
    ImPlot3DPlot& plot = *gp.CurrentPlot;
    SetupLock();

    ImPlot3DPoint plot_point;
    for (int i = 0; i < 3; i++) {
        ImPlot3DAxis& axis = plot.Axes[i];
        float ndc_range = 0.5f * plot.BoxScale[i];
        float t = ImPlot3D::ImHasFlag(axis.Flags, ImPlot3DAxisFlags_Invert) ? (ndc_range - point[i]) : (point[i] + ndc_range);
        t /= plot.BoxScale[i];
        plot_point[i] = axis.Range.Min + t * (axis.Range.Max - axis.Range.Min);
    }
    return plot_point;
}

ImVec2 NDCToPixels(const ImPlot3DPoint& point) {
    ImPlot3DContext& gp = *GImPlot3D;
    IM_ASSERT_USER_ERROR(gp.CurrentPlot != nullptr, "NDCToPixels() needs to be called between BeginPlot() and EndPlot()!");
    ImPlot3DPlot& plot = *gp.CurrentPlot;
    SetupLock();

    return NDCToPixels(plot, point);
}

ImPlot3DRay PixelsToNDCRay(const ImVec2& pix) {
    ImPlot3DContext& gp = *GImPlot3D;
    IM_ASSERT_USER_ERROR(gp.CurrentPlot != nullptr, "PixelsToNDCRay() needs to be called between BeginPlot() and EndPlot()!");
    ImPlot3DPlot& plot = *gp.CurrentPlot;
    SetupLock();

    // Calculate zoom factor and plot center
    float zoom = plot.GetBoxZoom();
    ImVec2 center = plot.PlotRect.GetCenter();

    // Undo screen transformations to get back to NDC space
    float x = (pix.x - center.x) / zoom;
    float y = -(pix.y - center.y) / zoom; // Invert y-axis

    // Define near and far points in NDC space along the z-axis
    ImPlot3DPoint ndc_near = plot.Rotation.Inverse() * ImPlot3DPoint(x, y, -10.0f);
    ImPlot3DPoint ndc_far = plot.Rotation.Inverse() * ImPlot3DPoint(x, y, 10.0f);

    // Create the ray in NDC space
    ImPlot3DRay ndc_ray;
    ndc_ray.Origin = ndc_near;
    ndc_ray.Direction = (ndc_far - ndc_near).Normalized();

    return ndc_ray;
}

ImPlot3DRay NDCRayToPlotRay(const ImPlot3DRay& ray) {
    ImPlot3DContext& gp = *GImPlot3D;
    IM_ASSERT_USER_ERROR(gp.CurrentPlot != nullptr, "NDCRayToPlotRay() needs to be called between BeginPlot() and EndPlot()!");
    SetupLock();

    // Convert NDC origin and a point along the ray to plot coordinates
    ImPlot3DPoint plot_origin = NDCToPlot(ray.Origin);
    ImPlot3DPoint ndc_point_along_ray = ray.Origin + ray.Direction;
    ImPlot3DPoint plot_point_along_ray = NDCToPlot(ndc_point_along_ray);

    // Compute the direction in plot coordinates
    ImPlot3DPoint plot_direction = (plot_point_along_ray - plot_origin).Normalized();

    // Create the ray in plot coordinates
    ImPlot3DRay plot_ray;
    plot_ray.Origin = plot_origin;
    plot_ray.Direction = plot_direction;

    return plot_ray;
}

//-----------------------------------------------------------------------------
// [SECTION] Setup Utils
//-----------------------------------------------------------------------------

static const float MOUSE_CURSOR_DRAG_THRESHOLD = 5.0f;

void HandleInput(ImPlot3DPlot& plot) {
    ImGuiIO& IO = ImGui::GetIO();

    // clang-format off
    const ImGuiButtonFlags plot_button_flags = ImGuiButtonFlags_AllowOverlap
                                             | ImGuiButtonFlags_PressedOnClick
                                             | ImGuiButtonFlags_PressedOnDoubleClick
                                             | ImGuiButtonFlags_MouseButtonLeft
                                             | ImGuiButtonFlags_MouseButtonRight
                                             | ImGuiButtonFlags_MouseButtonMiddle;
    // clang-format on
    const bool plot_clicked = ImGui::ButtonBehavior(plot.PlotRect, plot.ID, &plot.Hovered, &plot.Held, plot_button_flags);
#if (IMGUI_VERSION_NUM < 18966)
    ImGui::SetItemAllowOverlap(); // Handled by ButtonBehavior()
#endif

    // State
    const ImVec2 rot_drag = ImGui::GetMouseDragDelta(ImGuiMouseButton_Right);
    const bool rotating = ImLengthSqr(rot_drag) > MOUSE_CURSOR_DRAG_THRESHOLD;

    // Check if any axis/plane is hovered
    const ImPlot3DQuat& rotation = plot.Rotation;
    ImPlot3DPoint range_min = plot.RangeMin();
    ImPlot3DPoint range_max = plot.RangeMax();
    bool active_faces[3];
    int plane_2d = -1;
    ComputeActiveFaces(active_faces, rotation, plot.Axes, &plane_2d);
    ImPlot3DPoint corners[8];
    ComputeBoxCorners(corners, range_min, range_max);
    ImVec2 corners_pix[8];
    ComputeBoxCornersPix(plot, corners_pix, corners);
    int hovered_plane_idx = -1;
    int hovered_plane = GetMouseOverPlane(plot, active_faces, corners_pix, &hovered_plane_idx);
    int hovered_edge_idx = -1;
    int hovered_axis = GetMouseOverAxis(plot, active_faces, corners_pix, plane_2d, &hovered_edge_idx);
    if (hovered_axis != -1) {
        hovered_plane_idx = -1;
        hovered_plane = -1;
    }

    // If the user is no longer pressing the translation/zoom buttons, set axes as not held
    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left) && !ImGui::IsMouseDown(ImGuiMouseButton_Middle)) {
        for (int i = 0; i < 3; i++)
            plot.Axes[i].Held = false;
    }

    // Reset held edge/plane indices (it will be set if mouse button is down)
    if (!plot.Held) {
        plot.HeldEdgeIdx = -1;
        plot.HeldPlaneIdx = -1;
    }

    // Check which axes should be transformed (fit/zoom/translate)
    bool any_axis_held = plot.Axes[0].Held || plot.Axes[1].Held || plot.Axes[2].Held;
    if (!any_axis_held) {
        // Only update the transformation axes if the user is not already performing a transformation
        plot.Axes[0].Hovered = plot.Axes[1].Hovered = plot.Axes[2].Hovered = false;
        if (hovered_axis != -1) {
            plot.Axes[hovered_axis].Hovered = true;
        } else if (hovered_plane != -1) {
            plot.Axes[(hovered_plane + 1) % 3].Hovered = true;
            plot.Axes[(hovered_plane + 2) % 3].Hovered = true;
        } else {
            plot.Axes[0].Hovered = plot.Axes[1].Hovered = plot.Axes[2].Hovered = true;
        }
    }

    // Compute plane that is being hovered by mouse
    ImPlane3D mouse_plane = ImPlane3D_XY;
    if (plane_2d != -1)
        mouse_plane = plane_2d;
    else if (plot.Axes[1].Hovered && plot.Axes[2].Hovered)
        mouse_plane = ImPlane3D_YZ;
    else if (plot.Axes[0].Hovered && plot.Axes[2].Hovered)
        mouse_plane = ImPlane3D_XZ;
    else if (plot.Axes[2].Hovered)
        mouse_plane = ImPlane3D_YZ;
    ImVec2 mouse_pos = ImGui::GetMousePos();
    ImPlot3DPoint mouse_pos_plot = PixelsToPlotPlane(mouse_pos, mouse_plane, false);

    // Handle translation/zoom fit with double click
    if (plot_clicked && (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) || ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Middle))) {
        plot.FitThisFrame = true;
        for (int i = 0; i < 3; i++)
            plot.Axes[i].FitThisFrame = plot.Axes[i].Hovered;
    }

    // Handle auto fit
    for (int i = 0; i < 3; i++)
        if (plot.Axes[i].IsAutoFitting()) {
            plot.FitThisFrame = true;
            plot.Axes[i].FitThisFrame = true;
        }

    // Handle translation with right mouse button
    if (plot.Held && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        ImVec2 delta(IO.MouseDelta.x, IO.MouseDelta.y);

        if (plot.Axes[0].Hovered && plot.Axes[1].Hovered && plot.Axes[2].Hovered) {
            // Perform unconstrained translation (translate on the viewer plane)

            // Compute delta_pixels in 3D (invert y-axis)
            ImPlot3DPoint delta_pixels(delta.x, -delta.y, 0.0f);

            // Convert delta to NDC space
            float zoom = plot.GetBoxZoom();
            ImPlot3DPoint delta_NDC = plot.Rotation.Inverse() * (delta_pixels / zoom);

            // Convert delta to plot space
            ImPlot3DPoint delta_plot = delta_NDC * (plot.RangeMax() - plot.RangeMin()) / plot.BoxScale;

            // Adjust delta for inverted axes
            for (int i = 0; i < 3; i++) {
                if (ImHasFlag(plot.Axes[i].Flags, ImPlot3DAxisFlags_Invert))
                    delta_plot[i] *= -1;
            }

            // Adjust plot range to translate the plot
            for (int i = 0; i < 3; i++) {
                if (plot.Axes[i].Hovered) {
                    bool increasing = delta_plot[i] < 0.0f;
                    if (delta_plot[i] != 0.0f && !plot.Axes[i].IsPanLocked(increasing)) {
                        plot.Axes[i].SetMin(plot.Axes[i].Range.Min - delta_plot[i]);
                        plot.Axes[i].SetMax(plot.Axes[i].Range.Max - delta_plot[i]);
                    }
                    plot.Axes[i].Held = true;
                }
                // If no axis was held before (user started translating in this frame), set the held edge/plane indices
                if (!any_axis_held) {
                    plot.HeldEdgeIdx = hovered_edge_idx;
                    plot.HeldPlaneIdx = hovered_plane_idx;
                }
            }
        } else if (plot.Axes[0].Hovered || plot.Axes[1].Hovered || plot.Axes[2].Hovered) {
            // Translate along plane/axis

            // Mouse delta in pixels
            ImVec2 mouse_delta(IO.MouseDelta.x, IO.MouseDelta.y);

            ImPlot3DPoint mouse_delta_plot = PixelsToPlotPlane(mouse_pos + mouse_delta, mouse_plane, false);
            ImPlot3DPoint delta_plot = mouse_delta_plot - mouse_pos_plot;

            // Apply translation to the selected axes
            for (int i = 0; i < 3; i++) {
                if (plot.Axes[i].Hovered) {
                    bool increasing = delta_plot[i] < 0.0f;
                    if (delta_plot[i] != 0.0f && !plot.Axes[i].IsPanLocked(increasing)) {
                        plot.Axes[i].SetMin(plot.Axes[i].Range.Min - delta_plot[i]);
                        plot.Axes[i].SetMax(plot.Axes[i].Range.Max - delta_plot[i]);
                    }
                    plot.Axes[i].Held = true;
                }
                if (!any_axis_held) {
                    plot.HeldEdgeIdx = hovered_edge_idx;
                    plot.HeldPlaneIdx = hovered_plane_idx;
                }
            }
        }
    }

    // Handle context click with right mouse button
    if (plot.Held && ImGui::IsMouseClicked(ImGuiMouseButton_Right) && !ImPlot3D::ImHasFlag(plot.Flags, ImPlot3DFlags_NoMenus))
        plot.ContextClick = true;
    if (rotating || ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Right))
        plot.ContextClick = false;

    // Handle reset rotation with left mouse double click
    if (plot.Held && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Right) && !plot.IsRotationLocked()) {
        plot.RotationAnimationEnd = plot.Rotation;

        // Calculate rotation to align the z-axis with the camera direction
        if (hovered_plane == -1) {
            plot.RotationAnimationEnd = plot.InitialRotation;
        } else {
            // Compute plane normal
            ImPlot3DPoint axis_normal = ImPlot3DPoint(0.0f, 0.0f, 0.0f);
            axis_normal[hovered_plane] = active_faces[hovered_plane] ? -1.0f : 1.0f;
            if (ImPlot3D::ImHasFlag(plot.Axes[hovered_plane].Flags, ImPlot3DAxisFlags_Invert))
                axis_normal[hovered_plane] *= -1;

            // Compute rotation to align the plane normal with the z-axis
            ImPlot3DQuat align_normal = ImPlot3DQuat::FromTwoVectors(plot.RotationAnimationEnd * axis_normal, ImPlot3DPoint(0.0f, 0.0f, 1.0f));
            plot.RotationAnimationEnd = align_normal * plot.RotationAnimationEnd;

            if (hovered_plane != 2) {
                // Compute rotation to point z-axis up
                ImPlot3DQuat align_up =
                    ImPlot3DQuat::FromTwoVectors(plot.RotationAnimationEnd * ImPlot3DPoint(0.0f, 0.0f, 1.0f), ImPlot3DPoint(0.0f, 1.0f, 0.0f));
                plot.RotationAnimationEnd = align_up * plot.RotationAnimationEnd;
            } else {
                // Find the axis most aligned with the up direction
                ImPlot3DPoint up(0.0f, 1.0f, 0.0f);
                ImPlot3DPoint x_axis = plot.RotationAnimationEnd * ImPlot3DPoint(1.0f, 0.0f, 0.0f);
                ImPlot3DPoint y_axis = plot.RotationAnimationEnd * ImPlot3DPoint(0.0f, 1.0f, 0.0f);
                ImPlot3DPoint neg_x_axis = plot.RotationAnimationEnd * ImPlot3DPoint(-1.0f, 0.0f, 0.0f);
                ImPlot3DPoint neg_y_axis = plot.RotationAnimationEnd * ImPlot3DPoint(0.0f, -1.0f, 0.0f);

                struct AxisAlignment {
                    ImPlot3DPoint axis;
                    float dot;
                };

                AxisAlignment candidates[] = {
                    {x_axis, x_axis.Dot(up)},
                    {y_axis, y_axis.Dot(up)},
                    {neg_x_axis, neg_x_axis.Dot(up)},
                    {neg_y_axis, neg_y_axis.Dot(up)},
                };

                // Find the candidate with the maximum dot product
                AxisAlignment* best_candidate = &candidates[0];
                for (int i = 1; i < 4; i++) {
                    if (candidates[i].dot > best_candidate->dot) {
                        best_candidate = &candidates[i];
                    }
                }

                // Compute the rotation to align the best candidate with the up direction
                ImPlot3DQuat align_up = ImPlot3DQuat::FromTwoVectors(best_candidate->axis, up);
                plot.RotationAnimationEnd = align_up * plot.RotationAnimationEnd;
            }
        }

        // Calculate animation time
        plot.AnimationTime = CalcAnimationTime(plot.Rotation, plot.RotationAnimationEnd);
    }

    // Handle rotation with left mouse dragging
    if (plot.Held && ImGui::IsMouseDown(ImGuiMouseButton_Right) && !plot.IsRotationLocked()) {
        ImVec2 delta(IO.MouseDelta.x, IO.MouseDelta.y);

        // Map delta to rotation angles (in radians)
        float angle_x = delta.x * (3.1415f / 180.0f);
        float angle_y = delta.y * (3.1415f / 180.0f);

        // Detect if the plot is upside down by checking the transformed up vector
        ImPlot3DPoint up_vector = plot.Rotation * ImPlot3DPoint(0.0f, 0.0f, 1.0f);
        bool is_upside_down = up_vector.z < 0.0f;

        // Create quaternions for the rotations
        ImPlot3DQuat quat_x(angle_y, ImPlot3DPoint(1.0f, 0.0f, 0.0f));
        // Use -Z axis rotation when upside down to fix inverted rotation behavior
        ImPlot3DPoint z_axis = is_upside_down ? ImPlot3DPoint(0.0f, 0.0f, -1.0f) : ImPlot3DPoint(0.0f, 0.0f, 1.0f);
        ImPlot3DQuat quat_z(angle_x, z_axis);

        // Combine the new rotations with the current rotation
        plot.Rotation = quat_x * plot.Rotation * quat_z;
        plot.Rotation.Normalize();
    }

    // Handle zoom with mouse wheel
    if (plot.Hovered) {
        ImGui::SetKeyOwner(ImGuiKey_MouseWheelY, plot.ID);
        if (ImGui::IsMouseDown(ImGuiMouseButton_Middle) || IO.MouseWheel != 0.0f) {
            float delta = ImGui::IsMouseDown(ImGuiMouseButton_Middle) ? (-0.01f * IO.MouseDelta.y) : (-0.1f * IO.MouseWheel);
            float zoom = 1.0f + delta;
            for (int i = 0; i < 3; i++) {
                ImPlot3DAxis& axis = plot.Axes[i];
                float size = axis.Range.Max - axis.Range.Min;
                float new_min, new_max;
                if (hovered_axis != -1 || hovered_plane != -1) {
                    // If mouse over the plot box, zoom around the mouse plot position
                    float new_size = size * zoom;

                    // Calculate offset ratio of the mouse position relative to the axis range
                    float offset = mouse_pos_plot[i] - axis.Range.Min;
                    float ratio = offset / size;

                    // Adjust the axis range to zoom around the mouse position
                    new_min = mouse_pos_plot[i] - new_size * ratio;
                    new_max = mouse_pos_plot[i] + new_size * (1.0f - ratio);
                } else {
                    // If mouse is not over the plot box, zoom around the plot center
                    float center = (axis.Range.Min + axis.Range.Max) * 0.5f;

                    // Adjust the axis range to zoom around plot center
                    new_min = center - zoom * size * 0.5f;
                    new_max = center + zoom * size * 0.5f;
                }

                // Set new range after zoom
                if (plot.Axes[i].Hovered) {
                    if (!plot.Axes[i].IsInputLocked()) {
                        plot.Axes[i].SetMin(new_min);
                        plot.Axes[i].SetMax(new_max);
                    }
                    plot.Axes[i].Held = true;
                }

                // If no axis was held before (user started zoom in this frame), set the held edge/plane indices
                if (!any_axis_held) {
                    plot.HeldEdgeIdx = hovered_edge_idx;
                    plot.HeldPlaneIdx = hovered_plane_idx;
                }
            }
        }
    }

    // Handle context menu (should not happen if it is not a double click action)
    bool not_double_click = (float)(ImGui::GetTime() - IO.MouseClickedTime[ImGuiMouseButton_Right]) > IO.MouseDoubleClickTime;
    if (plot.Hovered && plot.ContextClick && not_double_click && !ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
        plot.ContextClick = false;
        plot.OpenContextThisFrame = true;
    }

    if (plot.OpenContextThisFrame) {
        if (plot.Items.Legend.Hovered)
            ImGui::OpenPopup("##LegendContext");
        else if (hovered_axis != -1) {
            ImGui::OpenPopup(axis_contexts[hovered_axis]);
        } else if (hovered_plane != -1) {
            ImGui::OpenPopup(plane_contexts[hovered_plane]);
        } else if (plot.Hovered) {
            ImGui::OpenPopup("##PlotContext");
        }
    }
}

void SetupLock() {
    ImPlot3DContext& gp = *GImPlot3D;
    IM_ASSERT_USER_ERROR(gp.CurrentPlot != nullptr, "SetupLock() needs to be called between BeginPlot() and EndPlot()!");
    ImPlot3DPlot& plot = *gp.CurrentPlot;
    if (plot.SetupLocked)
        return;
    // Lock setup
    plot.SetupLocked = true;

    ImGuiContext& g = *GImGui;
    ImGuiWindow* window = g.CurrentWindow;
    ImDrawList* draw_list = window->DrawList;

    ImGui::PushClipRect(plot.FrameRect.Min, plot.FrameRect.Max, true);

    // Set default formatter/locator
    for (int i = 0; i < 3; i++) {
        ImPlot3DAxis& axis = plot.Axes[i];

        // Set formatter
        if (axis.Formatter == nullptr) {
            axis.Formatter = Formatter_Default;
            if (axis.FormatterData == nullptr)
                axis.FormatterData = (void*)IMPLOT3D_LABEL_FORMAT;
        }

        // Set locator
        if (axis.Locator == nullptr)
            axis.Locator = Locator_Default;
    }

    // Draw frame background
    ImU32 f_bg_color = GetStyleColorU32(ImPlot3DCol_FrameBg);
    draw_list->AddRectFilled(plot.FrameRect.Min, plot.FrameRect.Max, f_bg_color);

    // Compute canvas/canvas rectangle
    plot.CanvasRect = ImRect(plot.FrameRect.Min + gp.Style.PlotPadding, plot.FrameRect.Max - gp.Style.PlotPadding);
    plot.PlotRect = plot.CanvasRect;

    // Compute ticks
    for (int i = 0; i < 3; i++) {
        ImPlot3DAxis& axis = plot.Axes[i];
        if (axis.ShowDefaultTicks) {
            float pixels = plot.GetBoxZoom() * plot.BoxScale[i];
            axis.Locator(axis.Ticker, axis.Range, pixels, axis.Formatter, axis.FormatterData);
        }
    }

    // Render title
    if (plot.HasTitle()) {
        ImU32 col = GetStyleColorU32(ImPlot3DCol_TitleText);
        ImVec2 top_center = ImVec2(plot.FrameRect.GetCenter().x, plot.CanvasRect.Min.y);
        AddTextCentered(draw_list, top_center, col, plot.GetTitle());
        plot.PlotRect.Min.y += ImGui::GetTextLineHeight() + gp.Style.LabelPadding.y;
    }

    // Handle animation
    if (plot.AnimationTime > 0.0f) {
        float dt = ImGui::GetIO().DeltaTime;
        float t = ImClamp(dt / plot.AnimationTime, 0.0f, 1.0f);
        plot.AnimationTime -= dt;
        if (plot.AnimationTime < 0.0f)
            plot.AnimationTime = 0.0f;
        plot.Rotation = ImPlot3DQuat::Slerp(plot.Rotation, plot.RotationAnimationEnd, t);
    }

    plot.Initialized = true;

    // Handle user input
    HandleInput(plot);

    // Render plot box
    RenderPlotBox(draw_list, plot);

    ImGui::PopClipRect();
}

//-----------------------------------------------------------------------------
// [SECTION] Miscellaneous
//-----------------------------------------------------------------------------

ImDrawList* GetPlotDrawList() { return ImGui::GetWindowDrawList(); }

//-----------------------------------------------------------------------------
// [SECTION] Styles
//-----------------------------------------------------------------------------

struct ImPlot3DStyleVarInfo {
    ImGuiDataType Type;
    ImU32 Count;
    ImU32 Offset;
    void* GetVarPtr(ImPlot3DStyle* style) const { return (void*)((unsigned char*)style + Offset); }
};

static const ImPlot3DStyleVarInfo GPlot3DStyleVarInfo[] = {
    // Item style
    {ImGuiDataType_Float, 1, (ImU32)offsetof(ImPlot3DStyle, LineWeight)},   // ImPlot3DStyleVar_LineWeight
    {ImGuiDataType_S32, 1, (ImU32)offsetof(ImPlot3DStyle, Marker)},         // ImPlot3DStyleVar_Marker
    {ImGuiDataType_Float, 1, (ImU32)offsetof(ImPlot3DStyle, MarkerSize)},   // ImPlot3DStyleVar_MarkerSize
    {ImGuiDataType_Float, 1, (ImU32)offsetof(ImPlot3DStyle, MarkerWeight)}, // ImPlot3DStyleVar_MarkerWeight
    {ImGuiDataType_Float, 1, (ImU32)offsetof(ImPlot3DStyle, FillAlpha)},    // ImPlot3DStyleVar_FillAlpha

    // Plot style
    {ImGuiDataType_Float, 2, (ImU32)offsetof(ImPlot3DStyle, PlotDefaultSize)}, // ImPlot3DStyleVar_Plot3DDefaultSize
    {ImGuiDataType_Float, 2, (ImU32)offsetof(ImPlot3DStyle, PlotMinSize)},     // ImPlot3DStyleVar_Plot3DMinSize
    {ImGuiDataType_Float, 2, (ImU32)offsetof(ImPlot3DStyle, PlotPadding)},     // ImPlot3DStyleVar_Plot3DPadding

    // Label style
    {ImGuiDataType_Float, 2, (ImU32)offsetof(ImPlot3DStyle, LabelPadding)},       // ImPlot3DStyleVar_LabelPaddine
    {ImGuiDataType_Float, 2, (ImU32)offsetof(ImPlot3DStyle, LegendPadding)},      // ImPlot3DStyleVar_LegendPadding
    {ImGuiDataType_Float, 2, (ImU32)offsetof(ImPlot3DStyle, LegendInnerPadding)}, // ImPlot3DStyleVar_LegendInnerPadding
    {ImGuiDataType_Float, 2, (ImU32)offsetof(ImPlot3DStyle, LegendSpacing)},      // ImPlot3DStyleVar_LegendSpacing
};

static const ImPlot3DStyleVarInfo* GetPlotStyleVarInfo(ImPlot3DStyleVar idx) {
    IM_ASSERT(idx >= 0 && idx < ImPlot3DStyleVar_COUNT);
    IM_ASSERT(IM_ARRAYSIZE(GPlot3DStyleVarInfo) == ImPlot3DStyleVar_COUNT);
    return &GPlot3DStyleVarInfo[idx];
}

ImPlot3DStyle& GetStyle() { return GImPlot3D->Style; }
void SetStyle(const ImPlot3DStyle& style) { GImPlot3D->Style = style; }

void StyleColorsAuto(ImPlot3DStyle* dst) {
    ImPlot3DStyle* style = dst ? dst : &ImPlot3D::GetStyle();
    ImVec4* colors = style->Colors;

    colors[ImPlot3DCol_Line] = IMPLOT3D_AUTO_COL;
    colors[ImPlot3DCol_Fill] = IMPLOT3D_AUTO_COL;
    colors[ImPlot3DCol_MarkerOutline] = IMPLOT3D_AUTO_COL;
    colors[ImPlot3DCol_MarkerFill] = IMPLOT3D_AUTO_COL;
    colors[ImPlot3DCol_TitleText] = IMPLOT3D_AUTO_COL;
    colors[ImPlot3DCol_InlayText] = IMPLOT3D_AUTO_COL;
    colors[ImPlot3DCol_FrameBg] = IMPLOT3D_AUTO_COL;
    colors[ImPlot3DCol_PlotBg] = IMPLOT3D_AUTO_COL;
    colors[ImPlot3DCol_PlotBorder] = IMPLOT3D_AUTO_COL;
    colors[ImPlot3DCol_LegendBg] = IMPLOT3D_AUTO_COL;
    colors[ImPlot3DCol_LegendBorder] = IMPLOT3D_AUTO_COL;
    colors[ImPlot3DCol_LegendText] = IMPLOT3D_AUTO_COL;
    colors[ImPlot3DCol_AxisText] = IMPLOT3D_AUTO_COL;
    colors[ImPlot3DCol_AxisGrid] = IMPLOT3D_AUTO_COL;
    colors[ImPlot3DCol_AxisTick] = IMPLOT3D_AUTO_COL;
}

void StyleColorsDark(ImPlot3DStyle* dst) {
    ImPlot3DStyle* style = dst ? dst : &ImPlot3D::GetStyle();
    ImVec4* colors = style->Colors;

    colors[ImPlot3DCol_Line] = IMPLOT3D_AUTO_COL;
    colors[ImPlot3DCol_Fill] = IMPLOT3D_AUTO_COL;
    colors[ImPlot3DCol_MarkerOutline] = IMPLOT3D_AUTO_COL;
    colors[ImPlot3DCol_MarkerFill] = IMPLOT3D_AUTO_COL;
    colors[ImPlot3DCol_TitleText] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
    colors[ImPlot3DCol_InlayText] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
    colors[ImPlot3DCol_FrameBg] = ImVec4(1.00f, 1.00f, 1.00f, 0.07f);
    colors[ImPlot3DCol_PlotBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.50f);
    colors[ImPlot3DCol_PlotBorder] = ImVec4(0.43f, 0.43f, 0.50f, 0.50f);
    colors[ImPlot3DCol_LegendBg] = ImVec4(0.08f, 0.08f, 0.08f, 0.94f);
    colors[ImPlot3DCol_LegendBorder] = ImVec4(0.43f, 0.43f, 0.50f, 0.50f);
    colors[ImPlot3DCol_LegendText] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
    colors[ImPlot3DCol_AxisText] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
    colors[ImPlot3DCol_AxisGrid] = ImVec4(1.00f, 1.00f, 1.00f, 0.25f);
    colors[ImPlot3DCol_AxisTick] = IMPLOT3D_AUTO_COL;
}

void StyleColorsLight(ImPlot3DStyle* dst) {
    ImPlot3DStyle* style = dst ? dst : &ImPlot3D::GetStyle();
    ImVec4* colors = style->Colors;

    colors[ImPlot3DCol_Line] = IMPLOT3D_AUTO_COL;
    colors[ImPlot3DCol_Fill] = IMPLOT3D_AUTO_COL;
    colors[ImPlot3DCol_MarkerOutline] = IMPLOT3D_AUTO_COL;
    colors[ImPlot3DCol_MarkerFill] = IMPLOT3D_AUTO_COL;
    colors[ImPlot3DCol_TitleText] = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
    colors[ImPlot3DCol_InlayText] = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
    colors[ImPlot3DCol_FrameBg] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
    colors[ImPlot3DCol_PlotBg] = ImVec4(0.42f, 0.57f, 1.00f, 0.13f);
    colors[ImPlot3DCol_PlotBorder] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImPlot3DCol_LegendBg] = ImVec4(1.00f, 1.00f, 1.00f, 0.98f);
    colors[ImPlot3DCol_LegendBorder] = ImVec4(0.82f, 0.82f, 0.82f, 0.80f);
    colors[ImPlot3DCol_LegendText] = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
    colors[ImPlot3DCol_AxisText] = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
    colors[ImPlot3DCol_AxisGrid] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
    colors[ImPlot3DCol_AxisTick] = IMPLOT3D_AUTO_COL;
}

void StyleColorsClassic(ImPlot3DStyle* dst) {
    ImPlot3DStyle* style = dst ? dst : &ImPlot3D::GetStyle();
    ImVec4* colors = style->Colors;

    colors[ImPlot3DCol_Line] = IMPLOT3D_AUTO_COL;
    colors[ImPlot3DCol_Fill] = IMPLOT3D_AUTO_COL;
    colors[ImPlot3DCol_MarkerOutline] = IMPLOT3D_AUTO_COL;
    colors[ImPlot3DCol_MarkerFill] = IMPLOT3D_AUTO_COL;
    colors[ImPlot3DCol_TitleText] = ImVec4(0.90f, 0.90f, 0.90f, 1.00f);
    colors[ImPlot3DCol_InlayText] = ImVec4(0.90f, 0.90f, 0.90f, 1.00f);
    colors[ImPlot3DCol_FrameBg] = ImVec4(0.43f, 0.43f, 0.43f, 0.39f);
    colors[ImPlot3DCol_PlotBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.35f);
    colors[ImPlot3DCol_PlotBorder] = ImVec4(0.50f, 0.50f, 0.50f, 0.50f);
    colors[ImPlot3DCol_LegendBg] = ImVec4(0.11f, 0.11f, 0.14f, 0.92f);
    colors[ImPlot3DCol_LegendBorder] = ImVec4(0.50f, 0.50f, 0.50f, 0.50f);
    colors[ImPlot3DCol_LegendText] = ImVec4(0.90f, 0.90f, 0.90f, 1.00f);
    colors[ImPlot3DCol_AxisText] = ImVec4(0.90f, 0.90f, 0.90f, 1.00f);
    colors[ImPlot3DCol_AxisGrid] = ImVec4(0.90f, 0.90f, 0.90f, 0.25f);
    colors[ImPlot3DCol_AxisTick] = IMPLOT3D_AUTO_COL;
}

void PushStyleColor(ImPlot3DCol idx, ImU32 col) {
    ImPlot3DContext& gp = *GImPlot3D;
    ImGuiColorMod backup;
    backup.Col = (ImGuiCol)idx;
    backup.BackupValue = gp.Style.Colors[idx];
    gp.ColorModifiers.push_back(backup);
    gp.Style.Colors[idx] = ImGui::ColorConvertU32ToFloat4(col);
}

void PushStyleColor(ImPlot3DCol idx, const ImVec4& col) {
    ImPlot3DContext& gp = *GImPlot3D;
    ImGuiColorMod backup;
    backup.Col = (ImGuiCol)idx;
    backup.BackupValue = gp.Style.Colors[idx];
    gp.ColorModifiers.push_back(backup);
    gp.Style.Colors[idx] = col;
}

void PopStyleColor(int count) {
    ImPlot3DContext& gp = *GImPlot3D;
    IM_ASSERT_USER_ERROR(count <= gp.ColorModifiers.Size, "You can't pop more modifiers than have been pushed!");
    while (count > 0) {
        ImGuiColorMod& backup = gp.ColorModifiers.back();
        gp.Style.Colors[backup.Col] = backup.BackupValue;
        gp.ColorModifiers.pop_back();
        count--;
    }
}

void PushStyleVar(ImPlot3DStyleVar idx, float val) {
    ImPlot3DContext& gp = *GImPlot3D;
    const ImPlot3DStyleVarInfo* var_info = GetPlotStyleVarInfo(idx);
    if (var_info->Type == ImGuiDataType_Float && var_info->Count == 1) {
        float* pvar = (float*)var_info->GetVarPtr(&gp.Style);
        gp.StyleModifiers.push_back(ImGuiStyleMod((ImGuiStyleVar)idx, *pvar));
        *pvar = val;
        return;
    }
    IM_ASSERT(0 && "Called PushStyleVar() float variant but variable is not a float!");
}

void PushStyleVar(ImPlot3DStyleVar idx, int val) {
    ImPlot3DContext& gp = *GImPlot3D;
    const ImPlot3DStyleVarInfo* var_info = GetPlotStyleVarInfo(idx);
    if (var_info->Type == ImGuiDataType_S32 && var_info->Count == 1) {
        int* pvar = (int*)var_info->GetVarPtr(&gp.Style);
        gp.StyleModifiers.push_back(ImGuiStyleMod((ImGuiStyleVar)idx, *pvar));
        *pvar = val;
        return;
    } else if (var_info->Type == ImGuiDataType_Float && var_info->Count == 1) {
        float* pvar = (float*)var_info->GetVarPtr(&gp.Style);
        gp.StyleModifiers.push_back(ImGuiStyleMod((ImGuiStyleVar)idx, *pvar));
        *pvar = (float)val;
        return;
    }
    IM_ASSERT(0 && "Called PushStyleVar() int variant but variable is not a int!");
}

void PushStyleVar(ImPlot3DStyleVar idx, const ImVec2& val) {
    ImPlot3DContext& gp = *GImPlot3D;
    const ImPlot3DStyleVarInfo* var_info = GetPlotStyleVarInfo(idx);
    if (var_info->Type == ImGuiDataType_Float && var_info->Count == 2) {
        ImVec2* pvar = (ImVec2*)var_info->GetVarPtr(&gp.Style);
        gp.StyleModifiers.push_back(ImGuiStyleMod((ImGuiStyleVar)idx, *pvar));
        *pvar = val;
        return;
    }
    IM_ASSERT(0 && "Called PushStyleVar() ImVec2 variant but variable is not a ImVec2!");
}

void PopStyleVar(int count) {
    ImPlot3DContext& gp = *GImPlot3D;
    IM_ASSERT_USER_ERROR(count <= gp.StyleModifiers.Size, "You can't pop more modifiers than have been pushed!");
    while (count > 0) {
        ImGuiStyleMod& backup = gp.StyleModifiers.back();
        const ImPlot3DStyleVarInfo* info = GetPlotStyleVarInfo(backup.VarIdx);
        void* data = info->GetVarPtr(&gp.Style);
        if (info->Type == ImGuiDataType_Float && info->Count == 1) {
            ((float*)data)[0] = backup.BackupFloat[0];
        } else if (info->Type == ImGuiDataType_Float && info->Count == 2) {
            ((float*)data)[0] = backup.BackupFloat[0];
            ((float*)data)[1] = backup.BackupFloat[1];
        } else if (info->Type == ImGuiDataType_S32 && info->Count == 1) {
            ((int*)data)[0] = backup.BackupInt[0];
        }
        gp.StyleModifiers.pop_back();
        count--;
    }
}

ImVec4 GetStyleColorVec4(ImPlot3DCol idx) { return IsColorAuto(idx) ? GetAutoColor(idx) : GImPlot3D->Style.Colors[idx]; }

ImU32 GetStyleColorU32(ImPlot3DCol idx) { return ImGui::ColorConvertFloat4ToU32(ImPlot3D::GetStyleColorVec4(idx)); }

//------------------------------------------------------------------------------
// [SECTION] Colormaps
//------------------------------------------------------------------------------

ImPlot3DColormap AddColormap(const char* name, const ImVec4* colormap, int size, bool qual) {
    ImPlot3DContext& gp = *GImPlot3D;
    IM_ASSERT_USER_ERROR(size > 1, "The colormap size must be greater than 1!");
    IM_ASSERT_USER_ERROR(gp.ColormapData.GetIndex(name) == -1, "The colormap name has already been used!");
    ImVector<ImU32> buffer;
    buffer.resize(size);
    for (int i = 0; i < size; i++)
        buffer[i] = ImGui::ColorConvertFloat4ToU32(colormap[i]);
    return gp.ColormapData.Append(name, buffer.Data, size, qual);
}

ImPlot3DColormap AddColormap(const char* name, const ImU32* colormap, int size, bool qual) {
    ImPlot3DContext& gp = *GImPlot3D;
    IM_ASSERT_USER_ERROR(size > 1, "The colormap size must be greater than 1!");
    IM_ASSERT_USER_ERROR(gp.ColormapData.GetIndex(name) == -1, "The colormap name has already be used!");
    return gp.ColormapData.Append(name, colormap, size, qual);
}

int GetColormapCount() {
    ImPlot3DContext& gp = *GImPlot3D;
    return gp.ColormapData.Count;
}

const char* GetColormapName(ImPlot3DColormap colormap) {
    ImPlot3DContext& gp = *GImPlot3D;
    return gp.ColormapData.GetName(colormap);
}

ImPlot3DColormap GetColormapIndex(const char* name) {
    ImPlot3DContext& gp = *GImPlot3D;
    return gp.ColormapData.GetIndex(name);
}

void PushColormap(ImPlot3DColormap colormap) {
    ImPlot3DContext& gp = *GImPlot3D;
    IM_ASSERT_USER_ERROR(colormap >= 0 && colormap < gp.ColormapData.Count, "The colormap index is invalid!");
    gp.ColormapModifiers.push_back(gp.Style.Colormap);
    gp.Style.Colormap = colormap;
}

void PushColormap(const char* name) {
    ImPlot3DContext& gp = *GImPlot3D;
    ImPlot3DColormap idx = gp.ColormapData.GetIndex(name);
    IM_ASSERT_USER_ERROR(idx != -1, "The colormap name is invalid!");
    PushColormap(idx);
}

void PopColormap(int count) {
    ImPlot3DContext& gp = *GImPlot3D;
    IM_ASSERT_USER_ERROR(count <= gp.ColormapModifiers.Size, "You can't pop more modifiers than have been pushed!");
    while (count > 0) {
        const ImPlot3DColormap& backup = gp.ColormapModifiers.back();
        gp.Style.Colormap = backup;
        gp.ColormapModifiers.pop_back();
        count--;
    }
}

ImU32 NextColormapColorU32() {
    ImPlot3DContext& gp = *GImPlot3D;
    IM_ASSERT_USER_ERROR(gp.CurrentItems != nullptr, "NextColormapColor() needs to be called between BeginPlot() and EndPlot()!");
    int idx = gp.CurrentItems->ColormapIdx % gp.ColormapData.GetKeyCount(gp.Style.Colormap);
    ImU32 col = gp.ColormapData.GetKeyColor(gp.Style.Colormap, idx);
    gp.CurrentItems->ColormapIdx++;
    return col;
}

ImVec4 NextColormapColor() { return ImGui::ColorConvertU32ToFloat4(NextColormapColorU32()); }

int GetColormapSize(ImPlot3DColormap cmap) {
    ImPlot3DContext& gp = *GImPlot3D;
    cmap = cmap == IMPLOT3D_AUTO ? gp.Style.Colormap : cmap;
    IM_ASSERT_USER_ERROR(cmap >= 0 && cmap < gp.ColormapData.Count, "Invalid colormap index!");
    return gp.ColormapData.GetKeyCount(cmap);
}

ImU32 GetColormapColorU32(int idx, ImPlot3DColormap cmap) {
    ImPlot3DContext& gp = *GImPlot3D;
    cmap = cmap == IMPLOT3D_AUTO ? gp.Style.Colormap : cmap;
    IM_ASSERT_USER_ERROR(cmap >= 0 && cmap < gp.ColormapData.Count, "Invalid colormap index!");
    idx = idx % gp.ColormapData.GetKeyCount(cmap);
    return gp.ColormapData.GetKeyColor(cmap, idx);
}

ImVec4 GetColormapColor(int idx, ImPlot3DColormap cmap) { return ImGui::ColorConvertU32ToFloat4(GetColormapColorU32(idx, cmap)); }

ImU32 SampleColormapU32(float t, ImPlot3DColormap cmap) {
    ImPlot3DContext& gp = *GImPlot3D;
    cmap = cmap == IMPLOT3D_AUTO ? gp.Style.Colormap : cmap;
    IM_ASSERT_USER_ERROR(cmap >= 0 && cmap < gp.ColormapData.Count, "Invalid colormap index!");
    return gp.ColormapData.LerpTable(cmap, t);
}

ImVec4 SampleColormap(float t, ImPlot3DColormap cmap) { return ImGui::ColorConvertU32ToFloat4(SampleColormapU32(t, cmap)); }

void RenderColorBar(const ImU32* colors, int size, ImDrawList& DrawList, const ImRect& bounds, bool vert, bool reversed, bool continuous) {
    const int n = continuous ? size - 1 : size;
    ImU32 col1, col2;
    if (vert) {
        const float step = bounds.GetHeight() / n;
        ImRect rect(bounds.Min.x, bounds.Min.y, bounds.Max.x, bounds.Min.y + step);
        for (int i = 0; i < n; ++i) {
            if (reversed) {
                col1 = colors[size - i - 1];
                col2 = continuous ? colors[size - i - 2] : col1;
            } else {
                col1 = colors[i];
                col2 = continuous ? colors[i + 1] : col1;
            }
            DrawList.AddRectFilledMultiColor(rect.Min, rect.Max, col1, col1, col2, col2);
            rect.TranslateY(step);
        }
    } else {
        const float step = bounds.GetWidth() / n;
        ImRect rect(bounds.Min.x, bounds.Min.y, bounds.Min.x + step, bounds.Max.y);
        for (int i = 0; i < n; ++i) {
            if (reversed) {
                col1 = colors[size - i - 1];
                col2 = continuous ? colors[size - i - 2] : col1;
            } else {
                col1 = colors[i];
                col2 = continuous ? colors[i + 1] : col1;
            }
            DrawList.AddRectFilledMultiColor(rect.Min, rect.Max, col1, col2, col2, col1);
            rect.TranslateX(step);
        }
    }
}

bool ColormapSlider(const char* label, float* t, ImVec4* out, const char* format, ImPlot3DColormap cmap) {
    *t = ImClamp(*t, 0.0f, 1.0f);
    ImGuiContext& G = *GImGui;
    ImGuiWindow* Window = G.CurrentWindow;
    if (Window->SkipItems)
        return false;
    ImPlot3DContext& gp = *GImPlot3D;
    cmap = cmap == IMPLOT3D_AUTO ? gp.Style.Colormap : cmap;
    IM_ASSERT_USER_ERROR(cmap >= 0 && cmap < gp.ColormapData.Count, "Invalid colormap index!");
    const ImU32* keys = gp.ColormapData.GetKeys(cmap);
    const int count = gp.ColormapData.GetKeyCount(cmap);
    const bool qual = gp.ColormapData.IsQual(cmap);
    const ImVec2 pos = ImGui::GetCurrentWindow()->DC.CursorPos;
    const float w = ImGui::CalcItemWidth();
    const float h = ImGui::GetFrameHeight();
    const ImRect rect = ImRect(pos.x, pos.y, pos.x + w, pos.y + h);
    RenderColorBar(keys, count, *ImGui::GetWindowDrawList(), rect, false, false, !qual);
    const ImU32 grab = CalcTextColor(gp.ColormapData.LerpTable(cmap, *t));
    // const ImU32 text = CalcTextColor(gp.ColormapData.LerpTable(cmap,0.5f));
    ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32_BLACK_TRANS);
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, IM_COL32_BLACK_TRANS);
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(1, 1, 1, 0.1f));
    ImGui::PushStyleColor(ImGuiCol_SliderGrab, grab);
    ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, grab);
    ImGui::PushStyleVar(ImGuiStyleVar_GrabMinSize, 2);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0);
    const bool changed = ImGui::SliderFloat(label, t, 0, 1, format);
    ImGui::PopStyleColor(5);
    ImGui::PopStyleVar(2);
    if (out != nullptr)
        *out = ImGui::ColorConvertU32ToFloat4(gp.ColormapData.LerpTable(cmap, *t));
    return changed;
}

//-----------------------------------------------------------------------------
// [SECTION] Context Utils
//-----------------------------------------------------------------------------

#define IMPLOT3D_APPEND_CMAP(name, qual) ctx->ColormapData.Append(#name, name, sizeof(name) / sizeof(ImU32), qual)
#define IM_RGB(r, g, b) IM_COL32(r, g, b, 255)

void InitializeContext(ImPlot3DContext* ctx) {
    ResetContext(ctx);

    const ImU32 Deep[] = {4289753676, 4283598045, 4285048917, 4283584196, 4289950337, 4284512403, 4291005402, 4287401100, 4285839820, 4291671396};
    const ImU32 Dark[] = {4280031972, 4290281015, 4283084621, 4288892568, 4278222847, 4281597951, 4280833702, 4290740727, 4288256409};
    const ImU32 Pastel[] = {4289639675, 4293119411, 4291161036, 4293184478, 4289124862, 4291624959, 4290631909, 4293712637, 4294111986};
    const ImU32 Paired[] = {4293119554, 4290017311, 4287291314, 4281114675, 4288256763, 4280031971,
                            4285513725, 4278222847, 4292260554, 4288298346, 4288282623, 4280834481};
    const ImU32 Viridis[] = {4283695428, 4285867080, 4287054913, 4287455029, 4287526954, 4287402273,
                             4286883874, 4285579076, 4283552122, 4280737725, 4280674301};
    const ImU32 Plasma[] = {4287039501, 4288480321, 4289200234, 4288941455, 4287638193, 4286072780,
                            4284638433, 4283139314, 4281771772, 4280667900, 4280416752};
    const ImU32 Hot[] = {4278190144, 4278190208, 4278190271, 4278190335, 4278206719, 4278223103,
                         4278239231, 4278255615, 4283826175, 4289396735, 4294967295};
    const ImU32 Cool[] = {4294967040, 4294960666, 4294954035, 4294947661, 4294941030, 4294934656,
                          4294928025, 4294921651, 4294915020, 4294908646, 4294902015};
    const ImU32 Pink[] = {4278190154, 4282532475, 4284308894, 4285690554, 4286879686, 4287870160,
                          4288794330, 4289651940, 4291685869, 4293392118, 4294967295};
    const ImU32 Jet[] = {4289331200, 4294901760, 4294923520, 4294945280, 4294967040, 4289396565,
                         4283826090, 4278255615, 4278233855, 4278212095, 4278190335};
    const ImU32 Twilight[] = {IM_RGB(226, 217, 226), IM_RGB(166, 191, 202), IM_RGB(109, 144, 192), IM_RGB(95, 88, 176),
                              IM_RGB(83, 30, 124),   IM_RGB(47, 20, 54),    IM_RGB(100, 25, 75),   IM_RGB(159, 60, 80),
                              IM_RGB(192, 117, 94),  IM_RGB(208, 179, 158), IM_RGB(226, 217, 226)};
    const ImU32 RdBu[] = {IM_RGB(103, 0, 31),    IM_RGB(178, 24, 43),   IM_RGB(214, 96, 77),   IM_RGB(244, 165, 130),
                          IM_RGB(253, 219, 199), IM_RGB(247, 247, 247), IM_RGB(209, 229, 240), IM_RGB(146, 197, 222),
                          IM_RGB(67, 147, 195),  IM_RGB(33, 102, 172),  IM_RGB(5, 48, 97)};
    const ImU32 BrBG[] = {IM_RGB(84, 48, 5),     IM_RGB(140, 81, 10),   IM_RGB(191, 129, 45),  IM_RGB(223, 194, 125),
                          IM_RGB(246, 232, 195), IM_RGB(245, 245, 245), IM_RGB(199, 234, 229), IM_RGB(128, 205, 193),
                          IM_RGB(53, 151, 143),  IM_RGB(1, 102, 94),    IM_RGB(0, 60, 48)};
    const ImU32 PiYG[] = {IM_RGB(142, 1, 82),    IM_RGB(197, 27, 125),  IM_RGB(222, 119, 174), IM_RGB(241, 182, 218),
                          IM_RGB(253, 224, 239), IM_RGB(247, 247, 247), IM_RGB(230, 245, 208), IM_RGB(184, 225, 134),
                          IM_RGB(127, 188, 65),  IM_RGB(77, 146, 33),   IM_RGB(39, 100, 25)};
    const ImU32 Spectral[] = {IM_RGB(158, 1, 66),    IM_RGB(213, 62, 79),   IM_RGB(244, 109, 67),  IM_RGB(253, 174, 97),
                              IM_RGB(254, 224, 139), IM_RGB(255, 255, 191), IM_RGB(230, 245, 152), IM_RGB(171, 221, 164),
                              IM_RGB(102, 194, 165), IM_RGB(50, 136, 189),  IM_RGB(94, 79, 162)};
    const ImU32 Greys[] = {IM_COL32_WHITE, IM_COL32_BLACK};

    IMPLOT3D_APPEND_CMAP(Deep, true);
    IMPLOT3D_APPEND_CMAP(Dark, true);
    IMPLOT3D_APPEND_CMAP(Pastel, true);
    IMPLOT3D_APPEND_CMAP(Paired, true);
    IMPLOT3D_APPEND_CMAP(Viridis, false);
    IMPLOT3D_APPEND_CMAP(Plasma, false);
    IMPLOT3D_APPEND_CMAP(Hot, false);
    IMPLOT3D_APPEND_CMAP(Cool, false);
    IMPLOT3D_APPEND_CMAP(Pink, false);
    IMPLOT3D_APPEND_CMAP(Jet, false);
    IMPLOT3D_APPEND_CMAP(Twilight, false);
    IMPLOT3D_APPEND_CMAP(RdBu, false);
    IMPLOT3D_APPEND_CMAP(BrBG, false);
    IMPLOT3D_APPEND_CMAP(PiYG, false);
    IMPLOT3D_APPEND_CMAP(Spectral, false);
    IMPLOT3D_APPEND_CMAP(Greys, false);
}

void ResetContext(ImPlot3DContext* ctx) {
    ctx->Plots.Clear();
    ctx->CurrentPlot = nullptr;
    ctx->CurrentItems = nullptr;
    ctx->CurrentItem = nullptr;
    ctx->NextItemData.Reset();
    ctx->Style = ImPlot3DStyle();
}

//-----------------------------------------------------------------------------
// [SECTION] Style Utils
//-----------------------------------------------------------------------------

bool IsColorAuto(const ImVec4& col) { return col.w == -1.0f; }

bool IsColorAuto(ImPlot3DCol idx) { return IsColorAuto(GImPlot3D->Style.Colors[idx]); }

ImVec4 GetAutoColor(ImPlot3DCol idx) {
    switch (idx) {
        case ImPlot3DCol_Line: return IMPLOT3D_AUTO_COL;          // Plot dependent
        case ImPlot3DCol_Fill: return IMPLOT3D_AUTO_COL;          // Plot dependent
        case ImPlot3DCol_MarkerOutline: return IMPLOT3D_AUTO_COL; // Plot dependent
        case ImPlot3DCol_MarkerFill: return IMPLOT3D_AUTO_COL;    // Plot dependent
        case ImPlot3DCol_TitleText: return ImGui::GetStyleColorVec4(ImGuiCol_Text);
        case ImPlot3DCol_InlayText: return ImGui::GetStyleColorVec4(ImGuiCol_Text);
        case ImPlot3DCol_FrameBg: return ImGui::GetStyleColorVec4(ImGuiCol_FrameBg);
        case ImPlot3DCol_PlotBg: return ImGui::GetStyleColorVec4(ImGuiCol_WindowBg);
        case ImPlot3DCol_PlotBorder: return ImGui::GetStyleColorVec4(ImGuiCol_Border);
        case ImPlot3DCol_LegendBg: return ImGui::GetStyleColorVec4(ImGuiCol_PopupBg);
        case ImPlot3DCol_LegendBorder: return ImGui::GetStyleColorVec4(ImGuiCol_Border);
        case ImPlot3DCol_LegendText: return ImGui::GetStyleColorVec4(ImGuiCol_Text);
        case ImPlot3DCol_AxisText: return ImGui::GetStyleColorVec4(ImGuiCol_Text);
        case ImPlot3DCol_AxisGrid: return ImGui::GetStyleColorVec4(ImGuiCol_Text) * ImVec4(1, 1, 1, 0.25f);
        case ImPlot3DCol_AxisTick: return GetStyleColorVec4(ImPlot3DCol_AxisGrid);
        default: return IMPLOT3D_AUTO_COL;
    }
}

const char* GetStyleColorName(ImPlot3DCol idx) {
    static const char* color_names[ImPlot3DCol_COUNT] = {
        "Line",       "Fill",     "MarkerOutline", "MarkerFill", "TitleText", "InlayText", "FrameBg",  "PlotBg",
        "PlotBorder", "LegendBg", "LegendBorder",  "LegendText", "AxisText",  "AxisGrid",  "AxisTick",
    };
    return color_names[idx];
}

const ImPlot3DNextItemData& GetItemData() { return GImPlot3D->NextItemData; }

} // namespace ImPlot3D

//-----------------------------------------------------------------------------
// [SECTION] ImPlot3DPoint
//-----------------------------------------------------------------------------

ImPlot3DPoint ImPlot3DPoint::operator*(float rhs) const { return ImPlot3DPoint(x * rhs, y * rhs, z * rhs); }
ImPlot3DPoint ImPlot3DPoint::operator/(float rhs) const { return ImPlot3DPoint(x / rhs, y / rhs, z / rhs); }
ImPlot3DPoint ImPlot3DPoint::operator+(const ImPlot3DPoint& rhs) const { return ImPlot3DPoint(x + rhs.x, y + rhs.y, z + rhs.z); }
ImPlot3DPoint ImPlot3DPoint::operator-(const ImPlot3DPoint& rhs) const { return ImPlot3DPoint(x - rhs.x, y - rhs.y, z - rhs.z); }
ImPlot3DPoint ImPlot3DPoint::operator*(const ImPlot3DPoint& rhs) const { return ImPlot3DPoint(x * rhs.x, y * rhs.y, z * rhs.z); }
ImPlot3DPoint ImPlot3DPoint::operator/(const ImPlot3DPoint& rhs) const { return ImPlot3DPoint(x / rhs.x, y / rhs.y, z / rhs.z); }
ImPlot3DPoint ImPlot3DPoint::operator-() const { return ImPlot3DPoint(-x, -y, -z); }

ImPlot3DPoint& ImPlot3DPoint::operator*=(float rhs) {
    x *= rhs;
    y *= rhs;
    z *= rhs;
    return *this;
}
ImPlot3DPoint& ImPlot3DPoint::operator/=(float rhs) {
    x /= rhs;
    y /= rhs;
    z /= rhs;
    return *this;
}
ImPlot3DPoint& ImPlot3DPoint::operator+=(const ImPlot3DPoint& rhs) {
    x += rhs.x;
    y += rhs.y;
    z += rhs.z;
    return *this;
}
ImPlot3DPoint& ImPlot3DPoint::operator-=(const ImPlot3DPoint& rhs) {
    x -= rhs.x;
    y -= rhs.y;
    z -= rhs.z;
    return *this;
}
ImPlot3DPoint& ImPlot3DPoint::operator*=(const ImPlot3DPoint& rhs) {
    x *= rhs.x;
    y *= rhs.y;
    z *= rhs.z;
    return *this;
}
ImPlot3DPoint& ImPlot3DPoint::operator/=(const ImPlot3DPoint& rhs) {
    x /= rhs.x;
    y /= rhs.y;
    z /= rhs.z;
    return *this;
}

bool ImPlot3DPoint::operator==(const ImPlot3DPoint& rhs) const { return x == rhs.x && y == rhs.y && z == rhs.z; }
bool ImPlot3DPoint::operator!=(const ImPlot3DPoint& rhs) const { return !(*this == rhs); }

float ImPlot3DPoint::Dot(const ImPlot3DPoint& rhs) const { return x * rhs.x + y * rhs.y + z * rhs.z; }

ImPlot3DPoint ImPlot3DPoint::Cross(const ImPlot3DPoint& rhs) const {
    return ImPlot3DPoint(y * rhs.z - z * rhs.y, z * rhs.x - x * rhs.z, x * rhs.y - y * rhs.x);
}

float ImPlot3DPoint::Length() const { return ImSqrt(x * x + y * y + z * z); }

float ImPlot3DPoint::LengthSquared() const { return x * x + y * y + z * z; }

void ImPlot3DPoint::Normalize() {
    float l = Length();
    x /= l;
    y /= l;
    z /= l;
}

ImPlot3DPoint ImPlot3DPoint::Normalized() const {
    float l = Length();
    return ImPlot3DPoint(x / l, y / l, z / l);
}

ImPlot3DPoint operator*(float lhs, const ImPlot3DPoint& rhs) { return ImPlot3DPoint(lhs * rhs.x, lhs * rhs.y, lhs * rhs.z); }

bool ImPlot3DPoint::IsNaN() const { return ImPlot3D::ImNan(x) || ImPlot3D::ImNan(y) || ImPlot3D::ImNan(z); }

//-----------------------------------------------------------------------------
// [SECTION] ImPlot3DBox
//-----------------------------------------------------------------------------

void ImPlot3DBox::Expand(const ImPlot3DPoint& point) {
    Min.x = ImMin(Min.x, point.x);
    Min.y = ImMin(Min.y, point.y);
    Min.z = ImMin(Min.z, point.z);
    Max.x = ImMax(Max.x, point.x);
    Max.y = ImMax(Max.y, point.y);
    Max.z = ImMax(Max.z, point.z);
}

bool ImPlot3DBox::Contains(const ImPlot3DPoint& point) const {
    return (point.x >= Min.x && point.x <= Max.x) && (point.y >= Min.y && point.y <= Max.y) && (point.z >= Min.z && point.z <= Max.z);
}

bool ImPlot3DBox::ClipLineSegment(const ImPlot3DPoint& p0, const ImPlot3DPoint& p1, ImPlot3DPoint& p0_clipped, ImPlot3DPoint& p1_clipped) const {
    // Check if the line segment is completely inside the box
    if (Contains(p0) && Contains(p1)) {
        p0_clipped = p0;
        p1_clipped = p1;
        return true;
    }

    // Perform Liang-Barsky 3D clipping
    double t0 = 0.0;
    double t1 = 1.0;
    ImPlot3DPoint d = p1 - p0;

    // Define the clipping boundaries
    const double xmin = Min.x, xmax = Max.x;
    const double ymin = Min.y, ymax = Max.y;
    const double zmin = Min.z, zmax = Max.z;

    // Lambda function to update t0 and t1
    auto update = [&](double p, double q) -> bool {
        if (p == 0.0) {
            if (q < 0.0)
                return false; // Line is parallel and outside the boundary
            else
                return true; // Line is parallel and inside or coincident with boundary
        }
        double r = q / p;
        if (p < 0.0) {
            if (r > t1)
                return false; // Line is outside
            if (r > t0)
                t0 = r; // Move up t0
        } else {
            if (r < t0)
                return false; // Line is outside
            if (r < t1)
                t1 = r; // Move down t1
        }
        return true;
    };

    // Clip against each boundary
    if (!update(-d.x, p0.x - xmin))
        return false; // Left
    if (!update(d.x, xmax - p0.x))
        return false; // Right
    if (!update(-d.y, p0.y - ymin))
        return false; // Bottom
    if (!update(d.y, ymax - p0.y))
        return false; // Top
    if (!update(-d.z, p0.z - zmin))
        return false; // Near
    if (!update(d.z, zmax - p0.z))
        return false; // Far

    // Compute clipped points
    p0_clipped = p0 + d * (float)t0;
    p1_clipped = p0 + d * (float)t1;

    return true;
}

//-----------------------------------------------------------------------------
// [SECTION] ImPlot3DRange
//-----------------------------------------------------------------------------

void ImPlot3DRange::Expand(float value) {
    Min = ImMin(Min, value);
    Max = ImMax(Max, value);
}

bool ImPlot3DRange::Contains(float value) const { return value >= Min && value <= Max; }

//-----------------------------------------------------------------------------
// [SECTION] ImPlot3DQuat
//-----------------------------------------------------------------------------

ImPlot3DQuat::ImPlot3DQuat(float _angle, const ImPlot3DPoint& _axis) {
    float half_angle = _angle * 0.5f;
    float s = ImSin(half_angle);
    x = s * _axis.x;
    y = s * _axis.y;
    z = s * _axis.z;
    w = ImCos(half_angle);
}

ImPlot3DQuat ImPlot3DQuat::FromTwoVectors(const ImPlot3DPoint& v0, const ImPlot3DPoint& v1) {
    ImPlot3DQuat q;

    // Compute the dot product and lengths of the vectors
    float dot = v0.Dot(v1);
    float length_v0 = v0.Length();
    float length_v1 = v1.Length();

    // Normalize the dot product
    float normalized_dot = dot / (length_v0 * length_v1);

    // Handle edge cases: if vectors are very close or identical
    const float epsilon = 1e-6f;
    if (ImFabs(normalized_dot - 1.0f) < epsilon) {
        // v0 and v1 are nearly identical; return an identity quaternion
        q.x = 0.0f;
        q.y = 0.0f;
        q.z = 0.0f;
        q.w = 1.0f;
        return q;
    }

    // Handle edge case: if vectors are opposite
    if (ImFabs(normalized_dot + 1.0f) < epsilon) {
        // v0 and v1 are opposite; choose an arbitrary orthogonal axis
        ImPlot3DPoint arbitrary_axis = ImFabs(v0.x) > ImFabs(v0.z) ? ImPlot3DPoint(-v0.y, v0.x, 0.0f) : ImPlot3DPoint(0.0f, -v0.z, v0.y);
        arbitrary_axis.Normalize();
        q.x = arbitrary_axis.x;
        q.y = arbitrary_axis.y;
        q.z = arbitrary_axis.z;
        q.w = 0.0f;
        return q;
    }

    // General case
    ImPlot3DPoint axis = v0.Cross(v1);
    axis.Normalize();
    float angle = ImAcos(normalized_dot);
    float half_angle = angle * 0.5f;
    float s = ImSin(half_angle);
    q.x = s * axis.x;
    q.y = s * axis.y;
    q.z = s * axis.z;
    q.w = ImCos(half_angle);

    return q;
}

ImPlot3DQuat ImPlot3DQuat::FromElAz(float elevation, float azimuth) {
    // Create quaternions for azimuth and elevation
    ImPlot3DQuat azimuth_quat(azimuth, ImPlot3DPoint(0.0f, 0.0f, 1.0f));     // Rotate around Z-axis
    ImPlot3DQuat elevation_quat(elevation, ImPlot3DPoint(1.0f, 0.0f, 0.0f)); // Rotate around X-axis
    ImPlot3DQuat zero_quat(-IM_PI / 2, ImPlot3DPoint(1.0f, 0.0f, 0.0f));     // Rotate to zero azimuth/elevation orientation

    // Combine rotations
    return elevation_quat * zero_quat * azimuth_quat;
}

float ImPlot3DQuat::Length() const { return ImSqrt(x * x + y * y + z * z + w * w); }

ImPlot3DQuat ImPlot3DQuat::Normalized() const {
    float l = Length();
    return ImPlot3DQuat(x / l, y / l, z / l, w / l);
}

ImPlot3DQuat ImPlot3DQuat::Conjugate() const { return ImPlot3DQuat(-x, -y, -z, w); }

ImPlot3DQuat ImPlot3DQuat::Inverse() const {
    float l_squared = x * x + y * y + z * z + w * w;
    return ImPlot3DQuat(-x / l_squared, -y / l_squared, -z / l_squared, w / l_squared);
}

ImPlot3DQuat ImPlot3DQuat::operator*(const ImPlot3DQuat& rhs) const {
    return ImPlot3DQuat(w * rhs.x + x * rhs.w + y * rhs.z - z * rhs.y, w * rhs.y - x * rhs.z + y * rhs.w + z * rhs.x,
                        w * rhs.z + x * rhs.y - y * rhs.x + z * rhs.w, w * rhs.w - x * rhs.x - y * rhs.y - z * rhs.z);
}

ImPlot3DQuat& ImPlot3DQuat::Normalize() {
    float l = Length();
    x /= l;
    y /= l;
    z /= l;
    w /= l;
    return *this;
}

ImPlot3DPoint ImPlot3DQuat::operator*(const ImPlot3DPoint& point) const {
    // Extract vector part of the quaternion
    ImPlot3DPoint qv(x, y, z);

    // Compute the cross products needed for rotation
    ImPlot3DPoint uv = qv.Cross(point); // uv = qv x point
    ImPlot3DPoint uuv = qv.Cross(uv);   // uuv = qv x uv

    // Compute the rotated vector
    return point + (uv * w * 2.0f) + (uuv * 2.0f);
}

bool ImPlot3DQuat::operator==(const ImPlot3DQuat& rhs) const { return x == rhs.x && y == rhs.y && z == rhs.z && w == rhs.w; }

bool ImPlot3DQuat::operator!=(const ImPlot3DQuat& rhs) const { return !(*this == rhs); }

ImPlot3DQuat ImPlot3DQuat::Slerp(const ImPlot3DQuat& q1, const ImPlot3DQuat& q2, float t) {
    // Clamp t to [0, 1]
    t = ImClamp(t, 0.0f, 1.0f);

    // Compute the dot product (cosine of the angle between quaternions)
    float dot = q1.x * q2.x + q1.y * q2.y + q1.z * q2.z + q1.w * q2.w;

    // If the dot product is negative, negate one quaternion to take the shorter path
    ImPlot3DQuat q2_ = q2;
    if (dot < 0.0f) {
        q2_ = ImPlot3DQuat(-q2.x, -q2.y, -q2.z, -q2.w);
        dot = -dot;
    }

    // If the quaternions are very close, use linear interpolation to avoid numerical instability
    if (dot > 0.9995f) {
        return ImPlot3DQuat(q1.x + t * (q2_.x - q1.x), q1.y + t * (q2_.y - q1.y), q1.z + t * (q2_.z - q1.z), q1.w + t * (q2_.w - q1.w)).Normalized();
    }

    // Compute the angle and the interpolation factors
    float theta_0 = ImAcos(dot);        // Angle between input quaternions
    float theta = theta_0 * t;          // Interpolated angle
    float sin_theta = ImSin(theta);     // Sine of interpolated angle
    float sin_theta_0 = ImSin(theta_0); // Sine of original angle

    float s1 = ImCos(theta) - dot * sin_theta / sin_theta_0;
    float s2 = sin_theta / sin_theta_0;

    // Interpolate and return the result
    return ImPlot3DQuat(s1 * q1.x + s2 * q2_.x, s1 * q1.y + s2 * q2_.y, s1 * q1.z + s2 * q2_.z, s1 * q1.w + s2 * q2_.w);
}

float ImPlot3DQuat::Dot(const ImPlot3DQuat& rhs) const { return x * rhs.x + y * rhs.y + z * rhs.z + w * rhs.w; }

//-----------------------------------------------------------------------------
// [SECTION] ImDrawList3D
//-----------------------------------------------------------------------------

void ImDrawList3D::PrimReserve(int idx_count, int vtx_count) {
    IM_ASSERT_PARANOID(idx_count >= 0 && vtx_count >= 0 && idx_count % 3 == 0);

    int vtx_buffer_old_size = VtxBuffer.Size;
    VtxBuffer.resize(vtx_buffer_old_size + vtx_count);
    _VtxWritePtr = VtxBuffer.Data + vtx_buffer_old_size;

    int idx_buffer_old_size = IdxBuffer.Size;
    IdxBuffer.resize(idx_buffer_old_size + idx_count);
    _IdxWritePtr = IdxBuffer.Data + idx_buffer_old_size;

    int z_buffer_old_size = ZBuffer.Size;
    ZBuffer.resize(z_buffer_old_size + idx_count / 3);
    _ZWritePtr = ZBuffer.Data + z_buffer_old_size;
}

void ImDrawList3D::PrimUnreserve(int idx_count, int vtx_count) {
    IM_ASSERT_PARANOID(idx_count >= 0 && vtx_count >= 0 && idx_count % 3 == 0);

    VtxBuffer.shrink(VtxBuffer.Size - vtx_count);
    IdxBuffer.shrink(IdxBuffer.Size - idx_count);
    ZBuffer.shrink(ZBuffer.Size - idx_count / 3);
}

void ImDrawList3D::SetTexture(ImTextureRef tex_ref) {
    if (_TextureBuffer.empty()) {
        // First texture assignment
        _TextureBuffer.push_back({tex_ref, _VtxCurrentIdx});
        return;
    }

    ImTextureBufferItem& prev_item = _TextureBuffer.back();
    if (prev_item.VtxIdx == _VtxCurrentIdx) {
        // Same vertex index: update existing texture ID
        prev_item.TexRef = tex_ref;

        // If the previous texture was the same, remove current texture
        if (_TextureBuffer.Size >= 2) {
            if (_TextureBuffer[_TextureBuffer.Size - 2].TexRef == tex_ref) {
                _TextureBuffer.pop_back();
            }
        }
    } else if (prev_item.TexRef != tex_ref) {
        // New vertex index and different texture: insert new item
        _TextureBuffer.push_back({tex_ref, _VtxCurrentIdx});
    }
}

void ImDrawList3D::ResetTexture() { SetTexture(ImTextureID(0)); }

#ifdef IMGUI_HAS_TEXTURES
#define SET_TEX_REF(cmd, tex_ref) (cmd).TexRef = (tex_ref)
#define GET_TEX_REF(cmd) (cmd).TexRef
#else
#define SET_TEX_REF(cmd, tex_ref) (cmd).TextureId = (tex_ref)
#define GET_TEX_REF(cmd) (cmd).TextureId
#endif

void ImDrawList3D::SortedMoveToImGuiDrawList() {
    ImDrawList& draw_list = *ImGui::GetWindowDrawList();

    const int tri_count = ZBuffer.Size;
    if (tri_count == 0) {
        // No triangles, just reset buffers and return
        ResetBuffers();
        return;
    }

    // Build an array of (z, tri_idx)
    struct TriRef {
        float z;
        int tri_idx;
    };
    TriRef* tris = (TriRef*)IM_ALLOC(sizeof(TriRef) * tri_count);
    for (int i = 0; i < tri_count; i++) {
        tris[i].z = ZBuffer[i];
        tris[i].tri_idx = i;
    }

    // Sort by z (distance from viewer)
    ImQsort(tris, (size_t)tri_count, sizeof(TriRef), [](const void* a, const void* b) {
        float za = ((const TriRef*)a)->z;
        float zb = ((const TriRef*)b)->z;
        return (za < zb) ? -1 : (za > zb) ? 1 : 0;
    });

    // Reserve space in the ImGui draw list
    draw_list.PrimReserve(IdxBuffer.Size, VtxBuffer.Size);

    // Copy vertices (no reordering needed)
    memcpy(draw_list._VtxWritePtr, VtxBuffer.Data, VtxBuffer.Size * sizeof(ImDrawVert));
    unsigned int idx_offset = draw_list._VtxCurrentIdx;
    draw_list._VtxWritePtr += VtxBuffer.Size;
    draw_list._VtxCurrentIdx += (unsigned int)VtxBuffer.Size;

    // Maximum index allowed to not overflow ImDrawIdx
    unsigned int max_index_allowed = MaxIdx() - idx_offset;

    // Copy indices with triangle sorting based on distance from viewer
    ImDrawIdx* idx_out_begin = draw_list._IdxWritePtr;
    ImDrawIdx* idx_out = idx_out_begin;
    ImDrawIdx* idx_in = IdxBuffer.Data;
    for (int i = 0; i < tri_count; i++) {
        int tri_i = tris[i].tri_idx;
        int base_idx = tri_i * 3;
        unsigned int i0 = (unsigned int)idx_in[base_idx + 0];
        unsigned int i1 = (unsigned int)idx_in[base_idx + 1];
        unsigned int i2 = (unsigned int)idx_in[base_idx + 2];

        // Check if after adding offset any of these indices exceed max_index_allowed
        if (i0 > max_index_allowed || i1 > max_index_allowed || i2 > max_index_allowed)
            continue;

        idx_out[0] = (ImDrawIdx)(i0 + idx_offset);
        idx_out[1] = (ImDrawIdx)(i1 + idx_offset);
        idx_out[2] = (ImDrawIdx)(i2 + idx_offset);

        idx_out += 3;
    }
    ImDrawIdx* idx_out_end = idx_out;
    draw_list._IdxWritePtr = idx_out_end;

    // If multiple textures were used (e.g. PlotImage was called), generate multiple ImDrawCmd
    if (_TextureBuffer.Size > 1) {
        ImTextureRef default_tex = GET_TEX_REF(draw_list._CmdHeader);
        ImTextureRef curr_tex = default_tex;

        // Remove elements reserved from PrimReserve
        draw_list.CmdBuffer.back().ElemCount -= IdxBuffer.Size;
        ImDrawIdx* last_cmd_buffer_idx = idx_out_begin;

        // For each triangle added to the draw_list
        for (idx_out = idx_out_begin; idx_out < idx_out_end; idx_out += 3) {
            // Get index of first vertex in the triangle
            unsigned int idx_in = (unsigned int)(*idx_out - idx_offset);

            // Get the texture for this triangle
            const ImTextureRef invalid_tex = ImTextureID(0);
            ImTextureRef tri_tex = invalid_tex;
            for (int j = 0; j < _TextureBuffer.Size; j++)
                if (idx_in >= _TextureBuffer[j].VtxIdx)
                    tri_tex = _TextureBuffer[j].TexRef;

            // If tri_tex is invalid, the default texture should be used
            if (tri_tex == invalid_tex)
                tri_tex = default_tex;

            if (tri_tex != curr_tex) {
                // Update element count of previous draw cmd
                draw_list.CmdBuffer.back().ElemCount += (unsigned int)(idx_out - last_cmd_buffer_idx);
                last_cmd_buffer_idx = idx_out;

                // Set custom texture
                curr_tex = tri_tex;
                SET_TEX_REF(draw_list._CmdHeader, curr_tex);

                // Add new draw cmd for the new texture
                ImDrawCmd draw_cmd;
                draw_cmd.ClipRect = draw_list._CmdHeader.ClipRect;
                SET_TEX_REF(draw_cmd, GET_TEX_REF(draw_list._CmdHeader));
                draw_cmd.VtxOffset = draw_list._CmdHeader.VtxOffset;
                draw_cmd.IdxOffset = (unsigned int)(idx_out - draw_list.IdxBuffer.Data);
                draw_list.CmdBuffer.push_back(draw_cmd);
            }
        }
        // Flush last elements to cmd buffer
        draw_list.CmdBuffer.back().ElemCount += (unsigned int)(&draw_list.IdxBuffer.back() - last_cmd_buffer_idx + 1);

        // Check if the last texture was not the default texture
        if (curr_tex != default_tex) {
            // Restore default texture
            SET_TEX_REF(draw_list._CmdHeader, default_tex);

            // Flush last draw cmd with custom texture
            draw_list.AddDrawCmd();
        }
    }

    // Reset buffers since we've moved them
    ResetBuffers();

    IM_FREE(tris);
}

//-----------------------------------------------------------------------------
// [SECTION] ImPlot3DAxis
//-----------------------------------------------------------------------------

bool ImPlot3DAxis::HasLabel() const { return !Label.empty() && !ImPlot3D::ImHasFlag(Flags, ImPlot3DAxisFlags_NoLabel); }
bool ImPlot3DAxis::HasGridLines() const { return !ImPlot3D::ImHasFlag(Flags, ImPlot3DAxisFlags_NoGridLines); }
bool ImPlot3DAxis::HasTickLabels() const { return !ImPlot3D::ImHasFlag(Flags, ImPlot3DAxisFlags_NoTickLabels); }
bool ImPlot3DAxis::HasTickMarks() const { return !ImPlot3D::ImHasFlag(Flags, ImPlot3DAxisFlags_NoTickMarks); }
bool ImPlot3DAxis::IsAutoFitting() const { return ImPlot3D::ImHasFlag(Flags, ImPlot3DAxisFlags_AutoFit); }

void ImPlot3DAxis::ExtendFit(float value) {
    FitExtents.Min = ImMin(FitExtents.Min, value);
    FitExtents.Max = ImMax(FitExtents.Max, value);
}

void ImPlot3DAxis::ApplyFit() {
    if (!IsLockedMin() && !ImPlot3D::ImNanOrInf(FitExtents.Min))
        Range.Min = FitExtents.Min;
    if (!IsLockedMax() && !ImPlot3D::ImNanOrInf(FitExtents.Max))
        Range.Max = FitExtents.Max;
    if (ImPlot3D::ImAlmostEqual(Range.Min, Range.Max)) {
        Range.Max += 0.5;
        Range.Min -= 0.5;
    }
    Constrain();
    FitExtents.Min = HUGE_VAL;
    FitExtents.Max = -HUGE_VAL;
}

//-----------------------------------------------------------------------------
// [SECTION] ImPlot3DPlot
//-----------------------------------------------------------------------------

void ImPlot3DPlot::ExtendFit(const ImPlot3DPoint& point) {
    for (int i = 0; i < 3; i++) {
        if (!ImPlot3D::ImNanOrInf(point[i]) && Axes[i].FitThisFrame)
            Axes[i].ExtendFit(point[i]);
    }
}

ImPlot3DPoint ImPlot3DPlot::RangeMin() const { return ImPlot3DPoint(Axes[0].Range.Min, Axes[1].Range.Min, Axes[2].Range.Min); }

ImPlot3DPoint ImPlot3DPlot::RangeMax() const { return ImPlot3DPoint(Axes[0].Range.Max, Axes[1].Range.Max, Axes[2].Range.Max); }

ImPlot3DPoint ImPlot3DPlot::RangeCenter() const {
    return ImPlot3DPoint((Axes[0].Range.Min + Axes[0].Range.Max) * 0.5f, (Axes[1].Range.Min + Axes[1].Range.Max) * 0.5f,
                         (Axes[2].Range.Min + Axes[2].Range.Max) * 0.5f);
}

void ImPlot3DPlot::SetRange(const ImPlot3DPoint& min, const ImPlot3DPoint& max) {
    Axes[0].SetRange(min.x, max.x);
    Axes[1].SetRange(min.y, max.y);
    Axes[2].SetRange(min.z, max.z);
}

float ImPlot3DPlot::GetBoxZoom() const { return ImMin(PlotRect.GetWidth(), PlotRect.GetHeight()) / 1.8f; }

//-----------------------------------------------------------------------------
// [SECTION] ImPlot3DStyle
//-----------------------------------------------------------------------------

ImPlot3DStyle::ImPlot3DStyle() {
    // Item style
    LineWeight = 1.0f;
    Marker = ImPlot3DMarker_None;
    MarkerSize = 4.0f;
    MarkerWeight = 1.0f;
    FillAlpha = 1.0f;
    // Plot style
    PlotDefaultSize = ImVec2(400, 400);
    PlotMinSize = ImVec2(200, 200);
    PlotPadding = ImVec2(10, 10);
    LabelPadding = ImVec2(5, 5);
    // Legend style
    LegendPadding = ImVec2(10, 10);
    LegendInnerPadding = ImVec2(5, 5);
    LegendSpacing = ImVec2(5, 0);
    // Colors
    ImPlot3D::StyleColorsAuto(this);
    Colormap = ImPlot3DColormap_Deep;
};

//-----------------------------------------------------------------------------
// [SECTION] Metrics
//-----------------------------------------------------------------------------

void ShowTicksMetrics(const ImPlot3DTicker& ticker) { ImGui::BulletText("Size: %d", ticker.TickCount()); }

void ShowAxisMetrics(const ImPlot3DAxis& axis) {
    ImGui::BulletText("Label: %s", axis.GetLabel());
    ImGui::BulletText("Flags: 0x%08X", axis.Flags);
    ImGui::BulletText("Range: [%f,%f]", axis.Range.Min, axis.Range.Max);

    ImGui::BulletText("ShowDefaultTicks: %s", axis.ShowDefaultTicks ? "true" : "false");
    ImGui::BulletText("FitThisFrame: %s", axis.FitThisFrame ? "true" : "false");
    ImGui::BulletText("FitExtents: [%f,%f]", axis.FitExtents.Min, axis.FitExtents.Min);
    ImGui::BulletText("ConstraintRange: [%f,%f]", axis.ConstraintRange.Min, axis.ConstraintRange.Min);
    ImGui::BulletText("ConstraintZoom: [%f,%f]", axis.ConstraintZoom.Min, axis.ConstraintZoom.Min);
    ImGui::BulletText("Hovered: %s", axis.Hovered ? "true" : "false");
    ImGui::BulletText("Held: %s", axis.Held ? "true" : "false");

    if (ImGui::TreeNode("Ticks")) {
        ShowTicksMetrics(axis.Ticker);
        ImGui::TreePop();
    }
}

void ImPlot3D::ShowMetricsWindow(bool* p_popen) {
    static bool show_frame_rects = false;
    static bool show_canvas_rects = false;
    static bool show_plot_rects = false;
    static bool show_plot_box = false;
    static bool show_axis_line = false;
    static bool show_axis_corner_indexes = false;
    static bool show_axis_face_indexes = false;
    static bool show_axis_edge_indexes = false;
    static bool show_legend_rects = false;

    ImDrawList& fg = *ImGui::GetForegroundDrawList();

    ImPlot3DContext& gp = *GImPlot3D;
    ImGuiIO& io = ImGui::GetIO();
    ImGui::Begin("Metrics (ImPlot3D)", p_popen);
    ImGui::Text("ImPlot3D " IMPLOT3D_VERSION);
    ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
    ImGui::Text("Mouse Position: [%.0f,%.0f]", io.MousePos.x, io.MousePos.y);
    ImGui::Separator();
    if (ImGui::TreeNode("Tools")) {
        if (ImGui::Button("Bust Plot Cache"))
            BustPlotCache();
        ImGui::SameLine();
        if (ImGui::Button("Bust Item Cache"))
            BustItemCache();
        ImGui::Checkbox("Show Frame Rects", &show_frame_rects);
        ImGui::Checkbox("Show Canvas Rects", &show_canvas_rects);
        ImGui::Checkbox("Show Plot Rects", &show_plot_rects);
        ImGui::Checkbox("Show Plot Box", &show_plot_box);
        ImGui::Checkbox("Show Axis Line", &show_axis_line);
        ImGui::Checkbox("Show Axis Corner Indexes", &show_axis_corner_indexes);
        ImGui::Checkbox("Show Axis Face Indexes", &show_axis_face_indexes);
        ImGui::Checkbox("Show Axis Edge Indexes", &show_axis_edge_indexes);
        ImGui::Checkbox("Show Legend Rects", &show_legend_rects);
        ImGui::TreePop();
    }
    const int n_plots = gp.Plots.GetBufSize();
    bool active_faces[3];
    ImVec2 corners_pix[8];
    ImPlot3DPoint corners[8];
    int plane_2d;
    int axis_corners[3][2];
    char buff[16];
    // Enum used to indicate how a certain type will be displayed
    enum class DisplayedType {
        Hidden,   // Hidden type that is not displayed. Used when the object should not be displayed. Will be used when:
                  // 1. 2D plot and this type should not be used,
                  // 2. The flag show_plot_box is disabled
        Active,   // Part of the faces of the box that are active
        NotActive // Part of the faces of the box that are not active when show_plot_box is active
    };

    // Render rectangles
    for (int p = 0; p < n_plots; ++p) {
        ImPlot3DPlot& plot = *gp.Plots.GetByIndex(p);
        if (show_frame_rects)
            fg.AddRect(plot.FrameRect.Min, plot.FrameRect.Max, IM_COL32(255, 0, 255, 255));
        if (show_canvas_rects)
            fg.AddRect(plot.CanvasRect.Min, plot.CanvasRect.Max, IM_COL32(0, 255, 255, 255));
        if (show_plot_rects)
            fg.AddRect(plot.PlotRect.Min, plot.PlotRect.Max, IM_COL32(255, 255, 0, 255));
        if (show_plot_box || show_axis_line || show_axis_corner_indexes || show_axis_face_indexes || show_axis_edge_indexes)
            GetAxesParameters(plot, active_faces, corners_pix, corners, plane_2d, axis_corners);
        if (show_plot_box || show_axis_edge_indexes) {
            // Determine which display type each of the edges are
            DisplayedType edge_types[12];
            for (int e = 0; e < 12; e++) {
                if (plane_2d == -1 && show_plot_box)
                    edge_types[e] = DisplayedType::NotActive;
                else
                    edge_types[e] = DisplayedType::Hidden;
            }
            for (int a = 0; a < 3; a++) {
                int face_idx = a + 3 * active_faces[a];
                for (size_t i = 0; (plane_2d == -1 || a == plane_2d) && i < 4; i++)
                    edge_types[face_edges[face_idx][i]] = DisplayedType::Active;
            }

            // Go through each edge and use the display type
            for (int e = 0; e < 12; e++) {
                int idx0 = edges[e][0];
                int idx1 = edges[e][1];

                if (show_plot_box) {
                    // Draw different lines depending on the type that the renderer thinks it is
                    if (edge_types[e] == DisplayedType::Active)
                        fg.AddLine(corners_pix[idx0], corners_pix[idx1], IM_COL32(255, 200, 0, 255));
                    else if (edge_types[e] == DisplayedType::NotActive)
                        fg.AddLine(corners_pix[idx0], corners_pix[idx1], IM_COL32(100, 255, 0, 125));
                    // If the render edge type is Hidden then there this edge does not get rendered
                }
                if (show_axis_edge_indexes) {
                    if (edge_types[e] == DisplayedType::Active || edge_types[e] == DisplayedType::NotActive) {
                        // Display the text in the middle between the points and at the same angle between the two points
                        ImVec2 edge_middle = (corners_pix[idx0] + corners_pix[idx1]) / 2;
                        ImVec2 point_difference = (corners_pix[idx1] - corners_pix[idx0]);
                        float angle = ImAtan2(-point_difference.y, point_difference.x);
                        // Adjust angle to keep labels upright
                        if (angle > IM_PI * 0.5f)
                            angle -= IM_PI;
                        if (angle < -IM_PI * 0.5f)
                            angle += IM_PI;

                        ImFormatString(buff, IM_ARRAYSIZE(buff), "E%d", e);
                        AddTextRotated(&fg, edge_middle, angle, IM_COL32(200, 0, 0, 200), buff);
                    }
                }
            }
        }
        if (show_axis_line) {
            // For each axis plot a line between the axis corners
            for (int a = 0; a < 3; a++) {
                if (plane_2d != -1 && a == plane_2d)
                    continue;
                int idx0 = axis_corners[a][0];
                int idx1 = axis_corners[a][1];

                // Draw axis line
                ImVec2 axis_start_pix = corners_pix[idx0];
                ImVec2 axis_end_pix = corners_pix[idx1];
                fg.AddLine(axis_start_pix, axis_end_pix, IM_COL32(0, 255, 255, 255));
            }
        }
        if (show_axis_corner_indexes) {
            // Determine which display type each of the corners are
            DisplayedType corner_types[8];
            for (int c = 0; c < 8; c++) {
                if (plane_2d == -1 && show_plot_box)
                    corner_types[c] = DisplayedType::NotActive;
                else
                    corner_types[c] = DisplayedType::Hidden;
            }
            for (int a = 0; a < 3; a++) {
                int face_idx = a + 3 * active_faces[a];
                for (size_t i = 0; (plane_2d == -1 || a == plane_2d) && i < 4; i++)
                    corner_types[faces[face_idx][i]] = DisplayedType::Active;
            }
            // Go through each corner and use the display type
            for (int c = 0; c < 8; c++) {
                if (corner_types[c] == DisplayedType::Active || corner_types[c] == DisplayedType::NotActive) {
                    ImVec2 corner = corners_pix[c];
                    ImFormatString(buff, IM_ARRAYSIZE(buff), "C%d", c);
                    AddTextRotated(&fg, corner, 0, IM_COL32(0, 200, 0, 200), buff);
                }
            }
        }
        if (show_axis_face_indexes) {
            // Go through each axis and then face to display the face index
            for (int a = 0; a < 3; a++) {
                for (int f = 0; f < 2; f++) {
                    if ((plane_2d == -1 || a == plane_2d) && (f == int(active_faces[a]) || show_plot_box)) {
                        const int face_idx = a + 3 * f;
                        const int* face_indexes = faces[face_idx];
                        // Display the text in the middle of the square by determining the point between two of opposite corner pixels
                        ImVec2 middle_of_face_sqaure = (corners_pix[face_indexes[0]] + corners_pix[face_indexes[2]]) / 2;
                        ImFormatString(buff, IM_ARRAYSIZE(buff), "F%d", face_idx);
                        AddTextRotated(&fg, middle_of_face_sqaure, 0, IM_COL32(0, 200, 200, 200), buff);
                    }
                }
            }
        }
        if (show_legend_rects && plot.Items.GetLegendCount() > 0)
            fg.AddRect(plot.Items.Legend.Rect.Min, plot.Items.Legend.Rect.Max, IM_COL32(255, 192, 0, 255));
    }
    if (ImGui::TreeNode("Plots", "Plots (%d)", n_plots)) {
        for (int p = 0; p < n_plots; ++p) {
            ImPlot3DPlot& plot = *gp.Plots.GetByIndex(p);
            ImGui::PushID(p);
            if (ImGui::TreeNode("Plot", "Plot [0x%08X]", plot.ID)) {
                int n_items = plot.Items.GetItemCount();
                if (ImGui::TreeNode("Items", "Items (%d)", n_items)) {
                    for (int i = 0; i < n_items; ++i) {
                        ImPlot3DItem* item = plot.Items.GetItemByIndex(i);
                        ImGui::PushID(i);
                        if (ImGui::TreeNode("Item", "Item [0x%08X]", item->ID)) {
                            ImGui::Bullet();
                            ImGui::Checkbox("Show", &item->Show);
                            ImGui::Bullet();
                            ImVec4 temp = ImGui::ColorConvertU32ToFloat4(item->Color);
                            if (ImGui::ColorEdit4("Color", &temp.x, ImGuiColorEditFlags_NoInputs))
                                item->Color = ImGui::ColorConvertFloat4ToU32(temp);

                            ImGui::BulletText("NameOffset: %d", item->NameOffset);
                            ImGui::BulletText("Name: %s", item->NameOffset != -1 ? plot.Items.Legend.Labels.Buf.Data + item->NameOffset : "N/A");
                            ImGui::BulletText("Hovered: %s", item->LegendHovered ? "true" : "false");
                            ImGui::TreePop();
                        }
                        ImGui::PopID();
                    }
                    ImGui::TreePop();
                }

                if (ImGui::TreeNode("Axes")) {
                    // Get the axes parameters of this plot and use it to plot
                    GetAxesParameters(plot, active_faces, corners_pix, corners, plane_2d, axis_corners);

                    for (int a = 0; a < 3; ++a) {
                        if (plane_2d != -1 && plane_2d == a)
                            continue;
                        if (ImGui::TreeNode(axis_labels[a])) {
                            ShowAxisMetrics(plot.Axes[a]);
                            ImGui::TreePop();
                        }
                    }

                    if (plane_2d != -1)
                        ImGui::BulletText("Plane2D: %d %s", plane_2d, plane_labels[plane_2d]);
                    else {
                        int active_corner_index = Active3DFacesToAxisLookupIndex(active_faces);
                        ImGui::BulletText("3D Active Faces: [%s,%s,%s]=%d", active_faces[0] ? "X-max" : "X-min", active_faces[1] ? "Y-max" : "Y-min",
                                          active_faces[2] ? "Z-max" : "Z-min", active_corner_index);
                        ImGui::BulletText("3D Active Corner Lookup: [[%d,%d],[%d, %d],[%d, %d]]", axis_corners_lookup_3d[active_corner_index][0][0],
                                          axis_corners_lookup_3d[active_corner_index][0][1], axis_corners_lookup_3d[active_corner_index][1][0],
                                          axis_corners_lookup_3d[active_corner_index][1][1], axis_corners_lookup_3d[active_corner_index][2][0],
                                          axis_corners_lookup_3d[active_corner_index][2][1]);
                        ImGui::BulletText("3D Active Edges Lookup: [%d,%d,%d]", axis_edges_lookup_3d[active_corner_index][0],
                                          axis_edges_lookup_3d[active_corner_index][1], axis_edges_lookup_3d[active_corner_index][2]);
                    }

                    for (int a = 0; a < 3; a++)
                        ImGui::BulletText("%s Label Corners: [%d,%d]", axis_labels[a], axis_corners[a][0], axis_corners[a][1]);
                    for (int c = 0; c < 8; c++)
                        ImGui::BulletText("Corner %d: [%.2f,%.2f,%.2f] [%.2f,%.2f]", c, corners[c].x, corners[c].y, corners[c].z, corners_pix[c].x,
                                          corners_pix[c].y);

                    ImGui::TreePop();
                }

                ImGui::BulletText("Title: %s", plot.HasTitle() ? plot.GetTitle() : "none");
                ImGui::BulletText("Flags: 0x%08X", plot.Flags);
                ImGui::BulletText("Initialized: %s", plot.Initialized ? "true" : "false");
                ImGui::BulletText("Hovered: %s", plot.Hovered ? "true" : "false");
                ImGui::BulletText("Held: %s", plot.Held ? "true" : "false");
                ImGui::BulletText("HeldEdgeIdx: %d", plot.HeldEdgeIdx);
                ImGui::BulletText("HeldPlaneIdx: %d", plot.HeldPlaneIdx);
                ImGui::BulletText("LegendLocation: %s", GetShortLegendLocationName(plot.Items.Legend.Location));
                ImGui::BulletText("LegendHovered: %s", plot.Items.Legend.Hovered ? "true" : "false");
                ImGui::BulletText("LegendHeld: %s", plot.Items.Legend.Held ? "true" : "false");
                ImGui::BulletText("RotationCond: 0x%08X", plot.RotationCond);
                ImGui::BulletText("InitialRotation: [%.2f,%.2f,%.2f,%.2f]", plot.InitialRotation.x, plot.InitialRotation.y, plot.InitialRotation.z,
                                  plot.InitialRotation.w);
                ImGui::BulletText("Rotation: [%.2f,%.2f,%.2f,%.2f]", plot.Rotation.x, plot.Rotation.y, plot.Rotation.z, plot.Rotation.w);
                ImGui::BulletText("Animation: Time=%.4f RotationEnd=[%.2f,%.2f,%.2f,%.2f]", plot.AnimationTime, plot.RotationAnimationEnd.x,
                                  plot.RotationAnimationEnd.y, plot.Rotation.z, plot.RotationAnimationEnd.w);
                ImGui::BulletText("BoxScale: [%.2f,%.2f,%.2f]", plot.BoxScale.x, plot.BoxScale.y, plot.BoxScale.z);
                ImGui::BulletText("BoxZoom: %.2f", plot.GetBoxZoom());

                ImGui::TreePop();
            }
            ImGui::PopID();
        }
        ImGui::TreePop();
    }

    if (ImGui::TreeNode("Colormaps")) {
        ImGui::BulletText("Colormaps:  %d", gp.ColormapData.Count);
        ImGui::BulletText("Memory: %d bytes", gp.ColormapData.Tables.Size * (int)(sizeof(gp.ColormapData.Tables.Data[0])));
        if (ImGui::TreeNode("Data")) {
            for (int m = 0; m < gp.ColormapData.Count; ++m) {
                if (ImGui::TreeNode(gp.ColormapData.GetName(m))) {
                    int count = gp.ColormapData.GetKeyCount(m);
                    int size = gp.ColormapData.GetTableSize(m);
                    bool qual = gp.ColormapData.IsQual(m);
                    ImGui::BulletText("Qualitative: %s", qual ? "true" : "false");
                    ImGui::BulletText("Key Count: %d", count);
                    ImGui::BulletText("Table Size: %d", size);
                    ImGui::Indent();

                    static float t = 0.5;
                    ImVec4 samp;
                    float wid = 32 * 10 - ImGui::GetFrameHeight() - ImGui::GetStyle().ItemSpacing.x;
                    ImGui::SetNextItemWidth(wid);
                    ColormapSlider("##Sample", &t, &samp, "%.3f", m);
                    ImGui::SameLine();
                    ImGui::ColorButton("Sampler", samp);
                    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0, 0, 0, 0));
                    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
                    for (int c = 0; c < size; ++c) {
                        ImVec4 col = ImGui::ColorConvertU32ToFloat4(gp.ColormapData.GetTableColor(m, c));
                        ImGui::PushID(m * 1000 + c);
                        ImGui::ColorButton("", col, 0, ImVec2(10, 10));
                        ImGui::PopID();
                        if ((c + 1) % 32 != 0 && c != size - 1)
                            ImGui::SameLine();
                    }
                    ImGui::PopStyleVar();
                    ImGui::PopStyleColor();
                    ImGui::Unindent();
                    ImGui::TreePop();
                }
            }
            ImGui::TreePop();
        }
        ImGui::TreePop();
    }
    ImGui::End();
}

#endif // #ifndef IMGUI_DISABLE
