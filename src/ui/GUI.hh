#pragma once
#ifndef GUI_HH
#define GUI_HH

#include "data/DataAcquisition.hh"
#include "data/DataSource.hh"
#include "fonts/CascadiaCode.ttf.h"
#include "imgui_internal.h"
#include "render/DigitalCodedExposure.hh"
#include "render/RenderTarget.hh"
#include "render/Visualizer.hh"
#include "ui/Scrubber.hh"
#include "util/ErrorQueue.hh"
#include "util/pch.hh"

// Forward declarations for callbacks
inline void SDLCALL stream_file_handle_callback(void *user_data, const char *const *data_file_list, int filter_unused);

/**
 * @brief This class provides functions to draw the GUI.
 */
class GUI
{
    public:
        enum class ViewMode
        {
            SINGLE,  // View and scrub a single selected data source
            SYNCED   // View all data sources synced to a single scrubber state
        };

        enum class SyncMode
        {
            START,  // Aligns start of data sources when sync scrubbing
            END     // Aligns end of data sources when sync scrubbing
        };

    private:
        // Modules
        DataAcquisition &data_acquisition;
        Visualizer &visualizer;
        ErrorQueue &error_queue;

        // GPU
        SDL_Window *window = nullptr;
        SDL_GPUDevice *gpu_device = nullptr;
        ImDrawData *draw_data = nullptr;

        // View mode and selection
        ViewMode view_mode = ViewMode::SINGLE;
        SyncMode sync_mode = SyncMode::START;
        int selected_source_index = 0;

        // Camera control state
        bool is_mouse_dragging = false;
        bool cursor_captured = false;

        static inline const std::string time_units[] = {"(s)", "(ms)", "(us)"};

        // Timeline visual constants
        static constexpr ImU32 kTrackColor = IM_COL32(60, 60, 60, 255);
        static constexpr ImU32 kWindowHighlight = IM_COL32(80, 130, 200, 100);
        static constexpr ImU32 kPositionMarker = IM_COL32(255, 200, 50, 255);
        static constexpr ImU32 kBorderColor = IM_COL32(120, 120, 120, 255);

        // Circular fps buffer
        std::vector<float> fps_history_buf;
        size_t fps_buf_index;
        bool check_for_layout_file;
        bool show_quickstart;

        /**
         * @brief Update circular buffer of fps data.
         * @param fps calculated fps to add to circular buffer.
         */
        void update_fps_buffer(const float &fps)
        {
            fps_history_buf[fps_buf_index] = fps;
            fps_buf_index = (fps_buf_index + 1) % fps_history_buf.size();
        }

        /**
         * @brief Get average fps from circular fps buffer.
         * @return average of circular fps buffer
         */
        float get_avg_fps()
        {
            float sum = 0.0f;
            for (const float &fps : fps_history_buf)
            {
                sum += fps;
            }
            return sum / fps_history_buf.size();
        }

        /**
         * @brief Get max fps.
         * @return maximum fps from circular fps buffer.
         */
        float get_max_fps()
        {
            float max = fps_history_buf[0];
            for (const float &fps : fps_history_buf)
            {
                if (fps > max)
                {
                    max = fps;
                }
            }
            return max;
        }

        /**
         * @brief Get min fps.
         * @return minimum fps from circular buffer.
         */
        float get_min_fps()
        {
            float min = fps_history_buf[0];
            for (const float &fps : fps_history_buf)
            {
                if (fps < min)
                {
                    min = fps;
                }
            }
            return min;
        }

        /**
         * @brief Draws error popup window.
         */
        void draw_error_popup_window()
        {
            std::string error_msg = error_queue.top_error();
            if (!error_msg.empty())
            {
                ImGui::OpenPopup("Error");
                ImGui::BeginPopup("Error");
                ImGui::Text("%s", error_msg.c_str());
                if (ImGui::Button("Acknowledged"))
                {
                    ImGui::CloseCurrentPopup();
                    error_queue.pop_error();
                }
                ImGui::EndPopup();
            }
        }

        /**
         * @brief Draws the info window and visualizer/DCE controls.
         */
        void draw_info_window()
        {
            if (selected_source_index < data_acquisition.size()) {
                std::shared_ptr<DataSource> data_source = data_acquisition.at(selected_source_index);

                DigitalCodedExposure::Parameters& dce_params = data_source->dce_parameters;
                Visualizer::Parameters& vis_params = data_source->visualizer_parameters;
                Scrubber::State& scrubber_state = data_source->scrubber.state;

                // Scrubber parameters
                ImGui::Begin("Scrubber Parameters");
                ImGui::SliderFloat("Event Discard Odds", &(data_source->event_discard_odds), 0.0f, 1.0f);
                ImGui::End();

                // Visualizer controls
                ImGui::Begin("Visualizer Parameters");
                ImGui::SliderFloat("Particle Scale", &vis_params.particle_scale, 0.1f, 6.0f);
                ImGui::Separator();
                ImGui::ColorEdit3("Negative Polarity Color", (float *)&vis_params.polarity_neg_color);
                ImGui::ColorEdit3("Positive Polarity Color", (float *)&vis_params.polarity_pos_color);
                ImGui::Separator();
                int32_t unit_type = static_cast<int32_t>(vis_params.unit_type);
                ImGui::Combo("Time Unit", &unit_type, "s\0ms\0us\0");
                vis_params.unit_type = static_cast<Visualizer::TIME>(unit_type);
                const float units[] = {1000000.0f, 1000.0f, 1.0f};
                vis_params.unit_time_conversion_factor = units[unit_type];
                ImGui::End();

                // DCE Controls
                ImGui::Begin("DCE Parameters");
                ImGui::SliderFloat("Event Contribution Weight", &dce_params.event_contrib_weight, 0.0f, 10.0f);
                ImGui::Separator();
                ImGui::Checkbox("Morlet Shutter", &dce_params.shutter_is_morlet);
                ImGui::Checkbox("Positive Events Only", &dce_params.shutter_is_positive_only);
                ImGui::Separator();
                ImGui::Combo("Digital Exposure Color", &dce_params.dce_color,
                            "High/Low\0Tricolor\0Use Visualizer Colors\0");
                if (dce_params.dce_color < 2)
                {
                    ImGui::ColorEdit3("Negative Color", (float *)&dce_params.polarity_neg_color);
                    ImGui::ColorEdit3("Positive Color", (float *)&dce_params.polarity_pos_color);
                    if (dce_params.dce_color == 1)
                    {
                        ImGui::ColorEdit3("Neutral Color", (float *)&dce_params.polarity_neut_color);
                    }
                }
                ImGui::Combo("Activation Function", &dce_params.activation_function, "Linear\0Sigmoid\0");
                ImGui::Separator();
                ImGui::SliderFloat("Morlet Frequency", &dce_params.morlet_frequency, 0.0f, 10000.0f);
                ImGui::SliderFloat("Morlet Width", &dce_params.morlet_width, 0.001f, 100000.0f);
                ImGui::End();
            }
        }

