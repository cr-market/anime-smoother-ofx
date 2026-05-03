#include "LoiloSmoothCore.h"

#include "ofxsImageEffect.h"
#include "ofxsProcessing.h"

#include <cstring>
#include <memory>
#include <string>
#include <vector>

namespace {

constexpr const char* kPluginIdentifier = "com.example.AnimeSmoother";
constexpr int kPluginVersionMajor = 0;
constexpr int kPluginVersionMinor = 42;

constexpr const char* kParamEnabled = "enabled";
constexpr const char* kParamBlendStrength = "blendStrength";
constexpr const char* kParamSmoothRange = "smoothRange";
constexpr const char* kParamAlphaAware = "alphaAware";
constexpr const char* kParamProcessPremultiplied = "processPremultiplied";
constexpr const char* kParamQuality = "quality";

template <class T>
float fromStorage(T value);

template <>
float fromStorage<unsigned char>(unsigned char value)
{
    return static_cast<float>(value) / 255.0f;
}

template <>
float fromStorage<unsigned short>(unsigned short value)
{
    return static_cast<float>(value) / 65535.0f;
}

template <>
float fromStorage<float>(float value)
{
    return value;
}

template <class T>
T toStorage(float value);

template <>
unsigned char toStorage<unsigned char>(float value)
{
    return static_cast<unsigned char>(mlaa::clamp01(value) * 255.0f + 0.5f);
}

template <>
unsigned short toStorage<unsigned short>(float value)
{
    return static_cast<unsigned short>(mlaa::clamp01(value) * 65535.0f + 0.5f);
}

template <>
float toStorage<float>(float value)
{
    return value;
}

template <class T, int Components>
mlaa::Pixel readPixel(const OFX::Image& image, int x, int y)
{
    const T* ptr = static_cast<const T*>(image.getPixelAddress(x, y));
    if (!ptr) {
        return {};
    }
    if constexpr (Components == 1) {
        const float a = fromStorage<T>(ptr[0]);
        return {a, a, a, a};
    } else if constexpr (Components == 3) {
        return {
            fromStorage<T>(ptr[0]),
            fromStorage<T>(ptr[1]),
            fromStorage<T>(ptr[2]),
            1.0f,
        };
    } else {
        return {
            fromStorage<T>(ptr[0]),
            fromStorage<T>(ptr[1]),
            fromStorage<T>(ptr[2]),
            fromStorage<T>(ptr[3]),
        };
    }
}

template <class T, int Components>
void writePixel(OFX::Image& image, int x, int y, const mlaa::Pixel& p)
{
    T* ptr = static_cast<T*>(image.getPixelAddress(x, y));
    if (!ptr) {
        return;
    }
    if constexpr (Components == 1) {
        ptr[0] = toStorage<T>(p.a);
    } else if constexpr (Components == 3) {
        ptr[0] = toStorage<T>(p.r);
        ptr[1] = toStorage<T>(p.g);
        ptr[2] = toStorage<T>(p.b);
    } else {
        ptr[0] = toStorage<T>(p.r);
        ptr[1] = toStorage<T>(p.g);
        ptr[2] = toStorage<T>(p.b);
        ptr[3] = toStorage<T>(p.a);
    }
}

class AnimeLinePlugin final : public OFX::ImageEffect {
public:
    explicit AnimeLinePlugin(OfxImageEffectHandle handle)
        : ImageEffect(handle)
        , sourceClip_(fetchClip(kOfxImageEffectSimpleSourceClipName))
        , outputClip_(fetchClip(kOfxImageEffectOutputClipName))
        , enabled_(fetchBooleanParam(kParamEnabled))
        , blendStrength_(fetchDoubleParam(kParamBlendStrength))
        , smoothRange_(fetchDoubleParam(kParamSmoothRange))
        , alphaAware_(fetchBooleanParam(kParamAlphaAware))
        , processPremultiplied_(fetchBooleanParam(kParamProcessPremultiplied))
        , quality_(fetchChoiceParam(kParamQuality))
    {
    }

