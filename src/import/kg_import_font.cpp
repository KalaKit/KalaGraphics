//Copyright(C) 2026 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#include <memory>

#include "hb.h"
#include "hb-ot.h"
#include "hb-raster.h"

#include "log_utils.hpp"
#include "file_utils.hpp"

#include "import/kg_import_font.hpp"
#include "core/kg_core.hpp"

using KalaHeaders::KalaCore::ContainsValue;

using KalaHeaders::KalaLog::Log;
using KalaHeaders::KalaLog::LogType;

using KalaHeaders::KalaFile::ReadBinaryDataFromFile;

using KalaGraphics::Core::KalaGraphicsCore;

using KalaGraphics::Import::FontData;
using KalaGraphics::Import::GlyphData;

using std::vector;
using std::array;
using std::string;
using std::string_view;
using std::to_string;
using std::unique_ptr;
using std::make_unique;
using std::filesystem::is_regular_file;
using std::ranges::find_if;

static constexpr string_view EXT_TTF = ".ttf";
static constexpr string_view EXT_OTF = ".otf";

static constexpr array<u32, 2> KEEP_EMPTY_GLYPHS =
{
    32, //space
    160 //non-breaking space
};

//for quadratic and cubic
static constexpr u8 CURVE_STEPS = 32;

//for line-to and close-path
static constexpr u8 STEPS = 64;

//final normalized x and y of character size printed to console
static constexpr u8 CONSOLE_WIDTH = 128;
static constexpr u8 CONSOLE_HEIGHT = 64;

static constexpr u32 PADDING = 1;

struct ConsoleGlyph
{
    array<array<char, CONSOLE_WIDTH>, CONSOLE_HEIGHT> pixels{};

    f32 currentX{};
    f32 currentY{};
};

struct DrawData
{
    ConsoleGlyph* glyph{};

    f32 minX{};
    f32 minY{};
    f32 width{};
    f32 height{};

    i32 drawWidth{};
};

namespace KalaGraphics::Import
{
    static KalaGraphicsRegistry<ImportFont> registry{};

    KalaGraphicsRegistry<ImportFont>& ImportFont::GetRegistry() { return registry; }

    GlyphData& ImportFont::GetGlyphData(
        FontData& fontData, 
        u32 codepoint)
    {
        static GlyphData empty{};

        auto it = find_if(
            fontData.glyphs, 
            [codepoint](const GlyphData& glyph) 
            { 
                return glyph.codepoint == codepoint;
            }); 
            
        if (it == fontData.glyphs.end()) 
        {
            Log::Print(
                "Failed to get glyph '" + to_string(codepoint) 
                + "' data because it was not found!",
                "KG_IMPORT_FONT",
                LogType::LOG_ERROR,
                2);

            return empty;
        }

        return *it;
    }

    ImportFont* ImportFont::Initialize(
        path&& fontPath,
        u8 fontScale)
    {
        if (fontScale == 0)
        {
            Log::Print(
                "Failed to import font '" + fontPath.string() + "' because its font scale was set to 0!",
                "KG_IMPORT_FONT",
                LogType::LOG_ERROR,
                2);

            return nullptr;
        }

        if (!exists(fontPath))
        {
            Log::Print(
                "Failed to import font '" + fontPath.string() + "' because it was not found!",
                "KG_IMPORT_FONT",
                LogType::LOG_ERROR,
                2);

            return nullptr;
        }

        if (!is_regular_file(fontPath))
        {
            Log::Print(
                "Failed to import font '" + fontPath.string() + "' because it is not a regular file!",
                "KG_IMPORT_FONT",
                LogType::LOG_ERROR,
                2);

            return nullptr;
        }

        string ext = fontPath.extension().string();
        if (ext != EXT_TTF
            && ext != EXT_OTF)
        {
            Log::Print(
                "Failed to import font '" + fontPath.string() + "' because its extension is not supported!",
                "KG_IMPORT_FONT",
                LogType::LOG_ERROR,
                2);

            return nullptr;
        }

        vector<u8> outData{};
        string errMsg = ReadBinaryDataFromFile(
            fontPath,
            outData);

        if (!errMsg.empty())
        {
            Log::Print(
                "Failed to import font '" + fontPath.string() + "'! Reason: " + errMsg,
                "KG_IMPORT_FONT",
                LogType::LOG_ERROR,
                2);

            return nullptr;
        }

        unique_ptr<ImportFont> newFont = make_unique<ImportFont>();
        ImportFont* fontPtr = newFont.get();

        FontData fontData{ .fontScale = fontScale };
        errMsg = fontPtr->Initialize_HarfBuzz(
            std::move(outData),
            fontData);

        if (!errMsg.empty())
        {
            Log::Print(
                "Failed to import font '" + fontPath.string() + "'! Reason: " + errMsg,
                "KG_IMPORT_FONT",
                LogType::LOG_ERROR,
                2);

            return nullptr;
        }

        u32 newID = KalaGraphicsCore::GetGlobalID() + 1;
        KalaGraphicsCore::SetGlobalID(newID);

        fontPtr->ID = newID;
        fontPtr->fontPath = std::move(fontPath);
        fontPtr->fontData = std::move(fontData);

        string err = registry.AddContent(newID, std::move(newFont));
        if (!err.empty())
        {
			KalaGraphicsCore::ForceClose(
				"KalaGraphics import font error",
				"Failed to initialize import font! Reason: " + err);
        }

        Log::Print(
			"Created new import font '" + to_string(newID) + "'!",
			"KG_IMPORT_FONT",
			LogType::LOG_SUCCESS);

        return fontPtr;
    }

