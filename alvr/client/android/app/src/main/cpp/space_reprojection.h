#pragma once

#include <memory>
#include <vector>

#include "gl_render_utils/render_pipeline.h"
#include "packet_types.h"

struct ReprojectionData {
    bool enabled;
    uint32_t eyeWidth;
    uint32_t eyeHeight;
};

struct ReprojectionFrame {
    gl_render_utils::Texture *texture;
    gl_render_utils::RenderState *state;
    ovrTracking2 *tracking;
    uint64_t time;
};

class Reprojection {
public:
    Reprojection(gl_render_utils::Texture *mInputSurface);

    void AddFrame(ovrTracking2 *tracking, uint64_t renderTime);

private:

    gl_render_utils::Texture *mInputSurface;

    ReprojectionFrame *mFrameTarget;
    ReprojectionFrame *mFrameRef;
    std::unique_ptr<gl_render_utils::RenderPipeline> mRGBtoLuminancePipeline;
    std::unique_ptr<gl_render_utils::RenderPipeline> mCopyPipeline;

    std::unique_ptr<gl_render_utils::Texture> mMotionVector;
    std::unique_ptr<gl_render_utils::RenderState> mMotionVectorState;
    std::unique_ptr<gl_render_utils::RenderPipeline> mMotionEstimationPipeline;

    std::unique_ptr<gl_render_utils::Texture> mReprojectedTexture;
    std::unique_ptr<gl_render_utils::RenderState> mReprojectedTextureState;
    std::unique_ptr<gl_render_utils::RenderPipeline> mReprojectionPipeline;
};
