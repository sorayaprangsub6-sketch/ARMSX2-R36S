// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "GS/Renderers/OpenGL/GLContextSDL.h"

#include "common/Console.h"
#include "common/Error.h"

GLContextSDL::GLContextSDL(const WindowInfo& wi)
	: GLContext(wi)
{
}

GLContextSDL::~GLContextSDL()
{
	if (m_abandoned)
	{
		m_context = nullptr;
		return;
	}

	if (m_context)
	{
		if (SDL_GL_GetCurrentContext() == m_context)
			SDL_GL_MakeCurrent(GetSDLWindow(), nullptr);

		SDL_GL_DestroyContext(m_context);
		m_context = nullptr;
	}
}

std::unique_ptr<GLContext> GLContextSDL::Create(const WindowInfo& wi,
	std::span<const Version> versions_to_try, Error* error)
{
	std::unique_ptr<GLContextSDL> context = std::make_unique<GLContextSDL>(wi);
	if (!context->Initialize(versions_to_try, error))
		return nullptr;

	return context;
}

bool GLContextSDL::Initialize(std::span<const Version> versions_to_try, Error* error)
{
	if (m_wi.type != WindowInfo::Type::SDL || !GetSDLWindow())
	{
		Error::SetStringView(error, "GLContextSDL requires an SDL window.");
		return false;
	}

	for (const Version& cv : versions_to_try)
	{
		if (CreateVersionContext(cv, nullptr, true, error))
		{
			m_version = cv;
			return true;
		}
	}

	Error::SetStringView(error, "Failed to create any SDL OpenGL contexts.");
	return false;
}

bool GLContextSDL::CreateVersionContext(const Version& version, SDL_GLContext share_context,
	bool make_current, Error* error)
{
	if (m_context)
	{
		if (SDL_GL_GetCurrentContext() == m_context)
			SDL_GL_MakeCurrent(GetSDLWindow(), nullptr);

		SDL_GL_DestroyContext(m_context);
		m_context = nullptr;
	}

	int profile;
	switch (version.profile)
	{
		case Profile::Core:
			profile = SDL_GL_CONTEXT_PROFILE_CORE;
			break;

		case Profile::ES:
			profile = SDL_GL_CONTEXT_PROFILE_ES;
			break;

		default:
			return false;
	}

	if (!SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, profile) ||
		!SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, version.major_version) ||
		!SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, version.minor_version))
	{
		Error::SetStringFmt(error, "SDL_GL_SetAttribute() failed: {}", SDL_GetError());
		return false;
	}

	if (!SDL_GL_SetAttribute(SDL_GL_SHARE_WITH_CURRENT_CONTEXT, share_context ? 1 : 0))
	{
		Error::SetStringFmt(error, "SDL_GL_SetAttribute(SHARE_WITH_CURRENT_CONTEXT) failed: {}", SDL_GetError());
		return false;
	}

	SDL_GLContext previous_context = nullptr;
	SDL_Window* previous_window = nullptr;

	if (share_context)
	{
		previous_context = SDL_GL_GetCurrentContext();
		previous_window = SDL_GL_GetCurrentWindow();

		if (previous_context != share_context)
		{
			Error::SetStringView(error, "SDL shared context is not current.");
			return false;
		}
	}

	SDL_GLContext new_context = SDL_GL_CreateContext(GetSDLWindow());
	if (!new_context)
	{
		if (share_context && previous_context)
			SDL_GL_MakeCurrent(previous_window, previous_context);

		return false;
	}

	if (!make_current)
	{
		if (!SDL_GL_MakeCurrent(GetSDLWindow(), nullptr))
		{
			SDL_GL_DestroyContext(new_context);
			return false;
		}

		if (previous_context && previous_window)
			SDL_GL_MakeCurrent(previous_window, previous_context);
	}

	m_context = new_context;
	return true;
}

void* GLContextSDL::GetProcAddress(const char* name)
{
	const SDL_FunctionPointer proc = SDL_GL_GetProcAddress(name);
	return reinterpret_cast<void*>(proc);
}

bool GLContextSDL::ChangeSurface(const WindowInfo& new_wi)
{
	if (new_wi.type != WindowInfo::Type::SDL || !new_wi.window_handle)
		return false;

	const bool was_current = IsCurrent();

	if (was_current)
		SDL_GL_MakeCurrent(GetSDLWindow(), nullptr);

	m_wi = new_wi;

	if (was_current)
		return MakeCurrent();

	return true;
}

void GLContextSDL::ResizeSurface(u32 new_surface_width, u32 new_surface_height)
{
	if (new_surface_width != 0 && new_surface_height != 0)
	{
		m_wi.surface_width = new_surface_width;
		m_wi.surface_height = new_surface_height;
		return;
	}

	int width = 0;
	int height = 0;

	if (SDL_GetWindowSizeInPixels(GetSDLWindow(), &width, &height))
	{
		m_wi.surface_width = static_cast<u32>(width);
		m_wi.surface_height = static_cast<u32>(height);
	}
}

bool GLContextSDL::SwapBuffers()
{
	if (m_abandoned || !GetSDLWindow())
		return false;

	return SDL_GL_SwapWindow(GetSDLWindow());
}

bool GLContextSDL::IsCurrent()
{
	return (m_context && SDL_GL_GetCurrentContext() == m_context);
}

bool GLContextSDL::MakeCurrent()
{
	if (m_abandoned || !m_context || !GetSDLWindow())
		return false;

	return SDL_GL_MakeCurrent(GetSDLWindow(), m_context);
}

bool GLContextSDL::DoneCurrent()
{
	if (m_abandoned)
		return true;

	return SDL_GL_MakeCurrent(GetSDLWindow(), nullptr);
}

bool GLContextSDL::SupportsNegativeSwapInterval() const
{
	return false;
}

bool GLContextSDL::SetSwapInterval(s32 interval)
{
	return SDL_GL_SetSwapInterval(interval);
}

std::unique_ptr<GLContext> GLContextSDL::CreateSharedContext(const WindowInfo& wi, Error* error)
{
	if (!IsCurrent())
	{
		Error::SetStringView(error, "SDL main OpenGL context must be current before creating a shared context.");
		return nullptr;
	}

	std::unique_ptr<GLContextSDL> context = std::make_unique<GLContextSDL>(wi);

	if (!context->CreateVersionContext(m_version, m_context, false, error))
		return nullptr;

	context->m_version = m_version;
	return context;
}