    u32 ImportFont::GetID() const { return ID; }

    const path& ImportFont::GetFontPath() const { return fontPath; }

    FontData& ImportFont::GetFontData() { return fontData; }

    vector<u8> ImportFont::GetGlyphPixelData(
        u32 codepoint,
        bool fromAtlas)
    {
        GlyphData& glyphData = GetGlyphData(
            fontData,
            codepoint);

        if (glyphData.codepoint == 0)
        {
            Log::Print(
                "Failed to get glyph '" + to_string(codepoint) 
                + "' pixel data because it was not found!",
                "KG_IMPORT_FONT",
                LogType::LOG_ERROR,
                2);

            return {};
        }

        return GetGlyphPixelData(
            glyphData, 
            fromAtlas);
    }

    vector<u8> ImportFont::GetGlyphPixelData(
        const GlyphData& glyphData,
        bool fromAtlas)
    {
        if (fromAtlas)
        {
            const u32 width = scast<u32>(fabsf(glyphData.size.x));
            const u32 height = scast<u32>(fabsf(glyphData.size.y));

            if (width == 0
                || height == 0)
            {
                Log::Print(
                    "Failed to get glyph '" + to_string(glyphData.codepoint) 
                    + "' pixel data because extent width or height was 0!",
                    "KG_IMPORT_FONT",
                    LogType::LOG_ERROR,
                    2);

                return {};
            }

            const u32 atlasWidth = scast<u32>(fontData.atlasSize.x);

            const u32 atlasX = scast<u32>(glyphData.atlasPosition.x);
            const u32 atlasY = scast<u32>(glyphData.atlasPosition.y);

            vector<u8> pixelData(width * height);

            for (u32 y = 0; y < height; ++y)
            {
                for (u32 x = 0; x < width; ++x)
                {
                    pixelData[y * width + x] = fontData.atlasPixels[
                        (atlasY + y) * atlasWidth 
                        + atlasX + x];
                }
            }

            return pixelData;
        }
        else
        {
            hb_raster_draw_t* raster = hb_raster_draw_create_or_fail();
            if (!raster)
            {
                Log::Print(
                    "Failed to get glyph '" + to_string(glyphData.codepoint) 
                    + "' pixel data because hb_raster_draw_create_or_fail failed!",
                    "KG_IMPORT_FONT",
                    LogType::LOG_ERROR,
                    2);

                return {};
            }

            hb_glyph_extents_t glyphExtents{};

            if (!hb_font_get_glyph_extents(
                font,
                glyphData.glyphIndex,
                &glyphExtents))
            {
                Log::Print(
                    "Failed to get glyph '" + to_string(glyphData.codepoint) 
                    + "' pixel data because hb_font_get_glyph_extents failed!",
                    "KG_IMPORT_FONT",
                    LogType::LOG_ERROR,
                    2);

                hb_raster_draw_destroy(raster);

                return {};
            }

            /*
            Log::Print(
                "@@@@@ glyph extents:\n"
                "x_bearing: " + to_string(glyphExtents.x_bearing) + "\n"
                "y_bearing: " + to_string(glyphExtents.y_bearing) + "\n"
                "width: " + to_string(glyphExtents.width) + "\n"
                "height: " + to_string(glyphExtents.height));
            */

            if (glyphExtents.width == 0
                || glyphExtents.height == 0)
            {
                Log::Print(
                    "Failed to get glyph '" + to_string(glyphData.codepoint) 
                    + "' pixel data because extent width or height was 0!",
                    "KG_IMPORT_FONT",
                    LogType::LOG_ERROR,
                    2);

                hb_raster_draw_destroy(raster);

                return {};
            }

            if (!hb_raster_draw_set_glyph_extents(
                raster,
                &glyphExtents))
            {
                Log::Print(
                    "Failed to get glyph '" + to_string(glyphData.codepoint) 
                    + "' pixel data because hb_raster_draw_set_glyph_extents failed!",
                    "KG_IMPORT_FONT",
                    LogType::LOG_ERROR,
                    2);

                hb_raster_draw_destroy(raster);

                return {};
            }

            if (!hb_raster_draw_glyph_or_fail(
                raster,
                font,
                glyphData.glyphIndex))
            {
                Log::Print(
                    "Failed to get glyph '" + to_string(glyphData.codepoint) 
                    + "' pixel data because hb_raster_draw_glyph_or_fail failed!",
                    "KG_IMPORT_FONT",
                    LogType::LOG_ERROR,
                    2);

                hb_raster_draw_destroy(raster);

                return {};
            }

            hb_raster_image_t* image = hb_raster_draw_render(raster);
            if (!image)
            {
                Log::Print(
                    "Failed to get glyph '" + to_string(glyphData.codepoint) 
                    + "' pixel data because hb_raster_draw_render failed!",
                    "KG_IMPORT_FONT",
                    LogType::LOG_ERROR,
                    2);

                hb_raster_draw_destroy(raster);

                return {};
            }

            hb_raster_extents_t extents{};
            hb_raster_image_get_extents(
                image,
                &extents);

            const u8* pixels = hb_raster_image_get_buffer(image);

            /*
            u32 nonZeroPixels{};

            for (u32 y = 0; y < extents.height; ++y)
            {
                for (u32 x = 0; x < extents.width; ++x)
                {
                    if (pixels[y * extents.stride + x] != 0) ++nonZeroPixels;
                }
            }

            Log::Print(
                "@@@@@ raster data:\n"
                "width: " + to_string(extents.width) + "\n"
                "height: " + to_string(extents.height) + "\n"
                "stride: " + to_string(extents.stride) + "\n"
                "bytes: " + to_string(extents.stride * extents.height) + "\n"
                "non-zero pixels: " + to_string(nonZeroPixels));
            */

            vector<u8> pixelData(extents.width * extents.height);

            for (u32 y = 0; y < extents.height; ++y)
            {
                for (u32 x = 0; x < extents.width; ++x)
                {
                    pixelData[y * extents.width + x] = pixels[y * extents.stride + x];
                }
            }

            hb_raster_image_destroy(image);
            hb_raster_draw_destroy(raster);

            return pixelData;
        }
    }

