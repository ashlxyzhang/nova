#pragma once

#ifndef CAMERA_HH
#define CAMERA_HH

#include "pch.hh"

/**
 * @brief A simplified orbital camera class using GLM
 *
 * This camera class provides functionality for:
 * - Orbital camera movement around a center point
 * - Zoom functionality (FOV-based)
 * - Panning (changing the orbit center)
 * - View and projection matrix generation
 */
class Camera
{
    private:
        // Camera position and orientation
        glm::vec3 m_position;
        glm::vec3 m_front;
        glm::vec3 m_up;
        glm::vec3 m_right;
        glm::vec3 m_world_up;

        // Camera angles (in degrees)
        float m_yaw;
        float m_pitch;

        // Orbit center and distance
        glm::vec3 m_orbit_center;
        float m_orbit_distance;

        // Projection parameters
        float m_fov; // Field of view in degrees
        float m_aspect_ratio;
        float m_near_plane;
        float m_far_plane;

        // Sensitivity settings
        float m_mouse_sensitivity;
        float m_zoom_sensitivity;

        // Zoom constraints
        float m_min_fov;
        float m_max_fov;

    public:
        /**
         * @brief Constructor
         * @param orbit_center Center point to orbit around
         * @param orbit_distance Initial distance from center
         * @param fov Field of view in degrees
         * @param aspect_ratio Aspect ratio (width/height)
         * @param near_plane Near clipping plane
         * @param far_plane Far clipping plane
         */
        Camera(glm::vec3 orbit_center = glm::vec3(0.0f, 0.0f, 0.0f), float orbit_distance = 5.0f,
               float fov = 45.0f, float aspect_ratio = 16.0f / 9.0f,
               float near_plane = 0.1f, float far_plane = 1000.0f)
            : m_world_up(0.0f, 1.0f, 0.0f), m_yaw(0.0f), m_pitch(0.0f),
              m_orbit_center(orbit_center), m_orbit_distance(orbit_distance),
              m_fov(fov), m_aspect_ratio(aspect_ratio), m_near_plane(near_plane), m_far_plane(far_plane),
              m_mouse_sensitivity(0.1f), m_zoom_sensitivity(2.0f), m_min_fov(1.0f), m_max_fov(120.0f)
        {
            updateOrbitPosition();
        }

        /**
         * @brief Get the view matrix
         * @return 4x4 view matrix
         */
        glm::mat4 getViewMatrix() const
        {
            return glm::lookAt(m_position, m_orbit_center, m_up);
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
         * @brief Process mouse movement for orbital camera rotation
         * @param xoffset Mouse X offset
         * @param yoffset Mouse Y offset
         */
        void processMouseMovement(float xoffset, float yoffset)
        {
            xoffset *= m_mouse_sensitivity;
            yoffset *= m_mouse_sensitivity;

            m_yaw += xoffset;
            m_pitch += yoffset;

            // Constrain pitch to prevent camera flipping
            if (m_pitch > 89.0f)
                m_pitch = 89.0f;
            if (m_pitch < -89.0f)
                m_pitch = -89.0f;

            updateOrbitPosition();
        }

        /**
         * @brief Process mouse scroll for zoom (FOV-based)
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
         * @brief Pan the camera by changing the orbit center
         * @param offset Translation offset for the orbit center
         */
        void pan(const glm::vec3 &offset)
        {
            m_orbit_center += offset;
            updateOrbitPosition();
        }

        /**
         * @brief Set the orbit center
         * @param center New orbit center
         */
        void setOrbitCenter(const glm::vec3& center)
        {
            m_orbit_center = center;
            updateOrbitPosition();
        }

        /**
         * @brief Set the orbit distance
         * @param distance Distance from the orbit center
         */
        void setOrbitDistance(float distance)
        {
            m_orbit_distance = distance;
            updateOrbitPosition();
        }

        /**
         * @brief Set aspect ratio
         * @param aspect_ratio Aspect ratio (width/height)
         */
        void setAspectRatio(float aspect_ratio)
        {
            m_aspect_ratio = aspect_ratio;
        }

        // Essential getters
        glm::vec3 getPosition() const { return m_position; }
        glm::vec3 getOrbitCenter() const { return m_orbit_center; }
        float getOrbitDistance() const { return m_orbit_distance; }
        float getFOV() const { return m_fov; }
        float getAspectRatio() const { return m_aspect_ratio; }
        float getYaw() const { return m_yaw; }
        float getPitch() const { return m_pitch; }

    private:
        /**
         * @brief Update camera position based on orbit parameters (spherical coordinates)
         */
        void updateOrbitPosition()
        {
            // Convert to spherical coordinates
            float pitch_rad = glm::radians(m_pitch);
            float yaw_rad = glm::radians(m_yaw);
            
            // Calculate position in spherical coordinates
            // Using standard spherical coordinates with yaw=0 pointing along +X
            float x = m_orbit_center.x + m_orbit_distance * cos(pitch_rad) * cos(yaw_rad);
            float y = m_orbit_center.y + m_orbit_distance * sin(pitch_rad);
            float z = m_orbit_center.z + m_orbit_distance * cos(pitch_rad) * sin(yaw_rad);
            
            m_position = glm::vec3(x, y, z);
            
            // Update front vector to look at the orbit center
            m_front = glm::normalize(m_orbit_center - m_position);
            
            // Recalculate right and up vectors
            m_right = glm::normalize(glm::cross(m_front, m_world_up));
            m_up = glm::normalize(glm::cross(m_right, m_front));
        }
};

#endif // CAMERA_HH
