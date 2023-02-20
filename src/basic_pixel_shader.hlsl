
struct vs_out {
    float4 position_clip : SV_POSITION;  // required output of VS
    float3 color : COL;
};
float4 main(vs_out input) : SV_TARGET {
    return float4(input.color, 1.0f);
}