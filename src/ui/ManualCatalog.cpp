// Export the compiled node registries for the offline reference and its checks.
// Only the isolated --demo documentation fixture calls this; no OS input.
#include "doc/EffectGraph.h"
#include "doc/GraphTypes.h"

namespace sx {
Json DocumentationRuntimeCatalog() {
    Json root=Json::Object();
    auto pins=[](const std::vector<PinDesc>& source) {
        Json result=Json::Array();
        for (const auto& pin:source) {
            Json p=Json::Object();p["name"]=pin.name;p["type"]=PinTypeLabel(pin.type);p["array"]=pin.array;result.Push(p);
        }
        return result;
    };
    Json shader=Json::Array();
    for (const auto& op:EffectGraphOps()) {
        Json item=Json::Object();item["id"]=op.op;item["label"]=op.label;item["category"]=op.category;
        item["outputSize"]=op.fixedOutType;item["hasValue"]=op.hasValue;
        Json ins=Json::Array(),outs=Json::Array(),kinds=Json::Array();
        for (const auto& pin:op.inputs) {Json p=Json::Object();p["name"]=pin.name;p["default"]=pin.defValue;ins.Push(p);}
        for (const auto& pin:op.outputs) outs.Push(pin);
        for (const auto& kind:{EGKind::Effect,EGKind::Blend,EGKind::Brush}) if (OpAllowed(op,kind)) kinds.Push(static_cast<int>(kind));
        item["inputs"]=ins;item["outputs"]=outs;item["kinds"]=kinds;shader.Push(item);
    }
    root["shader"]=shader;
    auto typed=[&](const auto& operations,bool execution) {
        Json result=Json::Array();
        for (const auto& op:operations) {
            Json item=Json::Object();item["id"]=op.id;item["label"]=op.label;item["keywords"]=op.keywords;
            item["inputs"]=pins(op.ins);item["outputs"]=pins(op.outs);
            Json defaults=Json::Array();for (const auto& value:op.defaults) defaults.Push(ValueToJson(value));
            item["defaults"]=defaults;result.Push(item);
        }
        return result;
    };
    root["math"]=typed(MathOps(),false);root["execution"]=typed(ExecActions(),true);
    Json nodes=Json::Array();
    const char* kinds[]={"Output","Surface","Effect","Blend","Split","Tool","Function","SubInput","SubOutput","Value","Math","VarGet","VarSet","Event","Action","Reroute"};
    for (int i=0;i<static_cast<int>(std::size(kinds));++i) {
        Node node;node.kind=static_cast<NodeKind>(i);std::vector<PinDesc> ins,outs;NodeSignature(node,&ins,&outs);
        Json item=Json::Object();item["id"]=kinds[i];item["inputs"]=pins(ins);item["outputs"]=pins(outs);nodes.Push(item);
    }
    root["nodes"]=nodes;
    return root;
}
} // namespace sx
