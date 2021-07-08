#pragma once

#include <EGL/egl.h>

#include <GLES3/gl3.h>
#include <GLES2/gl2ext.h>

#ifndef GL_QCOM_motion_estimation
#define GL_QCOM_motion_estimation 1
#define GL_MOTION_ESTIMATION_SEARCH_BLOCK_X_QCOM 0x8C90
#define GL_MOTION_ESTIMATION_SEARCH_BLOCK_Y_QCOM 0x8C91
typedef void (GL_APIENTRYP PFNGLTEXESTIMATEMOTIONQCOMPROC) (GLuint ref, GLuint target, GLuint output);
typedef void (GL_APIENTRYP PFNGLTEXESTIMATEMOTIONREGIONSQCOMPROC) (GLuint ref, GLuint target, GLuint output, GLuint mask);
#ifdef GL_GLEXT_PROTOTYPES
GL_APICALL void GL_APIENTRY glTexEstimateMotionQCOM (GLuint ref, GLuint target, GLuint output);
GL_APICALL void GL_APIENTRY glTexEstimateMotionRegionsQCOM (GLuint ref, GLuint target, GLuint output, GLuint mask);
#endif
#endif /* GL_QCOM_motion_estimation */

#include <VrApi_Types.h>
#include <memory>
#include <vector>

#include "gl_render_utils/render_pipeline.h"
#include "packet_types.h"

struct ReprojectionData {
    bool enabled;
    uint32_t eyeWidth;
    uint32_t eyeHeight;
    float refreshRate;
};

class Reprojection {
public:
    Reprojection(gl_render_utils::Texture *inputSurface);

    void Initialize(ReprojectionData reprojectionData);

    void AddFrame(ovrTracking2 *tracking, uint64_t renderTime);

    void EstimateMotion();

    void Reproject(uint64_t displayTime);

    bool Check(uint64_t current);

    bool GetFrameSent();
    void FrameSent();
    void ResetFrameSent();

    gl_render_utils::Texture *GetOutputTexture() { return mReprojectedTexture.get(); }
    ovrTracking2 *GetOutputTracking() { return mReprojectedTracking; }

private:

    float refreshRate;
    uint64_t frameTime;
    uint64_t lastSubmit;
    uint64_t displayTime;
    uint32_t emptyFrames;
    bool frameSent;

// need to add a check for extension string as this is not supported on quest 1
    PFNGLTEXESTIMATEMOTIONQCOMPROC glTexEstimateMotionQCOM = (PFNGLTEXESTIMATEMOTIONQCOMPROC)eglGetProcAddress("glTexEstimateMotionQCOM");

    gl_render_utils::Texture *mInputSurface;
    std::unique_ptr<gl_render_utils::RenderState> mInputSurfaceState;

    std::unique_ptr<gl_render_utils::Texture> mInputTexture;
    std::unique_ptr<gl_render_utils::RenderState> mInputState;

    std::unique_ptr<gl_render_utils::RenderPipeline> mCopyInputPipeline;

    std::unique_ptr<gl_render_utils::Texture> mTargetTexture;
    std::unique_ptr<gl_render_utils::RenderState> mTargetState;
    ovrTracking2 *mTargetTracking;
    uint64_t mTargetTime;

    std::unique_ptr<gl_render_utils::RenderPipeline> mRGBtoLuminancePipeline;

    std::unique_ptr<gl_render_utils::Texture> mRefTexture;
    std::unique_ptr<gl_render_utils::RenderState> mRefState;
    ovrTracking2 *mRefTracking;
    uint64_t mRefTime;

    std::unique_ptr<gl_render_utils::RenderPipeline> mCopyPipeline;

    std::unique_ptr<gl_render_utils::Texture> mMotionVector;

    std::unique_ptr<gl_render_utils::Texture> mReprojectedTexture;
    std::unique_ptr<gl_render_utils::RenderState> mReprojectedState;
    std::unique_ptr<gl_render_utils::RenderPipeline> mReprojectionPipeline;
    ovrTracking2 *mReprojectedTracking;
};
