#ifdef _WIN64

#pragma once
#include "rtx_light_base.h"

namespace RTX {
    struct RectProperties {
        float x, y, z;          // Position
        float dirX, dirY, dirZ; // Direction
        float xAxisX, xAxisY, xAxisZ; // X axis
        float yAxisX, yAxisY, yAxisZ; // Y axis
        float xSize, ySize;     // Rectangle dimensions
        float brightness;       // Intensity scaling
        float r, g, b;          // Color (0-1)
        bool enableShaping;     // Enable light shaping
        float shapingConeAngle; // Cone angle for shaped light
        float shapingConeSoftness; // Softness of cone edge
    };

    class RectLight : public ILight {
    public:
        RectLight(const RectProperties& props);
        ~RectLight() override;

        remixapi_LightHandle Create(remix::Interface* remixInterface) override;
        bool Update() override;
        remixapi_LightHandle GetHandle() const override { return m_handle; }
        uint64_t GetID() const override { return m_id; }
        std::string GetTypeName() const override { return "RectLight"; }
        
        // Specific setters for this light type
        void SetPosition(float x, float y, float z);
        void SetDirection(float x, float y, float z);
        void SetXAxis(float x, float y, float z);
        void SetYAxis(float x, float y, float z);
        void SetDimensions(float xSize, float ySize);
        void SetColor(float r, float g, float b);
        void SetBrightness(float brightness);
        void EnableShaping(bool enable);
        void SetShapingConeAngle(float degrees);
        void SetShapingConeSoftness(float softness);
        
        // Getters
        const RectProperties& GetProperties() const { return m_properties; }

    private:
        remix::Interface* m_remix;
        remixapi_LightHandle m_handle;
        RectProperties m_properties;
        uint64_t m_id;
        bool m_needsUpdate;
        
        static uint64_t GenerateID();
    };
}
#endif