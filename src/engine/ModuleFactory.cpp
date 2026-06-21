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
    }
    return nullptr;
}
