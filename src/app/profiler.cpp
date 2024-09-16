
#include "core/core.h"
#include "core/debug/time.h"
#include "core/os/time.h"
#include <imgui.h>
using namespace core::literals;

struct Color {
  u8 r, g, b;
  operator ImU32() const {
    return ImColor(r, g, b);
  }
};

struct frame_datum {
  struct entry {
    core::hstr8 name;
    os::time t;
    Color color;
  };
  core::storage<entry> as;
  os::time sum;
};

struct render_profiling_config {
  f32 graph_width = 500, legend_width = 220, height = 100;
  f32 frame_width              = 4;
  f32 frame_padding_horizontal = 1, frame_padding_vertical = 2;
  f32 frame_margin_horizontal   = 0;
  f32 legend_horizontal_margin  = 2;
  f32 legend_text_margin_margin = 2;
  f32 margin_vertical           = 1;
  f32 min_draw_height           = 0;
  f32 max_decay                 = 3.f;
};

ImVec2 to_ImVec2(core::Vec2 v) {
  return ImVec2(v.x, v.y);
}

void render_profiling_graph(
    core::Arena& arena_,
    core::storage<frame_datum> frame_data,
    usize offset,
    f32& max,
    f32 dt,
    render_profiling_config& config
) {
  auto ar = arena_.make_temp();
  defer { ar.retire(); };

  ImDrawList* drawList = ImGui::GetWindowDrawList();
  auto p               = ImGui::GetCursorScreenPos();
  const core::Vec2 graph_pos{p.x, p.y};
  const core::Vec2 graph_size{config.graph_width, config.height};

  const core::Vec2 frame_width              = config.frame_width * core::Vec2::X;
  const core::Vec2 frame_padding_horizontal = config.frame_padding_horizontal * core::Vec2::X;

  const usize max_frame_drawn = u32((graph_size.x - 2 * config.frame_margin_horizontal) /
                                    (frame_width + frame_padding_horizontal).x) -
                                1;
  const core::Vec2 graph_horizontal_margins{
      ((graph_size.x - 2 * config.frame_margin_horizontal) -
       f32(max_frame_drawn) * (frame_width + frame_padding_horizontal).x) /
          2,
      0,
  };

  const core::Vec2 graph_right_for_frame = graph_pos + graph_size - graph_horizontal_margins -
                                           config.frame_margin_horizontal * core::Vec2::X;

  f32 max_s = {};
  {
    for (usize idx = 0; idx < max_frame_drawn; idx++) {
      const usize frame_idx   = (2 * frame_data.size - idx + offset) % frame_data.size;
      const auto& frame_datum = frame_data[frame_idx];

      max_s =
          MAX(max_s, (f32)frame_datum.sum.ns * 1e-9f * config.height /
                         (config.height -
                          MAX(0.f, (f32)frame_datum.as.size - 1.f) * config.frame_padding_vertical -
                          2 * config.margin_vertical));
    }

    max = dt * config.max_decay * max_s + max * (1.f - dt * config.max_decay);
    max = max_s  = MAX(max, max_s);
    max_s       *= 1.01f;
  }

  drawList->AddRect(
      to_ImVec2(graph_pos), to_ImVec2(graph_pos + graph_size), Color{0x5F, 0x5F, 0x5F}
  );

  // Draw graph
  for (usize idx = 0; idx < max_frame_drawn; idx++) {
    const usize frame_idx   = (2 * frame_data.size - idx + offset) % frame_data.size;
    const auto& frame_datum = frame_data[frame_idx];
    const core::Vec2 frame_left =
        graph_right_for_frame - f32(idx) * (frame_width + frame_padding_horizontal) - frame_width;

    f32 cur_height = config.margin_vertical;
    for (auto& t : frame_datum.as.iter()) {
      const f32 height = ((f32)t.t.ns * 1e-9f / max_s) * config.height;
      if (height > config.min_draw_height) {
        drawList->AddRectFilled(
            to_ImVec2(frame_left - (height + cur_height) * core::Vec2::Y),
            to_ImVec2(frame_left + core::Vec2{config.frame_width, -cur_height}), t.color
        );
      }
      cur_height += height + config.frame_padding_vertical;
    }
  };

  // Draw Legend
  {
    const core::Vec2 legend_pos =
        graph_pos + graph_size + config.legend_horizontal_margin * core::Vec2::X;

    const usize frame_idx   = (2 * frame_data.size + offset) % frame_data.size;
    const auto& frame_datum = frame_data[frame_idx];
    const f32 textHeight    = config.height / (f32)frame_datum.as.size;
    const core::Vec2 textSizeY{0, ImGui::CalcTextSize("Text").y};

    core::Vec2 textPos =
        legend_pos + core::Vec2{30, -textHeight / 2 - config.frame_padding_vertical};
    f32 cur_height = config.margin_vertical;

    for (auto& t : frame_datum.as.iter()) {
      const f32 height = ((f32)t.t.ns * 1e-9f / max_s) * config.height;
      if (height > config.min_draw_height) {
        // Legend
        core::array points{
            to_ImVec2(legend_pos - (height + cur_height) * core::Vec2::Y),
            to_ImVec2(legend_pos - cur_height * core::Vec2::Y),
            to_ImVec2(textPos + (1.0f - 0.2f) * textSizeY),
            to_ImVec2(textPos + 0.4f * textSizeY),
        };
        drawList->AddConvexPolyFilled(points.data, (int)points.size(), t.color);
        core::string_builder sb{};
        sb.push(*ar, t.name);
        auto duration = os::duration_info::from_time(t.t);
        sb.pushf(*ar, "%3d.%03d ms", duration.msec, duration.usec);
        auto txt = sb.commit(*ar);
        drawList->AddText(
            to_ImVec2(textPos + config.legend_text_margin_margin * core::Vec2::X), t.color,
            (const char*)txt.data, (const char*)(txt.data + txt.len)
        );
        textPos.y -= textHeight;
      }
      cur_height += height + config.frame_padding_vertical;
    }
  }

  ImGui::Dummy(ImVec2(config.graph_width + config.legend_width, config.height));
}

