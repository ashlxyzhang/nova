#pragma once

#ifndef CAMERA_HH
#define CAMERA_HH

#include "pch.hh"

/**
 * @brief A header-only perspective camera class using GLM
 *
 * This camera class provides functionality for:
 * - Perspective projection with configurable field of view
 * - Camera positioning and orientation
 * - Zoom functionality (both FOV and distance-based)
 * - Rotation (pitch, yaw, roll)
 * - Panning (translation in world space)
 * - View and projection matrix generation
 */
class Camera
{
    public:
        // Camera movement types
        enum class MovementType
        {
            FORWARD,
            BACKWARD,
            LEFT,
            RIGHT,
            UP,
            DOWN
        };

        // Rotation types
        enum class RotationType
        {
            PITCH,
            YAW,
            ROLL
        };

    private:
        // Camera position and orientation
        glm::vec3 m_position;
        glm::vec3 m_front;
        glm::vec3 m_up;
        glm::vec3 m_right;
        glm::vec3 m_world_up;

        // Camera angles (in radians)
        float m_yaw;
        float m_pitch;
        float m_roll;

        // Projection parameters
        float m_fov; // Field of view in degrees
        float m_aspect_ratio;
        float m_near_plane;
        float m_far_plane;

        // Movement sensitivity
        float m_movement_speed;
        float m_mouse_sensitivity;
        float m_zoom_sensitivity;

        // Zoom constraints
        float m_min_fov;
        float m_max_fov;

        // Internal state
        bool m_constrain_pitch;
        bool m_constrain_roll;

    public:
        /**
         * @brief Constructor
         * @param position Initial camera position
         * @param world_up World up vector (usually (0, 1, 0))
         * @param yaw Initial yaw angle in degrees
         * @param pitch Initial pitch angle in degrees
         * @param fov Field of view in degrees
         * @param aspect_ratio Aspect ratio (width/height)
         * @param near_plane Near clipping plane
         * @param far_plane Far clipping plane
         */
        Camera(glm::vec3 position = glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3 world_up = glm::vec3(0.0f, 1.0f, 0.0f),
               float yaw = -90.0f, float pitch = 0.0f, float fov = 45.0f, float aspect_ratio = 16.0f / 9.0f,
               float near_plane = 0.1f, float far_plane = 1000.0f)
            : m_position(position), m_world_up(world_up), m_yaw(yaw), m_pitch(pitch), m_roll(0.0f), m_fov(fov),
              m_aspect_ratio(aspect_ratio), m_near_plane(near_plane), m_far_plane(far_plane), m_movement_speed(2.5f),
              m_mouse_sensitivity(0.1f), m_zoom_sensitivity(2.0f), m_min_fov(1.0f), m_max_fov(120.0f),
              m_constrain_pitch(true), m_constrain_roll(false)
        {
            updateCameraVectors();
        }

        /**
         * @brief Get the view matrix
         * @return 4x4 view matrix
         */
        glm::mat4 getViewMatrix() const
        {
            return glm::lookAt(m_position, m_position + m_front, m_up);
        }

        /**
         * @brief Get the projection matrix
         * @return 4x4 projection matrix
         */
        glm::mat4 getProjectionMatrix() const
        {
            return glm::perspective(glm::radians(m_fov), m_aspect_ratio, m_near_plane, m_far_plane);
        }

        /**
         * @brief Get the combined view-projection matrix
         * @return 4x4 view-projection matrix
         */
        glm::mat4 getViewProjectionMatrix() const
        {
            return getProjectionMatrix() * getViewMatrix();
        }

        /**
         * @brief Process keyboard input for camera movement
         * @param direction Movement direction
         * @param delta_time Time since last frame
         */
        void processKeyboard(MovementType direction, float delta_time)
        {
            float velocity = m_movement_speed * delta_time;
            glm::vec3 movement = glm::vec3(0.0f);

            switch (direction)
            {
            case MovementType::FORWARD:
                movement += m_front;
                break;
            case MovementType::BACKWARD:
                movement -= m_front;
                break;
            case MovementType::LEFT:
                movement -= m_right;
                break;
            case MovementType::RIGHT:
                movement += m_right;
                break;
            case MovementType::UP:
                movement += m_up;
                break;
            case MovementType::DOWN:
                movement -= m_up;
                break;
            }

            if (glm::length(movement) > 0.0f)
            {
                movement = glm::normalize(movement);
                m_position += movement * velocity;
            }
        }

