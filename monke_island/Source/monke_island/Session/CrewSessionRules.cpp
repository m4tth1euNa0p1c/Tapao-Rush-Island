// monke_island

#include "Session/CrewSessionRules.h"
#include "Engine/World.h"
#include "Misc/PackageName.h"

#define LOCTEXT_NAMESPACE "CrewSession"

namespace
{
	/** Characters a name or a message never keeps: controls (C0, DEL, C1), soft hyphen, zero-width characters and
	 *  marks, line and paragraph separators, bidirectional embeddings, overrides and isolates, invisible operators,
	 *  byte order mark, interlinear annotations, tags */
	bool IsInvisibleOrControl(uint32 CodePoint)
	{
		return CodePoint < 0x20u
			|| (CodePoint >= 0x7Fu && CodePoint <= 0x9Fu)
			|| CodePoint == 0xADu
			|| CodePoint == 0x180Eu
			|| (CodePoint >= 0x200Bu && CodePoint <= 0x200Fu)
			|| (CodePoint >= 0x2028u && CodePoint <= 0x202Eu)
			|| (CodePoint >= 0x2060u && CodePoint <= 0x206Fu)
			|| CodePoint == 0xFEFFu
			|| (CodePoint >= 0xFFF9u && CodePoint <= 0xFFFBu)
			|| (CodePoint >= 0xE0000u && CodePoint <= 0xE007Fu);
	}

	/** Spaces of every width: kept as one plain space between two words */
	bool IsSpace(uint32 CodePoint)
	{
		return CodePoint == 0x20u || CodePoint == 0xA0u || CodePoint == 0x1680u || (CodePoint >= 0x2000u && CodePoint <= 0x200Au)
			|| CodePoint == 0x202Fu || CodePoint == 0x205Fu || CodePoint == 0x3000u;
	}

	/** Combining marks: a few in a row accent a letter, a pile of them overflows the line ("zalgo") */
	bool IsCombiningMark(uint32 CodePoint)
	{
		return (CodePoint >= 0x0300u && CodePoint <= 0x036Fu) || (CodePoint >= 0x1AB0u && CodePoint <= 0x1AFFu)
			|| (CodePoint >= 0x1DC0u && CodePoint <= 0x1DFFu) || (CodePoint >= 0x20D0u && CodePoint <= 0x20FFu)
			|| (CodePoint >= 0xFE20u && CodePoint <= 0xFE2Fu);
	}

	/** Most combining marks kept in a row */
	constexpr int32 MaxCombiningInARow = 2;

	/** The filter of SanitizePlayerName and SanitizeMessage */
	FString FilterText(const FString& Raw, int32 MaxChars)
	{
		const int32 Limit = FMath::Max(0, MaxChars);
		FString Out;
		if (Limit == 0 || Raw.IsEmpty())
		{
			return Out;
		}
		Out.Reserve(FMath::Min(Raw.Len(), Limit * 2));
		const TCHAR* Chars = *Raw;
		const int32 Length = Raw.Len();
		int32 Kept = 0;
		int32 CombiningRun = 0;
		bool bPendingSpace = false;
		for (int32 Index = 0; Index < Length && Kept < Limit; ++Index)
		{
			uint32 CodePoint = static_cast<uint32>(Chars[Index]);
			int32 Units = 1;
			if constexpr (sizeof(TCHAR) == 2)
			{
				// UTF-16: a pair of surrogates is one character; half of one is dropped
				if (CodePoint >= 0xD800u && CodePoint <= 0xDBFFu)
				{
					const uint32 Next = Index + 1 < Length ? static_cast<uint32>(Chars[Index + 1]) : 0u;
					if (Next < 0xDC00u || Next > 0xDFFFu)
					{
						continue;
					}
					CodePoint = 0x10000u + ((CodePoint - 0xD800u) << 10) + (Next - 0xDC00u);
					Units = 2;
				}
				else if (CodePoint >= 0xDC00u && CodePoint <= 0xDFFFu)
				{
					continue;
				}
			}
			const int32 First = Index;
			Index += Units - 1;
			if (IsInvisibleOrControl(CodePoint))
			{
				continue;
			}
			if (IsSpace(CodePoint))
			{
				// a run of spaces is one space, never at the start nor at the end
				bPendingSpace = Kept > 0;
				CombiningRun = 0;
				continue;
			}
			if (IsCombiningMark(CodePoint))
			{
				if (Kept == 0 || bPendingSpace || CombiningRun >= MaxCombiningInARow)
				{
					continue;
				}
				++CombiningRun;
			}
			else
			{
				CombiningRun = 0;
			}
			if (bPendingSpace)
			{
				// the space needs room for the character after it
				if (Kept + 2 > Limit)
				{
					break;
				}
				Out.AppendChar(TEXT(' '));
				++Kept;
				bPendingSpace = false;
			}
			for (int32 Unit = First; Unit < First + Units; ++Unit)
			{
				Out.AppendChar(Chars[Unit]);
			}
			++Kept;
		}
		return Out;
	}
}

const TCHAR* LexToString(ECrewJoinRefusal Refusal)
{
	switch (Refusal)
	{
	case ECrewJoinRefusal::None:					return TEXT("None");
	case ECrewJoinRefusal::ExpeditionInProgress:	return TEXT("ExpeditionInProgress");
	case ECrewJoinRefusal::SessionFull:				return TEXT("SessionFull");
	case ECrewJoinRefusal::SpectatorOnly:			return TEXT("SpectatorOnly");
	default:										return TEXT("Unknown");
	}
}

FString CrewSession::MakeCrewKey(const FString& UniqueId, const FString& PlayerName)
{
	if (!UniqueId.IsEmpty())
	{
		return FString(TEXT("id:")) + UniqueId;
	}
	return PlayerName.IsEmpty() ? FString() : FString(TEXT("name:")) + PlayerName;
}

