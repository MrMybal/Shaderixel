#pragma once

#include "core/Common.h"

#include <array>
#include <memory>
#include <span>
#include <string_view>

namespace sx::particles {

enum class Stage { Emit, Update, Impact, Count };
enum Field {
    X, Y, Z, VX, VY, VZ, NX, NY, NZ, Radius, Life, Alpha, Age, Speed, Time, Step, OnSurface, Alive,
    AX, AY, AZ, Gravity, ImpactRadius, Strength, Response, Restitution, FieldCount
};
enum ImpactResponse { Native, Slide, Stop, Bounce };
using State = std::array<float, FieldCount>;
constexpr size_t MaxControls = 16;
using ControlValues = std::array<float, MaxControls>;

enum class ControlKind { Slider, Checkbox, Choice };

struct ChoiceOption {
    float value;
    std::string label;
};

struct Control {
    ControlKind kind = ControlKind::Slider;
    std::string id, label;
    float initial = 0, min = 0, max = 0;
    std::vector<ChoiceOption> options;

    float Restore(double value) const;
};

struct ControlGroup {
    std::string id, label;
    size_t firstControl = 0, endControl = 0;
    bool defaultOpen = true;
};

struct Diagnostic {
    int line = 0;
    std::string message;
};

class Program {
public:
    static std::shared_ptr<const Program> Compile(std::string_view source, Diagnostic& error);
    bool Has(Stage stage) const;
    const std::vector<Control>& Controls() const { return controls; }
    const std::vector<ControlGroup>& Groups() const { return groups; }
    bool IsControlVisible(size_t index, std::span<const float> values) const;
    bool IsGroupVisible(size_t index, std::span<const float> values) const;
    void Run(Stage stage, State& state, u32& randomState, std::span<const float> values = {}) const;

private:
    friend class Compiler;
    enum class Op : u8 {
        Literal, Load, LoadControl, Store, Negative, Not, Truth,
        Add, Subtract, Multiply, Divide, Remainder, Less, LessEqual, Greater, GreaterEqual, Equal, NotEqual,
        Abs, Sin, Cos, Sqrt, Floor, Min, Max, Clamp, Mix, Random,
        Jump, JumpFalse, JumpTrue
    };
    struct Instruction { Op op; u16 index = 0; float value = 0; };
    static float Execute(std::span<const Instruction> code, State& state, u32& randomState, std::span<const float> values);
    static bool Visible(std::span<const Instruction> condition, std::span<const float> values);
    std::array<std::vector<Instruction>,(size_t)Stage::Count> stages;
    std::array<std::vector<Instruction>,MaxControls> visibility;
    std::vector<Control> controls;
    std::vector<ControlGroup> groups;
    std::vector<std::vector<Instruction>> groupVisibility;
};

// The same editable source ships in the example preset and in the editor's help.
std::string_view ExampleProgram();

} // namespace sx::particles
