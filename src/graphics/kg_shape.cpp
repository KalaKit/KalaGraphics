//Copyright(C) 2026 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#include <memory>

#include "log_utils.hpp"

#include "graphics/kg_shape.hpp"
#include "core/kg_core.hpp"

using KalaGraphics::Core::MAX_NAME_LENGTH;
using KalaGraphics::Core::KalaGraphicsCore;

using KalaHeaders::KalaLog::Log;
using KalaHeaders::KalaLog::LogType;

using std::to_string;
using std::unique_ptr;
using std::make_unique;

namespace KalaGraphics::Graphics
{
    static KalaGraphicsRegistry<Shape> registry{};

    KalaGraphicsRegistry<Shape>& Shape::GetRegistry() { return registry; }

    Shape* Shape::Initialize(
        string&& name,
        u32 contextID,
        vector<Material>&& materials,
        Transform&& transform)
    {
        unique_ptr<Shape> newShape = make_unique<Shape>();
        Shape* shapePtr = newShape.get();

        u32 newID = KalaGraphicsCore::GetGlobalID() + 1;
        KalaGraphicsCore::SetGlobalID(newID);
        shapePtr->ID = newID;

        registry.AddContent(newID, std::move(newShape));

        Log::Print(
			"Created new mesh " + to_string(newID) + "'!",
			"KG_MESH",
			LogType::LOG_SUCCESS);

        return nullptr;
    }

    u32 Shape::GetID() const { return ID; }
    const vector<Material>& Shape::GetMaterials() const { return materials; }

    u32 Shape::GetContextID() const { return contextID; }
    void Shape::SetContextID(u32 newValue) { /*TODO: fill*/ }

    const string& Shape::GetName() const { return name; }
    void Shape::SetName(string_view newName)
    {
        if (newName.empty())
        {
            Log::Print(
                "Failed to set new shape name because it was empty!",
                "KG_SHAPE",
                LogType::LOG_ERROR,
                2);

            return;
        }
        if (newName.size() > MAX_NAME_LENGTH)
        {
            Log::Print(
                "Failed to set new shape name because it was too long!",
                "KG_SHAPE",
                LogType::LOG_ERROR,
                2);

            return;
        }

        name = newName;
    }

    bool Shape::IsEnabled() const { return isEnabled; }
    void Shape::SetEnabledstate(bool state) { isEnabled = state; }

    bool Shape::IsMaterialEnabled(u32 materialSlot) const { return false; }
    void Shape::SetMaterialEnabledState(u32 materialSlot) { }

    void Shape::Destroy()
    {
        registry.RemoveContent(ID);
    }

    Shape::~Shape()
    {
        Log::Print(
            "Destroying shape '" + to_string(ID) + "'.",
            "KG_SHAPE",
            LogType::LOG_INFO);

        /*TODO: fill*/
    }
}