        /**
         * @brief Draw debug window containing fps data.
         * @param fps Calculated fps in current frame.
         */
        void draw_debug_window(float fps)
        {
            ImGui::Begin("Debug");
            update_fps_buffer(fps);
            float avg_fps = get_avg_fps();
            float min_fps = get_min_fps();
            float max_fps = get_max_fps();
            ImGui::Text("FPS: %.1f", fps);
            ImGui::Text("Avg FPS: %.1f", avg_fps);
            ImGui::Text("Min FPS: %.1f", min_fps);
            ImGui::Text("Max FPS: %.1f", max_fps);
            ImGui::Separator();
            ImGui::PlotLines("##FPS History", fps_history_buf.data(), static_cast<int>(fps_history_buf.size()),
                             static_cast<int>(fps_buf_index), nullptr, 0.0f, max_fps + 10.0f, ImVec2(0, 80));
            ImGui::Separator();
            if (ImGui::Button("Reset Layout"))
            {
                reset_layout_with_dockbuilder();
            }
            if (ImGui::Button("Quickstart Guide"))
            {
                show_quickstart = true;
            }
            ImGui::End();
        }

        /**
         * @brief Draw Data Sources management window.
         */
        void draw_data_sources_window()
        {
            ImGui::Begin("Data Sources");

            // View mode selection
            ImGui::Text("View Mode:");
            int mode_int = static_cast<int>(view_mode);
            if (ImGui::RadioButton("Single Source", mode_int == 0))
            {
                view_mode = ViewMode::SINGLE;
            }
            ImGui::SameLine();
            if (ImGui::RadioButton("Synced", mode_int == 1))
            {
                view_mode = ViewMode::SYNCED;
            }

            ImGui::Separator();

            // Add new sources
            ImGui::Text("Add New Source:");
            
            if (ImGui::Button("Add File Source"))
            {
                SDL_ShowOpenFileDialog(stream_file_handle_callback, &data_acquisition, window, nullptr, 0, nullptr, 0);
            }

            ImGui::SameLine();
            if (ImGui::Button("Discover Cameras"))
            {
                data_acquisition.discover_cameras();
            }

            // Camera selection for adding
            std::vector<std::string> camera_names = data_acquisition.get_scanned_camera_names();
            if (!camera_names.empty())
            {
                static int camera_selection = 0;
                std::vector<const char*> camera_names_cstr;
                for (const auto &name : camera_names)
                {
                    camera_names_cstr.push_back(name.c_str());
                }
                
                ImGui::Combo("Available Cameras", &camera_selection, camera_names_cstr.data(), camera_names_cstr.size());
                
                if (ImGui::Button("Add Selected Camera"))
                {
                    data_acquisition.add_camera_source(camera_selection);
                    selected_source_index = ((int) data_acquisition.size()) - 1;
                }
            }

            ImGui::Separator();

            // List existing sources
            ImGui::Text("Existing Sources:");
            std::vector<std::shared_ptr<DataSource>> sources = data_acquisition.get_data_sources();
            
            for (size_t i = 0; i < sources.size(); ++i)
            {
                ImGui::PushID(i);
                
                bool is_selected = (static_cast<int>(i) == selected_source_index);
                if (ImGui::Selectable(sources[i]->name.c_str(), is_selected))
                {
                    selected_source_index = static_cast<int>(i);
                }
                
                if (ImGui::SmallButton("Delete"))
                {
                    data_acquisition.remove_data_source(i);
                    if (selected_source_index >= static_cast<int>(sources.size() - 1))
                    {
                        selected_source_index = (std::max)(0, static_cast<int>(sources.size()) - 2);
                    }
                }
                
                ImGui::PopID();
            }

            ImGui::End();
        }

