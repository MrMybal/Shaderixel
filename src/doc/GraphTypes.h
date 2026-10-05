#pragma once
// Typed graph nodes: value operations (Math nodes) and execution flow steps (Action nodes).
// Both are tables: adding an operation or an action is one entry here (GraphTypes.cpp) plus, for
// actions, its step in the interpreter (ui/GraphExec.cpp).
#include "doc/Document.h"

#include <functional>

namespace sx {

struct MathOpDef {
    std::string id, label, category, keywords;
    std::vector<PinDesc> ins;
    std::vector<Value> defaults;  // per input (unlinked value)
    std::vector<PinDesc> outs;
    std::function<std::vector<Value>(const std::vector<Value>&)> fn;  // inputs already converted to their pin types
};
const std::vector<MathOpDef>& MathOps();
const MathOpDef* FindMathOp(const std::string& id);

struct ExecActionDef {
    std::string id, label, keywords, desc;
    std::vector<PinDesc> ins;     // [0] is the execution input
    std::vector<Value> defaults;  // per input
    std::vector<PinDesc> outs;
    int passthrough = -1;         // image input shown until a result exists
    int resultOut = -1;           // image output carrying the stored result
};
const std::vector<ExecActionDef>& ExecActions();
const ExecActionDef* FindExecAction(const std::string& id);

// New typed node (Value: type / array, Math / Action: op, VarGet / VarSet: variable name, Event: name).
Node& AddTypedNode(Document& d, NodeKind kind, const std::string& op, PinType type, bool array, float2 pos);

// Value of an unlinked value input: the node's pinValues, else the default of the pin.
Value NodePinDefault(const Node& n, int i);

// JSON of values (documents, library).
class Json;
Json ValueToJson(const Value& v);
Value ValueFromJson(const Json& j);

}  // namespace sx
