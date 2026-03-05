#pragma once

#ifndef GUI_HH
#define GUI_HH

#include "pch.hh"

#include "imgui_internal.h"

#include "ParameterStore.hh"
#include "RenderTarget.hh"
#include "Scrubber.hh"

#include "fonts/CascadiaCode.ttf.h"
// Declared here to use with callback functions.

// Callback used with SDL_ShowOpenFileDialog in draw_stream_window
inline void SDLCALL stream_file_handle_callback(void *param_store, const char *const *data_file_list,
                                                int filter_unused);

// Callback used with SDL_ShowSaveFileDialog in draw_stream_window
inline void SDLCALL save_stream_handle_callback(void *param_store, const char *const *data_file_list,
                                                int filter_unused);

/**
 * @brief This class provides functions to draw the GUI.
 */
class GUI
{
    private:
        std::unordered_map<std::string, RenderTarget> &render_targets;
        ParameterStore *parameter_store;
        SDL_Window *window = nullptr;
        SDL_GPUDevice *gpu_device = nullptr;
        ImDrawData *draw_data = nullptr;
        Scrubber *scrubber = nullptr;

        static inline const std::string time_units[] = {"(s)", "(ms)", "(us)"};

        // Timeline visual constants
        static constexpr ImU32 kTrackColor      = IM_COL32(60, 60, 60, 255);
        static constexpr ImU32 kWindowHighlight = IM_COL32(80, 130, 200, 100);
        static constexpr ImU32 kPositionMarker  = IM_COL32(255, 200, 50, 255);
        static constexpr ImU32 kBorderColor     = IM_COL32(120, 120, 120, 255);

        // Circular fps buffer
        std::vector<float> fps_history_buf;
        size_t fps_buf_index;

        bool check_for_layout_file;
        bool show_quickstart;

        /**
         * @brief Update circular buffer of fps data.
         *        From old NOVA source code
         * @param fps calculated fps to add to circular buffer.
         */
        void update_fps_buffer(const float &fps)
        {
            fps_history_buf[fps_buf_index] = fps;
            fps_buf_index = (fps_buf_index + 1) % fps_history_buf.size();
        }

