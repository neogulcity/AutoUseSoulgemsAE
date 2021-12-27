#include <Windows.h>

const char* inipath = ".\\Data\\SKSE\\Plugins\\AutoUseSoulgems.ini";
constexpr RE::FormID AzuraStarID = 0x63B27;
constexpr RE::FormID BlackStarID = 0x63B29;
RE::TESForm* AzuraStar;
RE::TESForm* BlackStar;

struct SoulValue
{
	float kSoulValue[5] = { 250.0, 500.0, 1000.0, 2000.0, 3000.0 };
	float kExpValue[5] = { 0.05f, 0.1f, 0.2f, 0.4f, 0.6f };
} SoulValue;

struct IniValue
{
	float fLimit;
	bool bUseAzuraFisrt;
	bool bUseLowestFirst;
	bool bMessage;
} IniValue;

void AddItem(RE::BSScript::IVirtualMachine* a_vm, RE::TESObjectREFR* target, RE::TESForm* form, int32_t count, bool bSilent)
{
	using func_t = void (*)(RE::BSScript::IVirtualMachine * a_vm, std::uint32_t stackId, RE::TESObjectREFR * target, RE::TESForm * form, int32_t count, bool bSilent);
	REL::Relocation<func_t> func{ REL::ID(56145) };
	return func(a_vm, 0, target, form, count, bSilent);
}

bool ReadINI()
{
	char a_char[32];

	GetPrivateProfileStringA("Settings", "fLimit", "100.0", a_char, sizeof(a_char), inipath);
	IniValue.fLimit = std::stof(static_cast<std::string>(a_char)) < 0.0f ? 100.0f : std::stof(static_cast<std::string>(a_char));

	GetPrivateProfileStringA("Settings", "bUseAzuraFirst", "False", a_char, sizeof(a_char), inipath);
	_strlwr_s(a_char, sizeof(a_char));
	IniValue.bUseAzuraFisrt = static_cast<std::string>(a_char) == "true" ? true : false;

	GetPrivateProfileStringA("Settings", "bUseLowestFirst", "False", a_char, sizeof(a_char), inipath);
	_strlwr_s(a_char, sizeof(a_char));
	IniValue.bUseLowestFirst = static_cast<std::string>(a_char) == "true" ? true : false;

	GetPrivateProfileStringA("Settings", "bMessage", "True", a_char, sizeof(a_char), inipath);
	_strlwr_s(a_char, sizeof(a_char));
	IniValue.bMessage = static_cast<std::string>(a_char) == "true" ? true : false;

	return true;
}

bool InitializeData()
{
	AzuraStar = RE::TESDataHandler::GetSingleton()->LookupForm(AzuraStarID, "Skyrim.esm");
	BlackStar = RE::TESDataHandler::GetSingleton()->LookupForm(BlackStarID, "Skyrim.esm");

	return true;
}

