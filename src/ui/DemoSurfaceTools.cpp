// GPU regression for multi-channel retouch, selection and an editable path on a mesh with a UV seam.
#include "ui/App.h"
#include "render/Dispatch.h"
#include "ai/Net.h"
#include "core/Deflate.h"

namespace sx {
void App::SurfaceToolTestTick() {
    static std::vector<std::pair<const char*, gpu::ReadbackPtr>> reads;
    static std::vector<float> expectations;
    static int failures = 0;
    if (frameCounter < 3) return;
    auto check = [&](bool pass, const char* label) {
        Log("[surface-tools] %s %s", pass ? "PASS" : "FAIL", label);
        if (!pass) ++failures;
    };
    if (frameCounter == 3) {
        for (auto name : {"clear.slang", "clear_r.slang", "brush.slang", "fill_sel.slang"})
            Kernel(name);
        NewDocument(128, 128, Precision::Half16, DocMode::Layers, 0, "Surface tools fixture");
        auto& v = *Active();
        auto& d = *v.doc;
        d.kind = 1;
        d.pbrChannels = (1u << (int)PbrChannel::Count) - 1;
        MeshData m;
        for (int side = 0; side < 2; ++side) {
            const float left = -1.0f + side, right = left + 1;
            // Swap the UV halves: an address offset in texture space gives a wrong source across the join.
            const float u0 = side ? 0.02f : 0.52f, u1 = u0 + 0.46f;
            const u32 b = (u32)m.vertices.size();
            m.vertices.insert(m.vertices.end(), {{{left, -1, 0}, {0, 0, -1}, {u0, 0.98f}},
                                                 {{right, -1, 0}, {0, 0, -1}, {u1, 0.98f}},
                                                 {{right, 1, 0}, {0, 0, -1}, {u1, 0.02f}},
                                                 {{left, 1, 0}, {0, 0, -1}, {u0, 0.02f}}});
            m.indices.insert(m.indices.end(), {b, b + 2, b + 1, b, b + 3, b + 2});
        }
        m.boundsMin = {-1, -1, 0};
        m.boundsMax = {1, 1, 0};
        AttachMesh(v, std::move(m));
        CreatePbrChannels(v, true);
        Id material = AddPaintLayer3D(v, "Fixture");
        auto& s = *v.mesh;
        s.yaw = s.pitch = 0;
        s.ortho = true;
        s.distance = 2.8f;
        s.UpdateCamera(1);
        v.renderer.Prepare(d, *ctx);
        auto stop = [&](const char* label) {
            check(false, label);
            Log("[surface-tools] COMPLETE %d failures", failures);
            autoQuit = true;
        };
        if (!v.renderer.MeshPrepare(d, *ctx, s)) {
            stop("UV position map");
            return;
        }
        auto view = v.renderer.SurfaceUvView(*ctx, s, 128, 128);
        if (!view) {
            stop("view UV addresses");
            return;
        }
        auto sample = [&](const char* what, gpu::Texture* tex, float2 screen, float expected) {
            float2 uv;
            if (!PickMeshUv(s, screen, 128, 128, uv) || !tex) {
                check(false, what);
                return;
            }
            reads.emplace_back(what, v.renderer.ReadPixel(*ctx, tex, (int)(uv.x * 128), (int)(uv.y * 128)));
            expectations.push_back(expected);
        };
        std::vector<Id> targets;
        for (auto ch : {PbrChannel::BaseColor, PbrChannel::Roughness}) {
            Id id = MaterialChannelLayer(d, material, ch)->surface;
            auto* surf = d.GetSurface(id);
            std::vector<u16> pixels(128 * 128 * 4, 0);
            float value = ch == PbrChannel::BaseColor ? 1.0f : 0.5f;
            for (int y = 0; y < 128; ++y)
                for (int x = 0; x < 128; ++x)
                    for (int k = 0; k < 4; ++k)
                        pixels[(y * 128 + x) * 4 + k] = k == 3 ? 0x3c00 : x >= 64 ? (value == 1 ? 0x3c00 : 0x3800) : 0;
            ctx->Upload(surf->tex.get(), pixels.data(), 128 * 8);
            d.TouchSurface(id);
            targets.push_back(id);
        }
        const float2 src{49, 64}, dst{78, 64};
        BrushSettings b;
        b.size = 14;
        b.opacity = b.flow = b.hardness = 1;
        if (!v.renderer.BeginStrokeMulti(d, *ctx, targets)) {
            stop("begin multi-channel retouch");
            return;
        }
        check(v.renderer.RetouchSurface(d, *ctx, s, view, dst, dst, src - dst, b, 1, false, {}), "clone dispatch");
        v.renderer.EndStrokeMulti(d, *ctx, v.history, "Test clone");
        sample("clone color across seam", d.GetSurface(targets[0])->tex.get(), dst, 1);
        sample("clone roughness across seam", d.GetSurface(targets[1])->tex.get(), dst, 0.5f);
        v.history.Undo(d, *ctx);
        sample("undo clone restores material", d.GetSurface(targets[0])->tex.get(), dst, 0);
        v.history.Redo(d, *ctx);
        const float2 smear{86, 64};
        if (!v.renderer.BeginStrokeMulti(d, *ctx, targets)) {
            stop("begin smudge");
            return;
        }
        v.renderer.RetouchSurface(d, *ctx, s, view, dst, smear, dst - smear, b, 1, true, {});
        v.renderer.EndStrokeMulti(d, *ctx, v.history, "Test smudge");
        sample("smudge transports color", d.GetSurface(targets[0])->tex.get(), smear, 1);
        sample("smudge transports roughness", d.GetSurface(targets[1])->tex.get(), smear, 0.5f);
        std::vector<gpu::TexturePtr> merged;
        for (float value : {0.25f, 0.75f}) {
            auto image = gpu::Dev().CreateTexture(128, 128, gpu::Format::RGBA16F, "TestMergedChannel");
            v.renderer.FillTexture(*ctx, image.get(), {value, value, value, 1});
            merged.push_back(std::move(image));
        }
        const float2 mergedDest{78, 82};
        if (!v.renderer.BeginStrokeMulti(d, *ctx, targets)) {
            stop("begin merged-sample clone");
            return;
        }
        v.renderer.RetouchSurface(d, *ctx, s, view, mergedDest, mergedDest, src - mergedDest, b, 1, false, merged);
        v.renderer.EndStrokeMulti(d, *ctx, v.history, "Test merged sample");
        sample("visible composite supplies color", d.GetSurface(targets[0])->tex.get(), mergedDest, 0.25f);
        sample("visible composite supplies roughness", d.GetSurface(targets[1])->tex.get(), mergedDest, 0.75f);
        auto coverage = gpu::Dev().CreateTexture(128, 128, gpu::Format::R8, "TestMask");
        std::vector<u8> full(128 * 128, 255);
        ctx->Upload(coverage.get(), full.data(), 128);
        v.renderer.BrushSurfaceMask(d, *ctx, s, view, coverage.get(), mergedDest, mergedDest, b, false);
        v.renderer.SelectFrom(d, *ctx, v.history, coverage.get(), SelSource::Mask, SelMode::Replace, {}, 0, "Test mask");
        sample("mask protects brushed surface", d.Selection()->tex.get(), mergedDest, 0);
        sample("mask leaves other side writable", d.Selection()->tex.get(), src, 1);
        if (!v.renderer.BeginStrokeMulti(d, *ctx, targets)) {
            stop("begin protected clone");
            return;
        }
        v.renderer.RetouchSurface(d, *ctx, s, view, mergedDest, mergedDest, src - mergedDest, b, 1, false, {});
        v.renderer.EndStrokeMulti(d, *ctx, v.history, "Test protected clone");
        sample("protected material resists another tool", d.GetSurface(targets[0])->tex.get(), mergedDest, 0.25f);
        v.renderer.BrushSurfaceMask(d, *ctx, s, view, coverage.get(), mergedDest, mergedDest, b, true);
        sample("reverse mask stroke releases protection", coverage.get(), mergedDest, 1);
        v.renderer.Deselect(d, v.history);
        Json path = Json::Object(), pts = Json::Array();
        for (auto pt : {src, dst}) {
            u32 tri;
            float3 bary;
            if (!PickMeshHit(s, pt, 128, 128, tri, bary)) {
                stop("path pick");
                return;
            }
            Json a = Json::Array();
            a.Push(tri);
            a.Push(bary.x);
            a.Push(bary.y);
            a.Push(bary.z);
            pts.Push(a);
        }
        path["points"] = pts;
        path["radius"] = 0.045f;
        path["colors"]["BaseColor"] = ToJson(float4{1, 0, 0, 1});
        if (!WriteSurfacePath(v, 0, path, "Test path")) {
            stop("path construction across UV seam");
            return;
        }
        auto pathLayer = d.ActiveLayer();
        sample("path paints first side", d.GetSurface(pathLayer->surface)->tex.get(), {60, 64}, 1);
        sample("path paints second side", d.GetSurface(pathLayer->surface)->tex.get(), {70, 64}, 1);
        const Json saved = SerializeDocument(d);
        std::string error;
        auto restored = DeserializeDocument(saved, {}, &error);
        check(restored && restored->ActiveLayer() && restored->ActiveLayer()->surfacePath == pathLayer->surfacePath,
              "project metadata round-trip restores editable path definition");
        path["spacing"] = 0.2f;
        check(WriteSurfacePath(v, pathLayer->material, path, "Test spacing"), "recalculate path using stored anchors");
        v.history.Undo(d, *ctx);
        check(d.ActiveLayer() && d.ActiveLayer()->surfacePath.find("spacing") == std::string::npos,
              "undo path edit restores control settings");

        const Id pathMaterial = d.ActiveLayer()->material;
        path["mode"] = 1;
        path["spacing"] = 0.24f;
        const u8 gray = 128;
        const auto packed = deflate::ZlibCompress(&gray, 1);
        path["ribbon"]["w"] = 1;
        path["ribbon"]["h"] = 1;
        path["ribbon"]["mask"] = net::Base64Encode(packed.data(), packed.size());
        if (!WriteSurfacePath(v, pathMaterial, path, "Test ribbon")) {
            stop("embedded ribbon");
            return;
        }
        sample("ribbon applies embedded stencil before UV seam", d.GetSurface(d.ActiveLayer()->surface)->tex.get(), {60, 64}, 0.5f);
        sample("ribbon continues after UV seam", d.GetSurface(d.ActiveLayer()->surface)->tex.get(), {70, 64}, 0.5f);
        path["spacing"] = 0;
        path["mode"] = 2;
        if (!WriteSurfacePath(v, pathMaterial, path, "Test eraser")) {
            stop("editable eraser path");
            return;
        }
        check(d.ActiveLayer()->blend == BlendMode::Erase, "eraser path stores destination-out blend");
        auto erased = gpu::Dev().CreateTexture(128, 128, gpu::Format::RGBA16F, "TestErasedComposite");
        v.renderer.Composite(d, *ctx, erased.get(), merged.front().get(), d.GetSurface(d.ActiveLayer()->surface)->tex.get(), nullptr,
                             d.ActiveLayer()->blend, 1, false, nullptr);
        sample("eraser path removes lower material", erased.get(), {60, 64}, 0);
        sample("eraser path preserves lower material outside line", erased.get(), {60, 74}, 0.25f);
        path["mode"] = 3;
        for (auto pt : {float2{78, 81}, float2{49, 81}}) {
            u32 tri;
            float3 bary;
            if (!PickMeshHit(s, pt, 128, 128, tri, bary)) {
                stop("closed path pick");
                return;
            }
            Json a = Json::Array();
            a.Push(tri);
            a.Push(bary.x);
            a.Push(bary.y);
            a.Push(bary.z);
            pts.Push(a);
        }
        path["points"] = pts;
        if (!WriteSurfacePath(v, pathMaterial, path, "Test closed fill")) {
            stop("filled path on connected faces");
            return;
        }
        auto filled = d.GetSurface(d.ActiveLayer()->surface)->tex.get();
        sample("closed path fills interior before seam", filled, {58, 72}, 1);
        sample("closed path fills interior after seam", filled, {71, 72}, 1);
        sample("closed path preserves exterior", filled, {58, 84}, 0);

        // Exercise the UV canvas input path with an active Base Color layer, first with every
        // channel disabled, then with Roughness as the only enabled channel.
        Id uvMaterial = AddPaintLayer3D(v, "UV canvas test");
        v.renderer.Prepare(d, *ctx);
        std::fill(std::begin(brush3D.use), std::end(brush3D.use), false);
        brush3D.rough = 0.5f;
        fg = {1, 0, 0, 1};
        tool = Tool::Brush;
        SelectTip(brush, "");
        brush.size = 24;
        brush.hardness = brush.opacity = brush.flow = 1;
        v.zoom = 1;
        v.pan = {};
        frameSamples.clear();
        auto& io = ImGui::GetIO();
        auto oldPos = io.MousePos;
        auto oldDown = io.MouseDown[0];
        auto oldDuration = io.MouseDownDuration[0];
        io.MousePos = {64.5f, 64.5f};
        auto press = [&](bool down) {
            io.MouseDown[0] = down;
            io.MouseDownDuration[0] = down ? 0.0f : -1.0f;
            CanvasInput(v, {0, 0}, {128, 128}, true);
        };
        const size_t steps = v.history.Count();
        press(true);
        press(false);
        check(!stroking && v.history.Count() == steps, "UV canvas with zero channels leaves history untouched");
        brush3D.use[(int)PbrChannel::Roughness] = true;
        press(true);
        press(false);
        io.MousePos = oldPos;
        io.MouseDown[0] = oldDown;
        io.MouseDownDuration[0] = oldDuration;
        auto uvPixel = [&](PbrChannel ch, const char* label, float expected) {
            Id id = MaterialChannelLayer(d, uvMaterial, ch)->surface;
            reads.emplace_back(label, v.renderer.ReadPixel(*ctx, d.GetSurface(id)->tex.get(), 64, 64));
            expectations.push_back(expected);
        };
        uvPixel(PbrChannel::BaseColor, "UV single-channel stroke preserves Base Color", 0);
        uvPixel(PbrChannel::Roughness, "UV single-channel stroke paints Roughness", SrgbToLinear(0.5f));
        tool = Tool::Clone;
        brush3D.use[(int)PbrChannel::Roughness] = false;
        brush3D.use[(int)PbrChannel::BaseColor] = true;
        retouch = b;
        const bool oldCtrl = io.KeyCtrl;
        auto surfaceClick = [&](float2 point, bool ctrl) {
            io.KeyCtrl = ctrl;
            io.MouseDown[0] = true;
            io.MouseDownDuration[0] = 0;
            SurfaceToolInput(v, point, 128, 128, true);
            io.MouseDown[0] = false;
            io.MouseDownDuration[0] = -1;
            SurfaceToolInput(v, point, 128, 128, true);
        };
        surfaceClick(src, true);
        check(v.surfaceTools.hasSource, "Ctrl-click records a point on the surface");
        auto* cloned = d.GetSurface(MaterialChannelLayer(d, uvMaterial, PbrChannel::BaseColor)->surface)->tex.get();
        cloneVisible3D = false;
        surfaceClick(dst, false);
        sample("active-layer sample skips paint on lower layers", cloned, dst, 0);
        cloneVisible3D = true;
        surfaceClick(dst, false);
        sample("visible-layer sample captures paint on lower layers", cloned, dst, 1);
        io.MouseDown[0] = oldDown;
        io.MouseDownDuration[0] = oldDuration;
        io.KeyCtrl = oldCtrl;
        // Drain a nonempty layer and lay fresh paint over three protection levels. The
        // simulation runs at quarter resolution; the final mask is sampled at document resolution.
        for (int y=0;y<128;++y) for(int x=0;x<128;++x)
            full[(size_t)y*128+x]=x<42?0:x<85?128:255;
        ctx->Upload(coverage.get(),full.data(),128);
        v.renderer.SelectFrom(d,*ctx,v.history,coverage.get(),SelSource::Mask,SelMode::Replace,{},0,"Fluid selection");
        v.renderer.FillTexture(*ctx,cloned,{0.5f,0.5f,0.5f,1});
        d.TouchSurface(MaterialChannelLayer(d,uvMaterial,PbrChannel::BaseColor)->surface);
        fluidSet.simRes=3;
        if(!FluidStart(v)){stop("masked fluid simulation on mesh");return;}
        Renderer::FluidParams liquid;
        liquid.wetGloss=0;
        liquid.color={1,0,0,1};
        liquid.density=8;
        liquid.coat=4;
        liquid.hardness=1;
        v.renderer.FluidGravity(*ctx,liquid);
        v.renderer.FluidLoadLayers(*ctx,liquid,0); // No liquid remains at these texels after draining.
        v.renderer.FluidComposite(d,*ctx,liquid,true);
        auto liquidSample=[&](const char* label,int x,float expected){
            reads.emplace_back(label,v.renderer.ReadPixel(*ctx,cloned,x,64));
            expectations.push_back(expected);
        };
        liquidSample("drained fluid preserves protected original",20,0.5f);
        liquidSample("drained fluid preserves fractional original",72,0.25f);
        liquidSample("drained fluid clears unprotected original",100,0);
        v.renderer.FluidDeposit(*ctx,{{0,0,0,5}},{{1,0,0,-1}},true,false,liquid);
        v.renderer.FluidComposite(d,*ctx,liquid,true);
        liquidSample("wet fluid preserves protected original",20,0.5f);
        liquidSample("wet fluid follows feathered protection",72,0.75f);
        liquidSample("wet fluid reaches unprotected surface",100,1);
        FluidFinish(false);
        v.renderer.Deselect(d,v.history);
        auto* emissiveLayer = MaterialChannelLayer(d, uvMaterial, PbrChannel::Emissive);
        if (!emissiveLayer) {
            stop("emissive channel material layer");
            return;
        }
        v.renderer.FillTexture(*ctx, d.GetSurface(emissiveLayer->surface)->tex.get(),
                               {SrgbToLinear(0.2f), SrgbToLinear(0.6f), SrgbToLinear(0.8f), 1});
        d.TouchSurface(emissiveLayer->surface);
        brush3D.emissive = {};
        StartMaterialPick(v, src, 128, 128);
        return;
    }
    if (reads.empty() || autoQuit) return;
    for (const auto& r : reads)
        if (r.second && !r.second->ready) return;
    PollMaterialPick();
    if (matPick.active) return;
    check(std::abs(brush3D.emissive.x - 0.2f) < 0.01f && std::abs(brush3D.emissive.y - 0.6f) < 0.01f &&
              std::abs(brush3D.emissive.z - 0.8f) < 0.01f,
          "material eyedropper restores emissive RGB values");
    for (size_t i = 0; i < reads.size(); ++i) {
        auto& [label, r] = reads[i];
        float value = -1;
        if (r && r->format == gpu::Format::RGBA16F && r->data.size() >= 8) {
            u16 x;
            memcpy(&x, r->data.data(), 2);
            value = HalfToFloat(x);
        } else if (r && r->format == gpu::Format::R8 && !r->data.empty())
            value = r->data[0] / 255.0f;
        check(std::abs(value - expectations[i]) < 0.06f, label);
        if (std::abs(value - expectations[i]) >= 0.06f) Log("[surface-tools] value %.4f expected %.4f", value, expectations[i]);
    }
    Log("[surface-tools] COMPLETE %d failures", failures);
    reads.clear();
    autoQuit = true;
}
} // namespace sx
