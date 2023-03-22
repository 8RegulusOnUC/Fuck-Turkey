#include <Windows.h>
#include <TlHelp32.h>
#include <cstdint>
#include <string_view>
#include <thread>
#include <iostream>

#include "memory.h"
#include "offsets.h"
#include "vector.h"

auto mem = Memory{ "csgo.exe" };
const auto client = mem.GetModuleAddress("client.dll");
const auto engine = mem.GetModuleAddress("engine.dll");
const auto localPlayer = mem.Read<uintptr_t>(client + offsets::dwLocalPlayer);

constexpr Vector3 CalculateAngle(
	const Vector3& localPosition,
	const Vector3& enemyPosition,
	const Vector3& viewAngles) noexcept
{
	return ((enemyPosition - localPosition).ToAngle() - viewAngles);
}

void glow() {
	const auto localPlayer = mem.Read<std::uintptr_t>(client + offsets::dwLocalPlayer);
	const auto glowObjectManager = mem.Read<std::uintptr_t>(client + offsets::dwGlowObjectManager);

	for (auto i = 0; i < 64; ++i) {
		const auto entity = mem.Read<std::uintptr_t>(client + offsets::dwEntityList + i * 0x10);

		if (mem.Read<std::uintptr_t>(entity + offsets::m_iTeamNum) == mem.Read<std::uintptr_t>(localPlayer + offsets::m_iTeamNum))
			continue;

		const auto glowIndex = mem.Read<std::int32_t>(entity + offsets::m_iGlowIndex);

		mem.Write<float>(glowObjectManager + (glowIndex * 0x38) + 0x8, 1.0f);
		mem.Write<float>(glowObjectManager + (glowIndex * 0x38) + 0xC, 0.0f);
		mem.Write<float>(glowObjectManager + (glowIndex * 0x38) + 0x10, 0.0f);
		mem.Write<float>(glowObjectManager + (glowIndex * 0x38) + 0x14, 1.0f);

		mem.Write<bool>(glowObjectManager + (glowIndex * 0x38) + 0x27, true);
		mem.Write<bool>(glowObjectManager + (glowIndex * 0x38) + 0x28, true);
	}
}

void noflash() {
	INT32 localPlayer = mem.Read<INT32>(client + offsets::dwLocalPlayer);
	if (true)
	{
		if (mem.Read<float>(localPlayer + offsets::m_flFlashMaxAlpha) > 0.0f)
		{
			mem.Write<float>(localPlayer + offsets::m_flFlashMaxAlpha, 0.0f);
		}
	}
	else if (mem.Read<float>(localPlayer + offsets::m_flFlashMaxAlpha) == 0.0f)
	{
		mem.Write<float>(localPlayer + offsets::m_flFlashMaxAlpha, 255.0f);
	}
}

void bhop() {
	const auto localPlayer = mem.Read<uintptr_t>(client + offsets::dwLocalPlayer);

	if (localPlayer)
	{
		const auto onGround = mem.Read<bool>(localPlayer + offsets::m_fFlags);

		if (GetAsyncKeyState(VK_SPACE) && onGround & (1 << 0))
			mem.Write<BYTE>(client + offsets::dwForceJump, 6);
	}
}

void aimbot() {
	while (true) {
		if (!GetAsyncKeyState(VK_XBUTTON2))
			continue;

		// get local player
		const auto localPlayer = mem.Read<std::uintptr_t>(client + offsets::dwLocalPlayer);
		const auto localTeam = mem.Read<std::int32_t>(localPlayer + offsets::m_iTeamNum);

		// eye position = origin + viewOffset
		const auto localEyePosition = mem.Read<Vector3>(localPlayer + offsets::m_vecOrigin) +
			mem.Read<Vector3>(localPlayer + offsets::m_vecViewOffset);

		const auto clientState = mem.Read<std::uintptr_t>(engine + offsets::dwClientState);

		const auto localPlayerId =
			mem.Read<std::int32_t>(clientState + offsets::dwClientState_GetLocalPlayer);

		const auto viewAngles = mem.Read<Vector3>(clientState + offsets::dwClientState_ViewAngles);
		const auto aimPunch = mem.Read<Vector3>(localPlayer + offsets::m_aimPunchAngle) * 2;

		// aimbot fov
		auto bestFov = 10.f;
		auto bestAngle = Vector3{ };

		for (auto i = 1; i <= 32; ++i)
		{
			const auto player = mem.Read<std::uintptr_t>(client + offsets::dwEntityList + i * 0x10);

			if (mem.Read<std::int32_t>(player + offsets::m_iTeamNum) == localTeam)
				continue;

			if (mem.Read<bool>(player + offsets::m_bDormant))
				continue;

			if (mem.Read<std::int32_t>(player + offsets::m_lifeState))
				continue;

			if (mem.Read<std::int32_t>(player + offsets::m_bSpottedByMask) & (1 << localPlayerId))
			{
				const auto boneMatrix = mem.Read<std::uintptr_t>(player + offsets::m_dwBoneMatrix);

				// pos of player head in 3d space
				// 8 is the head bone index :)
				const auto playerHeadPosition = Vector3{
					mem.Read<float>(boneMatrix + 0x30 * 8 + 0x0C),
					mem.Read<float>(boneMatrix + 0x30 * 8 + 0x1C),
					mem.Read<float>(boneMatrix + 0x30 * 8 + 0x2C)
				};

				const auto angle = CalculateAngle(
					localEyePosition,
					playerHeadPosition,
					viewAngles + aimPunch
				);

				const auto fov = std::hypot(angle.x, angle.y);

				if (fov < bestFov)
				{
					bestFov = fov;
					bestAngle = angle;
				}
			}
		}

		// if we have a best angle, do aimbot
		if (!bestAngle.IsZero())
			mem.Write<Vector3>(clientState + offsets::dwClientState_ViewAngles, viewAngles + bestAngle / 1.f); // smoothing
	}
}

int main() {
	std::cout << "Made by 8Regulus on @unknowncheats.me";
	
	std::thread t1(aimbot);
	t1;
	while (true) {
		bhop();
		noflash();
		glow();
	}

	return 0;
}