        /**
         * @brief Get average fps from circular fps buffer.
         *        From old NOVA source code
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
         *        From old NOVA source code
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
         *        From old NOVA source code
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
         * @brief Draws error popup window. Will read
         *        pop_up_err_str in parameter store for
         *        a non-empty string. If a non-empty string
         *        is detected, then the pop up will occur on the frame
         *        with the string as message.
         */
        void draw_error_popup_window()
        {

            if (parameter_store->exists("pop_up_err_str") &&
                parameter_store->get<std::string>("pop_up_err_str") != std::string{""})
            {
                // Error string found, open pop up
                ImGui::OpenPopup("Error");

                ImGui::BeginPopup("Error");
                // Stop loading
                parameter_store->add("program_state", GUI::PROGRAM_STATE::IDLE);

                std::string pop_up_err_str{parameter_store->get<std::string>("pop_up_err_str")};
                ImGui::Text("%s", pop_up_err_str.c_str());

                if (ImGui::Button("Acknowledged"))
                {
                    parameter_store->add("pop_up_err_str", std::string{""}); // Reset popup window

                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }
        }

        /**
         * @brief Recreate info window from old NOVA.
         *        Draws the info window and DCE controls.
         */
        void draw_info_window()
        {
            // Info window
            ImGui::Begin("Info");

            if (!parameter_store->exists("particle_scale"))
            {
                parameter_store->add("particle_scale", 3.0f); // Default particle scale
            }

            float particle_scale{parameter_store->get<float>("particle_scale")};
            ImGui::SliderFloat("Particle Scale", &particle_scale, 0.1f, 6.0f);
            parameter_store->add("particle_scale", particle_scale);

            ImGui::Separator();

            if (!parameter_store->exists("polarity_neg_color"))
            {
                parameter_store->add("polarity_neg_color", glm::vec3(1.0f, 0.0f, 0.0f)); // Default particle scale
            }

            glm::vec3 polarity_neg_color{parameter_store->get<glm::vec3>("polarity_neg_color")};

            if (!parameter_store->exists("polarity_pos_color"))
            {
                parameter_store->add("polarity_pos_color", glm::vec3(0.0f, 1.0f, 0.0f)); // Default particle scale
            }
            glm::vec3 polarity_pos_color{parameter_store->get<glm::vec3>("polarity_pos_color")};

            ImGui::ColorEdit3("Negative Polarity Color", (float *)&polarity_neg_color);
            ImGui::ColorEdit3("Positive Polarity Color", (float *)&polarity_pos_color);

            parameter_store->add("polarity_neg_color", polarity_neg_color);
            parameter_store->add("polarity_pos_color", polarity_pos_color);

            ImGui::Separator();

            if (!parameter_store->exists("unit_type"))
            {
                parameter_store->add("unit_type", static_cast<uint8_t>(TIME::UNIT_US));
            }
            uint8_t unit_type{parameter_store->get<uint8_t>("unit_type")};

            const float units[] = {1000000.0f, 1000.0f, 1.0f};

            int32_t unit_type_copy{unit_type};
            ImGui::Combo("Time Unit", &unit_type_copy, "s\0ms\0us\0");
            unit_type = static_cast<uint8_t>(unit_type_copy);
            parameter_store->add("unit_time_conversion_factor", units[unit_type]);
            parameter_store->add("unit_type", unit_type);

            ImGui::End();

            ImGui::Begin("Digital Coded Exposure Controls");

            if (!parameter_store->exists("event_contrib_weight"))
            {
                parameter_store->add("event_contrib_weight", 0.5f);
            }
            float event_contrib_weight{parameter_store->get<float>("event_contrib_weight")};
            ImGui::SliderFloat("Event Contribution Weight", &event_contrib_weight, 0.0f, 10.0f);
            parameter_store->add("event_contrib_weight", event_contrib_weight);

            ImGui::Separator();

            if (!parameter_store->exists("shutter_is_morlet"))
            {
                parameter_store->add("shutter_is_morlet", false);
            }
            bool shutter_is_morlet{parameter_store->get<bool>("shutter_is_morlet")};
            ImGui::Checkbox("Morlet Shutter", &shutter_is_morlet);
            parameter_store->add("shutter_is_morlet", shutter_is_morlet);

            if (!parameter_store->exists("shutter_is_positive_only"))
            {
                parameter_store->add("shutter_is_positive_only", false);
            }
            bool shutter_is_positive_only{parameter_store->get<bool>("shutter_is_positive_only")};
            ImGui::Checkbox("Positive Events Only", &shutter_is_positive_only);
            parameter_store->add("shutter_is_positive_only", shutter_is_positive_only);

            ImGui::Separator();

            if (!parameter_store->exists("dce_color"))
            {
                parameter_store->add("dce_color", 0);
            }
            int32_t dce_color{parameter_store->get<int32_t>("dce_color")};

            ImGui::Combo("Digital Exposure Color", &dce_color, "High/Low\0Tricolor\0Use Visualizer Colors\0");

            parameter_store->add("dce_color", dce_color);

            if (!parameter_store->exists("polarity_neg_color_dce"))
            {
                parameter_store->add("polarity_neg_color_dce", glm::vec3(0.0f, 0.0f, 0.0f)); // Default particle scale
            }
            glm::vec3 polarity_neg_color_dce{parameter_store->get<glm::vec3>("polarity_neg_color_dce")};

            if (!parameter_store->exists("polarity_pos_color_dce"))
            {
                parameter_store->add("polarity_pos_color_dce", glm::vec3(1.0f, 1.0f, 1.0f)); // Default particle scale
            }
            glm::vec3 polarity_pos_color_dce{parameter_store->get<glm::vec3>("polarity_pos_color_dce")};

            if (!parameter_store->exists("polarity_neut_color_dce"))
            {
                parameter_store->add("polarity_neut_color_dce", glm::vec3(0.5f, 0.5f, 0.5f)); // Default particle scale
            }
            glm::vec3 polarity_neut_color_dce{parameter_store->get<glm::vec3>("polarity_neut_color_dce")};

            if (dce_color < 2) // Only allow editing colors if using visualizer colors
            {
                ImGui::ColorEdit3("Negative Color", (float *)&polarity_neg_color_dce);
                ImGui::ColorEdit3("Positive Color", (float *)&polarity_pos_color_dce);
                if (dce_color == 1)
                {
                    ImGui::ColorEdit3("Neutral Color", (float *)&polarity_neut_color_dce);
                }
            }

            parameter_store->add("polarity_neg_color_dce", polarity_neg_color_dce);
            parameter_store->add("polarity_pos_color_dce", polarity_pos_color_dce);
            parameter_store->add("polarity_neut_color_dce", polarity_neut_color_dce);

            if (!parameter_store->exists("combine_color"))
            {
                parameter_store->add("combine_color", false);
            }
            bool combine_color{parameter_store->get<bool>("combine_color")};
            // if(dce_color == 1){
            //     ImGui::Checkbox("Combine Simultaneous Event Color", &combine_color);
            // }
            parameter_store->add("combine_color", combine_color);

            if (!parameter_store->exists("activation_function"))
            {
                parameter_store->add("activation_function", 0);
            }
            int32_t activation_function{parameter_store->get<int32_t>("activation_function")};

            ImGui::Combo("Activation Function", &activation_function, "Linear\0Sigmoid\0");

            parameter_store->add("activation_function", activation_function);

            ImGui::Separator();

            if (!parameter_store->exists("morlet_frequency"))
            {
                parameter_store->add("morlet_frequency", 0.0f);
            }
            float morlet_frequency{parameter_store->get<float>("morlet_frequency")};
            ImGui::SliderFloat("Morlet Frequency", &morlet_frequency, 0.0f, 10000.0f);
            parameter_store->add("morlet_frequency", morlet_frequency);

            if (!parameter_store->exists("morlet_width"))
            {
                parameter_store->add("morlet_width", 0.01f);
            }
            float morlet_width{parameter_store->get<float>("morlet_width")};
            ImGui::SliderFloat("Morlet Width", &morlet_width, 0.001f, 100000.0f);
            parameter_store->add("morlet_width", morlet_width);

            ImGui::End();
        }

        /**
         * @brief Draw debug window containing fps data.
         * @param fps Calculated fps in current frame.
         */
        void draw_debug_window(float fps)
        {
            ImGui::Begin("Debug");
            // FPS

            // Update fps buffer
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
         * @brief Draw Streaming window. Contains controls for streaming data in.
         */
        void draw_stream_window()
        {
            ImGui::Begin("Streaming");

            if (!parameter_store->exists("program_state"))
            {
                parameter_store->add("program_state", GUI::PROGRAM_STATE::IDLE);
            }

            GUI::PROGRAM_STATE program_state{parameter_store->get<GUI::PROGRAM_STATE>("program_state")};

            // Display program state
            ImGui::Text("Program State:");
            switch (program_state)
            {
            case GUI::PROGRAM_STATE::IDLE:
                ImGui::Text("Program Is Currently Doing Nothing.");
                break;
            case GUI::PROGRAM_STATE::FILE_STREAM:
                ImGui::Text("Program Is Currently Streaming From FILE.");
                break;
            case GUI::PROGRAM_STATE::CAMERA_STREAM:
                ImGui::Text("Program Is Currently Streaming From CAMERA.");
                break;
            }

            ImGui::Separator();
            if (!parameter_store->exists("event_discard_odds"))
            {
                parameter_store->add("event_discard_odds", 1.0f);
            }

            // The higher this value is, the higher chance events will be discarded
            float event_discard_odds{parameter_store->get<float>("event_discard_odds")};
            ImGui::Text("Event Discard Odds");
            ImGui::SliderFloat("##Frequency Of Discarded Events", &event_discard_odds, 1.0f, 1500.0f, "%f");
            parameter_store->add("event_discard_odds", event_discard_odds);

            ImGui::Separator();

            // Stream from camera
            ImGui::Text("Stream From Camera:");
            if (ImGui::Button("Scan For Cameras"))
            {
                parameter_store->add("start_camera_scan", true);
            }

            if (!parameter_store->exists("camera_index"))
            {
                parameter_store->add("camera_index", -1);
            }

            int32_t camera_index = parameter_store->get<int32_t>("camera_index");
            int32_t camera_index_copy{camera_index};

            if (!parameter_store->exists("discovered_cameras"))
            {
                parameter_store->add("discovered_cameras", std::vector<std::string>{});
            }

            std::vector<std::string> discovered_cameras{
                parameter_store->get<std::vector<std::string>>("discovered_cameras")};

            // This is stupid but it seems to work
            // Dynamically populate IMGUI combo with camera options.
            std::vector<const char *> discovered_cameras_char{};
            for (std::string &element : discovered_cameras)
            {
                discovered_cameras_char.push_back(element.c_str());
            }
            ImGui::Combo("Camera", &camera_index, discovered_cameras_char.data(), discovered_cameras_char.size());

            if (camera_index_copy != camera_index) // Different camera chosen
            {
                parameter_store->add("camera_changed", true);
            }

            parameter_store->add("camera_index", camera_index);

            if (ImGui::Button(program_state == GUI::PROGRAM_STATE::CAMERA_STREAM ? "Stop Streaming"
                                                                                 : "Stream From Camera"))
            {
                if (program_state != GUI::PROGRAM_STATE::CAMERA_STREAM)
                {
                    parameter_store->add("camera_changed", true); // Reset reader
                    parameter_store->add("program_state", GUI::PROGRAM_STATE::CAMERA_STREAM);
                }
                else
                {
                    parameter_store->add("program_state", GUI::PROGRAM_STATE::IDLE);
                }
            }

            if (!parameter_store->exists("camera_stream_paused"))
            {
                parameter_store->add("camera_stream_paused", false);
            }

            bool camera_stream_paused{parameter_store->get<bool>("camera_stream_paused")};

            // Pause or resume stream
            if (ImGui::Button(camera_stream_paused ? "Camera Resume" : "Camera Pause"))
            {
                parameter_store->add("camera_stream_paused", !camera_stream_paused); // Toggle whether stream is paused
            }

            ImGui::Separator();

            // Stream from file
            ImGui::Text("Stream From File:");
            if (ImGui::Button("Open File To Stream"))
            {
                SDL_ShowOpenFileDialog(stream_file_handle_callback, parameter_store, nullptr, nullptr, 0, nullptr, 0);
            }

            if (!parameter_store->exists("stream_paused"))
            {
                parameter_store->add("stream_paused", false);
            }

            bool stream_paused{parameter_store->get<bool>("stream_paused")};
            // Pause or resume stream
            if (ImGui::Button(stream_paused ? "Resume" : "Pause"))
            {
                parameter_store->add("stream_paused", !stream_paused); // Toggle whether stream is paused
            }

            ImGui::Separator();

            // Stream save options
            ImGui::Text("Stream Save Options:");

            if (!parameter_store->exists("saving_message"))
            {
                std::string saving_message{"Nothing Being Saved Currently"};
                parameter_store->add("saving_message", saving_message);
            }

            std::string saving_message{parameter_store->get<std::string>("saving_message")};
            ImGui::Text("%s", saving_message.c_str());

            if (!parameter_store->exists("stream_save_frames"))
            {
                parameter_store->add("stream_save_frames", false);
            }

            bool stream_save_frames{parameter_store->get<bool>("stream_save_frames")};
            bool stream_save_frames_copy{stream_save_frames};
            // Save or stop saving stream frames
            ImGui::Checkbox("Save Frames On Next Stream (Will Stop Streaming)", &stream_save_frames);
            if (stream_save_frames != stream_save_frames_copy)
            {
                parameter_store->add("program_state",
                                     GUI::PROGRAM_STATE::IDLE); // Stop program to ensure correct initialization
            }
            parameter_store->add("stream_save_frames", stream_save_frames);

            if (!parameter_store->exists("stream_save_events"))
            {
                parameter_store->add("stream_save_events", false);
            }

            bool stream_save_events{parameter_store->get<bool>("stream_save_events")};
            bool stream_save_events_copy{stream_save_events};
            // Save or stop saving stream events
            ImGui::Checkbox("Save Events On Next Stream (Will Stop Streaming)", &stream_save_events);
            if (stream_save_events_copy != stream_save_events)
            {
                parameter_store->add("program_state",
                                     GUI::PROGRAM_STATE::IDLE); // Stop program to ensure correct initialization
            }
            parameter_store->add("stream_save_events", stream_save_events);

            if (!parameter_store->exists("stream_save_file_name"))
            {
                std::string stream_save_file_name{""}; // No filename
                parameter_store->add("stream_save_file_name", stream_save_file_name);
            }

            std::string stream_save_file_name{parameter_store->get<std::string>("stream_save_file_name")};

            if ((stream_save_frames || stream_save_events) && stream_save_file_name != "")
            {
                std::string will_save_message{"Will Save Streamed "};
                if (stream_save_events)
                {
                    will_save_message.append("Event Data ");
                }
                if (stream_save_frames)
                {
                    will_save_message.append(stream_save_events ? "And Frame Data " : "Frame Data ");
                }
                will_save_message.append("To \n");
                will_save_message.append(stream_save_file_name);
                will_save_message.append(" On Next Stream");
                ImGui::Text("%s", will_save_message.c_str());
            }
            else
            {
                ImGui::Text("Nothing Being Saved On Next Stream");
            }

            if (ImGui::Button("Open File To Save Stream To (Will Stop Streaming)"))
            {
                SDL_ShowSaveFileDialog(save_stream_handle_callback, parameter_store, nullptr, nullptr, 0, nullptr);
            }

            ImGui::End();
        }

        /**
         * @brief Draws 3D Visualizer window (event data particle plot) into IMGUI.
         */
        void draw_visualizer()
        {
            ImGui::Begin("3D Visualizer");

            // Check if the render target map and the specific target exist
            if (render_targets.count("VisualizerColor"))
            {
                SDL_GPUTexture *texture = render_targets.at("VisualizerColor").texture;
                if (texture)
                {
                    // Get the available pane size
                    ImVec2 pane_size = ImGui::GetContentRegionAvail();

                    // Get texture dimensions to calculate aspect ratio
                    Uint32 tex_w, tex_h;
                    float tex_aspect = (float)render_targets.at("VisualizerColor").width /
                                       (float)render_targets.at("VisualizerColor").height;

                    // Calculate display size to fit the pane while maintaining aspect ratio
                    ImVec2 display_size = pane_size;
                    float pane_aspect = pane_size.x / pane_size.y;

                    if (tex_aspect > pane_aspect)
                    {
                        // Texture is wider than pane, fit to width
                        display_size.y = pane_size.x / tex_aspect;
                    }
                    else
                    {
                        // Texture is taller than pane (or same aspect), fit to height
                        display_size.x = pane_size.y * tex_aspect;
                    }

                    // Center the image within the pane
                    float x_pad = (pane_size.x - display_size.x) * 0.5f;
                    float y_pad = (pane_size.y - display_size.y) * 0.5f;
                    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + x_pad);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + y_pad);

                    // Display the image. ImTextureID is typedef'd to SDL_GPUTexture*
                    ImGui::Image((ImTextureID)texture, display_size);

                    // Check if the item (image) we just rendered is hovered
                    render_targets.at("VisualizerColor").is_focused = ImGui::IsItemHovered();
                }
                else
                {
                    ImGui::Text("Texture for 'VisualizerColor' is null.");
                }
            }
            else
            {
                ImGui::Text("Render target 'VisualizerColor' not found.");
            }
            ImGui::End();
        }