        /**
         * @brief Custom-draw a horizontal timeline bar with window highlight and position marker.
         * @return true if user dragged to a new position (written to out_new_val)
         */
        bool draw_timeline_bar(float min_val, float max_val, float current_val, float window_val, float *out_new_val,
                               const char *format, const char *unit_suffix, bool interactive)
        {
            bool changed = false;
            ImDrawList *draw_list = ImGui::GetWindowDrawList();
            float frame_h = ImGui::GetFrameHeight();
            float track_h = frame_h * 1.2f;
            float avail_w = ImGui::GetContentRegionAvail().x;

            // Min/max labels
            char min_buf[64], max_buf[64];
            snprintf(min_buf, sizeof(min_buf), format, min_val);
            snprintf(max_buf, sizeof(max_buf), format, max_val);
            ImVec2 min_text_size = ImGui::CalcTextSize(min_buf);
            ImVec2 max_text_size = ImGui::CalcTextSize(max_buf);
            float label_w = (std::max)(min_text_size.x, max_text_size.x) + 4.0f;
            float track_w = avail_w - 2.0f * label_w;
            if (track_w < 20.0f)
                track_w = 20.0f;

            ImVec2 cursor = ImGui::GetCursorScreenPos();

            // Draw min label
            draw_list->AddText(ImVec2(cursor.x, cursor.y + (track_h - min_text_size.y) * 0.5f),
                               IM_COL32(200, 200, 200, 255), min_buf);

            // Track rect
            ImVec2 track_tl = ImVec2(cursor.x + label_w, cursor.y);
            ImVec2 track_br = ImVec2(track_tl.x + track_w, track_tl.y + track_h);
            draw_list->AddRectFilled(track_tl, track_br, kTrackColor);
            draw_list->AddRect(track_tl, track_br, kBorderColor);

            float range = max_val - min_val;
            if (range > 0.0f)
            {
                // Window highlight
                float win_start = (std::max)(current_val - window_val, min_val);
                float win_end = (std::min)(current_val, max_val);
                float win_start_frac = (win_start - min_val) / range;
                float win_end_frac = (win_end - min_val) / range;
                ImVec2 win_tl = ImVec2(track_tl.x + win_start_frac * track_w, track_tl.y);
                ImVec2 win_br = ImVec2(track_tl.x + win_end_frac * track_w, track_br.y);
                draw_list->AddRectFilled(win_tl, win_br, kWindowHighlight);

                // Position marker
                float pos_frac = (current_val - min_val) / range;
                float pos_x = track_tl.x + pos_frac * track_w;
                draw_list->AddLine(ImVec2(pos_x, track_tl.y), ImVec2(pos_x, track_br.y), kPositionMarker, 2.0f);

                float tri_size = 6.0f;
                draw_list->AddTriangleFilled(ImVec2(pos_x - tri_size, track_tl.y - 1.0f),
                                             ImVec2(pos_x + tri_size, track_tl.y - 1.0f),
                                             ImVec2(pos_x, track_tl.y + tri_size), kPositionMarker);
            }

            // Draw max label
            draw_list->AddText(ImVec2(track_br.x + 4.0f, cursor.y + (track_h - max_text_size.y) * 0.5f),
                               IM_COL32(200, 200, 200, 255), max_buf);

            // Invisible button for drag interaction
            ImGui::SetCursorScreenPos(track_tl);
            ImGui::InvisibleButton("##timeline", ImVec2(track_w, track_h));
            if (interactive && range > 0.0f)
            {
                if (ImGui::IsItemActive())
                {
                    float mouse_x = ImGui::GetIO().MousePos.x;
                    float frac = std::clamp((mouse_x - track_tl.x) / track_w, 0.0f, 1.0f);
                    *out_new_val = min_val + frac * range;
                    changed = true;
                }
                if (ImGui::IsItemHovered())
                {
                    float mouse_x = ImGui::GetIO().MousePos.x;
                    float frac = std::clamp((mouse_x - track_tl.x) / track_w, 0.0f, 1.0f);
                    float hover_val = min_val + frac * range;
                    char tip[128];
                    snprintf(tip, sizeof(tip), format, hover_val);
                    ImGui::SetTooltip("%s %s", tip, unit_suffix);
                }
            }

            // Move cursor past the track
            ImGui::SetCursorScreenPos(ImVec2(cursor.x, track_br.y + 2.0f));

            // Text readout of current position
            char pos_buf[128];
            snprintf(pos_buf, sizeof(pos_buf), format, current_val);
            ImGui::Text("Position: %s %s", pos_buf, unit_suffix);

            return changed;
        }

