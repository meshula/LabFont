
src=$(pwd)
pushd /var/tmp
#sokol-shdc -i $src/imm.glsl -o labimm-shd.h -l glsl330:glsl100:hlsl4:metal_macos:metal_ios:metal_sim:wgpu -b
sokol-shdc -i $src/imm.glsl -o labimm-shd.h -l metal_macos -b
cp labimm-shd.h $src/labimm-shd.h
popd

