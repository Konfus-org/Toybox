open a command line to this folder and run::
dxc.exe -T vs_6_0 -E main -spirv -fspv-target-env="vulkan1.1" -fvk-use-dx-layout -Fo "vertex.spv" "vertex.hlsl"
dxc.exe -T ps_6_0 -E main -spirv -fspv-target-env="vulkan1.1" -fvk-use-dx-layout -Fo "fragment.spv" "fragment.hlsl"
