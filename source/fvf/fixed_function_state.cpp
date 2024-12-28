// fixed_function_state.cpp
#include "fixed_function_state.h"
#include <tier0/dbg.h>
#include "ff_logging.h"
#include <d3d9.h>

FixedFunctionState::FixedFunctionState() {
    FF_LOG("Creating FixedFunctionState instance");
    m_isStored = false;
}

FixedFunctionState::~FixedFunctionState() {
    if (m_state.vertexShader) m_state.vertexShader->Release();
    if (m_state.pixelShader) m_state.pixelShader->Release();
}

bool FixedFunctionState::FindAndSetTexture(IDirect3DDevice9* device, IMaterial* material) {
    if (!device || !material) {
        FF_WARN("Invalid device or material in FindAndSetTexture");
        return false;
    }

    FF_LOG("Attempting to set texture for material: %s", material->GetName());

    try {
        // Safely get texture var
        IMaterialVar* textureVar = material->FindVar("$basetexture", nullptr);
        if (!textureVar) {
            FF_LOG("No $basetexture var for material %s", material->GetName());
            goto USE_FALLBACK;
        }

        FF_LOG("Found $basetexture var for %s", material->GetName());

        if (!textureVar->IsDefined()) {
            FF_LOG("$basetexture not defined for material %s", material->GetName());
            goto USE_FALLBACK;
        }

        // Safely get texture handle
        void* texHandle = nullptr;
        try {
            FF_LOG("Getting texture value for %s", material->GetName());
            texHandle = textureVar->GetTextureValue();
            FF_LOG("Got texture handle: %p", texHandle);
        }
        catch (...) {
            FF_WARN("Exception getting texture value for material %s", material->GetName());
            goto USE_FALLBACK;
        }

        if (!texHandle) {
            FF_LOG("Null texture handle for material %s", material->GetName());
            goto USE_FALLBACK;
        }

        // Safely cast and set texture
        IDirect3DBaseTexture9* d3dtex = static_cast<IDirect3DBaseTexture9*>(texHandle);
        if (!d3dtex) {
            FF_LOG("Failed to cast texture for material %s", material->GetName());
            goto USE_FALLBACK;
        }

        FF_LOG("Checking texture type for %s", material->GetName());
        // Verify texture type
        D3DRESOURCETYPE texType = d3dtex->GetType();
        FF_LOG("Texture type: %d", texType);
        
        if (texType != D3DRTYPE_TEXTURE && texType != D3DRTYPE_CUBETEXTURE) {
            FF_WARN("Invalid texture type %d for material %s", texType, material->GetName());
            goto USE_FALLBACK;
        }

        FF_LOG("Setting texture for %s", material->GetName());
        // Set texture with error checking
        HRESULT hr = device->SetTexture(0, d3dtex);
        if (FAILED(hr)) {
            FF_WARN("Failed to set texture for material %s (HRESULT: 0x%x)", 
                material->GetName(), hr);
            goto USE_FALLBACK;
        }

        FF_LOG("Successfully set texture for %s", material->GetName());

        // Setup texture stages safely
        FF_LOG("Setting up texture stages for %s", material->GetName());
        device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
        device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
        device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
        device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);

        return true;

    USE_FALLBACK:
        FF_LOG("Using fallback rendering for %s", material->GetName());
        // Safe fallback state
        device->SetTexture(0, nullptr);
        device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG2);
        device->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
        device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG2);
        device->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);

        // Set a default material color
        D3DMATERIAL9 mtrl;
        ZeroMemory(&mtrl, sizeof(mtrl));
        mtrl.Diffuse = D3DXCOLOR(0.7f, 0.7f, 0.7f, 1.0f);
        mtrl.Ambient = D3DXCOLOR(0.3f, 0.3f, 0.3f, 1.0f);
        device->SetMaterial(&mtrl);

        return false;
    }
    catch (const std::exception& e) {
        FF_WARN("Exception in FindAndSetTexture for %s: %s", 
            material->GetName(), e.what());
        return false;
    }
    catch (...) {
        FF_WARN("Unknown exception in FindAndSetTexture for %s", 
            material->GetName());
        return false;
    }
}


