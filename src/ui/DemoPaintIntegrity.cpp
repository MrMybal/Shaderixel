// GPU regression for exact undo across scattered UV regions and grouped projections.
#include "ui/App.h"
#include "render/Dispatch.h"
#include "mesh/UvUnwrap.h"

namespace sx {
void App::DemoPaintIntegrity() {
    static std::vector<Id> targets;
    static std::vector<gpu::ReadbackPtr> baseline, painted, undone, redone;
    static gpu::ReadbackPtr metal, rough;
    static std::vector<gpu::ReadbackPtr> uvEdits, uvMasks;
    static std::vector<gpu::ReadbackPtr> anchoredMasks;
    static std::vector<gpu::ReadbackPtr> viewportChecks;
    static std::unique_ptr<PtRenderState> cutoutRender;
    static gpu::ReadbackPtr tracedGpu;
    constexpr int frameSize=64;
    constexpr int columns[]={14,32,50};
    static Id projected = 0;
    static const int resolution = getenv("SX_HISTORY_4K") ? 4096 : 512;
    static int state = 0, failures = 0;
    auto check = [&](bool pass, const char* label) {
        if (!pass) { ++failures; ++uiErrorCount; }
        Log("[paint-integrity] %s %s", pass ? "PASS" : "FAIL", label);
    };
    if (frameCounter < 4) return;
    if (frameCounter > 240) { check(false,"GPU timeout"); autoQuit=true; return; }
    if (!state) {
        for (auto file : {"clear.slang","clear_r.slang","brush.slang"}) Kernel(file);
        NewDocument(resolution,resolution,Precision::Half16,DocMode::Layers,0,"Paint integrity");
        auto& v=*Active();
        auto& d=*v.doc;
        d.kind=1;
        d.pbrChannels = (1u << (int)PbrChannel::BaseColor) | (1u << (int)PbrChannel::Roughness) |
                        (1u << (int)PbrChannel::Height) | (1u << (int)PbrChannel::User2);
        AttachMesh(v,MakeSphere());
        CreatePbrChannels(v,true);
        const Id paint=AddPaintLayer3D(v,"Paint");
        for (auto c : {PbrChannel::BaseColor,PbrChannel::Height,PbrChannel::User2}) {
            auto* layer=MaterialChannelLayer(d,paint,c);
            if (!layer) { check(false,"missing channel"); autoQuit=true; return; }
            targets.push_back(layer->surface);
        }
        v.history.Clear();
        v.renderer.Prepare(d,*ctx);
        v.renderer.MeshPrepare(d,*ctx,*v.mesh);
        state=1;
        return;
    }
    auto& v=*Active();
    auto& d=*v.doc;
    auto& s=*v.mesh;
    auto capture=[&](auto& out) { for (Id id:targets) out.push_back(ctx->ReadbackTexture(d.GetSurface(id)->tex.get())); };
    auto ready=[](const auto& reads) { return std::all_of(reads.begin(),reads.end(),[](const auto& r){return r && r->ready;}); };
    if (state==1 && s.brushTileBounds && s.brushTileBounds->ready) {
        s.yaw=s.pitch=0;
        s.ortho=true;
        v.renderer.MeshRender(*ctx,s,GatherMaterial(v),256,256);
        capture(baseline);
        if (!v.renderer.BeginStrokeMulti(d,*ctx,targets)) { check(false,"begin stroke"); autoQuit=true; return; }
        v.renderer.meshSymmetry=1;
        BrushSettings b;
        b.flow=b.opacity=b.hardness=1;
        std::vector<float4> colors{{1,0,0,1},{0.8f,0.8f,0.8f,1},{0.2f,0.2f,0.2f,1}};
        v.renderer.MeshStrokeDabsMulti(d,*ctx,s,{{74,120,8,1},{117,153,8,1}},b,colors,false);
        v.renderer.EndStrokeMulti(d,*ctx,v.history,"Three channels");
        v.renderer.meshSymmetry=0;
        capture(painted);
        state=2;
    } else if (state==2 && ready(painted)) {
        v.history.Maintain(*ctx);
        Log("[paint-integrity] %d px, %zu channels, undo history %.2f MiB, whole snapshots %.2f MiB",resolution,targets.size(),
            v.history.MemoryBytes()/1048576.0,(double)resolution*resolution*16*targets.size()/1048576.0);
        check(v.history.MemoryBytes() < (u64)resolution*resolution*8*targets.size(),"compact history below half of full snapshots");
        v.history.Undo(d,*ctx);
        capture(undone);
        v.history.Redo(d,*ctx);
        capture(redone);
        state=3;
    } else if (state==3 && ready(redone) && ready(baseline) && ready(undone)) {
        for (size_t i=0; i<targets.size(); ++i) {
            check(baseline[i]->data!=painted[i]->data,"requested channel painted");
            check(undone[i]->data==baseline[i]->data,"all pixels identical after undo");
            check(redone[i]->data==painted[i]->data,"all pixels identical after redo");
        }
        baseline.clear(); painted.clear(); undone.clear(); redone.clear();
        auto image=gpu::Dev().CreateTexture(1,1,gpu::Format::RGBA8,"ProjectionFixture");
        const u8 red[]={255,0,0,255};
        ctx->Upload(image.get(),red,4);
        projImages["fixture-red"]=image;
        projected=AddProjectedMaterial(v,"fixture-red","Projected");
        CreateFolder3D(v,projected);
        for (auto& l:d.S().layers) if (l.material==projected) {
            Json p;
            Json::Parse(l.projection,p);
            p["scale"]=3; p["metal"]=0; p["roughFromImage"]=false; p["rough"]=0.5f;
            l.projection=p.Dump(0);
        }
        v.renderer.Prepare(d,*ctx);
        UpdateProjections(v);
        auto read=[&](PbrChannel c) {
            auto* layer=MaterialChannelLayer(d,projected,c);
            return v.renderer.ReadPixel(*ctx,d.GetSurface(layer->surface)->tex.get(),resolution/2,resolution/2);
        };
        metal=read(PbrChannel::Metallic);
        rough=read(PbrChannel::Roughness);
        state=4;
    } else if (state==4 && metal && rough && metal->ready && rough->ready) {
        auto value=[](const auto& rb) { u16 r; memcpy(&r,rb->data.data(),sizeof(r)); return LinearToSrgb(HalfToFloat(r)); };
        check(value(metal)<0.01f && std::abs(value(rough)-0.5f)<0.01f,"nested projected roughness and metallic");
        check(!IsPaintLayer3D(d,projected),"projected material targets its mask");
        auto a=AddMaterialLayerValues(v,"Anchor A",nullptr,&brush3D.rough,nullptr,nullptr,true);
        auto b=AddMaterialLayerValues(v,"Anchor B",nullptr,&brush3D.rough,nullptr,nullptr,true);
        for (auto& l:d.S().layers) if (l.material==a || l.material==b) {
            Json g=Json::Object(); g["type"]=8; g["source"]=(int)(l.material==a?b:a); l.generator=g.Dump(0);
        }
        UpdateSmartMasks(v);
        const Id mask=AnyMaterialLayer(d,a)->mask;
        auto previous=d.GetSurface(mask)->version;
        UpdateSmartMasks(v);
        check(d.GetSurface(mask)->version==previous,"legacy anchor cycle stabilizes");

        NewDocument(64,64,Precision::Half16,DocMode::Layers,3,"UV correction history");
        auto& uvView = *Active();
        auto& uvDoc = *uvView.doc;
        uvDoc.kind = 1;
        uvDoc.pbrChannels = 1;
        AttachMesh(uvView, MakePlane());
        CreatePbrChannels(uvView, true);
        const float3 color{1,1,1};
        const Id material = AddMaterialLayerValues(uvView, "UV mark", &color, nullptr, nullptr, nullptr, true);
        const Id correction = uvDoc.CreateSurface({});
        const Id output = MaterialChannelLayer(uvDoc, material, PbrChannel::BaseColor)->mask;
        for (auto& layer : uvDoc.S().layers) if (layer.material == material) {
            layer.maskEdit = correction;
            layer.generator = R"({"type":7,"invert":true,"noise":0})";  // black except for the painted white detail
        }
        uvView.renderer.Prepare(uvDoc, *ctx);
        auto* editTexture = uvDoc.GetSurface(correction)->tex.get();
        auto* outputTexture = uvDoc.GetSurface(output)->tex.get();
        uvView.renderer.FillTexture(*ctx, editTexture, {1,1,1,1}, {13,20,20,27});
        uvDoc.TouchSurface(correction);
        uvView.history.Clear();
        auto captureUv = [&] {
            uvView.renderer.MeshPrepare(uvDoc, *ctx, *uvView.mesh);
            // A stale or skipped regeneration leaves this recognizable gray at both sample points.
            uvView.renderer.FillTexture(*ctx, outputTexture, {0.25f,0.25f,0.25f,1});
            UpdateSmartMasks(uvView);
            uvEdits.push_back(ctx->ReadbackTexture(editTexture));
            uvMasks.push_back(ctx->ReadbackTexture(outputTexture));
        };
        captureUv();
        uvJob = std::make_shared<UvJob>();
        uvJob->input = uvView.mesh->data;
        auto& result = uvJob->result;
        result.mesh = uvJob->input;
        result.charts = 1;
        for (u32 i = 0; i < result.mesh.vertices.size(); ++i) {
            result.sourceVertex.push_back(i);
            result.mesh.vertices[i].uv.x = 1.0f - result.mesh.vertices[i].uv.x;
        }
        uvTool.hasResult = uvTool.transfer = true;
        ApplyUvUnwrap(uvView);
        check(uvView.mesh->data.vertices[0].uv == result.mesh.vertices[0].uv, "UV layout applied");
        captureUv();
        Undo();
        check(uvView.mesh->data.vertices[0].uv == uvJob->input.vertices[0].uv, "UV layout restored by undo");
        captureUv();
        Redo();
        check(uvView.mesh->data.vertices[0].uv == result.mesh.vertices[0].uv, "UV layout restored by redo");
        captureUv();
        state = 5;
    } else if (state==5 && ready(uvEdits) && ready(uvMasks)) {
        auto markAt = [](const gpu::ReadbackPtr& rb, int x) {
            if (rb->format != gpu::Format::RGBA16F || rb->width != 64 || rb->height != 64 || rb->data.size() != 64*64*8) return -1.0f;
            u16 value;
            memcpy(&value, rb->data.data() + (23*64+x)*8, sizeof(value));
            return HalfToFloat(value);
        };
        auto verify = [&](const auto& snapshots, const char* positionLabel, const char* historyLabel) {
            bool correct = snapshots.size() == 4;
            for (size_t i = 0; correct && i < snapshots.size(); ++i) {
                const int white = i % 2 == 0 ? 16 : 47;
                correct = markAt(snapshots[i], white) > 0.99f && std::abs(markAt(snapshots[i], 63-white)) < 0.01f;
            }
            check(correct, positionLabel);
            check(snapshots.size() == 4 && snapshots[0]->data == snapshots[2]->data && snapshots[1]->data == snapshots[3]->data, historyLabel);
        };
        verify(uvEdits, "painted maskEdit detail follows UV transfer, undo and redo", "maskEdit snapshots identical on undo and redo");
        verify(uvMasks, "regenerated smart mask follows painted UV detail", "regenerated masks identical on undo and redo");
        // Equal alpha on both writes distinguishes a height input from a coverage input.
        d.pbrChannels |= 1u << (int)PbrChannel::Height;
        CreatePbrChannels(v, true);
        const Id rivets = AddPaintLayer3D(v, "Rivets");
        const float3 white{1,1,1};
        const Id wear = AddMaterialLayerValues(v, "Wear", &white, nullptr, nullptr, nullptr, true);
        const Id wearMask = AnyMaterialLayer(d, wear)->mask;
        const Id heightSurface = MaterialChannelLayer(d, rivets, PbrChannel::Height)->surface;
        Json anchor = Json::Object();
        anchor["type"] = 8;
        anchor["channel"] = (int)PbrChannel::Height;
        anchor["source"] = (int)rivets;
        anchor["radius"] = 0;
        anchor["noise"] = 0;
        for (auto& layer : d.S().layers)
            if (layer.material == wear) layer.generator = anchor.Dump(0);
        v.renderer.Prepare(d, *ctx);
        for (float value : {1.0f, 0.0f}) {
            v.renderer.FillTexture(*ctx, d.GetSurface(heightSurface)->tex.get(), {value,value,value,1});
            d.TouchSurface(heightSurface);
            UpdateSmartMasks(v);
            anchoredMasks.push_back(v.renderer.ReadPixel(*ctx, d.GetSurface(wearMask)->tex.get(), 32, 32));
        }
        NewDocument(64,64,Precision::Half16,DocMode::Layers,0,"Opacity cutoff and user data");
        auto& p=*Active();
        auto& doc=*p.doc;
        doc.kind=1;
        doc.pbrChannels=1u|(1u<<(int)PbrChannel::Opacity)|(1u<<(int)PbrChannel::User2);
        MeshData plane=MakePlane();
        for(auto& vertex:plane.vertices) {vertex.pos={vertex.pos.x,vertex.pos.z,0}; vertex.normal={0,0,-1};}
        NormalizeMesh(plane);
        AttachMesh(p,std::move(plane));
        CreatePbrChannels(p,true);
        const Id paint=AddPaintLayer3D(p,"Cutout");
        p.renderer.Prepare(doc,*ctx);
        auto paintBands=[&](PbrChannel channel,std::array<float,3> visual) {
            auto* layer=MaterialChannelLayer(doc,paint,channel);
            for(int i=0;i<3;++i) {
                const float value=SrgbToLinear(visual[i]);
                p.renderer.FillTexture(*ctx,doc.GetSurface(layer->surface)->tex.get(),{value,value,value,1},{i*64/3,0,(i+1)*64/3,64});
            }
            doc.TouchSurface(layer->surface);
        };
        paintBands(PbrChannel::Opacity,{0.49f,0.5f,0.51f});
        paintBands(PbrChannel::User2,{0.125f,0.375f,0.875f});
        p.renderer.EvaluateComplete(doc,*ctx);
        auto& model=*p.mesh;
        model.ortho=true;
        model.yaw=model.pitch=0;
        model.distance=2;
        model.target={};
        model.envAtlas.reset(); model.envPixels.clear();
        model.envBackground=model.groundShadow=model.lightFollow=false;
        model.lightYaw=model.lightPitch=0;
        model.quality=1; model.ssao=0;
        p.renderer.MeshPrepare(doc,*ctx,model);
        model.viewChannel=0;
        p.renderer.MeshRender(*ctx,model,GatherMaterial(p),frameSize,frameSize);
        if(!model.color || !model.shadowMap) {check(false,"cutout viewport setup");autoQuit=true;return;}
        viewportChecks.push_back(ctx->ReadbackTexture(model.color.get()));
        // Sample the light's depth as an ordinary SRV, the same way the shadow receiver does.
        const float cover=1.05f*Length(model.data.boundsMax-model.data.boundsMin)*0.5f;
        auto depth=gpu::Dev().CreateTexture(1,1,gpu::Format::RGBA16F,"ShadowDepthProbe");
        for(int x:columns) {
            gpu::DispatchDesc sample;
            sample.pipeline=Kernel("channel_extract.slang");
            sample.srv[0]=model.shadowMap.get();sample.uav[0]=depth.get();
            const float wx=((x+0.5f)/frameSize*2-1)*model.OrthoHalfHeight();
            const int px=(int)((0.5f+wx/(2*cover))*model.shadowMap->Width());
            struct {u32 w,h;int x,y;u32 channel;} root{1,1,px,(int)model.shadowMap->Height()/2,1};
            SetConstants(sample,root);
            Dispatch2D(*ctx,sample,1,1);
            viewportChecks.push_back(ctx->ReadbackTexture(depth.get()));
        }
        for(int channel:{12,15}) {
            model.viewChannel=channel;
            auto* display=p.renderer.MeshRender(*ctx,model,GatherMaterial(p),frameSize,frameSize);
            if(!display){ check(false,"channel display image"); autoQuit=true;return; }
            viewportChecks.push_back(ctx->ReadbackTexture(display));
        }
        cutoutRender=std::make_unique<PtRenderState>();
        auto& r=*cutoutRender;
        PtObject o;
        o.type=PtObjectType::Project;
        o.material.useProject=true;
        o.material.emission={1,0,0}; o.material.emissionStrength=1;
        r.objects.push_back(o);
        r.distance=model.distance; r.yaw=r.pitch=0; r.settings.ortho=true; r.settings.fov=model.fov;
        r.settings.hdri.clear(); r.settings.background={0,0,1};
        r.settings.maxSamples=r.settings.maxBounces=1; r.settings.denoise=0;
        state=6;
    } else if(state==6 && ready(viewportChecks) && ready(anchoredMasks)) {
        auto halfAt=[](const gpu::ReadbackPtr& r,int x,int y,int component){
            u16 value=0;
            const size_t pos=((size_t)y*r->width+x)*8+component*2;
            if(r->format!=gpu::Format::RGBA16F||pos+2>r->data.size()) return -1.f;
            memcpy(&value,r->data.data()+pos,2);
            return HalfToFloat(value);
        };
        auto displayAt=[](const gpu::ReadbackPtr& r,int x) {
            const size_t pos=((size_t)32*r->width+x)*4;
            return r->format==gpu::Format::RGBA8 && pos<r->data.size()?r->data[pos]/255.f:-1.f;
        };
        check(halfAt(anchoredMasks[0],0,0,0)>0.99f && halfAt(anchoredMasks[1],0,0,0)<0.01f,
              "wear mask follows painted height edits at constant alpha");
        const float expectedUser[]={0.125f,0.375f,0.875f};
        const char* viewportLabels[]={"raster discards opacity 0.49","raster keeps opacity 0.50","raster keeps opacity 0.51"};
        const char* shadowLabels[]={"cutout casts no shadow at 0.49","cutout casts shadow at 0.50","cutout casts shadow at 0.51"};
        for(int i=0;i<3;++i) {
            const auto alpha=halfAt(viewportChecks[0],columns[i],32,3), z=halfAt(viewportChecks[i+1],0,0,0);
            check(i ? alpha>0.99f : alpha>=0 && alpha<0.01f,viewportLabels[i]);
            check(i ? z>=0 && z<0.99f : z>0.99f,shadowLabels[i]);
            check(std::abs(displayAt(viewportChecks[4],columns[i])-(0.49f+0.01f*i))<0.007f,"viewport displays painted opacity value");
            check(std::abs(displayAt(viewportChecks[5],columns[i])-expectedUser[i])<0.007f,"viewport displays selected user channel value");
        }
        state=7;
    } else if(state==7 || state==8) {
        auto& r=*cutoutRender;
        UpdateRenderState(v,r,frameSize,frameSize);
        if(state==7 && r.gpu.Samples()==1) {
            tracedGpu=ctx->ReadbackTexture(r.gpu.accum.get());
            r.settings.device=1;state=8;
        } else if(state==8 && r.cpuShown==1 && tracedGpu && tracedGpu->ready) {
            const char* gpuLabels[]={"GPU ray sees background at 0.49","GPU ray hits surface at 0.50","GPU ray hits surface at 0.51"};
            const char* cpuLabels[]={"CPU ray sees background at 0.49","CPU ray hits surface at 0.50","CPU ray hits surface at 0.51"};
            for(int i=0;i<3;++i) {
                const size_t index=32*frameSize+columns[i];
                float4 gpuValue{};
                if(tracedGpu->format==gpu::Format::RGBA32F && tracedGpu->data.size()>=(index+1)*sizeof(gpuValue))
                    memcpy(&gpuValue,tracedGpu->data.data()+index*sizeof(gpuValue),sizeof(gpuValue));
                auto expected=[i](float4 p){return i?p.x>0.95f && p.z<0.05f:p.z>0.95f && p.x<0.05f;};
                check(expected(gpuValue),gpuLabels[i]);
                check(index<r.cpuPixels.size() && expected(r.cpuPixels[index]),cpuLabels[i]);
            }
            cutoutRender.reset();
            Log("[paint-integrity] COMPLETE %d failures",failures);
            autoQuit=true;
        }
    }
}
}
