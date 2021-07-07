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
        out float color;
        void main() {
            color = texture(tex0, uv).r;
        }
    )glsl";

    const string RGB_TO_LUMINANCE_FRAGMENT_SHADER = R"glsl(
        uniform samplerExternalOES tex0;
        in vec2 uv;
        out float color;
        void main() {
            color = rgb_2_yuv(texture(tex0, uv).rgb, itu_601_full_range).r;
        }
    )glsl";

    const string REPROJECTION_FRAGMENT_SHADER = R"glsl(
        uniform samplerExternalOES tex0, tex1;
        uniform blockData {
            highp float magnitude;
        };
        in vec2 uv;
        out vec3 color;
        void main() {
            vec2 warp = uv + texture(tex1, uv).rg * -1. * magnitude;
            color = texture(tex0, warp).rgb;
        }
    )glsl";
}


Reprojection::Reprojection(Texture *inputSurface)
        : mInputSurface(inputSurface) {
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

    GLint searchBlockX, searchBlockY;
    glGetIntegerv(GL_MOTION_ESTIMATION_SEARCH_BLOCK_X_QCOM, &searchBlockX);
    glGetIntegerv(GL_MOTION_ESTIMATION_SEARCH_BLOCK_Y_QCOM, &searchBlockY);

    mMotionVector.reset(
            new Texture(false, reprojectionData.eyeWidth * 2 / searchBlockX, reprojectionData.eyeHeight / searchBlockY, GL_RGBA16F));

    mReprojectedTexture.reset(
            new Texture(false, reprojectionData.eyeWidth * 2, reprojectionData.eyeHeight, GL_RGB8));
    mReprojectedTextureState = make_unique<RenderState>(mReprojectedTexture.get());

    auto ReprojectionShaderStr = reprojectionCommonShaderStr + REPROJECTION_FRAGMENT_SHADER;
    mReprojectionPipeline = unique_ptr<RenderPipeline>(
            new RenderPipeline({mTargetTexture.get(), mMotionVector.get()}, QUAD_2D_VERTEX_SHADER,
                               ReprojectionShaderStr, 4));

    mTargetTracking = new ovrTracking2;
    mRefTracking = new ovrTracking2;
    mReprojectedTracking = new ovrTracking2;

    mTargetTime = 0;
    mRefTime = 0;
    refreshRate = reprojectionData.refreshRate;
    frameTime = 1e6 / refreshRate;
    lastSubmit = getTimestampUs();
    emptyFrames = 2;
    frameSent = false;
}

void Reprojection::AddFrame(ovrTracking2 *tracking, uint64_t renderTime) {
    if (emptyFrames < 2) {
        mRefState->ClearDepth();
        mCopyPipeline->Render(*mRefState);
        memcpy(mRefTracking, mTargetTracking, sizeof(ovrTracking2));
        mRefTime = mTargetTime;
    }

    mTargetState->ClearDepth();
    mRGBtoLuminancePipeline->Render(*mTargetState);
    mTargetTracking = tracking;
    memcpy(mTargetTracking, tracking, sizeof(ovrTracking2));
    memcpy(mReprojectedTracking, tracking, sizeof(ovrTracking2));
    mTargetTime = renderTime;

    if (emptyFrames > 0) emptyFrames--;
}

void Reprojection::EstimateMotion() {
    if (emptyFrames > 0) return;
// reversed inputs to TexEstimateMotionQCOM so the starting position doesnt need to be corrected
    GL(glTexEstimateMotionQCOM(mTargetTexture->GetGLTexture(), mRefTexture->GetGLTexture(), mMotionVector->GetGLTexture()));
}

void Reprojection::Reproject(uint64_t displayTime) {
    if (emptyFrames > 0) return;
    float magnitude = (double)(displayTime - mTargetTime) / (double)(mTargetTime - mRefTime);

    mReprojectedTextureState->ClearDepth();
    mReprojectionPipeline->Render(*mReprojectedTextureState, &magnitude);

    mReprojectedTracking->HeadPose.Pose.Orientation = Slerp(mRefTracking->HeadPose.Pose.Orientation, mTargetTracking->HeadPose.Pose.Orientation, 1 + magnitude);
}

bool Reprojection::Render(uint64_t current) {
    if (current < displayTime && emptyFrames == 0 && !frameSent) {
        uint64_t deltaTime = displayTime - current;
        if (deltaTime < 2000) {
            // Less than 2ms remaining
            Reprojection::Reproject(displayTime);
            return true;
        }
    }
    return false;
}

bool Reprojection::GetFrameSent() {
    return frameSent;
}

void Reprojection::FrameSent() {
    frameSent = true;
}

void Reprojection::ResetFrameSent() {
    lastSubmit = getTimestampUs();
    displayTime = lastSubmit + frameTime;
    frameSent = false;
}