void FixedFunctionState::Store(IDirect3DDevice9* device) {
    if (!device) return;

    // Store shaders
    device->GetVertexShader(&m_state.vertexShader);
    device->GetPixelShader(&m_state.pixelShader);
    device->GetFVF(&m_state.fvf);

    // Store matrices
    device->GetTransform(D3DTS_WORLD, &m_state.world);
    device->GetTransform(D3DTS_VIEW, &m_state.view);
    device->GetTransform(D3DTS_PROJECTION, &m_state.projection);

    // Store render states
    device->GetRenderState(D3DRS_LIGHTING, &m_state.lighting);
    device->GetRenderState(D3DRS_AMBIENT, &m_state.ambient);
    device->GetRenderState(D3DRS_COLORVERTEX, &m_state.colorVertex);
    device->GetRenderState(D3DRS_CULLMODE, &m_state.cullMode);
    device->GetRenderState(D3DRS_ZENABLE, &m_state.zEnable);
    device->GetRenderState(D3DRS_ALPHABLENDENABLE, &m_state.alphaBlendEnable);
    device->GetRenderState(D3DRS_SRCBLEND, &m_state.srcBlend);
    device->GetRenderState(D3DRS_DESTBLEND, &m_state.destBlend);

    // Store texture stages (store 8 stages)
    m_state.textureStages.clear();
    for (DWORD i = 0; i < 8; i++) {
        StoreTextureStage(device, i);
    }

    m_isStored = true;
    FF_LOG("Device state stored");
}

void FixedFunctionState::Restore(IDirect3DDevice9* device) {
    if (!device || !m_isStored) return;

    // Restore shaders
    device->SetVertexShader(m_state.vertexShader);
    device->SetPixelShader(m_state.pixelShader);
    device->SetFVF(m_state.fvf);

    // Restore matrices
    device->SetTransform(D3DTS_WORLD, &m_state.world);
    device->SetTransform(D3DTS_VIEW, &m_state.view);
    device->SetTransform(D3DTS_PROJECTION, &m_state.projection);

    // Restore render states
    device->SetRenderState(D3DRS_LIGHTING, m_state.lighting);
    device->SetRenderState(D3DRS_AMBIENT, m_state.ambient);
    device->SetRenderState(D3DRS_COLORVERTEX, m_state.colorVertex);
    device->SetRenderState(D3DRS_CULLMODE, m_state.cullMode);
    device->SetRenderState(D3DRS_ZENABLE, m_state.zEnable);
    device->SetRenderState(D3DRS_ALPHABLENDENABLE, m_state.alphaBlendEnable);
    device->SetRenderState(D3DRS_SRCBLEND, m_state.srcBlend);
    device->SetRenderState(D3DRS_DESTBLEND, m_state.destBlend);

    // Restore texture stages
    for (DWORD i = 0; i < m_state.textureStages.size(); i++) {
        RestoreTextureStage(device, i, m_state.textureStages[i]);
    }

    // Release shader references
    if (m_state.vertexShader) {
        m_state.vertexShader->Release();
        m_state.vertexShader = nullptr;
    }
    if (m_state.pixelShader) {
        m_state.pixelShader->Release();
        m_state.pixelShader = nullptr;
    }

    m_isStored = false;
    FF_LOG("Device state restored");
}

