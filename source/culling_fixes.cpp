#include "GarrysMod/Lua/Interface.h"
#include "e_utils.h"  
#include "iclientrenderable.h"
#include "materialsystem/imaterialsystem.h"
#include "materialsystem/materialsystem_config.h"
#include "interfaces/interfaces.h"  
#include "culling_fixes.h"  
#include "cdll_client_int.h"
#include "view.h"
#include "cbase.h"
#include "viewrender.h"
#include "globalconvars.h"

#include <Windows.h>
#include <vector>
#include <string>
#include <unordered_map>
#include <Psapi.h>


using namespace GarrysMod::Lua; 

Define_method_Hook(void, CViewRenderRender, CViewRender*, vrect_t* rect)
{
	// crashma
	//view->DisableVis();
	//CViewRender::GetMainView()->DisableVis();
	CViewRenderRender_trampoline()(_this, rect); 
	return;
}

Define_method_Hook(bool, CViewRenderShouldForceNoVis, void*)
{   
	bool original = CViewRenderShouldForceNoVis_trampoline()(_this);
	if (GlobalConvars::r_forcenovis && GlobalConvars::r_forcenovis->GetBool()) {
		//Msg("[Culling Fixes] Hi\n");
		return true;
	}
	return original;
}

Define_method_Hook(bool, MathLibR_CullBoxSkipNear_ENGINE, void*, const Vector& mins, const Vector& maxs, const Frustum_t& frustum)
{
	if (GlobalConvars::c_frustumcull && GlobalConvars::c_frustumcull->GetBool()) {
		return MathLibR_CullBoxSkipNear_ENGINE_trampoline()(_this, mins, maxs, frustum);
	}
	return false;
}

Define_method_Hook(bool, MathLibR_CullBoxSkipNear_CLIENT, void*, const Vector& mins, const Vector& maxs, const Frustum_t& frustum)
{
	if (GlobalConvars::c_frustumcull && GlobalConvars::c_frustumcull->GetBool()) {
		return MathLibR_CullBoxSkipNear_CLIENT_trampoline()(_this, mins, maxs, frustum);
	}
	return false;
}


//Define_method_Hook(bool, MathLibR_CullNode_ENGINE, void*, Frustum_t *pAreaFrustum, void *pNode, int& nClipMask)
//{
//	if (GlobalConvars::c_frustumcull && GlobalConvars::c_frustumcull->GetBool()) {
//		return MathLibR_CullNode_ENGINE_trampoline()(_this, pAreaFrustum, pNode, nClipMask);
//	}
//	return false;
//}

Define_method_Hook(bool, MathLibR_CullBox_ENGINE, void*, const Vector& mins, const Vector& maxs, const Frustum_t& frustum)
{
	if (GlobalConvars::c_frustumcull && GlobalConvars::c_frustumcull->GetBool()) {
		return MathLibR_CullBox_ENGINE_trampoline()(_this, mins, maxs, frustum);
	}
	return false; 
}

Define_method_Hook(bool, MathLibR_CullBox_CLIENT, void*, const Vector& mins, const Vector& maxs, const Frustum_t& frustum)
{
	if (GlobalConvars::c_frustumcull && GlobalConvars::c_frustumcull->GetBool()) {
		return MathLibR_CullBox_CLIENT_trampoline()(_this, mins, maxs, frustum);
	}
	return false;
}

Define_method_Hook(bool, CM_BoxVisible_ENGINE, void*, const Vector& mins, const Vector& maxs, const byte* visbits, int vissize)
{
	if (GlobalConvars::c_frustumcull && GlobalConvars::c_frustumcull->GetBool()) {
		return CM_BoxVisible_ENGINE_trampoline()(_this, mins, maxs, visbits, vissize);
	}
	return true;
}



//Define_method_Hook(bool, EngineR_BuildWorldLists, void*, void* pRenderListIn, WorldListInfo_t* pInfo, int iForceViewLeaf, const VisOverrideData_t* pVisData, bool bShadowDepth /* = false */, float* pWaterReflectionHeight)
//{
//	return EngineR_BuildWorldLists_trampoline()(_this, pRenderListIn, pInfo, iForceViewLeaf, pVisData, true, pWaterReflectionHeight);
//}


