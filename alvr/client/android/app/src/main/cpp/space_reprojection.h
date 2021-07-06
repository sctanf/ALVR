#pragma once

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
    Reprojection(gl_render_utils::Texture *mInputSurface);

    void Initialize(ReprojectionData reprojectionData);

    void AddFrame(ovrTracking2 *tracking, uint64_t renderTime);

    void EstimateMotion();

    void Reproject(uint64_t displayTime);

    void Render();

private:

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
};
