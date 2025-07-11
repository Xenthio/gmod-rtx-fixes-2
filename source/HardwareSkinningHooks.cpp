//#define HWSKIN_PATCHES

#ifdef HWSKIN_PATCHES

#include "HardwareSkinningHooks.h"
#include "e_utils.h"  
#include "cbase.h"
#include "optimize.h"
#include "TinyMathLib.h"

static IMaterialSystemHardwareConfig* g_pHardwareConfig;

static IDirect3DDevice9* m_pD3DDevice;

// Global variables for bone data handling
BoneData_t g_BONEDATA = { -1 };
static bool activate_holder = false;
static matrix3x4_t modelToWorld;

// Static variables for bone handling
static int bone_count = -1;

// Function pointer typedefs
typedef void* (__fastcall* F_StudioRenderFinal)(void*, void*, void*, void*, void*, void*, void*, void*, void*, void*, void*);
typedef void* (__fastcall* F_StudioBuildMeshGroup)(void*, void*, studiomeshgroup_t*, OptimizedModel::StripGroupHeader_t*, mstudiomesh_t*, studiohdr_t*);
typedef int(__fastcall* F_StudioDrawGroupHWSkin)(void*, void*, studiomeshgroup_t*, void*, void*);
typedef void* (__fastcall* F_malloc)(void*, void*, size_t);
typedef void(__fastcall* F_SetFixedFunctionStateSkinningMatrices)(void*, void*);

// Function pointers for hooks
F_StudioRenderFinal R_StudioRenderFinal = nullptr;
F_StudioBuildMeshGroup R_StudioBuildMeshGroup = nullptr;
F_StudioDrawGroupHWSkin R_StudioDrawGroupHWSkin = nullptr;
F_SetFixedFunctionStateSkinningMatrices CShaderAPIDX_SetFixedFunctionStateSkinningMatrices = nullptr;

// Type definition for method pointers, todo: work without maybe?
typedef void(__fastcall* srv_MethodSingleArg)(void*, void*, void*);
typedef void(__fastcall* srv_MethodDoubleArg)(void*, void*, void*, void*);
typedef void(__fastcall* srv_MethodTripleArg)(void*, void*, void*, void*, void*);
typedef void(__fastcall* srv_MethodQuadArg)(void*, void*, void*, void*, void*, void*);
typedef void* (__fastcall* srv_MethodSingleArgRet)(void*, void*);
typedef void* (__fastcall* srv_MethodQuadArgRet)(void*, void*, void*, void*, void*);

