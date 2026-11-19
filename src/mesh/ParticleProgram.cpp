#include "mesh/ParticleProgram.h"

#include <charconv>
#include <cctype>

namespace sx::particles {
namespace {
constexpr size_t kLocals = 16, kInstructions = 256, kSourceBytes = 16 * 1024;
constexpr size_t kMaxOptions = 16;
constexpr float kNumberLimit = 1e6f;
constexpr std::string_view fields[] = {
    "x","y","z","vx","vy","vz","nx","ny","nz","radius","life","alpha","age","speed","time","dt",
    "on_surface","alive","ax","ay","az","gravity","impact_radius","strength","response","restitution"
};
static_assert(std::size(fields) == FieldCount);
constexpr std::string_view stageNames[] = {"emit","update","impact"};
constexpr std::string_view controlNames[] = {"slider","checkbox","choice"};
constexpr std::pair<std::string_view,float> constants[] = {
    {"true",1.f},{"false",0.f},{"pi",3.14159265358979323846f},
    {"NATIVE",(float)Native},{"SLIDE",(float)Slide},{"STOP",(float)Stop},{"BOUNCE",(float)Bounce}
};

bool Writable(Stage stage, Field field) {
    if (stage == Stage::Impact) return field >= ImpactRadius;
    if (field >= VX && field <= VZ) return true;
    if (field == Radius || field == Life || field == Alpha || field == Alive) return true;
    if (stage == Stage::Emit) return field <= Z || field == OnSurface;
    return field >= AX && field <= Gravity;
}
}

float Control::Restore(double value) const {
    if (!std::isfinite(value)) return initial;
    switch (kind) {
        case ControlKind::Slider: return (float)std::clamp(value,(double)min,(double)max);
        case ControlKind::Checkbox: return value == 0 || value == 1 ? (float)value : initial;
        case ControlKind::Choice: {
            if (std::abs(value) > kNumberLimit) return initial;
            const auto match = std::ranges::find(options,(float)value,&ChoiceOption::value);
            return match == options.end() ? initial : match->value;
        }
    }
    return initial;
}

class Compiler {
    using Op = Program::Op;
    using Instruction = Program::Instruction;
    enum class Kind { End, Identifier, Number, Label, Symbol };
    struct Token { std::string_view text; Kind kind; int line; };
    struct Operator { std::string_view text; int precedence; Op op; };
    struct Function { std::string_view name; int args; Op op; };
    std::string_view source;
    size_t pos = 0;
    int line = 0;
    Token token{};
    Diagnostic& error;
    Stage stage = Stage::Emit;
    std::vector<Instruction>* code = nullptr;
    Program* program = nullptr;
    std::vector<std::string_view> locals;
    bool failed = false;
    bool readingVisibility = false;
    int depth = 0;

    void Fail(const char* message, int atLine = -1) {
        if (failed) return;
        error = {atLine < 0 ? token.line : atLine,message};
        failed = true;
    }

    void Next() {
        while (pos < source.size()) {
            if (source.substr(pos,2) == "//") {
                while (pos < source.size() && source[pos] != '\n') ++pos;
            } else if (std::isspace((unsigned char)source[pos])) {
                if (source[pos++] == '\n') ++line;
            } else break;
        }
        if (pos == source.size()) { token = {{},Kind::End,line}; return; }
        const auto start = pos++;
        const auto c = (unsigned char)source[start];
        Kind kind = Kind::Symbol;
        if (std::isalpha(c) || c == '_') {
            kind = Kind::Identifier;
            while (pos < source.size() && (std::isalnum((unsigned char)source[pos]) || source[pos] == '_')) ++pos;
        } else if (c == '"') {
            while (pos < source.size() && source[pos] != '"' && source[pos] != '\n' && source[pos] != '\r') ++pos;
            if (pos < source.size() && source[pos] == '"') { ++pos; kind = Kind::Label; }
        } else if (std::isdigit(c) || (c == '.' && pos < source.size() && std::isdigit((unsigned char)source[pos]))) {
            kind = Kind::Number;
            float unused;
            const auto* begin = source.data()+start;
            auto number = std::from_chars(begin,source.data()+source.size(),unused,std::chars_format::general);
            pos = std::max(pos,(size_t)(number.ptr-source.data()));
        } else {
            const auto two = source.substr(start,2);
            if (two == "==" || two == "!=" || two == "<=" || two == ">=" || two == "&&" || two == "||") ++pos;
        }
        token = {source.substr(start,pos-start),kind,line};
    }

