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

// Callback used with SDL_ShowOpenFileDialog in draw_load_file_window
inline void SDLCALL load_file_handle_callback(void *param_store, const char *const *data_file_list, int filter_unused);

// Callback used with SDL_ShowOpenFileDialog in draw_stream_window
inline void SDLCALL stream_file_handle_callback(void *param_store, const char *const *data_file_list,
                                                int filter_unused);

// Callback used with SDL_ShowSaveFileDialog in draw_stream_window
inline void SDLCALL save_stream_handle_callback(void *param_store, const char *const *data_file_list,
                                                int filter_unused);

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

        std::vector<float> fps_history_buf;
        size_t fps_buf_index;

        bool check_for_layout_file;

        // Update circular buffer of fps data
        // From old NOVA source code
        void update_fps_buffer(const float &fps)
        {
            fps_history_buf[fps_buf_index] = fps;
            fps_buf_index = (fps_buf_index + 1) % fps_history_buf.size();
        }

        // Get average fps
        // From old NOVA source code
        float get_avg_fps()
        {
            float sum = 0.0f;
            for (const float &fps : fps_history_buf)
            {
                sum += fps;
            }
            return sum / fps_history_buf.size();
        }

        // Get max fps
        // From old NOVA source code
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

        // Get min fps
        // From old NOVA source code
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

        // Draw error popup window()
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

        // Recreate info window from old NOVA
        // Also currently draws DCE controls for local variables' sake
        void draw_info_window()
        {
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

            // ImGui::Separator();

            ImGui::End();

            // ImGui::Text("Processing options");

            // if (!parameter_store->exists("shutter_type"))
            // {
            //     parameter_store->add("shutter_type", 0);
            // }
            // int32_t shutter_type{parameter_store->get<int32_t>("shutter_type")};
            // ImGui::Combo("Shutter", &shutter_type, "Time Based\0Event Based\0");
            // parameter_store->add("shutter_type", shutter_type);

            // if (static_cast<GUI::SHUTTER>(parameter_store->get<int32_t>("shutter_type")) == GUI::SHUTTER::TIME_BASED)
            // {

            //     if (parameter_store->exists("time_window_begin") && parameter_store->exists("time_window_end"))
            //     {
            //         float time_window_end{parameter_store->get<float>("time_window_end")};
            //         float time_window_begin{parameter_store->get<float>("time_window_begin")};
            //         float frameLength_T = time_window_end - time_window_begin;

            //         // Get user input for beginning of time shutter window
            //         if (!parameter_store->exists("time_shutter_window_begin"))
            //         {
            //             parameter_store->add("time_shutter_window_begin", time_window_begin);
            //         }
            //         float time_shutter_window_begin{parameter_store->get<float>("time_shutter_window_begin")};
            //         std::string shutter_initial{"Shutter Initial "};
            //         shutter_initial.append(time_units[parameter_store->get<int32_t>("unit_type")]);
            //         ImGui::SliderFloat(shutter_initial.c_str(), &time_shutter_window_begin, 0, frameLength_T,
            //         "%.4f");

            //         // Get user input for ending of time shutter window
            //         if (!parameter_store->exists("time_shutter_window_end"))
            //         {
            //             parameter_store->add("time_shutter_window_end", time_window_end);
            //         }
            //         float time_shutter_window_end{parameter_store->get<float>("time_shutter_window_end")};
            //         std::string shutter_final{"Shutter Final "};
            //         shutter_final.append(time_units[parameter_store->get<int32_t>("unit_type")]);
            //         ImGui::SliderFloat(shutter_final.c_str(), &time_shutter_window_end, 0, frameLength_T, "%.4f");

            //         // Not sure if clamping is necessary
            //         // evtData->getTimeShutterWindow_L() = std::clamp(evtData->getTimeShutterWindow_L(), 0.0f,
            //         // frameLength_T); evtData->getTimeShutterWindow_R() =
            //         std::clamp(evtData->getTimeShutterWindow_R(),
            //         // evtData->getTimeShutterWindow_L(), frameLength_T);

            //         // Add user time shutter window input to parameter store
            //         parameter_store->add("time_shutter_window_begin", time_shutter_window_begin);
            //         parameter_store->add("time_shutter_window_end", time_shutter_window_end);
            //     }
            // }
            // else if (static_cast<GUI::SHUTTER>(parameter_store->get<int32_t>("shutter_type")) ==
            //          GUI::SHUTTER::EVENT_BASED)
            // {

            //     if (parameter_store->exists("event_window_begin") && parameter_store->exists("event_window_end"))
            //     {
            //         uint32_t event_window_end{parameter_store->get<uint32_t>("event_window_end")};
            //         uint32_t event_window_begin{parameter_store->get<uint32_t>("event_window_begin")};
            //         uint32_t frameLength_E = event_window_end - event_window_begin;

            //         if (!parameter_store->exists("event_shutter_window_begin"))
            //         {
            //             parameter_store->add("event_shutter_window_begin", event_window_begin);
            //         }
            //         uint32_t
            //         event_shutter_window_begin{parameter_store->get<uint32_t>("event_shutter_window_begin")};
            //         ImGui::SliderInt("Shutter Initial (events)", (int *)&event_shutter_window_begin, 0,
            //         frameLength_E);

            //         if (!parameter_store->exists("event_shutter_window_end"))
            //         {
            //             parameter_store->add("event_shutter_window_end", event_window_end);
            //         }
            //         uint32_t event_shutter_window_end{parameter_store->get<uint32_t>("event_shutter_window_end")};
            //         ImGui::SliderInt("Shutter Final (events)", (int *)&event_shutter_window_end, 0, frameLength_E);

            //         // Not sure if clamping is necessary
            //         // evtData->getEventShutterWindow_L() = std::clamp(evtData->getEventShutterWindow_L(), (uint) 0,
            //         // frameLength_E); evtData->getEventShutterWindow_R() =
            //         // std::clamp(evtData->getEventShutterWindow_R(), evtData->getEventShutterWindow_L(),
            //         // frameLength_E);

            //         // Add user input back to parameter store
            //         parameter_store->add("event_shutter_window_begin", event_shutter_window_begin);
            //         parameter_store->add("event_shutter_window_end", event_shutter_window_end);
            //     }
            // }

            // // Auto update controls
            // if (!parameter_store->exists("shutter_fps"))
            // {
            //     parameter_store->add("shutter_fps", 0.0f);
            // }
            // float shutter_fps{parameter_store->get<float>("shutter_fps")};
            // ImGui::SliderFloat("FPS", &shutter_fps, 0.0f, 100.0f);
            // parameter_store->add("shutter_fps", shutter_fps);

            // // TODO Implement buttons
            // if (ImGui::Button("Play (Time period)"))
            // {
            // }
            // if (ImGui::Button("Play (Events period)"))
            // {
            // }

            // // "Post" processing
            // if (!parameter_store->exists("shutter_frequency"))
            // {
            //     parameter_store->add("shutter_frequency", 100.0f);
            // }
            // float shutter_frequency{parameter_store->get<float>("shutter_frequency")};
            // ImGui::SliderFloat("Frequency (Hz)", &shutter_frequency, 0.001f, 250.0f);
            // parameter_store->add("shutter_frequency", shutter_frequency);

            // if (parameter_store->exists("time_window_begin") && parameter_store->exists("time_window_end"))
            // {
            //     if (!parameter_store->exists("fwhm"))
            //     {
            //         parameter_store->add("fwhm", 0.0014f);
            //     }
            //     float fwhm{parameter_store->get<float>("fwhm")};
            //     std::string fwhm_str{"Full Width at Half Measure "};
            //     fwhm_str.append(time_units[parameter_store->get<int32_t>("unit_type")]);

            //     ImGui::SliderFloat(fwhm_str.c_str(), &fwhm, 0.0001f,
            //                        (parameter_store->get<float>("time_window_after") -
            //                         parameter_store->get<float>("time_window_after")) *
            //                            0.5,
            //                        "%.4f");
            // }

            ImGui::Begin("Digital Coded Exposure Controls");

            if (!parameter_store->exists("event_contrib_weight"))
            {
                parameter_store->add("event_contrib_weight", 0.5f);
            }
            float event_contrib_weight{parameter_store->get<float>("event_contrib_weight")};
            ImGui::SliderFloat("Event Contribution Weight", &event_contrib_weight, 0.0f, 1.0f);
            parameter_store->add("event_contrib_weight", event_contrib_weight);

            ImGui::Separator();

            if (!parameter_store->exists("shutter_is_morlet"))
            {
                parameter_store->add("shutter_is_morlet", false);
            }
            bool shutter_is_morlet{parameter_store->get<bool>("shutter_is_morlet")};
            ImGui::Checkbox("Morlet Shutter", &shutter_is_morlet);
            parameter_store->add("shutter_is_morlet", shutter_is_morlet);

            // if (!parameter_store->exists("shutter_is_pca"))
            // {
            //     parameter_store->add("shutter_is_pca", false);
            // }
            // bool shutter_is_pca{parameter_store->get<bool>("shutter_is_pca")};
            // ImGui::Checkbox("PCA", &shutter_is_pca);
            // parameter_store->add("shutter_is_pca", shutter_is_pca);

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

            // TODO implement video recording
            // Video (ffmpeg) controls
            // ImGui::Text("Video options"); // TODO add documentation
            // inputTextWrapper(video_name); // TODO consider including library to allow inputting string as parameter
            // if (ImGui::Button("Start Record")) {
            //     recording = true;
            // }
            // if (ImGui::Button("Stop Record")) {
            //     recording = false;
            // }

            ImGui::End();
        }

        // Draw debug window containing fps data
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
            ImGui::End();
        }

        void draw_stream_window()
        {
            ImGui::Begin("Streaming");
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
                parameter_store->add("discovered_cameras", std::string{""});
            }

            std::string discovered_cameras{parameter_store->get<std::string>("discovered_cameras")};
            ImGui::Combo("Camera", &camera_index, discovered_cameras.c_str());

            if (camera_index_copy != camera_index) // Different camera chosen
            {
                parameter_store->add("camera_changed", true);
            }

            // std::cout << "CAMERA INDEX: " << camera_index << std::endl;
            parameter_store->add("camera_index", camera_index);

            if (!parameter_store->exists("program_state"))
            {
                parameter_store->add("program_state", GUI::PROGRAM_STATE::IDLE);
            }

            GUI::PROGRAM_STATE program_state{parameter_store->get<GUI::PROGRAM_STATE>("program_state")};

            if (ImGui::Button(program_state == GUI::PROGRAM_STATE::CAMERA_STREAM ? "Streaming..."
                                                                                 : "Stream From Camera"))
            {
                if (program_state != GUI::PROGRAM_STATE::CAMERA_STREAM)
                {
                    parameter_store->add("camera_changed", true); // Reset reader
                }
                parameter_store->add("program_state", GUI::PROGRAM_STATE::CAMERA_STREAM);
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
            // Save or stop saving stream frames
            ImGui::Checkbox("Save Frames On Next Stream", &stream_save_frames);
            parameter_store->add("stream_save_frames", stream_save_frames);

            if (!parameter_store->exists("stream_save_events"))
            {
                parameter_store->add("stream_save_events", false);
            }

            bool stream_save_events{parameter_store->get<bool>("stream_save_events")};

            // Save or stop saving stream events
            ImGui::Checkbox("Save Events On Next Stream", &stream_save_events);

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

            // std::string stream_save_file_name{parameter_store->get<std::string>("stream_save_file_name")};
            //  From old NOVA source code
            //  const unsigned int max_length = 50;
            //  char buf[max_length];
            //  memset(buf, 0, max_length);
            //  memcpy(buf, stream_save_file_name.c_str(), stream_save_file_name.size());
            //  ImGui::InputText("Stream Output Name", buf, max_length);
            //  stream_save_file_name = buf;

            // if(stream_save_file_name.length() == 0)
            // {
            //     stream_save_file_name = "out";
            // }

            // if(!parameter_store->get<bool>("stream_save"))
            // {
            //     parameter_store->add("stream_save_file_name", stream_save_file_name);
            // }

            if (ImGui::Button("Open File To Save Stream To (Will Stop Streaming)"))
            {
                SDL_ShowSaveFileDialog(save_stream_handle_callback, parameter_store, nullptr, nullptr, 0, nullptr);
            }

            ImGui::End();
        }

        void draw_load_file_window()
        {
            ImGui::Begin("Load");
            ImGui::Text("File:");

            if (ImGui::Button("Open File"))
            {
                SDL_ShowOpenFileDialog(load_file_handle_callback, parameter_store, nullptr, nullptr, 0, nullptr, 0);
            }

            if (!parameter_store->exists("event_discard_odds"))
            {
                parameter_store->add("event_discard_odds", 1.0f);
            }

            // The higher this value is, the higher chance events will be discarded
            float event_discard_odds{parameter_store->get<float>("event_discard_odds")};
            ImGui::Text("Event Discard Odds");
            ImGui::SliderFloat("##Frequency Of Discarded Events", &event_discard_odds, 1.0f, 10000, "%f");
            parameter_store->add("event_discard_odds", event_discard_odds);

            // TODO: Cache recent files and state?
            ImGui::End();
        }

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

            // Current Index (for EVENT type)
            if (parameter_store->get<Scrubber::ScrubberType>("scrubber.type") == Scrubber::ScrubberType::EVENT)
            {
                int current_index_int = static_cast<int>(parameter_store->get<std::size_t>("scrubber.current_index"));

                // Get min/max values from scrubber if available
                int min_index_int = 0;
                int max_index_int = 0;
                if (scrubber)
                {
                    min_index_int = static_cast<int>(parameter_store->get<std::size_t>("scrubber.min_index"));
                    max_index_int = static_cast<int>(parameter_store->get<std::size_t>("scrubber.max_index"));
                }

                if (ImGui::SliderInt("Current Index", &current_index_int, min_index_int, max_index_int))
                {
                    if (current_index_int < min_index_int)
                        current_index_int = min_index_int;
                    if (current_index_int > max_index_int)
                        current_index_int = max_index_int;
                    parameter_store->add("scrubber.current_index", static_cast<std::size_t>(current_index_int));
                }

                // Index Window
                if (!parameter_store->exists("scrubber.index_window"))
                {
                    parameter_store->add("scrubber.index_window", static_cast<std::size_t>(50));
                }
                int index_window_int = static_cast<int>(parameter_store->get<std::size_t>("scrubber.index_window"));

                // Calculate maximum window size (half of data size, minimum 1)
                int max_window_size = 1;
                if (scrubber)
                {
                    int data_size = static_cast<int>(parameter_store->get<std::size_t>("scrubber.max_index") -
                                                     parameter_store->get<std::size_t>("scrubber.min_index") + 1);
                    max_window_size = std::max(1, data_size / 2);
                }

                if (ImGui::SliderInt("Index Window", &index_window_int, 1, max_window_size))
                {
                    if (index_window_int < 1)
                        index_window_int = 1;
                    if (index_window_int > max_window_size)
                        index_window_int = max_window_size;
                    parameter_store->add("scrubber.index_window", static_cast<std::size_t>(index_window_int));
                }

                 // Time Step
                if (!parameter_store->exists("scrubber.index_step"))
                {
                    size_t default_step{0};
                    parameter_store->add("scrubber.index_step", default_step);
                }
                size_t event_step = parameter_store->get<std::size_t>("scrubber.index_step");

                // Calculate maximum step size (total time range)
                size_t max_step_int = max_index_int - min_index_int;

                int32_t event_step_copy = static_cast<int32_t>(event_step);
                std::string event_step_label{"Index Step"};
                if (ImGui::SliderInt(event_step_label.c_str(), &event_step_copy, 0, max_step_int))
                {
                    if (max_step_int >= 0)
                    {
                        event_step = event_step_copy;
                        size_t lowest_step{0};
                        event_step = std::clamp(event_step, lowest_step, max_step_int);
                        parameter_store->add("scrubber.index_step", event_step);
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
                switch(static_cast<TIME>(unit_type))
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

                if(!parameter_store->exists("unit_time_conversion_factor"))
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
                if (ImGui::SliderFloat(current_time_label.c_str(), &current_time_unit_adjusted, min_time_unit_adjusted, max_time_unit_adjusted, time_format_str.c_str()))
                {
                    // STOP CLAMP FROM CRASHING THE PROGRAM FOR THE NTH TIME
                    if(max_time_unit_adjusted > min_time_unit_adjusted)
                    {
                        current_time_unit_adjusted = std::clamp(current_time_unit_adjusted, min_time_unit_adjusted, max_time_unit_adjusted);
                        current_time = current_time_unit_adjusted * unit_time_conversion_factor; // Revert conversion to store back into scrubber
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

                // Calculate maximum window size (half of total time range, minimum 0.001)
                float max_window_time = std::max(0.00001f, (max_time - min_time) * 0.5f);
                float max_window_time_unit_adjusted = max_window_time / unit_time_conversion_factor;


                std::string time_window_label = "Time Window " + time_unit_suffix;
                if (ImGui::SliderFloat(time_window_label.c_str(), &time_window_unit_adjusted, 0.00001f, max_window_time_unit_adjusted, time_format_str.c_str()))
                {
                    // STOP CLAMP FROM CRASHING THE PROGRAM FOR THE NTH TIME
                    if (max_window_time_unit_adjusted > 0.00001f)
                    {
                        time_window_unit_adjusted = std::clamp(time_window_unit_adjusted, 0.00001f, max_window_time_unit_adjusted);
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
                float max_step_time = max_time - min_time;
                float max_step_time_unit_adjusted = max_step_time / unit_time_conversion_factor;

                std::string time_step_label = "Time Step " + time_unit_suffix;
                if (ImGui::SliderFloat(time_step_label.c_str(), &time_step_unit_adjusted, 0.00001f, max_step_time_unit_adjusted, time_format_str.c_str()))
                {
                    // STOP CLAMP FROM CRASHING THE PROGRAM FOR THE NTH TIME
                    if (max_step_time_unit_adjusted > 0.00001f)
                    {
                        time_step_unit_adjusted = std::clamp(time_step_unit_adjusted, 0.00001f, max_step_time_unit_adjusted);
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

    public:
        enum class TIME : uint8_t
        {
            UNIT_S = 0,
            UNIT_MS = 1,
            UNIT_US = 2
        };

        enum class SHUTTER
        {
            TIME_BASED = 0,
            EVENT_BASED = 1
        };

        enum class PROGRAM_STATE : uint8_t
        {
            IDLE = 0,
            FILE_READ = 1,    // Program is reading from a file
            FILE_STREAM = 2,  // Program is streaming from a file
            CAMERA_STREAM = 3 // Program is streaming from a camera
        };

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

            // Setup Dear ImGui style
            // ImGui::StyleColorsDark();

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

        ~GUI()
        {
            // Cleanup ImGui
            ImGui_ImplSDL3_Shutdown();
            ImGui_ImplSDLGPU3_Shutdown();
            ImGui::DestroyContext();
        }

        void event_handler(SDL_Event *event)
        {
            ImGui_ImplSDL3_ProcessEvent(event);
        }

        // This is mandatory: call ImGui_ImplSDLGPU3_PrepareDrawData() to upload the vertex/index buffer!
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
                    // std::cout << "No imgui.ini found, setting default layout." << std::endl;
                    reset_layout_with_dockbuilder();
                }
                check_for_layout_file = false;
            }

            // draw error popup
            draw_error_popup_window();

            // Draw info block
            draw_info_window();

            // Draw debug block
            draw_debug_window(fps);
            draw_load_file_window();
            draw_digital_coded_exposure();
            draw_stream_window();
            draw_scrubber_window();
            draw_visualizer();

            // Rendering
            ImGui::Render();
            draw_data = ImGui::GetDrawData();

            ImGui_ImplSDLGPU3_PrepareDrawData(draw_data, command_buffer);
        }

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

        void reset_layout_with_dockbuilder()
        {
            // ImGuiID dockspace_id = ImGui::GetID("My Dockspace");
            // ImGuiViewport* viewport = ImGui::GetMainViewport();
            ImGuiID dockspace_id = ImGui::GetMainViewport()->ID;
            ImGui::DockBuilderRemoveNode(dockspace_id);
            ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
            ImGui::DockBuilderSetNodeSize(dockspace_id, ImGui::GetMainViewport()->Size);

            // if (ImGui::DockBuilderGetNode(dockspace_id) == nullptr)
            // {
            // ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
            // ImGui::DockBuilderSetNodeSize(dockspace_id, viewport->Size);
            ImGuiID dock_id_right; // right side for info and debug/load/stream windows
            ImGuiID dock_id_main = dockspace_id;
            ImGui::DockBuilderSplitNode(dock_id_main, ImGuiDir_Right, 0.40f, &dock_id_right,
                                        &dock_id_main); // split window 60% left, 40% right

            ImGuiID dock_id_left_bottom; // bottom left for scrubber
            ImGui::DockBuilderSplitNode(dock_id_main, ImGuiDir_Down, 0.20f, &dock_id_left_bottom, &dock_id_main);

            ImGuiID dock_id_right_top = dock_id_right; // top right for info
            ImGuiID dock_id_right_bottom;              // top bottom for debug/load/stream
            ImGui::DockBuilderSplitNode(dock_id_right_top, ImGuiDir_Down, 0.20f, &dock_id_right_bottom,
                                        &dock_id_right_top);

            ImGui::DockBuilderDockWindow("Info", dock_id_right_top);
            ImGui::DockBuilderDockWindow("Debug", dock_id_right_bottom);
            ImGui::DockBuilderDockWindow("Load", dock_id_right_bottom);
            ImGui::DockBuilderDockWindow("Streaming", dock_id_right_bottom);
            ImGui::DockBuilderDockWindow("Frame", dock_id_main); // DCE window
            ImGui::DockBuilderDockWindow("3D Visualizer", dock_id_main);
            ImGui::DockBuilderDockWindow("Scrubber", dock_id_left_bottom);

            ImGui::DockBuilderFinish(dockspace_id);
            // }
        }
};

// Callback used with SDL_ShowOpenFileDialog in draw_load_file_window
inline void SDLCALL load_file_handle_callback(void *param_store, const char *const *data_file_list, int filter_unused)
{
    ParameterStore *param_store_ptr{static_cast<ParameterStore *>(param_store)};
    if (data_file_list)
    {
        if (*data_file_list)
        {
            std::string file_name{*data_file_list};
            param_store_ptr->add("load_file_name", file_name);
            param_store_ptr->add("load_file_changed", true);
            param_store_ptr->add("program_state", GUI::PROGRAM_STATE::FILE_READ); // Determines if program is streaming

            // reset camera streams
            param_store_ptr->add("camera_changed", true);
        }
    }
    else
    {
        std::cerr << "Error happened when selecting file or no file was chosen" << std::endl;
    }
}

// Callback used with SDL_ShowOpenFileDialog in draw_stream_window
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
