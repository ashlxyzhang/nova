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
         * @brief Draws scrubber window with controls for scrubbing through event data.
         */
        void draw_scrubber_window()
        {
            ImGui::Begin("Scrubber");

            int scrubber_type_int = static_cast<int>(parameter_store->get<Scrubber::ScrubberType>("scrubber.type"));
            const char *scrubber_type_names[] = {"Event", "Time"};
            if (ImGui::Combo("Scrubber Type", &scrubber_type_int, scrubber_type_names, 2))
            {
                parameter_store->add("scrubber.type", static_cast<Scrubber::ScrubberType>(scrubber_type_int));
            }

            ImGui::Separator();

            // Scrubber Mode
            int scrubber_mode_int = static_cast<int>(parameter_store->get<Scrubber::ScrubberMode>("scrubber.mode"));
            const char *scrubber_mode_names[] = {"Paused", "Playing", "Latest"};
            if (ImGui::Combo("Mode", &scrubber_mode_int, scrubber_mode_names, 3))
            {
                parameter_store->add("scrubber.mode", static_cast<Scrubber::ScrubberMode>(scrubber_mode_int));
            }

            ImGui::Separator();

            if (!parameter_store->exists("scrubber.cap_mode"))
            {
                parameter_store->add("scrubber.cap_mode", 0);
            }
            int cap_mode_int = parameter_store->get<int>("scrubber.cap_mode");
            const char *cap_mode_names[] = {"Capped", "Uncapped"};
            if (ImGui::Combo("Scrubber Cap", &cap_mode_int, cap_mode_names, 2))
            {
                parameter_store->add("scrubber.cap_mode", cap_mode_int);
            }
            int window_div_factor = 100; // Default to 100 for capped mode
            int step_div_factor = 100;   // Default to 1 for step size
            if (cap_mode_int != 0)       // Uncapped mode
            {
                window_div_factor = 2; // Use 2 for uncapped mode
                step_div_factor = 10;  // Use 10 for uncapped mode
            }

            ImGui::Separator();

            // Current Index (for EVENT type)
            if (parameter_store->get<Scrubber::ScrubberType>("scrubber.type") == Scrubber::ScrubberType::EVENT)
            {
                size_t current_index_size_t = parameter_store->get<std::size_t>("scrubber.current_index");

                // Get min/max values from scrubber if available
                size_t min_index_size_t = 0;
                size_t max_index_size_t = 0;
                if (scrubber)
                {
                    min_index_size_t = parameter_store->get<std::size_t>("scrubber.min_index");
                    max_index_size_t = parameter_store->get<std::size_t>("scrubber.max_index");
                }

                float current_index_float{static_cast<float>(current_index_size_t)};
                float min_index_float{static_cast<float>(min_index_size_t)};
                float max_index_float{static_cast<float>(max_index_size_t)};

                if (ImGui::SliderFloat("Current Index", &current_index_float, min_index_float, max_index_float))
                {
                    if (current_index_float < min_index_float)
                        current_index_float = min_index_float;
                    if (current_index_float > max_index_float)
                        current_index_float = max_index_float;
                    parameter_store->add("scrubber.current_index", static_cast<std::size_t>(current_index_float));
                }

                // Index Window
                if (!parameter_store->exists("scrubber.index_window"))
                {
                    parameter_store->add("scrubber.index_window", static_cast<std::size_t>(50));
                }
                size_t index_window_size_t = parameter_store->get<std::size_t>("scrubber.index_window");

                float index_window_float{static_cast<float>(index_window_size_t)};

                // Calculate maximum window size (1/2 or 1/100 of data size, minimum 1)
                size_t max_window_size = 1;
                if (scrubber)
                {
                    size_t data_size = parameter_store->get<std::size_t>("scrubber.max_index") -
                                       parameter_store->get<std::size_t>("scrubber.min_index") + 1;
                    max_window_size = std::max(static_cast<size_t>(1), data_size / window_div_factor);
                }

                float max_window_size_float{static_cast<float>(max_window_size)};

                if (ImGui::SliderFloat("Index Window", &index_window_float, 1.0f, max_window_size_float))
                {
                    if (index_window_float < 1.0f)
                        index_window_float = 1.0f;
                    if (index_window_float > max_window_size_float)
                        index_window_float = max_window_size_float;
                    parameter_store->add("scrubber.index_window", static_cast<std::size_t>(index_window_float));
                }

                // Time Step
                if (!parameter_store->exists("scrubber.index_step"))
                {
                    size_t default_step{0};
                    parameter_store->add("scrubber.index_step", default_step);
                }
                size_t event_step = parameter_store->get<std::size_t>("scrubber.index_step");

                // Calculate maximum step size (total time range)
                size_t max_step_size_t = (max_index_size_t - min_index_size_t) / step_div_factor;
                float max_step_float{static_cast<float>(max_step_size_t)};

                float event_step_float = static_cast<float>(event_step);
                std::string event_step_label{"Index Step"};
                if (ImGui::SliderFloat(event_step_label.c_str(), &event_step_float, 0.0f, max_step_float))
                {
                    if (max_step_float >= 0.0f)
                    {
                        event_step_float = event_step_float;
                        float lowest_step_float{0.0f};
                        event_step_float = std::clamp(event_step_float, lowest_step_float, max_step_float);
                        parameter_store->add("scrubber.index_step", static_cast<size_t>(event_step_float));
                    }
                }
            }
            // Time-based controls (for TIME type)
            else if (parameter_store->get<Scrubber::ScrubberType>("scrubber.type") == Scrubber::ScrubberType::TIME)
            {
                // Get time unit information
                uint8_t unit_type = parameter_store->get<uint8_t>("unit_type");
                std::string time_unit_suffix = time_units[unit_type];

                // Determine format string based on time unit
                std::string time_format_str{};
                switch (static_cast<TIME>(unit_type))
                {
                case TIME::UNIT_US:
                    time_format_str = std::string{"%.2f"};
                    break;
                case TIME::UNIT_MS:
                    time_format_str = std::string{"%.4f"};
                    break;
                case TIME::UNIT_S:
                    time_format_str = std::string{"%.8f"};
                    break;
                }

                // Current Time
                if (!parameter_store->exists("scrubber.current_time"))
                {
                    parameter_store->add("scrubber.current_time", 0.0f);
                }
                float current_time = parameter_store->get<float>("scrubber.current_time");

                if (!parameter_store->exists("unit_time_conversion_factor"))
                {
                    parameter_store->add("unit_time_conversion_factor", 1.0f); // Assume default unit of microseconds
                }
                float unit_time_conversion_factor{parameter_store->get<float>("unit_time_conversion_factor")};
                float current_time_unit_adjusted = current_time / unit_time_conversion_factor;

                // Get min/max time values from scrubber if available
                float min_time = 0.0f;
                float max_time = 0.0f;
                if (scrubber)
                {
                    min_time = parameter_store->get<float>("scrubber.min_time");
                    max_time = parameter_store->get<float>("scrubber.max_time");
                }

                float min_time_unit_adjusted = min_time / unit_time_conversion_factor;
                float max_time_unit_adjusted = max_time / unit_time_conversion_factor;

                std::string current_time_label = "Current Time " + time_unit_suffix;
                if (ImGui::SliderFloat(current_time_label.c_str(), &current_time_unit_adjusted, min_time_unit_adjusted,
                                       max_time_unit_adjusted, time_format_str.c_str()))
                {
                    // STOP CLAMP FROM CRASHING THE PROGRAM FOR THE NTH TIME
                    if (max_time_unit_adjusted > min_time_unit_adjusted)
                    {
                        current_time_unit_adjusted =
                            std::clamp(current_time_unit_adjusted, min_time_unit_adjusted, max_time_unit_adjusted);
                        current_time = current_time_unit_adjusted *
                                       unit_time_conversion_factor; // Revert conversion to store back into scrubber
                        // Scrubber deals in us time unit
                        parameter_store->add("scrubber.current_time", current_time);
                    }
                }

                // Time Window
                if (!parameter_store->exists("scrubber.time_window"))
                {
                    parameter_store->add("scrubber.time_window", 1.0f);
                }
                float time_window = parameter_store->get<float>("scrubber.time_window");
                float time_window_unit_adjusted = time_window / unit_time_conversion_factor;

                // Calculate maximum window size (1/2 or 1/100 of total time range, minimum 0.001)
                float max_window_time = std::max(0.00001f, (max_time - min_time) / window_div_factor);
                float max_window_time_unit_adjusted = max_window_time / unit_time_conversion_factor;

                std::string time_window_label = "Time Window " + time_unit_suffix;
                if (ImGui::SliderFloat(time_window_label.c_str(), &time_window_unit_adjusted, 0.00001f,
                                       max_window_time_unit_adjusted, time_format_str.c_str()))
                {
                    // STOP CLAMP FROM CRASHING THE PROGRAM FOR THE NTH TIME
                    if (max_window_time_unit_adjusted > 0.00001f)
                    {
                        time_window_unit_adjusted =
                            std::clamp(time_window_unit_adjusted, 0.00001f, max_window_time_unit_adjusted);
                        // Adjust back to us to store into scrubber
                        time_window = time_window_unit_adjusted * unit_time_conversion_factor;
                        parameter_store->add("scrubber.time_window", time_window);
                    }
                }

                // Time Step
                if (!parameter_store->exists("scrubber.time_step"))
                {
                    parameter_store->add("scrubber.time_step", 0.1f);
                }
                float time_step = parameter_store->get<float>("scrubber.time_step");
                float time_step_unit_adjusted = time_step / unit_time_conversion_factor;

                // Calculate maximum step size (total time range)
                float max_step_time = (max_time - min_time) / step_div_factor;
                float max_step_time_unit_adjusted = max_step_time / unit_time_conversion_factor;

                std::string time_step_label = "Time Step " + time_unit_suffix;
                if (ImGui::SliderFloat(time_step_label.c_str(), &time_step_unit_adjusted, 0.00001f,
                                       max_step_time_unit_adjusted, time_format_str.c_str()))
                {
                    // STOP CLAMP FROM CRASHING THE PROGRAM FOR THE NTH TIME
                    if (max_step_time_unit_adjusted > 0.00001f)
                    {
                        time_step_unit_adjusted =
                            std::clamp(time_step_unit_adjusted, 0.00001f, max_step_time_unit_adjusted);
                        // Adjust back to us to store into data scrubber
                        time_step = time_step_unit_adjusted * unit_time_conversion_factor;
                        parameter_store->add("scrubber.time_step", time_step);
                    }
                }
            }

            // Control if frame data shows up with event data
            if (!parameter_store->exists("scrubber.show_frame_data"))
            {
                parameter_store->add("scrubber.show_frame_data", false);
            }
            bool show_frame_data{parameter_store->get<bool>("scrubber.show_frame_data")};
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

            const ImGuiViewport* viewport = ImGui::GetMainViewport();

            ImGui::SetNextWindowPos(
                viewport->GetCenter(),
                ImGuiCond_Appearing,
                ImVec2(0.5f, 0.5f)
            );

            ImVec2 windowSize = ImVec2(viewport->Size.x * 0.75f, viewport->Size.y * 0.75f);
            ImGui::SetNextWindowSize(windowSize, ImGuiCond_Appearing);

            if (ImGui::BeginPopupModal(
                    "Quickstart Guide",
                    &show_quickstart))
            {
                ImGui::BeginChild("QSContent", ImVec2(0, -50), true, ImGuiWindowFlags_HorizontalScrollbar);

                ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), "You can view this popup again by clicking the 'Quickstart Guide' button in the debug window.");
                ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), "Windows can be moved and resized, you can reset the layout to the default by clicking the 'Reset Layout' button in the debug window.");
                ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), "Sliders can be ctrl+clicked to enter a value directly.");
                ImGui::Separator();

                ImGui::TextColored(ImVec4(0.0f, 1.0f, 1.0f, 1.0f), "Streaming Data");
                ImGui::Separator();
                ImGui::TextWrapped(
                    "Users can stream data from the Streaming window (located in the top right by default). "
                    "To stream from the camera, users can click the 'Scan For Cameras' button to populate the Camera dropdown. " 
                    "From the Camera dropdown, users can select the desired, detected camera to stream from. "
                    "Once the camera is selected, users click the 'Stream From Camera' button to start the streaming. "
                    "To stream from a file, users can click the 'Open File To Stream' button to select an aedat4 file to stream from. "
                    "Streaming from the file will begin as soon as a file is selected. "
                    "The Event Discard Odds determines the odds that event data is randomly discarded, this setting is useful when streaming from a camera. "
                    "Users can click the 'Open File To Save Stream To' to select/create an aedat4 file to stream data to. "
                    "Users can select the 'Save Frames on Next Stream' and/or 'Save Events On Next Stream' checkboxes to save frame and/or event data to the save file. "
                    "Selecting any of the these options will stop streaming. "
                    "To start saving, start streaming from a file or camera with these save options set. "
                );
                ImGui::Spacing();

                ImGui::TextColored(ImVec4(0.0f, 1.0f, 1.0f, 1.0f), "3D Visualizer");
                ImGui::Separator();
                ImGui::TextWrapped(
                    "The 3D Visualizer is a point particle plot. "
                    "Each point in the plot represents event data. "
                    "The colors used to represent event polarity for each particle as well as particle scales can be changed in the Info window. "
                    "The axis with text is the time axis. "
                    "The other bottom axis is the x-pixel dimension of the event data. "
                    "The vertical axis is the y-pixel dimension of the event data. "
                    "Frame data will be shown should the 'Show Frame Data' checkbox be selected in the Scrubber window and should there be frame data received. "
                );
                ImGui::Spacing();

                ImGui::TextColored(ImVec4(0.0f, 1.0f, 1.0f, 1.0f), "Digital Coded Exposure");
                ImGui::Separator();
                ImGui::TextWrapped(
                    "The Digital Coded Exposure attempts to reconstruct frame data out of event data. "
                    "The controls are given in the Digital Coded Exposure Controls window. "
                    "There, the user can select the color scheme, "
                    "enable Morlet shutter contribution calculations, "
                    "choose the activation function (how each pixel's color is determined from event contributions), etc. "
                    "It should be noted that due to limitations in Vulkan shaders (specifically, the inability to atomically add floating point numbers), "
                    "the Morlet shutter will not work for high Current Index (Time) slider values in the Scrubber window. "
                    "To see Morlet Shutter output, a smaller data file with with high Morlet Frequency and Morlet Width values is recommended. "
                );
                ImGui::Spacing();

                ImGui::TextColored(ImVec4(0.0f, 1.0f, 1.0f, 1.0f), "Scrubbing Data");
                ImGui::Separator();
                ImGui::TextWrapped(
                    "Users can determine what data is shown in the Digital Coded Exposure and 3D Visualizer windows by using the Scrubber window. "
                    "The 'Scrubber Type' dropdown determines what the controls are based off of (event based or time based). "
                    "The 'Mode' dropdown provides three ways to view data: "
                    "'Paused' allows the user to scrub through past data, "
                    "'Playing' allows the user to play through data (controlled by the Index (Time) Step) slider, "
                    "Latest' fixes the Current Index (Time) to the latest received data (very useful when streaming from a camera). "
                    "The 'Scrubber Cap' dropdown puts a cap on the sliders by default to increase the precision of the slider controls."
                    "The Current Index (Time) determines the last event point being shown in the visualizations. "
                    "The Index (Time) Window determines the number of events before the Current Index (Time) that are shown in the visualizations. "
                    "For the Digital Coded Exposure, the Index (Time) Window is basically the shutter length. "
                    "The Index (Time) Step determines the increment to the Current Index (Time) for each frame should the Playing Mode be selected. "
                );

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