class OnHitEvent : public RE::BSTEventSink<RE::TESHitEvent>
{
	OnHitEvent() = default;
	~OnHitEvent() = default;
	OnHitEvent(const OnHitEvent&) = delete;
	OnHitEvent(OnHitEvent&&) = delete;
	OnHitEvent& operator=(const OnHitEvent&) = delete;
	OnHitEvent& operator=(OnHitEvent&&) = delete;

public:
	virtual RE::BSEventNotifyControl ProcessEvent(const RE::TESHitEvent* a_event, RE::BSTEventSource<RE::TESHitEvent>* a_eventSource)
	{
		if (!a_event || !a_eventSource)
			return RE::BSEventNotifyControl::kContinue;

		auto akAggressor = a_event->cause ? a_event->cause->As<RE::Actor>() : nullptr;

		if (!akAggressor)
			return RE::BSEventNotifyControl::kContinue;

		auto playerref = RE::PlayerCharacter::GetSingleton();
		if (akAggressor != playerref)
			return RE::BSEventNotifyControl::kContinue;

		auto rObj = akAggressor->GetEquippedObject(false);
		auto rCharge = akAggressor->GetActorValue(RE::ActorValue::kRightItemCharge);
		if (rObj && rCharge < IniValue.fLimit && rCharge != 0.0f) {
			const auto xContainer = akAggressor ? akAggressor->extraList.GetByType<RE::ExtraContainerChanges>() : nullptr;
			const auto invChanges = xContainer ? xContainer->changes : nullptr;
			float MAXcharge = 0.0f;
			bool IsEnchanted = false;

			if (invChanges) {
				auto inv = playerref->GetInventory();
				for (const auto& [item, data] : inv) {
					const auto& [count, entry] = data;
					if (count > 0 && item == rObj && entry->IsWorn()) {
						IsEnchanted = entry->IsEnchanted() ? true : false;
						auto extraLists = entry->extraLists;
						if (extraLists) {
							for (auto& xList : *extraLists) {
								if (xList) {
									if (xList->HasType(RE::ExtraDataType::kWorn)) {
										auto ench = rObj->As<RE::TESEnchantableForm>();
										if (ench && ench->amountofEnchantment != 0)
											MAXcharge = static_cast<float>(ench->amountofEnchantment);

										auto xEnch = xList->GetByType<RE::ExtraEnchantment>();
										if (xEnch && xEnch->enchantment && xEnch->charge != 0)
											MAXcharge = static_cast<float>(xEnch->charge);
									}
								}
							}
						}
						break;
					}
				}
			}

			if (IsEnchanted) {
				if (rCharge < IniValue.fLimit) {
					std::vector<std::pair<RE::TESBoundObject*, RE::SOUL_LEVEL>> a_Soulgem;
					std::vector<std::pair<RE::TESBoundObject*, RE::SOUL_LEVEL>> a_SoulgemStar;

					auto inv = playerref->GetInventory();
					for (const auto& [item, data] : inv) {
						if (!item->Is(RE::FormType::SoulGem))
							continue;

						const auto& [count, entry] = data;
						if (count > 0 && entry->GetSoulLevel() != RE::SOUL_LEVEL::kNone) {
							if (item == AzuraStar || item == BlackStar)
								a_SoulgemStar.push_back(std::pair<RE::TESBoundObject*, RE::SOUL_LEVEL>(item, entry->GetSoulLevel()));
							else
								a_Soulgem.push_back(std::pair<RE::TESBoundObject*, RE::SOUL_LEVEL>(item, entry->GetSoulLevel()));
						}
					}

					RE::TESBoundObject* UsingSoulgem = nullptr;
					RE::SOUL_LEVEL UsingSoulLevel = RE::SOUL_LEVEL::kNone;

					if (a_SoulgemStar.size() > 0 && !IniValue.bUseAzuraFisrt)
						a_Soulgem.push_back(a_SoulgemStar[0]);

					if (a_Soulgem.size() > 0) {
						if (IniValue.bUseLowestFirst) {
							sort(a_Soulgem.begin(), a_Soulgem.end(), [](auto& lhs, auto& rhs) {
								if (lhs.second == rhs.second)
									return lhs.first < rhs.first;
								return lhs.second < rhs.second;
							});
						} else {
							sort(a_Soulgem.begin(), a_Soulgem.end(), [](auto& lhs, auto& rhs) {
								if (lhs.second == rhs.second)
									return lhs.first > rhs.first;
								return lhs.second > rhs.second;
							});
						}
					}

					if (a_SoulgemStar.size() > 0 && IniValue.bUseAzuraFisrt) {
						UsingSoulgem = a_SoulgemStar[0].first;
						UsingSoulLevel = a_SoulgemStar[0].second;
					} else if (a_Soulgem.size() > 0) {
						UsingSoulgem = a_Soulgem[0].first;
						UsingSoulLevel = a_Soulgem[0].second;
					}

					if (UsingSoulgem) {
						if (IniValue.bMessage) {
							char message[64];
							std::string temp = UsingSoulgem->GetName();
							temp = "Soulgem Used : " + temp;
							strcpy_s(message, temp.c_str());
							RE::DebugNotification(message);
						}

						akAggressor->RemoveItem(UsingSoulgem, 1, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);

						float modvalue = SoulValue.kSoulValue[static_cast<uint32_t>(UsingSoulLevel) - 1];
						modvalue = rCharge + modvalue > MAXcharge ? MAXcharge - rCharge : modvalue;

						akAggressor->ModActorValue(RE::ActorValue::kRightItemCharge, modvalue);

						float expvalue = SoulValue.kExpValue[static_cast<uint32_t>(UsingSoulLevel) - 1];
						playerref->AddSkillExperience(RE::ActorValue::kEnchanting, expvalue);

						if (UsingSoulgem == AzuraStar || UsingSoulgem == BlackStar) {
							auto a_vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
							if (!a_vm)
								return RE::BSEventNotifyControl::kContinue;

							AddItem(a_vm, akAggressor, UsingSoulgem, 1, true);
						}
					}
				}
			}
		}

		auto lObj = akAggressor->GetEquippedObject(true);
		auto lCharge = akAggressor->GetActorValue(RE::ActorValue::kLeftItemCharge);
		if (lObj && lCharge < IniValue.fLimit && lCharge != 0.0f) {
			const auto xContainer = akAggressor ? akAggressor->extraList.GetByType<RE::ExtraContainerChanges>() : nullptr;
			const auto invChanges = xContainer ? xContainer->changes : nullptr;
			float MAXcharge = 0.0f;
			bool IsEnchanted = false;

			if (invChanges) {
				auto inv = playerref->GetInventory();
				for (const auto& [item, data] : inv) {
					const auto& [count, entry] = data;
					if (count > 0 && item == lObj && entry->IsWorn()) {
						IsEnchanted = entry->IsEnchanted() ? true : false;
						auto extraLists = entry->extraLists;
						if (extraLists) {
							for (auto& xList : *extraLists) {
								if (xList) {
									if (xList->HasType(RE::ExtraDataType::kWornLeft)) {
										auto ench = lObj->As<RE::TESEnchantableForm>();
										if (ench && ench->amountofEnchantment != 0)
											MAXcharge = static_cast<float>(ench->amountofEnchantment);

										auto xEnch = xList->GetByType<RE::ExtraEnchantment>();
										if (xEnch && xEnch->enchantment && xEnch->charge != 0)
											MAXcharge = static_cast<float>(xEnch->charge);
									}
								}
							}
						}
						break;
					}
				}
			}

			if (IsEnchanted) {
				if (lCharge < IniValue.fLimit) {
					std::vector<std::pair<RE::TESBoundObject*, RE::SOUL_LEVEL>> a_Soulgem;
					std::vector<std::pair<RE::TESBoundObject*, RE::SOUL_LEVEL>> a_SoulgemStar;

					auto inv = playerref->GetInventory();
					for (const auto& [item, data] : inv) {
						if (!item->Is(RE::FormType::SoulGem))
							continue;

						const auto& [count, entry] = data;
						if (count > 0 && entry->GetSoulLevel() != RE::SOUL_LEVEL::kNone) {
							if (item == AzuraStar || item == BlackStar)
								a_SoulgemStar.push_back(std::pair<RE::TESBoundObject*, RE::SOUL_LEVEL>(item, entry->GetSoulLevel()));
							else
								a_Soulgem.push_back(std::pair<RE::TESBoundObject*, RE::SOUL_LEVEL>(item, entry->GetSoulLevel()));
						}
					}

					RE::TESBoundObject* UsingSoulgem = nullptr;
					RE::SOUL_LEVEL UsingSoulLevel = RE::SOUL_LEVEL::kNone;

					if (a_SoulgemStar.size() > 0 && !IniValue.bUseAzuraFisrt)
						a_Soulgem.push_back(a_SoulgemStar[0]);

					if (a_Soulgem.size() > 0) {
						if (IniValue.bUseLowestFirst) {
							sort(a_Soulgem.begin(), a_Soulgem.end(), [](auto& lhs, auto& rhs) {
								if (lhs.second == rhs.second)
									return lhs.first < rhs.first;
								return lhs.second < rhs.second;
							});
						} else {
							sort(a_Soulgem.begin(), a_Soulgem.end(), [](auto& lhs, auto& rhs) {
								if (lhs.second == rhs.second)
									return lhs.first > rhs.first;
								return lhs.second > rhs.second;
							});
						}
					}

					if (a_SoulgemStar.size() > 0 && IniValue.bUseAzuraFisrt) {
						UsingSoulgem = a_SoulgemStar[0].first;
						UsingSoulLevel = a_SoulgemStar[0].second;
					} else if (a_Soulgem.size() > 0) {
						UsingSoulgem = a_Soulgem[0].first;
						UsingSoulLevel = a_Soulgem[0].second;
					}

					if (UsingSoulgem) {
						if (IniValue.bMessage) {
							char message[64];
							std::string temp = UsingSoulgem->GetName();
							temp = "Soulgem Used : " + temp;
							strcpy_s(message, temp.c_str());
							RE::DebugNotification(message);
						}

						akAggressor->RemoveItem(UsingSoulgem, 1, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);

						float modvalue = SoulValue.kSoulValue[static_cast<uint32_t>(UsingSoulLevel) - 1];
						modvalue = lCharge + modvalue > MAXcharge ? MAXcharge - lCharge : modvalue;

						akAggressor->ModActorValue(RE::ActorValue::kLeftItemCharge, modvalue);

						float expvalue = SoulValue.kExpValue[static_cast<uint32_t>(UsingSoulLevel) - 1];
						playerref->AddSkillExperience(RE::ActorValue::kEnchanting, expvalue);

						if (UsingSoulgem == AzuraStar || UsingSoulgem == BlackStar) {
							auto a_vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
							if (!a_vm)
								return RE::BSEventNotifyControl::kContinue;

							AddItem(a_vm, akAggressor, UsingSoulgem, 1, true);
						}
					}
				}
			}
		}
		return RE::BSEventNotifyControl::kContinue;
	}