        /**
         * @brief Custom-draw a horizontal timeline bar with window highlight and position marker.
         * @return true if user dragged to a new position (written to out_new_val)
         */
        bool draw_timeline_bar(float min_val, float max_val, float current_val, float window_val,
                               float *out_new_val, const char *format, const char *unit_suffix, bool interactive)
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
            float label_w = std::max(min_text_size.x, max_text_size.x) + 4.0f;
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
                // Window highlight (from current - window to current)
                float win_start = std::max(current_val - window_val, min_val);
                float win_end = std::min(current_val, max_val);
                float win_start_frac = (win_start - min_val) / range;
                float win_end_frac = (win_end - min_val) / range;

                ImVec2 win_tl = ImVec2(track_tl.x + win_start_frac * track_w, track_tl.y);
                ImVec2 win_br = ImVec2(track_tl.x + win_end_frac * track_w, track_br.y);
                draw_list->AddRectFilled(win_tl, win_br, kWindowHighlight);

                // Position marker (vertical line + triangle)
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
         * @brief Draw a row of playback control buttons (|<, <<, Play/Pause, >>, >|/LIVE).
         */
        void draw_playback_controls(Scrubber::ScrubberMode current_mode, Scrubber::ScrubberType scrubber_type)
        {
            float button_w = ImGui::GetFrameHeight() * 2.0f;
            float button_h = ImGui::GetFrameHeight();

            // |< Jump to start
            if (ImGui::Button("|<", ImVec2(button_w, button_h)))
            {
                if (scrubber_type == Scrubber::ScrubberType::EVENT)
                {
                    if (scrubber)
                    {
                        size_t min_idx = parameter_store->get<std::size_t>("scrubber.min_index");
                        parameter_store->add("scrubber.current_index", min_idx);
                    }
                }
                else
                {
                    if (scrubber)
                    {
                        float min_time = parameter_store->get<float>("scrubber.min_time");
                        parameter_store->add("scrubber.current_time", min_time);
                    }
                }
                parameter_store->add("scrubber.mode", Scrubber::ScrubberMode::PAUSED);
            }
            ImGui::SetItemTooltip("Jump to start");

            ImGui::SameLine();

            // << Step backward
            bool step_zero = false;
            if (scrubber_type == Scrubber::ScrubberType::EVENT)
                step_zero = parameter_store->get<std::size_t>("scrubber.index_step") == 0;
            else
                step_zero = parameter_store->get<float>("scrubber.time_step") <= 0.00001f;

            if (step_zero)
                ImGui::BeginDisabled();
            if (ImGui::Button("<<", ImVec2(button_w, button_h)))
            {
                if (scrubber_type == Scrubber::ScrubberType::EVENT)
                {
                    size_t cur = parameter_store->get<std::size_t>("scrubber.current_index");
                    size_t step = parameter_store->get<std::size_t>("scrubber.index_step");
                    size_t min_idx = parameter_store->get<std::size_t>("scrubber.min_index");
                    cur = (cur > step + min_idx) ? cur - step : min_idx;
                    parameter_store->add("scrubber.current_index", cur);
                }
                else
                {
                    float cur = parameter_store->get<float>("scrubber.current_time");
                    float step = parameter_store->get<float>("scrubber.time_step");
                    float min_t = parameter_store->get<float>("scrubber.min_time");
                    cur = std::max(cur - step, min_t);
                    parameter_store->add("scrubber.current_time", cur);
                }
                parameter_store->add("scrubber.mode", Scrubber::ScrubberMode::PAUSED);
            }
            ImGui::SetItemTooltip("Step backward");
            if (step_zero)
                ImGui::EndDisabled();

            ImGui::SameLine();

            // Play / Pause toggle
            if (current_mode == Scrubber::ScrubberMode::PLAYING)
            {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.2f, 0.2f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8f, 0.3f, 0.3f, 1.0f));
                if (ImGui::Button("Pause", ImVec2(button_w * 1.5f, button_h)))
                    parameter_store->add("scrubber.mode", Scrubber::ScrubberMode::PAUSED);
                ImGui::SetItemTooltip("Pause playback");
                ImGui::PopStyleColor(2);
            }
            else
            {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.2f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.7f, 0.3f, 1.0f));
                if (ImGui::Button("Play", ImVec2(button_w * 1.5f, button_h)))
                    parameter_store->add("scrubber.mode", Scrubber::ScrubberMode::PLAYING);
                ImGui::SetItemTooltip("Start playback");
                ImGui::PopStyleColor(2);
            }

            ImGui::SameLine();

            // >> Step forward
            if (step_zero)
                ImGui::BeginDisabled();
            if (ImGui::Button(">>", ImVec2(button_w, button_h)))
            {
                if (scrubber_type == Scrubber::ScrubberType::EVENT)
                {
                    size_t cur = parameter_store->get<std::size_t>("scrubber.current_index");
                    size_t step = parameter_store->get<std::size_t>("scrubber.index_step");
                    size_t max_idx = parameter_store->get<std::size_t>("scrubber.max_index");
                    cur = std::min(cur + step, max_idx);
                    parameter_store->add("scrubber.current_index", cur);
                }
                else
                {
                    float cur = parameter_store->get<float>("scrubber.current_time");
                    float step = parameter_store->get<float>("scrubber.time_step");
                    float max_t = parameter_store->get<float>("scrubber.max_time");
                    cur = std::min(cur + step, max_t);
                    parameter_store->add("scrubber.current_time", cur);
                }
                parameter_store->add("scrubber.mode", Scrubber::ScrubberMode::PAUSED);
            }
            ImGui::SetItemTooltip("Step forward");
            if (step_zero)
                ImGui::EndDisabled();

            ImGui::SameLine();

            // >| or LIVE
            if (current_mode == Scrubber::ScrubberMode::LATEST)
            {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.1f, 0.8f, 0.1f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.2f, 0.9f, 0.2f, 1.0f));
                if (ImGui::Button("LIVE", ImVec2(button_w * 1.5f, button_h)))
                    parameter_store->add("scrubber.mode", Scrubber::ScrubberMode::PAUSED);
                ImGui::SetItemTooltip("Currently tracking latest data. Click to pause.");
                ImGui::PopStyleColor(2);
            }
            else
            {
                if (ImGui::Button(">|", ImVec2(button_w, button_h)))
                {
                    if (scrubber_type == Scrubber::ScrubberType::EVENT)
                    {
                        if (scrubber)
                        {
                            size_t max_idx = parameter_store->get<std::size_t>("scrubber.max_index");
                            parameter_store->add("scrubber.current_index", max_idx);
                        }
                    }
                    else
                    {
                        if (scrubber)
                        {
                            float max_time = parameter_store->get<float>("scrubber.max_time");
                            parameter_store->add("scrubber.current_time", max_time);
                        }
                    }
                    parameter_store->add("scrubber.mode", Scrubber::ScrubberMode::LATEST);
                }
                ImGui::SetItemTooltip("Jump to end / track latest");
            }
        }

        /**
         * @brief Draws scrubber window with controls for scrubbing through event data.
         */
        void draw_scrubber_window()
        {
            ImGui::Begin("Scrubber");

            Scrubber::ScrubberType scrubber_type = parameter_store->get<Scrubber::ScrubberType>("scrubber.type");
            Scrubber::ScrubberMode scrubber_mode = parameter_store->get<Scrubber::ScrubberMode>("scrubber.mode");

            // Section 1 — Type selection via tab bar
            Scrubber::ScrubberType prev_type = scrubber_type;
            if (ImGui::BeginTabBar("##ScrubberTypeTabs"))
            {
                if (ImGui::BeginTabItem("Time"))
                {
                    scrubber_type = Scrubber::ScrubberType::TIME;
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Event"))
                {
                    scrubber_type = Scrubber::ScrubberType::EVENT;
                    ImGui::EndTabItem();
                }
                ImGui::EndTabBar();
            }
            if (scrubber_type != prev_type)
            {
                parameter_store->add("scrubber.type", scrubber_type);
                parameter_store->add("scrubber.mode", Scrubber::ScrubberMode::PAUSED);
            }

            ImGui::Separator();

            // Cap mode (for div factor calculations)
            if (!parameter_store->exists("scrubber.cap_mode"))
                parameter_store->add("scrubber.cap_mode", 0);
            int cap_mode_int = parameter_store->get<int>("scrubber.cap_mode");
            int window_div_factor = (cap_mode_int != 0) ? 2 : 100;
            int step_div_factor = (cap_mode_int != 0) ? 10 : 100;

            // Section 2-4 — Timeline, playback, settings
            if (scrubber_type == Scrubber::ScrubberType::EVENT)
            {
                // Initialize defaults
                size_t current_index = parameter_store->get<std::size_t>("scrubber.current_index");
                size_t min_index = 0, max_index = 0;
                if (scrubber)
                {
                    min_index = parameter_store->get<std::size_t>("scrubber.min_index");
                    max_index = parameter_store->get<std::size_t>("scrubber.max_index");
                }
                if (!parameter_store->exists("scrubber.index_window"))
                    parameter_store->add("scrubber.index_window", static_cast<std::size_t>(50));
                if (!parameter_store->exists("scrubber.index_step"))
                    parameter_store->add("scrubber.index_step", static_cast<std::size_t>(0));

                float min_f = static_cast<float>(min_index);
                float max_f = static_cast<float>(max_index);
                float cur_f = static_cast<float>(current_index);
                float win_f = static_cast<float>(parameter_store->get<std::size_t>("scrubber.index_window"));

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
                        if (scrubber_mode == Scrubber::ScrubberMode::LATEST)
                            parameter_store->add("scrubber.mode", Scrubber::ScrubberMode::PAUSED);
                        new_val = std::clamp(new_val, min_f, max_f);
                        parameter_store->add("scrubber.current_index", static_cast<std::size_t>(new_val));
                    }

                    ImGui::Separator();

                    // Playback controls
                    scrubber_mode = parameter_store->get<Scrubber::ScrubberMode>("scrubber.mode");
                    draw_playback_controls(scrubber_mode, scrubber_type);

                    ImGui::Separator();

                    // Window slider (always visible)
                    size_t data_size = max_index - min_index + 1;
                    size_t max_window = std::max(static_cast<size_t>(1), data_size / window_div_factor);
                    float max_window_f = static_cast<float>(max_window);
                    float win_slider = win_f;
                    if (ImGui::SliderFloat("Window", &win_slider, 1.0f, max_window_f, "%.0f"))
                    {
                        win_slider = std::clamp(win_slider, 1.0f, max_window_f);
                        parameter_store->add("scrubber.index_window", static_cast<std::size_t>(win_slider));
                    }
                    ImGui::SetItemTooltip("Number of events behind position to display");

                    // Step slider
                    {
                        size_t step = parameter_store->get<std::size_t>("scrubber.index_step");
                        size_t max_step = (max_index - min_index) / step_div_factor;
                        float max_step_f = static_cast<float>(max_step);
                        float step_f = static_cast<float>(step);
                        if (ImGui::SliderFloat("Step", &step_f, 0.0f, max_step_f, "%.0f"))
                        {
                            if (max_step_f >= 0.0f)
                            {
                                step_f = std::clamp(step_f, 0.0f, max_step_f);
                                parameter_store->add("scrubber.index_step", static_cast<size_t>(step_f));
                            }
                        }
                        ImGui::SetItemTooltip("Events to advance per frame during playback");
                    }
                }
            }
            else // TIME
            {
                // Get time unit information
                uint8_t unit_type = parameter_store->get<uint8_t>("unit_type");
                std::string time_unit_suffix = time_units[unit_type];

                std::string time_format_str{};
                switch (static_cast<TIME>(unit_type))
                {
                case TIME::UNIT_US:
                    time_format_str = "%.2f";
                    break;
                case TIME::UNIT_MS:
                    time_format_str = "%.4f";
                    break;
                case TIME::UNIT_S:
                    time_format_str = "%.8f";
                    break;
                }

                // Initialize defaults
                if (!parameter_store->exists("scrubber.current_time"))
                    parameter_store->add("scrubber.current_time", 0.0f);
                float current_time = parameter_store->get<float>("scrubber.current_time");

                if (!parameter_store->exists("unit_time_conversion_factor"))
                    parameter_store->add("unit_time_conversion_factor", 1.0f);
                float unit_conv = parameter_store->get<float>("unit_time_conversion_factor");
                float current_time_adj = current_time / unit_conv;

                float min_time = 0.0f, max_time = 0.0f;
                if (scrubber)
                {
                    min_time = parameter_store->get<float>("scrubber.min_time");
                    max_time = parameter_store->get<float>("scrubber.max_time");
                }
                float min_adj = min_time / unit_conv;
                float max_adj = max_time / unit_conv;

                if (!parameter_store->exists("scrubber.time_window"))
                    parameter_store->add("scrubber.time_window", 10000.0f);
                float time_window = parameter_store->get<float>("scrubber.time_window");
                float time_window_adj = time_window / unit_conv;

                if (!parameter_store->exists("scrubber.time_step"))
                    parameter_store->add("scrubber.time_step", 33333.0f);

                if (max_adj <= min_adj)
                {
                    ImGui::TextDisabled("No data loaded");
                }
                else
                {
                    // Timeline
                    float new_val = current_time_adj;
                    if (draw_timeline_bar(min_adj, max_adj, current_time_adj, time_window_adj, &new_val,
                                          time_format_str.c_str(), time_unit_suffix.c_str(), true))
                    {
                        if (scrubber_mode == Scrubber::ScrubberMode::LATEST)
                            parameter_store->add("scrubber.mode", Scrubber::ScrubberMode::PAUSED);
                        if (max_adj > min_adj)
                        {
                            new_val = std::clamp(new_val, min_adj, max_adj);
                            parameter_store->add("scrubber.current_time", new_val * unit_conv);
                        }
                    }

                    ImGui::Separator();

                    // Playback controls
                    scrubber_mode = parameter_store->get<Scrubber::ScrubberMode>("scrubber.mode");
                    draw_playback_controls(scrubber_mode, scrubber_type);

                    ImGui::Separator();

                    // Window slider (always visible)
                    float max_window_time = std::max(0.00001f, (max_time - min_time) / window_div_factor);
                    float max_window_adj = max_window_time / unit_conv;
                    if (ImGui::SliderFloat("Window", &time_window_adj, 0.00001f, max_window_adj,
                                           time_format_str.c_str()))
                    {
                        if (max_window_adj > 0.00001f)
                        {
                            time_window_adj = std::clamp(time_window_adj, 0.00001f, max_window_adj);
                            parameter_store->add("scrubber.time_window", time_window_adj * unit_conv);
                        }
                    }
                    ImGui::SetItemTooltip("Time window behind position to display");

                    // Step slider
                    {
                        float time_step = parameter_store->get<float>("scrubber.time_step");
                        float time_step_adj = time_step / unit_conv;
                        float max_step = (max_time - min_time) / step_div_factor;
                        float max_step_adj = max_step / unit_conv;
                        if (ImGui::SliderFloat("Step", &time_step_adj, 0.00001f, max_step_adj,
                                               time_format_str.c_str()))
                        {
                            if (max_step_adj > 0.00001f)
                            {
                                time_step_adj = std::clamp(time_step_adj, 0.00001f, max_step_adj);
                                parameter_store->add("scrubber.time_step", time_step_adj * unit_conv);
                            }
                        }
                        ImGui::SetItemTooltip("Time to advance per frame during playback");
                    }
                }
            }

            ImGui::Separator();

            // Cap mode — radio buttons
            int cap = parameter_store->get<int>("scrubber.cap_mode");
            if (ImGui::RadioButton("Capped", cap == 0))
                parameter_store->add("scrubber.cap_mode", 0);
            ImGui::SetItemTooltip("Limit slider ranges for finer control");
            ImGui::SameLine();
            if (ImGui::RadioButton("Uncapped", cap != 0))
                parameter_store->add("scrubber.cap_mode", 1);
            ImGui::SetItemTooltip("Full slider ranges");

            // Show Frame Data checkbox
            if (!parameter_store->exists("scrubber.show_frame_data"))
                parameter_store->add("scrubber.show_frame_data", false);
            bool show_frame_data = parameter_store->get<bool>("scrubber.show_frame_data");
            ImGui::Checkbox("Show Frame Data", &show_frame_data);
            parameter_store->add("scrubber.show_frame_data", show_frame_data);

            ImGui::End();
        }

        /**
         * @brief Draws Digital Coded Exposure window.
         */
        void draw_digital_coded_exposure()
        {
            ImGui::Begin("Frame");
            ImGui::Text("Digital Coded Exposure");

            if (render_targets.count("DigitalCodedExposure"))
            {
                SDL_GPUTexture *texture = render_targets.at("DigitalCodedExposure").texture;
                if (texture)
                {
                    // Get the available pane size
                    ImVec2 pane_size = ImGui::GetContentRegionAvail();

                    // Get texture dimensions to calculate aspect ratio
                    Uint32 tex_w, tex_h;
                    float tex_aspect = (float)render_targets.at("DigitalCodedExposure").width /
                                       (float)render_targets.at("DigitalCodedExposure").height;

                    // Calculate display size to fit the pane while maintaining aspect ratio
                    ImVec2 display_size = pane_size;
                    float pane_aspect = pane_size.x / pane_size.y;

                    if (tex_aspect > pane_aspect)
                    {
                        // Texture is wider than pane, fit to width
                        display_size.y = pane_size.x / tex_aspect;
                    }
                    else
                    {
                        // Texture is taller than pane (or same aspect), fit to height
                        display_size.x = pane_size.y * tex_aspect;
                    }

                    // Center the image within the pane
                    float x_pad = (pane_size.x - display_size.x) * 0.5f;
                    float y_pad = (pane_size.y - display_size.y) * 0.5f;
                    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + x_pad);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + y_pad);

                    // Display the image. ImTextureID is typedef'd to SDL_GPUTexture*
                    ImGui::Image((ImTextureID)texture, display_size);

                    // Check if the item (image) we just rendered is hovered
                    render_targets.at("DigitalCodedExposure").is_focused = ImGui::IsItemHovered();
                }
                else
                {
                    ImGui::Text("No Event Data."); // Null texture should indicate no event data
                }
            }
            else
            {
                ImGui::Text("Render target 'DigitalCodedExposure' not found.");
            }
            ImGui::End();
        }

        /**
         * @brief For testing, draws spinning cube.
         */
        void draw_spinning_cube_viewport()
        {
            ImGui::Begin("Spinning Cube Viewport");

            // Check if the render target map and the specific target exist
            if (render_targets.count("SpinningCubeColor"))
            {
                SDL_GPUTexture *texture = render_targets.at("SpinningCubeColor").texture;
                if (texture)
                {
                    // Get the available pane size
                    ImVec2 pane_size = ImGui::GetContentRegionAvail();

                    // Get texture dimensions to calculate aspect ratio
                    Uint32 tex_w, tex_h;
                    float tex_aspect = (float)render_targets.at("SpinningCubeColor").width /
                                       (float)render_targets.at("SpinningCubeColor").height;

                    // Calculate display size to fit the pane while maintaining aspect ratio
                    ImVec2 display_size = pane_size;
                    float pane_aspect = pane_size.x / pane_size.y;

                    if (tex_aspect > pane_aspect)
                    {
                        // Texture is wider than pane, fit to width
                        display_size.y = pane_size.x / tex_aspect;
                    }
                    else
                    {
                        // Texture is taller than pane (or same aspect), fit to height
                        display_size.x = pane_size.y * tex_aspect;
                    }

                    // Center the image within the pane
                    float x_pad = (pane_size.x - display_size.x) * 0.5f;
                    float y_pad = (pane_size.y - display_size.y) * 0.5f;
                    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + x_pad);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + y_pad);

                    // Display the image. ImTextureID is typedef'd to SDL_GPUTexture*
                    ImGui::Image((ImTextureID)texture, display_size);

                    // Check if the item (image) we just rendered is hovered
                    render_targets.at("SpinningCubeColor").is_focused = ImGui::IsItemHovered();
                }
                else
                {
                    ImGui::Text("Texture for 'SpinningCubeColor' is null.");
                }
            }
            else
            {
                ImGui::Text("Render target 'SpinningCubeColor' not found.");
            }
            ImGui::End();
        }

        /**
         * @brief Markdown render of quickstart guide for users to reference.
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

                ImGui::TextColored(
                    ImVec4(1.0f, 1.0f, 1.0f, 1.0f),
                    "You can view this popup again by clicking the 'Quickstart Guide' button in the debug window.");
                ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f),
                                   "Windows can be moved and resized, you can reset the layout to the default by "
                                   "clicking the 'Reset Layout' button in the debug window.");
                ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f),
                                   "Sliders can be ctrl+clicked to enter a value directly.");
                ImGui::Separator();

                ImGui::TextColored(ImVec4(0.0f, 1.0f, 1.0f, 1.0f), "Streaming Data");
                ImGui::Separator();
                ImGui::TextWrapped(
                    "Users can stream data from the Streaming window (located in the top right by default). "
                    "To stream from the camera, users can click the 'Scan For Cameras' button to populate the Camera "
                    "dropdown. "
                    "From the Camera dropdown, users can select the desired, detected camera to stream from. "
                    "Once the camera is selected, users click the 'Stream From Camera' button to start the streaming. "
                    "To stream from a file, users can click the 'Open File To Stream' button to select an aedat4 file "
                    "to stream from. "
                    "Streaming from the file will begin as soon as a file is selected. "
                    "The Event Discard Odds determines the odds that event data is randomly discarded, this setting is "
                    "useful when streaming from a camera. "
                    "Users can click the 'Open File To Save Stream To' to select/create an aedat4 file to stream data "
                    "to. "
                    "Users can select the 'Save Frames on Next Stream' and/or 'Save Events On Next Stream' checkboxes "
                    "to save frame and/or event data to the save file. "
                    "Selecting any of the these options will stop streaming. "
                    "To start saving, start streaming from a file or camera with these save options set. ");
                ImGui::Spacing();

                ImGui::TextColored(ImVec4(0.0f, 1.0f, 1.0f, 1.0f), "3D Visualizer");
                ImGui::Separator();
                ImGui::TextWrapped("The 3D Visualizer is a point particle plot. "
                                   "Each point in the plot represents event data. "
                                   "The colors used to represent event polarity for each particle as well as particle "
                                   "scales can be changed in the Info window. "
                                   "The axis with text is the time axis. "
                                   "The other bottom axis is the x-pixel dimension of the event data. "
                                   "The vertical axis is the y-pixel dimension of the event data. "
                                   "Frame data will be shown should the 'Show Frame Data' checkbox be selected in the "
                                   "Scrubber window and should there be frame data received. ");
                ImGui::Spacing();

                ImGui::TextColored(ImVec4(0.0f, 1.0f, 1.0f, 1.0f), "Digital Coded Exposure");
                ImGui::Separator();
                ImGui::TextWrapped("The Digital Coded Exposure attempts to reconstruct frame data out of event data. "
                                   "The controls are given in the Digital Coded Exposure Controls window. "
                                   "There, the user can select the color scheme, "
                                   "enable Morlet shutter contribution calculations, "
                                   "choose the activation function (how each pixel's color is determined from event "
                                   "contributions), etc. "
                                   "It should be noted that due to limitations in Vulkan shaders (specifically, the "
                                   "inability to atomically add floating point numbers), "
                                   "the Morlet shutter will not work for high Current Index (Time) slider values in "
                                   "the Scrubber window. "
                                   "To see Morlet Shutter output, a smaller data file with with high Morlet Frequency "
                                   "and Morlet Width values is recommended. ");
                ImGui::Spacing();

                ImGui::TextColored(ImVec4(0.0f, 1.0f, 1.0f, 1.0f), "Scrubbing Data");
                ImGui::Separator();
                ImGui::TextWrapped(
                    "Users can determine what data is shown in the Digital Coded Exposure and 3D Visualizer windows by "
                    "using the Scrubber window. "
                    "The 'Scrubber Type' dropdown determines what the controls are based off of (event based or time "
                    "based). "
                    "The 'Mode' dropdown provides three ways to view data: "
                    "'Paused' allows the user to scrub through past data, "
                    "'Playing' allows the user to play through data (controlled by the Index (Time) Step) slider, "
                    "'Latest' fixes the Current Index (Time) to the latest received data (very useful when streaming "
                    "from a camera). "
                    "The 'Scrubber Cap' dropdown puts a cap on the sliders by default to increase the precision of the "
                    "slider controls."
                    "The Current Index (Time) determines the last event point being shown in the visualizations. "
                    "The Index (Time) Window determines the number of events before the Current Index (Time) that are "
                    "shown in the visualizations. "
                    "For the Digital Coded Exposure, the Index (Time) Window is basically the shutter length. "
                    "The Index (Time) Step determines the increment to the Current Index (Time) for each frame should "
                    "the Playing Mode be selected. ");

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
         * @brief Represents time units used by NOVA.
         */
        enum class TIME : uint8_t
        {
            UNIT_S = 0,  // seconds
            UNIT_MS = 1, // milliseconds
            UNIT_US = 2  // microseconds
        };

        /**
         * @brief Two types of shutter (time or event based)
         */
        enum class SHUTTER
        {
            TIME_BASED = 0,
            EVENT_BASED = 1
        };

        /**
         * @brief State the program is in.
         */
        enum class PROGRAM_STATE : uint8_t
        {
            IDLE = 0,         // Program is doing nothing
            FILE_STREAM = 2,  // Program is streaming from a file
            CAMERA_STREAM = 3 // Program is streaming from a camera
        };

        /**
         * @brief Constructor for GUI.
         * @param render_targets Render targets of the program
         * @param parameter_store ParameterStore object containing data from GUI
         * @param window SDL_Window to draw on
         * @param gpu_device SDL_GPUDevice to create texture on
         * @param scrubber Scrubber object with data to compute DCE on
         */
        GUI(std::unordered_map<std::string, RenderTarget> &render_targets, ParameterStore *parameter_store,
            SDL_Window *window, SDL_GPUDevice *gpu_device, Scrubber *scrubber)
            : render_targets(render_targets), parameter_store(parameter_store), window(window), gpu_device(gpu_device),
              scrubber(scrubber), fps_history_buf(100, 0.0f), fps_buf_index(0)
        {
            // Setup Dear ImGui context
            IMGUI_CHECKVERSION();
            ImGui::CreateContext();
            ImGuiIO &io = ImGui::GetIO();
            (void)io;
            io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
            io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
            // io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
            io.ConfigWindowsMoveFromTitleBarOnly = true;
            void *font_memory = malloc(sizeof CascadiaCode_ttf);
            std::memcpy(font_memory, CascadiaCode_ttf, sizeof CascadiaCode_ttf);
            io.Fonts->AddFontFromMemoryTTF(font_memory, sizeof CascadiaCode_ttf, 16.0f);

            check_for_layout_file = true;
            show_quickstart = false;

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
            // Cleanup ImGui
            ImGui_ImplSDL3_Shutdown();
            ImGui_ImplSDLGPU3_Shutdown();
            ImGui::DestroyContext();
        }

        /**
         * @brief IMGUI event handler.
         * @param event SDL event to process.
         */
        void event_handler(SDL_Event *event)
        {
            ImGui_ImplSDL3_ProcessEvent(event);
        }

        /**
         * @brief Prepares to renders the GUI in current frame.
         *        This is mandatory: call ImGui_ImplSDLGPU3_PrepareDrawData() to upload the vertex/index buffer!
         * @param command_buffer GPU command buffer.
         * @param fps Calculated fps in current frame.
         */
        void prepare_to_render(SDL_GPUCommandBuffer *command_buffer, float fps)
        {

            // Start the Dear ImGui frame
            ImGui_ImplSDLGPU3_NewFrame();
            ImGui_ImplSDL3_NewFrame();
            ImGui::NewFrame();
            // ImGui::DockSpaceOverViewport();
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

            // draw error popup
            draw_error_popup_window();

            // Draw info block
            draw_info_window();

            // Draw debug block
            draw_debug_window(fps);
            draw_digital_coded_exposure();
            draw_stream_window();
            draw_scrubber_window();
            draw_visualizer();

            // Show quickstart popup (if enabled)
            draw_quickstart_window();

            // Rendering
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
            // multi viewport disabled while weird bug is being investigated
            // // Update and Render additional Platform Windows
            // ImGuiIO &io = ImGui::GetIO();
            // if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
            // {
            //     ImGui::UpdatePlatformWindows();
            //     ImGui::RenderPlatformWindowsDefault();
            // }
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

            ImGuiID dock_id_right; // right side for info, DCE controls, debug, load/stream, 3d visualizer windows
            ImGuiID dock_id_main = dockspace_id; // left side for DCE and scrubber
            ImGui::DockBuilderSplitNode(dock_id_main, ImGuiDir_Right, 0.25f, &dock_id_right,
                                        &dock_id_main); // split window 75% left, 25% right

            ImGuiID dock_id_left_bottom; // bottom left for scrubber
            ImGui::DockBuilderSplitNode(dock_id_main, ImGuiDir_Down, 0.25f, &dock_id_left_bottom, &dock_id_main);

            ImGuiID dock_id_right_top = dock_id_right; // top right for info/DCEcontrols, debug/load/stream windows
            ImGuiID dock_id_right_bottom;              // bottom right for 3d visualizer
            ImGui::DockBuilderSplitNode(dock_id_right_top, ImGuiDir_Down, 0.35f, &dock_id_right_bottom,
                                        &dock_id_right_top);

            ImGuiID dock_id_right_top_top = dock_id_right_top; // top for debug/load/stream windows
            ImGuiID dock_id_right_top_bottom;                  // bottom  for info/DCEcontrols,
            ImGui::DockBuilderSplitNode(dock_id_right_top_top, ImGuiDir_Down, 0.45f, &dock_id_right_top_bottom,
                                        &dock_id_right_top_top);

            ImGui::DockBuilderDockWindow("Digital Coded Exposure Controls", dock_id_right_top_bottom);
            ImGui::DockBuilderDockWindow("Info", dock_id_right_top_bottom);
            ImGui::DockBuilderDockWindow("Debug", dock_id_right_top_top);
            ImGui::DockBuilderDockWindow("Load", dock_id_right_top_top);
            ImGui::DockBuilderDockWindow("Streaming", dock_id_right_top_top);
            ImGui::DockBuilderDockWindow("Frame", dock_id_main); // DCE window
            ImGui::DockBuilderDockWindow("3D Visualizer", dock_id_right_bottom);
            ImGui::DockBuilderDockWindow("Scrubber", dock_id_left_bottom);

            ImGui::DockBuilderFinish(dockspace_id);
        }
};

