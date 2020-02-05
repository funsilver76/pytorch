#include "shader.h"
const char* glsl_binary_add_glsl = 
"layout(FORMAT, binding=0) writeonly uniform PRECISION image3D uOutput;\n"
"layout(location=1) uniform mediump sampler3D uInput0;\n"
"layout(location=2) uniform mediump sampler3D uInput1;\n"
"layout(location=3) uniform ivec4 imgSize;\n"
"layout (local_size_x = XLOCAL, local_size_y = YLOCAL, local_size_z = ZLOCAL) in;\n"
"void main()\n"
"{\n"
"    ivec3 pos = ivec3(gl_GlobalInvocationID);\n"
"    ivec3 inSize = imgSize.xyz;\n"
"    if(all(lessThan(pos, inSize)))\n"
"    {\n"
"        vec4 sum = texelFetch(uInput0, pos, 0) + texelFetch(uInput1, pos, 0);\n"
"        imageStore(uOutput, pos, sum);\n"
"    }\n"
"}\n"
;
const char* glsl_image_to_nchw_buffer_glsl = 
"layout(FORMAT, binding=0) readonly uniform PRECISION image3D uImage;\n"
"layout(binding=1) writeonly buffer destBuffer{\n"
"    float data[];\n"
"} uOutBuffer;\n"
"layout(location = 2) uniform int uWidth;\n"
"layout(location = 3) uniform int uHeight;\n"
"layout (local_size_x = 8, local_size_y = 8, local_size_z = 1) in;\n"
"void main()\n"
"{\n"
"    ivec3 pos = ivec3(gl_GlobalInvocationID);\n"
"    if (pos.x < uWidth && pos.y < uHeight)\n"
"    {\n"
"        vec4 color = imageLoad(uImage, pos);\n"
"        int z = pos.z*4;\n"
"        uOutBuffer.data[uWidth*pos.y+pos.x+(z+0)*uWidth*uHeight] = color.r;\n"
"        uOutBuffer.data[uWidth*pos.y+pos.x+(z+1)*uWidth*uHeight] = color.g;\n"
"        uOutBuffer.data[uWidth*pos.y+pos.x+(z+2)*uWidth*uHeight] = color.b;\n"
"        uOutBuffer.data[uWidth*pos.y+pos.x+(z+3)*uWidth*uHeight] = color.a;\n"
"    }\n"
"}\n"
;
const char* glsl_convolution_glsl = 
"layout(std430) buffer;\n"
"layout(FORMAT, binding=0) writeonly uniform PRECISION image3D uOutput;\n"
"layout(location=1) uniform mediump sampler3D uInput;\n"
"layout(location=2) uniform mediump sampler3D uKernel;\n"
"layout(binding=3) readonly buffer bias{\n"
"    vec4 data[];\n"
"} uBias;\n"
"layout(location=4) uniform ivec2 uPad;\n"
"layout(location=5) uniform ivec2 uKernelSize;\n"
"layout(location=6) uniform ivec2 uStride;\n"
"layout(location=7) uniform ivec2 uDilate;\n"
"layout(location=8) uniform int uUnroll;\n"
"layout(location=10) uniform ivec3 uOutputSize;\n"
"layout(location=11) uniform ivec3 uInputSize;\n"
"#define UP_DIV(x, y) (((x)+(y)-1)/(y))\n"
"//weight : oc ic h w -> oc/4, ic/4, ky kx ic4 oc4\n"
"layout (local_size_x = XLOCAL, local_size_y = YLOCAL, local_size_z = ZLOCAL) in;\n"
"void main()\n"
"{\n"
"    if (all(lessThan(ivec3(gl_GlobalInvocationID), uOutputSize)))\n"
"    {\n"
"        ivec3 pos = ivec3(gl_GlobalInvocationID)*ivec3(uUnroll, 1, 1);\n"
"        int kernelX = uKernelSize.x;\n"
"        int kernelY = uKernelSize.y;\n"
"        ivec3 inputSize = uInputSize;\n"
"        ivec2 s0 = pos.xy*uStride-uPad;\n"
"        int fx, fy, fz;\n"
"        ivec2 sfxy = max(ivec2(0), (UP_DIV(-s0, uDilate)));\n"
"        ivec2 efxy = min(uKernelSize, UP_DIV(inputSize.xy-s0, uDilate));\n"
"        vec4 color = uBias.data[pos.z];\n"
"        vec4 color2 = color;\n"
"        vec4 color3 = color;\n"
"        vec4 color4 = color;\n"
"        int kY = pos.z;\n"
"        for (fy=sfxy.y; fy<efxy.y; ++fy)\n"
"        {\n"
"            int sy = fy*uDilate.y + s0.y;\n"
"            for (fx=0; fx<kernelX; ++fx)\n"
"            {\n"
"                int kZ = fx + fy*kernelX;\n"
"                int sx1 = fx*uDilate.x + s0.x;\n"
"                int sx2 = sx1 + uStride.x;\n"
"                int sx3 = sx1 + uStride.x * 2;\n"
"                int sx4 = sx1 + uStride.x * 3;\n"
"                float m1 = sx1 >= 0 && sx1 < inputSize.x ? 1.0 : 0.0;\n"
"                float m2 = sx2 >= 0 && sx2 < inputSize.x ? 1.0 : 0.0;\n"
"                float m3 = sx3 >= 0 && sx3 < inputSize.x ? 1.0 : 0.0;\n"
"                float m4 = sx4 >= 0 && sx4 < inputSize.x ? 1.0 : 0.0;\n"
"                fz = 0;\n"
"                for (; fz<inputSize.z; ++fz)\n"
"                {\n"
"                    int kX = 4*fz;\n"
"                    vec4 k0 = texelFetch(uKernel, ivec3(kX+0, kY, kZ), 0);\n"
"                    vec4 k1 = texelFetch(uKernel, ivec3(kX+1, kY, kZ), 0);\n"
"                    vec4 k2 = texelFetch(uKernel, ivec3(kX+2, kY, kZ), 0);\n"
"                    vec4 k3 = texelFetch(uKernel, ivec3(kX+3, kY, kZ), 0);\n"
"                    mat4 k = mat4(k0, k1, k2, k3);\n"
"                    color  += k*texelFetch(uInput, ivec3(sx1, sy, fz), 0) * m1;\n"
"                    color2 += k*texelFetch(uInput, ivec3(sx2, sy, fz), 0) * m2;\n"
"                    color3 += k*texelFetch(uInput, ivec3(sx3, sy, fz), 0) * m3;\n"
"                    color4 += k*texelFetch(uInput, ivec3(sx4, sy, fz), 0) * m4;\n"
"                }\n"
"            }\n"
"        }\n"
"        #ifdef RELU\n"
"        color = max(color, vec4(0));\n"
"        color2 = max(color2, vec4(0));\n"
"        color3 = max(color3, vec4(0));\n"
"        color4 = max(color4, vec4(0));\n"
"        #endif\n"
"        #ifdef RELU6\n"
"        color = clamp(color, vec4(0), vec4(6));\n"
"        color2 = clamp(color2, vec4(0), vec4(6));\n"
"        color3 = clamp(color3, vec4(0), vec4(6));\n"
"        color4 = clamp(color4, vec4(0), vec4(6));\n"
"        #endif\n"
"        imageStore(uOutput, ivec3(pos.x+0, pos.y, pos.z), color);\n"
"        imageStore(uOutput, ivec3(pos.x+1, pos.y, pos.z), color2);\n"
"        imageStore(uOutput, ivec3(pos.x+2, pos.y, pos.z), color3);\n"
"        imageStore(uOutput, ivec3(pos.x+3, pos.y, pos.z), color4);\n"
"    }\n"
"}\n"
;
const char* glsl_kernel2image_adreno_glsl = 
"layout(std430) buffer;\n"
"layout(FORMAT, binding=0) writeonly uniform PRECISION image3D uOutput;\n"
"layout(binding=2) readonly buffer kernel{\n"
"    vec4 data[];\n"
"} uKernel;\n"
"layout(location = 3) uniform int uFxFy;\n"
"layout(location = 4) uniform int uIc_4;\n"
"layout (local_size_x = 1, local_size_y = 1, local_size_z = 1) in;\n"
"//weight buffer : oc ic h w -> oc/4, ic/4, ky kx ic4 oc4\n"
"//index : ky kx, oc/4, ic/4\n"
"//weight image : ky kx, oc/4, ic/4*ic4 oc4\n"
"void main()\n"
"{\n"
"    ivec3 pos = ivec3(gl_GlobalInvocationID) * ivec3(4, 1, 1);\n"
"    int kernelPos = 0\n"
"    + pos.x * uFxFy\n"
"    + 4*pos.y * uIc_4 * uFxFy\n"
"    + 4*pos.z\n"
"    ;\n"
"    vec4 color0 = uKernel.data[kernelPos+0];\n"
"    vec4 color1 = uKernel.data[kernelPos+1];\n"
"    vec4 color2 = uKernel.data[kernelPos+2];\n"
"    vec4 color3 = uKernel.data[kernelPos+3];\n"
"    \n"
"    imageStore(uOutput, ivec3(pos.x+0, pos.y, pos.z), color0);\n"
"    imageStore(uOutput, ivec3(pos.x+1, pos.y, pos.z), color1);\n"
"    imageStore(uOutput, ivec3(pos.x+2, pos.y, pos.z), color2);\n"
"    imageStore(uOutput, ivec3(pos.x+3, pos.y, pos.z), color3);\n"
"}\n"
;
const char* glsl_nc4hw4_buffer_to_image_glsl = 
"layout(FORMAT, binding=0) writeonly uniform PRECISION image3D uImage;\n"
"layout(binding=1) readonly buffer destBuffer{\n"
"    vec4 data[];\n"
"} uInBuffer;\n"
"layout(location = 2) uniform int uWidth;\n"
"layout(location = 3) uniform int uHeight;\n"
"layout (local_size_x = 8, local_size_y = 8, local_size_z = 1) in;\n"
"void main()\n"
"{\n"
"    ivec3 pos = ivec3(gl_GlobalInvocationID);\n"
"    if (pos.x < uWidth && pos.y < uHeight)\n"
"    {\n"
"        vec4 color = uInBuffer.data[uWidth*pos.y+pos.x+pos.z*uWidth*uHeight];\n"
"        imageStore(uImage, pos, color);\n"
"    }\n"
"}\n"
;
const char* glsl_nchw_buffer_to_image_glsl = 
"layout(FORMAT, binding=0) writeonly uniform PRECISION image3D uImage;\n"
"layout(binding=1) readonly buffer destBuffer{\n"
"    float data[];\n"
"} uInBuffer;\n"
"layout(location = 2) uniform int uWidth;\n"
"layout(location = 3) uniform int uHeight;\n"
"layout (local_size_x = 8, local_size_y = 8, local_size_z = 1) in;\n"
"void main()\n"
"{\n"
"    ivec3 pos = ivec3(gl_GlobalInvocationID);\n"
"    if (pos.x < uWidth && pos.y < uHeight)\n"
"    {\n"
"        vec4 color;\n"
"        int z = pos.z*4;\n"
"        color.r = uInBuffer.data[uWidth*pos.y+pos.x+(z+0)*uWidth*uHeight];\n"
"        color.g = uInBuffer.data[uWidth*pos.y+pos.x+(z+1)*uWidth*uHeight];\n"
"        color.b = uInBuffer.data[uWidth*pos.y+pos.x+(z+2)*uWidth*uHeight];\n"
"        color.a = uInBuffer.data[uWidth*pos.y+pos.x+(z+3)*uWidth*uHeight];\n"
"        imageStore(uImage, pos, color);\n"
"    }\n"
"}\n"
;
const char* glsl_image_to_nc4hw4_buffer_glsl = 
"layout(FORMAT, binding=0) readonly uniform PRECISION image3D uImage;\n"
"layout(std430, binding=1) writeonly buffer destBuffer{\n"
"    vec4 data[];\n"
"} uOutBuffer;\n"
"layout(location = 2) uniform int uWidth;\n"
"layout(location = 3) uniform int uHeight;\n"
"layout (local_size_x = 8, local_size_y = 8, local_size_z = 1) in;\n"
"void main()\n"
"{\n"
"    ivec3 pos = ivec3(gl_GlobalInvocationID);\n"
"    if (pos.x < uWidth && pos.y < uHeight)\n"
"    {\n"
"        vec4 color = imageLoad(uImage, pos);\n"
"        uOutBuffer.data[uWidth*pos.y+pos.x+pos.z*uWidth*uHeight] = color;\n"
"    }\n"
"}\n"
;
