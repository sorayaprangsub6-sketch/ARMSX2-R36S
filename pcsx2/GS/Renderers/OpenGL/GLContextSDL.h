// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "GS/Renderers/OpenGL/GLContext.h"

#include <SDL3/SDL.h>

#include <span>

class GLContextSDL final : public GLContext
{
public:
	GLContextSDL(const WindowInfo& wi);
	~GLContextSDL() override;

	static std::unique_ptr<GLContext> Create(const WindowInfo& wi,
		std::span<const Version> versions_to_try, Error* error);

	void* GetProcAddress(const char* name) override;
	bool ChangeSurface(const WindowInfo& new_wi) override;
	void ResizeSurface(u32 new_surface_width = 0, u32 new_surface_height = 0) override;
	bool SwapBuffers() override;
	bool IsCurrent() override;
	bool MakeCurrent() override;
	bool DoneCurrent() override;
	bool SupportsNegativeSwapInterval() const override;
	bool SetSwapInterval(s32 interval) override;
	std::unique_ptr<GLContext> CreateSharedContext(const WindowInfo& wi, Error* error) override;

private:
	SDL_Window* GetSDLWindow() const
	{
		return static_cast<SDL_Window*>(m_wi.window_handle);
	}

	bool Initialize(std::span<const Version> versions_to_try, Error* error);
	bool CreateVersionContext(const Version& version, SDL_GLContext share_context,
		bool make_current, Error* error);

	SDL_GLContext m_context = nullptr;
};