    void render(const OFX::RenderArguments& args) override
    {
        std::unique_ptr<OFX::Image> src(sourceClip_->fetchImage(args.time));
        std::unique_ptr<OFX::Image> dst(outputClip_->fetchImage(args.time));
        if (!src || !dst) {
            OFX::throwSuiteStatusException(kOfxStatFailed);
        }

        const OfxRectI renderWindow = args.renderWindow;
        const int width = renderWindow.x2 - renderWindow.x1;
        const int height = renderWindow.y2 - renderWindow.y1;

        if (src->getPixelDepth() == OFX::eBitDepthUByte) {
            dispatch<unsigned char>(*src, *dst, renderWindow, width, height, args.time);
        } else if (src->getPixelDepth() == OFX::eBitDepthUShort) {
            dispatch<unsigned short>(*src, *dst, renderWindow, width, height, args.time);
        } else if (src->getPixelDepth() == OFX::eBitDepthFloat) {
            dispatch<float>(*src, *dst, renderWindow, width, height, args.time);
        } else {
            OFX::throwSuiteStatusException(kOfxStatErrUnsupported);
        }
    }

    bool isIdentity(const OFX::IsIdentityArguments& args, OFX::Clip*& identityClip, double& identityTime) override
    {
        bool enabled = true;
        double blendStrength = 0.0;
        enabled_->getValueAtTime(args.time, enabled);
        blendStrength_->getValueAtTime(args.time, blendStrength);
        if (!enabled || blendStrength <= 0.0) {
            identityClip = sourceClip_;
            identityTime = args.time;
            return true;
        }
        return false;
    }

    void getRegionsOfInterest(const OFX::RegionsOfInterestArguments& args, OFX::RegionOfInterestSetter& rois) override
    {
        int quality = 1;
        quality_->getValueAtTime(args.time, quality);
        const int margin = quality == 2 ? 10 : (quality == 1 ? 8 : 4);
        OfxRectD roi = args.regionOfInterest;
        roi.x1 -= margin;
        roi.y1 -= margin;
        roi.x2 += margin;
        roi.y2 += margin;
        rois.setRegionOfInterest(*sourceClip_, roi);
    }

private:
    mlaa::Settings settingsAt(double time) const
    {
        mlaa::Settings settings;
        bool boolValue = false;
        int intValue = 0;
        double doubleValue = 0.0;

        enabled_->getValueAtTime(time, boolValue);
        settings.enabled = boolValue;
        blendStrength_->getValueAtTime(time, doubleValue);
        settings.blendStrength = static_cast<float>(doubleValue);
        smoothRange_->getValueAtTime(time, doubleValue);
        settings.smoothRange = static_cast<float>(doubleValue);
        alphaAware_->getValueAtTime(time, boolValue);
        settings.alphaAware = boolValue;
        processPremultiplied_->getValueAtTime(time, boolValue);
        settings.processPremultiplied = boolValue;
        quality_->getValueAtTime(time, intValue);
        settings.quality = intValue >= 1 ? mlaa::Quality::High : mlaa::Quality::Standard;
        settings.smoothingPasses = intValue == 2 ? 2 : 1;
        settings.debugView = mlaa::DebugView::Final;
        return settings;
    }

    template <class T>
    void dispatch(OFX::Image& src, OFX::Image& dst, const OfxRectI& renderWindow, int width, int height, double time)
    {
        if (src.getPixelComponents() == OFX::ePixelComponentRGBA) {
            processTyped<T, 4>(src, dst, renderWindow, width, height, time);
        } else if (src.getPixelComponents() == OFX::ePixelComponentRGB) {
            processTyped<T, 3>(src, dst, renderWindow, width, height, time);
        } else if (src.getPixelComponents() == OFX::ePixelComponentAlpha) {
            processTyped<T, 1>(src, dst, renderWindow, width, height, time);
        } else {
            OFX::throwSuiteStatusException(kOfxStatErrUnsupported);
        }
    }

    template <class T, int Components>
    void processTyped(OFX::Image& src, OFX::Image& dst, const OfxRectI& renderWindow, int width, int height, double time)
    {
        std::vector<mlaa::Pixel> srcPixels(static_cast<std::size_t>(width) * height);
        std::vector<mlaa::Pixel> dstPixels(static_cast<std::size_t>(width) * height);

        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                srcPixels[static_cast<std::size_t>(y) * width + x] =
                    readPixel<T, Components>(src, renderWindow.x1 + x, renderWindow.y1 + y);
            }
        }

        loilosmooth::process(srcPixels.data(), dstPixels.data(), width, height, settingsAt(time));

        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                writePixel<T, Components>(dst, renderWindow.x1 + x, renderWindow.y1 + y,
                    dstPixels[static_cast<std::size_t>(y) * width + x]);
            }
        }
    }

    OFX::Clip* sourceClip_;
    OFX::Clip* outputClip_;
    OFX::BooleanParam* enabled_;
    OFX::DoubleParam* blendStrength_;
    OFX::DoubleParam* smoothRange_;
    OFX::BooleanParam* alphaAware_;
    OFX::BooleanParam* processPremultiplied_;
    OFX::ChoiceParam* quality_;
};

