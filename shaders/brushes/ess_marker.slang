//@name Marqueur
//@category Essentiels
//@spacing 0.05
//@param Chisel float 0.35 0.1 1 "Épaisseur du biseau"
//@param Angle float 35 -90 90 "Angle du biseau (°)"
//@param Wetness float 0.55 0.1 1 "Encre par touche"

// Feutre à pointe biseautée : une touche rectangulaire inclinée, peu couvrante, qui fonce là où le trait repasse.
float4 Tip(Brush b)
{
    float2 q = Rotate(b.local, Angle * 0.0174533);
    float w = 1.0 - smoothstep(0.85, 1.0, abs(q.x));
    float h = 1.0 - smoothstep(Chisel * 0.85, Chisel, abs(q.y));
    return float4(b.color, w * h * Wetness);
}