// Hook implementation for StudioDrawGroupHWSkin
Define_method_Hook(int, R_StudioDrawGroupHWSkin, void*, IMatRenderContext* pRenderContext, studiomeshgroup_t* pGroup, IMesh* pMesh, ColorMeshInfo_t* pColorMeshInfo)
{
    studiohdr_t* pStudioHdr = *(studiohdr_t**)((long int)_this + 44 * sizeof(void*));
    if (pStudioHdr->numbones == 1 || !activate_holder)
    {
        return R_StudioDrawGroupHWSkin_trampoline()(_this, pRenderContext, pGroup, pMesh, pColorMeshInfo);
    }

    int numberTrianglesRendered = 0;

    matrix3x4_t* m_pBoneToWorld = *(matrix3x4_t**)((intptr_t)_this + 39 * sizeof(void*));
    matrix3x4_t* m_PoseToWorld = *(matrix3x4_t**)((intptr_t)_this + 40 * sizeof(void*));
    int zero = 0;

    matrix3x4_t worldToModel;
    TinyMathLib_MatrixInvert(modelToWorld, worldToModel);
    matrix3x4_t temp_pBoneToWorld[512];
    for (int i = 0; i < pStudioHdr->numbones; i++)
    {
        mstudiobone_t* bdata = pStudioHdr->pBone(i);
        matrix3x4_t invert;
        TinyMathLib_MatrixInverseTR(bdata->poseToBone, invert); 
        TinyMathLib_ConcatTransforms(m_PoseToWorld[i], invert, temp_pBoneToWorld[i]);
    }

    matrix3x4_t new_BoneToWorld[512];
    for (int i = 0; i < pStudioHdr->numbones; i++)
    {
        TinyMathLib_ConcatTransforms(worldToModel, temp_pBoneToWorld[i], new_BoneToWorld[i]);
    }

    matrix3x4_t new_PoseToWorld[512];
    for (int i = 0; i < pStudioHdr->numbones; i++)
    {
        mstudiobone_t* bdata = pStudioHdr->pBone(i);
        TinyMathLib_ConcatTransforms(new_BoneToWorld[i], bdata->poseToBone, new_PoseToWorld[i]);
    }

    matrix3x4_t viewmat;
    pRenderContext->GetMatrix(MATERIAL_VIEW, &viewmat);
    if (pStudioHdr->numbones == 1)
    {
        pRenderContext->MatrixMode(MATERIAL_MODEL);
        pRenderContext->LoadMatrix(m_PoseToWorld[0]);
        pRenderContext->SetNumBoneWeights(0);
        g_BONEDATA.bone_count = -1;
    }
    else
    {
        for (int i = 0; i < pStudioHdr->numbones; i++)
        {
            TinyMathLib_ConcatTransforms(modelToWorld, new_PoseToWorld[i], new_PoseToWorld[i]);
        }
    }

    for (int i = 0; i < pGroup->m_NumStrips; i++)
    {
        OptimizedModel::StripHeader_t* pStrip = &pGroup->m_pStripData[i];

        if (pStudioHdr->numbones > 1)
        {
            pRenderContext->SetNumBoneWeights(pStrip->numBones);

            // Will only activate if there's bone state changes, otherwise behaves like a software skin
            g_BONEDATA.bone_count = pStrip->numBoneStateChanges;
            for (int j = 0; j < pStrip->numBoneStateChanges; j++)
            {
                OptimizedModel::BoneStateChangeHeader_t* m_StateChange = pStrip->pBoneStateChange(j);
                if (m_StateChange->newBoneID < 0)
                    break;

                TinyMathLib_MatrixCopy(new_PoseToWorld[m_StateChange->newBoneID], g_BONEDATA.bone_matrices[m_StateChange->newBoneID]);
            }
        }
        // Tristrip optimization? If yes, mat_tristrip, or mat_triangles
        int flags = pStrip->flags & 2 ? 3 : 2;

        pMesh->SetColorMesh(NULL,0);
        pMesh->SetPrimitiveType((MaterialPrimitiveType_t)flags);
        pMesh->Draw(pStrip->indexOffset, pStrip->numIndices);
        pMesh->SetColorMesh(NULL,0);

        // Increment Magic
        numberTrianglesRendered += pGroup->m_pUniqueTris[i];
    }
    g_BONEDATA.bone_count = -1;

    if (pStudioHdr->numbones != 1)
    {
        pRenderContext->MatrixMode(MATERIAL_VIEW);
        pRenderContext->LoadMatrix(viewmat);
    }

    pRenderContext->MatrixMode(MATERIAL_MODEL);
    pRenderContext->LoadIdentity();

    return numberTrianglesRendered;
}

Define_method_Hook(void*, R_StudioBuildMeshGroup, void*, const char* pModelName, bool bNeedsTangentSpace, studiomeshgroup_t* pMeshGroup,
    OptimizedModel::StripGroupHeader_t* pStripGroup, mstudiomesh_t* pMesh,
    studiohdr_t* pStudioHdr, VertexFormat_t vertexFormat)
{
	//Msg("Running R_StudioBuildMeshGroup\n");
    if (pStudioHdr->numbones > 0 && !(pStripGroup->flags & 0x02))
    {
        pMeshGroup->m_Flags |= 2u;
        bone_count = pStudioHdr->numbones;
    }
    else
    {
        bone_count = -1;
    }
    return R_StudioBuildMeshGroup_trampoline()(_this, pModelName, bNeedsTangentSpace, pMeshGroup, pStripGroup, pMesh, pStudioHdr, vertexFormat);
}

Define_method_Hook(void*, R_StudioRenderFinal, void*, IMatRenderContext* pRenderContext,
    int skin, int nBodyPartCount, void* pBodyPartInfo, IClientEntity* pClientEntity,
    IMaterial** ppMaterials, int* pMaterialFlags, int boneMask, int lod, ColorMeshInfo_t* pColorMeshes)
{
    matrix3x4_t m_rgflCoordinateFrame = *(matrix3x4_t*)((intptr_t)pClientEntity + 776);  // NOTE!!!!! Offset might need adjustment, this is just what was given to me as is right now.

    TinyMathLib_MatrixCopy(m_rgflCoordinateFrame, modelToWorld);

    activate_holder = true;
    void* ret = R_StudioRenderFinal_trampoline()(_this, pRenderContext, skin, nBodyPartCount, pBodyPartInfo, pClientEntity, ppMaterials, pMaterialFlags, boneMask, lod, pColorMeshes);
    activate_holder = false;

    return ret;
}