    bool Take(std::string_view text) {
        if (token.text != text) return false;
        Next();
        return true;
    }
    bool Require(std::string_view text, const char* message) {
        if (!Take(text)) { Fail(message); return false; }
        return true;
    }
    size_t Add(Op op, u16 index = 0, float value = 0) {
        if (code->size() >= kInstructions) { Fail("Bloc trop long (256 instructions maximum)."); return 0; }
        code->push_back({op,index,value});
        return code->size()-1;
    }
    void Patch(size_t jump) { if (!failed) (*code)[jump].index = (u16)code->size(); }
    int Find(std::string_view name) const {
        for (size_t i=0; i<std::size(fields); ++i) if (fields[i] == name) return (int)i;
        for (size_t i=0; i<locals.size(); ++i) if (locals[i] == name) return FieldCount+(int)i;
        return -1;
    }
    int FindControl(std::string_view id) const {
        const auto& controls = program->controls;
        const auto found = std::ranges::find(controls,id,&Control::id);
        return found == controls.end() ? -1 : (int)(found - controls.begin());
    }
    bool Number(float& value) {
        if (token.kind != Kind::Number) { Fail("Nombre attendu."); return false; }
        const auto* end = token.text.data()+token.text.size();
        const auto result = std::from_chars(token.text.data(),end,value,std::chars_format::general);
        if (result.ec != std::errc{} || result.ptr != end || !std::isfinite(value)) {
            Fail("Nombre invalide.");
            return false;
        }
        Next();
        return true;
    }
    bool SignedNumber(float& value) {
        const bool negative = Take("-");
        if (!Number(value)) return false;
        if (negative) value = -value;
        return true;
    }
    bool Label(std::string& out) {
        if (token.kind != Kind::Label || token.text.size() <= 2 || token.text.size() > 82) {
            Fail("Libellé attendu entre guillemets (80 octets maximum).");
            return false;
        }
        const auto label = token.text.substr(1,token.text.size()-2);
        if (std::ranges::any_of(label,[](unsigned char c) { return c < 32 || c == 127; }) ||
            std::ranges::all_of(label,[](unsigned char c) { return std::isspace(c) != 0; })) {
            Fail("Libellé invalide.");
            return false;
        }
        out = label;
        Next();
        return true;
    }
    bool ReadSlider(Control& control) {
        const int valueLine = token.line;
        if (!SignedNumber(control.initial) ||
            !Require("[","Plage du curseur attendue entre crochets.") || !SignedNumber(control.min) ||
            !Require(",","Virgule attendue entre les bornes du curseur.") || !SignedNumber(control.max)) return false;
        if (control.min < -kNumberLimit || control.max > kNumberLimit || control.min >= control.max ||
            control.initial < control.min || control.initial > control.max) {
            Fail("Plage ou valeur par défaut du curseur invalide.",valueLine);
            return false;
        }
        return Require("]","Crochet fermant attendu.");
    }
    bool ReadCheckbox(Control& control) {
        if (Take("true")) control.initial = 1;
        else if (!Take("false")) { Fail("Valeur true ou false attendue pour la case à cocher."); return false; }
        return true;
    }
    bool ChoiceValue(float& value) {
        const auto found = std::ranges::find(constants,token.text,&std::pair<std::string_view,float>::first);
        if (found == std::end(constants)) return SignedNumber(value);
        value = found->second;
        Next();
        return true;
    }
    bool ReadChoice(Control& control) {
        const int valueLine = token.line;
        if (!ChoiceValue(control.initial) || !Require("[","Options de la liste attendues entre crochets.")) return false;
        do {
            const int optionLine = token.line;
            ChoiceOption option{};
            if (!ChoiceValue(option.value) || !Label(option.label)) return false;
            if (control.options.size() >= kMaxOptions || std::abs(option.value) > kNumberLimit ||
                std::ranges::find(control.options,option.value,&ChoiceOption::value) != control.options.end()) {
                Fail("Valeur d’option invalide, répétée ou limite de 16 options atteinte.",optionLine);
                return false;
            }
            control.options.push_back(std::move(option));
        } while (Take(","));
        if (std::ranges::find(control.options,control.initial,&ChoiceOption::value) == control.options.end()) {
            Fail("La valeur par défaut de la liste doit figurer parmi ses options.",valueLine);
            return false;
        }
        return Require("]","Crochet fermant attendu.");
    }
    void ReadVisibility(std::vector<Instruction>& condition) {
        if (!Take("when")) return;
        auto* previousCode = code;
        code = &condition;
        readingVisibility = true;
        Expression();
        readingVisibility = false;
        code = previousCode;
    }
    void DeclareControl(ControlKind kind) {
        if (program->controls.size() >= MaxControls) { Fail("Limite de 16 réglages par script."); return; }
        const auto id = token.text;
        const bool reserved = std::ranges::find(stageNames,id) != std::end(stageNames) ||
            std::ranges::any_of(constants,[&](const auto& constant) { return constant.first == id; }) ||
            id == "slider" || id == "let" || id == "if" || id == "else";
        if (token.kind != Kind::Identifier || reserved || Find(id) >= 0 || FindControl(id) >= 0) {
            Fail("Identifiant de réglage invalide ou déjà utilisé.");
            return;
        }
        Control control;
        control.kind = kind;
        control.id = id;
        Next();
        if (!Label(control.label) || !Require("=","Affectation attendue (=).")) return;
        bool valid = false;
        switch (kind) {
            case ControlKind::Slider: valid = ReadSlider(control); break;
            case ControlKind::Checkbox: valid = ReadCheckbox(control); break;
            case ControlKind::Choice: valid = ReadChoice(control); break;
        }
        if (!valid) return;
        ReadVisibility(program->visibility[program->controls.size()]);
        if (!failed && Require(";","Point-virgule attendu.")) program->controls.push_back(std::move(control));
    }
    bool ReadControl() {
        const auto kind = std::ranges::find(controlNames,token.text);
        if (kind == std::end(controlNames)) return false;
        Next();
        DeclareControl((ControlKind)(kind - std::begin(controlNames)));
        return true;
    }
    void DeclareGroup() {
        const auto id = token.text;
        if (token.kind != Kind::Identifier ||
            std::ranges::find(program->groups,id,&ControlGroup::id) != program->groups.end()) {
            Fail("Identifiant de groupe invalide ou déjà utilisé.");
            return;
        }
        const int groupLine = token.line;
        ControlGroup group;
        group.id = id;
        group.firstControl = program->controls.size();
        Next();
        if (!Label(group.label)) return;
        if (Take("collapsed")) group.defaultOpen = false;
        else Take("open");
        std::vector<Instruction> condition;
        ReadVisibility(condition);
        if (failed || !Require("{","Accolade ouvrante attendue.")) return;
        while (!failed && ReadControl()) {}
        if (failed || !Require("}","Réglage ou accolade fermante attendu dans le groupe.")) return;
        group.endControl = program->controls.size();
        if (group.firstControl == group.endControl) {
            Fail("Un groupe doit contenir au moins un réglage.",groupLine);
            return;
        }
        Take(";");
        program->groups.push_back(std::move(group));
        program->groupVisibility.push_back(std::move(condition));
    }
    static const Operator* Binary(std::string_view text) {
        static constexpr Operator operators[] = {
            {"||",1,Op::JumpTrue},{"&&",2,Op::JumpFalse},{"==",3,Op::Equal},{"!=",3,Op::NotEqual},
            {"<",4,Op::Less},{"<=",4,Op::LessEqual},{">",4,Op::Greater},{">=",4,Op::GreaterEqual},
            {"+",5,Op::Add},{"-",5,Op::Subtract},{"*",6,Op::Multiply},{"/",6,Op::Divide},{"%",6,Op::Remainder}
        };
        for (const auto& op : operators) if (op.text == text) return &op;
        return nullptr;
    }
    void Atom() {
        if (Take("-")) { Expression(7); Add(Op::Negative); return; }
        if (Take("!")) { Expression(7); Add(Op::Not); return; }
        if (Take("(")) {
            Expression();
            Require(")","Parenthèse fermante attendue.");
            return;
        }
        if (token.kind == Kind::Number) {
            float value = 0;
            if (Number(value)) Add(Op::Literal,0,value);
            return;
        }
        if (token.kind != Kind::Identifier) { Fail("Expression attendue."); return; }
        const auto name = token.text;
        const int nameLine = token.line;
        Next();
        if (Take("(")) {
            static constexpr Function functions[] = {
                {"abs",1,Op::Abs},{"sin",1,Op::Sin},{"cos",1,Op::Cos},{"sqrt",1,Op::Sqrt},{"floor",1,Op::Floor},
                {"min",2,Op::Min},{"max",2,Op::Max},{"clamp",3,Op::Clamp},{"mix",3,Op::Mix},{"random",0,Op::Random}
            };
            for (const auto& fn : functions) {
                if (fn.name != name) continue;
                if (readingVisibility && fn.op == Op::Random) {
                    Fail("random() est indisponible dans une condition d’affichage.",nameLine);
                    return;
                }
                for (int i=0; i<fn.args && !failed; ++i) {
                    if (i && !Require(",","Virgule attendue entre les arguments.")) return;
                    Expression();
                }
                Require(")","Nombre d’arguments incorrect ou parenthèse manquante.");
                Add(fn.op);
                return;
            }
            Fail("Fonction inconnue.");
            if (failed) error.line = nameLine;
            return;
        }
        const int index = readingVisibility ? -1 : Find(name);
        if (index >= 0) { Add(Op::Load,(u16)index); return; }
        const int control = FindControl(name);
        if (control >= 0) { Add(Op::LoadControl,(u16)control,program->controls[control].initial); return; }
        for (const auto& [key,value] : constants) if (key == name) { Add(Op::Literal,0,value); return; }
        Fail(readingVisibility ? "La condition d’affichage doit lire un réglage déclaré plus haut ou une constante." :
             "Variable inconnue.",nameLine);
    }
    void Expression(int minPrecedence = 1) {
        if (++depth > 32) Fail("Expression trop imbriquée.");
        if (!failed) Atom();
        while (!failed) {
            const auto* op = Binary(token.text);
            if (!op || op->precedence < minPrecedence) break;
            Next();
            if (op->op == Op::JumpFalse || op->op == Op::JumpTrue) {
                const auto branch = Add(op->op);
                Expression(op->precedence+1);
                Add(Op::Truth);
                const auto done = Add(Op::Jump);
                Patch(branch);
                Add(Op::Literal,0,op->op == Op::JumpTrue ? 1.0f : 0.0f);
                Patch(done);
            } else {
                Expression(op->precedence+1);
                Add(op->op);
            }
        }
        --depth;
    }
    void Block(bool top = false) {
        if (!Require("{","Accolade ouvrante attendue.")) return;
        if (++depth > 32) Fail("Blocs trop imbriqués.");
        while (!failed && token.kind != Kind::End && token.text != "}") {
            if (Take("if")) {
                Require("(","Parenthèse attendue après if.");
                Expression();
                Require(")","Parenthèse fermante attendue.");
                const auto branch = Add(Op::JumpFalse);
                Block();
                if (Take("else")) {
                    const auto end = Add(Op::Jump);
                    Patch(branch);
                    Block();
                    Patch(end);
                } else Patch(branch);
                continue;
            }
            const bool declare = Take("let");
            if (declare && !top) { Fail("Déclarez les variables locales en dehors des blocs if."); break; }
            if (token.kind != Kind::Identifier) { Fail("Affectation ou if attendu."); break; }
            const auto name = token.text;
            int index = Find(name);
            if (declare) {
                if (index >= 0 || FindControl(name) >= 0 || locals.size() >= kLocals) {
                    Fail("Variable déjà déclarée ou limite de 16 variables locales."); break;
                }
            } else if (index < 0 || (index < FieldCount && !Writable(stage,(Field)index))) {
                Fail("Variable inconnue ou en lecture seule dans ce bloc.");
                break;
            }
            Next();
            Require("=","Affectation attendue (=).");
            Expression();
            Require(";","Point-virgule attendu.");
            if (declare && !failed) {
                index = FieldCount + (int)locals.size();
                locals.push_back(name);
            }
            if (!failed) Add(Op::Store,(u16)index);
        }
        --depth;
        Require("}","Accolade fermante attendue.");
    }

public:
    Compiler(std::string_view text, Diagnostic& diagnostic) : source(text), error(diagnostic) {}
    std::shared_ptr<const Program> Run() {
        error = {};
        if (source.size() > kSourceBytes) { Fail("La source dépasse 16 Kio."); return {}; }
        auto result = std::make_shared<Program>();
        program = result.get();
        std::array<bool,(size_t)Stage::Count> seen{};
        Next();
        while (!failed) {
            if (Take("group")) DeclareGroup();
            else if (!ReadControl()) break;
        }
        while (!failed && token.kind != Kind::End) {
            const auto found = std::find(std::begin(stageNames),std::end(stageNames),token.text);
            const size_t index = found - std::begin(stageNames);
            if (found == std::end(stageNames) || seen[index]) { Fail("Bloc emit, update ou impact attendu, une fois chacun."); break; }
            seen[index] = true;
            stage = (Stage)index;
            code = &program->stages[index];
            locals.clear();
            Next();
            Block(true);
        }
        if (!std::any_of(seen.begin(),seen.end(),[](bool value) { return value; })) Fail("Ajoutez un bloc emit, update ou impact.");
        return failed ? nullptr : result;
    }
};

std::shared_ptr<const Program> Program::Compile(std::string_view source, Diagnostic& error) {
    return Compiler(source,error).Run();
}

bool Program::Has(Stage stage) const { return !stages[(size_t)stage].empty(); }

void Program::Run(Stage stage, State& state, u32& randomState, std::span<const float> values) const {
    Execute(stages[(size_t)stage],state,randomState,values);
}

bool Program::IsControlVisible(size_t index, std::span<const float> values) const {
    return Visible(visibility[index],values);
}

bool Program::IsGroupVisible(size_t index, std::span<const float> values) const {
    return Visible(groupVisibility[index],values);
}

bool Program::Visible(std::span<const Instruction> condition, std::span<const float> values) {
    if (condition.empty()) return true;
    State state{};
    u32 randomState = 0;
    return Execute(condition,state,randomState,values) != 0;
}

float Program::Execute(std::span<const Instruction> code, State& state, u32& randomState, std::span<const float> values) {
    std::array<float,kInstructions> stack;
    std::array<float,kLocals> local{};
    size_t n = 0;
    const auto take = [&] { return stack[--n]; };
    const auto push = [&](float value) {
        stack[n++] = std::isfinite(value) ? std::clamp(value,-kNumberLimit,kNumberLimit) : 0.0f;
    };
    for (size_t i=0; i<code.size();) {
        const auto op = code[i++];
        switch (op.op) {
            case Op::Literal: push(op.value); break;
            case Op::Load: push(op.index < FieldCount ? state[op.index] : local[op.index-FieldCount]); break;
            case Op::LoadControl: push(op.index < values.size() ? values[op.index] : op.value); break;
            case Op::Store: (op.index < FieldCount ? state[op.index] : local[op.index-FieldCount]) = take(); break;
            case Op::Negative: stack[n-1] = -stack[n-1]; break;
            case Op::Not: stack[n-1] = stack[n-1] == 0; break;
            case Op::Truth: stack[n-1] = stack[n-1] != 0; break;
            case Op::Jump: i = op.index; break;
            case Op::JumpFalse: if (take() == 0) i = op.index; break;
            case Op::JumpTrue: if (take() != 0) i = op.index; break;
            case Op::Abs: stack[n-1] = std::abs(stack[n-1]); break;
            case Op::Sin: stack[n-1] = std::sin(stack[n-1]); break;
            case Op::Cos: stack[n-1] = std::cos(stack[n-1]); break;
            case Op::Sqrt: stack[n-1] = std::sqrt(std::max(0.0f,stack[n-1])); break;
            case Op::Floor: stack[n-1] = std::floor(stack[n-1]); break;
            case Op::Random:
                randomState = randomState * 1664525u + 1013904223u;
                push((randomState >> 8) * (1.0f / 16777216.0f));
                break;
            case Op::Clamp: {
                const float hi = take(), lo = take(), value = take();
                push(std::clamp(value,std::min(lo,hi),std::max(lo,hi)));
                break;
            }
            case Op::Mix: {
                const float t = take(), b = take(), a = take();
                push(a+(b-a)*t);
                break;
            }
            default: {
                const float b = take(), a = take();
                switch (op.op) {
                    case Op::Add: push(a+b); break;
                    case Op::Subtract: push(a-b); break;
                    case Op::Multiply: push(a*b); break;
                    case Op::Divide: push(b != 0 ? a/b : 0); break;
                    case Op::Remainder: push(b != 0 ? std::fmod(a,b) : 0); break;
                    case Op::Less: push(a<b); break;
                    case Op::LessEqual: push(a<=b); break;
                    case Op::Greater: push(a>b); break;
                    case Op::GreaterEqual: push(a>=b); break;
                    case Op::Equal: push(a==b); break;
                    case Op::NotEqual: push(a!=b); break;
                    case Op::Min: push(std::min(a,b)); break;
                    case Op::Max: push(std::max(a,b)); break;
                    default: break;
                }
            }
        }
    }
    return n ? stack[n-1] : 0.f;
}

std::string_view ExampleProgram() {
    return R"(group impact "Impact" open {
    slider impact_radius_scale "Rayon d’impact" = 2.1 [0, 4];
    slider impact_force "Force d’impact" = 1 [0, 2];
    checkbox paint_impacts "Déposer aux impacts" = true;
    choice impact_mode "Réaction à l’impact" = SLIDE [
        SLIDE "Glisser", BOUNCE "Rebondir", STOP "S’arrêter"
    ];
}

group bounce "Rebond" collapsed when impact_mode == BOUNCE {
    slider bounce_strength "Force du rebond" = 0.35 [0, 1];
}

emit {
    // Different droplet sizes and lifetimes, using the stroke seed.
    let scale = mix(0.55, 1.3, random());
    radius = radius * scale;
    life = life * mix(0.8, 1.2, random());
}

update {
    // A crosswind that changes with each droplet's age.
    ax = sin(age * 9) * 0.95;
    alpha = clamp(0.8 - age * 0.24, 0.12, 0.8);
}

impact {
    impact_radius = radius * impact_radius_scale;
    strength = clamp((0.25 + speed * 0.2) * impact_force, 0, 1);
    if (!paint_impacts) { strength = 0; }
    response = impact_mode;
    if (impact_mode == BOUNCE) { restitution = bounce_strength; }
}
)";
}

} // namespace sx::particles