	static bool RegisterEvent()
	{
		static OnHitEvent g_hiteventhandler;

		auto ScriptEventSource = RE::ScriptEventSourceHolder::GetSingleton();

		if (!ScriptEventSource) {
			return false;
		}

		ScriptEventSource->AddEventSink(&g_hiteventhandler);

		return true;
	}
};

void RegisterEvent(SKSE::MessagingInterface::Message* msg)
{
	if (msg->type == SKSE::MessagingInterface::kDataLoaded) {
		OnHitEvent::RegisterEvent();
		ReadINI();
		InitializeData();
	}
}

extern "C" DLLEXPORT bool SKSEAPI SKSEPlugin_Query(const SKSE::QueryInterface* a_skse, SKSE::PluginInfo* a_info)
{
#ifndef NDEBUG
	auto sink = std::make_shared<spdlog::sinks::msvc_sink_mt>();
#else
	auto path = logger::log_directory();
	if (!path) {
		return false;
	}

	*path /= Version::PROJECT;
	*path += ".log"sv;
	auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true);
#endif

	auto log = std::make_shared<spdlog::logger>("global log"s, std::move(sink));

#ifndef NDEBUG
	log->set_level(spdlog::level::trace);
#else
	log->set_level(spdlog::level::info);
	log->flush_on(spdlog::level::warn);