void FixedFunctionState::SetupFixedFunction(
    IDirect3DDevice9* device,
    VertexFormat_t sourceFormat,
    IMaterial* material,
    bool enabled)
{
    static float lastLogTime = 0.0f;
    float currentTime = Plat_FloatTime();
    bool shouldLog = (currentTime - lastLogTime > 1.0f);
    
    if (shouldLog) {
        FF_LOG(">>> SetupFixedFunction Called <<<");
        FF_LOG("Material: %s", material ? material->GetName() : "null");
        lastLogTime = currentTime;
    }

    if (!device || !material) {
        FF_WARN("Invalid device or material");
        return;
    }

    bool isModel = (strstr(material->GetName(), "models/") != nullptr) ||
                  (strstr(material->GetShaderName(), "VertexLitGeneric") != nullptr);

    FF_LOG("SetupFixedFunction for %s", material->GetName());
    FF_LOG("  Format: 0x%x", sourceFormat);
    FF_LOG("  Is Model: %s", isModel ? "Yes" : "No");


    try {
        // Disable shaders
        device->SetVertexShader(nullptr);
        device->SetPixelShader(nullptr);

        // Setup vertex format
        DWORD fvf = GetFVFFromSourceFormat(sourceFormat);
        device->SetFVF(fvf);
    
        // For models, set up specific states
        if (isModel) {
            // Enable vertex blend for skinned meshes
            if (sourceFormat & FF_VERTEX_BONES) {
                FF_LOG("  Setting up skinned mesh states");
                device->SetRenderState(D3DRS_VERTEXBLEND, D3DVBF_3WEIGHTS);
                device->SetRenderState(D3DRS_INDEXEDVERTEXBLENDENABLE, TRUE);

                // Set up bone matrices
                IMaterialVar* boneVar = material->FindVar("$numbones", nullptr);
                int numBones = boneVar ? boneVar->GetIntValue() : 0;
                FF_LOG("  Bone Count: %d", numBones);

                for (int i = 0; i < numBones && i < 96; i++) {
                    D3DMATRIX boneMatrix = {
                        1, 0, 0, 0,
                        0, 1, 0, 0,
                        0, 0, 1, 0,
                        0, 0, 0, 1
                    };
                    device->SetTransform(D3DTS_WORLDMATRIX(i), &boneMatrix);
                }
            }

            // Enhanced model lighting
            FF_LOG("  Setting up model lighting");
            device->SetRenderState(D3DRS_LIGHTING, TRUE);
            device->SetRenderState(D3DRS_SPECULARENABLE, TRUE);
            device->SetRenderState(D3DRS_AMBIENT, D3DCOLOR_ARGB(255, 128, 128, 128));
            device->SetRenderState(D3DRS_NORMALIZENORMALS, TRUE);

            // Set up model material
            D3DMATERIAL9 mtrl;
            ZeroMemory(&mtrl, sizeof(mtrl));
            mtrl.Diffuse = D3DXCOLOR(1.0f, 1.0f, 1.0f, 1.0f);
            mtrl.Ambient = D3DXCOLOR(0.5f, 0.5f, 0.5f, 1.0f);
            mtrl.Specular = D3DXCOLOR(0.2f, 0.2f, 0.2f, 1.0f);
            mtrl.Power = 8.0f;
            device->SetMaterial(&mtrl);

            // Set up model lighting
            D3DLIGHT9 light;
            ZeroMemory(&light, sizeof(light));
            light.Type = D3DLIGHT_DIRECTIONAL;
            light.Diffuse = D3DXCOLOR(1.0f, 1.0f, 1.0f, 1.0f);
            light.Ambient = D3DXCOLOR(0.3f, 0.3f, 0.3f, 1.0f);
            light.Direction = D3DXVECTOR3(0.0f, -1.0f, -1.0f);
            device->SetLight(0, &light);
            device->LightEnable(0, TRUE);
        }

        // Setup transforms
        SetupTransforms(device, material);

        // Basic render states
        device->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
        device->SetRenderState(D3DRS_CULLMODE, D3DCULL_CCW);
        device->SetRenderState(D3DRS_ZENABLE, D3DZB_TRUE);
        device->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
        device->SetRenderState(D3DRS_LIGHTING, TRUE);
        device->SetRenderState(D3DRS_SPECULARENABLE, FALSE);
        device->SetRenderState(D3DRS_AMBIENT, D3DCOLOR_ARGB(255, 128, 128, 128));

        // Set up basic material properties
        D3DMATERIAL9 mtrl;
        ZeroMemory(&mtrl, sizeof(mtrl));
        mtrl.Diffuse = D3DXCOLOR(1.0f, 1.0f, 1.0f, 1.0f);
        mtrl.Ambient = D3DXCOLOR(1.0f, 1.0f, 1.0f, 1.0f);
        device->SetMaterial(&mtrl);

        // Setup basic light
        device->SetRenderState(D3DRS_LIGHTING, TRUE);
        D3DLIGHT9 light;
        ZeroMemory(&light, sizeof(light));
        light.Type = D3DLIGHT_DIRECTIONAL;
        light.Diffuse = D3DXCOLOR(1.0f, 1.0f, 1.0f, 1.0f);
        light.Direction = D3DXVECTOR3(0.0f, 0.0f, -1.0f);
        device->SetLight(0, &light);
        device->LightEnable(0, TRUE);

        // Color and alpha blending setup
        device->SetRenderState(D3DRS_COLORVERTEX, TRUE);
        device->SetRenderState(D3DRS_DIFFUSEMATERIALSOURCE, D3DMCS_MATERIAL);
        device->SetRenderState(D3DRS_AMBIENTMATERIALSOURCE, D3DMCS_MATERIAL);

        // Setup texture
        IMaterialVar* baseTexture = material->FindVar("$basetexture", nullptr);
        if (baseTexture && !baseTexture->IsDefined()) {
            bool hasValidTexture = false;
            void* texHandle = nullptr;

            try {
                texHandle = baseTexture->GetTextureValue();
                hasValidTexture = (texHandle != nullptr);
            }
            catch (...) {
                FF_WARN("Exception getting texture for material: %s", material->GetName());
                hasValidTexture = false;
            }

            if (hasValidTexture) {
                IDirect3DBaseTexture9* d3dtex = static_cast<IDirect3DBaseTexture9*>(texHandle);
                if (d3dtex) {
                    device->SetTexture(0, d3dtex);
                    
                    // Set up texture stage states
                    device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
                    device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
                    device->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
                    device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
                    device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
                    device->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);

                    // Texture addressing and filtering
                    device->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP);
                    device->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP);
                    device->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
                    device->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
                    device->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR);
                }
            } else {
                // Handle materials without valid textures
                FF_LOG("No valid texture for material: %s, using fallback color", material->GetName());
                device->SetTexture(0, nullptr);
                device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG2);
                device->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
                device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG2);
                device->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);

                // Set white color for untextured materials
                D3DMATERIAL9 mtrl;
                ZeroMemory(&mtrl, sizeof(mtrl));
                mtrl.Diffuse = D3DXCOLOR(1.0f, 1.0f, 1.0f, 1.0f);
                mtrl.Ambient = D3DXCOLOR(1.0f, 1.0f, 1.0f, 1.0f);
                device->SetMaterial(&mtrl);
            }
        } else {
            // No texture var at all
            FF_LOG("Material %s has no base texture", material->GetName());
            device->SetTexture(0, nullptr);
            device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG2);
            device->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
        }

        // Disable unused texture stages
        device->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
        device->SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);

        // Setup alpha blending
        bool isTranslucent = material->IsTranslucent();
        device->SetRenderState(D3DRS_ALPHABLENDENABLE, isTranslucent ? TRUE : FALSE);
        if (isTranslucent) {
            device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
            device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
        }

        bool hasTexture = FindAndSetTexture(device, material);
        if (!hasTexture) {
            // Set fallback color/material
            D3DMATERIAL9 mtrl;
            ZeroMemory(&mtrl, sizeof(mtrl));
            mtrl.Diffuse = D3DXCOLOR(0.7f, 0.7f, 0.7f, 1.0f);  // Light grey
            mtrl.Ambient = D3DXCOLOR(0.3f, 0.3f, 0.3f, 1.0f);
            device->SetMaterial(&mtrl);

            // Set texture stages for color-only rendering
            device->SetTexture(0, nullptr);
            device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG2);
            device->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
            device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG2);
            device->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
        }
        
        // Disable fog for now (if it's causing issues)
        device->SetRenderState(D3DRS_FOGENABLE, FALSE);

        if (shouldLog) {
            FF_LOG("Fixed function pipeline setup complete for material: %s", material->GetName());
        }
    }
    catch (const std::exception& e) {
        FF_WARN("Exception in SetupFixedFunction: %s", e.what());
        // Try to restore to a safe state
        device->SetTexture(0, nullptr);
        device->SetVertexShader(nullptr);
        device->SetPixelShader(nullptr);
    }
}