// I can't actually find SetFixedFunctionStateSkinningMatrices, only SetSkinningMatrices, so i'm just going to use that for now.
// (EDIT: i see it being called now) --BUT unfortunately, I can't actually see this being called at all at runtime???--
Define_method_Hook(void, SetFixedFunctionStateSkinningMatrices, void*)
{
	//Msg("Running SetFixedFunctionStateSkinningMatrices\n");
	int blend_count = g_pHardwareConfig->MaxBlendMatrices();
    if (blend_count < 1)
        return;

    // guesses, (206?), 196, 164, copilot reckons 140, 
    //IDirect3DDevice9* m_pD3DDevice = *(IDirect3DDevice9**)((intptr_t)_this + 164);  // NOTE!!!!! Offset might need adjustment, this is just what was given to me as is right now.

    if (g_BONEDATA.bone_count > 1)
        m_pD3DDevice->SetRenderState(D3DRS_INDEXEDVERTEXBLENDENABLE, TRUE);
    else
        m_pD3DDevice->SetRenderState(D3DRS_INDEXEDVERTEXBLENDENABLE, FALSE);

    for (int i = 0; i < g_BONEDATA.bone_count; i++)
    {
        VMatrix mat;
        TinyMathLib_MatrixCopy(g_BONEDATA.bone_matrices[i], mat);

        if (g_BONEDATA.bone_count != 1)
            TinyMathLib_MatrixTranspose(mat, mat);

        m_pD3DDevice->SetTransform(D3DTS_WORLDMATRIX(i), (D3DMATRIX*)&mat);
    }
}