        /**
         * @brief Process mouse movement for camera rotation
         * @param xoffset Mouse X offset
         * @param yoffset Mouse Y offset
         * @param constrain_pitch Whether to constrain pitch to [-89, 89] degrees
         */
        void processMouseMovement(float xoffset, float yoffset, bool constrain_pitch = true)
        {
            xoffset *= m_mouse_sensitivity;
            yoffset *= m_mouse_sensitivity;

            m_yaw += xoffset;
            m_pitch += yoffset;

            if (constrain_pitch)
            {
                if (m_pitch > 89.0f)
                    m_pitch = 89.0f;
                if (m_pitch < -89.0f)
                    m_pitch = -89.0f;
            }

            updateCameraVectors();
        }

        /**
         * @brief Process mouse scroll for zoom
         * @param yoffset Scroll offset
         */
        void processMouseScroll(float yoffset)
        {
            m_fov -= yoffset * m_zoom_sensitivity;
            if (m_fov < m_min_fov)
                m_fov = m_min_fov;
            if (m_fov > m_max_fov)
                m_fov = m_max_fov;
        }

        /**
         * @brief Rotate the camera by specific angles
         * @param type Rotation type (pitch, yaw, or roll)
         * @param angle Angle in degrees
         */
        void rotate(RotationType type, float angle)
        {
            switch (type)
            {
            case RotationType::PITCH:
                m_pitch += angle;
                if (m_constrain_pitch)
                {
                    if (m_pitch > 89.0f)
                        m_pitch = 89.0f;
                    if (m_pitch < -89.0f)
                        m_pitch = -89.0f;
                }
                break;
            case RotationType::YAW:
                m_yaw += angle;
                break;
            case RotationType::ROLL:
                if (m_constrain_roll)
                {
                    m_roll += angle;
                    if (m_roll > 89.0f)
                        m_roll = 89.0f;
                    if (m_roll < -89.0f)
                        m_roll = -89.0f;
                }
                else
                {
                    m_roll += angle;
                }
                break;
            }
            updateCameraVectors();
        }

        /**
         * @brief Pan the camera (translate position)
         * @param offset Translation offset
         */
        void pan(const glm::vec3 &offset)
        {
            m_position += offset;
        }

        /**
         * @brief Pan the camera along specific axes
         * @param right_offset Right vector offset
         * @param up_offset Up vector offset
         * @param forward_offset Front vector offset
         */
        void pan(float right_offset, float up_offset, float forward_offset)
        {
            m_position += m_right * right_offset;
            m_position += m_up * up_offset;
            m_position += m_front * forward_offset;
        }

        /**
         * @brief Set camera position
         * @param position New position
         */
        void setPosition(const glm::vec3 &position)
        {
            m_position = position;
        }

        /**
         * @brief Set camera orientation
         * @param yaw Yaw angle in degrees
         * @param pitch Pitch angle in degrees
         * @param roll Roll angle in degrees
         */
        void setOrientation(float yaw, float pitch, float roll = 0.0f)
        {
            m_yaw = yaw;
            m_pitch = pitch;
            m_roll = roll;
            updateCameraVectors();
        }

        /**
         * @brief Set field of view
         * @param fov Field of view in degrees
         */
        void setFOV(float fov)
        {
            m_fov = glm::clamp(fov, m_min_fov, m_max_fov);
        }

        /**
         * @brief Set aspect ratio
         * @param aspect_ratio Aspect ratio (width/height)
         */
        void setAspectRatio(float aspect_ratio)
        {
            m_aspect_ratio = aspect_ratio;
        }

        /**
         * @brief Set clipping planes
         * @param near_plane Near clipping plane
         * @param far_plane Far clipping plane
         */
        void setClippingPlanes(float near_plane, float far_plane)
        {
            m_near_plane = near_plane;
            m_far_plane = far_plane;
        }

        /**
         * @brief Set movement speed
         * @param speed Movement speed multiplier
         */
        void setMovementSpeed(float speed)
        {
            m_movement_speed = speed;
        }

        /**
         * @brief Set mouse sensitivity
         * @param sensitivity Mouse sensitivity multiplier
         */
        void setMouseSensitivity(float sensitivity)
        {
            m_mouse_sensitivity = sensitivity;
        }