        /**
         * @brief Draw a row of playback control buttons.
         */
        void draw_playback_controls(Scrubber::Mode current_mode, Scrubber::Type scrubber_type,
                                    Scrubber::State &state)
        {
            float button_w = ImGui::GetFrameHeight() * 2.0f;
            float button_h = ImGui::GetFrameHeight();

            // Jump to start
            if (ImGui::Button("|<", ImVec2(button_w, button_h)))
            {
                if (scrubber_type == Scrubber::Type::EVENT)
                {
                    state.current_index = state.min_index;
                }
                else
                {
                    state.current_time = state.min_time;
                }
                state.mode = Scrubber::Mode::PAUSED;
            }
            ImGui::SetItemTooltip("Jump to start");
            ImGui::SameLine();

            // Step backward
            bool step_zero = false;
            if (scrubber_type == Scrubber::Type::EVENT)
                step_zero = state.index_step == 0;
            else
                step_zero = state.time_step <= 0.00001f;

            if (step_zero)
                ImGui::BeginDisabled();
            if (ImGui::Button("<<", ImVec2(button_w, button_h)))
            {
                if (scrubber_type == Scrubber::Type::EVENT)
                {
                    state.current_index = (state.current_index > state.index_step + state.min_index)
                                              ? state.current_index - state.index_step
                                              : state.min_index;
                }
                else
                {
                    state.current_time = (std::max)(state.current_time - state.time_step, state.min_time);
                }
                state.mode = Scrubber::Mode::PAUSED;
            }
            ImGui::SetItemTooltip("Step backward");
            if (step_zero)
                ImGui::EndDisabled();
            ImGui::SameLine();

            // Play / Pause toggle
            if (current_mode == Scrubber::Mode::PLAYING)
            {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.2f, 0.2f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8f, 0.3f, 0.3f, 1.0f));
                if (ImGui::Button("Pause", ImVec2(button_w * 1.5f, button_h)))
                    state.mode = Scrubber::Mode::PAUSED;
                ImGui::SetItemTooltip("Pause playback");
                ImGui::PopStyleColor(2);
            }
            else
            {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.2f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.7f, 0.3f, 1.0f));
                if (ImGui::Button("Play", ImVec2(button_w * 1.5f, button_h)))
                    state.mode = Scrubber::Mode::PLAYING;
                ImGui::SetItemTooltip("Start playback");
                ImGui::PopStyleColor(2);
            }
            ImGui::SameLine();

            // Step forward
            if (step_zero)
                ImGui::BeginDisabled();
            if (ImGui::Button(">>", ImVec2(button_w, button_h)))
            {
                if (scrubber_type == Scrubber::Type::EVENT)
                {
                    state.current_index = (std::min)(state.current_index + state.index_step, state.max_index);
                }
                else
                {
                    state.current_time = (std::min)(state.current_time + state.time_step, state.max_time);
                }
                state.mode = Scrubber::Mode::PAUSED;
            }
            ImGui::SetItemTooltip("Step forward");
            if (step_zero)
                ImGui::EndDisabled();
            ImGui::SameLine();

            // Jump to end / LIVE
            if (current_mode == Scrubber::Mode::LATEST)
            {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.1f, 0.8f, 0.1f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.2f, 0.9f, 0.2f, 1.0f));
                if (ImGui::Button("LIVE", ImVec2(button_w * 1.5f, button_h)))
                    state.mode = Scrubber::Mode::PAUSED;
                ImGui::SetItemTooltip("Currently tracking latest data. Click to pause.");
                ImGui::PopStyleColor(2);
            }
            else
            {
                if (ImGui::Button(">|", ImVec2(button_w, button_h)))
                {
                    if (scrubber_type == Scrubber::Type::EVENT)
                    {
                        state.current_index = state.max_index;
                    }
                    else
                    {
                        state.current_time = state.max_time;
                    }
                    state.mode = Scrubber::Mode::LATEST;
                }
                ImGui::SetItemTooltip("Jump to end / track latest");
            }
        }


        void draw_scrubber_controls(Scrubber::State& state, Visualizer::TIME unit_time)
        {
            // Type selection via tab bar
            Scrubber::Type prev_type = state.type;
            if (ImGui::BeginTabBar("##TypeTabs"))
            {
                if (ImGui::BeginTabItem("Time"))
                {
                    state.type = Scrubber::Type::TIME;
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Event"))
                {
                    state.type = Scrubber::Type::EVENT;
                    ImGui::EndTabItem();
                }
                ImGui::EndTabBar();
            }
            if (state.type != prev_type)
            {
                state.mode = Scrubber::Mode::PAUSED;
            }

            ImGui::Separator();

            // Cap mode
            static int cap_mode_int = 0;
            const char *cap_mode_names[] = {"Capped", "Uncapped"};
            ImGui::Combo("Scrubber Cap", &cap_mode_int, cap_mode_names, 2);
            int window_div_factor = (cap_mode_int == 0) ? 100 : 2;
            int step_div_factor = (cap_mode_int == 0) ? 100 : 10;

            // If in sync mode, create drop down to select which type of synchronization 
            if (view_mode == ViewMode::SYNCED) 
            {
                static int sync_mode_int = 0;
                const char *sync_mode_names[] = {"Start", "End"};
                ImGui::Combo("Sync Mode", &sync_mode_int, sync_mode_names, 2);
                sync_mode = sync_mode_int == 0 ? SyncMode::START : SyncMode::END;
            }

            // Timeline, playback, settings
            if (state.type == Scrubber::Type::EVENT)
            {
                float min_f = static_cast<float>(state.min_index);
                float max_f = static_cast<float>(state.max_index);
                float cur_f = static_cast<float>(state.current_index);
                float win_f = static_cast<float>(state.index_window);

                if (max_f <= min_f)
                {
                    ImGui::TextDisabled("No data loaded");
                }
                else
                {
                    // Timeline
                    float new_val = cur_f;
                    if (draw_timeline_bar(min_f, max_f, cur_f, win_f, &new_val, "%.0f", "events", true))
                    {
                        if (state.mode == Scrubber::Mode::LATEST)
                            state.mode = Scrubber::Mode::PAUSED;
                        new_val = std::clamp(new_val, min_f, max_f);
                        state.current_index = static_cast<std::size_t>(new_val);
                    }

                    ImGui::Separator();

                    // Playback controls
                    draw_playback_controls(state.mode, state.type, state);

                    ImGui::Separator();

                    // Window slider
                    size_t max_window = 1000000; // (std::max)(static_cast<size_t>(1), (state.max_index - state.min_index + 1) / window_div_factor);
                    float max_window_f = static_cast<float>(max_window);
                    float win_slider = win_f;
                    if (ImGui::SliderFloat("Window", &win_slider, 1.0f, max_window_f, "%.0f"))
                    {
                        win_slider = std::clamp(win_slider, 1.0f, max_window_f);
                        state.index_window = static_cast<std::size_t>(win_slider);
                    }
                    ImGui::SetItemTooltip("Number of events behind position to display");

                    // Step slider
                    size_t max_step = 1000000; // (state.max_index - state.min_index) / step_div_factor;
                    float max_step_f = static_cast<float>(max_step);
                    float step_f = static_cast<float>(state.index_step);
                    if (ImGui::SliderFloat("Step", &step_f, 0.0f, max_step_f, "%.0f"))
                    {
                        if (max_step_f >= 0.0f)
                        {
                            step_f = std::clamp(step_f, 0.0f, max_step_f);
                            state.index_step = static_cast<size_t>(step_f);
                        }
                    }
                    ImGui::SetItemTooltip("Events to advance per frame during playback");
                }
            }
            else // TIME
            {
                std::string time_unit_suffix = time_units[static_cast<int>(unit_time)];
                std::string time_format_str;
                switch (unit_time)
                {
                case Visualizer::TIME::UNIT_US:
                    time_format_str = "%.2f";
                    break;
                case Visualizer::TIME::UNIT_MS:
                    time_format_str = "%.4f";
                    break;
                case Visualizer::TIME::UNIT_S:
                    time_format_str = "%.8f";
                    break;
                }

                const float units[] = {1000000.0f, 1000.0f, 1.0f};
                float unit_time_conversion_factor = units[(int) unit_time];

                float current_time_adj = state.current_time / unit_time_conversion_factor;
                float min_time_adj = state.min_time / unit_time_conversion_factor;
                float max_time_adj = state.max_time / unit_time_conversion_factor;
                float time_window_adj = state.time_window / unit_time_conversion_factor;

                if (max_time_adj <= min_time_adj)
                {
                    ImGui::TextDisabled("No data loaded");
                }
                else
                {
                    // Timeline
                    float new_val = current_time_adj;
                    if (draw_timeline_bar(min_time_adj, max_time_adj, current_time_adj, time_window_adj, &new_val,
                                          time_format_str.c_str(), time_unit_suffix.c_str(), true))
                    {
                        if (state.mode == Scrubber::Mode::LATEST)
                            state.mode = Scrubber::Mode::PAUSED;
                        if (max_time_adj > min_time_adj)
                        {
                            new_val = std::clamp(new_val, min_time_adj, max_time_adj);
                            state.current_time = new_val * unit_time_conversion_factor;
                        }
                    }

                    ImGui::Separator();

                    // Playback controls
                    draw_playback_controls(state.mode, state.type, state);

                    ImGui::Separator();

                    // Window slider
                    float max_window_time = 1000000; // (std::max)(0.00001f, (state.max_time - state.min_time) / window_div_factor);
                    float max_window_adj = max_window_time / unit_time_conversion_factor;

                    if (ImGui::SliderFloat("Window", &time_window_adj, 0.00001f, max_window_adj,
                                           time_format_str.c_str()))
                    {
                        if (max_window_adj > 0.00001f)
                        {
                            time_window_adj = std::clamp(time_window_adj, 0.00001f, max_window_adj);
                            state.time_window = time_window_adj * unit_time_conversion_factor;
                        }
                    }
                    ImGui::SetItemTooltip("Time window behind position to display");

                    // Step slider
                    float time_step_adj = state.time_step / unit_time_conversion_factor;
                    float max_step_time = 1000000; // (state.max_time - state.min_time) / step_div_factor;
                    float max_step_time_adj = max_step_time / unit_time_conversion_factor;
                    if (ImGui::SliderFloat("Step", &time_step_adj, 0.00001f, max_step_time_adj,
                                           time_format_str.c_str()))
                    {
                        if (max_step_time_adj > 0.00001f)
                        {
                            time_step_adj = std::clamp(time_step_adj, 0.00001f, max_step_time_adj);
                            state.time_step = time_step_adj * unit_time_conversion_factor;
                        }
                    }
                    ImGui::SetItemTooltip("Time to advance per frame during playback");
                }
            }

            ImGui::Separator();

            // Show Frame Data checkbox
            ImGui::Checkbox("Show Frame Data", &state.show_frame_data);
        }


        /**
         * @brief Draws scrubber window with controls for scrubbing through event data.
         */
        void draw_scrubber_window()
        {
            std::vector<std::shared_ptr<DataSource>> sources = data_acquisition.get_data_sources();
            int num_sources = (int) sources.size();

            ImGui::Begin("Scrubber");


            // Get state and status message depending on view mode
            Scrubber::State scrubber_state;
            std::string scrubber_message;

            if (view_mode == ViewMode::SINGLE)
            {       
                if (selected_source_index < num_sources)
                {
                    std::shared_ptr<DataSource> data_source = sources[selected_source_index];
                    scrubber_state = data_source->scrubber.state;
                    scrubber_message = "Single Source Scrubbing - " + data_source->name;
                } else 
                {
                    ImGui::End();
                    return;
                }
            }
            else if (view_mode == ViewMode::SYNCED)
            {
                if (num_sources > 0) 
                {
                    scrubber_state = data_acquisition.get_state();
                    scrubber_message = "Synced Scrubbing";
                }
                else 
                {
                    ImGui::End();
                    return;
                }
            }

            ImGui::Text("%s", scrubber_message.c_str());
            ImGui::Separator();
            
            // Draw scrubber controls
            draw_scrubber_controls(scrubber_state, Visualizer::TIME::UNIT_MS);

            
            // Write back potentially updated state to appropriate data_sources
            if (view_mode == ViewMode::SINGLE)
            {       
                if (selected_source_index >= 0 && selected_source_index < num_sources)
                {
                    sources[selected_source_index]->scrubber.state = scrubber_state;
                }
            }
            else if (view_mode == ViewMode::SYNCED)
            {
                data_acquisition.set_state(scrubber_state);

                if (sync_mode == SyncMode::END)         data_acquisition.sync_end();
                else if (sync_mode == SyncMode::START)  data_acquisition.sync_start();
            }

            ImGui::End();
        }

        /**
         * @brief Draws 3D Visualizer windows for visible data sources.
         */
        void draw_visualizers()
        {
            if (selected_source_index < data_acquisition.size())
            {
                std::shared_ptr<DataSource> data_source = data_acquisition.at(selected_source_index);
                RenderTarget& color_target = data_source->visualizer_render_targets.color;

                ImGui::Begin("3D Visualizer");
                if (color_target.texture)
                {
                    
                    ImVec2 pane_size = ImGui::GetContentRegionAvail();
                    float tex_aspect = (float)color_target.width / (float) color_target.height;
                    ImVec2 display_size = pane_size;
                    float pane_aspect = pane_size.x / pane_size.y;
                    if (tex_aspect > pane_aspect)
                    {
                        display_size.y = pane_size.x / tex_aspect;
                    }
                    else
                    {
                        display_size.x = pane_size.y * tex_aspect;
                    }
                    float x_pad = (pane_size.x - display_size.x) * 0.5f;
                    float y_pad = (pane_size.y - display_size.y) * 0.5f;
                    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + x_pad);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + y_pad);
                    ImGui::Image((ImTextureID) color_target.texture, display_size);
                    color_target.is_focused = ImGui::IsItemHovered();
                }
                else
                {
                    ImGui::Text("No texture available");
                }

                ImGui::End();
            }
            
        }

        void draw_single_dce(std::shared_ptr<DataSource> data_source, bool centered) {
            RenderTarget& output = data_source->dce_render_targets.output;

            if (output.texture)
            {
                ImVec2 display_size;
                if (centered)
                {
                    ImVec2 pane_size = ImGui::GetContentRegionAvail();
                    display_size = pane_size;
                    
                    float aspect_ratio = output.width / (float)output.height;
                    float pane_aspect = pane_size.x / pane_size.y;
                    if (aspect_ratio > pane_aspect)
                    {
                        display_size.y = pane_size.x / aspect_ratio;
                    }
                    else
                    {
                        display_size.x = pane_size.y * aspect_ratio;
                    }
                    float x_pad = (pane_size.x - display_size.x) * 0.5f;
                    float y_pad = (pane_size.y - display_size.y) * 0.5f;
                    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + x_pad);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + y_pad);
                }
                else 
                {
                    float available_width = ImGui::GetContentRegionAvail().x;
                    float aspect_ratio = output.width / (float) output.height;
                    display_size = {available_width, available_width / aspect_ratio};
                }

                // Draw image
                ImGui::Image((ImTextureID) output.texture, display_size);
                output.is_focused = ImGui::IsItemHovered();
            }
            else
            {
                ImGui::Text("No Event Data.");
            }
        }

        /**
         * @brief Draws Digital Coded Exposure windows for visible data sources.
         */
        void draw_digital_coded_exposure()
        {
            // Get all data sources
            std::vector<std::shared_ptr<DataSource>> sources = data_acquisition.get_data_sources();
            int num_sources = (int) sources.size();
            if (num_sources == 0) return;

            // Setup ImGui frame
            ImGuiStyle& style = ImGui::GetStyle();
            ImGui::Begin("Frame");
            ImGui::Text("Digital Coded Exposure");

            // Single view fills entire window
            if (view_mode == ViewMode::SINGLE) 
            {
                if (selected_source_index >= 0 && selected_source_index < num_sources) {
                    draw_single_dce(sources[selected_source_index], true);
                }   
            }

            // Shared view drawn in 2xN grid
            else if (view_mode == ViewMode::SYNCED)
            {       
                // Set table size
                int num_cols = 2;
                int num_rows = std::ceil(num_sources / (double) num_cols);

                // Make table fill horizontally and scroll vertically
                ImGuiTableFlags flags = ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingFixedFit;
                if (ImGui::BeginTable("", num_cols, flags))
                {
                    // Calculate width of columns
                    float col_width = (ImGui::GetContentRegionAvail().x - style.ScrollbarSize) / num_cols;
                    ImGui::TableSetupColumn("col0", ImGuiTableColumnFlags_WidthFixed, col_width);
                    ImGui::TableSetupColumn("col1", ImGuiTableColumnFlags_WidthFixed, col_width);
    
                    // Draw individual DCE textures
                    for (int row=0; row < num_rows; row++) {
                        ImGui::TableNextRow();
                        for (int col=0; col<num_cols; col++) {
                            ImGui::TableSetColumnIndex(col);
                            
                            int source_index = row*num_cols + col;
                            if (source_index < num_sources)
                            {
                                draw_single_dce(sources[source_index], false);
                            }
                        }
                    }
    
                    ImGui::EndTable();
                }
            }

            ImGui::End();
            
        }

        /**
         * @brief Markdown render of quickstart guide.
         */
        void draw_quickstart_window()
        {
            if (show_quickstart)
                ImGui::OpenPopup("Quickstart Guide");

            const ImGuiViewport *viewport = ImGui::GetMainViewport();
            ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
            ImVec2 windowSize = ImVec2(viewport->Size.x * 0.75f, viewport->Size.y * 0.75f);
            ImGui::SetNextWindowSize(windowSize, ImGuiCond_Appearing);

            if (ImGui::BeginPopupModal("Quickstart Guide", &show_quickstart))
            {
                ImGui::BeginChild("QSContent", ImVec2(0, -50), true, ImGuiWindowFlags_HorizontalScrollbar);

                ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f),
                                   "You can view this popup again by clicking the 'Quickstart Guide' button in the debug window.");

                ImGui::Separator();
                ImGui::TextColored(ImVec4(0.0f, 1.0f, 1.0f, 1.0f), "Data Sources");
                ImGui::Separator();
                ImGui::TextWrapped(
                    "The application now supports multiple data sources. Use the Data Sources window to add files or cameras. "
                    "You can switch between Single Source mode (view one at a time) or Synced mode (view all sources with a shared scrubber).");

                ImGui::Spacing();
                ImGui::TextColored(ImVec4(0.0f, 1.0f, 1.0f, 1.0f), "Camera Controls");
                ImGui::Separator();
                ImGui::TextWrapped(
                    "Click and drag in the 3D Visualizer to rotate the camera. Use mouse wheel to zoom.");

                ImGui::EndChild();
                ImGui::Separator();

                if (ImGui::Button("Got it!"))
                {
                    ImGui::CloseCurrentPopup();
                    show_quickstart = false;
                }
                ImGui::EndPopup();
            }
        }

    public:
        /**
         * @brief Constructor for GUI.
         */
        GUI(DataAcquisition &data_acquisition, Visualizer &visualizer, ErrorQueue &error_queue, SDL_Window *window, SDL_GPUDevice *gpu_device)
            : data_acquisition(data_acquisition), visualizer(visualizer), error_queue(error_queue), window(window), gpu_device(gpu_device), fps_history_buf(100, 0.0f), 
              fps_buf_index(0), check_for_layout_file(true), show_quickstart(false)
        {

            // Setup Dear ImGui context
            IMGUI_CHECKVERSION();
            ImGui::CreateContext();
            ImGuiIO &io = ImGui::GetIO();
            (void)io;
            io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
            io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
            io.ConfigWindowsMoveFromTitleBarOnly = true;

            void *font_memory = malloc(sizeof CascadiaCode_ttf);
            std::memcpy(font_memory, CascadiaCode_ttf, sizeof CascadiaCode_ttf);
            io.Fonts->AddFontFromMemoryTTF(font_memory, sizeof CascadiaCode_ttf, 16.0f);

            // Setup scaling
            float scaling_factor = SDL_GetDisplayContentScale(SDL_GetPrimaryDisplay());
            ImGuiStyle &style = ImGui::GetStyle();
            style.ScaleAllSizes(scaling_factor);
            style.FontScaleDpi = scaling_factor;
            io.ConfigDpiScaleFonts = true;
            io.ConfigDpiScaleViewports = true;

            // Setup Platform/Renderer backends
            ImGui_ImplSDL3_InitForSDLGPU(window);
            ImGui_ImplSDLGPU3_InitInfo init_info = {.Device = gpu_device,
                                                    .ColorTargetFormat =
                                                        SDL_GetGPUSwapchainTextureFormat(gpu_device, window),
                                                    .MSAASamples = SDL_GPU_SAMPLECOUNT_1,
                                                    .SwapchainComposition = SDL_GPU_SWAPCHAINCOMPOSITION_SDR,
                                                    .PresentMode = SDL_GPU_PRESENTMODE_VSYNC};
            ImGui_ImplSDLGPU3_Init(&init_info);
        }

        /**
         * @brief Destructor. Cleans up IMGUI.
         */
        ~GUI()
        {
            ImGui_ImplSDL3_Shutdown();
            ImGui_ImplSDLGPU3_Shutdown();
            ImGui::DestroyContext();
        }

        /**
         * @brief IMGUI and camera control event handler.
         * @param event SDL event to process.
         */
        void event_handler(SDL_Event *event)
        {
            ImGui_ImplSDL3_ProcessEvent(event);

            // Handle camera controls for visualizer
            std::vector<std::shared_ptr<DataSource>> sources = data_acquisition.get_data_sources();
            bool any_focused = false;

            // Check if any visualizer window is focused
            for (const auto &source : sources)
            {
                if (source->visualizer_render_targets.color.is_focused)
                {
                    any_focused = true;
                    break;
                }
            }
            if (any_focused)
            {
                switch (event->type)
                {
                case SDL_EVENT_MOUSE_BUTTON_DOWN:
                    if (event->button.button == SDL_BUTTON_LEFT)
                    {
                        is_mouse_dragging = true;
                        SDL_HideCursor();
                        SDL_SetWindowRelativeMouseMode(window, true);
                        cursor_captured = true;
                    }
                    break;

                case SDL_EVENT_MOUSE_BUTTON_UP:
                    if (event->button.button == SDL_BUTTON_LEFT)
                    {
                        is_mouse_dragging = false;
                        if (cursor_captured)
                        {
                            SDL_SetWindowRelativeMouseMode(window, false);
                            SDL_ShowCursor();
                            cursor_captured = false;
                        }
                    }
                    break;

                case SDL_EVENT_MOUSE_MOTION:
                    if (is_mouse_dragging && cursor_captured)
                    {
                        float x_offset = -event->motion.xrel;
                        float y_offset = event->motion.yrel;
                        visualizer.rotate_camera(x_offset, y_offset);
                    }
                    break;

                case SDL_EVENT_MOUSE_WHEEL:
                {
                    float scroll_delta = event->wheel.y;
                    if (event->wheel.direction == SDL_MOUSEWHEEL_FLIPPED)
                        scroll_delta = -scroll_delta;

                    if (scroll_delta != 0.0f)
                    {
                        visualizer.zoom_camera(scroll_delta * 0.1f);
                    }
                    break;
                }

                case SDL_EVENT_WINDOW_FOCUS_LOST:
                    if (cursor_captured)
                    {
                        SDL_SetWindowRelativeMouseMode(window, false);
                        SDL_ShowCursor();
                        cursor_captured = false;
                        is_mouse_dragging = false;
                    }
                    break;
                }

                // Handle panning (translation) for the SLAM global point cloud. Have this outside of above switch statement
                // because user might want to rotate camera and pan at the same time
                if(event->type == SDL_EVENT_KEY_DOWN && visualizer.is_slam_running() && visualizer.is_global_pointcloud_displayed())
                {
                    const float pan_distance = 0.5;
                    // X = left/right, Y = Up/Down, Z=forwards/backwards.
                    glm::vec3 direction(0, 0, 0);
                    // Setting direction. Don't use a switch because may want to hit multiple keys at once
                    if(event->key.key == SDLK_W)
                    {
                        direction.z += 1;
                    }
                    if(event->key.key == SDLK_S)
                    {
                        direction.z -= 1;
                    }
                    if(event->key.key ==  SDLK_A)
                    {
                        direction.x += 1;
                    }
                    if(event->key.key ==  SDLK_D)
                    {
                        direction.x -= 1;
                    }
                    if(event->key.key ==  SDLK_Q)
                    {
                        direction.y += 1;
                    }
                    if(event->key.key ==  SDLK_E)
                    {
                        direction.y -= 1;
                    }
                    // Panning the camera
                    if(direction.x !=0 || direction.y !=0 || direction.z != 0)
                    {
                        visualizer.pan_camera(direction, pan_distance);
                    }
                }

                // Clear focus flags
                for (auto &source : sources)
                {
                    source->visualizer_render_targets.color.is_focused = false;
                }
            }
        }

        /**
         * @brief Prepares to renders the GUI in current frame.
         * @param command_buffer GPU command buffer.
         * @param fps Calculated fps in current frame.
         */
        void prepare_to_render(SDL_GPUCommandBuffer *command_buffer, float fps)
        {
            ImGui_ImplSDLGPU3_NewFrame();
            ImGui_ImplSDL3_NewFrame();
            ImGui::NewFrame();

            ImGuiID dockspace_id = ImGui::GetMainViewport()->ID;
            ImGui::DockSpaceOverViewport(dockspace_id);

            if (check_for_layout_file)
            {
                if (!std::filesystem::exists("imgui.ini"))
                {
                    reset_layout_with_dockbuilder();
                    show_quickstart = true;
                }
                check_for_layout_file = false;
            }

            draw_error_popup_window();
            draw_info_window();
            draw_debug_window(fps);
            draw_data_sources_window();
            draw_scrubber_window();
            draw_digital_coded_exposure();
            draw_visualizers();
            draw_quickstart_window();

            ImGui::Render();
            draw_data = ImGui::GetDrawData();
            ImGui_ImplSDLGPU3_PrepareDrawData(draw_data, command_buffer);
        }

        /**
         * @brief Renders the current GUI frame.
         * @param command_buffer GPU command buffer.
         * @param render_pass Render pass.
         */
        void render(SDL_GPUCommandBuffer *command_buffer, SDL_GPURenderPass *render_pass)
        {
            ImGui_ImplSDLGPU3_RenderDrawData(draw_data, command_buffer, render_pass);
        }

        void render_viewports()
        {
            // Multi-viewport disabled while weird bug is being investigated
        }

        /**
         * @brief Resets GUI layout back to default.
         */
        void reset_layout_with_dockbuilder()
        {
            ImGuiID dockspace_id = ImGui::GetMainViewport()->ID;
            ImGui::DockBuilderRemoveNode(dockspace_id);
            ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
            ImGui::DockBuilderSetNodeSize(dockspace_id, ImGui::GetMainViewport()->Size);

            ImGuiID dock_id_right;
            ImGuiID dock_id_main = dockspace_id;
            ImGui::DockBuilderSplitNode(dock_id_main, ImGuiDir_Right, 0.25f, &dock_id_right, &dock_id_main);

            ImGuiID dock_id_left_bottom;
            ImGui::DockBuilderSplitNode(dock_id_main, ImGuiDir_Down, 0.25f, &dock_id_left_bottom, &dock_id_main);

            ImGuiID dock_id_right_top = dock_id_right;
            ImGuiID dock_id_right_bottom;
            ImGui::DockBuilderSplitNode(dock_id_right_top, ImGuiDir_Down, 0.35f, &dock_id_right_bottom,
                                        &dock_id_right_top);

            ImGuiID dock_id_right_top_top = dock_id_right_top;
            ImGuiID dock_id_right_top_bottom;
            ImGui::DockBuilderSplitNode(dock_id_right_top_top, ImGuiDir_Down, 0.45f, &dock_id_right_top_bottom,
                                        &dock_id_right_top_top);

            ImGui::DockBuilderDockWindow("Scrubber Parameters", dock_id_right_top_bottom);
            ImGui::DockBuilderDockWindow("DCE Parameters", dock_id_right_top_bottom);
            ImGui::DockBuilderDockWindow("Visualizer Parameters", dock_id_right_top_bottom);
            ImGui::DockBuilderDockWindow("Debug", dock_id_right_top_top);
            ImGui::DockBuilderDockWindow("Data Sources", dock_id_right_top_top);
            ImGui::DockBuilderDockWindow("Frame", dock_id_main);
            ImGui::DockBuilderDockWindow("3D Visualizer", dock_id_right_bottom);
            ImGui::DockBuilderDockWindow("Scrubber", dock_id_left_bottom);

            ImGui::DockBuilderFinish(dockspace_id);
        }
};

// Callback functions
inline void SDLCALL stream_file_handle_callback(void *user_data, const char *const *data_file_list, int filter_unused)
{
    DataAcquisition *data_acq = static_cast<DataAcquisition*>(user_data);
    if (data_file_list && *data_file_list)
    {
        std::string file_name{*data_file_list};
        data_acq->add_file_source(file_name);
    }
    else
    {
        std::cerr << "Error happened when selecting file or no file was chosen" << std::endl;
    }
}

#endif // GUI_HH