DWORD FixedFunctionState::GetFVFFromSourceFormat(VertexFormat_t format) {
    DWORD fvf = D3DFVF_XYZ; // Position is always present

    FF_LOG("Converting format 0x%x to FVF", format);

    // Add normal if present
    if (format & FF_VERTEX_NORMAL) {
        fvf |= D3DFVF_NORMAL;
        FF_LOG("  Added normal");
    }

    // Add diffuse color if present
    if (format & FF_VERTEX_COLOR) {
        fvf |= D3DFVF_DIFFUSE;
        FF_LOG("  Added color");
    }

    // Add specular color if present
    if (format & FF_VERTEX_SPECULAR) {
        fvf |= D3DFVF_SPECULAR;
        FF_LOG("  Added specular");
    }

    // Handle bone weights for skinned meshes
    if (format & FF_VERTEX_BONES) {
        FF_LOG("  Adding bone weights");
        fvf &= ~D3DFVF_XYZRHW;  // Remove XYZRHW if present
        fvf |= D3DFVF_XYZB4;    // Add room for 4 blend weights
        fvf |= D3DFVF_LASTBETA_UBYTE4; // Specify blend indices format
    }

    // Handle texture coordinates
    int texCoordCount = 0;
    for (int i = 0; i < 8; i++) {
        if (format & (FF_VERTEX_TEXCOORD0 << i))
            texCoordCount++;
    }

    if (texCoordCount > 0) {
        fvf |= (texCoordCount << D3DFVF_TEXCOUNT_SHIFT);
        FF_LOG("  Added %d texture coordinates", texCoordCount);
    }

    FF_LOG("Final FVF: 0x%x", fvf);
    return fvf;
}