        /**
         * @brief Set zoom sensitivity
         * @param sensitivity Zoom sensitivity multiplier
         */
        void setZoomSensitivity(float sensitivity)
        {
            m_zoom_sensitivity = sensitivity;
        }

        /**
         * @brief Set FOV constraints
         * @param min_fov Minimum FOV in degrees
         * @param max_fov Maximum FOV in degrees
         */
        void setFOVConstraints(float min_fov, float max_fov)
        {
            m_min_fov = min_fov;
            m_max_fov = max_fov;
            m_fov = glm::clamp(m_fov, m_min_fov, m_max_fov);
        }

        /**
         * @brief Set pitch constraint
         * @param constrain Whether to constrain pitch to [-89, 89] degrees
         */
        void setPitchConstraint(bool constrain)
        {
            m_constrain_pitch = constrain;
        }

        /**
         * @brief Set roll constraint
         * @param constrain Whether to constrain roll to [-89, 89] degrees
         */
        void setRollConstraint(bool constrain)
        {
            m_constrain_roll = constrain;
        }

        // Getters
        glm::vec3 getPosition() const
        {
            return m_position;
        }
        glm::vec3 getFront() const
        {
            return m_front;
        }
        glm::vec3 getUp() const
        {
            return m_up;
        }
        glm::vec3 getRight() const
        {
            return m_right;
        }
        glm::vec3 getWorldUp() const
        {
            return m_world_up;
        }

        float getYaw() const
        {
            return m_yaw;
        }
        float getPitch() const
        {
            return m_pitch;
        }
        float getRoll() const
        {
            return m_roll;
        }

        float getFOV() const
        {
            return m_fov;
        }
        float getAspectRatio() const
        {
            return m_aspect_ratio;
        }
        float getNearPlane() const
        {
            return m_near_plane;
        }
        float getFarPlane() const
        {
            return m_far_plane;
        }

        float getMovementSpeed() const
        {
            return m_movement_speed;
        }
        float getMouseSensitivity() const
        {
            return m_mouse_sensitivity;
        }
        float getZoomSensitivity() const
        {
            return m_zoom_sensitivity;
        }

        /**
         * @brief Look at a specific point
         * @param target Target point to look at
         * @param up Up vector (optional, uses world up if not provided)
         */
        void lookAt(const glm::vec3 &target, const glm::vec3 &up = glm::vec3(0.0f, 1.0f, 0.0f))
        {
            m_front = glm::normalize(target - m_position);
            m_right = glm::normalize(glm::cross(m_front, up));
            m_up = glm::normalize(glm::cross(m_right, m_front));

            // Update angles based on new orientation
            m_yaw = glm::degrees(atan2(m_front.z, m_front.x));
            m_pitch = glm::degrees(asin(m_front.y));
            m_roll = 0.0f;
        }

        /**
         * @brief Reset camera to default orientation
         */
        void reset()
        {
            m_position = glm::vec3(0.0f, 0.0f, 0.0f);
            m_yaw = -90.0f;
            m_pitch = 0.0f;
            m_roll = 0.0f;
            m_fov = 45.0f;
            updateCameraVectors();
        }

    private:
        /**
         * @brief Update camera vectors based on current angles
         */
        void updateCameraVectors()
        {
            // Calculate the new front vector
            glm::vec3 front;
            front.x = cos(glm::radians(m_yaw)) * cos(glm::radians(m_pitch));
            front.y = sin(glm::radians(m_pitch));
            front.z = sin(glm::radians(m_yaw)) * cos(glm::radians(m_pitch));
            m_front = glm::normalize(front);

            // Re-calculate the right and up vector
            m_right = glm::normalize(glm::cross(m_front, m_world_up));
            m_up = glm::normalize(glm::cross(m_right, m_front));

            // Apply roll if needed
            if (m_roll != 0.0f)
            {
                float roll_rad = glm::radians(m_roll);
                glm::mat4 roll_matrix = glm::rotate(glm::mat4(1.0f), roll_rad, m_front);
                m_up = glm::vec3(roll_matrix * glm::vec4(m_up, 0.0f));
                m_right = glm::normalize(glm::cross(m_front, m_up));
            }
        }
};

#endif // CAMERA_HH
