#include "ModuleFactory.h"
#include "../modules/GainModule.h"
#include "../modules/ParametricEQModule.h"
#include "../modules/GateModule.h"
#include "../modules/CompressorModule.h"
#include "../modules/LimiterModule.h"
#include "../modules/ReverbModule.h"
#include "../modules/DelayModule.h"
#include "../modules/SaturatorModule.h"
#include "../modules/UtilityModule.h"
#include "../modules/DenoiseModule.h"
#include "../modules/HumRemoverModule.h"
#include "../modules/DeEsserModule.h"
#include "../modules/DynamicEQModule.h"
#include "../modules/MultibandCompressorModule.h"
#include "../modules/SpectralRepairModule.h"
#include "../modules/TransientShaperModule.h"
#include "../modules/StereoWidthModule.h"

std::unique_ptr<RackModule> makeModule (ModuleType type)
{
    switch (type)
    {
    case ModuleType::Gain:
        return std::make_unique<GainModule> ();
    case ModuleType::ParametricEQ:
        return std::make_unique<ParametricEQModule> ();
    case ModuleType::Gate:
        return std::make_unique<GateModule> ();
    case ModuleType::Compressor:
        return std::make_unique<CompressorModule> ();
    case ModuleType::Limiter:
        return std::make_unique<LimiterModule> ();
    case ModuleType::Reverb:
        return std::make_unique<ReverbModule> ();
    case ModuleType::Delay:
        return std::make_unique<DelayModule> ();
    case ModuleType::Saturator:
        return std::make_unique<SaturatorModule> ();
    case ModuleType::Utility:
        return std::make_unique<UtilityModule> ();
    case ModuleType::Denoise:
        return std::make_unique<DenoiseModule> ();
    case ModuleType::HumRemover:
        return std::make_unique<HumRemoverModule> ();
    case ModuleType::DeEsser:
        return std::make_unique<DeEsserModule> ();
    case ModuleType::DynamicEQ:
        return std::make_unique<DynamicEQModule> ();
    case ModuleType::MultibandComp:
        return std::make_unique<MultibandCompressorModule> ();
    case ModuleType::SpectralRepair:
        return std::make_unique<SpectralRepairModule> ();
    case ModuleType::TransientShaper:
        return std::make_unique<TransientShaperModule> ();
    case ModuleType::StereoWidth:
        return std::make_unique<StereoWidthModule> ();
    }
    return nullptr;
}