class AnimeLinePluginFactory final : public OFX::PluginFactoryHelper<AnimeLinePluginFactory> {
public:
    AnimeLinePluginFactory()
        : PluginFactoryHelper(kPluginIdentifier, kPluginVersionMajor, kPluginVersionMinor)
    {
    }

    void describe(OFX::ImageEffectDescriptor& desc) override
    {
        desc.setLabels("Anime Smoother", "Anime Smoother", "Anime Smoother");
        desc.setPluginGrouping("Filter/Anime");
        desc.addSupportedContext(OFX::eContextFilter);
        desc.addSupportedContext(OFX::eContextGeneral);
        desc.addSupportedBitDepth(OFX::eBitDepthUByte);
        desc.addSupportedBitDepth(OFX::eBitDepthUShort);
        desc.addSupportedBitDepth(OFX::eBitDepthFloat);
        desc.setSingleInstance(false);
        desc.setHostFrameThreading(false);
        desc.setSupportsMultiResolution(false);
        desc.setSupportsTiles(false);
        desc.setRenderThreadSafety(OFX::eRenderFullySafe);
        desc.setTemporalClipAccess(false);
    }

    void describeInContext(OFX::ImageEffectDescriptor& desc, OFX::ContextEnum) override
    {
        OFX::ClipDescriptor* src = desc.defineClip(kOfxImageEffectSimpleSourceClipName);
        src->addSupportedComponent(OFX::ePixelComponentRGBA);
        src->addSupportedComponent(OFX::ePixelComponentRGB);
        src->addSupportedComponent(OFX::ePixelComponentAlpha);
        src->setTemporalClipAccess(false);
        src->setSupportsTiles(false);

        OFX::ClipDescriptor* dst = desc.defineClip(kOfxImageEffectOutputClipName);
        dst->addSupportedComponent(OFX::ePixelComponentRGBA);
        dst->addSupportedComponent(OFX::ePixelComponentRGB);
        dst->addSupportedComponent(OFX::ePixelComponentAlpha);
        dst->setSupportsTiles(false);

        OFX::PageParamDescriptor* page = desc.definePageParam("Controls");

        OFX::BooleanParamDescriptor* enabled = desc.defineBooleanParam(kParamEnabled);
        enabled->setLabels("Enabled", "Enabled", "Enabled");
        enabled->setDefault(true);
        page->addChild(*enabled);

        OFX::DoubleParamDescriptor* blendStrength = desc.defineDoubleParam(kParamBlendStrength);
        blendStrength->setLabels("Smoothness", "Smoothness", "Smoothness");
        blendStrength->setRange(0.0, 1.0);
        blendStrength->setDisplayRange(0.0, 1.0);
        blendStrength->setDefault(0.75);
        page->addChild(*blendStrength);

        OFX::DoubleParamDescriptor* smoothRange = desc.defineDoubleParam(kParamSmoothRange);
        smoothRange->setLabels("Smooth Range", "Smooth Range", "Smooth Range");
        smoothRange->setRange(0.0, 20.0);
        smoothRange->setDisplayRange(0.0, 20.0);
        smoothRange->setDefault(1.0);
        page->addChild(*smoothRange);

        OFX::BooleanParamDescriptor* alphaAware = desc.defineBooleanParam(kParamAlphaAware);
        alphaAware->setLabels("Alpha Aware", "Alpha Aware", "Alpha Aware");
        alphaAware->setDefault(true);
        page->addChild(*alphaAware);

        OFX::BooleanParamDescriptor* processPremultiplied = desc.defineBooleanParam(kParamProcessPremultiplied);
        processPremultiplied->setLabels("Process Premultiplied", "Process Premultiplied", "Process Premultiplied");
        processPremultiplied->setDefault(false);
        page->addChild(*processPremultiplied);

        OFX::ChoiceParamDescriptor* quality = desc.defineChoiceParam(kParamQuality);
        quality->setLabels("Smoothing Mode", "Smoothing Mode", "Smoothing Mode");
        quality->appendOption("Standard");
        quality->appendOption("Smooth");
        quality->appendOption("Extra Smooth");
        quality->setDefault(0);
        page->addChild(*quality);
    }

    OFX::ImageEffect* createInstance(OfxImageEffectHandle handle, OFX::ContextEnum) override
    {
        return new AnimeLinePlugin(handle);
    }
};

static AnimeLinePluginFactory p;

} // namespace

void OFX::Plugin::getPluginIDs(OFX::PluginFactoryArray& ids)
{
    ids.push_back(&p);
}
