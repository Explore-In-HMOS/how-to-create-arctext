/**
 * Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <arkui/native_interface.h>
#include <arkui/native_node.h>
#include <arkui/native_node_napi.h>
#include <hilog/log.h>
#include <js_native_api_types.h>
#include <native_drawing/drawing_brush.h>
#include <native_drawing/drawing_canvas.h>
#include <native_drawing/drawing_color.h>
#include <native_drawing/drawing_font.h>
#include <native_drawing/drawing_text_blob.h>
#include <native_drawing/drawing_text_typography.h>
#include <native_drawing/drawing_types.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define ARC_LOG(fmt, ...) OH_LOG_Print(LOG_APP, LOG_INFO, 0x0000, "ArcText", fmt, ##__VA_ARGS__)

// Default sizing ratios used when the caller omits `fontSize` / `radius`.
// Tuned for circular wearables; resolved per-frame from the actual canvas
// size so the same call works on 466, 454, 390, ... watch faces.
static constexpr float K_DEFAULT_FONT_SIZE_RATIO = 0.075f; // fraction of min(canvasW, canvasH)
static constexpr float K_RIM_MARGIN_RATIO = 0.01f;        // fraction of screen radius kept as inset

// Holds everything the draw callback needs. A value of 0 for `fontSize` /
// `radius` means "the caller did not specify it; pick a default from the
// canvas size at draw time".
struct ArcTextData {
    std::string text;
    float fontSize;      // px, or 0 to auto-size from canvas
    float radius;        // px (baseline distance from center), or 0 to auto-fit
    uint32_t color;      // ARGB
    float startAngleDeg; // 0 = top, positive = clockwise; angle of text center
    bool flipped;        // if true, draw text upside-down (useful for bottom arcs)
};

// Split a UTF-8 string into individual codepoint byte sequences.
static std::vector<std::string> SplitUtf8(const std::string &s)
{
    std::vector<std::string> out;
    size_t i = 0;
    while (i < s.size()) {
        unsigned char c = static_cast<unsigned char>(s[i]);
        size_t len = 1;
        if ((c & 0x80) == 0x00) {
            len = 1;
        } else if ((c & 0xE0) == 0xC0) {
            len = 2;
        } else if ((c & 0xF0) == 0xE0) {
            len = 3;
        } else if ((c & 0xF8) == 0xF0) {
            len = 4;
        }
        if (i + len > s.size()) {
            len = s.size() - i;
        }
        out.emplace_back(s.substr(i, len));
        i += len;
    }
    return out;
}

static void DrawArcText(OH_Drawing_Canvas *canvas, float cx, float cy, float screenRadius, const ArcTextData &d)
{
    if (canvas == nullptr || d.text.empty() || d.fontSize <= 0.0f || screenRadius <= 0.0f) {
        return;
    }

    OH_Drawing_Font *font = OH_Drawing_FontCreate();
    OH_Drawing_FontSetTextSize(font, d.fontSize);

    OH_Drawing_Brush *brush = OH_Drawing_BrushCreate();
    OH_Drawing_BrushSetColor(brush, d.color);
    OH_Drawing_BrushSetAntiAlias(brush, true);
    OH_Drawing_CanvasAttachBrush(canvas, brush);

    // Font metrics: ascent is negative (above baseline), descent is positive.
    OH_Drawing_Font_Metrics metrics{};
    OH_Drawing_FontGetMetrics(font, &metrics);
    const float ascent = -metrics.ascent;  // glyph extent above baseline
    const float descent = metrics.descent; // glyph extent below baseline

    // The outer pixel of the rendered text must stay within the screen disc.
    // Keep a small margin from the rim. Outer-pixel radius for the chosen
    // baseline is `baseline + ascent` (non-flipped) or `baseline + descent`
    // after the 180° flip... but we always derive `baseline` from a single
    // "text outer radius" so both orientations end up at the same place.
    const float maxOuter = screenRadius * (1.0f - K_RIM_MARGIN_RATIO);
    float outerRadius = (d.radius > 0.0f) ? (d.radius + ascent) : maxOuter;
    if (outerRadius > maxOuter) {
        outerRadius = maxOuter; // clamp explicit radius so it never spills past the rim
    }

    // Baseline distance from the canvas center. For flipped text the glyph
    // body extends inward from the baseline by `descent`, so we shift the
    // baseline outward by (ascent - descent) to keep the outer edge aligned.
    const float baselineRadius = d.flipped ? (outerRadius - descent) : (outerRadius - ascent);

    auto glyphs = SplitUtf8(d.text);

    // Measure each glyph and compute the arc angle (degrees) it occupies,
    // using the BASELINE radius (that's where the glyph centers sit).
    std::vector<float> widths(glyphs.size(), 0.0f);
    std::vector<float> angles(glyphs.size(), 0.0f);
    float totalAngle = 0.0f;
    const float degPerPx = (1.0f / baselineRadius) * 180.0f / static_cast<float>(M_PI);

    for (size_t i = 0; i < glyphs.size(); ++i) {
        float w = 0.0f;
        OH_Drawing_FontMeasureText(font, glyphs[i].c_str(), glyphs[i].size(), TEXT_ENCODING_UTF8, nullptr, &w);
        widths[i] = w;
        angles[i] = w * degPerPx;
        totalAngle += angles[i];
    }

    OH_Drawing_CanvasSave(canvas);
    OH_Drawing_CanvasTranslate(canvas, cx, cy);
    // The text run is always centered on `startAngleDeg`. When `flipped` is
    // true we walk counter-clockwise (and rotate each glyph 180° in place) so
    // the first character lands on the viewer's left and the run reads
    // left-to-right when viewed upright (e.g. on a bottom arc).
    const float dir = d.flipped ? -1.0f : 1.0f;
    OH_Drawing_CanvasRotate(canvas, d.startAngleDeg - dir * totalAngle / 2.0f, 0.0f, 0.0f);

    const float glyphRotationFlip = d.flipped ? 180.0f : 0.0f;

    for (size_t i = 0; i < glyphs.size(); ++i) {
        OH_Drawing_CanvasRotate(canvas, dir * angles[i] / 2.0f, 0.0f, 0.0f);

        OH_Drawing_CanvasSave(canvas);
        OH_Drawing_CanvasTranslate(canvas, 0.0f, -baselineRadius);
        if (glyphRotationFlip != 0.0f) {
            OH_Drawing_CanvasRotate(canvas, glyphRotationFlip, 0.0f, 0.0f);
        }

        OH_Drawing_TextBlob *blob = OH_Drawing_TextBlobCreateFromString(glyphs[i].c_str(), font, TEXT_ENCODING_UTF8);
        if (blob != nullptr) {
            OH_Drawing_CanvasDrawTextBlob(canvas, blob, -widths[i] / 2.0f, 0.0f);
            OH_Drawing_TextBlobDestroy(blob);
        }

        OH_Drawing_CanvasRestore(canvas);
        OH_Drawing_CanvasRotate(canvas, dir * angles[i] / 2.0f, 0.0f, 0.0f);
    }

    OH_Drawing_CanvasRestore(canvas);

    OH_Drawing_CanvasDetachBrush(canvas);
    OH_Drawing_BrushDestroy(brush);
    OH_Drawing_FontDestroy(font);
}

// Build a custom ArkUI node that fills its parent and dispatches to the
// appropriate draw routine based on `data->kind`.
static ArkUI_NodeHandle BuildDrawNode(ArkUI_NodeContentHandle handle, ArcTextData *data)
{
    auto *nodeAPI = reinterpret_cast<ArkUI_NativeNodeAPI_1 *>(
        OH_ArkUI_QueryModuleInterfaceByName(ARKUI_NATIVE_NODE, "ArkUI_NativeNodeAPI_1"));
    if (nodeAPI == nullptr) {
        return nullptr;
    }

    auto customNode = nodeAPI->createNode(ARKUI_NODE_CUSTOM);

    // Fill 100% of the parent (Stack → 466x466 watch face).
    ArkUI_NumberValue pct[] = {{.f32 = 1.0f}};
    ArkUI_AttributeItem pctItem = {pct, 1};
    nodeAPI->setAttribute(customNode, NODE_WIDTH_PERCENT, &pctItem);
    nodeAPI->setAttribute(customNode, NODE_HEIGHT_PERCENT, &pctItem);

    // ON_DRAW is the content draw pass for custom nodes — fires reliably
    // even when the node has no children.
    nodeAPI->registerNodeCustomEvent(customNode, ARKUI_NODE_CUSTOM_EVENT_ON_DRAW, 1, data);
    nodeAPI->registerNodeCustomEventReceiver([](ArkUI_NodeCustomEvent *event) {
        auto type = OH_ArkUI_NodeCustomEvent_GetEventType(event);
        if (type != ARKUI_NODE_CUSTOM_EVENT_ON_DRAW) {
            return;
        }
        auto *userData = reinterpret_cast<ArcTextData *>(OH_ArkUI_NodeCustomEvent_GetUserData(event));
        if (userData == nullptr) {
            return;
        }
        auto *drawContext = OH_ArkUI_NodeCustomEvent_GetDrawContextInDraw(event);
        auto *canvas = reinterpret_cast<OH_Drawing_Canvas *>(OH_ArkUI_DrawContext_GetCanvas(drawContext));
        ArkUI_IntSize size = OH_ArkUI_DrawContext_GetSize(drawContext);
        const float cx = size.width * 0.5f;
        const float cy = size.height * 0.5f;

        // Resolve dynamic dimensions from the actual canvas size so the same
        // call adapts to 466, 454, 390, ... watch faces.
        ArcTextData resolved = *userData;
        const float screenRadius = static_cast<float>(std::min(size.width, size.height)) * 0.5f;
        if (resolved.fontSize <= 0.0f) {
            resolved.fontSize = K_DEFAULT_FONT_SIZE_RATIO * (screenRadius * 2.0f);
        }
        // `radius` (baseline distance) is auto-fit inside DrawArcText based on
        // the resolved font metrics so the outer pixel never crosses the rim.

        ARC_LOG("draw canvas=%{public}dx%{public}d radius=%{public}.1f fontSize=%{public}.1f text=%{public}s",
                size.width, size.height, resolved.radius, resolved.fontSize, resolved.text.c_str());
        DrawArcText(canvas, cx, cy, screenRadius, resolved);
    });

    OH_ArkUI_NodeContent_AddNode(handle, customNode);
    return customNode;
}

static std::string NapiGetString(napi_env env, napi_value v)
{
    size_t size = 0;
    if (napi_get_value_string_utf8(env, v, nullptr, 0, &size) != napi_ok) {
        return std::string();
    }
    if (size == 0) {
        return std::string();
    }
    std::vector<char> buf(size + 1, '\0');
    napi_get_value_string_utf8(env, v, buf.data(), buf.size(), &size);
    return std::string(buf.data(), size);
}

static double NapiGetDouble(napi_env env, napi_value v, double fallback)
{
    double d = fallback;
    return (napi_get_value_double(env, v, &d) == napi_ok) ? d : fallback;
}

static uint32_t NapiGetUint32(napi_env env, napi_value v, uint32_t fallback)
{
    uint32_t u = fallback;
    return (napi_get_value_uint32(env, v, &u) == napi_ok) ? u : fallback;
}

static bool NapiGetBool(napi_env env, napi_value v, bool fallback)
{
    bool b = fallback;
    return (napi_get_value_bool(env, v, &b) == napi_ok) ? b : fallback;
}

// Returns the named property from `obj`, or nullptr if it is missing /
// undefined / null. Treats type errors as missing so callers can fall back
// to defaults cleanly.
static napi_value NapiGetProp(napi_env env, napi_value obj, const char *name)
{
    bool has = false;
    if (napi_has_named_property(env, obj, name, &has) != napi_ok || !has) {
        return nullptr;
    }
    napi_value v = nullptr;
    if (napi_get_named_property(env, obj, name, &v) != napi_ok) {
        return nullptr;
    }
    napi_valuetype t = napi_undefined;
    napi_typeof(env, v, &t);
    if (t == napi_undefined || t == napi_null) {
        return nullptr;
    }
    return v;
}

// drawArcText({ content, text, fontSize?, radius?, color?, startAngleDeg?, flipped? })
static napi_value DrawArcTextNapi(napi_env env, napi_callback_info info)
{
    size_t argc = 1;
    napi_value args[1] = {nullptr};
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
    if (argc < 1 || args[0] == nullptr) {
        ARC_LOG("drawArcText: missing options object");
        return nullptr;
    }
    napi_value opts = args[0];

    napi_value vContent = NapiGetProp(env, opts, "content");
    napi_value vText = NapiGetProp(env, opts, "text");
    napi_value vFontSize = NapiGetProp(env, opts, "fontSize");
    napi_value vRadius = NapiGetProp(env, opts, "radius");
    napi_value vColor = NapiGetProp(env, opts, "color");
    napi_value vStartAngleDeg = NapiGetProp(env, opts, "startAngleDeg");
    napi_value vFlipped = NapiGetProp(env, opts, "flipped");

    if (vContent == nullptr) {
        ARC_LOG("drawArcText: missing 'content'");
        return nullptr;
    }
    ArkUI_NodeContentHandle contentHandle = nullptr;
    OH_ArkUI_GetNodeContentFromNapiValue(env, vContent, &contentHandle);
    if (contentHandle == nullptr) {
        ARC_LOG("drawArcText: null content handle");
        return nullptr;
    }

    auto *data = new ArcTextData();
    // 0 means "auto-size from canvas at draw time".
    data->text = (vText != nullptr) ? NapiGetString(env, vText) : std::string("ArcText");
    data->fontSize = (vFontSize != nullptr) ? static_cast<float>(NapiGetDouble(env, vFontSize, 0.0)) : 0.0f;
    data->radius = (vRadius != nullptr) ? static_cast<float>(NapiGetDouble(env, vRadius, 0.0)) : 0.0f;
    data->color = (vColor != nullptr) ? NapiGetUint32(env, vColor, 0xFFFFFFFFu) : 0xFFFFFFFFu;
    data->startAngleDeg =
        (vStartAngleDeg != nullptr) ? static_cast<float>(NapiGetDouble(env, vStartAngleDeg, 0.0)) : 0.0f;
    data->flipped = (vFlipped != nullptr) ? NapiGetBool(env, vFlipped, false) : false;

    BuildDrawNode(contentHandle, data);
    return nullptr;
}

EXTERN_C_START
static napi_value Init(napi_env env, napi_value exports)
{
    napi_property_descriptor desc[] = {
        {"drawArcText", nullptr, DrawArcTextNapi, nullptr, nullptr, nullptr, napi_default, nullptr},
    };
    napi_define_properties(env, exports, sizeof(desc) / sizeof(desc[0]), desc);
    return exports;
}
EXTERN_C_END

static napi_module demoModule = {
    .nm_version = 1,
    .nm_flags = 0,
    .nm_filename = nullptr,
    .nm_register_func = Init,
    .nm_modname = "entry",
    .nm_priv = ((void *)0),
    .reserved = {0},
};

extern "C" __attribute__((constructor)) void RegisterEntryModule(void) { napi_module_register(&demoModule); }