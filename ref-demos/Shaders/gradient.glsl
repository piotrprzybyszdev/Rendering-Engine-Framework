vec4 gradient(vec2 imageCoords, vec2 imageSize, float time)
{
    return vec4(vec2(imageCoords) / imageSize, (1.0f + sin(time)) / 2.0f, 1.0f);
}
