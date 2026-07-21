//Copyright(C) 2026 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#include "log_utils.hpp"

#include "resources/kg_texture.hpp"

using KalaHeaders::KalaLog::Log;
using KalaHeaders::KalaLog::LogType;

using std::to_string;

namespace KalaGraphics::Resources
{
    static KalaGraphicsRegistry<Texture> registry{};

    KalaGraphicsRegistry<Texture>& Texture::GetRegistry() { return registry; }

    Texture* Texture::Initialize()
    {
        /*TODO: fill*/

        return nullptr;
    }

    u32 Texture::GetID() const { return ID; }

    void Texture::Destroy()
    {
        /*TODO: fill*/
    }

    Texture::~Texture()
    {
        Log::Print(
            "Destroying texture '" + to_string(ID) + "'.",
            "KG_TEXTURE",
            LogType::LOG_INFO);

        /*TODO: fill*/
    }
}