    void ImportFont::DrawGlyphToConsole(u32 codepoint)
    {
        GlyphData& glyphData = GetGlyphData(
            fontData,
            codepoint);
        if (glyphData.codepoint == 0)
        {
            Log::Print(
                "Failed to draw glyph '" + to_string(codepoint) 
                + "' because it was not found!",
                "KG_IMPORT_FONT",
                LogType::LOG_ERROR,
                2);

            return;
        }

        ConsoleGlyph consoleGlyph{};

        //normalize glyph coordinates into 64x64 console area

        const f32 paddingX = fabsf(glyphData.size.x) * 0.05f;
        const f32 paddingY = fabsf(glyphData.size.y) * 0.05f;

        const f32 minX = 
            glyphData.bearing.x
            - paddingX;
        const f32 maxX = 
            glyphData.bearing.x 
            + glyphData.size.x 
            + paddingX;

        const f32 minY = 
            glyphData.bearing.y 
            + glyphData.size.y
            - paddingY;
        const f32 maxY = 
            glyphData.bearing.y
            + paddingY;

        const vec2 size =
        {
            maxX - minX,
            maxY - minY
        };

        if (size.x == 0
            || size.y == 0)
        {
            return;
        }

        DrawData drawData = 
        {
            .glyph = &consoleGlyph,
            .minX = minX,
            .minY = minY,
            .width = size.x,
            .height = size.y,
            .drawWidth = scast<i32>(
                scast<f32>(CONSOLE_HEIGHT - 1)
                * (size.x / size.y)
                * 2.0f)
        };

        //create draw functions once 
        if (!drawFuncs)
        { 
            drawFuncs = hb_draw_funcs_create(); 
            
            //move-to
            hb_draw_funcs_set_move_to_func( 
                drawFuncs, 
                []( 
                    hb_draw_funcs_t*, 
                    void* drawData, 
                    hb_draw_state_t*, 
                    f32 toX,
                    f32 toY,
                    void*) 
                {
                    /*
                    Log::Print(
                        "@@@@@ move to: " 
                        + to_string(toX) + ", " 
                        + to_string(toY));
                    */

                    auto* data = rcast<DrawData*>(drawData);

                    data->glyph->currentX = toX;
                    data->glyph->currentY = toY;
                }, 
                nullptr,
                nullptr); 
                
            //line-to
            hb_draw_funcs_set_line_to_func(
                drawFuncs,
                []( 
                    hb_draw_funcs_t*, 
                    void* drawData, 
                    hb_draw_state_t*, 
                    f32 toX,
                    f32 toY, 
                    void*) 
                {
                    /*
                    Log::Print(
                        "@@@@@ line to: " 
                        + to_string(toX) + ", " 
                        + to_string(toY));
                    */

                    auto* data = rcast<DrawData*>(drawData);

                    const f32 fromX = data->glyph->currentX;
                    const f32 fromY = data->glyph->currentY;

                    for (u8 i = 0; i <= STEPS; ++i)
                    {
                        const f32 t = scast<f32>(i) / scast<f32>(STEPS);

                        const f32 x = fromX + (toX - fromX) * t;
                        const f32 y = fromY + (toY - fromY) * t;

                        const i32 px = scast<i32>(
                            ((x - data->minX) / data->width)
                            * scast<f32>(data->drawWidth));

                        const i32 py = scast<i32>(
                            (1.0f - ((y - data->minY) / data->height))
                            * scast<f32>(CONSOLE_HEIGHT - 1));

                        if (px >= 0
                            && px < CONSOLE_WIDTH
                            && py >= 0
                            && py < CONSOLE_HEIGHT)
                        {
                            data->glyph->pixels[py][px] = '#';
                        }
                    }

                    data->glyph->currentX = toX;
                    data->glyph->currentY = toY;
                }, 
                nullptr,
                nullptr); 
                
            //quadratic
            hb_draw_funcs_set_quadratic_to_func(
                drawFuncs,
                [](
                    hb_draw_funcs_t*,
                    void* drawData,
                    hb_draw_state_t*,
                    f32 controlX,
                    f32 controlY,
                    f32 toX,
                    f32 toY,
                    void*) 
                {
                    /*
                    Log::Print(
                        "@@@@@ quadratic: control " 
                        + to_string(controlX) + ", " 
                        + to_string(controlY) 
                        + " -> " 
                        + to_string(toX) + ", " 
                        + to_string(toY));
                    */

                    auto* data = rcast<DrawData*>(drawData);

                    const f32 fromX = data->glyph->currentX;
                    const f32 fromY = data->glyph->currentY;

                    for (u8 i = 0; i <= CURVE_STEPS; ++i)
                    {
                        const f32 t = scast<f32>(i) / scast<f32>(CURVE_STEPS);

                        const f32 mt = 1.0f - t;

                        const f32 x = 
                            mt * mt * fromX
                            + 2.0f * mt * t * controlX
                            + t * t * toX;
                        const f32 y = 
                            mt * mt * fromY
                            + 2.0f * mt * t * controlY
                            + t * t * toY;

                        const i32 px = scast<i32>(
                            ((x - data->minX) / data->width)
                            * scast<f32>(data->drawWidth));

                        const i32 py = scast<i32>(
                            (1.0f - ((y - data->minY) / data->height))
                            * scast<f32>(CONSOLE_HEIGHT - 1));

                        if (px >= 0
                            && px < CONSOLE_WIDTH
                            && py >= 0
                            && py < CONSOLE_HEIGHT)
                        {
                            data->glyph->pixels[py][px] = '#';
                        }
                    }

                    data->glyph->currentX = toX;
                    data->glyph->currentY = toY;
                }, 
                nullptr, 
                nullptr); 
                
            //cubic
            hb_draw_funcs_set_cubic_to_func(
                drawFuncs,
                []( 
                    hb_draw_funcs_t*, 
                    void* drawData, 
                    hb_draw_state_t*, 
                    f32 control1X, 
                    f32 control1Y, 
                    f32 control2X, 
                    f32 control2Y, 
                    f32 toX, 
                    f32 toY, 
                    void*) 
                {
                    /*
                    Log::Print(
                        "@@@@@ cubic: " 
                        + to_string(control1X) + ", " 
                        + to_string(control1Y) 
                        + " | " 
                        + to_string(control2X) + ", " 
                        + to_string(control2Y) 
                        + " -> " 
                        + to_string(toX) + ", " 
                        + to_string(toY));
                    */

                    auto* data = rcast<DrawData*>(drawData);

                    const f32 fromX = data->glyph->currentX;
                    const f32 fromY = data->glyph->currentY;

                    for (u8 i = 0; i <= CURVE_STEPS; ++i)
                    {
                        const f32 t = scast<f32>(i) / scast<f32>(CURVE_STEPS);

                        const f32 mt = 1.0f - t;

                        const f32 x = 
                            mt * mt * mt * fromX
                            + 3.0f * mt * mt * t * control1X
                            + 3.0f * mt * t * t * control2X
                            + t * t * t * toX;
                        const f32 y = 
                            mt * mt * mt * fromY
                            + 3.0f * mt * mt * t * control1Y
                            + 3.0f * mt * t * t * control2Y
                            + t * t * t * toY;

                        const i32 px = scast<i32>(
                            ((x - data->minX) / data->width)
                            * scast<f32>(data->drawWidth));

                        const i32 py = scast<i32>(
                            (1.0f - ((y - data->minY) / data->height))
                            * scast<f32>(CONSOLE_HEIGHT - 1));

                        if (px >= 0
                            && px < CONSOLE_WIDTH
                            && py >= 0
                            && py < CONSOLE_HEIGHT)
                        {
                            data->glyph->pixels[py][px] = '#';
                        }
                    }

                    data->glyph->currentX = toX;
                    data->glyph->currentY = toY;
                }, 
                nullptr, 
                nullptr);
                
            //close path
            hb_draw_funcs_set_close_path_func(
                drawFuncs,
                []( 
                    hb_draw_funcs_t*, 
                    void* drawData, 
                    hb_draw_state_t* drawState, 
                    void*) 
                { 
                    //Log::Print("@@@@@ close path");

                    auto* data = rcast<DrawData*>(drawData);

                    const f32 fromX = data->glyph->currentX;
                    const f32 fromY = data->glyph->currentY;

                    const f32 toX = drawState->path_start_x;
                    const f32 toY = drawState->path_start_y;

                    for (u8 i = 0; i <= STEPS; ++i)
                    {
                        const f32 t = scast<f32>(i) / scast<f32>(STEPS);

                        const f32 x = fromX + (toX - fromX) * t;
                        const f32 y = fromY + (toY - fromY) * t;

                        const i32 px = scast<i32>(
                            ((x - data->minX) / data->width)
                            * scast<f32>(data->drawWidth));

                        const i32 py = scast<i32>(
                            (1.0f - ((y - data->minY) / data->height))
                            * scast<f32>(CONSOLE_HEIGHT - 1));

                        if (px >= 0
                            && px < CONSOLE_WIDTH
                            && py >= 0
                            && py < CONSOLE_HEIGHT)
                        {
                            data->glyph->pixels[py][px] = '#';
                        }
                    }

                    data->glyph->currentX = toX;
                    data->glyph->currentY = toY;
                }, 
                nullptr,
                nullptr);
        }
                
        if (!hb_font_draw_glyph_or_fail(
            font,
            glyphData.glyphIndex,
            drawFuncs,
            &drawData))
        {
            Log::Print(
                "Failed to draw glyph '" + to_string(codepoint) 
                + "' to console because hb_font_draw_glyph_or_fail failed!",
                "KG_IMPORT_FONT",
                LogType::LOG_ERROR,
                2);
        }

        string output = "\n";

        for (const auto& row : consoleGlyph.pixels)
        {
            for (const auto& pixel : row)
            {
                output += pixel ? pixel : ' ';
            }

            output += '\n';
        }

        Log::Print(output);
    }