FString CrewSession::SanitizePlayerName(const FString& Raw, int32 MaxChars)
{
	return FilterText(Raw, MaxChars);
}

FString CrewSession::SanitizeMessage(const FString& Raw, int32 MaxChars)
{
	return FilterText(Raw, MaxChars);
}

FString CrewSession::MapShortName(const FString& URLOrPackage)
{
	FString Map = URLOrPackage;
	int32 Cut = INDEX_NONE;
	if (Map.FindChar(TEXT('?'), Cut))
	{
		Map.LeftInline(Cut);
	}
	if (Map.FindChar(TEXT('#'), Cut))
	{
		Map.LeftInline(Cut);
	}
	Map.TrimStartAndEndInline();
	// "/Game/Maps/L_Boot.L_Boot": the package, not the object in it
	int32 Slash = INDEX_NONE;
	Map.FindLastChar(TEXT('/'), Slash);
	int32 Dot = INDEX_NONE;
	if (Map.FindLastChar(TEXT('.'), Dot) && Dot > Slash)
	{
		Map.LeftInline(Dot);
	}
	return UWorld::RemovePIEPrefix(FPackageName::GetShortName(Map));
}

FText CrewSession::GetRefusalMessage(ECrewJoinRefusal Refusal, int32 MaxPlayers)
{
	switch (Refusal)
	{
	case ECrewJoinRefusal::ExpeditionInProgress:
		return LOCTEXT("ExpeditionInProgress", "Expédition en cours : seul l'équipage parti en expédition peut se reconnecter. Réessaie entre deux expéditions.");
	case ECrewJoinRefusal::SessionFull:
		return FText::Format(LOCTEXT("SessionFull", "Partie complète : {0} joueurs au plus."), FText::AsNumber(FMath::Max(1, MaxPlayers)));
	case ECrewJoinRefusal::SpectatorOnly:
		return LOCTEXT("SpectatorOnly", "Impossible de rejoindre en spectateur.");
	default:
		return FText::GetEmpty();
	}
}

FText CrewSession::DescribeJoinFailure(EJoinFailureKind Kind, const FString& EngineError)
{
	switch (Kind)
	{
	case EJoinFailureKind::Version:
		return LOCTEXT("FailureVersion", "Version incompatible : l'hôte et toi n'avez pas la même version du jeu.");
	case EJoinFailureKind::Timeout:
		return LOCTEXT("FailureTimeout", "Connexion expirée : partie introuvable à cette adresse, ou l'hôte ne répond pas.");
	case EJoinFailureKind::Lost:
		return LOCTEXT("FailureLost", "Connexion perdue pendant la connexion à la partie.");
	default:
		break;
	}
	const FString Clean = SanitizeMessage(EngineError, MaxFailureMessageChars);
	// the engine's own refusals (AGameSession::ApproveLogin, AGameModeBase::PreLogin, UPendingNetGame), in English
	if (Clean.IsEmpty() || Clean.Contains(TEXT("Pending Connection Failed"), ESearchCase::IgnoreCase))
	{
		return LOCTEXT("FailureGeneric", "Connexion refusée par l'hôte.");
	}
	if (Clean.Contains(TEXT("Server full"), ESearchCase::IgnoreCase))
	{
		return LOCTEXT("FailureFull", "Partie complète : impossible de la rejoindre.");
	}
	if (Clean.Contains(TEXT("incompatible_unique_net_id"), ESearchCase::IgnoreCase))
	{
		return LOCTEXT("FailureIdentity", "Connexion refusée : identité de joueur incompatible avec l'hôte.");
	}
	// the host's message (GetRefusalMessage on its side), already written for the player
	return FText::FromString(Clean);
}

// ---------------------------------------------------------------------------------------------------- roster

bool FCrewRoster::BeginServerWorld(const FString& WorldShortName)
{
	const bool bGoesOn = !PendingTravelMap.IsEmpty() && !WorldShortName.IsEmpty()
		&& PendingTravelMap.Equals(WorldShortName, ESearchCase::IgnoreCase);
	if (!bGoesOn)
	{
		Reset();
	}
	PendingTravelMap.Reset();
	return bGoesOn;
}

void FCrewRoster::NoteServerTravel(const FString& DestinationShortName)
{
	PendingTravelMap = DestinationShortName;
}

void FCrewRoster::Reset()
{
	bExpeditionLaunched = false;
	Members.Reset();
	PendingTravelMap.Reset();
}

void FCrewRoster::Launch(TConstArrayView<FString> PresentKeys)
{
	bExpeditionLaunched = true;
	for (const FString& Key : PresentKeys)
	{
		if (!Key.IsEmpty())
		{
			Members.Add(Key);
		}
	}
}

void FCrewRoster::NoteLogin(const FString& Key, bool bLobby)
{
	if (bExpeditionLaunched && !bLobby && !Key.IsEmpty())
	{
		Members.Add(Key);
	}
}

bool FCrewRoster::IsMember(const FString& Key) const
{
	return !Key.IsEmpty() && Members.Contains(Key);
}

ECrewJoinRefusal FCrewRoster::Evaluate(const FString& Key, bool bLobby, bool bSpectatorOnly, bool bSessionFull) const
{
	if (bSpectatorOnly)
	{
		return ECrewJoinRefusal::SpectatorOnly;
	}
	if (bExpeditionLaunched && !bLobby && !IsMember(Key))
	{
		return ECrewJoinRefusal::ExpeditionInProgress;
	}
	if (bSessionFull)
	{
		return ECrewJoinRefusal::SessionFull;
	}
	return ECrewJoinRefusal::None;
}

#undef LOCTEXT_NAMESPACE
