#include "space_reprojection.h"

#include <cmath>
#include <memory>

#include "utils.h"

using namespace std;
using namespace gl_render_utils;

namespace {
    const string REPROJECTION_COMMON_SHADER_FORMAT = R"glsl(
        #version 300 es
        #extension GL_OES_EGL_image_external_essl3 : enable
        #extension GL_EXT_YUV_target : enable
        #extension GL_QCOM_motion_estimation : enable
        precision highp float;

    )glsl";

    const string COPY_FRAGMENT_SHADER = R"glsl(
        uniform samplerExternalOES tex0;
        in vec2 uv;
        out vec1 color;
        void main() {
            color = texture(tex0, uv);
        }
    )glsl";

    const string RGB_TO_LUMINANCE_FRAGMENT_SHADER = R"glsl(
        uniform samplerExternalOES tex0;
        in vec2 uv;
        out vec1 color;
        void main() {
            color = rgb_2_yuv(texture(tex0, uv).rgb, itu_601_full_range).r;
        }
    )glsl";

    const string MOTION_ESTIMATION_FRAGMENT_SHADER = R"glsl(
        uniform samplerExternalOES tex0, tex1;
        in vec2 uv;
        out vec4 color;
        void main() {
            color.width = tex0.width / MOTION_ESTIMATION_SEARCH_BLOCK_X_QCOM;
            color.height = tex0.height / MOTION_ESTIMATION_SEARCH_BLOCK_Y_QCOM;
            TexEstimateMotionQCOM(tex1, tex0, color);
        }
    )glsl";
// reversed inputs to TexEstimateMotionQCOM so the starting position doesnt need to be corrected

    const string REPROJECTION_FRAGMENT_SHADER = R"glsl(
        uniform samplerExternalOES tex0, tex1;
        in vec2 uv;
        out vec4 color;
        void main() {
            uv += texture(tex1, uv).rg * -1. * .5;
            color = texture(tex0, uv);
        }
    )glsl";
// how to resize the motion vectors?
//uv -= texture(tex1, uv).rg;
}


Reprojection::Reprojection(Texture *mInputSurface)
    : mInputSurface(mInputSurface) {
}

void Reprojection::Initialize(ReprojectionData reprojectionData) {
    auto reprojectionCommonShaderStr = REPROJECTION_COMMON_SHADER_FORMAT;

    mFrameTarget.texture.reset(
            new Texture(false, reprojectionData.eyeWidth * 2, reprojectionData.eyeHeight, GL_R8));
    mFrameTarget.state = make_unique<RenderState>(mFrameTarget.texture.get());

    mFrameRef.texture.reset(
            new Texture(false, reprojectionData.eyeWidth * 2, reprojectionData.eyeHeight, GL_R8));
    mFrameRef.state = make_unique<RenderState>(mFrameRef.texture.get());

    auto RGBtoLuminanceShaderStr = reprojectionCommonShaderStr + RGB_TO_LUMINANCE_FRAGMENT_SHADER;
    mRGBtoLuminancePipeline = unique_ptr<RenderPipeline>(
            new RenderPipeline({mInputSurface}, QUAD_2D_VERTEX_SHADER,
                               RGBtoLuminanceShaderStr));

    auto CopyShaderStr = reprojectionCommonShaderStr + COPY_FRAGMENT_SHADER;
    mCopyPipeline = unique_ptr<RenderPipeline>(
            new RenderPipeline({mFrameTarget}, QUAD_2D_VERTEX_SHADER,
                               CopyShaderStr));

    mMotionVector.reset(
            new Texture(false, reprojectionData.eyeWidth * 2, reprojectionData.eyeHeight, GL_RGBA16F));
    mMotionVectorState = make_unique<RenderState>(mMotionVector.get());

    auto MotionEstimationShaderStr = reprojectionCommonShaderStr + MOTION_ESTIMATION_FRAGMENT_SHADER;
    mMotionEstimationPipeline = unique_ptr<RenderPipeline>(
            new RenderPipeline({mFrameRef.texture, mFrameTarget.texture}, QUAD_2D_VERTEX_SHADER,
                               MotionEstimationShaderStr));

    mReprojectedTexture.reset(
            new Texture(false, reprojectionData.eyeWidth * 2, reprojectionData.eyeHeight, GL_RGB8));
    mReprojectedTextureState = make_unique<RenderState>(mReprojectedTexture.get());

    auto ReprojectionShaderStr = reprojectionCommonShaderStr + REPROJECTION_FRAGMENT_SHADER;
    mReprojectionPipeline = unique_ptr<RenderPipeline>(
            new RenderPipeline({mInputSurface, mMotionVector}, QUAD_2D_VERTEX_SHADER,
                               ReprojectionShaderStr));
}

void Reprojection::AddFrame(ovrTracking2 *tracking, uint64_t renderTime) {
    mCopyPipeline->Render(mFrameRef.state);
    mFrameRef.tracking =  mFrameTarget.tracking;
    mFrameRef.time = mFrameTarget.time;

    mRGBtoLuminancePipeline->Render(mFrameTarget.state);
    mFrameTarget.tracking = tracking;
    mFrameTarget.time = renderTime;
}

void Reprojection::EstimateMotion() {
    mMotionEstimationPipeline->Render(mMotionVectorState);
}

void Reprojection::Reproject(uint64_t displayTime) {
    uint64_t frameDelta = mFrameTarget.time - mFrameRef.time;
// Estimate tracking?
    mReprojectionPipeline->Render(mReprojectedTextureState);
}

void Reprojection::Render() {
}
