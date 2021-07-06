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

    const string REPROJECTION_FRAGMENT_SHADER = R"glsl(
        uniform samplerExternalOES tex0, tex1;
        uniform magnitude{
            highp float magnitude
        }
        in vec2 uv;
        out vec4 color;
        void main() {
            uv += texture(tex1, uv).rg * -1. * magnitude.magnitude;
            color = texture(tex0, uv);
        }
    )glsl";
//uv -= texture(tex1, uv).rg;
}


Reprojection::Reprojection(Texture *mInputSurface)
    : mInputSurface(mInputSurface) {
}

void Reprojection::Initialize(ReprojectionData reprojectionData) {
    auto reprojectionCommonShaderStr = REPROJECTION_COMMON_SHADER_FORMAT;

    mTargetTexture.reset(
            new Texture(false, reprojectionData.eyeWidth * 2, reprojectionData.eyeHeight, GL_R8));
    mTargetState = make_unique<RenderState>(mTargetTexture.get());

    mRefTexture.reset(
            new Texture(false, reprojectionData.eyeWidth * 2, reprojectionData.eyeHeight, GL_R8));
    mRefState = make_unique<RenderState>(mRefTexture.get());

    auto RGBtoLuminanceShaderStr = reprojectionCommonShaderStr + RGB_TO_LUMINANCE_FRAGMENT_SHADER;
    mRGBtoLuminancePipeline = unique_ptr<RenderPipeline>(
            new RenderPipeline({mInputSurface}, QUAD_2D_VERTEX_SHADER,
                               RGBtoLuminanceShaderStr));

    auto CopyShaderStr = reprojectionCommonShaderStr + COPY_FRAGMENT_SHADER;
    mCopyPipeline = unique_ptr<RenderPipeline>(
            new RenderPipeline({mTargetTexture.get()}, QUAD_2D_VERTEX_SHADER,
                               CopyShaderStr));

    mMotionVector.reset(
            new Texture(false, reprojectionData.eyeWidth * 2 / MOTION_ESTIMATION_SEARCH_BLOCK_X_QCOM, reprojectionData.eyeHeight / MOTION_ESTIMATION_SEARCH_BLOCK_Y_QCOM, GL_RGBA16F));

    mReprojectedTexture.reset(
            new Texture(false, reprojectionData.eyeWidth * 2, reprojectionData.eyeHeight, GL_RGB8));
    mReprojectedTextureState = make_unique<RenderState>(mReprojectedTexture.get());

    auto ReprojectionShaderStr = reprojectionCommonShaderStr + REPROJECTION_FRAGMENT_SHADER;
    mReprojectionPipeline = unique_ptr<RenderPipeline>(
            new RenderPipeline({mInputSurface, mMotionVector.get()}, QUAD_2D_VERTEX_SHADER,
                               ReprojectionShaderStr, 4));
}

void Reprojection::AddFrame(ovrTracking2 *tracking, uint64_t renderTime) {
    mCopyPipeline->Render(*mRefState);
    mRefTracking =  mTargetTracking;
    mRefTime = mTargetTime;

    mRGBtoLuminancePipeline->Render(*mTargetState);
    mTargetTracking = tracking;
    mTargetTime = renderTime;
}

void Reprojection::EstimateMotion() {
    TexEstimateMotionQCOM(mTargetTexture->GetGLTexture(), mRefTexture->GetGLTexture(), mMotionVector->GetGLTexture());
// reversed inputs to TexEstimateMotionQCOM so the starting position doesnt need to be corrected
}

void Reprojection::Reproject(uint64_t displayTime) {
	float magnitude = (double)(displayTime - mTargetTime) / (double)(mTargetTime - mRefTime);
    mReprojectionPipeline->Render(*mReprojectedTextureState, &magnitude);
// need to estimate tracking
}

void Reprojection::Render() {
}