    string ImportFont::Initialize_HarfBuzz(
        vector<u8>&& binaryData,
        FontData& outFontData)
    {
        blob = hb_blob_create(
            rcast<const char*>(binaryData.data()),
            scast<unsigned int>(binaryData.size()),
            HB_MEMORY_MODE_DUPLICATE,
            nullptr,
            nullptr);

        face = hb_face_create(
            blob,
            0);
            
        font = hb_font_create(face);

        hb_ot_font_set_funcs(font);

        hb_font_set_scale(
            font,
            outFontData.fontScale,
            outFontData.fontScale);

        //unsigned int glyphCount = hb_face_get_glyph_count(face);

        hb_font_extents_t fontExtents{};
        if (!hb_font_get_h_extents(
            font,
            &fontExtents))
        {
            return "Failed to initialize HarfBuzz because hb_font_extents_t failed!";
        }

        /*
        Log::Print(
            "@@@@@\n"
            "glyph count: " + to_string(glyphCount) + "\n"
            "ascender: "    + to_string(fontExtents.ascender) + "\n"
            "descender: "   + to_string(fontExtents.descender) + "\n"
            "line gap: "    + to_string(fontExtents.line_gap) + "\n");
        */

        outFontData.ascender = fontExtents.ascender;
        outFontData.descender = fontExtents.descender;
        outFontData.lineGap = fontExtents.line_gap;

        unicodes = hb_set_create();
        hb_face_collect_unicodes(
            face,
            unicodes);

        outFontData.glyphs.reserve(hb_set_get_population(unicodes));

        hb_codepoint_t codepoint = HB_SET_VALUE_INVALID;

        while (hb_set_next(
            unicodes,
            &codepoint))
        {
            hb_codepoint_t glyph{};

            if (!hb_font_get_nominal_glyph(
                font,
                codepoint,
                &glyph))
            {
                Log::Print("@@@@@ skipped odd glyph '" + to_string(codepoint) + "'");

                continue;
            }

            hb_position_t advance = hb_font_get_glyph_h_advance(
                font,
                glyph);

            hb_glyph_extents_t extents{};

            if (hb_font_get_glyph_extents(
                font,
                glyph,
                &extents))
            {
                if (!ContainsValue(KEEP_EMPTY_GLYPHS, codepoint)
                    && extents.width == 0
                    && extents.height == 0)
                {
                    Log::Print("@@@@@ skipped empty glyph '" + to_string(codepoint) + "'");

                    continue; 
                }

                /*
                Log::Print(
                    "@@@@@\n"
                    "codepoint: " + to_string(codepoint) + "\n"
                    "glyph: "     + to_string(glyph) + "\n"
                    "advance: "   + to_string(advance) + "\n"
                    "x bearing: " + to_string(extents.x_bearing) + "\n"
                    "y bearing: " + to_string(extents.y_bearing) + "\n"
                    "width: "     + to_string(extents.width) + "\n"
                    "height: "    + to_string(extents.height) + "\n");
                */

                outFontData.glyphs.push_back(
                {
                    GlyphData{
                    .codepoint = codepoint,
                    .glyphIndex = glyph,
                    .advance = advance,
                    .bearing = { scast<f32>(extents.x_bearing), scast<f32>(extents.y_bearing) },
                    .size = { scast<f32>(extents.width), scast<f32>(extents.height) },
                }});
            }
        }

        string atlasError = GenerateAtlas(outFontData);
        if (!atlasError.empty())
        {
            return "Failed to initialize HarfBuzz because atlas generation failed! Reason: " + atlasError;
        }

        return "";
    }