// Callback used with SDL_ShowOpenFileDialog in draw_stream_window
/**
 * @brief Callback function to open file dialog for streaming from file.
 * @param param_store ParameterStore object containing data from GUI.
 * @param data_file_list Chosen file by user.
 * @param filter_unused unused filter.
 */
inline void SDLCALL stream_file_handle_callback(void *param_store, const char *const *data_file_list, int filter_unused)
{
    ParameterStore *param_store_ptr{static_cast<ParameterStore *>(param_store)};
    if (data_file_list)
    {
        if (*data_file_list)
        {
            std::string file_name{*data_file_list};
            param_store_ptr->add("stream_file_name", file_name);
            param_store_ptr->add("stream_file_changed", true);
            param_store_ptr->add("program_state",
                                 GUI::PROGRAM_STATE::FILE_STREAM); // Determines if program is streaming

            param_store_ptr->add("camera_changed", true);
        }
    }
    else
    {
        std::cerr << "Error happened when selecting file or no file was chosen" << std::endl;
    }
}

// Callback used with SDL_ShowSaveFileDialog in draw_stream_window
/**
 * @brief Callback function to open file dialog for selecting file to save data into.
 * @param param_store ParameterStore object containing data from GUI.
 * @param data_file_list Chosen file by user.
 * @param filter_unused unused filter.
 */
inline void SDLCALL save_stream_handle_callback(void *param_store, const char *const *data_file_list, int filter_unused)
{
    ParameterStore *param_store_ptr{static_cast<ParameterStore *>(param_store)};
    if (data_file_list)
    {
        if (*data_file_list)
        {
            std::string file_name{*data_file_list};
            param_store_ptr->add("stream_save_file_name", file_name);
            param_store_ptr->add("program_state",
                                 GUI::PROGRAM_STATE::IDLE); // Stop program to ensure correct initialization
        }
    }
    else
    {
        std::cerr << "Error happened when selecting file or no file was chosen" << std::endl;
    }
}

#endif // GUI_HH