#include "GarrysMod/InterfacePointers.hpp"

static StudioRenderConfig_t s_StudioRenderConfig;
 
// Original function pointer to the NoCull variant
typedef void (*R_RecursiveWorldNodeNoCull_t)(void* _this, void* pRenderList, void* node, int nCullMask);
R_RecursiveWorldNodeNoCull_t original_R_RecursiveWorldNodeNoCull = nullptr;

// Our hook for the normal R_RecursiveWorldNode
Define_method_Hook(void, R_RecursiveWorldNode, void*, void* pRenderList, void* node, int nCullMask)
{
    if (GlobalConvars::r_worldnodenocull && !GlobalConvars::r_worldnodenocull->GetBool()) {
        R_RecursiveWorldNode_trampoline()(_this, pRenderList, node, nCullMask);
    }
    // Just call the NoCull version instead
    if (original_R_RecursiveWorldNodeNoCull) {
        original_R_RecursiveWorldNodeNoCull(_this, pRenderList, node, nCullMask);
    }
    else {
        // Fall back to original if we somehow don't have the NoCull function
        R_RecursiveWorldNode_trampoline()(_this, pRenderList, node, nCullMask);
    }
}

// Initialize the hook
void SetupWorldNodeHook() {
    try {
        // Find the engine module
        auto engine = GetModuleHandle("engine.dll");

        // Find R_RecursiveWorldNode using signature
        // 32 bit sig: "55 8B EC 83 EC 08 56 8B 75 ?? 8B 0E"
        static const char recursiveWorldNodeSig[] = "48 8B C4 56 41 55";
        auto worldNodeFunc = ScanSign(engine, recursiveWorldNodeSig, sizeof(recursiveWorldNodeSig) - 1);

        if (!worldNodeFunc) {
            Warning("[Culling Fixes] Failed to find R_RecursiveWorldNode\n");
            return;
        }

        // Find R_RecursiveWorldNodeNoCull using signature
        // 32 bit sig: "55 8B EC 83 EC 08 53 8B 5D ?? 8B 0B"
        static const char recursiveWorldNodeNoCullSig[] = "48 89 5C 24 ? 55 48 83 EC 40 48 8B DA 48 8B E9 8B 12";
        auto worldNodeNoCullFunc = ScanSign(engine, recursiveWorldNodeNoCullSig, sizeof(recursiveWorldNodeNoCullSig) - 1);

        if (!worldNodeNoCullFunc) {
            Warning("[Culling Fixes] Failed to find R_RecursiveWorldNodeNoCull\n");
            return;
        }

        // Store the NoCull function pointer for use in our hook
        original_R_RecursiveWorldNodeNoCull = (R_RecursiveWorldNodeNoCull_t)worldNodeNoCullFunc;

        // Set up the hook to replace R_RecursiveWorldNode
        Setup_Hook(R_RecursiveWorldNode, worldNodeFunc);

        Msg("[Culling Fixes] Successfully hooked R_RecursiveWorldNode to use NoCull version\n");
    }
    catch (...) {
        Msg("[Culling Fixes] Exception in SetupWorldNodeHook\n");
    }
}
#include "memory_patcher.h"
void CullingHooks::Initialize() {
	try {
		auto client = GetModuleHandle("client.dll");
		auto engine = GetModuleHandle("engine.dll");
		auto server = GetModuleHandle("server.dll");
		if (!client) { Msg("client.dll == NULL\n"); }
		if (!engine) { Msg("engine.dll == NULL\n"); }
		if (!server) { Msg("server.dll == NULL\n"); }

		//static const char sign1[] = "44 8B B1 ? ? ? ? 48 89 55";
		//auto CViewRenderRender = ScanSign(client, sign1, sizeof(sign1) - 1);
		//if (!CViewRenderRender) { Msg("[Culling Fixes] CViewRender::Render == NULL\n"); }
		//else {
		//	Msg("[Culling Fixes] Hooked CViewRender::Render\n");
		//	Setup_Hook(CViewRenderRender, CViewRenderRender)
		//}

		// I cant find the signature, here's some possibilities??????????
		// 8B 81 ?? ?? ?? ?? C3 CC CC CC CC CC CC CC CC CC 8B 81 ?? ?? ?? ?? C3 CC CC CC CC CC CC CC CC CC 85 D2 75
		// 0F B6 81 ?? ?? ?? ?? C3 CC CC CC CC CC CC CC CC 8B 81 ?? ?? ?? ?? C3 CC CC CC CC CC CC CC CC CC 48 8B 81
		// 48 8D 81 4C 03 00 00 C3
		// 0F B6 81 54 03 00 00 C3
		// EDIT: its the 2nd/4th one, 2nd doesn't work, 4th works but will change with updates :(

		static const char sign2[] = "0F B6 81 54";
		auto CViewRenderShouldForceNoVis = ScanSign(client, sign2, sizeof(sign2) - 1);
		if (!CViewRenderShouldForceNoVis) { Msg("[Culling Fixes] CViewRender::ShouldForceNoVis == NULL\n"); }
		else {
			Msg("[Culling Fixes] Hooked CViewRender::ShouldForceNoVis\n");
			Setup_Hook(CViewRenderShouldForceNoVis, CViewRenderShouldForceNoVis)
		}

		//static const char R_CullNode_sign[] = "48 83 EC 48 80 3D ? ? ? ? ? 48 8B D1";

		static const char R_CullBox_sign[] = "48 83 EC 48 0F 10 22 33 C0";

		static const char CM_BoxVisible_sign[] = "48 89 5C 24 ? 48 89 6C 24 ? 56 57 41 56 48 81 EC ? ? ? ? 48 8B 05 ? ? ? ? 48 33 C4 48 89 84 24 ? ? ? ? F3 0F 10 22";

		//R_CullBoxSkipNear doesn't exist on chromium/x86-64, see https://commits.facepunch.com/202379
		//static const char R_CullBoxSkipNear_sign[] = "48 83 EC 48 0F 10 22 33 C0"; 

		auto CLIENT_R_CullBox = ScanSign(client, R_CullBox_sign, sizeof(R_CullBox_sign) - 1);
		auto ENGINE_R_CullBox = ScanSign(engine, R_CullBox_sign, sizeof(R_CullBox_sign) - 1);
		auto ENGINE_R_CullNode = ScanSign(engine, R_CullBox_sign, sizeof(R_CullBox_sign) - 1);

		auto ENGINE_CM_BoxVisible = ScanSign(engine, CM_BoxVisible_sign, sizeof(CM_BoxVisible_sign) - 1);

		if (!CLIENT_R_CullBox) { Msg("[Culling Fixes] MathLib (CLIENT) R_CullBox == NULL\n"); }
		else { Msg("[Culling Fixes] Hooked MathLib (CLIENT) R_CullBox\n"); Setup_Hook(MathLibR_CullBox_CLIENT, CLIENT_R_CullBox) }

		if (!ENGINE_R_CullBox) { Msg("[Culling Fixes] MathLib (ENGINE) R_CullBox == NULL\n"); }
		else { Msg("[Culling Fixes] Hooked MathLib (ENGINE) R_CullBox\n"); Setup_Hook(MathLibR_CullBox_ENGINE, ENGINE_R_CullBox) }


		//if (!CLIENT_R_CullBox) { Msg("[Culling Fixes] MathLib (ENGINE) R_CullNode == NULL\n"); }
		//else { Msg("[Culling Fixes] Hooked MathLib (ENGINE) R_CullNode\n"); Setup_Hook(MathLibR_CullNode_ENGINE, ENGINE_R_CullNode) }


        SetupWorldNodeHook();
        // Get the ICvar interface
   //     g_pCVar = (ICvar*)InterfacePointers::Cvar();
   //     if (g_pCVar) {
			//g_pCVar->RegisterConCommand(&rtx_check_patches);
   //     }

		if (!ENGINE_CM_BoxVisible) { Msg("[Culling Fixes] MathLib (ENGINE) CM_BoxVisible == NULL\n"); }
		else { Msg("[Culling Fixes] Hooked MathLib (ENGINE) CM_BoxVisible\n"); Setup_Hook(CM_BoxVisible_ENGINE, ENGINE_CM_BoxVisible) }

		//if (!SERVER_R_CullBox) { Msg("[Culling Fixes] MathLib (ENGINE) R_CullBox == NULL\n"); }
		//else { Msg("[Culling Fixes] Hooked MathLib (SERVER) R_CullBox\n"); Setup_Hook(MathLibR_CullBox_SERVER, SERVER_R_CullBox) }

		//static const char sign_RBuildWorldLists[] = "40 53 55 56 57 41 54 41 57 48 83 EC 78";
		//static const char sign_RRecursiveWorldNodeNoCull[] = "48 89 5C 24 ? 55 48 83 EC 40 48 8B DA 48 8B E9 8B 12";
		//static const char sign_RRecursiveWorldNode[] = "48 8B C4 56 41 55";

		//auto pointer_RBuildWorldLists = ScanSign(engine, sign_RBuildWorldLists, sizeof(sign_RBuildWorldLists) - 1);
		//auto pointer_RRecursiveWorldNodeNoCull = ScanSign(engine, sign_RRecursiveWorldNodeNoCull, sizeof(sign_RRecursiveWorldNodeNoCull) - 1);
		//auto pointer_RRecursiveWorldNode = ScanSign(engine, sign_RRecursiveWorldNode, sizeof(sign_RRecursiveWorldNode) - 1);


		//if (!pointer_RBuildWorldLists) { Msg("[Culling Fixes] MathLib engine R_BuildWorldLists == NULL\n"); }
		//else { Msg("[Culling Fixes] Hooked engine R_BuildWorldLists\n"); Setup_Hook(EngineR_BuildWorldLists, pointer_RBuildWorldLists) }

		HMODULE engineModule = GetModuleHandleEx("engine.dll");
		// Apply binary patches
        // credit: https://github.com/BlueAmulet/SourceRTXTweaks/blob/main/applypatch.py
		// Brush entity backfaces
		g_MemoryPatcher.FindAndPatch(
			"BrushEntityBackfaces",
			engineModule,
			"75 3C F3 0F 10",  // Pattern to search for
			"EB",              // Replace with unconditional jump (always drawn)
			"Disable brush entity backface culling"
		);

		// World backfaces patch 1
		g_MemoryPatcher.FindAndPatch(
			"WorldBackfaces1",
			engineModule,
			"7E 52 44",
			"EB",
			"Disable world backface culling (1/2)"
		);

		// World backfaces patch 2
		g_MemoryPatcher.FindAndPatch(
			"WorldBackfaces2",
			engineModule,
			"75 3C 49 8B 42 04",
			"EB",
			"Disable world backface culling (2/2)"
		);

		//ApplyPatches("client.dll", clientPatches); // we do these as hooks instead.
		//ApplyPatches("shaderapidx9.dll", shaderPatches); // these don't apply properly.
		//ApplyPatches("materialsystem.dll", materialPatches); // these also don't apply properly.

	}
	catch (...) {
		Msg("[Culling Fixes] Exception in CullingHooks::Initialize\n");
	}   
}

void CullingHooks::Shutdown() {
	// Existing shutdown code  
	//CViewRenderRender_hook.Disable();
	CViewRenderShouldForceNoVis_hook.Disable();
	MathLibR_CullBox_ENGINE_hook.Disable();
	MathLibR_CullBox_CLIENT_hook.Disable();
	MathLibR_CullBoxSkipNear_ENGINE_hook.Disable();
	MathLibR_CullBoxSkipNear_CLIENT_hook.Disable();
	//MathLibR_CullNode_ENGINE_hook.Disable();
    CM_BoxVisible_ENGINE_hook.Disable();


    R_RecursiveWorldNode_hook.Disable();


    // Clear the list
	g_MemoryPatcher.DisablePatch("BrushEntityBackfaces");
	g_MemoryPatcher.DisablePatch("WorldBackfaces1");
	g_MemoryPatcher.DisablePatch("WorldBackfaces2");
	//CM_BoxVisible_ENGINE_hook.Disable();
	//EngineR_BuildWorldLists_hook.Disable();

	// Log shutdown completion
	Msg("[Culling Fixes] Shutdown complete\n");
}
