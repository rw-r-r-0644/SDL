#!/bin/bash
cd "${0%/*}"
echo "Regenerating wiiu_shaders.c ..."
latte-assembler compile colorShader.gsh --vsh colorShader.vsh --psh colorShader.psh
latte-assembler compile textureShader.gsh --vsh textureShader.vsh --psh textureShader.psh
xxd -i colorShader.gsh > colorShader.gsh.h
xxd -i textureShader.gsh > textureShader.gsh.h
sed -i '/_gsh_len/d' colorShader.gsh.h textureShader.gsh.h
sed -i 's/colorShader_gsh/wiiuColorShaderData/g' colorShader.gsh.h
sed -i 's/textureShader_gsh/wiiuTextureShaderData/g' textureShader.gsh.h
cp wiiu_shaders.c.in ../wiiu_shaders.c
cat colorShader.gsh.h textureShader.gsh.h >> ../wiiu_shaders.c
rm -rf colorShader.gsh textureShader.gsh colorShader.gsh.h textureShader.gsh.h
echo "Done!"