void HardwareSkinningHooks::Initialize() {
    try {
        Msg("[Hardware Skinning] - Initializing...\n");

        // Get module handles
        auto studiorenderdll = GetModuleHandle("studiorender.dll");
        if (!studiorenderdll) {
            Msg("[Hardware Skinning] - studiorender.dll == NULL\n");
            return;
        }

        auto materialsystemdll = GetModuleHandle("materialsystem.dll");
        if (!materialsystemdll) {
            Msg("[Hardware Skinning] - materialsystem.dll == NULL\n");
            return;
        }

        auto shaderapidx9dll = GetModuleHandle("shaderapidx9.dll");
        if (!shaderapidx9dll) {
            Msg("[Hardware Skinning] - shaderapidx9.dll == NULL\n");
            return;
        }

        //m_pD3DDevice = static_cast<IDirect3DDevice9Ex*>(FindD3D9Device(shaderapidx9dll));

        // get g_pHardwareConfig global interface (MATERIALSYSTEM_HARDWARECONFIG_INTERFACE_VERSION)
#ifdef _WIN32
		Msg("[RTXF2 - Binary Module] - Loading materialsystem\n");
        HMODULE materialsystemLib = LoadLibraryA("materialsystem.dll");
        if (!materialsystemLib) {
            Warning("[RTXF2] - Failed to load materialsystem.dll: error code %d\n", GetLastError());
            return;
        }

        using CreateInterfaceFn = void* (*)(const char* pName, int* pReturnCode);
        CreateInterfaceFn createInterface = (CreateInterfaceFn)GetProcAddress(materialsystemLib, "CreateInterface");
        if (!createInterface) {
            Warning("[RTXF2] - Could not get CreateInterface from materialsystem.dll\n");
            return;
        }
        g_pHardwareConfig = (IMaterialSystemHardwareConfig*)createInterface(MATERIALSYSTEM_HARDWARECONFIG_INTERFACE_VERSION, nullptr);

		m_pD3DDevice = (IDirect3DDevice9*)FindD3D9Device();
#endif



		// Define signature patterns for the functions we need to hook, copilot wanted to autocomplete these so i let it lmao, Still have yet to actually find the signatures
#ifdef _WIN64
        static const char StudioDrawGroupHWSkin_sign[] = "44 0F B7 4B 10 48 8D 15 ?? ?? ?? ?? 44 8B 43 13";
        static const char StudioBuildMeshGroup_sign[] = "48 8D 15 ?? ?? ?? ?? E8 ?? ?? ?? ?? 48 8B 0D ?? ?? ?? ??";
        static const char StudioRenderFinal_sign[] = "48 89 5C 24 08 48 89 74 24 10 57 48 83 EC 20";
        static const char SetFixedFunctionStateSkinningMatrices_sign[] = "40 53 48 83 EC 20 48 8B 81";
#else   // the 32 bit ones have been done though.
        static const char StudioDrawGroupHWSkin_sign[] = "55 8B EC 83 EC 0C 53 8B 5D ? 56";
        static const char StudioBuildMeshGroup_sign[] = "55 8B EC 81 EC 00 02 00 00";
        static const char StudioRenderFinal_sign[] = "55 8B EC 83 EC 10 53 57";
        static const char SetFixedFunctionStateSkinningMatrices_sign[] = "55 8B EC 83 EC 48 53 8B D9"; // to find this, search for D3DXMatrixTranspose, it's actually SetSkinningMatrices and SetFixedFunctionStateSkinningMatrices is baked in i think
#endif

        // Scan for function addresses
        auto StudioDrawGroupHWSkin_addr = ScanSign(studiorenderdll, StudioDrawGroupHWSkin_sign, sizeof(StudioDrawGroupHWSkin_sign) - 1);
        Msg("[Hardware Skinning] - StudioDrawGroupHWSkin address: %p\n", StudioDrawGroupHWSkin_addr);
        auto StudioBuildMeshGroup_addr = ScanSign(studiorenderdll, StudioBuildMeshGroup_sign, sizeof(StudioBuildMeshGroup_sign) - 1);
        Msg("[Hardware Skinning] - StudioBuildMeshGroup address: %p\n", StudioBuildMeshGroup_addr);
        auto StudioRenderFinal_addr = ScanSign(studiorenderdll, StudioRenderFinal_sign, sizeof(StudioRenderFinal_sign) - 1);
        Msg("[Hardware Skinning] - StudioRenderFinal address: %p\n", StudioRenderFinal_addr);
        auto SetFixedFunctionStateSkinningMatrices_addr = ScanSign(shaderapidx9dll, SetFixedFunctionStateSkinningMatrices_sign, sizeof(SetFixedFunctionStateSkinningMatrices_sign) - 1);
        Msg("[Hardware Skinning] - SetFixedFunctionStateSkinningMatrices address: %p\n", SetFixedFunctionStateSkinningMatrices_addr);

        modelToWorld.SetToIdentity();

        // Check if all addresses were found
        if (!StudioDrawGroupHWSkin_addr) {
            Msg("[Hardware Skinning] - StudioDrawGroupHWSkin == NULL\n");
            return;
        }
        if (!StudioBuildMeshGroup_addr) {
            Msg("[Hardware Skinning] - StudioBuildMeshGroup == NULL\n");
            return;
        }
        if (!StudioRenderFinal_addr) {
            Msg("[Hardware Skinning] - StudioRenderFinal == NULL\n");
            return;
        }
        if (!SetFixedFunctionStateSkinningMatrices_addr) {
            Msg("[Hardware Skinning] - SetFixedFunctionStateSkinningMatrices == NULL\n");
            // This one isn't critical, so we'll continue
        }

        // Set up hooks
        R_StudioDrawGroupHWSkin = (F_StudioDrawGroupHWSkin)StudioDrawGroupHWSkin_addr;
        R_StudioBuildMeshGroup = (F_StudioBuildMeshGroup)StudioBuildMeshGroup_addr;
        R_StudioRenderFinal = (F_StudioRenderFinal)StudioRenderFinal_addr;
        CShaderAPIDX_SetFixedFunctionStateSkinningMatrices =
            (F_SetFixedFunctionStateSkinningMatrices)SetFixedFunctionStateSkinningMatrices_addr;

        // Create and enable the hooks
        Setup_Hook(R_StudioDrawGroupHWSkin, StudioDrawGroupHWSkin_addr);
        Setup_Hook(R_StudioBuildMeshGroup, StudioBuildMeshGroup_addr);
        Setup_Hook(R_StudioRenderFinal, StudioRenderFinal_addr);

        if (SetFixedFunctionStateSkinningMatrices_addr) {
            Setup_Hook(SetFixedFunctionStateSkinningMatrices, SetFixedFunctionStateSkinningMatrices_addr);
        }

        Msg("[Hardware Skinning] - Successfully initialized all hooks\n");
    }
    catch (...) {
        Msg("[Hardware Skinning] - Exception in HardwareSkinningHooks::Initialize\n");
    }
}

void HardwareSkinningHooks::Shutdown() {
    try {
        // Disable all hooks
        m_StudioDrawGroupHWSkin_hook.Disable();
        m_StudioBuildMeshGroup_hook.Disable();
        m_StudioRenderFinal_hook.Disable();
        m_SetFixedFunctionStateSkinningMatrices_hook.Disable();

        // Reset global state
        g_BONEDATA.bone_count = -1;
        activate_holder = false;

        // Log shutdown completion
        Msg("[Hardware Skinning] - Shutdown complete\n");
    }
    catch (...) {
        Msg("[Hardware Skinning] - Exception in HardwareSkinningHooks::Shutdown\n");
    }
}
#endif