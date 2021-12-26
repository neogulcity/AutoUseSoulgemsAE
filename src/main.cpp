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
	REL::Relocation<func_t> func{ REL::ID(55616) };
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
	if (AzuraStar)
		logger::debug("Azura's Star : {}", AzuraStar->GetName());
	else
		logger::debug("Azura's Star not found");

	BlackStar = RE::TESDataHandler::GetSingleton()->LookupForm(BlackStarID, "Skyrim.esm");
	if (BlackStar)
		logger::debug("The Black Star : {}", BlackStar->GetName());
	else
		logger::debug("The Black Star not found");

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

		logger::debug("OnHitEvent fired");

		auto akAggressor = a_event->cause ? a_event->cause->As<RE::Actor>() : nullptr;

		if (!akAggressor)
			return RE::BSEventNotifyControl::kContinue;

		auto playerref = RE::PlayerCharacter::GetSingleton();
		if (akAggressor != playerref)
			return RE::BSEventNotifyControl::kContinue;

		logger::debug("Player is akAggressor");

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
						logger::debug("Right entry name : {}", entry->GetDisplayName());
						IsEnchanted = entry->IsEnchanted() ? true : false;

						RE::WEAPON_TYPE a_type = rObj->As<RE::TESObjectWEAP>()->GetWeaponType();
						if (a_type == RE::WEAPON_TYPE::kBow || a_type == RE::WEAPON_TYPE::kCrossbow || a_type == RE::WEAPON_TYPE::kTwoHandAxe || a_type == RE::WEAPON_TYPE::kTwoHandSword) {
							auto ench = rObj->As<RE::TESEnchantableForm>();
							if (ench) {
								MAXcharge = static_cast<float>(ench->amountofEnchantment);
								logger::debug("amount : {}", MAXcharge);
								break;
							}
						} else {
							auto extraLists = entry->extraLists;
							if (extraLists) {
								for (auto& xList : *extraLists) {
									if (xList) {
										if (xList->HasType(RE::ExtraDataType::kWorn)) {
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
			}

			if (IsEnchanted) {
				logger::debug("Right weapon is enchnated");

				logger::debug("Charge : {}", rCharge);

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

					bool IsAzuraUsed = false;
					RE::TESBoundObject* UsingSoulgem;
					RE::SOUL_LEVEL UsingSoulLevel;
					if (a_SoulgemStar.size() > 0) {
						if (IniValue.bUseAzuraFisrt) {
							IsAzuraUsed = true;
							UsingSoulgem = a_SoulgemStar[0].first;
							UsingSoulLevel = a_SoulgemStar[0].second;

							if (IniValue.bMessage) {
								char message[64];
								std::string temp = UsingSoulgem->GetName();
								temp = temp + " Used";
								strcpy_s(message, temp.c_str());
								RE::DebugNotification(message);
							}

							akAggressor->RemoveItem(UsingSoulgem, 1, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);

							float modvalue = SoulValue.kSoulValue[static_cast<uint32_t>(UsingSoulLevel) - 1];
							modvalue = rCharge + modvalue > MAXcharge ? MAXcharge - rCharge : modvalue;

							logger::debug("modvalue : {}", modvalue);

							akAggressor->ModActorValue(RE::ActorValue::kRightItemCharge, modvalue);

							float expvalue = SoulValue.kExpValue[static_cast<uint32_t>(UsingSoulLevel) - 1];
							playerref->AddSkillExperience(RE::ActorValue::kEnchanting, expvalue);

							auto a_vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
							if (!a_vm)
								return RE::BSEventNotifyControl::kContinue;

							AddItem(a_vm, akAggressor, UsingSoulgem, 1, true);
						} else {
							a_Soulgem.push_back(a_SoulgemStar[0]);
						}
					}

					if (!IsAzuraUsed && a_Soulgem.size() > 0) {
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

						UsingSoulgem = a_Soulgem[0].first;
						UsingSoulLevel = a_Soulgem[0].second;

						if (IniValue.bMessage) {
							char message[64];
							std::string temp = UsingSoulgem->GetName();
							temp = temp + " Used";
							strcpy_s(message, temp.c_str());
							RE::DebugNotification(message);
						}

						akAggressor->RemoveItem(UsingSoulgem, 1, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);

						float modvalue = SoulValue.kSoulValue[static_cast<uint32_t>(UsingSoulLevel) - 1];
						modvalue = rCharge + modvalue > MAXcharge ? MAXcharge - rCharge : modvalue;

						akAggressor->ModActorValue(RE::ActorValue::kRightItemCharge, modvalue);

						float expvalue = SoulValue.kExpValue[static_cast<uint32_t>(UsingSoulLevel) - 1];
						playerref->AddSkillExperience(RE::ActorValue::kEnchanting, expvalue);
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
						logger::debug("Entry name : {}", entry->GetDisplayName());
						IsEnchanted = entry->IsEnchanted() ? true : false;
						auto extraLists = entry->extraLists;
						if (extraLists) {
							for (auto& xList : *extraLists) {
								if (xList) {
									if (xList->HasType(RE::ExtraDataType::kWornLeft)) {
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
				logger::debug("Left weapon is enchnated");

				logger::debug("Charge : {}", lCharge);

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

					bool IsAzuraUsed = false;
					RE::TESBoundObject* UsingSoulgem;
					RE::SOUL_LEVEL UsingSoulLevel;
					if (a_SoulgemStar.size() > 0) {
						if (IniValue.bUseAzuraFisrt) {
							IsAzuraUsed = true;
							UsingSoulgem = a_SoulgemStar[0].first;
							UsingSoulLevel = a_SoulgemStar[0].second;

							if (IniValue.bMessage) {
								char message[64];
								std::string temp = UsingSoulgem->GetName();
								temp = temp + " Used";
								strcpy_s(message, temp.c_str());
								RE::DebugNotification(message);
							}

							akAggressor->RemoveItem(UsingSoulgem, 1, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);

							float modvalue = SoulValue.kSoulValue[static_cast<uint32_t>(UsingSoulLevel) - 1];
							modvalue = lCharge + modvalue > MAXcharge ? MAXcharge - lCharge : modvalue;

							akAggressor->ModActorValue(RE::ActorValue::kLeftItemCharge, modvalue);

							float expvalue = SoulValue.kExpValue[static_cast<uint32_t>(UsingSoulLevel) - 1];
							playerref->AddSkillExperience(RE::ActorValue::kEnchanting, expvalue);

							auto a_vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
							if (!a_vm)
								return RE::BSEventNotifyControl::kContinue;

							AddItem(a_vm, akAggressor, UsingSoulgem, 1, true);
						} else {
							a_Soulgem.push_back(a_SoulgemStar[0]);
						}
					}

					if (!IsAzuraUsed && a_Soulgem.size() > 0) {
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

						UsingSoulgem = a_Soulgem[0].first;
						UsingSoulLevel = a_Soulgem[0].second;

						if (IniValue.bMessage) {
							char message[64];
							std::string temp = UsingSoulgem->GetName();
							temp = temp + " Used";
							strcpy_s(message, temp.c_str());
							RE::DebugNotification(message);
						}

						akAggressor->RemoveItem(UsingSoulgem, 1, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);

						float modvalue = SoulValue.kSoulValue[static_cast<uint32_t>(UsingSoulLevel) - 1];
						modvalue = lCharge + modvalue > MAXcharge ? MAXcharge - lCharge : modvalue;

						akAggressor->ModActorValue(RE::ActorValue::kLeftItemCharge, modvalue);

						float expvalue = SoulValue.kExpValue[static_cast<uint32_t>(UsingSoulLevel) - 1];
						playerref->AddSkillExperience(RE::ActorValue::kEnchanting, expvalue);
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
