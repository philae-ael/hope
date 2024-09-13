
#include "core/core.h"
#include "core/debug/time.h"
#include "core/os/time.h"
#include <imgui.h>
using namespace core::literals;

struct Color {
  u8 r, g, b;
};

struct frame_datum {
  struct a {
    core::hstr8 name;
    os::time t;
    Color color;
  };
  core::storage<a> as;
  os::time sum;
};

frame_datum::a cpu_frame_data_a[150][150];
frame_datum cpu_frame_data[150];

frame_datum::a gpu_frame_data_a[150][150];
frame_datum gpu_frame_data[150];

usize offset = 0;
bool freeze  = false;
core::array color_map{
    Color{0x84, 0x5e, 0xc2}, Color{0xd6, 0x5d, 0xb1}, Color{0xff, 0x6f, 0x91},
    Color{0xff, 0x96, 0x71}, Color{0xff, 0xc7, 0x5f},
};

void render_profiling_graph(
    core::Arena& arena_,
    core::storage<frame_datum> frame_data,
    usize offset,
    f32* max
) {
  auto ar = arena_.make_temp();
  defer { ar.retire(); };
  struct {
    u32 graph_width = 200, legend_width = 100, height = 200;
    u32 frame_width              = 4;
    u32 frame_padding_horizontal = 1;
    u32 frame_padding_vertical   = 2;
    u32 frame_margin_horizontal  = 1;
    u32 frame_margin_vertical    = 1;
    u32 min_draw_height          = 0;
  } config;

  ImDrawList* drawList = ImGui::GetWindowDrawList();
  auto p               = ImGui::GetCursorScreenPos();
  const core::Vec2 graph_pos{p.x, p.y};

  drawList->AddRect(
      ImVec2(graph_pos.x, graph_pos.y),
      ImVec2(graph_pos.x + (f32)config.graph_width, graph_pos.y + (f32)config.height), 0xFFFFFFFF
  );
  f32 max_ns;
  for (auto& frame_datum : frame_data.iter()) {
    max_ns =
        MAX(max_ns,
            (f32)frame_datum.sum.ns * (f32)config.height /
                f32(config.height - (frame_datum.as.size - 1) * config.frame_padding_vertical -
                    2 * config.frame_margin_vertical));
  }
  if (*max == 0) {
    *max = max_ns;
  }
  *max = *max +
         (1.0f -
          expf(-(f32)frame_data[(frame_data.size + offset) % frame_data.size].sum.ns * 1e-9f / 2.0f)
         ) * (max_ns - *max);
  max_ns = *max  = MAX(*max, max_ns);
  max_ns        *= 1.01f;

  f32 textSizeY  = ImGui::CalcTextSize("Text").y;
  f32 textHeight =
      (f32)config.height / (f32)frame_data[(frame_data.size + offset) % frame_data.size].as.size;
  ImVec2 textPos = ImVec2(
      graph_pos.x + (f32)config.graph_width + 30,
      graph_pos.y + (f32)config.height - textHeight / 2 - (f32)config.frame_padding_vertical
  );

  for (usize idx = 0; idx < frame_data.size; idx++) {
    core::Vec2 frame_left =
        graph_pos +
        core::Vec2{
            f32(config.graph_width - 1 - config.frame_margin_horizontal - config.frame_width -
                (config.frame_width + config.frame_padding_horizontal) * idx),
            f32(config.height),
        };

    bool overflow_left = frame_left.x - (f32)config.frame_margin_horizontal < graph_pos.x;
    if (overflow_left)
      break;

    auto& frame_datum = frame_data[(2 * frame_data.size - idx + offset) % frame_data.size];
    f32 cur_height    = (f32)config.frame_margin_vertical;
    for (auto& t : frame_datum.as.iter()) {
      f32 height = ((f32)t.t.ns / max_ns) * (f32)config.height;
      ImColor color(t.color.r, t.color.g, t.color.b);
      if (height > (f32)config.min_draw_height) {
        drawList->AddRectFilled(
            ImVec2(frame_left.x, frame_left.y - height - cur_height),
            ImVec2(frame_left.x + (f32)config.frame_width, frame_left.y - cur_height), color
        );

        if (idx == 0) {
          core::array points{
              ImVec2(
                  graph_pos.x + (f32)config.frame_padding_horizontal + (f32)config.graph_width,
                  graph_pos.y + (f32)config.height - cur_height - height
              ),
              ImVec2(
                  graph_pos.x + (f32)config.frame_padding_horizontal + (f32)config.graph_width,
                  graph_pos.y + (f32)config.height - cur_height
              ),
              ImVec2(textPos.x - 2, textPos.y + 0.4f * textSizeY),
              ImVec2(textPos.x - 2, textPos.y + (1.0f - 0.2f) * textSizeY),
          };
          drawList->AddConvexPolyFilled(points.data, (int)points.size(), color);
          core::string_builder sb{};
          sb.push(*ar, t.name);
          auto duration = os::duration_info::from_time(t.t);
          sb.pushf(*ar, "%3d ms %03d us %03d ns", duration.msec, duration.usec, duration.nsec);
          auto txt = sb.commit(*ar);
          drawList->AddText(
              textPos, color, (const char*)txt.data, (const char*)(txt.data + txt.len)
          );
          textPos.y -= textHeight;
        }
      }
      cur_height += height + (f32)config.frame_padding_vertical;
    }
  };

  ImGui::Dummy(ImVec2(f32(config.graph_width + config.legend_width), f32(config.height)));
}
void profiling_window() {
  auto frame_report_scope = debug::scope_start("frame report"_hs);
  defer { debug::scope_end(frame_report_scope); };

  auto scratch = core::scratch_get();
  defer { scratch.retire(); };
  auto frame_timing_infos =
      debug::get_last_frame_timing_infos(*scratch, debug::scope_category::CPU);

  if (!freeze) {
    offset = (offset + 1) % 150;
    {
      os::time sum{};
      for (auto [idx, timing_info] : core::enumerate{frame_timing_infos.timings.iter()}) {
        cpu_frame_data_a[offset][idx] = {
            .name  = timing_info->name,
            .t     = timing_info->time,
            .color = color_map[idx % color_map.size()]
        };
        sum = sum + timing_info->time;
      }
      cpu_frame_data[offset] = {
          .as  = {frame_timing_infos.timings.size(), cpu_frame_data_a[offset]},
          .sum = sum,
      };
    }

    {
      auto frame_timing_infos =
          debug::get_last_frame_timing_infos(*scratch, debug::scope_category::GPU);
      os::time sum{};
      for (auto [idx, timing_info] : core::enumerate{frame_timing_infos.timings.iter()}) {
        gpu_frame_data_a[offset][idx] = {
            .name  = timing_info->name,
            .t     = timing_info->time,
            .color = color_map[idx % color_map.size()]
        };
        sum = sum + timing_info->time;
      }
      gpu_frame_data[offset] = {
          .as  = {frame_timing_infos.timings.size(), gpu_frame_data_a[offset]},
          .sum = sum,
      };
    }
  }

  if (ImGui::Begin("profiling")) {
    auto duration = os::duration_info::from_time(frame_timing_infos.stats.mean_frame_time);
    ImGui::Text(
        "CPU: %3d ms %03d us %03d ns |  %.0f FPS", duration.msec, duration.usec, duration.nsec,
        frame_timing_infos.stats.mean_frame_time.hz()
    );
    static int frame_offset = 0;
    static f32 cpu_max      = 0;
    render_profiling_graph(
        *scratch, cpu_frame_data,
        (usize(int(ARRAY_SIZE(cpu_frame_data)) - frame_offset) + offset) %
            ARRAY_SIZE(cpu_frame_data),
        &cpu_max
    );
    ImGui::Text("GPU: ");
    static f32 gpu_max = 0;
    render_profiling_graph(
        *scratch, gpu_frame_data,
        (usize(int(ARRAY_SIZE(gpu_frame_data)) - frame_offset) + offset) %
            ARRAY_SIZE(gpu_frame_data),
        &gpu_max
    );
    ImGui::Checkbox("Freeze", &freeze);
    ImGui::SameLine();
    ImGui::DragInt("Offset", &frame_offset);
  }
  ImGui::End();
}
