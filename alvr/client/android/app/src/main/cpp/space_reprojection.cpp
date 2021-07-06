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
            //uv -= texture(tex1, uv).rg * magnitude.magnitude;
            color = texture(tex0, uv);
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
            new RenderPipeline({mInputSurface, mMotionVector.get()}, QUAD_2D_VERTEX_SHADER,
                               ReprojectionShaderStr, 4));

    mTargetTime = 0;
    mRefTime = 0;
    emptyFrames = 2;
    frameSent = false;
}

void Reprojection::AddFrame(ovrTracking2 *tracking, uint64_t renderTime) {
    mCopyPipeline->Render(*mRefState);
    //mRefTracking = mTargetTracking;
    memcpy(mRefTracking, mTargetTracking, sizeof(ovrTracking2));
    mRefTime = mTargetTime;

    mRGBtoLuminancePipeline->Render(*mTargetState);
    //mTargetTracking = tracking;
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

    mReprojectionPipeline->Render(*mReprojectedTextureState, &magnitude);

    mReprojectedTracking->HeadPose.Pose.Orientation = Slerp(mRefTracking->HeadPose.Pose.Orientation, mTargetTracking->HeadPose.Pose.Orientation, 1 + magnitude);
}

//Reprojection::Render((vrapi_GetPredictedDisplayTime(OvrContext.Ovr, OvrContext.FrameIndex) - vrapi_GetTimeInSeconds()) * 1000 * 1000);
void Reprojection::Render(uint64_t deltaTime) {
    if (emptyFrames > 0) return;
    if (!frameSent) {
        const uint64_t currentTimeUs = getTimestampUs();
        if (deltaTime < 2000) {
            // Less than 2ms remaining, submit a frame now
            Reprojection::Reproject(currentTimeUs + deltaTime);
            // Submit the reprojected frame
        }
    }
}

void Reprojection::Reset() {
    frameSent = true; //need to set to false after vsync
}
