#pragma once

#include <EGL/egl.h>

#include <GLES3/gl3.h>
#include <GLES2/gl2ext.h>

#include <VrApi.h>
#include <VrApi_Types.h>
#include <memory>
#include <vector>

#include "gl_render_utils/render_pipeline.h"
#include "packet_types.h"

struct ReprojectionData {
    bool enabled;
    uint32_t eyeWidth;
    uint32_t eyeHeight;
};

class Reprojection {
public:
    Reprojection(gl_render_utils::Texture *inputSurface);

    void Initialize(ReprojectionData reprojectionData);

    void AddFrame(ovrTracking2 *tracking, uint64_t renderTime);

    void EstimateMotion();

    void Reproject(uint64_t displayTime);

    void Render(uint64_t deltaTime);

    void Reset();

    gl_render_utils::Texture *GetOutputTexture() { return mReprojectedTexture.get(); }

private:

    uint32_t emptyFrames;
    bool frameSent;

// need to add a check for extension string as this is not supported on quest 1
    PFNGLTEXESTIMATEMOTIONQCOMPROC glTexEstimateMotionQCOM = (PFNGLTEXESTIMATEMOTIONQCOMPROC)eglGetProcAddress("glTexEstimateMotionQCOM");

    gl_render_utils::Texture *mInputSurface;

    std::unique_ptr<gl_render_utils::Texture> mTargetTexture;
    std::unique_ptr<gl_render_utils::RenderState> mTargetState;
    ovrTracking2 *mTargetTracking;
    uint64_t mTargetTime;

    std::unique_ptr<gl_render_utils::Texture> mRefTexture;
    std::unique_ptr<gl_render_utils::RenderState> mRefState;
    ovrTracking2 *mRefTracking;
    uint64_t mRefTime;

    std::unique_ptr<gl_render_utils::RenderPipeline> mRGBtoLuminancePipeline;
    std::unique_ptr<gl_render_utils::RenderPipeline> mCopyPipeline;

    std::unique_ptr<gl_render_utils::Texture> mMotionVector;

    std::unique_ptr<gl_render_utils::Texture> mReprojectedTexture;
    std::unique_ptr<gl_render_utils::RenderState> mReprojectedTextureState;
    std::unique_ptr<gl_render_utils::RenderPipeline> mReprojectionPipeline;
    ovrTracking2 *mReprojectedTracking;
};
