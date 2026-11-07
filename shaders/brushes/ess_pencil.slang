//@name Crayon
//@category Essentiels
//@spacing 0.06
//@param Grain float 0.55 0 1 "Grain du papier"
//@param GrainScale float 1.0 0.3 4 "Taille du grain"

// Mine de crayon : pointe nette dont le grain du papier (fixe sur la feuille) laisse des blancs, plus marqués à
// faible pression.
float Paper(float2 px)
{
    float2 p = px / GrainScale;
    float a = Hash1(dot(floor(p), float2(0.1031, 0.3071)));
    float c = Hash1(dot(floor(p * 0.5), float2(0.2131, 0.1171)));
    return a * 0.6 + c * 0.4;
}

float4 Tip(Brush b)
{
    float r = length(b.local);
    float edge = 1.0 - smoothstep(0.8, 1.0, r);
    float tooth = saturate((Paper(b.px) - Grain * (1.2 - b.pressure)) * 3.0 + 0.5);
    return float4(b.color, edge * tooth);
}