#endif

	spdlog::set_default_logger(std::move(log));
	spdlog::set_pattern("%g(%#): [%^%l%$] %v"s);

	logger::info(FMT_STRING("{} v{}"), Version::PROJECT, Version::NAME);

	a_info->infoVersion = SKSE::PluginInfo::kVersion;
	a_info->name = Version::PROJECT.data();
	a_info->version = Version::MAJOR;

	if (a_skse->IsEditor()) {
		logger::critical("Loaded in editor, marking as incompatible"sv);
		return false;
	}

	const auto ver = a_skse->RuntimeVersion();
	if (ver < SKSE::RUNTIME_1_5_39) {
		logger::critical(FMT_STRING("Unsupported runtime version {}"), ver.string());
		return false;
	}

	return true;
}

extern "C" DLLEXPORT bool SKSEAPI SKSEPlugin_Load(const SKSE::LoadInterface* a_skse)
{
	logger::info("loaded");

	SKSE::Init(a_skse);

	auto g_message = SKSE::GetMessagingInterface();
	if (!g_message->RegisterListener(RegisterEvent)) {
		return false;
	}

	return true;
}

extern "C" DLLEXPORT constinit auto SKSEPlugin_Version = []() {
	SKSE::PluginVersionData data{};
	data.pluginVersion = Version::MAJOR;
	data.PluginName(Version::NAME);
	data.AuthorName("neogulcity"sv);
	data.CompatibleVersions({ SKSE::RUNTIME_LATEST });
	data.UsesAddressLibrary(true);
	return data;
}();