void FixedFunctionState::SetupTransforms(IDirect3DDevice9* device, IMaterial* material) {
    // Get transforms from Source's material system
    D3DMATRIX worldMatrix = {
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
        0, 0, 0, 1
    };
    D3DMATRIX viewMatrix = worldMatrix;
    D3DMATRIX projMatrix = worldMatrix;

    // Set transforms
    device->SetTransform(D3DTS_WORLD, &worldMatrix);
    device->SetTransform(D3DTS_VIEW, &viewMatrix);
    device->SetTransform(D3DTS_PROJECTION, &projMatrix);
}

void FixedFunctionState::SetupTextureStages(IDirect3DDevice9* device, IMaterial* material) {
    // Setup first texture stage
    device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
    device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
    device->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
    device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
    device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
    device->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);

    // Disable other texture stages
    for (DWORD i = 1; i < 8; i++) {
        device->SetTextureStageState(i, D3DTSS_COLOROP, D3DTOP_DISABLE);
        device->SetTextureStageState(i, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
    }
}

void FixedFunctionState::SetupRenderStates(IDirect3DDevice9* device, IMaterial* material) {
    // Basic render states
    device->SetRenderState(D3DRS_LIGHTING, FALSE);
    device->SetRenderState(D3DRS_CULLMODE, D3DCULL_CCW);
    device->SetRenderState(D3DRS_ZENABLE, D3DZB_TRUE);

    // Setup alpha blending based on material properties
    bool isTranslucent = material->IsTranslucent();
    device->SetRenderState(D3DRS_ALPHABLENDENABLE, isTranslucent ? TRUE : FALSE);
    if (isTranslucent) {
        device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
        device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
    }
}

void FixedFunctionState::StoreTextureStage(IDirect3DDevice9* device, DWORD stage) {
    TextureStageState state;
    device->GetTextureStageState(stage, D3DTSS_COLOROP, &state.colorOp);
    device->GetTextureStageState(stage, D3DTSS_COLORARG1, &state.colorArg1);
    device->GetTextureStageState(stage, D3DTSS_COLORARG2, &state.colorArg2);
    device->GetTextureStageState(stage, D3DTSS_ALPHAOP, &state.alphaOp);
    device->GetTextureStageState(stage, D3DTSS_ALPHAARG1, &state.alphaArg1);
    device->GetTextureStageState(stage, D3DTSS_ALPHAARG2, &state.alphaArg2);
    device->GetTextureStageState(stage, D3DTSS_TEXCOORDINDEX, &state.texCoordIndex);
    device->GetTextureStageState(stage, D3DTSS_TEXTURETRANSFORMFLAGS, &state.textureTransformFlags);
    m_state.textureStages.push_back(state);
}

void FixedFunctionState::RestoreTextureStage(
    IDirect3DDevice9* device, 
    DWORD stage, 
    const TextureStageState& state)
{
    device->SetTextureStageState(stage, D3DTSS_COLOROP, state.colorOp);
    device->SetTextureStageState(stage, D3DTSS_COLORARG1, state.colorArg1);
    device->SetTextureStageState(stage, D3DTSS_COLORARG2, state.colorArg2);
    device->SetTextureStageState(stage, D3DTSS_ALPHAOP, state.alphaOp);
    device->SetTextureStageState(stage, D3DTSS_ALPHAARG1, state.alphaArg1);
    device->SetTextureStageState(stage, D3DTSS_ALPHAARG2, state.alphaArg2);
    device->SetTextureStageState(stage, D3DTSS_TEXCOORDINDEX, state.texCoordIndex);
    device->SetTextureStageState(stage, D3DTSS_TEXTURETRANSFORMFLAGS, state.textureTransformFlags);
}