#define PROFILER_MAX_FRAME 150
#define PROFILER_MAX_FRAME_ENTRY 150

static frame_datum::entry cpu_frame_data_entries[PROFILER_MAX_FRAME][PROFILER_MAX_FRAME_ENTRY];
static frame_datum cpu_frame_data[PROFILER_MAX_FRAME];
static f32 cpu_max = 0;

static frame_datum::entry gpu_frame_data_entries[PROFILER_MAX_FRAME][PROFILER_MAX_FRAME_ENTRY];
static frame_datum gpu_frame_data[PROFILER_MAX_FRAME];
static f32 gpu_max         = 0;

static usize cur_frame_idx = 0;
static bool freeze         = false;
static int frame_offset    = 0;
static core::array color_map{
    Color{0x84, 0x5e, 0xc2}, Color{0xd6, 0x5d, 0xb1}, Color{0xff, 0x6f, 0x91},
    Color{0xff, 0x96, 0x71}, Color{0xff, 0xc7, 0x5f},
};

void profiling_window() {
  auto frame_report_scope = debug::scope_start("frame report"_hs);
  defer { debug::scope_end(frame_report_scope); };

  auto scratch = core::scratch_get();
  defer { scratch.retire(); };
  auto frame_timing_infos =
      debug::get_last_frame_timing_infos(*scratch, debug::scope_category::CPU);

  if (!freeze) {
    cur_frame_idx = (cur_frame_idx + 1) % PROFILER_MAX_FRAME;
    {
      os::time sum{};
      for (auto [entry_idx, timing_info] : core::enumerate{frame_timing_infos.timings.iter()}) {
        cpu_frame_data_entries[cur_frame_idx][entry_idx] = {
            .name  = timing_info->name,
            .t     = timing_info->time,
            .color = color_map[entry_idx % color_map.size()]
        };
        sum = sum + timing_info->time;
      }
      cpu_frame_data[cur_frame_idx] = {
          .as  = {frame_timing_infos.timings.size(), cpu_frame_data_entries[cur_frame_idx]},
          .sum = sum,
      };
    }

    {
      auto frame_timing_infos =
          debug::get_last_frame_timing_infos(*scratch, debug::scope_category::GPU);
      os::time sum{};
      for (auto [entry_idx, timing_info] : core::enumerate{frame_timing_infos.timings.iter()}) {
        gpu_frame_data_entries[cur_frame_idx][entry_idx] = {
            .name  = timing_info->name,
            .t     = timing_info->time,
            .color = color_map[entry_idx % color_map.size()]
        };
        sum = sum + timing_info->time;
      }
      gpu_frame_data[cur_frame_idx] = {
          .as  = {frame_timing_infos.timings.size(), gpu_frame_data_entries[cur_frame_idx]},
          .sum = sum,
      };
    }
  }

  if (ImGui::Begin("profiling")) {
    auto duration = os::duration_info::from_time(frame_timing_infos.stats.mean_frame_time);
    ImGui::Checkbox("Freeze", &freeze);
    ImGui::SameLine();
    ImGui::DragInt("Offset", &frame_offset);

    render_profiling_config config{};

    ImVec2 vMin        = ImGui::GetWindowContentRegionMin();
    ImVec2 vMax        = ImGui::GetWindowContentRegionMax();
    config.graph_width = vMax.x - vMin.x - config.legend_width;
    config.height      = (vMax.y - vMin.y) / 2 - 31;
    if (config.graph_width > 10 && config.height > 25) {
      ImGui::Text(
          "CPU: %3d ms %03d us %03d ns |  %.0f FPS", duration.msec, duration.usec, duration.nsec,
          frame_timing_infos.stats.mean_frame_time.hz()
      );
      f32 dt = (f32)frame_timing_infos.stats.mean_frame_time.ns * 1e-9f;
      render_profiling_graph(
          *scratch, cpu_frame_data,
          (usize(int(ARRAY_SIZE(cpu_frame_data)) - frame_offset) + cur_frame_idx) %
              ARRAY_SIZE(cpu_frame_data),
          cpu_max, dt, config
      );
      ImGui::Text("GPU: ");
      render_profiling_graph(
          *scratch, gpu_frame_data,
          (usize(int(ARRAY_SIZE(gpu_frame_data)) - frame_offset) + cur_frame_idx) %
              ARRAY_SIZE(gpu_frame_data),
          gpu_max, dt, config
      );
    }
  }
  ImGui::End();
}