    string ImportFont::GenerateAtlas(FontData& outFontData)
    {
        //
        // CALCULATE TOTAL REQUIRED GLYPH AREA
        //

        u32 totalArea{};

        for (const GlyphData& glyph : outFontData.glyphs)
        {
            const u32 width = scast<u32>(fabsf(glyph.size.x));
            const u32 height = scast<u32>(fabsf(glyph.size.y));

            if (width == 0
                || height == 0)
            {
                continue;
            }

            totalArea += 
                (width + PADDING * 2)
                * (height + PADDING * 2);
        }

        if (totalArea == 0)
        {
            return "Failed to generate font atlas because no drawable glyphs were found!";
        }

        //
        // DERIVE ATLAS WIDTH
        //

        u32 atlasWidth = scast<u32>(ceilf(sqrtf(scast<f32>(totalArea))));

        //round up to next power of two
        u32 powerOfTwoWidth = 1;

        while (powerOfTwoWidth < atlasWidth) powerOfTwoWidth *= 2;

        atlasWidth = powerOfTwoWidth;

        //
        // CALCULATE GLYPH POSITIONS AND ATLAS HEIGHT
        //

        u32 currentX = PADDING;
        u32 currentY = PADDING;
        u32 rowHeight{};

        for (GlyphData& glyph : outFontData.glyphs)
        {
            const u32 width = scast<u32>(fabsf(glyph.size.x));
            const u32 height = scast<u32>(fabsf(glyph.size.y));

            if (width == 0
                || height == 0)
            {
                continue;
            }

            if (currentX + width + PADDING > atlasWidth)
            {
                currentX = PADDING;
                currentY += rowHeight + PADDING * 2;
                rowHeight = 0;
            }

            glyph.atlasPosition = 
            {
                scast<f32>(currentX),
                scast<f32>(currentY)
            };

            currentX += width + PADDING * 2;

            if (height > rowHeight) rowHeight = height;
        }

        const u32 atlasHeight = currentY + rowHeight + PADDING;

        outFontData.atlasSize = 
        {
            scast<f32>(atlasWidth),
            scast<f32>(atlasHeight)
        };

        //
        // RASTERIZE AND COPY GLYPHS INTO ATLAS
        //

        outFontData.atlasPixels.assign(
            atlasWidth * atlasHeight,
            0);

        for (const GlyphData& glyph : outFontData.glyphs)
        {
            const u32 width = scast<u32>(fabsf(glyph.size.x));
            const u32 height = scast<u32>(fabsf(glyph.size.y));

            if (width == 0
                || height == 0)
            {
                continue;
            }

            vector<u8> glyphPixels = GetGlyphPixelData(
                glyph, 
                false);

            if (glyphPixels.empty())
            {
                return 
                    "Failed to generate font atlas because "
                    "glyph '" + to_string(glyph.codepoint) + "' "
                    "pixel data could not be generated!";
            }

            const u32 atlasX = scast<u32>(glyph.atlasPosition.x);
            const u32 atlasY = scast<u32>(glyph.atlasPosition.y);

            for (u32 y = 0; y < height; ++y)
            {
                for (u32 x = 0; x < width; ++x)
                {
                    outFontData.atlasPixels[
                        (atlasY + y) * atlasWidth
                        + atlasX + x]
                        = glyphPixels[y * width + x];
                }
            }
        }

        return "";
    }

    void ImportFont::Destroy()
    {
        string err = registry.DestroyContent(ID);
        if (!err.empty())
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics import font error",
                "Failed to destroy import font '" + to_string(ID) + "'! Reason: " + err);
        }
    }

    ImportFont::~ImportFont()
    {
        Log::Print(
            "Destroying import font data '" + to_string(ID) + "'.",
            "KG_IMPORT_FONT",
            LogType::LOG_INFO);

        if (drawFuncs) hb_draw_funcs_destroy(drawFuncs);

        hb_set_destroy(unicodes);
        hb_font_destroy(font);
        hb_face_destroy(face);
        hb_blob_destroy(blob);
    }
}