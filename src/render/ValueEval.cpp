// Value pins on the CPU: constants, operations, function inputs and variables, values produced
// by an execution flow; effect parameters linked to value pins.
#include "render/Renderer.h"
#include "doc/GraphTypes.h"

namespace sx {

Value Renderer::InputValue(Document& doc, const Node& n, int i, int depth) {
    const PinDesc pin = NodeInputPin(n, i);
    if (i < (int)n.inputs.size() && n.inputs[i]) {
        const Graph& g = EvalGraph(doc);
        const Node* src = g.Find(n.inputs[i]);
        if (src) return ConvertValue(EvalValue(doc, src->id, NodeOutputPortOf(*src, InputChannel(n, i)), depth + 1), pin.type, pin.array);
    }
    return NodePinDefault(n, i);
}

Value Renderer::EvalValue(Document& doc, Id id, int port, int depth) {
    if (!id || depth > 256) return {};
    const u64 key = ((u64)id << 8) | (u64)(port & 0xFF);
    if (auto it = runtimeValues.find(key); it != runtimeValues.end()) return it->second;
    if (auto it = frameValues.find(key); it != frameValues.end()) return it->second;
    frameValues[key] = {};  // cycle guard
    const Node* n = EvalGraph(doc).Find(id);
    if (!n) return {};
    Value r;
    switch (n->kind) {
        case NodeKind::Value:
        case NodeKind::SubInput: r = n->value; break;
        case NodeKind::Split:
        case NodeKind::Reroute:
        case NodeKind::SubOutput:
        case NodeKind::Function:
            if (!n->inputs.empty() && n->inputs[0])
                if (const Node* src = EvalGraph(doc).Find(n->inputs[0])) r = EvalValue(doc, src->id, NodeOutputPortOf(*src, InputChannel(*n, 0)), depth + 1);
            break;
        case NodeKind::Math:
            if (auto* d = FindMathOp(n->op)) {
                std::vector<Value> args;
                for (int i = 0; i < (int)d->ins.size(); ++i) args.push_back(InputValue(doc, *n, i, depth));
                auto outs = d->fn(args);
                if (port < (int)outs.size()) r = outs[port];
            }
            break;
        case NodeKind::VarGet:
        case NodeKind::VarSet:
            if (const GraphVariable* v = doc.S().graph.Var(n->op)) r = ConvertValue(v->value, n->value.type, n->value.array);
            else r = n->value;
            break;
        default: {
            const PinDesc out = NodeOutputPin(*n, port);
            r = MakeValue(out.type, out.array);
            break;
        }
    }
    frameValues[key] = r;
    return r;
}

std::vector<float4> Renderer::EffectParams(Document& doc, const Node& n) {
    AlignParamPins(n);
    std::vector<float4> p = n.fx.params;
    auto def = n.fx.Def();
    if (!def || n.paramPins.empty()) return p;
    // Pins numbered as NodeSignature does: entries out of range have no pin.
    int pin = (int)def->inputs.size() - 1;
    for (size_t j = 0; j < n.paramPins.size(); ++j) {
        const int k = n.paramPins[j];
        if (k < 0 || k >= (int)def->params.size()) continue;
        ++pin;
        if (pin >= (int)n.inputs.size() || !n.inputs[pin]) continue;
        if ((int)p.size() <= k) p.resize(k + 1, def->params[k].def);
        Value v = InputValue(doc, n, pin, 0);
        p[k] = v.f;
    }
    return p;
}

}  // namespace sx
