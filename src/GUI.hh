#pragma once

#ifndef GUI_HH
#define GUI_HH

#include "pch.hh"

#include "imgui_internal.h"

#include "RenderTarget.hh"
#include "Scrubber.hh"
#include "DataAcquisition.hh"
#include "DataWriter.hh"
#include "Visualizer.hh"
#include "DigitalCodedExposure.hh"
#include "ErrorQueue.hh"

#include "fonts/CascadiaCode.ttf.h"

#include <iostream>

// Structure to pass pointers to callbacks (callback functions need pointers to both)
struct CallbackData {
    DataAcquisition* data_acquisition;
    DataWriter* data_writer;
};

// Forward declarations for callbacks
inline void SDLCALL stream_file_handle_callback(void *user_data, const char *const *data_file_list, int filter_unused);
inline void SDLCALL save_stream_handle_callback(void *user_data, const char *const *data_file_list, int filter_unused);

/**
 * @brief This class provides functions to draw the GUI.
 */
class GUI
{
    private:

        // Modules
        std::unordered_map<std::string, RenderTarget> &render_targets;
        DataAcquisition &data_acquisition;
        DataWriter &data_writer;
        Scrubber &scrubber;
        Visualizer &visualizer;
        DigitalCodedExposure &dce;
        ErrorQueue &error_queue;

        // GPU
        SDL_Window *window = nullptr;
        SDL_GPUDevice *gpu_device = nullptr;
        ImDrawData *draw_data = nullptr;

        // Callback data for file dialogs
        CallbackData callback_data;

        static inline const std::string time_units[] = {"(s)", "(ms)", "(us)"};

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
         * @brief Draws the info window and visualizer controls.
         */
        void draw_info_window()
        {
            ImGui::Begin("Info");

            auto params = visualizer.get_parameters();

            ImGui::SliderFloat("Particle Scale", &params.particle_scale, 0.1f, 6.0f);
            ImGui::Separator();

            ImGui::ColorEdit3("Negative Polarity Color", (float *)&params.polarity_neg_color);
            ImGui::ColorEdit3("Positive Polarity Color", (float *)&params.polarity_pos_color);
            ImGui::Separator();

            int32_t unit_type = static_cast<int32_t>(params.unit_type);
            ImGui::Combo("Time Unit", &unit_type, "s\0ms\0us\0");
            params.unit_type = static_cast<Visualizer::TIME>(unit_type);

            const float units[] = {1000000.0f, 1000.0f, 1.0f};
            params.unit_time_conversion_factor = units[unit_type];

            visualizer.set_parameters(params);

            ImGui::End();

            // DCE Controls
            ImGui::Begin("Digital Coded Exposure Controls");

            auto dce_params = dce.get_parameters();

            ImGui::SliderFloat("Event Contribution Weight", &dce_params.event_contrib_weight, 0.0f, 10.0f);
            ImGui::Separator();

            ImGui::Checkbox("Morlet Shutter", &dce_params.shutter_is_morlet);
            ImGui::Checkbox("Positive Events Only", &dce_params.shutter_is_positive_only);
            ImGui::Separator();

            ImGui::Combo("Digital Exposure Color", &dce_params.dce_color, "High/Low\0Tricolor\0Use Visualizer Colors\0");

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

            dce.set_parameters(dce_params);

            ImGui::End();
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
         * @brief Draw Streaming window. Contains controls for streaming data in.
         */
        void draw_stream_window()
        {
            ImGui::Begin("Streaming");

            DataAcquisition::STATE program_state = data_acquisition.get_state();

            // Display program state
            ImGui::Text("Program State:");
            switch (program_state)
            {
            case DataAcquisition::STATE::IDLE:
                ImGui::Text("Program Is Currently Doing Nothing.");
                break;
            case DataAcquisition::STATE::FILE_STREAM:
                ImGui::Text("Program Is Currently Streaming From FILE.");
                break;
            case DataAcquisition::STATE::CAMERA_STREAM:
                ImGui::Text("Program Is Currently Streaming From CAMERA.");
                break;
            }

            ImGui::Separator();

            float event_discard_odds = data_acquisition.get_event_discard_odds();
            ImGui::Text("Event Discard Odds");
            ImGui::SliderFloat("##Frequency Of Discarded Events", &event_discard_odds, 1.0f, 1500.0f, "%f");
            data_acquisition.set_event_discard_odds(event_discard_odds);

            ImGui::Separator();

            // Stream from camera
            ImGui::Text("Stream From Camera:");
            if (ImGui::Button("Scan For Cameras"))
            {
                data_acquisition.discover_cameras();
            }

            int32_t camera_index = data_acquisition.get_camera_index();
            int32_t camera_index_copy = camera_index;

            // Get camera names
            std::vector<const char *> discovered_cameras_char;
            std::vector<std::string> discovered_cameras = data_acquisition.get_scanned_camera_names();
            for (const auto &name : discovered_cameras)
            {
                discovered_cameras_char.push_back(name.c_str());
            }

            ImGui::Combo("Camera", &camera_index, discovered_cameras_char.data(), discovered_cameras_char.size());

            if (camera_index_copy != camera_index)
            {
                data_acquisition.set_camera_index(camera_index);
                data_acquisition.set_camera_stream_paused(true);
                data_acquisition.set_camera_stream_changed(true);
            }

            if (ImGui::Button(program_state == DataAcquisition::STATE::CAMERA_STREAM ? "Stop Streaming" : "Stream From Camera"))
            {
                if (program_state != DataAcquisition::STATE::CAMERA_STREAM)
                {
                    data_acquisition.set_state(DataAcquisition::STATE::CAMERA_STREAM);
                }
                else
                {
                    data_acquisition.set_state(DataAcquisition::STATE::IDLE);
                }
            }

            bool camera_stream_paused = data_acquisition.is_camera_stream_paused();
            if (ImGui::Button(camera_stream_paused ? "Camera Resume" : "Camera Pause"))
            {
                data_acquisition.set_camera_stream_paused(!camera_stream_paused);
            }

            ImGui::Separator();

            // Stream from file
            ImGui::Text("Stream From File:");
            if (ImGui::Button("Open File To Stream"))
            {
                SDL_ShowOpenFileDialog(stream_file_handle_callback, &callback_data, nullptr, nullptr, 0, nullptr, 0);
            }

            bool file_stream_paused = data_acquisition.is_file_stream_paused();
            if (ImGui::Button(file_stream_paused ? "Resume" : "Pause"))
            {
                data_acquisition.set_file_stream_paused(!file_stream_paused);
            }

            ImGui::Separator();

            // Stream save options
            ImGui::Text("Stream Save Options:");

            std::string saving_message = data_writer.get_saving_message();
            ImGui::Text("%s", saving_message.c_str());

            bool stream_save_frames = data_writer.get_save_frames_toggle();
            bool stream_save_frames_copy = stream_save_frames;
            ImGui::Checkbox("Save Frames On Next Stream (Will Stop Streaming)", &stream_save_frames);
            if (stream_save_frames != stream_save_frames_copy)
            {
                data_acquisition.set_state(DataAcquisition::STATE::IDLE);
                data_writer.set_save_frames_toggle(stream_save_frames);
            }

            bool stream_save_events = data_writer.get_save_events_toggle();
            bool stream_save_events_copy = stream_save_events;
            ImGui::Checkbox("Save Events On Next Stream (Will Stop Streaming)", &stream_save_events);
            if (stream_save_events_copy != stream_save_events)
            {
                data_acquisition.set_state(DataAcquisition::STATE::IDLE);
                data_writer.set_save_events_toggle(stream_save_events);
            }

            std::string stream_save_file_name = data_writer.get_stream_save_file_name();

            if ((stream_save_frames || stream_save_events) && !stream_save_file_name.empty())
            {
                std::string will_save_message = "Will Save Streamed ";
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
                SDL_ShowSaveFileDialog(save_stream_handle_callback, &callback_data, nullptr, nullptr, 0, nullptr);
            }

            ImGui::End();
        }

        /**
         * @brief Draws 3D Visualizer window into IMGUI.
         */
        void draw_visualizer()
        {
            ImGui::Begin("3D Visualizer");

            if (render_targets.count("VisualizerColor"))
            {
                SDL_GPUTexture *texture = render_targets.at("VisualizerColor").texture;
                if (texture)
                {
                    ImVec2 pane_size = ImGui::GetContentRegionAvail();

                    float tex_aspect = (float)render_targets.at("VisualizerColor").width /
                                       (float)render_targets.at("VisualizerColor").height;

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

                    ImGui::Image((ImTextureID)texture, display_size);

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

            Scrubber::ScrubberState state = scrubber.get_state();

            int scrubber_type_int = static_cast<int>(state.type);
            const char *scrubber_type_names[] = {"Event", "Time"};
            if (ImGui::Combo("Scrubber Type", &scrubber_type_int, scrubber_type_names, 2))
            {
                state.type = static_cast<Scrubber::ScrubberType>(scrubber_type_int);
                scrubber.set_state(state);
            }

            ImGui::Separator();

            int scrubber_mode_int = static_cast<int>(state.mode);
            const char *scrubber_mode_names[] = {"Paused", "Playing", "Latest"};
            if (ImGui::Combo("Mode", &scrubber_mode_int, scrubber_mode_names, 3))
            {
                state.mode = static_cast<Scrubber::ScrubberMode>(scrubber_mode_int);
                scrubber.set_state(state);
            }

            ImGui::Separator();

            // Scrubber cap mode (currently unused by scrubber, no clue what it's for)
            static int cap_mode_int = 0;
            const char *cap_mode_names[] = {"Capped", "Uncapped"};
            ImGui::Combo("Scrubber Cap", &cap_mode_int, cap_mode_names, 2);

            int window_div_factor = (cap_mode_int == 0) ? 100 : 2;
            int step_div_factor = (cap_mode_int == 0) ? 100 : 10;

            ImGui::Separator();

            // Get visualizer parameters for time unit conversion
            auto vis_params = visualizer.get_parameters();

            if (state.type == Scrubber::ScrubberType::EVENT)
            {
                float current_index_float = static_cast<float>(state.current_index);
                float min_index_float = static_cast<float>(state.min_index);
                float max_index_float = static_cast<float>(state.max_index);

                if (ImGui::SliderFloat("Current Index", &current_index_float, min_index_float, max_index_float))
                {
                    state.current_index = static_cast<std::size_t>(std::clamp(current_index_float, min_index_float, max_index_float));
                    scrubber.set_state(state);
                }

                float index_window_float = static_cast<float>(state.index_window);
                size_t max_window_size = (std::max)(static_cast<size_t>(1), (state.max_index - state.min_index + 1) / window_div_factor);
                float max_window_size_float = static_cast<float>(max_window_size);

                if (ImGui::SliderFloat("Index Window", &index_window_float, 1.0f, max_window_size_float))
                {
                    state.index_window = static_cast<std::size_t>(std::clamp(index_window_float, 1.0f, max_window_size_float));
                    scrubber.set_state(state);
                }

                float event_step_float = static_cast<float>(state.index_step);
                size_t max_step_size = (state.max_index - state.min_index) / step_div_factor;
                float max_step_float = static_cast<float>(max_step_size);

                if (ImGui::SliderFloat("Index Step", &event_step_float, 0.0f, max_step_float))
                {
                    state.index_step = static_cast<size_t>(std::clamp(event_step_float, 0.0f, max_step_float));
                    scrubber.set_state(state);
                }
            }
            else // TIME
            {
                std::string time_unit_suffix = time_units[static_cast<int>(vis_params.unit_type)];
                
                std::string time_format_str;
                switch (vis_params.unit_type)
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

                float current_time_adj = state.current_time / vis_params.unit_time_conversion_factor;
                float min_time_adj = state.min_time / vis_params.unit_time_conversion_factor;
                float max_time_adj = state.max_time / vis_params.unit_time_conversion_factor;

                std::string current_time_label = "Current Time " + time_unit_suffix;
                if (ImGui::SliderFloat(current_time_label.c_str(), &current_time_adj, min_time_adj, max_time_adj, time_format_str.c_str()))
                {
                    if (max_time_adj > min_time_adj)
                    {
                        current_time_adj = std::clamp(current_time_adj, min_time_adj, max_time_adj);
                        state.current_time = current_time_adj * vis_params.unit_time_conversion_factor;
                        scrubber.set_state(state);
                    }
                }

                float time_window_adj = state.time_window / vis_params.unit_time_conversion_factor;
                float max_window_time = (std::max)(0.00001f, (state.max_time - state.min_time) / window_div_factor);
                float max_window_time_adj = max_window_time / vis_params.unit_time_conversion_factor;

                std::string time_window_label = "Time Window " + time_unit_suffix;
                if (ImGui::SliderFloat(time_window_label.c_str(), &time_window_adj, 0.00001f, max_window_time_adj, time_format_str.c_str()))
                {
                    if (max_window_time_adj > 0.00001f)
                    {
                        time_window_adj = std::clamp(time_window_adj, 0.00001f, max_window_time_adj);
                        state.time_window = time_window_adj * vis_params.unit_time_conversion_factor;
                        scrubber.set_state(state);
                    }
                }

                float time_step_adj = state.time_step / vis_params.unit_time_conversion_factor;
                float max_step_time = (state.max_time - state.min_time) / step_div_factor;
                float max_step_time_adj = max_step_time / vis_params.unit_time_conversion_factor;

                std::string time_step_label = "Time Step " + time_unit_suffix;
                if (ImGui::SliderFloat(time_step_label.c_str(), &time_step_adj, 0.00001f, max_step_time_adj, time_format_str.c_str()))
                {
                    if (max_step_time_adj > 0.00001f)
                    {
                        time_step_adj = std::clamp(time_step_adj, 0.00001f, max_step_time_adj);
                        state.time_step = time_step_adj * vis_params.unit_time_conversion_factor;
                        scrubber.set_state(state);
                    }
                }
            }

            ImGui::Checkbox("Show Frame Data", &state.show_frame_data);
            scrubber.set_state(state);

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
                    ImVec2 pane_size = ImGui::GetContentRegionAvail();
                    float tex_aspect = (float)render_targets.at("DigitalCodedExposure").width /
                                       (float)render_targets.at("DigitalCodedExposure").height;

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

                    ImGui::Image((ImTextureID)texture, display_size);
                    render_targets.at("DigitalCodedExposure").is_focused = ImGui::IsItemHovered();
                }
                else
                {
                    ImGui::Text("No Event Data.");
                }
            }
            else
            {
                ImGui::Text("Render target 'DigitalCodedExposure' not found.");
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

                ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f),
                    "You can view this popup again by clicking the 'Quickstart Guide' button in the debug window.");
                ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f),
                    "Windows can be moved and resized, you can reset the layout to the default by clicking the 'Reset Layout' button in the debug window.");
                ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f),
                    "Sliders can be ctrl+clicked to enter a value directly.");
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
                    "To start saving, start streaming from a file or camera with these save options set.");
                ImGui::Spacing();

                ImGui::TextColored(ImVec4(0.0f, 1.0f, 1.0f, 1.0f), "3D Visualizer");
                ImGui::Separator();
                ImGui::TextWrapped(
                    "The 3D Visualizer is a point particle plot. Each point in the plot represents event data. "
                    "The colors used to represent event polarity for each particle as well as particle scales can be changed in the Info window. "
                    "The axis with text is the time axis. The other bottom axis is the x-pixel dimension of the event data. "
                    "The vertical axis is the y-pixel dimension of the event data. "
                    "Frame data will be shown should the 'Show Frame Data' checkbox be selected in the Scrubber window and should there be frame data received.");
                ImGui::Spacing();

                ImGui::TextColored(ImVec4(0.0f, 1.0f, 1.0f, 1.0f), "Digital Coded Exposure");
                ImGui::Separator();
                ImGui::TextWrapped(
                    "The Digital Coded Exposure attempts to reconstruct frame data out of event data. "
                    "The controls are given in the Digital Coded Exposure Controls window. "
                    "There, the user can select the color scheme, enable Morlet shutter contribution calculations, "
                    "choose the activation function (how each pixel's color is determined from event contributions), etc. "
                    "It should be noted that due to limitations in Vulkan shaders (specifically, the inability to atomically add floating point numbers), "
                    "the Morlet shutter will not work for high Current Index (Time) slider values in the Scrubber window. "
                    "To see Morlet Shutter output, a smaller data file with with high Morlet Frequency and Morlet Width values is recommended.");
                ImGui::Spacing();

                ImGui::TextColored(ImVec4(0.0f, 1.0f, 1.0f, 1.0f), "Scrubbing Data");
                ImGui::Separator();
                ImGui::TextWrapped(
                    "Users can determine what data is shown in the Digital Coded Exposure and 3D Visualizer windows by using the Scrubber window. "
                    "The 'Scrubber Type' dropdown determines what the controls are based off of (event based or time based). "
                    "The 'Mode' dropdown provides three ways to view data: 'Paused' allows the user to scrub through past data, "
                    "'Playing' allows the user to play through data (controlled by the Index (Time) Step) slider, "
                    "'Latest' fixes the Current Index (Time) to the latest received data (very useful when streaming from a camera). "
                    "The 'Scrubber Cap' dropdown puts a cap on the sliders by default to increase the precision of the slider controls. "
                    "The Current Index (Time) determines the last event point being shown in the visualizations. "
                    "The Index (Time) Window determines the number of events before the Current Index (Time) that are shown in the visualizations. "
                    "For the Digital Coded Exposure, the Index (Time) Window is basically the shutter length. "
                    "The Index (Time) Step determines the increment to the Current Index (Time) for each frame should the Playing Mode be selected.");

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
        GUI(std::unordered_map<std::string, RenderTarget> &render_targets,
            DataAcquisition &data_acquisition,
            DataWriter &data_writer,
            Scrubber &scrubber,
            Visualizer &visualizer,
            DigitalCodedExposure &dce,
            ErrorQueue &error_queue,
            SDL_Window *window,
            SDL_GPUDevice *gpu_device)
            : render_targets(render_targets),
              data_acquisition(data_acquisition),
              data_writer(data_writer),
              scrubber(scrubber),
              visualizer(visualizer),
              dce(dce),
              error_queue(error_queue),
              window(window),
              gpu_device(gpu_device),
              callback_data{&data_acquisition, &data_writer},
              fps_history_buf(100, 0.0f),
              fps_buf_index(0),
              check_for_layout_file(true),
              show_quickstart(false)
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
            ImGui_ImplSDLGPU3_InitInfo init_info = {
                .Device = gpu_device,
                .ColorTargetFormat = SDL_GetGPUSwapchainTextureFormat(gpu_device, window),
                .MSAASamples = SDL_GPU_SAMPLECOUNT_1,
                .SwapchainComposition = SDL_GPU_SWAPCHAINCOMPOSITION_SDR,
                .PresentMode = SDL_GPU_PRESENTMODE_VSYNC
            };
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
         * @brief IMGUI event handler.
         * @param event SDL event to process.
         */
        void event_handler(SDL_Event *event)
        {
            ImGui_ImplSDL3_ProcessEvent(event);
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
            draw_digital_coded_exposure();
            draw_stream_window();
            draw_scrubber_window();
            draw_visualizer();
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
            ImGui::DockBuilderSplitNode(dock_id_right_top, ImGuiDir_Down, 0.35f, &dock_id_right_bottom, &dock_id_right_top);

            ImGuiID dock_id_right_top_top = dock_id_right_top;
            ImGuiID dock_id_right_top_bottom;
            ImGui::DockBuilderSplitNode(dock_id_right_top_top, ImGuiDir_Down, 0.45f, &dock_id_right_top_bottom, &dock_id_right_top_top);

            ImGui::DockBuilderDockWindow("Digital Coded Exposure Controls", dock_id_right_top_bottom);
            ImGui::DockBuilderDockWindow("Info", dock_id_right_top_bottom);
            ImGui::DockBuilderDockWindow("Debug", dock_id_right_top_top);
            ImGui::DockBuilderDockWindow("Streaming", dock_id_right_top_top);
            ImGui::DockBuilderDockWindow("Frame", dock_id_main);
            ImGui::DockBuilderDockWindow("3D Visualizer", dock_id_right_bottom);
            ImGui::DockBuilderDockWindow("Scrubber", dock_id_left_bottom);

            ImGui::DockBuilderFinish(dockspace_id);
        }
};

// Callback functions
inline void SDLCALL stream_file_handle_callback(void *user_data, const char *const *data_file_list, int filter_unused)
{
    CallbackData* data = static_cast<CallbackData*>(user_data);
    if (data_file_list && *data_file_list)
    {
        std::string file_name{*data_file_list};
        data->data_acquisition->set_file_stream_name(file_name);
        data->data_acquisition->set_file_stream_paused(true);
        data->data_acquisition->set_file_stream_changed(true);
        data->data_acquisition->set_state(DataAcquisition::STATE::FILE_STREAM);
    }
    else
    {
        std::cerr << "Error happened when selecting file or no file was chosen" << std::endl;
    }
}

inline void SDLCALL save_stream_handle_callback(void *user_data, const char *const *data_file_list, int filter_unused)
{
    CallbackData* data = static_cast<CallbackData*>(user_data);
    if (data_file_list && *data_file_list)
    {
        std::string file_name{*data_file_list};
        data->data_writer->set_stream_save_file_name(file_name);
        data->data_acquisition->set_state(DataAcquisition::STATE::IDLE);
    }
    else
    {
        std::cerr << "Error happened when selecting file or no file was chosen" << std::endl;
    }
}

#endif // GUI_HH